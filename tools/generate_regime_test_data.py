#!/usr/bin/env python3
"""
Generate Multi-Regime SPY Test Data using MarS

This script generates synthetic SPY data with controlled market regimes using MarS.
The data will be used to validate our MarketRegimeDetector implementation.

Generated regimes:
1. TRENDING_UP: Strong upward momentum (bull market)
2. TRENDING_DOWN: Strong downward momentum (bear market)
3. CHOPPY: Sideways movement (range-bound)
4. HIGH_VOLATILITY: Elevated volatility
5. LOW_VOLATILITY: Calm market

Output: Multi-block CSV file with labeled regime segments
"""

import sys
import os
import pandas as pd
import numpy as np
from pathlib import Path
from datetime import datetime, timedelta

# Add MarS path
sys.path.insert(0, str(Path(__file__).parent.parent / "quote_simulation"))
from tools.online_quote_simulator import OnlineQuoteSimulator

print("=" * 80)
print("MULTI-REGIME SPY TEST DATA GENERATOR")
print("=" * 80)
print()

# Configuration
BASE_PRICE = 450.0
BARS_PER_BLOCK = 480  # 1 trading day (6.5 hours * 60 min / 1 min bars)
BLOCKS_PER_REGIME = 2  # 2 blocks per regime = 960 bars
REGIMES = [
    ("trending_up", "TRENDING_UP"),      # Bull market
    ("trending_down", "TRENDING_DOWN"),  # Bear market
    ("sideways", "CHOPPY"),              # Range-bound
    ("volatile", "HIGH_VOLATILITY"),     # High volatility
    ("normal", "LOW_VOLATILITY")         # Calm market
]

simulator = OnlineQuoteSimulator()

all_data = []
current_timestamp = datetime(2024, 1, 1, 9, 30, 0)  # Start at market open

print(f"Generating {len(REGIMES)} regimes × {BLOCKS_PER_REGIME} blocks × {BARS_PER_BLOCK} bars/block")
print(f"Total bars: {len(REGIMES) * BLOCKS_PER_REGIME * BARS_PER_BLOCK}")
print()

for mars_regime, our_regime in REGIMES:
    print(f"Generating {our_regime} regime ({mars_regime})...")
    print(f"  Duration: {BLOCKS_PER_REGIME} blocks ({BLOCKS_PER_REGIME * BARS_PER_BLOCK} bars)")

    # Generate data for this regime
    duration_minutes = BLOCKS_PER_REGIME * BARS_PER_BLOCK

    # FIXED: Reset price to BASE_PRICE for each regime to avoid compounding
    price = BASE_PRICE

    # FIXED: Realistic regime-specific parameters
    # Target: ~20-50% price movement for trending regimes over 960 bars
    if mars_regime == "trending_up":
        drift = 0.0001  # Base drift
        volatility = 0.008
        trend_strength = 0.05  # Adds ~0.0004 directional component
    elif mars_regime == "trending_down":
        drift = 0.0001  # Base drift (same as up to keep magnitude consistent)
        volatility = 0.008
        trend_strength = 0.05  # Direction handled separately
    elif mars_regime == "sideways":
        drift = 0.0
        volatility = 0.005  # Moderate volatility
        trend_strength = 0.0
    elif mars_regime == "volatile":
        drift = 0.0
        volatility = 0.018  # High volatility
        trend_strength = 0.0
    else:  # normal (low volatility)
        drift = 0.0
        volatility = 0.003  # Very low volatility
        trend_strength = 0.0

    # Generate bars
    bars = []
    timestamp = current_timestamp

    for i in range(duration_minutes):
        # Generate base returns
        base_returns = np.random.normal(0, volatility)

        # Add drift and trend strength for trending regimes
        if mars_regime in ["trending_up", "trending_down"]:
            # Add consistent directional component
            direction = 1 if mars_regime == "trending_up" else -1
            trend_component = trend_strength * volatility * direction
            returns = drift + base_returns + trend_component
        else:
            returns = drift + base_returns

        price = price * (1 + returns)

        # Generate OHLCV
        high = price * (1 + abs(np.random.normal(0, volatility/2)))
        low = price * (1 - abs(np.random.normal(0, volatility/2)))
        close = price
        volume = int(np.random.uniform(1e6, 5e6))

        bars.append({
            'timestamp': timestamp,
            'open': price,
            'high': max(high, price),
            'low': min(low, price),
            'close': close,
            'volume': volume,
            'regime': our_regime,  # Label for validation
            'mars_regime': mars_regime
        })

        timestamp += timedelta(minutes=1)

    all_data.extend(bars)
    current_timestamp = timestamp
    end_price = price

    print(f"  ✓ Generated {len(bars)} bars, price: {BASE_PRICE:.2f} → {end_price:.2f} ({((end_price/BASE_PRICE - 1) * 100):.1f}%)")

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
print(f"Duration: {len(df) / (60 * 6.5):.1f} trading days")
print()
print("Regime breakdown:")
for mars_regime, our_regime in REGIMES:
    regime_data = df[df['regime'] == our_regime]
    blocks = len(regime_data) / BARS_PER_BLOCK
    print(f"  {our_regime:20s}: {len(regime_data):4d} bars ({blocks:.1f} blocks)")

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
print("3. Compare detected vs expected regimes:")
print("   python3 scripts/validate_regime_detection.py")
print()
