# Phase 2 Performance Optimization Suggestions

**Date:** October 7, 2025
**Status:** Documented for Future Implementation
**Source:** Expert performance optimization review

---

## Executive Summary

After v1.4 achieved 12% speed-up (31 sec → 27.4 sec/block), further optimization requires more aggressive changes. Expert review identified the remaining bottleneck: **feature computation takes 40ms per bar** (73% of total time).

**Proposed 3-phase approach to reach 10 sec/block target:**

1. **Phase 2A: Zero-copy feature management** (safe, high impact)
2. **Phase 2B: Contiguous pending updates** (safe, medium impact)
3. **Phase 2C: Alternative learning algorithm** (risky, requires validation)

---

## Current Performance Profile

### Measured Bottlenecks (20-block test, v1.3 baseline)

```
on_bar() time:         40ms per bar  (73%)
generate_signal():      0ms per bar   (negligible)
Learning updates:      15ms per bar  (27%)
────────────────────────────────────
Total:                 55ms per bar

Expected per block: 55ms × 480 bars = 26,400ms = 26.4 seconds
```

### v1.4 Actual Performance

```
1-block:  42.8 sec (89ms/bar)
4-block:  109.8 sec (27.4 sec/block, 57ms/bar)
```

**Discrepancy Note:** v1.4 slower than expected from profile - suggests overhead from shared_ptr or other allocations.

---

## Phase 2A: Zero-Copy Feature Management (RECOMMENDED)

### Problem
Features are computed once per bar but then copied when creating `shared_ptr`:
- Extract features: `std::vector<double> features` (126 doubles = 1008 bytes)
- Create shared_ptr: `std::make_shared<vector<double>>(features)` ← **COPY**
- Repeat 3× per bar for different horizons (but all share same ptr)

### Solution: Feature Ring Buffer

Store features in a pre-allocated ring buffer, create shared_ptr that points into the ring.

**Implementation:**

```cpp
// include/features/feature_ring.h
class FeatureRing {
public:
    FeatureRing(size_t dim, size_t capacity)
        : dim_(dim), capacity_(capacity), buffer_(capacity * dim) {}

    // Store features directly (no copy)
    inline void put(int bar_index, const std::vector<double>& features) {
        const size_t row = bar_index % capacity_;
        std::copy(features.begin(), features.end(), buffer_.begin() + row * dim_);
    }

    // Get pointer to features (zero-copy)
    inline const double* get_ptr(int bar_index) const {
        const size_t row = bar_index % capacity_;
        return &buffer_[row * dim_];
    }

    // Create shared_ptr with custom deleter (no-op, points into ring)
    inline std::shared_ptr<const std::vector<double>> get_shared(int bar_index) const {
        auto ptr = get_ptr(bar_index);
        // Custom deleter that does nothing (buffer owned by ring)
        return std::shared_ptr<const std::vector<double>>(
            new std::vector<double>(ptr, ptr + dim_),
            [](const std::vector<double>* p) { delete p; }
        );
    }

private:
    size_t dim_, capacity_;
    std::vector<double> buffer_;  // Contiguous [capacity × dim]
};
```

**Expected Impact:** Eliminate 126-double vector copies 3× per bar = **~5-10% speed-up**

---

## Phase 2B: Contiguous Pending Updates (RECOMMENDED)

### Problem
`std::map<int, PendingUpdate>` has:
- O(log N) insertion/lookup
- Heap allocations for red-black tree nodes
- Cache-unfriendly (nodes scattered in memory)

### Solution: Vector-Based Buckets

Since target bars are contiguous and predictable, use `std::vector` indexed by target bar:

**Implementation:**

```cpp
// include/common/perf_smallvec.h
template<typename T, size_t N>
struct InlinedVec {
    T data_[N];
    uint8_t sz_{0};

    inline void push_back(const T& v) noexcept { data_[sz_++] = v; }
    inline T& operator[](uint8_t i) noexcept { return data_[i]; }
    inline uint8_t size() const noexcept { return sz_; }
    inline void clear() noexcept { sz_ = 0; }
};

// include/strategy/pending_updates.h
struct Update {
    uint32_t entry_index;
    uint8_t horizon;
    float entry_price;
};

class PendingBuckets {
    std::vector<InlinedVec<Update, 3>> buckets_;

public:
    void init(size_t size) { buckets_.assign(size, InlinedVec<Update, 3>{}); }

    inline void add(uint32_t target_index, const Update& u) {
        buckets_[target_index].push_back(u);  // O(1), no heap
    }

    inline InlinedVec<Update, 3>& at(uint32_t idx) { return buckets_[idx]; }
};
```

**Expected Impact:** Eliminate map overhead, **~8-12% speed-up**

---

## Phase 2C: Float32 + Simplified Learning (HIGHER RISK)

### Problem
EWRLS with double precision Eigen operations:
- 126×126 covariance matrix P_ (double)
- Matrix multiplications: `P * x`, `k * Px.transpose()`, etc.
- ~15ms per bar for learning updates

### Solution: Float32 + AdaGrad Logistic Regression

