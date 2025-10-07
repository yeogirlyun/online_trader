#!/usr/bin/env python3
"""
MarS Bridge for Sentio C++ Virtual Market Testing

This script creates a bridge between Microsoft Research's MarS (Market Simulation Engine)
and our C++ virtual market testing system. It generates realistic market data using MarS
and exports it in a format that our C++ system can consume.
"""

import sys
import os
import json
import pandas as pd
import numpy as np
from pathlib import Path
from datetime import datetime, timedelta
from typing import List, Dict, Any, Optional
import logging
import signal

# Add MarS to Python path
mars_path = Path(__file__).parent.parent / "MarS"
sys.path.insert(0, str(mars_path))

try:
    from market_simulation.agents.noise_agent import NoiseAgent
    from market_simulation.agents.background_agent import BackgroundAgent
    from market_simulation.states.trade_info_state import TradeInfoState
    from market_simulation.states.order_state import Converter, OrderState
    from market_simulation.rollout.model_client import ModelClient
    from market_simulation.conf import C
    from mlib.core.env import Env
    from mlib.core.event import create_exchange_events
    from mlib.core.exchange import Exchange
    from mlib.core.exchange_config import create_exchange_config_without_call_auction
    from mlib.core.trade_info import TradeInfo
    from pandas import Timestamp
    MARS_AVAILABLE = True
except ImportError as e:
    print(f"Warning: MarS not available: {e}")
    MARS_AVAILABLE = False
    # Define dummy classes for type hints when MarS is not available
    class Exchange: pass
    class TradeInfo: pass
    class Timestamp: pass

# Import our custom agent
try:
    from historical_context_agent import HistoricalContextAgent, create_historical_context_agent
    HISTORICAL_AGENT_AVAILABLE = True
except ImportError as e:
    print(f"Warning: HistoricalContextAgent not available: {e}")
    HISTORICAL_AGENT_AVAILABLE = False

# Configure logging with better error handling
def setup_logging():
    """Setup logging with broken pipe error handling"""
    try:
        logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
        logger = logging.getLogger(__name__)
        
        # Handle broken pipe errors gracefully
        def signal_handler(signum, frame):
            sys.exit(0)
        
        signal.signal(signal.SIGPIPE, signal_handler)
        return logger
    except:
        # Fallback to basic logging if there are issues
        return logging.getLogger(__name__)

logger = setup_logging()

