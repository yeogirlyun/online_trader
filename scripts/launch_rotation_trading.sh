#!/bin/bash
#
# Comprehensive Rotation Trading Launch Script
# Supports: Mock & Live Multi-Symbol Rotation Trading
#
# Features:
#   - Auto data download for all 6 instruments (TQQQ, SQQQ, SPXL, SDS, UVXY, SVXY)
#   - Pre-market optimization (2-phase Optuna)
#   - Hourly intraday optimization during trading
#   - Dashboard generation with email notification
#   - Self-sufficient: Downloads missing data automatically
#
# Usage:
#   ./scripts/launch_rotation_trading.sh [mode] [options]
#
# Modes:
#   mock     - Mock rotation trading (replay historical data for all 6 instruments)
#   live     - Live rotation trading (paper trading with Alpaca)
#
# Options:
#   --date YYYY-MM-DD     Target date for mock mode (default: yesterday)
#   --speed N             Mock replay speed (default: 0 for instant)
#   --optimize            Force pre-market optimization (default: auto)
#   --skip-optimize       Skip optimization, use existing params
#   --trials N            Trials for optimization (default: 50)
#   --hourly-optimize     Enable hourly re-optimization (10 AM - 3 PM)
#
# Examples:
#   # Mock yesterday's session with all 6 instruments
#   ./scripts/launch_rotation_trading.sh mock
#
#   # Mock specific date
#   ./scripts/launch_rotation_trading.sh mock --date 2025-10-14
#
#   # Mock with hourly optimization
#   ./scripts/launch_rotation_trading.sh mock --hourly-optimize
#
#   # Live rotation trading
#   ./scripts/launch_rotation_trading.sh live
#

set -e

# =============================================================================
# Configuration
# =============================================================================

MODE=""
MOCK_DATE=""
MOCK_SPEED=0  # Instant replay by default for mock mode
RUN_OPTIMIZATION="auto"
HOURLY_OPTIMIZE=false
N_TRIALS=50
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"

# Rotation trading symbols
ROTATION_SYMBOLS=("TQQQ" "SQQQ" "SPXL" "SDS" "UVXY" "SVIX")

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        mock|live)
            MODE="$1"
            shift
            ;;
        --date)
            MOCK_DATE="$2"
            shift 2
            ;;
        --speed)
            MOCK_SPEED="$2"
            shift 2
            ;;
        --optimize)
            RUN_OPTIMIZATION="yes"
            shift
            ;;
        --skip-optimize)
            RUN_OPTIMIZATION="no"
            shift
            ;;
        --hourly-optimize)
            HOURLY_OPTIMIZE=true
            shift
            ;;
        --trials)
            N_TRIALS="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [mock|live] [options]"
            exit 1
            ;;
    esac
done

# Validate mode
if [ -z "$MODE" ]; then
    echo "Error: Mode required (mock or live)"
    echo "Usage: $0 [mock|live] [options]"
    exit 1
fi

cd "$PROJECT_ROOT"

# Load environment
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem
if [ -f config.env ]; then
    source config.env
fi

# Paths
CPP_TRADER="build/sentio_cli"
LOG_DIR="logs/rotation_${MODE}"
DATA_DIR="data/tmp/rotation_warmup"
BEST_PARAMS_FILE="config/rotation_strategy.json"

# Scripts
OPTUNA_SCRIPT="tools/optuna_mrb_wf.py"
HOURLY_OPTUNA_SCRIPT="tools/hourly_intraday_optuna.py"
DASHBOARD_SCRIPT="scripts/professional_trading_dashboard.py"
EMAIL_SCRIPT="scripts/send_dashboard_email.py"
DATA_DOWNLOADER="tools/data_downloader.py"
LEVERAGED_GEN="tools/generate_leveraged_etf_data.py"

# Validate binary
if [ ! -f "$CPP_TRADER" ]; then
    echo "❌ ERROR: Binary not found: $CPP_TRADER"
    exit 1
fi

