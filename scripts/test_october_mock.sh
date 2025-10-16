#!/bin/bash
#
# Test all October trading days with mock rotation trading
# Collects MRD and performance metrics for each day
#

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

# October 2025 trading days (weekdays only)
TRADING_DAYS=(
    "2025-10-01"
    "2025-10-02"
    "2025-10-03"
    "2025-10-06"
    "2025-10-07"
    "2025-10-08"
    "2025-10-09"
    "2025-10-10"
    "2025-10-13"
    "2025-10-14"
)

OUTPUT_FILE="data/tmp/october_mock_results.txt"
SUMMARY_FILE="data/tmp/october_mock_summary.txt"

# Clear previous results
> "$OUTPUT_FILE"
> "$SUMMARY_FILE"

echo "========================================================================" | tee -a "$SUMMARY_FILE"
echo "October 2025 Mock Trading - All Trading Days" | tee -a "$SUMMARY_FILE"
echo "========================================================================" | tee -a "$SUMMARY_FILE"
echo "" | tee -a "$SUMMARY_FILE"

# Run tests for each trading day
for DATE in "${TRADING_DAYS[@]}"; do
    echo "========================================================================" | tee -a "$SUMMARY_FILE"
    echo "Testing $DATE..." | tee -a "$SUMMARY_FILE"
    echo "========================================================================" | tee -a "$SUMMARY_FILE"

    # Run mock test (skip optimization for speed)
    ./build/sentio_cli mock --mode mock --date "$DATE" 2>&1 | tee -a "$OUTPUT_FILE" | tail -50

    # Extract key metrics from the session summary
    echo "" >> "$SUMMARY_FILE"
    echo "Date: $DATE" >> "$SUMMARY_FILE"

    # Parse the output to extract metrics
    TRADES=$(grep "Trades executed:" "$OUTPUT_FILE" | tail -1 | awk '{print $3}')
    POSITIONS_OPENED=$(grep "Positions opened:" "$OUTPUT_FILE" | tail -1 | awk '{print $3}')
    POSITIONS_CLOSED=$(grep "Positions closed:" "$OUTPUT_FILE" | tail -1 | awk '{print $3}')
    PNL=$(grep "Total P&L:" "$OUTPUT_FILE" | tail -1 | awk '{print $3}')
    PNL_PCT=$(grep "Total P&L:" "$OUTPUT_FILE" | tail -1 | awk '{print $4}' | tr -d '()')
    FINAL_EQUITY=$(grep "Final equity:" "$OUTPUT_FILE" | tail -1 | awk '{print $3}')
    MRD=$(grep "MRD:" "$OUTPUT_FILE" | tail -1 | awk '{print $2}' | tr -d '%')

    echo "  Trades: $TRADES" >> "$SUMMARY_FILE"
    echo "  Positions Opened: $POSITIONS_OPENED" >> "$SUMMARY_FILE"
    echo "  Positions Closed: $POSITIONS_CLOSED" >> "$SUMMARY_FILE"
    echo "  Total P&L: $PNL ($PNL_PCT)" >> "$SUMMARY_FILE"
    echo "  Final Equity: $FINAL_EQUITY" >> "$SUMMARY_FILE"
    echo "  MRD: $MRD%" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"

    # Brief console output
    echo "$DATE: MRD = $MRD%, P&L = $PNL_PCT, Positions = $POSITIONS_OPENED/$POSITIONS_CLOSED" | tee -a "$SUMMARY_FILE"
    echo "" | tee -a "$SUMMARY_FILE"
done

echo "========================================================================" | tee -a "$SUMMARY_FILE"
echo "Testing Complete!" | tee -a "$SUMMARY_FILE"
echo "========================================================================" | tee -a "$SUMMARY_FILE"
echo "" | tee -a "$SUMMARY_FILE"
echo "Full output: $OUTPUT_FILE" | tee -a "$SUMMARY_FILE"
echo "Summary: $SUMMARY_FILE" | tee -a "$SUMMARY_FILE"

# Display summary
echo ""
echo "========================================================================"
echo "October 2025 Mock Trading Summary"
echo "========================================================================"
cat "$SUMMARY_FILE"
