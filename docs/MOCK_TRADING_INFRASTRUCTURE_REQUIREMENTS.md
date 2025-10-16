# Mock Trading Infrastructure Requirements

**Date**: 2025-10-09
**Version**: 1.0
**Status**: Requirements Specification
**Priority**: P1 - High (Needed for rapid development & testing)

---

## Executive Summary

We need a complete mock trading infrastructure that simulates the live trading environment without requiring actual Alpaca/Polygon connections. This enables:

1. **Replay Yesterday's Session** - Re-run Oct 8 session as if live
2. **Test EOD Fix** - Validate EOD liquidation without waiting for market close
3. **Midday Optimization Testing** - Simulate 3:15 PM parameter updates
4. **Rapid Iteration** - Test multiple configurations in minutes, not days
5. **Deterministic Testing** - Reproducible results for debugging

**Key Insight**: We already have historical bar data. We just need mock broker/feed layers that replay this data as if it's live, with simulated order fills and position tracking.

---

## Current Live Trading Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LiveTradeCommand                          │
│  (src/cli/live_trade_command.cpp)                           │
└─────────────────────────────────────────────────────────────┘
                    │                    │
         ┌──────────┴──────────┐        └─────────────────┐
         │                     │                          │
         ▼                     ▼                          ▼
┌─────────────────┐  ┌──────────────────┐  ┌────────────────────┐
│ AlpacaClient    │  │ PolygonWebSocket │  │ PositionBook       │
│ (REST API)      │  │ (Real-time bars) │  │ (Local tracking)   │
└─────────────────┘  └──────────────────┘  └────────────────────┘
         │                     │                          │
         ▼                     ▼                          ▼
┌─────────────────┐  ┌──────────────────┐  ┌────────────────────┐
│ Alpaca Broker   │  │ Polygon Feed     │  │ Execution Reports  │
│ (Paper Trading) │  │ (WebSocket)      │  │ (from Alpaca)      │
└─────────────────┘  └──────────────────┘  └────────────────────┘
```

---

## Proposed Mock Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LiveTradeCommand                          │
│  (UNCHANGED - uses abstract interfaces)                     │
└─────────────────────────────────────────────────────────────┘
                    │                    │
         ┌──────────┴──────────┐        └─────────────────┐
         │                     │                          │
         ▼                     ▼                          ▼
┌─────────────────┐  ┌──────────────────┐  ┌────────────────────┐
│ IBrokerClient   │  │ IBarFeed         │  │ PositionBook       │
│ (Interface)     │  │ (Interface)      │  │ (UNCHANGED)        │
└─────────────────┘  └──────────────────┘  └────────────────────┘
         │                     │
    ┌────┴────┐           ┌────┴────┐
    │         │           │         │
    ▼         ▼           ▼         ▼
┌──────┐ ┌──────┐   ┌──────┐ ┌──────────┐
│Alpaca│ │Mock  │   │Poly  │ │MockBar   │
│Client│ │Broker│   │gon   │ │FeedReplay│
└──────┘ └──────┘   └──────┘ └──────────┘
(Real)   (New)      (Real)   (New)
```

**Key Design Principles**:
1. **Interface-based** - Introduce `IBrokerClient` and `IBarFeed` interfaces
2. **Polymorphism** - LiveTradeCommand works with interfaces, not concrete classes
3. **Replay from CSV** - MockBarFeedReplay reads historical data and emits bars
4. **Simulated Fills** - MockBroker fills orders instantly at current price
5. **Time Acceleration** - Can replay 6.5 hours in 10 minutes (39x speedup)

---

## Requirements Specification

### R1: Mock Broker Client (MockBroker)

**Purpose**: Simulate Alpaca broker without network calls

**Interface**: `IBrokerClient`
```cpp
class IBrokerClient {
public:
    virtual ~IBrokerClient() = default;

    // Order Management
    virtual std::optional<Order> place_market_order(
        const std::string& symbol, double quantity,
        const std::string& tif) = 0;
    virtual bool cancel_order(const std::string& order_id) = 0;
    virtual bool cancel_all_orders() = 0;
    virtual std::vector<Order> get_open_orders() = 0;

    // Position Management
    virtual std::vector<Position> get_positions() = 0;
    virtual std::optional<Position> get_position(const std::string& symbol) = 0;
    virtual bool close_position(const std::string& symbol) = 0;
    virtual bool close_all_positions() = 0;

    // Account Info
    virtual std::optional<AccountInfo> get_account() = 0;
    virtual bool is_market_open() = 0;
};
```

