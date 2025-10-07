# Migration Summary: /tmp/ → data/tmp/

## Overview
All temporary files, logs, and reports have been migrated from `/tmp/` to `data/tmp/` for better organization and persistence.

## Files Migrated

### Reports & Documentation
- ✅ `/tmp/spy_config_summary.md` → `data/tmp/spy_config_summary.md` (5.3 KB)
- ✅ `/tmp/live_trading_analysis_report.md` → `data/tmp/live_trading_analysis_report.md` (4.8 KB)
- ✅ `/tmp/trading_summary_20251007_100730.txt` → `data/tmp/trading_summary_20251007_100730.txt` (233 B)

### Signal Generation
- ✅ `/tmp/qqq_full_signals.jsonl` → `data/tmp/qqq_full_signals.jsonl` (55 MB)
- ✅ `/tmp/signal_gen.log` → `data/tmp/signal_gen.log` (611 B)

### Scripts Created
- ✅ `data/tmp/start_live_trading.sh` - Start live trading with proper log paths
- ✅ `data/tmp/monitor_trading.sh` - Monitor trading with data/tmp/ paths
- ✅ `data/tmp/README.md` - Complete documentation

## Updated Paths

### Live Trading Log
**Old**: `/tmp/live_trade.log`  
**New**: `data/tmp/live_trade.log`

**Usage:**
```bash
# Start live trading (logs to data/tmp/)
./data/tmp/start_live_trading.sh

# Monitor log
tail -f data/tmp/live_trade.log
```

### Monitoring Script
**Old**: `/tmp/monitor_trading.sh` (hardcoded `/tmp/` paths)  
**New**: `data/tmp/monitor_trading.sh` (uses `data/tmp/` paths)

**Updated variables:**
- `LOG_FILE="data/tmp/live_trade.log"`
- `SUMMARY_FILE="data/tmp/trading_summary_$(date +%Y%m%d_%H%M%S).txt"`

## Directory Structure

```
data/
├── tmp/                              # NEW: Temporary files and logs
│   ├── README.md                     # Documentation
│   ├── start_live_trading.sh         # Startup script
│   ├── monitor_trading.sh            # Monitoring script
│   ├── live_trade.log               # Live trading log (runtime)
│   ├── spy_config_summary.md        # SPY configuration
│   ├── live_trading_analysis_report.md
│   ├── trading_summary_*.txt        # Market close reports
│   ├── qqq_full_signals.jsonl       # Generated signals
│   └── signal_gen.log               # Signal generation log
│
├── equities/                         # Historical market data
│   ├── SPY_RTH_NH.csv               # SPY warmup data
│   └── QQQ_RTH_NH.csv               # QQQ warmup data
│
└── dashboards/                       # Trading visualizations
    └── *.html                        # Performance dashboards
```

## Code Changes

### No C++ changes required
The live trading command writes logs to the `logs/live_trading/` directory (JSONL format).  
The shell redirect `> data/tmp/live_trade.log` captures stdout/stderr for human-readable logs.

### Shell scripts updated
- `data/tmp/start_live_trading.sh` - Uses `data/tmp/live_trade.log`
- `data/tmp/monitor_trading.sh` - Reads from `data/tmp/live_trade.log`

## Benefits of data/tmp/

1. **Persistence**: Files survive system reboots (unlike `/tmp/`)
2. **Organization**: All project data in one directory tree
3. **Version Control**: Can selectively add important files to git
4. **Portability**: Entire `data/` directory can be backed up/transferred
5. **Cleanup**: Easy to archive/delete old temporary files

## Quick Start Commands

### Start Live Trading
```bash
./data/tmp/start_live_trading.sh
```

### Monitor Real-Time
```bash
tail -f data/tmp/live_trade.log
```

### Check Latest Summary
```bash
cat data/tmp/trading_summary_*.txt | tail -50
```

### List All Temp Files
```bash
ls -lh data/tmp/
```

## Cleanup Recommendations

### Keep
- Reports and summaries (*.md, *.txt)
- Recent logs (last 7 days)
- Active scripts (*.sh)

### Archive (compress and move to archive/)
- Large signal files (>50 MB)
- Old logs (>30 days)

### Delete
- Temporary debug outputs
- Duplicate or obsolete files

## Next Steps

1. ✅ All files migrated to `data/tmp/`
2. ✅ Scripts updated with new paths
3. ✅ Documentation created
4. ⏳ Test live trading with new log paths
5. ⏳ Verify monitoring script works at market close
