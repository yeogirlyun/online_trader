#!/usr/bin/env python3
"""
Mock Alpaca Paper Trading Server
=================================

Simulates Alpaca REST API and Polygon WebSocket for local testing.
Replays MarS historical data to test live trading without connecting to real Alpaca.

Features:
- HTTP REST API mimicking Alpaca endpoints (account, positions, orders)
- WebSocket server mimicking Polygon real-time quotes
- Replays historical data from MarS at configurable speed
- Tracks simulated portfolio state (positions, cash, P&L)
- Validates all order requests
- Logs all API calls for debugging

Usage:
    # Start mock server
    python3 tools/mock_alpaca_server.py --data data/equities/SPY_4blocks.csv --port 8000 --ws-port 8765

    # In another terminal, run live trading pointing to mock server
    ./build/sentio_cli live-trade --alpaca-url http://localhost:8000 --polygon-url ws://localhost:8765
"""

import asyncio
import json
import csv
import time
from datetime import datetime, timedelta
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Dict, List, Optional
import argparse

# HTTP server
from http.server import HTTPServer, BaseHTTPRequestHandler
from threading import Thread
import urllib.parse

# WebSocket server
import websockets

@dataclass
class Position:
    symbol: str
    quantity: float
    avg_entry_price: float
    current_price: float
    market_value: float
    unrealized_pl: float
    unrealized_pl_pct: float

@dataclass
class Order:
    order_id: str
    symbol: str
    quantity: float
    side: str  # "buy" or "sell"
    type: str  # "market", "limit"
    time_in_force: str
    status: str  # "new", "filled", "canceled"
    filled_qty: float
    filled_avg_price: float
    submitted_at: str
    filled_at: Optional[str] = None

class MockAlpacaState:
    """Simulates Alpaca paper trading account state"""

    def __init__(self, starting_capital: float = 100000.0):
        self.account_number = "PA123456789"
        self.cash = starting_capital
        self.starting_capital = starting_capital
        self.positions: Dict[str, Position] = {}
        self.orders: Dict[str, Order] = {}
        self.order_counter = 1000

        # Current market prices
        self.current_prices: Dict[str, float] = {}

    def get_account_info(self):
        """Return account info like Alpaca /v2/account"""
        portfolio_value = self.cash + sum(p.market_value for p in self.positions.values())

        return {
            "account_number": self.account_number,
            "buying_power": self.cash * 2,  # 2x margin
            "cash": self.cash,
            "portfolio_value": portfolio_value,
            "equity": portfolio_value,
            "last_equity": portfolio_value,
            "pattern_day_trader": False,
            "trading_blocked": False,
            "account_blocked": False,
            "status": "ACTIVE",
            "currency": "USD",
            "created_at": "2024-01-01T00:00:00Z"
        }

    def get_positions(self):
        """Return all positions like Alpaca /v2/positions"""
        return [asdict(p) for p in self.positions.values()]

    def update_market_prices(self, prices: Dict[str, float]):
        """Update current market prices and recalculate position values"""
        self.current_prices.update(prices)

        for symbol, pos in self.positions.items():
            if symbol in self.current_prices:
                pos.current_price = self.current_prices[symbol]
                pos.market_value = pos.quantity * pos.current_price
                pos.unrealized_pl = pos.market_value - (pos.quantity * pos.avg_entry_price)
                if pos.quantity * pos.avg_entry_price != 0:
                    pos.unrealized_pl_pct = pos.unrealized_pl / abs(pos.quantity * pos.avg_entry_price)

    def execute_market_order(self, symbol: str, quantity: float, side: str):
        """Execute market order and update positions"""
        if symbol not in self.current_prices:
            raise ValueError(f"No market price available for {symbol}")

        price = self.current_prices[symbol]
        order_id = f"order_{self.order_counter}"
        self.order_counter += 1

        # Calculate order value
        notional = abs(quantity) * price

        # Check buying power
        if side == "buy" and notional > self.cash:
            raise ValueError(f"Insufficient buying power: need ${notional:.2f}, have ${self.cash:.2f}")

        # Update positions
        if symbol in self.positions:
            pos = self.positions[symbol]
            if side == "buy":
                new_qty = pos.quantity + quantity
                new_avg_price = ((pos.quantity * pos.avg_entry_price) + (quantity * price)) / new_qty
                pos.quantity = new_qty
                pos.avg_entry_price = new_avg_price
                self.cash -= notional
            else:  # sell
                pos.quantity -= quantity
                self.cash += notional
                if abs(pos.quantity) < 0.0001:  # Close position
                    del self.positions[symbol]
        else:
            # New position
            if side == "buy":
                self.positions[symbol] = Position(
                    symbol=symbol,
                    quantity=quantity,
                    avg_entry_price=price,
                    current_price=price,
                    market_value=quantity * price,
                    unrealized_pl=0.0,
                    unrealized_pl_pct=0.0
                )
                self.cash -= notional
            else:
                # Short position
                self.positions[symbol] = Position(
                    symbol=symbol,
                    quantity=-quantity,
                    avg_entry_price=price,
                    current_price=price,
                    market_value=-quantity * price,
                    unrealized_pl=0.0,
                    unrealized_pl_pct=0.0
                )
                self.cash += notional

        # Create order record
        order = Order(
            order_id=order_id,
            symbol=symbol,
            quantity=quantity,
            side=side,
            type="market",
            time_in_force="day",
            status="filled",
            filled_qty=quantity,
            filled_avg_price=price,
            submitted_at=datetime.now().isoformat(),
            filled_at=datetime.now().isoformat()
        )
        self.orders[order_id] = order

        print(f"✓ Executed {side.upper()} {quantity} {symbol} @ ${price:.2f} (Order {order_id})")
        print(f"  Cash: ${self.cash:.2f} | Portfolio: ${self.get_account_info()['portfolio_value']:.2f}")

        return order

