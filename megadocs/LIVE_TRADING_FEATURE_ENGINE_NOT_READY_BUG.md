# CRITICAL BUG: Feature Engine Not Ready After Warmup in Live Trading

**Date**: 2025-10-07
**Severity**: CRITICAL - Blocks all live trading
**Status**: OPEN - Root cause identified, fix pending
**Impact**: Live trading generates only neutral signals (p=0.5000) indefinitely

---

## Executive Summary

The live trading system successfully:
- ✅ Connects to Alpaca Paper Trading
- ✅ Receives real-time 1-minute bars from Polygon WebSocket
- ✅ Completes warmup with 960 historical bars
- ✅ Shows `Strategy.is_ready() = YES` after warmup
- ✅ Calls `on_bar()` before `generate_signal()` in correct order

**BUT** still produces signals with exactly `p=0.5000` (neutral) for every bar because:
- ❌ `UnifiedFeatureEngine.is_ready()` returns `false`
- ❌ `extract_features()` returns empty vector
- ❌ Strategy falls back to neutral signal

**Root Cause**: The `UnifiedFeatureEngine` requires 64 bars in its internal `bar_history_` before becoming ready, but somehow this history is not being populated during warmup despite calling `feature_engine_->update(bar)` 960 times.

---

## Symptoms

1. **Console Output**:
   ```
   [2025-10-07 23:47:00] Signal: NEUTRAL (p=0.5000, conf=0.00) [DBG: ready=YES]
     [DBG: p=0.5 reason=unknown]
   ```

2. **Strategy State**:
   - `OnlineEnsembleStrategy.is_ready()` = `YES` (samples_seen_ = 960)
   - `OnlineEnsembleStrategy.bar_history_.size()` = 960 (assumed)
   - `UnifiedFeatureEngine.is_ready()` = `NO` (bar_history_.size() < 64)

3. **Signal Generation Path**:
   ```
   on_bar() called ✅
   └─> feature_engine_->update(bar) ✅
   └─> samples_seen_++ ✅

   generate_signal() called ✅
   └─> is_ready() returns true ✅
   └─> extract_features() called ✅
       └─> feature_engine_->is_ready() returns FALSE ❌
       └─> returns {} (empty features) ❌
   └─> Returns p=0.5 neutral signal ❌
   ```

---

## Timeline of Debug Session

### Session Start: 22:33 ET
- Live trading launched, connected to Alpaca successfully
- **Problem discovered**: All signals showing `p=0.5000`

### Initial Investigation (22:40-23:00)
**Issue 1**: No warmup data loading
- **Cause**: Warmup file path not found from working directory
- **Fix**: Added fallback path check (`../data/equities/SPY_RTH_NH.csv`)
- **Result**: Warmup now loads 960 bars ✅

### Second Investigation (23:00-23:30)
**Issue 2**: Strategy not ready after warmup
- **Cause**: `warmup_strategy()` called `generate_signal()` but not `update()`
- **Fix**: Added `strategy_.update(bar, 0.0)` in warmup loop
- **Result**: Still not ready ❌

### Third Investigation (23:30-23:40)
**Issue 3**: Wrong update function called
- **Cause**: `update(bar, pnl)` doesn't increment `samples_seen_`
- **Discovery**: `on_bar(bar)` is the function that increments counter
- **Fix**: Changed warmup and live trading to call `on_bar()` instead
- **Result**: `samples_seen_` now = 960, `is_ready()` = YES ✅

### Fourth Investigation (23:40-23:45)
**Issue 4**: `on_bar()` called AFTER `generate_signal()`
- **Cause**: Wrong order in `on_new_bar()` - features not updated yet
- **Fix**: Moved `strategy_.on_bar(bar)` before `generate_signal(bar)`
- **Result**: Features should now be ready, but still p=0.5000 ❌

