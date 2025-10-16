#!/usr/bin/env python3
"""
Phase 2: Optimize secondary params on 20 blocks using Phase 1 best params
"""

import json
import sys
import time

sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

# Phase 1 best parameters (from recent 4 blocks, Trial 13, MRB=0.22%)
PHASE1_BEST = {
    'buy_threshold': 0.55,
    'sell_threshold': 0.43,
    'ewrls_lambda': 0.992,
    'bb_amplification_factor': 0.08
}

PHASE1_MRB = 0.22

print("=" * 80)
print("PHASE 2: ROBUSTNESS TEST ON 20 BLOCKS")
print("=" * 80)
print(f"Using Phase 1 best params (from recent 4 blocks)")
print(f"Phase 1 MRB (4 blocks): {PHASE1_MRB:.2f}%")
print()
print("Fixed Phase 1 parameters:")
for k, v in PHASE1_BEST.items():
    print(f"  {k}: {v}")
print()
print("Optimizing Phase 2 parameters on 20 blocks:")
print("  - Horizon weights (h1, h5, h10)")
print("  - BB parameters (period, std_dev, proximity)")
print("  - Regularization")
print("=" * 80)
print()

# Initialize optimizer for 20 blocks
optimizer = AdaptiveOptunaFramework(
    data_file='data/equities/SPY_20blocks.csv',
    build_dir='build',
    output_dir='data/tmp/optuna_2phase_corrected',
    n_trials=100,
    n_jobs=4
)

print(f"[Phase 2] Running 100 trials on 20 blocks...")
start_time = time.time()

# Run Phase 2 with Phase 1 params fixed
best_params, best_mrb, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=PHASE1_BEST
)

total_time = time.time() - start_time

print()
print("=" * 80)
print("PHASE 2 COMPLETE!")
print("=" * 80)
print(f"Phase 1 MRB (4 blocks):    {PHASE1_MRB:.4f}%")
print(f"Phase 2 MRB (20 blocks):   {best_mrb:.4f}%")
print()

if best_mrb > PHASE1_MRB:
    improvement_pct = (best_mrb / PHASE1_MRB - 1) * 100
    print(f"✅ IMPROVEMENT: +{improvement_pct:.1f}%")
    print(f"   Phase 2 params improved performance even on longer horizon!")
elif best_mrb > 0:
    degradation_pct = (1 - best_mrb / PHASE1_MRB) * 100
    print(f"⚠️  DEGRADATION: -{degradation_pct:.1f}% (but still positive MRB)")
    print(f"   This is EXPECTED - 20 blocks harder than 4 blocks")
    print(f"   Phase 1 params optimized for recent data")
else:
    print(f"❌ NEGATIVE MRB: Phase 1 params don't generalize")

print()
print(f"Target: 0.5% MRB")
print(f"Current: {best_mrb:.4f}% MRB")
print(f"Gap: {(0.5 - best_mrb):.4f}%")
print(f"Progress: {(best_mrb / 0.5 * 100):.1f}% of target")
print()
print(f"Optimization time: {total_time:.1f}s")
print("=" * 80)
print()
print("Phase 2 best params:")
for k, v in best_params.items():
    if k not in PHASE1_BEST:
        print(f"  {k}: {v}")
print()

# Save results
phase2_results = {
    'approach': 'recent_optimization_corrected',
    'phase1_dataset': 'SPY_4blocks.csv',
    'phase2_dataset': 'SPY_20blocks.csv',
    'phase1_best_params': PHASE1_BEST,
    'phase1_mrb_4blocks': PHASE1_MRB,
    'phase2_best_params': best_params,
    'phase2_mrb_20blocks': best_mrb,
    'improvement_absolute': best_mrb - PHASE1_MRB,
    'improvement_relative_pct': (best_mrb / PHASE1_MRB - 1) * 100 if PHASE1_MRB > 0 else None,
    'distance_to_target': 0.5 - best_mrb,
    'progress_to_target_pct': (best_mrb / 0.5 * 100) if best_mrb > 0 else 0,
    'n_trials': 100,
    'tuning_time': tuning_time,
    'total_time': total_time
}

output_path = 'data/tmp/optuna_2phase_corrected/phase2_final_results.json'
with open(output_path, 'w') as f:
    json.dump(phase2_results, f, indent=2)

print(f"Results saved to: {output_path}")
print()
