# Rotation Trading System - Critical Bugs Report

**Date:** October 15, 2025
**Severity:** CRITICAL - System Broken
**Status:** Partially Fixed (1/4 bugs resolved)
**Test Date:** October 15, 2025 06:57 EDT
**Session:** Mock trading (Oct 14, 2025 data)

---

## Executive Summary

The rotation trading system has **4 CRITICAL BUGS** that render it non-functional for production:

1. ✅ **FIXED:** Configuration loading bug (min_confidence, min_probability not loaded)
2. ❌ **UNFIXED:** Trade churning bug (enter/exit every bar, -4.54% MRD)
3. ❌ **UNFIXED:** Zero position size bug (0 shares traded despite 7,471 trades)
4. ❌ **UNFIXED:** Null trade data bug (missing exec_price, decision in logs)

**Impact:**
- **Expected MRD:** ≈+0.5% per day
- **Actual MRD:** -4.544% per day (899% worse than expected!)
- **P&L:** $0.00 (no actual capital deployed)
- **Sharpe Ratio:** -16.8 (catastrophic)

**Root Cause:** Configuration cascade failure → aggressive filtering → chicken-and-egg predictor problem → excessive churning + broken position sizing

---

## Test Results Summary

### Session Metrics

```
Date: October 14, 2025 (mock replay)
Symbols: TQQQ, SQQQ, SPXL, SDS, UVXY, SVXY
Bars processed: 8,602
Warmup: 1,560 bars per symbol
```

### Before Fix (Config Bug Present)

```
Signals generated: 51,586
Decisions made: 0           ← NO DECISIONS!
Trades executed: 0          ← NO TRADES!
MRD: 0.000%                 ← BROKEN
```

**Cause:** `min_confidence=0.55` (hardcoded) but actual confidence=0.000007 → all signals filtered out

### After Fix (Config Bug Resolved)

```
Signals generated: 51,586
Decisions made: 14,882      ✅ Now making decisions
Trades executed: 7,471      ✅ Now executing trades
MRD: -4.544%                ❌ MASSIVE LOSSES
Sharpe: -16.8               ❌ TERRIBLE
P&L: $0.00                  ❌ NO ACTUAL POSITIONS
```

### Signal Distribution

```
Total signals: 51,586
  - NEUTRAL (0): 5,708 (11%)
  - LONG (1):    20,726 (40%)
  - SHORT (2):   25,152 (49%)

Probability >= 0.51: 21,647 signals (42%)
Confidence: 0.000001 - 0.0001 (extremely low, predictor not ready)
```

### Decision Distribution

```
Total decisions: 14,882
  - ENTER_LONG (2):  7,451 (50%)
  - EXIT (1):        7,427 (50%)
  - STOP_LOSS (6):   4 (0.03%)
```

### Trades by Symbol

```
SPXL: 1,161 trades (15.5%)
TQQQ: 1,861 trades (24.9%)
SQQQ: 990 trades (13.2%)
SDS: 348 trades (4.7%)
UVXY: 746 trades (10.0%)
SVXY: 2,365 trades (31.6%)
```

---

## Bug #1: Configuration Loading Failure ✅ FIXED

### Description

The `load_config()` function in `rotation_trade_command.cpp` was only loading `min_strength` from the JSON config, but NOT `min_confidence` or `min_probability`.

### Root Cause

**File:** `src/cli/rotation_trade_command.cpp:149-153`

**Before (Broken):**
```cpp
// Load signal aggregator config
if (j.contains("signal_aggregator_config")) {
    auto agg = j["signal_aggregator_config"];
    config.aggregator_config.min_strength = agg.value("min_strength", 0.40);
    // ❌ MISSING: min_confidence, min_probability
}
```

**Result:**
- Config file says: `min_confidence: 0.000001`
- Code uses: `min_confidence: 0.55` (hardcoded default)
- Actual signal confidence: `0.000007`
- Filter result: **ALL SIGNALS REJECTED** (0.000007 < 0.55)

### Impact

**Before Fix:**
- 0 decisions made
- 0 trades executed
- System completely non-functional

### Fix Applied

