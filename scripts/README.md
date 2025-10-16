# Scripts Folder - Live Trading

This folder contains **ONLY** scripts used by `launch_trading.sh` for live/mock trading sessions.

**All other utilities and tools belong in the `tools/` folder.**

---

## Core Principles

### 1. No Fallback - Crash Fast
- If something fails, **fail immediately** with clear error message
- Never fall back to defaults silently
- Never continue with degraded functionality
- User must fix the root cause

### 2. No Duplicate Code
- **ONE script per task** - no versions, no variants
- Never create: `script_v2.py`, `script_enhanced.sh`, `script_improved.py`
- Never create: `script_old.py`, `script_backup.sh`, `script_new.py`
- Improvements go **directly into the original script**
- Keep the original script name forever

### 3. Direct Modification Only
- To improve a script: **edit it directly**
- Delete old code, write new code in the same file
- No "enhanced" or "advanced" versions
- No parallel implementations

### 4. New Scripts Only for New Tasks
- Create new script **only if** it's a completely different task
- Not for: improvements, enhancements, alternatives
- Only for: genuinely new functionality

**Example**:
- ✅ GOOD: Edit `launch_trading.sh` to add feature
- ❌ BAD: Create `launch_trading_enhanced.sh`
- ❌ BAD: Create `launch_trading_v2.sh`
- ❌ BAD: Keep both old and new versions

---

## Core Scripts (4 files)

### 1. `launch_trading.sh` ✅
**Main unified launcher for all trading sessions**

**Features**:
- Mock mode: Replay historical data for testing
- Live mode: Paper trading with Alpaca REST API
- Pre-market 2-phase Optuna optimization (50 trials/phase)
- Midday re-optimization (optional)
- Auto warmup and dashboard generation

**Usage**:

```bash
# Mock trading (testing)
./scripts/launch_trading.sh mock
./scripts/launch_trading.sh mock --data data/equities/SPY_4blocks.csv

# Live trading (full workflow)
./scripts/launch_trading.sh live

# Live with midday optimization
./scripts/launch_trading.sh live --midday-optimize

# Skip pre-market optimization
./scripts/launch_trading.sh live --skip-optimize

# Custom optimization trials
./scripts/launch_trading.sh live --optimize --trials 100
```

**What It Does** (see "Complete Workflow" section for full details):
- Pre-market 2-phase optimization (7:00-9:00 AM)
- Strategy warmup with predictor training (9:00-9:30 AM)
- Position reconciliation at startup (if positions exist)
- Live trading with Alpaca REST API direct connection
- Full logging (signals, decisions, trades, positions)
- Midday re-optimization at 12:45 PM (optional)
- EOD position close at 3:58 PM
- Dashboard generation and email report at 4:00 PM

**Dependencies**:
- `run_2phase_optuna.py`
- `comprehensive_warmup.sh` (live mode only)
- `professional_trading_dashboard.py`
- `send_dashboard_email.py` (in ../tools/)

---

### 2. `run_2phase_optuna.py` ✅
**2-phase parameter optimization engine**

**What it does**:
- Phase 1: Optimizes buy/sell thresholds, lambda, BB amplification (50 trials)
- Phase 2: Optimizes horizon weights, BB params, regularization (50 trials)
- Uses Phase 1 best params as base for Phase 2
- Saves results to `config/best_params.json`

**Usage**:

```bash
# Standard optimization
python3 scripts/run_2phase_optuna.py \
  --data data/equities/SPY_4blocks.csv \
  --output config/best_params.json

# More trials for better results
python3 scripts/run_2phase_optuna.py \
  --data data/equities/SPY_warmup_latest.csv \
  --output config/best_params.json \
  --n-trials-phase1 100 \
  --n-trials-phase2 100

# Parallel optimization
python3 scripts/run_2phase_optuna.py \
  --data data/equities/SPY_4blocks.csv \
  --output config/best_params.json \
  --n-jobs 4
```

**Output**: `config/best_params.json`

**Called by**: `launch_trading.sh` (when optimization enabled)

---

### 3. `comprehensive_warmup.sh` ✅
**Strategy warmup data fetcher**

**What it collects**:
- 20 trading blocks (7800 bars @ 390 bars/block)
- 64 additional bars for feature engine initialization
- Today's bars if launched after market open
- Only Regular Trading Hours (9:30 AM - 4:00 PM ET)

**Usage**:

```bash
# Fetch warmup data
./scripts/comprehensive_warmup.sh
```

**Output**: `data/equities/SPY_warmup_latest.csv`

