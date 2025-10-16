# Alpaca-Related Scripts Review

This document lists all scripts that connect to Alpaca or are related to live trading sessions.

## Currently in scripts/ folder

### 1. `scripts/launch_trading.sh` ✅ KEEP
**Purpose**: Unified launcher for both mock and live trading
**Alpaca Connection**: Yes (live mode only)
**What it does**:
- Mock mode: Replays historical data for testing
- Live mode: Connects to Alpaca Paper Trading API via C++ REST client
- Pre-market optimization: Runs 2-phase Optuna (50 trials/phase)
- Midday re-optimization: Optional re-tuning at 2:30 PM ET
- Auto warmup and dashboard generation
**Status**: NEW - This is the ONE script to keep for live trading
**Dependencies**:
- Alpaca API credentials (ALPACA_PAPER_API_KEY, ALPACA_PAPER_SECRET_KEY)
- C++ sentio_cli binary
- run_2phase_optuna.py for optimization

### 2. `scripts/run_2phase_optuna.py` ✅ KEEP
**Purpose**: 2-phase parameter optimization for strategy
**Alpaca Connection**: No (uses local data files)
**What it does**:
- Phase 1: Optimizes buy/sell thresholds, lambda, BB amplification
- Phase 2: Optimizes horizon weights, BB params, regularization
- Saves best params to config/best_params.json
**Status**: Required by launch_trading.sh
**Dependencies**:
- sentio_cli binary
- Historical data CSV files

---

## Currently in tools/ folder

### Live Trading Launchers (REDUNDANT - DELETE)

