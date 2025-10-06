# OnlineTrader Mega Documents

This directory contains comprehensive bug reports and source code compilations for debugging and documentation purposes.

## Files

### 1. PSM_TRADE_EXECUTION_BUG.md
**Concise bug report** documenting the critical issue where only 3 trades are executed instead of the expected 200+ trades per 2000 bars.

**Key findings:**
- Signal generation: ✓ WORKING (23% LONG, 27% SHORT, 50% NEUTRAL)
- State mapping: ✓ WORKING (direct probability thresholds)
- Trade execution: ✗ BROKEN (transitions calculated but not executed)

**Most likely cause:** `current_position.state` not updated after trade execution

### 2. PSM_TRADE_EXECUTION_MEGADOC.md
**Comprehensive mega document** containing:
- Full bug report
- All 7 related source files (execute_trades_command.cpp, generate_signals_command.cpp, etc.)
- Test data and execution logs
- Debug instrumentation recommendations
- Proposed fixes

**Size:** ~120KB
**Sections:** 6
**Source files:** 7

## Quick Reference

### Problem Statement
Only **3 trades** executed on 2000-bar dataset despite:
- 228 non-cash signals (11.4% of bars)
- Direct state mapping with relaxed thresholds (0.49-0.51 for CASH)
- Target: 50 trades per 480-bar block (~240 trades for 2000 bars)

### Evidence
Verbose output at bar 500:
```
[500] Signal: 0.51 | Current: CASH_ONLY | Target: QQQ_ONLY | PnL: 0.01%
```
- Target state calculated correctly (QQQ_ONLY)
- But NO TRADE executed
- This proves blocking happens in execution logic

### Proposed Fix
Add after line 260 in `execute_trades_command.cpp`:
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

## Related Files

### Core Execution
- `/src/cli/execute_trades_command.cpp` - Main bug location
- `/include/cli/ensemble_workflow_command.h` - Command interface

### Signal Generation (Working)
- `/src/cli/generate_signals_command.cpp` - EWRLS signal generation
- `/src/strategy/online_ensemble_strategy.cpp` - Multi-horizon predictor

### Data Structures
- `/include/strategy/signal_output.h` - Signal definitions
- `/src/strategy/signal_output.cpp` - JSON parsing
- `/include/backend/position_state_machine.h` - PSM states

## Timeline

- **2025-10-06 20:00:** EWRLS signal generation fixed
- **2025-10-06 20:15:** Direct state mapping implemented
- **2025-10-06 20:30:** Thresholds relaxed
- **2025-10-06 20:45:** Bug identified - trade execution blocked
- **2025-10-06 21:00:** Bug report & mega document created

## Success Criteria

Fix is successful when:
1. **Primary:** > 200 trades on 2000-bar dataset (currently: 3)
2. **Secondary:** > 80% utilization of non-cash signals (currently: 1.3%)
3. **Tertiary:** Profit-taking and stop-loss triggers activate
4. **Validation:** Full dataset produces 25K-35K trades (50±10 per block)

---

**Generated:** 2025-10-06
**Last Updated:** 2025-10-06 21:05
