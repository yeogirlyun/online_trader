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