### Final Investigation (23:45-23:51)
**Issue 5**: CURRENT BUG - Feature engine not ready
- **Discovery**: Added debug logging showing `reason=unknown`
- **Root Cause Found**: `UnifiedFeatureEngine.is_ready()` returns false
- **Why**: `bar_history_.size() < 64` despite 960 bars in warmup
- **Status**: NEEDS FIX

---

## Root Cause Analysis

### The Feature Engine Ready Check

**File**: `src/features/unified_feature_engine.cpp:625-629`
```cpp
bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    return bar_history_.size() >= 64;
}
```

### The Update Flow

**Warmup (src/cli/live_trade_command.cpp:304-307)**:
```cpp
for (size_t i = start_idx; i < all_bars.size(); ++i) {
    strategy_.on_bar(all_bars[i]); // Feed bar to strategy (increments sample counter)
    generate_signal(all_bars[i]); // Generate signal (after features are ready)
}
```

**on_bar() Implementation (src/strategy/online_ensemble_strategy.cpp:140-157)**:
```cpp
void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);  // ← Should add bar to feature engine's history

    samples_seen_++;

    // ... threshold calibration ...
}
```

**Feature Engine Update (src/features/unified_feature_engine.cpp:??)**:
```cpp
void UnifiedFeatureEngine::update(const Bar& bar) {
    // TODO: VERIFY THIS IS ADDING TO bar_history_
    // If not, this is the bug!
}
```

### Hypothesis

The `UnifiedFeatureEngine::update()` method is **NOT** adding bars to its internal `bar_history_` deque, OR it's resetting/clearing the history somewhere.

**Evidence**:
1. After 960 warmup bars + 5 live bars (965 total), feature engine still not ready
2. Requires only 64 bars, so should have been ready after bar 64
3. `on_bar()` is being called correctly (verified by samples_seen_ = 960)
4. `feature_engine_->update(bar)` is being called (line 148)
5. Yet `bar_history_.size()` < 64

**Conclusion**: The feature engine's `update()` method is either:
- Not adding bars to `bar_history_`
- Clearing `bar_history_` somewhere
- Has a bug in the update logic
- OR there's a separate instance being created

---

## Code Locations

### Primary Bug Location
```
src/features/unified_feature_engine.cpp
  - Line 625-629: is_ready() check
  - update() method: NEEDS INVESTIGATION
```

### Related Files Modified During Debug
```
src/cli/live_trade_command.cpp
  - Line 247-311: warmup_strategy()
  - Line 329-360: on_new_bar()
  - Line 372-406: generate_signal() with debug logging
```

### Signal Generation Flow
```
src/strategy/online_ensemble_strategy.cpp
  - Line 54-126: generate_signal()
  - Line 140-157: on_bar()
  - Line 163-175: extract_features()
```

---

## Attempted Fixes (All Failed)

1. ✅ **Fixed warmup file path** - Warmup now loads successfully
2. ✅ **Called update() in warmup** - Changed to on_bar(), samples_seen_ increments
3. ✅ **Changed call order** - on_bar() before generate_signal()
4. ❌ **Feature engine still not ready** - Current state

---

## Next Steps to Fix

### Investigation Needed
1. **Add logging to UnifiedFeatureEngine::update()**
   ```cpp
   void UnifiedFeatureEngine::update(const Bar& bar) {
       // Add this
       std::cout << "[UFE] update() called, bar_history_.size()="
                 << bar_history_.size() << std::endl;

       // ... existing code ...

       // Add this
       std::cout << "[UFE] after update, bar_history_.size()="
                 << bar_history_.size() << std::endl;
   }
   ```

2. **Check if bar_history_ is actually being populated**
   - Verify bars are being added in update()
   - Check if there's a max size limit being hit
   - Verify no clearing/reset happening

3. **Verify single instance**
   - Ensure feature_engine_ pointer is stable
   - Check no recreation during warmup→live transition

### Possible Fixes

