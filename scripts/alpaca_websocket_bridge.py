#!/usr/bin/env python3 -u
"""
Alpaca WebSocket Bridge for C++ Live Trading

Connects to Alpaca IEX WebSocket and writes bars to a named pipe (FIFO)
for consumption by the C++ live trading system.

Uses official alpaca-py SDK with built-in reconnection.
"""

import os
import sys
import json
import time
import signal
from datetime import datetime
from alpaca.data.live import StockDataStream
from alpaca.data.models import Bar
from alpaca.data.enums import DataFeed

# FIFO pipe path for C++ communication
FIFO_PATH = "/tmp/alpaca_bars.fifo"

# Track connection health
last_bar_time = None
running = True


def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n[BRIDGE] Shutdown signal received - closing connection...")
    running = False
    sys.exit(0)


def create_fifo():
    """Create named pipe (FIFO) if it doesn't exist"""
    if os.path.exists(FIFO_PATH):
        os.remove(FIFO_PATH)

    os.mkfifo(FIFO_PATH)
    print(f"[BRIDGE] Created FIFO pipe: {FIFO_PATH}")




async def bar_handler(bar: Bar):
    """
    Handle incoming bar from Alpaca WebSocket
    Only forward SPY bars - trader makes decisions based on SPY only
    """
    global last_bar_time

    try:
        # Only process SPY bars (trader only needs SPY for signal generation)
        if bar.symbol != "SPY":
            return

        # Convert Alpaca Bar to our JSON format
        bar_data = {
            "symbol": bar.symbol,
            "timestamp_ms": int(bar.timestamp.timestamp() * 1000),
            "open": float(bar.open),
            "high": float(bar.high),
            "low": float(bar.low),
            "close": float(bar.close),
            "volume": int(bar.volume),
            "vwap": float(bar.vwap) if bar.vwap else 0.0,
            "trade_count": int(bar.trade_count) if bar.trade_count else 0
        }

        # Log received bar
        timestamp_str = bar.timestamp.strftime('%Y-%m-%d %H:%M:%S')
        print(f"[BRIDGE] ✓ {bar.symbol} @ {timestamp_str} | "
              f"O:{bar.open:.2f} H:{bar.high:.2f} L:{bar.low:.2f} C:{bar.close:.2f} V:{bar.volume}", flush=True)

        # Send SPY bar immediately to FIFO
        try:
            with open(FIFO_PATH, 'w') as fifo:
                json.dump(bar_data, fifo)
                fifo.write('\n')
                fifo.flush()
            print(f"[BRIDGE] → Sent SPY bar to trader", flush=True)
        except Exception as e:
            # If C++ not reading, skip (don't block)
            pass

        last_bar_time = time.time()

    except Exception as e:
        print(f"[BRIDGE] ❌ Error processing bar: {e}", file=sys.stderr, flush=True)


async def connection_handler(conn_status):
    """Handle WebSocket connection status changes"""
    if conn_status == "connected":
        print("[BRIDGE] ✓ WebSocket connected to Alpaca IEX")
    elif conn_status == "disconnected":
        print("[BRIDGE] ⚠️  WebSocket disconnected - auto-reconnecting...")
    elif conn_status == "auth_success":
        print("[BRIDGE] ✓ Authentication successful")
    elif conn_status == "auth_failed":
        print("[BRIDGE] ❌ Authentication failed - check credentials", file=sys.stderr)
    else:
        print(f"[BRIDGE] Connection status: {conn_status}")


def main():
    """Main bridge loop"""
    global running

    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("=" * 70)
    print("Alpaca WebSocket Bridge for C++ Live Trading")
    print("=" * 70)

    # Get credentials from environment
    api_key = os.getenv('ALPACA_PAPER_API_KEY')
    api_secret = os.getenv('ALPACA_PAPER_SECRET_KEY')

    if not api_key or not api_secret:
        print("[BRIDGE] ❌ ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set")
        sys.exit(1)

    print(f"[BRIDGE] API Key: {api_key[:8]}...")
    print(f"[BRIDGE] Using Alpaca Paper Trading (IEX data)")
    print()

    # Create FIFO pipe
    create_fifo()
    print()

    # Create WebSocket client
    print("[BRIDGE] Initializing Alpaca WebSocket client...")
    wss_client = StockDataStream(api_key, api_secret, feed=DataFeed.IEX)  # IEX = free tier

    # Subscribe to SPY bars only (trader only needs SPY for signal generation)
    instruments = ['SPY']
    print(f"[BRIDGE] Subscribing to SPY bars only")
    print(f"[BRIDGE] (Trader makes all decisions based on SPY, uses market orders for other symbols)")

    wss_client.subscribe_bars(bar_handler, 'SPY')

    print()
    print("[BRIDGE] ✓ Bridge active - forwarding bars to C++ via FIFO")
    print(f"[BRIDGE] FIFO path: {FIFO_PATH}")
    print("[BRIDGE] Press Ctrl+C to stop")
    print("=" * 70)
    print()

    try:
        # Run WebSocket client (blocks until stopped)
        # Built-in reconnection handled by SDK
        wss_client.run()

    except KeyboardInterrupt:
        print("\n[BRIDGE] Stopped by user")
    except Exception as e:
        print(f"\n[BRIDGE] ❌ Fatal error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        # Cleanup
        if os.path.exists(FIFO_PATH):
            os.remove(FIFO_PATH)
            print(f"[BRIDGE] Removed FIFO: {FIFO_PATH}")


if __name__ == "__main__":
    main()
