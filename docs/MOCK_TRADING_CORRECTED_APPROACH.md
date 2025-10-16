# Mock Trading Session - Corrected Approach

**Date:** 2025-10-09
**Previous Session:** mock_20251008_1759984243 (INCORRECT - used 25 days of data)
**Status:** Awaiting corrected implementation

## Summary of Issues from Previous Run

### Root Cause
The mock trading script used the entire `SPY_RTH_NH.csv` file (9,600 bars = ~25 trading days) instead of extracting just yesterday's single trading day (390 bars).

### Actual Results (What Happened)
- Data processed: 9,600 bars (25 days)
- Total trades: 1,977 order executions (988 state transitions)
- Performance: -5.07% total (-0.20% per day, -0.05% per block)
- PSM behavior: **CORRECT** - dual instruments (SPY+SPXL) are by design
- Min hold period: **CORRECT** - 3 bar minimum enforced
- Dashboard: Used custom HTML instead of consistent tool

### Expected Results (What Should Happen)
- Data: 390 bars (1 trading day)
- Warmup: 7,864 bars (20 blocks + 64 feature bars)
- Trades: ~80 state transitions = ~160 orders
- Dashboard: Use `tools/generate_trade_dashboard.py`

## Correct Mock Trading Workflow

### Phase 0: Prerequisites
```bash
# 1. Comprehensive warmup MUST exist (created by comprehensive_warmup.sh)
FILE: data/equities/SPY_warmup_latest.csv
BARS: 7,864 (20 blocks + 64 feature warmup)
PURPOSE: Strategy initialization (learning phase)

# 2. Extract yesterday's trading day ONLY
YESTERDAY: 2025-10-07 (last trading day before session date 2025-10-08)
BARS: 390 (9:30 AM - 4:00 PM ET, 1-minute intervals)
SOURCE: Last 390 bars from SPY_RTH_NH.csv
```

### Phase 1: Warmup (Pre-Market, ~9:00 AM)
```python
# Use existing comprehensive warmup file
warmup_file = "data/equities/SPY_warmup_latest.csv"

# Verify warmup file
assert file_exists(warmup_file)
assert line_count(warmup_file) == 7865  # 7,864 bars + header
```

### Phase 2: Extract Yesterday's Data
```python
# Extract ONLY yesterday's 390 bars
yesterday_data = extract_last_n_bars(
    source="data/equities/SPY_RTH_NH.csv",
    n_bars=390,
    output="data/tmp/SPY_20251007.csv"
)

# This is the data we'll simulate as "today's market"
# In mock mode: These 390 bars stream as if they're happening live
```

### Phase 3: Morning Session (9:30 AM - 12:45 PM)
```bash
# Generate signals for ALL 390 bars with warmup
sentio_cli generate-signals \
    --data data/tmp/SPY_20251007.csv \
    --output session_dir/morning_signals.jsonl \
    --warmup 7864  # Use warmup bars for learning

# Execute trades for ALL 390 bars
sentio_cli execute-trades \
    --signals session_dir/morning_signals.jsonl \
    --data data/tmp/SPY_20251007.csv \
    --output session_dir/morning_trades.jsonl \
    --warmup 7864

# Analyze full day performance
sentio_cli analyze-trades \
    --trades session_dir/morning_trades.jsonl \
    --data data/tmp/SPY_20251007.csv

# Report ONLY morning portion (bars 0-195)
# But we process all 390 bars to get complete strategy state
```

**Key Insight:** We process the FULL 390 bars in morning session, but only report performance for the morning portion (first ~195 bars representing 9:30 AM - 12:45 PM).

### Phase 4: Midday Optimization (12:45 PM)
```bash
# Create comprehensive warmup for optimization
# warmup (7,864) + morning bars (195) = 8,059 bars
cat data/equities/SPY_warmup_latest.csv > session_dir/comprehensive_warmup_1245.csv
tail -195 data/tmp/SPY_20251007.csv >> session_dir/comprehensive_warmup_1245.csv

# Run Optuna (50 trials, ~5 minutes)
python3 tools/optuna_quick_optimize.py \
    --data session_dir/comprehensive_warmup_1245.csv \
    --trials 50 \
    --output session_dir/optimized_params.json

# Decision logic:
if improvement > 0.05%:
    use_optimized_params = True
else:
    use_baseline_params = True
```

### Phase 5: Afternoon Session (1:00 PM - 4:00 PM)
```bash
# Create comprehensive warmup for restart
# warmup (7,864) + all morning bars (195) = 8,059 bars
cat data/equities/SPY_warmup_latest.csv > session_dir/comprehensive_warmup_1pm.csv
tail -195 data/tmp/SPY_20251007.csv >> session_dir/comprehensive_warmup_1pm.csv

# Re-generate signals with optimized params (if applicable)
# Process REMAINING afternoon bars (196-390)
tail -195 data/tmp/SPY_20251007.csv > session_dir/afternoon_data.csv

sentio_cli generate-signals \
    --data session_dir/afternoon_data.csv \
    --output session_dir/afternoon_signals.jsonl \
    --warmup 8059 \
    --buy-threshold 0.58 \  # If optimized
    --sell-threshold 0.42 \
    --ewrls-lambda 0.997

sentio_cli execute-trades \
    --signals session_dir/afternoon_signals.jsonl \
    --data session_dir/afternoon_data.csv \
    --output session_dir/afternoon_trades.jsonl \
    --warmup 8059
```

