#!/usr/bin/env python3
"""
Online Quote Simulation Tool
============================

A comprehensive tool for simulating real-time quotes and market data
for online learning algorithms. Integrates with MarS for realistic
market simulation.

Features:
- Real-time quote generation
- MarS integration for realistic data
- Mock Polygon API replacement
- Online learning algorithm testing
- Multiple market regimes
- Historical data continuation

Usage:
    python online_quote_simulator.py --strategy online_sgd --symbol QQQ --duration 60
    python online_quote_simulator.py --mars --regime volatile --simulations 10
    python online_quote_simulator.py --realtime --symbol SPY --duration 30
"""

import argparse
import json
import os
import sys
import time
import threading
from datetime import datetime, timedelta
from typing import List, Dict, Any, Optional, Callable
import pandas as pd
import numpy as np

# Add MarS to Python path
mars_path = os.path.join(os.path.dirname(__file__), "MarS")
if os.path.exists(mars_path):
    sys.path.insert(0, mars_path)

try:
    from mars_bridge import MarsDataGenerator
    MARS_AVAILABLE = True
except ImportError:
    MARS_AVAILABLE = False
    print("Warning: MarS not available, using basic simulation")

class OnlineQuoteSimulator:
    """Online quote simulation for real-time trading algorithms"""
    
    def __init__(self, symbol: str = "QQQ", base_price: float = 458.0):
        self.symbol = symbol
        self.base_price = base_price
        self.mars_available = MARS_AVAILABLE
        self.streaming_active = False
        self.streaming_thread = None
        
        if not self.mars_available:
            print("⚠️ MarS not available, using basic simulation")
    
    def generate_realtime_quotes(self, 
                               duration_minutes: int = 60,
                               interval_ms: int = 1000,
                               market_regime: str = "normal") -> List[Dict[str, Any]]:
        """
        Generate real-time quotes for online learning algorithms
        
        Args:
            duration_minutes: Duration of simulation in minutes
            interval_ms: Interval between quotes in milliseconds
            market_regime: Market regime type
            
        Returns:
            List of quote dictionaries
        """
        quotes = []
        current_time = datetime.now()
        current_price = self.base_price
        
        # Market regime parameters
        regime_config = self._get_regime_config(market_regime)
        
        num_quotes = duration_minutes * 60 * 1000 // interval_ms
        
        print(f"🚀 Generating {num_quotes} real-time quotes for {self.symbol}")
        print(f"📊 Market regime: {market_regime}")
        print(f"⏱️  Duration: {duration_minutes} minutes")
        print(f"🔄 Interval: {interval_ms}ms")
        
        for i in range(num_quotes):
            # Generate quote
            quote = self._generate_quote(current_time, current_price, regime_config)
            quotes.append(quote)
            
            # Update price for next quote
            price_change = np.random.normal(regime_config['trend'], regime_config['volatility'])
            current_price *= (1 + price_change)
            
            # Update time
            current_time += timedelta(milliseconds=interval_ms)
            
            # Progress reporting
            if num_quotes >= 100 and (i + 1) % (num_quotes // 10) == 0:
                progress = (i + 1) / num_quotes * 100
                print(f"📊 Progress: {progress:.0f}% ({i + 1}/{num_quotes})")
        
        print(f"✅ Generated {len(quotes)} quotes")
        return quotes
    
    def generate_mars_quotes(self,
                            duration_minutes: int = 60,
                            interval_ms: int = 1000,
                            market_regime: str = "normal",
                            num_simulations: int = 1) -> List[Dict[str, Any]]:
        """
        Generate quotes using MarS for realistic market simulation
        
        Args:
            duration_minutes: Duration of simulation in minutes
            interval_ms: Interval between quotes in milliseconds
            market_regime: Market regime type
            num_simulations: Number of simulations to run
            
        Returns:
            List of quote dictionaries
        """
        if not self.mars_available:
            print("⚠️ MarS not available, falling back to basic simulation")
            return self.generate_realtime_quotes(duration_minutes, interval_ms, market_regime)
        
        print(f"🤖 Using MarS for realistic quote generation")
        print(f"📊 Market regime: {market_regime}")
        print(f"🎲 Simulations: {num_simulations}")
        
        all_quotes = []
        
        for sim_idx in range(num_simulations):
            print(f"🔄 Running MarS simulation {sim_idx + 1}/{num_simulations}")
            
            try:
                # Use MarS bridge to generate market data
                mars_generator = MarsDataGenerator(symbol=self.symbol, base_price=self.base_price)
                
                # Generate market data using MarS
                mars_data = mars_generator.generate_market_data(
                    duration_minutes=duration_minutes,
                    bar_interval_seconds=interval_ms // 1000,
                    num_simulations=1,
                    market_regime=market_regime
                )
                
                # Convert MarS data to quote format
                quotes = self._convert_mars_to_quotes(mars_data, interval_ms)
                all_quotes.extend(quotes)
                
                print(f"✅ MarS simulation {sim_idx + 1} completed: {len(quotes)} quotes")
                
            except Exception as e:
                print(f"❌ MarS simulation {sim_idx + 1} failed: {e}")
                # Fallback to basic simulation
                fallback_quotes = self.generate_realtime_quotes(duration_minutes, interval_ms, market_regime)
                all_quotes.extend(fallback_quotes)
        
        print(f"✅ Generated {len(all_quotes)} total quotes using MarS")
        return all_quotes
    
    def start_realtime_streaming(self,
                                quote_callback: Callable[[Dict[str, Any]], None],
                                interval_ms: int = 1000,
                                market_regime: str = "normal") -> None:
        """
        Start real-time quote streaming
        
        Args:
            quote_callback: Function to call with each quote
            interval_ms: Interval between quotes in milliseconds
            market_regime: Market regime type
        """
        if self.streaming_active:
            print("⚠️ Streaming already active")
            return
        
        self.streaming_active = True
        self.streaming_thread = threading.Thread(
            target=self._streaming_worker,
            args=(quote_callback, interval_ms, market_regime)
        )
        self.streaming_thread.start()
        
        print(f"🚀 Started real-time quote streaming for {self.symbol}")
        print(f"📊 Market regime: {market_regime}")
        print(f"🔄 Interval: {interval_ms}ms")
    
    def stop_realtime_streaming(self) -> None:
        """Stop real-time quote streaming"""
        if not self.streaming_active:
            return
        
        self.streaming_active = False
        if self.streaming_thread and self.streaming_thread.is_alive():
            self.streaming_thread.join()
        
        print("⏹️ Stopped real-time quote streaming")
    
    def _generate_quote(self, timestamp: datetime, current_price: float, regime_config: Dict[str, Any]) -> Dict[str, Any]:
        """Generate a single quote"""
        # Calculate bid/ask spread
        spread = current_price * 0.0001  # 0.01% spread
        bid_price = current_price - spread / 2
        ask_price = current_price + spread / 2
        
        # Generate realistic sizes
        base_size = 1000 + np.random.randint(0, 5000)
        bid_size = int(base_size * regime_config['volume_multiplier'])
        ask_size = int(base_size * regime_config['volume_multiplier'])
        
        # Generate last trade
        last_price = current_price
        last_size = 100 + np.random.randint(0, 1000)
        
        # Generate volume
        base_volume = 50000 + np.random.randint(0, 150000)
        volume = int(base_volume * regime_config['volume_multiplier'])
        
        return {
            "timestamp": int(timestamp.timestamp()),
            "symbol": self.symbol,
            "bid_price": round(bid_price, 2),
            "ask_price": round(ask_price, 2),
            "bid_size": bid_size,
            "ask_size": ask_size,
            "last_price": round(last_price, 2),
            "last_size": last_size,
            "volume": volume,
            "market_regime": regime_config['name']
        }
    
    def _convert_mars_to_quotes(self, mars_data: List[Dict[str, Any]], interval_ms: int) -> List[Dict[str, Any]]:
        """Convert MarS market data to quote format"""
        quotes = []
        
        for bar in mars_data:
            # Create multiple quotes per bar based on interval
            bar_start_time = datetime.fromtimestamp(bar['timestamp'])
            
            # Generate intra-bar quotes
            num_quotes_per_bar = max(1, 60000 // interval_ms)  # 1 minute = 60000ms
            
            for i in range(num_quotes_per_bar):
                quote_time = bar_start_time + timedelta(milliseconds=i * interval_ms)
                
                # Interpolate price within bar
                price_ratio = i / num_quotes_per_bar
                current_price = bar['open'] + (bar['close'] - bar['open']) * price_ratio
                
                quote = self._generate_quote(quote_time, current_price, {"name": "mars", "volume_multiplier": 1.0})
                quotes.append(quote)
        
        return quotes
    
    def _get_regime_config(self, market_regime: str) -> Dict[str, Any]:
        """Get market regime configuration"""
        regimes = {
            "normal": {"name": "NORMAL", "volatility": 0.008, "trend": 0.001, "volume_multiplier": 1.0},
            "volatile": {"name": "VOLATILE", "volatility": 0.025, "trend": 0.005, "volume_multiplier": 1.5},
            "trending": {"name": "TRENDING", "volatility": 0.015, "trend": 0.02, "volume_multiplier": 1.2},
            "bear": {"name": "BEAR", "volatility": 0.025, "trend": -0.015, "volume_multiplier": 1.3},
            "bull": {"name": "BULL", "volatility": 0.015, "trend": 0.02, "volume_multiplier": 1.1},
            "sideways": {"name": "SIDEWAYS", "volatility": 0.008, "trend": 0.001, "volume_multiplier": 0.8}
        }
        return regimes.get(market_regime, regimes["normal"])
    
    def _streaming_worker(self, quote_callback: Callable[[Dict[str, Any]], None], 
                         interval_ms: int, market_regime: str) -> None:
        """Worker thread for real-time streaming"""
        current_price = self.base_price
        regime_config = self._get_regime_config(market_regime)
        
        while self.streaming_active:
            quote = self._generate_quote(datetime.now(), current_price, regime_config)
            quote_callback(quote)
            
            # Update price for next quote
            price_change = np.random.normal(regime_config['trend'], regime_config['volatility'])
            current_price *= (1 + price_change)
            
            time.sleep(interval_ms / 1000.0)

class MockPolygonAPI:
    """Mock Polygon API for testing online learning algorithms"""
    
    def __init__(self, api_key: str = "mock_key"):
        self.api_key = api_key
        self.quote_simulator = OnlineQuoteSimulator()
        self.subscriptions = {}
        self.subscription_active = False
        self.subscription_thread = None
    
    def get_realtime_quote(self, symbol: str) -> Dict[str, Any]:
        """Get real-time quote (replaces Polygon API call)"""
        quote = self.quote_simulator._generate_quote(
            datetime.now(), 
            self.quote_simulator.base_price,
            self.quote_simulator._get_regime_config("normal")
        )
        quote["symbol"] = symbol
        return quote
    
    def subscribe_to_quotes(self, symbol: str, callback: Callable[[Dict[str, Any]], None]) -> None:
        """Subscribe to real-time quotes (replaces Polygon WebSocket)"""
        self.subscriptions[symbol] = callback
        
        if not self.subscription_active:
            self.subscription_active = True
            self.subscription_thread = threading.Thread(target=self._subscription_worker)
            self.subscription_thread.start()
    
    def unsubscribe_from_quotes(self, symbol: str) -> None:
        """Unsubscribe from quotes"""
        if symbol == "ALL":
            self.subscriptions.clear()
            self.subscription_active = False
            if self.subscription_thread and self.subscription_thread.is_alive():
                self.subscription_thread.join()
        else:
            self.subscriptions.pop(symbol, None)
    
    def _subscription_worker(self) -> None:
        """Worker thread for quote subscriptions"""
        while self.subscription_active:
            for symbol, callback in self.subscriptions.items():
                quote = self.get_realtime_quote(symbol)
                callback(quote)
            time.sleep(1.0)  # 1 second updates

def export_quotes_to_csv(quotes: List[Dict[str, Any]], filename: str) -> None:
    """Export quotes to CSV format"""
    if not quotes:
        print("⚠️ No quotes to export")
        return
    
    df = pd.DataFrame(quotes)
    df.to_csv(filename, index=False)
    print(f"✅ Exported {len(quotes)} quotes to {filename}")

def export_quotes_to_json(quotes: List[Dict[str, Any]], filename: str) -> None:
    """Export quotes to JSON format"""
    with open(filename, 'w') as f:
        json.dump(quotes, f, indent=2)
    print(f"✅ Exported {len(quotes)} quotes to {filename}")

def main():
    """Main function for online quote simulation"""
    parser = argparse.ArgumentParser(description="Online Quote Simulation Tool")
    parser.add_argument("--symbol", default="QQQ", help="Symbol to simulate")
    parser.add_argument("--duration", type=int, default=60, help="Duration in minutes")
    parser.add_argument("--interval", type=int, default=1000, help="Quote interval in milliseconds")
    parser.add_argument("--regime", default="normal", 
                       choices=["normal", "volatile", "trending", "bear", "bull", "sideways"],
                       help="Market regime")
    parser.add_argument("--simulations", type=int, default=1, help="Number of simulations")
    parser.add_argument("--mars", action="store_true", help="Use MarS for realistic simulation")
    parser.add_argument("--realtime", action="store_true", help="Start real-time streaming")
    parser.add_argument("--output", default="quotes.json", help="Output filename")
    parser.add_argument("--format", default="json", choices=["json", "csv"], help="Output format")
    parser.add_argument("--base-price", type=float, default=458.0, help="Base price for simulation")
    parser.add_argument("--quiet", action="store_true", help="Suppress debug output")
    
    args = parser.parse_args()
    
    # Create quote simulator
    simulator = OnlineQuoteSimulator(symbol=args.symbol, base_price=args.base_price)
    
    if args.realtime:
        # Real-time streaming mode
        print(f"🚀 Starting real-time quote streaming for {args.symbol}")
        print(f"📊 Market regime: {args.regime}")
        print(f"🔄 Interval: {args.interval}ms")
        print("Press Ctrl+C to stop...")
        
        def quote_callback(quote):
            if not args.quiet:
                print(f"📊 {quote['symbol']}: Bid={quote['bid_price']:.2f} Ask={quote['ask_price']:.2f} "
                      f"Last={quote['last_price']:.2f} Volume={quote['volume']:,}")
        
        try:
            simulator.start_realtime_streaming(quote_callback, args.interval, args.regime)
            
            # Keep running until interrupted
            while True:
                time.sleep(1)
                
        except KeyboardInterrupt:
            print("\n⏹️ Stopping real-time streaming...")
            simulator.stop_realtime_streaming()
    
    else:
        # Batch generation mode
        if args.mars:
            quotes = simulator.generate_mars_quotes(
                duration_minutes=args.duration,
                interval_ms=args.interval,
                market_regime=args.regime,
                num_simulations=args.simulations
            )
        else:
            quotes = simulator.generate_realtime_quotes(
                duration_minutes=args.duration,
                interval_ms=args.interval,
                market_regime=args.regime
            )
        
        # Export quotes
        if args.format == "csv":
            export_quotes_to_csv(quotes, args.output)
        else:
            export_quotes_to_json(quotes, args.output)
        
        if not args.quiet:
            print(f"\n📊 Quote Generation Summary:")
            print(f"  Symbol: {args.symbol}")
            print(f"  Duration: {args.duration} minutes")
            print(f"  Interval: {args.interval}ms")
            print(f"  Market Regime: {args.regime}")
            print(f"  MarS Integration: {'Enabled' if args.mars else 'Disabled'}")
            print(f"  Total Quotes: {len(quotes)}")
            
            if quotes:
                valid_quotes = [q for q in quotes if 'bid_price' in q and 'ask_price' in q]
                if valid_quotes:
                    bid_prices = [q['bid_price'] for q in valid_quotes]
                    ask_prices = [q['ask_price'] for q in valid_quotes]
                    volumes = [q['volume'] for q in valid_quotes]
                    
                    print(f"  Price Range: ${min(bid_prices):.2f} - ${max(ask_prices):.2f}")
                    print(f"  Volume Range: {min(volumes):,} - {max(volumes):,}")
                    print(f"  Average Spread: ${np.mean([q['ask_price'] - q['bid_price'] for q in valid_quotes]):.4f}")

if __name__ == "__main__":
    main()