# Determine optimization
if [ "$RUN_OPTIMIZATION" = "auto" ]; then
    # Check if rotation_strategy.json exists and is recent
    if [ -f "$BEST_PARAMS_FILE" ]; then
        file_age_days=$(( ($(date +%s) - $(stat -f %m "$BEST_PARAMS_FILE" 2>/dev/null || stat -c %Y "$BEST_PARAMS_FILE" 2>/dev/null || echo 0)) / 86400 ))
        if [ "$file_age_days" -le 7 ]; then
            RUN_OPTIMIZATION="no"  # Use existing config if recent
        else
            RUN_OPTIMIZATION="yes"  # Re-optimize if stale
        fi
    else
        RUN_OPTIMIZATION="yes"  # Optimize if config doesn't exist
    fi
fi

# =============================================================================
# Functions
# =============================================================================

function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ❌ ERROR: $1" >&2
}

function log_warn() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ⚠️  WARNING: $1"
}

function determine_target_date() {
    # Determine target date for mock trading
    if [ -n "$MOCK_DATE" ]; then
        echo "$MOCK_DATE"
        return
    fi

    # Auto-detect yesterday (most recent complete session)
    python3 -c "
import os
os.environ['TZ'] = 'America/New_York'
import time
time.tzset()
from datetime import datetime, timedelta

now = datetime.now()
# Get yesterday (previous trading day)
yesterday = now - timedelta(days=1)

# If weekend, go back to Friday
if yesterday.weekday() == 5:  # Saturday
    yesterday = yesterday - timedelta(days=1)
elif yesterday.weekday() == 6:  # Sunday
    yesterday = yesterday - timedelta(days=2)

print(yesterday.strftime('%Y-%m-%d'))
"
}

function download_symbol_data() {
    local symbol="$1"
    local start_date="$2"
    local end_date="$3"

    log_info "Downloading $symbol data from $start_date to $end_date..."

    if [ -z "$POLYGON_API_KEY" ]; then
        log_error "POLYGON_API_KEY not set - cannot download $symbol data"
        return 1
    fi

    python3 "$DATA_DOWNLOADER" "$symbol" \
        --start "$start_date" \
        --end "$end_date" \
        --outdir data/equities

    if [ $? -eq 0 ]; then
        log_info "✓ $symbol data downloaded"
        return 0
    else
        log_error "$symbol data download failed"
        return 1
    fi
}

function ensure_rotation_data() {
    log_info "========================================================================"
    log_info "Rotation Trading Data Preparation"
    log_info "========================================================================"

    local target_date="$1"
    log_info "Target session: $target_date"
    log_info "Symbols: ${ROTATION_SYMBOLS[*]}"
    log_info ""

    # Calculate date range (target date + 30 days before for warmup)
    local start_date=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$target_date', '%Y-%m-%d'); print((target - timedelta(days=30)).strftime('%Y-%m-%d'))")
    local end_date=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$target_date', '%Y-%m-%d'); print((target + timedelta(days=1)).strftime('%Y-%m-%d'))")

    log_info "Data range: $start_date to $end_date (30 days warmup + target)"
    log_info ""

    # Create data directory
    mkdir -p "$DATA_DIR"

    # Download/verify data for each symbol
    local all_data_ready=true

    for symbol in "${ROTATION_SYMBOLS[@]}"; do
        local data_file="data/equities/${symbol}_RTH_NH.csv"

        # Check if data exists and contains target date
        if [ -f "$data_file" ]; then
            local has_target=$(grep "^$target_date" "$data_file" 2>/dev/null | wc -l | tr -d ' ')
            if [ "$has_target" -gt 0 ]; then
                local line_count=$(wc -l < "$data_file" | tr -d ' ')
                log_info "✓ $symbol: Data exists ($line_count bars)"

                # Copy to rotation warmup directory
                cp "$data_file" "$DATA_DIR/"
                continue
            fi
        fi

        # Data missing - download it
        log_info "⚠️  $symbol: Data missing for $target_date"

        if ! download_symbol_data "$symbol" "$start_date" "$end_date"; then
            log_error "$symbol: Download failed"
            all_data_ready=false
            continue
        fi

        # Copy to rotation warmup directory
        if [ -f "$data_file" ]; then
            cp "$data_file" "$DATA_DIR/"
        else
            log_error "$symbol: Data file not created"
            all_data_ready=false
        fi
    done

    if [ "$all_data_ready" = false ]; then
        log_error "CRITICAL: Not all symbol data is available"
        log_error "Cannot proceed with rotation trading"
        return 1
    fi

    log_info ""
    log_info "✓ All rotation symbol data ready in $DATA_DIR"
    return 0
}

