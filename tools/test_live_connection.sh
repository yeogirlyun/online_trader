#!/bin/bash
# Test live trading connection with new Alpaca credentials
# Usage: ./tools/test_live_connection.sh

set -e

echo "============================================"
echo "Testing Live Trading Connection"
echo "============================================"
echo ""

# Load credentials
export ALPACA_PAPER_API_KEY=PKDYQYCJE5MMCTSD2AR5
export ALPACA_PAPER_SECRET_KEY=3CJhMKERktF2T7eZMfu3WX4002phT50RmHDxNFps
export POLYGON_API_KEY=fE68VnU8xUR7NQFMAM4yl3cULTHbigrb

echo "✓ Credentials loaded"
echo "  API Key: ${ALPACA_PAPER_API_KEY:0:10}..."
echo "  Polygon Key: ${POLYGON_API_KEY:0:10}..."
echo ""

# Test Alpaca connection with curl
echo "Testing Alpaca API connection..."
ALPACA_RESPONSE=$(curl -s -X GET \
  "https://paper-api.alpaca.markets/v2/account" \
  -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
  -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY")

if echo "$ALPACA_RESPONSE" | grep -q "account_number"; then
    echo "✓ Alpaca connection successful!"
    ACCOUNT_NUM=$(echo "$ALPACA_RESPONSE" | grep -o '"account_number":"[^"]*"' | cut -d'"' -f4)
    BUYING_POWER=$(echo "$ALPACA_RESPONSE" | grep -o '"buying_power":"[^"]*"' | cut -d'"' -f4)
    echo "  Account: $ACCOUNT_NUM"
    echo "  Buying Power: \$$BUYING_POWER"
else
    echo "✗ Alpaca connection failed!"
    echo "  Response: $ALPACA_RESPONSE"
    exit 1
fi
echo ""

# Test Polygon connection
echo "Testing Polygon API connection..."
POLYGON_RESPONSE=$(curl -s "https://api.polygon.io/v2/aggs/ticker/SPY/range/1/minute/2025-10-07/2025-10-07?apiKey=$POLYGON_API_KEY")

if echo "$POLYGON_RESPONSE" | grep -q "results"; then
    echo "✓ Polygon connection successful!"
else
    echo "✗ Polygon connection failed!"
    echo "  Response: $POLYGON_RESPONSE"
    exit 1
fi
echo ""

# Check market hours
echo "Current time (ET):"
TZ='America/New_York' date
echo ""

# Verify CLI is built
CLI_PATH="/Volumes/ExternalSSD/Dev/C++/online_trader/build/sentio_cli"
if [ -f "$CLI_PATH" ]; then
    echo "✓ sentio_cli executable found"
    echo "  Version:"
    $CLI_PATH --help | head -1
else
    echo "✗ sentio_cli not found - rebuild required"
    exit 1
fi
echo ""

echo "============================================"
echo "Configuration Summary"
echo "============================================"
echo ""
echo "Strategy: OnlineEnsemble v1.0"
echo "Regime Detection: DISABLED (baseline parameters)"
echo "Parameters:"
echo "  - buy_threshold: 0.55"
echo "  - sell_threshold: 0.45"
echo "  - ewrls_lambda: 0.995"
echo "  - warmup_samples: 960 bars (2 days)"
echo "  - BB amplification: ENABLED (0.10 factor)"
echo "  - Adaptive learning: ENABLED"
echo ""
echo "Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)"
echo "Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)"
echo ""
echo "============================================"
echo "✓ All systems ready for live trading!"
echo "============================================"
echo ""
echo "To start live trading:"
echo "  export ALPACA_PAPER_API_KEY=PKDYQYCJE5MMCTSD2AR5"
echo "  export ALPACA_PAPER_SECRET_KEY=3CJhMKERktF2T7eZMfu3WX4002phT50RmHDxNFps"
echo "  export POLYGON_API_KEY=fE68VnU8xUR7NQFMAM4yl3cULTHbigrb"
echo "  /Volumes/ExternalSSD/Dev/C++/online_trader/build/sentio_cli live-trade"
echo ""
echo "Or simply source config.env and run from project root:"
echo "  cd /Volumes/ExternalSSD/Dev/C++/online_trader"
echo "  source config.env"
echo "  ./build/sentio_cli live-trade"
echo ""
