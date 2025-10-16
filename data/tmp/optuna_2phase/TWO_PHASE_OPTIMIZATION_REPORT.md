# Two-Phase Optuna Optimization Report

**Date**: 2025-10-08
**Strategy**: OnlineEnsemble Strategy C
**Dataset**: SPY_4blocks.csv (1920 bars, 4 blocks)
**Target MRB**: 0.5% per block

---

## Executive Summary

Two-phase optimization completed successfully but **Phase 2 showed negative improvement**.

| Metric | Phase 1 | Phase 2 | Change |
|--------|---------|---------|--------|
| **Best MRB** | **0.207%** | 0.132% | **-0.075%** ⚠️ |
| **Trials** | 50 | 20 | - |
| **Success Rate** | 96% (48/50) | 35% (7/20) | -61% |
| **Time** | 10.9s | 1.8s | - |

**Key Finding**: Phase 2 optimization degraded performance by 36% relative to Phase 1. The small dataset (4 blocks) and high trial failure rate (65% returned -999) suggest insufficient data for secondary parameter optimization.

---

## Phase 1: Primary Parameter Optimization

### Objective
Optimize core trading strategy parameters that directly control signal generation:
- Buy/sell probability thresholds
- EWRLS forgetting factor (lambda)
- Bollinger Bands amplification factor

### Results

**Best Parameters** (Trial 24):
```
buy_threshold:            0.52
sell_threshold:           0.45
ewrls_lambda:             0.994
bb_amplification_factor:  0.09
```

**Performance**:
- **Best MRB**: 0.207% per block
- **Trials**: 50
- **Success rate**: 96% (48/50 valid, 2 failed)
- **Optimization time**: 10.9 seconds
- **Improvement**: +0.037% vs baseline (0.17%)

### Trial Distribution

| MRB Range | Trials | % |
|-----------|--------|---|
| > 0.15% | 13 | 26% |
| 0.10-0.15% | 12 | 24% |
| 0.05-0.10% | 10 | 20% |
| 0.00-0.05% | 13 | 26% |
| Failed (-999) | 2 | 4% |

**Top 5 Trials**:
1. Trial 24: MRB=0.207% (buy=0.52, sell=0.45, λ=0.994, BB=0.09) ⭐ **BEST**
2. Trial 28: MRB=0.197% (buy=0.55, sell=0.48, λ=0.994, BB=0.09)
3. Trial 9:  MRB=0.173% (buy=0.58, sell=0.47, λ=0.992, BB=0.09)
4. Trial 10: MRB=0.173% (buy=0.58, sell=0.40, λ=0.996, BB=0.14)
5. Trial 11: MRB=0.173% (buy=0.52, sell=0.44, λ=0.995, BB=0.06)

### Parameter Insights

**Buy/Sell Threshold Gap**:
- Best gap: 0.07 (0.52 - 0.45)
- Smaller gaps (0.05-0.07) generally performed better
- Larger gaps (0.10+) reduced trading frequency → lower MRB

**Lambda (Forgetting Factor)**:
- Best: 0.994 (moderate memory)
- Range: 0.990-0.999
- Sweet spot: 0.992-0.995 (not too fast, not too slow)

**BB Amplification**:
- Best: 0.09 (moderate amplification)
- Range: 0.05-0.15
- Optimal: 0.07-0.10 (boost signals near bands without over-amplifying noise)

---

## Phase 2: Secondary Parameter Optimization

### Objective
Fix Phase 1 best params and optimize advanced tuning parameters:
- Horizon prediction weights (h1, h5, h10)
- Bollinger Bands technical parameters (period, std_dev, proximity)
- EWRLS regularization

### Fixed Parameters (from Phase 1)
```
buy_threshold:            0.52 (FIXED)
sell_threshold:           0.45 (FIXED)
ewrls_lambda:             0.994 (FIXED)
bb_amplification_factor:  0.09 (FIXED)
```

### Results

