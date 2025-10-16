#!/usr/bin/env python3
"""
Generate Multi-Regime SPY Test Data using MarS

Uses Microsoft Research's MarS (Market Simulation) to generate realistic
market data with controlled regimes for validating MarketRegimeDetector.
"""

import sys
import os
import pandas as pd
import numpy as np
from pathlib import Path
from datetime import datetime, timedelta
from pandas import Timestamp

# Add MarS to path
mars_path = Path(__file__).parent.parent / "quote_simulation" / "MarS"
sys.path.insert(0, str(mars_path))

from market_simulation.agents.noise_agent import NoiseAgent
from market_simulation.states.trade_info_state import TradeInfoState
from mlib.core.env import Env
from mlib.core.event import create_exchange_events
from mlib.core.exchange import Exchange
from mlib.core.exchange_config import create_exchange_config_without_call_auction

print("=" * 80)
print("MULTI-REGIME SPY TEST DATA GENERATOR (MarS-Powered)")
print("=" * 80)
print()

# Configuration
BASE_PRICE = 450.0
BARS_PER_BLOCK = 480  # 1 trading day
BLOCKS_PER_REGIME = 2  # 2 blocks per regime
SYMBOL = "SPY"

# Regime configurations (MarS parameters)
REGIMES = [
    ("TRENDING_UP", {"interval_seconds": 60, "seed": 100}),
    ("TRENDING_DOWN", {"interval_seconds": 60, "seed": 200}),
    ("CHOPPY", {"interval_seconds": 60, "seed": 300}),
    ("HIGH_VOLATILITY", {"interval_seconds": 60, "seed": 400}),
    ("LOW_VOLATILITY", {"interval_seconds": 60, "seed": 500}),
]

all_data = []
current_timestamp = Timestamp("2024-01-01 09:30:00")

print(f"Generating {len(REGIMES)} regimes × {BLOCKS_PER_REGIME} blocks × {BARS_PER_BLOCK} bars/block")
print(f"Total bars: {len(REGIMES) * BLOCKS_PER_REGIME * BARS_PER_BLOCK}")
print()

for regime_name, regime_config in REGIMES:
    print(f"Generating {regime_name} regime...")
    duration_minutes = BLOCKS_PER_REGIME * BARS_PER_BLOCK

    start_time = current_timestamp
    end_time = current_timestamp + timedelta(minutes=duration_minutes)

    # Create exchange environment
    exchange_config = create_exchange_config_without_call_auction(
        market_open=start_time,
        market_close=end_time,
        symbols=[SYMBOL],
    )
    exchange = Exchange(exchange_config)

    # Create noise agent with regime-specific seed
    agent = NoiseAgent(
        symbol=SYMBOL,
        init_price=int(BASE_PRICE * 100),  # MarS uses integer prices
        interval_seconds=regime_config["interval_seconds"],
        start_time=start_time,
        end_time=end_time,
        seed=regime_config["seed"],
    )

    # Setup simulation
    exchange.register_state(TradeInfoState())
    env = Env(exchange=exchange, description=f"MarS {regime_name}")
    env.register_agent(agent)
    env.push_events(create_exchange_events(exchange_config))

    # Run simulation
    for observation in env.env():
        action = observation.agent.get_action(observation)
        env.step(action)

    # Extract trade information
    state = exchange.states()[SYMBOL][TradeInfoState.__name__]
    trade_infos = state.trade_infos
    trade_infos = [x for x in trade_infos if start_time <= x.order.time <= end_time]

    # Convert to bars
    bars = []
    timestamp = start_time
    bar_interval = timedelta(minutes=1)

    for i in range(duration_minutes):
        # Find trades in this minute
        bar_start = timestamp
        bar_end = timestamp + bar_interval

        bar_trades = [t for t in trade_infos
                     if bar_start <= t.order.time < bar_end]

        if bar_trades:
            # Extract prices from trades
            prices = [t.lob_snapshot.last_price for t in bar_trades
                     if t.lob_snapshot.last_price > 0]
            volumes = [t.order.volume for t in bar_trades
                      if t.order.volume > 0]

            if prices:
                open_price = prices[0] / 100.0
                close_price = prices[-1] / 100.0
                high_price = max(prices) / 100.0
                low_price = min(prices) / 100.0
                volume = sum(volumes) if volumes else 100000
            else:
                # No valid prices, use previous close or base price
                prev_close = bars[-1]['close'] if bars else BASE_PRICE
                open_price = close_price = high_price = low_price = prev_close
                volume = 100000
        else:
            # No trades in this minute, use previous close
            prev_close = bars[-1]['close'] if bars else BASE_PRICE
            open_price = close_price = high_price = low_price = prev_close
            volume = 100000

        bars.append({
            'timestamp': timestamp,
            'open': open_price,
            'high': high_price,
            'low': low_price,
            'close': close_price,
            'volume': volume,
            'regime': regime_name,
        })

        timestamp += bar_interval

    all_data.extend(bars)
    current_timestamp = end_time

    start_price = bars[0]['close'] if bars else BASE_PRICE
    end_price = bars[-1]['close'] if bars else BASE_PRICE
    pct_change = ((end_price / start_price - 1) * 100) if start_price > 0 else 0

    print(f"  ✓ Generated {len(bars)} bars, price: ${start_price:.2f} → ${end_price:.2f} ({pct_change:+.1f}%)")

# Convert to DataFrame
df = pd.DataFrame(all_data)

# Save as CSV in sentio format
output_file = "data/equities/SPY_regime_test.csv"
print()
print(f"Saving to {output_file}...")

# Format timestamp as required
df['timestamp_ms'] = (df['timestamp'].astype(np.int64) // 1e6).astype(int)
df['date'] = df['timestamp'].dt.strftime('%Y-%m-%d')
df['time'] = df['timestamp'].dt.strftime('%H:%M:%S')

# Select and order columns
output_df = df[['timestamp_ms', 'date', 'time', 'open', 'high', 'low', 'close', 'volume']]

# Save without headers (sentio format)
output_df.to_csv(output_file, index=False, header=False)

# Also save with labels for validation
labeled_output = "data/tmp/spy_regime_test_labeled.csv"
df.to_csv(labeled_output, index=False)

print(f"✓ Saved {len(df)} bars to {output_file}")
print(f"✓ Saved labeled version to {labeled_output}")
print()

# Print summary
print("=" * 80)
print("GENERATION SUMMARY")
print("=" * 80)
print()
print(f"Total bars: {len(df)}")
print(f"Total blocks: {len(df) / BARS_PER_BLOCK:.1f}")
print()
print("Regime breakdown:")
for regime_name, _ in REGIMES:
    regime_data = df[df['regime'] == regime_name]
    blocks = len(regime_data) / BARS_PER_BLOCK
    print(f"  {regime_name:20s}: {len(regime_data):4d} bars ({blocks:.1f} blocks)")

print()
print("Price range:")
print(f"  Start: ${df['close'].iloc[0]:.2f}")
print(f"  End:   ${df['close'].iloc[-1]:.2f}")
print(f"  Min:   ${df['low'].min():.2f}")
print(f"  Max:   ${df['high'].max():.2f}")
print()

print("=" * 80)
print("NEXT STEPS")
print("=" * 80)
print()
print("1. Build regime test program:")
print("   cmake --build build --target test_regime_detector")
print()
print("2. Run regime validation:")
print("   ./build/test_regime_detector data/equities/SPY_regime_test.csv")
print()
