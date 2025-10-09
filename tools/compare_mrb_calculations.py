#!/usr/bin/env python3
"""
Compare Block-Based MRB vs. Trading-Day-Based MRB

This script calculates MRB two ways:
1. Method A: Block-based (current approach) - 390-bar chunks from arbitrary start
2. Method B: Trading-day-based - 9:30 AM to 3:58 PM segments only

If they differ significantly, the expert's concern is validated.

Author: Claude Code
Date: 2025-10-09
"""

import json
import sys
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Tuple
import pandas as pd


def load_trades(trades_file: str) -> pd.DataFrame:
    """Load trades from JSONL into DataFrame"""
    trades = []
    with open(trades_file, 'r') as f:
        for line in f:
            if line.strip():
                trade = json.loads(line)
                trade['datetime'] = datetime.fromtimestamp(trade['timestamp_ms'] / 1000)
                trades.append(trade)

    df = pd.DataFrame(trades)
    df['date'] = df['datetime'].dt.date
    df['time'] = df['datetime'].dt.time
    df['hour'] = df['datetime'].dt.hour
    df['minute'] = df['datetime'].dt.minute

    return df


def calculate_block_based_mrb(trades_df: pd.DataFrame, bars_per_block: int = 391) -> Dict:
    """
    Calculate MRB using block-based method (current approach).

    MRB = total_return / num_blocks
    """
    if trades_df.empty:
        return {'method': 'block-based', 'mrb': 0.0, 'error': 'no trades'}

    # Get initial and final portfolio values
    initial_capital = 100000.0  # Standard starting capital
    final_value = trades_df['portfolio_value'].iloc[-1]

    total_return_pct = ((final_value - initial_capital) / initial_capital) * 100.0

    # Estimate number of blocks from bar indices
    max_bar_index = trades_df['bar_index'].max()
    num_blocks = (max_bar_index + 1) / bars_per_block

    mrb = total_return_pct / num_blocks if num_blocks > 0 else 0.0

    return {
        'method': 'block-based',
        'mrb': mrb,
        'total_return_pct': total_return_pct,
        'num_blocks': num_blocks,
        'initial_capital': initial_capital,
        'final_value': final_value
    }


def calculate_daily_mrb(trades_df: pd.DataFrame) -> Dict:
    """
    Calculate MRB using trading-day-based method.

    For each trading day:
      - Start equity = end of previous day (or initial capital)
      - End equity = portfolio value at last trade of day
      - Daily return = (end - start) / start

    MRB = mean(daily_returns)
    """
    if trades_df.empty:
        return {'method': 'trading-day', 'mrb': 0.0, 'error': 'no trades'}

    initial_capital = 100000.0

    # Group by trading day
    daily_returns = []
    daily_details = []

    prev_end_value = initial_capital

    for date, day_trades in trades_df.groupby('date'):
        # Get final portfolio value of the day
        day_end_value = day_trades['portfolio_value'].iloc[-1]

        # Calculate daily return
        daily_return_pct = ((day_end_value - prev_end_value) / prev_end_value) * 100.0
        daily_returns.append(daily_return_pct)

        daily_details.append({
            'date': str(date),
            'start_value': prev_end_value,
            'end_value': day_end_value,
            'return_pct': daily_return_pct,
            'num_trades': len(day_trades)
        })

        # Update for next day
        prev_end_value = day_end_value

    # MRB = mean daily return
    mrb = sum(daily_returns) / len(daily_returns) if daily_returns else 0.0

    total_return_pct = ((prev_end_value - initial_capital) / initial_capital) * 100.0

    return {
        'method': 'trading-day',
        'mrb': mrb,
        'total_return_pct': total_return_pct,
        'num_days': len(daily_returns),
        'daily_returns': daily_returns,
        'daily_details': daily_details,
        'initial_capital': initial_capital,
        'final_value': prev_end_value
    }


