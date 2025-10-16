# v1.0 Configuration Analysis: Best Performance Setup

**Date**: 2025-10-08
**Purpose**: Document v1.0 configuration that achieved 16.7% return on 20 blocks, 6% on 5 blocks

---

## Executive Summary

v1.0 achieved **8.16x MRB improvement** (0.0746% → 0.6086% per block) through systematic optimization, equivalent to **10.5% monthly returns** (~125% annualized).

### Key Performance Metrics
- **20-block test**: 16.7% return (0.835% MRB)
- **5-block test**: 6% return (1.2% MRB)
- **Testing data**: 292,385 bars QQQ 1-min (2019-2024)
- **Trades executed**: 491,410

---

## 1. OnlineEnsemble Strategy Parameters (v1.0)

From commit `95c2b88` (Release v1.0):

```cpp
struct OnlineEnsembleConfig {
    // EWRLS Core
    double ewrls_lambda = 0.995;                    // Forgetting factor
    double initial_variance = 100.0;                // Parameter uncertainty
    double regularization = 0.01;                   // L2 regularization
    int warmup_samples = 100;                       // Min samples before trading

    // Multi-horizon Ensemble
    vector<int> prediction_horizons = {1, 5, 10};   // Prediction bars ahead
    vector<double> horizon_weights = {0.3, 0.5, 0.2}; // Ensemble weights

    // Adaptive Learning
    bool enable_adaptive_learning = true;
    double min_lambda = 0.990;                      // Fast adaptation limit
    double max_lambda = 0.999;                      // Slow adaptation limit

    // Signal Thresholds
    double buy_threshold = 0.53;                    // LONG entry probability
    double sell_threshold = 0.47;                   // SHORT entry probability
    double neutral_zone = 0.06;                     // Width of neutral zone

    // Bollinger Bands Amplification
    bool enable_bb_amplification = true;
    int bb_period = 20;
    double bb_std_dev = 2.0;
    double bb_proximity_threshold = 0.30;           // Within 30% of band
    double bb_amplification_factor = 0.10;          // +10% probability boost

    // Adaptive Calibration
    bool enable_threshold_calibration = true;
    int calibration_window = 200;
    double target_win_rate = 0.60;
    double threshold_step = 0.005;

    // Risk Management
    bool enable_kelly_sizing = true;
    double kelly_fraction = 0.25;                   // 25% of full Kelly
    double max_position_size = 0.50;                // Max 50% capital

    int performance_window = 200;
    double target_monthly_return = 0.10;            // 10% monthly target
};
```

---

## 2. Position State Machine (PSM) Configuration

### Asymmetric Probability Thresholds (Phase 1 improvement)
From v1.0 commit message:

```
LONG thresholds:
  - 0.55 (1x QQQ)
  - 0.60 (blend between 1x and 3x)
  - 0.68 (3x TQQQ)

SHORT thresholds:
  - 0.45 (-1x PSQ)
  - 0.35 (blend between -1x and -2x)
  - 0.32 (-2x SQQQ)
```

### Risk Management (Phase 2 improvements)
- **3-bar minimum hold**: Prevents whipsaws
- **Profit target**: +2.0% (tight)
- **Stop loss**: -1.5% (tight)
- **Instruments**: QQQ, TQQQ (3x bull), PSQ (-1x bear), SQQQ (-2x bear)

---

## 3. Feature Set Analysis

### Feature Engine Status at v1.0

**Total claimed**: 126 features
**Actually implemented**: ~72 features (57% complete)
**Status**: **INCOMPLETE IMPLEMENTATION**

#### What Was Implemented (from code analysis):

**Incremental Calculators**:
- SMA: periods {5, 10, 20, 50, 100, 200}
- EMA: periods {5, 10, 20, 50}
- RSI: (basic implementation)

**Feature Categories** (partial):
1. **Time features** (8): hour_sin, hour_cos, minute_sin, minute_cos, dow_sin, dow_cos, dom_sin, dom_cos
2. **Price action**: Returns, log returns, OHLC ratios
3. **Moving averages**: SMA/EMA combinations
4. **Momentum**: RSI, basic price momentum
5. **Volatility**: ATR estimates
6. **Volume**: Basic volume features
7. **Statistical**: Lags, correlations (partial)
8. **Pattern**: Candlestick patterns (doji, hammer, shooting star, engulfing)

### Critical Finding: Feature Engine Was a Stub!

The v1.0 `get_feature_names()` method returned **only 8 time features** plus placeholders:

