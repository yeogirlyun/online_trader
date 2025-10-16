#!/usr/bin/env python3
"""
October Rotation Trading Test Suite
===================================

Runs mock rotation trading tests for each trading day in October 2025
and generates comprehensive dashboard reports.

Features:
- Runs rotation trading for each October trading day
- Generates individual dashboard for each day
- Creates summary dashboard across all days
- Aggregates performance metrics

Usage:
    python tools/run_october_rotation_tests.py [--start-date YYYY-MM-DD] [--end-date YYYY-MM-DD]

    # Run full October
    python tools/run_october_rotation_tests.py

    # Run specific date range
    python tools/run_october_rotation_tests.py --start-date 2025-10-01 --end-date 2025-10-10
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path
from datetime import datetime, timedelta
import json
from collections import defaultdict

class OctoberRotationTestSuite:
    """Test suite for running rotation trading across October days"""

    def __init__(self, start_date=None, end_date=None, data_dir='data/equities',
                 output_dir='logs/october_rotation_tests'):
        self.start_date = start_date or '2025-10-01'
        self.end_date = end_date or '2025-10-31'
        self.data_dir = Path(data_dir)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.symbols = ['SDS', 'SPXL', 'SQQQ', 'SVIX', 'TQQQ', 'UVXY']
        self.cli_path = Path('build/sentio_cli')

        # Results tracking
        self.daily_results = []
        self.summary_metrics = defaultdict(float)

    def get_trading_days(self):
        """Extract actual trading days from SPY data"""
        print(f"\n📅 Finding trading days from {self.start_date} to {self.end_date}...")

        spy_file = self.data_dir / 'SPY_RTH_NH.csv'
        if not spy_file.exists():
            print(f"❌ SPY data file not found: {spy_file}")
            return []

        trading_days = set()
        with open(spy_file, 'r') as f:
            next(f)  # Skip header
            for line in f:
                parts = line.strip().split(',')
                if len(parts) > 0:
                    timestamp = parts[0]
                    date_str = timestamp.split('T')[0]

                    # Check if within date range
                    if self.start_date <= date_str <= self.end_date:
                        trading_days.add(date_str)

        trading_days = sorted(list(trading_days))
        print(f"✓ Found {len(trading_days)} trading days")
        for day in trading_days:
            print(f"   {day}")

        return trading_days

    def run_daily_rotation(self, date):
        """Run rotation trading for a specific date"""
        print(f"\n" + "="*80)
        print(f"📊 Running rotation trading for {date}")
        print("="*80)

        # Create day-specific output directory
        day_output = self.output_dir / date
        day_output.mkdir(parents=True, exist_ok=True)

        # Output files
        trades_file = day_output / 'trades.jsonl'
        signals_file = day_output / 'signals.jsonl'
        positions_file = day_output / 'positions.jsonl'
        decisions_file = day_output / 'decisions.jsonl'

        # Run mock trading command
        cmd = [
            str(self.cli_path),
            'mock',
            '--date', date,
            '--data-dir', str(self.data_dir),
            '--output-dir', str(day_output)
        ]

        print(f"\n🚀 Command: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                check=True,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )

            print(result.stdout)
            if result.stderr:
                print("Warnings/Errors:", result.stderr)

            # Check if trades were generated
            if trades_file.exists():
                num_trades = sum(1 for _ in open(trades_file))
                print(f"✓ Generated {num_trades} trades for {date}")
                return True, num_trades, day_output
            else:
                print(f"⚠️  No trades file generated for {date}")
                return False, 0, day_output

        except subprocess.TimeoutExpired:
            print(f"❌ Timeout running rotation trading for {date}")
            return False, 0, day_output
        except subprocess.CalledProcessError as e:
            print(f"❌ Error running rotation trading for {date}")
            print(f"   Exit code: {e.returncode}")
            print(f"   Stdout: {e.stdout}")
            print(f"   Stderr: {e.stderr}")
            return False, 0, day_output

    def generate_daily_dashboard(self, date, day_output):
        """Generate dashboard for a specific trading day"""
        print(f"\n📈 Generating dashboard for {date}...")

        trades_file = day_output / 'trades.jsonl'
        signals_file = day_output / 'signals.jsonl'
        positions_file = day_output / 'positions.jsonl'
        decisions_file = day_output / 'decisions.jsonl'
        dashboard_file = day_output / 'dashboard.html'

        if not trades_file.exists():
            print(f"⚠️  No trades file found, skipping dashboard for {date}")
            return False

        cmd = [
            'python3',
            'scripts/rotation_trading_dashboard.py',
            '--trades', str(trades_file),
            '--output', str(dashboard_file)
        ]

        # Add optional files if they exist
        if signals_file.exists():
            cmd.extend(['--signals', str(signals_file)])
        if positions_file.exists():
            cmd.extend(['--positions', str(positions_file)])
        if decisions_file.exists():
            cmd.extend(['--decisions', str(decisions_file)])

        try:
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            print(f"✓ Dashboard generated: {dashboard_file}")

            # Extract metrics from output
            metrics = self.extract_metrics_from_output(result.stdout)
            metrics['date'] = date
            metrics['dashboard_path'] = str(dashboard_file)
            self.daily_results.append(metrics)

            return True

        except subprocess.CalledProcessError as e:
            print(f"❌ Error generating dashboard for {date}")
            print(f"   Error: {e.stderr}")
            return False

    def extract_metrics_from_output(self, output):
        """Extract performance metrics from dashboard output"""
        metrics = {
            'total_trades': 0,
            'total_pnl': 0.0,
            'return_pct': 0.0,
            'win_rate': 0.0,
            'wins': 0,
            'losses': 0
        }

        for line in output.split('\n'):
            if 'Total Trades:' in line:
                parts = line.split(':')[1].strip().split()
                metrics['total_trades'] = int(parts[0])
            elif 'Total P&L:' in line:
                # Format: "Total P&L:        $+1,302.02 (+1.30%)"
                parts = line.split('$')[1].split()
                pnl_str = parts[0].replace(',', '').replace('+', '')
                metrics['total_pnl'] = float(pnl_str)
                if '(' in line:
                    pct_str = line.split('(')[1].split('%')[0].replace('+', '')
                    metrics['return_pct'] = float(pct_str)
            elif 'Win Rate:' in line:
                # Format: "Win Rate:         60.6% (20W / 13L)"
                parts = line.split(':')[1].strip().split()
                metrics['win_rate'] = float(parts[0].replace('%', ''))
                if '(' in line:
                    win_loss = line.split('(')[1].split(')')[0]
                    wins_str = win_loss.split('W')[0].strip()
                    losses_str = win_loss.split('/')[1].strip().replace('L', '')
                    metrics['wins'] = int(wins_str)
                    metrics['losses'] = int(losses_str)

        return metrics

    def generate_summary_dashboard(self):
        """Generate summary dashboard across all October days"""
        print(f"\n" + "="*80)
        print(f"📊 GENERATING OCTOBER SUMMARY DASHBOARD")
        print("="*80)

        if not self.daily_results:
            print("⚠️  No daily results to summarize")
            return

        # Aggregate metrics
        total_trades = sum(r['total_trades'] for r in self.daily_results)
        total_pnl = sum(r['total_pnl'] for r in self.daily_results)
        total_wins = sum(r['wins'] for r in self.daily_results)
        total_losses = sum(r['losses'] for r in self.daily_results)
        avg_daily_return = sum(r['return_pct'] for r in self.daily_results) / len(self.daily_results)

        win_rate = (total_wins / (total_wins + total_losses) * 100) if (total_wins + total_losses) > 0 else 0.0

        # Create summary report
        summary_file = self.output_dir / 'OCTOBER_SUMMARY.md'

        summary_content = f"""# October 2025 Rotation Trading Summary

