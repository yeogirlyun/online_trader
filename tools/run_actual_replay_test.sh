#!/bin/bash
# Actual Replay Test - Get Real Results for Yesterday's Session
#
# This runs the complete workflow with actual OES baseline parameters
# and shows what the results would have been if optimization worked.

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

BUILD_DIR="./build"
DATA_FILE="data/equities/SPY_4blocks.csv"
OUTPUT_DIR="data/tmp/actual_replay_$(date +%Y%m%d_%H%M%S)"

mkdir -p "$OUTPUT_DIR"

echo "=========================================="
echo "ACTUAL REPLAY TEST - Baseline OES"
echo "=========================================="
echo "Data: $DATA_FILE"
echo "Output: $OUTPUT_DIR"
echo ""

# Phase 1: Generate signals with baseline params
echo "Phase 1: Generating signals (baseline OES parameters)..."
$BUILD_DIR/sentio_cli generate-signals \
  --data "$DATA_FILE" \
  --output "$OUTPUT_DIR/signals.jsonl" \
  --warmup 3900

echo "✓ Signals generated"
echo ""

# Phase 2: Execute trades
echo "Phase 2: Executing trades..."
$BUILD_DIR/sentio_cli execute-trades \
  --signals "$OUTPUT_DIR/signals.jsonl" \
  --data "$DATA_FILE" \
  --output "$OUTPUT_DIR/trades.jsonl" \
  --warmup 3900

echo "✓ Trades executed"
echo ""

# Phase 3: Analyze performance
echo "Phase 3: Analyzing performance..."
$BUILD_DIR/sentio_cli analyze-trades \
  --trades "$OUTPUT_DIR/trades.jsonl" \
  --data "$DATA_FILE" > "$OUTPUT_DIR/analysis.txt"

echo "✓ Analysis complete"
echo ""

echo "=========================================="
echo "RESULTS"
echo "=========================================="
cat "$OUTPUT_DIR/analysis.txt"
echo ""

echo "=========================================="
echo "OUTPUT FILES"
echo "=========================================="
ls -lh "$OUTPUT_DIR"
echo ""

echo "✅ Test complete! Output saved to: $OUTPUT_DIR"