**MockBroker Implementation**:
```cpp
class MockBroker : public IBrokerClient {
public:
    MockBroker(double initial_capital = 100000.0);

    // Configuration
    void set_current_price(const std::string& symbol, double price);
    void set_fill_delay_ms(int ms);  // Simulate realistic fill delays
    void set_slippage_bps(double bps);  // Simulate slippage

    // Inspection (for testing)
    const std::vector<ExecutionReport>& get_fill_history() const;
    double get_realized_pnl() const;

private:
    double capital_;
    std::map<std::string, double> current_prices_;
    std::map<std::string, Position> positions_;
    std::vector<Order> open_orders_;
    std::vector<ExecutionReport> fill_history_;
    int fill_delay_ms_{0};
    double slippage_bps_{0.0};
};
```

**Behavior**:
- **Market Orders**: Fill immediately at `current_price_ + slippage`
- **Order IDs**: Generate sequential IDs: "MOCK_0001", "MOCK_0002", ...
- **Position Tracking**: Update internal positions map
- **P&L Calculation**: Track realized P&L from position reductions
- **Account Info**: Return mock account with current equity

**Fill Simulation**:
```cpp
std::optional<Order> MockBroker::place_market_order(...) {
    Order order;
    order.order_id = generate_order_id();
    order.symbol = symbol;
    order.quantity = quantity;
    order.status = "new";

    // Simulate fill delay
    if (fill_delay_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(fill_delay_ms_));
    }

    // Fill at current price + slippage
    double fill_price = current_prices_[symbol];
    if (quantity > 0) {
        fill_price *= (1.0 + slippage_bps_ / 10000.0);  // Buy slippage
    } else {
        fill_price *= (1.0 - slippage_bps_ / 10000.0);  // Sell slippage
    }

    order.filled_qty = std::abs(quantity);
    order.filled_avg_price = fill_price;
    order.status = "filled";

    update_positions(order);
    fill_history_.push_back(to_execution_report(order));

    return order;
}
```

---

### R2: Mock Bar Feed (MockBarFeedReplay)

**Purpose**: Replay historical bars as if they're arriving in real-time

**Interface**: `IBarFeed`
```cpp
struct Bar {
    std::string symbol;
    uint64_t timestamp;  // Unix timestamp in microseconds
    double open;
    double high;
    double low;
    double close;
    uint64_t volume;
};

class IBarFeed {
public:
    virtual ~IBarFeed() = default;

    // Start feeding bars
    virtual void start() = 0;

    // Stop feeding
    virtual void stop() = 0;

    // Check if more bars available
    virtual bool has_next() const = 0;

    // Get next bar (blocking or returns nullopt if none)
    virtual std::optional<Bar> get_next_bar(int timeout_ms = 1000) = 0;

    // Skip to specific time (for testing)
    virtual void skip_to_time(const std::string& et_time) = 0;
};
```

**MockBarFeedReplay Implementation**:
```cpp
class MockBarFeedReplay : public IBarFeed {
public:
    // Load bars from CSV file
    explicit MockBarFeedReplay(const std::string& csv_path);

    // Configuration
    void set_speed(double multiplier);  // 1.0 = realtime, 39.0 = 39x faster
    void set_start_time(const std::string& et_time);  // "09:30:00"
    void set_end_time(const std::string& et_time);    // "16:00:00"

    // IBarFeed interface
    void start() override;
    void stop() override;
    bool has_next() const override;
    std::optional<Bar> get_next_bar(int timeout_ms) override;
    void skip_to_time(const std::string& et_time) override;

private:
    std::vector<Bar> bars_;
    size_t current_index_{0};
    double speed_multiplier_{1.0};
    uint64_t start_timestamp_{0};
    uint64_t end_timestamp_{0};
    std::chrono::steady_clock::time_point replay_start_time_;
    bool running_{false};
};
```

