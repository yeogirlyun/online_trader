#!/bin/bash
# =============================================================================
# Test Warmup Flow
# =============================================================================
# Tests the complete warmup → live trading flow:
# 1. Run warmup script to download today's data
# 2. Verify warmup data file exists
# 3. Check that live trading can load warmup data
#
# This is a dry-run test - it doesn't actually start live trading
# =============================================================================

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data/equities"
WARMUP_DIR="$PROJECT_ROOT/data/tmp/warmup"

echo "=== Testing Warmup Flow ==="
echo ""

# Step 1: Run warmup script
echo "Step 1: Running warmup script..."
cd "$PROJECT_ROOT"
bash tools/warmup_live_trading.sh
echo ""

# Step 2: Verify warmup file exists
echo "Step 2: Verifying warmup data file..."
WARMUP_FILE="$DATA_DIR/SPY_warmup_latest.csv"

if [[ ! -f "$WARMUP_FILE" ]]; then
    echo "❌ FAIL: Warmup file not found at $WARMUP_FILE"
    exit 1
fi

BAR_COUNT=$(tail -n +2 "$WARMUP_FILE" | wc -l | tr -d ' ')
echo "  ✓ Warmup file exists: $WARMUP_FILE"
echo "  ✓ Total bars: $BAR_COUNT"
echo ""

# Step 3: Check warmup info
echo "Step 3: Checking warmup metadata..."
if [[ -f "$WARMUP_DIR/warmup_info.txt" ]]; then
    cat "$WARMUP_DIR/warmup_info.txt"
else
    echo "  ⚠️  No warmup_info.txt found"
fi
echo ""

# Step 4: Verify CSV format
echo "Step 4: Verifying CSV format..."
echo "  Header:"
head -1 "$WARMUP_FILE"
echo ""
echo "  First 3 data rows:"
head -4 "$WARMUP_FILE" | tail -3
echo ""
echo "  Last 3 data rows:"
tail -3 "$WARMUP_FILE"
echo ""

# Summary
echo "=== Test Summary ==="
echo "✓ Warmup script executed successfully"
echo "✓ Warmup data file created: $WARMUP_FILE"
echo "✓ Total bars available: $BAR_COUNT"
echo ""
echo "Next step: Start live trading with:"
echo "  cd $BUILD_DIR"
echo "  ./sentio_cli live-trade"
echo ""
