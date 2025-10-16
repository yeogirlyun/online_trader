# launchd Auto-Start Setup Guide
**For OnlineTrader Automated Trading**

## ✅ What Was Created

Three new files in `tools/`:

1. **`com.onlinetrader.autostart.plist`** - launchd job definition
2. **`install_launchd.sh`** - Installation script
3. **`uninstall_launchd.sh`** - Uninstallation script

---

## 🚀 Quick Start (Installation)

```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./tools/install_launchd.sh
```

This will:
- Copy plist to `~/Library/LaunchAgents/`
- Load the job into launchd
- Schedule Monday-Friday at 9:15 AM ET

---

## ⚡ Advantages Over Cron

| Feature | cron | launchd |
|---------|------|---------|
| **Wake from sleep** | ❌ No | ✅ Yes (with Power Nap) |
| **Persist after reboot** | ✅ Yes | ✅ Yes |
| **Better logging** | ⚠️ Basic | ✅ Separate stdout/stderr |
| **macOS native** | ⚠️ Legacy | ✅ Native |
| **Single-instance protection** | ✅ Yes (in scripts) | ✅ Yes (in scripts) |
| **Run when not logged in** | ⚠️ Limited | ✅ Configurable |

---

## 📋 How It Works

### Schedule
- **Days:** Monday-Friday
- **Time:** 9:15 AM ET (automatically handles DST)
- **Action:** Runs `scripts/launch_trading.sh live`

### Workflow
1. **Check if already running** (single-instance protection)
2. Run 2-phase Optuna optimization (50 trials per phase)
3. Perform comprehensive warmup (20 blocks + today's bars)
4. Launch Python websocket bridge (Alpaca/Polygon data feed)
5. Launch C++ live trader at 9:30 AM
6. Trade until 4:00 PM ET with EOD liquidation at 3:58 PM
7. Generate and email dashboard at end of day

### Logging
Three log files are created:
- `logs/launchd_stdout.log` - Standard output from launchd
- `logs/launchd_stderr.log` - Error output from launchd
- `logs/cron_YYYYMMDD.log` - Daily log from cron_launcher.sh

---

## 🔧 Management Commands

### Check if Installed
```bash
launchctl list | grep onlinetrader
```

**Expected output:**
```
-    0    com.onlinetrader.autostart
```

### View Job Details
```bash
launchctl list com.onlinetrader.autostart
```

### Check Logs
```bash
# View live logs
tail -f logs/launchd_stdout.log
tail -f logs/launchd_stderr.log

# View daily trading logs
tail -f logs/cron_$(date +%Y%m%d).log
```

### Test Manually (Outside Schedule)
```bash
# Run the launcher script directly
./tools/cron_launcher.sh
```

### Disable (Temporarily)
```bash
launchctl unload ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
```

### Re-enable
```bash
launchctl load ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
```

### Uninstall Completely
```bash
./tools/uninstall_launchd.sh
```

---

## 🌙 Enable Wake from Sleep (Important!)

For launchd to wake your Mac from sleep, enable **Power Nap**:

### macOS Ventura and Later
1. Open **System Settings**
2. Go to **Battery** (or **Energy Saver** on desktop)
3. Click **Options** (if on a laptop)
4. Check **☑ Enable Power Nap while plugged into power adapter**
5. Also recommended: **☑ Prevent automatic sleeping when the display is off**

### Alternative: caffeinate
Keep Mac awake during trading hours:
```bash
# Add to cron_launcher.sh to keep awake until 5 PM
caffeinate -t 28800 &  # 8 hours (9 AM to 5 PM)
```

---

## 🔍 Troubleshooting

### Job Not Running at Scheduled Time

**Check 1: Is it loaded?**
```bash
launchctl list | grep onlinetrader
```

**Check 2: View logs**
```bash
cat logs/launchd_stderr.log
```

**Check 3: Verify plist syntax**
```bash
plutil -lint ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
```

**Check 4: Mac was asleep?**
- Enable Power Nap (see above)
- Or keep Mac awake during trading hours

### Permission Errors

If you see permission errors:
```bash
# Fix plist permissions
chmod 644 ~/Library/LaunchAgents/com.onlinetrader.autostart.plist

# Fix script permissions
chmod +x tools/cron_launcher.sh
chmod +x scripts/launch_trading.sh
```

### Job Runs But Trading Doesn't Start

**Check single-instance protection:**
```bash
ps aux | grep sentio_cli | grep -v grep
```

If already running, that's expected behavior (won't start duplicate).

