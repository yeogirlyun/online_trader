# Incremental Warmup System

## Overview

The live trading system now includes an **incremental warmup update** mechanism that automatically downloads and appends today's completed bars to the warmup file before starting live trading. This ensures the model always has the most recent market context.

## How It Works

### 1. **Initial Warmup** (First-time setup)
```bash
./tools/warmup_live_trading.sh
```
- Downloads ~10 trading days (3900+ bars)
- Creates `data/equities/SPY_warmup_latest.csv`
- Run this once when setting up live trading

### 2. **Incremental Update** (Every startup)
```bash
./tools/update_warmup_incremental.sh
```
- Checks last timestamp in warmup file
- Downloads today's bars from Polygon
- Appends only new bars to warmup file
- Skips if warmup is current (<2 minutes old)

### 3. **Automated Startup** (Recommended)
```bash
./tools/start_live_trading.sh
```
- **Checks** if initial warmup exists
- **Updates** warmup with today's bars
- **Displays** warmup summary
- **Starts** live trading

This is the recommended way to start live trading!

## Benefits

### Before (Manual Warmup)
- Warmup data ends at yesterday's 4:00 PM close
- Model starts "cold" without today's market context
- Miss first 1-2 hours of trading patterns
- Strategy needs to learn today's regime from scratch

### After (Incremental Warmup)
- Warmup data includes today's completed bars up to current time
- Model starts "warm" with latest market context
- Already trained on today's market movement
- Strategy is pre-adapted to today's regime
- Smoother transition from warmup to live trading

## Example Workflow

### Scenario: Starting live trading at 11:30 AM ET

**Without incremental update:**
```
Last warmup bar: 2025-10-07 16:00:00 (yesterday close)
Gap: 19.5 hours
Missing: Today's 9:30-11:30 movement (120 bars)
```

**With incremental update:**
```bash
./tools/start_live_trading.sh
```

Output:
```
=== Incremental Warmup Update ===
Current time (ET): 2025-10-08 11:30:00
Last warmup bar: 2025-10-07 16:00:00
Downloading today's bars...
✓ Update complete!
  Added 120 new bars
  Total bars: 4485
  Latest bar: 2025-10-08T11:29:00-04:00

=== Ready for Live Trading ===
Warmup file: 4485 bars
Estimated warmup: ~11.5 trading blocks
Status: ✓ Sufficient for full warmup
```

Now the strategy starts with today's market context already learned!

## Technical Details

### Incremental Update Logic

1. **Check market status**
   - If before 9:30 AM ET → Skip (market hasn't opened)
   - If after 9:30 AM ET → Continue with update

2. **Read last timestamp**
   ```bash
   LAST_BAR=$(tail -1 SPY_warmup_latest.csv | cut -d',' -f1)
   ```

3. **Download today's data**
   - Fetch all bars for today from Polygon
   - Filter bars with timestamp > last timestamp
   - Append to warmup file

4. **Safety checks**
   - Skip if last bar is <2 minutes old (already current)
   - Create backup before appending
   - Only append bars newer than existing data

### File Structure

**Before update:**
```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2025-09-23T09:30:00-04:00,1758634200,666.72,666.83,666.61,666.76,586345.0
...
2025-10-07T16:00:00-04:00,1759932000,671.5,671.6,671.4,671.52,450123.0
```

**After update (11:30 AM):**
```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2025-09-23T09:30:00-04:00,1758634200,666.72,666.83,666.61,666.76,586345.0
...
2025-10-07T16:00:00-04:00,1759932000,671.5,671.6,671.4,671.52,450123.0
2025-10-08T09:30:00-04:00,1759932600,671.73,671.84,671.73,671.84,802.0        ← NEW
2025-10-08T09:31:00-04:00,1759932660,671.91,671.94,671.81,671.81,3301.0       ← NEW
...
2025-10-08T11:29:00-04:00,1759940940,672.1,672.3,672.05,672.25,5234.0         ← NEW
```

## Usage Examples

### Quick Start (Recommended)
```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./tools/start_live_trading.sh
```

### Manual Control
```bash
# 1. Initial setup (first time only)
./tools/warmup_live_trading.sh

# 2. Update with today's bars (every time before trading)
./tools/update_warmup_incremental.sh

# 3. Start live trading
cd build && source ../config.env && ./sentio_cli live-trade
```

### Check Warmup Status
```bash
wc -l data/equities/SPY_warmup_latest.csv
tail -3 data/equities/SPY_warmup_latest.csv
```

## Performance Impact

### Warmup Time
- **Initial warmup**: ~10 blocks (3900 bars) = ~15-20 seconds
- **Incremental download**: ~120 bars = ~2-3 seconds
- **Total startup**: ~18-23 seconds (vs ~60 seconds for cold start)

### Model Quality
- ✓ Strategy sees today's market regime
- ✓ Indicators pre-seeded with current volatility
- ✓ EWRLS predictor trained on latest patterns
- ✓ Adaptive thresholds calibrated to current conditions

## Maintenance

### Daily Routine
1. Market opens at 9:30 AM ET
2. Run `./tools/start_live_trading.sh`
3. System automatically updates warmup with today's bars
4. Live trading starts with current market context

### Weekly Routine
None! The incremental update handles everything automatically.

### Manual Refresh (Optional)
If you want to completely refresh the warmup data:
```bash
# This downloads fresh 10 days of data
./tools/warmup_live_trading.sh
```

## Troubleshooting

### "No update needed" message
```
✓ Warmup file is current (last bar from 1 minutes ago)
  No update needed
```
**This is normal!** The system detected warmup is already current.

### "Failed to download incremental data"
```
❌ Failed to download incremental data
```
**Check:**
1. POLYGON_API_KEY is set in config.env
2. API key is valid and has remaining quota
3. Network connection is working
4. Market is open (not before 9:30 AM or after 4:00 PM)

### Backup and Recovery
The system automatically creates backups:
```bash
ls -lh data/equities/SPY_warmup_latest.csv*
-rw-r--r--  1 user  staff  320K Oct 8 10:34 SPY_warmup_latest.csv
-rw-r--r--  1 user  staff  312K Oct 8 10:30 SPY_warmup_latest.csv.bak
```

To restore backup:
```bash
mv data/equities/SPY_warmup_latest.csv.bak data/equities/SPY_warmup_latest.csv
```

## Summary

The incremental warmup system provides:
- ✅ **Automatic** - No manual intervention needed
- ✅ **Fast** - Only downloads new bars (<3 seconds)
- ✅ **Smart** - Skips update if already current
- ✅ **Safe** - Creates backups before updating
- ✅ **Seamless** - Integrated into startup workflow

**Result**: Live trading starts with maximum market context, leading to better strategy performance and smoother operation!

---

Generated by Claude Code - 2025-10-08