**File:** `src/cli/rotation_trade_command.cpp:149-155`

```cpp
// Load signal aggregator config
if (j.contains("signal_aggregator_config")) {
    auto agg = j["signal_aggregator_config"];
    config.aggregator_config.min_probability = agg.value("min_probability", 0.51);
    config.aggregator_config.min_confidence = agg.value("min_confidence", 0.55);
    config.aggregator_config.min_strength = agg.value("min_strength", 0.40);
}
```

**Status:** ✅ FIXED (committed, rebuilt, tested)

---

## Bug #2: Trade Churning (Enter/Exit Every Bar) ❌ UNFIXED

### Description

The system is entering and immediately exiting positions **every single bar**, creating massive churning losses.

### Symptom Pattern

```
Decision Log Shows:
Bar 1: ENTER SPXL (rank=1, strength=0.000011)
Bar 2: EXIT SPXL (reason: "Rank fell below threshold (1)")
Bar 3: ENTER SPXL (rank=1, strength=0.000011)
Bar 4: EXIT SPXL (reason: "Rank fell below threshold (1)")
... repeats 7,451 times
```

### Root Cause Analysis

**File:** `src/strategy/rotation_position_manager.cpp:56-73`

```cpp
// Update current rank and strength
const auto* signal = find_signal(symbol, ranked_signals);
if (signal) {
    position.current_rank = signal->rank;
    position.current_strength = signal->strength;
} else {
    // Signal not in ranked list - mark for exit
    position.current_rank = 9999;       ← BUG: Triggers exit
    position.current_strength = 0.0;
}

// Check exit conditions
Decision decision = check_exit_conditions(position, ranked_signals, current_time_minutes);
```

**The Logic Error:**

The `find_signal()` function is failing to find the signal in `ranked_signals`, so it sets rank=9999, which triggers immediate exit.

**Hypothesis:** Signal aggregator is filtering out the signal AFTER it's been entered, due to:
1. Low confidence threshold violations
2. Staleness filtering
3. Neutral signal filtering (signal changes to NEUTRAL after entry)

**File:** `src/strategy/signal_aggregator.cpp:136-140`

```cpp
bool SignalAggregator::passes_filters(const SignalOutput& signal) const {
    // Filter NEUTRAL signals
    if (signal.signal_type == SignalType::NEUTRAL) {
        return false;  // ← If signal becomes NEUTRAL, it's filtered out
    }
    ...
}
```

### Impact

```
Trades: 7,471 (should be ~100-500 for rotation strategy)
MRD: -4.544% (losses from churning)
Win Rate: 0% (positions closed before any profit)
```

### Why This Happens

**Chicken-and-Egg Problem:**

1. Predictor has 0 training samples (samples_seen=0)
2. Low confidence predictions (confidence=0.000007)
3. Signal probability fluctuates wildly (0.1 → 0.9 → 0.5 bar-to-bar)
4. When signal drops to NEUTRAL or low probability, it's filtered from ranked list
5. Position manager can't find signal → assumes exit condition
6. Exit immediately → no learning → confidence stays low → cycle repeats

### Evidence

**Signal Volatility:**
```json
Bar 123: {"symbol":"SPXL","signal":1,"probability":0.616,"confidence":0.000012}
Bar 124: {"symbol":"SPXL","signal":0,"probability":0.501,"confidence":0.000099}  ← NEUTRAL!
Bar 125: {"symbol":"SPXL","signal":2,"probability":0.113,"confidence":0.000007}  ← SHORT!
```

Signals are changing every bar because:
- Predictor is untrained (is_ready=0)
- Returns essentially random predictions
- No stability or persistence

---

## Bug #3: Zero Position Size ❌ UNFIXED

### Description

Despite executing 7,471 trades, **0 shares** are actually being traded, resulting in **$0.00 P&L**.

### Evidence

**Trade Log:**
```json
{
  "symbol": "TQQQ",
  "action": "ENTRY",
  "direction": "LONG",
  "price": 103.0,         ✅ Valid price
  "shares": 0,            ❌ ZERO SHARES!
  "value": 0.0            ❌ ZERO CAPITAL!
}
```

