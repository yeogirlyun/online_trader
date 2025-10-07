# CAUTION: Performance Optimization & MRB Preservation

**Date:** October 7, 2025
**Priority:** CRITICAL
**Audience:** All developers working on performance optimization

---

## Core Principle

> **MRB (Mean Return per Block) is the supreme metric. Speed is secondary.**
>
> **Never sacrifice MRB for speed-up. A 10× faster system with lower MRB is a failed optimization.**

---

## Context: The v1.3 → v1.4 Journey

### v1.3: The Critical Fix (+44% MRB)
- **Issue:** System was losing 67% of learning updates (only 1 of 3 horizons processed)
- **Fix:** Changed from `map<int, HorizonPrediction>` to `map<int, vector<HorizonPrediction>>`
- **Result:**
  - ✅ MRB: 0.24% → **0.345%** (+44% improvement)
  - ⚠️ Speed: 16.9 sec/block → 31 sec/block (1.8× slower)

**Decision:** Accepted the 1.8× slowdown because MRB improved by 44%.

### v1.4: Safe Optimizations (+12% Speed)
- **Optimizations:**
  1. Shared feature storage with `shared_ptr`
  2. Fixed-size arrays (`std::array<HorizonPrediction, 3>`)
  3. Eigen zero-copy (`Map` instead of copy)
  4. Optimized matrix operations (`.noalias()`, reuse Px)

- **Result:**
  - ✅ MRB: **0.345%** (unchanged, correctness preserved)
  - ✅ Speed: 31 sec/block → 27.4 sec/block (12% faster)

**Decision:** Accepted because MRB unchanged AND speed improved.

---

## Optimization Classification

### Category A: SAFE Optimizations (Always Acceptable)

**Definition:** Optimizations that change HOW we compute, not WHAT we compute.

**Characteristics:**
- Same algorithm, same precision, same logic
- Only changes: memory layout, allocation strategy, loop ordering
- Output is **bit-for-bit identical** (or within floating-point epsilon)

**Examples:**
- ✅ Using `shared_ptr` to avoid copying features (v1.4)
- ✅ Using `std::array` instead of `std::vector` for fixed-size data (v1.4)
- ✅ Using `Eigen::Map` to avoid vector copy (v1.4)
- ✅ Reordering matrix operations (P - k*Px.T vs P - k*x.T*P) if mathematically equivalent
- ✅ Using `.noalias()` to disable aliasing checks (safe if no aliasing exists)
- ✅ Contiguous storage (`vector` instead of `map`) for O(1) access
- ✅ Pre-allocation (`reserve()`) to avoid reallocations
- ✅ Compiler flags: `-O3 -march=native -funroll-loops`
- ✅ Link-Time Optimization (LTO), Profile-Guided Optimization (PGO)

**Validation Required:**
- ✅ Unit tests pass
- ✅ MRB within ±0.5% of baseline (floating-point noise acceptable)
- ✅ 4-block test: MRB ≥ 0.340%

---

### Category B: RISKY Optimizations (Requires Extensive Validation)

**Definition:** Optimizations that change algorithm, precision, or numerical behavior.

**Characteristics:**
- Different algorithm (e.g., EWRLS → AdaGrad)
- Different precision (double → float)
- Different feature computation (batch → incremental)
- May produce **different predictions**, hence different MRB

**Examples:**
- ⚠️ Switching from EWRLS to AdaGrad logistic regression
- ⚠️ Switching from `double` to `float` precision
- ⚠️ Changing RSI from batch window to incremental Wilder smoothing
- ⚠️ Changing feature definitions or computation order
- ⚠️ Using `-ffast-math` (breaks IEEE 754 compliance)
- ⚠️ Changing learning rate, lambda, or other hyperparameters

**Validation Required:**
- ✅ 4-block test: MRB ≥ 0.340% (within 1% of 0.345%)
- ✅ 20-block test: MRB stable and ≥ baseline
- ✅ 100-block test: Long-term stability check
- ✅ A/B comparison: Old vs new side-by-side for 30+ days
- ✅ Statistical significance test (t-test, p < 0.05)