**Replay Logic**:
```cpp
std::optional<Bar> MockBarFeedReplay::get_next_bar(int timeout_ms) {
    if (!running_ || current_index_ >= bars_.size()) {
        return std::nullopt;
    }

    const Bar& bar = bars_[current_index_];

    // Calculate how long to wait before returning this bar
    if (current_index_ > 0) {
        const Bar& prev_bar = bars_[current_index_ - 1];
        uint64_t bar_delta_us = bar.timestamp - prev_bar.timestamp;

        // Apply speed multiplier
        auto wait_duration = std::chrono::microseconds(
            static_cast<uint64_t>(bar_delta_us / speed_multiplier_)
        );

        std::this_thread::sleep_for(wait_duration);
    } else {
        replay_start_time_ = std::chrono::steady_clock::now();
    }

    current_index_++;
    return bar;
}
```

**CSV Format** (SPY_RTH_NH.csv):
```
timestamp,open,high,low,close,volume
2025-10-08 09:30:00,581.23,581.45,581.10,581.30,1234567
2025-10-08 09:31:00,581.30,581.50,581.25,581.40,987654
...
```

---

### R3: Mock Session Runner (MockLiveSession)

**Purpose**: Orchestrate mock trading session with all components

```cpp
class MockLiveSession {
public:
    struct Config {
        std::string csv_data_path;
        double initial_capital{100000.0};
        double speed_multiplier{1.0};  // 1.0 = realtime
        std::string start_time{"09:30:00"};
        std::string end_time{"16:00:00"};
        bool enable_midday_optim{true};
        std::string midday_optim_time{"15:15:00"};
        bool enable_eod{true};
        std::string eod_window_start{"15:55:00"};
    };

    explicit MockLiveSession(const Config& config);

    // Run complete session
    void run();

    // Get results
    SessionReport get_report() const;

private:
    Config config_;
    std::unique_ptr<MockBroker> broker_;
    std::unique_ptr<MockBarFeedReplay> feed_;
    std::unique_ptr<OnlineEnsembleStrategy> strategy_;
    std::unique_ptr<PositionBook> position_book_;
    std::unique_ptr<EodGuardian> eod_guardian_;
};
```

**Usage Example**:
```cpp
// Replay yesterday's session at 39x speed
MockLiveSession::Config config;
config.csv_data_path = "data/equities/SPY_RTH_NH.csv";
config.speed_multiplier = 39.0;  // 6.5 hours → 10 minutes
config.initial_capital = 100000.0;

MockLiveSession session(config);
session.run();

SessionReport report = session.get_report();
std::cout << "Final equity: " << report.final_equity << std::endl;
std::cout << "Total trades: " << report.total_trades << std::endl;
std::cout << "EOD liquidated: " << report.eod_liquidated << std::endl;
```

---

### R4: Session Report & Analytics

```cpp
struct SessionReport {
    // Performance
    double initial_capital;
    double final_equity;
    double total_pnl;
    double total_pnl_pct;

    // Trading Activity
    int total_trades;
    int winning_trades;
    int losing_trades;
    double win_rate;

    // State Transitions
    std::vector<std::string> state_history;  // "CASH_ONLY", "BASE_BULL_3X", ...
    int state_transition_count;

    // EOD Verification
    bool eod_liquidated;
    std::string eod_status;  // "DONE", "FAILED", "SKIPPED"
    int positions_at_eod;

    // Midday Optimization
    bool midday_optim_triggered;
    std::string midday_optim_time;
    std::map<std::string, double> params_before;
    std::map<std::string, double> params_after;

    // Timing
    std::string session_start;
    std::string session_end;
    double session_duration_sec;
    double bars_processed;

    // Errors
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};
```

---

## Use Cases

### UC1: Replay Yesterday's Session

**Goal**: Verify Oct 8 session would have EOD liquidated correctly

```bash
./build/sentio_cli mock-session \
    --data data/equities/SPY_20251008.csv \
    --speed 39 \
    --start 09:30:00 \
    --end 16:00:00 \
    --enable-eod \
    --report /tmp/session_report.json
```

**Expected Output**:
```json
{
    "final_equity": 99724.59,
    "total_pnl": -275.41,
    "total_trades": 8,
    "eod_liquidated": true,
    "eod_status": "DONE",
    "positions_at_eod": 0,
    "session_duration_sec": 600
}
```

---

### UC2: Test EOD Fix (Intraday Restart Scenario)

**Goal**: Reproduce the bug scenario - restart at 3:34 PM and verify EOD still works

