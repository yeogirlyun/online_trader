# EOD Liquidation Bug - Fix Checklist

**Bug ID**: EOD-2025-10-08
**Severity**: CRITICAL (Safety Issue)
**Status**: ❌ OPEN

---

## Quick Summary

**Problem**: System fails to liquidate positions at 3:58 PM ET before market close.

**Evidence**: Account closed 10/8/2025 with 2,732 SH shares ($100K exposure).

**Impact**: Overnight position risk, violates safety requirement.

---

## Fix Steps

### 1. Code Review (15 min)

Check if EOD closing logic exists:

```bash
# Search for EOD closing implementation
grep -rn "is_eod_close_time\|15:58\|eod.*close\|eod.*liquidat" src/cli/

# Check time utils for EOD method
grep -n "eod\|15:58" include/common/time_utils.h
```

**Expected**: Method to detect 3:58 PM ET and trigger liquidation
**If Missing**: Add implementation (see below)

### 2. Implement EOD Closing (30 min)

**File**: `src/cli/live_trade_command.cpp`

**Location**: In `process_bar()` method, BEFORE regular trading logic

**Pseudo-code**:
```cpp
void process_bar(const Bar& bar) {
    // ... existing bar validation ...

    // *** ADD THIS SECTION ***
    // Step X: EOD Position Closing (3:58 PM ET)
    if (et_time_.is_eod_close_time() && !eod_done_today_) {
        log_system("🔔 EOD CLOSING TIME (3:58 PM ET)");
        log_system("Liquidating ALL positions before market close...");

        // Force liquidate (ignore min hold period)
        liquidate_all_positions();

        // Update state
        current_state_ = PositionStateMachine::State::CASH_ONLY;
        bars_held_ = 0;
        eod_done_today_ = true;

        // Persist EOD completion
        eod_state_.mark_complete(get_current_date());

        log_system("✓ EOD closing complete - 100% CASH");
        return;  // Skip normal trading logic
    }
    // *** END NEW SECTION ***

    // ... existing trading logic ...
}
```

**File**: `include/common/time_utils.h`

**Add method** (if doesn't exist):
```cpp
bool is_eod_close_time() const {
    auto et_tm = get_current_et_tm();
    return (et_tm.tm_hour == 15 && et_tm.tm_min == 58);
}
```

### 3. Add Daily Reset (10 min)

Reset `eod_done_today_` flag at market open:

```cpp
// In process_bar() - detect new trading day
if (is_new_trading_day()) {
    eod_done_today_ = false;
    log_system("New trading day - EOD flag reset");
}
```

### 4. Test Implementation (30 min)

#### Unit Test
```bash
# Create test to verify 3:58 PM detection
# Test with different times: 15:57, 15:58, 15:59
# Verify EOD logic triggers only at 15:58
```

#### Integration Test
```bash
# Test with historical data
./build/sentio_cli backtest \
  --data data/equities/SPY_4blocks.csv \
  --warmup-blocks 2 \
  --test-blocks 1

# Verify final bar shows CASH_ONLY state
```

#### Live Paper Test
```bash
# Start system at 3:55 PM ET
# Monitor logs for EOD closing at 3:58 PM
# Verify position liquidation occurs
# Confirm account is 100% cash at 4:00 PM
```

### 5. Verification Checklist

- [ ] `is_eod_close_time()` method exists and works
- [ ] EOD check happens BEFORE regular trading logic
- [ ] `liquidate_all_positions()` called at 3:58 PM
- [ ] Min hold period is IGNORED during EOD
- [ ] EOD completion is logged clearly
- [ ] EOD state file is updated
- [ ] Daily reset works (eod_done_today_ flag)
- [ ] Account is 100% cash after 3:58 PM
- [ ] Tested with paper trading
- [ ] Log messages are clear and visible

---

## Testing Commands

### Check Current Position (Morning)
```bash
source config.env
curl -X GET "https://paper-api.alpaca.markets/v2/positions" \
  -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
  -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY" | jq
```

### Manual Liquidation (if needed)
```bash
# Close all positions via Alpaca dashboard:
# https://app.alpaca.markets/paper/dashboard/overview

# Or via API:
curl -X DELETE "https://paper-api.alpaca.markets/v2/positions" \
  -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
  -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY"
```

### Build and Test
```bash
# Rebuild
cmake --build build --target sentio_cli

# Run backtest
build/sentio_cli backtest \
  --data data/equities/SPY_4blocks.csv \
  --warmup-blocks 2 \
  --test-blocks 1

# Check logs
tail -100 data/logs/cpp_trader_*.log | grep -i "eod\|liquidat\|15:58"
```

---

## Success Criteria

✅ **Code Review**: EOD closing logic implemented
✅ **Unit Test**: 3:58 PM detection works
✅ **Integration Test**: Backtest shows CASH_ONLY at end
✅ **Paper Test**: Live test at 3:58 PM liquidates positions
✅ **Logs**: Clear EOD closing messages visible
✅ **Account**: 100% cash after 3:58 PM (verified via API)

---

## Files to Review/Modify

1. `src/cli/live_trade_command.cpp` - Add EOD closing logic
2. `include/common/time_utils.h` - Add is_eod_close_time() if missing
3. `include/cli/live_trade_command.hpp` - Add eod_done_today_ flag if missing

---

## Current Status (as of 2025-10-09 04:22 AM KST)

- [x] Bug identified and documented
- [ ] Code review completed
- [ ] Fix implemented
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Live paper test successful
- [ ] Ready for production

---

**Priority**: Fix before next live trading session
**Estimated Time**: 90 minutes (review + implement + test)
**Owner**: You (to review in morning)

---

*This checklist will guide you through fixing the EOD bug when you review in the morning.*
