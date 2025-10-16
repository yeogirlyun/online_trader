# Script Organization Summary
**Date**: 2025-10-10

## Changes Made

### 1. Script Folder Reorganization

**Moved TO scripts/** (used by launch_trading.sh):
- `send_dashboard_email.py` - from tools/ to scripts/

**Moved TO tools/** (not used by launch_trading.sh):
- `monitor_trading.sh` - from scripts/ to tools/

### 2. Scripts Folder - Launch System Components

These scripts are **only** used by `launch_trading.sh`:

```
scripts/
├── alpaca_websocket_bridge.py       # Live market data streaming
├── comprehensive_warmup.sh           # Strategy warmup process
├── launch_trading.sh                 # Main launcher (mock/live)
├── professional_trading_dashboard.py # HTML dashboard generation
├── run_2phase_optuna.py             # Parameter optimization
└── send_dashboard_email.py          # Email notifications (NEW)
```

### 3. Tools Folder - Utilities

Supporting utilities not directly called by launch_trading.sh:

```
tools/
├── data_downloader.py               # Download market data
├── extract_session_data.py          # Extract session data
├── generate_spy_leveraged_data.py   # Generate leveraged ETF data
├── monitor_trading.sh               # Manual trading monitor (MOVED)
├── ab_test_runner.sh                # A/B testing
├── backtest.py                      # Standalone backtester
├── ... (other utilities)
```

### 4. Email Integration Added

**Updated**: `scripts/launch_trading.sh`

The `generate_dashboard()` function now:
1. Generates HTML dashboard
2. **Sources config.env** for Gmail credentials
3. **Sends email** with dashboard screenshot
4. Opens browser (mock mode only)

**Location**: `scripts/launch_trading.sh:658-677`

**Code Added**:
```bash
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
```

## Workflow Integration

Dashboard & email are now **automatically** sent for **both modes**:

### Mock Mode:
```bash
./scripts/launch_trading.sh mock --trials 3
```
1. Data preparation
2. Optimization (3 trials Phase 1 & 2)
3. Mock trading session
4. **Dashboard generation** ← NEW: Includes email
5. Summary

### Live Mode:
```bash
./scripts/launch_trading.sh live
```
1. Data preparation
2. Optimization (optional)
3. Live trading session
4. **Dashboard generation** ← NEW: Includes email
5. Summary

## Configuration

Ensure `config.env` contains:
```bash
export GMAIL_USER="yeogirl@gmail.com"
export GMAIL_APP_PASSWORD="pxam kfad bpuy akmu"
```

## Testing

To test email integration manually:
```bash
source config.env
python3 scripts/send_dashboard_email.py \
    --dashboard data/dashboards/latest_mock.html \
    --trades logs/mock_trading/trades_latest.jsonl \
    --recipient yeogirl@gmail.com
```

## Benefits

✅ **Single Command**: Just run `launch_trading.sh` - everything happens automatically
✅ **Email Notifications**: Dashboard + screenshot emailed after every session
✅ **Clean Organization**: Scripts folder only contains launch system components
✅ **Automatic**: No manual dashboard generation or email sending needed
