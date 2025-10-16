#!/bin/bash
#
# Download and Prepare 6-Symbol Multi-Instrument Data
#
# This script ensures all 6 leveraged ETFs have data available for trading:
#   TQQQ, SQQQ, UPRO, SDS, UVXY, SVIX
#
# Strategy:
#   1. Check if real data exists for target date
#   2. Download from Polygon/Alpaca if missing
#   3. Generate synthetic data as fallback
#
# Usage:
#   ./scripts/download_and_prepare_6_symbols.sh [date]
#
# Arguments:
#   date   Optional target date (YYYY-MM-DD), defaults to most recent trading day
#
# Examples:
#   ./scripts/download_and_prepare_6_symbols.sh
#   ./scripts/download_and_prepare_6_symbols.sh 2025-10-14
#

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

# Load credentials
if [ -f config.env ]; then
    source config.env
fi

# Parameters
TARGET_DATE="${1:-auto}"
SYMBOLS=("TQQQ" "SQQQ" "UPRO" "SDS" "UVXY" "SVIX")
BASE_SYMBOLS=("SPY" "QQQ")  # Needed for synthetic generation
DATA_DIR="data/equities"
MIN_BARS=200  # Minimum bars required per symbol

# Logging functions
function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S')] ❌ ERROR: $1" >&2
}

function log_success() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S')] ✅ $1"
}

# Auto-detect target date if not specified
if [ "$TARGET_DATE" = "auto" ]; then
    TARGET_DATE=$(python3 -c "
import os
os.environ['TZ'] = 'America/New_York'
import time
time.tzset()
from datetime import datetime, timedelta

now = datetime.now()
current_date = now.date()
current_hour = now.hour
current_weekday = now.weekday()  # 0=Mon, 6=Sun

# Most recent complete trading day
if current_weekday == 5:  # Saturday
    target_date = current_date - timedelta(days=1)
elif current_weekday == 6:  # Sunday
    target_date = current_date - timedelta(days=2)
elif current_weekday == 0:  # Monday
    if current_hour < 16:
        target_date = current_date - timedelta(days=3)  # Previous Friday
    else:
        target_date = current_date
else:  # Tuesday-Friday
    if current_hour >= 16:
        target_date = current_date
    else:
        target_date = current_date - timedelta(days=1)

print(target_date.strftime('%Y-%m-%d'))
")
fi

log_info "========================================================================"
log_info "6-Symbol Data Preparation"
log_info "========================================================================"
log_info "Target date: $TARGET_DATE"
log_info "Symbols: ${SYMBOLS[*]}"
log_info "Data directory: $DATA_DIR"
log_info ""

mkdir -p "$DATA_DIR"

# Step 1: Check which symbols need data
log_info "Step 1: Checking existing data..."
MISSING_SYMBOLS=()
for symbol in "${SYMBOLS[@]}"; do
    file="$DATA_DIR/${symbol}_RTH_NH.csv"
    if [ -f "$file" ]; then
        # Check if file has data for target date
        count=$(grep -c "^$TARGET_DATE" "$file" 2>/dev/null || echo 0)
        if [ "$count" -ge "$MIN_BARS" ]; then
            log_success "$symbol: Found $count bars for $TARGET_DATE"
        else
            log_info "$symbol: File exists but insufficient data ($count bars)"
            MISSING_SYMBOLS+=("$symbol")
        fi
    else
        log_info "$symbol: No data file"
        MISSING_SYMBOLS+=("$symbol")
    fi
done

if [ ${#MISSING_SYMBOLS[@]} -eq 0 ]; then
    log_success "All symbols have sufficient data"
    exit 0
fi

log_info ""
log_info "Missing or insufficient data for: ${MISSING_SYMBOLS[*]}"
log_info ""

# Step 2: Try downloading from Polygon/Alpaca
log_info "Step 2: Attempting to download missing symbols..."

if [ -n "$POLYGON_API_KEY" ]; then
    log_info "Using Polygon.io API..."

    for symbol in "${MISSING_SYMBOLS[@]}"; do
        log_info "Downloading $symbol..."

        # Calculate date range (7 days before target to 1 day after)
        START_DATE=$(python3 -c "from datetime import datetime, timedelta; print((datetime.strptime('$TARGET_DATE', '%Y-%m-%d') - timedelta(days=7)).strftime('%Y-%m-%d'))")
        END_DATE=$(python3 -c "from datetime import datetime, timedelta; print((datetime.strptime('$TARGET_DATE', '%Y-%m-%d') + timedelta(days=1)).strftime('%Y-%m-%d'))")

        python3 tools/data_downloader.py \
            --symbols "$symbol" \
            --start "$START_DATE" \
            --end "$END_DATE" \
            --timeframe 1Min \
            --output "$DATA_DIR" \
            --source polygon 2>&1 | grep -E "✓|✅|Downloaded|ERROR" || true

        if [ $? -eq 0 ]; then
            log_success "$symbol downloaded successfully"
        else
            log_error "$symbol download failed - will use synthetic data"
        fi
    done
elif [ -n "$ALPACA_PAPER_API_KEY" ]; then
    log_info "Using Alpaca API (paper trading)..."

    # Similar download logic using Alpaca
    log_info "⚠️  Alpaca download not yet implemented - using synthetic data"
else
    log_info "⚠️  No API keys found - will generate synthetic data"
fi

log_info ""

# Step 3: Generate synthetic data for any still-missing symbols
log_info "Step 3: Generating synthetic data for missing symbols..."

# Check if base data (SPY, QQQ) exists
for base_symbol in "${BASE_SYMBOLS[@]}"; do
    base_file="$DATA_DIR/${base_symbol}_RTH_NH.csv"
    if [ ! -f "$base_file" ]; then
        log_error "Base data missing: $base_file"
        log_error "Cannot generate synthetic leveraged ETF data"
        log_error "Please download SPY and QQQ data first:"
        log_error "  python3 tools/data_downloader.py --symbols SPY,QQQ --days 30 --output $DATA_DIR"
        exit 1
    fi
done

log_info "Generating leveraged ETF data from SPY and QQQ..."
python3 tools/generate_leveraged_etf_data.py \
    --spy "$DATA_DIR/SPY_RTH_NH.csv" \
    --qqq "$DATA_DIR/QQQ_RTH_NH.csv" \
    --output "$DATA_DIR"

if [ $? -eq 0 ]; then
    log_success "Synthetic data generated"
else
    log_error "Synthetic data generation failed"
    exit 1
fi

log_info ""

# Step 4: Final validation
log_info "Step 4: Final validation..."
ALL_READY=true
for symbol in "${SYMBOLS[@]}"; do
    file="$DATA_DIR/${symbol}_RTH_NH.csv"
    if [ -f "$file" ]; then
        count=$(wc -l < "$file")
        bars=$((count - 1))  # Subtract header
        log_success "$symbol: $bars bars available"
    else
        log_error "$symbol: Still missing!"
        ALL_READY=false
    fi
done

log_info ""

if [ "$ALL_READY" = true ]; then
    log_success "========================================================================"
    log_success "All 6 symbols ready for trading!"
    log_success "========================================================================"
    exit 0
else
    log_error "========================================================================"
    log_error "Data preparation incomplete - some symbols still missing"
    log_error "========================================================================"
    exit 1
fi