## Test Period
- **Start Date**: {self.daily_results[0]['date']}
- **End Date**: {self.daily_results[-1]['date']}
- **Trading Days**: {len(self.daily_results)}

## Aggregate Performance

### Overall Statistics
- **Total Trades**: {total_trades:,}
- **Total P&L**: ${total_pnl:+,.2f}
- **Average Daily Return**: {avg_daily_return:+.2f}%
- **Win Rate**: {win_rate:.1f}% ({total_wins}W / {total_losses}L)

### Daily Breakdown

| Date | Trades | P&L | Return % | Win Rate | Dashboard |
|------|--------|-----|----------|----------|-----------|
"""

        for result in self.daily_results:
            summary_content += f"| {result['date']} | {result['total_trades']} | ${result['total_pnl']:+.2f} | {result['return_pct']:+.2f}% | {result['win_rate']:.1f}% | [View]({result['dashboard_path']}) |\n"

        summary_content += f"""
## Best Performing Days

### Top 3 by P&L
"""
        sorted_by_pnl = sorted(self.daily_results, key=lambda x: x['total_pnl'], reverse=True)[:3]
        for i, result in enumerate(sorted_by_pnl, 1):
            summary_content += f"{i}. **{result['date']}**: ${result['total_pnl']:+.2f} ({result['return_pct']:+.2f}%)\n"

        summary_content += f"""
### Top 3 by Return %
"""
        sorted_by_return = sorted(self.daily_results, key=lambda x: x['return_pct'], reverse=True)[:3]
        for i, result in enumerate(sorted_by_return, 1):
            summary_content += f"{i}. **{result['date']}**: {result['return_pct']:+.2f}% (${result['total_pnl']:+.2f})\n"

        summary_content += f"""
## Worst Performing Days

### Bottom 3 by P&L
"""
        sorted_by_pnl_worst = sorted(self.daily_results, key=lambda x: x['total_pnl'])[:3]
        for i, result in enumerate(sorted_by_pnl_worst, 1):
            summary_content += f"{i}. **{result['date']}**: ${result['total_pnl']:+.2f} ({result['return_pct']:+.2f}%)\n"

        summary_content += f"""
## Statistics