#### 3. `tools/launch_live_trading.sh` ❌ DELETE
**Purpose**: Old live trading launcher with Python websocket bridge
**Alpaca Connection**: Yes (via Python bridge)
**What it does**:
- Starts Python WebSocket bridge (alpaca_websocket_bridge.py)
- Launches C++ trader that reads from FIFO
- Monitors session until market close
- Generates dashboard
**Status**: REPLACED by scripts/launch_trading.sh
**Why delete**:
- Uses Polygon WebSocket (we're moving to Alpaca REST)
- Requires Python bridge (unnecessary complexity)
- All functionality in new unified script

#### 4. `tools/start_live_trading.sh` ❌ DELETE
**Purpose**: Simple live trading starter
**Alpaca Connection**: Yes (indirectly via sentio_cli)
**What it does**:
- Checks for warmup data
- Updates warmup with today's bars
- Starts sentio_cli live-trade command
**Status**: REPLACED by scripts/launch_trading.sh
**Why delete**:
- No optimization support
- Simpler version of launch_live_trading.sh
- All functionality in new unified script

#### 5. `tools/run_mock_session.sh` ❌ DELETE
**Purpose**: Run mock trading session with auto-dashboard
**Alpaca Connection**: No (mock mode only)
**What it does**:
- Runs sentio_cli in mock mode
- Generates dashboard
- Opens dashboard in browser
**Status**: REPLACED by scripts/launch_trading.sh mock
**Why delete**:
- Functionality integrated into unified script
- scripts/launch_trading.sh mock does the same thing

#### 6. `tools/launch_mock_trading_session.py` ❌ DELETE
**Purpose**: Python-based mock trading launcher
**Alpaca Connection**: No (mock mode only)
**What it does**:
- Launches C++ trader in mock mode
- Monitors session
- Generates reports
**Status**: REPLACED by scripts/launch_trading.sh mock
**Why delete**:
- Python version when we have shell version
- Functionality in unified script

---

### Alpaca/Polygon WebSocket Bridge (NO LONGER NEEDED)

#### 7. `tools/alpaca_websocket_bridge.py` ❌ DELETE
**Purpose**: Python bridge between Polygon WebSocket and C++ trader
**Alpaca Connection**: No (connects to Polygon, not Alpaca)
**What it does**:
- Connects to Polygon WebSocket for real-time bars
- Writes bars to FIFO pipe
- C++ trader reads from FIFO
**Status**: NO LONGER NEEDED
**Why delete**:
- We're moving to Alpaca REST API (no WebSocket needed)
- C++ will poll Alpaca directly (no Python bridge)
- Adds complexity and potential failure points

#### 8. `tools/test_python_cpp_bridge.sh` ❌ DELETE
**Purpose**: Tests Python-C++ FIFO bridge connection
**Alpaca Connection**: No
**What it does**:
- Tests if Python bridge and C++ trader can communicate
- Validates FIFO pipe setup
**Status**: NO LONGER NEEDED
**Why delete**:
- Python bridge being removed
- No longer relevant

#### 9. `tools/test_live_connection.sh` ❌ DELETE
**Purpose**: Tests connection to Alpaca and Polygon APIs
**Alpaca Connection**: Yes
**What it does**:
- Checks Alpaca account status
- Tests Polygon WebSocket connection
- Validates credentials
**Status**: NO LONGER NEEDED
**Why delete**:
- We can test Alpaca connection directly from C++
- Polygon WebSocket being removed
- check_alpaca_status.py does similar thing

---

### Testing Scripts

#### 10. `tools/test_live_trading_mock.sh` ❌ DELETE
**Purpose**: Old mock trading test script
**Alpaca Connection**: No
**What it does**:
- Tests mock trading functionality
- Older version of run_mock_session.sh
**Status**: REPLACED by scripts/launch_trading.sh mock
**Why delete**:
- Redundant with run_mock_session.sh
- Both replaced by unified script

---

### Automation/Scheduling Scripts

#### 11. `tools/midday_optuna_relaunch.sh` ❌ DELETE
**Purpose**: Midday optimization and trader restart
**Alpaca Connection**: Yes (restarts live trader)
**What it does**:
- Runs at 2:30 PM ET
- Stops running trader
- Runs Optuna optimization
- Restarts trader with new params
**Status**: REPLACED by scripts/launch_trading.sh --midday-optimize
**Why delete**:
- Functionality integrated into unified script
- Unified script handles it better (no restart, hot-reload params)

#### 12. `tools/cron_launcher.sh` ⚠️ DECIDE
**Purpose**: Cron job to launch trading at market open
**Alpaca Connection**: Yes (launches live trading)
**What it does**:
- Meant to run via cron at 9:30 AM ET
- Launches live trading session
- Logs output
**Status**: COULD BE UPDATED or DELETE
**Options**:
- UPDATE: Change to call scripts/launch_trading.sh live
- DELETE: User can set up cron manually
**Recommendation**: UPDATE if you use cron, DELETE if you don't

---

### Monitoring/Utility Scripts

#### 13. `tools/monitor_live_trading.sh` ⚠️ DECIDE
**Purpose**: Monitor running live trading session
**Alpaca Connection**: No (just monitors logs)
**What it does**:
- Tails log files
- Shows recent trades
- Displays P&L summary
- Monitors for errors
**Status**: UTILITY - Keep if useful
**Recommendation**:
- KEEP if you want separate monitoring
- DELETE if launch_trading.sh monitoring is enough

#### 14. `tools/check_alpaca_status.py` ⚠️ DECIDE
**Purpose**: Check Alpaca account status and positions
**Alpaca Connection**: Yes (REST API)
**What it does**:
- Gets account info (buying power, equity, etc.)
- Lists current positions
- Shows open orders
- Checks market hours
**Status**: UTILITY - Standalone diagnostic tool
**Recommendation**:
- KEEP: Useful for checking account without running trader
- DELETE: Can check from Alpaca dashboard

#### 15. `tools/mock_alpaca_server.py` ⚠️ DECIDE
**Purpose**: Mock Alpaca server for testing
**Alpaca Connection**: No (mocks it)
**What it does**:
- Simulates Alpaca REST API responses
- Useful for testing without real API calls
- Development/testing tool
**Status**: TESTING UTILITY
**Recommendation**:
- KEEP: Useful for development/testing
- DELETE: If you don't do offline testing

---

### Warmup Scripts (KEEP - General Utility)

#### 16. `tools/warmup_live_trading.sh` ✅ KEEP
**Purpose**: Standalone warmup script
**Alpaca Connection**: Yes (fetches today's bars via Alpaca)
**What it does**:
- Fetches historical data from Alpaca
- Creates warmup CSV file
- Used by comprehensive_warmup.sh
**Status**: GENERAL UTILITY
**Why keep**:
- Used by comprehensive_warmup.sh
- Can be used standalone
- General data fetching utility

#### 17. `tools/comprehensive_warmup.sh` ✅ KEEP
**Purpose**: Complete warmup data preparation
**Alpaca Connection**: Yes (via warmup_live_trading.sh)
**What it does**:
- Fetches 20 blocks of historical data
- Gets today's pre-market bars
- Creates SPY_warmup_latest.csv
**Status**: GENERAL UTILITY - Used by launch_trading.sh
**Why keep**:
- Required by scripts/launch_trading.sh
- General data preparation tool

---

### Other Potentially Related Scripts

#### 18. `tools/run_actual_replay_test.sh` (if exists)
**Purpose**: Replay actual trading session
**Alpaca Connection**: Depends on implementation
**Status**: CHECK IF EXISTS
**Recommendation**: Review if found

#### 19. `scripts/*.py` (other scripts)
**Purpose**: Various utilities
**Status**: Review individually if they connect to Alpaca

---

## Summary Recommendations

### DEFINITELY DELETE (9 scripts):
1. ❌ `tools/launch_live_trading.sh` - Replaced
2. ❌ `tools/start_live_trading.sh` - Replaced
3. ❌ `tools/run_mock_session.sh` - Replaced
4. ❌ `tools/launch_mock_trading_session.py` - Replaced
5. ❌ `tools/alpaca_websocket_bridge.py` - No longer needed (moving to REST)
6. ❌ `tools/test_python_cpp_bridge.sh` - No longer needed
7. ❌ `tools/test_live_connection.sh` - No longer needed
8. ❌ `tools/test_live_trading_mock.sh` - Redundant
9. ❌ `tools/midday_optuna_relaunch.sh` - Replaced

### YOUR DECISION NEEDED (4 scripts):
1. ⚠️ `tools/cron_launcher.sh` - Update to use new script OR delete?
2. ⚠️ `tools/monitor_live_trading.sh` - Keep for monitoring OR delete?
3. ⚠️ `tools/check_alpaca_status.py` - Keep utility OR delete?
4. ⚠️ `tools/mock_alpaca_server.py` - Keep for testing OR delete?

### KEEP (2 scripts in scripts/):
1. ✅ `scripts/launch_trading.sh` - Main unified launcher
2. ✅ `scripts/run_2phase_optuna.py` - Optimization engine

### KEEP (2 warmup scripts in tools/):
1. ✅ `tools/warmup_live_trading.sh` - Warmup utility
2. ✅ `tools/comprehensive_warmup.sh` - Complete warmup

---

## Next Steps

Please review and let me know:

1. **Delete the 9 redundant scripts?** (Yes/No)

2. **For the 4 "DECIDE" scripts, what to do with each:**
   - `cron_launcher.sh`: Update / Delete
   - `monitor_live_trading.sh`: Keep / Delete
   - `check_alpaca_status.py`: Keep / Delete
   - `mock_alpaca_server.py`: Keep / Delete

3. **Any other scripts to review?**

Once you decide, I'll execute the deletions and any updates needed.