```cpp
MockLiveSession::Config config;
config.csv_data_path = "data/equities/SPY_20251008.csv";
config.start_time = "15:34:00";  // Start late (simulating restart)
config.speed_multiplier = 10.0;  // 26 min → 2.6 min

// Pre-mark EOD complete (simulating the bug scenario)
EodStateStore eod_state("logs/live_trading/eod_state.txt");
eod_state.save("2025-10-08", EodState{EodStatus::DONE, "", 0});

MockLiveSession session(config);
session.run();

SessionReport report = session.get_report();
assert(report.eod_liquidated == true);  // Should liquidate despite DONE status
assert(report.positions_at_eod == 0);   // Should be flat
```

---

### UC3: Midday Optimization Testing

**Goal**: Test parameter updates at 3:15 PM

```cpp
MockLiveSession::Config config;
config.csv_data_path = "data/equities/SPY_20251008.csv";
config.speed_multiplier = 39.0;
config.enable_midday_optim = true;
config.midday_optim_time = "15:15:00";

MockLiveSession session(config);
session.run();

SessionReport report = session.get_report();
assert(report.midday_optim_triggered == true);
assert(report.params_before.size() > 0);
assert(report.params_after.size() > 0);
// Verify params actually changed
assert(report.params_before["threshold_base"] != report.params_after["threshold_base"]);
```

---

### UC4: Rapid Parameter Tuning

**Goal**: Test 10 different parameter sets in 10 minutes (instead of 10 days)

```cpp
std::vector<ParamSet> param_sets = load_param_sets("test_params.json");
std::vector<SessionReport> reports;

for (const auto& params : param_sets) {
    MockLiveSession::Config config;
    config.csv_data_path = "data/equities/SPY_20251008.csv";
    config.speed_multiplier = 100.0;  // Ultra-fast: 6.5h → 4 min

    MockLiveSession session(config);
    session.set_strategy_params(params);
    session.run();

    reports.push_back(session.get_report());
}

// Find best params
auto best = std::max_element(reports.begin(), reports.end(),
    [](const auto& a, const auto& b) { return a.total_pnl < b.total_pnl; });

std::cout << "Best PnL: " << best->total_pnl << std::endl;
```

---

### UC5: Multi-Day Backtesting

**Goal**: Test over 20 trading days (Oct 2024 - Nov 2024)

```cpp
std::vector<std::string> csv_files = {
    "data/equities/SPY_20241001.csv",
    "data/equities/SPY_20241002.csv",
    // ... 20 files
};

double cumulative_pnl = 0.0;
for (const auto& csv_file : csv_files) {
    MockLiveSession::Config config;
    config.csv_data_path = csv_file;
    config.speed_multiplier = 100.0;

    MockLiveSession session(config);
    session.run();

    SessionReport report = session.get_report();
    cumulative_pnl += report.total_pnl;

    std::cout << csv_file << ": " << report.total_pnl << std::endl;
}

std::cout << "Total PnL over 20 days: " << cumulative_pnl << std::endl;
```

---

## Implementation Plan

### Phase 1: Interfaces & Mock Broker (2 hours)
- [ ] Create `include/testing/ibroker_client.h` interface
- [ ] Implement `src/testing/mock_broker.cpp`
- [ ] Unit tests for MockBroker

### Phase 2: Mock Bar Feed (1 hour)
- [ ] Create `include/testing/ibar_feed.h` interface
- [ ] Implement `src/testing/mock_bar_feed_replay.cpp`
- [ ] Unit tests for replay logic

### Phase 3: Session Runner (2 hours)
- [ ] Create `include/testing/mock_live_session.h`
- [ ] Implement `src/testing/mock_live_session.cpp`
- [ ] Session report generation

### Phase 4: Integration (2 hours)
- [ ] Refactor `LiveTradeCommand` to use `IBrokerClient`
- [ ] Refactor bar ingestion to use `IBarFeed`
- [ ] Add mock session CLI command

### Phase 5: Testing & Validation (2 hours)
- [ ] UC1: Replay Oct 8 session
- [ ] UC2: Test EOD fix with intraday restart
- [ ] UC3: Test midday optimization
- [ ] UC4: Rapid parameter tuning
- [ ] UC5: Multi-day backtesting

**Total Estimated Time**: 9 hours

---

## File Structure

