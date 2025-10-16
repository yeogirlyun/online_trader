# NAN_BUG_FIX_IMPLEMENTATION - Complete Analysis

**Generated**: 2025-10-15 03:30:46
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: Review document: /Volumes/ExternalSSD/Dev/C++/online_trader/megadocs/NAN_BUG_FIX_IMPLEMENTATION.md (1 valid files)

**Total Files**: 1

---

## 📋 **TABLE OF CONTENTS**

1. [megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md](#file-1)

---

## 📄 **FILE 1 of 1**: megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md

**File Information**:
- **Path**: `megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md`

- **Size**: 419 lines
- **Modified**: 2025-10-15 03:18:19

- **Type**: .md

```text
# Rotation Trading NaN Features Bug Report

**Date:** October 15, 2025
**Severity:** CRITICAL
**Status:** PARTIALLY RESOLVED
**Impact:** System generates 48,425 signals but 0 trades due to predictor receiving NaN features

---

## Executive Summary

Multi-symbol rotation trading system generates signals for all 6 leveraged ETFs but executes zero trades. Root cause: OnlinePredictor receives NaN feature values (Bollinger Bands features 25-29), preventing learning and causing all predictions to return neutral signals (prob=0.5).

**Two Critical Bugs Identified:**

1. **FIXED:** Predictor warmup requirement preventing predictions (predictor.is_ready=false)
2. **ONGOING:** Bollinger Bands features remain NaN despite 780-bar warmup

---

## Bug #1: Predictor Warmup Requirement (FIXED)

### Symptoms
- Predictor returns `predicted_return=0, confidence=0, is_ready=0`
- After 781 bars processed (780 warmup + 1 trading)
- Features extracted successfully (58 features, non-empty)

### Root Cause
**Warmup only updates feature engine, never trains the predictor.**

Warmup flow:
```
rotation_trade_command.cpp::execute_mock_trading()
  └─> backend_->warmup(warmup_data)
      └─> oes_manager_->warmup_all(symbol_bars)
          └─> oes->on_bar(bar)  [called 780 times per symbol]
              └─> feature_engine_->update(bar)  ✅ Feature engine updated
              └─> predictor->update(features, return)  ❌ NEVER CALLED
```

**Result:** Feature engine has 780 bars, but predictor has 0 training samples.

Since `predictor.samples_seen_ = 0 < warmup_samples (50)`, `is_ready()` returns false.

### Fix Applied
**File:** `src/strategy/online_ensemble_strategy.cpp:36`

**Before:**
```cpp
predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
```

**After:**
```cpp
predictor_config.warmup_samples = 0;  // No warmup - start predicting immediately
```

**Rationale:** Predictor starts with high uncertainty (`initial_variance=100.0`) and learns online as trades execute. Strategy-level warmup ensures feature engine is ready before first signal.

---

## Bug #2: Bollinger Bands NaN Features (ONGOING)

### Symptoms
```
[OnlinePredictor] WARNING: Features contain NaN/inf! Skipping update.
[OnlinePredictor] NaN feature indices: 25(nan) 26(nan) 27(nan) 28(nan) 29(nan)
Total features: 58
```

### Feature Mapping
| Index | Feature | Source Indicator |
|-------|---------|-----------------|
| 25 | bb20_.mean | Bollinger Bands 20-period mean |
| 26 | bb20_.sd | Bollinger Bands standard deviation |
| 27 | bb20_.upper | Upper band (mean + 2*sd) |
| 28 | bb20_.lower | Lower band (mean - 2*sd) |
| 29 | bb20_.percent_b | Price position within bands |

### Debug Evidence
```
[FeatureEngine #0] BB NaN detected! bar_count=0, bb20_.win.size=1, bb20_.win.full=0
[FeatureEngine #1] BB NaN detected! bar_count=1, bb20_.win.size=2, bb20_.win.full=0
[FeatureEngine #2] BB NaN detected! bar_count=2, bb20_.win.size=3, bb20_.win.full=0
[FeatureEngine #3] BB NaN detected! bar_count=3, bb20_.win.size=4, bb20_.win.full=0
[FeatureEngine #4] BB NaN detected! bar_count=4, bb20_.win.size=5, bb20_.win.full=0
```

**First 5 warnings during warmup (expected - BB needs 20 bars to be full).**

But during trading:
```
[OES::generate_signal #1] samples_seen=781, features.size=58, has_NaN=YES
[OES::generate_signal #4] samples_seen=782, features.size=58, has_NaN=YES
```

**Features still contain NaN after 781+ bars!**

### Expected vs Actual
- **Expected:** After 780 warmup bars, BB window has 20 bars → `bb20_.win.full()` returns true → BB values computed
- **Actual:** Features still contain NaN during trading → predictor skips all updates → no learning → neutral signals only

### Hypotheses Under Investigation
1. ✅ **Feature engine reset after warmup:** Debug shows `on_bar()` calls during warmup increment `bar_count`
2. ✅ **Multiple feature engines:** Each symbol has its own OES instance with separate feature engine (expected)
3. ⚠️ **New OES instances created after warmup:** Need to verify with constructor debug logging
4. ⚠️ **BB window not accumulating correctly:** Need to verify `bb20_.win.push()` is called during warmup

---

## Affected Modules and Source Files

### 1. Strategy Layer
**File:** `src/strategy/online_ensemble_strategy.cpp`
- `on_bar()` method - Updates feature engine but not predictor during warmup
- `generate_signal()` - Returns neutral when predictor not ready or features have NaN
- Constructor - Sets predictor warmup configuration
- **Lines Modified:** 36 (predictor warmup_samples), 88-90 (debug)

**File:** `include/strategy/online_ensemble_strategy.h`
- `is_ready()` inline - Checks `samples_seen_ >= warmup_samples`
- **Lines:** 147

### 2. Multi-Symbol OES Manager
**File:** `src/strategy/multi_symbol_oes_manager.cpp`
- `warmup()` method - Only calls `on_bar()`, never `update()`
- `on_bar()` method - Updates OES instances when data available in snapshot
- `generate_all_signals()` - Generates signals for all symbols with valid data
- **Lines:** 231-265 (warmup), 186-205 (on_bar), 40-127 (signal generation)

**File:** `include/strategy/multi_symbol_oes_manager.h`
- OES instance management (one instance per symbol)
- **Lines:** 45-50 (instance map)

### 3. Feature Engine
**File:** `src/features/unified_feature_engine.cpp`
- `update()` method - Calls `bb20_.update()` and `recompute_vector_()`
- `recompute_vector_()` - Extracts BB features (indices 25-29)
- **Lines:** 188-247 (update), 248-333 (recompute_vector_), 315-323 (BB NaN debug)

**File:** `include/features/unified_feature_engine.h`
- Feature schema definition (58 total features)
- BB indicator state (`bb20_` member)
- **Lines:** 67-80 (feature schema), 110 (bb20_ member)

### 4. Bollinger Bands Indicator
**File:** `include/features/indicators.h`
- `Boll` struct - Rolling window indicator
- `update()` method - Returns NaN if window not full (`!win.full()`)
- `is_ready()` method - Returns `win.full()`
- **Lines:** 85-116 (Boll definition)

**Code:**
```cpp
struct Boll {
    roll::Ring<double> win;
    int k = 2;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sd = std::numeric_limits<double>::quiet_NaN();
    // ...

    void update(double close) {
        win.push(close);

        if (!win.full()) {
            // ❌ All values remain NaN until window is full (20 bars)
            mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
            percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        mean = win.mean();
        sd = win.stdev();
        // ...
    }

    bool is_ready() const {
        return win.full();
    }
};
```

### 5. Online Predictor (EWRLS)
**File:** `src/learning/online_predictor.cpp`
- `predict()` method - Returns zeros if `!is_ready()`
- `update()` method - Skips update if features contain NaN
- `is_ready()` method - Checks `samples_seen_ >= config_.warmup_samples`
- **Lines:** 25-76 (predict), 78-185 (update), 90-109 (NaN feature detection)

**File:** `include/learning/online_predictor.h`
- Config struct with `warmup_samples` parameter
- **Lines:** 18-29 (Config definition)

### 6. Multi-Horizon Predictor
**File:** `src/learning/online_predictor.cpp`
- `MultiHorizonPredictor::predict()` - Aggregates predictions from multiple horizons
- `add_horizon()` - Creates OnlinePredictor instances with adjusted lambda
- **Lines:** 354-407 (MultiHorizonPredictor implementation)

### 7. Rotation Trading Backend
**File:** `src/backend/rotation_trading_backend.cpp`
- `warmup()` method - Calls `oes_manager_->warmup_all()`
- `on_bar()` method - Called during trading, updates OES and generates signals
- **Lines:** 63-90 (warmup), 226-246 (on_bar)

**File:** `include/backend/rotation_trading_backend.h`
- Backend configuration
- **Lines:** 28-75 (Config struct)

### 8. Rotation Trade Command (CLI)
**File:** `src/cli/rotation_trade_command.cpp`
- `load_warmup_data()` - Loads 780 bars per symbol from CSV
- `execute_mock_trading()` - Calls backend warmup before starting trading
- **Lines:** 203-290 (warmup data loading), 292-380 (mock trading execution)

**File:** `include/cli/rotation_trade_command.h`
- Command options and interface
- **Lines:** 29-56 (Options struct)

### 9. Data Management
**File:** `src/data/multi_symbol_data_manager.cpp`
- `get_latest_snapshot()` - Returns snapshot with latest bars for each symbol
- `update_symbol()` - Updates symbol state with new bar
- **Lines:** 25-113 (get_latest_snapshot), 115-156 (update_symbol)

**File:** `include/data/multi_symbol_data_manager.h`
- Snapshot structure
- Symbol state tracking
- **Lines:** 24-48 (MultiSymbolSnapshot), 50-66 (SymbolSnapshot)

### 10. Mock Multi-Symbol Feed
**File:** `src/data/mock_multi_symbol_feed.cpp`
- `replay_next_bar()` - Replays bars chronologically across all symbols
- Bar callback triggering (triggers backend `on_bar()`)
- **Lines:** 237-273 (replay logic)

**File:** `include/data/mock_multi_symbol_feed.h`
- Feed interface and configuration
- **Lines:** 18-51 (class definition)

### 11. Configuration
**File:** `config/rotation_strategy.json`
- `oes_config.warmup_samples: 100` - Strategy-level warmup
- `oes_config.bb_period: 20` - Bollinger Bands window size
- **Lines:** 22 (warmup_samples), 32 (bb_period)

---

## Impact Analysis

### Current State
```
Bars processed: 8211
Signals generated: 48,425
Trades executed: 0        ❌ ZERO TRADES
Positions opened: 0
Positions closed: 0
Total P&L: $0.00
MRD: 0.000%
```

### Expected State (After Full Fix)
```
Bars processed: 8211
Signals generated: 48,425
Trades executed: 1000+    ✅ Active trading
Positions opened: 500+
Positions closed: 500+
Total P&L: Variable
MRD: Target > 0.3%
```

---

## Next Steps

### Immediate Actions
1. ✅ **DONE:** Set `predictor_config.warmup_samples = 0`
2. 🔄 **IN PROGRESS:** Add OES constructor debug to detect multiple instances
3. 🔄 **IN PROGRESS:** Verify BB window accumulation during warmup
4. ⏳ **PENDING:** Add per-symbol BB readiness check before signal generation

### Alternative Solutions Under Consideration

#### Option A: Forward-fill BB features during warmup
Replace NaN with last valid value or 0.0 during first 20 bars.

**Pros:**
- Predictor can start learning immediately
- No modifications to indicator logic

**Cons:**
- Forward-filled features may mislead predictor
- Not addressing root cause

#### Option B: Start trading after BB warmup
Skip signal generation until all feature engines report `bb20_.is_ready()`.

**Pros:**
- Guarantees no NaN features
- Clean separation of concerns

**Cons:**
- Delays trading start by 20 bars per symbol
- Reduces training data for predictor

#### Option C: Feature-level warmup tracking
Track readiness per feature, skip unready features in prediction.

**Pros:**
- Gradual feature availability
- Predictor can start with partial features

**Cons:**
- Complex feature selection logic
- Predictor expects fixed-size feature vector

---

## Testing Plan

### Test 1: Verify Predictor Readiness
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup \
  --warmup-dir data/tmp/rotation_warmup 2>&1 | \
  grep "is_ready="
```

**Expected:** `is_ready=1` after warmup
**Actual:** `is_ready=0` (before fix), `is_ready=1` (after fix) ✅

### Test 2: Count NaN Features During Trading
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup 2>&1 | \
  grep "NaN feature indices" | wc -l
```

**Expected:** 0 (no NaN features after warmup)
**Actual:** 100+ (NaN features persist) ❌

### Test 3: Verify Trade Execution
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup 2>&1 | \
  grep "Trades executed"
```

**Expected:** `Trades executed: 1000+`
**Actual:** `Trades executed: 0` ❌

---

## Related Issues

- **Data Availability:** Some symbols missing from snapshot during trading
  - SPXL and SDS frequently show "No data in snapshot"
  - Caused by different bar counts across symbols (7125-8211 bars)

- **Callback Timing:** Previously fixed - callback was per-symbol instead of per-timestamp
  - Fixed in `src/data/mock_multi_symbol_feed.cpp:267`

---

## References

### Debug Outputs
```
[FeatureEngine] WARNING: bb20_ not ready! bar_count=0, bb20_.win.size=1, bb20_.win.full=0
[OnlinePredictor] NaN feature indices: 25(nan) 26(nan) 27(nan) 28(nan) 29(nan)
[OES] generate_signal #1: predicted_return=0, confidence=0, is_ready=0
[OES::generate_signal #1] samples_seen=781, features.size=58, has_NaN=YES
```

### Key Code Locations
- Predictor warmup config: `src/strategy/online_ensemble_strategy.cpp:36`
- BB NaN check: `src/features/unified_feature_engine.cpp:316`
- Predictor NaN skip: `src/learning/online_predictor.cpp:91`
- Warmup execution: `src/strategy/multi_symbol_oes_manager.cpp:243`
- BB indicator logic: `include/features/indicators.h:100`

---

## Source Module Reference

### Complete List of Related Files

**Source Files (Implementation):**
1. `src/strategy/online_ensemble_strategy.cpp` - Main OES implementation with predictor integration
2. `src/strategy/multi_symbol_oes_manager.cpp` - Multi-symbol OES coordination and warmup
3. `src/features/unified_feature_engine.cpp` - Feature computation including Bollinger Bands
4. `src/learning/online_predictor.cpp` - EWRLS predictor with NaN detection
5. `src/backend/rotation_trading_backend.cpp` - Trading session management
6. `src/cli/rotation_trade_command.cpp` - CLI entry point and warmup data loading
7. `src/data/multi_symbol_data_manager.cpp` - Multi-symbol data synchronization
8. `src/data/mock_multi_symbol_feed.cpp` - CSV-based bar replay for backtesting

**Header Files (Interface):**
1. `include/strategy/online_ensemble_strategy.h` - OES interface and configuration
2. `include/strategy/multi_symbol_oes_manager.h` - Multi-symbol manager interface
3. `include/features/unified_feature_engine.h` - Feature engine interface and schema
4. `include/features/indicators.h` - Bollinger Bands indicator definition
5. `include/learning/online_predictor.h` - Predictor configuration and interface
6. `include/backend/rotation_trading_backend.h` - Backend configuration
7. `include/cli/rotation_trade_command.h` - CLI command interface
8. `include/data/multi_symbol_data_manager.h` - Data manager configuration
9. `include/data/mock_multi_symbol_feed.h` - Mock feed interface

**Configuration Files:**
1. `config/rotation_strategy.json` - Strategy parameters and symbol configuration

**Total Files:** 18 (8 source, 9 headers, 1 config)

---

**Bug Report Generated:** October 15, 2025
**Last Updated:** October 15, 2025
**Tracking ID:** ROTATION-NAN-001

```

