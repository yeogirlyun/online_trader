# Optimization 0 Trades - Complete Root Cause Analysis

**Date**: 2025-10-10
**Status**: Multiple bugs identified, still not resolved
**Commits**: 423ec57, 12ce955, d2358b3

---

## Summary

After 4 hours of debugging and 3 attempted fixes, optimization still produces **0 trades** for all trials. The core issue is more complex than anticipated, involving multiple interacting bugs in the day-by-day backtesting system.

---

## Bugs Identified & Fixed

### ✅ Bug 1: Hardcoded PSM Thresholds (FIXED - Commit 423ec57)

**Problem**: Execute-trades used hardcoded thresholds (0.49-0.55 → CASH_ONLY) instead of optimized parameters.

**Fix**: Replaced 7-state hardcoded PSM mapping with simple binary threshold model:
```cpp
if (prob >= buy_threshold) → QQQ_ONLY (LONG)
else if (prob <= sell_threshold) → PSQ_ONLY (SHORT)
else → CASH_ONLY (NEUTRAL)
```

**Manual Test**: 220 trades with SPY_4blocks.csv ✅

---

### ✅ Bug 2: Thresholds Not Passed to Execute-Trades (FIXED - Commit 12ce955)

**Problem**: Optimization passed `--buy-threshold` and `--sell-threshold` to generate-signals but NOT to execute-trades.

**Fix**: Added threshold parameters to execute-trades command:
```python
cmd_execute = [
    self.sentio_cli, "execute-trades",
    ...
    "--buy-threshold", str(params['buy_threshold']),
    "--sell-threshold", str(params['sell_threshold'])
]
```

**Status**: Fixed but still 0 trades

---

### ❌ Bug 3: Multi-Instrument Bar Count Mismatch (PARTIALLY FIXED - Commit d2358b3)

**Problem**:
- Day files: 4301 bars (10 blocks warmup + 1 day test)
- Global SPXL/SH/SDS files: 2346 bars (full history)
- Execute-trades loads from `data/equities/` hardcoded path (line 107)
- Bar count mismatch → 0 trades

**Attempted Fix**:
1. Generate day-specific SPXL/SH/SDS files from SPY data ✅
2. Copy to `data/equities/` for execute-trades to find ✅
3. Execute-trades still produces 0 trades ❌

**Root Cause**: Execute-trades loads instruments but bar alignment is still wrong, OR there's another issue preventing trades.

---

## Current State

**Manual Test** (single 4-block file):
- ✅ Generates 220 trades
- ✅ Returns: -0.17%
- ✅ Uses correct thresholds

**Optimization Test** (day-by-day with 40 blocks):
- ❌ All trials: 0 trades
- ❌ All trials rejected (< 5 trades minimum)
- ❌ MRD: 0.0000%

---

## Possible Remaining Issues

### Theory 1: Signal/Bar Alignment
- Signals: 4301 (warmup + test)
- Bars in execute-trades: May be loading different subset
- Mismatch causes array index issues → no trades executed

### Theory 2: EOD Enforcement Too Aggressive
- Day-by-day testing enforces EOD liquidation
- Min-hold-period (3 bars) prevents entries near EOD
- Combination may prevent ANY trades from completing

### Theory 3: Warmup Index Calculation
- Execute-trades skips first N bars for warmup
- Day-by-day warmup calculation may be off by one
- Results in processing 0 actual trading bars

### Theory 4: Symbol Detection
- Even with "_SPY_" in filename, symbol detection may fail
- Falls back to default behavior that doesn't work

---

## Recommended Path Forward

### Option A: Simplify to Single-Day Testing ⭐ RECOMMENDED

**Abandon day-by-day testing temporarily**. Test optimization with full multi-day files:

```python
# Instead of 30 separate days, test on continuous 30-day file
test_data = df.iloc[warmup_idx:warmup_idx + 30*391]
test_data.to_csv("test_30days.csv")

# Run optimization on this single file
generate-signals → execute-trades → analyze-trades

# This avoids:
- Day-by-day file generation
- Leveraged data generation per day
- File copying issues
- Bar count mismatches
```

