#!/usr/bin/env python3
"""
Extract trading session data by date for mock testing.

Usage:
    python3 tools/extract_session_data.py [--date YYYY-MM-DD] [--output-warmup FILE] [--output-session FILE]

If no date specified, uses the most recent trading day in the data.
"""

import sys
import argparse
from datetime import datetime, timedelta
import pandas as pd

def extract_session_data(input_file, target_date=None, output_warmup=None, output_session=None):
    """
    Extract warmup and session data for a specific trading date.

    Args:
        input_file: Path to SPY_RTH_NH.csv
        target_date: Target session date (YYYY-MM-DD). If None, uses most recent.
        output_warmup: Output file for warmup data (all data before target date)
        output_session: Output file for session data (391 bars for target date)

    Returns:
        tuple: (warmup_file, session_file, target_date_str)
    """

    # Read data
    print(f"📖 Reading data from {input_file}...")
    df = pd.read_csv(input_file)

    # Parse timestamp column (first column)
    timestamp_col = df.columns[0]
    df['datetime'] = pd.to_datetime(df[timestamp_col])
    df['date'] = df['datetime'].dt.date

    # Find available trading dates
    available_dates = sorted(df['date'].unique())
    print(f"📅 Available trading dates: {len(available_dates)} days")
    print(f"   First: {available_dates[0]}")
    print(f"   Last: {available_dates[-1]}")

    # Determine target date
    if target_date:
        target_date_obj = datetime.strptime(target_date, '%Y-%m-%d').date()
        if target_date_obj not in available_dates:
            print(f"❌ ERROR: Date {target_date} not found in data")
            print(f"   Available dates: {[str(d) for d in available_dates[-5:]]}")
            sys.exit(1)
    else:
        # Use most recent date
        target_date_obj = available_dates[-1]
        target_date = str(target_date_obj)
        print(f"✓ Using most recent date: {target_date}")

    # Extract warmup data (all bars BEFORE target date)
    warmup_df = df[df['date'] < target_date_obj].copy()
    warmup_bars = len(warmup_df)

    # Extract session data (all bars ON target date)
    session_df = df[df['date'] == target_date_obj].copy()
    session_bars = len(session_df)

    print(f"\n📊 Data Split:")
    print(f"   Warmup: {warmup_bars} bars (before {target_date})")
    print(f"   Session: {session_bars} bars (on {target_date})")

    # Verify session has 391 bars (full trading day)
    if session_bars != 391:
        print(f"⚠️  WARNING: Session has {session_bars} bars (expected 391 for full day)")

    # Drop helper columns
    warmup_df = warmup_df.drop(['datetime', 'date'], axis=1)
    session_df = session_df.drop(['datetime', 'date'], axis=1)

    # Save files with headers (required for dashboard script)
    if output_warmup:
        warmup_df.to_csv(output_warmup, index=False, header=True)
        print(f"✓ Warmup saved: {output_warmup} ({warmup_bars} bars)")

    if output_session:
        session_df.to_csv(output_session, index=False, header=True)
        print(f"✓ Session saved: {output_session} ({session_bars} bars)")

    return output_warmup, output_session, target_date


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Extract session data by date for mock testing')
    parser.add_argument('--input', default='data/equities/SPY_RTH_NH.csv',
                       help='Input CSV file (default: SPY_RTH_NH.csv)')
    parser.add_argument('--date', help='Target session date (YYYY-MM-DD). If not specified, uses most recent.')
    parser.add_argument('--output-warmup', default='data/equities/SPY_warmup_latest.csv',
                       help='Output warmup file (default: SPY_warmup_latest.csv)')
    parser.add_argument('--output-session', default='/tmp/SPY_session.csv',
                       help='Output session file (default: /tmp/SPY_session.csv)')

    args = parser.parse_args()

    warmup, session, date = extract_session_data(
        args.input,
        args.date,
        args.output_warmup,
        args.output_session
    )

    print(f"\n✅ Extraction complete for {date}")
    print(f"   Use these files for mock testing to replicate {date} session")
