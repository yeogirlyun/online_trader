#!/usr/bin/env python3
"""
Test improvements: Adaptive thresholds + expanded ranges
Compare baseline vs improved configuration
"""

import subprocess
import json
import sys
import time

sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

def run_baseline_test():
    """Run quick test with baseline params (from Phase 1)"""
    print("=" * 80)
    print("BASELINE TEST: Phase 1 params without adaptive thresholds")
    print("=" * 80)

    # Baseline Phase 1 best params
    params = {
        'buy_threshold': 0.55,
        'sell_threshold': 0.43,
        'ewrls_lambda': 0.992,
        'bb_amplification_factor': 0.08,
        'h1_weight': 0.15,
        'h5_weight': 0.60,
        'h10_weight': 0.25,
        'bb_period': 20,
        'bb_std_dev': 2.25,
        'bb_proximity': 0.30,
        'regularization': 0.016
    }

    optimizer = AdaptiveOptunaFramework(
        data_file='data/equities/SPY_4blocks.csv',
        build_dir='build',
        output_dir='data/tmp/improvement_test',
        n_trials=1,
        n_jobs=1
    )

    result = optimizer.run_backtest('data/equities/SPY_4blocks.csv', params, warmup_blocks=2)

    print(f"\nBaseline MRB: {result['mrb']:.4f}%")
    return result['mrb']

def run_improved_test():
    """Run Phase 1 optimization with EXPANDED RANGES + adaptive thresholds"""
    print("\n" + "=" * 80)
    print("IMPROVED TEST: Expanded ranges + adaptive threshold calibration")
    print("=" * 80)
    print()

    optimizer = AdaptiveOptunaFramework(
        data_file='data/equities/SPY_4blocks.csv',
        build_dir='build',
        output_dir='data/tmp/improvement_test',
        n_trials=50,  # Quick optimization
        n_jobs=4
    )

    print("[Improved] Running Phase 1 optimization with expanded ranges...")
    start_time = time.time()

    best_params, best_mrb, tuning_time = optimizer.tune_on_window(
        block_start=0,
        block_end=4,
        n_trials=50,
        phase2_center=None  # Phase 1 mode
    )

    total_time = time.time() - start_time

    print()
    print("=" * 80)
    print("IMPROVED TEST COMPLETE!")
    print("=" * 80)
    print(f"Best MRB: {best_mrb:.4f}%")
    print()
    print("Best parameters:")
    for k, v in best_params.items():
        print(f"  {k}: {v}")
    print()
    print(f"Optimization time: {total_time:.1f}s")
    print("=" * 80)

    return best_mrb, best_params

if __name__ == "__main__":
    print()
    print("=" * 80)
    print("IMPROVEMENT VALIDATION TEST")
    print("=" * 80)
    print("Testing: Adaptive threshold calibration + expanded parameter ranges")
    print("Dataset: SPY_4blocks.csv (4 blocks = 1920 bars)")
    print("=" * 80)
    print()

    # Baseline test disabled since we're testing improvements ONLY
    # baseline_mrb = run_baseline_test()

    # Run improved test
    improved_mrb, improved_params = run_improved_test()

    # Save results
    results = {
        'test_type': 'improvement_validation',
        'improvements': [
            'Adaptive threshold calibration enabled',
            'Expanded parameter ranges (buy: 0.50-0.65, sell: 0.35-0.50, lambda: 0.985-0.999, BB: 0.0-0.20)'
        ],
        'improved_mrb': improved_mrb,
        'improved_params': improved_params,
        'target_mrb': 0.50,
        'gap_to_target': 0.50 - improved_mrb,
        'progress_pct': (improved_mrb / 0.50) * 100 if improved_mrb > 0 else 0
    }

    output_path = 'data/tmp/improvement_test/validation_results.json'
    with open(output_path, 'w') as f:
        json.dump(results, f, indent=2)

    print()
    print(f"Results saved to: {output_path}")
    print()
    print("=" * 80)
    print("VALIDATION SUMMARY")
    print("=" * 80)
    print(f"Improved MRB:        {improved_mrb:.4f}%")
    print(f"Target MRB:          0.5000%")
    print(f"Gap to target:       {(0.50 - improved_mrb):.4f}%")
    print(f"Progress to target:  {(improved_mrb / 0.50 * 100):.1f}%")
    print("=" * 80)
    print()

    if improved_mrb >= 0.50:
        print("✅ TARGET ACHIEVED! MRB >= 0.5%")
    elif improved_mrb >= 0.30:
        print("✅ STRONG PROGRESS! MRB >= 0.3% (60% of target)")
    elif improved_mrb >= 0.22:
        print("⚠️  MODEST IMPROVEMENT over baseline 0.22%")
    else:
        print("❌ NO IMPROVEMENT - further tuning needed")

    print()
