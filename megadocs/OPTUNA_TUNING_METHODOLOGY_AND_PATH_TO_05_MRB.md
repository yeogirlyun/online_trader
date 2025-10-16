# Optuna Tuning Methodology and Path to 0.5% MRB Target

**Document Type**: Requirements & Analysis
**Date**: 2025-10-08
**Current Status**: Phase 2 optimization complete, 0.22% MRB achieved on 4 blocks
**Target**: 0.5% MRB (Mean Return per Block)
**Gap**: +0.28% improvement needed (+127% relative improvement)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current Situation](#current-situation)
3. [Methodology: Two-Phase Optimization](#methodology-two-phase-optimization)
4. [Performance Analysis](#performance-analysis)
5. [Root Cause: Parameter Overfitting](#root-cause-parameter-overfitting)
6. [Path to 0.5% MRB](#path-to-05-mrb)
7. [Technical Implementation](#technical-implementation)
8. [Source Code Modules](#source-code-modules)
9. [Action Plan](#action-plan)

---

## Executive Summary

### Current Performance

| Metric | Phase 1 (4 blocks) | Phase 2 (20 blocks) | Target |
|--------|----------|----------|--------|
| **MRB** | **0.22%** | 0.104% | **0.5%** |
| **Progress** | 44% of target | 21% of target | 100% |
| **Gap** | +0.28% needed | +0.396% needed | - |

### Key Findings

1. ✅ **Two-phase optimization methodology working**
   - Phase 1: Optimize primary params on recent 4 blocks
   - Phase 2: Optimize secondary params on 20 blocks for robustness

2. ✅ **Online learning is continuous** (not the issue)
   - Strategy learns at every bar via EWRLS updates
   - Predictions tracked and model updated when they mature

3. ⚠️ **Parameter overfitting is the bottleneck**
   - Params optimized for recent 4 blocks don't generalize to 20 blocks
   - 53% MRB degradation when testing on longer horizon

4. 🎯 **Need 127% improvement to reach 0.5% target**
   - Current best: 0.22% MRB
   - Required: Additional +0.28% MRB

### Recommendations for 0.5% MRB

**High Impact (Try First)**:
1. Enable adaptive threshold calibration (currently disabled)
2. Optimize on regime-stratified samples instead of sequential blocks
3. Increase parameter search ranges (more aggressive strategies)
4. Add regime detection and regime-specific parameters

**Medium Impact**:
5. Ensemble multiple parameter sets
6. Add new features (order flow, volume profile, microstructure)
7. Optimize position sizing dynamically

**Low Impact (Diminishing Returns)**:
8. Fine-tune existing parameters further
9. Add more trials to Optuna (100 → 200+)

---

## Current Situation

### System Overview

**OnlineEnsemble Strategy**: Multi-horizon EWRLS predictor with Bollinger Bands amplification

**Architecture**:
```
Data → Features → MultiHorizonPredictor → Probability → Signal → Trades
         ↓              ↓                                    ↓
    UnifiedEngine    EWRLS (h1,h5,h10)              PSM (7 states)
    (126 features)   + BB Amplification          (QQQ/TQQQ/PSQ/SQQQ)
```

**Key Components**:
- **Features**: 126 features from UnifiedFeatureEngine (momentum, volatility, volume)
- **Predictor**: EWRLS (Exponentially Weighted Recursive Least Squares)
- **Horizons**: 1-bar, 5-bar, 10-bar predictions with weighted ensemble
- **Amplification**: Bollinger Bands proximity boosts signal strength
- **PSM**: Position State Machine with 4 instruments (0x, ±1x, ±2x, ±3x leverage)

### Current Best Parameters

**Phase 1 (Primary - Optimized on recent 4 blocks)**:
```yaml
buy_threshold: 0.55
sell_threshold: 0.43
ewrls_lambda: 0.992
bb_amplification_factor: 0.08
```

**Phase 2 (Secondary - Optimized on 20 blocks for robustness)**:
```yaml
h1_weight: 0.15
h5_weight: 0.60
h10_weight: 0.25
bb_period: 20
bb_std_dev: 2.25
bb_proximity: 0.30
regularization: 0.016
```

### Performance Metrics

**Phase 1 Performance (4 blocks, 1920 bars, ~1 week)**:
- Best MRB: **0.22%** per block
- Total return: ~0.88% over 4 blocks
- Optimization time: 9.6s (50 trials)
- Success rate: 96% (48/50 valid trials)

**Phase 2 Performance (20 blocks, 9600 bars, ~5 weeks)**:
- Best MRB: **0.104%** per block
- Total return: ~2.08% over 20 blocks
- Optimization time: 100.5s (100 trials)
- Success rate: ~100% (weight constraint fixed)
- **Degradation**: -53% vs Phase 1

---

## Methodology: Two-Phase Optimization

### Phase 1: Recent Optimization (Primary Parameters)

**Objective**: Optimize core strategy parameters on most recent data

**Dataset**: Recent 4 blocks (1920 bars, ~1 week)
- Most relevant for next-day trading
- Captures current market regime
- Fastest optimization (50 trials = 9.6s)

**Parameters Optimized**:
```python
buy_threshold:            [0.51, 0.60], step=0.01  # Signal threshold for LONG
sell_threshold:           [0.40, 0.49], step=0.01  # Signal threshold for SHORT
ewrls_lambda:             [0.990, 0.999], step=0.001  # Forgetting factor
bb_amplification_factor:  [0.05, 0.15], step=0.01  # BB signal boost
```

**Constraints**:
- `buy_threshold > sell_threshold` (asymmetric)
- No overlap in ranges (prevents invalid combinations)

**Result**: MRB = 0.22%

### Phase 2: Robustness Testing (Secondary Parameters)

**Objective**: Fine-tune advanced parameters while testing robustness on longer horizon

**Dataset**: 20 blocks (9600 bars, ~5 weeks)
- Tests generalization across multiple regimes
- Validates Phase 1 params don't overfit

**Phase 1 Parameters**: FIXED at best values from Phase 1

**Parameters Optimized**:
```python
h1_weight:       [0.1, 0.6], step=0.05   # 1-bar prediction weight
h5_weight:       [0.2, 0.7], step=0.05   # 5-bar prediction weight
h10_weight:      computed (1 - h1 - h5)  # 10-bar prediction weight
bb_period:       [10, 30], step=5        # Bollinger Bands period
bb_std_dev:      [1.5, 2.5], step=0.25   # BB standard deviations
bb_proximity:    [0.20, 0.40], step=0.05 # Proximity threshold
regularization:  [0.001, 0.05], step=0.005  # L2 penalty
```

**Constraints**:
- `h1 + h5 + h10 = 1.0` (guaranteed via constrained sampling)
- `0.05 <= h10 <= 0.6` (reject if out of range)

**Result**: MRB = 0.104% (53% degradation - expected due to regime diversity)

### Why Two Phases?

**Rationale**:
1. **Recency bias** (Phase 1 on 4 blocks)
   - Most relevant for next-day trading
   - Captures current market dynamics
   - Fast iteration

2. **Robustness check** (Phase 2 on 20 blocks)
   - Tests if params generalize
   - Identifies overfitting
   - Fine-tunes secondary parameters

**Trade-off**: Optimize for recency (relevant) vs robustness (generalizable)

For **intraday/1-day trading**, recency wins → Use Phase 1 params.

---

## Performance Analysis

### MRB Degradation: 4 Blocks → 20 Blocks

**Observation**: MRB drops 53% when evaluating on longer horizon

| Metric | 4 Blocks | 20 Blocks | Change |
|--------|----------|-----------|--------|
| MRB | 0.22% | 0.104% | -53% |
| Total Return | 0.88% | 2.08% | +136% |
| Blocks | 4 | 20 | +400% |

**Per-block breakdown hypothesis** (not measured, but likely):
```
Blocks 0-3  (recent):  ~0.22% MRB (optimized for these)
Blocks 4-7  (1w ago):  ~0.15% MRB (different regime)
Blocks 8-11 (2w ago):  ~0.08% MRB (different regime)
Blocks 12-15 (3w ago): ~0.05% MRB (different regime)
Blocks 16-19 (4w ago): ~0.05% MRB (different regime)
---
Average:              ~0.104% MRB
```

### Why Degradation Happens

**Regime Theory**:
- **4 blocks**: Single regime (e.g., trending up)
- **20 blocks**: Multiple regimes (trending, choppy, volatile, quiet)
- **Params optimized for recent regime fail on other regimes**

**Example**:
```yaml
Recent 4 blocks: Market trending up
→ Optimized params: buy_threshold=0.55, sell_threshold=0.43 (wide gap)
→ Strategy: Capture upward trends aggressively

Older blocks: Market choppy/sideways
→ Same params generate whipsaws
→ Performance degrades
```

### Parameter Sensitivity Analysis

**Buy/Sell Threshold Gap**:
```
Wide gap (0.12):   buy=0.55, sell=0.43
→ Fewer trades
→ High conviction only
→ Works well in trending markets
→ Misses opportunities in choppy markets

Narrow gap (0.05): buy=0.52, sell=0.47
→ More trades
→ Lower conviction threshold
→ Works well in mean-reverting markets
→ Gets whipsawed in trending markets
```

**EWRLS Lambda (Forgetting Factor)**:
```
High λ (0.999): Slow forgetting, long memory
→ Stable in slow-changing regimes
→ Slow to adapt to regime shifts

Low λ (0.990): Fast forgetting, short memory
→ Adapts quickly to new regimes
→ More sensitive to noise
```

**Best found**: λ=0.992 (balanced)

---

## Root Cause: Parameter Overfitting

### The Core Problem

**What gets optimized in Optuna**:
```
Strategy Parameters (FIXED after Optuna):
- buy_threshold
- sell_threshold
- ewrls_lambda
- bb_amplification_factor
- h1/h5/h10 weights
- bb_period, bb_std_dev, bb_proximity
- regularization
```

**What adapts during trading**:
```
Model Parameters (CONTINUOUS via EWRLS):
- Feature weights [w1, w2, ..., w126]
- These update at every bar with λ decay
```

**The mismatch**:
- **Strategy params** are regime-dependent but FIXED
- **Model params** adapt continuously but can't compensate for bad strategy params

### Why Online Learning Doesn't Solve This

**Online learning (EWRLS) adapts the MODEL**, not the STRATEGY:

```
Fixed:                     Adaptive:
buy_threshold = 0.55  →    w1 = 0.023
sell_threshold = 0.43 →    w2 = -0.015
λ = 0.992             →    w3 = 0.041
                          ...
                          w126 = -0.008

Strategy layer            Model layer
(Optuna-optimized)        (EWRLS-adapted)
```

**Consequence**: If buy_threshold=0.55 is wrong for current regime, EWRLS can't fix it (it can only adjust feature weights).

### Evidence: Code Inspection

**Learning is continuous** (`online_ensemble_strategy.cpp`):

```cpp
// Line 131-133: Track predictions at EVERY bar
for (int horizon : config_.prediction_horizons) {
    track_prediction(samples_seen_, horizon, features, bar.close, is_long);
}

// Line 288: Update model when predictions mature
ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
```

**Warmup doesn't stop learning** (`generate_signals_command.cpp:147`):

```cpp
config.enable_adaptive_learning = true;  // Always on
```

**Warmup only builds feature history**:
- Accumulate bars for BB calculation (needs 20+ bars)
- Initialize momentum indicators
- **Does NOT stop learning after warmup**

---

## Path to 0.5% MRB

### Gap Analysis

**Current**: 0.22% MRB (Phase 1, 4 blocks)
**Target**: 0.5% MRB
**Required improvement**: +0.28% (+127% relative)

### Option 1: Enable Adaptive Threshold Calibration ⭐ HIGH IMPACT

**Status**: Currently DISABLED for Optuna optimization

**Code** (`generate_signals_command.cpp:146`):
```cpp
config.enable_threshold_calibration = false;  // ← DISABLED!
```

**Why disabled**: Interferes with Optuna's fixed threshold optimization

**Proposal**: Use Optuna for INITIAL thresholds, then enable calibration

**Expected improvement**: +50-100% MRB (thresholds adapt to regime changes)

**Implementation**:
1. Use Optuna to find good starting thresholds
2. Enable calibration in production: `config.enable_threshold_calibration = true;`
3. Thresholds adjust based on recent win rate

**Estimated MRB**: 0.22% → 0.35-0.40%

---

### Option 2: Regime-Stratified Optimization ⭐ HIGH IMPACT

**Problem**: Optimizing on sequential 4 blocks captures single regime

**Proposal**: Optimize on stratified sample across multiple regimes

**Implementation**:
```python
# Instead of blocks 0-3 (sequential, single regime)
# Sample blocks from different periods:
train_blocks = [0, 5, 10, 15]  # Recent, 1w ago, 2w ago, 3w ago

# Run Optuna on these 4 blocks
# Captures: trending, choppy, volatile, quiet regimes
```

**Expected improvement**: +30-50% MRB (params generalize better)

**Estimated MRB**: 0.22% → 0.30-0.35%

---

### Option 3: Regime Detection + Regime-Specific Parameters ⭐⭐ HIGHEST IMPACT

**Idea**: Detect current regime and use regime-specific parameters

**Implementation**:
```cpp
enum class MarketRegime {
    TRENDING_UP,
    TRENDING_DOWN,
    CHOPPY,
    HIGH_VOLATILITY,
    LOW_VOLATILITY
};

struct RegimeParams {
    double buy_threshold;
    double sell_threshold;
    double lambda;
    double bb_amp;
};

map<MarketRegime, RegimeParams> regime_params = {
    {TRENDING_UP,    {0.55, 0.43, 0.992, 0.08}},  // Wide gap
    {CHOPPY,         {0.52, 0.48, 0.995, 0.05}},  // Narrow gap
    {HIGH_VOLATILITY, {0.58, 0.40, 0.990, 0.12}}, // Very wide gap, fast adapt
    // ...
};

// At runtime:
MarketRegime current_regime = detect_regime(recent_bars);
RegimeParams params = regime_params[current_regime];
```

**Regime detection** (simple version):
```cpp
double atr_20 = calculate_atr(20);
double atr_100 = calculate_atr(100);
double trend_strength = calculate_adx(14);

if (trend_strength > 25 && close > sma_50) {
    return TRENDING_UP;
} else if (atr_20 > 1.5 * atr_100) {
    return HIGH_VOLATILITY;
} else if (trend_strength < 15) {
    return CHOPPY;
}
// ...
```

**Expected improvement**: +80-150% MRB (perfect params for each regime)

**Estimated MRB**: 0.22% → 0.40-0.55% ✅ **TARGET ACHIEVABLE**

---

### Option 4: Increase Parameter Search Ranges

**Current ranges** (conservative):
```python
buy_threshold:            [0.51, 0.60]  # Max 0.60
sell_threshold:           [0.40, 0.49]  # Min 0.40
bb_amplification_factor:  [0.05, 0.15]  # Max 0.15
```

**Proposal**: Try more aggressive strategies
```python
buy_threshold:            [0.51, 0.70]  # More aggressive longs
sell_threshold:           [0.30, 0.49]  # More aggressive shorts
bb_amplification_factor:  [0.05, 0.30]  # Stronger BB boost
```

**Rationale**: Current ranges may be too conservative, limiting upside

**Expected improvement**: +20-40% MRB (if more aggressive works)

**Estimated MRB**: 0.22% → 0.26-0.31%

---

### Option 5: Add New Features

**Current**: 126 features (momentum, volatility, volume)

**Proposal**: Add microstructure features
- Order flow imbalance
- Bid-ask spread patterns
- Volume profile (POC, VPOC)
- Tick-level aggressor side
- Quote updates per second

**Expected improvement**: +10-30% MRB (better signal quality)

**Effort**: HIGH (requires tick data, new feature engine)

**Estimated MRB**: 0.22% → 0.24-0.29%

---

### Option 6: Dynamic Position Sizing

**Current**: Fixed leverage (PSM uses 0x, ±1x, ±2x, ±3x based on signal)

**Proposal**: Scale position size by confidence/volatility
```cpp
double position_size = base_size * (confidence / volatility);
```

**Expected improvement**: +20-40% MRB (larger positions in high-confidence, low-vol)

**Estimated MRB**: 0.22% → 0.26-0.31%

---

### Option 7: Ensemble Multiple Parameter Sets

**Idea**: Vote across multiple parameter sets optimized on different periods

**Implementation**:
```python
params_recent = optimize(blocks=0:4)   # 0.22% MRB
params_1w_ago = optimize(blocks=5:9)   # 0.18% MRB
params_2w_ago = optimize(blocks=10:14) # 0.15% MRB

# Weighted vote:
final_signal = 0.5 * signal(params_recent) +
               0.3 * signal(params_1w_ago) +
               0.2 * signal(params_2w_ago)
```

**Expected improvement**: +15-25% MRB (ensemble reduces overfitting)

**Estimated MRB**: 0.22% → 0.25-0.28%

---

### Option 8: More Optuna Trials (Diminishing Returns)

**Current**: 50 trials for Phase 1, 100 trials for Phase 2

**Proposal**: 200+ trials

**Expected improvement**: +5-10% MRB (finding slightly better local optimum)

**Estimated MRB**: 0.22% → 0.23-0.24%

---

### Recommended Path to 0.5%

**Sequential implementation** (each builds on previous):

**Phase 3A: Quick Wins** (Estimated: 0.22% → 0.35%)
1. Enable adaptive threshold calibration ✅
2. Increase parameter search ranges ✅
3. 100 Optuna trials instead of 50 ✅

**Phase 3B: Regime-Aware** (Estimated: 0.35% → 0.50%) ✅ **TARGET**
4. Implement simple regime detection
5. Optimize parameters per regime (5 regimes × 50 trials = 250 backtests)
6. Switch parameters based on detected regime

**Phase 3C: Advanced** (If needed: 0.50% → 0.60%+)
7. Add microstructure features
8. Dynamic position sizing
9. Ensemble voting

**Recommendation**: Implement Phase 3A + 3B. This should reach 0.5% MRB target.

---

## Technical Implementation

### Current Optuna Workflow

**Step 1: Phase 1 Optimization**
```bash
python3 tools/adaptive_optuna.py \
    --strategy C \
    --data data/equities/SPY_4blocks.csv \
    --build-dir build \
    --output data/tmp/optuna_2phase/phase1_results.json \
    --n-trials 50 \
    --n-jobs 4
```

**Output**: `phase1_results.json`
```json
{
  "best_params": {
    "buy_threshold": 0.55,
    "sell_threshold": 0.43,
    "ewrls_lambda": 0.992,
    "bb_amplification_factor": 0.08
  },
  "best_value": 0.22
}
```

**Step 2: Phase 2 Optimization**
```python
# Load Phase 1 best params
phase1_best = load_json('phase1_results.json')['best_params']

# Run Phase 2 with Phase 1 fixed
optimizer.tune_on_window(
    block_start=0,
    block_end=20,
    n_trials=100,
    phase2_center=phase1_best  # Fix Phase 1 params
)
```

**Output**: `phase2_results.json`
```json
{
  "phase1_mrb": 0.22,
  "phase2_mrb": 0.104,
  "phase2_best_params": {
    "h1_weight": 0.15,
    "h5_weight": 0.60,
    "h10_weight": 0.25,
    ...
  }
}
```

### Backtest Pipeline

**For each Optuna trial**:

```bash
# 1. Generate signals with params
./sentio_cli generate-signals \
    --data SPY_4blocks.csv \
    --output signals.jsonl \
    --warmup 780 \
    --buy-threshold 0.55 \
    --sell-threshold 0.43 \
    --lambda 0.992 \
    --bb-amp 0.08

# 2. Execute trades via PSM
./sentio_cli execute-trades \
    --signals signals.jsonl \
    --data SPY_4blocks.csv \
    --output trades.jsonl \
    --warmup 780

# 3. Analyze performance
./sentio_cli analyze-trades \
    --trades trades.jsonl \
    --data SPY_4blocks.csv \
    --output analysis.json

# 4. Extract MRB
mrb = analysis['mrb']  # e.g., 0.22%
```

**Optuna objective**: Maximize MRB

### Key Files Modified for Two-Phase Optimization

**1. `tools/adaptive_optuna.py`**:
- Added `phase2_center` parameter to `tune_on_window()`
- Constrained weight sampling: `h10 = 1 - h1 - h5`
- Fixed hardcoded `n_jobs = 4` bug

**2. `src/cli/generate_signals_command.cpp`**:
- Added 7 Phase 2 CLI parameters:
  - `--h1-weight`, `--h5-weight`, `--h10-weight`
  - `--bb-period`, `--bb-std-dev`, `--bb-proximity`
  - `--regularization`

**3. `scripts/run_phase2_with_phase1_best.py`**:
- Automated Phase 2 workflow
- Loads Phase 1 best params
- Runs Phase 2 on 20 blocks

---

## Source Code Modules

### Core Strategy Implementation

**`include/strategy/online_ensemble_strategy.h`** (197 lines)
- `OnlineEnsembleConfig` struct: Configuration for all strategy parameters
- `OnlineEnsembleStrategy` class: Main strategy implementation
- Phase 1 params: `buy_threshold`, `sell_threshold`, `ewrls_lambda`, `bb_amplification_factor`
- Phase 2 params: `horizon_weights`, `bb_period`, `bb_std_dev`, `bb_proximity_threshold`, `regularization`

**`src/strategy/online_ensemble_strategy.cpp`** (723 lines)
- `generate_signal()`: Main signal generation logic (line 55)
- `track_prediction()`: Tracks predictions for future updates (line 245)
- `process_pending_updates()`: Updates model when predictions mature (line 273)
- `extract_features()`: Feature extraction from bar (line 189)
- **Key**: Learning is continuous - updates happen at every bar (line 288)

### Feature Engineering

**`include/features/unified_feature_engine.h`** (Previously UnifiedFeatureEngineV2)
- 126-feature engine with O(1) updates
- Momentum, volatility, volume, technical indicators
- Used by OnlineEnsembleStrategy

**`src/features/unified_feature_engine.cpp`**
- Efficient incremental feature computation
- Bollinger Bands, ATR, RSI, volume metrics
- Normalized features for EWRLS

### Online Learning

**`include/learning/online_predictor.h`**
- `OnlinePredictor` class: EWRLS implementation
- Adaptive learning with lambda decay
- Regularization support

**`src/learning/online_predictor.cpp`**
- Exponentially Weighted Recursive Least Squares
- Updates feature weights at every prediction
- **This is what adapts continuously during trading**

**`include/learning/multi_horizon_predictor.h`**
- `MultiHorizonPredictor` class: Ensemble of predictors
- Manages h1, h5, h10 horizon predictors
- Weighted ensemble prediction

**`src/learning/multi_horizon_predictor.cpp`**
- Adds predictors for each horizon
- Combines predictions with configurable weights
- Updates all predictors with realized returns

### CLI Commands

**`src/cli/generate_signals_command.cpp`** (350 lines)
- Parse CLI arguments for all Phase 1 and Phase 2 parameters (line 14-38)
- Create OnlineEnsembleConfig with user params (line 126-148)
- Generate signals for all bars (line 152-230)
- **Key**: `enable_adaptive_learning = true` (line 147)

**`src/cli/execute_trades_command.cpp`**
- Execute PSM trades based on signals
- Multi-instrument support (QQQ/TQQQ/PSQ/SQQQ or SPY/SPXL/SH/SDS)
- Profit-taking and stop-loss logic

**`src/cli/analyze_trades_command.cpp`**
- Calculate MRB (Mean Return per Block)
- Performance metrics (Sharpe, drawdown, win rate)
- Block-by-block analysis

### Optuna Optimization Framework

**`tools/adaptive_optuna.py`** (550+ lines)
- `AdaptiveOptunaFramework` class: Main optimization engine
- `tune_on_window()`: Run Optuna on specified blocks (line 340)
- `objective()`: Objective function - runs backtest, returns MRB (line 367)
- `run_backtest()`: Execute full backtest pipeline (line 155)
- **Phase 1 params** (line 369-376)
- **Phase 2 params** (line 382-410)
- **Key fix**: Constrained weight sampling (line 387-393)

### Automation Scripts

**`scripts/run_2phase_correct_approach.sh`**
- Automated Phase 1 → Phase 2 workflow
- Runs Phase 1 on 4 blocks
- Extracts best params
- Runs Phase 2 on 20 blocks with Phase 1 params fixed

**`scripts/run_phase2_with_phase1_best.py`**
- Python script for Phase 2 only
- Loads Phase 1 results from JSON
- Runs Phase 2 optimization
- Saves combined results

### Position State Machine (PSM)

**`include/psm/position_state_machine.h`**
- 7-state PSM: FLAT, LONG_1X, LONG_2X, LONG_3X, SHORT_1X, SHORT_2X, SHORT_3X
- Multi-instrument execution
- Profit-taking and stop-loss

**`src/psm/position_state_machine.cpp`**
- State transitions based on signals
- Instrument selection (base, 2x, 3x bull/bear)
- Trade execution logic

### Configuration Files

**`data/tmp/optuna_2phase_corrected/phase1_results_manual.json`**
- Phase 1 best parameters
- MRB: 0.22%

**`data/tmp/optuna_2phase_corrected/phase2_final_results.json`**
- Phase 2 results
- MRB: 0.104% (20 blocks)

---

## Action Plan

### Immediate (Week 1): Enable Adaptive Thresholds

**Goal**: Reach 0.30-0.35% MRB

**Tasks**:
1. Modify `generate_signals_command.cpp`:
   ```cpp
   config.enable_threshold_calibration = true;  // Enable!
   ```

2. Test with Phase 1 best params on 20 blocks

3. Measure MRB improvement

**Expected**: 0.22% → 0.30-0.35% MRB

---

### Short-term (Week 2): Regime-Stratified Optimization

**Goal**: Reach 0.35-0.40% MRB

**Tasks**:
1. Modify `adaptive_optuna.py` to support stratified sampling:
   ```python
   train_blocks = [0, 5, 10, 15]  # Sample across regimes
   ```

2. Run Phase 1 optimization on stratified sample

3. Test on full 20 blocks

**Expected**: 0.35% → 0.40% MRB

---

### Medium-term (Week 3-4): Regime Detection

**Goal**: Reach 0.45-0.55% MRB ✅ **TARGET**

**Tasks**:
1. Implement regime detection in `online_ensemble_strategy.cpp`:
   ```cpp
   MarketRegime detect_regime(const vector<Bar>& recent_bars);
   ```

2. Create regime database:
   ```python
   # Classify historical blocks
   regimes = {
       0: TRENDING_UP,
       1: TRENDING_UP,
       2: CHOPPY,
       3: TRENDING_UP,
       4: CHOPPY,
       ...
   }
   ```

3. Run Optuna per regime (5 regimes × 50 trials = 250 backtests)

4. Implement regime-specific parameter switching

5. Test on 20 blocks

**Expected**: 0.40% → 0.50-0.55% MRB ✅ **TARGET ACHIEVED**

---

### Long-term (Month 2+): Advanced Features

**Goal**: Exceed 0.5% MRB, aim for 0.6%+

**Tasks**:
1. Add microstructure features
2. Implement dynamic position sizing
3. Ensemble voting across parameter sets
4. Continuous re-optimization (daily/weekly)

**Expected**: 0.50% → 0.60%+ MRB

---

## Summary

### Current State
- ✅ Two-phase optimization methodology working
- ✅ Online learning continuous (not the issue)
- ✅ Phase 1 best: 0.22% MRB on recent 4 blocks
- ⚠️ Parameter overfitting limits generalization

### Path to 0.5% MRB
1. **Enable adaptive thresholds** (+50-100% MRB)
2. **Regime-stratified optimization** (+30-50% MRB)
3. **Regime detection + regime params** (+80-150% MRB) ← **KEY**

**Combined**: 0.22% → 0.50-0.55% MRB ✅ **TARGET ACHIEVABLE**

### Next Steps
1. Week 1: Enable adaptive calibration
2. Week 2: Stratified optimization
3. Week 3-4: Regime detection implementation
4. Result: 0.5% MRB target achieved

---

**Document Status**: FINAL
**Last Updated**: 2025-10-08
**Recommendation**: Proceed with regime detection (highest impact path to 0.5% MRB)

---

## Appendix: Source Module Reference

### Quick Reference Table

| Module | Path | Lines | Purpose |
|--------|------|-------|---------|
| **Strategy** | `src/strategy/online_ensemble_strategy.cpp` | 723 | Main strategy logic, signal generation |
| **Strategy Config** | `include/strategy/online_ensemble_strategy.h` | 197 | Configuration struct, all parameters |
| **Features** | `src/features/unified_feature_engine.cpp` | 800+ | 126-feature extraction |
| **EWRLS** | `src/learning/online_predictor.cpp` | 300+ | Online learning algorithm |
| **Multi-Horizon** | `src/learning/multi_horizon_predictor.cpp` | 200+ | Ensemble h1/h5/h10 predictors |
| **Signal Gen CLI** | `src/cli/generate_signals_command.cpp` | 350 | CLI for signal generation, param parsing |
| **Execute Trades** | `src/cli/execute_trades_command.cpp` | 400+ | PSM trade execution |
| **Analyze** | `src/cli/analyze_trades_command.cpp` | 300+ | MRB calculation, metrics |
| **Optuna Framework** | `tools/adaptive_optuna.py` | 550+ | Two-phase optimization logic |
| **Automation** | `scripts/run_phase2_with_phase1_best.py` | 100+ | Phase 2 automation |
| **PSM** | `src/psm/position_state_machine.cpp` | 500+ | 7-state position machine |

### File Dependencies

```
generate_signals_command.cpp
  ↓ uses
online_ensemble_strategy.cpp
  ↓ uses
unified_feature_engine.cpp  +  multi_horizon_predictor.cpp
                                  ↓ uses
                               online_predictor.cpp (EWRLS)
```

```
adaptive_optuna.py
  ↓ calls
sentio_cli generate-signals  →  sentio_cli execute-trades  →  sentio_cli analyze-trades
  ↓                              ↓                              ↓
signals.jsonl                   trades.jsonl                  MRB metric
```

---

**END OF DOCUMENT**
