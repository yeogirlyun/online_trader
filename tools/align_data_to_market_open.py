#!/usr/bin/env python3
"""
Align Historical Data to Market Open

This script preprocesses historical data to ensure:
1. Data starts at market open (9:30 AM ET)
2. All days are complete trading sessions (390 bars)
3. Blocks align with trading day boundaries

Usage:
    python3 align_data_to_market_open.py --input data.csv --output data_aligned.csv

Author: Claude Code
Date: 2025-10-09
"""

import sys
import argparse
from pathlib import Path
import pandas as pd
from datetime import datetime, time


def align_to_market_open(df: pd.DataFrame, verbose: bool = True) -> pd.DataFrame:
    """
    Align data to market open (9:30 AM ET).

    Steps:
    1. Parse timestamps
    2. Find first 09:30:00 bar
    3. Trim everything before
    4. Validate complete trading days

    Returns:
        Aligned DataFrame
    """
    if verbose:
        print("="*80)
        print("ALIGNING DATA TO MARKET OPEN")
        print("="*80)
        print()

    # Parse timestamps
    df['timestamp'] = pd.to_datetime(df['ts_utc'])
    df['date'] = df['timestamp'].dt.date
    df['time'] = df['timestamp'].dt.time

    # Original stats
    original_bars = len(df)
    original_first_time = df['time'].iloc[0]
    original_dates = df['date'].nunique()

    if verbose:
        print(f"Original Data:")
        print(f"  Total bars: {original_bars}")
        print(f"  First timestamp: {df['timestamp'].iloc[0]}")
        print(f"  Last timestamp:  {df['timestamp'].iloc[-1]}")
        print(f"  Trading days: {original_dates}")
        print(f"  First bar time: {original_first_time}")
        print()

    # Find first market open (09:30:00)
    market_open = time(9, 30, 0)
    market_open_mask = df['time'] == market_open

    if not market_open_mask.any():
        print("⚠️  WARNING: No 09:30:00 bars found in data")
        print("   Data may already be aligned or use different timestamps")
        return df

    first_open_idx = df[market_open_mask].index[0]

    if verbose:
        print(f"Alignment:")
        print(f"  First 09:30:00 bar found at index: {first_open_idx}")
        print(f"  Trimming {first_open_idx} bars from start")
        print()

    # Trim data
    df_aligned = df.iloc[first_open_idx:].copy()
    df_aligned = df_aligned.reset_index(drop=True)

    # Validate complete days
    if verbose:
        print(f"Aligned Data:")
        print(f"  Total bars: {len(df_aligned)}")
        print(f"  First timestamp: {df_aligned['timestamp'].iloc[0]}")
        print(f"  Last timestamp:  {df_aligned['timestamp'].iloc[-1]}")
        print(f"  First bar time: {df_aligned['time'].iloc[0]}")
        print()

    # Check day completeness
    daily_bars = df_aligned.groupby('date').size()

    if verbose:
        print("Daily Bar Counts:")
        print("-"*80)

        complete_days = 0
        incomplete_days = 0

        for date, count in daily_bars.items():
            status = "✅" if count == 391 else "⚠️ "
            if count == 391:
                complete_days += 1
            else:
                incomplete_days += 1

            if count != 391 or verbose:
                print(f"  {status} {date}: {count} bars" +
                      ("" if count == 391 else f" (expected 391)"))

        print()
        print(f"Summary:")
        print(f"  Complete days (391 bars): {complete_days}")
        print(f"  Incomplete days: {incomplete_days}")
        print()

    # Verify block alignment
    bars_per_block = 391  # Complete trading day (9:30 AM - 4:00 PM inclusive)
    num_blocks = len(df_aligned) // bars_per_block
    blocks_spanning_days = 0

    if verbose:
        print("Block Alignment Check:")
        print("-"*80)

    for block_idx in range(min(5, num_blocks)):
        start_idx = block_idx * bars_per_block
        end_idx = (block_idx + 1) * bars_per_block

        block_dates = df_aligned.iloc[start_idx:end_idx]['date'].unique()

        if len(block_dates) > 1:
            blocks_spanning_days += 1

        if verbose and block_idx < 3:  # Show first 3 blocks
            span_status = "⚠️ " if len(block_dates) > 1 else "✅"
            print(f"  {span_status} Block {block_idx}: ", end="")
            if len(block_dates) == 1:
                print(f"Single day ({block_dates[0]})")
            else:
                print(f"Spans {len(block_dates)} days ({block_dates[0]} → {block_dates[-1]})")

    if verbose:
        print()
        if blocks_spanning_days == 0:
            print("✅ SUCCESS: All blocks align with single trading days")
        else:
            print(f"⚠️  {blocks_spanning_days}/{num_blocks} blocks still span multiple days")
            print("   This can happen if days have variable bar counts")
        print()

    # Drop temporary columns
    df_aligned = df_aligned.drop(columns=['timestamp', 'date', 'time'])

    return df_aligned


def main():
    parser = argparse.ArgumentParser(
        description="Align historical data to market open (9:30 AM ET)"
    )
    parser.add_argument('--input', '-i', required=True,
                        help='Input CSV file')
    parser.add_argument('--output', '-o', required=True,
                        help='Output CSV file (aligned)')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress verbose output')

    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    if not input_path.exists():
        print(f"❌ Error: Input file not found: {input_path}")
        return 1

    # Load data
    if not args.quiet:
        print(f"📄 Loading: {input_path}")
        print()

    df = pd.read_csv(input_path)

    # Align data
    df_aligned = align_to_market_open(df, verbose=not args.quiet)

    # Save aligned data
    df_aligned.to_csv(output_path, index=False)

    if not args.quiet:
        print("="*80)
        print(f"✅ Aligned data saved to: {output_path}")
        print(f"   Original bars: {len(df)}")
        print(f"   Aligned bars:  {len(df_aligned)}")
        print(f"   Trimmed bars:  {len(df) - len(df_aligned)}")
        print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