**Called by**: `launch_trading.sh` (live mode only, before 9:30 AM and at 12:45 PM for midday update)

**Requirements**:
- Alpaca API credentials in `config.env`
- Internet connection
- Sufficient historical data available from Alpaca

---

### 4. `professional_trading_dashboard.py` ✅
**HTML dashboard generator**

**What it generates**:
- Performance charts (equity curve, drawdown)
- Trade analysis (P&L distribution, win rate)
- Position timeline
- Strategy metrics (Sharpe, MRB, etc.)

**Usage**:

```bash
python3 scripts/professional_trading_dashboard.py \
  --tradebook logs/live_trading/trades_20251009.jsonl \
  --data data/equities/SPY_warmup_latest.csv \
  --output data/dashboards/session.html \
  --start-equity 100000
```

**Output**: HTML dashboard with interactive charts

**Called by**: `launch_trading.sh` (all modes)

**Requirements**:
- pandas
- plotly

---

## Complete Workflow

### Full Day Trading Session

When you run:
```bash
./scripts/launch_trading.sh live --midday-optimize
```

Here's the complete automated workflow:

#### 7:00-9:00 AM ET: Pre-Market Optimization
1. **Phase 1 Optimization** (50 trials)
   - Optimize: buy_threshold, sell_threshold, ewrls_lambda, bb_amplification_factor
   - Uses all available historical data

2. **Phase 2 Optimization** (50 trials)
   - Fix Phase 1 best params
   - Optimize: horizon_weights (h1, h5, h10), bb_period, bb_std_dev, bb_proximity, regularization

3. **Save Best Parameters**
   - Write to: `config/best_params.json`
   - Copy to: `data/tmp/midday_selected_params.json`

#### 9:00-9:30 AM ET: Strategy Warmup
1. **Fetch Historical Data**
   - 20 trading blocks (7800 bars)
   - 64 additional bars for feature engine initialization
   - Today's bars if market already opened (catch-up mode)

2. **Warmup Feature Engine**
   - Initialize all rolling windows (64 bars)
   - Prepare technical indicators
   - Ready for real-time feature extraction

3. **Warmup EWRLS Predictor**
   - Train on historical bar-to-bar returns
   - Initialize multi-horizon predictions (1, 5, 10 bars)
   - Adaptive learning ready

#### 9:30 AM ET: Live Trading Starts
1. **Position Reconciliation**
   - Check existing positions in Alpaca account
   - Map positions to PSM state (CASH_ONLY, SPY_ONLY, SPY_SPXL, SH_ONLY, SH_SDS, etc.)
   - Set correct bars_held_ counter for each position
   - Resume trading from current state (no forced liquidation)

2. **Connect to Alpaca REST API**
   - Direct C++ connection (no Python bridge, no Polygon)
   - Poll `AlpacaClient::get_latest_bars()` every 1 minute
   - Fetch bars for: SPY, SPXL, SH, SDS

3. **Real-Time Trading Loop**
   - Receive bar → Extract features → Generate predictions
   - Apply PSM logic → Generate signals → Execute trades
   - **Full Logging** (every event logged):
     - `signals_*.jsonl` - All signal generations
     - `decisions_*.jsonl` - All PSM decisions
     - `trades_*.jsonl` - All trade executions
     - `positions_*.jsonl` - Position updates
     - `trader_*.log` - Detailed timestamped logs

#### 12:45 PM ET: Midday Re-Optimization (if --midday-optimize enabled)
1. **Stop Trader Cleanly**
   - Send SIGTERM signal to stop trader
   - Wait for clean shutdown

2. **Fetch Morning Bars for Seamless Warmup**
   - Fetch all bars from 9:30 AM - 12:45 PM via Alpaca API
   - Append morning bars to `SPY_warmup_latest.csv`
   - Ensures predictor and feature engine maintain continuous state

3. **Run Quick 2-Phase Optimization** (25 trials/phase for speed)
   - Phase 1: Re-optimize primary parameters
   - Phase 2: Re-optimize secondary parameters
   - Uses updated warmup data (historical + morning bars)
   - Better calibration with morning's trading data

4. **Deploy New Parameters**
   - Save to: `config/best_params.json`
   - Update: `data/tmp/midday_selected_params.json`

5. **Restart Trader Immediately**
   - Launch new `sentio_cli live-trade` process
   - Uses updated warmup file (with morning bars)
   - Seamless continuation: predictor maintains context, feature engine rolling windows intact
   - No fixed 1 PM restart time - resumes as soon as optimization completes
   - Continue trading with optimized settings

