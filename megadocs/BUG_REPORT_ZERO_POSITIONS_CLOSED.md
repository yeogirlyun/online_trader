# BUG REPORT: Zero Positions Closed Despite 84 Positions Opened

**Date:** 2025-10-15
**Severity:** CRITICAL
**Status:** OPEN
**Reporter:** System Analysis

---

## Executive Summary

A critical bug prevents any positions from being closed during mock trading sessions. Despite opening 84 positions during a single trading day, the system reports **0 positions closed**, resulting in:
- 99.94% capital loss
- 0% MRD (no completed trades)
- Complete strategy failure

This bug affects the core position management system and renders the rotation trading strategy non-functional.

---

## Symptoms

### Test Configuration
- **Test Date:** 2025-10-01 (single day)
- **Warmup:** 4 days prior (Sept 25-30)
- **Instruments:** 6 symbols (TQQQ, SQQQ, SPXL, SDS, UVXY, SVXY)
- **Starting Capital:** $100,000

### Observed Behavior
```
========================================
Session Summary
========================================
Bars processed: 391
Signals generated: 2345
Trades executed: 84
Positions opened: 84
Positions closed: 0        ← CRITICAL BUG
Rotations: 0

Total P&L: $-99939.426000 (-99.939426%)
Final equity: $60.574000
Max drawdown: 99.939691%

Win rate: 0.000000%
Avg win: 0.000000%
Avg loss: 0.000000%
Sharpe ratio: -33.068096
MRD: 0.000000%
========================================
```

### Key Evidence
1. **84 positions opened** throughout the day
2. **0 positions closed** - no exits executed
3. **EOD liquidation triggered correctly** (logs confirm)
4. **liquidate_all_positions() found 0 positions** when called at EOD
5. **Win rate: 0%** - no completed round-trip trades

---

## Expected Behavior

### Normal Operation
1. Positions should be opened when signals meet entry criteria
2. Positions should be closed when:
   - **Profit target** reached (+3%)
   - **Stop loss** triggered (-1.5%)
   - **Rank falls below threshold** (rotation out)
   - **EOD liquidation** at 3:58 PM ET (388 minutes from open)
3. Each position should have a corresponding exit
4. Session should show: `Positions opened: N, Positions closed: N`

### Expected for Oct 1st Test
- **~80-100 positions** opened (reasonable for 6 instruments × 391 bars)
- **~80-100 positions** closed (same count as opened)
- **Some profit/loss realized** from completed trades
- **Non-zero MRD** reflecting actual trading performance

---

## Actual Behavior

### Position Lifecycle Breakdown

**Position Entry (WORKING):**
- 84 positions successfully opened
- Entry logic executing correctly
- Capital deployed: ~$99,940

**Position Exit (BROKEN):**
- 0 exits via profit target
- 0 exits via stop loss
- 0 exits via rotation
- 0 exits via EOD liquidation

**EOD Liquidation (PARTIALLY BROKEN):**
```
[EOD triggered at 3:58 PM]
liquidate_all_positions() called
get_positions() returned: 0 positions  ← BUG: Should return 84
No positions to close
```

---

## Root Cause Analysis

### Hypothesis 1: Position Tracking Failure
**Most Likely:** Positions are being opened but not tracked in the position manager.

**Evidence:**
- `execute_decision(ENTER_LONG/SHORT)` creates positions
- `get_positions()` returns empty map at EOD
- Session stats show 84 opens but 0 closes

**Probable cause:**
- Position manager's internal `positions_` map not being updated on entry
- OR positions being removed immediately after creation
- OR positions stored with wrong key/structure

### Hypothesis 2: Exit Logic Never Triggered
**Less Likely:** Positions are tracked but exit conditions never met.

**Evidence:**
- Profit target: +3% (should trigger on volatile leveraged ETFs)
- Stop loss: -1.5% (should trigger frequently)
- 391 bars × 6 instruments = 2,346 opportunities for exit
- **0 exits** suggests systematic failure, not bad luck

### Hypothesis 3: Execute Decision for EXIT Broken
**Possible:** EXIT decisions aren't actually closing positions.

**Evidence:**
- EOD liquidation creates EXIT decisions
- `liquidate_all_positions()` finds 0 positions to close
- Suggests positions already gone OR never stored

---

## Technical Details

### EOD Liquidation Flow

**Expected Flow:**
```cpp
1. is_eod(388) returns true
2. liquidate_all_positions() called
3. get_positions() returns map<symbol, Position>
4. For each position:
   - Create EXIT decision
   - execute_decision(EXIT)
   - Position removed from tracking
   - Trade logged
5. Result: All positions closed
```

