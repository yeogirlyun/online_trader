# Live Trading Root Cause - FOUND

**Date**: 2025-10-08 00:10 KST
**Status**: ROOT CAUSE IDENTIFIED
**Impact**: All live trading signals stuck at p=0.5000 (neutral)

---

## Executive Summary

After extensive debugging with detailed logging, the root cause has been identified:

**The EWRLS predictor returns `predicted_return=0` because it hasn't learned anything yet.**

---

## Debug Trail

### Initial Hypothesis (WRONG)
- Feature engine not accumulating bars during warmup
- Multiple instances of feature engine being created
- `is_ready()` returning false incorrectly

### Debug Findings (Step by Step)

**1. Feature Engine History Accumulation**
```
[UFE@0x14ee0fc00] update #1: before=0 → after_push=1 → final=1
...
[UFE@0x14ee0fc00] update #64: before=63 → after_push=64 → final=64 (ready=YES)
...
[UFE@0x14ee0fc00] update #900: before=899 → after_push=900 → final=900 (ready=YES)
```
✅ **WORKING**: Bar history accumulates correctly, same instance throughout

**2. Feature Engine Ready State**
```
[UFE@0x14ee0fc00] is_ready() check #1: bar_history_.size()=960 (need 64) → READY
```
✅ **WORKING**: Feature engine becomes ready after 64 bars, stays ready

**3. Feature Extraction**
```
[OES] extract_features #2: got 126 features from engine
```
✅ **WORKING**: Full feature vector (126 features) extracted successfully

**4. Prediction Generation** (THE PROBLEM)
```
[OES] generate_signal #1: predicted_return=0, confidence=0
[OES]   → base_prob=0.5
```
❌ **PROBLEM FOUND**: Predictor returns zero prediction!

---

## Root Cause Analysis

### Why Predicted Return is Zero

The `MultiHorizonPredictor` (EWRLS-based) works as follows:

1. **Initial State**: All weights initialized to zero
2. **Learning**: Weights update when `update()` is called with realized returns
3. **Prediction**: `predict(features)` returns weighted sum of features

**In Live Trading**:
- Warmup: Processes 960 bars via `on_bar()` → updates features ✅
- Warmup: Calls `generate_signal()` but doesn't call `predictor->update()` ❌
- Result: Predictor weights remain at zero
- Prediction: `dot(features, zero_weights) = 0`
- Probability: `0.5 + tanh(0 * 50) * 0.4 = 0.5`

**In Batch Mode (generate-signals + execute-trades)**:
- Both commands work independently
- `generate-signals`: Processes bars, generates signals, **performs multi-horizon updates**
- Multi-horizon tracking: Calls `track_prediction()` during warmup
- Horizon updates: When bar N+horizon arrives, calls `predictor->update(features, realized_return)`
- Result: Predictor learns from historical data ✅

### The Key Difference

**Batch Mode**:
```cpp
// In generate-signals warmup/processing:
for each bar:
    strategy.on_bar(bar)  // Updates features
    signal = strategy.generate_signal(bar)  // Tracks multi-horizon predictions
    // Later, when horizon completed:
    strategy.process_pending_updates(bar)  // Calls predictor->update()
```

**Live Trading**:
```cpp
// In warmup:
for each historical bar:
    strategy.on_bar(bar)  // Updates features only
    generate_signal(bar)  // Generates signal but predictor has zero weights
    // No predictor->update() during warmup!
```

---

## Why This Happens

### Missing Predictor Initialization in Live Trading

