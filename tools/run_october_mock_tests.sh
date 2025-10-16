#!/bin/bash

# Run mock tests for 10 October trading days
# Each day is independent (EOD liquidation)

DATES=(
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

INSTRUMENTS=("TQQQ" "SQQQ" "SPXL" "SDS" "UVXY" "SVXY")
DATA_DIR="data/equities"
TMP_DIR="data/tmp/october_tests"
OUTPUT_DIR="logs/october_mock_tests"

echo "=========================================="
echo "October 2025 Mock Testing - 10 Days"
echo "=========================================="
echo ""
echo "Instruments: ${INSTRUMENTS[@]}"
echo "Starting capital: \$100,000 per day"
echo ""

# Create directories
mkdir -p "$TMP_DIR"
mkdir -p "$OUTPUT_DIR"

# Results file
RESULTS_FILE="$OUTPUT_DIR/october_summary.txt"
echo "October 2025 Mock Test Results" > "$RESULTS_FILE"
echo "===============================" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"
echo "Date          | MRD      | Total Return | Trades | Win Rate | Final Capital" >> "$RESULTS_FILE"
echo "--------------------------------------------------------------------------" >> "$RESULTS_FILE"

# Track totals
TOTAL_MRD=0
TOTAL_RETURN=0
TOTAL_TRADES=0
DAY_COUNT=0

# Run each day
for DATE in "${DATES[@]}"; do
    echo "=========================================="
    echo "Testing: $DATE"
    echo "=========================================="

    # Extract this day's data for all instruments into temp directory
    for INSTRUMENT in "${INSTRUMENTS[@]}"; do
        INPUT_FILE="$DATA_DIR/${INSTRUMENT}_RTH_NH.csv"
        OUTPUT_FILE="$TMP_DIR/${INSTRUMENT}_${DATE}.csv"

        if [ ! -f "$INPUT_FILE" ]; then
            echo "ERROR: Missing data for $INSTRUMENT"
            continue
        fi

        # Extract header
        head -1 "$INPUT_FILE" > "$OUTPUT_FILE"

        # Extract this date's bars (9:30 AM - 4:00 PM)
        grep "^${DATE}T" "$INPUT_FILE" >> "$OUTPUT_FILE"

        BAR_COUNT=$(wc -l < "$OUTPUT_FILE")
        echo "  $INSTRUMENT: $((BAR_COUNT - 1)) bars"
    done

    echo ""
    echo "Running mock trading..."

    # Create date-specific config (use temp directory for this day's data)
    # For now, just run with the extracted day's data
    # The rotation-trade command will load from data/tmp/october_tests

    # Run mock trading for this day
    # Note: This requires the rotation-trade command to support single-day data
    # For now, we'll run it and capture output

    cd /Volumes/ExternalSSD/Dev/C++/online_trader

    # Run the mock test (this will use all data in the temp directory)
    # We need to move only this day's files to the data directory temporarily
    for INSTRUMENT in "${INSTRUMENTS[@]}"; do
        cp "$TMP_DIR/${INSTRUMENT}_${DATE}.csv" "$TMP_DIR/${INSTRUMENT}_RTH_NH.csv"
    done

    # Run rotation-trade in mock mode
    ./build/sentio_cli rotation-trade --mode mock --data-dir "$TMP_DIR" \
        > "$OUTPUT_DIR/session_${DATE}.log" 2>&1

    # Parse results from log
    if [ -f "$OUTPUT_DIR/session_${DATE}.log" ]; then
        # Extract key metrics
        MRD=$(grep -i "MRD" "$OUTPUT_DIR/session_${DATE}.log" | tail -1 | grep -oE "[-+]?[0-9]*\.?[0-9]+" | head -1)
        RETURN=$(grep -i "Total return" "$OUTPUT_DIR/session_${DATE}.log" | grep -oE "[-+]?[0-9]*\.?[0-9]+" | head -1)
        TRADES=$(grep -i "Total trades" "$OUTPUT_DIR/session_${DATE}.log" | grep -oE "[0-9]+" | head -1)
        WINRATE=$(grep -i "Win rate" "$OUTPUT_DIR/session_${DATE}.log" | grep -oE "[0-9]*\.?[0-9]+" | head -1)
        FINAL_CAP=$(grep -i "Final capital" "$OUTPUT_DIR/session_${DATE}.log" | grep -oE "[0-9]*\.?[0-9]+" | head -1)

        # Default values if parsing failed
        MRD=${MRD:-"N/A"}
        RETURN=${RETURN:-"N/A"}
        TRADES=${TRADES:-"N/A"}
        WINRATE=${WINRATE:-"N/A"}
        FINAL_CAP=${FINAL_CAP:-"N/A"}

        # Write to results
        printf "%-13s | %8s | %12s | %6s | %8s | %13s\n" \
            "$DATE" "$MRD%" "$RETURN%" "$TRADES" "$WINRATE%" "\$$FINAL_CAP" >> "$RESULTS_FILE"

        # Accumulate totals (if numeric)
        if [[ "$MRD" != "N/A" ]]; then
            TOTAL_MRD=$(echo "$TOTAL_MRD + $MRD" | bc)
            DAY_COUNT=$((DAY_COUNT + 1))
        fi
        if [[ "$RETURN" != "N/A" ]]; then
            TOTAL_RETURN=$(echo "$TOTAL_RETURN + $RETURN" | bc)
        fi
        if [[ "$TRADES" != "N/A" ]]; then
            TOTAL_TRADES=$((TOTAL_TRADES + TRADES))
        fi

        echo "  MRD: $MRD%"
        echo "  Total Return: $RETURN%"
        echo "  Trades: $TRADES"
        echo "  Win Rate: $WINRATE%"
        echo ""
    else
        echo "  ERROR: No output log generated"
        printf "%-13s | %8s | %12s | %6s | %8s | %13s\n" \
            "$DATE" "ERROR" "ERROR" "ERROR" "ERROR" "ERROR" >> "$RESULTS_FILE"
    fi
done

# Calculate averages
if [ $DAY_COUNT -gt 0 ]; then
    AVG_MRD=$(echo "scale=3; $TOTAL_MRD / $DAY_COUNT" | bc)
    AVG_RETURN=$(echo "scale=3; $TOTAL_RETURN / $DAY_COUNT" | bc)
    AVG_TRADES=$(echo "scale=0; $TOTAL_TRADES / $DAY_COUNT" | bc)
else
    AVG_MRD="N/A"
    AVG_RETURN="N/A"
    AVG_TRADES="N/A"
fi

# Write summary
echo "" >> "$RESULTS_FILE"
echo "--------------------------------------------------------------------------" >> "$RESULTS_FILE"
echo "SUMMARY" >> "$RESULTS_FILE"
echo "--------------------------------------------------------------------------" >> "$RESULTS_FILE"
echo "Days Tested:     $DAY_COUNT" >> "$RESULTS_FILE"
echo "Average MRD:     $AVG_MRD%" >> "$RESULTS_FILE"
echo "Average Return:  $AVG_RETURN%" >> "$RESULTS_FILE"
echo "Total Trades:    $TOTAL_TRADES" >> "$RESULTS_FILE"
echo "Avg Trades/Day:  $AVG_TRADES" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# Annualized projection
if [[ "$AVG_MRD" != "N/A" ]]; then
    # Annualized return = (1 + daily_return)^252 - 1
    # Approximation: annualized ≈ daily * 252 for small values
    ANNUALIZED=$(echo "scale=1; $AVG_MRD * 252" | bc)
    echo "Projected Annualized Return: $ANNUALIZED% (simple approximation)" >> "$RESULTS_FILE"
fi

echo ""
echo "=========================================="
echo "October Testing Complete!"
echo "=========================================="
echo ""
cat "$RESULTS_FILE"
echo ""
echo "Full logs: $OUTPUT_DIR/session_*.log"
echo "Summary:   $RESULTS_FILE"
