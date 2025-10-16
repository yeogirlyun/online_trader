#!/usr/bin/env python3
"""Analyze actual volatility in MarS-generated data"""

import pandas as pd
import numpy as np

# Load labeled data
df = pd.read_csv('data/tmp/spy_regime_test_labeled.csv')

print("=" * 80)
print("MARS DATA VOLATILITY ANALYSIS")
print("=" * 80)
print()

for regime in ['TRENDING_UP', 'TRENDING_DOWN', 'CHOPPY', 'HIGH_VOLATILITY', 'LOW_VOLATILITY']:
    regime_data = df[df['regime'] == regime].copy()

    if len(regime_data) == 0:
        continue

    # Calculate ATR (simple version: high-low range)
    regime_data['range'] = regime_data['high'] - regime_data['low']
    regime_data['atr_simple'] = regime_data['range'].rolling(window=14).mean()

    # Calculate true range
    regime_data['prev_close'] = regime_data['close'].shift(1)
    regime_data['tr'] = regime_data.apply(
        lambda row: max(
            row['high'] - row['low'],
            abs(row['high'] - row['prev_close']) if pd.notna(row['prev_close']) else 0,
            abs(row['low'] - row['prev_close']) if pd.notna(row['prev_close']) else 0
        ),
        axis=1
    )
    regime_data['atr'] = regime_data['tr'].rolling(window=14).mean()

    # Calculate volatility ratio
    regime_data['volatility'] = regime_data['atr'] / regime_data['close']

    # Get statistics (skip NaN from rolling window)
    valid_data = regime_data.dropna()

    print(f"{regime}:")
    print(f"  Price: ${valid_data['close'].mean():.2f} (${valid_data['close'].min():.2f}-${valid_data['close'].max():.2f})")
    print(f"  ATR:   ${valid_data['atr'].mean():.4f} (${valid_data['atr'].min():.4f}-${valid_data['atr'].max():.4f})")
    print(f"  Volatility (ATR/price): {valid_data['volatility'].mean():.6f} ({valid_data['volatility'].min():.6f}-{valid_data['volatility'].max():.6f})")
    print(f"  Bars: {len(regime_data)}")
    print()

print("=" * 80)
print("RECOMMENDED THRESHOLDS")
print("=" * 80)
print()

all_data = df.copy()
all_data['range'] = all_data['high'] - all_data['low']
all_data['prev_close'] = all_data['close'].shift(1)
all_data['tr'] = all_data.apply(
    lambda row: max(
        row['high'] - row['low'],
        abs(row['high'] - row['prev_close']) if pd.notna(row['prev_close']) else 0,
        abs(row['low'] - row['prev_close']) if pd.notna(row['prev_close']) else 0
    ),
    axis=1
)
all_data['atr'] = all_data['tr'].rolling(window=14).mean()
all_data['volatility'] = all_data['atr'] / all_data['close']

valid = all_data.dropna()
percentiles = valid['volatility'].quantile([0.2, 0.5, 0.8]).values

print(f"20th percentile (low volatility):  {percentiles[0]:.6f}")
print(f"50th percentile (medium):          {percentiles[1]:.6f}")
print(f"80th percentile (high volatility): {percentiles[2]:.6f}")
print()
print("Suggested thresholds:")
print(f"  VOLATILITY_LOW_THRESHOLD:  {percentiles[0]:.6f}")
print(f"  VOLATILITY_HIGH_THRESHOLD: {percentiles[2]:.6f}")
print()