def main():
    print("="*80)
    print("BLOCK-BASED MRB vs. TRADING-DAY-BASED MRB COMPARISON")
    print("="*80)
    print()

    # Find latest trades file
    trades_dir = Path(__file__).parent.parent / "data" / "tmp"
    trades_files = list(trades_dir.glob("*trades*.jsonl"))

    if not trades_files:
        print("❌ No trades files found in data/tmp/")
        return 1

    # Use most recent
    trades_file = max(trades_files, key=lambda p: p.stat().st_mtime)
    print(f"📄 Analyzing: {trades_file.name}")
    print()

    # Load trades
    trades_df = load_trades(str(trades_file))
    print(f"✓ Loaded {len(trades_df)} trades")
    print(f"  Date range: {trades_df['date'].min()} → {trades_df['date'].max()}")
    print()

    # Method A: Block-based MRB
    print("="*80)
    print("METHOD A: BLOCK-BASED MRB (Current Approach)")
    print("="*80)

    block_result = calculate_block_based_mrb(trades_df)

    print(f"Initial Capital:   ${block_result['initial_capital']:,.2f}")
    print(f"Final Value:       ${block_result['final_value']:,.2f}")
    print(f"Total Return:      {block_result['total_return_pct']:+.4f}%")
    print(f"Number of Blocks:  {block_result['num_blocks']:.2f}")
    print(f"MRB (per block):   {block_result['mrb']:+.4f}%")
    print()

    # Method B: Trading-day-based MRB
    print("="*80)
    print("METHOD B: TRADING-DAY-BASED MRB (True Daily Returns)")
    print("="*80)

    daily_result = calculate_daily_mrb(trades_df)

    print(f"Initial Capital:   ${daily_result['initial_capital']:,.2f}")
    print(f"Final Value:       ${daily_result['final_value']:,.2f}")
    print(f"Total Return:      {daily_result['total_return_pct']:+.4f}%")
    print(f"Number of Days:    {daily_result['num_days']}")
    print(f"MRB (per day):     {daily_result['mrb']:+.4f}%")
    print()

    print("Daily Returns:")
    for detail in daily_result['daily_details']:
        print(f"  {detail['date']}: {detail['return_pct']:+6.2f}% "
              f"(${detail['start_value']:,.0f} → ${detail['end_value']:,.0f})")
    print()

    # Comparison
    print("="*80)
    print("COMPARISON & ANALYSIS")
    print("="*80)

    mrb_diff = block_result['mrb'] - daily_result['mrb']
    mrb_diff_pct = (mrb_diff / abs(daily_result['mrb'])) * 100 if daily_result['mrb'] != 0 else 0

    print(f"Block-based MRB:   {block_result['mrb']:+.4f}%")
    print(f"Daily MRB:         {daily_result['mrb']:+.4f}%")
    print(f"Difference:        {mrb_diff:+.4f}% ({mrb_diff_pct:+.1f}% relative)")
    print()

    # Verdict
    if abs(mrb_diff_pct) < 5:
        print("✅ VERDICT: Difference is negligible (<5%)")
        print("   Block-based MRB is a reasonable approximation of daily MRB")
        print("   Current optimization approach is likely fine")
    elif abs(mrb_diff_pct) < 15:
        print("⚠️  VERDICT: Moderate difference (5-15%)")
        print("   Block-based MRB differs from daily MRB")
        print("   Consider switching to daily-based calculation for more accuracy")
    else:
        print("❌ VERDICT: Significant difference (>15%)")
        print("   Block-based MRB substantially differs from daily MRB")
        print("   Expert's concern is VALIDATED - optimization may be misaligned")
        print()
        print("   Recommendations:")
        print("   1. Use daily-based MRB for optimization")
        print("   2. Ensure data starts at market open (9:30 AM)")
        print("   3. Re-optimize parameters with correct metric")

    print()
    print("="*80)
    print("TECHNICAL EXPLANATION")
    print("="*80)
    print()
    print("Why they differ:")
    print("- Block-based: Divides total return by fractional number of 390-bar blocks")
    print("- Daily-based: Averages actual per-day returns")
    print()
    print("Example:")
    print("  If data has 4.88 blocks but 15 trading days:")
    print("  - Block MRB = total_return / 4.88")
    print("  - Daily MRB = mean([day1_return, day2_return, ..., day15_return])")
    print()
    print("These are mathematically different when blocks don't align with days.")
    print()

    # Annualized projections
    print("="*80)
    print("ANNUALIZED PROJECTIONS (252 trading days)")
    print("="*80)

    block_annual = block_result['mrb'] * 252
    daily_annual = daily_result['mrb'] * 252

    print(f"Block-based MRB → Annual: {block_annual:+.2f}%")
    print(f"Daily MRB → Annual:       {daily_annual:+.2f}%")
    print(f"Difference in annual:     {block_annual - daily_annual:+.2f}%")
    print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
