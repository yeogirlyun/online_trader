# PSM Trade Execution Bug - Mega Document

**Generated:** 2025-10-06
**Purpose:** Comprehensive source code reference for debugging trade execution blocking issue

This document contains all source files related to the PSM trade execution bug where only 3 trades are executed instead of the expected 200+ trades.

---

## Table of Contents

1. [Bug Report](#bug-report)
2. [Core Execution Logic](#core-execution-logic)
   - execute_trades_command.cpp
   - execute_trades_command.h (partial)
3. [Signal Generation](#signal-generation)
   - generate_signals_command.cpp
   - online_ensemble_strategy.cpp
   - online_ensemble_strategy.h
4. [Data Structures](#data-structures)
   - signal_output.h
   - signal_output.cpp
   - position_state_machine.h (interface only)
5. [Supporting Files](#supporting-files)
   - ensemble_workflow_command.h
6. [Test Data & Logs](#test-data--logs)

---


# 1. Bug Report

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


---

# 2. Core Source Files



## Core Execution Logic - execute_trades_command.cpp

**File:** `src/cli/execute_trades_command.cpp`
**Lines:** 512

```cpp
#include "cli/ensemble_workflow_command.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include "strategy/signal_output.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace sentio {
namespace cli {

int ExecuteTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string signal_path = get_arg(args, "--signals", "");
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "trades.jsonl");
    double starting_capital = std::stod(get_arg(args, "--capital", "100000"));
    double buy_threshold = std::stod(get_arg(args, "--buy-threshold", "0.53"));
    double sell_threshold = std::stod(get_arg(args, "--sell-threshold", "0.47"));
    bool enable_kelly = !has_flag(args, "--no-kelly");
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    if (signal_path.empty() || data_path.empty()) {
        std::cerr << "Error: --signals and --data are required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Trade Execution ===\n";
    std::cout << "Signals: " << signal_path << "\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Starting Capital: $" << std::fixed << std::setprecision(2) << starting_capital << "\n";
    std::cout << "Kelly Sizing: " << (enable_kelly ? "Enabled" : "Disabled") << "\n\n";

    // Load signals
    std::cout << "Loading signals...\n";
    std::vector<SignalOutput> signals;
    std::ifstream sig_file(signal_path);
    if (!sig_file) {
        std::cerr << "Error: Could not open signal file\n";
        return 1;
    }

    std::string line;
    while (std::getline(sig_file, line)) {
        // Parse JSONL (simplified)
        SignalOutput sig = SignalOutput::from_json(line);
        signals.push_back(sig);
    }
    std::cout << "Loaded " << signals.size() << " signals\n";

    // Load market data
    std::cout << "Loading market data...\n";
    auto bars = utils::read_csv_data(data_path);
    if (bars.empty()) {
        std::cerr << "Error: Could not load data\n";
        return 1;
    }
    std::cout << "Loaded " << bars.size() << " bars\n\n";

    if (signals.size() != bars.size()) {
        std::cerr << "Warning: Signal count (" << signals.size() << ") != bar count (" << bars.size() << ")\n";
    }

    // Create Position State Machine for 4-instrument strategy
    PositionStateMachine psm;

    // Portfolio state tracking
    PortfolioState portfolio;
    portfolio.cash_balance = starting_capital;
    portfolio.total_equity = starting_capital;

    // Trade history
    PortfolioHistory history;
    history.starting_capital = starting_capital;
    history.equity_curve.push_back(starting_capital);

    // Track position entry for profit-taking and stop-loss
    struct PositionTracking {
        double entry_price = 0.0;
        double entry_equity = 0.0;
        int bars_held = 0;
        PositionStateMachine::State state = PositionStateMachine::State::CASH_ONLY;
    };
    PositionTracking current_position;
    current_position.entry_equity = starting_capital;

    // Risk management parameters
    const double PROFIT_TARGET = 0.02;      // 2% profit → take profit
    const double STOP_LOSS = -0.015;        // -1.5% loss → exit to cash
    const double CONFIDENCE_THRESHOLD = 0.55; // Lower threshold for more trades
    const int MIN_HOLD_BARS = 1;            // Minimum hold (prevent flip-flop)
    const int MAX_HOLD_BARS = 100;          // Maximum hold (force re-evaluation)

    std::cout << "Executing trades with Position State Machine...\n";
    std::cout << "Target: ~50 trades/block, 2% profit-taking, -1.5% stop-loss\n\n";

    for (size_t i = 0; i < std::min(signals.size(), bars.size()); ++i) {
        const auto& signal = signals[i];
        const auto& bar = bars[i];

        // Update position tracking
        current_position.bars_held++;
        double current_equity = portfolio.cash_balance + get_position_value(portfolio, bar.close);
        double position_pnl_pct = (current_equity - current_position.entry_equity) / current_position.entry_equity;

        // Check profit-taking condition
        bool should_take_profit = (position_pnl_pct >= PROFIT_TARGET &&
                                   current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check stop-loss condition
        bool should_stop_loss = (position_pnl_pct <= STOP_LOSS &&
                                current_position.state != PositionStateMachine::State::CASH_ONLY);

        // Check maximum hold period
        bool should_reevaluate = (current_position.bars_held >= MAX_HOLD_BARS);

        // Force exit to cash if profit target hit or stop loss triggered
        PositionStateMachine::State forced_target_state = PositionStateMachine::State::INVALID;
        std::string exit_reason = "";

        if (should_take_profit) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "PROFIT_TARGET (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_stop_loss) {
            forced_target_state = PositionStateMachine::State::CASH_ONLY;
            exit_reason = "STOP_LOSS (" + std::to_string(position_pnl_pct * 100) + "%)";
        } else if (should_reevaluate) {
            exit_reason = "MAX_HOLD_PERIOD";
            // Don't force cash, but allow PSM to reevaluate
        }

        // Direct state mapping from probability (bypass PSM's internal logic)
        // Relaxed thresholds for more frequent trading
        PositionStateMachine::State target_state;

        if (signal.probability >= 0.65) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Strong long (3x)
        } else if (signal.probability >= 0.55) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Moderate long (blended)
        } else if (signal.probability >= 0.51) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Weak long (1x)
        } else if (signal.probability >= 0.49) {
            target_state = PositionStateMachine::State::CASH_ONLY;  // Very uncertain → cash
        } else if (signal.probability >= 0.45) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Weak short (-1x)
        } else if (signal.probability >= 0.35) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Moderate short (blended)
        } else {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Strong short (-3x)
        }

        // Prepare transition structure
        PositionStateMachine::StateTransition transition;
        transition.current_state = current_position.state;
        transition.target_state = target_state;

        // Override with forced exit if needed
        if (forced_target_state != PositionStateMachine::State::INVALID) {
            transition.target_state = forced_target_state;
            transition.optimal_action = exit_reason;
        }

        // Apply minimum hold period (prevent flip-flop)
        if (current_position.bars_held < MIN_HOLD_BARS &&
            transition.current_state != PositionStateMachine::State::CASH_ONLY &&
            forced_target_state == PositionStateMachine::State::INVALID) {
            // Keep current state
            transition.target_state = transition.current_state;
        }

        // Debug: Log state transitions
        if (verbose && i % 500 == 0) {
            std::cout << "  [" << i << "] Signal: " << signal.probability
                     << " | Current: " << psm.state_to_string(transition.current_state)
                     << " | Target: " << psm.state_to_string(transition.target_state)
                     << " | PnL: " << (position_pnl_pct * 100) << "%\n";
        }

        // Execute state transition
        if (transition.target_state != transition.current_state) {
            // Calculate positions for target state
            std::map<std::string, double> target_positions =
                calculate_target_positions(transition.target_state, portfolio.cash_balance + get_position_value(portfolio, bar.close), bar.close);

            // Execute transitions
            for (const auto& [symbol, target_shares] : target_positions) {
                double current_shares = portfolio.positions.count(symbol) ?
                                       portfolio.positions[symbol].quantity : 0.0;

                double delta_shares = target_shares - current_shares;

                if (std::abs(delta_shares) > 0.01) {  // Min trade size
                    TradeAction action = delta_shares > 0 ? TradeAction::BUY : TradeAction::SELL;
                    double quantity = std::abs(delta_shares);
                    double trade_value = quantity * bar.close;

                    // Execute trade
                    if (action == TradeAction::BUY) {
                        if (trade_value <= portfolio.cash_balance) {
                            portfolio.cash_balance -= trade_value;
                            portfolio.positions[symbol].quantity += quantity;
                            portfolio.positions[symbol].avg_price = bar.close;
                            portfolio.positions[symbol].symbol = symbol;

                            // Record trade
                            TradeRecord trade;
                            trade.bar_id = bar.bar_id;
                            trade.timestamp_ms = bar.timestamp_ms;
                            trade.bar_index = i;
                            trade.symbol = symbol;
                            trade.action = action;
                            trade.quantity = quantity;
                            trade.price = bar.close;
                            trade.trade_value = trade_value;
                            trade.fees = 0.0;
                            trade.cash_balance = portfolio.cash_balance;
                            trade.portfolio_value = portfolio.cash_balance + get_position_value(portfolio, bar.close);
                            trade.position_quantity = portfolio.positions[symbol].quantity;
                            trade.position_avg_price = portfolio.positions[symbol].avg_price;
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state);

                            history.trades.push_back(trade);

                            if (verbose) {
                                std::cout << "  [" << i << "] " << symbol << " BUY "
                                         << quantity << " @ $" << bar.close
                                         << " | Portfolio: $" << trade.portfolio_value << "\n";
                            }
                        }
                    } else {  // SELL
                        double sell_quantity = std::min(quantity, portfolio.positions[symbol].quantity);
                        if (sell_quantity > 0) {
                            portfolio.cash_balance += sell_quantity * bar.close;
                            portfolio.positions[symbol].quantity -= sell_quantity;

                            if (portfolio.positions[symbol].quantity < 0.01) {
                                portfolio.positions.erase(symbol);
                            }

                            // Record trade
                            TradeRecord trade;
                            trade.bar_id = bar.bar_id;
                            trade.timestamp_ms = bar.timestamp_ms;
                            trade.bar_index = i;
                            trade.symbol = symbol;
                            trade.action = action;
                            trade.quantity = sell_quantity;
                            trade.price = bar.close;
                            trade.trade_value = sell_quantity * bar.close;
                            trade.fees = 0.0;
                            trade.cash_balance = portfolio.cash_balance;
                            trade.portfolio_value = portfolio.cash_balance + get_position_value(portfolio, bar.close);
                            trade.position_quantity = portfolio.positions.count(symbol) ? portfolio.positions[symbol].quantity : 0.0;
                            trade.position_avg_price = portfolio.positions.count(symbol) ? portfolio.positions[symbol].avg_price : 0.0;
                            trade.reason = "PSM: " + psm.state_to_string(transition.current_state) +
                                         " -> " + psm.state_to_string(transition.target_state);

                            history.trades.push_back(trade);

                            if (verbose) {
                                std::cout << "  [" << i << "] " << symbol << " SELL "
                                         << sell_quantity << " @ $" << bar.close
                                         << " | Portfolio: $" << trade.portfolio_value << "\n";
                            }
                        }
                    }
                }
            }

            // Reset position tracking on state change
            current_position.entry_price = bar.close;
            current_position.entry_equity = portfolio.cash_balance + get_position_value(portfolio, bar.close);
            current_position.bars_held = 0;
            current_position.state = transition.target_state;
        }

        // Update portfolio total equity
        portfolio.total_equity = portfolio.cash_balance + get_position_value(portfolio, bar.close);

        // Record equity curve
        history.equity_curve.push_back(portfolio.total_equity);

        // Calculate drawdown
        double peak = *std::max_element(history.equity_curve.begin(), history.equity_curve.end());
        double drawdown = (peak - portfolio.total_equity) / peak;
        history.drawdown_curve.push_back(drawdown);
        history.max_drawdown = std::max(history.max_drawdown, drawdown);
    }

    history.final_capital = portfolio.total_equity;
    history.total_trades = static_cast<int>(history.trades.size());

    // Calculate win rate
    for (const auto& trade : history.trades) {
        if (trade.action == TradeAction::SELL) {
            double pnl = (trade.price - trade.position_avg_price) * trade.quantity;
            if (pnl > 0) history.winning_trades++;
        }
    }

    std::cout << "\nTrade execution complete!\n";
    std::cout << "Total trades: " << history.total_trades << "\n";
    std::cout << "Final capital: $" << std::fixed << std::setprecision(2) << history.final_capital << "\n";
    std::cout << "Total return: " << ((history.final_capital / history.starting_capital - 1.0) * 100) << "%\n";
    std::cout << "Max drawdown: " << (history.max_drawdown * 100) << "%\n\n";

    // Save trade history
    std::cout << "Saving trade history to " << output_path << "...\n";
    if (csv_output) {
        save_trades_csv(history, output_path);
    } else {
        save_trades_jsonl(history, output_path);
    }

    // Save equity curve
    std::string equity_path = output_path.substr(0, output_path.find_last_of('.')) + "_equity.csv";
    save_equity_curve(history, equity_path);

    std::cout << "✅ Trade execution complete!\n";
    return 0;
}

// Helper function: Calculate total value of all positions
double ExecuteTradesCommand::get_position_value(const PortfolioState& portfolio, double current_price) {
    double total = 0.0;
    for (const auto& [symbol, position] : portfolio.positions) {
        total += position.quantity * current_price;
    }
    return total;
}

// Helper function: Calculate target positions for each PSM state
std::map<std::string, double> ExecuteTradesCommand::calculate_target_positions(
    PositionStateMachine::State state,
    double total_capital,
    double price) {

    std::map<std::string, double> positions;

    switch (state) {
        case PositionStateMachine::State::CASH_ONLY:
            // No positions - all cash
            break;

        case PositionStateMachine::State::QQQ_ONLY:
            // 100% in QQQ (moderate long)
            positions["QQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::TQQQ_ONLY:
            // 100% in TQQQ (strong long, 3x leverage)
            positions["TQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::PSQ_ONLY:
            // 100% in PSQ (moderate short, -1x)
            positions["PSQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::SQQQ_ONLY:
            // 100% in SQQQ (strong short, -3x)
            positions["SQQQ"] = total_capital / price;
            break;

        case PositionStateMachine::State::QQQ_TQQQ:
            // Split: 50% QQQ + 50% TQQQ (blended long)
            positions["QQQ"] = (total_capital * 0.5) / price;
            positions["TQQQ"] = (total_capital * 0.5) / price;
            break;

        case PositionStateMachine::State::PSQ_SQQQ:
            // Split: 50% PSQ + 50% SQQQ (blended short)
            positions["PSQ"] = (total_capital * 0.5) / price;
            positions["SQQQ"] = (total_capital * 0.5) / price;
            break;

        default:
            // INVALID or unknown state - go to cash
            break;
    }

    return positions;
}

void ExecuteTradesCommand::save_trades_jsonl(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& trade : history.trades) {
        out << "{"
            << "\"bar_id\":" << trade.bar_id << ","
            << "\"timestamp_ms\":" << trade.timestamp_ms << ","
            << "\"bar_index\":" << trade.bar_index << ","
            << "\"symbol\":\"" << trade.symbol << "\","
            << "\"action\":\"" << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << "\","
            << "\"quantity\":" << std::fixed << std::setprecision(4) << trade.quantity << ","
            << "\"price\":" << std::setprecision(2) << trade.price << ","
            << "\"trade_value\":" << trade.trade_value << ","
            << "\"fees\":" << trade.fees << ","
            << "\"cash_balance\":" << trade.cash_balance << ","
            << "\"portfolio_value\":" << trade.portfolio_value << ","
            << "\"position_quantity\":" << trade.position_quantity << ","
            << "\"position_avg_price\":" << trade.position_avg_price << ","
            << "\"reason\":\"" << trade.reason << "\""
            << "}\n";
    }
}

void ExecuteTradesCommand::save_trades_csv(const PortfolioHistory& history,
                                          const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,action,quantity,price,trade_value,fees,"
        << "cash_balance,portfolio_value,position_quantity,position_avg_price,reason\n";

    // Data
    for (const auto& trade : history.trades) {
        out << trade.bar_id << ","
            << trade.timestamp_ms << ","
            << trade.bar_index << ","
            << trade.symbol << ","
            << (trade.action == TradeAction::BUY ? "BUY" : "SELL") << ","
            << std::fixed << std::setprecision(4) << trade.quantity << ","
            << std::setprecision(2) << trade.price << ","
            << trade.trade_value << ","
            << trade.fees << ","
            << trade.cash_balance << ","
            << trade.portfolio_value << ","
            << trade.position_quantity << ","
            << trade.position_avg_price << ","
            << "\"" << trade.reason << "\"\n";
    }
}

void ExecuteTradesCommand::save_equity_curve(const PortfolioHistory& history,
                                            const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open equity curve file: " + path);
    }

    // Header
    out << "bar_index,equity,drawdown\n";

    // Data
    for (size_t i = 0; i < history.equity_curve.size(); ++i) {
        double drawdown = (i < history.drawdown_curve.size()) ? history.drawdown_curve[i] : 0.0;
        out << i << ","
            << std::fixed << std::setprecision(2) << history.equity_curve[i] << ","
            << std::setprecision(4) << drawdown << "\n";
    }
}

void ExecuteTradesCommand::show_help() const {
    std::cout << R"(
Execute OnlineEnsemble Trades
==============================

Execute trades from signal file and generate portfolio history.

USAGE:
    sentio_cli execute-trades --signals <path> --data <path> [OPTIONS]

REQUIRED:
    --signals <path>           Path to signal file (JSONL or CSV)
    --data <path>              Path to market data file

OPTIONS:
    --output <path>            Output trade file (default: trades.jsonl)
    --capital <amount>         Starting capital (default: 100000)
    --buy-threshold <val>      Buy signal threshold (default: 0.53)
    --sell-threshold <val>     Sell signal threshold (default: 0.47)
    --no-kelly                 Disable Kelly criterion sizing
    --csv                      Output in CSV format
    --verbose, -v              Show each trade

EXAMPLES:
    # Execute trades with default settings
    sentio_cli execute-trades --signals signals.jsonl --data data/SPY.csv

    # Custom capital and thresholds
    sentio_cli execute-trades --signals signals.jsonl --data data/QQQ.bin \
        --capital 50000 --buy-threshold 0.55 --sell-threshold 0.45

    # Verbose mode with CSV output
    sentio_cli execute-trades --signals signals.jsonl --data data/futures.bin \
        --verbose --csv --output trades.csv

OUTPUT FILES:
    - trades.jsonl (or .csv)   Trade-by-trade history
    - trades_equity.csv        Equity curve and drawdowns

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

---


## Signal Generation - generate_signals_command.cpp

**File:** `src/cli/generate_signals_command.cpp`
**Lines:** 236

```cpp
#include "cli/ensemble_workflow_command.h"
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace sentio {
namespace cli {

int GenerateSignalsCommand::execute(const std::vector<std::string>& args) {
    using namespace sentio;

    // Parse arguments
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "signals.jsonl");
    int warmup_bars = std::stoi(get_arg(args, "--warmup", "100"));
    int start_bar = std::stoi(get_arg(args, "--start", "0"));
    int end_bar = std::stoi(get_arg(args, "--end", "-1"));
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    if (data_path.empty()) {
        std::cerr << "Error: --data is required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Signal Generation ===\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Warmup: " << warmup_bars << " bars\n\n";

    // Load market data
    std::cout << "Loading market data...\n";
    auto bars = utils::read_csv_data(data_path);
    if (bars.empty()) {
        std::cerr << "Error: Could not load data from " << data_path << "\n";
        return 1;
    }

    if (end_bar < 0 || end_bar > static_cast<int>(bars.size())) {
        end_bar = static_cast<int>(bars.size());
    }

    std::cout << "Loaded " << bars.size() << " bars\n";
    std::cout << "Processing range: " << start_bar << " to " << end_bar << "\n\n";

    // Create OnlineEnsembleStrategy
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.warmup_samples = warmup_bars;
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.ewrls_lambda = 0.995;
    config.buy_threshold = 0.53;
    config.sell_threshold = 0.47;
    config.enable_threshold_calibration = false;  // Disable auto-calibration
    config.enable_adaptive_learning = true;

    OnlineEnsembleStrategy strategy(config);

    // Generate signals
    std::vector<SignalOutput> signals;
    int progress_interval = (end_bar - start_bar) / 20;  // 5% increments

    std::cout << "Generating signals...\n";
    for (int i = start_bar; i < end_bar; ++i) {
        // Update strategy with bar (processes pending updates)
        strategy.on_bar(bars[i]);

        // Generate signal from strategy
        sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);

        // Convert to CLI output format
        SignalOutput output;
        output.bar_id = strategy_signal.bar_id;
        output.timestamp_ms = strategy_signal.timestamp_ms;
        output.bar_index = strategy_signal.bar_index;
        output.symbol = strategy_signal.symbol;
        output.probability = strategy_signal.probability;
        output.signal_type = strategy_signal.signal_type;
        output.prediction_horizon = strategy_signal.prediction_horizon;

        // Calculate ensemble agreement from metadata
        output.ensemble_agreement = 0.0;
        if (strategy_signal.metadata.count("ensemble_agreement")) {
            output.ensemble_agreement = std::stod(strategy_signal.metadata.at("ensemble_agreement"));
        }

        signals.push_back(output);

        // Progress reporting
        if (verbose && progress_interval > 0 && (i - start_bar) % progress_interval == 0) {
            double pct = 100.0 * (i - start_bar) / (end_bar - start_bar);
            std::cout << "  Progress: " << std::fixed << std::setprecision(1)
                     << pct << "% (" << (i - start_bar) << "/" << (end_bar - start_bar) << ")\n";
        }
    }

    std::cout << "Generated " << signals.size() << " signals\n\n";

    // Save signals
    std::cout << "Saving signals to " << output_path << "...\n";
    if (csv_output) {
        save_signals_csv(signals, output_path);
    } else {
        save_signals_jsonl(signals, output_path);
    }

    // Print summary
    int long_signals = 0, short_signals = 0, neutral_signals = 0;
    for (const auto& sig : signals) {
        if (sig.signal_type == SignalType::LONG) long_signals++;
        else if (sig.signal_type == SignalType::SHORT) short_signals++;
        else neutral_signals++;
    }

    std::cout << "\n=== Signal Summary ===\n";
    std::cout << "Total signals: " << signals.size() << "\n";
    std::cout << "Long signals:  " << long_signals << " (" << (100.0 * long_signals / signals.size()) << "%)\n";
    std::cout << "Short signals: " << short_signals << " (" << (100.0 * short_signals / signals.size()) << "%)\n";
    std::cout << "Neutral:       " << neutral_signals << " (" << (100.0 * neutral_signals / signals.size()) << "%)\n";

    // Get strategy performance - not implemented in stub
    // auto metrics = strategy.get_performance_metrics();
    std::cout << "\n=== Strategy Metrics ===\n";
    std::cout << "Strategy: OnlineEnsemble (stub version)\n";
    std::cout << "Note: Full metrics available after execute-trades and analyze-trades\n";

    std::cout << "\n✅ Signals saved successfully!\n";
    return 0;
}

void GenerateSignalsCommand::save_signals_jsonl(const std::vector<SignalOutput>& signals,
                                               const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& sig : signals) {
        // Convert signal_type enum to string
        std::string signal_type_str;
        switch (sig.signal_type) {
            case SignalType::LONG: signal_type_str = "LONG"; break;
            case SignalType::SHORT: signal_type_str = "SHORT"; break;
            default: signal_type_str = "NEUTRAL"; break;
        }

        // Create JSON line
        out << "{"
            << "\"bar_id\":" << sig.bar_id << ","
            << "\"timestamp_ms\":" << sig.timestamp_ms << ","
            << "\"bar_index\":" << sig.bar_index << ","
            << "\"symbol\":\"" << sig.symbol << "\","
            << "\"probability\":" << std::fixed << std::setprecision(6) << sig.probability << ","
            << "\"signal_type\":\"" << signal_type_str << "\","
            << "\"prediction_horizon\":" << sig.prediction_horizon << ","
            << "\"ensemble_agreement\":" << sig.ensemble_agreement
            << "}\n";
    }
}

void GenerateSignalsCommand::save_signals_csv(const std::vector<SignalOutput>& signals,
                                             const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,probability,confidence,signal_type,prediction_horizon,ensemble_agreement\n";

    // Data
    for (const auto& sig : signals) {
        out << sig.bar_id << ","
            << sig.timestamp_ms << ","
            << sig.bar_index << ","
            << sig.symbol << ","
            << std::fixed << std::setprecision(6) << sig.probability << ","
            // << sig.confidence << ","  // Field not in SignalOutput
            << static_cast<int>(sig.signal_type) << ","
            << sig.prediction_horizon << ","
            << sig.ensemble_agreement << "\n";
    }
}

void GenerateSignalsCommand::show_help() const {
    std::cout << R"(
Generate OnlineEnsemble Signals
================================

Generate trading signals from market data using OnlineEnsemble strategy.

USAGE:
    sentio_cli generate-signals --data <path> [OPTIONS]

REQUIRED:
    --data <path>              Path to market data file (CSV or binary)

OPTIONS:
    --output <path>            Output signal file (default: signals.jsonl)
    --warmup <bars>            Warmup period before trading (default: 100)
    --start <bar>              Start bar index (default: 0)
    --end <bar>                End bar index (default: all)
    --csv                      Output in CSV format instead of JSONL
    --verbose, -v              Show progress updates

EXAMPLES:
    # Generate signals from data
    sentio_cli generate-signals --data data/SPY_1min.csv --output signals.jsonl

    # With custom warmup and range
    sentio_cli generate-signals --data data/QQQ.bin --warmup 200 --start 1000 --end 5000

    # CSV output with verbose progress
    sentio_cli generate-signals --data data/futures.bin --csv --verbose

OUTPUT FORMAT (JSONL):
    Each line contains:
    {
        "bar_id": 12345,
        "timestamp_ms": 1609459200000,
        "probability": 0.6234,
        "confidence": 0.82,
        "signal_type": "1",  // 0=NEUTRAL, 1=LONG, 2=SHORT
        "prediction_horizon": 5,
        "ensemble_agreement": 0.75
    }

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

---


## EWRLS Strategy - online_ensemble_strategy.cpp

**File:** `src/strategy/online_ensemble_strategy.cpp`
**Lines:** 381

```cpp
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // Initialize feature engine first to get actual feature count
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>();

    // Initialize multi-horizon EWRLS predictor with correct feature count
    size_t num_features = feature_engine_->get_features().size();
    if (num_features == 0) {
        // Feature engine not ready yet, use default config total
        features::UnifiedFeatureEngineConfig default_config;
        num_features = default_config.total_features();  // 126 features
    }
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    prob = std::clamp(prob, 0.05, 0.95);  // Keep within reasonable bounds

    output.probability = prob;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    if (bar_history_.size() < MIN_FEATURES_BARS) {
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_ready()) {
        return {};
    }

    return feature_engine_->get_features();
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = features;
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    pending_updates_[pred.target_bar_index] = pred;
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& pred = it->second;

        // Calculate actual return
        double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
        if (!pred.is_long) {
            actual_return = -actual_return;  // Invert for short
        }

        // Update the appropriate horizon predictor
        ensemble_predictor_->update(pred.horizon, pred.features, actual_return);

        // Debug logging
        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed update for horizon " + std::to_string(pred.horizon) +
                           ", return=" + std::to_string(actual_return) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        // Remove processed prediction
        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

} // namespace sentio

```

---


## CLI Interface - ensemble_workflow_command.h

**File:** `include/cli/ensemble_workflow_command.h`
**Lines:** 242

```cpp
#pragma once

#include "cli/command_interface.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Complete workflow command for OnlineEnsemble experiments
 *
 * Workflow:
 * 1. generate-signals: Create signal file from market data
 * 2. execute-trades: Simulate trading with portfolio manager
 * 3. analyze: Generate performance reports
 * 4. run-all: Execute complete workflow
 */
class EnsembleWorkflowCommand : public Command {
public:
    std::string get_name() const override { return "ensemble"; }
    std::string get_description() const override {
        return "OnlineEnsemble workflow: generate signals, execute trades, analyze results";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    // Sub-commands
    int generate_signals(const std::vector<std::string>& args);
    int execute_trades(const std::vector<std::string>& args);
    int analyze(const std::vector<std::string>& args);
    int run_all(const std::vector<std::string>& args);

    // Configuration structures
    struct SignalGenConfig {
        std::string data_path;
        std::string output_path;
        int warmup_bars = 100;
        int start_bar = 0;
        int end_bar = -1;  // -1 = all

        // Strategy config
        std::vector<int> horizons = {1, 5, 10};
        std::vector<double> weights = {0.3, 0.5, 0.2};
        double lambda = 0.995;
        bool verbose = false;
    };

    struct TradeExecConfig {
        std::string signal_path;
        std::string data_path;
        std::string output_path;

        double starting_capital = 100000.0;
        double buy_threshold = 0.53;
        double sell_threshold = 0.47;
        double kelly_fraction = 0.25;
        bool enable_kelly = true;
        bool verbose = false;
    };

    struct AnalysisConfig {
        std::string trades_path;
        std::string output_path;
        bool show_detailed = true;
        bool show_trades = false;
        bool export_csv = false;
        bool export_json = true;
    };

    // Parsing helpers
    SignalGenConfig parse_signal_config(const std::vector<std::string>& args);
    TradeExecConfig parse_trade_config(const std::vector<std::string>& args);
    AnalysisConfig parse_analysis_config(const std::vector<std::string>& args);
};

/**
 * @brief Signal generation command (standalone)
 */
class GenerateSignalsCommand : public Command {
public:
    std::string get_name() const override { return "generate-signals"; }
    std::string get_description() const override {
        return "Generate OnlineEnsemble signals from market data";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct SignalOutput {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        double probability;
        double confidence;
        SignalType signal_type;
        int prediction_horizon;

        // Multi-horizon data
        std::map<int, double> horizon_predictions;
        double ensemble_agreement;
    };

    void save_signals_jsonl(const std::vector<SignalOutput>& signals,
                           const std::string& path);
    void save_signals_csv(const std::vector<SignalOutput>& signals,
                         const std::string& path);
};

/**
 * @brief Trade execution command (standalone)
 */
class ExecuteTradesCommand : public Command {
public:
    std::string get_name() const override { return "execute-trades"; }
    std::string get_description() const override {
        return "Execute trades from signal file and generate portfolio history";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

public:
    struct TradeRecord {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        TradeAction action;
        double quantity;
        double price;
        double trade_value;
        double fees;
        std::string reason;

        // Portfolio state after trade
        double cash_balance;
        double portfolio_value;
        double position_quantity;
        double position_avg_price;
    };

    struct PortfolioHistory {
        std::vector<TradeRecord> trades;
        std::vector<double> equity_curve;
        std::vector<double> drawdown_curve;

        double starting_capital;
        double final_capital;
        double max_drawdown;
        int total_trades;
        int winning_trades;
    };

    void save_trades_jsonl(const PortfolioHistory& history, const std::string& path);
    void save_trades_csv(const PortfolioHistory& history, const std::string& path);
    void save_equity_curve(const PortfolioHistory& history, const std::string& path);

    // PSM helper functions
    static double get_position_value(const PortfolioState& portfolio, double current_price);
    static std::map<std::string, double> calculate_target_positions(
        PositionStateMachine::State state,
        double total_capital,
        double price);
};

/**
 * @brief Analysis and reporting command (standalone)
 */
class AnalyzeTradesCommand : public Command {
public:
    std::string get_name() const override { return "analyze-trades"; }
    std::string get_description() const override {
        return "Analyze trade history and generate performance reports";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct PerformanceReport {
        // Returns
        double total_return_pct;
        double annualized_return;
        double monthly_return;
        double daily_return;

        // Risk metrics
        double max_drawdown;
        double avg_drawdown;
        double volatility;
        double downside_deviation;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;

        // Trading metrics
        int total_trades;
        int winning_trades;
        int losing_trades;
        double win_rate;
        double profit_factor;
        double avg_win;
        double avg_loss;
        double avg_trade;
        double largest_win;
        double largest_loss;

        // Position metrics
        double avg_holding_period;
        double max_holding_period;
        int total_long_trades;
        int total_short_trades;

        // Kelly metrics
        double kelly_criterion;
        double avg_position_size;
        double max_position_size;

        // Time analysis
        int trading_days;
        int bars_traded;
        std::string start_date;
        std::string end_date;
    };

    PerformanceReport calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades);
    void print_report(const PerformanceReport& report);
    void save_report_json(const PerformanceReport& report, const std::string& path);
    void generate_plots(const std::vector<double>& equity_curve, const std::string& output_dir);
};

} // namespace cli
} // namespace sentio

```

---


## Strategy Interface - online_ensemble_strategy.h

**File:** `include/strategy/online_ensemble_strategy.h`
**Lines:** 155

```cpp
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::vector<double> features;
        double entry_price;
        bool is_long;
    };
    std::map<int, HorizonPrediction> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Private methods
    std::vector<double> extract_features(const Bar& current_bar);
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

---


## Data Structures - signal_output.h

**File:** `include/strategy/signal_output.h`
**Lines:** 39

```cpp
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

enum class SignalType {
    NEUTRAL,
    LONG,
    SHORT
};

struct SignalOutput {
    // Core fields
    uint64_t bar_id = 0;           
    int64_t timestamp_ms = 0;
    int bar_index = 0;             
    std::string symbol;
    double probability = 0.0;       
    SignalType signal_type = SignalType::NEUTRAL;
    std::string strategy_name;
    std::string strategy_version;
    
    // NEW: Multi-bar prediction fields
    int prediction_horizon = 1;        // How many bars ahead this predicts (default=1 for backward compat)
    uint64_t target_bar_id = 0;       // The bar this prediction targets
    bool requires_hold = false;        // Signal requires minimum hold period
    int signal_generation_interval = 1; // How often signals are generated
    
    std::map<std::string, std::string> metadata;

    std::string to_json() const;
    std::string to_csv() const;
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio
```

---


## Signal Parsing - signal_output.cpp

**File:** `src/strategy/signal_output.cpp`
**Lines:** 327

```cpp
#include "strategy/signal_output.h"
#include "common/utils.h"

#include <sstream>
#include <iostream>

#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif

namespace sentio {

std::string SignalOutput::to_json() const {
#ifdef NLOHMANN_JSON_AVAILABLE
    nlohmann::json j;
    j["version"] = "2.0";  // Version field for migration
    if (bar_id != 0) j["bar_id"] = bar_id;  // Numeric
    j["timestamp_ms"] = timestamp_ms;  // Numeric
    j["bar_index"] = bar_index;  // Numeric
    j["symbol"] = symbol;
    j["probability"] = probability;  // Numeric - CRITICAL FIX
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        j["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        j["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        j["signal_type"] = "NEUTRAL";
    }
    
    j["strategy_name"] = strategy_name;
    j["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        j["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        j["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        j["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        j["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        j["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        j["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        j["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        j["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        j["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return j.dump();
#else
    // Fallback to string-based JSON (legacy format v1.0)
    std::map<std::string, std::string> m;
    m["version"] = "1.0";
    if (bar_id != 0) m["bar_id"] = std::to_string(bar_id);
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["probability"] = std::to_string(probability);
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        m["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        m["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        m["signal_type"] = "NEUTRAL";
    }
    
    m["strategy_name"] = strategy_name;
    m["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        m["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        m["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        m["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        m["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        m["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        m["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        m["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        m["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        m["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return utils::to_json(m);
#endif
}

std::string SignalOutput::to_csv() const {
    std::ostringstream os;
    os << timestamp_ms << ','
       << bar_index << ','
       << symbol << ','
       << probability << ',';
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        os << "LONG,";
    } else if (signal_type == SignalType::SHORT) {
        os << "SHORT,";
    } else {
        os << "NEUTRAL,";
    }
    
    os << strategy_name << ','
       << strategy_version;
    return os.str();
}

SignalOutput SignalOutput::from_json(const std::string& json_str) {
    SignalOutput s;
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        
        // Handle both numeric (v2.0) and string (v1.0) formats
        if (j.contains("bar_id")) {
            if (j["bar_id"].is_number()) {
                s.bar_id = j["bar_id"].get<uint64_t>();
            } else if (j["bar_id"].is_string()) {
                s.bar_id = static_cast<uint64_t>(std::stoull(j["bar_id"].get<std::string>()));
            }
        }
        
        if (j.contains("timestamp_ms")) {
            if (j["timestamp_ms"].is_number()) {
                s.timestamp_ms = j["timestamp_ms"].get<int64_t>();
            } else if (j["timestamp_ms"].is_string()) {
                s.timestamp_ms = std::stoll(j["timestamp_ms"].get<std::string>());
            }
        }
        
        if (j.contains("bar_index")) {
            if (j["bar_index"].is_number()) {
                s.bar_index = j["bar_index"].get<int>();
            } else if (j["bar_index"].is_string()) {
                s.bar_index = std::stoi(j["bar_index"].get<std::string>());
            }
        }
        
        if (j.contains("symbol")) s.symbol = j["symbol"].get<std::string>();
        
        // Parse signal_type
        if (j.contains("signal_type")) {
            std::string type_str = j["signal_type"].get<std::string>();
            std::cerr << "DEBUG: Parsing signal_type='" << type_str << "'" << std::endl;
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
                std::cerr << "DEBUG: Set to LONG" << std::endl;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
                std::cerr << "DEBUG: Set to SHORT" << std::endl;
            } else {
                s.signal_type = SignalType::NEUTRAL;
                std::cerr << "DEBUG: Set to NEUTRAL (default)" << std::endl;
            }
        } else {
            std::cerr << "DEBUG: signal_type field NOT FOUND in JSON" << std::endl;
        }
        
        if (j.contains("probability")) {
            if (j["probability"].is_number()) {
                s.probability = j["probability"].get<double>();
            } else if (j["probability"].is_string()) {
                std::string prob_str = j["probability"].get<std::string>();
                if (!prob_str.empty()) {
                    try {
                        s.probability = std::stod(prob_str);
                    } catch (const std::exception& e) {
                        std::cerr << "ERROR: Failed to parse probability '" << prob_str << "': " << e.what() << "\n";
                        std::cerr << "JSON: " << json_str << "\n";
                        throw;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Fallback to string-based parsing
        std::cerr << "WARNING: nlohmann::json parsing failed, falling back to string parsing: " << e.what() << "\n";
        auto m = utils::from_json(json_str);
        if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
        if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
        if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
        if (m.count("symbol")) s.symbol = m["symbol"];
        if (m.count("signal_type")) {
            std::string type_str = m["signal_type"];
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
            } else {
                s.signal_type = SignalType::NEUTRAL;
            }
        }
        if (m.count("probability") && !m["probability"].empty()) {
            try {
                s.probability = std::stod(m["probability"]);
            } catch (const std::exception& e2) {
                std::cerr << "ERROR: Failed to parse probability from string map '" << m["probability"] << "': " << e2.what() << "\n";
                std::cerr << "Original JSON: " << json_str << "\n";
                throw;
            }
        }
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
    if (m.count("symbol")) s.symbol = m["symbol"];
    if (m.count("signal_type")) {
        std::string type_str = m["signal_type"];
        if (type_str == "LONG") {
            s.signal_type = SignalType::LONG;
        } else if (type_str == "SHORT") {
            s.signal_type = SignalType::SHORT;
        } else {
            s.signal_type = SignalType::NEUTRAL;
        }
    }
    if (m.count("probability") && !m["probability"].empty()) {
        s.probability = std::stod(m["probability"]);
    }
#endif
    
    // Parse additional metadata (strategy_name, strategy_version, etc.)
    // Note: signal_type is already parsed above in the main parsing section
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("strategy_name")) s.strategy_name = j["strategy_name"].get<std::string>();
        if (j.contains("strategy_version")) s.strategy_version = j["strategy_version"].get<std::string>();
        if (j.contains("market_data_path")) s.metadata["market_data_path"] = j["market_data_path"].get<std::string>();
        if (j.contains("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = j["minimum_hold_bars"].get<std::string>();
    } catch (...) {
        // Fallback to string map
        auto m = utils::from_json(json_str);
        if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
        if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
        if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
        if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
    if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
    if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
    if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
#endif
    return s;
}

} // namespace sentio



```

---

---

# 3. Test Data & Execution Logs

## Signal Generation Results

### Full Dataset Summary
```
=== OnlineEnsemble Signal Generation ===
Data: data/equities/QQQ_RTH_NH.csv
Output: /tmp/qqq_full_signals.jsonl
Warmup: 960 bars

Loading market data...
Loaded 292385 bars
Processing range: 0 to 292385

Generating signals...
Generated 292385 signals

=== Signal Summary ===
Total signals: 292385
Long signals:  67711 (23.1582%)
Short signals: 77934 (26.6546%)
Neutral:       146740 (50.1873%)

✓ Signals saved successfully!
```

### Test Dataset (2000 bars) - Probability Distribution
```
< 0.30:      25 signals (1.2%)  → SQQQ_ONLY (strong short, -3x)
0.30-0.40:   13 signals (0.7%)  → PSQ_SQQQ (moderate short, blended)
0.40-0.47:   86 signals (4.3%)  → PSQ_ONLY (weak short, -1x)
0.47-0.53: 1772 signals (88.6%) → CASH_ONLY (uncertain)
0.53-0.60:   69 signals (3.5%)  → QQQ_ONLY (weak long, 1x)
0.60-0.70:   20 signals (1.0%)  → QQQ_TQQQ (moderate long, blended)
> 0.70:      15 signals (0.8%)  → TQQQ_ONLY (strong long, 3x)

Total: 2000 signals
Non-cash signals: 228 (11.4%)
```

### Non-Neutral Signal Runs
```
Total non-neutral runs: 178
Longest run: 11 bars
Average run length: 1.3 bars

First 10 runs (showing which executed trades):
  Run 1: bars 136-136 (1 bar)  ← ✓ Trade executed here
  Run 2: bars 140-140 (1 bar)  ← ✗ No trade!
  Run 3: bars 178-178 (1 bar)  ← ✗ No trade!
  Run 4: bars 210-210 (1 bar)  ← ✗ No trade!
  Run 5: bars 271-271 (1 bar)  ← ✗ No trade!
  Run 6: bars 276-276 (1 bar)  ← ✗ No trade!
  Run 7: bars 301-301 (1 bar)  ← ✗ No trade!
  Run 8: bars 304-304 (1 bar)  ← ✗ No trade!
  Run 9: bars 318-318 (1 bar)  ← ✗ No trade!
  Run 10: bars 330-330 (1 bar) ← ✗ No trade!
```

---

## Trade Execution Logs

### Verbose Output (2000 bars)
```
=== OnlineEnsemble Trade Execution ===
Signals: /tmp/test_signals.jsonl
Data: /tmp/test_2k.csv
Output: /tmp/test_trades_v3.jsonl
Starting Capital: $100000.00
Kelly Sizing: Enabled

Loading signals...
Loaded 2000 signals
Loading market data...
Loaded 2000 bars

Executing trades with Position State Machine...
Target: ~50 trades/block, 2% profit-taking, -1.5% stop-loss

[0] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: 0.00%
  [119] PSQ BUY 342.35 @ $292.10 | Portfolio: $100000.00
  [136] PSQ SELL 171.17 @ $292.04 | Portfolio: $99979.80
  [136] SQQQ BUY 171.17 @ $292.04 | Portfolio: $99979.80
  [500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY | PnL: 0.01%
  [1000] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: -0.05%
  [1500] Signal: 0.50 | Current: CASH_ONLY | Target: CASH_ONLY | PnL: 0.03%

Trade execution complete!
Total trades: 3
Final capital: $96504.62
Total return: -3.50%
Max drawdown: 4.83%

Saving trade history to /tmp/test_trades_v3.jsonl...
✓ Trade execution complete!
```

### Trade History (JSONL)
```json
{"bar_id":14360292025081007808,"timestamp_ms":1663256760000,"bar_index":136,"symbol":"PSQ","action":"BUY","quantity":342.4177,"price":292.04,"trade_value":100000.00,"fees":0.00,"cash_balance":0.00,"portfolio_value":100000.00,"position_quantity":342.42,"position_avg_price":292.04,"reason":"PSM: CASH_ONLY -> PSQ_ONLY"}
{"bar_id":14360292025159247808,"timestamp_ms":1663335000000,"bar_index":391,"symbol":"PSQ","action":"SELL","quantity":171.2088,"price":286.95,"trade_value":49128.38,"fees":0.00,"cash_balance":49128.38,"portfolio_value":98256.75,"position_quantity":171.21,"position_avg_price":292.04,"reason":"PSM: PSQ_ONLY -> PSQ_SQQQ"}
{"bar_id":14360292025159247808,"timestamp_ms":1663335000000,"bar_index":391,"symbol":"SQQQ","action":"BUY","quantity":171.2088,"price":286.95,"trade_value":49128.38,"fees":0.00,"cash_balance":0.00,"portfolio_value":98256.75,"position_quantity":171.21,"position_avg_price":286.95,"reason":"PSM: PSQ_ONLY -> PSQ_SQQQ"}
```

---

## Sample Signal Data

### First 10 signals from test dataset
```json
{"bar_id":14360292025072847808,"timestamp_ms":1663248600000,"bar_index":1,"symbol":"UNKNOWN","probability":0.500000,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025072907808,"timestamp_ms":1663248660000,"bar_index":2,"symbol":"UNKNOWN","probability":0.500000,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025072967808,"timestamp_ms":1663248720000,"bar_index":3,"symbol":"UNKNOWN","probability":0.500000,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025073027808,"timestamp_ms":1663248780000,"bar_index":4,"symbol":"UNKNOWN","probability":0.500000,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025073087808,"timestamp_ms":1663248840000,"bar_index":5,"symbol":"UNKNOWN","probability":0.500000,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
```

### Last 10 signals from test dataset (showing diversity)
```json
{"bar_id":14360292025680047808,"timestamp_ms":1663855800000,"bar_index":1996,"symbol":"UNKNOWN","probability":0.426886,"signal_type":"SHORT","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025680107808,"timestamp_ms":1663855860000,"bar_index":1997,"symbol":"UNKNOWN","probability":0.490987,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025680167808,"timestamp_ms":1663855920000,"bar_index":1998,"symbol":"UNKNOWN","probability":0.501581,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025680227808,"timestamp_ms":1663855980000,"bar_index":1999,"symbol":"UNKNOWN","probability":0.570766,"signal_type":"LONG","prediction_horizon":1,"ensemble_agreement":0.000000}
{"bar_id":14360292025680287808,"timestamp_ms":1663856040000,"bar_index":2000,"symbol":"UNKNOWN","probability":0.520554,"signal_type":"NEUTRAL","prediction_horizon":1,"ensemble_agreement":0.000000}
```

---

# 4. Configuration & Build Files

## Risk Management Parameters
**File:** `src/cli/execute_trades_command.cpp:94-99`

```cpp
const double PROFIT_TARGET = 0.02;        // 2% profit → take profit
const double STOP_LOSS = -0.015;          // -1.5% loss → exit to cash  
const double CONFIDENCE_THRESHOLD = 0.55; // Unused (bypassed by direct mapping)
const int MIN_HOLD_BARS = 1;              // Minimum hold (prevent flip-flop)
const int MAX_HOLD_BARS = 100;            // Maximum hold (force re-evaluation)
```

## State Mapping Configuration
**File:** `src/cli/execute_trades_command.cpp:143-156`

```cpp
// Direct state mapping from probability (bypass PSM's internal logic)
// Relaxed thresholds for more frequent trading
PositionStateMachine::State target_state;

if (signal.probability >= 0.65) {
    target_state = PositionStateMachine::State::TQQQ_ONLY;  // Strong long (3x)
} else if (signal.probability >= 0.55) {
    target_state = PositionStateMachine::State::QQQ_TQQQ;   // Moderate long (blended)
} else if (signal.probability >= 0.51) {
    target_state = PositionStateMachine::State::QQQ_ONLY;   // Weak long (1x)
} else if (signal.probability >= 0.49) {
    target_state = PositionStateMachine::State::CASH_ONLY;  // Very uncertain → cash
} else if (signal.probability >= 0.45) {
    target_state = PositionStateMachine::State::PSQ_ONLY;   // Weak short (-1x)
} else if (signal.probability >= 0.35) {
    target_state = PositionStateMachine::State::PSQ_SQQQ;   // Moderate short (blended)
} else {
    target_state = PositionStateMachine::State::SQQQ_ONLY;  // Strong short (-3x)
}
```

---

# 5. Critical Code Analysis

## The Blocking Point

**File:** `src/cli/execute_trades_command.cpp`
**Lines:** 186-187

```cpp
// Execute state transition
if (transition.target_state != transition.current_state) {
    // Calculate positions for target state
    std::map<std::string, double> target_positions = ...
```

**Issue:** Despite verbose logs showing:
```
[500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY
```

The condition `transition.target_state != transition.current_state` appears to be failing, OR a downstream condition is blocking execution.

## Missing State Update

**Current code** (after trade execution loop, ~line 260):
```cpp
// No update to current_position.state!
// Loop continues to next bar
```

**Expected code:**
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

---

# 6. Debug Instrumentation Needed

Add the following debug statements to identify the exact blocking point:

## Before State Transition Check (Line ~186)
```cpp
std::cerr << "DEBUG [" << i << "]: Transition check\n"
          << "  current_state=" << static_cast<int>(transition.current_state) 
          << " (" << psm.state_to_string(transition.current_state) << ")\n"
          << "  target_state=" << static_cast<int>(transition.target_state)
          << " (" << psm.state_to_string(transition.target_state) << ")\n"
          << "  states_different=" << (transition.target_state != transition.current_state) << "\n"
          << "  cash=" << portfolio.cash_balance
          << "  positions=" << portfolio.positions.size() << "\n";
```

## Inside Trade Execution Loop (Line ~198)
```cpp
std::cerr << "DEBUG: Attempting trade: " << (action == TradeAction::BUY ? "BUY" : "SELL")
          << " " << symbol << " qty=" << quantity
          << " price=" << bar.close
          << " value=" << trade_value
          << " cash=" << portfolio.cash_balance
          << " can_execute=" << (trade_value <= portfolio.cash_balance) << "\n";
```

## After Trade Execution (Line ~260)
```cpp
std::cerr << "DEBUG: Trades completed for bar " << i
          << " | New cash=" << portfolio.cash_balance
          << " | Position tracking state=" << psm.state_to_string(current_position.state)
          << " | Should be=" << psm.state_to_string(transition.target_state) << "\n";
```

---

**End of Mega Document**

Generated: 2025-10-06
Total sections: 6
Total source files: 7
Purpose: Debugging PSM trade execution blocking issue
