# Optuna MRB Optimization Requirements & Parameter Ranges

**Date:** 2025-10-08
**Objective:** Define proper parameter settings and ranges for Optuna to optimize OnlineEnsembleStrategy (OES) on Mean Return per Block (MRB)
**Current Baseline:** MRB = -0.165% per block (4-block test, 2-block warmup)

---

## 1. OPTIMIZATION OBJECTIVE

### Primary Metric: Mean Return per Block (MRB)
```
MRB = Total Return % / Number of Test Blocks
```

**Why MRB over Total Return:**
- Normalizes performance across different test periods
- Allows fair comparison between 10-block vs 100-block runs
- Represents expected return per trading day (480 bars = 1 day)
- More stable metric than total return for parameter selection

**Target:** Maximize MRB while maintaining:
- Win rate ≥ 55%
- Max drawdown ≤ 5%
- Trade frequency: 50-200% per block (reasonable activity)

---

## 2. PARAMETER CATEGORIES & RANGES

### Category A: EWRLS Learning Parameters (HIGH PRIORITY)
These control how fast/slow the model adapts to new data.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `ewrls_lambda` | 0.995 | 0.990 | 0.999 | float | Forgetting factor: higher = slower adaptation |
| `initial_variance` | 100.0 | 10.0 | 500.0 | float | Initial parameter uncertainty |
| `regularization` | 0.01 | 0.001 | 0.1 | float | L2 penalty to prevent overfitting |
| `warmup_samples` | 100 | 50 | 200 | int | Bars before trading starts |

**Rationale:**
- `ewrls_lambda`: 0.995 means ~140 bars half-life. SPY volatility changes over weeks/months, so range 0.990-0.999 covers 70-700 bars.
- `initial_variance`: Higher = more aggressive initial learning. Range allows 10x variation.
- `regularization`: Prevents feature weights from exploding. Log-scale search recommended.
- `warmup_samples`: Too low = noisy predictions, too high = missed opportunities.

---

### Category B: Multi-Horizon Ensemble (MEDIUM PRIORITY)
Controls how different time horizons are weighted.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `horizon_1_weight` | 0.3 | 0.1 | 0.5 | float | Weight for 1-bar predictor |
| `horizon_5_weight` | 0.5 | 0.2 | 0.6 | float | Weight for 5-bar predictor |
| `horizon_10_weight` | 0.2 | 0.1 | 0.4 | float | Weight for 10-bar predictor |

**Constraint:** `horizon_1_weight + horizon_5_weight + horizon_10_weight = 1.0`

**Rationale:**
- Current weights favor 5-bar (medium-term) predictions
- 1-bar: captures immediate momentum
- 5-bar: ~1 hour of trading (most reliable?)
- 10-bar: ~2 hours (longer trends)

**Alternative Approach:** Make horizons themselves tunable:
- `horizon_1`: 1-3 bars
- `horizon_2`: 5-10 bars
- `horizon_3`: 10-20 bars

---

### Category C: Signal Generation Thresholds (HIGH PRIORITY)
Controls when LONG/SHORT signals are triggered.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `buy_threshold` | 0.53 | 0.51 | 0.60 | float | Probability threshold for LONG |
| `sell_threshold` | 0.47 | 0.40 | 0.49 | float | Probability threshold for SHORT |

**Constraint:** `buy_threshold > 0.50 > sell_threshold` and `buy_threshold - sell_threshold >= 0.02`

**Rationale:**
- Current neutral zone: 0.47-0.53 (6% wide)
- Wider neutral zone = fewer trades, higher quality
- Narrower neutral zone = more trades, potentially noisier
- Asymmetric thresholds possible (e.g., 0.55/0.48 if market has upward bias)

---

### Category D: Bollinger Bands Amplification (MEDIUM PRIORITY)
Boosts signal strength near BB extremes.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `bb_period` | 20 | 10 | 50 | int | BB calculation window |
| `bb_std_dev` | 2.0 | 1.5 | 3.0 | float | Standard deviations for bands |
| `bb_proximity_threshold` | 0.30 | 0.20 | 0.50 | float | Within X% of band triggers boost |
| `bb_amplification_factor` | 0.10 | 0.05 | 0.20 | float | How much to boost probability |

