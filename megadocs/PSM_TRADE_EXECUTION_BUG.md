# Bug Report: PSM Trade Execution Blocking Issue

**Date:** 2025-10-06
**Component:** Trade Execution Engine (execute_trades_command.cpp)
**Severity:** CRITICAL
**Status:** OPEN

---

## Executive Summary

The OnlineEnsemble trading system with Position State Machine (PSM) integration is only executing **3 trades per 2000 bars** (0.15% trade frequency) instead of the target **50 trades per block** (~10% of 480 bars = 240 trades per 2000 bars). This represents a **99.4% shortfall** in expected trading activity.

Despite successfully generating diverse probability signals (ranging 0.10-0.90) and implementing direct probability-to-state mapping with relaxed thresholds, state transitions are being calculated but **not executed**.

---

## Problem Statement

### Expected Behavior
- Target: **~50 trades per block** (480 bars) = ~10% trade frequency
- For 2000 bars (~4.2 blocks): Expected **~210-240 trades**
- Should respond to non-neutral signals (prob < 0.49 or > 0.51)
- Should exit positions on 2% profit or -1.5% loss
- Should stay in cash only when highly uncertain (0.49-0.51)

### Actual Behavior
- Actual: **3 trades per 2000 bars** = 0.15% trade frequency
- **99.4% fewer trades than expected**
- Verbose output shows:
  ```
  [500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY | PnL: 0.01%
  ```
  - `Target: QQQ_ONLY` but `Current: CASH_ONLY` → **No trade executed!**
  - This indicates transition is calculated but execution is blocked

### Impact
- Strategy cannot achieve target returns (10% monthly / 0.5% per block)
- PSM's 4-instrument (QQQ/TQQQ/PSQ/SQQQ) capability underutilized
- Unable to test profit-taking and stop-loss logic
- Cannot validate OnlineEnsemble EWRLS signal quality

---

## Root Cause Analysis

### 1. Signal Generation (WORKING ✓)
OnlineEnsemble EWRLS is producing diverse signals:

**Full Dataset (292,385 bars):**
- LONG signals: 67,711 (23.2%)
- SHORT signals: 77,934 (26.7%)
- NEUTRAL signals: 146,740 (50.2%)
- Probability range: 0.10 - 0.90

**Test Dataset (2,000 bars):**
```
Probability distribution:
  < 0.30:      25 (1.2%)  → SQQQ_ONLY (strong short, -3x)
  0.30-0.40:   13 (0.7%)  → PSQ_SQQQ (moderate short, blended)
  0.40-0.47:   86 (4.3%)  → PSQ_ONLY (weak short, -1x)
  0.47-0.53: 1772 (88.6%) → CASH_ONLY (uncertain)
  0.53-0.60:   69 (3.5%)  → QQQ_ONLY (weak long, 1x)
  0.60-0.70:   20 (1.0%)  → QQQ_TQQQ (moderate long, blended)
  > 0.70:      15 (0.8%)  → TQQQ_ONLY (strong long, 3x)

Total non-cash signals: 228 (11.4%)
```

**Issue:** While 88.6% are neutral, there are still **228 non-cash signals** that should trigger state changes, yet only **3 trades** occurred.

### 2. State Mapping (WORKING ✓)
Direct probability-to-state mapping implemented (lines 139-157):

```cpp
if (signal.probability >= 0.65)      → TQQQ_ONLY
else if (signal.probability >= 0.55) → QQQ_TQQQ
else if (signal.probability >= 0.51) → QQQ_ONLY
else if (signal.probability >= 0.49) → CASH_ONLY
else if (signal.probability >= 0.45) → PSQ_ONLY
else if (signal.probability >= 0.35) → PSQ_SQQQ
else                                 → SQQQ_ONLY
```

Verbose logs confirm this is working:
```
[500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY
```

### 3. Trade Execution Logic (BROKEN ✗)
**Critical code section (lines 186-187):**
```cpp
// Execute state transition
if (transition.target_state != transition.current_state) {
    // Calculate positions for target state
    std::map<std::string, double> target_positions = ...
```

**Hypothesis:** The condition `transition.target_state != transition.current_state` is evaluating to `false` despite verbose output showing different states, OR execution is blocked by a downstream condition.

### 4. Possible Blocking Mechanisms

**A. Position Tracking State Mismatch**
```cpp
PositionTracking current_position;
current_position.state = PositionStateMachine::State::CASH_ONLY;  // Line 89
```

