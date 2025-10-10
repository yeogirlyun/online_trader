#!/bin/bash
#
# Unified Trading Launch Script - Mock & Live Trading with Auto-Optimization
#
# Features:
#   - Mock Mode: Replay historical data for testing
#   - Live Mode: Real paper trading with Alpaca REST API
#   - Pre-Market Optimization: 2-phase Optuna (50 trials each)
#   - Auto warmup and dashboard generation
#
# Usage:
#   ./scripts/launch_trading.sh [mode] [options]
#
# Modes:
#   mock     - Mock trading session (replay historical data)
#   live     - Live paper trading session (9:30 AM - 4:00 PM ET)
#
# Options:
#   --data FILE           Data file for mock mode (default: auto - last 391 bars)
#   --date YYYY-MM-DD     Replay specific date in mock mode (default: most recent day)
#   --speed N             Mock replay speed (default: 39.0x for proper time simulation)
#   --optimize            Run 2-phase Optuna before trading (default: auto for live)
#   --skip-optimize       Skip optimization, use existing params
#   --trials N            Trials per phase for optimization (default: 50)
#   --midday-optimize     Enable midday re-optimization at 2:30 PM ET (live mode only)
#   --midday-time HH:MM   Midday optimization time (default: 14:30)
#   --version VERSION     Binary version: "release" or "build" (default: build)
#
# Examples:
#   # Mock trading - replicates most recent live session exactly
#   # Includes: pre-market optimization, full session replay, EOD close, auto-shutdown, email
#   ./scripts/launch_trading.sh mock
#
#   # Mock specific date (e.g., Oct 7, 2025)
#   ./scripts/launch_trading.sh mock --date 2025-10-07
#
#   # Mock at real-time speed (1x) for detailed observation
#   ./scripts/launch_trading.sh mock --speed 1.0
#
#   # Mock with instant replay (0x speed)
#   ./scripts/launch_trading.sh mock --speed 0
#
#   # Live trading
#   ./scripts/launch_trading.sh live
#   ./scripts/launch_trading.sh live --skip-optimize
#   ./scripts/launch_trading.sh live --optimize --trials 100
#

set -e

# =============================================================================
# Configuration
# =============================================================================

# Defaults
MODE=""
DATA_FILE="auto"  # Auto-generate from SPY_RTH_NH.csv using date extraction
MOCK_SPEED=39.0   # Default 39x speed for proper time simulation
MOCK_DATE=""      # Optional: specific date to replay (YYYY-MM-DD), default=most recent
MOCK_SEND_EMAIL=false  # Send email in mock mode (for testing email system)
RUN_OPTIMIZATION="auto"
MIDDAY_OPTIMIZE=false
MIDDAY_TIME="15:15"  # Corrected to 3:15 PM ET (not 2:30 PM)
N_TRIALS=50
VERSION="build"
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        mock|live)
            MODE="$1"
            shift
            ;;
        --data)
            DATA_FILE="$2"
            shift 2
            ;;
        --speed)
            MOCK_SPEED="$2"
            shift 2
            ;;
        --date)
            MOCK_DATE="$2"
            shift 2
            ;;
        --send-email)
            MOCK_SEND_EMAIL=true
            shift
            ;;
        --optimize)
            RUN_OPTIMIZATION="yes"
            shift
            ;;
        --skip-optimize)
            RUN_OPTIMIZATION="no"
            shift
            ;;
        --midday-optimize)
            MIDDAY_OPTIMIZE=true
            shift
            ;;
        --midday-time)
            MIDDAY_TIME="$2"
            shift 2
            ;;
        --trials)
            N_TRIALS="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
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

# =============================================================================
# Single Instance Protection
# =============================================================================

# Check if trading is already running (only for live mode)
if [ "$MODE" = "live" ]; then
    if pgrep -f "sentio_cli.*live-trade" > /dev/null 2>&1; then
        echo "❌ ERROR: Live trading session already running"
        echo ""
        echo "Running processes:"
        ps aux | grep -E "sentio_cli.*live-trade|alpaca_websocket_bridge" | grep -v grep
        echo ""
        echo "To stop existing session:"
        echo "  pkill -f 'sentio_cli.*live-trade'"
        echo "  pkill -f 'alpaca_websocket_bridge'"
        exit 1
    fi
fi

# Determine optimization behavior
if [ "$RUN_OPTIMIZATION" = "auto" ]; then
    # ALWAYS run optimization for both live and mock modes
    # Mock mode should replicate live mode exactly, including optimization
    RUN_OPTIMIZATION="yes"
fi

cd "$PROJECT_ROOT"

# SSL Certificate
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem

# Load credentials
if [ -f config.env ]; then
    source config.env