### Trading Activity
- **Average Trades per Day**: {total_trades / len(self.daily_results):.1f}
- **Most Active Day**: {max(self.daily_results, key=lambda x: x['total_trades'])['date']} ({max(self.daily_results, key=lambda x: x['total_trades'])['total_trades']} trades)
- **Least Active Day**: {min(self.daily_results, key=lambda x: x['total_trades'])['date']} ({min(self.daily_results, key=lambda x: x['total_trades'])['total_trades']} trades)

### Performance Distribution
- **Winning Days**: {sum(1 for r in self.daily_results if r['total_pnl'] > 0)} ({sum(1 for r in self.daily_results if r['total_pnl'] > 0) / len(self.daily_results) * 100:.1f}%)
- **Losing Days**: {sum(1 for r in self.daily_results if r['total_pnl'] < 0)} ({sum(1 for r in self.daily_results if r['total_pnl'] < 0) / len(self.daily_results) * 100:.1f}%)
- **Breakeven Days**: {sum(1 for r in self.daily_results if r['total_pnl'] == 0)}

## Files Generated

- **Summary Report**: `{summary_file.name}`
- **Individual Dashboards**: `logs/october_rotation_tests/<date>/dashboard.html`
- **Trade Data**: `logs/october_rotation_tests/<date>/trades.jsonl`

---

Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
"""

        with open(summary_file, 'w') as f:
            f.write(summary_content)

        print(f"\n✅ Summary report saved: {summary_file}")

        # Print summary to console
        print(f"\n" + "="*80)
        print(f"OCTOBER 2025 ROTATION TRADING SUMMARY")
        print("="*80)
        print(f"Trading Days:        {len(self.daily_results)}")
        print(f"Total Trades:        {total_trades:,}")
        print(f"Total P&L:           ${total_pnl:+,.2f}")
        print(f"Avg Daily Return:    {avg_daily_return:+.2f}%")
        print(f"Win Rate:            {win_rate:.1f}% ({total_wins}W / {total_losses}L)")
        print(f"Winning Days:        {sum(1 for r in self.daily_results if r['total_pnl'] > 0)} / {len(self.daily_results)}")
        print("="*80)

        # Save JSON results
        json_file = self.output_dir / 'october_results.json'
        with open(json_file, 'w') as f:
            json.dump({
                'summary': {
                    'trading_days': len(self.daily_results),
                    'total_trades': total_trades,
                    'total_pnl': total_pnl,
                    'avg_daily_return': avg_daily_return,
                    'win_rate': win_rate,
                    'total_wins': total_wins,
                    'total_losses': total_losses
                },
                'daily_results': self.daily_results
            }, f, indent=2)

        print(f"\n✅ JSON results saved: {json_file}")

    def run(self):
        """Run the complete October test suite"""
        print("\n" + "="*80)
        print("🎯 OCTOBER 2025 ROTATION TRADING TEST SUITE")
        print("="*80)

        # Check CLI exists
        if not self.cli_path.exists():
            print(f"❌ CLI not found: {self.cli_path}")
            print("   Please build the project first: cmake --build build")
            return 1

        # Get trading days
        trading_days = self.get_trading_days()
        if not trading_days:
            print("❌ No trading days found")
            return 1

        # Run rotation trading for each day
        success_count = 0
        for i, date in enumerate(trading_days, 1):
            print(f"\n[{i}/{len(trading_days)}] Processing {date}...")

            success, num_trades, day_output = self.run_daily_rotation(date)
            if success:
                # Generate dashboard for this day
                if self.generate_daily_dashboard(date, day_output):
                    success_count += 1

        # Generate summary dashboard
        if success_count > 0:
            self.generate_summary_dashboard()

            print(f"\n✅ Successfully processed {success_count}/{len(trading_days)} trading days")
            print(f"📂 Results saved to: {self.output_dir}")
            return 0
        else:
            print(f"\n❌ No days successfully processed")
            return 1


def main():
    parser = argparse.ArgumentParser(
        description='Run October 2025 rotation trading test suite',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run full October
  python tools/run_october_rotation_tests.py

  # Run specific date range
  python tools/run_october_rotation_tests.py --start-date 2025-10-01 --end-date 2025-10-10

  # Custom output directory
  python tools/run_october_rotation_tests.py --output-dir logs/my_october_tests
        """
    )

    parser.add_argument('--start-date', type=str,
                       help='Start date (YYYY-MM-DD). Default: 2025-10-01')
    parser.add_argument('--end-date', type=str,
                       help='End date (YYYY-MM-DD). Default: 2025-10-31')
    parser.add_argument('--data-dir', type=str, default='data/equities',
                       help='Data directory. Default: data/equities')
    parser.add_argument('--output-dir', type=str, default='logs/october_rotation_tests',
                       help='Output directory. Default: logs/october_rotation_tests')

    args = parser.parse_args()

    suite = OctoberRotationTestSuite(
        start_date=args.start_date,
        end_date=args.end_date,
        data_dir=args.data_dir,
        output_dir=args.output_dir
    )

    return suite.run()


if __name__ == '__main__':
    sys.exit(main())
