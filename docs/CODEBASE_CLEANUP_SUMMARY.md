# Codebase Cleanup Summary

## Overview
Cleaned up legacy scripts and ensured all live Alpaca trading code is properly organized in `scripts/`.

## Removed Legacy Scripts (from tools/)

The following scripts were removed as their functionality has been replaced by `scripts/launch_trading.sh`:

1. **tools/cron_launcher.sh** - Old launcher wrapper (replaced by launchd calling scripts/launch_trading.sh directly)
2. **tools/install_cron.sh** - Cron installation (replaced by tools/install_launchd.sh)
3. **tools/launch_live_trading.sh** - Old live trading launcher (replaced by scripts/launch_trading.sh)
4. **tools/start_live_trading.sh** - Old live trading starter (replaced by scripts/launch_trading.sh)
5. **tools/warmup_live_trading.sh** - Old warmup script (replaced by scripts/comprehensive_warmup.sh)
6. **tools/update_warmup_incremental.sh** - Old incremental warmup (replaced by scripts/comprehensive_warmup.sh)

## Current Architecture: Alpaca/Polygon Connections

### Live Trading Scripts (scripts/)
All scripts that connect to Alpaca/Polygon for **live trading** are in `scripts/`:

- **scripts/alpaca_websocket_bridge.py** - WebSocket bridge for live market data
- **scripts/comprehensive_warmup.sh** - Downloads historical bars via Alpaca REST API
- **scripts/launch_trading.sh** - Main launcher that orchestrates live trading
- **scripts/monitor_trading.sh** - Monitors live trading sessions

### Utility Scripts (tools/)
Scripts in `tools/` are **utilities and testing only**, not for launching live trading:

- **tools/check_alpaca_status.py** - Check account status (READ-ONLY utility)
- **tools/data_downloader.py** - Download historical data from Polygon (READ-ONLY utility)
- **tools/test_live_connection.sh** - Test Alpaca connection (READ-ONLY test)
- **tools/test_python_cpp_bridge.sh** - Test bridge integration (uses scripts/alpaca_websocket_bridge.py)
- **tools/mock_alpaca_server.py** - Local mock server for testing (NO live connection)

### Optimization Scripts (tools/)
Optuna optimization scripts (do not connect to Alpaca):

- tools/adaptive_optuna.py
- tools/run_optuna_*.sh
- tools/optuna_*.py

### Mock/Replay Scripts (tools/)
Mock trading and replay scripts (do not connect to live Alpaca):

- tools/launch_mock_trading_session.py
- tools/replay_yesterday_session.py
- tools/run_mock_session.sh

## Fixes Applied

1. **Removed hardcoded API keys** from `tools/check_alpaca_status.py`
   - Now loads from environment variables (config.env)

2. **Fixed path** in `tools/test_python_cpp_bridge.sh`
   - Changed from `tools/alpaca_websocket_bridge.py` → `scripts/alpaca_websocket_bridge.py`

3. **Updated launchd setup** to call `scripts/launch_trading.sh live` directly
   - No intermediate wrapper needed

## Auto-Start Configuration

**launchd** (recommended for macOS):
- Job definition: `tools/com.onlinetrader.autostart.plist`
- Calls: `scripts/launch_trading.sh live`
- Schedule: Monday-Friday 9:15 AM ET
- Install: `./tools/install_launchd.sh`

## Verification

All Alpaca/Polygon connections outside `scripts/`:
```bash
# Search for Alpaca connections
grep -r "alpaca\|polygon" tools/ --include="*.py" --include="*.sh" -i

# Result: Only utilities/tests, no live trading launchers ✓
```

## Summary

✅ **All live trading code is in scripts/**
✅ **tools/ contains only utilities, tests, and optimization scripts**
✅ **No hardcoded API keys**
✅ **launchd directly calls scripts/launch_trading.sh**
✅ **Legacy scripts removed**

---

**Date:** 2025-10-09
**Status:** Complete