fi

# Paths
if [ "$VERSION" = "release" ]; then
    CPP_TRADER="release/sentio_cli_latest"
else
    CPP_TRADER="build/sentio_cli"
fi

OPTUNA_SCRIPT="$PROJECT_ROOT/scripts/run_2phase_optuna.py"
WARMUP_SCRIPT="$PROJECT_ROOT/scripts/comprehensive_warmup.sh"
DASHBOARD_SCRIPT="$PROJECT_ROOT/scripts/professional_trading_dashboard.py"
EMAIL_SCRIPT="$PROJECT_ROOT/scripts/send_dashboard_email.py"
BEST_PARAMS_FILE="$PROJECT_ROOT/config/best_params.json"
LOG_DIR="logs/${MODE}_trading"

# Validate binary
if [ ! -f "$CPP_TRADER" ]; then
    echo "❌ ERROR: Binary not found: $CPP_TRADER"
    exit 1
fi

# Validate credentials for live mode
if [ "$MODE" = "live" ]; then
    if [ -z "$ALPACA_PAPER_API_KEY" ] || [ -z "$ALPACA_PAPER_SECRET_KEY" ]; then
        echo "❌ ERROR: Missing Alpaca credentials in config.env"
        exit 1
    fi
    export ALPACA_PAPER_API_KEY
    export ALPACA_PAPER_SECRET_KEY
fi

# Validate data file for mock mode (skip if auto-generating)
if [ "$MODE" = "mock" ] && [ "$DATA_FILE" != "auto" ] && [ ! -f "$DATA_FILE" ]; then
    echo "❌ ERROR: Data file not found: $DATA_FILE"
    exit 1
fi

# PIDs
TRADER_PID=""
BRIDGE_PID=""

# =============================================================================
# Functions
# =============================================================================

function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ❌ ERROR: $1" >&2
}

function cleanup() {
    if [ -n "$TRADER_PID" ] && kill -0 $TRADER_PID 2>/dev/null; then
        log_info "Stopping trader (PID: $TRADER_PID)..."
        kill -TERM $TRADER_PID 2>/dev/null || true
        sleep 2
        kill -KILL $TRADER_PID 2>/dev/null || true
    fi

    if [ -n "$BRIDGE_PID" ] && kill -0 $BRIDGE_PID 2>/dev/null; then
        log_info "Stopping Alpaca WebSocket bridge (PID: $BRIDGE_PID)..."
        kill -TERM $BRIDGE_PID 2>/dev/null || true
        sleep 1
        kill -KILL $BRIDGE_PID 2>/dev/null || true
    fi
}

function ensure_optimization_data() {
    log_info "========================================================================"
    log_info "Data Availability Check"
    log_info "========================================================================"

    local target_file="data/equities/SPY_RTH_NH_5years.csv"
    local min_days=30  # Minimum 30 trading days for meaningful optimization

    # Check if 5-year data exists and is recent
    if [ -f "$target_file" ]; then
        local file_age_days=$(( ($(date +%s) - $(stat -f %m "$target_file" 2>/dev/null || stat -c %Y "$target_file" 2>/dev/null)) / 86400 ))
        local line_count=$(wc -l < "$target_file")
        local trading_days=$((line_count / 391))

        log_info "Found 5-year data: $trading_days trading days (file age: $file_age_days days)"

        if [ "$trading_days" -ge "$min_days" ] && [ "$file_age_days" -le 7 ]; then
            log_info "✓ Data is sufficient and recent"
            echo "$target_file"
            return 0
        fi

        if [ "$file_age_days" -gt 7 ]; then
            log_warn "Data is older than 7 days - will continue with existing data"
            echo "$target_file"
            return 0
        fi
    else
        log_warn "5-year data file not found"
    fi

    # Fallback: Check for existing files with sufficient data
    for fallback_file in "data/equities/SPY_100blocks.csv" "data/equities/SPY_30blocks.csv" "data/equities/SPY_20blocks.csv"; do
        if [ -f "$fallback_file" ]; then
            local fallback_days=$(($(wc -l < "$fallback_file") / 391))
            if [ "$fallback_days" -ge "$min_days" ]; then
                log_warn "Using fallback: $fallback_file ($fallback_days days)"
                echo "$fallback_file"
                return 0
            fi
        fi
    done

    # Last resort: Try to generate from existing data
    if [ -f "data/equities/SPY_RTH_NH.csv" ]; then
        local existing_days=$(($(wc -l < "data/equities/SPY_RTH_NH.csv") / 391))
        if [ "$existing_days" -ge "$min_days" ]; then
            log_warn "Using existing RTH file: $existing_days days"
            echo "data/equities/SPY_RTH_NH.csv"
            return 0
        fi
    fi

    log_error "CRITICAL: Cannot find or generate sufficient data for optimization"
    log_error "Need at least $min_days trading days (~$((min_days * 391)) bars)"
    log_error "To fix: Run tools/data_downloader.py to generate SPY_RTH_NH_5years.csv"
    return 1
}

