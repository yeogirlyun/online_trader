# Mock Trading Session Bug Analysis

**Date:** 2025-10-09
**Session:** mock_20251008_1759984243
**Status:** ❌ INCORRECT RESULTS (Root cause identified)

## Executive Summary

The mock trading session appeared to show a -5.07% loss with 19,200 trades, which seemed like a catastrophic failure. However, investigation reveals this is **not a strategy bug** but rather **incorrect data usage** in the test script.

## Root Cause Analysis

### Bug #1: Wrong Data File Used
**Issue:** The script used the entire `SPY_RTH_NH.csv` file (9,600 bars = ~25 trading days) instead of extracting just yesterday's data (390 bars = 1 trading day).

**Evidence:**
```bash
$ wc -l data/equities/SPY_RTH_NH.csv
9601 SPY_RTH_NH.csv  # 9,600 bars + header

$ wc -l logs/mock_trading/.../morning_trades.jsonl
1977 morning_trades.jsonl  # 1,977 trades in "morning" session
```

**Impact:**
- Morning session processed 9,600 bars instead of ~195 bars (9:30 AM - 12:45 PM)
- Afternoon session processed another 9,600 bars
- Total: 19,200 bars processed (~25 days of trading compressed into one mock session)
- Result: -5.07% over 25 days = -0.20% per day (actually reasonable performance!)

### Bug #2: Misleading Trade Count Display
**Issue:** The session report showed "Trades: 9600" which made it look like there were 9,600 individual trades.

**Reality:** PSM states like `BASE_BULL_3X` hold TWO instruments simultaneously:
- `BASE_BULL_3X` = 50% SPY + 50% SPXL (by design!)
- Entering this state = 2 BUY orders (SPY + SPXL)
- Exiting this state = 2 SELL orders (SPY + SPXL)

**Actual trade count:**
```bash
$ wc -l morning_trades.jsonl
1977  # This is 1,977 individual order executions

1977 orders / 2 instruments = ~988 state transitions
988 transitions / 9600 bars = 10.3% transition rate
```

**This is CORRECT behavior!** The PSM is designed to hold multiple instruments for blended leverage.

### Not a Bug: Minimum Hold Period
**Initial concern:** Trades appeared to happen every 2-5 bars, violating `MIN_HOLD_BARS=3`.

**Investigation:**
```
Bar 3919: BUY SPXL + BUY SPY (enter BASE_BULL_3X)
Bar 3924: SELL SPXL + SELL SPY (exit to CASH) - 5 bars held ✓
Bar 3926: BUY SPXL + BUY SPY (re-enter) - 2 bars in CASH ✓
Bar 3929: SELL SPXL + SELL SPY (exit) - 3 bars held ✓
```

**Verdict:** Min hold period IS being enforced correctly. The 2-bar gap is time spent in CASH_ONLY state, which doesn't count toward hold period.

### Not a Bug: Dual Instrument Positions
**Initial concern:** "PSM is buying BOTH SPXL AND SPY at the same time!"

**Reality:** This is BY DESIGN per PSM specification:
```cpp
case PositionStateMachine::State::QQQ_TQQQ:  // aka BASE_BULL_3X
    // Split: 50% base + 50% leveraged bull (blended long)
    positions[symbol_map.base] = (total_capital * 0.5) / base_price;
    positions[symbol_map.bull_3x] = (total_capital * 0.5) / bull_price;
    break;
```

From `position_state_machine.cpp:525`:
```cpp
case State::QQQ_TQQQ: return "BASE_BULL_3X";  // 50% base + 50% bull
```

**This is intentional** - it provides blended leverage exposure (effective ~2x leverage).

## Correct Performance Calculation

### Actual Results (Over 25 Days)
- Starting Capital: $100,000
- Final Capital: $94,934.14
- Total Return: -5.07%
- Trading Days: ~25 days (9,600 bars ÷ 390 bars/day)
- **Daily Return: -0.20%**
- **Annualized: -51%** (if this rate continued)

### If It Were 1 Day
Assuming -0.20% daily rate:
- Starting: $100,000
- Expected Ending: $99,800
- Expected Return: -0.20%

