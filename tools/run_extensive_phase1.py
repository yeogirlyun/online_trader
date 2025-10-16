#!/usr/bin/env python3
"""
Extensive Phase 1 Optimization: 200+ trials on 4 blocks
Target: 0.5% MRB with expanded parameter ranges
"""

import json
import sys
import time

sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

print("=" * 80)
print("EXTENSIVE PHASE 1 OPTIMIZATION")
print("=" * 80)
print("Dataset: SPY_4blocks.csv (1920 bars)")
print("Trials: 200 (extensive search)")
print("Target: 0.5% MRB")
print("Baseline: 0.22% MRB (from previous Phase 1)")
print()
print("Expanded parameter ranges:")
print("  buy_threshold: [0.50, 0.65]")
print("  sell_threshold: [0.35, 0.50]")
print("  ewrls_lambda: [0.985, 0.999]")
print("  bb_amplification_factor: [0.00, 0.20]")
print("=" * 80)
print()

optimizer = AdaptiveOptunaFramework(
    data_file='data/equities/SPY_4blocks.csv',
    build_dir='build',
    output_dir='data/tmp/extensive_phase1',
    n_trials=200,
    n_jobs=4
)

print("[Phase 1] Running extensive optimization (200 trials)...")
start_time = time.time()

best_params, best_mrb, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=4,
    n_trials=200,
    phase2_center=None  # Phase 1 mode
)

total_time = time.time() - start_time

print()
print("=" * 80)
print("PHASE 1 OPTIMIZATION COMPLETE!")
print("=" * 80)
print(f"Best MRB: {best_mrb:.4f}%")
print()
print("Best parameters:")
for k, v in best_params.items():
    print(f"  {k}: {v}")
print()
print(f"Optimization time: {total_time:.1f}s ({total_time/60:.1f}min)")
print("=" * 80)
print()

# Compare to target
if best_mrb >= 0.50:
    print("✅ TARGET ACHIEVED! MRB >= 0.5%")
    print("   No need for Phase 2 or regime detection!")
elif best_mrb >= 0.35:
    print("✅ STRONG PROGRESS! MRB >= 0.35%")
    print(f"   Gap to target: {(0.50 - best_mrb):.4f}%")
    print("   Recommendation: Run Phase 2 on 20 blocks")
elif best_mrb >= 0.25:
    print("⚠️  MODEST IMPROVEMENT over baseline 0.22%")
    print(f"   Gap to target: {(0.50 - best_mrb):.4f}%")
    print("   Recommendation: Integrate regime detection")
else:
    print("❌ NO SIGNIFICANT IMPROVEMENT")
    print(f"   Gap to target: {(0.50 - best_mrb):.4f}%")
    print("   Recommendation: Regime detection + new features required")

# Save results
results = {
    'optimization_type': 'extensive_phase1',
    'dataset': 'SPY_4blocks.csv',
    'n_trials': 200,
    'best_mrb': best_mrb,
    'best_params': best_params,
    'target_mrb': 0.50,
    'baseline_mrb': 0.22,
    'improvement_absolute': best_mrb - 0.22,
    'improvement_relative_pct': ((best_mrb / 0.22) - 1) * 100 if best_mrb > 0 else 0,
    'gap_to_target': 0.50 - best_mrb,
    'progress_to_target_pct': (best_mrb / 0.50) * 100 if best_mrb > 0 else 0,
    'tuning_time': tuning_time,
    'total_time': total_time
}

output_path = 'data/tmp/extensive_phase1/results.json'
with open(output_path, 'w') as f:
    json.dump(results, f, indent=2)

print()
print(f"Results saved to: {output_path}")
print()