```
include/testing/
    ibroker_client.h           # Broker interface
    ibar_feed.h                # Bar feed interface
    mock_broker.h              # Mock broker implementation
    mock_bar_feed_replay.h     # CSV replay implementation
    mock_live_session.h        # Session orchestrator

src/testing/
    mock_broker.cpp
    mock_bar_feed_replay.cpp
    mock_live_session.cpp

tests/
    test_mock_broker.cpp
    test_mock_bar_feed.cpp
    test_mock_session.cpp
    test_eod_fix_replay.cpp    # Specific test for EOD bug fix
```

---

## Benefits

1. **Rapid Development**: Test in minutes, not days
2. **Deterministic**: Same input → same output (no network randomness)
3. **EOD Testing**: Don't wait until 3:55 PM to test EOD logic
4. **Parameter Tuning**: Test 100 param sets in 1 hour
5. **Regression Testing**: Verify fixes don't break existing behavior
6. **CI/CD Ready**: Can run in automated tests
7. **No API Costs**: No Polygon API quota usage
8. **Offline Development**: Work without internet

---

## Comparison to Existing Backtest

**Existing `backtest` command**:
- Processes ALL bars in one pass
- No real-time bar arrival
- No WebSocket simulation
- No order execution simulation
- Simplified position tracking

**Proposed Mock Session**:
- Emulates real-time bar arrival with delays
- Simulates broker order fills
- Full position tracking with P&L
- Tests actual LiveTradeCommand code path
- Can replay with time acceleration

**Key Difference**: Mock session runs the **actual live trading code**, just with mock data sources. This tests the real system, not a simplified backtest.

---

## Success Criteria

- [ ] Can replay Oct 8 session and reproduce same trades
- [ ] EOD liquidation works in mock environment
- [ ] Midday optimization triggers at 3:15 PM (simulated time)
- [ ] Can run 20-day backtest in under 1 hour
- [ ] Session report matches live trading logs
- [ ] No code changes to strategy logic required

---

## Related Components

### Source Modules to Review:
1. `src/cli/live_trade_command.cpp` - Main live trading loop
2. `include/live/alpaca_client.hpp` - Broker interface to abstract
3. `src/live/polygon_websocket_fifo.cpp` - Bar feed to abstract
4. `include/live/position_book.h` - Position tracking (reusable)
5. `include/common/eod_guardian.h` - EOD logic (reusable)
6. `src/strategy/online_ensemble_strategy.cpp` - Strategy (reusable)

### Reusable Components:
- ✅ `PositionBook` - Already decoupled, works as-is
- ✅ `EodGuardian` - Already decoupled, works as-is
- ✅ `OnlineEnsembleStrategy` - Already decoupled, works as-is
- ❌ `AlpacaClient` - Needs interface abstraction
- ❌ `PolygonWebSocket` - Needs interface abstraction
- ❌ `LiveTradeCommand` - Needs dependency injection

---

## Risk Assessment

**Low Risk** ✅:
- No changes to production code (only additions)
- Interfaces don't break existing functionality
- Mock code is isolated in `testing/` directory

**Medium Risk** ⚠️:
- Refactoring `LiveTradeCommand` to use interfaces
- Ensuring mock broker behaves exactly like real broker

**High Risk** ❌:
- None

---

## Open Questions

1. **Fill Simulation Realism**: How realistic should order fills be?
   - Instant fills vs. delayed fills?
   - Slippage modeling?
   - Partial fills?

2. **Time Acceleration Limits**: What's max safe speed multiplier?
   - 100x = 6.5 hours → 4 minutes
   - Any threading issues at high speeds?

3. **State Persistence**: Should mock session write to real state files?
   - Or use in-memory state only?

4. **Multi-Symbol Support**: Currently focused on SPY
   - Extend to QQQ, TQQQ, PSQ, SQQQ?

---

## Next Steps

1. **Review Requirements**: Confirm this approach makes sense
2. **Design Interfaces**: Finalize `IBrokerClient` and `IBarFeed` APIs
3. **Implement Phase 1**: Start with MockBroker
4. **Test EOD Fix**: Use mock to validate yesterday's bug fix
5. **Integrate**: Update LiveTradeCommand to use interfaces

---

**Status**: Requirements Specification Complete
**Ready for Implementation**: ✅ YES
**Estimated Timeline**: 9 hours (1-2 days)

---

*This mock infrastructure will transform development velocity from days-per-test to minutes-per-test, enabling rapid iteration and comprehensive testing of the live trading system.*
