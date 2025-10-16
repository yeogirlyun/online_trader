#!/bin/bash
#
# Download 6 Leveraged ETFs for Multi-Symbol Rotation v2.0
#
# Symbols: TQQQ, SQQQ, UPRO, SDS, UVXY, SVIX
# Usage: ./scripts/download_6_symbols.sh [days]
#

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

# Load API keys
if [ -f config.env ]; then
    source config.env
fi

# Parameters
DAYS=${1:-30}  # Default: last 30 days
SYMBOLS="TQQQ,SQQQ,UPRO,SDS,UVXY,SVIX"
OUTPUT_DIR="data/equities"
SOURCE="polygon"  # or "alpaca"

echo "========================================================================"
echo "Multi-Symbol Data Download"
echo "========================================================================"
echo "Symbols: $SYMBOLS"
echo "Days: $DAYS"
echo "Source: $SOURCE"
echo "Output: $OUTPUT_DIR"
echo ""

# Check for API key
if [ "$SOURCE" = "polygon" ] && [ -z "$POLYGON_API_KEY" ]; then
    echo "❌ ERROR: POLYGON_API_KEY not set"
    echo "Please set in config.env or environment"
    exit 1
fi

if [ "$SOURCE" = "alpaca" ] && [ -z "$ALPACA_PAPER_API_KEY" ]; then
    echo "❌ ERROR: ALPACA_PAPER_API_KEY not set"
    echo "Please set in config.env or environment"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Download data
echo "Downloading data..."
python3 tools/data_downloader.py \
    --symbols "$SYMBOLS" \
    --days "$DAYS" \
    --timeframe 1Min \
    --output "$OUTPUT_DIR" \
    --source "$SOURCE"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Download complete!"
    echo ""
    echo "Files created:"
    ls -lh "$OUTPUT_DIR"/*_RTH_NH.csv 2>/dev/null | tail -6 || echo "No RTH files found"
    echo ""
    echo "To test:"
    echo "  ./build/sentio_cli live-trade --mock --mock-date 2025-10-14"
else
    echo ""
    echo "❌ Download failed"
    exit 1
fi
