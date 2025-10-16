# Unified Launch System - Complete Implementation

## Overview

Single unified launch script that handles:
- ✅ Mock trading (replay historical data)
- ✅ Live trading (Alpaca paper trading)
- ✅ Pre-market 2-phase Optuna optimization (50 trials/phase)
- ✅ Midday re-optimization (optional)
- ✅ Auto warmup and dashboard generation

## Main Script

**Location**: `tools/launch_trading.sh`

This is the ONLY script you need for all trading sessions.

## Usage

### Mock Trading (Testing)

```bash
# Quick mock session with instant replay
./tools/launch_trading.sh mock

# Mock with custom data file
./tools/launch_trading.sh mock --data data/equities/SPY_4blocks.csv

# Mock with real-time replay (1x speed)
./tools/launch_trading.sh mock --data /tmp/SPY_yesterday.csv --speed 1
```

### Live Trading (Paper Account)

```bash
# Full workflow: optimize → warmup → trade
./tools/launch_trading.sh live

# Skip pre-market optimization, use existing params
./tools/launch_trading.sh live --skip-optimize

# Enable midday re-optimization at 2:30 PM
./tools/launch_trading.sh live --midday-optimize

# Custom midday time (e.g., 12:00 PM)
./tools/launch_trading.sh live --midday-optimize --midday-time 12:00

# More optimization trials (100 per phase)
./tools/launch_trading.sh live --optimize --trials 100
```

## Workflow

### Live Trading Workflow

```
7:00-9:00 AM ET → Pre-Market Optimization (if --optimize)
                   ├─ Phase 1: Primary params (50 trials)
                   └─ Phase 2: Secondary params (50 trials)

9:00-9:30 AM ET → Strategy Warmup
                   └─ Fetch 20 blocks + today's bars

9:30 AM ET      → Live Trading Starts
                   └─ Uses Alpaca REST API for quotes

2:30 PM ET      → Midday Re-Optimization (if --midday-optimize)
(optional)         ├─ Pause trading
                   ├─ Run quick optimization (25 trials/phase)
                   ├─ Deploy new params
                   └─ Resume trading

3:58 PM ET      → EOD Position Close
                   └─ Liquidate all positions

4:00 PM ET      → Market Close
                   ├─ Generate dashboard
                   └─ Show summary
```

### Mock Trading Workflow

```
Start           → Load historical data

Replay          → Execute trades in mock mode
                   └─ Speed: 0=instant, 1=real-time

End             → Generate dashboard
                   ├─ Save to data/dashboards/
                   └─ Auto-open in browser
```

## 2-Phase Optuna Optimization

### Phase 1: Primary Parameters (Wide Search)
- `buy_threshold`: 0.50 - 0.65 (step 0.01)
- `sell_threshold`: 0.35 - 0.50 (step 0.01)
- `ewrls_lambda`: 0.985 - 0.999 (step 0.001)
- `bb_amplification_factor`: 0.00 - 0.20 (step 0.01)

### Phase 2: Secondary Parameters (Fine-Tuning)
- `h1_weight`, `h5_weight`, `h10_weight`: Multi-horizon weights (sum=1.0)
- `bb_period`: 5 - 40 (step 5)
- `bb_std_dev`: 1.0 - 3.0 (step 0.25)
- `bb_proximity`: 0.10 - 0.50 (step 0.05)
- `regularization`: 0.0 - 0.10 (step 0.005)

**Output**: Best parameters saved to `config/best_params.json`

## Parameter Flow

```
Pre-market Optimization
  ↓
config/best_params.json ← Best parameters from Optuna
  ↓
data/tmp/midday_selected_params.json ← Copy for live trader
  ↓
Live Trader reads params on startup
  ↓
(Optional) Midday optimization updates params
  ↓
Live Trader hot-reloads new params
```

## Scripts Organization

### Keep These:
- ✅ `tools/launch_trading.sh` - **MAIN UNIFIED SCRIPT**
- ✅ `tools/run_2phase_optuna.py` - Optimization engine
- ✅ `tools/comprehensive_warmup.sh` - Warmup data fetcher
- ✅ `tools/professional_trading_dashboard.py` - Dashboard generator