**Rationale:**
- BB amplification should help catch mean-reversion opportunities
- `bb_period=20`: Standard setting, but 10-50 covers intraday to multi-day
- `bb_std_dev=2.0`: Standard 95% coverage, but 1.5-3.0 allows tuning sensitivity
- Current amplification is conservative (0.10), range allows 0.05-0.20

---

### Category E: Adaptive Calibration (LOW PRIORITY - Consider Disabling)
Auto-adjusts thresholds based on recent win rate.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `enable_threshold_calibration` | true | - | - | bool | Enable/disable feature |
| `calibration_window` | 200 | 100 | 500 | int | Bars for calibration |
| `target_win_rate` | 0.60 | 0.55 | 0.65 | float | Target accuracy |
| `threshold_step` | 0.005 | 0.002 | 0.010 | float | Adjustment size |

**WARNING:** This feature may interfere with Optuna optimization by changing thresholds during test period.

**Recommendation:** DISABLE during optimization (`enable_threshold_calibration=false`), then re-enable if needed.

---

### Category F: Risk Management (MEDIUM PRIORITY)
Controls position sizing and Kelly criterion.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `kelly_fraction` | 0.25 | 0.10 | 0.50 | float | Fraction of Kelly to use |
| `max_position_size` | 0.50 | 0.25 | 1.00 | float | Max capital per position |

**Rationale:**
- `kelly_fraction=0.25`: Conservative (1/4 Kelly). Range 0.10-0.50 covers very conservative to half-Kelly.
- `max_position_size=0.50`: Allows 50% of capital per trade. SPY is liquid, so could go higher, but keep 0.25-1.0 for safety.

---

## 3. PARAMETER INTERACTION ANALYSIS

### Critical Interactions:

**1. `ewrls_lambda` ↔ `warmup_samples`**
- Higher lambda (slower learning) → needs more warmup
- Lower lambda (faster learning) → can use less warmup
- **Suggestion:** If `ewrls_lambda > 0.997`, require `warmup_samples >= 150`

**2. `buy_threshold` ↔ `sell_threshold` ↔ `bb_amplification_factor`**
- Higher BB amplification → can use stricter thresholds (higher buy, lower sell)
- Wider neutral zone → less dependent on BB amplification
- **Suggestion:** Sample `neutral_zone_width = buy_threshold - sell_threshold` as single parameter (0.02-0.15)

**3. `horizon_weights` ↔ `ewrls_lambda`**
- Longer horizons (10-bar) work better with higher lambda (smoother trends)
- Shorter horizons (1-bar) work better with lower lambda (faster reaction)
- **Suggestion:** Adjust per-horizon lambda based on horizon length

**4. `kelly_fraction` ↔ Signal Quality**
- If win rate is high (>60%), can increase Kelly fraction
- If win rate is low (<55%), should decrease Kelly fraction
- **Suggestion:** Make Kelly fraction adaptive to recent win rate

---

## 4. OPTUNA SEARCH STRATEGY

### Phase 1: Coarse Grid Search (50 trials)
Explore wide ranges to identify promising regions.

```python
# Optuna trial suggestions
lambda_val = trial.suggest_float('ewrls_lambda', 0.990, 0.999)
buy_thresh = trial.suggest_float('buy_threshold', 0.51, 0.60)
sell_thresh = trial.suggest_float('sell_threshold', 0.40, 0.49)
bb_amp = trial.suggest_float('bb_amplification_factor', 0.05, 0.20)
kelly_frac = trial.suggest_float('kelly_fraction', 0.10, 0.50)

# Ensure constraint
if buy_thresh - sell_thresh < 0.02:
    raise optuna.TrialPruned()
```

### Phase 2: Fine-Tuning (100 trials)
Narrow ranges around top 10 parameter sets from Phase 1.

```python
# Example: If Phase 1 found best lambda=0.995, narrow to 0.993-0.997
lambda_val = trial.suggest_float('ewrls_lambda', 0.993, 0.997)
buy_thresh = trial.suggest_float('buy_threshold', 0.52, 0.56)
# ... etc
```