class MarsDataGenerator:
    """Generates realistic market data using MarS simulation engine."""
    
    def __init__(self, symbol: str = "QQQ", base_price: float = 458.0):
        self.symbol = symbol
        self.base_price = base_price
        self.mars_available = MARS_AVAILABLE
        
        if not self.mars_available:
            logger.warning("MarS not available, falling back to basic simulation")
    
    def generate_market_data(self, 
                           duration_minutes: int = 60,
                           bar_interval_seconds: int = 60,
                           num_simulations: int = 1,
                           market_regime: str = "normal") -> List[Dict[str, Any]]:
        """
        Generate realistic market data using MarS.
        
        Args:
            duration_minutes: Duration of simulation in minutes
            bar_interval_seconds: Interval between bars in seconds
            num_simulations: Number of simulations to run
            market_regime: Market regime type ("normal", "volatile", "trending")
            
        Returns:
            List of market data dictionaries
        """
        if not self.mars_available:
            return self._generate_fallback_data(duration_minutes, bar_interval_seconds, num_simulations)
        
        all_results = []
        
        for sim_idx in range(num_simulations):
            logger.info(f"Running MarS simulation {sim_idx + 1}/{num_simulations}")
            
            try:
                simulation_data = self._run_mars_simulation(
                    duration_minutes, bar_interval_seconds, market_regime, sim_idx
                )
                all_results.extend(simulation_data)
                
            except Exception as e:
                logger.error(f"MarS simulation {sim_idx + 1} failed: {e}")
                # Fallback to basic simulation
                fallback_data = self._generate_fallback_data(duration_minutes, bar_interval_seconds, 1)
                all_results.extend(fallback_data)
        
        return all_results
    
    def _run_mars_simulation(self, 
                            duration_minutes: int,
                            bar_interval_seconds: int,
                            market_regime: str,
                            seed: int) -> List[Dict[str, Any]]:
        """Run a single MarS simulation."""
        
        # Calculate time range
        start_time = Timestamp("2024-01-01 09:30:00")
        end_time = start_time + timedelta(minutes=duration_minutes)
        
        # Configure market parameters based on regime
        regime_config = self._get_regime_config(market_regime)
        
        # Create exchange environment
        exchange_config = create_exchange_config_without_call_auction(
            market_open=start_time,
            market_close=end_time,
            symbols=[self.symbol],
        )
        exchange = Exchange(exchange_config)
        
        # Initialize noise agent with regime-specific parameters
        agent = NoiseAgent(
            symbol=self.symbol,
            init_price=int(self.base_price * 100),  # MarS uses integer prices
            interval_seconds=bar_interval_seconds,
            start_time=start_time,
            end_time=end_time,
            seed=seed,
        )
        
        # Configure simulation environment
        exchange.register_state(TradeInfoState())
        env = Env(exchange=exchange, description=f"MarS simulation - {market_regime}")
        env.register_agent(agent)
        env.push_events(create_exchange_events(exchange_config))
        
        # Run simulation
        for observation in env.env():
            action = observation.agent.get_action(observation)
            env.step(action)
        
        # Extract trade information
        trade_infos = self._extract_trade_information(exchange, start_time, end_time)
        
        # Convert to our format
        return self._convert_to_bar_format(trade_infos, bar_interval_seconds)
    
    def _get_regime_config(self, market_regime: str) -> Dict[str, Any]:
        """Get market regime configuration."""
        regimes = {
            "normal": {"volatility": 0.008, "trend": 0.001},
            "volatile": {"volatility": 0.025, "trend": 0.005},
            "trending": {"volatility": 0.015, "trend": 0.02},
            "bear": {"volatility": 0.025, "trend": -0.015},
        }
        return regimes.get(market_regime, regimes["normal"])
    
    def _extract_trade_information(self, exchange: Exchange, start_time: Timestamp, end_time: Timestamp) -> List[TradeInfo]:
        """Extract trade information from completed simulation."""
        state = exchange.states()[self.symbol][TradeInfoState.__name__]
        trade_infos = state.trade_infos
        trade_infos = [x for x in trade_infos if start_time <= x.order.time <= end_time]
        return trade_infos
    
    def _convert_to_bar_format(self, trade_infos: List[TradeInfo], bar_interval_seconds: int) -> List[Dict[str, Any]]:
        """Convert MarS trade information to bar format compatible with our C++ system."""
        if not trade_infos:
            return []
        
        # Group trades by time intervals
        bars = []
        current_time = trade_infos[0].order.time
        bar_trades = []
        
        for trade_info in trade_infos:
            # Check if we need to create a new bar
            if (trade_info.order.time - current_time).total_seconds() >= bar_interval_seconds:
                if bar_trades:
                    bar = self._create_bar_from_trades(bar_trades, current_time)
                    if bar:
                        bars.append(bar)
                current_time = trade_info.order.time
                bar_trades = [trade_info]
            else:
                bar_trades.append(trade_info)
        
        # Add the last bar
        if bar_trades:
            bar = self._create_bar_from_trades(bar_trades, current_time)
            if bar:
                bars.append(bar)
        
        return bars
    
    def generate_market_data_with_historical_context(self,
                                                   historical_data_file: str,
                                                   continuation_minutes: int = 60,
                                                   bar_interval_seconds: int = 60,
                                                   num_simulations: int = 1,
                                                   use_mars_ai: bool = True) -> List[Dict[str, Any]]:
        """
        Generate market data using historical context transitioning to MarS AI.
        
        Args:
            historical_data_file: Path to CSV file with historical 1-minute bars
            continuation_minutes: Minutes to continue after historical data
            bar_interval_seconds: Interval between bars in seconds
            num_simulations: Number of simulations to run
            use_mars_ai: Whether to use MarS AI for continuation (vs basic synthetic)
            
        Returns:
            List of market data dictionaries
        """
        if not self.mars_available or not HISTORICAL_AGENT_AVAILABLE:
            logger.warning("MarS or HistoricalContextAgent not available, falling back to basic simulation")
            return self._generate_fallback_data(continuation_minutes, bar_interval_seconds, num_simulations)
        
        all_results = []
        
        for sim_idx in range(num_simulations):
            logger.info(f"Running historical context simulation {sim_idx + 1}/{num_simulations}")
            
            try:
                simulation_data = self._run_historical_context_simulation(
                    historical_data_file, continuation_minutes, bar_interval_seconds, 
                    use_mars_ai, sim_idx
                )
                all_results.extend(simulation_data)
                
            except Exception as e:
                logger.error(f"Historical context simulation {sim_idx + 1} failed: {e}")
                # Fallback to basic simulation
                fallback_data = self._generate_fallback_data(continuation_minutes, bar_interval_seconds, 1)
                all_results.extend(fallback_data)
        
        return all_results
    
    def _run_historical_context_simulation(self,
                                         historical_data_file: str,
                                         continuation_minutes: int,
                                         bar_interval_seconds: int,
                                         use_mars_ai: bool,
                                         seed: int) -> List[Dict[str, Any]]:
        """Run a simulation using historical context."""
        
        # Create historical context agent
        historical_agent = create_historical_context_agent(
            symbol=self.symbol,
            historical_data_file=historical_data_file,
            continuation_minutes=continuation_minutes,
            seed=seed
        )
        
        # Calculate time range - ensure timezone consistency
        start_time = historical_agent.start_time
        end_time = historical_agent.end_time
        
        # Convert to timezone-naive for MarS compatibility
        if start_time.tz is not None:
            start_time = start_time.tz_convert('UTC').tz_localize(None)
        if end_time.tz is not None:
            end_time = end_time.tz_convert('UTC').tz_localize(None)
        
        # Create exchange environment
        exchange_config = create_exchange_config_without_call_auction(
            market_open=start_time,
            market_close=end_time,
            symbols=[self.symbol],
        )
        exchange = Exchange(exchange_config)
        
        # Register states
        exchange.register_state(TradeInfoState())
        
        # Add AI-powered continuation if requested
        agents = [historical_agent]
        
        if use_mars_ai:
            try:
                # Set up MarS AI agent for continuation
                converter_dir = Path(C.directory.input_root_dir) / C.order_model.converter_dir
                converter = Converter(converter_dir)
                model_client = ModelClient(
                    model_name=C.model_serving.model_name, 
                    ip=C.model_serving.ip, 
                    port=C.model_serving.port
                )
                
                # Create BackgroundAgent for AI-powered continuation
                mars_agent = BackgroundAgent(
                    symbol=self.symbol,
                    converter=converter,
                    start_time=historical_agent.historical_end_time,
                    end_time=end_time,
                    model_client=model_client,
                    init_agent=historical_agent
                )
                
                agents.append(mars_agent)
                
                # Register OrderState for AI agent
                exchange.register_state(
                    OrderState(
                        num_max_orders=C.order_model.seq_len,
                        num_bins_price_level=converter.price_level.num_bins,
                        num_bins_pred_order_volume=converter.pred_order_volume.num_bins,
                        num_bins_order_interval=converter.order_interval.num_bins,
                        converter=converter,
                    )
                )
                
                logger.info("✅ Using MarS AI for continuation")
                
            except Exception as e:
                logger.warning(f"Failed to set up MarS AI agent: {e}, using historical agent only")
        
        # Configure simulation environment
        env = Env(exchange=exchange, description=f"Historical context simulation - {self.symbol}")
        
        for agent in agents:
            env.register_agent(agent)
        
        env.push_events(create_exchange_events(exchange_config))
        
        # Run simulation
        for observation in env.env():
            action = observation.agent.get_action(observation)
            env.step(action)
        
        # Extract trade information
        trade_infos = self._extract_trade_information(exchange, start_time, end_time)
        
        # Convert to our format
        return self._convert_to_bar_format(trade_infos, bar_interval_seconds)
    
    def _create_bar_from_trades(self, trades: List[TradeInfo], timestamp: Timestamp) -> Dict[str, Any]:
        """Create a bar from a list of trades."""
        if not trades:
            return None
        
        # Extract prices and volumes
        prices = [t.lob_snapshot.last_price for t in trades if t.lob_snapshot.last_price > 0]
        volumes = [t.order.volume for t in trades if t.order.volume > 0]
        
        if not prices:
            return None
        
        # Calculate OHLC
        open_price = prices[0] / 100.0  # Convert from MarS integer format
        close_price = prices[-1] / 100.0
        high_price = max(prices) / 100.0
        low_price = min(prices) / 100.0
        volume = sum(volumes) if volumes else 1000
        
        return {
            "timestamp": int(timestamp.timestamp()),
            "open": open_price,
            "high": high_price,
            "low": low_price,
            "close": close_price,
            "volume": volume,
            "symbol": self.symbol
        }
    
    def _generate_fallback_data(self, 
                              duration_minutes: int,
                              bar_interval_seconds: int,
                              num_simulations: int) -> List[Dict[str, Any]]:
        """Generate fallback data when MarS is not available."""
        logger.info("Generating fallback market data")
        
        bars = []
        current_time = datetime.now()
        
        for sim in range(num_simulations):
            current_price = self.base_price
            
            for i in range(duration_minutes):
                # Simple random walk with realistic parameters
                price_change = np.random.normal(0, 0.001)  # 0.1% volatility
                current_price *= (1 + price_change)
                
                # Add some intraday variation
                high_price = current_price * (1 + abs(np.random.normal(0, 0.0005)))
                low_price = current_price * (1 - abs(np.random.normal(0, 0.0005)))
                
                # Realistic volume
                volume = int(np.random.normal(150000, 50000))
                volume = max(volume, 50000)  # Minimum volume
                
                bar = {
                    "timestamp": int(current_time.timestamp()),
                    "open": current_price,
                    "high": high_price,
                    "low": low_price,
                    "close": current_price,
                    "volume": volume,
                    "symbol": self.symbol
                }
                
                bars.append(bar)
                current_time += timedelta(seconds=bar_interval_seconds)
        
        return bars

