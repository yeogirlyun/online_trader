#!/bin/bash
# Start Live Trading with SPY instruments via Alpaca IEX

echo "=== Starting Live Trading ==="
echo "Instruments: SPY, SPXL, SH, SDS"
echo "Data Feed: Alpaca IEX WebSocket"
echo "Account: Paper Trading"
echo ""

# Kill any existing live trading process
pkill -f "sentio_cli live-trade" 2>/dev/null
sleep 2

# Create data/tmp if it doesn't exist
mkdir -p data/tmp

# Start live trading in background
nohup ./build/sentio_cli live-trade > data/tmp/live_trade.log 2>&1 &
PID=$!

echo "Live trading started (PID: $PID)"
echo "Log file: data/tmp/live_trade.log"
echo ""
echo "Monitor with:"
echo "  tail -f data/tmp/live_trade.log"
echo ""
echo "Check status:"
echo "  ps aux | grep sentio_cli"
echo ""
echo "Stop trading:"
echo "  pkill -f 'sentio_cli live-trade'"
