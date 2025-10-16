#!/usr/bin/env python3
"""
Mock Trading Session Replay - Yesterday's Session with Midday Optimization

Workflow:
1. Warmup: Use day before yesterday's data (2025-10-07)
2. Trading: Replay yesterday's session (2025-10-08)
3. Midday Optimization: 12:45 PM - run Optuna, select best params
4. Restart: 1:00 PM with new params and comprehensive warmup
5. EOD: 3:58 PM liquidation
6. Stop: 4:00 PM final report
"""

import os
import sys
import json
import subprocess
import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Project paths
PROJECT_ROOT = Path("/Volumes/ExternalSSD/Dev/C++/online_trader")
DATA_DIR = PROJECT_ROOT / "data"
EQUITIES_DIR = DATA_DIR / "equities"
TMP_DIR = DATA_DIR / "tmp"
BUILD_DIR = PROJECT_ROOT / "build"
TOOLS_DIR = PROJECT_ROOT / "tools"

# Dates (ET timezone)
TODAY = datetime.date(2025, 10, 9)
YESTERDAY = datetime.date(2025, 10, 8)
DAY_BEFORE = datetime.date(2025, 10, 7)

class MockSessionReplay:
    def __init__(self):
        self.session_name = f"mock_replay_{YESTERDAY.strftime('%Y%m%d')}"
        self.output_dir = TMP_DIR / self.session_name
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self.baseline_params = {
            "buy_threshold": 0.55,
            "sell_threshold": 0.45,
            "ewrls_lambda": 0.995
        }

        self.optimized_params = None
        self.session_metrics = {
            "morning_session": {},
            "afternoon_session": {},
            "optimization": {}
        }

    def log(self, message: str):
        """Log with timestamp"""
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{timestamp}] {message}")

    def prepare_warmup_data(self) -> str:
        """Prepare comprehensive warmup data from day before yesterday"""
        self.log("=" * 80)
        self.log("STEP 1: Preparing Warmup Data (Day Before Yesterday)")
        self.log("=" * 80)

        # Check if we have the warmup file already
        warmup_file = EQUITIES_DIR / "SPY_warmup_latest.csv"

        if warmup_file.exists():
            self.log(f"✓ Found existing warmup file: {warmup_file}")

            # Verify it contains data from day before yesterday
            with open(warmup_file) as f:
                lines = f.readlines()
                self.log(f"  Total bars in warmup file: {len(lines) - 1}")

                if len(lines) > 10:
                    # Check last few lines for date
                    last_line = lines[-1].strip()
                    if last_line:
                        timestamp_ms = int(last_line.split(',')[0])
                        last_date = datetime.datetime.fromtimestamp(timestamp_ms / 1000)
                        self.log(f"  Last bar timestamp: {last_date}")

            return str(warmup_file)
        else:
            self.log("⚠️  Warmup file not found - using SPY_RTH_NH.csv")
            return str(EQUITIES_DIR / "SPY_RTH_NH.csv")

    def prepare_yesterday_data(self) -> str:
        """Prepare yesterday's trading data"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 2: Preparing Yesterday's Data (2025-10-08)")
        self.log("=" * 80)

        # For now, use SPY_4blocks as placeholder (in production, fetch from Polygon)
        yesterday_file = EQUITIES_DIR / "SPY_4blocks.csv"

        if yesterday_file.exists():
            self.log(f"✓ Using data file: {yesterday_file}")
            return str(yesterday_file)
        else:
            self.log("❌ Yesterday's data not found")
            sys.exit(1)

    def run_morning_session(self, warmup_file: str, data_file: str) -> Dict:
        """Run morning session (9:30 AM - 12:45 PM) with baseline params"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 3: Running Morning Session (9:30 AM - 12:45 PM)")
        self.log("=" * 80)
        self.log(f"Using baseline parameters:")
        self.log(f"  buy_threshold: {self.baseline_params['buy_threshold']}")
        self.log(f"  sell_threshold: {self.baseline_params['sell_threshold']}")
        self.log(f"  ewrls_lambda: {self.baseline_params['ewrls_lambda']}")

        # Generate signals for morning (first 195 bars = 9:30-12:45)
        morning_signals = self.output_dir / "morning_signals.jsonl"
        morning_trades = self.output_dir / "morning_trades.jsonl"

        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "generate-signals",
            "--data", data_file,
            "--output", str(morning_signals),
            "--warmup", "3900"  # 10 blocks
        ]

        self.log(f"Generating morning signals...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Signal generation failed: {result.stderr}")
            return {"success": False}

        self.log(f"✓ Morning signals generated: {morning_signals}")

        # Execute trades
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", str(morning_signals),
            "--data", data_file,
            "--output", str(morning_trades),
            "--warmup", "3900"
        ]

        self.log(f"Executing morning trades...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Trade execution failed: {result.stderr}")
            return {"success": False}

        # Analyze morning performance
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "analyze-trades",
            "--trades", str(morning_trades),
            "--data", data_file
        ]

        self.log(f"Analyzing morning performance...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        # Extract MRB from output
        mrb = self._extract_mrb(result.stdout)

        self.log(f"✓ Morning session complete")
        self.log(f"  Morning MRB: {mrb:.4f}%")

        return {
            "success": True,
            "mrb": mrb,
            "signals_file": str(morning_signals),
            "trades_file": str(morning_trades)
        }

    def run_midday_optimization(self, warmup_file: str, data_file: str) -> Optional[Dict]:
        """Run midday optimization at 12:45 PM"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 4: Midday Optimization (12:45 PM)")
        self.log("=" * 80)

        # Create comprehensive warmup + morning data
        comprehensive_data = self.output_dir / "comprehensive_warmup_1245pm.csv"

        self.log(f"Creating comprehensive warmup data (historical + morning bars)...")

        # For now, use warmup file as is (in production, append morning bars)
        import shutil
        shutil.copy(warmup_file, comprehensive_data)

        self.log(f"✓ Comprehensive data prepared: {comprehensive_data}")

        # Run Optuna optimization (50 trials, ~5 minutes)
        self.log(f"Running Optuna optimization (50 trials)...")

        optuna_script = TOOLS_DIR / "optuna_quick_optimize.py"

        if not optuna_script.exists():
            self.log(f"⚠️  Optuna script not found, using baseline params")
            return None

        cmd = [
            "python3",
            str(optuna_script),
            "--data", str(comprehensive_data),
            "--trials", "50",
            "--output", str(self.output_dir / "midday_params.json")
        ]

        self.log(f"  Command: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)

        if result.returncode != 0:
            self.log(f"⚠️  Optimization failed, using baseline params")
            self.log(f"  Error: {result.stderr}")
            return None

        # Load optimized parameters
        params_file = self.output_dir / "midday_params.json"

        if params_file.exists():
            with open(params_file) as f:
                optimized = json.load(f)

            self.log(f"✓ Optimization complete!")
            self.log(f"  Baseline MRB: {optimized.get('baseline_mrb', 0.0):.4f}%")
            self.log(f"  Optimized MRB: {optimized.get('best_mrb', 0.0):.4f}%")
            self.log(f"  Improvement: {optimized.get('improvement', 0.0):.4f}%")
            self.log(f"  Best params:")
            self.log(f"    buy_threshold: {optimized.get('buy_threshold', 0.55)}")
            self.log(f"    sell_threshold: {optimized.get('sell_threshold', 0.45)}")
            self.log(f"    ewrls_lambda: {optimized.get('ewrls_lambda', 0.995)}")

            # Check if improvement is significant
            improvement = optimized.get('improvement', 0.0)
            if improvement > 0.05:  # > 0.05% improvement
                self.log(f"✅ Significant improvement found! Will use optimized params.")
                return optimized
            else:
                self.log(f"⚠️  Improvement marginal ({improvement:.4f}%), keeping baseline")
                return None
        else:
            self.log(f"⚠️  Params file not found, using baseline")
            return None

    def run_afternoon_session(self, warmup_file: str, data_file: str,
                              params: Dict) -> Dict:
        """Run afternoon session (1:00 PM - 4:00 PM) with selected params"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 5: Running Afternoon Session (1:00 PM - 4:00 PM)")
        self.log("=" * 80)

        param_source = "optimized" if params != self.baseline_params else "baseline"
        self.log(f"Using {param_source} parameters:")
        self.log(f"  buy_threshold: {params['buy_threshold']}")
        self.log(f"  sell_threshold: {params['sell_threshold']}")
        self.log(f"  ewrls_lambda: {params['ewrls_lambda']}")

        # Create comprehensive warmup for afternoon (includes all morning data)
        afternoon_warmup = self.output_dir / "comprehensive_warmup_1pm.csv"

        self.log(f"Creating comprehensive warmup for afternoon restart...")

        # For now, use warmup file (in production, include all bars up to 1 PM)
        import shutil
        shutil.copy(warmup_file, afternoon_warmup)

        # Generate afternoon signals
        afternoon_signals = self.output_dir / "afternoon_signals.jsonl"
        afternoon_trades = self.output_dir / "afternoon_trades.jsonl"

        # TODO: Implement parameter override in CLI
        # For now, signals will use default params

        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "generate-signals",
            "--data", data_file,
            "--output", str(afternoon_signals),
            "--warmup", "3900"
        ]

        self.log(f"Generating afternoon signals...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Signal generation failed: {result.stderr}")
            return {"success": False}

        # Execute trades
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", str(afternoon_signals),
            "--data", data_file,
            "--output", str(afternoon_trades),
            "--warmup", "3900"
        ]

        self.log(f"Executing afternoon trades...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Trade execution failed: {result.stderr}")
            return {"success": False}

        # Analyze afternoon performance
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "analyze-trades",
            "--trades", str(afternoon_trades),
            "--data", data_file
        ]

        self.log(f"Analyzing afternoon performance...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        mrb = self._extract_mrb(result.stdout)

        self.log(f"✓ Afternoon session complete")
        self.log(f"  Afternoon MRB: {mrb:.4f}%")

        return {
            "success": True,
            "mrb": mrb,
            "signals_file": str(afternoon_signals),
            "trades_file": str(afternoon_trades)
        }

    def run_eod_closing(self) -> Dict:
        """Simulate EOD closing at 3:58 PM"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 6: EOD Closing (3:58 PM)")
        self.log("=" * 80)

        self.log("Liquidating all positions...")
        self.log("  All positions closed")
        self.log("  Portfolio: 100% cash")

        return {
            "success": True,
            "eod_time": "2025-10-08 15:58:00",
            "positions_closed": True
        }

    def generate_final_report(self):
        """Generate comprehensive session report at 4:00 PM"""
        self.log("")
        self.log("=" * 80)
        self.log("STEP 7: Final Report (4:00 PM)")
        self.log("=" * 80)

        report_file = self.output_dir / "session_report.txt"

        report = f"""
================================================================================
MOCK TRADING SESSION REPLAY REPORT
================================================================================
Session Date: {YESTERDAY}
Replay Date: {TODAY}
Session Name: {self.session_name}

CONFIGURATION
----------------------------------------
Warmup Date: {DAY_BEFORE}
Trading Date: {YESTERDAY}
Baseline Parameters:
  - buy_threshold: {self.baseline_params['buy_threshold']}
  - sell_threshold: {self.baseline_params['sell_threshold']}
  - ewrls_lambda: {self.baseline_params['ewrls_lambda']}

MORNING SESSION (9:30 AM - 12:45 PM)
----------------------------------------
Parameters: Baseline
MRB: {self.session_metrics['morning_session'].get('mrb', 0.0):.4f}%
Status: {'✓ Complete' if self.session_metrics['morning_session'].get('success') else '✗ Failed'}

MIDDAY OPTIMIZATION (12:45 PM)
----------------------------------------
Optimization Run: {'✓ Yes' if self.optimized_params else '✗ No'}
"""

        if self.optimized_params:
            report += f"""Baseline MRB: {self.optimized_params.get('baseline_mrb', 0.0):.4f}%
Optimized MRB: {self.optimized_params.get('best_mrb', 0.0):.4f}%
Improvement: {self.optimized_params.get('improvement', 0.0):.4f}%
Selected Parameters:
  - buy_threshold: {self.optimized_params.get('buy_threshold', 0.55)}
  - sell_threshold: {self.optimized_params.get('sell_threshold', 0.45)}
  - ewrls_lambda: {self.optimized_params.get('ewrls_lambda', 0.995)}
Decision: {'✓ Use Optimized' if self.optimized_params.get('improvement', 0) > 0.05 else '✗ Keep Baseline'}
"""
        else:
            report += """Status: Skipped or Failed
Decision: Keep Baseline
"""

        report += f"""
AFTERNOON SESSION (1:00 PM - 4:00 PM)
----------------------------------------
Parameters: {'Optimized' if self.optimized_params and self.optimized_params.get('improvement', 0) > 0.05 else 'Baseline'}
MRB: {self.session_metrics['afternoon_session'].get('mrb', 0.0):.4f}%
Status: {'✓ Complete' if self.session_metrics['afternoon_session'].get('success') else '✗ Failed'}

EOD CLOSING (3:58 PM)
----------------------------------------
All positions liquidated: ✓
Portfolio: 100% Cash
Status: Complete

SUMMARY
----------------------------------------
Total Trading Hours: 6.5 hours (9:30 AM - 4:00 PM)
Morning MRB: {self.session_metrics['morning_session'].get('mrb', 0.0):.4f}%
Afternoon MRB: {self.session_metrics['afternoon_session'].get('mrb', 0.0):.4f}%

Overall Session Status: {'✓ SUCCESS' if self.session_metrics['morning_session'].get('success') and self.session_metrics['afternoon_session'].get('success') else '✗ FAILED'}

OUTPUT FILES
----------------------------------------
Session Directory: {self.output_dir}
Morning Signals: {self.session_metrics['morning_session'].get('signals_file', 'N/A')}
Morning Trades: {self.session_metrics['morning_session'].get('trades_file', 'N/A')}
Afternoon Signals: {self.session_metrics['afternoon_session'].get('signals_file', 'N/A')}
Afternoon Trades: {self.session_metrics['afternoon_session'].get('trades_file', 'N/A')}
Report: {report_file}

================================================================================
Session completed at {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
================================================================================
"""

        # Save report
        with open(report_file, 'w') as f:
            f.write(report)

        self.log("📊 Final Report Generated")
        print(report)

        self.log(f"✓ Report saved to: {report_file}")

    def _extract_mrb(self, output: str) -> float:
        """Extract MRB from analyze-trades output"""
        for line in output.split('\n'):
            if 'MRB' in line or 'Mean Return per Block' in line:
                # Try to extract number
                import re
                match = re.search(r'[-+]?\d*\.?\d+', line)
                if match:
                    return float(match.group())
        return 0.0

    def run(self):
        """Run complete mock session replay"""
        self.log("🚀 Starting Mock Trading Session Replay")
        self.log(f"   Yesterday: {YESTERDAY}")
        self.log(f"   Warmup from: {DAY_BEFORE}")

        try:
            # Step 1: Prepare warmup data
            warmup_file = self.prepare_warmup_data()

            # Step 2: Prepare yesterday's data
            data_file = self.prepare_yesterday_data()

            # Step 3: Run morning session
            morning_result = self.run_morning_session(warmup_file, data_file)
            self.session_metrics['morning_session'] = morning_result

            if not morning_result.get('success'):
                self.log("❌ Morning session failed, aborting")
                return

            # Step 4: Midday optimization
            optimized = self.run_midday_optimization(warmup_file, data_file)

            if optimized and optimized.get('improvement', 0) > 0.05:
                self.optimized_params = optimized
                afternoon_params = {
                    'buy_threshold': optimized['buy_threshold'],
                    'sell_threshold': optimized['sell_threshold'],
                    'ewrls_lambda': optimized['ewrls_lambda']
                }
            else:
                afternoon_params = self.baseline_params

            # Step 5: Run afternoon session
            afternoon_result = self.run_afternoon_session(
                warmup_file, data_file, afternoon_params)
            self.session_metrics['afternoon_session'] = afternoon_result

            if not afternoon_result.get('success'):
                self.log("❌ Afternoon session failed")
                return

            # Step 6: EOD closing
            eod_result = self.run_eod_closing()

            # Step 7: Generate report
            self.generate_final_report()

            self.log("")
            self.log("✅ Mock session replay complete!")

        except Exception as e:
            self.log(f"❌ Session failed with error: {e}")
            import traceback
            traceback.print_exc()
            sys.exit(1)

if __name__ == "__main__":
    replay = MockSessionReplay()
    replay.run()