function run_morning_optimization() {
    log_info "========================================================================"
    log_info "Morning Pre-Market Optimization (6-10 AM ET)"
    log_info "========================================================================"
    log_info "Phase 1: Primary params (buy/sell thresholds, lambda, BB amp) - $N_TRIALS trials"
    log_info "Phase 2: DISABLED (using Phase 1 only for speed)"
    log_info ""

    local current_hour=$(TZ='America/New_York' date '+%H')
    local current_min=$(TZ='America/New_York' date '+%M')

    # Only run optimization during morning hours (6-10 AM ET)
    if [ "$current_hour" -lt 6 ] || [ "$current_hour" -ge 10 ]; then
        log_info "⚠️  Outside morning optimization window (6-10 AM ET)"
        log_info "   Current time: $current_hour:$current_min ET"
        log_info "   Skipping optimization - using existing parameters"
        return 0
    fi

    log_info "✓ Within morning optimization window (${current_hour}:${current_min} ET)"

    # Ensure we have sufficient data - never compromise!
    local opt_data_file
    opt_data_file=$(ensure_optimization_data 2>&1 | tail -1)
    local check_result=$?

    if [ $check_result -ne 0 ] || [ -z "$opt_data_file" ] || [ ! -f "$opt_data_file" ]; then
        log_error "Data availability check failed"
        return 1
    fi

    log_info "Optimizing on: $opt_data_file"

    python3 "$OPTUNA_SCRIPT" \
        --data "$opt_data_file" \
        --output "$BEST_PARAMS_FILE" \
        --n-trials-phase1 "$N_TRIALS" \
        --n-trials-phase2 0 \
        --n-jobs 4

    if [ $? -eq 0 ]; then
        log_info "✓ Optimization complete - params saved to $BEST_PARAMS_FILE"

        # Save morning baseline for micro-adaptations
        local baseline_file="data/tmp/morning_baseline_params.json"
        cp "$BEST_PARAMS_FILE" "$baseline_file"
        log_info "✓ Morning baseline saved to $baseline_file (for micro-adaptation)"

        # Copy to location where live trader reads from
        cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json" 2>/dev/null || true
        return 0
    else
        log_error "Optimization failed"
        return 1
    fi
}

function run_optimization() {
    # Wrapper for backward compatibility - calls morning optimization
    run_morning_optimization
}

function run_warmup() {
    log_info "========================================================================"
    log_info "Strategy Warmup (20 blocks + today's bars)"
    log_info "========================================================================"

    if [ -f "$WARMUP_SCRIPT" ]; then
        bash "$WARMUP_SCRIPT" 2>&1 | tee "$LOG_DIR/warmup_$(date +%Y%m%d).log"
        if [ $? -eq 0 ]; then
            log_info "✓ Warmup complete"
            return 0
        else
            log_error "Warmup failed"
            return 1
        fi
    else
        log_info "Warmup script not found - strategy will learn from live data"
        return 0
    fi
}

function run_mock_trading() {
    log_info "========================================================================"
    log_info "Mock Trading Session"
    log_info "========================================================================"
    log_info "Data: $DATA_FILE"
    log_info "Speed: ${MOCK_SPEED}x (0=instant)"
    log_info ""

    mkdir -p "$LOG_DIR"

    "$CPP_TRADER" live-trade --mock --mock-data "$DATA_FILE" --mock-speed "$MOCK_SPEED"

    if [ $? -eq 0 ]; then
        log_info "✓ Mock session completed"
        return 0
    else
        log_error "Mock session failed"
        return 1
    fi
}

