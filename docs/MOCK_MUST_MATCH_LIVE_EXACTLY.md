# Mock Trading Must Match Live Trading Exactly

**Date:** 2025-10-09
**Purpose:** Ensure mock trading produces **identical results** to live trading
**Principle:** Mock is NOT a simulation - it's a **replay** of what would have happened live

## Critical Understanding

The purpose of mock trading is NOT to "test" or "simulate" - it's to **replay yesterday's session exactly as if we ran live trading**. This means:

- ✅ Same warmup process (7,864 bars)
- ✅ Same feature engine initialization (64 bars)
- ✅ Same predictor training (bar-to-bar on all warmup bars)
- ✅ Same strategy state when live bars arrive
- ✅ Same trading logic, PSM states, risk management
- ✅ Same logging, dashboard, performance metrics

**If we did it right, the mock session results should be indistinguishable from a live session.**

## Live Trading Workflow (from live_trade_command.cpp)

### Phase 0: Prerequisites (Before Market Open, ~9:00 AM)

```bash
# Run comprehensive warmup script
./tools/comprehensive_warmup.sh

# This creates: data/equities/SPY_warmup_latest.csv
# Contents:
#   - 7,864 bars total
#   - 64 bars: Feature engine warmup
#   - 7,800 bars: 20 trading blocks (390 bars/block)
#   - Format: timestamp_ms,open,high,low,close,volume
```

### Phase 1: Strategy Warmup (Market Open, ~9:30 AM)

From `live_trade_command.cpp:393-516`:

```cpp
void warmup_strategy() {
    // 1. Load warmup file
    std::string warmup_file = "data/equities/SPY_warmup_latest.csv";
    std::vector<Bar> all_bars = load_csv(warmup_file);  // 7,864 bars

    // 2. Feed ALL bars to strategy
    for (size_t i = 0; i < all_bars.size(); ++i) {
        strategy_.on_bar(all_bars[i]);

        // 3. Train predictor bar-by-bar
        if (strategy_.is_ready() && i + 1 < all_bars.size()) {
            auto features = strategy_.extract_features(all_bars[i]);
            double current_close = all_bars[i].close;
            double next_close = all_bars[i + 1].close;
            double realized_return = (next_close - current_close) / current_close;

            strategy_.train_predictor(features, realized_return);
        }

        // Milestones:
        // - Bar 64: Feature engine ready
        // - Bar 3964 (64 + 3900): Strategy ready (EWRLS has enough samples)
        // - Bar 7864: Predictor trained on 3,900+ bar-to-bar transitions
    }

    // 4. Strategy is now ready for live trading
    // - Feature engine has all rolling windows initialized
    // - EWRLS predictor fully trained
    // - Multi-horizon predictions ready (1-bar, 5-bar, 10-bar)
}
```

### Phase 2: Live Trading Loop (9:30 AM - 4:00 PM)

```cpp
void on_new_bar(const Bar& bar) {
    // 1. Validate bar (OHLC checks, timestamp monotonicity)
    if (!validate_bar(bar)) return;

    // 2. Check trading hours (only trade 9:30 AM - 4:00 PM ET)
    if (!is_regular_hours()) return;

    // 3. Feed bar to strategy
    strategy_.on_bar(bar);

    // 4. Train predictor (if previous bar exists)
    if (previous_bar_) {
        auto features = strategy_.extract_features(previous_bar_.value());
        double prev_close = previous_bar_->close;
        double curr_close = bar.close;
        double realized_return = (curr_close - prev_close) / prev_close;
        strategy_.train_predictor(features, realized_return);
    }

    // 5. Generate prediction
    auto signal = strategy_.predict(bar);

    // 6. PSM decision logic
    auto target_state = signal_to_psm_state(signal.probability);

    // 7. Risk management checks
    if (should_take_profit() || should_stop_loss() || is_eod_time()) {
        target_state = CASH_ONLY;
    }

    // 8. Execute trades via Alpaca
    execute_psm_transition(current_state_, target_state, bar);

    // 9. Update state
    previous_bar_ = bar;
    bar_count_++;
}
```

### Phase 3: EOD Closing (3:58 PM)

```cpp
// Automatic EOD liquidation
if (et_time_.is_eod_liquidation_window()) {
    // Force CASH_ONLY state
    target_state = PositionStateMachine::State::CASH_ONLY;

    // Close all positions
    execute_psm_transition(current_state_, CASH_ONLY, bar);

    // Mark EOD complete (idempotent)
    eod_state_.mark_eod_complete(today_date);
}
```

## Mock Trading Workflow (MUST MATCH LIVE EXACTLY)

### Phase 0: Data Preparation