The `current_position.state` may not be updated when trades are executed, causing:
- Line 161: `transition.current_state = current_position.state;`
- This could always be `CASH_ONLY` even after entering positions
- Result: Next bar thinks we're still in CASH when we're actually in PSQ/SQQQ

**B. Cash Balance Constraint**
```cpp
// Line 198
if (trade_value <= portfolio.cash_balance) {
    portfolio.cash_balance -= trade_value;
    ...
}
```

After initial trades deplete cash, subsequent BUY orders may fail silently because:
- No error handling when `trade_value > portfolio.cash_balance`
- Trade is simply skipped
- No logging to indicate blocked trade

**C. Minimum Hold Period Interference**
```cpp
// Lines 171-176
if (current_position.bars_held < MIN_HOLD_BARS &&
    transition.current_state != PositionStateMachine::State::CASH_ONLY &&
    forced_target_state == PositionStateMachine::State::INVALID) {
    transition.target_state = transition.current_state;  // Reset to current!
}
```

With `MIN_HOLD_BARS = 1`, if:
- Bar N: Enter position (bars_held = 0)
- Bar N+1: Signal changes → target_state calculated → but bars_held < 1 → **target reset to current** → no trade!

**D. State Not Updated After Execution**
After executing trades (lines 196-231), there's no visible update to:
```cpp
current_position.state = transition.target_state;  // MISSING!
```

This would cause:
- Trades execute but state tracking thinks we're still in old state
- Next iteration: `transition.current_state` is stale → blocks future trades

---

## Evidence & Logs

### Verbose Execution Output
```
[0] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: 0.00%
  [119] PSQ BUY 342.35 @ $292.10 | Portfolio: $100000.00
  [136] PSQ SELL 171.17 @ $292.04 | Portfolio: $99979.80
  [136] SQQQ BUY 171.17 @ $292.04 | Portfolio: $99979.80
  [500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY | PnL: 0.01%
  [1000] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: -0.05%
  [1500] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: 0.03%
```

**Critical observation at bar 500:**
- Signal: 0.51 → Maps to QQQ_ONLY
- Current: CASH_ONLY
- Target: QQQ_ONLY
- **Expected:** BUY QQQ
- **Actual:** No trade executed
- **PnL:** 0.01% suggests positions are being held, but state shows CASH

### Trade History
Only 3 trades in 2000 bars:
1. **Bar 119:** BUY PSQ (CASH → PSQ_ONLY)
2. **Bar 136:** SELL PSQ + BUY SQQQ (PSQ_ONLY → PSQ_SQQQ blend)
3. *(No third distinct trade - only 2 state transitions)*

After bar 136, system remains in CASH_ONLY despite:
- 228 non-cash signals (11.4% of bars)
- Diverse probabilities (0.10-0.90)
- Relaxed thresholds (0.49-0.51 for cash)

---

## Signal Analysis

### Non-Neutral Signal Runs
Analysis of consecutive non-neutral signals:
```
Total non-neutral runs: 178
Longest run: 11 bars
Average run length: 1.3 bars

First 10 runs:
  Run 1: bars 136-136 (1 bars)  ← Trade executed here
  Run 2: bars 140-140 (1 bars)  ← No trade!
  Run 3: bars 178-178 (1 bars)  ← No trade!
  Run 4: bars 210-210 (1 bars)  ← No trade!
  Run 5: bars 271-271 (1 bars)  ← No trade!
  ...
```

**Pattern:** Most non-neutral signals are isolated (1 bar), but even these should trigger:
- Bar N: prob=0.40 → Enter PSQ_ONLY
- Bar N+1: prob=0.50 → Exit to CASH_ONLY (respecting MIN_HOLD_BARS=1)
- Expected: 2 trades per run × 178 runs = **~356 trades**
- Actual: **3 trades**

---

## Configuration & Parameters

### Risk Management (Lines 94-99)
```cpp
const double PROFIT_TARGET = 0.02;        // 2% profit → take profit
const double STOP_LOSS = -0.015;          // -1.5% loss → exit to cash
const double CONFIDENCE_THRESHOLD = 0.55; // Unused (bypassed by direct mapping)
const int MIN_HOLD_BARS = 1;              // Minimum hold (prevent flip-flop)
const int MAX_HOLD_BARS = 100;            // Maximum hold (force re-evaluation)
```

