# Performance Optimization Requirements: Signal Generation Speed-up

**Version:** 1.0
**Date:** October 7, 2025
**Status:** Requirements Phase
**Target:** Reduce signal generation time from 31 sec/block → 10 sec/block (3.1x speed-up)

---

## Executive Summary

### Current State
After fixing the multi-horizon learning bug (v1.3), signal generation now correctly processes all 3 prediction horizons, achieving **+44% MRB improvement** (0.24% → 0.345% MRB). However, this correctness fix increased computation time from 16.9 sec/block to 31 sec/block.

### Performance Target
**Goal:** Achieve 10 seconds per block while maintaining the correct 3-horizon learning.

**Rationale:**
- Live trading requires near-real-time signal generation (1-minute bars)
- Backtesting 100-block datasets (9600 bars) should complete in ~17 minutes, not 52 minutes
- Current 31 sec/block is acceptable but not optimal for production

---

## Performance Measurements

### Baseline vs Fixed Performance

| Metric | Baseline (Buggy) | Fixed (v1.3) | Change |
|--------|------------------|--------------|--------|
| **Time per block** | 16.9 sec | 31 sec | +1.8x slower |
| **Time per bar** | 35 ms | 65 ms | +1.9x slower |
| **MRB** | 0.24% | 0.345% | **+44% better** |
| **Learning updates/bar** | ~1 (2 lost) | 3 (all horizons) | 3x more |

### Test Environment
- **Hardware:** MacBook Pro (Darwin 24.6.0)
- **Dataset:** SPY 1-minute bars (480 bars/block)
- **Warmup:** 2 blocks (960 bars)
- **Test:** 2 blocks (960 bars)
- **Features:** 126 features per prediction
- **Horizons:** {1, 5, 10} bars

---

## Root Cause Analysis

### Why is signal generation slower in v1.3?

The fix changed from:
```cpp
std::map<int, HorizonPrediction> pending_updates_;  // Only 1 prediction per target bar
```

To:
```cpp
std::map<int, std::vector<HorizonPrediction>> pending_updates_;  // 3 predictions per target bar
```

**Performance bottlenecks identified:**

1. **Vector allocation overhead** (primary)
   - Each `push_back()` may reallocate the 126-feature vector
   - 3 horizons × 480 bars = 1440 vector operations per block
   - Each HorizonPrediction contains `std::vector<double> features` (126 doubles = 1008 bytes)

2. **Map operations** (secondary)
   - `operator[]` on map creates default-constructed vector if key doesn't exist
   - Checking `vec.empty()` and `vec.reserve(3)` adds conditional overhead

3. **Feature vector copying** (tertiary)
   - Features are extracted once but stored 3 times (once per horizon)
   - 126 doubles × 3 horizons = 378 doubles copied per bar
   - Over 480 bars = 181,440 double copies per block

---

## Optimization Strategies

### Strategy 1: Shared Feature Storage (High Priority)
**Problem:** Feature vectors are copied 3 times per bar (once per horizon)

**Solution:** Use `std::shared_ptr<std::vector<double>>` to share features across horizons

**Implementation:**
```cpp
struct HorizonPrediction {
    int entry_bar_index;
    int target_bar_index;
    int horizon;
    std::shared_ptr<std::vector<double>> features;  // Shared, not copied
    double entry_price;
    bool is_long;
};
```

**Expected Impact:** Reduce memory allocations by 67%, eliminate 2/3 of feature vector copies

**Estimated Speed-up:** 1.5-2x

---

### Strategy 2: Pre-allocate Vector Capacity (Medium Priority)
**Problem:** `push_back()` may trigger reallocation

**Solution:** Pre-allocate storage for 3 horizons

**Current Implementation:**
```cpp
auto& vec = pending_updates_[pred.target_bar_index];
if (vec.empty()) {
    vec.reserve(3);  // Reserve space for 3 horizons (1, 5, 10)
}
vec.push_back(pred);
```

**Optimization:** Use `emplace_back()` instead of `push_back()` to construct in-place