```bash
# 1. Verify warmup file exists (created by comprehensive_warmup.sh)
FILE: data/equities/SPY_warmup_latest.csv
BARS: 7,864
VERIFICATION: Must have same file live trading uses

# 2. Extract yesterday's trading day
SOURCE: data/equities/SPY_RTH_NH.csv (full historical file)
EXTRACT: Last 390 bars (yesterday's full trading day)
OUTPUT: data/tmp/SPY_yesterday.csv
BARS: 390 (9:30 AM - 4:00 PM ET)
```

### Phase 1: Strategy Warmup (IDENTICAL TO LIVE)

```python
# Python script calling C++ exactly like live does

# 1. Load warmup file (SAME FILE LIVE USES)
warmup_bars = load_csv("data/equities/SPY_warmup_latest.csv")
assert len(warmup_bars) == 7864

# 2. Initialize strategy with SAME config
config = OnlineEnsembleConfig(
    buy_threshold=0.55,
    sell_threshold=0.45,
    ewrls_lambda=0.995,
    warmup_samples=3900,
    enable_bb_amplification=True,
    bb_amplification_factor=0.10,
    enable_adaptive_learning=True,
    enable_threshold_calibration=True,
    enable_regime_detection=False
)
strategy = OnlineEnsembleStrategy(config)

# 3. Feed ALL warmup bars (EXACT SAME PROCESS)
for i, bar in enumerate(warmup_bars):
    strategy.on_bar(bar)

    # Train predictor bar-to-bar (SAME AS LIVE)
    if strategy.is_ready() and i + 1 < len(warmup_bars):
        features = strategy.extract_features(warmup_bars[i])
        current_close = warmup_bars[i].close
        next_close = warmup_bars[i + 1].close
        realized_return = (next_close - current_close) / current_close
        strategy.train_predictor(features, realized_return)

# 4. Strategy is now in EXACT SAME STATE as live at market open
```

### Phase 2: "Live" Trading (Replay Yesterday's Bars)

```python
# Load yesterday's 390 bars
yesterday_bars = load_csv("data/tmp/SPY_yesterday.csv")
assert len(yesterday_bars) == 390

# Process each bar EXACTLY like live does
previous_bar = warmup_bars[-1]  # Last warmup bar
trades = []

for i, bar in enumerate(yesterday_bars):
    # 1. Feed bar to strategy (SAME)
    strategy.on_bar(bar)

    # 2. Train predictor (SAME)
    features = strategy.extract_features(previous_bar)
    prev_close = previous_bar.close
    curr_close = bar.close
    realized_return = (curr_close - prev_close) / prev_close
    strategy.train_predictor(features, realized_return)

    # 3. Generate prediction (SAME)
    signal = strategy.predict(bar)

    # 4. PSM logic (SAME)
    target_state = signal_to_psm_state(signal.probability)

    # 5. Risk management (SAME)
    if should_take_profit() or should_stop_loss() or is_eod_time(bar):
        target_state = CASH_ONLY

    # 6. Execute transition (MockBroker instead of Alpaca)
    trade = execute_psm_transition(current_state, target_state, bar)
    if trade:
        trades.append(trade)

    # 7. Update state (SAME)
    previous_bar = bar
```

### Phase 3: EOD Closing (SAME)

```python
# Last bars (>= 3:58 PM) automatically force CASH_ONLY
# This happens in the loop above, exactly like live
```

### Phase 4: Dashboard (USE SAME TOOL)

```bash
# Use the EXACT SAME dashboard tool we use for analyzing live trades
python3 tools/generate_trade_dashboard.py \
    --trades logs/mock_trading/trades.jsonl \
    --data data/tmp/SPY_yesterday.csv \
    --output data/dashboards/mock_yesterday_dashboard.html \
    --warmup 7864 \
    --test-blocks 1 \
    --title "Mock Replay - Yesterday's Session"
```

## Key Differences That Are Acceptable

| Aspect | Live | Mock | Why Different? |
|--------|------|------|----------------|
| Data Source | Polygon WebSocket | CSV file | Mock replays historical, live streams real-time |
| Broker | Alpaca API calls | MockBroker in-memory | Mock simulates fills, live executes real orders |
| Speed | 1x (real-time) | 39x (accelerated) | Mock can run faster for testing |
| Market Impact | Real slippage | Simulated (5 bps) | Mock estimates, live experiences actual |

## Key Aspects That MUST Be Identical

| Aspect | Requirement |
|--------|-------------|
| Warmup File | Same `SPY_warmup_latest.csv` (7,864 bars) |
| Warmup Process | Feed all 7,864 bars to strategy.on_bar() |
| Predictor Training | Train on all warmup bar-to-bar transitions |
| Strategy Config | Same thresholds, lambda, BB, calibration |
| Strategy State | strategy.is_ready() = true before live bars |
| Bar Processing | Feed each bar, train predictor, generate signal |
| PSM Logic | Same state transitions, same thresholds |
| Risk Management | Same profit target, stop loss, min hold |
| EOD Closing | Same 3:58 PM liquidation logic |
| Logging Format | Same JSONL files (signals, trades, decisions) |
| Dashboard Tool | Same `generate_trade_dashboard.py` |
| Performance Metrics | Same MRB, win rate, max drawdown calculations |