#### 3:58 PM ET: End-of-Day Position Close
1. **Liquidate All Positions**
   - Close all SPY, SPXL, SH, SDS positions
   - Use market orders for guaranteed fills
   - Transition to CASH_ONLY state

2. **Stop Accepting New Orders**
   - No new trade signals processed
   - Wait for existing orders to fill
   - Maintain flat position until 4:00 PM

#### 4:00 PM ET: Market Close - Session End
1. **Stop Live Trading**
   - Gracefully terminate sentio_cli
   - Save EOD state: `logs/live_trading/eod_state.txt`
   - Final position snapshot

2. **Generate Dashboard**
   - Read all session logs (signals, trades, decisions, positions)
   - Create interactive HTML dashboard
   - Charts: equity curve, drawdown, P&L distribution
   - Metrics: total return, Sharpe ratio, MRB, win rate, etc.
   - Save to: `data/dashboards/live_session_YYYYMMDD_HHMMSS.html`
   - Symlink: `data/dashboards/latest_live.html`

3. **Send Email Report**
   - Email dashboard HTML to configured address
   - Include session summary in email body:
     - Total P&L, number of trades, win rate
     - Best/worst trades
     - Final positions
   - Attach dashboard file

4. **Session Complete**
   - All logs saved in `logs/live_trading/`
   - Dashboard available for review
   - System ready for next trading day

---

## File Structure

```
scripts/
├── launch_trading.sh                 # Main launcher
├── run_2phase_optuna.py             # Optimization
├── comprehensive_warmup.sh          # Warmup data
├── professional_trading_dashboard.py # Dashboard
└── README.md                         # This file

config/
└── best_params.json                  # Optimized parameters

logs/
├── live_trading/                     # Live session logs
└── mock_trading/                     # Mock session logs

data/
├── dashboards/                       # HTML dashboards
├── equities/
│   └── SPY_warmup_latest.csv        # Warmup data
└── tmp/
    └── midday_selected_params.json  # Current params
```

---

## Configuration

### Required for Live Trading

Create `config.env`:
```bash
ALPACA_PAPER_API_KEY=your_key_here
ALPACA_PAPER_SECRET_KEY=your_secret_here
```

### Optional

`config/best_params.json` - Auto-generated by optimization

---

## Examples

### Morning Launch (Full Automated Day)

```bash
# Before market open (7:00 AM)
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./scripts/launch_trading.sh live --midday-optimize
```

**This single command runs the entire day automatically:**

| Time | Action | Details |
|------|--------|---------|
| 7:00-9:00 AM | Pre-market optimization | Phase 1 + Phase 2 (50 trials each) |
| 9:00-9:30 AM | Strategy warmup | 20 blocks + today's bars, predictor training |
| 9:30 AM | Live trading starts | Position reconciliation, connect to Alpaca |
| 9:30 AM-12:45 PM | Morning session | Real-time trading with full logging |
| 12:45 PM | Midday optimization | Stop trader, fetch morning bars, optimize (25 trials/phase) |
| ~1:00 PM | Restart immediately | Resume with seamless warmup + new params |
| ~1:00-3:58 PM | Afternoon trading | Continue with optimized settings |
| 3:58 PM | EOD liquidation | Close all positions, stop new orders |
| 4:00 PM | Session end | Generate dashboard, send email, stop |

**No manual intervention needed** - fully automated from start to finish.

### Quick Mock Test

```bash
# Test on yesterday's data
./scripts/launch_trading.sh mock

# Test on 4-block sample
./scripts/launch_trading.sh mock --data data/equities/SPY_4blocks.csv
```

### Manual Optimization + Trading

```bash
# Step 1: Optimize separately
python3 scripts/run_2phase_optuna.py \
  --data data/equities/SPY_warmup_latest.csv \
  --output config/best_params.json \
  --n-trials-phase1 100 \
  --n-trials-phase2 100

# Step 2: Trade with optimized params
./scripts/launch_trading.sh live --skip-optimize
```

---

## Logs and Output

### Live Trading Logs: `logs/live_trading/`
- `trader_YYYYMMDD_HHMMSS.log` - Main trader log
- `trades_YYYYMMDD_HHMMSS.jsonl` - Trade records
- `signals_YYYYMMDD_HHMMSS.jsonl` - Signal records
- `decisions_YYYYMMDD_HHMMSS.jsonl` - Decision log
- `positions_YYYYMMDD_HHMMSS.jsonl` - Position updates

### Mock Trading Logs: `logs/mock_trading/`
- Same structure as live trading

