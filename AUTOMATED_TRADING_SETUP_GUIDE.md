# Automated Daily Trading Setup Guide

## Overview

This guide configures your Mac to automatically:
1. Wake from sleep at 9:00 AM ET (Monday-Friday)
2. Launch the trading system at 9:15 AM ET via launchd
3. Run pre-market optimization, live trading, and send email reports

## Prerequisites

- Mac mini (Apple Silicon) running macOS
- Mac connected to power (recommended for reliability)
- System timezone set to America/New_York (ET)
- All API keys configured in `config.env`:
  - `ALPACA_PAPER_API_KEY`
  - `ALPACA_PAPER_SECRET_KEY`
  - `POLYGON_API_KEY`
  - `GMAIL_APP_PASSWORD`

## Step 1: Configure Wake Schedule (Requires sudo)

Your Mac needs to wake from sleep before the trading job runs.

```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
sudo ./tools/setup_daily_wake.sh
```

**What this does:**
- Schedules your Mac to wake Monday-Friday at 9:00 AM ET
- Uses `pmset repeat wake MTWRF 09:00:00`
- Gives the system 15 minutes to fully wake before trading starts

**Verify it worked:**
```bash
pmset -g sched
```

You should see:
```
Repeating power events:
 wake at 9:0:0 on MTWRF
```

## Step 2: Install launchd Job (No sudo required)

The launchd job launches the trading system at 9:15 AM ET.

```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./tools/install_launchd.sh
```

**What this does:**
- Copies `com.onlinetrader.autostart.plist` to `~/Library/LaunchAgents/`
- Loads the job to run Monday-Friday at 9:15 AM ET
- Creates log files in `logs/launchd_*.log`

**Verify it worked:**
```bash
launchctl list | grep onlinetrader
```

You should see:
```
-	0	com.onlinetrader.autostart
```

## Step 3: Test the Setup

### Option A: Test Manually (Without Waiting)

```bash
# Test the trading script directly
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./scripts/launch_trading.sh live
```

This will run the full trading session as if the launchd job triggered it.

### Option B: Test Wake Schedule

1. Put your Mac to sleep **before 9:00 AM on a weekday**:
   ```bash
   pmset sleepnow
   ```

2. Your Mac should automatically wake at 9:00 AM ET

3. At 9:15 AM ET, the trading job will start

4. Monitor the logs:
   ```bash
   tail -f logs/launchd_stdout.log
   tail -f logs/live_trading/system_*.log
   ```

## Monitoring

### Check if launchd job is loaded
```bash
launchctl list | grep onlinetrader
```

### View wake schedule
```bash
pmset -g sched
```

### View launchd logs
```bash
tail -f logs/launchd_stdout.log
tail -f logs/launchd_stderr.log
```

### View trading session logs
```bash
# Most recent session
ls -lht logs/live_trading/ | head -10

# Follow live session
tail -f logs/live_trading/system_$(date +%Y%m%d)*.log
```

### View power management settings
```bash
pmset -g
```

## Troubleshooting

### Problem: Mac doesn't wake at 9:00 AM

**Cause:** Wake schedule not configured or Mac unplugged

**Fix:**
```bash
# Re-run wake setup
sudo ./tools/setup_daily_wake.sh

# Verify schedule
pmset -g sched

# Ensure Mac is plugged in (required for wake)
```

### Problem: launchd job doesn't run at 9:15 AM

**Cause:** Job not loaded or plist file corrupted

**Fix:**
```bash
# Reload the job
launchctl unload ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
launchctl load ~/Library/LaunchAgents/com.onlinetrader.autostart.plist

# Or reinstall
./tools/install_launchd.sh
```

### Problem: Trading session fails to start

**Cause:** Missing API keys or binary not built

**Fix:**
```bash
# Check API keys
cat config.env | grep -E "ALPACA|POLYGON|GMAIL"

# Rebuild binary
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j$(sysctl -n hw.ncpu)

# Test manually
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./scripts/launch_trading.sh live
```