## Validation Checklist

After running mock session, verify:

- [ ] Warmup used 7,864 bars (not 3,900!)
- [ ] Strategy reached ready state at bar 3,964 (64 + 3,900)
- [ ] Predictor trained on ~3,900 bar-to-bar samples during warmup
- [ ] First live bar processed with fully-initialized state
- [ ] All 390 yesterday bars processed
- [ ] Each bar trained predictor before generating signal
- [ ] PSM transitions match expected thresholds
- [ ] Min hold period (3 bars) enforced
- [ ] EOD closing happened at 3:58 PM (bar ~388-390)
- [ ] Final state = CASH_ONLY
- [ ] Dashboard shows trade book with all orders
- [ ] Performance metrics calculated on correct test period

## Example: What Live Trading Log Shows

```
[2025-10-08 09:28:45] === Starting Warmup Process ===
[2025-10-08 09:28:45]   Target: 3900 bars (10 blocks @ 390 bars/block)
[2025-10-08 09:28:45]   Available: 7864 bars
[2025-10-08 09:28:45]
[2025-10-08 09:28:47] ✓ Feature Engine Warmup Complete (64 bars)
[2025-10-08 09:28:47]   - All rolling windows initialized
[2025-10-08 09:28:47]   - Technical indicators ready
[2025-10-08 09:28:47]   - Starting predictor training...
[2025-10-08 09:28:52] ✓ Strategy Warmup Complete (3964 bars)
[2025-10-08 09:28:52]   - EWRLS predictor fully trained
[2025-10-08 09:28:52]   - Multi-horizon predictions ready
[2025-10-08 09:28:52]   - Strategy ready for live trading
[2025-10-08 09:28:54]
[2025-10-08 09:28:54] === Warmup Summary ===
[2025-10-08 09:28:54] ✓ Total bars processed: 7864
[2025-10-08 09:28:54] ✓ Feature engine ready: Bar 64
[2025-10-08 09:28:54] ✓ Strategy ready: Bar 3964
[2025-10-08 09:28:54] ✓ Predictor trained: 3899 samples
[2025-10-08 09:28:54] ✓ Last warmup bar: 2025-10-08 09:29:00
[2025-10-08 09:28:54] ✓ Strategy is_ready() = YES
[2025-10-08 09:28:54]
[2025-10-08 09:30:01] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[2025-10-08 09:30:01] 📊 BAR #7865 Received from Polygon
[2025-10-08 09:30:01]   Time: 2025-10-08 09:30:00 ET
[2025-10-08 09:30:01]   OHLC: O=$642.50 H=$642.75 L=$642.40 C=$642.60
```

## Mock Must Show IDENTICAL Log

```
[Mock 09:00:00] === Starting Warmup Process ===
[Mock 09:00:00]   Target: 3900 bars (10 blocks @ 390 bars/block)
[Mock 09:00:00]   Available: 7864 bars
[Mock 09:00:00]
[Mock 09:00:02] ✓ Feature Engine Warmup Complete (64 bars)
[Mock 09:00:02]   - All rolling windows initialized
[Mock 09:00:02]   - Technical indicators ready
[Mock 09:00:02]   - Starting predictor training...
[Mock 09:00:07] ✓ Strategy Warmup Complete (3964 bars)
[Mock 09:00:07]   - EWRLS predictor fully trained
[Mock 09:00:07]   - Multi-horizon predictions ready
[Mock 09:00:07]   - Strategy ready for live trading
[Mock 09:00:09]
[Mock 09:00:09] === Warmup Summary ===
[Mock 09:00:09] ✓ Total bars processed: 7864
[Mock 09:00:09] ✓ Feature engine ready: Bar 64
[Mock 09:00:09] ✓ Strategy ready: Bar 3964
[Mock 09:00:09] ✓ Predictor trained: 3899 samples
[Mock 09:00:09] ✓ Last warmup bar: 2025-10-07 09:29:00  # Yesterday!
[Mock 09:00:09] ✓ Strategy is_ready() = YES
[Mock 09:00:09]
[Mock 09:30:00] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[Mock 09:30:00] 📊 BAR #7865 Received (Replay Yesterday)
[Mock 09:30:00]   Time: 2025-10-07 09:30:00 ET
[Mock 09:30:00]   OHLC: O=$641.20 H=$641.45 L=$641.10 C=$641.30
```

**Notice:** Everything identical except the date (live = today, mock = yesterday)

---

**Generated:** 2025-10-09
**Status:** Requirements Complete
**Next Step:** Implement mock script matching this specification exactly