The live trading warmup should:
1. Load 960 historical bars ✅
2. Update feature engine ✅
3. **Train the predictor on historical returns** ❌ (NOT DONE)
4. Generate first real prediction ❌ (BLOCKED by #3)

### What's Missing

The predictor needs to learn from historical bar-to-bar returns:

```cpp
// What SHOULD happen in warmup:
for (size_t i = start_idx + 1; i < all_bars.size(); ++i) {
    // Update features
    strategy_.on_bar(all_bars[i]);

    // Extract features for previous bar
    auto features = strategy_.extract_features(all_bars[i-1]);

    // Calculate realized return from i-1 to i
    double realized_return = (all_bars[i].close - all_bars[i-1].close) / all_bars[i-1].close;

    // Train predictor
    predictor->update(features, realized_return);
}
```

---

## Solution Architecture (From User)

The user provided a comprehensive production-grade fix:

### 1. Sticky Ready State
```cpp
// In UnifiedFeatureEngine:
std::atomic<bool> sticky_ready_{false};

bool UnifiedFeatureEngine::is_ready() const {
    if (sticky_ready_.load()) return true;  // Once ready, always ready

    bool ready = bar_history_.size() >= 64;
    if (ready) {
        sticky_ready_.store(true);  // Flip to ready permanently
    }
    return ready;
}
```

### 2. Feature Engine Persistence
```cpp
// Ensure feature engine survives warmup→live transition
// Already done - single instance in OnlineEnsembleStrategy member
```

### 3. Deterministic Bar Processing Order
```cpp
// In on_new_bar():
void on_new_bar(const Bar& bar) {
    // 1. Update state FIRST
    strategy_.on_bar(bar);  // ✅ Already done

    // 2. Generate signal AFTER features ready
    auto signal = generate_signal(bar);  // ✅ Already done

    // 3. Trade decision
    auto decision = make_decision(signal, bar);

    // 4. Execute
    if (decision.should_trade) {
        execute_transition(decision);
    }
}
```

### 4. Predictor Warmup (THE FIX)
```cpp
void warmup_strategy() {
    // Load bars...

    // Warmup predictor with historical returns
    for (size_t i = start_idx + 1; i < all_bars.size(); ++i) {
        // Update features for bar i
        strategy_.on_bar(all_bars[i]);

        // Train predictor on i-1 → i return
        if (i > start_idx + 64) {  // After feature engine ready
            auto features = strategy_.extract_features(all_bars[i-1]);
            if (!features.empty()) {
                double realized_return =
                    (all_bars[i].close - all_bars[i-1].close) / all_bars[i-1].close;
                strategy_.train_predictor(features, realized_return);
            }
        }
    }

    log_system("✓ Predictor trained on " + std::to_string(warmup_count - 64) + " historical returns");
}
```

### 5. No Silent Neutral Fallback
```cpp
// In generate_signal():
if (!is_ready()) {
    output.metadata["skip_reason"] = "strategy_not_ready";
    return neutral_output;
}

if (features.empty()) {
    output.metadata["skip_reason"] = "features_not_extracted";
    return neutral_output;
}

if (config_.enable_volatility_filter && !has_sufficient_volatility()) {
    output.metadata["skip_reason"] = "insufficient_volatility";
    return neutral_output;
}

// ✅ All paths now set skip_reason in metadata
```

---

## Implementation Priority

### P0 (CRITICAL - Blocks Live Trading)
1. **Add predictor warmup** in `warmup_strategy()`
2. **Expose `train_predictor()` method** in OnlineEnsembleStrategy

### P1 (HIGH - Robustness)
3. **Sticky ready state** in UnifiedFeatureEngine
4. **Enhanced metadata** for all neutral paths

### P2 (MEDIUM - Safety)
5. **Health guardrails** (assert ready state after warmup)
6. **Metrics** (track predictor weight norms)

---

## Test Plan

### After Fix

1. **Rebuild** with predictor warmup
2. **Restart** live trading
3. **Expected Output** (first live bar):
```
[OES] generate_signal #1: predicted_return=0.0042, confidence=0.15
[OES]   → base_prob=0.58
[2025-10-08 00:15:00] Signal: LONG (p=0.5800, conf=0.16) [DBG: ready=YES]
```

4. **Verify**:
   - Predicted return ≠ 0
   - Base probability ≠ 0.5
   - Signal type varies (LONG/SHORT/NEUTRAL)
   - Confidence > 0

---

## Files to Modify

1. **src/cli/live_trade_command.cpp** (lines 247-311)
   - Add predictor training loop in `warmup_strategy()`

2. **src/strategy/online_ensemble_strategy.cpp** (lines 87-127)
   - Add public method `train_predictor(features, return)`
   - Enhance metadata for neutral paths

3. **src/features/unified_feature_engine.cpp** (lines 645-662)
   - Add sticky ready state

4. **include/strategy/online_ensemble_strategy.h**
   - Declare `train_predictor()` method

5. **include/features/unified_feature_engine.h**
   - Add `std::atomic<bool> sticky_ready_` member

---

## Lessons Learned

1. **Different code paths, different bugs**
   - Batch mode (generate-signals) worked fine
   - Live mode had completely different initialization

2. **EWRLS requires training data**
   - Not enough to just update features
   - Predictor needs (feature, realized_return) pairs to learn

3. **Warmup is not just for features**
   - Feature engine warmup: ✅ Working
   - Predictor warmup: ❌ Missing

4. **Debug logging is essential**
   - Without detailed logging, would never have found this
   - Instance pointers, feature counts, predictions all crucial

---

## Performance Impact

### Before Fix
- Predicted return: Always 0
- Base probability: Always 0.5
- Signals: 100% neutral
- Trades: 0

### After Fix
- Predicted return: Varies based on learned patterns
- Base probability: 0.05 to 0.95 range
- Signals: Mix of LONG/SHORT/NEUTRAL
- Trades: Active based on thresholds (0.55 buy, 0.45 sell)

---

**Reporter**: Claude Code
**Debug Session**: 2025-10-07 22:33 ET to 2025-10-08 00:10 KST (5.5 hours)
**Root Cause Found**: 2025-10-08 00:10 KST
**Status**: Ready for implementation

