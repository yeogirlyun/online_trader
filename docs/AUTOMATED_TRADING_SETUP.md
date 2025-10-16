# Automated Trading Setup Guide

**Version**: 2.2
**Date**: 2025-10-09
**Status**: ✅ PRODUCTION READY

---

## Overview

OnlineTrader now supports fully automated trading with:
- 🤖 **Auto-start**: Cron job starts trading at 9:15 AM ET (Monday-Friday)
- 🔄 **Auto-warmup**: 20 blocks + today's bars loaded automatically
- 📊 **Auto-dashboard**: HTML dashboard generated at end of session
- 📧 **Auto-email**: Dashboard sent to yeogirl@gmail.com with P&L summary
- 🎯 **Version control**: Release vs build binary selection

---

## Quick Start

### 1. Setup Gmail App Password (Required for Email Notifications)

```bash
# Generate app password at: https://myaccount.google.com/apppasswords
# Add to config.env:
echo "GMAIL_USER=yeogirl@gmail.com" >> config.env
echo "GMAIL_APP_PASSWORD=your-16-char-password-here" >> config.env
```

### 2. Install Cron Job

```bash
./tools/install_cron.sh
```

This will:
- Install cron job for Monday-Friday at 9:15 AM ET
- Perform warmup and launch trading at 9:30 AM ET
- Send dashboard email at end of day

### 3. Manual Testing (Optional)

```bash
# Test warmup
bash tools/comprehensive_warmup.sh

# Test manual launch with release binary
bash tools/launch_live_trading.sh release

# Test email notification
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_YYYYMMDD_HHMMSS.html \
    --trades logs/live_trading/trades_YYYYMMDD_HHMMSS.jsonl \
    --recipient yeogirl@gmail.com
```

---

## Architecture

### Release Management

**Release Folder**: `release/`
```
release/
├── sentio_cli_v2.1     # Previous version
├── sentio_cli_v2.2     # Current version
└── sentio_cli_latest   # Symlink to current (→ v2.2)
```

**Creating New Release**:
```bash
# Build and test
cmake --build build --target sentio_cli -j8
# ... test thoroughly ...

# Create release
cp build/sentio_cli release/sentio_cli_v2.3
ln -sf sentio_cli_v2.3 release/sentio_cli_latest
```

### Binary Selection

**Launch Script**: `tools/launch_live_trading.sh [version] [midday_time] [eod_time]`

```bash
# Use release binary (default)
./tools/launch_live_trading.sh release

# Use build binary (for testing)
./tools/launch_live_trading.sh build

# Custom times with release
./tools/launch_live_trading.sh release 14:30 15:58
```

### Automated Workflow

```
┌─────────────────────────────────────────────────────────┐
│  9:15 AM ET - Cron Job Triggers                        │
│  (tools/cron_launcher.sh)                              │
└────────────────┬────────────────────────────────────────┘
                 │
                 ├─ ✓ Check if trading day (Mon-Fri)
                 ├─ ✓ Check not already running
                 ├─ ✓ Validate credentials
                 │
                 ├─ 📚 Comprehensive Warmup
                 │   ├─ Load 20 historical blocks (7800 bars)
                 │   ├─ Load 64 bars for feature init
                 │   └─ Fetch today's bars from Polygon
                 │
                 ├─ ⏰ Wait until 9:28 AM ET
                 │
                 └─ 🚀 Launch Live Trading
                     │
                     ├─ Python WebSocket Bridge (Alpaca IEX)
                     ├─ C++ Trader (release/sentio_cli_latest)
                     │
                     ├─ Trading (9:30 AM - 4:00 PM ET)
                     │   ├─ Process real-time bars
                     │   ├─ Generate signals
                     │   ├─ Execute trades
                     │   └─ Log all decisions
                     │
                     └─ Session End (4:00 PM ET or Ctrl+C)
                         ├─ 📊 Generate HTML Dashboard
                         ├─ 📧 Email to yeogirl@gmail.com
                         │   ├─ Subject: "OnlineTrader Report - YYYY-MM-DD (P&L: +X.XX%)"
                         │   ├─ Body: HTML summary with metrics
                         │   └─ Attachment: Interactive dashboard.html
                         └─ 🎉 Complete!
```

---

## Email Notifications

### Email Content

**Subject**: `OnlineTrader Report - 2025-10-09 (P&L: +0.25%)`

**Body** (HTML formatted):
- **Trading Summary**:
  - Total Trades
  - Win Rate
  - Final Equity
  - Session P&L ($ and %)
- **Attachment**: Full interactive dashboard HTML

**Attachment**: `session_YYYYMMDD_HHMMSS.html`
- Complete equity curve chart
- Trade-by-trade analysis
- Performance metrics
- Open in browser for interactive features

### Gmail Setup

