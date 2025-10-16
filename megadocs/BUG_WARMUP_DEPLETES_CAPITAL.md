# BUG REPORT: Warmup Phase Depletes All Trading Capital

**Date**: 2025-10-15
**Severity**: P0 - Critical Blocker
**Status**: Root Cause Identified, Fix In Progress

---

## Executive Summary

The warmup phase is **executing real trades** and **depleting all $100,000 capital** before the actual test date begins. By the time the test starts, only $60-$98 remains, causing all subsequent position entries to return 0 shares due to insufficient capital.

**Result**: 84 positions opened with 0 shares (83/84), 0 positions closed, -99.94% loss.

---

## Root Cause

During warmup (lines 400-448 in `rotation_trading_backend.cpp`), the backend:

1. Calls `execute_trade_decision()` for EVERY signal generated
2. This calculates position size using `calculate_position_size()`
3. Executes the trade with `rotation_manager_->execute_decision()`
4. **DEDUCTS CASH** from `current_cash_` (line 426)

**Warmup should only train models, NOT execute trades!**

---

## Evidence from logs/app.log

```
# Warmup phase (12:23:21-12:23:23)
2025-10-15T12:23:21Z INFO - Position sizing: cash=$100000.000000, shares=153  # Trade 1
2025-10-15T12:23:21Z INFO - Position sizing: cash=$68509.524700, shares=105   # Trade 2
2025-10-15T12:23:22Z INFO - Position sizing: cash=$46834.374700, shares=71    # Trade 3
...
2025-10-15T12:23:22Z INFO - Position sizing: cash=$142.687000, shares=3       # Trade 19
2025-10-15T12:23:22Z INFO - Position sizing: cash=$98.047000, shares=0        # Trade 20
2025-10-15T12:23:22Z WARNING - Insufficient cash for position: $98.047000     # Hundreds of warnings

# Test phase (15:37:46) - No capital left!
2025-10-15T15:37:46Z WARNING - Insufficient cash for position: $60.574000
```

**Timeline**:
- 12:23:21: Started with $100,000
- 12:23:23: Down to $98 (2 seconds later!)
- 15:37:46: Down to $60 (test phase)

---

## Impact

1. **All positions have 0 shares**: 83 out of 84 positions created with shares=0
2. **No positions can close**: Can't close positions that were never entered
3. **-99.94% loss**: System loses entire capital during warmup
4. **Strategy cannot be tested**: No valid trading occurs on test date

---

## Fix Required

**Option 1: Skip trade execution during warmup** (Recommended)
- Add `is_warmup_` flag to backend
- In `execute_trade_decision()`, return early if `is_warmup_` is true
- Only execute trades after warmup completes

**Option 2: Reset capital after warmup**
- Store initial capital
- Reset `current_cash_` to initial value after warmup
- Also reset `rotation_manager_` positions

**Option 3: Separate warmup and trading flows**
- Use different code paths for warmup vs trading
- Warmup only calls `oes_->on_bar()` and `update_learning()`
- Trading calls full execution flow

---

## Source Modules Involved

### Primary Suspects
1. **src/backend/rotation_trading_backend.cpp** (Lines 400-448)
   - `execute_trade_decision()` - Executes trades during warmup
   - Line 426: `current_cash_ -= shares * execution_price;` - Depletes capital
   - Line 408: `calculate_position_size()` - Allocates from depleted cash

2. **src/cli/rotation_trade_command.cpp** (Lines 276-316)
   - Warmup loop that feeds data to backend
   - Should set a warmup flag before starting

### Related Modules
3. **include/backend/rotation_trading_backend.h**
   - Needs `is_warmup_` flag added
   - Needs `start_trading()` method to end warmup

---

## Implementation Plan

1. Add `bool is_warmup_ = true;` to RotationTradingBackend
2. Add `void start_trading()` method to set `is_warmup_ = false`
3. Modify `execute_trade_decision()` to skip trades if `is_warmup_`
4. Call `backend->start_trading()` after warmup completes in CLI
5. Test with Oct 1st data to verify capital is preserved

---

## Expected Outcome After Fix

- Warmup: Capital remains at $100,000
- Test phase: Positions opened with valid share counts
- EOD: Positions close normally
- P&L: Reflects actual trading performance (not -99.94% loss)
