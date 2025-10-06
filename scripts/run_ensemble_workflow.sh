#!/bin/bash

# OnlineEnsemble Complete Workflow Script
# This script runs the complete experimental workflow:
# 1. Generate signals from market data
# 2. Execute trades with portfolio manager
# 3. Analyze results and generate reports

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

# Paths
DATA_PATH="${DATA_PATH:-data/QQQ_1min.csv}"
OUTPUT_DIR="${OUTPUT_DIR:-results/experiment_$(date +%Y%m%d_%H%M%S)}"
CLI_PATH="${CLI_PATH:-build/sentio_cli}"

# Strategy parameters
WARMUP_BARS="${WARMUP_BARS:-100}"
START_BAR="${START_BAR:-0}"
END_BAR="${END_BAR:--1}"
STARTING_CAPITAL="${STARTING_CAPITAL:-100000}"
BUY_THRESHOLD="${BUY_THRESHOLD:-0.53}"
SELL_THRESHOLD="${SELL_THRESHOLD:-0.47}"

# Flags
VERBOSE="${VERBOSE:-false}"
CSV_OUTPUT="${CSV_OUTPUT:-false}"

# ============================================================================
# Setup
# ============================================================================

echo "╔════════════════════════════════════════════════════════════╗"
echo "║      OnlineEnsemble Trading Strategy Workflow              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Check if CLI exists
if [ ! -f "$CLI_PATH" ]; then
    echo "Error: CLI not found at $CLI_PATH"
    echo "Please build the project first: ./build.sh Release"
    exit 1
fi

# Check if data exists
if [ ! -f "$DATA_PATH" ]; then
    echo "Error: Data file not found at $DATA_PATH"
    exit 1
fi

# ============================================================================
# Step 1: Generate Signals
# ============================================================================

echo "════════════════════════════════════════════════════════════"
echo "Step 1: Generating signals..."
echo "════════════════════════════════════════════════════════════"
echo ""

SIGNAL_PATH="$OUTPUT_DIR/signals.jsonl"
VERBOSE_FLAG=""
if [ "$VERBOSE" = "true" ]; then
    VERBOSE_FLAG="--verbose"
fi

CSV_FLAG=""
if [ "$CSV_OUTPUT" = "true" ]; then
    CSV_FLAG="--csv"
    SIGNAL_PATH="$OUTPUT_DIR/signals.csv"
fi

$CLI_PATH generate-signals \
    --data "$DATA_PATH" \
    --output "$SIGNAL_PATH" \
    --warmup "$WARMUP_BARS" \
    --start "$START_BAR" \
    --end "$END_BAR" \
    $VERBOSE_FLAG \
    $CSV_FLAG

echo ""
echo "✅ Signals generated: $SIGNAL_PATH"
echo ""

# ============================================================================
# Step 2: Execute Trades
# ============================================================================

echo "════════════════════════════════════════════════════════════"
echo "Step 2: Executing trades..."
echo "════════════════════════════════════════════════════════════"
echo ""

TRADES_PATH="$OUTPUT_DIR/trades.jsonl"
if [ "$CSV_OUTPUT" = "true" ]; then
    TRADES_PATH="$OUTPUT_DIR/trades.csv"
fi

$CLI_PATH execute-trades \
    --signals "$SIGNAL_PATH" \
    --data "$DATA_PATH" \
    --output "$TRADES_PATH" \
    --capital "$STARTING_CAPITAL" \
    --buy-threshold "$BUY_THRESHOLD" \
    --sell-threshold "$SELL_THRESHOLD" \
    $VERBOSE_FLAG \
    $CSV_FLAG

echo ""
echo "✅ Trades executed: $TRADES_PATH"
echo "✅ Equity curve: ${TRADES_PATH%.*}_equity.csv"
echo ""

# ============================================================================
# Step 3: Analyze Results
# ============================================================================

echo "════════════════════════════════════════════════════════════"
echo "Step 3: Analyzing results..."
echo "════════════════════════════════════════════════════════════"
echo ""

REPORT_PATH="$OUTPUT_DIR/analysis_report.json"

$CLI_PATH analyze-trades \
    --trades "$TRADES_PATH" \
    --output "$REPORT_PATH"

echo ""
echo "✅ Analysis complete: $REPORT_PATH"
echo ""

# ============================================================================
# Summary
# ============================================================================

echo "════════════════════════════════════════════════════════════"
echo "Workflow Complete!"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Output files:"
echo "  - Signals:      $SIGNAL_PATH"
echo "  - Trades:       $TRADES_PATH"
echo "  - Equity curve: ${TRADES_PATH%.*}_equity.csv"
echo "  - Analysis:     $REPORT_PATH"
echo ""
echo "To view results:"
echo "  cat $REPORT_PATH | jq ."
echo "  python3 scripts/plot_results.py $OUTPUT_DIR"
echo ""

# ============================================================================
# Optional: Generate plots if Python is available
# ============================================================================

if command -v python3 &> /dev/null; then
    PLOT_SCRIPT="scripts/plot_results.py"
    if [ -f "$PLOT_SCRIPT" ]; then
        echo "Generating plots..."
        python3 "$PLOT_SCRIPT" "$OUTPUT_DIR" 2>/dev/null || true
        echo ""
    fi
fi

echo "🎉 Workflow complete! Results saved in: $OUTPUT_DIR"