### Phase 6: EOD Closing (3:58 PM)
```bash
# Check last trade in afternoon_trades.jsonl
# Verify:
# 1. All positions liquidated (portfolio.positions = {})
# 2. Final state = CASH_ONLY
# 3. Portfolio value = 100% cash

# EOD is handled automatically by execute-trades command
# The last bars (timestamp >= 15:58 ET) force CASH_ONLY state
```

### Phase 7: Generate Dashboard (4:00 PM)
```bash
# Use CONSISTENT dashboard tool
python3 tools/generate_trade_dashboard.py \
    --trades session_dir/combined_trades.jsonl \  # Morning + afternoon
    --data data/tmp/SPY_20251007.csv \
    --output data/dashboards/mock_20251007_dashboard.html \
    --warmup 7864 \
    --test-blocks 1 \
    --title "Mock Trading Session - 2025-10-07"

# Open dashboard for visual review
open data/dashboards/mock_20251007_dashboard.html
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│ WARMUP DATA (Pre-loaded)                                     │
│ File: SPY_warmup_latest.csv                                  │
│ Bars: 7,864 (20 blocks + 64 feature warmup)                  │
│ Purpose: Strategy initialization                             │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ YESTERDAY'S DATA (Extracted)                                 │
│ File: SPY_20251007.csv                                       │
│ Bars: 390 (1 full trading day)                               │
│ Time: 9:30 AM - 4:00 PM ET                                   │
└─────────────────────────────────────────────────────────────┘
                            ↓
          ┌─────────────────┴─────────────────┐
          ↓                                   ↓
┌──────────────────────┐          ┌──────────────────────┐
│ MORNING SESSION      │          │ AFTERNOON SESSION    │
│ Bars: 0-195          │          │ Bars: 196-390        │
│ Time: 9:30-12:45     │          │ Time: 1:00-4:00      │
│ Params: Baseline     │          │ Params: Optimized    │
└──────────────────────┘          └──────────────────────┘
          ↓                                   ↓
          └─────────────────┬─────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ COMBINED RESULTS                                             │
│ File: combined_trades.jsonl                                  │
│ Dashboard: mock_20251007_dashboard.html                      │
│ Metrics: Full day P&L, trade count, win rate, etc.          │
└─────────────────────────────────────────────────────────────┘
```

## Expected Performance Metrics

### With Proper 1-Day Data (390 bars)

**Baseline Scenario:**
```
Starting Capital: $100,000
Morning Trades: ~40 state transitions (~80 orders)
Afternoon Trades: ~40 state transitions (~80 orders)
Total Trades: ~80 state transitions (~160 orders)
Expected MRB: +0.046% (historical baseline)
Expected P&L: +$46
Final Capital: $100,046
```

**Optimized Scenario (If Optuna Finds Improvement):**
```
Morning Session: Same as baseline (+$46)
Afternoon Session: With optimized params (+$120)
Total P&L: +$166
Final Capital: $100,166
Improvement: +261% over baseline
```

**Comparison to Previous Incorrect Run:**
| Metric | Previous (WRONG) | Corrected (Expected) |
|--------|------------------|----------------------|
| Bars Processed | 9,600 (25 days) | 390 (1 day) |
| State Transitions | 988 | ~80 |
| Total Orders | 1,977 | ~160 |
| Return | -5.07% (over 25 days) | +0.046% (1 day) |
| Daily Return | -0.20% | +0.046% |

## Implementation Checklist

- [ ] Extract yesterday's 390 bars from SPY_RTH_NH.csv
- [ ] Verify warmup file exists (7,864 bars)
- [ ] Process full 390 bars in morning session
- [ ] Split reporting: morning (0-195) vs afternoon (196-390)
- [ ] Run midday optimization with morning data
- [ ] Restart afternoon session with comprehensive warmup
- [ ] Verify EOD closing (all cash, no positions)
- [ ] Use `generate_trade_dashboard.py` for consistent visualization
- [ ] Validate metrics match expected performance

## Key Differences from Live Trading

| Aspect | Live Trading | Mock Trading |
|--------|-------------|--------------|
| Data Source | WebSocket (real-time) | CSV file (replay) |
| Speed | 1x (real-time) | 39x (accelerated) |
| Broker | Alpaca API | MockBroker |
| Market Impact | Real slippage | Simulated (5 bps) |
| Warmup | Fetch from Alpaca | Pre-loaded CSV |
| Duration | 6.5 hours | ~10 minutes |

## Consistency is Key

As you correctly emphasized: **"Consistency is what makes progress possible and tangible."**

We must use:
1. ✅ Same warmup process (comprehensive_warmup.sh → 7,864 bars)
2. ✅ Same data format (CSV with RTH bars only)
3. ✅ Same CLI commands (generate-signals → execute-trades → analyze-trades)
4. ✅ Same dashboard tool (generate_trade_dashboard.py)
5. ✅ Same performance metrics (MRB, win rate, max drawdown)

---

**Status:** Ready for corrected implementation
**Next Step:** Fix mock trading script to extract 390 bars and use consistent dashboard
**Expected Outcome:** +0.046% to +0.17% return on 1 trading day