def export_to_csv(data: List[Dict[str, Any]], filename: str):
    """Export market data to CSV format."""
    if not data:
        logger.warning("No data to export")
        return
    
    df = pd.DataFrame(data)
    df.to_csv(filename, index=False)
    logger.info(f"Exported {len(data)} bars to {filename}")

def export_to_json(data: List[Dict[str, Any]], filename: str):
    """Export market data to JSON format."""
    with open(filename, 'w') as f:
        json.dump(data, f, indent=2)
    logger.info(f"Exported {len(data)} bars to {filename}")

def main():
    """Main function for testing MarS integration."""
    import argparse
    
    parser = argparse.ArgumentParser(description="MarS Bridge for Sentio C++ Virtual Market Testing")
    parser.add_argument("--symbol", default="QQQ", help="Symbol to simulate")
    parser.add_argument("--duration", type=int, default=60, help="Duration in minutes")
    parser.add_argument("--interval", type=int, default=60, help="Bar interval in seconds")
    parser.add_argument("--simulations", type=int, default=1, help="Number of simulations")
    parser.add_argument("--regime", default="normal", choices=["normal", "volatile", "trending", "bear"], help="Market regime")
    parser.add_argument("--output", default="mars_data.json", help="Output filename")
    parser.add_argument("--format", default="json", choices=["json", "csv"], help="Output format")
    parser.add_argument("--historical-data", help="Path to historical CSV data file")
    parser.add_argument("--continuation-minutes", type=int, default=60, help="Minutes to continue after historical data")
    parser.add_argument("--use-mars-ai", action="store_true", help="Use MarS AI for continuation (requires model server)")
    parser.add_argument("--quiet", action="store_true", help="Suppress debug output")
    
    args = parser.parse_args()
    
    # Generate market data
    generator = MarsDataGenerator(symbol=args.symbol, base_price=458.0)
    
    if args.historical_data:
        # Use historical context approach
        print(f"🔄 Using historical data: {args.historical_data}")
        print(f"⏱️  Continuation: {args.continuation_minutes} minutes")
        print(f"🤖 MarS AI: {'Enabled' if args.use_mars_ai else 'Disabled'}")
        
        data = generator.generate_market_data_with_historical_context(
            historical_data_file=args.historical_data,
            continuation_minutes=args.continuation_minutes,
            bar_interval_seconds=args.interval,
            num_simulations=args.simulations,
            use_mars_ai=args.use_mars_ai
        )
    else:
        # Use standard MarS approach
        data = generator.generate_market_data(
            duration_minutes=args.duration,
            bar_interval_seconds=args.interval,
            num_simulations=args.simulations,
            market_regime=args.regime
        )
    
    # Export data
    if args.format == "csv":
        export_to_csv(data, args.output)
    else:
        export_to_json(data, args.output)
    
    if not args.quiet:
        print(f"Generated {len(data)} bars for {args.symbol}")
        print(f"MarS available: {MARS_AVAILABLE}")
        
        if data:
            valid_bars = [bar for bar in data if 'low' in bar and 'high' in bar and 'volume' in bar]
            if valid_bars:
                print(f"Price range: ${min(bar['low'] for bar in valid_bars):.2f} - ${max(bar['high'] for bar in valid_bars):.2f}")
                print(f"Volume range: {min(bar['volume'] for bar in valid_bars):,} - {max(bar['volume'] for bar in valid_bars):,}")
            else:
                print("No valid bars generated")

if __name__ == "__main__":
    main()
