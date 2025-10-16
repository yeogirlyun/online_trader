#!/bin/bash

# Optuna Optimization Script for 58-Feature Set
# Created: 2025-10-08
# Purpose: Find optimal parameters for time + pattern + professional indicators

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data/equities"
OUTPUT_DIR="$PROJECT_ROOT/data/tmp/optuna_58features"

# Data files
DATA_FILE="$DATA_DIR/SPY_30blocks.csv"

# Optuna parameters
STRATEGY="C"              # C = Static baseline (tune once, deploy fixed)
N_TRIALS=200              # Number of optimization trials
TIMEOUT_MINUTES=360       # 6 hours max (for long runs)

# Output files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_JSON="$OUTPUT_DIR/results_${TIMESTAMP}.json"
LOG_FILE="$OUTPUT_DIR/log_${TIMESTAMP}.txt"

# ============================================================================
# Validation
# ============================================================================

echo "═══════════════════════════════════════════════════════════════════"
echo "  OPTUNA OPTIMIZATION - 58 Feature Set"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# Check if data file exists
if [ ! -f "$DATA_FILE" ]; then
    echo "❌ Error: Data file not found: $DATA_FILE"
    echo ""
    echo "Available data files:"
    ls -lh "$DATA_DIR"/*.csv 2>/dev/null || echo "  No CSV files found in $DATA_DIR"
    exit 1
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "❌ Error: Build directory not found: $BUILD_DIR"
    echo "Please run 'cmake .. && make' in the build directory first"
    exit 1
fi

# Check if sentio_cli exists
if [ ! -f "$BUILD_DIR/sentio_cli" ]; then
    echo "❌ Error: sentio_cli not found in $BUILD_DIR"
    echo "Please build the project first: cd build && make sentio_cli"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# ============================================================================
# Display Configuration
# ============================================================================

echo "Configuration:"
echo "─────────────────────────────────────────────────────────────────"
echo "  Strategy:        $STRATEGY (Static baseline)"
echo "  Data:            $DATA_FILE"
echo "  Build Dir:       $BUILD_DIR"
echo "  Output:          $OUTPUT_JSON"
echo "  Log:             $LOG_FILE"
echo "  Trials:          $N_TRIALS"
echo "  Timeout:         $TIMEOUT_MINUTES minutes"
echo "─────────────────────────────────────────────────────────────────"
echo ""

# Show data file info
echo "Data File Info:"
DATA_LINES=$(wc -l < "$DATA_FILE" | xargs)
DATA_SIZE=$(ls -lh "$DATA_FILE" | awk '{print $5}')
echo "  Lines: $DATA_LINES"
echo "  Size:  $DATA_SIZE"
echo ""

# ============================================================================
# Confirmation
# ============================================================================

echo "This will run $N_TRIALS optimization trials, which may take several hours."
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 0
fi

echo ""
echo "═══════════════════════════════════════════════════════════════════"
echo "  Starting Optimization..."
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# ============================================================================
# Run Optuna
# ============================================================================

START_TIME=$(date +%s)

cd "$PROJECT_ROOT"

python3 tools/adaptive_optuna.py \
    --strategy "$STRATEGY" \
    --data "$DATA_FILE" \
    --build-dir "$BUILD_DIR" \
    --output "$OUTPUT_JSON" \
    2>&1 | tee "$LOG_FILE"

EXIT_CODE=$?

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
ELAPSED_MIN=$((ELAPSED / 60))
ELAPSED_SEC=$((ELAPSED % 60))

# ============================================================================
# Results Summary
# ============================================================================

echo ""
echo "═══════════════════════════════════════════════════════════════════"
echo "  Optimization Complete"
echo "═══════════════════════════════════════════════════════════════════"
echo ""
echo "Exit Code:    $EXIT_CODE"
echo "Duration:     ${ELAPSED_MIN}m ${ELAPSED_SEC}s"
echo ""
echo "Results:"
echo "  JSON:       $OUTPUT_JSON"
echo "  Log:        $LOG_FILE"
echo ""

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ Optimization completed successfully!"
    echo ""

    # Try to extract best parameters from results
    if [ -f "$OUTPUT_JSON" ]; then
        echo "Best Parameters:"
        echo "─────────────────────────────────────────────────────────────────"

        # Extract key metrics using Python
        python3 - <<EOF
import json
import sys

try:
    with open("$OUTPUT_JSON", "r") as f:
        results = json.load(f)

    if "best_params" in results:
        params = results["best_params"]
        print(f"  buy_threshold:           {params.get('buy_threshold', 'N/A')}")
        print(f"  sell_threshold:          {params.get('sell_threshold', 'N/A')}")
        print(f"  ewrls_lambda:            {params.get('ewrls_lambda', 'N/A')}")
        print(f"  bb_amplification_factor: {params.get('bb_amplification_factor', 'N/A')}")
        print("")

    if "best_value" in results:
        print(f"  Best MRB:                {results['best_value']:.4f}%")
        print("")

    if "training_mrb" in results:
        print(f"  Training MRB:            {results['training_mrb']:.4f}%")
        print("")

    if "test_mrb" in results:
        print(f"  Test MRB:                {results['test_mrb']:.4f}%")
        print("")

except Exception as e:
    print(f"  Could not parse results: {e}", file=sys.stderr)
    sys.exit(1)
EOF

        echo "─────────────────────────────────────────────────────────────────"
        echo ""

        # Show how to use these parameters
        echo "To test these parameters:"
        echo "  cd build"
        echo "  ./sentio_cli backtest --blocks 20 --warmup-blocks 10 \\"
        echo "    --params '{\"buy_threshold\": <value>, \"sell_threshold\": <value>}' \\"
        echo "    --data ../data/equities/SPY_30blocks.csv"
        echo ""
    fi
else
    echo "❌ Optimization failed with exit code $EXIT_CODE"
    echo ""
    echo "Check the log file for details:"
    echo "  tail -100 $LOG_FILE"
    echo ""
fi

echo "═══════════════════════════════════════════════════════════════════"