### Root Cause

**File:** `src/backend/rotation_trading_backend.cpp:445-449`

```cpp
int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    double capital_allocated = current_cash_ * config_.rotation_config.capital_per_position;
    double price = get_execution_price(decision.symbol, "BUY");

    if (price <= 0) {
        return 0;  // ← Returns 0 if price invalid
    }

    int shares = static_cast<int>(capital_allocated / price);
    return shares;
}
```

**Hypothesis:**
1. `get_execution_price()` is returning 0 or negative
2. OR `current_cash_` is 0
3. OR `capital_per_position` is 0

**File:** `src/backend/rotation_trading_backend.cpp:430-443`

```cpp
double RotationTradingBackend::get_execution_price(
    const std::string& symbol,
    const std::string& side
) {
    auto snapshot = data_manager_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        utils::log_warning("No data for " + symbol + " - using 0.0");
        return 0.0;  // ← BUG: Returns 0 instead of throwing error
    }

    // Use close price (in live trading, would use bid/ask)
    return snapshot.snapshots.at(symbol).latest_bar.close;
}
```

### Impact

```
Expected: ~$33,333 per position (3 positions, $100k capital)
Actual: $0 (no capital deployed)
Result: MRD = 0%, but system thinks it's trading
```

### Why P&L Shows $0.00

Even with 7,471 trades, if `shares=0`, then:
```
P&L = (exit_price - entry_price) × shares
P&L = (103.5 - 103.0) × 0 = $0.00
```

---

## Bug #4: Null Trade Data ❌ UNFIXED

### Description

Trade log contains NULL values for critical fields: `exec_price`, `decision`.

### Evidence

**Early trades show:**
```json
{
  "symbol": "SPXL",
  "decision": null,      ← Should be integer (Decision enum)
  "exec_price": null,    ← Should be double
  "shares": 160          ← Only shares is populated
}
```

**Later trades show:**
```json
{
  "symbol": "TQQQ",
  "action": "ENTRY",     ← Different schema!
  "direction": "LONG",
  "price": 103.0,
  "shares": 0,
  "value": 0.0
}
```

### Root Cause

**File:** `src/backend/rotation_trading_backend.cpp:504-526`

**Two different logging functions are writing to the same file:**

```cpp
void RotationTradingBackend::log_trade(
    const RotationPositionManager::PositionDecision& decision,
    double execution_price,
    int shares
) {
    if (!trade_log_.is_open()) {
        return;
    }

    json j;
    j["symbol"] = decision.symbol;
    // ❌ BUG: Not setting "decision" field properly
    // ❌ BUG: Not setting "exec_price" field

    trade_log_ << j.dump() << std::endl;
}
```

**AND:**

Position manager has its own trade logging that uses a different schema.

### Impact

- Dashboard cannot parse trades correctly
- Performance analysis is inaccurate
- Debugging is extremely difficult
- Data corruption in trade logs

---

## Diagnostic Evidence

### Configuration File Used

**File:** `config/rotation_strategy.json`

```json
{
  "signal_aggregator_config": {
    "min_probability": 0.51,
    "min_confidence": 0.000001,    ← Ultra-low to allow cold start
    "min_strength": 0.000000001,   ← Ultra-low to allow cold start
    ...
  },

  "rotation_manager_config": {
    "max_positions": 3,
    "min_rank_to_hold": 5,
    "min_strength_to_enter": 0.000000001,
    "min_strength_to_hold": 0.0000000005,
    ...
  }
}
```

### Predictor State

```
Warmup: 1,560 bars processed
is_ready: 0                    ← NOT READY!
samples_seen: 0                ← NO TRAINING SAMPLES!
confidence: 0.000001-0.0001    ← EXTREMELY LOW
```

**Why Not Ready:**

**File:** `include/learning/online_predictor.h:73`

```cpp
bool is_ready() const {
    return samples_seen_ >= config_.warmup_samples;
}
```

`samples_seen_` only increments when receiving **trade outcomes**, but:
- No trades → No outcomes → samples_seen=0 → is_ready=false → low confidence → filtered signals → no trades → **INFINITE LOOP**

