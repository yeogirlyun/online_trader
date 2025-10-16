# PSM Asymmetric Thresholds: Detailed Explanation

**Date**: 2025-10-08
**Purpose**: Explain symmetric vs asymmetric probability thresholds and their impact on trading performance

---

## 1. What Are Probability Thresholds?

The OnlineEnsemble strategy generates a **probability** (0.0 to 1.0) that the next bar will go up:
- **0.0** = 100% certain price will go DOWN
- **0.5** = neutral (50/50 coin flip)
- **1.0** = 100% certain price will go UP

We use **thresholds** to convert this probability into trading signals:

```cpp
SignalType determine_signal(double probability) const {
    if (probability > buy_threshold) {
        return SignalType::LONG;  // Go long
    } else if (probability < sell_threshold) {
        return SignalType::SHORT; // Go short
    } else {
        return SignalType::NEUTRAL; // Stay in cash
    }
}
```

---

## 2. Symmetric vs Asymmetric Thresholds

### 2.1 Symmetric Thresholds (Default/Naive)

**Configuration**:
```cpp
buy_threshold = 0.53;   // Need 53% probability to go LONG
sell_threshold = 0.47;  // Need <47% probability to go SHORT
```

**Visual representation**:
```
Probability:  0.0    0.3    0.47   0.50   0.53    0.7    1.0
              |------|------|------|------|------|------|------|
Signal:       SHORT  SHORT  NEUTRAL     NEUTRAL  LONG   LONG
                           ← 6% gap →
```

**Characteristics**:
- **Centered around 0.50** (neutral point)
- **Equal distance** from center (0.03 above, 0.03 below)
- **Assumption**: Market is perfectly balanced (equal probability of going up/down)

**Current system default**: 0.53 / 0.47 (symmetric around 0.50)

---

### 2.2 Asymmetric Thresholds (v1.0 Discovery)

**Configuration** (from v1.0 that achieved 16.7% return):
```cpp
buy_threshold = 0.55;   // Need 55% probability to go LONG
sell_threshold = 0.45;  // Need <45% probability to go SHORT
```

**Visual representation**:
```
Probability:  0.0    0.3    0.45   0.50   0.55    0.7    1.0
              |------|------|------|------|------|------|------|
Signal:       SHORT  SHORT  NEUTRAL     NEUTRAL  LONG   LONG
                           ← 10% gap →
```

**Characteristics**:
- **STILL centered around 0.50** (neutral point)
- **Wider neutral zone** (0.10 vs 0.06)
- **More selective** on both sides

**Key difference**: Asymmetric doesn't mean "off-center" - it means **different selectivity for LONG vs SHORT**.

---

### 2.3 True Asymmetric Thresholds (Off-Center)

**Example configuration** (hypothetical):
```cpp
buy_threshold = 0.60;   // Need 60% probability to go LONG
sell_threshold = 0.35;  // Need <35% probability to go SHORT
```

**Visual representation**:
```
Probability:  0.0    0.35   0.47    0.50  0.60    0.7    1.0
              |------|------|--------|-----|------|------|------|
Signal:       SHORT  SHORT  NEUTRAL  NEUTRAL  NEUTRAL LONG LONG
                           ← 25% gap, off-center →
```

**Characteristics**:
- **NOT centered** (center would be at 0.475)
- **LONG requires higher confidence** (0.60 vs 0.55)
- **SHORT is easier** (0.35 vs 0.45)
- **Use case**: When model is better at predicting shorts than longs (or vice versa)

---

## 3. Why v1.0 Used 0.55 / 0.45 (Wider Symmetric)

### 3.1 The Problem v1.0 Was Solving

**Baseline performance** (default 0.50/0.50):
- **Win rate imbalance**: LONG 48.6% vs SHORT 52.7%
- **MRB**: 0.0746% (poor)
- **Issue**: Trading too frequently with low confidence signals

**Analysis**: The model was generating too many marginal signals near 0.50, leading to:
1. Excessive trading (high commission costs)
2. Low-confidence whipsaws
3. Win rate below 50%

### 3.2 Phase 1: Move to 0.55 / 0.45

**Change**: Increase selectivity on BOTH sides equally
- `buy_threshold`: 0.50 → 0.55 (+0.05)
- `sell_threshold`: 0.50 → 0.45 (-0.05)
- **Neutral zone**: 0.0% → 10%

**Results**:
- **MRB**: 0.0746% → 0.3970% (**+5.3x improvement!**)
- **Win rate**: Improved to 60%+ (both LONG and SHORT)
- **Trade frequency**: Reduced by ~40%
- **Trade quality**: Only high-confidence signals