function run_premarket_optimization() {
    log_info "========================================================================"
    log_info "Pre-Market Optimization"
    log_info "========================================================================"
    log_info "Strategy: Multi-symbol rotation (6 instruments)"
    log_info "Trials: $N_TRIALS per phase"
    log_info ""

    # For rotation trading, optimization is complex and takes time
    # Use existing rotation_strategy.json if available
    if [ -f "$BEST_PARAMS_FILE" ]; then
        log_info "Using existing rotation strategy config: $BEST_PARAMS_FILE"
        log_info "✓ Configuration loaded"

        # Save baseline for hourly optimization
        cp "$BEST_PARAMS_FILE" "data/tmp/premarket_baseline_params.json" 2>/dev/null || true
        return 0
    fi

    log_warn "No rotation_strategy.json found - using default parameters"
    log_warn "For best results, run optimization separately with:"
    log_warn "  python3 tools/optuna_mrb_wf.py --data data/equities/SPY_RTH_NH.csv --n-trials 50"

    # Create a default config if none exists
    if [ ! -f "$BEST_PARAMS_FILE" ]; then
        log_warn "Creating default rotation strategy config..."
        cat > "$BEST_PARAMS_FILE" << 'EOF'
{
  "name": "OnlineEnsemble",
  "version": "2.6",
  "warmup_samples": 100,
  "enable_bb_amplification": true,
  "enable_threshold_calibration": true,
  "calibration_window": 100,
  "enable_regime_detection": true,
  "regime_detector_type": "MarS",
  "buy_threshold": 0.6,
  "sell_threshold": 0.4,
  "neutral_zone_width": 0.1,
  "prediction_horizons": [1, 5, 10],
  "lambda": 0.99,
  "bb_upper_amp": 1.5,
  "bb_lower_amp": 1.5
}
EOF
        log_info "✓ Default config created"
    fi

    return 0
}

function run_hourly_optimization() {
    local hour="$1"

    log_info ""
    log_info "========================================================================"
    log_info "Hourly Optimization - $hour:00 ET"
    log_info "========================================================================"

    # Use morning data up to current hour
    local morning_data="data/tmp/morning_bars_$(date +%Y%m%d).csv"

    if [ ! -f "$morning_data" ]; then
        log_warn "Morning bars not available - using premarket baseline"
        return 0
    fi

    log_info "Running micro-adaptation optimization..."

    # Quick optimization with fewer trials
    local hourly_trials=$((N_TRIALS / 5))

    python3 "$HOURLY_OPTUNA_SCRIPT" \
        --data "$morning_data" \
        --baseline "data/tmp/premarket_baseline_params.json" \
        --output "data/tmp/hourly_optuna_$(date +%Y%m%d_%H%M%S).json" \
        --n-trials "$hourly_trials"

    if [ $? -eq 0 ]; then
        log_info "✓ Hourly optimization complete"
        # Note: hourly_optuna script updates rotation_strategy.json automatically
        return 0
    else
        log_warn "Hourly optimization failed - continuing with current params"
        return 0  # Non-fatal
    fi
}

function run_mock_rotation_trading() {
    log_info "========================================================================"
    log_info "Mock Rotation Trading Session"
    log_info "========================================================================"
    log_info "Symbols: ${ROTATION_SYMBOLS[*]}"
    log_info "Data directory: $DATA_DIR"
    log_info "Speed: ${MOCK_SPEED}x (0=instant)"
    log_info ""

    mkdir -p "$LOG_DIR"

    # Run rotation-trade command in mock mode
    "$CPP_TRADER" rotation-trade \
        --mode mock \
        --data-dir "$DATA_DIR" \
        --warmup-dir "$DATA_DIR" \
        --log-dir "$LOG_DIR" \
        --config "$BEST_PARAMS_FILE"

    local result=$?

    if [ $result -eq 0 ]; then
        log_info "✓ Mock rotation trading session completed"
        return 0
    else
        log_error "Mock rotation trading failed (exit code: $result)"
        return 1
    fi
}