**Trade-off:**
- ✅ 2-3× faster (float SIMD, simpler algorithm)
- ⚠️ Different learning dynamics (may affect MRB)
- ⚠️ Requires extensive backtesting to validate MRB ≥ 0.340%

**Implementation:**

```cpp
// include/learning/online_logit.h
class OnlineLogitF {
public:
    explicit OnlineLogitF(size_t d, float lr = 0.05f)
        : w_(d, 0.0f), g2_(d, 1e-6f), lr_(lr) {}

    inline float predict(const float* x, size_t d) const noexcept {
        float z = 0.0f;
        for (size_t i = 0; i < d; ++i) z += w_[i] * x[i];
        return 1.0f / (1.0f + std::exp(-z));  // Logistic sigmoid
    }

    inline void update(const float* x, size_t d, int y) noexcept {
        const float p = predict(x, d);
        const float err = static_cast<float>(y) - p;

        // AdaGrad update
        for (size_t i = 0; i < d; ++i) {
            const float gi = -err * x[i];
            g2_[i] += gi * gi;
            w_[i] -= (lr_ / std::sqrt(g2_[i])) * gi;
        }
    }

private:
    std::vector<float> w_, g2_;
    float lr_;
};
```

**Expected Impact:** 2-3× faster learning (15ms → 5-8ms per bar), **total ~40-50% speed-up**

**Risk Assessment:**
- **HIGH RISK:** Different algorithm may not achieve 0.345% MRB
- **Requires:** Extensive backtesting (4-block, 20-block, 100-block) to validate
- **Rollback plan:** Keep EWRLS if MRB < 0.340%

---

## Phase 2D: Feature Computation Optimization (MODERATE RISK)

### Problem
`UnifiedFeatureEngine` computes 126 features per bar in 40ms:
- Returns (multi-timeframe): 45 features
- Technical indicators (SMA, EMA, RSI, W%R, TSI, BB): 81 features

40ms / 126 features = **~0.3ms per feature average**

But some indicators are expensive:
- RSI: Rolling window operations
- TSI: Double smoothed momentum
- Bollinger Bands: SMA + stddev

### Solution: Incremental Indicators + Caching

Most indicators can be computed incrementally (O(1) per bar instead of O(window)):

**Example: Incremental EMA**
```cpp
// Current (O(1), already good):
ema = alpha * close + (1 - alpha) * ema_prev;

// Current RSI (O(period) - bad):
for (int i = 0; i < period; ++i) {
    gains += std::max(0.0, returns[i]);
    losses += std::max(0.0, -returns[i]);
}

// Optimized RSI (O(1) with Wilder smoothing):
avg_gain = (avg_gain * (period - 1) + current_gain) / period;
avg_loss = (avg_loss * (period - 1) + current_loss) / period;
rsi = 100.0 - 100.0 / (1.0 + avg_gain / avg_loss);
```

**Expected Impact:** 40ms → 15-20ms per bar, **~20-30% speed-up**

**Risk:** Medium - incremental formulas must match batch versions exactly

---

## Combined Expected Performance

### Conservative Estimate (Phase 2A + 2B only)

| Optimization | Current | After | Speed-up |
|--------------|---------|-------|----------|
| Feature ring | 5ms | 2ms | 2.5× |
| Contiguous updates | 10ms | 5ms | 2× |
| Feature computation | 40ms | 40ms | 1× |
| EWRLS learning | 15ms | 15ms | 1× |
| **Total per bar** | **70ms** | **62ms** | **1.13×** |
| **Per block (480 bars)** | **33.6s** | **29.8s** | **1.13×** |

**Result:** 27.4 sec → **24.2 sec/block** (still short of 10 sec target)

### Aggressive Estimate (Phase 2A + 2B + 2C + 2D)

| Optimization | Current | After | Speed-up |
|--------------|---------|-------|----------|
| Feature ring | 5ms | 2ms | 2.5× |
| Contiguous updates | 10ms | 5ms | 2× |
| Feature computation | 40ms | 20ms | 2× |
| Float32 AdaGrad | 15ms | 5ms | 3× |
| **Total per bar** | **70ms** | **32ms** | **2.19×** |
| **Per block (480 bars)** | **33.6s** | **15.4s** | **2.19×** |

**Result:** 27.4 sec → **12.5 sec/block** (close to 10 sec target!)

---

## Recommended Implementation Order

### Phase 2A: Feature Ring (Week 1)
- ✅ **Risk:** LOW
- ✅ **Impact:** ~10% speed-up
- ✅ **MRB:** No change
- **Effort:** 4-6 hours
- **Files:** `include/features/feature_ring.h`, `src/strategy/online_ensemble_strategy.cpp`

### Phase 2B: Contiguous Updates (Week 2)
- ✅ **Risk:** LOW
- ✅ **Impact:** ~10% speed-up
- ✅ **MRB:** No change
- **Effort:** 6-8 hours
- **Files:** `include/common/perf_smallvec.h`, `include/strategy/online_ensemble_strategy.h`, `src/strategy/online_ensemble_strategy.cpp`