### Phase 3: Validation (20 trials)
Test top 3 parameter sets on different time periods (walk-forward).

---

## 5. EVALUATION PROTOCOL

### Test Configuration:
- **Data:** SPY_RTH_NH_cpp.bin (2020-2025, 1018 blocks total)
- **Train Period:** First 800 blocks (80%)
- **Test Period:** Last 218 blocks (20%)
- **Warmup:** 10 blocks before each test period
- **Objective:** Maximize MRB on test period

### Optimization Objective Function:
```python
def objective(trial):
    # Sample parameters
    params = sample_parameters(trial)

    # Run backtest
    mrb, win_rate, max_dd, trades_per_block = run_backtest(params)

    # Penalty for poor constraints
    penalty = 0
    if win_rate < 0.50:
        penalty += (0.50 - win_rate) * 10  # Severe penalty for <50% win rate
    if max_dd > 0.10:
        penalty += (max_dd - 0.10) * 5     # Penalty for >10% drawdown
    if trades_per_block < 50 or trades_per_block > 300:
        penalty += 0.01  # Light penalty for extreme trade frequency

    # Return MRB with penalties
    return mrb - penalty
```

### Constraints to Enforce:
1. **Minimum trades:** ≥50 trades total (avoid overly conservative configs)
2. **Win rate:** ≥50% (break-even baseline)
3. **Max drawdown:** ≤10% (risk management)
4. **Valid parameter ranges:** As defined above

---

## 6. RECOMMENDED PARAMETER PRIORITIES

### Tier 1 (Optimize First):
1. `buy_threshold` / `sell_threshold` - Controls trade frequency and quality
2. `ewrls_lambda` - Controls learning speed
3. `bb_amplification_factor` - Signal enhancement

### Tier 2 (Optimize Second):
4. `horizon_weights` - Multi-horizon blending
5. `kelly_fraction` - Position sizing
6. `regularization` - Overfitting control

### Tier 3 (Fine-tune Last):
7. `bb_period`, `bb_std_dev` - BB parameters
8. `warmup_samples` - Warmup length
9. `initial_variance` - Initial learning aggressiveness

### Disabled During Optimization:
- `enable_threshold_calibration` = false (interferes with optimization)
- `enable_adaptive_learning` = true (keep enabled, part of EWRLS design)

---

## 7. BASELINE EXPERIMENT DESIGN

### Experiment 1: Threshold-Only Optimization
Fix all other parameters, optimize only `buy_threshold` and `sell_threshold`.

**Purpose:** Establish baseline improvement from signal filtering alone.

### Experiment 2: EWRLS-Only Optimization
Fix thresholds, optimize `ewrls_lambda`, `regularization`, `initial_variance`.

**Purpose:** Measure impact of learning rate tuning.

### Experiment 3: Full Optimization
Optimize all Tier 1 + Tier 2 parameters simultaneously.

**Purpose:** Find global optimum.

### Experiment 4: Ensemble Weights
Fix best params from Exp 3, optimize `horizon_weights`.

**Purpose:** Test if ensemble weighting matters.

---

## 8. SUCCESS CRITERIA

### Minimum Viable Improvement:
- MRB improvement: ≥0.10% per block (from -0.165% to -0.065% or better)
- Win rate: ≥55%
- Max drawdown: ≤5%

### Stretch Goal:
- MRB: ≥+0.05% per block (profitable on average)
- Win rate: ≥60%
- Sharpe ratio: ≥0.5 (assuming 252 trading days/year)

### Red Flags:
- If MRB degrades below -0.20%, parameter ranges may be too wide
- If all trials have <50% win rate, signal quality is fundamentally poor
- If optimization finds edge cases (e.g., lambda=0.999, buy_threshold=0.60), ranges are wrong

---

## 9. IMPLEMENTATION CHECKLIST

### Before Running Optuna:
- [ ] Verify backtest command accepts parameter overrides
- [ ] Implement parameter validation in OES constructor
- [ ] Add MRB calculation to analyze-trades output
- [ ] Create Optuna study database (SQLite or in-memory)
- [ ] Set up logging for each trial (save to `data/tmp/optuna_trials/`)

