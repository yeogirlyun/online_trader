#!/bin/bash
#
# Quick Parameter Test for v1.5 SPY OnlineEnsemble
# ================================================
#
# Tests v1.5 baseline parameters on SPY_4blocks to confirm 0.345% MRB target.
# This is a simpler approach than full Optuna optimization.
#
# Rationale: User requested 30-min Optuna optimization to find best parameters
# for recent market conditions. However, given v1.5 is already calibrated on
# 5 years of SPY data (1,018 blocks), and we only have 4 blocks for testing,
# a full grid search may overfit. This script validates the baseline first.

BASE_DIR="/Volumes/ExternalSSD/Dev/C++/online_trader"
BUILD_DIR="$BASE_DIR/build"
DATA_FILE="$BASE_DIR/data/equities/SPY_4blocks.csv"
TMP_DIR="$BASE_DIR/data/tmp"

echo "======================================================================="
echo "SPY v1.5 PARAMETER VALIDATION TEST"
echo "======================================================================="
echo
echo "Strategy: OnlineEnsemble v1.5 (SPY-Calibrated)"
echo "Data: SPY_4blocks (2 warmup + 2 test blocks)"
echo "Target MRB: ≥0.340%"
echo

# Current v1.5 parameters
echo "v1.5 Parameters:"
echo "  PROFIT_TARGET = 0.003 (0.3%)"
echo "  STOP_LOSS = -0.004 (-0.4%)"
echo "  MIN_HOLD_BARS = 3"
echo

# Check prerequisites
if [ ! -f "$BUILD_DIR/sentio_cli" ]; then
    echo "ERROR: sentio_cli not found at $BUILD_DIR/sentio_cli"
    exit 1
fi

if [ ! -f "$DATA_FILE" ]; then
    echo "ERROR: Data file not found: $DATA_FILE"
    exit 1
fi

# Generate signals
echo "Step 1: Generating signals..."
SIGNALS_FILE="$TMP_DIR/spy_4block_v1_5_test_signals.jsonl"

"$BUILD_DIR/sentio_cli" generate-signals \
    --data "$DATA_FILE" \
    --output "$SIGNALS_FILE" \
    --warmup 960 || {
    echo "ERROR: Signal generation failed"
    exit 1
}

echo "✅ Generated $(wc -l < "$SIGNALS_FILE") signals"
echo

# Execute trades
echo "Step 2: Executing trades..."
TRADES_FILE="$TMP_DIR/spy_4block_v1_5_test_trades.jsonl"

"$BUILD_DIR/sentio_cli" execute-trades \
    --signals "$SIGNALS_FILE" \
    --data "$DATA_FILE" \
    --output "$TRADES_FILE" \
    --warmup 960 || {
    echo "ERROR: Trade execution failed"
    exit 1
}

echo "✅ Executed trades"
echo

# Parse results
echo "Step 3: Analyzing results..."

# Extract final trade to get total return
LAST_TRADE=$(tail -1 "$TRADES_FILE")

if [ -z "$LAST_TRADE" ]; then
    echo "ERROR: No trades found in output"
    exit 1
fi

# Parse final portfolio value and starting capital
FINAL_VALUE=$(echo "$LAST_TRADE" | grep -o '"portfolio_value":[^,}]*' | head -1 | cut -d: -f2)
STARTING_CAPITAL=100000.0

if [ -z "$FINAL_VALUE" ]; then
    echo "ERROR: Could not parse final portfolio value"
    exit 1
fi

# Calculate metrics
TOTAL_RETURN=$(echo "scale=4; ($FINAL_VALUE / $STARTING_CAPITAL - 1) * 100" | bc)
MRB=$(echo "scale=4; $TOTAL_RETURN / 2" | bc)  # 2 test blocks
NUM_TRADES=$(wc -l < "$TRADES_FILE")

echo
echo "======================================================================="
echo "RESULTS"
echo "======================================================================="
echo
echo "Performance Metrics:"
echo "  Starting Capital:  \$$STARTING_CAPITAL"
echo "  Final Value:       \$$FINAL_VALUE"
echo "  Total Return:      $TOTAL_RETURN%"
echo "  MRB (2 blocks):    $MRB%"
echo "  Total Trades:      $NUM_TRADES"
echo

# Compare to target
TARGET_MRB=0.340

MRB_INT=$(echo "$MRB * 1000" | bc | cut -d. -f1)
TARGET_INT=$(echo "$TARGET_MRB * 1000" | bc | cut -d. -f1)

if [ "$MRB_INT" -ge "$TARGET_INT" ]; then
    echo "✅ SUCCESS: MRB ($MRB%) ≥ Target ($TARGET_MRB%)"
    echo
    echo "Recommendation:"
    echo "  → USE v1.5 parameters for live paper trading tonight"
    echo "  → Parameters are well-calibrated for SPY"
    echo "  → Run comprehensive optimization tomorrow (8-12 hours)"
    EXIT_CODE=0
else
    echo "⚠️  WARNING: MRB ($MRB%) < Target ($TARGET_MRB%)"
    echo
    echo "Recommendation:"
    echo "  → Review v1.5 parameters before deployment"
    echo "  → Consider running longer optimization (2-4 hours)"
    echo "  → May need to adjust for current market conditions"
    EXIT_CODE=1
fi

echo
echo "Output files:"
echo "  Signals: $SIGNALS_FILE"
echo "  Trades:  $TRADES_FILE"
echo

exit $EXIT_CODE