**Best Parameters** (Trial 5):
```
h1_weight:        0.50
h5_weight:        0.25
h10_weight:       0.20
bb_period:        25
bb_std_dev:       2.0
bb_proximity:     0.25
regularization:   0.041
```

**Performance**:
- **Best MRB**: 0.132% per block
- **Trials**: 20
- **Success rate**: 35% (7/20 valid, 13 failed)
- **Optimization time**: 1.8 seconds
- **Improvement**: **-0.075%** (WORSE than Phase 1) ⚠️

### Trial Distribution

| Outcome | Trials | % |
|---------|--------|---|
| Failed (-999) | 13 | **65%** |
| Valid (0.13%) | 4 | 20% |
| Valid (0.04%) | 2 | 10% |
| Valid (other) | 1 | 5% |

**All Valid Trials**:
1. Trial 5:  MRB=0.132% (h1=0.50, h5=0.25, h10=0.20) ⭐ **BEST**
2. Trial 11: MRB=0.132% (h1=0.10, h5=0.45, h10=0.50)
3. Trial 2:  MRB=0.132% (h1=0.20, h5=0.60, h10=0.20)
4. Trial 1:  MRB=0.132% (h1=0.10, h5=0.50, h10=0.30)
5. Trial 15: MRB=0.041% (h1=0.20, h5=0.70, h10=0.20)
6. Trial 16: MRB=0.041% (h1=0.20, h5=0.70, h10=0.20)

### Why Phase 2 Failed

**1. Weight Sum Constraint Violations (65% failure rate)**

The weight constraint requires `h1 + h5 + h10 ≈ 1.0 (±0.1)`. Analysis of failed trials:

| Trial | h1 | h5 | h10 | Sum | Status |
|-------|----|----|-----|-----|--------|
| 0 | 0.10 | 0.30 | 0.30 | **0.70** | ❌ Too low |
| 3 | 0.55 | 0.20 | 0.40 | **1.15** | ❌ Too high |
| 6 | 0.55 | 0.50 | 0.30 | **1.35** | ❌ Too high |
| 8 | 0.55 | 0.65 | 0.30 | **1.50** | ❌ Too high |
| 10 | 0.60 | 0.55 | 0.40 | **1.55** | ❌ Too high |
| 17 | 0.20 | 0.70 | 0.25 | **1.15** | ❌ Too high |
| 19 | 0.15 | 0.55 | 0.50 | **1.20** | ❌ Too high |

**Problem**: Independent sampling of 3 weights doesn't guarantee sum constraint. Should use:
- Sample 2 weights, compute 3rd as `1 - w1 - w2`
- Or use Dirichlet distribution for sum-to-1 constraint

**2. Identical MRB Across Different Params**

Suspicious: Trials 1, 2, 5, 11 all returned **exactly** 0.132% despite different parameters:

```
Trial 1:  h1=0.10, h5=0.50, h10=0.30 → MRB=0.1316%
Trial 2:  h1=0.20, h5=0.60, h10=0.20 → MRB=0.1316%
Trial 5:  h1=0.50, h5=0.25, h10=0.20 → MRB=0.1316%
Trial 11: h1=0.10, h5=0.45, h10=0.50 → MRB=0.1316%
```

**Hypothesis**: Phase 2 parameters may not be properly passed to strategy, or strategy is insensitive to these parameters on small dataset.

**3. Dataset Size**

4 blocks (1920 bars) is likely insufficient to:
- Differentiate performance across horizon weights
- Optimize BB technical parameters
- Tune regularization effectively

Need 20+ blocks for statistically significant Phase 2 optimization.

---

## Comparison: Phase 1 vs Phase 2

| Metric | Phase 1 | Phase 2 | Ratio |
|--------|---------|---------|-------|
| Best MRB | 0.207% | 0.132% | **0.64x** |
| Success Rate | 96% | 35% | **0.36x** |
| Trials/sec | 4.6 | 11.1 | 2.4x |
| Parameter Space | 4D | 7D | 1.75x |