**Actual Flow:**
```cpp
1. is_eod(388) returns true ✓
2. liquidate_all_positions() called ✓
3. get_positions() returns EMPTY map ✗ BUG
4. No iterations (0 positions)
5. Result: No positions closed
```

### Position Manager State

**Critical Question:** Why does `get_positions()` return empty?

**Possible Reasons:**
1. Positions never added to `positions_` map on ENTER
2. Positions removed immediately after creation
3. `execute_decision(ENTER)` not calling position manager correctly
4. Position manager's `add_position()` or equivalent failing silently
5. Wrong data structure or key being used

---

## Evidence & Logs

### 1. Session Summary (Oct 1, 2025)
```
Positions opened: 84
Positions closed: 0
Total P&L: -$99,939.43
MRD: 0.000000%
```

### 2. EOD Liquidation Logs
```
[INSIDE IF] EOD reached! Liquidating...
[liquidate_all_positions] Found 0 positions to close
[liquidate_all_positions] Liquidation complete
[AFTER LIQUIDATE] Done liquidating
```

### 3. Test Parameters
```
Test date: 2025-10-01
Warmup: 4 days (Sept 25-30)
Bars processed: 391 (1 day)
Signals: 2,345 (6 symbols × 391 bars)
```

---

## Impact Assessment

### Severity: CRITICAL

**Impact on Strategy:**
- **Complete strategy failure** - no trading results
- **100% capital loss** (unrealized)
- **Cannot measure MRD** - primary performance metric
- **Cannot evaluate strategy** - no completed trades

**Impact on Testing:**
- **October 10-day test blocked** - cannot proceed
- **Performance analysis impossible** - no data
- **Optimization blocked** - no feedback signal
- **Production deployment blocked** - system non-functional

**Financial Impact:**
- If deployed to live: **Immediate 100% loss**
- All capital locked in unclosable positions
- Margin calls, liquidation risk

---

## Reproduction Steps

1. Build system: `cmake --build build`
2. Run single-day test:
   ```bash
   build/sentio_cli mock --mode mock --date 2025-10-01
   ```
3. Observe session summary: `Positions closed: 0`
4. Check logs: EOD liquidation finds 0 positions
5. Result: 84 positions opened, 0 closed

**Reproducible:** Yes - 100% reproduction rate

---

## Investigation Plan

### Phase 1: Verify Position Tracking (HIGH PRIORITY)
1. Add debug logging to `RotationPositionManager::execute_decision(ENTER_LONG/SHORT)`
2. Check if positions are added to `positions_` map
3. Log `positions_.size()` after each entry
4. Verify position storage structure

### Phase 2: Verify Position Retrieval
1. Add debug logging to `get_positions()`
2. Log `positions_.size()` when called
3. Check if positions exist but aren't returned
4. Verify map iteration logic

### Phase 3: Verify Exit Logic
1. Check profit target/stop loss conditions
2. Verify `should_exit()` logic
3. Test `execute_decision(EXIT)` independently
4. Check position removal logic

### Phase 4: End-to-End Test
1. Create unit test for position lifecycle
2. Test: ENTER → Track → UPDATE → EXIT
3. Verify position appears in `get_positions()`
4. Verify position removed after EXIT

---

## Affected Workflows

### Blocked Features
- [x] Single-day testing
- [x] Multi-day testing
- [x] Performance measurement (MRD)
- [x] Strategy evaluation
- [x] Parameter optimization
- [x] Live trading deployment

### Workarounds
**None available** - This is a core system bug with no workaround.

---

## Related Issues

### Potentially Related Bugs
1. **EOD liquidation** - Working correctly (confirmed)
2. **Time calculation** - Fixed (3:58 PM = 388 minutes)
3. **Date filtering** - Working correctly (confirmed)

### NOT Related
- Signal generation (2,345 signals generated correctly)
- Feature engine (warmup successful)
- Predictor learning (not tested yet due to this bug)

---

## Next Steps

### Immediate Actions
1. **Debug position manager** - Add extensive logging
2. **Verify enter/exit flow** - Trace through code
3. **Check position storage** - Inspect data structures
4. **Test position lifecycle** - Create unit test

### Before Proceeding
**DO NOT:**
- Run October 10-day tests (same bug will occur)
- Deploy to live trading (critical data loss risk)
- Optimize parameters (no feedback signal)

**MUST FIX:**
- Position tracking on entry
- Position retrieval for exit
- Complete enter → exit lifecycle

---

## Source Modules Reference

See **Source Modules Involved** section below for complete list of files.

---

## Priority & Assignment

**Priority:** P0 - Critical Blocker
**Assignee:** Development Team
**Target Fix Date:** ASAP
**Blocking:** All strategy testing and deployment

