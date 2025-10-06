#!/usr/bin/env python3
"""
Generate synthetic leveraged/inverse ETF data from SPY data.

This creates:
- SPXL (3x leveraged bull)
- SH (-1x inverse)
- SDS (-2x inverse)
"""

import csv
import sys
import argparse

def generate_leveraged_data(spy_file, output_dir):
    """Generate SPXL, SH, SDS data from SPY."""

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

    # Initialize starting prices (using first SPY bar as reference)
    spy_start = spy_bars[0]['close']
    spxl_start = 100.0  # Start SPXL at $100
    sh_start = 50.0     # Start SH at $50
    sds_start = 50.0    # Start SDS at $50

    # Generate leveraged/inverse data
    instruments = {
        'SPXL': {'leverage': 3.0, 'prev_close': spxl_start, 'bars': []},
        'SH': {'leverage': -1.0, 'prev_close': sh_start, 'bars': []},
        'SDS': {'leverage': -2.0, 'prev_close': sds_start, 'bars': []}
    }

    spy_prev_close = spy_start

    for i, spy_bar in enumerate(spy_bars):
        # Calculate SPY returns for this bar
        spy_open_ret = (spy_bar['open'] - spy_prev_close) / spy_prev_close
        spy_high_ret = (spy_bar['high'] - spy_prev_close) / spy_prev_close
        spy_low_ret = (spy_bar['low'] - spy_prev_close) / spy_prev_close
        spy_close_ret = (spy_bar['close'] - spy_prev_close) / spy_prev_close

        # For each leveraged instrument
        for symbol, inst in instruments.items():
            leverage = inst['leverage']
            prev_close = inst['prev_close']

            # Apply leverage to returns
            open_price = prev_close * (1 + spy_open_ret * leverage)
            high_price = prev_close * (1 + spy_high_ret * leverage)
            low_price = prev_close * (1 + spy_low_ret * leverage)
            close_price = prev_close * (1 + spy_close_ret * leverage)

            # Ensure high >= low
            if high_price < low_price:
                high_price, low_price = low_price, high_price

            # Ensure open/close are within high/low
            open_price = max(low_price, min(high_price, open_price))
            close_price = max(low_price, min(high_price, close_price))

            inst['bars'].append({
                'ts_utc': spy_bar['ts_utc'],
                'ts_nyt_epoch': spy_bar['ts_nyt_epoch'],
                'open': open_price,
                'high': high_price,
                'low': low_price,
                'close': close_price,
                'volume': spy_bar['volume']  # Use same volume as SPY
            })

            inst['prev_close'] = close_price

        spy_prev_close = spy_bar['close']

        if (i + 1) % 50000 == 0:
            print(f"  Processed {i + 1}/{len(spy_bars)} bars...")

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
    parser = argparse.ArgumentParser(description='Generate leveraged ETF data from SPY')
    parser.add_argument('--spy', default='data/equities/SPY_RTH_NH.csv',
                       help='Path to SPY data file')
    parser.add_argument('--output-dir', default='data/equities',
                       help='Output directory for generated files')

    args = parser.parse_args()

    generate_leveraged_data(args.spy, args.output_dir)
