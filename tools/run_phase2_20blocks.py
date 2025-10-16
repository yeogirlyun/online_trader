#!/usr/bin/env python3
"""
Phase 2 Optuna Optimization on 20 Blocks

Fixes Phase 1 best parameters and optimizes secondary parameters:
- Horizon weights (h1, h5, h10)
- Bollinger Bands parameters (period, std_dev, proximity)
- EWRLS regularization

Uses constrained weight sampling to guarantee sum = 1.0
"""

import json
import sys
import time

sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

# Phase 1 best parameters (from phase1_results_fixed.json)
PHASE1_BEST = {
    'buy_threshold': 0.52,
    'sell_threshold': 0.45,
    'ewrls_lambda': 0.994,
    'bb_amplification_factor': 0.09
}

PHASE1_MRB = 0.207

print("=" * 80)
print("PHASE 2 OPTIMIZATION - 20 BLOCKS")
print("=" * 80)
print(f"Dataset: data/equities/SPY_20blocks.csv")
print(f"Trials: 100")
print(f"Parallel jobs: 4")
print(f"")
print(f"Phase 1 best params (FIXED):")
for k, v in PHASE1_BEST.items():
    print(f"  {k}: {v}")
print(f"Phase 1 MRB: {PHASE1_MRB:.4f}%")
print("=" * 80)
print("")

# Initialize optimizer
optimizer = AdaptiveOptunaFramework(
    data_file='data/equities/SPY_20blocks.csv',
    build_dir='build',
    output_dir='data/tmp/optuna_2phase',
    n_trials=100,
    n_jobs=4
)

print(f"[Phase 2] Starting optimization...")
start_time = time.time()

# Run Phase 2 optimization with Phase 1 params fixed
best_params, best_mrb, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=PHASE1_BEST
)

total_time = time.time() - start_time

print("")
print("=" * 80)
print("PHASE 2 COMPLETE!")
print("=" * 80)
print(f"Phase 1 MRB: {PHASE1_MRB:.4f}%")
print(f"Phase 2 MRB: {best_mrb:.4f}%")
print(f"Improvement: {best_mrb - PHASE1_MRB:+.4f}%")
print(f"Relative improvement: {(best_mrb / PHASE1_MRB - 1) * 100:+.2f}%")
print(f"Total time: {total_time:.1f}s")
print("=" * 80)
print("")
print("Phase 2 best params:")
for k, v in best_params.items():
    if k not in PHASE1_BEST:  # Only show Phase 2 params
        print(f"  {k}: {v}")
print("")

# Save results
phase2_results = {
    'phase': 2,
    'dataset': 'SPY_20blocks.csv',
    'phase1_best_params': PHASE1_BEST,
    'phase1_mrb': PHASE1_MRB,
    'phase2_best_params': best_params,
    'phase2_mrb': best_mrb,
    'improvement_absolute': best_mrb - PHASE1_MRB,
    'improvement_relative': (best_mrb / PHASE1_MRB - 1) * 100,
    'n_trials': 100,
    'tuning_time': tuning_time,
    'total_time': total_time
}

output_path = 'data/tmp/optuna_2phase/phase2_20blocks_results.json'
with open(output_path, 'w') as f:
    json.dump(phase2_results, f, indent=2)

print(f"Results saved to: {output_path}")
print("")

# Show comparison table
print("COMPARISON: Phase 1 (4 blocks) vs Phase 2 (20 blocks)")
print("-" * 80)
print(f"{'Metric':<30} {'Phase 1':<15} {'Phase 2':<15} {'Change':<15}")
print("-" * 80)
print(f"{'MRB':<30} {PHASE1_MRB:.4f}%{' '*9} {best_mrb:.4f}%{' '*9} {best_mrb - PHASE1_MRB:+.4f}%")
print(f"{'Dataset size':<30} {'4 blocks':<15} {'20 blocks':<15} {'+400%':<15}")
print(f"{'Target (0.5%)':<30} {PHASE1_MRB/0.5*100:.1f}%{' '*8} {best_mrb/0.5*100:.1f}%{' '*8} -")
print("-" * 80)
print("")

if best_mrb >= 0.5:
    print("🎉 TARGET REACHED! MRB >= 0.5%")
elif best_mrb > PHASE1_MRB:
    print(f"✅ IMPROVEMENT! Phase 2 increased MRB by {(best_mrb / PHASE1_MRB - 1) * 100:+.2f}%")
    print(f"   Still need {(0.5 / best_mrb - 1) * 100:+.1f}% more to reach 0.5% target")
else:
    print(f"⚠️  REGRESSION: Phase 2 decreased MRB by {(1 - best_mrb / PHASE1_MRB) * 100:.2f}%")
    print(f"   Need to investigate why Phase 2 made performance worse")
print("")