**Key insight**: The 10% neutral zone **filtered out noise** and kept only strong signals.

---

## 4. Current System: Adaptive Calibration

### 4.1 How It Works

Our current system **starts with symmetric thresholds** but **adapts asymmetrically** based on performance:

**Initial values** (online_ensemble_strategy.h:50-51):
```cpp
double buy_threshold = 0.53;   // Initial buy threshold
double sell_threshold = 0.47;  // Initial sell threshold
```

**Adaptive calibration** (online_ensemble_strategy.cpp:326-354):
```cpp
void OnlineEnsembleStrategy::calibrate_thresholds() {
    // Calculate win rate over recent trades
    double win_rate = calculate_win_rate();

    if (win_rate < target_win_rate) {
        // Win rate too low -> make thresholds MORE selective (move apart)
        current_buy_threshold_ += threshold_step;   // Increase (harder to go LONG)
        current_sell_threshold_ -= threshold_step;  // Decrease (harder to go SHORT)
    } else if (win_rate > target_win_rate + 0.05) {
        // Win rate too high -> trade MORE (move together)
        current_buy_threshold_ -= threshold_step;   // Decrease (easier to go LONG)
        current_sell_threshold_ += threshold_step;  // Increase (easier to go SHORT)
    }

    // Clamp to reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation (4% neutral zone)
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }
}
```

**Example evolution**:
```
Bar 100:  buy=0.53, sell=0.47 (start)
Bar 300:  buy=0.56, sell=0.44 (adapted wider after low win rate)
Bar 500:  buy=0.54, sell=0.46 (narrowed after high win rate)
Bar 700:  buy=0.58, sell=0.42 (adapted wider again)
```

### 4.2 Why This Is Better Than Fixed Thresholds

**Advantages**:
1. **Adapts to market regime**: Bull markets may need different thresholds than bear markets
2. **Learns from mistakes**: If win rate drops, automatically becomes more selective
3. **Prevents overfitting**: Doesn't hardcode magic numbers like 0.55/0.45
4. **Self-correcting**: Bounces back if it becomes too conservative or aggressive

**Disadvantages**:
1. **Slower to find optimal**: Takes time to calibrate
2. **May oscillate**: Can bounce between too aggressive and too conservative
3. **Harder to reproduce**: Different runs may converge to different thresholds

---

## 5. True Asymmetry: Different Selectivity for LONG vs SHORT

### 5.1 When Would We Use Off-Center Thresholds?

**Scenario 1: Model has LONG bias**
- Model is better at predicting rallies than crashes
- **Solution**: Make SHORT require higher confidence
  ```cpp
  buy_threshold = 0.55;   // Standard selectivity for LONG
  sell_threshold = 0.40;  // More selective for SHORT (need <40%)
  ```

**Scenario 2: Market has structural upward drift** (SPY long-term trend)
- Market goes up 55% of the time historically
- **Solution**: Make LONG easier, SHORT harder
  ```cpp
  buy_threshold = 0.52;   // Easier to go LONG (drift)
  sell_threshold = 0.42;  // Harder to go SHORT (against drift)
  ```

**Scenario 3: WIN rate imbalance**
From v1.0 analysis (megadocs/V1_0_CONFIGURATION_ANALYSIS.md:154):
```
Win rate imbalance: LONG 48.6% < SHORT 52.7%
```

This suggests the model was **better at predicting SHORT** than LONG.

**Asymmetric solution**:
```cpp
buy_threshold = 0.58;   // MORE selective for LONG (compensate for low win rate)
sell_threshold = 0.45;  // LESS selective for SHORT (model is good at this)
```

---

## 6. Comparison: v1.0 vs Current

| Aspect | v1.0 (Fixed) | Current (Adaptive) |
|--------|--------------|---------------------|
| **Initial buy threshold** | 0.55 | 0.53 |
| **Initial sell threshold** | 0.45 | 0.47 |
| **Neutral zone** | 10% (fixed) | 6% (adaptive 4-16%) |
| **Symmetry** | Symmetric (centered at 0.50) | Starts symmetric, can become asymmetric |
| **Adaptation** | None | Calibrates every 200 bars |
| **Bounds** | None | buy ∈ [0.51, 0.70], sell ∈ [0.30, 0.49] |
| **Target win rate** | None | 60% (self-adjusts) |
| **Performance** | 0.6086% MRB (20 blocks) | -0.125% MRB (4 blocks, schema mismatch) |

---

## 7. Optuna Optimization: What It's Searching For

The current Optuna optimization is searching for **optimal initial thresholds** and **whether to use adaptive calibration**.