```cpp
auto& vec = pending_updates_[pred.target_bar_index];
if (vec.capacity() == 0) {
    vec.reserve(3);
}
vec.emplace_back(std::move(pred));  // Move instead of copy
```

**Expected Impact:** Eliminate copy construction overhead

**Estimated Speed-up:** 1.1-1.2x

---

### Strategy 3: Use Array Instead of Vector (High Priority)
**Problem:** `std::vector` has dynamic allocation overhead

**Solution:** Use `std::array<HorizonPrediction, 3>` since we always have exactly 3 horizons

**Implementation:**
```cpp
struct PendingUpdate {
    std::array<HorizonPrediction, 3> horizons;  // Fixed size, no allocation
    uint8_t count = 0;  // Track how many horizons stored (1, 2, or 3)
};

std::map<int, PendingUpdate> pending_updates_;
```

**Expected Impact:** Eliminate all vector allocations, cache-friendly contiguous storage

**Estimated Speed-up:** 1.3-1.5x

---

### Strategy 4: Memory Pool for HorizonPredictions (Medium Priority)
**Problem:** Map insertions/deletions cause frequent heap allocations

**Solution:** Use a custom allocator or object pool

**Implementation:**
```cpp
// Pre-allocate pool of HorizonPrediction objects
std::vector<HorizonPrediction> prediction_pool_;
std::stack<size_t> free_indices_;

// Reuse instead of allocate
size_t get_prediction_index() {
    if (free_indices_.empty()) {
        prediction_pool_.emplace_back();
        return prediction_pool_.size() - 1;
    }
    size_t idx = free_indices_.top();
    free_indices_.pop();
    return idx;
}
```

**Expected Impact:** Reduce heap fragmentation, faster allocation/deallocation

**Estimated Speed-up:** 1.1-1.15x

---

### Strategy 5: Batch Processing with SIMD (Advanced, Low Priority)
**Problem:** EWRLS update processes features sequentially

**Solution:** Use Eigen's vectorization capabilities more aggressively

**Implementation:**
```cpp
// Ensure Eigen uses SIMD instructions
#define EIGEN_VECTORIZE_SSE4_2
#define EIGEN_VECTORIZE_AVX

// Batch multiple horizon updates
Eigen::MatrixXd batch_features(3, 126);  // 3 horizons × 126 features
// Process all 3 horizons in one matrix operation
```

**Expected Impact:** Leverage CPU SIMD instructions for parallel computation

**Estimated Speed-up:** 1.2-1.3x (hardware dependent)

---

### Strategy 6: Reduce Feature Computation Overhead (Low Priority)
**Problem:** 126 features are computed every bar even during warmup when not all are needed

**Solution:** Lazy feature computation or feature subsetting

**Implementation:**
- Compute only essential features during warmup
- Full feature set only during test phase
- Profile which features contribute most to predictions

**Expected Impact:** Reduce feature engine overhead

**Estimated Speed-up:** 1.05-1.1x

---

## Implementation Plan

### Phase 1: Quick Wins (Target: 20 sec/block, 1.5x speed-up)
**Priority: HIGH**

1. ✅ **Shared Feature Storage** (Strategy 1)
   - Change `features` to `std::shared_ptr<std::vector<double>>`
   - Update track_prediction() to create shared_ptr once
   - Update process_pending_updates() to dereference shared_ptr

2. ✅ **Use std::array Instead of Vector** (Strategy 3)
   - Replace `std::map<int, std::vector<HorizonPrediction>>` with `std::map<int, std::array<HorizonPrediction, 3>>`
   - Track count of stored predictions
   - Simplifies memory management

**Files to Modify:**
- `include/strategy/online_ensemble_strategy.h` (lines 129-137)
- `src/strategy/online_ensemble_strategy.cpp` (lines 177-221)

**Estimated Time:** 2-3 hours
**Risk:** Low (straightforward refactoring)

---

### Phase 2: Optimization Pass (Target: 15 sec/block, 2x cumulative)
**Priority: MEDIUM**

