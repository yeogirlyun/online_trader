#!/bin/bash

# SPY Data Download Setup Script
# This script helps set up the environment and download SPY data

set -e

echo "=== SPY Data Download Setup ==="
echo ""

# Check if config.env exists
if [ ! -f "config.env" ]; then
    echo "Creating config.env from template..."
    cp config.env.example config.env
    echo "✅ config.env created"
    echo ""
    echo "⚠️  IMPORTANT: You need to edit config.env and add your Polygon.io API key"
    echo "   Get a free API key from: https://polygon.io/"
    echo "   Then edit config.env and replace 'your_api_key_here' with your actual key"
    echo ""
    echo "Press Enter when you've added your API key to config.env..."
    read
fi

# Source the config file
if [ -f "config.env" ]; then
    source config.env
    echo "✅ Loaded configuration from config.env"
else
    echo "❌ config.env not found"
    exit 1
fi

# Check if API key is set
if [ -z "$POLYGON_API_KEY" ] || [ "$POLYGON_API_KEY" = "your_api_key_here" ]; then
    echo "❌ POLYGON_API_KEY not set in config.env"
    echo "   Please edit config.env and add your Polygon.io API key"
    exit 1
fi

echo "✅ POLYGON_API_KEY is configured"
echo ""

# Calculate date range (past 5 years)
END_DATE=$(date +%Y-%m-%d)
START_DATE=$(date -v-5y +%Y-%m-%d 2>/dev/null || date -d "5 years ago" +%Y-%m-%d)

echo "📅 Date range: $START_DATE to $END_DATE"
echo ""

# Create data directory if it doesn't exist
mkdir -p data/equities

# Download SPY data
echo "🚀 Downloading SPY data..."
python3 tools/data_downloader.py SPY --start "$START_DATE" --end "$END_DATE" --outdir data/equities

echo ""
echo "✅ SPY data download complete!"
echo ""
echo "📁 Files created:"
echo "   - data/equities/SPY_RTH_NH.csv"
echo "   - data/equities/SPY_RTH_NH.bin"
echo ""
echo "🔍 Data summary:"
if [ -f "data/equities/SPY_RTH_NH.csv" ]; then
    echo "   CSV file size: $(ls -lh data/equities/SPY_RTH_NH.csv | awk '{print $5}')"
    echo "   Records: $(wc -l < data/equities/SPY_RTH_NH.csv)"
fi
if [ -f "data/equities/SPY_RTH_NH.bin" ]; then
    echo "   Binary file size: $(ls -lh data/equities/SPY_RTH_NH.bin | awk '{print $5}')"
fi
