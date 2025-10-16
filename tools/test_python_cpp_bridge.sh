#!/bin/bash
#
# Test Python WebSocket Bridge → C++ Live Trading Integration
#
# This script starts both the Python bridge and C++ live trader
# to verify end-to-end bar communication via FIFO.

echo "======================================================================"
echo "Python WebSocket Bridge → C++ Integration Test"
echo "======================================================================"
echo ""

# Set SSL certificate path for Python
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem

# Set Alpaca credentials
export ALPACA_PAPER_API_KEY=PKDYQYCJE5MMCTSD2AR5
export ALPACA_PAPER_SECRET_KEY=3CJhMKERktF2T7eZMfu3WX4002phT50RmHDxNFps
export POLYGON_API_KEY=fE68VnU8xUR7NQFMAM4yl3cULTHbigrb

# Clean up any existing FIFO
rm -f /tmp/alpaca_bars.fifo

echo "[TEST] Step 1: Starting Python WebSocket Bridge..."
python3 scripts/alpaca_websocket_bridge.py > /tmp/python_bridge.log 2>&1 &
BRIDGE_PID=$!
echo "[TEST] Python bridge PID: $BRIDGE_PID"
echo ""

# Wait for FIFO to be created
echo "[TEST] Step 2: Waiting for FIFO creation..."
for i in {1..10}; do
    if [ -p /tmp/alpaca_bars.fifo ]; then
        echo "[TEST] ✓ FIFO created successfully"
        break
    fi
    sleep 1
done

if [ ! -p /tmp/alpaca_bars.fifo ]; then
    echo "[TEST] ❌ ERROR: FIFO not created after 10 seconds"
    kill $BRIDGE_PID 2>/dev/null
    exit 1
fi
echo ""

# Show Python bridge output
echo "[TEST] Python Bridge Status:"
echo "---"
head -20 /tmp/python_bridge.log
echo "---"
echo ""

echo "[TEST] Step 3: Starting C++ Live Trader (will run for 30 seconds)..."
echo "[TEST] Watching for bars..."
echo ""

# Run C++ live trader and monitor output
timeout 30 ./build/sentio_cli live-trade 2>&1 | tee /tmp/cpp_trader.log &
CPP_PID=$!

# Wait for trader to complete or timeout
wait $CPP_PID 2>/dev/null

echo ""
echo "[TEST] Step 4: Shutting down..."
kill $BRIDGE_PID 2>/dev/null
sleep 1

echo ""
echo "======================================================================"
echo "Test Complete"
echo "======================================================================"
echo ""
echo "Python bridge log: /tmp/python_bridge.log"
echo "C++ trader log: /tmp/cpp_trader.log"
echo ""
echo "Check logs to verify bars were successfully communicated."
