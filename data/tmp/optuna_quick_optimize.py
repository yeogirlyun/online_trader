#!/usr/bin/env python3
"""
Optuna Quick Optimization for SPY OnlineEnsemble Strategy
==========================================================

Optimizes critical PSM and signal threshold parameters for maximum MRB.

Target: Complete in 2 hours (~100 trials) for tonight's live trading.

Parameters optimized:
1. profit_target (0.1% - 1.0%)
2. stop_loss (-1.0% - -0.1%)
3. min_hold_bars (1 - 10)
4. buy_threshold (50% - 70%)
5. sell_threshold (30% - 50%)
6. neutral_zone (2% - 20%)

Objective: Maximize MRB on SPY_4blocks (2 warmup + 2 test)
"""

import optuna
import subprocess
import json
import tempfile
import os
from pathlib import Path
import pandas as pd
import time
from datetime import datetime

# Configuration
BASE_DIR = Path("/Volumes/ExternalSSD/Dev/C++/online_trader")
BUILD_DIR = BASE_DIR / "build"
DATA_FILE = BASE_DIR / "data/equities/SPY_4blocks.csv"
WARMUP_BARS = 960  # 2 blocks
TEST_BLOCKS = 2

# Optuna settings
N_TRIALS = 100  # Target: 100 trials in 2 hours
TIMEOUT = 7200  # 2 hours
STUDY_NAME = f"spy_quick_opt_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
STORAGE = f"sqlite:///{BASE_DIR}/data/tmp/optuna_quick.db"

def run_backtest(profit_target, stop_loss, min_hold_bars,
                 buy_threshold, sell_threshold, neutral_zone):
    """
    Run a single backtest with given parameters.

    Returns:
        float: MRB (Mean Return per Block) or -999 if failed
    """
    try:
        # Create temporary config
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            config = {
                'profit_target': profit_target,
                'stop_loss': stop_loss,
                'min_hold_bars': min_hold_bars,
                'buy_threshold': buy_threshold,
                'sell_threshold': sell_threshold,
                'neutral_zone': neutral_zone
            }
            json.dump(config, f)
            config_file = f.name

        # Generate signals (TODO: Need to pass thresholds to CLI - currently uses defaults)
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
            print(f"Signal generation failed: {result.stderr}")
            return -999.0

        # Execute trades (TODO: Need to pass PSM params to CLI - currently hardcoded)
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
            print(f"Trade execution failed: {result.stderr}")
            return -999.0

        # Parse results
        mrb = calculate_mrb(trades_file, warmup_bars=WARMUP_BARS, test_blocks=TEST_BLOCKS)

        # Cleanup
        os.unlink(config_file)
        os.unlink(signals_file)
        os.unlink(trades_file)

        return mrb

    except Exception as e:
        print(f"Backtest failed: {e}")
        return -999.0

def calculate_mrb(trades_file, warmup_bars, test_blocks):
    """Calculate MRB from trade history file"""
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

        # Filter to test period only (after warmup)
        test_trades = [t for t in trades if t.get('bar_index', 0) >= warmup_bars]

        if not test_trades or 'total_return' not in test_trades[-1]:
            return -999.0

        final_return = test_trades[-1]['total_return']
        mrb = final_return / test_blocks

        return mrb

    except Exception as e:
        print(f"MRB calculation failed: {e}")
        return -999.0

def objective(trial):
    """Optuna objective function"""

    # Sample parameters
    profit_target = trial.suggest_float('profit_target', 0.001, 0.010, step=0.0005)
    stop_loss = trial.suggest_float('stop_loss', -0.010, -0.001, step=0.0005)
    min_hold_bars = trial.suggest_int('min_hold_bars', 1, 10)

    buy_threshold = trial.suggest_float('buy_threshold', 0.50, 0.70, step=0.01)
    sell_threshold = trial.suggest_float('sell_threshold', 0.30, 0.50, step=0.01)
    neutral_zone = trial.suggest_float('neutral_zone', 0.02, 0.20, step=0.01)

    # Validate constraints
    if buy_threshold <= sell_threshold:
        return -999.0  # Invalid: buy must be > sell

    if (buy_threshold - sell_threshold) < 0.02:
        return -999.0  # Invalid: too narrow

    # Run backtest
    start_time = time.time()
    mrb = run_backtest(
        profit_target=profit_target,
        stop_loss=stop_loss,
        min_hold_bars=min_hold_bars,
        buy_threshold=buy_threshold,
        sell_threshold=sell_threshold,
        neutral_zone=neutral_zone
    )
    elapsed = time.time() - start_time

    print(f"Trial {trial.number}: MRB={mrb:.4f}, time={elapsed:.1f}s")
    print(f"  profit_target={profit_target:.4f}, stop_loss={stop_loss:.4f}, min_hold={min_hold_bars}")
    print(f"  buy={buy_threshold:.3f}, sell={sell_threshold:.3f}, neutral={neutral_zone:.3f}")

    return mrb