1. Go to https://myaccount.google.com/apppasswords
2. Create app password for "Mail"
3. Copy 16-character password
4. Add to `config.env`:
   ```bash
   GMAIL_USER=yeogirl@gmail.com
   GMAIL_APP_PASSWORD=abcd efgh ijkl mnop
   ```

### Email Sending Logic

**From Code** (`src/cli/live_trade_command.cpp:380-397`):
```cpp
// Send email notification (only for live mode, not mock)
if (!is_mock_mode_) {
    std::cout << "📧 Sending email notification...\n";

    std::string email_cmd = "python3 tools/send_dashboard_email.py "
                           "--dashboard " + dashboard_file + " "
                           "--trades " + trades_file + " "
                           "--recipient yeogirl@gmail.com "
                           "> /dev/null 2>&1";

    int email_result = system(email_cmd.c_str());

    if (email_result == 0) {
        std::cout << "✅ Email sent to yeogirl@gmail.com\n";
    } else {
        std::cout << "⚠️  Email sending failed (check GMAIL_APP_PASSWORD)\n";
    }
}
```

**Note**: Email is sent in **live mode only**, not in mock mode.

---

## Cron Schedule

### Installation

```bash
./tools/install_cron.sh
```

### Cron Entry

```cron
# OnlineTrader Live Trading (9:15 AM ET)
# DST (March-November): Use 13:15 UTC
# Standard Time (November-March): Use 14:15 UTC
15 13 * * 1-5 cd /Volumes/ExternalSSD/Dev/C++/online_trader && /bin/bash tools/cron_launcher.sh >> logs/cron_$(date +\%Y\%m\%d).log 2>&1
```

### Timezone Handling

**Important**: Adjust UTC time based on Daylight Saving Time:

| Period | ET Time | UTC Time | Cron Schedule |
|--------|---------|----------|---------------|
| DST (Mar-Nov) | 9:15 AM | 13:15 | `15 13 * * 1-5` |
| Standard (Nov-Mar) | 9:15 AM | 14:15 | `15 14 * * 1-5` |

**Manual Edit**:
```bash
crontab -e
# Change 13 to 14 (or vice versa) when DST changes
```

### Monitoring Cron

```bash
# View cron logs
tail -f logs/cron_$(date +%Y%m%d).log

# Check if cron job is installed
crontab -l | grep OnlineTrader

# View all cron jobs
crontab -l
```

---

## File Locations

### Binaries
- `build/sentio_cli` - Development build
- `release/sentio_cli_v2.2` - Production release v2.2
- `release/sentio_cli_latest` - Symlink to current release

### Scripts
- `tools/launch_live_trading.sh` - Main launch script (supports release/build selection)
- `tools/cron_launcher.sh` - Cron wrapper (warmup + launch)
- `tools/install_cron.sh` - Cron installation helper
- `tools/send_dashboard_email.py` - Email notification script
- `tools/comprehensive_warmup.sh` - Warmup data collector

### Data
- `data/dashboards/session_YYYYMMDD_HHMMSS.html` - Auto-generated dashboards
- `logs/live_trading/*.jsonl` - Trading logs (signals, trades, positions, decisions)
- `logs/cron_YYYYMMDD.log` - Cron execution logs
- `logs/eod_state.txt` - EOD liquidation tracking

### Configuration
- `config.env` - API credentials (Alpaca, Polygon, Gmail)
- `data/equities/SPY_warmup_latest.csv` - Pre-loaded warmup data (20 blocks)

---

## Troubleshooting

### Email Not Sending

**Symptom**: "⚠️  Email sending failed (check GMAIL_APP_PASSWORD)"

**Solution**:
1. Check `config.env` has `GMAIL_APP_PASSWORD`
2. Test email manually:
   ```bash
   export GMAIL_APP_PASSWORD='your-password'
   python3 tools/send_dashboard_email.py \
       --dashboard data/dashboards/session_20251009_162312.html \
       --trades logs/live_trading/trades_20251009_162312.jsonl \
       --recipient yeogirl@gmail.com
   ```
3. Verify Gmail app password is correct (16 characters, spaces optional)

### Cron Job Not Running

**Check**:
```bash
# Verify cron job is installed
crontab -l | grep OnlineTrader

# Check system cron logs (macOS)
log show --predicate 'process == "cron"' --last 1h

# Check our cron logs
ls -lht logs/cron_*.log | head -5
tail -50 logs/cron_$(date +%Y%m%d).log
```

**Common Issues**:
1. **Wrong timezone**: Adjust UTC hour in crontab for DST/Standard time
2. **Path issues**: Cron uses limited PATH, ensure full paths in scripts
3. **Permissions**: Ensure scripts are executable (`chmod +x`)
4. **Weekend run**: Cron launcher exits silently on weekends

### Release Binary Not Found

**Symptom**: "❌ ERROR: Binary not found: release/sentio_cli_latest"