### Signal Quality Analysis

**Sample signals from log:**

```json
{"bar_id":29299050,"confidence":0.0,"probability":0.5,"signal":0,"symbol":"SDS"}
  ↑ First bar: NEUTRAL, zero confidence (predictor not ready)

{"bar_id":29299070,"confidence":0.000007,"probability":0.114,"signal":2,"symbol":"SQQQ"}
  ↑ Bar 20: SHORT signal, tiny confidence

{"bar_id":29341195,"confidence":0.000022,"probability":0.433,"signal":2,"symbol":"SDS"}
  ↑ Last bar: Still low confidence after 8,602 bars
```

**Confidence never improves** because predictor never receives trade outcomes to learn from.

---

## Source Module Reference

### Core Trading Components

#### Backend

1. **src/backend/rotation_trading_backend.cpp** (22,731 bytes)
   - Main trading loop (`on_bar()` line 226-286)
   - Position sizing (`calculate_position_size()` line 445-449)
   - Execution price lookup (`get_execution_price()` line 430-443)
   - Trade execution (`execute_decision()` line 369-428)
   - Trade logging (`log_trade()` line 504-526)
   - Decision logging (`log_decision()` line 484-502)

2. **include/backend/rotation_trading_backend.h** (356 lines)
   - Backend configuration struct (line 50-82)
   - Session statistics struct (line 87-110)
   - Method declarations

#### Strategy Components

3. **src/strategy/rotation_position_manager.cpp** (15,089 bytes)
   - Decision making (`make_decisions()` line 19-207)
   - Exit condition checking (`check_exit_conditions()` line 56-73)
   - Signal lookup (`find_signal()` - not shown in excerpts)
   - Position tracking

4. **include/strategy/rotation_position_manager.h** (100+ lines)
   - Position struct (line 70-83)
   - Decision enum (line 88-97)
   - PositionDecision struct (line 99+)
   - Config struct (line 41-65)

5. **src/strategy/signal_aggregator.cpp** (182 lines)
   - Signal ranking (`rank_signals()` line 18-86)
   - Signal filtering (`passes_filters()` line 136-169)
   - Strength calculation (`calculate_strength()` line 119-134)

6. **include/strategy/signal_aggregator.h** (100+ lines)
   - RankedSignal struct (line 60-72)
   - Config struct (line 32-55)
   - Filtering parameters

#### Multi-Symbol OES Management

7. **src/strategy/multi_symbol_oes_manager.cpp** (not fully shown)
   - Manages 6 independent OES instances
   - Signal generation (`generate_all_signals()`)
   - Warmup coordination

8. **include/strategy/multi_symbol_oes_manager.h** (not fully shown)
   - OES manager interface
   - Per-symbol strategy instances

#### Online Ensemble Strategy

9. **src/strategy/online_ensemble_strategy.cpp** (referenced)
   - Signal generation (`generate_signal()`)
   - Feature engine integration
   - Predictor integration

10. **include/strategy/online_ensemble_strategy.h** (referenced)
    - OES configuration
    - Signal output structure

#### Online Predictor

11. **src/learning/online_predictor.cpp** (referenced)
    - Prediction (`predict()` line ~27)
    - Readiness check (`is_ready()`)
    - Learning updates

12. **include/learning/online_predictor.h** (referenced)
    - PredictionResult struct (line 43-49)
    - Readiness condition (line 73)
    - Sample tracking (`samples_seen_`)

#### CLI Command

13. **src/cli/rotation_trade_command.cpp** (referenced)
    - Configuration loading (`load_config()` line 118-201)
    - **BUG FIXED HERE** (line 149-155)
    - Warmup data loading
    - Session orchestration

14. **include/cli/rotation_trade_command.h** (referenced)
    - Command interface
    - Options structure

#### Signal Output

15. **src/strategy/signal_output.cpp** (referenced)
    - Signal structure implementation

16. **include/strategy/signal_output.h** (referenced)
    - SignalOutput struct
    - SignalType enum

#### Data Management

17. **src/data/multi_symbol_data_manager.cpp** (not shown)
    - Multi-symbol data synchronization
    - Snapshot creation
    - Staleness tracking

