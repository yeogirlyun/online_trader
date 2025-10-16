#!/bin/bash
# Live Trading Monitor - Real-time status display (macOS compatible)

while true; do
    clear
    echo "═══════════════════════════════════════════════════════════"
    echo "  🔴 LIVE TRADING - Alpaca Paper Account PA3FOCO5XA55"
    echo "  Time: $(TZ=America/New_York date)"
    echo "═══════════════════════════════════════════════════════════"
    echo ""
    echo "📊 Recent Activity (last 25 lines):"
    echo ""
    tail -25 data/tmp/live_20251007/live_final.log
    echo ""
    echo "═══════════════════════════════════════════════════════════"
    echo "Refreshing every 5 seconds... Press Ctrl+C to stop"
    sleep 5
done
