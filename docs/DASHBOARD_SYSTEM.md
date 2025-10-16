# Unified Trading Dashboard System

## Overview

OnlineTrader uses **ONE** dashboard generator for all trading sessions (live and mock). The dashboard is automatically generated at the end of each trading session.

## Dashboard Generator

**File**: `tools/professional_trading_dashboard.py`

This is the **ONLY** dashboard generator used by the system. Do not use or create any other dashboard scripts.

### Features

- **Full trading day visualization**: SPY price chart with all trade markers
- **All instruments supported**: SPY, SPXL, SH, SDS (all positioned on SPY chart for easy viewing)
- **Interactive markers**: Bright green BUY triangles, bright red SELL triangles
- **Detailed hover information**: Symbol, price, quantity, cash, portfolio value, P&L, reason
- **Portfolio value chart**: Equity curve over time
- **Trade table**: Complete trade history with all details
- **Performance metrics**: Win rate, total return, max drawdown, etc.
- **ET time labels**: Market hours displayed in Eastern Time

### Usage

```bash
python3 tools/professional_trading_dashboard.py \
  --tradebook logs/mock_trading/trades_20251009_161619.jsonl \
  --data /tmp/SPY_yesterday_with_header.csv \
  --output data/dashboards/session_FULL_DAY.html \
  --start-equity 100000
```

## Automatic Dashboard Generation

### Live Trading

**Script**: `tools/launch_live_trading.sh`

The live trading launch script automatically generates the dashboard when the trading session ends:

```bash
# Start live trading (dashboard generated automatically at end)
./tools/launch_live_trading.sh
```

Dashboard is saved to:
- `data/dashboards/session_YYYYMMDD_HHMMSS.html`
- `data/dashboards/latest.html` (symlink to most recent)

### Mock Trading

**Script**: `tools/run_mock_session.sh`

The mock trading script automatically generates and opens the dashboard:

```bash
# Run mock session with instant replay
./tools/run_mock_session.sh

# Run mock session with custom data
./tools/run_mock_session.sh data/equities/SPY_4blocks.csv 0

# Run mock session in real-time
./tools/run_mock_session.sh /tmp/SPY_yesterday.csv 1
```

Dashboard is saved to:
- `data/dashboards/mock_session_YYYYMMDD_HHMMSS.html`
- `data/dashboards/latest_mock.html` (symlink to most recent)

## Dashboard Files

### Active Scripts
- `tools/professional_trading_dashboard.py` - **Main dashboard generator (use this one)**
- `tools/send_dashboard_email.py` - Email notification with dashboard
- `tools/screenshot_dashboard.py` - Dashboard screenshot for email

### Archived Scripts (Do Not Use)
- `tools/archive/generate_trade_dashboard.py` - Old duplicate
- `tools/archive/generate_dashboard_image.py` - Old duplicate
- `tools/archive/professional_trading_dashboard_v2.py.backup` - Old backup

## Email Notifications

To receive email notifications with the dashboard after each session:

1. Configure Gmail credentials in `config.env`:
   ```bash
   GMAIL_USER="your-email@gmail.com"
   GMAIL_APP_PASSWORD="your-app-password"
   ```

2. Email will be sent automatically after dashboard generation (configured in launch scripts)

## Dashboard Output Location

All dashboards are saved to `data/dashboards/`:

```
data/dashboards/
├── session_20251009_160000.html          # Live trading session
├── mock_session_20251009_153000.html     # Mock trading session
├── latest.html -> session_20251009_160000.html        # Symlink to latest live
└── latest_mock.html -> mock_session_20251009_153000.html  # Symlink to latest mock
```

## Customization

### Marker Appearance

Edit `tools/professional_trading_dashboard.py`:

```python
# BUY markers (line ~464)
marker=dict(symbol='triangle-up', size=20, color='#00ff00', line=dict(width=2, color='darkgreen'))

# SELL markers (line ~531)
marker=dict(symbol='triangle-down', size=20, color='#ff0000', line=dict(width=2, color='darkred'))
```

### Dashboard Layout

The dashboard uses Plotly subplots with 2 rows:
- Row 1: SPY price chart with trade markers
- Row 2: Portfolio value chart

## Troubleshooting

### No trade markers visible

Check if trades have the correct fields:
```bash
head -1 logs/mock_trading/trades_YYYYMMDD_HHMMSS.jsonl | jq .
```

Required fields:
- `timestamp` (string: "2025-10-07 09:30:00 America/New_York") OR `timestamp_ms` (number)
- `symbol` (string: "SPY", "SPXL", "SH", "SDS")
- `side` (string: "buy", "sell") OR `action` (string: "BUY", "SELL")
- `price` or `filled_avg_price` (number)
- `quantity` or `filled_qty` (number)
- `portfolio_value` (number)
- `cash_balance` (number)

### Dashboard shows only 1 hour of data

Make sure you're using a complete trading session file. Check file size:
```bash
ls -lh logs/mock_trading/trades_*.jsonl
```

Full session files are typically 15-25KB (80-100 trades for a full day).

### Market data not loading

Ensure the CSV has the correct header:
```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
```

## Development

When improving the dashboard:

1. **Only modify** `tools/professional_trading_dashboard.py`
2. **Test with real data**: Use `logs/mock_trading/trades_20251009_161619.jsonl` (97 trades, full day)
3. **Check all features**: Verify markers, hover info, time labels, portfolio chart
4. **Update this document** if you change the interface or behavior

## Summary

**Use ONE dashboard system:**
- Live trading: `./tools/launch_live_trading.sh` → auto-generates dashboard
- Mock trading: `./tools/run_mock_session.sh` → auto-generates dashboard
- Manual: `python3 tools/professional_trading_dashboard.py` → generate on demand

**All dashboards use the same code**: `tools/professional_trading_dashboard.py`

**No other dashboard scripts should be used or created.**
