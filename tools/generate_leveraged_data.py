#!/usr/bin/env python3
"""
Generate synthetic leveraged/inverse ETF data from QQQ data.

This creates TQQQ (3x), PSQ (-1x), and SQQQ (-3x) data by applying
leverage multipliers to QQQ returns.
"""

import csv
import sys
import argparse

def generate_leveraged_data(qqq_file, output_dir):
    """Generate TQQQ, PSQ, SQQQ data from QQQ."""

    # Read QQQ data
    qqq_bars = []
    with open(qqq_file) as f:
        reader = csv.DictReader(f)
        for row in reader:
            qqq_bars.append({
                'ts_utc': row['ts_utc'],
                'ts_nyt_epoch': row['ts_nyt_epoch'],
                'open': float(row['open']),
                'high': float(row['high']),
                'low': float(row['low']),
                'close': float(row['close']),
                'volume': float(row['volume'])
            })

    print(f"Loaded {len(qqq_bars)} QQQ bars")

    # Initialize starting prices (using first QQQ bar as reference)
    # These are arbitrary starting values - what matters is the returns
    qqq_start = qqq_bars[0]['close']
    tqqq_start = 100.0  # Start TQQQ at $100
    psq_start = 50.0    # Start PSQ at $50
    sqqq_start = 50.0   # Start SQQQ at $50

    # Generate leveraged/inverse data
    instruments = {
        'TQQQ': {'leverage': 3.0, 'prev_close': tqqq_start, 'bars': []},
        'PSQ': {'leverage': -1.0, 'prev_close': psq_start, 'bars': []},
        'SQQQ': {'leverage': -3.0, 'prev_close': sqqq_start, 'bars': []}
    }

    qqq_prev_close = qqq_start

    for i, qqq_bar in enumerate(qqq_bars):
        # Calculate QQQ returns for this bar
        qqq_open_ret = (qqq_bar['open'] - qqq_prev_close) / qqq_prev_close
        qqq_high_ret = (qqq_bar['high'] - qqq_prev_close) / qqq_prev_close
        qqq_low_ret = (qqq_bar['low'] - qqq_prev_close) / qqq_prev_close
        qqq_close_ret = (qqq_bar['close'] - qqq_prev_close) / qqq_prev_close

        # For each leveraged instrument
        for symbol, inst in instruments.items():
            leverage = inst['leverage']
            prev_close = inst['prev_close']

            # Apply leverage to returns
            open_price = prev_close * (1 + qqq_open_ret * leverage)
            high_price = prev_close * (1 + qqq_high_ret * leverage)
            low_price = prev_close * (1 + qqq_low_ret * leverage)
            close_price = prev_close * (1 + qqq_close_ret * leverage)

            # Ensure high >= low
            if high_price < low_price:
                high_price, low_price = low_price, high_price

            # Ensure open/close are within high/low
            open_price = max(low_price, min(high_price, open_price))
            close_price = max(low_price, min(high_price, close_price))

            inst['bars'].append({
                'ts_utc': qqq_bar['ts_utc'],
                'ts_nyt_epoch': qqq_bar['ts_nyt_epoch'],
                'open': open_price,
                'high': high_price,
                'low': low_price,
                'close': close_price,
                'volume': qqq_bar['volume']  # Use same volume as QQQ
            })

            inst['prev_close'] = close_price

        qqq_prev_close = qqq_bar['close']

        if (i + 1) % 50000 == 0:
            print(f"  Processed {i + 1}/{len(qqq_bars)} bars...")

    # Write output files
    for symbol, inst in instruments.items():
        output_file = f"{output_dir}/{symbol}_RTH_NH.csv"
        with open(output_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume'])

            for bar in inst['bars']:
                writer.writerow([
                    bar['ts_utc'],
                    bar['ts_nyt_epoch'],
                    f"{bar['open']:.4f}",
                    f"{bar['high']:.4f}",
                    f"{bar['low']:.4f}",
                    f"{bar['close']:.4f}",
                    f"{bar['volume']:.1f}"
                ])

        print(f"✅ Generated {output_file} ({len(inst['bars'])} bars)")

    print(f"\n🎉 Successfully generated leveraged data for {len(instruments)} instruments")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate leveraged ETF data from QQQ')
    parser.add_argument('--qqq', default='data/equities/QQQ_RTH_NH.csv',
                       help='Path to QQQ data file')
    parser.add_argument('--output-dir', default='data/equities',
                       help='Output directory for generated files')

    args = parser.parse_args()

    generate_leveraged_data(args.qqq, args.output_dir)