---

# Source Modules Involved

This section lists all source code files involved in the position lifecycle and this bug.

---

## Core Position Management

### 1. `include/strategy/rotation_position_manager.h`
**Role:** Position manager interface and data structures

**Key Components:**
- `struct Position` - Position state tracking
- `struct PositionDecision` - Entry/exit decisions
- `enum Decision` - ENTER_LONG, ENTER_SHORT, EXIT, EOD_EXIT, etc.
- `make_decisions()` - Core decision logic
- `execute_decision()` - Execute entry/exit
- `get_positions()` - **CRITICAL:** Returns current positions map
- `update_position()` - Update position state
- `check_exit_conditions()` - Profit target, stop loss checks

**Bug Relevance:** Position tracking data structure, get_positions() implementation

**Lines of Interest:**
- Line 72-86: `struct Position` definition
- Line 91-100: `enum Decision`
- Line 102-109: `struct PositionDecision`

---

### 2. `src/strategy/rotation_position_manager.cpp`
**Role:** Position manager implementation

**Key Functions:**
- `make_decisions()` - Analyzes signals, creates decisions
- `execute_decision()` - **CRITICAL:** Adds/removes positions
- `get_positions()` - **CRITICAL:** Returns `positions_` map
- `update_position()` - Updates position P&L, bars held
- `check_exit_conditions()` - Checks if position should exit
- `should_exit()` - Evaluates exit triggers
- `add_position()` / `remove_position()` - **CRITICAL:** Position lifecycle

**Bug Relevance:** Position map manipulation, entry/exit implementation

**Critical Sections:**
- Position entry logic (ENTER_LONG/SHORT handling)
- Position exit logic (EXIT, EOD_EXIT handling)
- `positions_` map updates
- `get_positions()` return value

---

## Backend Orchestration

### 3. `include/backend/rotation_trading_backend.h`
**Role:** Trading backend interface

**Key Components:**
- `Config` - System configuration
- `on_bar()` - Main bar processing loop
- `liquidate_all_positions()` - EOD liquidation trigger
- `execute_decision()` - Decision execution wrapper
- `update_session_stats()` - Track opens/closes

**Bug Relevance:** Orchestrates position manager calls

**Lines of Interest:**
- Line declarations for position tracking methods
- Session statistics structure

---

### 4. `src/backend/rotation_trading_backend.cpp`
**Role:** Trading backend implementation

**Key Functions:**
- `on_bar()` - **CRITICAL:** Main processing loop
  - Line 251-257: EOD check and liquidation trigger
- `liquidate_all_positions()` - **CRITICAL:** Line 293-307
  - Calls `rotation_manager_->get_positions()`
  - Creates EOD_EXIT decisions
  - Calls `execute_decision()`
- `execute_decision()` - Wraps position manager execution
- `update_session_stats()` - Counts positions opened/closed

**Bug Relevance:** EOD liquidation shows get_positions() returns empty

**Critical Code Sections:**
```cpp
// Line 293-307: Liquidation logic
bool RotationTradingBackend::liquidate_all_positions(const std::string& reason) {
    auto positions = rotation_manager_->get_positions();  // Returns EMPTY

    for (const auto& [symbol, position] : positions) {    // Never iterates
        RotationPositionManager::PositionDecision decision;
        decision.decision = RotationPositionManager::Decision::EOD_EXIT;
        execute_decision(decision);                        // Never called
    }
    return true;
}
```

---

## CLI & Test Execution

### 5. `src/cli/rotation_trade_command.cpp`
**Role:** Command-line interface for mock/live trading

**Key Functions:**
- `execute_mock_trading()` - Runs mock test
- `load_warmup_data()` - Loads historical data
- `print_summary()` - Displays session results

**Bug Relevance:** Test harness that revealed the bug

**Lines of Interest:**
- Line 345-430: Mock trading execution loop
- Session summary printing (shows 0 closes)

---

### 6. `include/cli/rotation_trade_command.h`
**Role:** CLI command interface

**Key Components:**
- `Options` struct - Test configuration
- `execute_mock_trading()` declaration

**Bug Relevance:** Defines test execution flow

---

## Data Management

### 7. `src/data/mock_multi_symbol_feed.cpp`
**Role:** Replays CSV data for testing

**Key Functions:**
- `load_csv()` - Loads and filters bars by date (line 309-398)
- `replay_loop()` - Feeds bars to backend
- `replay_next_bar()` - Processes single bar

**Bug Relevance:** Provides data to trigger position entries

**Date Filtering:** Line 361-388 (working correctly)

---