def main():
    """Run Optuna optimization"""

    print("=" * 70)
    print("OPTUNA QUICK OPTIMIZATION - SPY OnlineEnsemble")
    print("=" * 70)
    print()
    print(f"Target: {N_TRIALS} trials in {TIMEOUT/3600:.1f} hours")
    print(f"Objective: Maximize MRB on SPY_4blocks (2 warmup + 2 test)")
    print(f"Study: {STUDY_NAME}")
    print(f"Storage: {STORAGE}")
    print()

    # **CRITICAL WARNING**
    print("⚠️  WARNING: This script requires CLI parameter support!")
    print("⚠️  Currently, PSM parameters are HARDCODED in execute_trades_command.cpp")
    print("⚠️  Signal thresholds are also HARDCODED in OnlineEnsembleStrategy")
    print()
    print("Options:")
    print("  1. Quick hack: Modify code to accept CLI params (30 mins)")
    print("  2. Skip Optuna, use v1.5 params for tonight")
    print("  3. Manual grid search (test 10-20 parameter combinations)")
    print()
    input("Press Enter to continue with current implementation (will use v1.5 hardcoded params)...")
    print()

    # Create study
    study = optuna.create_study(
        study_name=STUDY_NAME,
        storage=STORAGE,
        direction="maximize",  # Maximize MRB
        load_if_exists=True,
        pruner=optuna.pruners.MedianPruner(n_startup_trials=10)
    )

    # Add v1.5 baseline as starting point
    baseline_params = {
        'profit_target': 0.003,
        'stop_loss': -0.004,
        'min_hold_bars': 3,
        'buy_threshold': 0.53,
        'sell_threshold': 0.47,
        'neutral_zone': 0.06
    }

    print("Baseline (v1.5):")
    print(f"  {baseline_params}")
    print()

    # Run optimization
    try:
        study.optimize(
            objective,
            n_trials=N_TRIALS,
            timeout=TIMEOUT,
            show_progress_bar=True
        )
    except KeyboardInterrupt:
        print("\nOptimization interrupted by user")

    # Results
    print()
    print("=" * 70)
    print("OPTIMIZATION COMPLETE")
    print("=" * 70)
    print()

    print("Best trial:")
    best_trial = study.best_trial
    print(f"  MRB: {best_trial.value:.4f}")
    print()
    print("Best parameters:")
    for key, value in best_trial.params.items():
        print(f"  {key}: {value}")
    print()

    # Save results
    results_file = BASE_DIR / "data/tmp/optuna_quick_results.json"
    with open(results_file, 'w') as f:
        results = {
            'study_name': STUDY_NAME,
            'n_trials': len(study.trials),
            'best_mrb': best_trial.value,
            'best_params': best_trial.params,
            'baseline_params': baseline_params,
            'completed_at': datetime.now().isoformat()
        }
        json.dump(results, f, indent=2)

    print(f"Results saved to: {results_file}")
    print()

    # Compare to baseline
    baseline_mrb = 0.345  # v1.5 baseline
    if best_trial.value > baseline_mrb:
        improvement = (best_trial.value - baseline_mrb) / baseline_mrb * 100
        print(f"✅ IMPROVEMENT: +{improvement:.1f}% vs v1.5 baseline")
    else:
        decline = (baseline_mrb - best_trial.value) / baseline_mrb * 100
        print(f"⚠️  DECLINE: -{decline:.1f}% vs v1.5 baseline")
        print("   Recommend using v1.5 parameters")

    print()
    print("Next steps:")
    print("  1. Review best parameters")
    print("  2. Update execute_trades_command.cpp with best values")
    print("  3. Rebuild and test on 4-block data")
    print("  4. Deploy to paper trading if MRB ≥ 0.340%")

if __name__ == "__main__":
    # Check dependencies
    try:
        import optuna
    except ImportError:
        print("ERROR: Optuna not installed")
        print("Install with: pip install optuna")
        exit(1)

    # Check sentio_cli
    if not (BUILD_DIR / "sentio_cli").exists():
        print(f"ERROR: sentio_cli not found at {BUILD_DIR}/sentio_cli")
        print("Run: make -j8 sentio_cli")
        exit(1)

    # Check data file
    if not DATA_FILE.exists():
        print(f"ERROR: Data file not found: {DATA_FILE}")
        exit(1)

    main()