### Dashboards: `data/dashboards/`
- `live_session_YYYYMMDD_HHMMSS.html` - Live session
- `mock_session_YYYYMMDD_HHMMSS.html` - Mock session
- `latest_live.html` - Symlink to latest live
- `latest_mock.html` - Symlink to latest mock

---

## Error Handling Philosophy

### Crash Fast - No Fallbacks
All scripts follow the "crash fast" principle:

**What This Means**:
- Missing credentials? → **CRASH** with error message
- Binary not found? → **CRASH** (don't search elsewhere)
- Invalid data? → **CRASH** (don't use old data)
- API call failed? → **CRASH** (don't retry silently)
- Wrong parameters? → **CRASH** (don't use defaults)

**Why**:
- Forces you to fix root cause immediately
- No silent failures that compound later
- No mysterious "it worked yesterday" bugs
- Clear, loud errors are better than silent corruption

**Examples**:
```bash
# GOOD - Crash immediately
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found: $BINARY"
    exit 1
fi

# BAD - Silent fallback
if [ ! -f "$BINARY" ]; then
    BINARY="/usr/local/bin/sentio_cli"  # DON'T DO THIS
fi
```

---

## Troubleshooting

### "Binary not found"
```bash
cd build
cmake --build . -j8
```

### "Missing Alpaca credentials"
```bash
cat > config.env << EOF
ALPACA_PAPER_API_KEY=your_key
ALPACA_PAPER_SECRET_KEY=your_secret
EOF
```

### "No warmup data"
```bash
./scripts/comprehensive_warmup.sh
```

### "Optimization failed"
- Check historical data availability
- Try fewer trials: `--trials 10`
- Ensure enough RAM (8GB+)

**Note**: Scripts will NOT fall back to defaults. Fix the error and re-run.

---

## Related Documentation

- `../LAUNCH_SYSTEM_SUMMARY.md` - System overview
- `../config/best_params.json` - Current parameters
- `../tools/` - General utilities (not used by launch_trading.sh)

---

---

## Development Guidelines

### When Improving Scripts

**DO**:
- ✅ Edit the original script directly
- ✅ Test changes thoroughly
- ✅ Update this README if behavior changes
- ✅ Delete old code completely

**DON'T**:
- ❌ Create `script_v2.py` or `script_new.sh`
- ❌ Keep backup versions (`script.sh.bak`)
- ❌ Add `_enhanced`, `_improved`, `_advanced` suffixes
- ❌ Create parallel implementations
- ❌ Add silent fallbacks or default behaviors

### Version Control
- Git is your backup - no need for manual versions
- Use git history to see old code
- Use git branches for experimental changes
- Commit often, name scripts consistently

### Script Naming Rules
- Use descriptive, permanent names
- Never include version numbers (v1, v2, etc.)
- Never include status words (old, new, tmp, test)
- Never include modifiers (enhanced, improved, fast)

**Examples**:
- ✅ GOOD: `launch_trading.sh`, `run_2phase_optuna.py`
- ❌ BAD: `launch_trading_v2.sh`, `run_optuna_enhanced.py`

---

**Note**: This folder contains ONLY scripts for live/mock trading via `launch_trading.sh`.
All other tools, utilities, and development scripts are in `../tools/`.

**Last Updated**: 2025-10-09

---

## Critical Implementation Details

### Position Reconciliation on Startup
**Problem**: If you restart trading mid-day, you may have open positions from earlier.

**Solution**: `sentio_cli live-trade` checks Alpaca account on startup:
```cpp
// Pseudo-code in live_trade_command.cpp
auto positions = alpaca_client.get_positions();
PSMState current_state = map_positions_to_psm_state(positions);
strategy.set_initial_state(current_state);
bars_held_ = calculate_bars_held(positions);  // Don't force exit immediately
```

**States Mapped**:
- No positions → `CASH_ONLY`
- SPY only → `SPY_ONLY`
- SPY + SPXL → `SPY_SPXL`
- SH only → `SH_ONLY`
- SH + SDS → `SH_SDS`

### Alpaca REST API (No Polygon, No Python Bridge)
**Current Implementation**: Uses Polygon WebSocket via Python bridge (to be removed)

**Target Implementation** (in progress):
```cpp
AlpacaClient alpaca(api_key, secret_key);
while (market_open && current_time < "15:58:00") {
    auto bars = alpaca.get_latest_bars({"SPY", "SPXL", "SH", "SDS"});
    for (auto& bar : bars) {
        strategy.on_bar(bar);
        auto signal = strategy.generate_signal();
        if (signal.action != HOLD) {
            execute_trade(signal);
        }
    }
    sleep(60);  // 1-minute bars
}
```

### Full Logging for Dashboard Generation
Every event is logged to JSONL files:

**`signals_*.jsonl`**:
```json
{"timestamp": "2025-10-09T10:30:00", "prediction_1": 0.0012, "prediction_5": 0.0015, "signal": "LONG", "confidence": 0.72}
```

**`decisions_*.jsonl`**:
```json
{"timestamp": "2025-10-09T10:30:00", "current_state": "CASH_ONLY", "signal": "LONG", "decision": "ENTER_SPY", "next_state": "SPY_ONLY"}
```

**`trades_*.jsonl`**:
```json
{"timestamp": "2025-10-09T10:30:00", "symbol": "SPY", "action": "BUY", "qty": 100, "price": 450.25, "order_id": "abc123"}
```

**`positions_*.jsonl`**:
```json
{"timestamp": "2025-10-09T10:30:00", "SPY": 100, "SPXL": 0, "SH": 0, "SDS": 0, "equity": 45025.00, "cash": 54975.00}
```

### Midday Optimization Timing
**12:45 PM**: Stop trader, fetch morning bars, optimize, restart immediately
- Trader stopped cleanly (SIGTERM, not paused)
- Morning bars (9:30 AM - 12:45 PM) fetched and appended to warmup data
- Quick optimization runs (25 trials/phase, ~10-20 minutes)
- Trader restarts immediately when optimization completes (not at fixed 1 PM)
- **Seamless warmup**: Predictor and feature engine maintain continuous state

**Why 12:45 PM not 2:30 PM?**
- More trading time in afternoon (3 hours vs 1.5 hours)
- Capture morning volatility in optimization
- Avoid late-day whipsaws

**Configurable**: Use `--midday-time` to change (e.g., `--midday-time 14:30`)

**Seamless Continuation**: The key innovation is appending morning bars to the warmup file before restart. This ensures:
- **Predictor**: Has continuous bar-to-bar returns from historical data → morning session → afternoon session
- **Feature Engine**: Rolling windows (64-bar lookback) maintain state across restart
- **No Gap**: Strategy doesn't "forget" morning context when restarting

### EOD Position Close at 3:58 PM
**Not 4:00 PM** - gives 2 minutes buffer for:
- Order submission
- Order fills
- Position confirmation
- Avoid after-hours positions

**Implementation**:
```cpp
if (current_time >= "15:58:00") {
    close_all_positions();
    stop_accepting_new_signals = true;
    transition_to_cash_only();
}
```

### Email Report Requirements
**Configure in** `config.env`:
```bash
EMAIL_TO="your_email@example.com"
EMAIL_FROM="trading_bot@example.com"
SMTP_SERVER="smtp.gmail.com"
SMTP_PORT="587"
SMTP_USER="your_gmail@gmail.com"
SMTP_PASSWORD="your_app_password"
```

**Report Contains**:
- Subject: "Trading Session Report - YYYY-MM-DD"
- Body: Session summary (P&L, trades, win rate)
- Attachment: HTML dashboard file

**Sent by**: `tools/send_dashboard_email.py` (called automatically)

---

## Implementation Status

### ✅ Currently Working
1. Pre-market 2-phase optimization
2. Strategy warmup (historical + today's bars)
3. Mock trading mode (testing)
4. Dashboard generation
5. Midday optimization trigger (12:45 PM)
6. EOD position close (3:58 PM)

### 🚧 In Progress
1. **Alpaca REST API Integration** (replacing Polygon WebSocket)
   - Location: `src/cli/live_trade_command.cpp`
   - Need to: Remove Python bridge, add direct Alpaca polling
   
2. **Position Reconciliation on Startup**
   - Location: `src/cli/live_trade_command.cpp`
   - Need to: Add `map_positions_to_psm_state()` function
   
3. **Email Report Sending**
   - Location: `scripts/launch_trading.sh` (call send_dashboard_email.py)
   - Need to: Integrate email sending at 4:00 PM

### 📋 Testing Checklist
- [ ] Pre-market optimization saves params correctly
- [ ] Warmup loads 20 blocks + today's bars
- [ ] Position reconciliation maps states correctly
- [ ] Alpaca REST API receives bars every minute
- [ ] All logs written (signals, decisions, trades, positions)
- [ ] Midday optimization closes positions, optimizes, restarts
- [ ] EOD closes all positions by 3:58 PM
- [ ] Dashboard generates correctly with all data
- [ ] Email sends successfully at 4:00 PM

---