### Problem: No email received after session

**Cause:** Missing or incorrect `GMAIL_APP_PASSWORD`

**Fix:**
1. Generate app password: https://myaccount.google.com/apppasswords
2. Add to `config.env`:
   ```bash
   GMAIL_USER=yeogirl@gmail.com
   GMAIL_APP_PASSWORD=your_16_char_app_password_here
   ```
3. Test email:
   ```bash
   python3 scripts/send_dashboard_email.py \
       --dashboard data/dashboards/latest_live.html \
       --trades logs/live_trading/trades_*.jsonl \
       --recipient yeogirl@gmail.com
   ```

## Daily Workflow (Fully Automated)

**6:00 - 9:00 AM ET:** Pre-market hours (Mac can be asleep)
- 9:00 AM: Mac wakes automatically (if asleep)

**9:00 - 9:30 AM ET:** Pre-market optimization
- 9:15 AM: launchd triggers `launch_trading.sh live`
- Downloads latest market data
- Runs 2-phase Optuna optimization (20 trials/phase)
- Prepares warmup data (20 blocks + today's bars)

**9:30 AM - 4:00 PM ET:** Live trading session
- Connects to Alpaca WebSocket
- Executes OnlineEnsemble strategy
- Manages positions across SPY, SPXL, SH, SDS
- 3:58 PM: Liquidates all positions (EOD close)

**4:00 PM+ ET:** Post-market analysis
- Generates professional trading dashboard
- Sends email to yeogirl@gmail.com with results
- Archives logs to `logs/live_trading/`

## Uninstalling

### Remove wake schedule
```bash
sudo pmset repeat cancel
```

### Remove launchd job
```bash
./tools/uninstall_launchd.sh
```

## Advanced Configuration

### Change wake time
```bash
# Wake at 8:30 AM instead of 9:00 AM
sudo pmset repeat wake MTWRF 08:30:00
```

### Change trading start time

Edit `tools/com.onlinetrader.autostart.plist`:
```xml
<key>Hour</key>
<integer>9</integer>    <!-- Change this -->
<key>Minute</key>
<integer>15</integer>   <!-- And this -->
```

Then reload:
```bash
launchctl unload ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
launchctl load ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
```

### Enable midday optimization

Add `--midday-optimize` to the launch script arguments in the plist:
```xml
<key>ProgramArguments</key>
<array>
    <string>/bin/bash</string>
    <string>/Volumes/ExternalSSD/Dev/C++/online_trader/scripts/launch_trading.sh</string>
    <string>live</string>
    <string>--midday-optimize</string>
</array>
```

## Summary Checklist

- [ ] Wake schedule configured: `sudo ./tools/setup_daily_wake.sh`
- [ ] Launchd job installed: `./tools/install_launchd.sh`
- [ ] API keys in `config.env`
- [ ] Binary built: `build/sentio_cli`
- [ ] Tested manually: `./scripts/launch_trading.sh live`
- [ ] Mac connected to power
- [ ] System timezone = America/New_York

## Quick Reference

| Task | Command |
|------|---------|
| Check wake schedule | `pmset -g sched` |
| Check launchd status | `launchctl list \| grep onlinetrader` |
| View launchd logs | `tail -f logs/launchd_stdout.log` |
| View trading logs | `tail -f logs/live_trading/system_*.log` |
| Test manually | `./scripts/launch_trading.sh live` |
| Put Mac to sleep | `pmset sleepnow` |
| Remove wake schedule | `sudo pmset repeat cancel` |
| Uninstall launchd | `./tools/uninstall_launchd.sh` |

---

**Questions?** Check logs first:
- `logs/launchd_stdout.log` - Launch script output
- `logs/launchd_stderr.log` - Launch script errors
- `logs/live_trading/system_*.log` - Trading session details