# Global state
alpaca_state = None

class AlpacaHTTPHandler(BaseHTTPRequestHandler):
    """HTTP handler mimicking Alpaca REST API"""

    def log_message(self, format, *args):
        # Suppress default logging
        pass

    def do_GET(self):
        """Handle GET requests"""
        parsed_path = urllib.parse.urlparse(self.path)

        if parsed_path.path == "/v2/account":
            self.send_json(alpaca_state.get_account_info())

        elif parsed_path.path == "/v2/positions":
            self.send_json(alpaca_state.get_positions())

        elif parsed_path.path.startswith("/v2/positions/"):
            symbol = parsed_path.path.split("/")[-1]
            if symbol in alpaca_state.positions:
                self.send_json(asdict(alpaca_state.positions[symbol]))
            else:
                self.send_error(404, f"Position for {symbol} not found")

        else:
            self.send_error(404, "Endpoint not found")

    def do_POST(self):
        """Handle POST requests (orders)"""
        parsed_path = urllib.parse.urlparse(self.path)

        if parsed_path.path == "/v2/orders":
            content_length = int(self.headers['Content-Length'])
            body = json.loads(self.rfile.read(content_length))

            try:
                order = alpaca_state.execute_market_order(
                    symbol=body['symbol'],
                    quantity=float(body['qty']),
                    side=body['side']
                )
                self.send_json(asdict(order))
            except Exception as e:
                self.send_error(400, str(e))
        else:
            self.send_error(404, "Endpoint not found")

    def do_DELETE(self):
        """Handle DELETE requests (cancel orders)"""
        parsed_path = urllib.parse.urlparse(self.path)

        if parsed_path.path.startswith("/v2/orders/"):
            order_id = parsed_path.path.split("/")[-1]
            if order_id in alpaca_state.orders:
                alpaca_state.orders[order_id].status = "canceled"
                self.send_json(asdict(alpaca_state.orders[order_id]))
            else:
                self.send_error(404, "Order not found")
        else:
            self.send_error(404, "Endpoint not found")

    def send_json(self, data):
        """Send JSON response"""
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

def run_http_server(port: int):
    """Run HTTP server for Alpaca REST API"""
    server = HTTPServer(('localhost', port), AlpacaHTTPHandler)
    print(f"✓ Mock Alpaca REST API running on http://localhost:{port}")
    server.serve_forever()

async def polygon_websocket_handler(websocket, path):
    """Handle WebSocket connections from PolygonClient"""
    print(f"✓ New WebSocket connection from {websocket.remote_address}")

    try:
        # Wait for subscription message
        subscribe_msg = await websocket.recv()
        subscribe_data = json.loads(subscribe_msg)

        if subscribe_data.get("action") == "subscribe":
            symbols = subscribe_data.get("symbols", [])
            print(f"  Subscribed to: {', '.join(symbols)}")

            # Send confirmation
            await websocket.send(json.dumps({
                "status": "success",
                "message": f"Subscribed to {len(symbols)} symbols"
            }))

            # Keep connection alive
            while True:
                await asyncio.sleep(1)

    except websockets.exceptions.ConnectionClosed:
        print("  WebSocket connection closed")
    except Exception as e:
        print(f"  WebSocket error: {e}")

