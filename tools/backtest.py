#!/usr/bin/env python3
"""
Backtest Tool - Run end-to-end backtest on last N blocks of SPY data

Usage:
    python tools/backtest.py --blocks 20
    python tools/backtest.py --blocks 100 --data data/equities/SPY_RTH_NH_5years.csv
"""

import subprocess
import sys
import os
import argparse
from pathlib import Path

# Constants
BARS_PER_BLOCK = 480
DEFAULT_DATA = "data/equities/SPY_RTH_NH_5years.csv"
BUILD_DIR = "build"

def count_csv_lines(csv_path):
    """Count lines in CSV file (excluding header)"""
    with open(csv_path, 'r') as f:
        return sum(1 for line in f) - 1  # Subtract header

def extract_last_n_blocks(input_csv, output_csv, num_blocks):
    """Extract last N blocks from CSV file"""
    total_lines = count_csv_lines(input_csv)
    bars_needed = num_blocks * BARS_PER_BLOCK

    print(f"📊 Data Statistics:")
    print(f"   Total bars available: {total_lines:,}")
    print(f"   Blocks requested: {num_blocks}")
    print(f"   Bars needed: {bars_needed:,} ({num_blocks} × {BARS_PER_BLOCK})")

    if bars_needed > total_lines:
        print(f"⚠️  Warning: Requested {bars_needed} bars but only {total_lines} available")
        print(f"   Using all {total_lines} bars ({total_lines // BARS_PER_BLOCK} blocks)")
        bars_needed = total_lines

    # Read header and last N bars
    with open(input_csv, 'r') as fin:
        lines = fin.readlines()
        header = lines[0]
        data_lines = lines[1:]  # Skip header

        # Take last N bars
        selected_lines = data_lines[-bars_needed:]

        # Write to output
        with open(output_csv, 'w') as fout:
            fout.write(header)
            fout.writelines(selected_lines)

    actual_blocks = len(selected_lines) / BARS_PER_BLOCK
    print(f"✅ Extracted {len(selected_lines):,} bars ({actual_blocks:.1f} blocks) to {output_csv}")
    return len(selected_lines)

def run_command(cmd, description):
    """Run shell command and return success status"""
    print(f"\n🔧 {description}...")
    print(f"   Command: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode == 0:
        print(f"✅ {description} completed")
        # Print last few lines of output
        if result.stdout:
            lines = result.stdout.strip().split('\n')
            for line in lines[-10:]:
                print(f"   {line}")
        return True
    else:
        print(f"❌ {description} failed!")
        if result.stderr:
            print(f"   Error: {result.stderr}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Run backtest on last N blocks of data')
    parser.add_argument('--blocks', type=int, required=True, help='Number of blocks to test')
    parser.add_argument('--data', default=DEFAULT_DATA, help='Input CSV file path')
    parser.add_argument('--warmup', type=int, default=100, help='Warmup bars')
    parser.add_argument('--output-dir', default='data/tmp', help='Output directory for results')

    args = parser.parse_args()

    # Validate inputs
    if not os.path.exists(args.data):
        print(f"❌ Data file not found: {args.data}")
        sys.exit(1)

    if not os.path.exists(BUILD_DIR):
        print(f"❌ Build directory not found: {BUILD_DIR}")
        sys.exit(1)

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # File paths
    test_csv = f"{args.output_dir}/backtest_{args.blocks}blocks.csv"
    signals_file = f"{args.output_dir}/backtest_{args.blocks}blocks_signals.jsonl"
    trades_file = f"{args.output_dir}/backtest_{args.blocks}blocks_trades.jsonl"
    analysis_file = f"{args.output_dir}/backtest_{args.blocks}blocks_analysis.txt"

    print("="*70)
    print(f"🎯 BACKTEST - {args.blocks} Blocks")
    print("="*70)

    # Step 1: Extract data
    num_bars = extract_last_n_blocks(args.data, test_csv, args.blocks)

    # Step 2: Generate signals
    if not run_command([
        f"{BUILD_DIR}/sentio_cli",
        "generate-signals",
        "--data", test_csv,
        "--output", signals_file,
        "--warmup", str(args.warmup)
    ], f"Generate signals ({args.blocks} blocks)"):
        sys.exit(1)

    # Step 3: Execute trades
    if not run_command([
        f"{BUILD_DIR}/sentio_cli",
        "execute-trades",
        "--signals", signals_file,
        "--data", test_csv,
        "--output", trades_file,
        "--warmup", str(args.warmup)
    ], f"Execute trades ({args.blocks} blocks)"):
        sys.exit(1)

    # Step 4: Analyze performance
    if not run_command([
        f"{BUILD_DIR}/sentio_cli",
        "analyze-trades",
        "--trades", trades_file,
        "--data", test_csv,
        "--output", analysis_file
    ], f"Analyze performance ({args.blocks} blocks)"):
        sys.exit(1)

    print("\n" + "="*70)
    print(f"✅ BACKTEST COMPLETE - {args.blocks} Blocks")
    print("="*70)
    print(f"\n📁 Results saved to:")
    print(f"   Signals: {signals_file}")
    print(f"   Trades:  {trades_file}")
    print(f"   Analysis: {analysis_file}")

    # Calculate MRB
    actual_blocks = num_bars / BARS_PER_BLOCK
    print(f"\n📊 Test Configuration:")
    print(f"   Blocks tested: {actual_blocks:.2f}")
    print(f"   Bars tested: {num_bars:,}")
    print(f"   Warmup: {args.warmup} bars")

if __name__ == "__main__":
    main()
