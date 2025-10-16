#!/usr/bin/env python3
"""
Optuna Fast Optimization - Using CLI Parameters
================================================

Optimizes PSM parameters efficiently using CLI arguments (no file modification!).

Top 3 Most Impactful PSM Parameters:
1. profit_target - When to take profit
2. stop_loss - When to cut losses
3. min_hold_bars - Minimum holding period

Target: 30 minutes, ~48 trials on SPY_4blocks (2 warmup + 2 test)

ADVANTAGE: No source code modification, no rebuilding!
Each trial runs in ~30 seconds vs 90 seconds (3x faster).
"""

import subprocess
import json
import tempfile
import os
from pathlib import Path
import time
from datetime import datetime
import sys

# Configuration
BASE_DIR = Path("/Volumes/ExternalSSD/Dev/C++/online_trader")
BUILD_DIR = BASE_DIR / "build"
DATA_FILE = BASE_DIR / "data/equities/SPY_4blocks.csv"
WARMUP_BARS = 960  # 2 blocks
TEST_BLOCKS = 2

# Optuna settings
TIMEOUT = 1800  # 30 minutes
STUDY_NAME = f"spy_cli_opt_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

def run_backtest_cli(profit_target, stop_loss, min_hold_bars):
    """
    Run a single backtest using CLI parameters.
    NO file modification, NO rebuilding!

    Returns:
        float: MRB or -999 if failed
    """
    try:
        # Generate signals (only once, can be reused!)
        signals_file = tempfile.mktemp(suffix='_signals.jsonl')

        cmd_signals = [
            str(BUILD_DIR / "sentio_cli"),
            "generate-signals",
            "--data", str(DATA_FILE),
            "--output", signals_file,
            "--warmup", str(WARMUP_BARS)
        ]

        result = subprocess.run(cmd_signals, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            print(f"  Signal generation failed: {result.stderr[:200]}")
            return -999.0

        # Execute trades with custom PSM parameters via CLI
        trades_file = tempfile.mktemp(suffix='_trades.jsonl')

        cmd_trades = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", signals_file,
            "--data", str(DATA_FILE),
            "--output", trades_file,
            "--warmup", str(WARMUP_BARS),
            "--profit-target", str(profit_target),
            "--stop-loss", str(stop_loss),
            "--min-hold-bars", str(min_hold_bars)
        ]

        result = subprocess.run(cmd_trades, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            print(f"  Trade execution failed: {result.stderr[:200]}")
            return -999.0

        # Calculate MRB
        mrb = calculate_mrb(trades_file)

        # Cleanup
        try:
            os.unlink(signals_file)
            os.unlink(trades_file)
        except:
            pass

        return mrb

    except subprocess.TimeoutExpired:
        print(f"  Timeout during backtest")
        return -999.0
    except Exception as e:
        print(f"  Backtest error: {e}")
        return -999.0

def calculate_mrb(trades_file):
    """Calculate MRB from trades file"""
    try:
        trades = []
        with open(trades_file, 'r') as f:
            for line in f:
                try:
                    trades.append(json.loads(line))
                except:
                    pass

        if not trades:
            return -999.0

        # Get final portfolio value from last trade
        last_trade = trades[-1]
        final_value = last_trade.get('portfolio_value', 0)

        if final_value == 0:
            return -999.0

        # Calculate return
        starting_capital = 100000.0
        total_return = (final_value / starting_capital - 1.0) * 100
        mrb = total_return / TEST_BLOCKS

        return mrb

    except Exception as e:
        print(f"  MRB calculation error: {e}")
        return -999.0

def grid_search_optimization():
    """
    Fast grid search over top 3 PSM parameters.
    Uses CLI parameters - NO file modification!
    """

    print("=" * 70)
    print("FAST CLI-BASED GRID SEARCH - TOP 3 PSM PARAMETERS")
    print("=" * 70)
    print()
    print("Target: 30 minutes on SPY_4blocks (2 warmup + 2 test)")
    print("Parameters: profit_target, stop_loss, min_hold_bars")
    print("Method: CLI arguments (no source modification!)")
    print()

    # Define grid (carefully selected for SPY)
    profit_targets = [0.002, 0.003, 0.004, 0.005]  # 0.2% - 0.5%
    stop_losses = [-0.003, -0.004, -0.005, -0.006]  # -0.3% - -0.6%
    min_hold_bars_options = [2, 3, 5]  # 2-5 bars

    total_trials = len(profit_targets) * len(stop_losses) * len(min_hold_bars_options)
    print(f"Total trials: {total_trials}")
    print(f"Estimated time: {total_trials * 30:.0f} seconds (~{total_trials * 0.5:.0f} minutes)")
    print()

    # Run grid search
    results = []
    best_mrb = -999.0
    best_params = None

    trial_num = 0
    start_time = time.time()

    for profit in profit_targets:
        for stop in stop_losses:
            for hold in min_hold_bars_options:
                trial_num += 1

                # Check timeout
                if time.time() - start_time > TIMEOUT:
                    print("\n⏱️  Timeout reached, stopping optimization")
                    break

                print(f"\nTrial {trial_num}/{total_trials}")
                print(f"  profit={profit:.4f}, stop={stop:.4f}, hold={hold}")

                trial_start = time.time()
                mrb = run_backtest_cli(profit, stop, hold)
                elapsed = time.time() - trial_start

                results.append({
                    'trial': trial_num,
                    'profit_target': profit,
                    'stop_loss': stop,
                    'min_hold_bars': hold,
                    'mrb': mrb,
                    'time': elapsed
                })

                print(f"  MRB: {mrb:.4f}%, time: {elapsed:.1f}s")

                if mrb > best_mrb:
                    best_mrb = mrb
                    best_params = {
                        'profit_target': profit,
                        'stop_loss': stop,
                        'min_hold_bars': hold
                    }
                    print(f"  ✨ NEW BEST! MRB: {best_mrb:.4f}%")

    total_time = time.time() - start_time

    # Results
    print()
    print("=" * 70)
    print("OPTIMIZATION COMPLETE")
    print("=" * 70)
    print(f"Total trials: {trial_num}")
    print(f"Total time: {total_time/60:.1f} minutes")
    print()

    if best_params is None:
        print("❌ ERROR: No successful trials completed")
        return None, -999.0, results

    print("🏆 BEST PARAMETERS:")
    print(f"  MRB: {best_mrb:.4f}%")
    print()
    for key, value in best_params.items():
        print(f"  {key}: {value}")
    print()

    # Baseline comparison
    baseline_mrb = 0.345  # v1.5
    if best_mrb > baseline_mrb:
        improvement = (best_mrb - baseline_mrb) / baseline_mrb * 100
        print(f"✅ IMPROVEMENT: +{improvement:.1f}% vs v1.5 baseline ({baseline_mrb:.4f}%)")
    else:
        decline = (baseline_mrb - best_mrb) / baseline_mrb * 100
        print(f"⚠️  DECLINE: -{decline:.1f}% vs v1.5 baseline")
        print("   Recommend sticking with v1.5 parameters")

    # Save results
    results_file = BASE_DIR / "data/tmp/optuna_cli_results.json"
    with open(results_file, 'w') as f:
        output = {
            'study_name': STUDY_NAME,
            'total_trials': trial_num,
            'total_time_seconds': total_time,
            'best_mrb': best_mrb,
            'best_params': best_params,
            'baseline_mrb': baseline_mrb,
            'all_results': results,
            'completed_at': datetime.now().isoformat()
        }
        json.dump(output, f, indent=2)

    print()
    print(f"Results saved to: {results_file}")
    print()

    return best_params, best_mrb, results

def main():
    """Main optimization entry point"""

    # Check prerequisites
    if not (BUILD_DIR / "sentio_cli").exists():
        print("ERROR: sentio_cli not found")
        print(f"Expected: {BUILD_DIR}/sentio_cli")
        return 1

    if not DATA_FILE.exists():
        print(f"ERROR: Data file not found: {DATA_FILE}")
        return 1

    print("Starting CLI-based optimization...")
    print("ADVANTAGE: No source modification, no rebuilding!")
    print("Each trial: ~30 seconds (3x faster than old method)")
    print()

    # Run optimization
    try:
        best_params, best_mrb, results = grid_search_optimization()

        if best_params is None:
            print("\n❌ Optimization failed - no successful trials")
            return 1

        print()
        print("=" * 70)
        print("NEXT STEPS")
        print("=" * 70)
        print()

        if best_mrb > 0.345:
            print("✅ Best parameters BEAT v1.5 baseline!")
            print()
            print("To deploy:")
            print(f"1. Use these CLI parameters in live trading:")
            print(f"   --profit-target {best_params['profit_target']}")
            print(f"   --stop-loss {best_params['stop_loss']}")
            print(f"   --min-hold-bars {best_params['min_hold_bars']}")
            print()
            print("2. Test on 4-block data to verify")
            print("3. Deploy to paper trading")
        else:
            print("⚠️  Best parameters DID NOT beat v1.5 baseline")
            print()
            print("Recommendation:")
            print("✅ Use v1.5 parameters for tonight:")
            print("   --profit-target 0.003")
            print("   --stop-loss -0.004")
            print("   --min-hold-bars 3")
            print()
            print("🔬 Run longer optimization tomorrow (8-12 hours)")

        return 0

    except KeyboardInterrupt:
        print("\n\n⏹️  Optimization interrupted by user")
        return 1
    except Exception as e:
        print(f"\n\n❌ ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