**Advantages**:
- Proven to work (manual test produced 220 trades)
- No day-by-day complexity
- Simpler debugging

**Disadvantages**:
- Loses strict EOD enforcement between days
- But EOD still enforced at end of 30-day period

---

### Option B: Single-Instrument Only (SPY)

**Remove multi-instrument support temporarily**:

```cpp
// In execute_trades_command.cpp
// Replace QQQ_ONLY/PSQ_ONLY with SPY-only states
if (prob >= buy_threshold) {
    // Buy SPY (go long)
    target_quantity = calculate_shares(cash, spy_price);
    execute_order("BUY", "SPY", target_quantity);
} else if (prob <= sell_threshold) {
    // Sell SPY (go flat or short)
    if (has_position("SPY")) {
        execute_order("SELL", "SPY", current_quantity);
    }
}
```

**Advantages**:
- No leveraged ETF generation needed
- No bar count mismatch
- Simplest possible implementation

**Disadvantages**:
- Loses leverage capability
- Less sophisticated

---

### Option C: Debug Multi-Instrument Bar Loading

**Add verbose logging to execute-trades**:
```cpp
std::cout << "Loading bars for day file: " << data_path << "\n";
std::cout << "  Warmup: " << warmup_bars << " bars\n";
std::cout << "  Total bars in file: " << all_bars.size() << "\n";
std::cout << "  SPXL bars: " << instrument_bars["SPXL"].size() << "\n";
std::cout << "  Test bars: " << (all_bars.size() - warmup_bars) << "\n";

// In trading loop
if (i % 100 == 0) {
    std::cout << "Bar " << i << ": SPY=" << bars[i].close
              << " SPXL=" << instrument_bars["SPXL"][i].close << "\n";
}
```

Then run optimization with `--verbose` to see where it breaks.

---

## My Strong Recommendation: Option A

**Simplify the test methodology** before adding back complexity:

1. **Phase 1**: Get optimization working with continuous 30-day files
   - Proven to work (220 trades in manual test)
   - No day-by-day complexity
   - Fast iteration

2. **Phase 2**: Once MRD is positive, add back day-by-day if needed
   - But question: Is day-by-day really necessary?
   - EOD liquidation can happen at end of test period
   - Strict daily reset may not be critical

3. **Phase 3**: Add back multi-instrument if performance warrants it
   - Start with 1x SPY
   - Add leverage only if base strategy works

---

## Time Investment So Far

- **4+ hours debugging**
- **3 commits** with fixes
- **Multiple manual tests**
- **Still 0 trades** in optimization

**At this point**: Simplification beats debugging. Get something working, then iterate.

---

## Next Steps

**If choosing Option A (Recommended)**:

1. Modify `run_backtest_with_eod_validation()` to use continuous files:
   ```python
   # Generate single test file instead of 30 day files
   test_start_idx = warmup_blocks * BARS_PER_DAY
   test_end_idx = test_start_idx + 30 * BARS_PER_DAY
   test_data = self.df.iloc[test_start_idx:test_end_idx]
   test_file = f"{self.output_dir}/test_30days.csv"
   test_data.to_csv(test_file)

   # Run once instead of loop
   generate_signals(test_file) → execute_trades(test_file) → analyze
   ```

2. Test with 3 trials - expect 10-50 trades per trial

3. Run full 50-trial optimization

4. Deploy if MRD > 0%

**Estimated time**: 30 minutes to implement, 20 minutes to test

---

## Lessons Learned

1. **Day-by-day testing is complex** - many edge cases
2. **Multi-instrument adds significant complexity** - bar alignment issues
3. **Manual tests work but optimization doesn't** - suggests infrastructure problem
4. **Simplification often beats debugging** - get something working first

---

## Files Affected

**Modified**:
- `src/cli/execute_trades_command.cpp` (binary threshold model)
- `scripts/run_2phase_optuna.py` (threshold passing, leveraged data generation)

**Created**:
- `CRITICAL_THRESHOLD_MISMATCH_BUG.md` (analysis)
- `OPTIMIZATION_0_TRADES_ROOT_CAUSES.md` (this file)

**Status**: Ready to pivot to Option A (continuous file testing)