```cpp
std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());

    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos",
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});

    // Add more feature names as needed...
    // (This is a simplified version for now)  ← PLACEHOLDER!

    return names;
}
```

**Analysis**: The 126-feature claim was **aspirational**, not actual. The EWRLS model likely trained on a much smaller, **incomplete** feature set with many zeros/placeholders.

---

## 4. Performance Evolution (Phases 1-3)

### Baseline (Original)
- **MRB**: 0.0746%
- **Config**: Default thresholds (0.50/0.50), no hold period, wide targets

### Phase 1: Asymmetric Thresholds
- **MRB**: 0.3970% (+5.3x improvement)
- **Change**: LONG ≥0.55, SHORT ≤0.45
- **Rationale**: Address win rate imbalance (LONG 48.6% < SHORT 52.7%)

### Phase 2: Hold Period + Tight Targets (FINAL v1.0)
- **MRB**: 0.6086% (+8.2x improvement from baseline)
- **Changes**:
  - 3-bar minimum hold period
  - Profit target: 2.0%
  - Stop loss: -1.5%
- **Monthly return**: 10.5% (~125% annual)

### Phase 3: Complex Optimizations (ALL FAILED)
Attempted but **reverted** due to performance degradation:
- ✗ Instrument-specific targets: 0% improvement
- ✗ Multi-timeframe filters: -62% to -83%
- ✗ Regime detection: -187% (catastrophic)
- ✗ Dynamic position sizing: -184%

**Key Learning**: Simple, consistent rules >> complex adaptive systems for 1-min intraday

---

## 5. Comparison: v1.0 vs Current (Fixed Features)

| Aspect | v1.0 (Best Performance) | Current (Fixed return_1) |
|--------|------------------------|--------------------------|
| **Features** | ~8-72 (incomplete, claimed 126) | 45 (complete, production-grade) |
| **EWRLS Lambda** | 0.995 | 0.995 (same) |
| **Buy Threshold** | 0.53 | 0.53 (default) |
| **Sell Threshold** | 0.47 | 0.47 (default) |
| **BB Amplification** | 0.10 | 0.10 (default) |
| **MRB (4-block test)** | +0.47% (estimated) | -0.125% ⚠️ |
| **Feature Quality** | Incomplete/placeholders | Complete/correct ✅ |
| **price.return_1** | BROKEN (always 0) | FIXED ✅ |

---

## 6. Critical Insights

### Why v1.0 Performed Well Despite Broken Features

1. **Incomplete features forced simpler model**: Less overfitting risk
2. **Broken `price.return_1`** acted as **implicit regularization**
3. **Time features dominated**: 8 properly implemented time-of-day features may have been sufficient for 1-min intraday patterns
4. **PSM thresholds did heavy lifting**: Asymmetric thresholds (0.55/0.45) and tight risk controls (2%/-1.5%) were the real performance drivers

### Why Current System Shows Worse Performance

1. **Schema mismatch**: Fixed feature set incompatible with old hyperparameters
2. **Different feature space**: 45 complete features ≠ 8-72 incomplete features
3. **Needs re-optimization**: Parameters tuned for broken features won't work with correct ones

---

## 7. Recommendations

### Immediate Action: Re-optimize with Fixed Features

Run Optuna with:
- ✅ **Fixed `price.return_1`** (correct 1-bar returns)
- ✅ **45 complete features** (production-grade V2)
- ✅ **Strategy C** (static baseline)
- 🎯 **Find new optimal**: `buy_threshold`, `sell_threshold`, `ewrls_lambda`, `bb_amplification_factor`

### Testing Strategy

1. **Baseline test**: Use v1.0 parameters on fixed features (for comparison)
2. **Optimization**: Run 200 trials on 20 blocks
3. **Validation**: Test on remaining blocks
4. **Target**: Match or exceed v1.0's 0.6086% MRB

### Hypothesis

The v1.0 success was **NOT due to the 126-feature engine** (which was incomplete), but rather:
- Asymmetric probability thresholds
- 3-bar hold period
- Tight profit/stop targets
- Time-of-day features

With proper features and re-optimized parameters, we should achieve **better** performance than v1.0.

---

## 8. Action Items

- [ ] Complete Optuna optimization (Strategy C, 30 blocks)
- [ ] Compare optimized parameters vs v1.0 defaults
- [ ] Test if time features still matter (analyze feature importance)
- [ ] Consider removing incomplete features, focusing on the 45 production-grade ones
- [ ] Document new optimal configuration as v2.0

---

**Status**: v1.0 analysis complete. Awaiting Optuna optimization results to establish new baseline with corrected features.
