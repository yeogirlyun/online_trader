# OnlineEnsemble Strategy (OES) - Complete Parameter Catalog

## Parameters Available for Optuna Calibration

### Category 1: EWRLS Learning Parameters
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `ewrls_lambda` | 0.995 | [0.990, 0.999] | Forgetting factor - higher = slower adaptation |
| `initial_variance` | 100.0 | [10.0, 1000.0] | Initial parameter uncertainty |
| `regularization` | 0.01 | [0.001, 0.1] | L2 regularization strength |
| `warmup_samples` | 100 | [50, 200] | Minimum samples before trading |

**Recommendation:** Keep `ewrls_lambda` as-is (auto-adapts), tune `regularization`.

---

### Category 2: Multi-Horizon Ensemble
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `prediction_horizons` | [1, 5, 10] | Fixed | Which horizons to predict |
| `horizon_weights` | [0.3, 0.5, 0.2] | [0.0, 1.0] each | Ensemble weights (must sum to 1.0) |

**Recommendation:** **CRITICAL for Optuna** - optimize horizon weights.

---

### Category 3: Signal Thresholds (HIGH IMPACT)
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `buy_threshold` | 0.53 | [0.50, 0.70] | Probability threshold for LONG signal |
| `sell_threshold` | 0.47 | [0.30, 0.50] | Probability threshold for SHORT signal |
| `neutral_zone` | 0.06 | [0.02, 0.20] | Width of neutral zone (buy - sell) |

**Recommendation:** **TOP PRIORITY for Optuna** - biggest impact on MRB.

---

### Category 4: Bollinger Band Amplification
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `enable_bb_amplification` | true | {true, false} | Enable/disable BB boost |
| `bb_period` | 20 | [10, 30] | Bollinger Band period |
| `bb_std_dev` | 2.0 | [1.5, 3.0] | BB standard deviations |
| `bb_proximity_threshold` | 0.30 | [0.10, 0.50] | Distance from band for amplification |
| `bb_amplification_factor` | 0.10 | [0.05, 0.30] | How much to boost probability |

**Recommendation:** **MEDIUM PRIORITY** - `bb_amplification_factor` most impactful.

---

### Category 5: PSM Risk Management (CRITICAL)
Located in: `execute_trades_command.cpp` (lines 179-185)

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `PROFIT_TARGET` | 0.003 (0.3%) | [0.001, 0.010] | Long profit target |
| `STOP_LOSS` | -0.004 (-0.4%) | [-0.010, -0.001] | Stop loss threshold |
| `MIN_HOLD_BARS` | 3 | [1, 10] | Minimum holding period |
| `MAX_HOLD_BARS` | 100 | [20, 200] | Maximum holding period |

**Recommendation:** **TOP PRIORITY for Optuna** - directly controls win rate.

---

### Category 6: Adaptive Calibration
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `enable_threshold_calibration` | true | {true, false} | Auto-adjust thresholds |
| `calibration_window` | 200 | [50, 500] | Bars for calibration |
| `target_win_rate` | 0.60 | [0.50, 0.70] | Target accuracy |
| `threshold_step` | 0.005 | [0.001, 0.020] | Calibration step size |

**Recommendation:** **LOW PRIORITY** - works well as-is.

---

### Category 7: Kelly Position Sizing
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `enable_kelly_sizing` | true | {true, false} | Enable Kelly Criterion |
| `kelly_fraction` | 0.25 | [0.10, 0.50] | Fraction of full Kelly |
| `max_position_size` | 0.50 | [0.20, 1.00] | Max capital per position |

**Recommendation:** **MEDIUM PRIORITY** - `kelly_fraction` = 0.25 is conservative.

---

### Category 8: Volatility Filter
Located in: `OnlineEnsembleConfig`

| Parameter | Current Value | Range | Impact |
|-----------|---------------|-------|--------|
| `enable_volatility_filter` | false | {true, false} | Enable ATR filter |
| `atr_period` | 20 | [10, 50] | ATR calculation period |
| `min_atr_ratio` | 0.001 | [0.0001, 0.010] | Minimum volatility to trade |

**Recommendation:** **LOW PRIORITY** - currently disabled.

---

## Optuna Optimization Priority

### Tier 1: MUST OPTIMIZE (Highest Impact)
These parameters directly control MRB:

1. **PSM Risk Management**
   - `PROFIT_TARGET` (0.001 - 0.010)
   - `STOP_LOSS` (-0.010 - -0.001)
   - `MIN_HOLD_BARS` (1 - 10)

2. **Signal Thresholds**
   - `buy_threshold` (0.50 - 0.70)
   - `sell_threshold` (0.30 - 0.50)
   - `neutral_zone` (0.02 - 0.20)

3. **Horizon Weights**
   - `horizon_weights[0]` (0.0 - 1.0) for 1-bar
   - `horizon_weights[1]` (0.0 - 1.0) for 5-bar
   - `horizon_weights[2]` (0.0 - 1.0) for 10-bar
   - Constraint: sum = 1.0

### Tier 2: SHOULD OPTIMIZE (Medium Impact)
4. **Bollinger Amplification**
   - `bb_amplification_factor` (0.05 - 0.30)
   - `bb_proximity_threshold` (0.10 - 0.50)

5. **Kelly Sizing**
   - `kelly_fraction` (0.10 - 0.50)

6. **EWRLS**
   - `regularization` (0.001 - 0.1)

### Tier 3: OPTIONAL (Lower Impact)
7. **BB Parameters**
   - `bb_period` (10 - 30)
   - `bb_std_dev` (1.5 - 3.0)

8. **Warmup**
   - `warmup_samples` (50 - 200)

---

