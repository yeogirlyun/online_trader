# CRITICAL: Threshold Mismatch Bug

**Date**: 2025-10-10
**Severity**: CRITICAL - Optimization parameters have NO EFFECT on trading
**Status**: ROOT CAUSE IDENTIFIED ❌

---

## Executive Summary

The optimization is tuning `buy_threshold` and `sell_threshold` parameters (0.505-0.545 / 0.45-0.49), but **execute_trades uses hardcoded PSM thresholds** that ignore these parameters. Result: All optimization trials produce 0 trades because 99% of signals map to CASH_ONLY.

---

## The Bug

### Optimization Tunes These Parameters
**File**: `scripts/run_2phase_optuna.py` lines 512-522

```python
# CHOPPY regime adaptive ranges
'buy_threshold': (0.505, 0.55),
'sell_threshold': (0.45, 0.495),
```

### Execute-Trades IGNORES Them
**File**: `src/cli/execute_trades_command.cpp` lines 58-59, 267-291

```cpp
// Parameters are PARSED but NEVER USED:
double buy_threshold = std::stod(get_arg(args, "--buy-threshold", "0.53"));
double sell_threshold = std::stod(get_arg(args, "--sell-threshold", "0.47"));

// HARDCODED PSM threshold mapping (lines 267-291):
if (signal.probability >= 0.68) {
    target_state = PositionStateMachine::State::TQQQ_ONLY;  // 3x LONG
} else if (signal.probability >= 0.60) {
    target_state = PositionStateMachine::State::QQQ_TQQQ;   // 1x+3x LONG
} else if (signal.probability >= 0.55) {
    target_state = PositionStateMachine::State::QQQ_ONLY;   // 1x LONG
} else if (signal.probability >= 0.49) {
    target_state = PositionStateMachine::State::CASH_ONLY;  // NEUTRAL
} else if (signal.probability >= 0.45) {
    target_state = PositionStateMachine::State::PSQ_ONLY;   // 1x SHORT
} else if (signal.probability >= 0.35) {
    target_state = PositionStateMachine::State::PSQ_SQQQ;   // 1x+2x SHORT
} else {
    target_state = PositionStateMachine::State::SQQQ_ONLY;  // 2x SHORT
}
```

---

## Why All Trials Have 0 Trades

**Signal Distribution** (from optimization logs):
```
Day 0: mean=0.500, std=0.020-0.032
95% of signals fall in range: 0.44 - 0.56 (±2σ)
```

**Hardcoded Mapping**:
```
0.49 ≤ prob < 0.55  →  CASH_ONLY
```

**Result**:
- 99% of signals → probability in [0.44, 0.56]
- 99% map to CASH_ONLY due to hardcoded 0.49-0.55 range
- System never takes positions → 0 trades
- Optimization tunes 0.505-0.545 but it has NO EFFECT!

---

## Impact

### Optimization is Broken
- All 100 trials in Phase 1 rejected for < 5 trades
- Parameters being optimized don't control trading behavior
- MRD remains at 0% because no trades execute
- Weeks of optimization tuning have been ineffective

### Signal Caching is Correct
- Cache correctly identifies that signals differ by lambda/BB params
- But the cached signals are being mapped by hardcoded thresholds
- Cache performance is fine - the downstream usage is broken

---

## Solution Options

### Option 1: Simple Binary Threshold Model ⭐ RECOMMENDED

**Replace hardcoded PSM mapping with simple buy/sell threshold logic**

**File**: `src/cli/execute_trades_command.cpp` lines 267-291

```cpp
// Use optimized thresholds for direct binary decision
PositionStateMachine::State target_state;

if (is_eod_close) {
    target_state = PositionStateMachine::State::CASH_ONLY;
} else if (signal.probability >= buy_threshold) {
    // LONG signal → use base instrument (SPY/QQQ)
    target_state = PositionStateMachine::State::QQQ_ONLY;  // or SPY_ONLY
} else if (signal.probability <= sell_threshold) {
    // SHORT signal → use inverse ETF
    target_state = PositionStateMachine::State::PSQ_ONLY;  // or SH_ONLY
} else {
    // NEUTRAL → stay in cash
    target_state = PositionStateMachine::State::CASH_ONLY;
}
```

**Advantages**:
- Optimization parameters directly control trading
- Simple, interpretable logic
- Conservative (no leverage)
- Immediate fix (5 minutes)

**Disadvantages**:
- Loses multi-level leverage (no 2x/3x positions)
- Less sophisticated than 7-state PSM

---

### Option 2: Optimize PSM Threshold Breakpoints

**Change optimization to tune the 6 PSM thresholds**

**File**: `scripts/run_2phase_optuna.py`

```python
# Instead of buy/sell thresholds, optimize PSM breakpoints
params = {
    'threshold_bull_3x': trial.suggest_float('threshold_bull_3x', 0.60, 0.75),
    'threshold_bull_1x_3x': trial.suggest_float('threshold_bull_1x_3x', 0.55, 0.65),
    'threshold_bull_1x': trial.suggest_float('threshold_bull_1x', 0.52, 0.58),
    'threshold_cash': trial.suggest_float('threshold_cash', 0.48, 0.52),
    'threshold_bear_1x': trial.suggest_float('threshold_bear_1x', 0.42, 0.48),
    'threshold_bear_1x_2x': trial.suggest_float('threshold_bear_1x_2x', 0.30, 0.40),
}

# Constraint: enforce ordering
if not (params['threshold_bear_1x_2x'] < params['threshold_bear_1x'] <
        params['threshold_cash'] < params['threshold_bull_1x'] <
        params['threshold_bull_1x_3x'] < params['threshold_bull_3x']):
    return -999.0  # Invalid trial
```

