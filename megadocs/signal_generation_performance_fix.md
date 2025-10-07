# Signal Generation Performance Fix - Complete Solution

**Generated**: 2025-10-07 15:32:12
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (9 files)
**Description**: Root cause confirmed: on_bar() takes 40ms per bar due to 126-feature recalculation. Solution: O(1) incremental feature engine with RSI/TSI/Williams/BB optimizations. Includes optimized code ready for integration.

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [data/tmp/PERFORMANCE_FIX_SOLUTION.md](#file-1)
2. [data/tmp/SIGNAL_GENERATION_STRATEGY_HANG_BUG.md](#file-2)
3. [include/features/unified_feature_engine.h](#file-3)
4. [include/learning/online_predictor.h](#file-4)
5. [include/strategy/online_ensemble_strategy.h](#file-5)
6. [src/cli/generate_signals_command.cpp](#file-6)
7. [src/features/unified_feature_engine.cpp](#file-7)
8. [src/learning/online_predictor.cpp](#file-8)
9. [src/strategy/online_ensemble_strategy.cpp](#file-9)

---

## 📄 **FILE 1 of 9**: data/tmp/PERFORMANCE_FIX_SOLUTION.md

**File Information**:
- **Path**: `data/tmp/PERFORMANCE_FIX_SOLUTION.md`

- **Size**: 311 lines
- **Modified**: 2025-10-07 15:31:56

- **Type**: .md

```text
# Performance Fix Solution for Signal Generation Hang

**Date:** October 7, 2025
**Status:** Solution Identified - Ready for Implementation
**Performance Issue:** CONFIRMED - `on_bar()` takes 40-46ms per bar

---

## Root Cause CONFIRMED ✅

**Measured Performance:**
```
[DEBUG] Bar 100: on_bar=46ms, generate_signal=0ms
[DEBUG] Bar 200: on_bar=40ms, generate_signal=0ms
[DEBUG] Bar 300: on_bar=41ms, generate_signal=0ms
[DEBUG] Bar 400: on_bar=40ms, generate_signal=0ms
```

**Impact:**
- 500 bars × 40ms = 20 seconds ✅ (matches observed)
- 9,600 bars × 40ms = 384 seconds (6.4 minutes) ❌ HANG
- 48,000 bars × 40ms = 1,920 seconds (32 minutes) ❌ HANG

**Root Cause:**
`UnifiedFeatureEngine::get_features()` computes **126 features** on every call with:
- Historical lookback loops (RSI, Williams %R, TSI, Bollinger Bands)
- No incremental updates
- No memoization
- Feature recalculation every bar

---

## Solution: Optimized Online Feature Engine

### Key Optimizations

1. **O(1) Incremental Updates**
   - RSI: Wilder's smoothing (running average, not full recalc)
   - TSI: Dual EMA (no historical loops)
   - Williams %R: Fixed-size ring buffer (O(period))
   - Bollinger Bands: Rolling window with O(period) scan

2. **Bounded Memory**
   - Fixed deque size (max_history = 512)
   - No unbounded growth
   - Automatic pop_front() when exceeding cap

3. **Zero Allocations in Hot Path**
   - Feature vector reused (reserved)
   - No stringstream, no temporary vectors
   - No I/O or logging inside on_bar()

4. **Frequent Progress Reporting**
   - Every 256 bars OR every 1 second
   - Immediate visibility if real stall occurs

---

## Implementation Files

### Core Components (Provided by User)

1. **`include/common/types.h`**
   - Lightweight Bar struct
   - SignalOutput with enum SignalKind

2. **`include/features/unified_feature_engine.h`**
   - O(1) incremental RSI with Wilder smoothing
   - O(1) TSI with dual EMA
   - O(period) Williams %R with ring buffer
   - O(period) Bollinger Bands with rolling window
   - Bounded history (512 bars max)

3. **`include/learning/multi_horizon_predictor.h`**
   - OnlineLogit with AdaGrad (O(d) per update)
   - No matrix inversion
   - No blocking operations
   - Decoupled label updates

4. **`include/strategy/online_ensemble_strategy.h`**
   - Minimal constructor (no heavy init)
   - Non-blocking on_bar()
   - Warmup + horizon check before predictions

5. **`src/cli/generate_signals_command.cpp`**
   - Tight loop with progress pulses
   - No I/O in hot path
   - Buffered JSONL writing

---

## Expected Performance After Fix

| Dataset Size | Current (40ms/bar) | Optimized (<1ms/bar) | Improvement |
|--------------|-------------------|---------------------|-------------|
| 500 bars | 20 seconds | <1 second | 20x faster |
| 9,600 bars | 384 seconds (HANG) | ~10 seconds | 38x faster |
| 48,000 bars | 1,920 seconds (HANG) | ~48 seconds | 40x faster |

**Target:** <1ms per bar average (O(1) per-bar complexity)

---

## Integration Steps

### Phase 1: Drop-in Replacement (Recommended)

1. **Replace UnifiedFeatureEngine**
   ```bash
   # Backup current implementation
   mv include/features/unified_feature_engine.h include/features/unified_feature_engine.h.backup
   mv src/features/unified_feature_engine.cpp src/features/unified_feature_engine.cpp.backup

   # Copy optimized version (from user's provided code)
   # Place new unified_feature_engine.h with O(1) updates
   ```

2. **Add MultiHorizonPredictor**
   ```bash
   # Create new file with OnlineLogit + AdaGrad
   # Replace existing EWRLS-based predictor
   ```

3. **Update OnlineEnsembleStrategy**
   ```bash
   # Modify constructor to avoid heavy initialization
   # Update on_bar() to use new feature engine
   # Ensure generate_signal() is non-blocking
   ```

4. **Update Generate Signals Command**
   ```bash
   # Add progress pulses (every 256 bars or 1s)
   # Remove I/O from hot path
   ```

### Phase 2: Validation

1. **Test with small dataset (500 bars)**
   ```bash
   time ./sentio_cli generate-signals \
     --data data/equities/SPY_test_small.csv \
     --output /tmp/test.jsonl \
     --verbose

   # Expected: <1 second total, <1ms per bar
   ```

2. **Test with medium dataset (9,600 bars)**
   ```bash
   time ./sentio_cli generate-signals \
     --data data/equities/SPY_20blocks.csv \
     --output /tmp/test_20.jsonl \
     --verbose

   # Expected: ~10 seconds total
   ```

3. **Test with large dataset (48,000 bars)**
   ```bash
   time ./sentio_cli generate-signals \
     --data data/equities/SPY_100blocks.csv \
     --output /tmp/test_100.jsonl \
     --verbose

   # Expected: ~48 seconds total
   ```

### Phase 3: Fine-Tuning

1. **Add per-bar timing diagnostics**
   ```cpp
   if (i % 100 == 0) {
       auto dt = duration_cast<milliseconds>(t2 - t1).count();
       if (dt > 5) std::cerr << "[slow] bar " << i << " took " << dt << "ms\n";
   }
   ```

2. **Profile hotspots if still slow**
   ```bash
   # macOS: Instruments Time Profiler
   # Linux: perf record / perf report
   # Identify any remaining O(n²) loops
   ```

3. **Verify no I/O in hot path**
   ```bash
   # Check for:
   # - utils::log_debug() calls
   # - std::cout / std::cerr inside on_bar()
   # - File writes during signal generation
   ```

---

## Code Reference: Optimized Feature Engine Snippet

### Key Method: Incremental RSI (Wilder)

```cpp
// Update RSI state (O(1) per bar)
if (init_) {
    double ch = b.close - last_close_;
    double gain = ch > 0 ? ch : 0.0;
    double loss = ch < 0 ? -ch : 0.0;
    avg_gain_ = (avg_gain_ * (cfg_.rsi_period - 1) + gain) / cfg_.rsi_period;
    avg_loss_ = (avg_loss_ * (cfg_.rsi_period - 1) + loss) / cfg_.rsi_period;
}

// Calculate RSI value
double rsi() const {
    if (!init_ || avg_loss_ == 0.0) return 100.0;
    double rs = avg_gain_ / avg_loss_;
    return 100.0 - (100.0 / (1.0 + rs));
}
```

**Complexity:** O(1) per bar (no loops over history)

### Key Method: Incremental TSI (Dual EMA)

```cpp
// Update TSI state (O(1) per bar)
if (has_prev_) {
    double m = b.close - last_close_;
    double am = std::fabs(m);
    // EMA1
    double a1 = 2.0 / (cfg_.tsi_len1 + 1.0);
    ema1_m_  = ema1_init_ ? (a1*m + (1-a1)*ema1_m_)   : (ema1_init_=true, m);
    ema1_am_ = ema1a_init_? (a1*am+ (1-a1)*ema1_am_)  : (ema1a_init_=true, am);
    // EMA2
    double a2 = 2.0 / (cfg_.tsi_len2 + 1.0);
    ema2_m_  = ema2_init_ ? (a2*ema1_m_  + (1-a2)*ema2_m_)  : (ema2_init_=true,  ema1_m_);
    ema2_am_ = ema2a_init? (a2*ema1_am_ + (1-a2)*ema2_am_) : (ema2a_init=true, ema1_am_);
}
```

**Complexity:** O(1) per bar (exponential smoothing)

---

## Architectural Benefits

1. **Scalability**
   - Linear O(n) total time for n bars
   - Can process millions of bars efficiently

2. **Real-Time Ready**
   - Sub-millisecond latency per bar
   - Suitable for live trading

3. **Memory Bounded**
   - Fixed 512-bar history cap
   - Predictable memory footprint

4. **Maintainability**
   - Clear separation: features / learning / strategy
   - Easy to test individual components
   - No hidden O(n²) traps

---

## Alternative: Binary Preprocessing (Future Enhancement)

If even optimized CSV + online features is too slow for very large datasets (>1M bars):

1. **Precompute Features Offline**
   ```bash
   # One-time: Convert CSV → Binary with precomputed features
   ./sentio_preprocess --input SPY_5years.csv --output SPY_5years.bin
   ```

2. **mmap Binary Data**
   ```cpp
   // Fast random access, no parsing
   BinaryDataReader reader("SPY_5years.bin");
   auto bars = reader.read_range(start, count); // O(1) seek
   ```

3. **Expected Speed**
   - 5-10x faster than optimized CSV
   - Ideal for backtesting on full historical datasets

---

## Next Steps

1. ✅ **CONFIRMED:** on_bar() takes 40ms per bar (too slow)
2. ⏱️ **Implement optimized UnifiedFeatureEngine** (from provided code)
3. ⏱️ **Test with 9,600 bar dataset** (should complete in ~10 seconds)
4. ⏱️ **Verify O(1) per-bar complexity** (timing should be constant)
5. ⏱️ **Run full 100-block test** (48,000 bars in ~48 seconds)

---

## Files Provided by User (Ready to Integrate)

All code provided in user's message is production-ready:

1. `include/common/types.h` - Lightweight types
2. `include/features/unified_feature_engine.h` - O(1) incremental features
3. `include/learning/multi_horizon_predictor.h` - OnlineLogit with AdaGrad
4. `include/strategy/online_ensemble_strategy.h` - Non-blocking strategy
5. `src/strategy/online_ensemble_strategy.cpp` - Minimal constructor
6. `src/cli/generate_signals_command.cpp` - Tight loop with progress

**All dependencies:** STL only (no Eigen, no external deps)

---

**End of Solution Document**

```

## 📄 **FILE 2 of 9**: data/tmp/SIGNAL_GENERATION_STRATEGY_HANG_BUG.md

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

## 📄 **FILE 3 of 9**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`

- **Size**: 254 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <string>

namespace sentio {
namespace features {

/**
 * @brief Configuration for Unified Feature Engine
 */
struct UnifiedFeatureEngineConfig {
    // Feature categories
    bool enable_time_features = true;
    bool enable_price_action = true;
    bool enable_moving_averages = true;
    bool enable_momentum = true;
    bool enable_volatility = true;
    bool enable_volume = true;
    bool enable_statistical = true;
    bool enable_pattern_detection = true;
    bool enable_price_lags = true;
    bool enable_return_lags = true;
    
    // Normalization
    bool normalize_features = true;
    bool use_robust_scaling = false;  // Use median/IQR instead of mean/std
    
    // Performance optimization
    bool enable_caching = true;
    bool enable_incremental_updates = true;
    int max_history_size = 1000;
    
    // Feature dimensions (matching Kochi analysis)
    int time_features = 8;
    int price_action_features = 12;
    int moving_average_features = 16;
    int momentum_features = 20;
    int volatility_features = 15;
    int volume_features = 12;
    int statistical_features = 10;
    int pattern_features = 8;
    int price_lag_features = 10;
    int return_lag_features = 15;
    
    // Total: Variable based on enabled features  
    int total_features() const {
        int total = 0;
        if (enable_time_features) total += time_features;
        if (enable_price_action) total += price_action_features;
        if (enable_moving_averages) total += moving_average_features;
        if (enable_momentum) total += momentum_features;
        if (enable_volatility) total += volatility_features;
        if (enable_volume) total += volume_features;
        if (enable_statistical) total += statistical_features;
        if (enable_pattern_detection) total += pattern_features;
        if (enable_price_lags) total += price_lag_features;
        if (enable_return_lags) total += return_lag_features;
        return total;
    }
};

/**
 * @brief Incremental calculator for O(1) moving averages
 */
class IncrementalSMA {
public:
    explicit IncrementalSMA(int period);
    double update(double value);
    double get_value() const { return sum_ / std::min(period_, static_cast<int>(values_.size())); }
    bool is_ready() const { return static_cast<int>(values_.size()) >= period_; }

private:
    int period_;
    std::deque<double> values_;
    double sum_ = 0.0;
};

/**
 * @brief Incremental calculator for O(1) EMA
 */
class IncrementalEMA {
public:
    IncrementalEMA(int period, double alpha = -1.0);
    double update(double value);
    double get_value() const { return ema_value_; }
    bool is_ready() const { return initialized_; }

private:
    double alpha_;
    double ema_value_ = 0.0;
    bool initialized_ = false;
};

/**
 * @brief Incremental calculator for O(1) RSI
 */
class IncrementalRSI {
public:
    explicit IncrementalRSI(int period);
    double update(double price);
    double get_value() const;
    bool is_ready() const { return gain_sma_.is_ready() && loss_sma_.is_ready(); }

private:
    double prev_price_ = 0.0;
    bool first_update_ = true;
    IncrementalSMA gain_sma_;
    IncrementalSMA loss_sma_;
};

/**
 * @brief Unified Feature Engine implementing Kochi's 126-feature set
 * 
 * This engine provides a comprehensive set of technical indicators optimized
 * for machine learning applications. It implements all features identified
 * in the Kochi analysis with proper normalization and O(1) incremental updates.
 * 
 * Feature Categories:
 * 1. Time Features (8): Cyclical encoding of time components
 * 2. Price Action (12): OHLC patterns, gaps, shadows
 * 3. Moving Averages (16): SMA/EMA ratios at multiple periods
 * 4. Momentum (20): RSI, MACD, Stochastic, Williams %R
 * 5. Volatility (15): ATR, Bollinger Bands, Keltner Channels
 * 6. Volume (12): VWAP, OBV, A/D Line, Volume ratios
 * 7. Statistical (10): Correlation, regression, distribution metrics
 * 8. Patterns (8): Candlestick pattern detection
 * 9. Price Lags (10): Historical price references
 * 10. Return Lags (15): Historical return references
 */
class UnifiedFeatureEngine {
public:
    using Config = UnifiedFeatureEngineConfig;
    
    explicit UnifiedFeatureEngine(const Config& config = Config{});
    ~UnifiedFeatureEngine() = default;

    /**
     * @brief Update engine with new bar data
     * @param bar New OHLCV bar
     */
    void update(const Bar& bar);

    /**
     * @brief Get current feature vector
     * @return Vector of 126 normalized features
     */
    std::vector<double> get_features() const;

    /**
     * @brief Get specific feature category
     */
    std::vector<double> get_time_features() const;
    std::vector<double> get_price_action_features() const;
    std::vector<double> get_moving_average_features() const;
    std::vector<double> get_momentum_features() const;
    std::vector<double> get_volatility_features() const;
    std::vector<double> get_volume_features() const;
    std::vector<double> get_statistical_features() const;
    std::vector<double> get_pattern_features() const;
    std::vector<double> get_price_lag_features() const;
    std::vector<double> get_return_lag_features() const;

    /**
     * @brief Get feature names for debugging/analysis
     */
    std::vector<std::string> get_feature_names() const;

    /**
     * @brief Reset engine state
     */
    void reset();

    /**
     * @brief Check if engine has enough data for all features
     */
    bool is_ready() const;

    /**
     * @brief Get number of bars processed
     */
    size_t get_bar_count() const { return bar_history_.size(); }

private:
    Config config_;
    
    // Data storage
    std::deque<Bar> bar_history_;
    std::deque<double> returns_;
    std::deque<double> log_returns_;
    
    // Incremental calculators
    std::unordered_map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalEMA>> ema_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalRSI>> rsi_calculators_;
    
    // Cached features
    mutable std::vector<double> cached_features_;
    mutable bool cache_valid_ = false;
    
    // Normalization parameters
    struct NormalizationParams {
        double mean = 0.0;
        double std = 1.0;
        double median = 0.0;
        double iqr = 1.0;
        std::deque<double> history;
        bool initialized = false;
    };
    mutable std::unordered_map<std::string, NormalizationParams> norm_params_;
    
    // Private methods
    void initialize_calculators();
    void update_returns(const Bar& bar);
    void update_calculators(const Bar& bar);
    void invalidate_cache();
    
    // Feature calculation methods
    std::vector<double> calculate_time_features(const Bar& bar) const;
    std::vector<double> calculate_price_action_features() const;
    std::vector<double> calculate_moving_average_features() const;
    std::vector<double> calculate_momentum_features() const;
    std::vector<double> calculate_volatility_features() const;
    std::vector<double> calculate_volume_features() const;
    std::vector<double> calculate_statistical_features() const;
    std::vector<double> calculate_pattern_features() const;
    std::vector<double> calculate_price_lag_features() const;
    std::vector<double> calculate_return_lag_features() const;
    
    // Utility methods
    double normalize_feature(const std::string& name, double value) const;
    void update_normalization_params(const std::string& name, double value) const;
    double safe_divide(double numerator, double denominator, double fallback = 0.0) const;
    double calculate_atr(int period) const;
    double calculate_true_range(size_t index) const;
    
    // Pattern detection helpers
    bool is_doji(const Bar& bar) const;
    bool is_hammer(const Bar& bar) const;
    bool is_shooting_star(const Bar& bar) const;
    bool is_engulfing_bullish(size_t index) const;
    bool is_engulfing_bearish(size_t index) const;
    
    // Statistical helpers
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y, int period) const;
    double calculate_linear_regression_slope(const std::vector<double>& values, int period) const;
};

} // namespace features
} // namespace sentio
```

## 📄 **FILE 4 of 9**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`

- **Size**: 133 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <deque>
#include <memory>
#include <cmath>

namespace sentio {
namespace learning {

/**
 * Online learning predictor that eliminates train/inference parity issues
 * Uses Exponentially Weighted Recursive Least Squares (EWRLS)
 */
class OnlinePredictor {
public:
    struct Config {
        double lambda;
        double initial_variance;
        double regularization;
        int warmup_samples;
        bool adaptive_learning;
        double min_lambda;
        double max_lambda;
        
        Config()
            : lambda(0.995),
              initial_variance(100.0),
              regularization(0.01),
              warmup_samples(100),
              adaptive_learning(true),
              min_lambda(0.990),
              max_lambda(0.999) {}
    };
    
    struct PredictionResult {
        double predicted_return;
        double confidence;
        double volatility_estimate;
        bool is_ready;
        
        PredictionResult()
            : predicted_return(0.0),
              confidence(0.0),
              volatility_estimate(0.0),
              is_ready(false) {}
    };
    
    explicit OnlinePredictor(size_t num_features, const Config& config = Config());
    
    // Main interface - predict and optionally update
    PredictionResult predict(const std::vector<double>& features);
    void update(const std::vector<double>& features, double actual_return);
    
    // Combined predict-then-update for efficiency
    PredictionResult predict_and_update(const std::vector<double>& features, 
                                        double actual_return);
    
    // Adaptive learning rate based on recent volatility
    void adapt_learning_rate(double market_volatility);
    
    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);
    
    // Diagnostics
    double get_recent_rmse() const;
    double get_directional_accuracy() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }
    
private:
    Config config_;
    size_t num_features_;
    int samples_seen_;
    
    // EWRLS parameters
    Eigen::VectorXd theta_;      // Model parameters
    Eigen::MatrixXd P_;          // Covariance matrix
    double current_lambda_;      // Adaptive forgetting factor
    
    // Performance tracking
    std::deque<double> recent_errors_;
    std::deque<bool> recent_directions_;
    static constexpr size_t HISTORY_SIZE = 100;
    
    // Volatility estimation for adaptive learning
    std::deque<double> recent_returns_;
    double estimate_volatility() const;
    
    // Numerical stability
    void ensure_positive_definite();
    static constexpr double EPSILON = 1e-8;
};

/**
 * Ensemble of online predictors for different time horizons
 */
class MultiHorizonPredictor {
public:
    struct HorizonConfig {
        int horizon_bars;
        double weight;
        OnlinePredictor::Config predictor_config;
        
        HorizonConfig()
            : horizon_bars(1),
              weight(1.0),
              predictor_config() {}
    };
    
    explicit MultiHorizonPredictor(size_t num_features);
    
    // Add predictors for different horizons
    void add_horizon(int bars, double weight = 1.0);
    
    // Ensemble prediction
    OnlinePredictor::PredictionResult predict(const std::vector<double>& features);
    
    // Update all predictors
    void update(int bars_ago, const std::vector<double>& features, double actual_return);
    
private:
    size_t num_features_;
    std::vector<std::unique_ptr<OnlinePredictor>> predictors_;
    std::vector<HorizonConfig> configs_;
};

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 5 of 9**: include/strategy/online_ensemble_strategy.h

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

## 📄 **FILE 6 of 9**: src/cli/generate_signals_command.cpp

**File Information**:
- **Path**: `src/cli/generate_signals_command.cpp`

- **Size**: 255 lines
- **Modified**: 2025-10-07 15:30:24

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

    auto loop_start = std::chrono::high_resolution_clock::now();
    int64_t total_onbar_us = 0;
    int64_t total_gensig_us = 0;

    for (int i = start_bar; i < end_bar; ++i) {
        // DEBUG: Timing for on_bar
        auto t1 = std::chrono::high_resolution_clock::now();
        strategy.on_bar(bars[i]);
        auto t2 = std::chrono::high_resolution_clock::now();

        // DEBUG: Timing for generate_signal
        sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);
        auto t3 = std::chrono::high_resolution_clock::now();

        // Accumulate timings
        total_onbar_us += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        total_gensig_us += std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        // DEBUG: Log slow iterations
        if (i < 1000 && i % 100 == 0) {
            auto onbar_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            auto gensig_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
            std::cerr << "[DEBUG] Bar " << i << ": on_bar=" << onbar_ms << "ms, generate_signal=" << gensig_ms << "ms\n";
        }

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

## 📄 **FILE 7 of 9**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`

- **Size**: 647 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "features/unified_feature_engine.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sentio {
namespace features {

// IncrementalSMA implementation
IncrementalSMA::IncrementalSMA(int period) : period_(period) {
    // Note: std::deque doesn't have reserve(), but that's okay
}

double IncrementalSMA::update(double value) {
    if (values_.size() >= period_) {
        sum_ -= values_.front();
        values_.pop_front();
    }
    
    values_.push_back(value);
    sum_ += value;
    
    return get_value();
}

// IncrementalEMA implementation
IncrementalEMA::IncrementalEMA(int period, double alpha) {
    if (alpha < 0) {
        alpha_ = 2.0 / (period + 1.0);  // Standard EMA alpha
    } else {
        alpha_ = alpha;
    }
}

double IncrementalEMA::update(double value) {
    if (!initialized_) {
        ema_value_ = value;
        initialized_ = true;
    } else {
        ema_value_ = alpha_ * value + (1.0 - alpha_) * ema_value_;
    }
    return ema_value_;
}

// IncrementalRSI implementation
IncrementalRSI::IncrementalRSI(int period) 
    : gain_sma_(period), loss_sma_(period) {
}

double IncrementalRSI::update(double price) {
    if (first_update_) {
        prev_price_ = price;
        first_update_ = false;
        return 50.0;  // Neutral RSI
    }
    
    double change = price - prev_price_;
    double gain = std::max(0.0, change);
    double loss = std::max(0.0, -change);
    
    gain_sma_.update(gain);
    loss_sma_.update(loss);
    
    prev_price_ = price;
    
    return get_value();
}

double IncrementalRSI::get_value() const {
    if (!is_ready()) return 50.0;
    
    double avg_gain = gain_sma_.get_value();
    double avg_loss = loss_sma_.get_value();
    
    if (avg_loss == 0.0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// UnifiedFeatureEngine implementation
UnifiedFeatureEngine::UnifiedFeatureEngine(const Config& config) : config_(config) {
    initialize_calculators();
    cached_features_.resize(config_.total_features(), 0.0);
}

void UnifiedFeatureEngine::initialize_calculators() {
    // Initialize SMA calculators for various periods
    std::vector<int> sma_periods = {5, 10, 20, 50, 100, 200};
    for (int period : sma_periods) {
        sma_calculators_["sma_" + std::to_string(period)] = 
            std::make_unique<IncrementalSMA>(period);
    }
    
    // Initialize EMA calculators
    std::vector<int> ema_periods = {5, 10, 20, 50};
    for (int period : ema_periods) {
        ema_calculators_["ema_" + std::to_string(period)] = 
            std::make_unique<IncrementalEMA>(period);
    }
    
    // Initialize RSI calculators
    std::vector<int> rsi_periods = {14, 21};
    for (int period : rsi_periods) {
        rsi_calculators_["rsi_" + std::to_string(period)] = 
            std::make_unique<IncrementalRSI>(period);
    }
}

void UnifiedFeatureEngine::update(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    
    // Maintain max history size
    if (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
    }
    
    // Update returns
    update_returns(bar);
    
    // Update incremental calculators
    update_calculators(bar);
    
    // Invalidate cache
    invalidate_cache();
}

void UnifiedFeatureEngine::update_returns(const Bar& bar) {
    if (!bar_history_.empty() && bar_history_.size() > 1) {
        double prev_close = bar_history_[bar_history_.size() - 2].close;
        double current_close = bar.close;
        
        double return_val = (current_close - prev_close) / prev_close;
        double log_return = std::log(current_close / prev_close);
        
        returns_.push_back(return_val);
        log_returns_.push_back(log_return);
        
        // Maintain max size
        if (returns_.size() > config_.max_history_size) {
            returns_.pop_front();
            log_returns_.pop_front();
        }
    }
}

void UnifiedFeatureEngine::update_calculators(const Bar& bar) {
    double close = bar.close;
    
    // Update SMA calculators
    for (auto& [name, calc] : sma_calculators_) {
        calc->update(close);
    }
    
    // Update EMA calculators
    for (auto& [name, calc] : ema_calculators_) {
        calc->update(close);
    }
    
    // Update RSI calculators
    for (auto& [name, calc] : rsi_calculators_) {
        calc->update(close);
    }
}

void UnifiedFeatureEngine::invalidate_cache() {
    cache_valid_ = false;
}

std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_ && config_.enable_caching) {
        return cached_features_;
    }
    
    std::vector<double> features;
    features.reserve(config_.total_features());
    
    if (bar_history_.empty()) {
        // Return zeros if no data
        features.resize(config_.total_features(), 0.0);
        return features;
    }
    
    const Bar& current_bar = bar_history_.back();
    
    // 1. Time features (8)
    if (config_.enable_time_features) {
        auto time_features = calculate_time_features(current_bar);
        features.insert(features.end(), time_features.begin(), time_features.end());
    }
    
    // 2. Price action features (12)
    if (config_.enable_price_action) {
        auto price_features = calculate_price_action_features();
        features.insert(features.end(), price_features.begin(), price_features.end());
    }
    
    // 3. Moving average features (16)
    if (config_.enable_moving_averages) {
        auto ma_features = calculate_moving_average_features();
        features.insert(features.end(), ma_features.begin(), ma_features.end());
    }
    
    // 4. Momentum features (20)
    if (config_.enable_momentum) {
        auto momentum_features = calculate_momentum_features();
        features.insert(features.end(), momentum_features.begin(), momentum_features.end());
    }
    
    // 5. Volatility features (15)
    if (config_.enable_volatility) {
        auto vol_features = calculate_volatility_features();
        features.insert(features.end(), vol_features.begin(), vol_features.end());
    }
    
    // 6. Volume features (12)
    if (config_.enable_volume) {
        auto volume_features = calculate_volume_features();
        features.insert(features.end(), volume_features.begin(), volume_features.end());
    }
    
    // 7. Statistical features (10)
    if (config_.enable_statistical) {
        auto stat_features = calculate_statistical_features();
        features.insert(features.end(), stat_features.begin(), stat_features.end());
    }
    
    // 8. Pattern features (8)
    if (config_.enable_pattern_detection) {
        auto pattern_features = calculate_pattern_features();
        features.insert(features.end(), pattern_features.begin(), pattern_features.end());
    }
    
    // 9. Price lag features (10)
    if (config_.enable_price_lags) {
        auto price_lag_features = calculate_price_lag_features();
        features.insert(features.end(), price_lag_features.begin(), price_lag_features.end());
    }
    
    // 10. Return lag features (15)
    if (config_.enable_return_lags) {
        auto return_lag_features = calculate_return_lag_features();
        features.insert(features.end(), return_lag_features.begin(), return_lag_features.end());
    }
    
    // Ensure we have the right number of features
    features.resize(config_.total_features(), 0.0);
    
    // Cache results
    if (config_.enable_caching) {
        cached_features_ = features;
        cache_valid_ = true;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_time_features(const Bar& bar) const {
    std::vector<double> features(config_.time_features, 0.0);
    
    // Convert timestamp to time components
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);
    
    if (time_info) {
        // Cyclical encoding of time components
        double hour = time_info->tm_hour;
        double minute = time_info->tm_min;
        double day_of_week = time_info->tm_wday;
        double day_of_month = time_info->tm_mday;
        
        // Hour cyclical encoding (24 hours)
        features[0] = std::sin(2 * M_PI * hour / 24.0);
        features[1] = std::cos(2 * M_PI * hour / 24.0);
        
        // Minute cyclical encoding (60 minutes)
        features[2] = std::sin(2 * M_PI * minute / 60.0);
        features[3] = std::cos(2 * M_PI * minute / 60.0);
        
        // Day of week cyclical encoding (7 days)
        features[4] = std::sin(2 * M_PI * day_of_week / 7.0);
        features[5] = std::cos(2 * M_PI * day_of_week / 7.0);
        
        // Day of month cyclical encoding (31 days)
        features[6] = std::sin(2 * M_PI * day_of_month / 31.0);
        features[7] = std::cos(2 * M_PI * day_of_month / 31.0);
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_action_features() const {
    std::vector<double> features(config_.price_action_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    const Bar& previous = bar_history_[bar_history_.size() - 2];
    
    // Basic OHLC ratios
    features[0] = safe_divide(current.high - current.low, current.close);  // Range/Close
    features[1] = safe_divide(current.close - current.open, current.close);  // Body/Close
    features[2] = safe_divide(current.high - std::max(current.open, current.close), current.close);  // Upper shadow
    features[3] = safe_divide(std::min(current.open, current.close) - current.low, current.close);  // Lower shadow
    
    // Gap analysis
    features[4] = safe_divide(current.open - previous.close, previous.close);  // Gap
    
    // Price position within range
    features[5] = safe_divide(current.close - current.low, current.high - current.low);  // Close position
    
    // Volatility measures
    features[6] = safe_divide(current.high - current.low, previous.close);  // True range ratio
    
    // Price momentum
    features[7] = safe_divide(current.close - previous.close, previous.close);  // Return
    features[8] = safe_divide(current.high - previous.high, previous.high);  // High momentum
    features[9] = safe_divide(current.low - previous.low, previous.low);  // Low momentum
    
    // Volume-price relationship
    features[10] = safe_divide(current.volume, previous.volume + 1.0) - 1.0;  // Volume change
    features[11] = safe_divide(current.volume * std::abs(current.close - current.open), current.close);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_moving_average_features() const {
    std::vector<double> features(config_.moving_average_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    int idx = 0;
    
    // SMA ratios
    for (const auto& [name, calc] : sma_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    // EMA ratios
    for (const auto& [name, calc] : ema_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    std::vector<double> features(config_.momentum_features, 0.0);
    
    int idx = 0;
    
    // RSI features
    for (const auto& [name, calc] : rsi_calculators_) {
        if (idx >= config_.momentum_features) break;
        if (calc->is_ready()) {
            features[idx] = calc->get_value() / 100.0;  // Normalize to [0,1]
        }
        idx++;
    }
    
    // Simple momentum features
    if (bar_history_.size() >= 10 && idx < config_.momentum_features) {
        double current = bar_history_.back().close;
        double past_5 = bar_history_[bar_history_.size() - 6].close;
        double past_10 = bar_history_[bar_history_.size() - 11].close;
        
        features[idx++] = safe_divide(current - past_5, past_5);  // 5-period momentum
        features[idx++] = safe_divide(current - past_10, past_10);  // 10-period momentum
    }
    
    // Rate of change features
    if (returns_.size() >= 5 && idx < config_.momentum_features) {
        // Recent return statistics
        auto recent_returns = std::vector<double>(returns_.end() - 5, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 5.0;
        features[idx++] = mean_return;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    std::vector<double> features(config_.volatility_features, 0.0);
    
    if (bar_history_.size() < 20) return features;
    
    // ATR-based features
    double atr_14 = calculate_atr(14);
    double atr_20 = calculate_atr(20);
    double current_price = bar_history_.back().close;
    
    features[0] = safe_divide(atr_14, current_price);  // ATR ratio
    features[1] = safe_divide(atr_20, current_price);  // ATR 20 ratio
    
    // Return volatility
    if (returns_.size() >= 20) {
        auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 20.0;
        
        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= 19.0;  // Sample variance
        
        features[2] = std::sqrt(variance);  // Return volatility
        features[3] = std::sqrt(variance * 252);  // Annualized volatility
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volume_features() const {
    std::vector<double> features(config_.volume_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Volume ratios and trends
    if (bar_history_.size() >= 20) {
        // Calculate average volume over last 20 periods
        double avg_volume = 0.0;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            avg_volume += bar_history_[i].volume;
        }
        avg_volume /= 20.0;
        
        features[0] = safe_divide(current.volume, avg_volume) - 1.0;  // Volume ratio
    }
    
    // Volume-price correlation
    features[1] = current.volume * std::abs(current.close - current.open);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_statistical_features() const {
    std::vector<double> features(config_.statistical_features, 0.0);
    
    if (returns_.size() < 20) return features;
    
    // Return distribution statistics
    auto recent_returns = std::vector<double>(returns_.end() - std::min(20UL, returns_.size()), returns_.end());
    
    // Skewness approximation
    double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / recent_returns.size();
    double variance = 0.0;
    double skew_sum = 0.0;
    
    for (double ret : recent_returns) {
        double diff = ret - mean_return;
        variance += diff * diff;
        skew_sum += diff * diff * diff;
    }
    
    variance /= (recent_returns.size() - 1);
    double std_dev = std::sqrt(variance);
    
    if (std_dev > 1e-8) {
        features[0] = skew_sum / (recent_returns.size() * std_dev * std_dev * std_dev);  // Skewness
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_pattern_features() const {
    std::vector<double> features(config_.pattern_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Basic candlestick patterns
    features[0] = is_doji(current) ? 1.0 : 0.0;
    features[1] = is_hammer(current) ? 1.0 : 0.0;
    features[2] = is_shooting_star(current) ? 1.0 : 0.0;
    
    if (bar_history_.size() >= 2) {
        features[3] = is_engulfing_bullish(bar_history_.size() - 1) ? 1.0 : 0.0;
        features[4] = is_engulfing_bearish(bar_history_.size() - 1) ? 1.0 : 0.0;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_lag_features() const {
    std::vector<double> features(config_.price_lag_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(bar_history_.size()) > lag) {
            double lagged_price = bar_history_[bar_history_.size() - 1 - lag].close;
            features[i] = safe_divide(current_price, lagged_price) - 1.0;
        }
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_return_lag_features() const {
    std::vector<double> features(config_.return_lag_features, 0.0);
    
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100, 150, 200, 250, 300, 500};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(returns_.size()) > lag) {
            features[i] = returns_[returns_.size() - 1 - lag];
        }
    }
    
    return features;
}

// Utility methods
double UnifiedFeatureEngine::safe_divide(double numerator, double denominator, double fallback) const {
    if (std::abs(denominator) < 1e-10) {
        return fallback;
    }
    return numerator / denominator;
}

double UnifiedFeatureEngine::calculate_atr(int period) const {
    if (static_cast<int>(bar_history_.size()) < period + 1) return 0.0;
    
    double atr_sum = 0.0;
    for (int i = 0; i < period; ++i) {
        size_t idx = bar_history_.size() - 1 - i;
        atr_sum += calculate_true_range(idx);
    }
    
    return atr_sum / period;
}

double UnifiedFeatureEngine::calculate_true_range(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return 0.0;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    
    return std::max({tr1, tr2, tr3});
}

// Pattern detection helpers
bool UnifiedFeatureEngine::is_doji(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double range = bar.high - bar.low;
    return range > 0 && (body / range) < 0.1;
}

bool UnifiedFeatureEngine::is_hammer(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return lower_shadow > 2 * body && upper_shadow < body;
}

bool UnifiedFeatureEngine::is_shooting_star(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return upper_shadow > 2 * body && lower_shadow < body;
}

bool UnifiedFeatureEngine::is_engulfing_bullish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bearish = previous.close < previous.open;
    bool curr_bullish = current.close > current.open;
    bool engulfs = current.open < previous.close && current.close > previous.open;
    
    return prev_bearish && curr_bullish && engulfs;
}

bool UnifiedFeatureEngine::is_engulfing_bearish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bullish = previous.close > previous.open;
    bool curr_bearish = current.close < current.open;
    bool engulfs = current.open > previous.close && current.close < previous.open;
    
    return prev_bullish && curr_bearish && engulfs;
}

void UnifiedFeatureEngine::reset() {
    bar_history_.clear();
    returns_.clear();
    log_returns_.clear();
    
    // Reset calculators
    initialize_calculators();
    
    invalidate_cache();
}

bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    return bar_history_.size() >= 64;
}

std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());
    
    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos", 
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});
    
    // Add more feature names as needed...
    // (This is a simplified version for now)
    
    return names;
}

} // namespace features
} // namespace sentio


```

## 📄 **FILE 8 of 9**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`

- **Size**: 338 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>

namespace sentio {
namespace learning {

OnlinePredictor::OnlinePredictor(size_t num_features, const Config& config)
    : config_(config), num_features_(num_features), samples_seen_(0),
      current_lambda_(config.lambda) {
    
    // Initialize parameters to zero
    theta_ = Eigen::VectorXd::Zero(num_features);
    
    // Initialize covariance with high uncertainty
    P_ = Eigen::MatrixXd::Identity(num_features, num_features) * config.initial_variance;
    
    utils::log_info("OnlinePredictor initialized with " + std::to_string(num_features) + 
                   " features, lambda=" + std::to_string(config.lambda));
}

OnlinePredictor::PredictionResult OnlinePredictor::predict(const std::vector<double>& features) {
    PredictionResult result;
    result.is_ready = is_ready();
    
    if (!result.is_ready) {
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }
    
    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Linear prediction
    result.predicted_return = theta_.dot(x);
    
    // Confidence from prediction variance
    double prediction_variance = x.transpose() * P_ * x;
    result.confidence = 1.0 / (1.0 + std::sqrt(prediction_variance));
    
    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();
    
    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;
    
    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }
    
    // Convert to Eigen
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Current prediction
    double predicted = theta_.dot(x);
    double error = actual_return - predicted;
    
    // Store error for diagnostics
    recent_errors_.push_back(error);
    if (recent_errors_.size() > HISTORY_SIZE) {
        recent_errors_.pop_front();
    }
    
    // Store direction accuracy
    bool correct_direction = (predicted > 0 && actual_return > 0) || 
                           (predicted < 0 && actual_return < 0);
    recent_directions_.push_back(correct_direction);
    if (recent_directions_.size() > HISTORY_SIZE) {
        recent_directions_.pop_front();
    }
    
    // EWRLS update with regularization
    double lambda_reg = current_lambda_ + config_.regularization;
    
    // Kalman gain
    Eigen::VectorXd Px = P_ * x;
    double denominator = lambda_reg + x.dot(Px);
    
    if (std::abs(denominator) < EPSILON) {
        utils::log_warning("Near-zero denominator in EWRLS update, skipping");
        return;
    }
    
    Eigen::VectorXd k = Px / denominator;
    
    // Update parameters
    theta_ += k * error;
    
    // Update covariance
    P_ = (P_ - k * x.transpose() * P_) / current_lambda_;
    
    // Ensure numerical stability
    ensure_positive_definite();
    
    // Adapt learning rate if enabled
    if (config_.adaptive_learning && samples_seen_ % 10 == 0) {
        adapt_learning_rate(estimate_volatility());
    }
}

OnlinePredictor::PredictionResult OnlinePredictor::predict_and_update(
    const std::vector<double>& features, double actual_return) {
    
    auto result = predict(features);
    update(features, actual_return);
    return result;
}

void OnlinePredictor::adapt_learning_rate(double market_volatility) {
    // Higher volatility -> faster adaptation (lower lambda)
    // Lower volatility -> slower adaptation (higher lambda)
    
    double baseline_vol = 0.001;  // 0.1% baseline volatility
    double vol_ratio = market_volatility / baseline_vol;
    
    // Map volatility ratio to lambda
    // High vol (ratio=2) -> lambda=0.990
    // Low vol (ratio=0.5) -> lambda=0.999
    double target_lambda = config_.lambda - 0.005 * std::log(vol_ratio);
    target_lambda = std::clamp(target_lambda, config_.min_lambda, config_.max_lambda);
    
    // Smooth transition
    current_lambda_ = 0.9 * current_lambda_ + 0.1 * target_lambda;
    
    utils::log_debug("Adapted lambda: " + std::to_string(current_lambda_) + 
                    " (volatility=" + std::to_string(market_volatility) + ")");
}

bool OnlinePredictor::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Save config
        file.write(reinterpret_cast<const char*>(&config_), sizeof(Config));
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_lambda_), sizeof(double));
        
        // Save theta
        file.write(reinterpret_cast<const char*>(theta_.data()), 
                  sizeof(double) * theta_.size());
        
        // Save P (covariance)
        file.write(reinterpret_cast<const char*>(P_.data()), 
                  sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Saved predictor state to: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlinePredictor::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Load config
        file.read(reinterpret_cast<char*>(&config_), sizeof(Config));
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_lambda_), sizeof(double));
        
        // Load theta
        theta_.resize(num_features_);
        file.read(reinterpret_cast<char*>(theta_.data()), 
                 sizeof(double) * theta_.size());
        
        // Load P
        P_.resize(num_features_, num_features_);
        file.read(reinterpret_cast<char*>(P_.data()), 
                 sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Loaded predictor state from: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

double OnlinePredictor::get_recent_rmse() const {
    if (recent_errors_.empty()) return 0.0;
    
    double sum_sq = 0.0;
    for (double error : recent_errors_) {
        sum_sq += error * error;
    }
    return std::sqrt(sum_sq / recent_errors_.size());
}

double OnlinePredictor::get_directional_accuracy() const {
    if (recent_directions_.empty()) return 0.5;
    
    int correct = std::count(recent_directions_.begin(), recent_directions_.end(), true);
    return static_cast<double>(correct) / recent_directions_.size();
}

std::vector<double> OnlinePredictor::get_feature_importance() const {
    // Feature importance based on parameter magnitude * covariance
    std::vector<double> importance(num_features_);
    
    for (size_t i = 0; i < num_features_; ++i) {
        // Combine parameter magnitude with certainty (inverse variance)
        double param_importance = std::abs(theta_[i]);
        double certainty = 1.0 / (1.0 + std::sqrt(P_(i, i)));
        importance[i] = param_importance * certainty;
    }
    
    // Normalize
    double max_imp = *std::max_element(importance.begin(), importance.end());
    if (max_imp > 0) {
        for (double& imp : importance) {
            imp /= max_imp;
        }
    }
    
    return importance;
}

double OnlinePredictor::estimate_volatility() const {
    if (recent_returns_.size() < 20) return 0.001;  // Default 0.1%
    
    double mean = std::accumulate(recent_returns_.begin(), recent_returns_.end(), 0.0) 
                 / recent_returns_.size();
    
    double sum_sq = 0.0;
    for (double ret : recent_returns_) {
        sum_sq += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sum_sq / recent_returns_.size());
}

void OnlinePredictor::ensure_positive_definite() {
    // Eigenvalue decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);
    Eigen::VectorXd eigenvalues = solver.eigenvalues();
    
    // Ensure all eigenvalues are positive
    bool needs_correction = false;
    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (eigenvalues[i] < EPSILON) {
            eigenvalues[i] = EPSILON;
            needs_correction = true;
        }
    }
    
    if (needs_correction) {
        // Reconstruct with corrected eigenvalues
        P_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        utils::log_debug("Corrected covariance matrix for positive definiteness");
    }
}

// MultiHorizonPredictor Implementation

MultiHorizonPredictor::MultiHorizonPredictor(size_t num_features) 
    : num_features_(num_features) {
}

void MultiHorizonPredictor::add_horizon(int bars, double weight) {
    HorizonConfig config;
    config.horizon_bars = bars;
    config.weight = weight;

    // Adjust learning rate based on horizon
    config.predictor_config.lambda = 0.995 + 0.001 * std::log(bars);
    config.predictor_config.lambda = std::clamp(config.predictor_config.lambda, 0.990, 0.999);

    // Reduce warmup for multi-horizon learning
    // Updates arrive delayed by horizon length, so effective warmup is longer
    config.predictor_config.warmup_samples = 20;

    predictors_.emplace_back(std::make_unique<OnlinePredictor>(num_features_, config.predictor_config));
    configs_.push_back(config);

    utils::log_info("Added predictor for " + std::to_string(bars) + "-bar horizon");
}

OnlinePredictor::PredictionResult MultiHorizonPredictor::predict(const std::vector<double>& features) {
    OnlinePredictor::PredictionResult ensemble_result;
    ensemble_result.predicted_return = 0.0;
    ensemble_result.confidence = 0.0;
    ensemble_result.volatility_estimate = 0.0;
    
    double total_weight = 0.0;
    int ready_count = 0;
    
    for (size_t i = 0; i < predictors_.size(); ++i) {
        auto result = predictors_[i]->predict(features);
        
        if (result.is_ready) {
            double weight = configs_[i].weight * result.confidence;
            ensemble_result.predicted_return += result.predicted_return * weight;
            ensemble_result.confidence += result.confidence * configs_[i].weight;
            ensemble_result.volatility_estimate += result.volatility_estimate * configs_[i].weight;
            total_weight += weight;
            ready_count++;
        }
    }
    
    if (total_weight > 0) {
        ensemble_result.predicted_return /= total_weight;
        ensemble_result.confidence /= configs_.size();
        ensemble_result.volatility_estimate /= configs_.size();
        ensemble_result.is_ready = true;
    }
    
    return ensemble_result;
}

void MultiHorizonPredictor::update(int bars_ago, const std::vector<double>& features, 
                                   double actual_return) {
    // Update the appropriate predictor
    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (configs_[i].horizon_bars == bars_ago) {
            predictors_[i]->update(features, actual_return);
            break;
        }
    }
}

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 9 of 9**: src/strategy/online_ensemble_strategy.cpp

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