**Acceptance Criteria:**
- MRB must be ≥ 0.340% on ALL tests
- If MRB < 0.340% on ANY test → **REJECT optimization, revert to v1.4**

---

### Category C: FORBIDDEN Optimizations (Never Acceptable)

**Definition:** Optimizations that are known to degrade MRB.

**Examples:**
- ❌ Removing learning updates to save time
- ❌ Using fewer horizons (3 → 1) to reduce computation
- ❌ Skipping feature computation during warmup
- ❌ Approximating features with cached values from N bars ago
- ❌ Reducing feature count without validation
- ❌ Using stale data to avoid recomputation

**Why Forbidden:**
These directly reduce the quality or quantity of learning, guaranteed to hurt MRB.

---

## Decision Framework: Accept or Reject Optimization?

### Step 1: Classify the Optimization

| Question | Category A | Category B | Category C |
|----------|------------|------------|------------|
| Changes algorithm? | No | Yes | N/A |
| Changes precision? | No | Yes | N/A |
| Changes features? | No | Maybe | N/A |
| Removes learning? | No | No | **Yes** |
| Output identical? | Yes (±ε) | No | N/A |

### Step 2: Test Requirements

| Category | Unit Tests | 4-Block | 20-Block | 100-Block | A/B Test |
|----------|------------|---------|----------|-----------|----------|
| A (Safe) | Required | Required | Optional | Optional | No |
| B (Risky) | Required | Required | Required | Required | Required |
| C (Forbidden) | N/A | N/A | N/A | N/A | **REJECT** |

### Step 3: Acceptance Criteria

**Category A:**
```
IF unit_tests_pass AND mrb_4block >= 0.340:
    ACCEPT optimization
ELSE:
    REJECT optimization, investigate discrepancy
```

**Category B:**
```
IF unit_tests_pass AND
   mrb_4block >= 0.340 AND
   mrb_20block >= baseline_20block AND
   mrb_100block >= baseline_100block AND
   ab_test_pvalue < 0.05 AND ab_test_mrb_delta >= 0:
    ACCEPT optimization
ELSE:
    REJECT optimization, revert to v1.4
```

**Category C:**
```
REJECT immediately, do not implement
```

---

## Real-World Examples

### Example 1: Shared Pointer Optimization (v1.4) ✅

**Type:** Category A (Safe)

**Change:**
```cpp
// Before: Create 3 copies of features per bar
for (int horizon : {1, 5, 10}) {
    HorizonPrediction pred;
    pred.features = features;  // Copy 126 doubles
    // ...
}

// After: Share features via shared_ptr
auto shared_features = std::make_shared<const std::vector<double>>(features);
for (int horizon : {1, 5, 10}) {
    HorizonPrediction pred;
    pred.features = shared_features;  // Share pointer, no copy
    // ...
}
```

**Analysis:**
- ✅ Same features passed to predictor
- ✅ Same values, just different storage mechanism
- ✅ Output identical

**Validation:**
- ✅ 4-block test: 0.345% MRB (unchanged from v1.3)
- ✅ Speed: 12% improvement

**Decision:** ACCEPTED

---

### Example 2: AdaGrad Replacement (Proposed, Not Implemented) ⚠️

**Type:** Category B (Risky)

**Change:**
```cpp
// Before: EWRLS with 126×126 covariance matrix
OnlinePredictor (EWRLS, double precision)

// After: AdaGrad logistic regression
OnlineLogitF (AdaGrad, float precision)
```

**Analysis:**
- ⚠️ Different algorithm (EWRLS vs AdaGrad)
- ⚠️ Different precision (double vs float)
- ⚠️ Different learning dynamics
- ⚠️ **Output will differ**, MRB may change

**Required Validation:**
1. 4-block test
2. 20-block test
3. 100-block test
4. 30-day A/B test in paper trading
5. Statistical significance validation

**Estimated Effort:** 40-60 hours (implementation + testing)

**Decision:** DEFERRED (not worth risk without validation)

---

### Example 3: Feature Ring Buffer (My Implementation) ❌

**Type:** Category A (Safe) but **INEFFICIENT**