function run_live_trading() {
    log_info "========================================================================"
    log_info "Live Paper Trading Session"
    log_info "========================================================================"
    log_info "Strategy: OnlineEnsemble EWRLS"
    log_info "Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)"
    log_info "Data source: Alpaca REST API (IEX feed)"
    log_info "EOD close: 3:58 PM ET"
    if [ "$MIDDAY_OPTIMIZE" = true ]; then
        log_info "Midday re-optimization: $MIDDAY_TIME ET"
    fi
    log_info ""

    # Load optimized params if available
    if [ -f "$BEST_PARAMS_FILE" ]; then
        log_info "Using optimized parameters from: $BEST_PARAMS_FILE"
        mkdir -p data/tmp
        cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json"
    fi

    mkdir -p "$LOG_DIR"

    # Start Alpaca WebSocket bridge (Python → FIFO → C++)
    log_info "Starting Alpaca WebSocket bridge..."
    local bridge_log="$LOG_DIR/bridge_$(date +%Y%m%d_%H%M%S).log"
    python3 "$PROJECT_ROOT/scripts/alpaca_websocket_bridge.py" > "$bridge_log" 2>&1 &
    BRIDGE_PID=$!

    log_info "Bridge PID: $BRIDGE_PID"
    log_info "Bridge log: $bridge_log"

    # Wait for FIFO to be created
    log_info "Waiting for FIFO pipe..."
    local fifo_wait=0
    while [ ! -p "/tmp/alpaca_bars.fifo" ] && [ $fifo_wait -lt 10 ]; do
        sleep 1
        fifo_wait=$((fifo_wait + 1))
    done

    if [ ! -p "/tmp/alpaca_bars.fifo" ]; then
        log_error "FIFO pipe not created - bridge may have failed"
        tail -20 "$bridge_log"
        return 1
    fi

    log_info "✓ Bridge connected and FIFO ready"
    log_info ""

    # Start C++ trader (reads from FIFO)
    log_info "Starting C++ trader..."
    local trader_log="$LOG_DIR/trader_$(date +%Y%m%d_%H%M%S).log"
    "$CPP_TRADER" live-trade > "$trader_log" 2>&1 &
    TRADER_PID=$!

    log_info "Trader PID: $TRADER_PID"
    log_info "Trader log: $trader_log"

    sleep 3
    if ! kill -0 $TRADER_PID 2>/dev/null; then
        log_error "Trader exited immediately"
        tail -30 "$trader_log"
        return 1
    fi

    log_info "✓ Live trading started"

    # Track if midday optimization was done
    local midday_opt_done=false

    # Monitor until market close or process dies
    while true; do
        sleep 30

        if ! kill -0 $TRADER_PID 2>/dev/null; then
            log_info "Trader process ended"
            break
        fi

        local current_time=$(TZ='America/New_York' date '+%H:%M')
        local time_num=$(echo "$current_time" | tr -d ':')

        if [ "$time_num" -ge 1600 ]; then
            log_info "Market closed (4:00 PM ET)"
            break
        fi

        # Midday optimization check
        if [ "$MIDDAY_OPTIMIZE" = true ] && [ "$midday_opt_done" = false ]; then
            local midday_num=$(echo "$MIDDAY_TIME" | tr -d ':')
            # Trigger if within 5 minutes of midday time
            if [ "$time_num" -ge "$midday_num" ] && [ "$time_num" -lt $((midday_num + 5)) ]; then
                log_info ""
                log_info "⚡ MIDDAY OPTIMIZATION TIME: $MIDDAY_TIME ET"
                log_info "Stopping trader for re-optimization and restart..."

                # Stop trader and bridge cleanly (send SIGTERM)
                log_info "Stopping trader..."
                kill -TERM $TRADER_PID 2>/dev/null || true
                wait $TRADER_PID 2>/dev/null || true
                log_info "✓ Trader stopped"

                log_info "Stopping bridge..."
                kill -TERM $BRIDGE_PID 2>/dev/null || true
                wait $BRIDGE_PID 2>/dev/null || true
                log_info "✓ Bridge stopped"

                # Fetch morning bars (9:30 AM - current time) for seamless warmup
                log_info "Fetching morning bars for seamless warmup..."
                local today=$(TZ='America/New_York' date '+%Y-%m-%d')
                local morning_bars_file="data/tmp/morning_bars_$(date +%Y%m%d).csv"
                mkdir -p data/tmp

                # Use Python to fetch morning bars via Alpaca API
                python3 -c "
import os
import sys
import json
import requests
from datetime import datetime, timezone
import pytz

api_key = os.getenv('ALPACA_PAPER_API_KEY')
secret_key = os.getenv('ALPACA_PAPER_SECRET_KEY')

if not api_key or not secret_key:
    print('ERROR: Missing Alpaca credentials', file=sys.stderr)
    sys.exit(1)

# Fetch bars from 9:30 AM ET to now
et_tz = pytz.timezone('America/New_York')
now_et = datetime.now(et_tz)
start_time = now_et.replace(hour=9, minute=30, second=0, microsecond=0)

# Convert to ISO format with timezone
start_iso = start_time.isoformat()
end_iso = now_et.isoformat()

url = f'https://data.alpaca.markets/v2/stocks/SPY/bars?start={start_iso}&end={end_iso}&timeframe=1Min&limit=10000&adjustment=raw&feed=iex'
headers = {
    'APCA-API-KEY-ID': api_key,
    'APCA-API-SECRET-KEY': secret_key
}

try:
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    data = response.json()

    bars = data.get('bars', [])
    if not bars:
        print('WARNING: No morning bars returned', file=sys.stderr)
        sys.exit(0)

    # Write to CSV
    with open('$morning_bars_file', 'w') as f:
        f.write('timestamp,open,high,low,close,volume\\n')
        for bar in bars:
            ts_str = bar['t']
            dt = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
            ts_ms = int(dt.timestamp() * 1000)
            f.write(f\"{ts_ms},{bar['o']},{bar['h']},{bar['l']},{bar['c']},{bar['v']}\\n\")

    print(f'✓ Fetched {len(bars)} morning bars')
except Exception as e:
    print(f'ERROR: Failed to fetch morning bars: {e}', file=sys.stderr)
    sys.exit(1)
"
                # CRASH FAST: If morning bars fetch fails, EXIT immediately
                if [ $? -ne 0 ]; then
                    log_error "❌ FATAL: Failed to fetch morning bars for midday optimization"
                    log_error "   Cannot proceed with midday optimization without fresh data"
                    log_error "   Stopping trader and exiting..."
                    kill -TERM $TRADER_PID 2>/dev/null || true
                    kill -TERM $BRIDGE_PID 2>/dev/null || true
                    exit 1
                fi

                # Append morning bars to warmup file for seamless continuation
                if [ -f "$morning_bars_file" ]; then
                    local morning_bar_count=$(tail -n +2 "$morning_bars_file" | wc -l | tr -d ' ')
                    log_info "Appending $morning_bar_count morning bars to warmup data..."
                    tail -n +2 "$morning_bars_file" >> "data/equities/SPY_warmup_latest.csv"
                    log_info "✓ Seamless warmup data prepared"
                fi

                # Run quick optimization (fewer trials for speed)
                local midday_trials=$((N_TRIALS / 2))
                log_info "Running midday optimization ($midday_trials trials/phase)..."

                python3 "$OPTUNA_SCRIPT" \
                    --data "data/equities/SPY_warmup_latest.csv" \
                    --output "$BEST_PARAMS_FILE" \
                    --n-trials-phase1 "$midday_trials" \
                    --n-trials-phase2 "$midday_trials" \
                    --n-jobs 4

                # CRASH FAST: Midday optimization must succeed
                if [ $? -ne 0 ]; then
                    log_error "❌ FATAL: Midday optimization failed"
                    log_error "   Cannot restart trading with unoptimized parameters"
                    log_error "   This is a CRITICAL error - exiting immediately"
                    exit 1
                fi

                log_info "✓ Midday optimization complete"
                cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json"
                log_info "✓ New parameters deployed"

                # Restart bridge and trader immediately with new params and seamless warmup
                log_info "Restarting bridge and trader with optimized params and seamless warmup..."

                # Restart bridge first
                local restart_bridge_log="$LOG_DIR/bridge_restart_$(date +%Y%m%d_%H%M%S).log"
                python3 "$PROJECT_ROOT/scripts/alpaca_websocket_bridge.py" > "$restart_bridge_log" 2>&1 &
                BRIDGE_PID=$!
                log_info "✓ Bridge restarted (PID: $BRIDGE_PID)"

                # Wait for FIFO
                log_info "Waiting for FIFO pipe..."
                local fifo_wait=0
                while [ ! -p "/tmp/alpaca_bars.fifo" ] && [ $fifo_wait -lt 10 ]; do
                    sleep 1
                    fifo_wait=$((fifo_wait + 1))
                done

                if [ ! -p "/tmp/alpaca_bars.fifo" ]; then
                    log_error "FIFO pipe not created - bridge restart failed"
                    tail -20 "$restart_bridge_log"
                    exit 1
                fi

                # Restart trader
                local restart_trader_log="$LOG_DIR/trader_restart_$(date +%Y%m%d_%H%M%S).log"
                "$CPP_TRADER" live-trade > "$restart_trader_log" 2>&1 &
                TRADER_PID=$!

                log_info "✓ Trader restarted (PID: $TRADER_PID)"
                log_info "✓ Bridge log: $restart_bridge_log"
                log_info "✓ Trader log: $restart_trader_log"

                sleep 3
                if ! kill -0 $TRADER_PID 2>/dev/null; then
                    log_error "Trader failed to restart"
                    tail -30 "$restart_log"
                    exit 1
                fi

                midday_opt_done=true
                log_info "✓ Midday optimization and restart complete - trading resumed"
                log_info ""
            fi
        fi

        # Status every 5 minutes
        if [ $(($(date +%s) % 300)) -lt 30 ]; then
            log_info "Status: Trading ✓ | Time: $current_time ET"
        fi
    done

    return 0
}

function generate_dashboard() {
    log_info ""
    log_info "========================================================================"
    log_info "Generating Trading Dashboard"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades_*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ]; then
        log_error "No trade log file found"
        return 1
    fi

    log_info "Trade log: $latest_trades"

    # Determine market data file
    local market_data="$DATA_FILE"
    if [ "$MODE" = "live" ] && [ -f "data/equities/SPY_warmup_latest.csv" ]; then
        market_data="data/equities/SPY_warmup_latest.csv"
    fi

    local timestamp=$(date +%Y%m%d_%H%M%S)
    local output_file="data/dashboards/${MODE}_session_${timestamp}.html"

    mkdir -p data/dashboards

    python3 "$DASHBOARD_SCRIPT" \
        --tradebook "$latest_trades" \
        --data "$market_data" \
        --output "$output_file" \
        --start-equity 100000

    if [ $? -eq 0 ]; then
        log_info "✓ Dashboard: $output_file"
        ln -sf "$(basename $output_file)" "data/dashboards/latest_${MODE}.html"
        log_info "✓ Latest: data/dashboards/latest_${MODE}.html"

        # Send email notification
        log_info ""
        log_info "Sending email notification..."

        # Source config.env for GMAIL credentials
        if [ -f "$PROJECT_ROOT/config.env" ]; then
            source "$PROJECT_ROOT/config.env"
        fi

        # Send email with dashboard
        python3 "$EMAIL_SCRIPT" \
            --dashboard "$output_file" \
            --trades "$latest_trades" \
            --recipient "${GMAIL_USER:-yeogirl@gmail.com}"

        if [ $? -eq 0 ]; then
            log_info "✓ Email notification sent"
        else
            log_warn "⚠️  Email notification failed (check GMAIL_APP_PASSWORD in config.env)"
        fi

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

function show_summary() {
    log_info ""
    log_info "========================================================================"
    log_info "Trading Session Summary"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades_*.jsonl 2>/dev/null | head -1)
    local latest_signals=$(ls -t "$LOG_DIR"/signals_*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ] || [ ! -f "$latest_trades" ]; then
        log_error "No trades file found - session may have failed"
        return 1
    fi

    local num_trades=$(wc -l < "$latest_trades")
    log_info "Total trades: $num_trades"

    if command -v jq &> /dev/null && [ "$num_trades" -gt 0 ]; then
        log_info "Symbols traded:"
        jq -r '.symbol' "$latest_trades" 2>/dev/null | sort | uniq -c | awk '{print "  - " $2 ": " $1 " trades"}' || true
    fi

    log_info ""
    log_info "Dashboard: data/dashboards/latest_${MODE}.html"

    # Run analyze-trades to get MRD and performance metrics
    if [ "$num_trades" -gt 0 ] && [ -n "$latest_signals" ] && [ -f "$latest_signals" ]; then
        log_info ""
        log_info "========================================================================"
        log_info "Performance Analysis (via analyze-trades)"
        log_info "========================================================================"

        # Determine market data file
        local market_data="$DATA_FILE"
        if [ "$MODE" = "live" ] && [ -f "data/equities/SPY_warmup_latest.csv" ]; then
            market_data="data/equities/SPY_warmup_latest.csv"
        fi

        # Run analyze-trades and capture output
        local analysis_output=$("$CPP_TRADER" analyze-trades \
            --signals "$latest_signals" \
            --trades "$latest_trades" \
            --data "$market_data" \
            --start-equity 100000 2>&1)

        # Extract and display key metrics
        if echo "$analysis_output" | grep -q "Mean Return"; then
            echo "$analysis_output" | grep -E "Mean Return|Total Return|Win Rate|Sharpe|Max Drawdown|Total Trades" | while read line; do
                log_info "  $line"
            done

            # Extract MRD specifically and highlight it
            local mrd=$(echo "$analysis_output" | grep "Mean Return per Day" | awk '{print $NF}' | tr -d '%')
            if [ -n "$mrd" ]; then
                log_info ""
                log_info "🎯 KEY METRIC: MRD = ${mrd}%"

                # Provide context based on MRD
                local mrd_float=$(echo "$mrd" | sed 's/%//')
                if (( $(echo "$mrd_float > 0.5" | bc -l) )); then
                    log_info "   ✅ EXCELLENT - Above 0.5% target!"
                elif (( $(echo "$mrd_float > 0.3" | bc -l) )); then
                    log_info "   ✓ GOOD - Above 0.3% baseline"
                elif (( $(echo "$mrd_float > 0" | bc -l) )); then
                    log_info "   ⚠️  MARGINAL - Positive but below target"
                else
                    log_info "   ❌ POOR - Negative returns"
                fi
            fi
        else
            log_info "⚠️  Could not extract performance metrics from analyze-trades"
            log_info "Raw output:"
            echo "$analysis_output" | head -20
        fi
    else
        log_info "⚠️  Skipping performance analysis (no trades or signals)"
    fi

    log_info ""
}