18. **include/data/multi_symbol_data_manager.h** (not shown)
    - DataManager interface
    - Snapshot structure

### Feature Engineering

19. **src/features/unified_feature_engine.cpp** (referenced)
    - Feature calculation
    - Bollinger Bands implementation

20. **include/features/unified_feature_engine.h** (referenced)
    - Feature engine interface

21. **src/features/rolling.cpp** (if exists)
    - Rolling statistics (Welford algorithm)

22. **include/features/rolling.h** (referenced)
    - Welford statistics implementation
    - **PREVIOUS BUG FIXED:** negative m2 clamping

### Configuration

23. **config/rotation_strategy.json** (102 lines)
    - All system parameters
    - OES configuration (line 18-45)
    - Signal aggregator config (line 47-57)
    - Rotation manager config (line 59-79)
    - **BUG:** Config values not being loaded properly

### Logging Output

24. **logs/rotation_mock/signals.jsonl** (51,586 lines)
    - All signals generated during session
    - Shows signal distribution and quality

25. **logs/rotation_mock/decisions.jsonl** (14,882 lines)
    - All position decisions
    - Shows churning pattern

26. **logs/rotation_mock/trades.jsonl** (7,471 lines)
    - **CORRUPT:** Contains null values
    - Shows zero position sizes

27. **logs/rotation_mock/positions.jsonl** (7,898 lines)
    - Position state snapshots

### Launch Infrastructure

28. **scripts/launch_rotation_trading.sh** (600+ lines)
    - Auto data download
    - Session orchestration
    - Dashboard generation
    - Email notification

---

## Reproduction Steps

### Prerequisites

```bash
# Ensure in project root
cd /Volumes/ExternalSSD/Dev/C++/online_trader

# Ensure built with latest fixes
cd build && cmake .. && make -j4 sentio_cli
```

### Reproduce Bug #1 (Configuration Loading) - NOW FIXED

**Before fix:**

1. Set ultra-low thresholds in `config/rotation_strategy.json`:
   ```json
   "min_confidence": 0.000001
   ```

2. Run without fix:
   ```bash
   ./build/sentio_cli rotation-trade --mode mock --data-dir data/tmp/rotation_warmup
   ```

3. **Observe:** 0 trades executed (config not loaded)

**After fix:**

Same steps → 7,471 trades executed ✅

### Reproduce Bug #2 (Trade Churning)

1. Run mock trading:
   ```bash
   ./scripts/launch_rotation_trading.sh mock
   ```

2. Check decision pattern:
   ```bash
   head -20 logs/rotation_mock/decisions.jsonl | jq -c '{symbol, decision, reason}'
   ```

3. **Observe:** Alternating ENTER(2) → EXIT(1) → ENTER(2) → EXIT(1) pattern

### Reproduce Bug #3 (Zero Shares)

1. Run mock trading (as above)

2. Check trade shares:
   ```bash
   cat logs/rotation_mock/trades.jsonl | jq '.shares' | sort | uniq -c
   ```

3. **Observe:** Many trades with `shares: 0`

4. Check final P&L:
   ```bash
   grep "Total P&L" logs/rotation_mock/*.log
   ```

5. **Observe:** $0.00 despite 7,471 trades

### Reproduce Bug #4 (Null Data)

1. Run mock trading (as above)

2. Check for null values:
   ```bash
   head -10 logs/rotation_mock/trades.jsonl | jq 'select(.exec_price == null or .decision == null)'
   ```

3. **Observe:** Multiple trades with null fields

---

## Impact Assessment

### Critical System Failures

| Failure | Expected | Actual | Impact |
|---------|----------|--------|--------|
| Configuration loading | Values from JSON | Hardcoded defaults | Total system blockage |
| Trade frequency | 100-500/day | 7,471/day | 10-75x excessive |
| Position holding | 5-50 bars | 1 bar | 98% churn rate |
| MRD | +0.5% | -4.544% | 1009% worse |
| Sharpe Ratio | +2.0 | -16.8 | 940% worse |
| Capital deployed | $100,000 | $0 | No actual trading |