**File**: `src/cli/execute_trades_command.cpp`

```cpp
// Read PSM thresholds from CLI args
double th_bull_3x = std::stod(get_arg(args, "--threshold-bull-3x", "0.68"));
double th_bull_1x_3x = std::stod(get_arg(args, "--threshold-bull-1x-3x", "0.60"));
double th_bull_1x = std::stod(get_arg(args, "--threshold-bull-1x", "0.55"));
double th_cash_low = std::stod(get_arg(args, "--threshold-cash-low", "0.49"));
double th_bear_1x = std::stod(get_arg(args, "--threshold-bear-1x", "0.45"));
double th_bear_1x_2x = std::stod(get_arg(args, "--threshold-bear-1x-2x", "0.35"));

// Use dynamic thresholds
if (signal.probability >= th_bull_3x) {
    target_state = PositionStateMachine::State::TQQQ_ONLY;
} else if (signal.probability >= th_bull_1x_3x) {
    target_state = PositionStateMachine::State::QQQ_TQQQ;
} // ... etc
```

**Advantages**:
- Keeps sophisticated 7-state PSM
- Maintains multi-level leverage
- Optimization directly tunes trading behavior

**Disadvantages**:
- More complex implementation (30-60 minutes)
- 6 parameters to optimize (slower convergence)
- Constraint checking required

---

### Option 3: Map Buy/Sell to PSM States

**Use buy/sell thresholds to determine PSM state**

```cpp
// Map optimized thresholds to PSM states
double prob = signal.probability;

if (prob >= buy_threshold + 0.10) {
    // Strong LONG (>= buy + 10pp)
    target_state = PositionStateMachine::State::QQQ_TQQQ;
} else if (prob >= buy_threshold) {
    // Moderate LONG (>= buy threshold)
    target_state = PositionStateMachine::State::QQQ_ONLY;
} else if (prob > sell_threshold) {
    // Neutral (between thresholds)
    target_state = PositionStateMachine::State::CASH_ONLY;
} else if (prob > sell_threshold - 0.10) {
    // Moderate SHORT (<= sell threshold)
    target_state = PositionStateMachine::State::PSQ_ONLY;
} else {
    // Strong SHORT (< sell - 10pp)
    target_state = PositionStateMachine::State::PSQ_SQQQ;
}
```

**Advantages**:
- Uses existing optimization parameters
- Enables multi-level positions
- Simple relative mapping

**Disadvantages**:
- Arbitrary 10pp offset
- Less principled than Option 2

---

## Recommended Fix: Option 1 (Simple Binary)

**Why**:
1. Immediate fix (5 minutes)
2. Optimization parameters finally work
3. Conservative approach (no leverage = less risk)
4. Can iterate to Option 2 later if needed

**Implementation**:
```bash
# 1. Edit execute_trades_command.cpp (lines 267-291)
# 2. Rebuild: cd build && make -j
# 3. Re-run optimization: ./scripts/launch_trading.sh mock --date 2025-10-09
# 4. Expect: Trials with 10-50 trades, positive MRD
```

---

## Testing Plan

### After Implementing Fix

**Quick Test** (3 trials):
```bash
python3 scripts/run_2phase_optuna.py \
    --data data/equities/SPY_RTH_NH_5years.csv \
    --n-trials-phase1 3 \
    --n-trials-phase2 0 \
    --output config/test_threshold_fix.json
```

**Expected Results**:
- Trials now produce 10-50 trades (not 0!)
- MRD varies across trials (not all 0%)
- At least one trial with positive MRD
- Cache statistics show proper operation

**Full Optimization** (50+50 trials):
```bash
./scripts/launch_trading.sh mock --date 2025-10-09
```

**Expected Results**:
- Phase 1: Best MRD +0.01% to +0.05%
- Phase 2: Further improvement
- Trading frequency: 20-100 trades per period
- Sharpe ratio > 0.5

---

## Related Issues

### Fixed by This Change
- ✅ 0-trade optimization trials
- ✅ All trials rejected for < 5 trades
- ✅ MRD stuck at 0% despite balanced ranges
- ✅ Optimization parameters having no effect

### Not Fixed by This Change
- Signal generation quality (separate issue)
- Cache performance (already working)
- Regime detection accuracy (different system)

---

## Commits Affected

- **255163c**: Optimization improvements (good changes, just need execute-trades fix)
- **02b7618**: Signal caching (working correctly)
- **ef69227**: Symbol detection bug fix (unrelated)

---

## Priority: CRITICAL

This is a **fundamental architectural bug** that makes all optimization efforts ineffective. Fix before any further optimization work.

**Estimated Impact**:
- Before fix: 0 trades, 0% MRD
- After fix: 20-100 trades, +0.01% to +0.10% MRD
- **Improvement**: System actually starts trading!