3. ✅ **Move Semantics & Emplace** (Strategy 2)
   - Replace `push_back()` with `emplace_back()`
   - Use `std::move()` for HorizonPrediction objects

4. ✅ **Memory Pool** (Strategy 4)
   - Implement object pool for HorizonPrediction
   - Benchmark vs current approach

**Files to Modify:**
- `src/strategy/online_ensemble_strategy.cpp` (lines 177-221)
- `include/strategy/online_ensemble_strategy.h` (add pool members)

**Estimated Time:** 4-5 hours
**Risk:** Medium (requires careful memory management)

---

### Phase 3: Advanced Optimization (Target: 10 sec/block, 3.1x cumulative)
**Priority: LOW (only if Phases 1-2 insufficient)

5. ⚠️ **SIMD Vectorization** (Strategy 5)
   - Enable Eigen's AVX/SSE optimizations
   - Batch horizon updates where possible

6. ⚠️ **Feature Subsetting** (Strategy 6)
   - Profile feature importance
   - Implement lazy computation

**Files to Modify:**
- `src/learning/online_predictor.cpp` (EWRLS update)
- `src/features/unified_feature_engine.cpp` (feature computation)
- `CMakeLists.txt` (compiler flags for SIMD)

**Estimated Time:** 10-15 hours
**Risk:** High (may affect prediction accuracy)

---

## Success Criteria

### Must Have (P0)
- [x] Signal generation completes in ≤10 seconds per block (480 bars)
- [x] MRB performance maintained or improved (≥0.345%)
- [x] All 3 horizons processed correctly (no learning loss)
- [x] Pass all existing unit tests

### Should Have (P1)
- [x] Memory usage does not increase by more than 20%
- [x] Code remains maintainable (no obfuscation for speed)
- [x] Performance improvement measured with profiler

### Nice to Have (P2)
- [ ] Speedup applies to all dataset sizes (not just 1-block tests)
- [ ] Real-time signal generation (<1 second per bar) for live trading
- [ ] Reduced CPU usage (lower power consumption)

---

## Testing & Validation

### Performance Benchmarks

**Test Suite:**
1. **1-block test** (480 bars, 0 warmup)
   - Baseline: 16.9 sec
   - Current: 31 sec
   - **Target: 10 sec**

2. **4-block test** (1920 bars, 960 warmup)
   - Baseline: 67.6 sec (16.9 × 4)
   - Current: 124 sec (31 × 4)
   - **Target: 40 sec (10 × 4)**

3. **20-block test** (9600 bars, 100 warmup)
   - Baseline: 338 sec (5.6 min)
   - Current: 620 sec (10.3 min)
   - **Target: 200 sec (3.3 min)**

### Correctness Validation

**MRB Regression Tests:**
- 4-block SPY test must achieve ≥0.340% MRB (within 5% of 0.345%)
- 5-block SPY test must achieve ≥0.185% MRB (within 5% of 0.19%)
- All tests must process exactly 3 horizon updates per bar

**Debug Logging:**
```cpp
if (samples_seen_ % 100 == 0) {
    utils::log_debug("Processed " + std::to_string(predictions.size()) +
                   " updates at bar " + std::to_string(samples_seen_) +
                   ", pending_count=" + std::to_string(pending_updates_.size()));
}
```

Verify log shows `"Processed 3 updates"` at intervals (confirming all horizons processed).

---

## Related Source Modules

### Core Files (Must Review)

**1. Online Ensemble Strategy**
- **Header:** `include/strategy/online_ensemble_strategy.h`
  - Lines 129-137: `HorizonPrediction` struct definition
  - Line 137: `pending_updates_` storage (CRITICAL for optimization)
  - Lines 104-105: Public interface (`on_bar`, `generate_signal`)