### User Expectations vs Reality

**User Request:**
> "MRD should be close to 0.5"

**Reality:**
- MRD: -4.544% (899% off target)
- System loses money 10x faster than expected gains

**User Request:**
> "Check for 0 signals or neutral signals problems"

**Reality:**
- Signals: 51,586 generated ✅
- Non-neutral: 45,878 (89%) ✅
- But: Excessive churning due to signal instability

### Production Readiness

**VERDICT: NOT PRODUCTION READY**

Critical blockers:
1. ✅ Config loading (FIXED)
2. ❌ Trade churning (must fix before production)
3. ❌ Zero position size (must fix before production)
4. ❌ Null trade data (must fix before production)

**Estimated Fix Effort:**
- Bug #2 (churning): 4-8 hours (complex logic fix)
- Bug #3 (zero shares): 2-4 hours (debugging + fix)
- Bug #4 (null data): 1-2 hours (simple logging fix)

**Total:** 1-2 days to production ready

---

## Recommended Next Steps

### Immediate Actions (Priority 1)

1. **Fix Bug #3 (Zero Shares)** - Easiest to diagnose
   - Add debug logging to `calculate_position_size()`
   - Check `current_cash_`, `capital_per_position`, `execution_price`
   - Fix null/zero return from `get_execution_price()`

2. **Fix Bug #4 (Null Data)** - Simple logging fix
   - Standardize trade log schema
   - Populate all required fields
   - Remove duplicate logging calls

3. **Fix Bug #2 (Churning)** - Most complex
   - Option A: Fix `find_signal()` to handle NEUTRAL signals
   - Option B: Change exit logic (don't exit if rank=1)
   - Option C: Add signal persistence/smoothing
   - Option D: Pre-train predictor with synthetic data

### Medium-Term Improvements (Priority 2)

4. **Predictor Cold-Start Solution**
   - Ship pre-trained model weights
   - OR use simpler heuristics until confidence builds
   - OR require minimum bars before trading

5. **Signal Stability**
   - Add signal smoothing (EMA of predictions)
   - Require N consecutive bars before entry/exit
   - Add hysteresis to thresholds

6. **Risk Management**
   - Enforce max trades per day (prevent churning)
   - Add minimum holding period (e.g., 5 bars)
   - Add circuit breaker for excessive losses

### Long-Term Architecture (Priority 3)

7. **Predictor Training Pipeline**
   - Offline training on historical data
   - Confidence calibration
   - Backtested model validation

8. **Testing Framework**
   - Unit tests for position manager logic
   - Integration tests for full trading loop
   - Regression tests for bugs #1-4

9. **Monitoring & Alerts**
   - Real-time MRD tracking
   - Churn rate alerts
   - Position size validation

---

## Related Documentation

1. **megadocs/ROTATION_MOCK_TRADING_COMPLETE.md**
   - Initial implementation documentation
   - Usage guide
   - Expected behavior

2. **megadocs/WELFORD_NEGATIVE_M2_BUG_FIX.md**
   - Previous critical bug (NaN features)
   - Fixed with m2 clamping
   - Related to feature quality

3. **config/rotation_strategy.json**
   - Current configuration
   - Ultra-low thresholds (temporary workaround)
   - Production values need recalibration

4. **scripts/launch_rotation_trading.sh**
   - Self-sufficient launcher
   - Data download automation
   - Dashboard generation

---

## Conclusion

The rotation trading system has made significant progress:
- ✅ Configuration bug fixed
- ✅ Signals are generating
- ✅ Trades are executing

But **3 CRITICAL BUGS** remain that must be fixed before production:
1. Trade churning (-4.54% MRD from excessive trading)
2. Zero position size ($0 capital deployed)
3. Null trade data (corrupt logs)

**Estimated time to production:** 1-2 days of focused debugging and fixes.

**Immediate priority:** Fix zero shares bug first (easiest), then churning (hardest), then logging (simplest).

---

**Report Status:** COMPLETE
**Next Action:** Debug Bug #3 (zero shares) with detailed logging
**Owner:** Development Team
**Deadline:** Before live trading deployment
