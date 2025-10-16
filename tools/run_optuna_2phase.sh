#!/bin/bash
#
# Two-Phase Optuna Optimization
#
# Phase 1: Optimize buy/sell thresholds, lambda, BB amplification (50 trials)
# Phase 2: Fix Phase 1 params, optimize horizon weights, BB params, regularization (100 trials)
#
# Expected improvement: Phase 1 gets to ~0.17% MRB, Phase 2 fine-tunes to reach 0.5%+ MRB target
#

set -e

cd /Volumes/ExternalSSD/Dev/C++/online_trader

DATA_FILE="data/equities/SPY_4blocks.csv"
OUTPUT_DIR="data/tmp/optuna_2phase"
PHASE1_TRIALS=50
PHASE2_TRIALS=100
N_JOBS=4

mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "TWO-PHASE OPTUNA OPTIMIZATION"
echo "=========================================="
echo "Data: $DATA_FILE"
echo "Phase 1: $PHASE1_TRIALS trials (primary params)"
echo "Phase 2: $PHASE2_TRIALS trials (secondary params)"
echo "Parallel jobs: $N_JOBS"
echo "Start time: $(date)"
echo "=========================================="
echo ""

# PHASE 1
echo "=========================================="
echo "PHASE 1: Optimizing primary parameters"
echo "=========================================="
python3 tools/adaptive_optuna.py \
    --strategy C \
    --data "$DATA_FILE" \
    --build-dir build \
    --output "$OUTPUT_DIR/phase1_results.json" \
    --n-trials "$PHASE1_TRIALS" \
    --n-jobs "$N_JOBS" 2>&1 | tee "$OUTPUT_DIR/phase1_log.txt"

echo ""
echo "Phase 1 complete! Best MRB from Phase 1:"
python3 -c "import json; d=json.load(open('$OUTPUT_DIR/phase1_results.json')); print(f\"  MRB: {d['best_value']:.4f}%\"); print(f\"  Params: {d['best_params']}\")"
echo ""

# PHASE 2
echo "=========================================="
echo "PHASE 2: Optimizing secondary parameters"
echo "=========================================="

# Extract Phase 1 best params and run Phase 2
python3 << 'PYTHON_EOF'
import json
import sys
sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptuna

# Load Phase 1 results
with open('data/tmp/optuna_2phase/phase1_results.json') as f:
    phase1 = json.load(f)

phase1_best = phase1['best_params']
phase1_mrb = phase1['best_value']

print(f"Using Phase 1 best params as FIXED:")
print(f"  buy_threshold: {phase1_best['buy_threshold']}")
print(f"  sell_threshold: {phase1_best['sell_threshold']}")
print(f"  ewrls_lambda: {phase1_best['ewrls_lambda']}")
print(f"  bb_amplification_factor: {phase1_best['bb_amplification_factor']}")
print()

# Run Phase 2
optimizer = AdaptiveOptuna(
    data_file='data/equities/SPY_4blocks.csv',
    build_dir='build',
    output_dir='data/tmp/optuna_2phase',
    n_trials=100,
    n_jobs=4
)

best_params, best_mrb, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=phase1_best  # Use Phase 1 best as center
)

# Save Phase 2 results
phase2_results = {
    'phase': 2,
    'phase1_best_params': phase1_best,
    'phase1_mrb': phase1_mrb,
    'phase2_best_params': best_params,
    'phase2_mrb': best_mrb,
    'improvement': best_mrb - phase1_mrb,
    'tuning_time': tuning_time
}

with open('data/tmp/optuna_2phase/phase2_results.json', 'w') as f:
    json.dump(phase2_results, f, indent=2)

print("\n" + "="*80)
print("PHASE 2 COMPLETE!")
print("="*80)
print(f"Phase 1 MRB: {phase1_mrb:.4f}%")
print(f"Phase 2 MRB: {best_mrb:.4f}%")
print(f"Improvement: {phase2_results['improvement']:+.4f}%")
print("="*80)
PYTHON_EOF

echo ""
echo "=========================================="
echo "TWO-PHASE OPTIMIZATION COMPLETE!"
echo "=========================================="
echo "Results saved to: $OUTPUT_DIR/"
echo "  - phase1_results.json"
echo "  - phase2_results.json"
echo "End time: $(date)"
