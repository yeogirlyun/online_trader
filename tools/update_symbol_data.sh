#!/bin/bash
#
# Update Symbol Data from Configuration File
#
# This script reads the rotation_strategy.json config file and downloads
# up-to-date historical data for all symbols listed in the configuration.
#
# Usage: ./tools/update_symbol_data.sh [CONFIG_FILE] [START_DATE] [END_DATE]
#
# Arguments:
#   CONFIG_FILE  - Path to JSON config file (default: config/rotation_strategy.json)
#   START_DATE   - Start date for data download (default: auto-calculated for 6 months back)
#   END_DATE     - End date for data download (default: today)
#
# Examples:
#   ./tools/update_symbol_data.sh
#   ./tools/update_symbol_data.sh config/rotation_strategy.json
#   ./tools/update_symbol_data.sh config/rotation_strategy.json 2025-04-01 2025-10-15
#

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Parameters
CONFIG_FILE="${1:-config/rotation_strategy.json}"
START_DATE="${2:-$(date -v-6m '+%Y-%m-%d' 2>/dev/null || date -d '6 months ago' '+%Y-%m-%d')}"
END_DATE="${3:-$(date '+%Y-%m-%d')}"
OUTPUT_DIR="data/equities"

echo "========================================================================"
echo "Symbol Data Update Utility"
echo "========================================================================"
echo "Config File: $CONFIG_FILE"
echo "Date Range: $START_DATE to $END_DATE"
echo "Output Directory: $OUTPUT_DIR"
echo ""

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ ERROR: Config file not found: $CONFIG_FILE"
    exit 1
fi

# Load API keys
if [ -f config.env ]; then
    source config.env
else
    echo "⚠️  WARNING: config.env not found - API keys may not be set"
fi

# Check for API key
if [ -z "$POLYGON_API_KEY" ]; then
    echo "❌ ERROR: POLYGON_API_KEY not set"
    echo "Please set in config.env or environment"
    exit 1
fi

# Extract symbols from JSON config using Python
echo "Extracting symbols from config..."
SYMBOLS=$(python3 -c "
import json
import sys

try:
    with open('$CONFIG_FILE', 'r') as f:
        config = json.load(f)

    if 'symbols' in config and 'active' in config['symbols']:
        symbols = config['symbols']['active']
        print(' '.join(symbols))
    else:
        print('ERROR: symbols.active not found in config', file=sys.stderr)
        sys.exit(1)
except Exception as e:
    print(f'ERROR: {e}', file=sys.stderr)
    sys.exit(1)
")

if [ $? -ne 0 ]; then
    echo "❌ Failed to extract symbols from config"
    exit 1
fi

SYMBOL_COUNT=$(echo "$SYMBOLS" | wc -w)
echo "Found $SYMBOL_COUNT symbols:"
echo "  $SYMBOLS"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Download data
echo "Downloading historical data..."
echo "----------------------------------------"
python3 tools/data_downloader.py \
    --start "$START_DATE" \
    --end "$END_DATE" \
    --outdir "$OUTPUT_DIR" \
    --timespan minute \
    --multiplier 1 \
    $SYMBOLS

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================================================"
    echo "✅ Download Complete!"
    echo "========================================================================"
    echo ""
    echo "Symbol data files:"
    for sym in $SYMBOLS; do
        FILE="$OUTPUT_DIR/${sym}_RTH_NH.csv"
        if [ -f "$FILE" ]; then
            SIZE=$(ls -lh "$FILE" | awk '{print $5}')
            LINES=$(wc -l < "$FILE")
            echo "  ✓ $sym: $LINES bars ($SIZE)"
        else
            echo "  ✗ $sym: FILE NOT FOUND"
        fi
    done
    echo ""
    echo "Data is ready for trading!"
    echo ""
    echo "To verify:"
    echo "  build/sentio_cli mock --mode mock --start-date $(date '+%Y-%m-%d') --end-date $(date '+%Y-%m-%d')"
    echo ""
else
    echo ""
    echo "❌ Download failed"
    exit 1
fi
