#!/bin/bash
#
# Corrected Two-Phase Optimization Strategy
#
# PHASE 1: Optimize on RECENT 4 blocks (most relevant for next day trading)
#          → Get best buy/sell thresholds, lambda, BB amplification
#
# PHASE 2: Use Phase 1 best params, optimize secondary params on 20 blocks
#          → Test robustness on longer horizon while fine-tuning
#
# This approach balances:
# - Recency bias (Phase 1 on 4 blocks = ~1 week of trading)
# - Robustness testing (Phase 2 on 20 blocks = ~5 weeks)
#

set -e
cd /Volumes/ExternalSSD/Dev/C++/online_trader

OUTPUT_DIR="data/tmp/optuna_2phase_corrected"
mkdir -p "$OUTPUT_DIR"

echo "================================================================================"
echo "TWO-PHASE OPTIMIZATION (CORRECTED APPROACH)"
echo "================================================================================"
echo "Strategy: Optimize recent, test robust"
echo ""
echo "PHASE 1: Optimize primary params on RECENT 4 blocks"
echo "  → Focus on recent market behavior (last ~1 week)"
echo "  → Find best buy/sell thresholds, lambda, BB amplification"
echo ""
echo "PHASE 2: Fix Phase 1 params, optimize secondary on 20 blocks"
echo "  → Test robustness across longer horizon (~5 weeks)"
echo "  → Fine-tune horizon weights, BB params, regularization"
echo ""
echo "Start time: $(date)"
echo "================================================================================"
echo ""

# ============================================================================
# PHASE 1: RECENT 4 BLOCKS
# ============================================================================
echo "================================================================================"
echo "PHASE 1: Optimizing on RECENT 4 blocks"
echo "================================================================================"
python3 tools/adaptive_optuna.py \
    --strategy C \
    --data data/equities/SPY_4blocks.csv \
    --build-dir build \
    --output "$OUTPUT_DIR/phase1_results.json" \
    --n-trials 50 \
    --n-jobs 4 \
    2>&1 | tee "$OUTPUT_DIR/phase1_log.txt"

echo ""
echo "Phase 1 complete! Best parameters from recent 4 blocks:"
python3 -c "
import json
with open('$OUTPUT_DIR/phase1_results.json') as f:
    d = json.load(f)
print(f'  MRB: {d[\"best_value\"]:.4f}%')
print(f'  buy_threshold: {d[\"best_params\"][\"buy_threshold\"]}')
print(f'  sell_threshold: {d[\"best_params\"][\"sell_threshold\"]}')
print(f'  ewrls_lambda: {d[\"best_params\"][\"ewrls_lambda\"]}')
print(f'  bb_amplification_factor: {d[\"best_params\"][\"bb_amplification_factor\"]}')
"
echo ""

# ============================================================================
# PHASE 2: ROBUSTNESS TEST ON 20 BLOCKS
# ============================================================================
echo "================================================================================"
echo "PHASE 2: Testing robustness + optimizing secondary params on 20 blocks"
echo "================================================================================"

python3 << 'PYTHON_EOF'
import json
import sys
import time
sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

# Load Phase 1 results
with open('data/tmp/optuna_2phase_corrected/phase1_results.json') as f:
    phase1 = json.load(f)

phase1_best = phase1['best_params']
phase1_mrb = phase1['best_value']

print(f"Using Phase 1 best params (from recent 4 blocks) as FIXED:")
for k, v in phase1_best.items():
    print(f"  {k}: {v}")
print(f"Phase 1 MRB (4 blocks): {phase1_mrb:.4f}%")
print()

# Initialize optimizer for 20 blocks
optimizer = AdaptiveOptunaFramework(
    data_file='data/equities/SPY_20blocks.csv',
    build_dir='build',
    output_dir='data/tmp/optuna_2phase_corrected',
    n_trials=100,
    n_jobs=4
)

print("[Phase 2] Running 100 trials on 20 blocks...")
print("[Phase 2] This tests if Phase 1 params generalize to longer horizon")
print()

start_time = time.time()

# Run Phase 2 with Phase 1 params fixed
best_params, best_mrb, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=phase1_best
)

total_time = time.time() - start_time

print()
print("=" * 80)
print("PHASE 2 COMPLETE!")
print("=" * 80)
print(f"Phase 1 MRB (4 blocks):   {phase1_mrb:.4f}%")
print(f"Phase 2 MRB (20 blocks):  {best_mrb:.4f}%")
print()

if best_mrb > phase1_mrb:
    improvement_pct = (best_mrb / phase1_mrb - 1) * 100
    print(f"✅ IMPROVEMENT: +{improvement_pct:.1f}% (Phase 2 params improved performance)")
elif best_mrb > 0:
    degradation_pct = (1 - best_mrb / phase1_mrb) * 100
    print(f"⚠️  DEGRADATION: -{degradation_pct:.1f}% (but still positive MRB)")
    print(f"   This is EXPECTED - 20 blocks is harder than 4 blocks")
    print(f"   Phase 1 params optimized for recent data, may not generalize perfectly")
else:
    print(f"❌ NEGATIVE MRB: Phase 1 params don't generalize to 20 blocks")

print()
print(f"Distance to target (0.5%): {(0.5 - best_mrb):.4f}% remaining")
print(f"Optimization time: {total_time:.1f}s")
print("=" * 80)
print()
print("Phase 2 best params (optimized for robustness):")
for k, v in best_params.items():
    if k not in phase1_best:
        print(f"  {k}: {v}")
print()

# Save results
phase2_results = {
    'approach': 'recent_optimization',
    'phase1_dataset': 'SPY_4blocks.csv',
    'phase2_dataset': 'SPY_20blocks.csv',
    'phase1_best_params': phase1_best,
    'phase1_mrb_4blocks': phase1_mrb,
    'phase2_best_params': best_params,
    'phase2_mrb_20blocks': best_mrb,
    'improvement_absolute': best_mrb - phase1_mrb,
    'improvement_relative_pct': (best_mrb / phase1_mrb - 1) * 100 if phase1_mrb > 0 else None,
    'n_trials': 100,
    'tuning_time': tuning_time,
    'total_time': total_time
}

output_path = 'data/tmp/optuna_2phase_corrected/phase2_results.json'
with open(output_path, 'w') as f:
    json.dump(phase2_results, f, indent=2)

print(f"Results saved to: {output_path}")
PYTHON_EOF

echo ""
echo "================================================================================"
echo "TWO-PHASE OPTIMIZATION COMPLETE!"
echo "================================================================================"
echo "Results saved to: $OUTPUT_DIR/"
echo "  - phase1_results.json (4 blocks - recent optimization)"
echo "  - phase2_results.json (20 blocks - robustness test)"
echo "End time: $(date)"
echo ""
