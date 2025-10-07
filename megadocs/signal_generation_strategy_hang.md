# Signal Generation Strategy Hang Bug Report

**Generated**: 2025-10-07 15:19:58
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (7 files)
**Description**: Detailed analysis of signal generation hanging during OnlineEnsemble strategy processing for datasets >500 bars. CSV loading is fixed, hang is in strategy loop.

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [data/tmp/SIGNAL_GENERATION_STRATEGY_HANG_BUG.md](#file-1)
2. [include/cli/ensemble_workflow_command.h](#file-2)
3. [include/common/types.h](#file-3)
4. [include/strategy/online_ensemble_strategy.h](#file-4)
5. [include/strategy/signal_output.h](#file-5)
6. [src/cli/generate_signals_command.cpp](#file-6)
7. [src/strategy/online_ensemble_strategy.cpp](#file-7)

---

## 📄 **FILE 1 of 7**: data/tmp/SIGNAL_GENERATION_STRATEGY_HANG_BUG.md

**File Information**:
- **Path**: `data/tmp/SIGNAL_GENERATION_STRATEGY_HANG_BUG.md`

- **Size**: 462 lines
- **Modified**: 2025-10-07 15:19:46

- **Type**: .md

```text
# Bug Report: Signal Generation Hangs During Strategy Processing (>500 Bars)

**Date:** October 7, 2025
**Severity:** Critical
**Status:** Unresolved
**Reporter:** System Analysis (Post-CSV Fix)

---

## Summary

After fixing the CSV reader performance issues, signal generation **STILL HANGS** on datasets larger than ~500 bars. The CSV loading now completes successfully in <1 second with progress reporting, but the program hangs during the **signal generation loop** in `generate_signals_command.cpp`.

The hang occurs **after** CSV loading completes but **during** the strategy's `on_bar()` or `generate_signal()` calls.

---

## Critical Finding: CSV Reader is FIXED ✅

The CSV reader performance fix is **successful**:

**Before Fix:**
- 500 bars: ~15 seconds
- 9,600 bars: Hung indefinitely (>5 minutes)

**After Fix:**
- 500 bars: <1 second with progress output
- 9,600 bars: CSV loads in <1 second, **then hangs in strategy loop**

**Log Evidence:**
```
[read_csv_data] Loading CSV: ../data/equities/SPY_test_small.csv (symbol: SPY)
[read_csv_data] Completed: 499 bars loaded from ../data/equities/SPY_test_small.csv
Loaded 499 bars
Processing range: 0 to 499

Generating signals...
  Progress: 0.0% (0/499)
  ...
  Progress: 96.2% (480/499)
Generated 499 signals
```

This proves CSV loading works perfectly. The hang is **downstream in the strategy**.

---

## New Hang Location: Strategy Signal Generation Loop

### Working Case (500 bars)
```bash
./sentio_cli generate-signals \
  --data ../data/equities/SPY_test_small.csv \
  --output ../data/tmp/spy_test_signals.jsonl \
  --verbose
```

**Output:**
```
=== OnlineEnsemble Signal Generation ===
Data: ../data/equities/SPY_test_small.csv
Output: ../data/tmp/spy_test_signals.jsonl
Warmup: 100 bars

Loading market data...
[read_csv_data] Loading CSV: ../data/equities/SPY_test_small.csv (symbol: SPY)
[read_csv_data] Completed: 499 bars loaded from ../data/equities/SPY_test_small.csv
Loaded 499 bars
Processing range: 0 to 499

Generating signals...
  Progress: 0.0% (0/499)
  Progress: 4.8% (24/499)
  ...
  Progress: 96.2% (480/499)
Generated 499 signals
✅ Signals saved successfully!
```

**Time:** ~3 seconds total
**Status:** ✅ Works perfectly

### Hanging Case (9,600 bars)
```bash
./sentio_cli generate-signals \
  --data ../data/equities/SPY_20blocks.csv \
  --output ../data/tmp/spy_20block_signals.jsonl \
  --verbose
```

**Last Output Before Hang:**
```
=== OnlineEnsemble Signal Generation ===
Data: ../data/equities/SPY_20blocks.csv
Output: ../data/tmp/spy_20block_signals.jsonl
Warmup: 100 bars

Loading market data...
[read_csv_data] Loading CSV: ../data/equities/SPY_20blocks.csv (symbol: SPY)
[read_csv_data] Parsed 8192 rows...
[read_csv_data] Completed: 9600 bars loaded from ../data/equities/SPY_20blocks.csv
Loaded 9600 bars
Processing range: 0 to 9600

Generating signals...
[HANGS HERE - NO PROGRESS OUTPUT]
```

**Time:** CSV loads in <1 second, then hangs forever (>60 seconds)
**Status:** ❌ Hangs during signal generation loop

---

## Analysis: The Hang is in OnlineEnsembleStrategy

### Code Path That Hangs

**File:** `src/cli/generate_signals_command.cpp` (lines 68-99)

```cpp
std::cout << "Generating signals...\n";
for (int i = start_bar; i < end_bar; ++i) {
    // Update strategy with bar (processes pending updates)
    strategy.on_bar(bars[i]);  // ← SUSPECT #1: May hang here

    // Generate signal from strategy
    sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);  // ← SUSPECT #2: May hang here

    // Convert to CLI output format
    SignalOutput output;
    output.bar_id = strategy_signal.bar_id;
    output.timestamp_ms = strategy_signal.timestamp_ms;
    // ... (simple field copies)

    signals.push_back(output);

    // Progress reporting
    if (verbose && progress_interval > 0 && (i - start_bar) % progress_interval == 0) {
        double pct = 100.0 * (i - start_bar) / (end_bar - start_bar);
        std::cout << "  Progress: " << std::fixed << std::setprecision(1)
                 << pct << "% (" << (i - start_bar) << "/" << (end_bar - start_bar) << ")\n";
    }
}
```

### Key Observations

1. **Progress reporting exists** (lines 94-98) but **never prints** for large datasets
   - This means the loop **never completes even one iteration** beyond bar ~0-24
   - OR the loop is stuck in `on_bar()` or `generate_signal()` call

2. **Small dataset (500 bars):**
   - Progress prints every 24 bars (5% increments)
   - Loop completes 499 iterations in ~3 seconds
   - Average: ~6ms per iteration

3. **Large dataset (9,600 bars):**
   - Progress never prints (not even 0.0%)
   - Loop never completes (>60 seconds timeout)
   - Expected time at 6ms/iteration: ~58 seconds
   - **Actual time: INFINITE**

### Hypothesis: Quadratic Complexity in Strategy

**Possible O(n²) behavior in OnlineEnsembleStrategy:**

1. **Hypothesis 1: Data accumulation in `on_bar()`**
   - Strategy may be accumulating all bars in memory
   - Each `on_bar()` processes all historical bars → O(n²)
   - 500 bars: 250k operations (fast)
   - 9,600 bars: 92 million operations (slow/hangs)

2. **Hypothesis 2: Feature calculation explosion**
   - Strategy calculates 126 features per bar
   - Feature calculation may loop over all history
   - Each bar triggers recalculation of all features → O(n²)

3. **Hypothesis 3: Model retraining on every bar**
   - EWRLS predictor may retrain on full history each bar
   - Instead of online update, may be batch training → O(n²) or worse

4. **Hypothesis 4: Memory allocation thrashing**
   - Large feature matrices being reallocated repeatedly
   - No reserve() on internal vectors
   - Each `on_bar()` triggers realloc → O(n²) memory ops

---

## Evidence: Progress Interval Calculation

**File:** `src/cli/generate_signals_command.cpp` (line 65)

```cpp
int progress_interval = (end_bar - start_bar) / 20;  // 5% increments
```

**For 9,600 bars:**
```
progress_interval = 9600 / 20 = 480
```

**Expected Progress Output:**
```
  Progress: 0.0% (0/9600)
  Progress: 5.0% (480/9600)
  Progress: 10.0% (960/9600)
  ...
```

**Actual Progress Output:**
```
(NOTHING - hang occurs before first progress report)
```

**Conclusion:** The loop **never reaches iteration 480**, meaning it hangs in the first ~480 iterations, likely in the first few iterations of `on_bar()` or `generate_signal()`.

---

## Relevant Source Modules

### Primary Suspects

1. **`src/strategy/online_ensemble_strategy.cpp`**
   - `on_bar()` method - Updates strategy state per bar
   - `generate_signal()` method - Generates trading signal
   - **CRITICAL:** May have O(n²) complexity

2. **`include/strategy/online_ensemble_strategy.h`**
   - Strategy configuration
   - Internal state variables
   - Feature cache, predictor state

3. **`src/learning/ewrls_predictor.cpp`** (if exists)
   - EWRLS online learning implementation
   - Matrix operations
   - May be doing full retraining instead of online update

4. **`src/features/unified_feature_engine.cpp`** (if exists)
   - 126-feature calculation
   - May be recalculating all historical features

### Secondary Modules

5. **`src/cli/generate_signals_command.cpp`** (lines 68-99)
   - Main signal generation loop
   - Already has progress reporting
   - Need to add debug output **inside** loop

6. **`include/strategy/signal_output.h`**
   - SignalOutput struct definition
   - Metadata handling

---

## Diagnostic Steps Needed

### 1. Add Fine-Grained Debug Logging

**Modify `src/cli/generate_signals_command.cpp` loop:**

```cpp
for (int i = start_bar; i < end_bar; ++i) {
    // DEBUG: Log entry to each iteration
    if (i % 100 == 0) {
        std::cerr << "[DEBUG] Processing bar " << i << "/" << end_bar << std::endl;
    }

    // DEBUG: Log before on_bar
    auto t1 = std::chrono::high_resolution_clock::now();
    strategy.on_bar(bars[i]);
    auto t2 = std::chrono::high_resolution_clock::now();

    // DEBUG: Log before generate_signal
    sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);
    auto t3 = std::chrono::high_resolution_clock::now();

    // DEBUG: Log timing
    if (i % 100 == 0) {
        auto on_bar_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        auto gen_signal_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
        std::cerr << "  on_bar: " << on_bar_ms << "ms, generate_signal: " << gen_signal_ms << "ms" << std::endl;
    }

    // ... rest of loop
}
```

### 2. Profile OnlineEnsembleStrategy

**Check for:**
- Memory growth over time (memory leak or accumulation)
- CPU usage pattern (constant vs increasing)
- Disk I/O (unexpected file access)

### 3. Test Intermediate Dataset Sizes

**Binary search for hang threshold:**
- 500 bars: ✅ Works (3 seconds)
- 1,000 bars: ⏱️ Test needed
- 2,000 bars: ⏱️ Test needed
- 4,000 bars: ⏱️ Test needed
- 9,600 bars: ❌ Hangs (>60 seconds)

**Expected behavior if O(n²):**
- 500 bars: 3 seconds
- 1,000 bars: 12 seconds (4x slower)
- 2,000 bars: 48 seconds (16x slower)
- 4,000 bars: 192 seconds (64x slower) ← Likely hangs here
- 9,600 bars: 1,105 seconds (368x slower) ← Definitely hangs

---

## Performance Calculation

**Assumption: O(n²) complexity in strategy**

| Dataset Size | Expected Time (O(n²)) | Status |
|--------------|----------------------|--------|
| 500 bars | 3 seconds | ✅ Works |
| 1,000 bars | 12 seconds | ⏱️ Unknown |
| 2,000 bars | 48 seconds | ⏱️ Unknown |
| 5,000 bars | 300 seconds (5 min) | ❌ Likely hangs |
| 9,600 bars | 1,105 seconds (18 min) | ❌ Hangs |
| 48,000 bars | 27,648 seconds (7.7 hrs) | ❌ Hangs |

**If O(n) complexity (desired):**

| Dataset Size | Expected Time (O(n)) | Status |
|--------------|---------------------|--------|
| 500 bars | 3 seconds | ✅ Works |
| 9,600 bars | 58 seconds | ❌ Should work but hangs |
| 48,000 bars | 288 seconds (4.8 min) | ❌ Should work but hangs |

---

## Recommended Fixes

### Immediate (Debug)
1. Add iteration-level logging to pinpoint exact hang location
2. Add timing measurements to `on_bar()` and `generate_signal()`
3. Test with 1k, 2k, 4k bars to find performance curve

### Short-Term (Strategy Optimization)
1. **Fix O(n²) complexity in OnlineEnsembleStrategy:**
   - Ensure `on_bar()` is truly online (O(1) per bar)
   - Ensure feature calculation is incremental
   - Ensure EWRLS uses online update, not batch retrain

2. **Add internal progress logging to strategy:**
   ```cpp
   void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
       if (bar_count_ % 1000 == 0) {
           std::cerr << "[Strategy] Processed " << bar_count_ << " bars" << std::endl;
       }
       // ... strategy logic
   }
   ```

3. **Reserve capacity for internal vectors**

### Long-Term (Architecture)
1. Implement batch processing with checkpointing
2. Add timeout/kill-switch for long-running operations
3. Profile-guided optimization of hot paths

---

## Test Commands

### Small (Control)
```bash
./sentio_cli generate-signals \
  --data ../data/equities/SPY_test_small.csv \
  --output /tmp/signals_500.jsonl \
  --verbose
```

### Medium (Threshold Testing)
```bash
# Create test datasets
head -1001 ../data/equities/SPY_20blocks.csv > SPY_1k.csv
head -2001 ../data/equities/SPY_20blocks.csv > SPY_2k.csv
head -4001 ../data/equities/SPY_20blocks.csv > SPY_4k.csv

# Test each
time ./sentio_cli generate-signals --data SPY_1k.csv --output /tmp/sig_1k.jsonl --verbose
time ./sentio_cli generate-signals --data SPY_2k.csv --output /tmp/sig_2k.jsonl --verbose
time ./sentio_cli generate-signals --data SPY_4k.csv --output /tmp/sig_4k.jsonl --verbose
```

### Large (Reproduce Hang)
```bash
time ./sentio_cli generate-signals \
  --data ../data/equities/SPY_20blocks.csv \
  --output /tmp/signals_9600.jsonl \
  --verbose
```

---

## Related Files - Complete List

### Core Signal Generation
1. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/generate_signals_command.cpp`**
   - Lines 68-99: Signal generation loop (hang location)
   - Line 70: `strategy.on_bar(bars[i])` call
   - Line 73: `strategy.generate_signal(bars[i])` call

### Strategy Implementation (PRIMARY SUSPECTS)
2. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/online_ensemble_strategy.cpp`**
   - `on_bar()` implementation
   - `generate_signal()` implementation
   - Feature calculation logic
   - Predictor update logic

3. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/online_ensemble_strategy.h`**
   - Strategy state variables
   - Configuration struct
   - Internal caches

### Learning Components
4. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/learning/ewrls_predictor.cpp`** (if exists)
   - EWRLS online update
   - Matrix operations
   - Potential O(n²) operations

5. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/learning/ewrls_predictor.h`** (if exists)
   - Predictor state
   - Matrix dimensions

### Feature Engineering
6. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/features/unified_feature_engine.cpp`** (if exists)
   - 126-feature calculation
   - Historical data access patterns
   - Potential O(n²) loops

7. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/features/unified_feature_engine.h`** (if exists)
   - Feature cache
   - Rolling window logic

### Support Files
8. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/signal_output.h`**
   - SignalOutput struct
   - Metadata fields

9. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/common/types.h`**
   - Bar struct
   - Common data types

---

## Next Steps

1. ✅ CSV reader fixed - hang is **NOT** in data loading
2. ⏱️ Add debug logging to signal generation loop
3. ⏱️ Profile OnlineEnsembleStrategy with 1k-4k bar datasets
4. ⏱️ Identify O(n²) bottleneck in strategy code
5. ⏱️ Fix complexity issue or add batching/checkpointing

---

**End of Bug Report**

```

## 📄 **FILE 2 of 7**: include/cli/ensemble_workflow_command.h

**File Information**:
- **Path**: `include/cli/ensemble_workflow_command.h`

- **Size**: 263 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "cli/command_interface.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Complete workflow command for OnlineEnsemble experiments
 *
 * Workflow:
 * 1. generate-signals: Create signal file from market data
 * 2. execute-trades: Simulate trading with portfolio manager
 * 3. analyze: Generate performance reports
 * 4. run-all: Execute complete workflow
 */
class EnsembleWorkflowCommand : public Command {
public:
    std::string get_name() const override { return "ensemble"; }
    std::string get_description() const override {
        return "OnlineEnsemble workflow: generate signals, execute trades, analyze results";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    // Sub-commands
    int generate_signals(const std::vector<std::string>& args);
    int execute_trades(const std::vector<std::string>& args);
    int analyze(const std::vector<std::string>& args);
    int run_all(const std::vector<std::string>& args);

    // Configuration structures
    struct SignalGenConfig {
        std::string data_path;
        std::string output_path;
        int warmup_bars = 100;
        int start_bar = 0;
        int end_bar = -1;  // -1 = all

        // Strategy config
        std::vector<int> horizons = {1, 5, 10};
        std::vector<double> weights = {0.3, 0.5, 0.2};
        double lambda = 0.995;
        bool verbose = false;
    };

    struct TradeExecConfig {
        std::string signal_path;
        std::string data_path;
        std::string output_path;

        double starting_capital = 100000.0;
        double buy_threshold = 0.53;
        double sell_threshold = 0.47;
        double kelly_fraction = 0.25;
        bool enable_kelly = true;
        bool verbose = false;
    };

    struct AnalysisConfig {
        std::string trades_path;
        std::string output_path;
        bool show_detailed = true;
        bool show_trades = false;
        bool export_csv = false;
        bool export_json = true;
    };

    // Parsing helpers
    SignalGenConfig parse_signal_config(const std::vector<std::string>& args);
    TradeExecConfig parse_trade_config(const std::vector<std::string>& args);
    AnalysisConfig parse_analysis_config(const std::vector<std::string>& args);
};

/**
 * @brief Signal generation command (standalone)
 */
class GenerateSignalsCommand : public Command {
public:
    std::string get_name() const override { return "generate-signals"; }
    std::string get_description() const override {
        return "Generate OnlineEnsemble signals from market data";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct SignalOutput {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        double probability;
        double confidence;
        SignalType signal_type;
        int prediction_horizon;

        // Multi-horizon data
        std::map<int, double> horizon_predictions;
        double ensemble_agreement;
    };

    void save_signals_jsonl(const std::vector<SignalOutput>& signals,
                           const std::string& path);
    void save_signals_csv(const std::vector<SignalOutput>& signals,
                         const std::string& path);
};

/**
 * @brief Trade execution command (standalone)
 */
class ExecuteTradesCommand : public Command {
public:
    std::string get_name() const override { return "execute-trades"; }
    std::string get_description() const override {
        return "Execute trades from signal file and generate portfolio history";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

public:
    struct TradeRecord {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        TradeAction action;
        double quantity;
        double price;
        double trade_value;
        double fees;
        std::string reason;

        // Portfolio state after trade
        double cash_balance;
        double portfolio_value;
        double position_quantity;
        double position_avg_price;
    };

    struct PortfolioHistory {
        std::vector<TradeRecord> trades;
        std::vector<double> equity_curve;
        std::vector<double> drawdown_curve;

        double starting_capital;
        double final_capital;
        double max_drawdown;
        int total_trades;
        int winning_trades;
    };

    void save_trades_jsonl(const PortfolioHistory& history, const std::string& path);
    void save_trades_csv(const PortfolioHistory& history, const std::string& path);
    void save_equity_curve(const PortfolioHistory& history, const std::string& path);

    // PSM helper functions
    static double get_position_value(const PortfolioState& portfolio, double current_price);
    static std::map<std::string, double> calculate_target_positions(
        PositionStateMachine::State state,
        double total_capital,
        double price);

    // Multi-instrument versions (use correct price per instrument)
    static double get_position_value_multi(
        const PortfolioState& portfolio,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index);

    // Symbol mapping for PSM (to support both QQQ and SPY)
    struct SymbolMap {
        std::string base;      // QQQ or SPY
        std::string bull_3x;   // TQQQ or SPXL
        std::string bear_1x;   // PSQ or SH
        std::string bear_nx;   // SQQQ (-3x) or SDS (-2x)
    };

    static std::map<std::string, double> calculate_target_positions_multi(
        PositionStateMachine::State state,
        double total_capital,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index,
        const SymbolMap& symbol_map);
};

/**
 * @brief Analysis and reporting command (standalone)
 */
class AnalyzeTradesCommand : public Command {
public:
    std::string get_name() const override { return "analyze-trades"; }
    std::string get_description() const override {
        return "Analyze trade history and generate performance reports";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct PerformanceReport {
        // Returns
        double total_return_pct;
        double annualized_return;
        double monthly_return;
        double daily_return;

        // Risk metrics
        double max_drawdown;
        double avg_drawdown;
        double volatility;
        double downside_deviation;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;

        // Trading metrics
        int total_trades;
        int winning_trades;
        int losing_trades;
        double win_rate;
        double profit_factor;
        double avg_win;
        double avg_loss;
        double avg_trade;
        double largest_win;
        double largest_loss;

        // Position metrics
        double avg_holding_period;
        double max_holding_period;
        int total_long_trades;
        int total_short_trades;

        // Kelly metrics
        double kelly_criterion;
        double avg_position_size;
        double max_position_size;

        // Time analysis
        int trading_days;
        int bars_traded;
        std::string start_date;
        std::string end_date;
    };

    PerformanceReport calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades);
    void print_report(const PerformanceReport& report);
    void save_report_json(const PerformanceReport& report, const std::string& path);
    void generate_plots(const std::vector<double>& equity_curve, const std::string& output_dir);
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 3 of 7**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`

- **Size**: 113 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// System Constants
// -----------------------------------------------------------------------------

/// Standard block size for backtesting and signal processing
/// One block represents approximately 8 hours of trading (480 minutes)
/// This constant ensures consistency across strattest, trade, and audit commands
static constexpr size_t STANDARD_BLOCK_SIZE = 480;

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp_ms and symbol at load time
    uint64_t bar_id = 0;
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
    // Derived fields for traceability/debugging (filled by loader)
    uint32_t sequence_num = 0;   // Position in original dataset
    uint16_t block_num = 0;      // STANDARD_BLOCK_SIZE partition index
    std::string date_str;        // e.g. "2025-09-09" for human-readable logs
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio

```

## 📄 **FILE 4 of 7**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 182 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Volatility filter (prevent trading in flat markets)
        bool enable_volatility_filter = false;  // Enable ATR-based volatility filter
        int atr_period = 20;                    // ATR calculation period
        double min_atr_ratio = 0.001;           // Minimum ATR as % of price (0.1%)

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::vector<double> features;
        double entry_price;
        bool is_long;
    };
    std::map<int, HorizonPrediction> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Private methods
    std::vector<double> extract_features(const Bar& current_bar);
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Volatility filter
    double calculate_atr(int period) const;
    bool has_sufficient_volatility() const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

## 📄 **FILE 5 of 7**: include/strategy/signal_output.h

**File Information**:
- **Path**: `include/strategy/signal_output.h`

- **Size**: 39 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

enum class SignalType {
    NEUTRAL,
    LONG,
    SHORT
};

struct SignalOutput {
    // Core fields
    uint64_t bar_id = 0;           
    int64_t timestamp_ms = 0;
    int bar_index = 0;             
    std::string symbol;
    double probability = 0.0;       
    SignalType signal_type = SignalType::NEUTRAL;
    std::string strategy_name;
    std::string strategy_version;
    
    // NEW: Multi-bar prediction fields
    int prediction_horizon = 1;        // How many bars ahead this predicts (default=1 for backward compat)
    uint64_t target_bar_id = 0;       // The bar this prediction targets
    bool requires_hold = false;        // Signal requires minimum hold period
    int signal_generation_interval = 1; // How often signals are generated
    
    std::map<std::string, std::string> metadata;

    std::string to_json() const;
    std::string to_csv() const;
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio
```

## 📄 **FILE 6 of 7**: src/cli/generate_signals_command.cpp

**File Information**:
- **Path**: `src/cli/generate_signals_command.cpp`

- **Size**: 236 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "cli/ensemble_workflow_command.h"
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace sentio {
namespace cli {

int GenerateSignalsCommand::execute(const std::vector<std::string>& args) {
    using namespace sentio;

    // Parse arguments
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "signals.jsonl");
    int warmup_bars = std::stoi(get_arg(args, "--warmup", "100"));
    int start_bar = std::stoi(get_arg(args, "--start", "0"));
    int end_bar = std::stoi(get_arg(args, "--end", "-1"));
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    if (data_path.empty()) {
        std::cerr << "Error: --data is required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Signal Generation ===\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Warmup: " << warmup_bars << " bars\n\n";

    // Load market data
    std::cout << "Loading market data...\n";
    auto bars = utils::read_csv_data(data_path);
    if (bars.empty()) {
        std::cerr << "Error: Could not load data from " << data_path << "\n";
        return 1;
    }

    if (end_bar < 0 || end_bar > static_cast<int>(bars.size())) {
        end_bar = static_cast<int>(bars.size());
    }

    std::cout << "Loaded " << bars.size() << " bars\n";
    std::cout << "Processing range: " << start_bar << " to " << end_bar << "\n\n";

    // Create OnlineEnsembleStrategy
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.warmup_samples = warmup_bars;
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.ewrls_lambda = 0.995;
    config.buy_threshold = 0.53;
    config.sell_threshold = 0.47;
    config.enable_threshold_calibration = false;  // Disable auto-calibration
    config.enable_adaptive_learning = true;

    OnlineEnsembleStrategy strategy(config);

    // Generate signals
    std::vector<SignalOutput> signals;
    int progress_interval = (end_bar - start_bar) / 20;  // 5% increments

    std::cout << "Generating signals...\n";
    for (int i = start_bar; i < end_bar; ++i) {
        // Update strategy with bar (processes pending updates)
        strategy.on_bar(bars[i]);

        // Generate signal from strategy
        sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);

        // Convert to CLI output format
        SignalOutput output;
        output.bar_id = strategy_signal.bar_id;
        output.timestamp_ms = strategy_signal.timestamp_ms;
        output.bar_index = strategy_signal.bar_index;
        output.symbol = strategy_signal.symbol;
        output.probability = strategy_signal.probability;
        output.signal_type = strategy_signal.signal_type;
        output.prediction_horizon = strategy_signal.prediction_horizon;

        // Calculate ensemble agreement from metadata
        output.ensemble_agreement = 0.0;
        if (strategy_signal.metadata.count("ensemble_agreement")) {
            output.ensemble_agreement = std::stod(strategy_signal.metadata.at("ensemble_agreement"));
        }

        signals.push_back(output);

        // Progress reporting
        if (verbose && progress_interval > 0 && (i - start_bar) % progress_interval == 0) {
            double pct = 100.0 * (i - start_bar) / (end_bar - start_bar);
            std::cout << "  Progress: " << std::fixed << std::setprecision(1)
                     << pct << "% (" << (i - start_bar) << "/" << (end_bar - start_bar) << ")\n";
        }
    }

    std::cout << "Generated " << signals.size() << " signals\n\n";

    // Save signals
    std::cout << "Saving signals to " << output_path << "...\n";
    if (csv_output) {
        save_signals_csv(signals, output_path);
    } else {
        save_signals_jsonl(signals, output_path);
    }

    // Print summary
    int long_signals = 0, short_signals = 0, neutral_signals = 0;
    for (const auto& sig : signals) {
        if (sig.signal_type == SignalType::LONG) long_signals++;
        else if (sig.signal_type == SignalType::SHORT) short_signals++;
        else neutral_signals++;
    }

    std::cout << "\n=== Signal Summary ===\n";
    std::cout << "Total signals: " << signals.size() << "\n";
    std::cout << "Long signals:  " << long_signals << " (" << (100.0 * long_signals / signals.size()) << "%)\n";
    std::cout << "Short signals: " << short_signals << " (" << (100.0 * short_signals / signals.size()) << "%)\n";
    std::cout << "Neutral:       " << neutral_signals << " (" << (100.0 * neutral_signals / signals.size()) << "%)\n";

    // Get strategy performance - not implemented in stub
    // auto metrics = strategy.get_performance_metrics();
    std::cout << "\n=== Strategy Metrics ===\n";
    std::cout << "Strategy: OnlineEnsemble (stub version)\n";
    std::cout << "Note: Full metrics available after execute-trades and analyze-trades\n";

    std::cout << "\n✅ Signals saved successfully!\n";
    return 0;
}

void GenerateSignalsCommand::save_signals_jsonl(const std::vector<SignalOutput>& signals,
                                               const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& sig : signals) {
        // Convert signal_type enum to string
        std::string signal_type_str;
        switch (sig.signal_type) {
            case SignalType::LONG: signal_type_str = "LONG"; break;
            case SignalType::SHORT: signal_type_str = "SHORT"; break;
            default: signal_type_str = "NEUTRAL"; break;
        }

        // Create JSON line
        out << "{"
            << "\"bar_id\":" << sig.bar_id << ","
            << "\"timestamp_ms\":" << sig.timestamp_ms << ","
            << "\"bar_index\":" << sig.bar_index << ","
            << "\"symbol\":\"" << sig.symbol << "\","
            << "\"probability\":" << std::fixed << std::setprecision(6) << sig.probability << ","
            << "\"signal_type\":\"" << signal_type_str << "\","
            << "\"prediction_horizon\":" << sig.prediction_horizon << ","
            << "\"ensemble_agreement\":" << sig.ensemble_agreement
            << "}\n";
    }
}

void GenerateSignalsCommand::save_signals_csv(const std::vector<SignalOutput>& signals,
                                             const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,probability,confidence,signal_type,prediction_horizon,ensemble_agreement\n";

    // Data
    for (const auto& sig : signals) {
        out << sig.bar_id << ","
            << sig.timestamp_ms << ","
            << sig.bar_index << ","
            << sig.symbol << ","
            << std::fixed << std::setprecision(6) << sig.probability << ","
            // << sig.confidence << ","  // Field not in SignalOutput
            << static_cast<int>(sig.signal_type) << ","
            << sig.prediction_horizon << ","
            << sig.ensemble_agreement << "\n";
    }
}

void GenerateSignalsCommand::show_help() const {
    std::cout << R"(
Generate OnlineEnsemble Signals
================================

Generate trading signals from market data using OnlineEnsemble strategy.

USAGE:
    sentio_cli generate-signals --data <path> [OPTIONS]

REQUIRED:
    --data <path>              Path to market data file (CSV or binary)

OPTIONS:
    --output <path>            Output signal file (default: signals.jsonl)
    --warmup <bars>            Warmup period before trading (default: 100)
    --start <bar>              Start bar index (default: 0)
    --end <bar>                End bar index (default: all)
    --csv                      Output in CSV format instead of JSONL
    --verbose, -v              Show progress updates

EXAMPLES:
    # Generate signals from data
    sentio_cli generate-signals --data data/SPY_1min.csv --output signals.jsonl

    # With custom warmup and range
    sentio_cli generate-signals --data data/QQQ.bin --warmup 200 --start 1000 --end 5000

    # CSV output with verbose progress
    sentio_cli generate-signals --data data/futures.bin --csv --verbose

OUTPUT FORMAT (JSONL):
    Each line contains:
    {
        "bar_id": 12345,
        "timestamp_ms": 1609459200000,
        "probability": 0.6234,
        "confidence": 0.82,
        "signal_type": "1",  // 0=NEUTRAL, 1=LONG, 2=SHORT
        "prediction_horizon": 5,
        "ensemble_agreement": 0.75
    }

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 7 of 7**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 532 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // Initialize feature engine first to get actual feature count
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>();

    // Initialize multi-horizon EWRLS predictor with correct feature count
    size_t num_features = feature_engine_->get_features().size();
    if (num_features == 0) {
        // Feature engine not ready yet, use default config total
        features::UnifiedFeatureEngineConfig default_config;
        num_features = default_config.total_features();  // 126 features
    }
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
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
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check volatility filter (skip trading in flat markets)
    if (config_.enable_volatility_filter && !has_sufficient_volatility()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.metadata["skip_reason"] = "insufficient_volatility";
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
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

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    if (bar_history_.size() < MIN_FEATURES_BARS) {
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_ready()) {
        return {};
    }

    return feature_engine_->get_features();
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = features;
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    pending_updates_[pred.target_bar_index] = pred;
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& pred = it->second;

        // Calculate actual return
        double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
        if (!pred.is_long) {
            actual_return = -actual_return;  // Invert for short
        }

        // Update the appropriate horizon predictor
        ensemble_predictor_->update(pred.horizon, pred.features, actual_return);

        // Debug logging
        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed update for horizon " + std::to_string(pred.horizon) +
                           ", return=" + std::to_string(actual_return) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        // Remove processed prediction
        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

double OnlineEnsembleStrategy::calculate_atr(int period) const {
    if (bar_history_.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    // Calculate True Range for each bar and average
    double sum_tr = 0.0;
    for (size_t i = bar_history_.size() - period; i < bar_history_.size(); ++i) {
        const auto& curr = bar_history_[i];
        const auto& prev = bar_history_[i - 1];

        // True Range = max(high-low, |high-prev_close|, |low-prev_close|)
        double hl = curr.high - curr.low;
        double hc = std::abs(curr.high - prev.close);
        double lc = std::abs(curr.low - prev.close);

        double tr = std::max({hl, hc, lc});
        sum_tr += tr;
    }

    return sum_tr / period;
}

bool OnlineEnsembleStrategy::has_sufficient_volatility() const {
    if (bar_history_.empty()) {
        return false;
    }

    // Calculate ATR
    double atr = calculate_atr(config_.atr_period);

    // Get current price
    double current_price = bar_history_.back().close;

    // Calculate ATR as percentage of price
    double atr_ratio = (current_price > 0) ? (atr / current_price) : 0.0;

    // Check if ATR ratio meets minimum threshold
    return atr_ratio >= config_.min_atr_ratio;
}

} // namespace sentio

```

