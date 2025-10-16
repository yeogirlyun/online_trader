#!/bin/bash

# Daily Optuna Optimization Script for Live Trading
# Created: 2025-10-08
# Purpose: Fast daily optimization for next-day Alpaca live trading
# Strategy: 2-block warmup + 2-block test (focused on short-term performance)

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data/equities"
OUTPUT_DIR="$PROJECT_ROOT/data/tmp/daily_optuna"

# Data files - use recent 4 blocks (2 warmup + 2 test)
DATA_FILE="$DATA_DIR/SPY_4blocks.csv"

# Optuna parameters for daily optimization
STRATEGY="C"              # C = Static baseline (tune once, deploy fixed)
TRAIN_BLOCKS=2            # Train on 2 blocks (2 days)
TEST_BLOCKS=2             # Test on 2 blocks (2 days)
N_TRIALS=100              # Fewer trials for faster daily optimization (30-45 min)
TIMEOUT_MINUTES=60        # 1 hour max

# Output files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_JSON="$OUTPUT_DIR/daily_params_${TIMESTAMP}.json"
LOG_FILE="$OUTPUT_DIR/daily_log_${TIMESTAMP}.txt"

# ============================================================================
# Validation
# ============================================================================

echo "═══════════════════════════════════════════════════════════════════"
echo "  DAILY OPTUNA OPTIMIZATION - Live Trading Prep"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# Check if data file exists
if [ ! -f "$DATA_FILE" ]; then
    echo "❌ Error: Data file not found: $DATA_FILE"
    echo ""
    echo "Available data files:"
    ls -lh "$DATA_DIR"/*.csv 2>/dev/null || echo "  No CSV files found in $DATA_DIR"
    echo ""
    echo "💡 Tip: Download latest data first:"
    echo "   python3 tools/data_downloader.py SPY --days 4 --outdir data/equities"
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
echo "  Train Blocks:    $TRAIN_BLOCKS (warmup + optimization)"
echo "  Test Blocks:     $TEST_BLOCKS (validation)"
echo "  Data:            $DATA_FILE"
echo "  Build Dir:       $BUILD_DIR"
echo "  Output:          $OUTPUT_JSON"
echo "  Log:             $LOG_FILE"
echo "  Trials:          $N_TRIALS"
echo "  Est. Duration:   30-45 minutes"
echo "─────────────────────────────────────────────────────────────────"
echo ""

# Show data file info
echo "Data File Info:"
DATA_LINES=$(wc -l < "$DATA_FILE" | xargs)
DATA_SIZE=$(ls -lh "$DATA_FILE" | awk '{print $5}')
DATA_BLOCKS=$((($DATA_LINES - 1) / 390))  # Subtract header, 390 bars per block
echo "  Lines:  $DATA_LINES"
echo "  Size:   $DATA_SIZE"
echo "  Blocks: $DATA_BLOCKS (should be 4 for 2 train + 2 test)"
echo ""

# Validate block count
if [ "$DATA_BLOCKS" -lt 4 ]; then
    echo "⚠️  Warning: Only $DATA_BLOCKS blocks found, need at least 4"
    echo "    (2 for training + 2 for testing)"
    echo ""
fi

# ============================================================================
# Confirmation
# ============================================================================

echo "This will optimize parameters for tomorrow's live trading session."
echo "Focus: Short-term (1-day) performance, not long-term metrics."
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 0
fi

echo ""
echo "═══════════════════════════════════════════════════════════════════"
echo "  Starting Daily Optimization..."
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# ============================================================================
# Run Optuna with Daily Parameters
# ============================================================================

START_TIME=$(date +%s)

cd "$PROJECT_ROOT"

# Create a custom Python script call with daily parameters
python3 - <<EOF
import sys
import os
sys.path.insert(0, os.path.join('$PROJECT_ROOT', 'tools'))

from adaptive_optuna import AdaptiveOptuna

# Initialize optimizer
optimizer = AdaptiveOptuna(
    data_file='$DATA_FILE',
    build_dir='$BUILD_DIR',
    bars_per_block=390
)

# Run Strategy C with daily parameters (2 train + 2 test)
results = optimizer.strategy_c_static(
    train_blocks=$TRAIN_BLOCKS,
    test_horizon=$TEST_BLOCKS
)

# Save results
import json
output = {
    'strategy': 'C',
    'train_blocks': $TRAIN_BLOCKS,
    'test_blocks': $TEST_BLOCKS,
    'best_params': results[0]['params'] if results else {},
    'best_value': results[0]['mrb'] if results else -999,
    'training_mrb': results[0]['mrb'] if results else -999,
    'test_results': results,
    'timestamp': '$TIMESTAMP'
}

with open('$OUTPUT_JSON', 'w') as f:
    json.dump(output, f, indent=2)

print(f"\n✅ Results saved to: $OUTPUT_JSON")
EOF

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
echo "  Daily Optimization Complete"
echo "═══════════════════════════════════════════════════════════════════"
echo ""
echo "Exit Code:    $EXIT_CODE"
echo "Duration:     ${ELAPSED_MIN}m ${ELAPSED_SEC}s"
echo ""
echo "Results:"
echo "  JSON:       $OUTPUT_JSON"
echo "  Log:        $LOG_FILE"
echo ""

if [ $EXIT_CODE -eq 0 ] && [ -f "$OUTPUT_JSON" ]; then
    echo "✅ Optimization completed successfully!"
    echo ""

    # Extract best parameters
    echo "Best Parameters for Tomorrow's Live Trading:"
    echo "─────────────────────────────────────────────────────────────────"

    python3 - <<EOF
import json
import sys

try:
    with open("$OUTPUT_JSON", "r") as f:
        results = json.load(f)

    if "best_params" in results:
        params = results["best_params"]
        print(f"  buy_threshold:           {params.get('buy_threshold', 'N/A'):.4f}")
        print(f"  sell_threshold:          {params.get('sell_threshold', 'N/A'):.4f}")
        print(f"  ewrls_lambda:            {params.get('ewrls_lambda', 'N/A'):.6f}")
        print(f"  bb_amplification_factor: {params.get('bb_amplification_factor', 'N/A'):.4f}")

        if 'enable_threshold_calibration' in params:
            print(f"  threshold_calibration:   {params.get('enable_threshold_calibration', 'N/A')}")

        print("")

    if "best_value" in results:
        print(f"  Training MRB (2 blocks):  {results['best_value']:.4f}%")
        print("")

    if "test_results" in results and results["test_results"]:
        test_mrb = results["test_results"][0].get('mrb', 0)
        print(f"  Test MRB (2 blocks):      {test_mrb:.4f}%")
        print("")

except Exception as e:
    print(f"  Could not parse results: {e}", file=sys.stderr)
    sys.exit(1)
EOF

    echo "─────────────────────────────────────────────────────────────────"
    echo ""

    # Show next steps
    echo "Next Steps:"
    echo "  1. Review parameters above"
    echo "  2. Test with backtest:"
    echo "     cd build"
    echo "     ./sentio_cli backtest --blocks 2 --warmup-blocks 2 \\"
    echo "       --params '\$(cat $OUTPUT_JSON | jq -c .best_params)' \\"
    echo "       --data ../data/equities/SPY_4blocks.csv"
    echo ""
    echo "  3. Deploy to live trading:"
    echo "     ./sentio_cli live-trade \\"
    echo "       --params '\$(cat $OUTPUT_JSON | jq -c .best_params)'"
    echo ""
else
    echo "❌ Optimization failed with exit code $EXIT_CODE"
    echo ""
    echo "Check for errors above or in the log file"
    echo ""
fi

echo "═══════════════════════════════════════════════════════════════════"