**Search space** (from tools/adaptive_optuna.py):
```python
def suggest_params(trial):
    return {
        'buy_threshold': trial.suggest_float('buy_threshold', 0.51, 0.65),
        'sell_threshold': trial.suggest_float('sell_threshold', 0.35, 0.49),
        'enable_threshold_calibration': trial.suggest_categorical('enable_threshold_calibration', [True, False]),
        'threshold_step': trial.suggest_float('threshold_step', 0.001, 0.01),
        # ... other params
    }
```

**Possible outcomes**:
1. **Wider symmetric** (like v1.0): `buy=0.58, sell=0.42`
2. **Asymmetric off-center**: `buy=0.62, sell=0.40`
3. **Narrow adaptive**: `buy=0.52, sell=0.48, enable_calibration=True`
4. **Wide fixed**: `buy=0.60, sell=0.40, enable_calibration=False`

Optuna will test ~200 combinations and find what works best with the **current 45-feature set**.

---

## 8. Recommendations

### 8.1 For Current Optimization

**Hypothesis**: With current's complete feature set, we may need **different thresholds** than v1.0 because:
1. **Professional indicators** (MACD, Stochastic, Williams%R) may produce more confident predictions
2. **Fixed `price.return_1`** removes noise that v1.0 had
3. **Bollinger Bands in features** (not just amplification) may improve boundary detection

**Expected optimal range**:
- `buy_threshold`: 0.54-0.60 (slightly higher than v1.0's 0.55)
- `sell_threshold`: 0.40-0.46 (slightly higher than v1.0's 0.45)
- `neutral_zone`: 8-14% (similar to v1.0's 10%)

### 8.2 Consider Win Rate Analysis

After Optuna completes, analyze:
```bash
./sentio_cli analyze-trades \
    --trades data/tmp/optuna_best_trades.jsonl \
    --data data/equities/SPY_30blocks.csv \
    --json | jq '.signal_accuracy'
```

**If we see**:
```json
{
  "LONG": {"win_rate": 0.52, "count": 450},
  "SHORT": {"win_rate": 0.64, "count": 380}
}
```

**Then use asymmetric thresholds**:
- LONG has 52% win rate → need higher threshold (e.g., 0.58)
- SHORT has 64% win rate → can use lower threshold (e.g., 0.42)

### 8.3 Test Off-Center Asymmetry

After Optuna, manually test:
```bash
# Test 1: LONG-biased (easier to go long)
./sentio_cli backtest --data data/equities/SPY_20blocks.csv \
    --params '{"buy_threshold": 0.52, "sell_threshold": 0.43}'

# Test 2: SHORT-biased (easier to go short)
./sentio_cli backtest --data data/equities/SPY_20blocks.csv \
    --params '{"buy_threshold": 0.58, "sell_threshold": 0.40}'

# Test 3: v1.0 thresholds (for comparison)
./sentio_cli backtest --data data/equities/SPY_20blocks.csv \
    --params '{"buy_threshold": 0.55, "sell_threshold": 0.45}'
```

---

## 9. Key Takeaways

### ✅ What "Asymmetric" Means in v1.0 Context

**NOT off-center** - v1.0 used 0.55/0.45, which is **symmetric around 0.50**.

The term "asymmetric" in the v1.0 docs refers to:
1. **Different from default** (0.50/0.50 → 0.55/0.45)
2. **Non-zero neutral zone** (0% → 10%)
3. **Selective on both sides** (not trading near 0.50)

### ✅ Current System Is More Sophisticated

- **Starts symmetric** (0.53/0.47)
- **Adapts asymmetrically** if needed (based on win rate)
- **Can become off-center** if LONG and SHORT have different accuracies
- **Self-calibrates** to target 60% win rate

### ✅ Why v1.0's 0.55/0.45 Worked

1. **10% neutral zone** filtered out low-confidence signals
2. **Reduced trade frequency** by 40% (fewer commission costs)
3. **Improved win rate** from ~48% to 60%
4. **Allowed 3-bar hold period** to work (fewer whipsaws)

The **width of the neutral zone** (0.10) was more important than the exact values (0.55/0.45).

---

## 10. Action Items

- [x] Explained symmetric vs asymmetric thresholds
- [x] Documented v1.0's 0.55/0.45 configuration
- [x] Explained current adaptive calibration system
- [ ] **Wait for Optuna results** to see optimal thresholds with current features
- [ ] **Analyze win rate by signal type** to determine if true asymmetry is needed
- [ ] **Test off-center thresholds** if LONG and SHORT have different accuracies

---

**Status**: PSM threshold mechanics fully documented. Awaiting Optuna optimization to determine optimal configuration for current 45-feature set.