**Change:**
```cpp
// Current v1.4: 1 copy (features → shared_ptr)
shared_features = std::make_shared<const std::vector<double>>(features);

// Proposed: 2 copies (features → ring → shared_ptr)
feature_ring_.put(bar_index, features);  // Copy 1
auto shared_features = feature_ring_.get_shared(bar_index);  // Copy 2
```

**Analysis:**
- ✅ Same features, same values
- ❌ **Slower** than v1.4 (2 copies vs 1 copy)

**Decision:** REJECTED (slower, no benefit)

---

## Phase 2 Optimization Guidelines

From `megadocs/PHASE2_OPTIMIZATION_SUGGESTIONS.md`:

### Phase 2A: Feature Ring (Expert Suggestion) ⚠️

**Expert's Zero-Copy Approach:**
```cpp
// Store features as float32, return raw pointer
const float* feature_ring.get_ptr(bar_index);
```

**Issue:**
- Requires changing EWRLS interface from `vector<double>` to `const float*`
- Requires double → float precision change
- **Category B (Risky)** - needs MRB validation

**Decision:** Deferred to Phase 2C (requires algorithm rewrite)

### Phase 2B: Contiguous Updates (Expert Suggestion) ✅

**Change:**
```cpp
// Before: std::map<int, PendingUpdate>
// After: std::vector<InlinedVec<Update, 3>>
```

**Analysis:**
- ✅ Same updates, same order, same values
- ✅ Just different storage (contiguous vs tree)
- ✅ **Category A (Safe)**

**Decision:** Can implement if speed improvement needed

### Phase 2C: AdaGrad + Float32 (Expert Suggestion) ⚠️

**Analysis:**
- ⚠️ Different algorithm
- ⚠️ Different precision
- ⚠️ **Category B (Risky)**

**Decision:** Only implement if:
1. Willing to invest 40-60 hours in validation
2. Prepared to revert if MRB < 0.340%
3. 10 sec/block target is business-critical

---

## Testing Checklist

### For Category A (Safe) Optimizations

```bash
# 1. Build optimized version
make clean && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j8

# 2. Run 4-block test
./sentio_cli generate-signals --data ../data/equities/SPY_4blocks.csv \
  --output /tmp/signals_opt.jsonl --warmup 960

./sentio_cli execute-trades --signals /tmp/signals_opt.jsonl \
  --data ../data/equities/SPY_4blocks.csv \
  --output /tmp/trades_opt.jsonl --warmup 960

# Extract MRB (should be 0.69% return = 0.345% MRB)
grep "Total return" /tmp/trades_opt.jsonl

# 3. Compare signals (should be identical or within epsilon)
diff ../data/tmp/spy_4block_signals_eigen_opt.jsonl /tmp/signals_opt.jsonl

# 4. If MRB >= 0.340% and signals match → ACCEPT
# 5. If MRB < 0.340% or signals differ → INVESTIGATE or REJECT
```

### For Category B (Risky) Optimizations

**Same as Category A, PLUS:**

```bash
# 6. Run 20-block test
./sentio_cli generate-signals --data ../data/equities/SPY_20blocks.csv \
  --output /tmp/signals_20block.jsonl --warmup 960

# 7. Run 100-block test (if available)
./sentio_cli generate-signals --data ../data/equities/SPY_100blocks.csv \
  --output /tmp/signals_100block.jsonl --warmup 960

# 8. A/B test: Run both versions in parallel for 30+ days
# Compare MRB distributions, perform t-test

# 9. If p-value < 0.05 AND new_MRB >= old_MRB → ACCEPT
# 10. Otherwise → REJECT, revert to v1.4
```

---

## Red Flags: Signs an Optimization May Hurt MRB

🚩 **Immediate Rejection Indicators:**
- "Let's reduce the number of learning updates to save time"
- "We can skip warmup bars to speed up testing"
- "Approximating features with yesterday's values is good enough"
- "Using float16 will be 10× faster" (float16 has only 11-bit mantissa!)

🚩 **Investigate Further:**
- "This new algorithm is theoretically equivalent"
  - **Ask:** Has it been tested on our exact data?
- "Float32 is fine for most ML"
  - **Ask:** Have we validated MRB with float32 on our EWRLS?
- "We can cache features for 5 bars"
  - **Ask:** How does this affect feature freshness and learning?

