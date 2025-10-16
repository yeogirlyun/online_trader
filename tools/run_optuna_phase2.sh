#!/bin/bash
#
# Two-Phase Optuna Optimization
#
# Phase 1: Wide search (coarse granularity) - 50 trials
# Phase 2: Narrow micro-tuning around best params (fine granularity) - 100 trials
#
# This approach improves MRB by exploring broadly first, then refining.
#

set -e  # Exit on error

# Navigate to project root
cd /Volumes/ExternalSSD/Dev/C++/online_trader

# Configuration
DATA_FILE="data/equities/SPY_4blocks.csv"
BUILD_DIR="build"
OUTPUT_DIR="data/tmp/optuna_phase2"
PHASE1_TRIALS=50
PHASE2_TRIALS=100
N_JOBS=4  # Parallel trials

# Check if data file exists
if [ ! -f "$DATA_FILE" ]; then
    echo "Error: Data file not found: $DATA_FILE"
    exit 1
fi

# Check if CLI binary exists
if [ ! -f "$BUILD_DIR/sentio_cli" ]; then
    echo "Error: sentio_cli not found in $BUILD_DIR"
    echo "Please build the project first: cd build && cmake --build . -j8"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Print configuration
echo "=========================================="
echo "Two-Phase Optuna Optimization"
echo "=========================================="
echo "Data file:       $DATA_FILE"
echo "Build dir:       $BUILD_DIR"
echo "Output dir:      $OUTPUT_DIR"
echo "Phase 1 trials:  $PHASE1_TRIALS (wide search)"
echo "Phase 2 trials:  $PHASE2_TRIALS (micro-tuning)"
echo "Parallel jobs:   $N_JOBS"
echo "Start time:      $(date)"
echo "=========================================="
echo ""

# ========================================
# PHASE 1: Wide search
# ========================================
echo ""
echo "=========================================="
echo "PHASE 1: WIDE SEARCH"
echo "=========================================="
echo "Running $PHASE1_TRIALS trials with coarse granularity..."
echo ""

python3 tools/adaptive_optuna.py \
    --strategy C \
    --data "$DATA_FILE" \
    --build-dir "$BUILD_DIR" \
    --output "$OUTPUT_DIR/phase1_results.json" \
    --n-trials "$PHASE1_TRIALS" \
    --n-jobs "$N_JOBS" \
    2>&1 | tee "$OUTPUT_DIR/phase1_log.txt"

# Extract best parameters from Phase 1
if [ ! -f "$OUTPUT_DIR/phase1_results.json" ]; then
    echo "Error: Phase 1 results not found!"
    exit 1
fi

echo ""
echo "=========================================="
echo "Phase 1 Complete!"
echo "=========================================="
echo "Best parameters from Phase 1:"
python3 -m json.tool "$OUTPUT_DIR/phase1_results.json" | grep -A 10 "best_params"
echo ""

# ========================================
# PHASE 2: Micro-tuning
# ========================================
echo ""
echo "=========================================="
echo "PHASE 2: MICRO-TUNING"
echo "=========================================="
echo "Running $PHASE2_TRIALS trials with fine granularity around best params..."
echo ""

# Create a Python script to run Phase 2 with Phase 1 results
cat > "$OUTPUT_DIR/run_phase2.py" <<'PYTHON_EOF'
#!/usr/bin/env python3
import json
import sys
import os

# Load Phase 1 results
with open('data/tmp/optuna_phase2/phase1_results.json') as f:
    phase1_data = json.load(f)

# Get best parameters from Phase 1
best_params = phase1_data.get('best_params', {})
if not best_params:
    print("Error: No best_params found in Phase 1 results!")
    sys.exit(1)

print("Phase 1 best params:")
print(f"  buy_threshold: {best_params.get('buy_threshold', 0.53)}")
print(f"  sell_threshold: {best_params.get('sell_threshold', 0.48)}")
print(f"  ewrls_lambda: {best_params.get('ewrls_lambda', 0.992)}")
print(f"  bb_amplification_factor: {best_params.get('bb_amplification_factor', 0.05)}")
print()

# Import and run Phase 2
sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptuna

# Create optimizer
optimizer = AdaptiveOptuna(
    data_file='data/equities/SPY_4blocks.csv',
    build_dir='build',
    output_dir='data/tmp/optuna_phase2',
    use_cache=False,
    n_trials=100,
    n_jobs=4
)

# Run Phase 2 with best params from Phase 1 as center
print("Running Phase 2 micro-tuning...")
best_params_phase2, best_mrb_phase2, tuning_time = optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=best_params
)

# Save Phase 2 results
phase2_results = {
    'phase': 2,
    'phase1_best_params': best_params,
    'phase1_best_mrb': phase1_data.get('best_value', 0.0),
    'phase2_best_params': best_params_phase2,
    'phase2_best_mrb': best_mrb_phase2,
    'improvement': best_mrb_phase2 - phase1_data.get('best_value', 0.0),
    'tuning_time_seconds': tuning_time
}

with open('data/tmp/optuna_phase2/phase2_results.json', 'w') as f:
    json.dump(phase2_results, f, indent=2)

print("\n" + "="*80)
print("PHASE 2 COMPLETE!")
print("="*80)
print(f"Phase 1 MRB: {phase2_results['phase1_best_mrb']:.4f}%")
print(f"Phase 2 MRB: {phase2_results['phase2_best_mrb']:.4f}%")
print(f"Improvement: {phase2_results['improvement']:+.4f}%")
print("="*80)
PYTHON_EOF

chmod +x "$OUTPUT_DIR/run_phase2.py"
python3 "$OUTPUT_DIR/run_phase2.py" 2>&1 | tee "$OUTPUT_DIR/phase2_log.txt"

# Print final results
echo ""
echo "=========================================="
echo "TWO-PHASE OPTIMIZATION COMPLETE!"
echo "=========================================="
echo "End time:        $(date)"
echo ""

if [ -f "$OUTPUT_DIR/phase2_results.json" ]; then
    echo "Phase 2 Results:"
    cat "$OUTPUT_DIR/phase2_results.json"
    echo ""
    echo "=========================================="
    echo "Updating production parameters..."
    echo "=========================================="

    # Create a temporary file with Phase 2 results in the format update_best_params.py expects
    cat > "$OUTPUT_DIR/phase2_for_update.json" <<EOF
{
  "strategy": "optuna_phase2",
  "best_params": $(cat "$OUTPUT_DIR/phase2_results.json" | python3 -c "import json, sys; d=json.load(sys.stdin); print(json.dumps(d['phase2_best_params']))"),
  "best_value": $(cat "$OUTPUT_DIR/phase2_results.json" | python3 -c "import json, sys; d=json.load(sys.stdin); print(d['phase2_best_mrb'])"),
  "total_tests": $((PHASE1_TRIALS + PHASE2_TRIALS)),
  "data_file": "$DATA_FILE"
}
EOF

    python3 tools/update_best_params.py --optuna-results "$OUTPUT_DIR/phase2_for_update.json"
else
    echo "Warning: Phase 2 results not found!"
fi

echo ""
echo "Results saved to: $OUTPUT_DIR/"
echo "  - phase1_results.json"
echo "  - phase2_results.json"
echo "  - phase1_log.txt"
echo "  - phase2_log.txt"