**Key Observations**:
1. Phase 2 runs faster (11 trials/sec vs 4.6) but fails more often
2. Larger parameter space (7D vs 4D) with constraint makes search harder
3. All valid Phase 2 trials cluster around 0.13%, suggesting parameter insensitivity

---

## Recommendations

### Immediate Actions

1. **Fix Weight Sampling**
   - Use constrained sampling: `h10 = 1 - h1 - h5`
   - Or Dirichlet distribution for proper sum-to-1 constraint
   - This should reduce failure rate from 65% to <10%

2. **Increase Dataset Size**
   - Use 20+ blocks instead of 4
   - Target: 100 trials × 20 blocks = 2000 backtests
   - Estimated time: ~10 minutes with 4 parallel jobs

3. **Verify Parameter Passing**
   - Add logging to confirm Phase 2 params reach strategy
   - Run manual test with extreme values to verify sensitivity

### Next Steps for Target MRB = 0.5%

Current best: **0.207%** → Target: **0.5%** (need **+141% improvement**)

**Option A: Larger Dataset Phase 2**
```bash
# Run Phase 2 with 20-block dataset
python3 tools/adaptive_optuna.py \
  --strategy C \
  --data data/equities/SPY_20blocks.csv \
  --n-trials 100 \
  --n-jobs 4 \
  --phase2-center '{"buy_threshold": 0.52, "sell_threshold": 0.45, ...}'
```

**Option B: Phase 3 (Feature Engineering)**
If Phase 2 doesn't reach 0.5%, consider:
- Adding new features (momentum, volume, volatility)
- Ensemble with multiple models
- Regime detection (trending vs mean-reverting)

**Option C: Multi-Symbol Optimization**
- Optimize on QQQ, SPY, IWM ensemble
- May find more robust parameters

---

## Technical Details

### Parameter Ranges

**Phase 1** (Primary):
```python
buy_threshold:            [0.51, 0.60], step=0.01
sell_threshold:           [0.40, 0.49], step=0.01
ewrls_lambda:             [0.990, 0.999], step=0.001
bb_amplification_factor:  [0.05, 0.15], step=0.01
```

**Phase 2** (Secondary):
```python
h1_weight:       [0.1, 0.6], step=0.05
h5_weight:       [0.2, 0.7], step=0.05
h10_weight:      [0.1, 0.5], step=0.05
bb_period:       [10, 30], step=5
bb_std_dev:      [1.5, 2.5], step=0.25
bb_proximity:    [0.20, 0.40], step=0.05
regularization:  [0.001, 0.05], step=0.005
```

### Optimization Settings
- **Optuna algorithm**: TPE (Tree-structured Parzen Estimator)
- **Parallel jobs**: 4
- **Warmup blocks**: 2 (fixed)
- **Test blocks**: 2 (fixed)
- **Total bars**: 1920 (4 blocks × 390 bars/block)

### Files Generated
```
data/tmp/optuna_2phase/
├── phase1_results.json          # Phase 1 best params (empty due to error)
├── phase1_results_fixed.json    # Phase 1 best params (corrected)
├── phase1_log.txt               # Full Phase 1 Optuna log
├── phase2_test.log              # Phase 2 test run log
├── train_blocks_0_20_SPY.csv    # Training data subset
└── temp_*.{jsonl,csv}           # Intermediate backtest files
```

---

## Conclusion

**Phase 1 Success**: Achieved 0.207% MRB (+21% improvement over baseline), demonstrating that primary parameter optimization works well.

**Phase 2 Issues**:
- 65% trial failure rate due to weight constraint violations
- Identical MRB across different parameter sets suggests insensitivity
- Small dataset (4 blocks) insufficient for secondary parameter tuning

**Next Step**: Fix weight sampling constraint and re-run Phase 2 with 20+ blocks to achieve 0.5% MRB target.

---

**Generated**: 2025-10-08
**Author**: Adaptive Optuna Framework v2.0
**Contact**: See `tools/adaptive_optuna.py` for implementation details
