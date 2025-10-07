#!/bin/bash
# Monitor live trading until market close (4:00 PM ET)

LOG_FILE="data/tmp/live_trade.log"
SUMMARY_FILE="data/tmp/trading_summary_$(date +%Y%m%d_%H%M%S).txt"

echo "=== Live Trading Monitor Started: $(TZ='America/New_York' date) ===" > "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

while true; do
    current_time=$(TZ='America/New_York' date +%H%M)
    
    # Check if market is closed (after 4:00 PM ET = 1600)
    if [ "$current_time" -gt 1600 ]; then
        echo "" >> "$SUMMARY_FILE"
        echo "=== Market Closed: $(TZ='America/New_York' date) ===" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        
        # Count bars received
        echo "Bars Received:" >> "$SUMMARY_FILE"
        grep "✓ Bar:" "$LOG_FILE" | awk '{print $3}' | sort | uniq -c >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        
        # Check for signals/trades
        echo "Signals Generated:" >> "$SUMMARY_FILE"
        signals_file=$(ls -t logs/live_trading/signals_*.jsonl 2>/dev/null | head -1)
        if [ -n "$signals_file" ] && [ -s "$signals_file" ]; then
            wc -l "$signals_file" >> "$SUMMARY_FILE"
        else
            echo "0 signals" >> "$SUMMARY_FILE"
        fi
        echo "" >> "$SUMMARY_FILE"
        
        echo "Trades Executed:" >> "$SUMMARY_FILE"
        trades_file=$(ls -t logs/live_trading/trades_*.jsonl 2>/dev/null | head -1)
        if [ -n "$trades_file" ] && [ -s "$trades_file" ]; then
            wc -l "$trades_file" >> "$SUMMARY_FILE"
        else
            echo "0 trades" >> "$SUMMARY_FILE"
        fi
        
        echo "" >> "$SUMMARY_FILE"
        echo "Summary saved to: $SUMMARY_FILE"
        cat "$SUMMARY_FILE"
        break
    fi
    
    # Sleep for 5 minutes
    sleep 300
done