**Solution**:
```bash
# Create release
cp build/sentio_cli release/sentio_cli_v2.2
ln -sf sentio_cli_v2.2 release/sentio_cli_latest

# Verify
ls -lh release/
```

### Dashboard Not Generated

**Check**:
```bash
# Verify Python script exists
ls -l tools/professional_trading_dashboard.py

# Check recent dashboards
ls -lht data/dashboards/ | head -10

# Test manual generation
python3 tools/professional_trading_dashboard.py \
    --tradebook logs/live_trading/trades_YYYYMMDD_HHMMSS.jsonl \
    --signals logs/live_trading/signals_YYYYMMDD_HHMMSS.jsonl \
    --output /tmp/test_dashboard.html \
    --start-equity 100000
```

---

## Daily Workflow (Automated)

### Morning (9:15 AM ET)
1. ✅ Cron job triggers automatically
2. ✅ Warmup runs (20 blocks + today's bars)
3. ✅ Trading launches at 9:30 AM ET

### During Market Hours (9:30 AM - 4:00 PM ET)
1. ✅ Real-time bars processed
2. ✅ Signals generated every minute
3. ✅ Trades executed via Alpaca Paper
4. ✅ All decisions logged to JSONL files

### End of Day (4:00 PM ET)
1. ✅ Positions liquidated at 3:58 PM ET
2. ✅ Final equity calculated
3. ✅ Dashboard HTML generated automatically
4. ✅ Email sent to yeogirl@gmail.com with:
   - P&L summary
   - Win rate
   - Final equity
   - Attached dashboard HTML

### While Traveling
1. 📧 Check email for daily report
2. 📊 Download and open dashboard HTML attachment
3. 📈 Review equity curve, trades, and decisions
4. 🎯 Adjust parameters if needed (remote SSH or wait until return)

---

## Advanced Usage

### Manual Launch (Local Testing)

```bash
# Use release binary
./tools/launch_live_trading.sh release

# Use build binary (for testing new features)
./tools/launch_live_trading.sh build

# Custom times (midday optimization, EOD close)
./tools/launch_live_trading.sh release 14:30 15:58
```

### Stopping Trading Session

```bash
# Graceful stop (Ctrl+C)
# - Triggers dashboard generation
# - Sends email notification
# - Closes all positions

# Find PID
ps aux | grep sentio_cli | grep live-trade

# Send SIGTERM (graceful)
kill -TERM <PID>

# Emergency kill (not recommended, skips dashboard/email)
pkill -9 sentio_cli
```

### Viewing Logs

```bash
# Today's cron log
tail -f logs/cron_$(date +%Y%m%d).log

# Latest trading session logs
ls -lt logs/live_trading/*.jsonl | head -20

# View decisions in real-time
tail -f logs/live_trading/decisions_$(date +%Y%m%d)_*.jsonl
```

---

## Production Checklist

Before enabling automated trading:

- [ ] **Credentials configured** in `config.env`:
  - [ ] `ALPACA_PAPER_API_KEY`
  - [ ] `ALPACA_PAPER_SECRET_KEY`
  - [ ] `POLYGON_API_KEY`
  - [ ] `GMAIL_APP_PASSWORD`
  - [ ] `GMAIL_USER=yeogirl@gmail.com`

- [ ] **Release binary created**:
  ```bash
  ls -lh release/sentio_cli_latest
  ```

- [ ] **Scripts executable**:
  ```bash
  chmod +x tools/*.sh tools/*.py
  ```

- [ ] **Warmup data available**:
  ```bash
  ls -lh data/equities/SPY_warmup_latest.csv
  ```

- [ ] **Manual test successful**:
  ```bash
  bash tools/cron_launcher.sh
  ```

- [ ] **Email test successful**:
  ```bash
  python3 tools/send_dashboard_email.py --dashboard <path> --trades <path> --recipient yeogirl@gmail.com
  ```

- [ ] **Cron installed and verified**:
  ```bash
  crontab -l | grep OnlineTrader
  ```

- [ ] **Timezone correct for DST/Standard time**

---

## Support

**Documentation**:
- `LIVE_TRADING_GUIDE.md` - General live trading guide
- `PRICE_FALLBACK_BUG_FIX.md` - Recent bug fixes
- `EOD_BUG_FIX_CHECKLIST.md` - EOD liquidation details

**Logs**:
- `logs/cron_YYYYMMDD.log` - Cron execution logs
- `logs/live_trading/*.jsonl` - Trading session logs
- `data/dashboards/*.html` - Generated dashboards

**Email**:
- Daily reports sent to: yeogirl@gmail.com
- Check spam folder if not received
- Verify `GMAIL_APP_PASSWORD` in `config.env`

---

**STATUS**: ✅ PRODUCTION READY - Automated trading with email notifications enabled

**Version**: 2.2
**Last Updated**: 2025-10-09
