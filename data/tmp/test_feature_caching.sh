#!/bin/bash

# Feature Caching Test Script
# Demonstrates complete workflow for feature caching in Optuna optimization

set -e

echo "========================================="
echo "Feature Caching Test Workflow"
echo "========================================="
echo ""

cd /Volumes/ExternalSSD/Dev/C++/online_trader/build

DATA_FILE="../data/equities/SPY_4blocks.csv"
FEATURES_FILE="/tmp/spy_features_test.csv"
SIGNALS_CACHED="/tmp/signals_cached_test.jsonl"
SIGNALS_NORMAL="/tmp/signals_normal_test.jsonl"

# Clean up previous test files
rm -f "$FEATURES_FILE" "$SIGNALS_CACHED" "$SIGNALS_NORMAL"

echo "Step 1: Extract features (one-time operation)"
echo "---------------------------------------------"
time ./sentio_cli extract-features \
  --data "$DATA_FILE" \
  --output "$FEATURES_FILE"

echo ""
echo "Step 2: Verify feature file"
echo "----------------------------"
echo "Feature file size: $(wc -c < "$FEATURES_FILE") bytes"
echo "Feature rows: $(wc -l < "$FEATURES_FILE") (including header)"
echo "Feature columns: $(head -1 "$FEATURES_FILE" | tr ',' '\n' | wc -l)"
echo "First feature row:"
head -2 "$FEATURES_FILE" | tail -1 | cut -d',' -f1-5
echo ""

echo "Step 3: Generate signals WITH cached features"
echo "----------------------------------------------"
time ./sentio_cli generate-signals \
  --data "$DATA_FILE" \
  --features "$FEATURES_FILE" \
  --output "$SIGNALS_CACHED" \
  --warmup 960

echo ""
echo "Step 4: Generate signals WITHOUT cached features (baseline)"
echo "------------------------------------------------------------"
time ./sentio_cli generate-signals \
  --data "$DATA_FILE" \
  --output "$SIGNALS_NORMAL" \
  --warmup 960

echo ""
echo "Step 5: Verify correctness (signals should be identical)"
echo "---------------------------------------------------------"
DIFF_OUTPUT=$(diff "$SIGNALS_CACHED" "$SIGNALS_NORMAL" || true)

if [ -z "$DIFF_OUTPUT" ]; then
    echo "✅ SUCCESS: Cached and non-cached signals are IDENTICAL!"
    echo ""
    echo "Signal count (cached): $(wc -l < "$SIGNALS_CACHED")"
    echo "Signal count (normal): $(wc -l < "$SIGNALS_NORMAL")"
    echo ""
    echo "Sample signals:"
    head -3 "$SIGNALS_CACHED"
else
    echo "❌ FAILURE: Signals differ!"
    echo "$DIFF_OUTPUT" | head -20
    exit 1
fi

echo ""
echo "========================================="
echo "Feature Caching Test: PASSED ✅"
echo "========================================="
