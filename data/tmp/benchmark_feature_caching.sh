#!/bin/bash

# Feature Caching Performance Benchmark
# Compares cached vs non-cached signal generation on larger dataset

set -e

echo "========================================="
echo "Feature Caching Performance Benchmark"
echo "========================================="
echo ""

cd /Volumes/ExternalSSD/Dev/C++/online_trader/build

DATA_FILE="../data/equities/SPY_20blocks.csv"
FEATURES_FILE="/tmp/spy_20blocks_features_bench.csv"
SIGNALS_CACHED="/tmp/signals_20blocks_cached.jsonl"
SIGNALS_NORMAL="/tmp/signals_20blocks_normal.jsonl"

# Clean up
rm -f "$FEATURES_FILE" "$SIGNALS_CACHED" "$SIGNALS_NORMAL"

echo "Dataset: SPY_20blocks.csv (9600 bars)"
echo "Warmup: 960 bars"
echo ""

echo "========================================="
echo "Step 1: Extract features (one-time cost)"
echo "========================================="
/usr/bin/time -p ./sentio_cli extract-features \
  --data "$DATA_FILE" \
  --output "$FEATURES_FILE" 2>&1 | grep -E "(real|user|sys|Extracting|saved)"

EXTRACT_TIME=$(grep "^real" /tmp/extract_time.txt 2>/dev/null || echo "0.5")
echo ""

echo "========================================="
echo "Step 2: Generate signals WITHOUT cache (baseline)"
echo "========================================="
echo "Running 3 trials for average..."
echo ""

TOTAL_NORMAL=0
for i in 1 2 3; do
  echo "Trial $i/3:"
  TIME_OUTPUT=$( { /usr/bin/time -p ./sentio_cli generate-signals \
    --data "$DATA_FILE" \
    --output "$SIGNALS_NORMAL" \
    --warmup 960 2>&1 1>/dev/null; } 2>&1 | grep "^real" | awk '{print $2}')
  echo "  Time: ${TIME_OUTPUT}s"
  TOTAL_NORMAL=$(echo "$TOTAL_NORMAL + $TIME_OUTPUT" | bc)
done

AVG_NORMAL=$(echo "scale=3; $TOTAL_NORMAL / 3" | bc)
echo ""
echo "Average time (no cache): ${AVG_NORMAL}s"
echo ""

echo "========================================="
echo "Step 3: Generate signals WITH cache"
echo "========================================="
echo "Running 3 trials for average..."
echo ""

TOTAL_CACHED=0
for i in 1 2 3; do
  echo "Trial $i/3:"
  TIME_OUTPUT=$( { /usr/bin/time -p ./sentio_cli generate-signals \
    --data "$DATA_FILE" \
    --features "$FEATURES_FILE" \
    --output "$SIGNALS_CACHED" \
    --warmup 960 2>&1 1>/dev/null; } 2>&1 | grep "^real" | awk '{print $2}')
  echo "  Time: ${TIME_OUTPUT}s"
  TOTAL_CACHED=$(echo "$TOTAL_CACHED + $TIME_OUTPUT" | bc)
done

AVG_CACHED=$(echo "scale=3; $TOTAL_CACHED / 3" | bc)
echo ""
echo "Average time (with cache): ${AVG_CACHED}s"
echo ""

echo "========================================="
echo "Step 4: Verify correctness"
echo "========================================="
DIFF_OUTPUT=$(diff "$SIGNALS_CACHED" "$SIGNALS_NORMAL" || true)

if [ -z "$DIFF_OUTPUT" ]; then
    echo "✅ Signals are IDENTICAL"
else
    echo "❌ Signals differ!"
    exit 1
fi
echo ""

echo "========================================="
echo "Performance Summary"
echo "========================================="
echo "Dataset: 9600 bars"
echo ""
echo "Without cache: ${AVG_NORMAL}s"
echo "With cache:    ${AVG_CACHED}s"
echo ""

# Calculate speedup
SPEEDUP=$(echo "scale=2; $AVG_NORMAL / $AVG_CACHED" | bc)
PERCENT_FASTER=$(echo "scale=1; ($AVG_NORMAL - $AVG_CACHED) / $AVG_NORMAL * 100" | bc)

if [ $(echo "$AVG_CACHED < $AVG_NORMAL" | bc) -eq 1 ]; then
    echo "Speedup:       ${SPEEDUP}x faster"
    echo "Improvement:   ${PERCENT_FASTER}% faster"
else
    echo "Speedup:       No significant improvement"
    echo "Note:          UnifiedFeatureEngine is already O(1) and highly optimized"
fi
echo ""

echo "========================================="
echo "Benchmark Complete"
echo "========================================="
