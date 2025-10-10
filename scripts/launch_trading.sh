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

function run_optimization() {
    log_info "========================================================================"
    log_info "2-Phase Optuna Optimization"
    log_info "========================================================================"
    log_info "Phase 1: Primary params (buy/sell thresholds, lambda, BB amp) - $N_TRIALS trials"
    log_info "Phase 2: Secondary params (horizon weights, BB, regularization) - $N_TRIALS trials"
    log_info ""

    # Use appropriate data file
    local opt_data_file="data/equities/SPY_warmup_latest.csv"
    if [ ! -f "$opt_data_file" ]; then
        opt_data_file="data/equities/SPY_4blocks.csv"
    fi

    if [ ! -f "$opt_data_file" ]; then
        log_error "No data file available for optimization"
        return 1
    fi

    log_info "Optimizing on: $opt_data_file"

    python3 "$OPTUNA_SCRIPT" \
        --data "$opt_data_file" \
        --output "$BEST_PARAMS_FILE" \
        --n-trials-phase1 "$N_TRIALS" \
        --n-trials-phase2 "$N_TRIALS" \
        --n-jobs 4

    if [ $? -eq 0 ]; then
        log_info "✓ Optimization complete - params saved to $BEST_PARAMS_FILE"
        # Copy to location where live trader reads from
        cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json" 2>/dev/null || true
        return 0
    else
        log_error "Optimization failed"
        return 1
    fi
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
" || log_info "⚠️  Failed to fetch morning bars - continuing without"

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

                if [ $? -eq 0 ]; then
                    log_info "✓ Midday optimization complete"
                    cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json"
                    log_info "✓ New parameters deployed"
                else
                    log_info "⚠️  Midday optimization failed - keeping current params"
                fi

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

    if [ -n "$latest_trades" ] && [ -f "$latest_trades" ]; then
        local num_trades=$(wc -l < "$latest_trades")
        log_info "Total trades: $num_trades"

        if command -v jq &> /dev/null && [ "$num_trades" -gt 0 ]; then
            log_info "Symbols traded:"
            jq -r '.symbol' "$latest_trades" 2>/dev/null | sort | uniq -c | awk '{print "  - " $2 ": " $1 " trades"}' || true
        fi
    fi

    log_info ""
    log_info "Dashboard: data/dashboards/latest_${MODE}.html"
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

    # Step 0: Mock Data Extraction (if needed)
    if [ "$MODE" = "mock" ] && [ "$DATA_FILE" = "auto" ]; then
        log_info "========================================================================"
        log_info "Extracting Session Data for Mock Mode"
        log_info "========================================================================"

        WARMUP_FILE="data/equities/SPY_warmup_latest.csv"
        SESSION_FILE="/tmp/SPY_session.csv"

        if [ -n "$MOCK_DATE" ]; then
            log_info "Target date: $MOCK_DATE"
            python3 tools/extract_session_data.py \
                --input data/equities/SPY_RTH_NH.csv \
                --date "$MOCK_DATE" \
                --output-warmup "$WARMUP_FILE" \
                --output-session "$SESSION_FILE"
        else
            log_info "Target date: Most recent (auto-detect)"
            python3 tools/extract_session_data.py \
                --input data/equities/SPY_RTH_NH.csv \
                --output-warmup "$WARMUP_FILE" \
                --output-session "$SESSION_FILE"
        fi

        if [ $? -ne 0 ]; then
            log_error "Failed to extract session data"
            exit 1
        fi

        DATA_FILE="$SESSION_FILE"
        log_info "✓ Session data extracted"
        log_info "  Warmup: $WARMUP_FILE (for optimization)"
        log_info "  Session: $DATA_FILE (for mock replay)"
        log_info ""
    fi

    # Step 1: Optimization (if enabled)
    if [ "$RUN_OPTIMIZATION" = "yes" ]; then
        if ! run_optimization; then
            log_info "⚠️  Optimization failed - continuing with existing params"
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
