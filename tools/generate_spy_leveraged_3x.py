#!/usr/bin/env python3
"""
Generate SPXS (-3x short) data from SPY.

This creates SPXS (Direxion Daily S&P 500 Bear 3X) to match QQQ's -3x SQQQ.
"""

import csv
import sys
import argparse

def generate_spxs_data(spy_file, output_dir):
    """Generate SPXS (-3x short) data from SPY."""

    # Read SPY data
    spy_bars = []
    with open(spy_file) as f:
        reader = csv.DictReader(f)
        for row in reader:
            spy_bars.append({
                'ts_utc': row['ts_utc'],
                'ts_nyt_epoch': row['ts_nyt_epoch'],
                'open': float(row['open']),
                'high': float(row['high']),
                'low': float(row['low']),
                'close': float(row['close']),
                'volume': float(row['volume'])
            })

    print(f"Loaded {len(spy_bars)} SPY bars")

    # Initialize starting price for SPXS
    spy_start = spy_bars[0]['close']
    spxs_start = 50.0  # Start SPXS at $50 (similar to SDS starting point)

    # Generate SPXS (-3x inverse) data
    spxs_bars = []
    spxs_prev_close = spxs_start
    spy_prev_close = spy_start

    for i, spy_bar in enumerate(spy_bars):
        # Calculate SPY returns for this bar
        spy_open_ret = (spy_bar['open'] - spy_prev_close) / spy_prev_close
        spy_high_ret = (spy_bar['high'] - spy_prev_close) / spy_prev_close
        spy_low_ret = (spy_bar['low'] - spy_prev_close) / spy_prev_close
        spy_close_ret = (spy_bar['close'] - spy_prev_close) / spy_prev_close

        # Apply -3x leverage to returns
        leverage = -3.0

        open_price = spxs_prev_close * (1 + spy_open_ret * leverage)
        high_price = spxs_prev_close * (1 + spy_high_ret * leverage)
        low_price = spxs_prev_close * (1 + spy_low_ret * leverage)
        close_price = spxs_prev_close * (1 + spy_close_ret * leverage)

        # Ensure high >= low
        if high_price < low_price:
            high_price, low_price = low_price, high_price

        # Ensure open/close are within high/low
        open_price = max(low_price, min(high_price, open_price))
        close_price = max(low_price, min(high_price, close_price))

        spxs_bars.append({
            'ts_utc': spy_bar['ts_utc'],
            'ts_nyt_epoch': spy_bar['ts_nyt_epoch'],
            'open': open_price,
            'high': high_price,
            'low': low_price,
            'close': close_price,
            'volume': spy_bar['volume']  # Use same volume as SPY
        })

        spxs_prev_close = close_price
        spy_prev_close = spy_bar['close']

        if (i + 1) % 50000 == 0:
            print(f"  Processed {i + 1}/{len(spy_bars)} bars...")

    # Write output file
    output_file = f"{output_dir}/SPXS_RTH_NH.csv"
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume'])

        for bar in spxs_bars:
            writer.writerow([
                bar['ts_utc'],
                bar['ts_nyt_epoch'],
                f"{bar['open']:.4f}",
                f"{bar['high']:.4f}",
                f"{bar['low']:.4f}",
                f"{bar['close']:.4f}",
                f"{bar['volume']:.1f}"
            ])

    print(f"✅ Generated {output_file} ({len(spxs_bars)} bars)")
    print(f"\n🎉 Successfully generated SPXS (-3x short) data")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate SPXS (-3x short) data from SPY')
    parser.add_argument('--spy', default='data/equities/SPY_RTH_NH.csv',
                       help='Path to SPY data file')
    parser.add_argument('--output-dir', default='data/equities',
                       help='Output directory for generated file')

    args = parser.parse_args()

    generate_spxs_data(args.spy, args.output_dir)
