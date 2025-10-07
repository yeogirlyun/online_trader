#!/usr/bin/env python3
"""
Generate Realistic Live Test Data for Alpaca Paper Trading
===========================================================

This script generates realistic 1-block (480 bars) market data for live trading simulation.
It uses the last bar from warmup data as the starting point and generates realistic
continuation based on SPY's historical volatility and market characteristics.

Features:
- Realistic intraday volatility patterns
- Proper OHLCV relationship
- Volume modeling based on time of day
- Maintains market microstructure
"""

import pandas as pd
import numpy as np
import csv
import sys
from datetime import datetime, timedelta

def generate_realistic_live_data(warmup_file, output_file, num_bars=480):
    """
    Generate realistic live test data continuing from warmup data.

    Args:
        warmup_file: Path to warmup CSV file
        output_file: Path to output CSV file
        num_bars: Number of bars to generate (default: 480 = 1 block)
    """
    print(f"📊 Generating {num_bars} bars of realistic live test data...")

    # Read warmup data to get last bar
    warmup_df = pd.read_csv(warmup_file)
    last_bar = warmup_df.iloc[-1]

    # Extract starting values
    last_close = last_bar['close']
    # Handle both 'timestamp' and 'ts_nyt_epoch' column names
    if 'timestamp' in last_bar:
        last_timestamp = last_bar['timestamp']
    elif 'ts_nyt_epoch' in last_bar:
        last_timestamp = int(last_bar['ts_nyt_epoch'])
    else:
        raise ValueError("No timestamp column found (expected 'timestamp' or 'ts_nyt_epoch')")

    last_volume = last_bar['volume']

    print(f"Starting from: Close={last_close:.2f}, Timestamp={last_timestamp}, Volume={last_volume}")

    # Calculate historical volatility from warmup data
    warmup_df['returns'] = warmup_df['close'].pct_change()
    historical_vol = warmup_df['returns'].std()

    print(f"Historical volatility: {historical_vol:.4f} ({historical_vol*100:.2f}%)")

    # Market regime parameters (calibrated to SPY)
    base_vol = historical_vol if not np.isnan(historical_vol) else 0.008  # 0.8% per bar
    trend_drift = 0.0001  # Slight positive drift

    # Intraday patterns
    # Morning: higher volatility (9:30-11:00)
    # Midday: lower volatility (11:00-14:00)
    # Afternoon: moderate volatility (14:00-15:30)
    # Power hour: higher volatility (15:30-16:00)

    bars = []
    current_price = last_close
    current_timestamp = last_timestamp + 60  # 1 minute later

    for i in range(num_bars):
        # Time of day effect (assuming 390 bars per trading day)
        bar_of_day = i % 390

        # Volatility multiplier based on time of day
        if bar_of_day < 90:  # First 90 minutes (9:30-11:00)
            vol_multiplier = 1.5
        elif bar_of_day < 270:  # Midday (11:00-14:00)
            vol_multiplier = 0.8
        elif bar_of_day < 360:  # Afternoon (14:00-15:30)
            vol_multiplier = 1.0
        else:  # Power hour (15:30-16:00)
            vol_multiplier = 1.8

        # Generate price movement
        price_vol = base_vol * vol_multiplier
        price_change = np.random.normal(trend_drift, price_vol)

        # Update price
        new_close = current_price * (1 + price_change)

        # Generate OHLC with realistic intrabar movement
        intrabar_vol = price_vol * 0.3  # Intrabar volatility is smaller

        # Open near previous close
        open_price = current_price * (1 + np.random.normal(0, intrabar_vol * 0.2))

        # High and low based on close
        high_offset = abs(np.random.normal(0, intrabar_vol))
        low_offset = abs(np.random.normal(0, intrabar_vol))

        high_price = max(open_price, new_close) * (1 + high_offset)
        low_price = min(open_price, new_close) * (1 - low_offset)

        # Ensure OHLC consistency
        high_price = max(high_price, open_price, new_close)
        low_price = min(low_price, open_price, new_close)

        # Generate volume based on time of day
        if bar_of_day < 30:  # First 30 minutes: high volume
            volume_multiplier = 2.0
        elif bar_of_day < 90:  # Rest of morning: moderate-high volume
            volume_multiplier = 1.3
        elif bar_of_day < 270:  # Midday: low volume
            volume_multiplier = 0.7
        elif bar_of_day < 360:  # Afternoon: moderate volume
            volume_multiplier = 1.0
        else:  # Power hour: high volume
            volume_multiplier = 2.5

        base_volume = last_volume if last_volume > 0 else 150000
        volume = int(base_volume * volume_multiplier * (1 + np.random.normal(0, 0.2)))
        volume = max(volume, 50000)  # Minimum volume

        # Create bar
        bar = {
            'timestamp': int(current_timestamp),
            'open': round(open_price, 2),
            'high': round(high_price, 2),
            'low': round(low_price, 2),
            'close': round(new_close, 2),
            'volume': volume
        }

        bars.append(bar)

        # Update for next iteration
        current_price = new_close
        current_timestamp += 60  # 1 minute

        # Progress reporting
        if (i + 1) % 100 == 0:
            progress = (i + 1) / num_bars * 100
            print(f"  Progress: {progress:.0f}% ({i + 1}/{num_bars} bars)")

    # Write to CSV in the same format as input
    with open(output_file, 'w', newline='') as f:
        # Use ts_nyt_epoch to match input format
        fieldnames = ['ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume']
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        # Rename timestamp to ts_nyt_epoch for output
        output_bars = []
        for bar in bars:
            output_bar = bar.copy()
            output_bar['ts_nyt_epoch'] = output_bar.pop('timestamp')
            output_bars.append(output_bar)

        writer.writerows(output_bars)

    print(f"✅ Generated {len(bars)} bars")
    print(f"   Output: {output_file}")

    # Calculate statistics
    closes = [bar['close'] for bar in bars]
    volumes = [bar['volume'] for bar in bars]

    total_return = (closes[-1] - closes[0]) / closes[0] * 100
    max_price = max(closes)
    min_price = min(closes)
    avg_volume = np.mean(volumes)

    print(f"\n📈 Statistics:")
    print(f"   Starting price: ${closes[0]:.2f}")
    print(f"   Ending price: ${closes[-1]:.2f}")
    print(f"   Total return: {total_return:+.2f}%")
    print(f"   Price range: ${min_price:.2f} - ${max_price:.2f}")
    print(f"   Average volume: {avg_volume:,.0f}")

    return bars

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 generate_live_test_data.py <warmup_file> <output_file> [num_bars]")
        sys.exit(1)

    warmup_file = sys.argv[1]
    output_file = sys.argv[2]
    num_bars = int(sys.argv[3]) if len(sys.argv) > 3 else 480

    generate_realistic_live_data(warmup_file, output_file, num_bars)
