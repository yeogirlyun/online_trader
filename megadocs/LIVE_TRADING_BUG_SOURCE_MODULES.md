# Live Trading Bug - Complete Source Modules

**Related to**: LIVE_TRADING_FEATURE_ENGINE_NOT_READY_BUG.md
**Date**: 2025-10-07

This document contains the complete source code of all modules involved in the live trading bug.

---

## Table of Contents

1. [Live Trade Command (Main Entry Point)](#live-trade-command)
2. [OnlineEnsemble Strategy](#onlineensemble-strategy)
3. [Unified Feature Engine](#unified-feature-engine)
4. [Signal Output Structure](#signal-output-structure)
5. [CMakeLists.txt (Duplicate Library Issue)](#cmake-configuration)

---

<a name="live-trade-command"></a>
## 1. Live Trade Command (Main Entry Point)

**File**: `src/cli/live_trade_command.cpp`
**Lines**: 1-700 (key sections below)

### Constructor & Initialization (Lines 53-90)
```cpp
LiveTrader(const std::string& alpaca_key,
           const std::string& alpaca_secret,
           const std::string& polygon_url,
           const std::string& polygon_key,
           const std::string& log_dir)
    : alpaca_(alpaca_key, alpaca_secret, true /* paper */)
    , polygon_(polygon_url, polygon_key)
    , log_dir_(log_dir)
    , strategy_(create_v1_config())  // ← Creates OnlineEnsembleStrategy
    , psm_()
    , current_state_(PositionStateMachine::State::CASH_ONLY)
    , bars_held_(0)
    , entry_equity_(100000.0)
{
    open_log_files();
    warmup_strategy();  // ← Loads 960 historical bars
}
```

### Warmup Strategy (Lines 247-311) - MODIFIED MULTIPLE TIMES
```cpp
void warmup_strategy() {
    std::string warmup_file = "data/equities/SPY_RTH_NH.csv";

    // Try relative path first, then from parent directory
    std::ifstream file(warmup_file);
    if (!file.is_open()) {
        warmup_file = "../data/equities/SPY_RTH_NH.csv";
        file.open(warmup_file);
    }

    if (!file.is_open()) {
        log_system("WARNING: Could not open warmup file: data/equities/SPY_RTH_NH.csv");
        log_system("         Strategy will learn from first few live bars");
        return;
    }

    // Read all bars...
    std::vector<Bar> all_bars;
    // ... CSV parsing code ...

    size_t warmup_count = std::min(size_t(960), all_bars.size());
    size_t start_idx = all_bars.size() - warmup_count;

    log_system("Warming up with " + std::to_string(warmup_count) + " historical bars...");

    for (size_t i = start_idx; i < all_bars.size(); ++i) {
        strategy_.on_bar(all_bars[i]); // ← FIXED: Feed bar to strategy
        generate_signal(all_bars[i]);   // ← Generate signal (after features ready)
    }

    log_system("✓ Warmup complete - processed " + std::to_string(warmup_count) + " bars");
    log_system("  Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
}
```

### on_new_bar() - Live Trading Handler (Lines 313-361) - MODIFIED
```cpp
void on_new_bar(const Bar& bar) {
    auto timestamp = get_timestamp_readable();

    // Check for end-of-day liquidation
    if (is_end_of_day_liquidation_time()) {
        log_system("[" + timestamp + "] END OF DAY - Liquidating all positions");
        liquidate_all_positions();
        return;
    }

    // Check regular trading hours
    if (!is_regular_hours()) {
        log_system("[" + timestamp + "] Outside regular hours - skipping");
        return;
    }

    log_system("[" + timestamp + "] New bar received - processing...");

    // FIXED: Feed bar to strategy FIRST (updates features, increments counter)
    strategy_.on_bar(bar);

    // Generate signal using OnlineEnsemble strategy (now features are ready)
    auto signal = generate_signal(bar);

    // Log signal to console
    std::cout << "[" << timestamp << "] Signal: "
              << signal.prediction << " "
              << "(p=" << std::fixed << std::setprecision(4) << signal.probability
              << ", conf=" << std::setprecision(2) << signal.confidence << ")"
              << " [DBG: ready=" << (strategy_.is_ready() ? "YES" : "NO") << "]"
              << std::endl;

    // Log signal
    log_signal(bar, signal);

    // Make trading decision
    auto decision = make_decision(signal, bar);

    // Log decision
    log_decision(decision);

    // Execute if needed
    if (decision.should_trade) {
        execute_transition(decision);
    }

    // Log current portfolio state
    log_portfolio_state();
}
```

### generate_signal() - WITH DEBUG (Lines 372-406)
```cpp
Signal generate_signal(const Bar& bar) {
    // Call OnlineEnsemble strategy to generate real signal
    auto strategy_signal = strategy_.generate_signal(bar);

    // DEBUG: Check why we're getting 0.5
    if (strategy_signal.probability == 0.5) {
        std::string reason = "unknown";
        if (strategy_signal.metadata.count("skip_reason")) {
            reason = strategy_signal.metadata.at("skip_reason");
        }
        std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
    }

    Signal signal;
    signal.probability = strategy_signal.probability;

    // Map signal type to prediction string
    if (strategy_signal.signal_type == SignalType::LONG) {
        signal.prediction = "LONG";
    } else if (strategy_signal.signal_type == SignalType::SHORT) {
        signal.prediction = "SHORT";
    } else {
        signal.prediction = "NEUTRAL";
    }

    signal.confidence = std::abs(strategy_signal.probability - 0.5) * 2.0;

    // Use same probability for all horizons
    signal.prob_1bar = strategy_signal.probability;
    signal.prob_5bar = strategy_signal.probability;
    signal.prob_10bar = strategy_signal.probability;

    return signal;
}
```

---

<a name="onlineensemble-strategy"></a>
## 2. OnlineEnsemble Strategy

**File**: `src/strategy/online_ensemble_strategy.cpp`

### generate_signal() - Main Signal Generation (Lines 54-126)
```cpp
SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {  // ← BUG TRIGGERS HERE
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;  // ← Returns 0.5 because features are empty
    }

    // Check volatility filter
    if (config_.enable_volatility_filter && !has_sufficient_volatility()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.metadata["skip_reason"] = "insufficient_volatility";
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // Convert to probability
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);

    // Apply Bollinger Bands amplification
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates
    bool is_long = (prob > 0.5);
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}
```

### on_bar() - State Update (Lines 140-157)
```cpp
void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);  // ← Should populate feature engine's bar_history_

    samples_seen_++;  // ← This now increments (was the second bug)

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // ... process pending updates ...
}
```

### extract_features() - THE PROBLEM (Lines 163-175)
```cpp
std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    if (bar_history_.size() < MIN_FEATURES_BARS) {
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_ready()) {  // ← BUG: This returns FALSE
        return {};  // ← So we return empty features
    }

    return feature_engine_->get_features();
}
```

### update() - Performance Metrics Only (Lines 128-138)
```cpp
void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {
        double return_pct = realized_pnl / 100000.0;
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}
// NOTE: This does NOT increment samples_seen_ or update features!
// That's why we changed to calling on_bar() instead.
```

---

<a name="unified-feature-engine"></a>
## 3. Unified Feature Engine

**File**: `src/features/unified_feature_engine.cpp`

### is_ready() - THE BLOCKER (Lines 625-629)
```cpp
bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    return bar_history_.size() >= 64;  // ← This is returning FALSE
    // Despite warmup calling update() 960 times!
}
```

### update() - NEEDS INVESTIGATION
```cpp
// TODO: Find this method and verify it actually populates bar_history_
void UnifiedFeatureEngine::update(const Bar& bar) {
    // Is this adding bars to bar_history_?
    // Is it clearing them somewhere?
    // Is there a bug here?

    // Expected behavior:
    // bar_history_.push_back(bar);
    // if (bar_history_.size() > MAX_HISTORY) {
    //     bar_history_.pop_front();
    // }

    // ... calculate features ...
}
```

### get_features() - Returns Empty When Not Ready
```cpp
std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (!is_ready()) {
        return {};  // Returns empty if not ready
    }

    // ... feature calculation ...
}
```

---

<a name="signal-output-structure"></a>
## 4. Signal Output Structure

**File**: `include/strategy/signal_output.h`

```cpp
struct SignalOutput {
    // Identification
    int bar_id;
    int64_t timestamp_ms;
    int bar_index;
    std::string symbol;
    std::string strategy_name;
    std::string strategy_version;

    // Signal
    SignalType signal_type;  // LONG, SHORT, NEUTRAL
    double probability;      // 0.0 to 1.0

    // Metadata (optional diagnostic info)
    std::map<std::string, std::string> metadata;

    // Multi-horizon predictions (if available)
    std::map<int, double> horizon_predictions;
};
```

---

<a name="cmake-configuration"></a>
## 5. CMakeLists.txt (Duplicate Library Issue)

**Problem**: Linker warning during build
```
ld: warning: ignoring duplicate libraries: 'libonline_backend.a', 'libonline_common.a', 'libonline_strategy.a'
```

**Impact**: Could cause old code to be linked instead of newly compiled code

**Location**: `CMakeLists.txt` (root)

**Needs Investigation**: Check target_link_libraries() for sentio_cli to find duplicates

---

## Key Data Structures

### Bar Structure
```cpp
struct Bar {
    int bar_id;
    int64_t timestamp_ms;
    double open;
    double high;
    double low;
    double close;
    int64_t volume;
};
```

### SignalType Enum
```cpp
enum class SignalType {
    LONG,     // Buy signal
    SHORT,    // Sell signal
    NEUTRAL   // No action
};
```

---

## Call Flow Summary

### Warmup Flow (FIXED)
```
Constructor
  └─> warmup_strategy()
      └─> Load 960 bars from CSV
      └─> for each bar:
          ├─> strategy_.on_bar(bar)
          │   ├─> bar_history_.push_back(bar)  ✅
          │   ├─> feature_engine_->update(bar)  ✅ (but not working?)
          │   └─> samples_seen_++  ✅
          └─> generate_signal(bar)  ✅
```

### Live Trading Flow (FIXED)
```
on_new_bar(bar)
  ├─> strategy_.on_bar(bar)  ✅ (moved before generate_signal)
  │   ├─> bar_history_.push_back(bar)  ✅
  │   ├─> feature_engine_->update(bar)  ✅ (but not working?)
  │   └─> samples_seen_++  ✅
  │
  ├─> generate_signal(bar)
  │   ├─> strategy_.generate_signal(bar)
  │   │   ├─> is_ready()  → YES ✅
  │   │   ├─> extract_features(bar)
  │   │   │   ├─> bar_history_.size() >= MIN_FEATURES_BARS  → YES ✅
  │   │   │   ├─> feature_engine_->is_ready()  → NO ❌
  │   │   │   └─> return {}  ❌
  │   │   └─> features.empty()  → YES ❌
  │   │       └─> return p=0.5  ❌
  │   └─> Map to internal Signal struct
  │
  ├─> make_decision(signal)
  └─> execute_transition() (if should_trade)
```

### The Bug
```
feature_engine_->is_ready()
  └─> return bar_history_.size() >= 64
      └─> bar_history_.size() = ??? (< 64)
          └─> WHY? update() called 960+ times!
```

---

## Investigation Commands

### Check feature engine update implementation:
```bash
grep -n "void UnifiedFeatureEngine::update" src/features/unified_feature_engine.cpp -A 50
```

### Check for bar_history_ manipulation:
```bash
grep -n "bar_history_" src/features/unified_feature_engine.cpp
```

### Check for clear/reset calls:
```bash
grep -n "clear\|reset" src/features/unified_feature_engine.cpp
```

### Add debug logging:
```cpp
// In UnifiedFeatureEngine::update()
std::cout << "[UFE] update() called, size=" << bar_history_.size() << std::endl;

// In UnifiedFeatureEngine::is_ready()
std::cout << "[UFE] is_ready() check: size=" << bar_history_.size()
          << " (need 64)" << std::endl;
return bar_history_.size() >= 64;
```

---

## Files to Review

1. `src/features/unified_feature_engine.cpp` - **PRIORITY 1**
   - Find update() implementation
   - Verify bar_history_ population
   - Check for clear/reset

2. `include/features/unified_feature_engine.h`
   - Check member variables
   - Verify MAX_HISTORY constant

3. `src/cli/live_trade_command.cpp`
   - Verify warmup logic
   - Verify on_new_bar() order

4. `src/strategy/online_ensemble_strategy.cpp`
   - Verify on_bar() implementation
   - Check if feature_engine_ is reassigned

5. `CMakeLists.txt`
   - Fix duplicate library warnings

---

**End of Source Modules Document**