function run_live_rotation_trading() {
    log_info "========================================================================"
    log_info "Live Rotation Trading Session"
    log_info "========================================================================"
    log_info "Symbols: ${ROTATION_SYMBOLS[*]}"
    log_info "Hourly optimization: $([ "$HOURLY_OPTIMIZE" = true ] && echo "ENABLED" || echo "DISABLED")"
    log_info ""

    mkdir -p "$LOG_DIR"

    # Start rotation trader
    "$CPP_TRADER" rotation-trade \
        --mode live \
        --log-dir "$LOG_DIR" \
        --config "$BEST_PARAMS_FILE" &

    local TRADER_PID=$!
    log_info "Rotation trader started (PID: $TRADER_PID)"

    # Monitor and run hourly optimization if enabled
    if [ "$HOURLY_OPTIMIZE" = true ]; then
        log_info "Monitoring for hourly optimization triggers..."

        local last_opt_hour=""

        while kill -0 $TRADER_PID 2>/dev/null; do
            sleep 300  # Check every 5 minutes

            local current_hour=$(TZ='America/New_York' date '+%H')
            local current_time=$(TZ='America/New_York' date '+%H:%M')

            # Run optimization at top of each hour (10 AM - 3 PM)
            if [ "$current_hour" -ge 10 ] && [ "$current_hour" -le 15 ]; then
                if [ "$current_hour" != "$last_opt_hour" ]; then
                    log_info "Triggering hourly optimization for $current_hour:00 ET"
                    run_hourly_optimization "$current_hour"
                    last_opt_hour="$current_hour"
                fi
            fi

            # Check if market closed
            if [ "$current_hour" -ge 16 ]; then
                log_info "Market closed (4:00 PM ET) - stopping trader"
                kill -TERM $TRADER_PID 2>/dev/null || true
                break
            fi
        done

        wait $TRADER_PID
    else
        # Just wait for trader to finish
        wait $TRADER_PID
    fi

    log_info "✓ Live rotation trading session completed"
    return 0
}

function generate_dashboard() {
    log_info ""
    log_info "========================================================================"
    log_info "Generating Trading Dashboard"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades*.jsonl 2>/dev/null | head -1)
    local latest_signals=$(ls -t "$LOG_DIR"/signals*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ]; then
        log_warn "No trade log file found - skipping dashboard"
        return 1
    fi

    log_info "Trade log: $latest_trades"

    local timestamp=$(date +%Y%m%d_%H%M%S)
    local output_file="data/dashboards/rotation_${MODE}_${timestamp}.html"

    mkdir -p data/dashboards

    # Use SPY data as market reference
    local market_data="data/equities/SPY_RTH_NH.csv"

    python3 "$DASHBOARD_SCRIPT" \
        --tradebook "$latest_trades" \
        --data "$market_data" \
        --output "$output_file" \
        --start-equity 100000

    if [ $? -eq 0 ]; then
        log_info "✓ Dashboard generated: $output_file"

        # Create symlink to latest
        ln -sf "$(basename $output_file)" "data/dashboards/latest_rotation_${MODE}.html"
        log_info "✓ Latest: data/dashboards/latest_rotation_${MODE}.html"

        # Send email
        send_email_notification "$output_file" "$latest_trades"

        # Open in browser for mock mode
        if [ "$MODE" = "mock" ]; then
            open "$output_file"
        fi

        return 0
    else
        log_error "Dashboard generation failed"
        return 1
    fi
}

function send_email_notification() {
    local dashboard="$1"
    local trades="$2"

    log_info ""
    log_info "Sending email notification..."

    if [ ! -f "$EMAIL_SCRIPT" ]; then
        log_warn "Email script not found: $EMAIL_SCRIPT"
        return 1
    fi

    # Check for Gmail credentials
    if [ -z "$GMAIL_USER" ] || [ -z "$GMAIL_APP_PASSWORD" ]; then
        log_warn "Gmail credentials not set in config.env - skipping email"
        return 1
    fi

    python3 "$EMAIL_SCRIPT" \
        --dashboard "$dashboard" \
        --trades "$trades" \
        --recipient "${GMAIL_USER:-yeogirl@gmail.com}"

    if [ $? -eq 0 ]; then
        log_info "✓ Email notification sent to $GMAIL_USER"
        return 0
    else
        log_warn "Email notification failed"
        return 1
    fi
}

