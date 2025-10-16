# EOD Trading Bug - Analysis and Fix

## Date: 2025-10-09

## 🐛 Bug Report

### Issue Description
Mock trading session on October 7, 2025 showed a BUY order at **bar 388 (3:58 PM)** with NO subsequent SELL, leaving 74 SPY shares in the portfolio at end of day. EOD liquidation did NOT prevent this trade from happening.

### Expected Behavior
- EOD liquidation at 3:58 PM should close all positions
- **NO new trades should be allowed after EOD liquidation**
- Final portfolio should be 100% cash

### Actual Behavior
- EOD liquidation executed at 3:55 PM, closed all positions correctly
- **BUT trading continued after liquidation:**
  - 3:56 PM: Tried to enter BEAR_1X_ONLY (failed due to price fetch)
  - 3:57 PM: Tried to enter BEAR_1X_ONLY again (failed)
  - **3:58 PM: Successfully entered BASE_BULL_3X (BUY 74 SPY)** ❌
  - 3:59 PM: Held position, couldn't exit due to min hold period
- Final portfolio: 74 SPY + $50,363 cash (WRONG)

## 🔍 Root Cause Analysis

### Bug #1: Trading Continued After EOD Liquidation
**File**: `src/cli/live_trade_command.cpp:679`

**Problem**: The code checked `is_regular_hours()` which returns `true` during 9:30-16:00, including the EOD window (3:55-4:00). After liquidation at 3:55, the system continued to:
1. Generate signals
2. Make trading decisions
3. Execute new trades

**Code Before Fix**:
```cpp
// Line 679
if (!is_regular_hours()) {
    log_system("⏰ After-hours - learning only, no trading");
    return;
}

log_system("🕐 Regular Trading Hours - processing for signals and trades");
```

**Issue**: No check for EOD liquidation window! System treats 3:55-4:00 PM as regular trading time.

### Bug #2: Wrong EOD Time (User Request)
**File**: `include/common/time_utils.h:150`

**Problem**: EOD window was 3:55-4:00 PM, but user requested 3:58-4:00 PM

**Code Before**:
```cpp
// EOD window: 3:55 PM - 4:00 PM ET
if (hour == 15 && minute >= 55) return true;  // 3:55-3:59 PM
```

## ✅ Fixes Applied

### Fix #1: Block Trading After EOD Liquidation
**File**: `src/cli/live_trade_command.cpp:684-688`

**Solution**: Added explicit check for EOD window BEFORE allowing trading:

```cpp
// CRITICAL: Block trading after EOD liquidation (3:58 PM - 4:00 PM)
if (et_time_.is_eod_liquidation_window()) {
    log_system("🔴 EOD window active - learning only, no new trades");
    return;  // Learning continues, but no new positions
}
```

**Logic Flow Now**:
1. Check if regular hours (9:30-16:00) → if not, skip trading
2. **NEW:** Check if EOD window (3:58-4:00) → if yes, skip trading
3. Only if both pass → allow trading

### Fix #2: Change EOD Time to 3:58 PM
**File**: `include/common/time_utils.h:150-160`

**Solution**: Updated EOD window from 3:55-4:00 to 3:58-4:00:

```cpp
/**
 * @brief Check if current time is in EOD liquidation window (3:58 PM - 4:00 PM ET)
 * Uses a 2-minute window to liquidate positions before market close
 */
bool is_eod_liquidation_window() const {
    auto et_tm = get_current_et_tm();
    int hour = et_tm.tm_hour;
    int minute = et_tm.tm_min;

    // EOD window: 3:58 PM - 4:00 PM ET
    if (hour == 15 && minute >= 58) return true;  // 3:58-3:59 PM
    if (hour == 16 && minute == 0) return true;   // 4:00 PM exactly

    return false;
}
```

## 🎯 Expected Behavior After Fix

### Timeline (with fixes):
- **9:30 AM - 3:57 PM**: Normal trading allowed
- **3:58 PM**:
  - EOD liquidation triggers
  - All positions closed
  - System enters "learning-only" mode
- **3:58 PM - 4:00 PM**:
  - Strategy continues to update
  - Signals generated
  - **NO trades executed** (blocked by EOD window check)
- **4:00 PM+**: After-hours mode

### Final Result:
- Portfolio: 100% cash
- No open positions
- Clean EOD state

## 📋 Testing Required

1. ✅ Rebuild with fixes: `cmake --build build --target sentio_cli`
2. ⏳ Run mock session for Oct 7, 2025
3. ⏳ Verify:
   - EOD liquidation at 3:58 PM
   - No trades after 3:58 PM
   - Final portfolio = 100% cash
   - Log shows "🔴 EOD window active - learning only, no new trades"

## 🚧 Additional Bug Found (Separate Issue)

**MockBroker Missing Prices for SH/SDS**

During testing, found systematic failures:
```
ERROR: ❌ CRITICAL: Target state is BEAR_1X_NX but failed to calculate positions
       (likely price fetch failure)
```

**Cause**: MockBroker only has SPY prices, missing SH, SDS, SPXL prices

**Impact**: All BEAR state transitions fail in mock mode

**TODO**: Generate synthetic leveraged prices for SH/SDS/SPXL in mock mode
(Same solution as used for backtest mode)

## 📝 Summary

- **Bug**: Trading continued after EOD liquidation, allowed 3:58 PM buy
- **Root Cause**: Missing EOD window check in trading gate
- **Fix**: Added explicit block for EOD window (3:58-4:00 PM)
- **User Request**: Changed EOD time from 3:55 to 3:58 PM
- **Status**: Code fixed, needs testing with full mock session
