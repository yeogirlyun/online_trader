#!/bin/bash
#
# Optuna Optimization - 4 Blocks with Parallel Trials
#
# This script runs parameter optimization on SPY_4blocks.csv using:
# - 50 Optuna trials
# - 4 parallel jobs (4x speedup)
# - Strategy C (static baseline - optimize once)
# - No feature caching (deprecated)
#
# Expected runtime: ~3-5 minutes (vs 12-20 minutes without parallelization)
#

set -e  # Exit on error

# Navigate to project root
cd /Volumes/ExternalSSD/Dev/C++/online_trader

# Configuration
DATA_FILE="data/equities/SPY_4blocks.csv"
BUILD_DIR="build"
OUTPUT_DIR="data/tmp/optuna_4blocks_parallel"
N_TRIALS=50
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
echo "Optuna Optimization - 4 Blocks (Parallel)"
echo "=========================================="
echo "Data file:       $DATA_FILE"
echo "Build dir:       $BUILD_DIR"
echo "Output dir:      $OUTPUT_DIR"
echo "Trials:          $N_TRIALS"
echo "Parallel jobs:   $N_JOBS (4x speedup)"
echo "Strategy:        C (static baseline)"
echo "Start time:      $(date)"
echo "=========================================="
echo ""

# Run optimization
python3 tools/adaptive_optuna.py \
    --strategy C \
    --data "$DATA_FILE" \
    --build-dir "$BUILD_DIR" \
    --output "$OUTPUT_DIR/optuna_results.json" \
    --n-trials "$N_TRIALS" \
    --n-jobs "$N_JOBS" \
    2>&1 | tee "$OUTPUT_DIR/optuna_log.txt"

# Print results
echo ""
echo "=========================================="
echo "Optimization Complete!"
echo "=========================================="
echo "End time:        $(date)"
echo "Results:         $OUTPUT_DIR/optuna_results.json"
echo "Log:             $OUTPUT_DIR/optuna_log.txt"
echo ""

# Display best parameters if available
if [ -f "$OUTPUT_DIR/optuna_results.json" ]; then
    echo "Best parameters found:"
    cat "$OUTPUT_DIR/optuna_results.json" | python3 -m json.tool | grep -A 20 "best_params"

    echo ""
    echo "=========================================="
    echo "Updating production parameters..."
    echo "=========================================="
    python3 tools/update_best_params.py --optuna-results "$OUTPUT_DIR/optuna_results.json"
fi