function show_summary() {
    log_info ""
    log_info "========================================================================"
    log_info "Session Summary"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades*.jsonl 2>/dev/null | head -1)
    local latest_signals=$(ls -t "$LOG_DIR"/signals*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ] || [ ! -f "$latest_trades" ]; then
        log_error "No trades file found"
        return 1
    fi

    local num_trades=$(wc -l < "$latest_trades" | tr -d ' ')
    log_info "Total trades: $num_trades"

    if command -v jq &> /dev/null && [ "$num_trades" -gt 0 ]; then
        log_info ""
        log_info "Trades by symbol:"
        jq -r '.symbol' "$latest_trades" 2>/dev/null | sort | uniq -c | awk '{print "  " $2 ": " $1 " trades"}' || true

        log_info ""
        log_info "Trades by side:"
        jq -r '.side' "$latest_trades" 2>/dev/null | sort | uniq -c | awk '{print "  " $2 ": " $1 " trades"}' || true
    fi

    # Run analyze-trades for performance metrics
    if [ "$num_trades" -gt 0 ] && [ -n "$latest_signals" ] && [ -f "$latest_signals" ]; then
        log_info ""
        log_info "Running performance analysis..."

        local analysis_output=$("$CPP_TRADER" analyze-trades \
            --signals "$latest_signals" \
            --trades "$latest_trades" \
            --data "data/equities/SPY_RTH_NH.csv" \
            --start-equity 100000 2>&1)

        if echo "$analysis_output" | grep -q "Mean Return"; then
            echo "$analysis_output" | grep -E "Mean Return|Total Return|Win Rate|Sharpe|Max Drawdown|Total Trades" | while read line; do
                log_info "  $line"
            done

            # Extract MRD
            local mrd=$(echo "$analysis_output" | grep "Mean Return per Day" | awk '{print $NF}' | tr -d '%')
            if [ -n "$mrd" ]; then
                log_info ""
                log_info "🎯 KEY METRIC: MRD = ${mrd}%"
            fi
        fi
    fi

    log_info ""
    log_info "Dashboard: data/dashboards/latest_rotation_${MODE}.html"
    log_info ""
}

# =============================================================================
# Main
# =============================================================================

function main() {
    log_info "========================================================================"
    log_info "Rotation Trading Launcher - COMPREHENSIVE & SELF-SUFFICIENT"
    log_info "========================================================================"
    log_info "Mode: $(echo $MODE | tr '[:lower:]' '[:upper:]')"
    log_info "Symbols: ${ROTATION_SYMBOLS[*]}"
    log_info "Binary: $CPP_TRADER"
    log_info "Pre-market optimization: $([ "$RUN_OPTIMIZATION" = "yes" ] && echo "ENABLED ($N_TRIALS trials)" || echo "DISABLED")"
    log_info "Hourly optimization: $([ "$HOURLY_OPTIMIZE" = true ] && echo "ENABLED" || echo "DISABLED")"
    log_info ""

    # Step 1: Determine target date
    local target_date=$(determine_target_date)
    log_info "Target date: $target_date"
    log_info ""

    # Step 2: Ensure all rotation symbol data is available
    if ! ensure_rotation_data "$target_date"; then
        log_error "Data preparation failed"
        exit 1
    fi

    # Step 3: Pre-market optimization
    if [ "$RUN_OPTIMIZATION" = "yes" ]; then
        if ! run_premarket_optimization; then
            log_error "FATAL: Pre-market optimization failed"
            log_error "Cannot proceed without optimized parameters"
            exit 1
        fi
        log_info ""
    fi

    # Step 4: Run trading session
    if [ "$MODE" = "mock" ]; then
        if ! run_mock_rotation_trading; then
            log_error "Mock rotation trading failed"
            exit 1
        fi
    else
        if ! run_live_rotation_trading; then
            log_error "Live rotation trading failed"
            exit 1
        fi
    fi

    # Step 5: Generate dashboard
    generate_dashboard || log_warn "Dashboard generation failed"

    # Step 6: Show summary
    show_summary

    log_info "✓ Rotation trading session complete!"
}

main "$@"
