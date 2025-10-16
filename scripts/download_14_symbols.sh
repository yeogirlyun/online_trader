#!/bin/bash
#
# Download 14 Leveraged ETFs for Multi-Symbol Rotation Trading
#
# Symbols: TQQQ, SQQQ, SSO, SDS, TNA, TZA, FAS, FAZ, ERX, ERY, UVXY, SVXY, NUGT, DUST
# Usage: ./scripts/download_14_symbols.sh [days]
#

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

# Load API keys
if [ -f config.env ]; then
    source config.env
fi

# Parameters
START_DATE=${1:-2025-09-15}  # Default: Start from Sept 15 (to cover October)
END_DATE=${2:-2025-10-15}     # Default: End at Oct 15
SYMBOLS="TQQQ SQQQ SSO SDS TNA TZA FAS FAZ ERX ERY UVXY SVXY NUGT DUST"
OUTPUT_DIR="data/equities"

echo "========================================================================"
echo "Multi-Symbol Data Download (14 Instruments)"
echo "========================================================================"
echo "Symbols: $SYMBOLS"
echo "Date Range: $START_DATE to $END_DATE"
echo "Output: $OUTPUT_DIR"
echo ""

# Check for API key
if [ -z "$POLYGON_API_KEY" ]; then
    echo "❌ ERROR: POLYGON_API_KEY not set"
    echo "Please set in config.env or environment"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Download data
echo "Downloading data..."
python3 tools/data_downloader.py \
    --start "$START_DATE" \
    --end "$END_DATE" \
    --outdir "$OUTPUT_DIR" \
    --timespan minute \
    --multiplier 1 \
    $SYMBOLS

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Download complete!"
    echo ""
    echo "Files created:"
    ls -lh "$OUTPUT_DIR"/*_RTH_NH.csv 2>/dev/null | tail -14 || echo "No RTH files found"
    echo ""
    echo "To test:"
    echo "  ./build/sentio_cli mock --mode mock --start-date 2025-10-01 --end-date 2025-10-15"
else
    echo ""
    echo "❌ Download failed"
    exit 1
fi
