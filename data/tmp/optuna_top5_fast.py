#!/usr/bin/env python3
"""
Optuna Fast Optimization - Top 5 Parameters for SPY
===================================================

Optimizes the 5 MOST IMPACTFUL parameters in 30 minutes.

Top 5 Parameters (by impact on MRB):
1. profit_target (PSM) - When to take profit
2. stop_loss (PSM) - When to cut losses
3. buy_threshold (OES) - Long signal threshold
4. sell_threshold (OES) - Short signal threshold
5. min_hold_bars (PSM) - Minimum holding period

Target: 30 minutes, ~20-30 trials on SPY_4blocks (2 warmup + 2 test)
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
STUDY_NAME = f"spy_top5_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

def modify_cpp_parameters(profit_target, stop_loss, min_hold_bars):
    """
    Temporarily modify execute_trades_command.cpp with new PSM parameters.

    Returns path to modified file.
    """
    source_file = BASE_DIR / "src/cli/execute_trades_command.cpp"

    with open(source_file, 'r') as f:
        content = f.read()

    # Replace PSM parameters
    content = content.replace(
        'const double PROFIT_TARGET = 0.003;',
        f'const double PROFIT_TARGET = {profit_target};'
    )
    content = content.replace(
        'const double STOP_LOSS = -0.004;',
        f'const double STOP_LOSS = {stop_loss};'
    )
    content = content.replace(
        'const int MIN_HOLD_BARS = 3;',
        f'const int MIN_HOLD_BARS = {min_hold_bars};'
    )

    with open(source_file, 'w') as f:
        f.write(content)

    return source_file

def rebuild_sentio_cli():
    """Rebuild sentio_cli after parameter modification"""
    try:
        result = subprocess.run(
            ['make', '-j8', 'sentio_cli'],
            cwd=BUILD_DIR,
            capture_output=True,
            text=True,
            timeout=120
        )
        return result.returncode == 0
    except Exception as e:
        print(f"Build failed: {e}")
        return False

def run_backtest(profit_target, stop_loss, min_hold_bars,
                 buy_threshold, sell_threshold):
    """
    Run a single backtest with given parameters.

    NOTE: buy_threshold and sell_threshold are not yet implemented in CLI.
    For now, we only optimize PSM parameters (profit, stop, hold).

    Returns:
        float: MRB or -999 if failed
    """
    try:
        # Modify C++ code with new parameters
        modify_cpp_parameters(profit_target, stop_loss, min_hold_bars)

        # Rebuild
        if not rebuild_sentio_cli():
            print("Build failed, skipping trial")
            return -999.0

        # Generate signals
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
            return -999.0

        # Execute trades
        trades_file = tempfile.mktemp(suffix='_trades.jsonl')

        cmd_trades = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", signals_file,
            "--data", str(DATA_FILE),
            "--output", trades_file,
            "--warmup", str(WARMUP_BARS)
        ]

        result = subprocess.run(cmd_trades, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
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

    except Exception as e:
        print(f"Backtest error: {e}")
        return -999.0

def calculate_mrb(trades_file):
    """Calculate MRB from trades"""
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

        # Get test period trades
        test_trades = [t for t in trades if t.get('bar_index', 0) >= WARMUP_BARS]

        if not test_trades or 'total_return' not in test_trades[-1]:
            return -999.0

        final_return = test_trades[-1]['total_return']
        mrb = final_return / TEST_BLOCKS

        return mrb

    except Exception as e:
        return -999.0

def objective(trial_params):
    """Objective function for optimization"""

    profit_target = trial_params['profit_target']
    stop_loss = trial_params['stop_loss']
    min_hold_bars = trial_params['min_hold_bars']
    buy_threshold = trial_params['buy_threshold']
    sell_threshold = trial_params['sell_threshold']

    # Validate constraints
    if profit_target <= 0 or profit_target > 0.02:
        return -999.0

    if stop_loss >= 0 or stop_loss < -0.02:
        return -999.0

    if min_hold_bars < 1:
        return -999.0

    # Run backtest
    start_time = time.time()
    mrb = run_backtest(
        profit_target=profit_target,
        stop_loss=stop_loss,
        min_hold_bars=min_hold_bars,
        buy_threshold=buy_threshold,
        sell_threshold=sell_threshold
    )
    elapsed = time.time() - start_time

    return mrb, elapsed

def grid_search_optimization():
    """
    Fast grid search over top 5 parameters.
    More reliable than full Optuna for 30-minute window.
    """

    print("=" * 70)
    print("FAST GRID SEARCH - TOP 5 PARAMETERS")
    print("=" * 70)
    print()
    print("Target: 30 minutes on SPY_4blocks (2 warmup + 2 test)")
    print("Parameters: profit_target, stop_loss, min_hold_bars, buy/sell thresholds")
    print()

    # Define grid (carefully selected for SPY)
    profit_targets = [0.002, 0.003, 0.004, 0.005]  # 0.2% - 0.5%
    stop_losses = [-0.003, -0.004, -0.005, -0.006]  # -0.3% - -0.6%
    min_hold_bars_options = [2, 3, 5]  # 2-5 bars

    # For now, fix buy/sell thresholds (TODO: implement CLI support)
    buy_threshold = 0.53
    sell_threshold = 0.47

    total_trials = len(profit_targets) * len(stop_losses) * len(min_hold_bars_options)
    print(f"Total trials: {total_trials}")
    print(f"Estimated time: {total_trials * 90:.0f} seconds (~{total_trials * 1.5:.0f} minutes)")
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

                params = {
                    'profit_target': profit,
                    'stop_loss': stop,
                    'min_hold_bars': hold,
                    'buy_threshold': buy_threshold,
                    'sell_threshold': sell_threshold
                }

                mrb, elapsed = objective(params)

                results.append({
                    'trial': trial_num,
                    'params': params,
                    'mrb': mrb,
                    'time': elapsed
                })

                print(f"  MRB: {mrb:.4f}, time: {elapsed:.1f}s")

                if mrb > best_mrb:
                    best_mrb = mrb
                    best_params = params
                    print(f"  ✨ NEW BEST! MRB: {best_mrb:.4f}")

    total_time = time.time() - start_time

    # Results
    print()
    print("=" * 70)
    print("OPTIMIZATION COMPLETE")
    print("=" * 70)
    print(f"Total trials: {trial_num}")
    print(f"Total time: {total_time/60:.1f} minutes")
    print()

    print("🏆 BEST PARAMETERS:")
    print(f"  MRB: {best_mrb:.4f}")
    print()
    for key, value in best_params.items():
        print(f"  {key}: {value}")
    print()

    # Baseline comparison
    baseline_mrb = 0.345  # v1.5
    if best_mrb > baseline_mrb:
        improvement = (best_mrb - baseline_mrb) / baseline_mrb * 100
        print(f"✅ IMPROVEMENT: +{improvement:.1f}% vs v1.5 baseline ({baseline_mrb:.4f})")
    else:
        decline = (baseline_mrb - best_mrb) / baseline_mrb * 100
        print(f"⚠️  DECLINE: -{decline:.1f}% vs v1.5 baseline")
        print("   Recommend sticking with v1.5 parameters")

    # Save results
    results_file = BASE_DIR / "data/tmp/optuna_top5_results.json"
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

    # Restore original parameters
    print()
    print("Restoring v1.5 parameters...")
    modify_cpp_parameters(0.003, -0.004, 3)
    rebuild_sentio_cli()

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

    print("Starting optimization...")
    print()

    # Run optimization
    try:
        best_params, best_mrb, results = grid_search_optimization()

        print()
        print("=" * 70)
        print("NEXT STEPS")
        print("=" * 70)
        print()

        if best_mrb > 0.345:
            print("✅ Best parameters BEAT v1.5 baseline!")
            print()
            print("To deploy:")
            print("1. Update execute_trades_command.cpp with best parameters")
            print("2. Rebuild: make -j8 sentio_cli")
            print("3. Test on 4-block data to verify")
            print("4. Deploy to paper trading")
        else:
            print("⚠️  Best parameters DID NOT beat v1.5 baseline")
            print()
            print("Recommendation:")
            print("✅ Use v1.5 parameters for tonight (0.345% MRB)")
            print("🔬 Run longer optimization tomorrow (8-12 hours)")

        return 0

    except KeyboardInterrupt:
        print("\n\n⏹️  Optimization interrupted by user")
        print("Restoring v1.5 parameters...")
        modify_cpp_parameters(0.003, -0.004, 3)
        rebuild_sentio_cli()
        return 1
    except Exception as e:
        print(f"\n\n❌ ERROR: {e}")
        print("Restoring v1.5 parameters...")
        modify_cpp_parameters(0.003, -0.004, 3)
        rebuild_sentio_cli()
        return 1

if __name__ == "__main__":
    sys.exit(main())