**Check time window:**
The launcher only runs 9:00-9:25 AM ET. Outside this window, it will exit.

**Check API credentials:**
```bash
grep "ALPACA_PAPER" config.env
```

---

## 📊 Verification Steps

### 1. Install and Verify
```bash
./tools/install_launchd.sh
launchctl list | grep onlinetrader
```

### 2. Check Job is Scheduled
```bash
launchctl print gui/$(id -u)/com.onlinetrader.autostart
```

### 3. Manual Test (Outside Time Window)
```bash
# This will exit with time window error, but shows it works
./tools/cron_launcher.sh 2>&1 | head -20
```

### 4. Wait for Next Trading Day
- Monitor logs: `tail -f logs/launchd_stdout.log`
- Should see activity at 9:15 AM ET Monday-Friday

---

## 🔄 Switching from Cron to launchd

If you previously installed cron:

### 1. Remove Cron Job
```bash
crontab -l | grep -v "OnlineTrader" | crontab -
```

### 2. Verify Removal
```bash
crontab -l
```

### 3. Install launchd
```bash
./tools/install_launchd.sh
```

---

## 🎯 Best Practices

### For Development/Testing
- **Don't install** launchd during development
- Start manually: `./scripts/launch_trading.sh live`
- Use mock mode: `./scripts/launch_trading.sh mock`

### For Production (Automated Trading)
- **Install launchd** for auto-start
- Enable Power Nap or disable sleep
- Monitor logs daily until confident
- Set up email notifications (GMAIL_APP_PASSWORD in config.env)

### For Maximum Reliability
1. Install launchd ✅
2. Keep Mac plugged in and awake during trading hours
3. Disable screen saver during trading hours
4. Set "Start up automatically after power failure" in Energy Saver
5. Monitor first few runs manually

---

## 📁 File Locations

```
~/Library/LaunchAgents/com.onlinetrader.autostart.plist  ← Installed job
/Volumes/ExternalSSD/Dev/C++/online_trader/
├── tools/
│   ├── com.onlinetrader.autostart.plist  ← Source template
│   ├── install_launchd.sh                ← Installation script
│   └── uninstall_launchd.sh              ← Uninstallation script
├── scripts/
│   └── launch_trading.sh                 ← Main launcher (called by launchd)
└── logs/
    ├── launchd_stdout.log                ← launchd stdout
    ├── launchd_stderr.log                ← launchd stderr
    └── live_trading/                     ← Live trading logs
```

---

## ⚙️ Advanced Configuration

### Run Even When Not Logged In

Edit the plist and uncomment these lines:
```xml
<key>SessionCreate</key>
<true/>
```

Then reload:
```bash
launchctl unload ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
launchctl load ~/Library/LaunchAgents/com.onlinetrader.autostart.plist
```

### Change Schedule

Edit `~/Library/LaunchAgents/com.onlinetrader.autostart.plist`:
```xml
<key>Hour</key>
<integer>9</integer>  <!-- Change this -->
<key>Minute</key>
<integer>15</integer>  <!-- Change this -->
```

Then reload the job.

### Add Holidays

Edit `scripts/launch_trading.sh` to add holiday checking at the start:
```bash
# Check for holidays
TODAY=$(date +%Y-%m-%d)
if [ "$TODAY" = "2025-12-25" ]; then  # Christmas
    echo "Market holiday - no trading"
    exit 0
fi
```

---

## 🆚 Comparison: cron vs launchd

Both are installed and ready. Choose one:

### Use **launchd** if:
- ✅ You want Mac to wake from sleep
- ✅ You want better logging
- ✅ You prefer macOS-native solution

### Use **cron** if:
- ✅ You're familiar with cron
- ✅ Mac is always awake anyway
- ✅ You prefer simpler setup

**Recommended:** launchd for production trading

---

## 📞 Support

**Verify installation:**
```bash
./tools/install_launchd.sh  # Shows detailed output
```

**Check status:**
```bash
launchctl list | grep onlinetrader
```

**View logs:**
```bash
tail -f logs/launchd_stdout.log
```

**Test manually:**
```bash
./scripts/launch_trading.sh live
```

---

**Status:** launchd setup COMPLETE ✅
**Next Step:** Run `./tools/install_launchd.sh` when ready to enable auto-trading
