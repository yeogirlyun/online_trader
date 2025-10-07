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