### 8. `include/data/mock_multi_symbol_feed.h`
**Role:** Mock feed interface

**Key Components:**
- `Config` struct - Feed configuration
- `filter_date` field - Single-day filtering (line 42)

**Bug Relevance:** Test data pipeline (working)

---

## Common Types & Utilities

### 9. `include/common/types.h`
**Role:** Core data structures

**Key Types:**
- `struct Bar` - Price bar (line 42-57)
- `struct Position` - Position representation (line 64-71)
- `enum SignalType` - LONG, SHORT, NEUTRAL

**Bug Relevance:** Position data structure definition

---

### 10. `src/common/utils.cpp`
**Role:** Logging and utilities

**Key Functions:**
- `log_info()`, `log_error()` - Logging
- Session tracking utilities

**Bug Relevance:** Used for debugging, not involved in bug

---

## Configuration

### 11. `config/rotation_strategy.json`
**Role:** Strategy parameters

**Key Settings:**
- `max_positions: 3` - Maximum concurrent positions
- `profit_target_pct: 0.03` - 3% profit target
- `stop_loss_pct: 0.015` - 1.5% stop loss
- `eod_liquidation: true` - Enable EOD exits
- `eod_exit_time_minutes: 388` - 3:58 PM ET

**Bug Relevance:** Exit conditions that should trigger but don't

---

## Signal Generation

### 12. `src/strategy/online_ensemble_strategy.cpp`
**Role:** Generates trading signals

**Key Functions:**
- `generate_signal()` - ML-based signal generation
- `on_bar()` - Process new bar

**Bug Relevance:** Signals working correctly (2,345 generated)

---

### 13. `include/strategy/online_ensemble_strategy.h`
**Role:** Signal generator interface

**Bug Relevance:** Not involved in position tracking bug

---

## Summary of Critical Modules

### Primary Suspects (Most Likely Bug Location):
1. **`src/strategy/rotation_position_manager.cpp`** - Position map manipulation
2. **`include/strategy/rotation_position_manager.h`** - Position data structures
3. **`src/backend/rotation_trading_backend.cpp`** - Position orchestration

### Secondary Modules (Supporting Evidence):
4. `src/cli/rotation_trade_command.cpp` - Test execution
5. `src/data/mock_multi_symbol_feed.cpp` - Data replay

### Working Correctly (Not Bug Sources):
- Time calculation (fixed)
- EOD detection (working)
- Date filtering (working)
- Signal generation (working)
- Data loading (working)

---

## Investigation Focus

### Critical Code Paths to Debug:

1. **Position Entry:**
   - `RotationPositionManager::execute_decision(ENTER_LONG)`
   - Where is position added to `positions_` map?
   - Is `positions_[symbol] = position` called?

2. **Position Storage:**
   - `RotationPositionManager::positions_` member variable
   - Is it being populated?
   - What is `positions_.size()` after entries?

3. **Position Retrieval:**
   - `RotationPositionManager::get_positions()`
   - Does it return reference to `positions_`?
   - Or does it construct new map?
   - Is map being cleared somewhere?

4. **Position Exit:**
   - `RotationPositionManager::execute_decision(EXIT)`
   - Is it removing from `positions_` map?
   - When is removal happening?

---

## Recommended Debug Instrumentation

Add logging to these exact locations:

```cpp
// In rotation_position_manager.cpp

void RotationPositionManager::execute_decision(Decision type, ...) {
    if (type == ENTER_LONG || type == ENTER_SHORT) {
        // ADD LOG HERE
        std::fprintf(stderr, "[POS ENTER] Symbol=%s, positions_.size()=%zu BEFORE\n",
                    symbol.c_str(), positions_.size());

        // ... position creation logic ...

        // ADD LOG HERE
        std::fprintf(stderr, "[POS ENTER] Symbol=%s, positions_.size()=%zu AFTER\n",
                    symbol.c_str(), positions_.size());
    }
}

std::map<std::string, Position> RotationPositionManager::get_positions() {
    // ADD LOG HERE
    std::fprintf(stderr, "[GET POSITIONS] Returning %zu positions\n", positions_.size());
    return positions_;  // Or positions_copy? Check implementation
}
```

This will reveal exactly where positions disappear.

---

## Conclusion

This is a **critical, blocking bug** in the core position management system. The position lifecycle is broken between entry and exit, with positions either:
- Not being stored after creation, OR
- Being immediately removed, OR
- Not being retrieved correctly for exit

**All testing and deployment is blocked** until this is resolved.

**Estimated Impact:** Complete strategy failure, 100% capital loss risk.

**Priority:** P0 - Drop everything and fix.

---

**End of Bug Report**