## Recommended Optuna Configuration

### Quick Optimization (2-3 hours, ~100 trials)
**Goal:** Optimize for tonight's live trading

**Parameters to optimize (10 total):**
```python
{
    # Tier 1: Risk Management (4)
    'profit_target': (0.001, 0.010),      # 0.1% - 1.0%
    'stop_loss': (-0.010, -0.001),        # -1.0% - -0.1%
    'min_hold_bars': (1, 10),             # 1 - 10 bars

    # Tier 1: Signal Thresholds (3)
    'buy_threshold': (0.50, 0.70),        # 50% - 70%
    'sell_threshold': (0.30, 0.50),       # 30% - 50%
    'neutral_zone': (0.02, 0.20),         # 2% - 20%

    # Tier 1: Horizon Weights (3 - simplex constraint)
    'weight_1bar': (0.0, 1.0),
    'weight_5bar': (0.0, 1.0),
    'weight_10bar': (0.0, 1.0),
    # Constraint: weight_1bar + weight_5bar + weight_10bar = 1.0
}
```

**Objective:** Maximize MRB on 2 test blocks (960 bars) after 2 warmup blocks

**Estimated time:** 1.5-2 minutes per trial × 100 trials = **2.5-3 hours**

---

### Comprehensive Optimization (8-12 hours, ~300 trials)
**Goal:** Full parameter sweep for maximum MRB

**Additional parameters (6 more):**
```python
{
    # Above 10 +

    # Tier 2: Amplification (2)
    'bb_amplification_factor': (0.05, 0.30),
    'bb_proximity_threshold': (0.10, 0.50),

    # Tier 2: Kelly & Regularization (2)
    'kelly_fraction': (0.10, 0.50),
    'regularization': (0.001, 0.1),

    # Tier 3: BB (2)
    'bb_period': (10, 30),
    'bb_std_dev': (1.5, 3.0),
}
```

**Total: 16 parameters**

**Estimated time:** 2 minutes per trial × 300 trials = **10 hours**

---

## Implementation Notes

### Challenge: Parameters in Multiple Locations
1. **OES Config parameters** (in C++ header): Need to expose via CLI flags
2. **PSM parameters** (hardcoded in execute_trades_command.cpp): Need to parameterize

### Solution Options:

#### Option A: Quick Hack (Tonight)
**Modify `execute_trades_command.cpp` to accept CLI parameters:**
```cpp
// Add CLI parameters
--profit-target 0.003
--stop-loss -0.004
--min-hold-bars 3
```

**Pros:** Fast to implement (30 mins)
**Cons:** Doesn't optimize OES config params

#### Option B: Full Integration (Tomorrow)
**Create config file system:**
```json
{
  "oes_config": {
    "buy_threshold": 0.53,
    "sell_threshold": 0.47,
    "horizon_weights": [0.3, 0.5, 0.2]
  },
  "psm_config": {
    "profit_target": 0.003,
    "stop_loss": -0.004,
    "min_hold_bars": 3
  }
}
```

**Pros:** Clean, reusable, optimizes all params
**Cons:** 2-3 hours implementation

---

## Optuna Objective Function

```python
def objective(trial):
    # Sample parameters
    profit_target = trial.suggest_float('profit_target', 0.001, 0.010)
    stop_loss = trial.suggest_float('stop_loss', -0.010, -0.001)
    min_hold_bars = trial.suggest_int('min_hold_bars', 1, 10)

    buy_threshold = trial.suggest_float('buy_threshold', 0.50, 0.70)
    sell_threshold = trial.suggest_float('sell_threshold', 0.30, 0.50)
    neutral_zone = trial.suggest_float('neutral_zone', 0.02, 0.20)

    # Horizon weights with simplex constraint
    w1 = trial.suggest_float('weight_1bar', 0.0, 1.0)
    w2 = trial.suggest_float('weight_5bar', 0.0, 1.0)
    w3 = 1.0 - w1 - w2  # Ensure sum = 1.0
    if w3 < 0:
        return -999.0  # Invalid trial

    # Run sentio_cli with these parameters
    signals = generate_signals(buy_threshold, sell_threshold, [w1, w2, w3])
    trades = execute_trades(signals, profit_target, stop_loss, min_hold_bars)

    # Calculate MRB
    mrb = calculate_mrb(trades, warmup_bars=960, test_blocks=2)

    return mrb  # Optuna maximizes
```

---

## Recommendation for Tonight

**Given 3 hours until market open:**

### Phase 1: Quick Optimization (NOW - 2 hours)
1. Implement Option A (CLI parameters for PSM)
2. Run Optuna with **Tier 1 parameters only** (10 params)
3. Target: 50-100 trials in 2 hours
4. Use best parameters for tonight's trading

### Phase 2: Validation (2 hours - market open)
1. Test best parameters on 4-block data
2. Verify MRB ≥ 0.340%
3. Create backup config with v1.5 params
4. Deploy to paper trading

### Phase 3: Comprehensive Optimization (Tomorrow)
1. Implement Option B (full config system)
2. Run 300+ trials overnight
3. Validate on 20-block data
4. Deploy v1.6 if MRB > 0.400%

---

## Expected Improvement

**Current v1.5:** 0.345% MRB (SPY-calibrated, manual)

**Optuna v1.6 target:** 0.40-0.50% MRB (data-driven optimization)

**Potential gain:** +0.05-0.15% MRB (+15-45% improvement)

---

**Next Steps:**
1. Decide: Quick optimization (tonight) or full optimization (tomorrow)?
2. If tonight: Implement CLI parameters for PSM (30 mins)
3. Run Optuna (2 hours)
4. Deploy best config

What's your preference?