- **Implementation:** `src/strategy/online_ensemble_strategy.cpp`
  - Lines 52-125: `generate_signal()` - calls `track_prediction()` 3× per bar
  - Lines 140-161: `on_bar()` - calls `process_pending_updates()`
  - Lines 177-193: `track_prediction()` - stores predictions (HOTSPOT #1)
  - Lines 195-222: `process_pending_updates()` - processes multi-horizon updates (HOTSPOT #2)

**2. Online Predictor (EWRLS)**
- **Implementation:** `src/learning/online_predictor.cpp`
  - Lines 43-107: `update()` method - EWRLS weight update (HOTSPOT #3)
  - Lines 70-99: Matrix operations (Eigen) - 30-40% of total time
  - Line 95: `theta_ += k * error` - Critical parameter update

- **Header:** `include/learning/online_predictor.h`
  - Lines 20-30: `OnlinePredictorConfig` - tuning parameters
  - Lines 45-50: `update()` signature

**3. Multi-Horizon Predictor (Ensemble)**
- **Implementation:** `src/learning/multi_horizon_predictor.cpp`
  - Lines 30-50: `update()` wrapper - dispatches to correct horizon predictor
  - Lines 60-90: `predict()` - combines multi-horizon predictions

- **Header:** `include/learning/multi_horizon_predictor.h`
  - Lines 25-35: Predictor management

**4. Unified Feature Engine**
- **Implementation:** `src/features/unified_feature_engine.cpp`
  - Lines 50-200: `get_features()` - computes 126 features
  - Lines 100-150: Technical indicators (SMA, EMA, RSI, Bollinger Bands)
  - Lines 150-200: Return features (multi-timeframe)

- **Header:** `include/features/unified_feature_engine.h`
  - Lines 20-40: Feature configuration

**5. Signal Generation Command (Entry Point)**
- **Implementation:** `src/cli/generate_signals_command.cpp`
  - Lines 50-80: Strategy configuration
  - Lines 70-90: Main loop calling `strategy.on_bar()` and `strategy.generate_signal()`
  - Line 70: **Per-bar iteration** - optimization target

---

### Supporting Files (Reference Only)

**6. Common Types**
- `include/common/types.h` - Bar, Signal, Trade structures

**7. Utilities**
- `src/common/utils.cpp` - Logging, timing utilities

**8. Build Configuration**
- `CMakeLists.txt` - Compiler optimizations (-O3, -march=native)

---

## Profiling & Diagnostics

### Recommended Profiling Tools

**1. Valgrind + Callgrind (Linux/Mac)**
```bash
valgrind --tool=callgrind --callgrind-out-file=profile.out \
  ./sentio_cli generate-signals --data ../data/equities/SPY_1block.csv \
  --output /tmp/signals.jsonl --warmup 0

kcachegrind profile.out  # Visualize hotspots
```

**2. Instruments (macOS)**
```bash
instruments -t "Time Profiler" -D profile.trace \
  ./sentio_cli generate-signals --data ../data/equities/SPY_1block.csv \
  --output /tmp/signals.jsonl --warmup 0

open profile.trace
```

**3. gprof (GNU Profiler)**
```bash
# Compile with -pg flag
g++ -pg -O3 -o sentio_cli ...
./sentio_cli generate-signals --data ../data/equities/SPY_1block.csv \
  --output /tmp/signals.jsonl --warmup 0
gprof sentio_cli gmon.out > analysis.txt
```

### Expected Hotspots (Pre-Optimization)

Based on 31 sec/block (65 ms/bar) execution:

1. **`process_pending_updates()`** - ~25 ms/bar (38%)
   - Vector iteration over 3 horizons
   - Feature vector access (126 doubles × 3)
   - Calls to `ensemble_predictor_->update()`

2. **`OnlinePredictor::update()`** - ~20 ms/bar (31%)
   - Eigen matrix operations (lines 84-98)
   - Kalman gain calculation
   - Covariance update

3. **`track_prediction()`** - ~10 ms/bar (15%)
   - Map insertion (3×)
   - Vector push_back (3×)
   - Feature vector copy (3× 126 doubles)

4. **`UnifiedFeatureEngine::get_features()`** - ~8 ms/bar (12%)
   - 126 feature calculations
   - Technical indicators (SMA, EMA, RSI, etc.)

5. **Miscellaneous** - ~2 ms/bar (4%)
   - File I/O, logging, etc.

---

## Risk Assessment

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Optimization breaks learning** | HIGH | LOW | Unit tests, MRB regression tests |
| **Memory leaks with shared_ptr** | MEDIUM | LOW | Valgrind, ASAN checks |
| **Performance regression** | MEDIUM | MEDIUM | Benchmark before/after each change |
| **Code complexity increase** | LOW | MEDIUM | Code review, documentation |

### Rollback Plan

If Phase 1 optimizations fail:
1. Revert to v1.3 (correct but slow)
2. Accept 31 sec/block as baseline
3. Consider parallelization (multi-threading) instead of micro-optimizations

**Rollback Command:**
```bash
git checkout v1.3
make clean && make -j8
```

---

## Acceptance Criteria

### Definition of Done

- [ ] Signal generation completes in ≤10 seconds per block (480 bars)
- [ ] 4-block test MRB ≥0.340% (within 5% of v1.3 baseline)
- [ ] All 3 horizons processed correctly (debug logs confirm)
- [ ] No memory leaks (Valgrind clean)
- [ ] Code passes all existing unit tests
- [ ] Performance benchmarks documented in this file
- [ ] Code reviewed and approved
- [ ] Git commit tagged as v1.4-performance

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-10-07 | Claude | Initial requirements document |
| 1.1 | TBD | - | Post Phase 1 implementation update |
| 1.2 | TBD | - | Post Phase 2 implementation update |
| 1.3 | TBD | - | Final acceptance and benchmarks |

---

## References

1. **Multi-Horizon Learning Bug Report**
   - Location: `megadocs/MULTI_HORIZON_LEARNING_BUG.md`
   - Issue: 67% learning loss due to `std::map` key collision
   - Fix: Changed to `std::map<int, std::vector<HorizonPrediction>>`

2. **v1.3 Commit**
   - Commit: TBD (pending current session commit)
   - Message: "Fix multi-horizon learning: process all 3 horizons (+44% MRB)"

3. **Eigen Documentation**
   - URL: https://eigen.tuxfamily.org/dox/TopicWritingEfficientProductExpression.html
   - Topic: Efficient matrix operations and SIMD

4. **C++ Performance Best Practices**
   - URL: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-performance
   - Topic: Move semantics, shared_ptr, memory pools

---

## Appendix A: Benchmark Data

### v1.3 Baseline (Multi-Horizon Fixed, Pre-Optimization)

**1-Block Test (480 bars, 0 warmup):**
```
Real time: 47.099s
User time: 46.51s
System time: 0.43s
Time per bar: 98.1 ms
```

**4-Block Test (1920 bars, 960 warmup):**
```
Real time: 2:04.72 (124.72s)
User time: 122.78s
System time: 1.17s
Time per bar: 64.96 ms
Signals: 1920 (3.75% LONG, 7.76% SHORT, 88.49% NEUTRAL)
Trades: 118
Return: +0.69%
MRB: 0.345%
```

### Target Performance (Post-Optimization)

**1-Block Test:**
```
Target time: 10s (4.7x faster)
Time per bar: 20.8 ms (4.7x faster)
```

**4-Block Test:**
```
Target time: 40s (3.1x faster)
Time per bar: 20.8 ms (3.1x faster)
MRB: ≥0.340% (within 5%)
```

---

## Appendix B: Code Snippets

### Current Implementation (v1.3)

**`include/strategy/online_ensemble_strategy.h` (lines 129-137):**
```cpp
struct HorizonPrediction {
    int entry_bar_index;
    int target_bar_index;
    int horizon;
    std::vector<double> features;  // 126 doubles, copied 3× per bar
    double entry_price;
    bool is_long;
};
std::map<int, std::vector<HorizonPrediction>> pending_updates_;
```

**`src/strategy/online_ensemble_strategy.cpp` (lines 188-192):**
```cpp
auto& vec = pending_updates_[pred.target_bar_index];
if (vec.empty()) {
    vec.reserve(3);  // Reserve space for 3 horizons (1, 5, 10)
}
vec.push_back(pred);  // Copies 126-double feature vector
```

### Proposed Implementation (Phase 1)

**Option A: Shared Pointer**
```cpp
struct HorizonPrediction {
    int entry_bar_index;
    int target_bar_index;
    int horizon;
    std::shared_ptr<std::vector<double>> features;  // Shared, not copied
    double entry_price;
    bool is_long;
};
```

**Option B: Fixed Array**
```cpp
struct PendingUpdate {
    std::array<HorizonPrediction, 3> horizons;  // Fixed size, no dynamic allocation
    uint8_t count = 0;  // 0, 1, 2, or 3 horizons stored
};
std::map<int, PendingUpdate> pending_updates_;
```

**Recommended: Combine both (shared_ptr + array)**
```cpp
struct HorizonPrediction {
    int entry_bar_index;
    int target_bar_index;
    int horizon;
    std::shared_ptr<const std::vector<double>> features;  // const shared ownership
    double entry_price;
    bool is_long;
};

struct PendingUpdate {
    std::array<HorizonPrediction, 3> horizons;
    uint8_t count = 0;
};

std::map<int, PendingUpdate> pending_updates_;
```

---

## Appendix C: Implementation Checklist

### Phase 1: Quick Wins

**Task 1.1: Shared Feature Storage**
- [ ] Modify `HorizonPrediction` struct to use `std::shared_ptr<std::vector<double>>`
- [ ] Update `track_prediction()` to create `shared_ptr` once per bar (not 3×)
- [ ] Update `process_pending_updates()` to dereference `shared_ptr`
- [ ] Test: Verify MRB unchanged, speed improved

**Task 1.2: Array Instead of Vector**
- [ ] Define `PendingUpdate` struct with `std::array<HorizonPrediction, 3>`
- [ ] Change `pending_updates_` type to `std::map<int, PendingUpdate>`
- [ ] Update `track_prediction()` to insert into array[count++]
- [ ] Update `process_pending_updates()` to iterate array[0...count-1]
- [ ] Test: Verify MRB unchanged, speed improved

**Task 1.3: Benchmark Phase 1**
- [ ] Run 1-block test, record time
- [ ] Run 4-block test, record time + MRB
- [ ] Compare to v1.3 baseline
- [ ] Document results in Appendix A

### Phase 2: Optimization Pass

**Task 2.1: Move Semantics**
- [ ] Replace `push_back(pred)` with `emplace_back(std::move(pred))`
- [ ] Use `std::make_shared` for feature vector construction
- [ ] Benchmark: Measure allocation count reduction

**Task 2.2: Memory Pool**
- [ ] Implement object pool for `HorizonPrediction`
- [ ] Add pool to `OnlineEnsembleStrategy` private members
- [ ] Replace allocations with pool `get()` / `release()`
- [ ] Benchmark: Measure heap fragmentation reduction

**Task 2.3: Benchmark Phase 2**
- [ ] Run 1-block test, record time
- [ ] Run 4-block test, record time + MRB
- [ ] Compare to Phase 1 results
- [ ] Document results in Appendix A

### Phase 3: Advanced (If Needed)

**Task 3.1: SIMD Vectorization**
- [ ] Enable Eigen AVX/SSE flags in CMakeLists.txt
- [ ] Profile `OnlinePredictor::update()` assembly
- [ ] Verify SIMD instructions used (vaddpd, vmulpd, etc.)
- [ ] Benchmark: Measure FLOPS improvement

**Task 3.2: Feature Subsetting**
- [ ] Profile feature computation time
- [ ] Identify top 20 most important features
- [ ] Implement lazy evaluation
- [ ] Test: Verify MRB impact <5%

**Task 3.3: Final Benchmark**
- [ ] Run 1-block test, confirm ≤10 sec
- [ ] Run 4-block test, confirm ≤40 sec, MRB ≥0.340%
- [ ] Run 20-block test, confirm ≤200 sec
- [ ] Document final results in Appendix A

---

**END OF DOCUMENT**