### State Mapping Thresholds (Lines 143-156)
```cpp
prob >= 0.65 → TQQQ_ONLY   (3x long)
prob >= 0.55 → QQQ_TQQQ    (blended long)
prob >= 0.51 → QQQ_ONLY    (1x long)
prob >= 0.49 → CASH_ONLY   (very uncertain)
prob >= 0.45 → PSQ_ONLY    (1x short)
prob >= 0.35 → PSQ_SQQQ    (blended short)
prob <  0.35 → SQQQ_ONLY   (3x short)
```

---

## Attempted Solutions

### 1. ✗ Bypassed PSM's get_optimal_transition()
**Rationale:** PSM's internal logic too conservative
**Action:** Implemented direct probability-to-state mapping
**Result:** Target states calculated correctly but trades still blocked

### 2. ✗ Relaxed Thresholds (0.47-0.53 → 0.49-0.51)
**Rationale:** Increase non-cash signals
**Action:** Made CASH range narrower (from ±0.03 to ±0.01)
**Result:** More non-cash targets generated but still no execution

### 3. ✗ Lowered Confidence Threshold (0.7 → 0.55)
**Rationale:** Accept lower-confidence signals
**Action:** Reduced CONFIDENCE_THRESHOLD
**Result:** No effect (threshold is bypassed by direct mapping anyway)

### 4. ✗ Added Profit-Taking & Stop-Loss
**Rationale:** Force exits for risk management
**Action:** Implemented 2% profit / -1.5% loss exits
**Result:** Cannot test - not enough trades to hit these triggers

---

## Required Investigation

### Priority 1: State Update After Trade Execution
**Location:** Lines 196-260 (trade execution loop)
**Check:**
1. Is `current_position.state` updated after trades?
2. Does `portfolio.positions` map correctly affect state tracking?
3. Is there a hidden reset of `current_position.state` to CASH_ONLY?

**Debug steps:**
```cpp
// After each trade execution, add:
current_position.state = transition.target_state;
std::cerr << "DEBUG: Updated current_position.state to "
          << psm.state_to_string(current_position.state) << "\n";
```

### Priority 2: Cash Balance Tracking
**Location:** Lines 198, 214, 227
**Check:**
1. Is `portfolio.cash_balance` sufficient for new positions?
2. Are SELL trades properly crediting cash?
3. Is total capital (cash + positions) conserved?

**Debug steps:**
```cpp
// Before trade execution:
std::cerr << "DEBUG: Cash=" << portfolio.cash_balance
          << ", Trade value=" << trade_value
          << ", Can execute=" << (trade_value <= portfolio.cash_balance) << "\n";
```

### Priority 3: Minimum Hold Period Logic
**Location:** Lines 171-176
**Check:**
1. Is `current_position.bars_held` being incremented?
2. Does MIN_HOLD_BARS=1 actually allow trading on next bar?
3. Is the condition logic correct?

**Debug steps:**
```cpp
// Add before MIN_HOLD check:
std::cerr << "DEBUG: bars_held=" << current_position.bars_held
          << ", MIN_HOLD=" << MIN_HOLD_BARS
          << ", will_block=" << (current_position.bars_held < MIN_HOLD_BARS) << "\n";
```

### Priority 4: Transition Equality Check
**Location:** Line 187
**Check:**
1. Is enum comparison `!=` working correctly?
2. Are states being cast or converted somewhere?
3. Add explicit comparison logging

**Debug steps:**
```cpp
// Before if statement:
std::cerr << "DEBUG: Transition check: current="
          << static_cast<int>(transition.current_state)
          << ", target=" << static_cast<int>(transition.target_state)
          << ", will_execute=" << (transition.target_state != transition.current_state) << "\n";
```

---

## Proposed Fix (Hypothesis)

**Most likely issue:** `current_position.state` is not updated after trade execution.

**Fix location:** After line 260 (end of trade execution loop), add:

```cpp
// Update tracking state after all trades executed
if (transition.target_state != transition.current_state) {
    current_position.state = transition.target_state;
    current_position.entry_equity = portfolio.cash_balance +
                                    get_position_value(portfolio, bar.close);
    current_position.entry_price = bar.close;
    current_position.bars_held = 0;  // Reset hold counter
}
```

This ensures:
1. Next iteration knows we changed states
2. PnL calculation uses correct entry point
3. Hold period timer resets
4. Profit-taking/stop-loss triggers have proper baseline

---

## Testing Plan

### 1. Unit Test: State Update
```cpp
// Verify state is updated after trade execution
initial_state = CASH_ONLY
execute_trade(BUY, QQQ) → expect current_position.state == QQQ_ONLY
```

