#!/bin/bash
# =============================================================================
# A/B/C Test Runner for Adaptive Optuna Strategies
# =============================================================================
# Orchestrates execution of three adaptive strategies:
#   Strategy A: Per-block adaptive (retune every block)
#   Strategy B: Twice-daily adaptive (9:30 AM and 12:45 PM)
#   Strategy C: Static baseline (tune once, fixed params)
#
# Usage:
#   ./ab_test_runner.sh [--data DATA_FILE] [--strategies A,B,C]
#
# Author: Claude Code
# Date: 2025-10-08
# =============================================================================

set -e

# Configuration
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
TOOLS_DIR="$PROJECT_ROOT/tools"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data/equities"
RESULTS_DIR="$PROJECT_ROOT/data/tmp/ab_test_results"

# Default parameters
DATA_FILE="$DATA_DIR/SPY_100blocks.csv"
STRATEGIES="A,B,C"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --data)
            DATA_FILE="$2"
            shift 2
            ;;
        --strategies)
            STRATEGIES="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [--data DATA_FILE] [--strategies A,B,C]"
            echo ""
            echo "Options:"
            echo "  --data        Path to data CSV file (default: SPY_100blocks.csv)"
            echo "  --strategies  Comma-separated list of strategies to run (default: A,B,C)"
            echo "  --help        Show this help message"
            echo ""
            echo "Strategies:"
            echo "  A - Per-block adaptive (retune every block, ~6.5 hours)"
            echo "  B - Twice-daily adaptive (9:30 AM and 12:45 PM)"
            echo "  C - Static baseline (tune once, fixed params)"
            echo ""
            echo "Example:"
            echo "  $0 --data data/equities/SPY_50blocks.csv --strategies B,C"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Create results directory
mkdir -p "$RESULTS_DIR"

# Check if data file exists
if [[ ! -f "$DATA_FILE" ]]; then
    echo "❌ Data file not found: $DATA_FILE"
    exit 1
fi

# Check if sentio_cli exists
if [[ ! -f "$BUILD_DIR/sentio_cli" ]]; then
    echo "❌ sentio_cli not found in $BUILD_DIR"
    echo "   Please build the project first"
    exit 1
fi

echo "==================================================================="
echo "A/B/C ADAPTIVE OPTUNA TEST"
echo "==================================================================="
echo "Data file: $DATA_FILE"
echo "Strategies: $STRATEGIES"
echo "Results dir: $RESULTS_DIR"
echo "==================================================================="
echo ""

# Parse strategies
IFS=',' read -ra STRATEGY_ARRAY <<< "$STRATEGIES"

# Track start time
TEST_START_TIME=$(date +%s)

# Run each strategy
for STRATEGY in "${STRATEGY_ARRAY[@]}"; do
    STRATEGY=$(echo "$STRATEGY" | tr -d ' ')  # Trim whitespace

    case "$STRATEGY" in
        A)
            STRATEGY_NAME="Per-Block Adaptive"
            OUTPUT_FILE="$RESULTS_DIR/strategy_a_results.json"
            ;;
        B)
            STRATEGY_NAME="Twice-Daily Adaptive (9:30 AM, 12:45 PM)"
            OUTPUT_FILE="$RESULTS_DIR/strategy_b_results.json"
            ;;
        C)
            STRATEGY_NAME="Static Baseline"
            OUTPUT_FILE="$RESULTS_DIR/strategy_c_results.json"
            ;;
        *)
            echo "⚠️  Unknown strategy: $STRATEGY (skipping)"
            continue
            ;;
    esac

    echo ""
    echo "==================================================================="
    echo "RUNNING STRATEGY $STRATEGY: $STRATEGY_NAME"
    echo "==================================================================="
    echo "Output: $OUTPUT_FILE"
    echo ""

    STRATEGY_START=$(date +%s)

    # Run adaptive optuna
    python3 "$TOOLS_DIR/adaptive_optuna.py" \
        --strategy "$STRATEGY" \
        --data "$DATA_FILE" \
        --build-dir "$BUILD_DIR" \
        --output "$OUTPUT_FILE" \
        2>&1 | tee "$RESULTS_DIR/strategy_${STRATEGY}_log.txt"

    STRATEGY_END=$(date +%s)
    STRATEGY_TIME=$((STRATEGY_END - STRATEGY_START))

    echo ""
    echo "✓ Strategy $STRATEGY complete in $((STRATEGY_TIME / 60)) minutes"
    echo ""
done

TEST_END_TIME=$(date +%s)
TOTAL_TIME=$((TEST_END_TIME - TEST_START_TIME))

echo ""
echo "==================================================================="
echo "ALL STRATEGIES COMPLETE"
echo "==================================================================="
echo "Total time: $((TOTAL_TIME / 60)) minutes"
echo ""

# Generate comparison report
echo "Generating comparison report..."
echo ""

# Check which strategies completed
COMPLETED_STRATEGIES=""
for STRATEGY in "${STRATEGY_ARRAY[@]}"; do
    STRATEGY=$(echo "$STRATEGY" | tr -d ' ')
    RESULT_FILE="$RESULTS_DIR/strategy_${STRATEGY,,}_results.json"

    if [[ -f "$RESULT_FILE" ]]; then
        COMPLETED_STRATEGIES="${COMPLETED_STRATEGIES}${STRATEGY},"
    fi
done

# Remove trailing comma
COMPLETED_STRATEGIES="${COMPLETED_STRATEGIES%,}"

if [[ -z "$COMPLETED_STRATEGIES" ]]; then
    echo "❌ No strategies completed successfully"
    exit 1
fi

echo "Completed strategies: $COMPLETED_STRATEGIES"

# Run comparison tool
python3 "$TOOLS_DIR/compare_strategies.py" \
    --strategies "$COMPLETED_STRATEGIES" \
    --results-dir "$RESULTS_DIR" \
    --output "$RESULTS_DIR/comparison_report.md"

echo ""
echo "==================================================================="
echo "RESULTS SUMMARY"
echo "==================================================================="
echo "Results directory: $RESULTS_DIR"
echo "Comparison report: $RESULTS_DIR/comparison_report.md"
echo ""
echo "Individual strategy results:"
for STRATEGY in "${STRATEGY_ARRAY[@]}"; do
    STRATEGY=$(echo "$STRATEGY" | tr -d ' ')
    RESULT_FILE="$RESULTS_DIR/strategy_${STRATEGY,,}_results.json"

    if [[ -f "$RESULT_FILE" ]]; then
        echo "  Strategy $STRATEGY: $RESULT_FILE"
    fi
done
echo ""
echo "View comparison report:"
echo "  cat $RESULTS_DIR/comparison_report.md"
echo ""
echo "✓ A/B/C Test Complete!"
echo "==================================================================="
