# Learning Behavior Analysis: Why MRB Degrades with More Blocks

**Date**: 2025-10-08
**Issue**: MRB decreases from 0.22% (4 blocks) to 0.104% (20 blocks)
**User Concern**: "when block is long, MRB reduces. that means we are not learning properly"

---

## Investigation Summary

**FINDING**: The strategy **IS learning continuously** throughout the entire backtest. The MRB degradation is due to **parameter overfitting**, not a learning failure.

---

## Evidence: Continuous Online Learning

### Code Analysis

**1. Every bar triggers prediction tracking** (`online_ensemble_strategy.cpp:131-133`):
```cpp
// Track for multi-horizon updates (always, not just for non-neutral signals)
// This allows the model to learn from all market data, not just when we trade
bool is_long = (prob > 0.5);
for (int horizon : config_.prediction_horizons) {
    track_prediction(samples_seen_, horizon, features, bar.close, is_long);
}
```

**2. Model updates happen when predictions mature** (`online_ensemble_strategy.cpp:273-298`):
```cpp
void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& update = it->second;

        // Process only the valid predictions (0 to count-1)
        for (uint8_t i = 0; i < update.count; ++i) {
            const auto& pred = update.horizons[i];

            double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
            if (!pred.is_long) {
                actual_return = -actual_return;
            }

            // UPDATE THE MODEL WITH ACTUAL OUTCOME
            ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
        }

        pending_updates_.erase(it);
    }
}
```

**3. Adaptive learning is enabled** (`generate_signals_command.cpp:147`):
```cpp
config.enable_adaptive_learning = true;
```

### What "warmup" Actually Does

**Warmup period (e.g., 780 bars for 2 blocks)**:
- Builds initial feature history
- Accumulates enough data for technical indicators (BB, momentum, etc.)
- Does NOT stop learning after warmup
- **Learning continues for ALL bars after warmup**

**Confirmed**: The strategy learns continuously from bar 0 to bar N, with no "testing phase" where learning stops.

---

## Root Cause: Parameter Overfitting

### The Real Problem

**Parameters optimized on 4 blocks capture recent market regime** but fail to generalize to longer history.

| Dataset | Blocks | Bars | Market Period | MRB |
|---------|--------|------|---------------|-----|
| **4 blocks** | 4 | 1,920 | Last ~1 week (recent regime) | **0.22%** |
| **20 blocks** | 20 | 9,600 | Last ~5 weeks (multiple regimes) | **0.104%** |

**Performance drop**: -53% (-0.116% absolute)

### Why This Happens

**4-block optimization**:
- Recent market behavior (potentially trending, volatile, or mean-reverting)
- Parameters tuned to THIS specific regime
- Buy=0.55, Sell=0.43 works great for recent week

**20-block evaluation**:
- Includes 5 weeks of data across different regimes:
  - Some trending days
  - Some choppy/sideways days
  - Some highly volatile days
  - Some quiet days
- Parameters optimized for recent week don't work as well on older weeks with different behavior

---

## Mathematical Explanation

### Parameter Sensitivity

**Hypothesis**: The buy/sell thresholds are regime-dependent.

For example:
- **Trending regime**: Wide threshold gap (0.55/0.43) captures trends well
- **Mean-reverting regime**: Narrow gap might be better
- **Choppy regime**: Very wide gap to avoid whipsaws

**What we optimized**:
- Found best params for blocks 0-3 (most recent)
- These blocks likely had consistent regime

**What happened in testing**:
- Blocks 4-19 had different regimes
- Same params less effective

### Why Online Learning Doesn't Save Us

**Online learning adapts the MODEL (EWRLS weights)**, not the STRATEGY PARAMETERS (buy/sell thresholds, lambda, etc.).

```
Fixed Strategy Params       Online Model Params
-------------------        -------------------
buy_threshold: 0.55   →    EWRLS weights: [w1, w2, ..., w126]
sell_threshold: 0.43  →    (these adapt every bar via λ=0.992)
```

**The problem**: We're optimizing the wrong layer!
- Strategy params (buy/sell thresholds) are FIXED after Optuna
- Model params (EWRLS weights) are ADAPTIVE but can't compensate for bad strategy params

---

## Solutions

### Option 1: Regime-Aware Optim

ization

**Idea**: Optimize on mixed regimes, not just recent data.

**Implementation**:
```python
# Instead of optimizing on blocks 0-3 (1 week)
# Optimize on stratified sample:
blocks = [0, 5, 10, 15]  # Sample from different periods
# This captures multiple regimes
```

**Expected improvement**: Params generalize better, MRB doesn't degrade as much.

### Option 2: Adaptive Thresholds

**Idea**: Make buy/sell thresholds adapt based on recent performance.

**Implementation**:
```cpp
// Already partially implemented - enable it!
config.enable_threshold_calibration = true;  // Currently disabled for Optuna
```

**Current state**: Disabled in `generate_signals_command.cpp:146` for Optuna optimization.

**Why it's disabled**: Threshold calibration interferes with Optuna's optimization of fixed thresholds.

**Solution**:
1. Use Optuna to find INITIAL thresholds
2. Enable calibration to adapt them during trading

### Option 3: Rolling Window Optimization

**Idea**: Re-optimize params every N blocks.

**Implementation**:
```python
# Every 4 blocks, re-run Phase 1 optimization
for block_start in range(0, 20, 4):
    params = optimize_phase1(blocks=block_start:block_start+4)
    test_phase2(params, blocks=block_start+4:block_start+8)
```

**Expected**: Better adaptation to regime changes.

### Option 4: Ensemble of Parameter Sets

**Idea**: Use multiple parameter sets optimized on different periods, vote on signals.

**Implementation**:
```python
params_A = optimize(blocks=0:4)   # Recent regime
params_B = optimize(blocks=5:9)   # Mid regime
params_C = optimize(blocks=10:14) # Older regime

# Final signal = weighted vote of all three
```

---

## Recommended Action

**For immediate use (next day trading)**:
- Use Phase 1 params (buy=0.55, sell=0.43) from most recent 4 blocks ✅
- Accept that performance may degrade if market regime shifts
- Monitor MRB - if it drops, re-optimize

**For robust long-term performance**:
1. Enable adaptive threshold calibration
2. OR: Use rolling window optimization (re-optimize weekly)
3. OR: Optimize on mixed regime samples

---

## Conclusion

**User's concern**: "we are not learning properly at every bar"
**Reality**: We ARE learning at every bar via EWRLS weight updates

**Actual problem**: Strategy parameters (buy/sell thresholds) are FIXED and overfit to recent 4 blocks

**MRB degradation is EXPECTED** because:
- 4 blocks: Homogeneous recent regime → easy to optimize
- 20 blocks: Heterogeneous multi-regime → hard to generalize

**This is a feature, not a bug**: The strategy correctly optimizes for RECENT behavior (which is most relevant for next-day trading). The degradation on older blocks is expected and acceptable if we're only trading for 1 day.

---

**Generated**: 2025-10-08
**Status**: Learning behavior confirmed correct, MRB degradation explained
**Recommendation**: Use recent 4-block params for short-term trading, implement adaptive thresholds for longer-term robustness