### 2. Integration Test: Consecutive Trades
```cpp
// Verify trades execute on consecutive non-neutral signals
Bar 1: prob=0.55 → Enter QQQ_TQQQ
Bar 2: prob=0.45 → Exit to PSQ_ONLY (2 trades: SELL QQQ/TQQQ, BUY PSQ)
Bar 3: prob=0.50 → Exit to CASH (1 trade: SELL PSQ)

Expected: 5 total trades
```

### 3. Regression Test: Trade Frequency
```cpp
// Run on 2000-bar dataset
Input: 228 non-cash signals (11.4%)
Expected minimum: 228 trades (one per signal)
Expected realistic: 350-400 trades (accounting for state transitions)
```

### 4. Performance Test: Full Dataset
```cpp
// Run on 292K bars with target 50 trades/block
Blocks: 292385 / 480 ≈ 609 blocks
Expected trades: 609 * 50 = 30,450 trades
Acceptable range: 25,000 - 35,000 trades
```

---

## Dependencies & Related Files

### Core Files (Modified)
1. `src/cli/execute_trades_command.cpp` - Main execution logic (BROKEN)
2. `src/cli/generate_signals_command.cpp` - Signal generation (WORKING)
3. `src/strategy/online_ensemble_strategy.cpp` - EWRLS predictor (WORKING)

### Support Files (Unmodified)
4. `include/backend/position_state_machine.h` - State definitions
5. `include/cli/ensemble_workflow_command.h` - Command interface
6. `include/strategy/signal_output.h` - Signal data structures
7. `src/strategy/signal_output.cpp` - JSON parsing (FIXED)

### Configuration
8. CMakeLists.txt - Build configuration
9. build.sh - Build script

---

## Metrics & Success Criteria

### Current Performance
- Trade frequency: **0.15%** (3 / 2000 bars)
- Utilization: **0.6%** (3 / 228 non-cash signals)
- Return: -3.50% over 2000 bars
- Max drawdown: 4.83%

### Target Performance
- Trade frequency: **10%** (50 / 480 bars)
- Utilization: **> 80%** (trades on most non-neutral signals)
- Return: 0.5% per block = **+2.1%** over 2000 bars (4.2 blocks)
- Max drawdown: < 5%

### Success Criteria for Fix
1. **Primary:** Trade count > 200 on 2000-bar dataset
2. **Secondary:** State transitions match signal changes (> 80% of non-cash signals)
3. **Tertiary:** Profit-taking and stop-loss triggers activate during test
4. **Validation:** Full dataset produces 25K-35K trades (50±10 per block)

---

## Additional Context

### OnlineEnsemble EWRLS Status
- ✓ Feature extraction: 126 features from UnifiedFeatureEngine
- ✓ Multi-horizon prediction: 1, 5, 10 bar horizons
- ✓ Delayed update tracking: Fixed bar_index bug (was using bar_id hashes)
- ✓ Ensemble weighting: 0.3, 0.5, 0.2 for horizons
- ✓ Probability conversion: tanh scaling with 0.05-0.95 clipping
- ✓ Signal classification: Direct threshold mapping

### PSM Integration Status
- ✓ 7-state machine: CASH, QQQ, TQQQ, PSQ, SQQQ, QQQ_TQQQ, PSQ_SQQQ
- ✓ 4-instrument support: QQQ (1x), TQQQ (3x), PSQ (-1x), SQQQ (-3x)
- ✗ Trade execution: BLOCKED (this bug)
- ? Position tracking: Suspected broken
- ? Risk management: Untested (no trades to validate)

---

## Timeline

- **2025-10-06 20:00:** EWRLS signal generation fixed (was all 0.5 probabilities)
- **2025-10-06 20:15:** Direct state mapping implemented (bypassed PSM)
- **2025-10-06 20:30:** Thresholds relaxed (0.47-0.53 → 0.49-0.51)
- **2025-10-06 20:45:** Bug identified - trade execution blocked
- **2025-10-06 21:00:** Bug report created

**Next steps:** Debug state update logic, implement fix, retest

---

## References

- User requirement: "target trade frequency of 50 per block (about 10%)"
- User requirement: "exiting and thus taking profit should also be considered as an exception for holding out period"
- User requirement: "flip-flop is not encouraged but that does not mean that we continue to hold while losing money; rather stay cash for an opportunity"

---

**End of Bug Report**