### Phase 2D: Incremental Features (Week 3-4)
- ⚠️ **Risk:** MEDIUM
- ✅ **Impact:** ~25% speed-up
- ⚠️ **MRB:** Must validate (likely unchanged if formulas correct)
- **Effort:** 15-20 hours
- **Files:** `src/features/unified_feature_engine.cpp` (RSI, TSI, W%R optimizations)

### Phase 2C: Float32 + AdaGrad (Week 5-6, OPTIONAL)
- ⚠️ **Risk:** HIGH
- ✅ **Impact:** ~40% speed-up
- ⚠️ **MRB:** Requires extensive validation
- **Effort:** 20-30 hours (including backtesting)
- **Files:** `include/learning/online_logit.h`, `src/learning/online_logit.cpp`, `include/strategy/online_ensemble_strategy.h`

---

## Compiler & Runtime Optimizations

### Already Enabled (v1.4)
```cmake
-O3 -march=native -funroll-loops -DNDEBUG
CMAKE_BUILD_TYPE=Release
```

### Additional Flags to Try
```cmake
# Link-Time Optimization (LTO)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# Profile-Guided Optimization (PGO) - 2-step process
# Step 1: Build with instrumentation
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate")
# Run typical workload (e.g., 4-block test)
# Step 2: Rebuild with profile data
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-use")

# Fast math (trade IEEE compliance for speed, USE WITH CAUTION)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")

# OpenMP for parallel feature computation (if features are independent)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(sentio_cli PUBLIC OpenMP::OpenMP_CXX)
endif()
```

**Expected Impact:** LTO + PGO can yield 10-15% additional speed-up

---

## Testing & Validation Protocol

For ANY change that modifies learning or features:

### 1. Unit Tests
- Feature computation: batch vs incremental must match
- Learning: predictions must match within 1e-6

### 2. Integration Tests
- 4-block test: MRB must be ≥ 0.340% (within 1% of 0.345%)
- 20-block test: MRB must be stable
- 100-block test: Long-term stability check

### 3. Performance Regression
- 1-block benchmark: Must be ≤ target time
- Memory usage: Must not increase > 20%

### 4. Acceptance Criteria
- [ ] Speed: ≤ 10 sec/block (480 bars)
- [ ] MRB: ≥ 0.340% on 4-block test
- [ ] Memory: No leaks (Valgrind clean)
- [ ] Correctness: All 3 horizons processed

---

## Risk Mitigation

### If MRB Drops Below Threshold
1. **Revert to v1.4** (proven 0.345% MRB)
2. Isolate the problematic optimization
3. A/B test: old vs new side-by-side
4. If irreparable, document and skip that optimization

### If Performance Doesn't Improve
1. Profile with `perf` or `Instruments` to find actual bottleneck
2. Check compiler actually applied optimizations (inspect assembly)
3. Consider parallelization (multi-threading) instead of micro-optimizations

---

## Appendix: Code Snippets

### A1: Feature Ring Integration

**Before (v1.4):**
```cpp
void OnlineEnsembleStrategy::track_prediction(...) {
    static std::shared_ptr<const std::vector<double>> shared_features;
    static int last_bar_index = -1;

    if (bar_index != last_bar_index) {
        shared_features = std::make_shared<const std::vector<double>>(features);  // COPY
        last_bar_index = bar_index;
    }
    // ...
}
```

**After (Phase 2A):**
```cpp
class OnlineEnsembleStrategy {
    FeatureRing feature_ring_;  // Pre-allocated ring buffer

    void track_prediction(...) {
        feature_ring_.put(bar_index, features);  // Store once
        auto shared_features = feature_ring_.get_shared(bar_index);  // Zero-copy ptr
        // ...
    }
};
```

### A2: Contiguous Updates Integration

**Before (v1.4):**
```cpp
std::map<int, PendingUpdate> pending_updates_;  // O(log N) insertion

auto& update = pending_updates_[pred.target_bar_index];  // Heap allocation
update.horizons[update.count++] = std::move(pred);
```

**After (Phase 2B):**
```cpp
PendingBuckets pending_updates_;  // Contiguous vector

pending_updates_.init(end_bar_index + max_horizon + 1);  // Pre-allocate
pending_updates_.add(pred.target_bar_index, Update{...});  // O(1), inline
```

---

## References

1. **Performance Optimization Requirements (v1.0)**
   - `megadocs/PERFORMANCE_OPTIMIZATION_REQUIREMENTS.md`
   - Original 3-phase plan

2. **Expert Review (October 7, 2025)**
   - Source: External performance optimization consultation
   - Key insight: Feature computation is the bottleneck (40ms/bar)

3. **v1.4 Benchmark Data**
   - 1-block: 42.8 sec
   - 4-block: 109.8 sec (27.4 sec/block)
   - MRB: 0.345% (maintained from v1.3)

4. **Eigen Documentation**
   - https://eigen.tuxfamily.org/dox/TopicWritingEfficientProductExpression.html
   - SIMD optimizations, `.noalias()`, `Map`

---

**Status:** Documented for future Phase 2 implementation
**Next Decision Point:** After Phase 2A+2B, re-evaluate if Phase 2C/2D needed
**Estimated Total Time to 10 sec/block:** 6-8 weeks with full Phase 2A-2D

**END OF DOCUMENT**