🚩 **Requires Extensive Testing:**
- Any change to `online_predictor.cpp` (learning core)
- Any change to `unified_feature_engine.cpp` (feature computation)
- Any change to `online_ensemble_strategy.cpp` prediction logic

---

## Rollback Procedure

If an optimization causes MRB to drop below 0.340%:

```bash
# 1. Immediately stop using the optimized version
git log --oneline -5  # Find the commit before optimization

# 2. Revert to v1.4 (last known good version)
git checkout 409907b  # v1.4 commit hash
make clean && make -j8

# 3. Verify v1.4 MRB
./sentio_cli generate-signals --data ../data/equities/SPY_4blocks.csv \
  --output /tmp/signals_v14.jsonl --warmup 960
./sentio_cli execute-trades --signals /tmp/signals_v14.jsonl \
  --data ../data/equities/SPY_4blocks.csv \
  --output /tmp/trades_v14.jsonl --warmup 960

# Should show: Total return: 0.69%, MRB = 0.345%

# 4. Document why the optimization failed
echo "Optimization X reduced MRB from 0.345% to Y%" >> megadocs/FAILED_OPTIMIZATIONS.md

# 5. Analyze root cause before attempting alternative approach
```

---

## Summary: The Golden Rule

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│  IF (speed_improves AND mrb_unchanged):             │
│      ACCEPT optimization ✅                          │
│                                                      │
│  ELSE IF (speed_improves AND mrb_improves):         │
│      ACCEPT optimization ✅✅ (best case!)          │
│                                                      │
│  ELSE IF (speed_improves AND mrb_degrades):         │
│      REJECT optimization ❌                          │
│                                                      │
│  ELSE IF (speed_degrades AND mrb_improves):         │
│      ACCEPT optimization ✅ (v1.3 example)          │
│                                                      │
│  ELSE:                                              │
│      REJECT optimization ❌ (no benefit)            │
│                                                      │
└──────────────────────────────────────────────────────┘
```

**In other words:**

> **MRB is king. Speed is the servant.**
>
> If forced to choose, we choose MRB every time.

---

## Current Baseline (v1.4)

**Protection:** This version is KNOWN GOOD. All future optimizations are compared against v1.4.

```
Version: v1.4
Commit: 409907b
Date: October 7, 2025

Performance:
- 1-block: 42.8 sec
- 4-block: 109.8 sec (27.4 sec/block)
- MRB: 0.345% (4-block test, 2 warmup + 2 test)

Correctness:
- All 3 horizons processed ✅
- EWRLS learning with double precision ✅
- 126 features computed per bar ✅

Status: PRODUCTION BASELINE
```

**Do not modify v1.4 behavior without:**
1. Creating a new branch
2. Running full test suite
3. Documenting changes in this file
4. Getting explicit approval after MRB validation

---

## Contact & Escalation

**If unsure whether an optimization is safe:**

1. Check this document's classification (Category A/B/C)
2. If Category B, assume **HIGH RISK** until proven otherwise
3. Implement in a separate branch, never in main
4. Run full validation suite before merging
5. **When in doubt, preserve MRB over speed**

**Remember:** A 3× slower system with +44% MRB (v1.3) was accepted. A 3× faster system with -10% MRB would be rejected immediately.

---

**Last Updated:** October 7, 2025
**Status:** ACTIVE POLICY
**Version:** 1.0

---

## Appendix: Historical Decisions

| Version | Change | Speed Impact | MRB Impact | Decision | Rationale |
|---------|--------|--------------|------------|----------|-----------|
| v1.2 | EOD closing | Neutral | Neutral | ✅ Accept | Risk management |
| v1.3 | Multi-horizon fix | -1.8× | +44% | ✅ Accept | MRB gain worth slowdown |
| v1.4 | Shared ptr + arrays | +12% | 0% | ✅ Accept | Speed gain, MRB unchanged |
| Feature ring (my impl) | 2 copies | -10% est | 0% | ❌ Reject | Slower than v1.4 |
| AdaGrad (proposed) | +200% est | Unknown | ⚠️ Defer | Requires 40-60h validation |

---

**END OF DOCUMENT**