### During Optimization:
- [ ] Monitor trial progress (expected ~2 min per trial)
- [ ] Check for early pruning opportunities (e.g., if MRB < -0.50% after 2 blocks)
- [ ] Save intermediate best parameters every 10 trials
- [ ] Plot parameter importance after 50 trials

### After Optimization:
- [ ] Validate best parameters on holdout period (separate 100 blocks)
- [ ] Compare best vs baseline on multiple metrics (not just MRB)
- [ ] Test robustness: run best params on QQQ data
- [ ] Document winning parameter set in configuration file

---

## 10. SOURCE CODE MODULES REFERENCE

### Strategy Configuration:
- `include/strategy/online_ensemble_strategy.h:33-80` - OnlineEnsembleConfig struct
- `src/strategy/online_ensemble_strategy.cpp:10-53` - Constructor with config initialization

### EWRLS Learning:
- `include/learning/online_predictor.h:20-37` - OnlinePredictor::Config
- `src/learning/online_predictor.cpp:50-109` - EWRLS update logic
- `src/learning/online_predictor.cpp:119-137` - Adaptive learning rate

### Multi-Horizon Ensemble:
- `include/learning/online_predictor.h:102-130` - MultiHorizonPredictor
- `src/learning/online_predictor.cpp:277-294` - add_horizon() with auto lambda adjustment
- `src/learning/online_predictor.cpp:296-326` - Ensemble prediction weighting

### Signal Generation:
- `src/strategy/online_ensemble_strategy.cpp:55-145` - generate_signal() with BB amplification
- `src/strategy/online_ensemble_strategy.cpp:294-302` - determine_signal() threshold logic
- `src/strategy/online_ensemble_strategy.cpp:516-566` - BB calculation and amplification

### Delayed Learning Updates:
- `src/strategy/online_ensemble_strategy.cpp:238-264` - track_prediction() for horizon tracking
- `src/strategy/online_ensemble_strategy.cpp:266-292` - process_pending_updates() for delayed EWRLS updates

### Risk Management:
- `src/backend/adaptive_portfolio_manager.cpp` - Kelly sizing implementation
- `src/backend/ensemble_position_state_machine.cpp` - Multi-instrument PSM

### Backtest Infrastructure:
- `src/cli/backtest_command.cpp:51-315` - End-to-end backtest workflow
- `src/cli/analyze_trades_command.cpp` - Performance analysis and MRB calculation

### Feature Engineering:
- `include/features/unified_engine_v2.h` - 45-feature production engine
- `src/features/unified_engine_v2.cpp` - O(1) feature updates

---

## 11. EXPECTED RUNTIME

### Single Trial:
- Signal generation: ~30 seconds (2880 bars)
- Trade execution: ~10 seconds
- Analysis: ~5 seconds
- **Total: ~45-60 seconds per trial**

### Full Optimization:
- 50 trials (Phase 1): ~45 minutes
- 100 trials (Phase 2): ~90 minutes
- 20 trials (Phase 3): ~20 minutes
- **Total: ~2.5-3 hours for complete optimization**

### Parallelization:
- Can run 4-8 trials in parallel if using multi-process Optuna
- Reduces total time to ~30-45 minutes

---

## 12. NEXT STEPS

1. **Implement Optuna wrapper script** (`tools/optimize_oes_params.py`)
2. **Add parameter override CLI flags** to `sentio_cli backtest`
3. **Run Experiment 1** (threshold-only) to validate framework
4. **Iterate** based on results
5. **Deploy best parameters** to production config

---

## CONCLUSION

This document provides comprehensive parameter ranges and optimization strategy for tuning OnlineEnsembleStrategy to maximize MRB. The key insight is to focus on **signal quality** (thresholds, BB amplification) and **learning dynamics** (EWRLS lambda) before fine-tuning ensemble weights and risk management.

**Critical Success Factor:** Optuna must search intelligently in the high-dimensional parameter space. Use constraints, early pruning, and multi-phase optimization to avoid wasting trials on obviously bad configs.

**Risk Mitigation:** Always validate on holdout data and test robustness across different market regimes (2020 COVID crash, 2021 rally, 2022 bear, 2023-2024 recovery).