**Option A**: If `update()` doesn't add to history:
```cpp
void UnifiedFeatureEngine::update(const Bar& bar) {
    // Add this at the beginning
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // ... rest of update logic ...
}
```

**Option B**: Force feature engine ready after warmup:
```cpp
// In warmup_strategy() after loop
if (!strategy_.is_features_ready()) {
    log_system("WARNING: Feature engine not ready after warmup!");
    log_system("  Forcing feature engine initialization...");
    // Force populate or reduce threshold
}
```

**Option C**: Reduce ready threshold temporarily:
```cpp
// In UnifiedFeatureEngine::is_ready()
// TEMPORARY FIX - reduce from 64 to 20 for testing
return bar_history_.size() >= 20;
```

---

## Workaround for Tonight

Since market closes in ~10 minutes (16:00 ET), and we need 64 live bars (64 minutes):

**Option 1**: Let it run overnight with paper trading
- After 64 minutes, features will be ready
- Will start generating real signals
- Monitor tomorrow morning

**Option 2**: Quick fix the threshold
- Change 64 to 1 in is_ready()
- Rebuild and restart
- Should work immediately

**Option 3**: Continue investigation tomorrow
- Market opens 9:30am ET (10:30pm KST)
- Fix before market opens
- Full day of trading to test

---

## Test Plan After Fix

1. **Rebuild** with fix applied
2. **Restart** live trading
3. **Verify** within first 3 bars:
   ```
   [timestamp] Signal: LONG/SHORT (p=0.XXXX, conf=0.YY) [DBG: ready=YES]
   ```
   - Probability should vary (not 0.5000)
   - Should see LONG/SHORT predictions
   - Confidence should be > 0.00

4. **Monitor** for first 10 minutes:
   - Signal probabilities varying
   - Trading decisions being made
   - Positions being taken

5. **Verify** first trade execution:
   - Order placement logged
   - Position updated
   - P&L tracking started

---

## Impact Assessment

### Current State
- ✅ Connection to Alpaca: WORKING
- ✅ Real-time data feed: WORKING
- ✅ Strategy warmup: WORKING
- ✅ Sample counter: WORKING
- ❌ Feature extraction: **BROKEN**
- ❌ Signal generation: **BROKEN** (stuck at neutral)
- ❌ Trade execution: **BLOCKED** (no signals to trade)

### Business Impact
- **Zero trades** being executed
- **Zero revenue** from live strategy
- **Opportunity cost**: Every minute market is open without trading
- **Testing blocked**: Cannot validate strategy performance live

### Technical Debt
- Feature engine initialization needs review
- Warmup→live transition needs better validation
- Need comprehensive integration tests
- Need feature engine unit tests

---

## Lessons Learned

1. **Separate code paths are dangerous**
   - `generate-signals` + `execute-trades` worked fine
   - `live-trade` had completely different bugs
   - Need unified code path or better testing

2. **on_bar() vs update() confusion**
   - Two similar-sounding functions with different purposes
   - Should rename for clarity:
     - `on_bar()` → `process_bar_and_update_state()`
     - `update()` → `update_performance_metrics()`

3. **Feature engine is stateful**
   - Maintains its own history separate from strategy
   - Needs explicit initialization/warmup
   - Current warmup doesn't properly initialize it

4. **Need better observability**
   - Should log feature engine ready status
   - Should log bar history sizes
   - Should have health checks at startup

---

## Related Issues

- **Duplicate library warnings** in linking
  - `ld: warning: ignoring duplicate libraries: libonline_backend.a, libonline_common.a, libonline_strategy.a`
  - Could cause issues with old code being linked
  - Should fix CMakeLists.txt to remove duplicates

---

## Files Attached

See `megadocs/LIVE_TRADING_BUG_SOURCE_MODULES.md` for complete source code of all related modules.

---

**Reporter**: Claude Code
**Assignee**: TBD
**Priority**: P0 (Blocks production)
**ETA**: Should be fixed before next market open (9:30am ET Tuesday)