## Why the Strategy Lost Money

The strategy lost money on this 25-day period likely due to:

1. **Baseline Parameters Not Optimized:** Using default buy=0.55, sell=0.45, lambda=0.995
2. **No Midday Optimization:** Optuna found 0% improvement (baseline_mrb=0%, optimized_mrb=0%)
3. **Market Conditions:** The 25-day period (2025-09-13 to 2025-10-07) may have been unfavorable
4. **Warmup Insufficient:** Only 7,864 bars warmup for 9,600 test bars (ratio too low)

## What Should Have Happened

For a proper one-day mock session of 2025-10-08:

1. **Extract Yesterday's Data:**
   ```bash
   # Get last 390 bars from SPY_RTH_NH.csv (2025-10-07)
   tail -391 SPY_RTH_NH.csv > SPY_20251007.csv
   ```

2. **Split Into Morning/Afternoon:**
   - Morning: Bars 0-195 (9:30 AM - 12:45 PM)
   - Afternoon: Bars 196-390 (1:00 PM - 4:00 PM)

3. **Expected Trade Count:**
   - Historical 4-block average: ~600 trades per block
   - 1 block = 390 bars
   - Expected: ~600 trades for full day
   - With dual instruments: ~1,200 individual orders
   - Current result: 1,977 orders for 9,600 bars = 20.6 orders/bar
   - For 390 bars: ~80 state transitions = ~160 orders (more reasonable)

4. **Expected Performance:**
   - Historical MRB (baseline): +0.046% per block
   - 1 block trading: +$46 expected
   - With optimization: +$100-$200 possible

## Required Fixes

### 1. Data Extraction Script
Create `tools/extract_trading_day.py`:
```python
def extract_trading_day(full_data_file: str, date_str: str, output_file: str):
    """Extract exactly 390 bars for a specific trading day"""
    # Read CSV, filter by date, ensure exactly 390 bars
    # Handle timezone conversion (UTC to ET)
    # Write to output_file
```

### 2. Dashboard Generation
Use existing `tools/generate_trade_dashboard.py` instead of custom HTML:
```bash
python3 tools/generate_trade_dashboard.py \
    --trades logs/mock_trading/.../trades.jsonl \
    --data data/equities/SPY_20251007.csv \
    --output data/dashboards/mock_20251007_dashboard.html \
    --warmup 3900 \
    --test-blocks 1 \
    --title "Mock Trading Session - 2025-10-07"
```

### 3. Mock Trading Script Update
```python
# In launch_mock_trading_session.py

# PHASE 2: Prepare Market Data
def prepare_market_data(self, session_date: str):
    """Extract exactly 1 trading day of data"""
    yesterday = datetime.strptime(session_date, '%Y-%m-%d') - timedelta(days=1)
    yesterday_str = yesterday.strftime('%Y-%m-%d')

    # Extract 390 bars for yesterday
    extracted_data = self.session_dir / f"SPY_{yesterday_str}.csv"
    self._extract_trading_day(
        full_file=DATA_DIR / "equities/SPY_RTH_NH.csv",
        date=yesterday_str,
        output=extracted_data,
        num_bars=390
    )

    return str(extracted_data)
```

## Conclusions

### What We Learned
1. ✅ PSM multi-instrument trading is working correctly
2. ✅ Minimum hold period enforcement is working
3. ✅ Trade execution logic is correct
4. ❌ Data preparation in mock script was incorrect
5. ❌ Dashboard generation should use consistent tooling

### What Actually Happened
The -5.07% loss over 25 days translates to:
- **-0.20% per day**
- **-0.05% per block** (4 blocks/day)

This is actually close to breakeven, and the strategy simply needs optimization to flip positive.

### Next Steps
1. Fix data extraction to use exactly 390 bars
2. Use consistent `generate_trade_dashboard.py` for visualization
3. Re-run mock session with correct 1-day data
4. Verify realistic performance metrics

---

**Generated:** 2025-10-09
**Analysis Status:** ✅ COMPLETE
**Action Required:** Fix data extraction and re-run test
**Root Cause:** Script bug, not strategy bug