### To Remove (Redundant):
- ❌ `tools/launch_live_trading.sh` - Replaced by unified script
- ❌ `tools/start_live_trading.sh` - Replaced by unified script
- ❌ `tools/run_mock_session.sh` - Replaced by unified script
- ❌ `tools/midday_optuna_relaunch.sh` - Integrated into unified script
- ❌ `tools/test_live_trading_mock.sh` - Use unified script instead

## Data Source Update (To Do)

### Current: Polygon WebSocket via Python Bridge
```cpp
// Old approach (to remove)
bar_feed = std::make_unique<PolygonClientAdapter>(POLYGON_KEY);
```

### New: Alpaca REST API Direct
```cpp
// New approach (to implement)
AlpacaClient alpaca(api_key, secret_key);
auto bars = alpaca.get_latest_bars({"SPY", "SPXL", "SH", "SDS"});
```

**Benefits**:
- ✅ No Python bridge needed
- ✅ Simpler architecture
- ✅ One less dependency
- ✅ Alpaca IEX feed is free and reliable

## Implementation Status

### Completed
- ✅ Unified launch script with mock/live modes
- ✅ 2-phase Optuna optimization script
- ✅ Pre-market optimization integration
- ✅ Midday re-optimization support
- ✅ Alpaca REST API methods added (`get_latest_bars`, `get_bars`)

### To Do
1. **Update live_trade_command.cpp**:
   - Replace Polygon WebSocket with Alpaca REST API polling
   - Remove Python bridge dependency
   - Use `alpaca.get_latest_bars()` every 1 minute

2. **Test Complete Workflow**:
   ```bash
   # Test mock mode
   ./tools/launch_trading.sh mock --data data/equities/SPY_4blocks.csv

   # Test live mode with optimization
   ./tools/launch_trading.sh live --optimize --trials 10
   ```

3. **Clean Up Old Scripts**:
   ```bash
   rm tools/launch_live_trading.sh
   rm tools/start_live_trading.sh
   rm tools/run_mock_session.sh
   rm tools/midday_optuna_relaunch.sh
   rm tools/test_live_trading_mock.sh
   ```

4. **Update Documentation**:
   - Update README with new unified script
   - Add examples for common workflows

## Examples

### Typical Morning Workflow (Live Trading)

```bash
# Before market open (7:00 AM)
./tools/launch_trading.sh live --midday-optimize

# Script will:
# 1. Run 2-phase Optuna (7:00-8:00 AM)
# 2. Wait for market open (9:30 AM)
# 3. Run warmup
# 4. Start live trading
# 5. Re-optimize at 2:30 PM
# 6. Close positions at 3:58 PM
# 7. Generate dashboard at 4:00 PM
```

### Quick Mock Test

```bash
# Test strategy on yesterday's data (instant replay)
./tools/launch_trading.sh mock

# Dashboard opens automatically in browser
```

### Weekend Backtesting

```bash
# Test with optimization on 4-block sample
./tools/launch_trading.sh mock --data data/equities/SPY_4blocks.csv

# Shows P&L, trades, and strategy performance
```

## Configuration

All settings in `config/best_params.json`:

```json
{
  "last_updated": "2025-10-09T07:30:00Z",
  "optimization_source": "2phase_optuna_premarket",
  "best_mrb": 0.046,
  "parameters": {
    "buy_threshold": 0.55,
    "sell_threshold": 0.45,
    "ewrls_lambda": 0.995,
    "bb_amplification_factor": 0.10,
    "h1_weight": 0.3,
    "h5_weight": 0.5,
    "h10_weight": 0.2,
    "bb_period": 20,
    "bb_std_dev": 2.0,
    "bb_proximity": 0.30,
    "regularization": 0.01
  }
}
```

## Next Steps

1. Complete Alpaca REST API integration in C++
2. Remove Polygon WebSocket and Python bridge
3. Test end-to-end workflow
4. Remove redundant scripts
5. Update main README

---

**Author**: Claude Code
**Date**: 2025-10-09
**Version**: 2.1.0
