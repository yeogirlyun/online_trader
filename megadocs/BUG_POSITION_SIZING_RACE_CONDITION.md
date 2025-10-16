# CRITICAL BUG: Position Sizing Race Condition - Capital Over-Allocation

**Date**: 2025-10-15
**Severity**: P0 - Critical Blocker
**Status**: Root Cause Identified
**Previous Report**: BUG_WARMUP_DEPLETES_CAPITAL.md (diagnosis was incorrect)

---

## Executive Summary

The system over-allocates capital by opening multiple positions before cash is deducted from previous trades. In the first 5 minutes of trading, 4 positions are opened totaling $99,887, depleting 99.9% of the $100,000 capital despite `max_positions=3`.

**Root Cause**: Race condition between position decision-making and cash deduction. The `calculate_position_size()` function uses `current_cash_` which hasn't been updated yet for recently-opened positions.

---

## Evidence from Oct 1st Test

### First 4 Trades (Timestamp Order)
```
1. 09:30:00 - TQQQ:  311 shares × $101.63 = $31,607
2. 09:31:00 - UVXY: 3073 shares × $10.57  = $32,482
3. 09:32:00 - SPXL:  162 shares × $209.38 = $33,920
4. 09:35:00 - SDS:   129 shares × $14.56  = $1,878
                                   Total: $99,887
```

### Subsequent Trades (5-84)
- Trade 5-84: All have 0 shares (insufficient cash)
- Final equity: $60.57 (0.06% of starting capital)
- 0 positions closed (no capital to continue trading)

---

## Root Cause Analysis

### The Position Sizing Logic

**File**: `src/backend/rotation_trading_backend.cpp:475-535`

```cpp
int RotationTradingBackend::calculate_position_size(...) {
    // Get CURRENT available cash, not starting capital
    double available_cash = current_cash_;  // ← PROBLEM: Uses stale value

    double usable_cash = available_cash * 0.95;

    // Calculate allocation based on current positions
    int active_positions = rotation_manager_->get_position_count();
    int max_positions = config_.rotation_config.max_positions;
    int remaining_slots = std::max(1, max_positions - active_positions);

    // Divide available cash by remaining slots
    double capital_per_position = usable_cash / remaining_slots;

    int shares = static_cast<int>(capital_per_position / price);
    return shares;
}
```

### The Cash Deduction Logic

**File**: `src/backend/rotation_trading_backend.cpp:424-426`

```cpp
if (decision.decision == ENTER_LONG || decision.decision == ENTER_SHORT) {
    current_cash_ -= shares * execution_price;  // ← Cash deducted AFTER sizing
    session_stats_.positions_opened++;
}
```

### The Race Condition

**Timeline of Events**:

1. **09:30:00** - Trade 1 (TQQQ):
   - `current_cash_` = $100,000
   - `active_positions` = 0
   - `remaining_slots` = 3 - 0 = 3
   - `capital_per_position` = $95,000 / 3 = $31,667
   - Shares = 311 (spent $31,607)
   - Cash deducted: `current_cash_` = $68,393

2. **09:31:00** - Trade 2 (UVXY) - **1 minute later!**:
   - `current_cash_` = $68,393 (correctly updated)
   - `active_positions` = 1
   - `remaining_slots` = 3 - 1 = 2
   - `capital_per_position` = $64,973 / 2 = **$32,487**
   - Shares = 3,073 (spent $32,482)
   - Cash deducted: `current_cash_` = $35,911

3. **09:32:00** - Trade 3 (SPXL) - **2 minutes later!**:
   - `current_cash_` = $35,911
   - `active_positions` = 2
   - `remaining_slots` = 3 - 2 = 1
   - `capital_per_position` = $34,115 / 1 = **$34,115** (ALL remaining cash!)
   - Shares = 162 (spent $33,920)
   - Cash deducted: `current_cash_` = $1,991

4. **09:35:00** - Trade 4 (SDS):
   - `current_cash_` = $1,991
   - Opens 4th position despite `max_positions = 3`!
   - Spent $1,878
   - Cash remaining: $113

The problem is that positions 2 and 3 allocated TOO MUCH capital because:
- Position 2 allocated $32k (should be ~$33k based on 3 max positions)
- Position 3 allocated ALL remaining cash (~$34k)
- Together they spent $66k when only $67k was left after position 1

The issue is that `remaining_slots` keeps shrinking as positions open, causing the allocation per position to INCREASE rather than decrease!

---

## Why This Happens

### Flawed Allocation Formula

The formula `capital_per_position = usable_cash / remaining_slots` is correct for INITIAL sizing, but becomes problematic as positions open:

- **Start**: 3 slots, $95k → $31.67k per slot ✓
- **After 1 position**: 2 slots, $64k → **$32k per slot** (increased!)
- **After 2 positions**: 1 slot, $34k → **$34k per slot** (ALL remaining cash!)

This creates an "acceleration" effect where later positions get larger allocations!

### Intended Behavior

With `max_positions = 3` and $100k capital:
- Each position should get ~$33k
- After opening 3 positions, should have $0-1k left
- Should NOT open a 4th position

### Actual Behavior

- Position 1: $31.6k ✓
- Position 2: $32.5k ✓
- Position 3: $33.9k ✗ (should stop here!)
- Position 4: $1.9k ✗ (4 > max_positions!)

---

## Impact

1. **Capital Depletion**: 99.9% of capital spent in 5 minutes
2. **No Closing Positions**: Can't close positions that were never properly entered
3. **Strategy Cannot Trade**: Remaining 385 minutes have insufficient capital
4. **False Performance Metrics**: -99.94% loss is not representative of strategy

---

## Fix Required

### Option 1: Pre-deduct Cash (Recommended)

Deduct cash from `current_cash_` BEFORE executing the trade:

```cpp
// In execute_decision(), BEFORE calling rotation_manager_->execute_decision()
if (decision.decision == ENTER_LONG || decision.decision == ENTER_SHORT) {
    current_cash_ -= shares * execution_price;  // Deduct FIRST
}

bool success = rotation_manager_->execute_decision(decision, execution_price);

if (!success) {
    // Rollback on failure
    current_cash_ += shares * execution_price;
}
```

### Option 2: Fix Allocation Formula

Use FIXED capital allocation that doesn't change:

```cpp
double capital_per_position = (config_.starting_capital * 0.95) / max_positions;
// Always allocate $31.67k per position regardless of how many are open
```

### Option 3: Track Pending Capital

Add `pending_cash_` field to track allocated but not yet deducted capital:

```cpp
double available_cash = current_cash_ - pending_cash_;
```

---

## Recommended Solution

Implement **Option 1** (pre-deduct cash) because:
- Simplest implementation
- Prevents over-allocation by design
- Matches actual capital flow
- Easy to rollback on failure

---

## Testing Plan

1. Apply Option 1 fix
2. Run Oct 1st test
3. Verify:
   - Only 3 positions open (respects max_positions)
   - Each position ~$33k (equal allocation)
   - Capital preserved throughout day
   - Positions can close normally

---

## Related Files

- `src/backend/rotation_trading_backend.cpp:475-535` - calculate_position_size()
- `src/backend/rotation_trading_backend.cpp:391-448` - execute_decision()
- `include/backend/rotation_trading_backend.h:327-342` - State variables