async def replay_market_data(data_file: Path, websocket_server, symbols: List[str], speed_multiplier: float = 1.0):
    """Replay historical data through WebSocket"""
    print(f"\n📊 Loading market data from {data_file}")

    # Load all bars
    bars_by_symbol = {sym: [] for sym in symbols}

    with open(data_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            symbol = row['symbol']
            if symbol in bars_by_symbol:
                bars_by_symbol[symbol].append({
                    'timestamp_ms': int(row['ts_nyt_epoch']) * 1000,
                    'symbol': symbol,
                    'open': float(row['open']),
                    'high': float(row['high']),
                    'low': float(row['low']),
                    'close': float(row['close']),
                    'volume': float(row['volume'])
                })

    total_bars = sum(len(bars) for bars in bars_by_symbol.values())
    print(f"✓ Loaded {total_bars} bars")

    # Replay data
    print(f"\n🎬 Replaying market data (speed: {speed_multiplier}x)")
    print("=" * 60)

    bar_index = 0
    while True:
        for symbol in symbols:
            if bar_index < len(bars_by_symbol[symbol]):
                bar = bars_by_symbol[symbol][bar_index]

                # Update Alpaca state prices
                alpaca_state.update_market_prices({symbol: bar['close']})

                # Broadcast to all connected WebSocket clients
                if websocket_server.websockets:
                    message = json.dumps({
                        'type': 'bar',
                        'symbol': symbol,
                        'data': bar
                    })
                    await asyncio.gather(
                        *[ws.send(message) for ws in websocket_server.websockets],
                        return_exceptions=True
                    )

                    if symbol == symbols[0]:  # Log only first symbol to reduce noise
                        timestamp = datetime.fromtimestamp(bar['timestamp_ms'] / 1000)
                        print(f"[{timestamp.strftime('%H:%M')}] {symbol}: ${bar['close']:.2f} | " +
                              f"Portfolio: ${alpaca_state.get_account_info()['portfolio_value']:.2f}")

        bar_index += 1
        if bar_index >= max(len(bars) for bars in bars_by_symbol.values()):
            print("\n✓ Replay completed")
            break

        # Sleep to simulate real-time (1 minute per bar, adjusted by speed)
        await asyncio.sleep(60 / speed_multiplier)

async def main():
    parser = argparse.ArgumentParser(description="Mock Alpaca + Polygon server for testing")
    parser.add_argument('--data', required=True, help='Path to CSV file with historical data')
    parser.add_argument('--port', type=int, default=8000, help='HTTP port for Alpaca REST API')
    parser.add_argument('--ws-port', type=int, default=8765, help='WebSocket port for Polygon')
    parser.add_argument('--symbols', default='SPY,SPXL,SH,SDS', help='Symbols to trade')
    parser.add_argument('--capital', type=float, default=100000, help='Starting capital')
    parser.add_argument('--speed', type=float, default=60.0, help='Replay speed multiplier (60=1min/sec)')

    args = parser.parse_args()

    global alpaca_state
    alpaca_state = MockAlpacaState(args.capital)

    symbols = args.symbols.split(',')

    print("=" * 60)
    print("🏦 Mock Alpaca Paper Trading Server")
    print("=" * 60)
    print(f"Starting Capital: ${args.capital:,.0f}")
    print(f"Symbols: {', '.join(symbols)}")
    print(f"Data Source: {args.data}")
    print(f"Replay Speed: {args.speed}x")
    print()

    # Start HTTP server in background thread
    http_thread = Thread(target=run_http_server, args=(args.port,), daemon=True)
    http_thread.start()

    # Start WebSocket server
    async with websockets.serve(polygon_websocket_handler, 'localhost', args.ws_port) as server:
        print(f"✓ Mock Polygon WebSocket running on ws://localhost:{args.ws_port}")
        print()
        print("=" * 60)
        print("🚀 Mock server ready! Connect your live trading client:")
        print(f"   export ALPACA_API_URL=http://localhost:{args.port}")
        print(f"   export POLYGON_WS_URL=ws://localhost:{args.ws_port}")
        print("=" * 60)
        print()

        # Wait a bit for connections
        await asyncio.sleep(2)

        # Start replaying data
        await replay_market_data(Path(args.data), server, symbols, args.speed)

        # Keep server running
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