# =============================================================================
# Main
# =============================================================================

function main() {
    log_info "========================================================================"
    log_info "OnlineTrader - Unified Trading Launcher"
    log_info "========================================================================"
    log_info "Mode: $(echo $MODE | tr '[:lower:]' '[:upper:]')"
    log_info "Binary: $CPP_TRADER"
    if [ "$MODE" = "live" ]; then
        log_info "Pre-market optimization: $([ "$RUN_OPTIMIZATION" = "yes" ] && echo "YES ($N_TRIALS trials/phase)" || echo "NO")"
        log_info "Midday re-optimization: $([ "$MIDDAY_OPTIMIZE" = true ] && echo "YES at $MIDDAY_TIME ET" || echo "NO")"
        log_info "API Key: ${ALPACA_PAPER_API_KEY:0:8}..."
    else
        log_info "Data: $DATA_FILE"
        log_info "Speed: ${MOCK_SPEED}x"
    fi
    log_info ""

    trap cleanup EXIT INT TERM

    # Step 0: Data Preparation
    log_info "========================================================================"
    log_info "Data Preparation"
    log_info "========================================================================"

    # Determine target session date
    if [ -n "$MOCK_DATE" ]; then
        TARGET_DATE="$MOCK_DATE"
        log_info "Target session: $TARGET_DATE (specified)"
    else
        # Auto-detect most recent trading session from current date/time
        # Use Python for reliable date/time handling
        TARGET_DATE=$(python3 -c "
import os
os.environ['TZ'] = 'America/New_York'
import time
time.tzset()

from datetime import datetime, timedelta

now = datetime.now()
current_date = now.date()
current_hour = now.hour
current_weekday = now.weekday()  # 0=Mon, 4=Fri, 5=Sat, 6=Sun

# Determine most recent complete trading session
if current_weekday == 5:  # Saturday
    target_date = current_date - timedelta(days=1)  # Friday
elif current_weekday == 6:  # Sunday
    target_date = current_date - timedelta(days=2)  # Friday
elif current_weekday == 0:  # Monday
    if current_hour < 16:  # Before market close
        target_date = current_date - timedelta(days=3)  # Previous Friday
    else:  # After market close
        target_date = current_date  # Today (Monday)
else:  # Tuesday-Friday
    if current_hour >= 16:  # After market close (4 PM ET)
        target_date = current_date  # Today is complete
    else:  # Before market close
        target_date = current_date - timedelta(days=1)  # Yesterday

print(target_date.strftime('%Y-%m-%d'))
")

        log_info "Target session: $TARGET_DATE (auto-detected - market closed)"
    fi

    # Check if data exists for target date
    DATA_EXISTS=$(grep "^$TARGET_DATE" data/equities/SPY_RTH_NH.csv 2>/dev/null | wc -l | tr -d ' ')

    if [ "$DATA_EXISTS" -eq 0 ]; then
        log_info "⚠️  Data for $TARGET_DATE not found in SPY_RTH_NH.csv"
        log_info "Downloading data from Polygon.io..."

        # Check for API key
        if [ -z "$POLYGON_API_KEY" ]; then
            log_error "POLYGON_API_KEY not set - cannot download data"
            log_error "Please set POLYGON_API_KEY in your environment or config.env"
            exit 1
        fi

        # Download data for target date (include a few days before for safety)
        # Use Python for cross-platform date arithmetic
        START_DATE=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$TARGET_DATE', '%Y-%m-%d'); print((target - timedelta(days=7)).strftime('%Y-%m-%d'))")
        END_DATE=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$TARGET_DATE', '%Y-%m-%d'); print((target + timedelta(days=1)).strftime('%Y-%m-%d'))")

        log_info "Downloading SPY data from $START_DATE to $END_DATE..."
        python3 tools/data_downloader.py SPY \
            --start "$START_DATE" \
            --end "$END_DATE" \
            --outdir data/equities

        if [ $? -ne 0 ]; then
            log_error "Data download failed"
            exit 1
        fi

        log_info "✓ Data downloaded and saved to data/equities/SPY_RTH_NH.csv"
    else
        log_info "✓ Data for $TARGET_DATE exists ($DATA_EXISTS bars)"
    fi

    # Extract warmup and session data
    if [ "$MODE" = "mock" ]; then
        log_info ""
        log_info "Extracting session data for mock replay..."

        WARMUP_FILE="data/equities/SPY_warmup_latest.csv"
        SESSION_FILE="/tmp/SPY_session.csv"

        python3 tools/extract_session_data.py \
            --input data/equities/SPY_RTH_NH.csv \
            --date "$TARGET_DATE" \
            --output-warmup "$WARMUP_FILE" \
            --output-session "$SESSION_FILE"

        if [ $? -ne 0 ]; then
            log_error "Failed to extract session data"
            exit 1
        fi

        DATA_FILE="$SESSION_FILE"
        log_info "✓ Session data extracted"
        log_info "  Warmup: $WARMUP_FILE (for optimization)"
        log_info "  Session: $DATA_FILE (for mock replay)"

        # Generate leveraged ETF data from SPY
        log_info ""
        log_info "Generating leveraged ETF price data..."
        if [ -f "tools/generate_spy_leveraged_data.py" ]; then
            python3 tools/generate_spy_leveraged_data.py \
                --spy data/equities/SPY_RTH_NH.csv \
                --output-dir data/equities 2>&1 | grep -E "✓|✅|Generated|ERROR" || true
            log_info "✓ Leveraged ETF data ready"

            # Copy leveraged ETF files to /tmp for mock broker
            log_info "Copying leveraged ETF data to /tmp for mock broker..."
            for symbol in SH SDS SPXL; do
                if [ -f "data/equities/${symbol}_RTH_NH.csv" ]; then
                    cp "data/equities/${symbol}_RTH_NH.csv" "/tmp/${symbol}_yesterday.csv"
                fi
            done
            log_info "✓ Leveraged ETF data copied to /tmp"
        else
            log_warn "generate_spy_leveraged_data.py not found - skipping"
        fi

    elif [ "$MODE" = "live" ]; then
        log_info ""
        log_info "Preparing warmup data for live trading..."

        WARMUP_FILE="data/equities/SPY_warmup_latest.csv"

        # For live mode: extract all data UP TO yesterday (exclude today)
        YESTERDAY=$(python3 -c "from datetime import datetime, timedelta; print((datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d'))")

        python3 tools/extract_session_data.py \
            --input data/equities/SPY_RTH_NH.csv \
            --date "$YESTERDAY" \
            --output-warmup "$WARMUP_FILE" \
            --output-session /tmp/dummy.csv  # Not used in live mode

        if [ $? -ne 0 ]; then
            log_error "Failed to extract warmup data"
            exit 1
        fi

        log_info "✓ Warmup data prepared"
        log_info "  Warmup: $WARMUP_FILE (up to $YESTERDAY)"
    fi

    log_info ""

    # Step 1: Optimization (if enabled)
    # CRASH FAST PRINCIPLE: If optimization fails, STOP IMMEDIATELY
    if [ "$RUN_OPTIMIZATION" = "yes" ]; then
        if ! run_optimization; then
            log_error "❌ FATAL: Optimization failed"
            log_error "   Reason: Optimization is REQUIRED before trading"
            log_error "   Action: Script will EXIT immediately (no fallback)"
            log_error ""
            log_error "CRASH FAST PRINCIPLE: Never trade with unoptimized or stale parameters"
            exit 1
        fi
        log_info ""
    fi

    # Step 2: Warmup (live mode only, before market open)
    if [ "$MODE" = "live" ]; then
        local current_hour=$(TZ='America/New_York' date '+%H')
        if [ "$current_hour" -lt 9 ] || [ "$current_hour" -ge 16 ]; then
            log_info "Waiting for market open (9:30 AM ET)..."
            while true; do
                current_hour=$(TZ='America/New_York' date '+%H')
                current_min=$(TZ='America/New_York' date '+%M')
                current_dow=$(TZ='America/New_York' date '+%u')

                # Skip weekends
                if [ "$current_dow" -ge 6 ]; then
                    log_info "Weekend - waiting..."
                    sleep 3600
                    continue
                fi

                # Check if market hours
                if [ "$current_hour" -ge 9 ] && [ "$current_hour" -lt 16 ]; then
                    break
                fi

                sleep 60
            done
        fi

        if ! run_warmup; then
            log_info "⚠️  Warmup failed - strategy will learn from live data"
        fi
        log_info ""
    fi

    # Step 3: Trading session
    if [ "$MODE" = "mock" ]; then
        if ! run_mock_trading; then
            log_error "Mock trading failed"
            exit 1
        fi
    else
        if ! run_live_trading; then
            log_error "Live trading failed"
            exit 1
        fi
    fi

    # Step 4: Dashboard
    log_info ""
    generate_dashboard || log_info "⚠️  Dashboard generation failed"

    # Step 5: Summary
    show_summary

    log_info ""
    log_info "✓ Session complete!"
}

main "$@"
