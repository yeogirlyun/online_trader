# Mock Trading Infrastructure - Complete Simulation Environment

**Generated**: 2025-10-09 12:03:34

**Description**: Requirements and source modules for implementing mock broker and bar feed to enable rapid testing of live trading system without real Alpaca/Polygon connections. Enables replaying historical sessions, testing EOD liquidation, midday optimization, and rapid parameter tuning.

---

## Table of Contents

1. [MOCK_TRADING_INFRASTRUCTURE_REQUIREMENTS.md](#MOCK_TRADING_INFRASTRUCTURE_REQUIREMENTSmd)
2. [src/cli/live_trade_command.cpp](#src-cli-live_trade_commandcpp)
3. [include/live/alpaca_client.hpp](#include-live-alpaca_clienthpp)
4. [src/live/alpaca_client.cpp](#src-live-alpaca_clientcpp)
5. [include/live/polygon_client.hpp](#include-live-polygon_clienthpp)
6. [src/live/polygon_websocket_fifo.cpp](#src-live-polygon_websocket_fifocpp)
7. [include/live/position_book.h](#include-live-position_bookh)
8. [src/live/position_book.cpp](#src-live-position_bookcpp)
9. [include/common/eod_guardian.h](#include-common-eod_guardianh)
10. [src/common/eod_guardian.cpp](#src-common-eod_guardiancpp)
11. [include/common/eod_state.h](#include-common-eod_stateh)
12. [src/common/eod_state.cpp](#src-common-eod_statecpp)
13. [include/strategy/online_ensemble_strategy.h](#include-strategy-online_ensemble_strategyh)
14. [src/strategy/online_ensemble_strategy.cpp](#src-strategy-online_ensemble_strategycpp)
15. [include/backend/adaptive_trading_mechanism.h](#include-backend-adaptive_trading_mechanismh)
16. [src/backend/adaptive_trading_mechanism.cpp](#src-backend-adaptive_trading_mechanismcpp)
17. [include/common/time_utils.h](#include-common-time_utilsh)
18. [src/common/time_utils.cpp](#src-common-time_utilscpp)
19. [include/common/types.h](#include-common-typesh)
20. [src/common/types.cpp](#src-common-typescpp)

---

## File: `MOCK_TRADING_INFRASTRUCTURE_REQUIREMENTS.md`

**Path**: `MOCK_TRADING_INFRASTRUCTURE_REQUIREMENTS.md`

```markdown
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
```

---

## File: `src/cli/live_trade_command.cpp`

**Path**: `src/cli/live_trade_command.cpp`

```cpp
#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include "common/time_utils.h"
#include "common/bar_validator.h"
#include "common/exceptions.h"
#include "common/eod_state.h"
#include "common/nyse_calendar.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <optional>

namespace sentio {
namespace cli {

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config() {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // v1.0 asymmetric thresholds (from optimization)
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;

    // EWRLS parameters
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 3900;  // 10 blocks @ 390 bars/block

    // Enable BB amplification
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;

    // Adaptive learning
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    // TEMPORARY: Disable regime detection to test NaN fix
    config.enable_regime_detection = false;
    config.regime_check_interval = 60;  // Check every 60 bars

    return config;
}

/**
 * Live Trading Runner for OnlineEnsemble Strategy v1.0
 *
 * - Trades SPY/SDS/SPXL/SH during regular hours (9:30am - 4:00pm ET)
 * - Uses OnlineEnsemble EWRLS with asymmetric thresholds
 * - Comprehensive logging of all decisions and trades
 */
class LiveTrader {
public:
    LiveTrader(const std::string& alpaca_key,
               const std::string& alpaca_secret,
               const std::string& polygon_url,
               const std::string& polygon_key,
               const std::string& log_dir)
        : alpaca_(alpaca_key, alpaca_secret, true /* paper */)
        , polygon_(polygon_url, polygon_key)
        , log_dir_(log_dir)
        , strategy_(create_v1_config())
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
        , et_time_()  // Initialize ET time manager
        , eod_state_(log_dir + "/eod_state.txt")  // Persistent EOD tracking
        , nyse_calendar_()  // NYSE holiday calendar
    {
        // Initialize log files
        init_logs();

        // SPY trading configuration (maps to sentio PSM states)
        symbol_map_ = {
            {"SPY", "SPY"},      // Base 1x
            {"SPXL", "SPXL"},    // Bull 3x
            {"SH", "SH"},        // Bear -1x
            {"SDS", "SDS"}       // Bear -2x
        };
    }

    void run() {
        log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to Alpaca
        log_system("Connecting to Alpaca Paper Trading...");
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to connect to Alpaca");
            return;
        }
        log_system("✓ Connected - Account: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to Polygon
        log_system("Connecting to Polygon proxy...");
        if (!polygon_.connect()) {
            log_error("Failed to connect to Polygon");
            return;
        }
        log_system("✓ Connected to Polygon");

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!polygon_.subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Reconcile existing positions on startup (seamless continuation)
        reconcile_startup_positions();

        // Check for missed EOD and startup catch-up liquidation
        check_startup_eod_catch_up();

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Start main trading loop
        polygon_.start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Keep running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

private:
    AlpacaClient alpaca_;
    PolygonClient polygon_;
    std::string log_dir_;
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // NEW: Production safety infrastructure
    PositionBook position_book_;
    ETTimeManager et_time_;  // Centralized ET time management
    EodStateStore eod_state_;  // Idempotent EOD tracking
    NyseCalendar nyse_calendar_;  // Holiday and half-day calendar
    std::optional<Bar> previous_bar_;  // For bar-to-bar learning
    uint64_t bar_count_{0};

    // Mid-day optimization (15:15 PM ET / 3:15pm)
    std::vector<Bar> todays_bars_;  // Collect ALL bars from 9:30 onwards
    bool midday_optimization_done_{false};  // Flag to track if optimization ran today
    std::string midday_optimization_date_;  // Date of last optimization (YYYY-MM-DD)

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        auto timestamp = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + timestamp + ".log");
        log_signals_.open(log_dir_ + "/signals_" + timestamp + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + timestamp + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + timestamp + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + timestamp + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        return et_time_.get_current_et_string();
    }

    bool is_regular_hours() const {
        return et_time_.is_regular_hours();
    }

    bool is_end_of_day_liquidation_time() const {
        return et_time_.is_eod_liquidation_window();
    }

    void log_system(const std::string& message) {
        auto timestamp = get_timestamp_readable();
        std::cout << "[" << timestamp << "] " << message << std::endl;
        log_system_ << "[" << timestamp << "] " << message << std::endl;
        log_system_.flush();
    }

    void log_error(const std::string& message) {
        log_system("ERROR: " + message);
    }

    void reconcile_startup_positions() {
        // Get current broker positions and cash
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to get account info for startup reconciliation");
            return;
        }

        auto broker_positions = get_broker_positions();

        log_system("=== Startup Position Reconciliation ===");
        log_system("  Cash: $" + std::to_string(account->cash));
        log_system("  Portfolio Value: $" + std::to_string(account->portfolio_value));

        if (broker_positions.empty()) {
            log_system("  Current Positions: NONE (starting flat)");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;
            log_system("  Initial State: CASH_ONLY");
            log_system("  Bars Held: 0 (no positions)");
        } else {
            log_system("  Current Positions:");
            for (const auto& pos : broker_positions) {
                log_system("    " + pos.symbol + ": " +
                          std::to_string(pos.qty) + " shares @ $" +
                          std::to_string(pos.avg_entry_price) +
                          " (P&L: $" + std::to_string(pos.unrealized_pnl) + ")");

                // Initialize position book with existing positions
                position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
            }

            // Infer current PSM state from positions
            current_state_ = infer_state_from_positions(broker_positions);

            // CRITICAL FIX: Set bars_held to MIN_HOLD_BARS to allow immediate exits
            // since we don't know how long the positions have been held
            bars_held_ = MIN_HOLD_BARS;

            log_system("  Inferred PSM State: " + psm_.state_to_string(current_state_));
            log_system("  Bars Held: " + std::to_string(bars_held_) +
                      " (set to MIN_HOLD to allow immediate exits on startup)");
            log_system("  NOTE: Positions were reconciled from broker - assuming min hold satisfied");
        }

        log_system("✓ Startup reconciliation complete - resuming trading seamlessly");
        log_system("");
    }

    void check_startup_eod_catch_up() {
        log_system("=== Startup EOD Catch-Up Check ===");

        auto et_tm = et_time_.get_current_et_tm();
        std::string today_et = format_et_date(et_tm);
        std::string prev_trading_day = get_previous_trading_day(et_tm);

        log_system("  Current ET Time: " + et_time_.get_current_et_string());
        log_system("  Today (ET): " + today_et);
        log_system("  Previous Trading Day: " + prev_trading_day);

        // Check 1: Did we miss previous trading day's EOD?
        if (!eod_state_.is_eod_complete(prev_trading_day)) {
            log_system("  ⚠️  WARNING: Previous trading day's EOD not completed");

            auto broker_positions = get_broker_positions();
            if (!broker_positions.empty()) {
                log_system("  ⚠️  Open positions detected - executing catch-up liquidation");
                liquidate_all_positions();
                eod_state_.mark_eod_complete(prev_trading_day);
                log_system("  ✓ Catch-up liquidation complete for " + prev_trading_day);
            } else {
                log_system("  ✓ No open positions - marking previous EOD as complete");
                eod_state_.mark_eod_complete(prev_trading_day);
            }
        } else {
            log_system("  ✓ Previous trading day EOD already complete");
        }

        // Check 2: Started outside trading hours with positions?
        if (et_time_.should_liquidate_on_startup(has_open_positions())) {
            log_system("  ⚠️  Started outside trading hours with open positions");
            log_system("  ⚠️  Executing immediate liquidation");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("  ✓ Startup liquidation complete");
        }

        log_system("✓ Startup EOD check complete");
        log_system("");
    }

    std::string format_et_date(const std::tm& tm) const {
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        return std::string(buffer);
    }

    std::string get_previous_trading_day(const std::tm& current_tm) const {
        // Walk back day-by-day until we find a trading day
        std::tm tm = current_tm;
        for (int i = 1; i <= 10; ++i) {
            // Subtract i days (approximate - good enough for recent history)
            std::time_t t = std::mktime(&tm) - (i * 86400);
            std::tm* prev_tm = std::localtime(&t);
            std::string prev_date = format_et_date(*prev_tm);

            // Check if weekday and not holiday
            if (prev_tm->tm_wday >= 1 && prev_tm->tm_wday <= 5) {
                if (nyse_calendar_.is_trading_day(prev_date)) {
                    return prev_date;
                }
            }
        }
        // Fallback: return today if can't find previous trading day
        return format_et_date(current_tm);
    }

    bool has_open_positions() {
        auto broker_positions = get_broker_positions();
        return !broker_positions.empty();
    }

    PositionStateMachine::State infer_state_from_positions(
        const std::vector<BrokerPosition>& positions) {

        // Map SPY instruments to equivalent QQQ PSM states
        // SPY/SPXL/SH/SDS → QQQ/TQQQ/PSQ/SQQQ
        bool has_base = false;   // SPY
        bool has_bull3x = false; // SPXL
        bool has_bear1x = false; // SH
        bool has_bear_nx = false; // SDS

        for (const auto& pos : positions) {
            if (pos.qty > 0) {
                if (pos.symbol == "SPXL") has_bull3x = true;
                if (pos.symbol == "SPY") has_base = true;
                if (pos.symbol == "SH") has_bear1x = true;
                if (pos.symbol == "SDS") has_bear_nx = true;
            }
        }

        // Check for dual-instrument states first
        if (has_base && has_bull3x) return PositionStateMachine::State::QQQ_TQQQ;    // BASE_BULL_3X
        if (has_bear1x && has_bear_nx) return PositionStateMachine::State::PSQ_SQQQ; // BEAR_1X_NX

        // Single instrument states
        if (has_bull3x) return PositionStateMachine::State::TQQQ_ONLY;  // BULL_3X_ONLY
        if (has_base) return PositionStateMachine::State::QQQ_ONLY;     // BASE_ONLY
        if (has_bear1x) return PositionStateMachine::State::PSQ_ONLY;   // BEAR_1X_ONLY
        if (has_bear_nx) return PositionStateMachine::State::SQQQ_ONLY; // BEAR_NX_ONLY

        return PositionStateMachine::State::CASH_ONLY;
    }

    void warmup_strategy() {
        // Load warmup data created by comprehensive_warmup.sh script
        // This file contains: 7864 warmup bars (20 blocks @ 390 bars/block + 64 feature bars) + all of today's bars up to now
        std::string warmup_file = "data/equities/SPY_warmup_latest.csv";

        // Try relative path first, then from parent directory
        std::ifstream file(warmup_file);
        if (!file.is_open()) {
            warmup_file = "../data/equities/SPY_warmup_latest.csv";
            file.open(warmup_file);
        }

        if (!file.is_open()) {
            log_system("WARNING: Could not open warmup file: " + warmup_file);
            log_system("         Run tools/warmup_live_trading.sh first!");
            log_system("         Strategy will learn from first few live bars");
            return;
        }

        // Read all bars from warmup file
        std::vector<Bar> all_bars;
        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string ts_str, open_str, high_str, low_str, close_str, volume_str;

            // CSV format: timestamp,open,high,low,close,volume
            if (std::getline(iss, ts_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_str);  // Already in milliseconds
                bar.open = std::stod(open_str);
                bar.high = std::stod(high_str);
                bar.low = std::stod(low_str);
                bar.close = std::stod(close_str);
                bar.volume = std::stoll(volume_str);
                all_bars.push_back(bar);
            }
        }
        file.close();

        if (all_bars.empty()) {
            log_system("WARNING: No bars loaded from warmup file");
            return;
        }

        log_system("Loaded " + std::to_string(all_bars.size()) + " bars from warmup file");
        log_system("");

        // Feed ALL bars (3900 warmup + today's bars)
        // This ensures we're caught up to the current time
        log_system("=== Starting Warmup Process ===");
        log_system("  Target: 3900 bars (10 blocks @ 390 bars/block)");
        log_system("  Available: " + std::to_string(all_bars.size()) + " bars");
        log_system("");

        int predictor_training_count = 0;
        int feature_engine_ready_bar = 0;
        int strategy_ready_bar = 0;

        for (size_t i = 0; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]);

            // Report feature engine ready
            if (i == 64 && feature_engine_ready_bar == 0) {
                feature_engine_ready_bar = i;
                log_system("✓ Feature Engine Warmup Complete (64 bars)");
                log_system("  - All rolling windows initialized");
                log_system("  - Technical indicators ready");
                log_system("  - Starting predictor training...");
                log_system("");
            }

            // Train predictor on bar-to-bar returns (wait for strategy to be fully ready)
            if (strategy_.is_ready() && i + 1 < all_bars.size()) {
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }

            // Report strategy ready
            if (strategy_.is_ready() && strategy_ready_bar == 0) {
                strategy_ready_bar = i;
                log_system("✓ Strategy Warmup Complete (" + std::to_string(i) + " bars)");
                log_system("  - EWRLS predictor fully trained");
                log_system("  - Multi-horizon predictions ready");
                log_system("  - Strategy ready for live trading");
                log_system("");
            }

            // Progress indicator every 1000 bars
            if ((i + 1) % 1000 == 0) {
                log_system("  Progress: " + std::to_string(i + 1) + "/" + std::to_string(all_bars.size()) +
                          " bars (" + std::to_string(predictor_training_count) + " training samples)");
            }

            // Update bar_count_ and previous_bar_ for seamless transition to live
            bar_count_++;
            previous_bar_ = all_bars[i];
        }

        log_system("");
        log_system("=== Warmup Summary ===");
        log_system("✓ Total bars processed: " + std::to_string(all_bars.size()));
        log_system("✓ Feature engine ready: Bar " + std::to_string(feature_engine_ready_bar));
        log_system("✓ Strategy ready: Bar " + std::to_string(strategy_ready_bar));
        log_system("✓ Predictor trained: " + std::to_string(predictor_training_count) + " samples");
        log_system("✓ Last warmup bar: " + format_bar_time(all_bars.back()));
        log_system("✓ Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("");
    }

    std::string format_bar_time(const Bar& bar) const {
        time_t time_t_val = static_cast<time_t>(bar.timestamp_ms / 1000);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void on_new_bar(const Bar& bar) {
        auto timestamp = get_timestamp_readable();
        bar_count_++;

        // Log bar received
        log_system("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        log_system("📊 BAR #" + std::to_string(bar_count_) + " Received from Polygon");
        log_system("  Time: " + timestamp);
        log_system("  OHLC: O=$" + std::to_string(bar.open) + " H=$" + std::to_string(bar.high) +
                  " L=$" + std::to_string(bar.low) + " C=$" + std::to_string(bar.close));
        log_system("  Volume: " + std::to_string(bar.volume));

        // =====================================================================
        // STEP 1: Bar Validation (NEW - P4)
        // =====================================================================
        if (!is_valid_bar(bar)) {
            log_error("❌ Invalid bar dropped: " + BarValidator::get_error_message(bar));
            return;
        }
        log_system("✓ Bar validation passed");

        // =====================================================================
        // STEP 2: Feed to Strategy (ALWAYS - for continuous learning)
        // =====================================================================
        log_system("⚙️  Feeding bar to strategy (updating indicators)...");
        strategy_.on_bar(bar);

        // =====================================================================
        // STEP 3: Continuous Bar-to-Bar Learning (NEW - P1-1 fix)
        // =====================================================================
        if (previous_bar_.has_value()) {
            auto features = strategy_.extract_features(*previous_bar_);
            if (!features.empty()) {
                double return_1bar = (bar.close - previous_bar_->close) /
                                    previous_bar_->close;
                strategy_.train_predictor(features, return_1bar);
                log_system("✓ Predictor updated (learning from previous bar return: " +
                          std::to_string(return_1bar * 100) + "%)");
            }
        }
        previous_bar_ = bar;

        // =====================================================================
        // STEP 3.5: Increment bars_held counter (CRITICAL for min hold period)
        // =====================================================================
        if (current_state_ != PositionStateMachine::State::CASH_ONLY) {
            bars_held_++;
            log_system("📊 Position holding duration: " + std::to_string(bars_held_) + " bars");
        }

        // =====================================================================
        // STEP 4: Periodic Position Reconciliation (NEW - P0-3)
        // =====================================================================
        if (bar_count_ % 60 == 0) {  // Every 60 bars (60 minutes)
            try {
                auto broker_positions = get_broker_positions();
                position_book_.reconcile_with_broker(broker_positions);
            } catch (const PositionReconciliationError& e) {
                log_error("[" + timestamp + "] RECONCILIATION FAILED: " +
                         std::string(e.what()));
                log_error("[" + timestamp + "] Initiating emergency flatten");
                liquidate_all_positions();
                throw;  // Exit for supervisor restart
            }
        }

        // =====================================================================
        // STEP 5: Check End-of-Day Liquidation (IDEMPOTENT)
        // =====================================================================
        std::string today_et = timestamp.substr(0, 10);  // Extract YYYY-MM-DD from timestamp

        // Check if today is a trading day
        if (!nyse_calendar_.is_trading_day(today_et)) {
            log_system("⏸️  Holiday/Weekend - no trading (learning continues)");
            return;
        }

        // Idempotent EOD check: only liquidate once per trading day
        if (is_end_of_day_liquidation_time() && !eod_state_.is_eod_complete(today_et)) {
            log_system("🔔 END OF DAY - Liquidation window active");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("✓ EOD liquidation complete for " + today_et);
            return;
        }

        // =====================================================================
        // STEP 5.5: Mid-Day Optimization at 16:05 PM ET (NEW)
        // =====================================================================
        // Reset optimization flag for new trading day
        if (midday_optimization_date_ != today_et) {
            midday_optimization_done_ = false;
            midday_optimization_date_ = today_et;
            todays_bars_.clear();  // Clear today's bars for new day
        }

        // Collect ALL bars during regular hours (9:30-16:00) for optimization
        if (is_regular_hours()) {
            todays_bars_.push_back(bar);

            // Check if it's 15:15 PM ET and optimization hasn't been done yet
            if (et_time_.is_midday_optimization_time() && !midday_optimization_done_) {
                log_system("🔔 MID-DAY OPTIMIZATION TIME (15:15 PM ET / 3:15pm)");

                // Liquidate all positions before optimization
                log_system("Liquidating all positions before optimization...");
                liquidate_all_positions();
                log_system("✓ Positions liquidated - going 100% cash");

                // Run optimization
                run_midday_optimization();

                // Mark as done
                midday_optimization_done_ = true;

                // Skip trading for this bar (optimization takes time)
                return;
            }
        }

        // =====================================================================
        // STEP 6: Trading Hours Gate (NEW - only trade during RTH)
        // =====================================================================
        if (!is_regular_hours()) {
            log_system("⏰ After-hours - learning only, no trading");
            return;  // Learning continues, but no trading
        }

        log_system("🕐 Regular Trading Hours - processing for signals and trades");

        // =====================================================================
        // STEP 7: Generate Signal and Trade (RTH only)
        // =====================================================================
        log_system("🧠 Generating signal from strategy...");
        auto signal = generate_signal(bar);

        // Log signal with detailed info
        log_system("📈 SIGNAL GENERATED:");
        log_system("  Prediction: " + signal.prediction);
        log_system("  Probability: " + std::to_string(signal.probability));
        log_system("  Confidence: " + std::to_string(signal.confidence));
        log_system("  Strategy Ready: " + std::string(strategy_.is_ready() ? "YES" : "NO"));

        log_signal(bar, signal);

        // Make trading decision
        log_system("🎯 Evaluating trading decision...");
        auto decision = make_decision(signal, bar);

        // Enhanced decision logging with detailed explanation
        log_enhanced_decision(signal, decision);
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
        } else {
            log_system("⏸️  NO TRADE: " + decision.reason);
        }

        // Log current portfolio state
        log_portfolio_state();
    }

    struct Signal {
        double probability;
        double confidence;
        std::string prediction;  // "LONG", "SHORT", "NEUTRAL"
        double prob_1bar;
        double prob_5bar;
        double prob_10bar;
    };

    Signal generate_signal(const Bar& bar) {
        // Call OnlineEnsemble strategy to generate real signal
        auto strategy_signal = strategy_.generate_signal(bar);

        // DEBUG: Check why we're getting 0.5
        if (strategy_signal.probability == 0.5) {
            std::string reason = "unknown";
            if (strategy_signal.metadata.count("skip_reason")) {
                reason = strategy_signal.metadata.at("skip_reason");
            }
            std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
        }

        Signal signal;
        signal.probability = strategy_signal.probability;
        signal.confidence = strategy_signal.confidence;  // Use confidence from strategy

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Use same probability for all horizons (OnlineEnsemble provides single probability)
        signal.prob_1bar = strategy_signal.probability;
        signal.prob_5bar = strategy_signal.probability;
        signal.prob_10bar = strategy_signal.probability;

        return signal;
    }

    struct Decision {
        bool should_trade;
        PositionStateMachine::State target_state;
        std::string reason;
        double current_equity;
        double position_pnl_pct;
        bool profit_target_hit;
        bool stop_loss_hit;
        bool min_hold_violated;
    };

    Decision make_decision(const Signal& signal, const Bar& bar) {
        Decision decision;
        decision.should_trade = false;

        // Get current portfolio state
        auto account = alpaca_.get_account();
        if (!account) {
            decision.reason = "Failed to get account info";
            return decision;
        }

        decision.current_equity = account->portfolio_value;
        decision.position_pnl_pct = (decision.current_equity - entry_equity_) / entry_equity_;

        // Check profit target / stop loss
        decision.profit_target_hit = (decision.position_pnl_pct >= PROFIT_TARGET &&
                                      current_state_ != PositionStateMachine::State::CASH_ONLY);
        decision.stop_loss_hit = (decision.position_pnl_pct <= STOP_LOSS &&
                                  current_state_ != PositionStateMachine::State::CASH_ONLY);

        // Check minimum hold period
        decision.min_hold_violated = (bars_held_ < MIN_HOLD_BARS);

        // Force exit to cash if profit/stop hit
        if (decision.profit_target_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "PROFIT_TARGET (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        if (decision.stop_loss_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "STOP_LOSS (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        // Map signal probability to PSM state (v1.0 asymmetric thresholds)
        PositionStateMachine::State target_state;

        if (signal.probability >= 0.68) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Maps to SPXL
        } else if (signal.probability >= 0.60) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Mixed
        } else if (signal.probability >= 0.55) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Maps to SPY
        } else if (signal.probability >= 0.49) {
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= 0.45) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Maps to SH
        } else if (signal.probability >= 0.35) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Mixed
        } else if (signal.probability < 0.32) {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Maps to SDS
        } else {
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        decision.target_state = target_state;

        // Check if state transition needed
        if (target_state != current_state_) {
            // Check minimum hold period
            if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
                decision.should_trade = false;
                decision.reason = "MIN_HOLD_PERIOD (held " + std::to_string(bars_held_) + " bars)";
            } else {
                decision.should_trade = true;
                decision.reason = "STATE_TRANSITION (prob=" + std::to_string(signal.probability) + ")";
            }
        } else {
            decision.should_trade = false;
            decision.reason = "NO_CHANGE";
        }

        return decision;
    }

    void liquidate_all_positions() {
        log_system("Closing all positions for end of day...");

        if (alpaca_.close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = alpaca_.get_account();
            if (account) {
                log_system("Final portfolio value: $" + std::to_string(account->portfolio_value));
                entry_equity_ = account->portfolio_value;
            }
        } else {
            log_error("Failed to close all positions");
        }

        log_portfolio_state();
    }

    void execute_transition(const Decision& decision) {
        log_system("");
        log_system("🚀 *** EXECUTING TRADE ***");
        log_system("  Current State: " + psm_.state_to_string(current_state_));
        log_system("  Target State: " + psm_.state_to_string(decision.target_state));
        log_system("  Reason: " + decision.reason);
        log_system("");

        // Step 1: Close all current positions
        log_system("📤 Step 1: Closing current positions...");
        if (!alpaca_.close_all_positions()) {
            log_error("❌ Failed to close positions - aborting transition");
            return;
        }
        log_system("✓ All positions closed");

        // Wait a moment for orders to settle
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Step 2: Get current account info
        log_system("💰 Step 2: Fetching account balance from Alpaca...");
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("❌ Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        double portfolio_value = account->portfolio_value;
        log_system("✓ Account Status:");
        log_system("  Cash: $" + std::to_string(available_capital));
        log_system("  Portfolio Value: $" + std::to_string(portfolio_value));
        log_system("  Buying Power: $" + std::to_string(account->buying_power));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // CRITICAL: If target is not CASH_ONLY but we got empty positions, something is wrong
        bool position_entry_failed = false;
        if (target_positions.empty() && decision.target_state != PositionStateMachine::State::CASH_ONLY) {
            log_error("❌ CRITICAL: Target state is " + psm_.state_to_string(decision.target_state) +
                     " but failed to calculate positions (likely price fetch failure)");
            log_error("   Staying in CASH_ONLY for safety");
            position_entry_failed = true;
        }

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("");
            log_system("📥 Step 3: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  🔵 Sending BUY order to Alpaca:");
                    log_system("     Symbol: " + symbol);
                    log_system("     Quantity: " + std::to_string(quantity) + " shares");

                    auto order = alpaca_.place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order Confirmed:");
                        log_system("     Order ID: " + order->order_id);
                        log_system("     Status: " + order->status);
                        log_trade(*order);
                    } else {
                        log_error("  ❌ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("💵 Target state is CASH_ONLY - no positions to open");
        }

        // Update state - CRITICAL FIX: Only update to target state if we successfully entered positions
        // or if target was CASH_ONLY
        if (position_entry_failed) {
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            log_system("⚠️  State forced to CASH_ONLY due to position entry failure");
        } else {
            current_state_ = decision.target_state;
        }
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        // Final account status
        log_system("");
        log_system("✓ Transition Complete!");
        log_system("  New State: " + psm_.state_to_string(current_state_));
        log_system("  Entry Equity: $" + std::to_string(entry_equity_));
        log_system("");
    }

    // Calculate position allocations based on PSM state
    std::map<std::string, double> calculate_target_allocations(
        PositionStateMachine::State state, double capital) {

        std::map<std::string, double> allocations;

        // Map PSM states to SPY instrument allocations
        switch (state) {
            case PositionStateMachine::State::TQQQ_ONLY:
                // 3x bull → SPXL only
                allocations["SPXL"] = capital;
                break;

            case PositionStateMachine::State::QQQ_TQQQ:
                // Blended long → SPY (50%) + SPXL (50%)
                allocations["SPY"] = capital * 0.5;
                allocations["SPXL"] = capital * 0.5;
                break;

            case PositionStateMachine::State::QQQ_ONLY:
                // 1x base → SPY only
                allocations["SPY"] = capital;
                break;

            case PositionStateMachine::State::CASH_ONLY:
                // No positions
                break;

            case PositionStateMachine::State::PSQ_ONLY:
                // -1x bear → SH only
                allocations["SH"] = capital;
                break;

            case PositionStateMachine::State::PSQ_SQQQ:
                // Blended short → SH (50%) + SDS (50%)
                allocations["SH"] = capital * 0.5;
                allocations["SDS"] = capital * 0.5;
                break;

            case PositionStateMachine::State::SQQQ_ONLY:
                // -2x bear → SDS only
                allocations["SDS"] = capital;
                break;

            default:
                break;
        }

        // Convert dollar allocations to share quantities
        std::map<std::string, double> quantities;
        for (const auto& [symbol, dollar_amount] : allocations) {
            // Get current price from recent bars
            auto bars = polygon_.get_recent_bars(symbol, 1);
            if (!bars.empty() && bars[0].close > 0) {
                double shares = std::floor(dollar_amount / bars[0].close);
                if (shares > 0) {
                    quantities[symbol] = shares;
                }
            }
        }

        return quantities;
    }

    void log_trade(const AlpacaClient::Order& order) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;

        log_trades_ << j.dump() << std::endl;
        log_trades_.flush();
    }

    void log_signal(const Bar& bar, const Signal& signal) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_timestamp_ms"] = bar.timestamp_ms;
        j["probability"] = signal.probability;
        j["confidence"] = signal.confidence;
        j["prediction"] = signal.prediction;
        j["prob_1bar"] = signal.prob_1bar;
        j["prob_5bar"] = signal.prob_5bar;
        j["prob_10bar"] = signal.prob_10bar;

        log_signals_ << j.dump() << std::endl;
        log_signals_.flush();
    }

    void log_enhanced_decision(const Signal& signal, const Decision& decision) {
        log_system("");
        log_system("╔═══════════════════════════════════════════════════════════════");
        log_system("║ 📋 DECISION ANALYSIS");
        log_system("╠═══════════════════════════════════════════════════════════════");

        // Current state
        log_system("║ Current State: " + psm_.state_to_string(current_state_));
        log_system("║   - Bars Held: " + std::to_string(bars_held_) + " bars");
        log_system("║   - Min Hold: " + std::to_string(MIN_HOLD_BARS) + " bars required");
        log_system("║   - Position P&L: " + std::to_string(decision.position_pnl_pct * 100) + "%");
        log_system("║   - Current Equity: $" + std::to_string(decision.current_equity));
        log_system("║");

        // Signal analysis
        log_system("║ Signal Input:");
        log_system("║   - Probability: " + std::to_string(signal.probability));
        log_system("║   - Prediction: " + signal.prediction);
        log_system("║   - Confidence: " + std::to_string(signal.confidence));
        log_system("║");

        // Target state mapping
        log_system("║ PSM Threshold Mapping:");
        if (signal.probability >= 0.68) {
            log_system("║   ✓ prob >= 0.68 → BULL_3X_ONLY (SPXL)");
        } else if (signal.probability >= 0.60) {
            log_system("║   ✓ 0.60 <= prob < 0.68 → BASE_BULL_3X (SPY+SPXL)");
        } else if (signal.probability >= 0.55) {
            log_system("║   ✓ 0.55 <= prob < 0.60 → BASE_ONLY (SPY)");
        } else if (signal.probability >= 0.49) {
            log_system("║   ✓ 0.49 <= prob < 0.55 → CASH_ONLY");
        } else if (signal.probability >= 0.45) {
            log_system("║   ✓ 0.45 <= prob < 0.49 → BEAR_1X_ONLY (SH)");
        } else if (signal.probability >= 0.35) {
            log_system("║   ✓ 0.35 <= prob < 0.45 → BEAR_1X_NX (SH+SDS)");
        } else {
            log_system("║   ✓ prob < 0.35 → BEAR_NX_ONLY (SDS)");
        }
        log_system("║   → Target State: " + psm_.state_to_string(decision.target_state));
        log_system("║");

        // Decision logic
        log_system("║ Decision Logic:");
        if (decision.profit_target_hit) {
            log_system("║   🎯 PROFIT TARGET HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.stop_loss_hit) {
            log_system("║   🛑 STOP LOSS HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.target_state == current_state_) {
            log_system("║   ✓ Target matches current state");
            log_system("║   → NO CHANGE (hold position)");
        } else if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
            log_system("║   ⏳ MIN HOLD PERIOD VIOLATED");
            log_system("║      - Currently held: " + std::to_string(bars_held_) + " bars");
            log_system("║      - Required: " + std::to_string(MIN_HOLD_BARS) + " bars");
            log_system("║      - Remaining: " + std::to_string(MIN_HOLD_BARS - bars_held_) + " bars");
            log_system("║   → BLOCKED (must wait)");
        } else {
            log_system("║   ✓ State transition approved");
            log_system("║      - Target differs from current");
            log_system("║      - Min hold satisfied or in CASH");
            log_system("║   → EXECUTE TRANSITION");
        }
        log_system("║");

        // Final decision
        if (decision.should_trade) {
            log_system("║ ✅ FINAL DECISION: TRADE");
            log_system("║    Transition: " + psm_.state_to_string(current_state_) +
                      " → " + psm_.state_to_string(decision.target_state));
        } else {
            log_system("║ ⏸️  FINAL DECISION: NO TRADE");
        }
        log_system("║    Reason: " + decision.reason);
        log_system("╚═══════════════════════════════════════════════════════════════");
        log_system("");
    }

    void log_decision(const Decision& decision) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["should_trade"] = decision.should_trade;
        j["current_state"] = psm_.state_to_string(current_state_);
        j["target_state"] = psm_.state_to_string(decision.target_state);
        j["reason"] = decision.reason;
        j["current_equity"] = decision.current_equity;
        j["position_pnl_pct"] = decision.position_pnl_pct;
        j["bars_held"] = bars_held_;

        log_decisions_ << j.dump() << std::endl;
        log_decisions_.flush();
    }

    void log_portfolio_state() {
        auto account = alpaca_.get_account();
        if (!account) return;

        auto positions = alpaca_.get_positions();

        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["cash"] = account->cash;
        j["buying_power"] = account->buying_power;
        j["portfolio_value"] = account->portfolio_value;
        j["equity"] = account->equity;
        j["total_return"] = account->portfolio_value - 100000.0;
        j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;

        nlohmann::json positions_json = nlohmann::json::array();
        for (const auto& pos : positions) {
            nlohmann::json p;
            p["symbol"] = pos.symbol;
            p["quantity"] = pos.quantity;
            p["avg_entry_price"] = pos.avg_entry_price;
            p["current_price"] = pos.current_price;
            p["market_value"] = pos.market_value;
            p["unrealized_pl"] = pos.unrealized_pl;
            p["unrealized_pl_pct"] = pos.unrealized_pl_pct;
            positions_json.push_back(p);
        }
        j["positions"] = positions_json;

        log_positions_ << j.dump() << std::endl;
        log_positions_.flush();
    }

    // NEW: Convert Alpaca positions to BrokerPosition format for reconciliation
    std::vector<BrokerPosition> get_broker_positions() {
        auto alpaca_positions = alpaca_.get_positions();
        std::vector<BrokerPosition> broker_positions;

        for (const auto& pos : alpaca_positions) {
            BrokerPosition bp;
            bp.symbol = pos.symbol;
            bp.qty = static_cast<int64_t>(pos.quantity);
            bp.avg_entry_price = pos.avg_entry_price;
            bp.current_price = pos.current_price;
            bp.unrealized_pnl = pos.unrealized_pl;
            broker_positions.push_back(bp);
        }

        return broker_positions;
    }

    /**
     * Save comprehensive warmup data: historical bars + all of today's bars
     * This ensures optimization uses ALL available data up to current moment
     */
    std::string save_comprehensive_warmup_to_csv() {
        auto et_tm = et_time_.get_current_et_tm();
        std::string today = format_et_date(et_tm);

        std::string filename = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/comprehensive_warmup_" +
                               today + ".csv";

        std::ofstream csv(filename);
        if (!csv.is_open()) {
            log_error("Failed to open file for writing: " + filename);
            return "";
        }

        // Write CSV header
        csv << "timestamp,open,high,low,close,volume\n";

        log_system("Building comprehensive warmup data...");

        // Step 1: Load historical warmup bars (20 blocks = 7800 bars + 64 feature bars)
        std::string warmup_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_warmup_latest.csv";
        std::ifstream warmup_csv(warmup_file);

        if (!warmup_csv.is_open()) {
            log_error("Failed to open historical warmup file: " + warmup_file);
            log_error("Falling back to today's bars only");
        } else {
            std::string line;
            std::getline(warmup_csv, line);  // Skip header

            int historical_count = 0;
            while (std::getline(warmup_csv, line)) {
                // Filter: only include bars BEFORE today (to avoid duplicates)
                if (line.find(today) == std::string::npos) {
                    csv << line << "\n";
                    historical_count++;
                }
            }
            warmup_csv.close();

            log_system("  ✓ Historical bars: " + std::to_string(historical_count));
        }

        // Step 2: Append all of today's bars collected so far
        for (const auto& bar : todays_bars_) {
            csv << bar.timestamp_ms << ","
                << bar.open << ","
                << bar.high << ","
                << bar.low << ","
                << bar.close << ","
                << bar.volume << "\n";
        }

        csv.close();

        log_system("  ✓ Today's bars: " + std::to_string(todays_bars_.size()));
        log_system("✓ Comprehensive warmup saved: " + filename);

        return filename;
    }

    /**
     * Load optimized parameters from midday_selected_params.json
     */
    struct OptimizedParams {
        bool success{false};
        std::string source;
        double buy_threshold{0.55};
        double sell_threshold{0.45};
        double ewrls_lambda{0.995};
        double expected_mrb{0.0};
    };

    OptimizedParams load_optimized_parameters() {
        OptimizedParams params;

        std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
        std::ifstream file(json_file);

        if (!file.is_open()) {
            log_error("Failed to open optimization results: " + json_file);
            return params;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            params.success = true;
            params.source = j.value("source", "baseline");
            params.buy_threshold = j.value("buy_threshold", 0.55);
            params.sell_threshold = j.value("sell_threshold", 0.45);
            params.ewrls_lambda = j.value("ewrls_lambda", 0.995);
            params.expected_mrb = j.value("expected_mrb", 0.0);

            log_system("✓ Loaded optimized parameters from: " + json_file);
            log_system("  Source: " + params.source);
            log_system("  buy_threshold: " + std::to_string(params.buy_threshold));
            log_system("  sell_threshold: " + std::to_string(params.sell_threshold));
            log_system("  ewrls_lambda: " + std::to_string(params.ewrls_lambda));
            log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");

        } catch (const std::exception& e) {
            log_error("Failed to parse optimization results: " + std::string(e.what()));
            params.success = false;
        }

        return params;
    }

    /**
     * Update strategy configuration with new parameters
     */
    void update_strategy_parameters(const OptimizedParams& params) {
        log_system("📊 Updating strategy parameters...");

        // Create new config with optimized parameters
        auto config = create_v1_config();
        config.buy_threshold = params.buy_threshold;
        config.sell_threshold = params.sell_threshold;
        config.ewrls_lambda = params.ewrls_lambda;

        // Update strategy
        strategy_.update_config(config);

        log_system("✓ Strategy parameters updated for afternoon session");
    }

    /**
     * Run mid-day optimization at 15:15 PM ET (3:15pm)
     */
    void run_midday_optimization() {
        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("🔄 MID-DAY OPTIMIZATION TRIGGERED (15:15 PM ET / 3:15pm)");
        log_system("════════════════════════════════════════════════");
        log_system("");

        // Step 1: Save comprehensive warmup data (historical + today's bars)
        log_system("Step 1: Saving comprehensive warmup data to CSV...");
        std::string warmup_data_file = save_comprehensive_warmup_to_csv();
        if (warmup_data_file.empty()) {
            log_error("Failed to save warmup data - continuing with baseline parameters");
            return;
        }

        // Step 2: Call optimization script
        log_system("Step 2: Running Optuna optimization script...");
        log_system("  (This will take ~5 minutes for 50 trials)");

        std::string cmd = "/Volumes/ExternalSSD/Dev/C++/online_trader/tools/midday_optuna_relaunch.sh \"" +
                          warmup_data_file + "\" 2>&1 | tail -30";

        int exit_code = system(cmd.c_str());

        if (exit_code != 0) {
            log_error("Optimization script failed (exit code: " + std::to_string(exit_code) + ")");
            log_error("Continuing with baseline parameters");
            return;
        }

        log_system("✓ Optimization script completed");

        // Step 3: Load optimized parameters
        log_system("Step 3: Loading optimized parameters...");
        auto params = load_optimized_parameters();

        if (!params.success) {
            log_error("Failed to load optimized parameters - continuing with baseline");
            return;
        }

        // Step 4: Update strategy configuration
        log_system("Step 4: Updating strategy configuration...");
        update_strategy_parameters(params);

        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("✅ MID-DAY OPTIMIZATION COMPLETE");
        log_system("════════════════════════════════════════════════");
        log_system("  Parameters: " + params.source);
        log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");
        log_system("  Resuming trading at 14:46 PM ET");
        log_system("════════════════════════════════════════════════");
        log_system("");
    }
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Read Alpaca credentials from environment (config.env)
    const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
    const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

    if (!alpaca_key_env || !alpaca_secret_env) {
        std::cerr << "ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set" << std::endl;
        std::cerr << "Run: source config.env" << std::endl;
        return 1;
    }

    const std::string ALPACA_KEY = alpaca_key_env;
    const std::string ALPACA_SECRET = alpaca_secret_env;

    // Polygon API key from config.env
    const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
    // Use Alpaca's IEX WebSocket for real-time market data (FREE tier)
    // IEX provides ~3% of market volume, sufficient for paper trading
    // Upgrade to SIP ($49/mo) for 100% market coverage in production
    const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";

    // Polygon key (kept for future use, currently using Alpaca IEX)
    const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

    // NOTE: Only switch to LIVE credentials after paper trading success is confirmed!
    // LIVE credentials would be ALPACA_LIVE_API_KEY / ALPACA_LIVE_SECRET_KEY

    // Log directory
    std::string log_dir = "logs/live_trading";

    LiveTrader trader(ALPACA_KEY, ALPACA_SECRET, ALPACA_MARKET_DATA_URL, POLYGON_KEY, log_dir);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade\n\n";
    std::cout << "Run OnlineTrader v1.0 with paper account\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Data: Real-time via Alpaca (upgradeable to Polygon WebSocket)\n";
    std::cout << "  Account: Paper trading (PK3NCBT07OJZJULDJR5V)\n\n";
    std::cout << "Logs: logs/live_trading/\n";
    std::cout << "  - system_*.log: Human-readable events\n";
    std::cout << "  - signals_*.jsonl: Every prediction\n";
    std::cout << "  - decisions_*.jsonl: Trading decisions\n";
    std::cout << "  - trades_*.jsonl: Order executions\n";
    std::cout << "  - positions_*.jsonl: Portfolio snapshots\n\n";
    std::cout << "Example:\n";
    std::cout << "  sentio_cli live-trade\n";
}

} // namespace cli
} // namespace sentio
```

---

## File: `include/live/alpaca_client.hpp`

**Path**: `include/live/alpaca_client.hpp`

```cpp
#ifndef SENTIO_ALPACA_CLIENT_HPP
#define SENTIO_ALPACA_CLIENT_HPP

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace sentio {

/**
 * Alpaca Paper Trading API Client
 *
 * REST API client for Alpaca Markets paper trading.
 * Supports account info, positions, and order execution.
 */
class AlpacaClient {
public:
    struct Position {
        std::string symbol;
        double quantity;           // Positive for long, negative for short
        double avg_entry_price;
        double current_price;
        double market_value;
        double unrealized_pl;
        double unrealized_pl_pct;
    };

    struct AccountInfo {
        std::string account_number;
        double buying_power;
        double cash;
        double portfolio_value;
        double equity;
        double last_equity;
        bool pattern_day_trader;
        bool trading_blocked;
        bool account_blocked;
    };

    struct Order {
        std::string symbol;
        double quantity;
        std::string side;          // "buy" or "sell"
        std::string type;          // "market", "limit", etc.
        std::string time_in_force; // "day", "gtc", "ioc", "fok"
        std::optional<double> limit_price;

        // Response fields
        std::string order_id;
        std::string status;        // "new", "filled", "canceled", etc.
        double filled_qty;
        double filled_avg_price;
    };

    /**
     * Constructor
     * @param api_key Alpaca API key (APCA-API-KEY-ID)
     * @param secret_key Alpaca secret key (APCA-API-SECRET-KEY)
     * @param paper_trading Use paper trading endpoint (default: true)
     */
    AlpacaClient(const std::string& api_key,
                 const std::string& secret_key,
                 bool paper_trading = true);

    ~AlpacaClient() = default;

    /**
     * Get account information
     * GET /v2/account
     */
    std::optional<AccountInfo> get_account();

    /**
     * Get all open positions
     * GET /v2/positions
     */
    std::vector<Position> get_positions();

    /**
     * Get position for specific symbol
     * GET /v2/positions/{symbol}
     */
    std::optional<Position> get_position(const std::string& symbol);

    /**
     * Place a market order
     * POST /v2/orders
     *
     * @param symbol Stock symbol (e.g., "QQQ", "TQQQ")
     * @param quantity Number of shares (positive for buy, negative for sell)
     * @param time_in_force "day" or "gtc" (good till canceled)
     * @return Order details if successful
     */
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc");

    /**
     * Close position for a symbol
     * DELETE /v2/positions/{symbol}
     */
    bool close_position(const std::string& symbol);

    /**
     * Close all positions
     * DELETE /v2/positions
     */
    bool close_all_positions();

    /**
     * Get order by ID
     * GET /v2/orders/{order_id}
     */
    std::optional<Order> get_order(const std::string& order_id);

    /**
     * Cancel order by ID
     * DELETE /v2/orders/{order_id}
     */
    bool cancel_order(const std::string& order_id);

    /**
     * Get all open orders
     * GET /v2/orders?status=open
     */
    std::vector<Order> get_open_orders();

    /**
     * Cancel all open orders (idempotent)
     * DELETE /v2/orders
     */
    bool cancel_all_orders();

    /**
     * Check if market is open
     * GET /v2/clock
     */
    bool is_market_open();

private:
    std::string api_key_;
    std::string secret_key_;
    std::string base_url_;

    /**
     * Make HTTP GET request
     */
    std::string http_get(const std::string& endpoint);

    /**
     * Make HTTP POST request with JSON body
     */
    std::string http_post(const std::string& endpoint, const std::string& json_body);

    /**
     * Make HTTP DELETE request
     */
    std::string http_delete(const std::string& endpoint);

    /**
     * Add authentication headers
     */
    std::map<std::string, std::string> get_headers();

    /**
     * Parse JSON response
     */
    static std::optional<AccountInfo> parse_account_json(const std::string& json);
    static std::vector<Position> parse_positions_json(const std::string& json);
    static std::optional<Position> parse_position_json(const std::string& json);
    static std::optional<Order> parse_order_json(const std::string& json);
};

} // namespace sentio

#endif // SENTIO_ALPACA_CLIENT_HPP
```

---

## File: `src/live/alpaca_client.cpp`

**Path**: `src/live/alpaca_client.cpp`

```cpp
#include "live/alpaca_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace sentio {

// Callback for libcurl to capture response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

AlpacaClient::AlpacaClient(const std::string& api_key,
                           const std::string& secret_key,
                           bool paper_trading)
    : api_key_(api_key)
    , secret_key_(secret_key)
{
    if (paper_trading) {
        base_url_ = "https://paper-api.alpaca.markets/v2";
    } else {
        base_url_ = "https://api.alpaca.markets/v2";
    }
}

std::map<std::string, std::string> AlpacaClient::get_headers() {
    return {
        {"APCA-API-KEY-ID", api_key_},
        {"APCA-API-SECRET-KEY", secret_key_},
        {"Content-Type", "application/json"}
    };
}

std::string AlpacaClient::http_get(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP GET failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_post(const std::string& endpoint, const std::string& json_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP POST failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_delete(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP DELETE failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::optional<AlpacaClient::AccountInfo> AlpacaClient::get_account() {
    try {
        std::string response = http_get("/account");
        return parse_account_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting account: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::get_positions() {
    try {
        std::string response = http_get("/positions");
        return parse_positions_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting positions: " << e.what() << std::endl;
        return {};
    }
}

std::optional<AlpacaClient::Position> AlpacaClient::get_position(const std::string& symbol) {
    try {
        std::string response = http_get("/positions/" + symbol);
        return parse_position_json(response);
    } catch (const std::exception& e) {
        // Position not found is not an error
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::place_market_order(const std::string& symbol,
                                                                    double quantity,
                                                                    const std::string& time_in_force) {
    try {
        json order_json;
        order_json["symbol"] = symbol;
        order_json["qty"] = std::abs(quantity);
        order_json["side"] = (quantity > 0) ? "buy" : "sell";
        order_json["type"] = "market";
        order_json["time_in_force"] = time_in_force;

        std::string json_body = order_json.dump();
        std::string response = http_post("/orders", json_body);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error placing order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::close_position(const std::string& symbol) {
    try {
        http_delete("/positions/" + symbol);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing position: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::close_all_positions() {
    try {
        http_delete("/positions");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing all positions: " << e.what() << std::endl;
        return false;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::get_order(const std::string& order_id) {
    try {
        std::string response = http_get("/orders/" + order_id);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::cancel_order(const std::string& order_id) {
    try {
        http_delete("/orders/" + order_id);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error canceling order: " << e.what() << std::endl;
        return false;
    }
}

std::vector<AlpacaClient::Order> AlpacaClient::get_open_orders() {
    try {
        std::string response = http_get("/orders?status=open");
        json orders_json = json::parse(response);

        std::vector<Order> orders;
        for (const auto& order_json : orders_json) {
            Order order;
            order.order_id = order_json.value("id", "");
            order.symbol = order_json.value("symbol", "");
            order.quantity = order_json.value("qty", 0.0);
            order.side = order_json.value("side", "");
            order.type = order_json.value("type", "");
            order.time_in_force = order_json.value("time_in_force", "");
            order.status = order_json.value("status", "");
            order.filled_qty = order_json.value("filled_qty", 0.0);
            order.filled_avg_price = order_json.value("filled_avg_price", 0.0);

            if (order_json.contains("limit_price") && !order_json["limit_price"].is_null()) {
                order.limit_price = order_json["limit_price"].get<double>();
            }

            orders.push_back(order);
        }

        return orders;
    } catch (const std::exception& e) {
        std::cerr << "Error getting open orders: " << e.what() << std::endl;
        return {};
    }
}

bool AlpacaClient::cancel_all_orders() {
    try {
        http_delete("/orders");
        std::cout << "[AlpacaClient] All orders cancelled" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error canceling all orders: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::is_market_open() {
    try {
        std::string response = http_get("/clock");
        json clock = json::parse(response);
        return clock["is_open"].get<bool>();
    } catch (const std::exception& e) {
        std::cerr << "Error checking market status: " << e.what() << std::endl;
        return false;
    }
}

// JSON parsing helpers

std::optional<AlpacaClient::AccountInfo> AlpacaClient::parse_account_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        AccountInfo info;
        info.account_number = j["account_number"].get<std::string>();
        info.buying_power = std::stod(j["buying_power"].get<std::string>());
        info.cash = std::stod(j["cash"].get<std::string>());
        info.portfolio_value = std::stod(j["portfolio_value"].get<std::string>());
        info.equity = std::stod(j["equity"].get<std::string>());
        info.last_equity = std::stod(j["last_equity"].get<std::string>());
        info.pattern_day_trader = j.value("pattern_day_trader", false);
        info.trading_blocked = j.value("trading_blocked", false);
        info.account_blocked = j.value("account_blocked", false);
        return info;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing account JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::parse_positions_json(const std::string& json_str) {
    std::vector<Position> positions;
    try {
        json j = json::parse(json_str);
        for (const auto& item : j) {
            Position pos;
            pos.symbol = item["symbol"].get<std::string>();
            pos.quantity = std::stod(item["qty"].get<std::string>());
            pos.avg_entry_price = std::stod(item["avg_entry_price"].get<std::string>());
            pos.current_price = std::stod(item["current_price"].get<std::string>());
            pos.market_value = std::stod(item["market_value"].get<std::string>());
            pos.unrealized_pl = std::stod(item["unrealized_pl"].get<std::string>());
            pos.unrealized_pl_pct = std::stod(item["unrealized_plpc"].get<std::string>());
            positions.push_back(pos);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing positions JSON: " << e.what() << std::endl;
    }
    return positions;
}

std::optional<AlpacaClient::Position> AlpacaClient::parse_position_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Position pos;
        pos.symbol = j["symbol"].get<std::string>();
        pos.quantity = std::stod(j["qty"].get<std::string>());
        pos.avg_entry_price = std::stod(j["avg_entry_price"].get<std::string>());
        pos.current_price = std::stod(j["current_price"].get<std::string>());
        pos.market_value = std::stod(j["market_value"].get<std::string>());
        pos.unrealized_pl = std::stod(j["unrealized_pl"].get<std::string>());
        pos.unrealized_pl_pct = std::stod(j["unrealized_plpc"].get<std::string>());
        return pos;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing position JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::parse_order_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Order order;
        order.order_id = j["id"].get<std::string>();
        order.symbol = j["symbol"].get<std::string>();
        order.quantity = std::stod(j["qty"].get<std::string>());
        order.side = j["side"].get<std::string>();
        order.type = j["type"].get<std::string>();
        order.time_in_force = j["time_in_force"].get<std::string>();
        order.status = j["status"].get<std::string>();
        order.filled_qty = std::stod(j["filled_qty"].get<std::string>());
        if (!j["filled_avg_price"].is_null()) {
            order.filled_avg_price = std::stod(j["filled_avg_price"].get<std::string>());
        } else {
            order.filled_avg_price = 0.0;
        }
        return order;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing order JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace sentio
```

---

## File: `include/live/polygon_client.hpp`

**Path**: `include/live/polygon_client.hpp`

```cpp
#ifndef SENTIO_POLYGON_CLIENT_HPP
#define SENTIO_POLYGON_CLIENT_HPP

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <deque>
#include <mutex>
#include <chrono>
#include <atomic>

namespace sentio {

/**
 * Polygon.io WebSocket Client for Real-Time Market Data
 *
 * Connects to Polygon proxy server and receives 1-minute aggregated bars
 * for SPY, SDS, SPXL, and SH in real-time.
 */
class PolygonClient {
public:
    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Constructor
     * @param proxy_url WebSocket URL for Polygon proxy (e.g., "ws://proxy.example.com:8080")
     * @param auth_key Authentication key for proxy
     */
    PolygonClient(const std::string& proxy_url, const std::string& auth_key);
    ~PolygonClient();

    /**
     * Connect to Polygon proxy and authenticate
     */
    bool connect();

    /**
     * Subscribe to symbols for 1-minute aggregates
     */
    bool subscribe(const std::vector<std::string>& symbols);

    /**
     * Start receiving data (runs in separate thread)
     * @param callback Function called when new bar arrives
     */
    void start(BarCallback callback);

    /**
     * Stop receiving data and disconnect
     */
    void stop();

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const;

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Store a bar in history (public for WebSocket callback access)
     */
    void store_bar(const std::string& symbol, const Bar& bar);

    /**
     * Update last message timestamp (called by WebSocket callback)
     */
    void update_last_message_time();

    /**
     * Check if connection is healthy (received message recently)
     */
    bool is_connection_healthy() const;

    /**
     * Get seconds since last message
     */
    int get_seconds_since_last_message() const;

private:
    std::string proxy_url_;
    std::string auth_key_;
    bool connected_;
    bool running_;

    // Health monitoring
    std::atomic<std::chrono::steady_clock::time_point> last_message_time_;
    static constexpr int HEALTH_CHECK_TIMEOUT_SECONDS = 120;  // 2 minutes

    // Thread-safe storage of recent bars (per symbol)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // WebSocket implementation
    void receive_loop(BarCallback callback);
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_HPP
```

---

## File: `src/live/polygon_websocket_fifo.cpp`

**Path**: `src/live/polygon_websocket_fifo.cpp`

```cpp
// Alpaca IEX WebSocket Client via Python Bridge - Real-time market data
// Reads JSON bars from FIFO pipe written by Python bridge
// Python bridge: tools/alpaca_websocket_bridge.py

#include "live/polygon_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>

using json = nlohmann::json;

namespace sentio {

// FIFO pipe path (must match Python bridge)
constexpr const char* FIFO_PATH = "/tmp/alpaca_bars.fifo";

PolygonClient::PolygonClient(const std::string& proxy_url, const std::string& auth_key)
    : proxy_url_(proxy_url)
    , auth_key_(auth_key)
    , connected_(false)
    , running_(false)
    , last_message_time_(std::chrono::steady_clock::now())
{
}

PolygonClient::~PolygonClient() {
    stop();
}

bool PolygonClient::connect() {
    std::cout << "Connecting to Python WebSocket bridge..." << std::endl;
    std::cout << "Reading from FIFO: " << FIFO_PATH << std::endl;
    connected_ = true;  // Will be verified when first bar arrives
    return true;
}

bool PolygonClient::subscribe(const std::vector<std::string>& symbols) {
    std::cout << "Subscribing to: ";
    for (const auto& s : symbols) std::cout << s << " ";
    std::cout << std::endl;
    return true;
}

void PolygonClient::start(BarCallback callback) {
    if (running_) return;

    running_ = true;
    std::thread fifo_thread([this, callback]() {
        receive_loop(callback);
    });
    fifo_thread.detach();
}

void PolygonClient::stop() {
    running_ = false;
    connected_ = false;
}

void PolygonClient::receive_loop(BarCallback callback) {
    std::cout << "→ FIFO read loop started" << std::endl;

    while (running_) {
        try {
            // Open FIFO for reading
            // NOTE: This blocks until Python bridge opens it for writing
            std::ifstream fifo(FIFO_PATH);
            if (!fifo.is_open()) {
                std::cerr << "❌ Failed to open FIFO: " << FIFO_PATH << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            connected_ = true;
            std::cout << "✓ FIFO opened - reading bars from Python bridge" << std::endl;

            // Read JSON bars line by line
            std::string line;
            while (running_ && std::getline(fifo, line)) {
                if (line.empty()) continue;

                try {
                    json j = json::parse(line);

                    // Extract bar data from Python bridge format
                    Bar bar;
                    bar.timestamp_ms = j["timestamp_ms"];
                    bar.open = j["open"];
                    bar.high = j["high"];
                    bar.low = j["low"];
                    bar.close = j["close"];
                    bar.volume = j["volume"];

                    std::string symbol = j["symbol"];

                    // Update health timestamp
                    update_last_message_time();

                    std::cout << "✓ Bar: " << symbol << " $" << bar.close
                              << " (O:" << bar.open << " H:" << bar.high
                              << " L:" << bar.low << " V:" << bar.volume << ")" << std::endl;

                    // Store bar
                    store_bar(symbol, bar);

                    // Callback (only for SPY to trigger strategy)
                    if (callback && symbol == "SPY") {
                        callback(symbol, bar);
                    }

                } catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON from FIFO: " << e.what() << std::endl;
                    std::cerr << "Line was: " << line.substr(0, 200) << std::endl;
                }
            }

            fifo.close();

            if (!running_) {
                std::cout << "FIFO read loop ended (stop requested)" << std::endl;
                return;
            }

            // FIFO closed (Python bridge restarted?) - reconnect
            std::cerr << "❌ FIFO closed - attempting to reopen in 3s..." << std::endl;
            connected_ = false;
            std::this_thread::sleep_for(std::chrono::seconds(3));

        } catch (const std::exception& e) {
            std::cerr << "❌ FIFO read error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    std::cout << "FIFO read loop terminated" << std::endl;
}

void PolygonClient::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto& history = bars_history_[symbol];
    history.push_back(bar);

    if (history.size() > MAX_BARS_HISTORY) {
        history.pop_front();
    }
}

std::vector<Bar> PolygonClient::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto it = bars_history_.find(symbol);
    if (it == bars_history_.end()) {
        return {};
    }

    const auto& history = it->second;
    size_t start = (history.size() > count) ? (history.size() - count) : 0;

    std::vector<Bar> result;
    for (size_t i = start; i < history.size(); ++i) {
        result.push_back(history[i]);
    }

    return result;
}

bool PolygonClient::is_connected() const {
    return connected_;
}

void PolygonClient::update_last_message_time() {
    last_message_time_.store(std::chrono::steady_clock::now());
}

bool PolygonClient::is_connection_healthy() const {
    auto now = std::chrono::steady_clock::now();
    auto last_msg = last_message_time_.load();
    auto silence_duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_msg
    ).count();

    return silence_duration < HEALTH_CHECK_TIMEOUT_SECONDS;
}

int PolygonClient::get_seconds_since_last_message() const {
    auto now = std::chrono::steady_clock::now();
    auto last_msg = last_message_time_.load();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - last_msg
    ).count();
}

} // namespace sentio
```

---

## File: `include/live/position_book.h`

**Path**: `include/live/position_book.h`

```cpp
#pragma once

#include "common/types.h"
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace sentio {

struct BrokerPosition {
    std::string symbol;
    int64_t qty{0};
    double avg_entry_price{0.0};
    double unrealized_pnl{0.0};
    double current_price{0.0};

    bool is_flat() const { return qty == 0; }
};

struct ExecutionReport {
    std::string order_id;
    std::string client_order_id;
    std::string symbol;
    std::string side;  // "buy" or "sell"
    int64_t filled_qty{0};
    double avg_fill_price{0.0};
    std::string status;  // "filled", "partial_fill", "pending", etc.
    uint64_t timestamp{0};
};

struct ReconcileResult {
    double realized_pnl{0.0};
    int64_t filled_qty{0};
    bool flat{false};
    std::string status;
};

/**
 * @brief Position book that tracks positions and reconciles with broker
 *
 * This class maintains local position state and provides reconciliation
 * against broker truth to detect position drift.
 */
class PositionBook {
public:
    PositionBook() = default;

    /**
     * @brief Update position from execution report
     * @param exec Execution report from broker
     */
    void on_execution(const ExecutionReport& exec);

    /**
     * @brief Get current position for symbol
     * @param symbol Symbol to query
     * @return BrokerPosition (returns flat position if symbol not found)
     */
    BrokerPosition get_position(const std::string& symbol) const;

    /**
     * @brief Reconcile local positions against broker truth
     * @param broker_positions Positions from broker API
     * @throws PositionReconciliationError if drift detected
     */
    void reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions);

    /**
     * @brief Get all non-flat positions
     * @return Map of symbol -> position
     */
    std::map<std::string, BrokerPosition> get_all_positions() const;

    /**
     * @brief Get total realized P&L since timestamp
     * @param since_ts Unix timestamp in microseconds
     * @return Realized P&L in dollars
     */
    double get_realized_pnl_since(uint64_t since_ts) const;

    /**
     * @brief Get total realized P&L today
     * @return Realized P&L in dollars
     */
    double get_total_realized_pnl() const { return total_realized_pnl_; }

    /**
     * @brief Reset daily P&L (call at market open)
     */
    void reset_daily_pnl() { total_realized_pnl_ = 0.0; }

    /**
     * @brief Update current market prices for unrealized P&L calculation
     * @param symbol Symbol
     * @param price Current market price
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * @brief Set position directly (for startup reconciliation)
     * @param symbol Symbol
     * @param qty Quantity
     * @param avg_price Average entry price
     */
    void set_position(const std::string& symbol, int64_t qty, double avg_price);

    /**
     * @brief Check if all positions are flat (for EOD safety)
     * @return true if no positions held
     */
    bool is_flat() const {
        for (const auto& [symbol, pos] : positions_) {
            if (pos.qty != 0) return false;
        }
        return true;
    }

    /**
     * @brief Calculate SHA1 hash of positions (for EOD verification)
     * @return Hex string of sorted positions hash (empty string if flat)
     *
     * Format: sorted by symbol, "SYMBOL:QTY|SYMBOL:QTY|..."
     * Example: "SPY:100|TQQQ:-50" → SHA1 → hex string
     */
    std::string positions_hash() const;

private:
    std::map<std::string, BrokerPosition> positions_;
    std::vector<ExecutionReport> execution_history_;
    double total_realized_pnl_{0.0};

    void update_position_on_fill(const ExecutionReport& exec);
    double calculate_realized_pnl(const BrokerPosition& old_pos, const ExecutionReport& exec);
};

} // namespace sentio
```

---

## File: `src/live/position_book.cpp`

**Path**: `src/live/position_book.cpp`

```cpp
#include "live/position_book.h"
#include "common/exceptions.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace sentio {

void PositionBook::on_execution(const ExecutionReport& exec) {
    execution_history_.push_back(exec);

    if (exec.filled_qty == 0) {
        return;  // No fill, nothing to update
    }

    auto& pos = positions_[exec.symbol];

    // Calculate realized P&L if reducing position
    double realized_pnl = calculate_realized_pnl(pos, exec);
    total_realized_pnl_ += realized_pnl;

    // Update position
    update_position_on_fill(exec);

    // Log update
    std::cout << "[PositionBook] " << exec.symbol
              << " qty=" << pos.qty
              << " avg_px=" << pos.avg_entry_price
              << " realized_pnl=" << realized_pnl << std::endl;
}

void PositionBook::update_position_on_fill(const ExecutionReport& exec) {
    auto& pos = positions_[exec.symbol];

    // Convert side to signed qty
    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    int64_t new_qty = pos.qty + fill_qty;

    if (pos.qty == 0) {
        // Opening new position
        pos.avg_entry_price = exec.avg_fill_price;
    } else if ((pos.qty > 0 && fill_qty > 0) || (pos.qty < 0 && fill_qty < 0)) {
        // Adding to position - update weighted average entry price
        double total_cost = pos.qty * pos.avg_entry_price +
                           fill_qty * exec.avg_fill_price;
        pos.avg_entry_price = total_cost / new_qty;
    }
    // If reducing/reversing, keep old avg_entry_price for P&L calculation

    pos.qty = new_qty;
    pos.symbol = exec.symbol;

    // Reset avg price when flat
    if (pos.qty == 0) {
        pos.avg_entry_price = 0.0;
        pos.unrealized_pnl = 0.0;
    }
}

double PositionBook::calculate_realized_pnl(const BrokerPosition& old_pos,
                                            const ExecutionReport& exec) {
    if (old_pos.qty == 0) {
        return 0.0;  // Opening position, no P&L
    }

    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    // Only calculate P&L if reducing position
    if ((old_pos.qty > 0 && fill_qty >= 0) || (old_pos.qty < 0 && fill_qty <= 0)) {
        return 0.0;  // Adding to position
    }

    // Calculate how many shares we're closing
    int64_t closed_qty = std::min(std::abs(fill_qty), std::abs(old_pos.qty));

    // P&L per share = exit price - entry price
    double pnl_per_share = exec.avg_fill_price - old_pos.avg_entry_price;

    // For short positions, invert the P&L
    if (old_pos.qty < 0) {
        pnl_per_share = -pnl_per_share;
    }

    return closed_qty * pnl_per_share;
}

BrokerPosition PositionBook::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return BrokerPosition{.symbol = symbol};
    }
    return it->second;
}

void PositionBook::update_market_price(const std::string& symbol, double price) {
    auto it = positions_.find(symbol);
    if (it == positions_.end() || it->second.qty == 0) {
        return;  // No position, no unrealized P&L
    }

    auto& pos = it->second;
    pos.current_price = price;

    // Calculate unrealized P&L
    double pnl_per_share = price - pos.avg_entry_price;
    if (pos.qty < 0) {
        pnl_per_share = -pnl_per_share;  // Short position
    }
    pos.unrealized_pnl = std::abs(pos.qty) * pnl_per_share;
}

void PositionBook::reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions) {
    std::cout << "[PositionBook] === Position Reconciliation ===" << std::endl;

    // Build broker position map
    std::map<std::string, BrokerPosition> broker_map;
    for (const auto& bp : broker_positions) {
        broker_map[bp.symbol] = bp;
    }

    // Check for discrepancies
    bool has_drift = false;

    // Check local positions against broker
    for (const auto& [symbol, local_pos] : positions_) {
        if (local_pos.qty == 0) continue;  // Skip flat positions

        auto bit = broker_map.find(symbol);

        if (bit == broker_map.end()) {
            std::cerr << "[PositionBook] DRIFT: Local has " << symbol
                     << " (" << local_pos.qty << "), broker has 0" << std::endl;
            has_drift = true;
        } else {
            const auto& broker_pos = bit->second;
            if (local_pos.qty != broker_pos.qty) {
                std::cerr << "[PositionBook] DRIFT: " << symbol
                         << " local=" << local_pos.qty
                         << " broker=" << broker_pos.qty << std::endl;
                has_drift = true;
            }
        }
    }

    // Check for positions broker has but we don't
    for (const auto& [symbol, broker_pos] : broker_map) {
        if (broker_pos.qty == 0) continue;

        auto lit = positions_.find(symbol);
        if (lit == positions_.end() || lit->second.qty == 0) {
            std::cerr << "[PositionBook] DRIFT: Broker has " << symbol
                     << " (" << broker_pos.qty << "), local has 0" << std::endl;
            has_drift = true;
        }
    }

    if (has_drift) {
        std::cerr << "[PositionBook] === POSITION DRIFT DETECTED ===" << std::endl;
        throw PositionReconciliationError("Position drift detected - local != broker");
    } else {
        std::cout << "[PositionBook] Position reconciliation: OK" << std::endl;
    }
}

double PositionBook::get_realized_pnl_since(uint64_t since_ts) const {
    double pnl = 0.0;
    for (const auto& exec : execution_history_) {
        if (exec.timestamp >= since_ts && exec.status == "filled") {
            // Note: This is simplified. In production, track per-exec P&L
            // For now, return total realized P&L
        }
    }
    return total_realized_pnl_;
}

std::map<std::string, BrokerPosition> PositionBook::get_all_positions() const {
    std::map<std::string, BrokerPosition> result;
    for (const auto& [symbol, pos] : positions_) {
        if (pos.qty != 0) {
            result[symbol] = pos;
        }
    }
    return result;
}

void PositionBook::set_position(const std::string& symbol, int64_t qty, double avg_price) {
    BrokerPosition pos;
    pos.symbol = symbol;
    pos.qty = qty;
    pos.avg_entry_price = avg_price;
    pos.current_price = avg_price;  // Will be updated on next price update
    pos.unrealized_pnl = 0.0;
    positions_[symbol] = pos;
}

std::string PositionBook::positions_hash() const {
    if (is_flat()) {
        return "";  // Empty hash for flat book
    }

    // Build sorted position string
    std::stringstream ss;
    bool first = true;

    // positions_ is already sorted (std::map)
    for (const auto& [symbol, pos] : positions_) {
        if (pos.qty == 0) continue;  // Skip flat positions

        if (!first) ss << "|";
        ss << symbol << ":" << pos.qty;
        first = false;
    }

    std::string pos_str = ss.str();

    // Compute hash (using std::hash as placeholder for production SHA1)
    std::hash<std::string> hasher;
    size_t hash_val = hasher(pos_str);

    // Convert to hex string
    std::stringstream hex_ss;
    hex_ss << std::hex << std::setfill('0') << std::setw(16) << hash_val;

    return hex_ss.str();
}

} // namespace sentio
```

---

## File: `include/common/eod_guardian.h`

**Path**: `include/common/eod_guardian.h`

```cpp
#pragma once

#include "common/eod_state.h"
#include "common/time_utils.h"
#include "live/position_book.h"
#include "live/alpaca_client.hpp"
#include <memory>
#include <string>

namespace sentio {

/**
 * @brief EOD liquidation decision
 */
struct EodDecision {
    bool in_window{false};          // Are we in EOD liquidation window?
    bool has_positions{false};       // Does PositionBook have open positions?
    bool should_liquidate{false};    // Final decision: execute liquidation?
    std::string reason;              // Human-readable reason for decision
};

/**
 * @brief Production-grade EOD Guardian subsystem
 *
 * Safety-first design principles:
 * 1. Idempotency is anchored to FACTS (flatness), not file flags
 * 2. Always liquidate if (in_window AND has_positions)
 * 3. Position hash verification prevents stale state bugs
 * 4. Atomic state updates with status tracking
 * 5. Fail-safe: If uncertain, liquidate
 *
 * State Machine:
 *   PENDING → IN_PROGRESS → DONE
 *   ↑__________↓ (new positions opened after DONE)
 *
 * Usage:
 *   EodGuardian guardian(broker, calendar, state_store, time_mgr, position_book);
 *   // In main trading loop:
 *   guardian.tick();  // Call every heartbeat
 */
class EodGuardian {
public:
    /**
     * @brief Construct EOD Guardian
     * @param alpaca Alpaca broker client for order execution
     * @param state_store Persistent EOD state storage
     * @param time_mgr ET time manager
     * @param position_book Position tracking
     */
    EodGuardian(AlpacaClient& alpaca,
                EodStateStore& state_store,
                ETTimeManager& time_mgr,
                PositionBook& position_book);

    /**
     * @brief Main entry point - call every heartbeat
     *
     * This method:
     * 1. Checks if we're in EOD window (3:55-4:00 PM ET)
     * 2. Checks if positions are open
     * 3. Decides whether to liquidate
     * 4. Executes liquidation if needed
     * 5. Updates state atomically
     */
    void tick();

    /**
     * @brief Force liquidation (for testing/manual override)
     */
    void force_liquidate();

    /**
     * @brief Get current EOD state
     */
    EodState get_state() const;

    /**
     * @brief Check if EOD is complete for today
     * @return true if status == DONE and positions are flat
     */
    bool is_eod_complete() const;

private:
    AlpacaClient& alpaca_;
    EodStateStore& state_store_;
    ETTimeManager& time_mgr_;
    PositionBook& position_book_;

    std::string current_et_date_;
    EodState current_state_;
    bool liquidation_in_progress_{false};

    /**
     * @brief Calculate EOD decision based on current state
     * @return EodDecision with liquidation decision and reason
     */
    EodDecision calc_eod_decision() const;

    /**
     * @brief Execute EOD liquidation
     *
     * Steps:
     * 1. Mark state as IN_PROGRESS
     * 2. Cancel all open orders
     * 3. Flatten all positions
     * 4. Verify flatness
     * 5. Calculate position hash
     * 6. Mark state as DONE with hash
     */
    void execute_eod_liquidation();

    /**
     * @brief Verify positions are flat
     * @throws std::runtime_error if not flat
     */
    void verify_flatness() const;

    /**
     * @brief Update current date and reload state if day changed
     */
    void refresh_state_if_needed();

    /**
     * @brief Log EOD decision (for debugging)
     */
    void log_decision(const EodDecision& decision) const;
};

} // namespace sentio
```

---

## File: `src/common/eod_guardian.cpp`

**Path**: `src/common/eod_guardian.cpp`

```cpp
#include "common/eod_guardian.h"
#include "common/exceptions.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace sentio {

EodGuardian::EodGuardian(AlpacaClient& alpaca,
                         EodStateStore& state_store,
                         ETTimeManager& time_mgr,
                         PositionBook& position_book)
    : alpaca_(alpaca)
    , state_store_(state_store)
    , time_mgr_(time_mgr)
    , position_book_(position_book)
    , current_et_date_(time_mgr_.get_current_et_date())
    , current_state_(state_store_.load(current_et_date_)) {
}

void EodGuardian::tick() {
    // Refresh state if day changed
    refresh_state_if_needed();

    // Calculate decision
    EodDecision decision = calc_eod_decision();

    // Log decision (only when in window or status changes)
    if (decision.in_window || decision.should_liquidate) {
        log_decision(decision);
    }

    // Execute if needed
    if (decision.should_liquidate && !liquidation_in_progress_) {
        execute_eod_liquidation();
    }
}

void EodGuardian::force_liquidate() {
    std::cout << "[EodGuardian] FORCE LIQUIDATE requested" << std::endl;
    execute_eod_liquidation();
}

EodState EodGuardian::get_state() const {
    return current_state_;
}

bool EodGuardian::is_eod_complete() const {
    return current_state_.status == EodStatus::DONE && position_book_.is_flat();
}

EodDecision EodGuardian::calc_eod_decision() const {
    EodDecision decision;

    // Check if we're in EOD window (3:55-4:00 PM ET)
    decision.in_window = time_mgr_.is_eod_liquidation_window();
    decision.has_positions = !position_book_.is_flat();

    // Safety-first rule: If in window AND have positions, liquidate
    if (decision.in_window && decision.has_positions) {
        decision.should_liquidate = true;
        decision.reason = "In EOD window with open positions - LIQUIDATE";
        return decision;
    }

    // If in window but flat, check if we need to mark DONE
    if (decision.in_window && !decision.has_positions) {
        if (current_state_.status != EodStatus::DONE) {
            decision.should_liquidate = true;  // Will just mark DONE
            decision.reason = "In EOD window, already flat - mark DONE";
        } else {
            decision.should_liquidate = false;
            decision.reason = "In EOD window, flat, already marked DONE";
        }
        return decision;
    }

    // Not in window
    decision.should_liquidate = false;
    decision.reason = "Not in EOD window";
    return decision;
}

void EodGuardian::execute_eod_liquidation() {
    liquidation_in_progress_ = true;

    try {
        std::cout << "[EodGuardian] === EXECUTING EOD LIQUIDATION ===" << std::endl;

        // Step 1: Mark as IN_PROGRESS
        current_state_.status = EodStatus::IN_PROGRESS;
        current_state_.last_attempt_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_store_.save(current_et_date_, current_state_);
        std::cout << "[EodGuardian] State marked IN_PROGRESS" << std::endl;

        // Step 2: Cancel all open orders
        std::cout << "[EodGuardian] Cancelling all open orders..." << std::endl;
        alpaca_.cancel_all_orders();

        // Step 3: Flatten all positions (if any)
        if (!position_book_.is_flat()) {
            std::cout << "[EodGuardian] Flattening all positions..." << std::endl;
            alpaca_.close_all_positions();

            // Wait for fills (up to 3 seconds)
            for (int i = 0; i < 30; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (position_book_.is_flat()) {
                    break;
                }
            }
        }

        // Step 4: Verify flatness
        verify_flatness();
        std::cout << "[EodGuardian] ✓ Verified flat" << std::endl;

        // Step 5: Calculate position hash (should be empty for flat book)
        std::string hash = position_book_.positions_hash();
        if (!hash.empty()) {
            throw std::runtime_error("Position hash non-empty after liquidation");
        }

        // Step 6: Mark DONE
        current_state_.status = EodStatus::DONE;
        current_state_.positions_hash = hash;
        current_state_.last_attempt_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_store_.save(current_et_date_, current_state_);

        std::cout << "[EodGuardian] ✓ EOD liquidation complete for " << current_et_date_ << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[EodGuardian] ERROR during liquidation: " << e.what() << std::endl;
        liquidation_in_progress_ = false;
        throw;
    }

    liquidation_in_progress_ = false;
}

void EodGuardian::verify_flatness() const {
    if (!position_book_.is_flat()) {
        auto positions = position_book_.get_all_positions();
        std::cerr << "[EodGuardian] FLATNESS VERIFICATION FAILED:" << std::endl;
        for (const auto& [symbol, pos] : positions) {
            std::cerr << "  " << symbol << ": " << pos.qty << " shares" << std::endl;
        }
        throw std::runtime_error("EOD liquidation failed - positions still open");
    }
}

void EodGuardian::refresh_state_if_needed() {
    std::string today = time_mgr_.get_current_et_date();
    if (today != current_et_date_) {
        std::cout << "[EodGuardian] Day changed: " << current_et_date_
                  << " → " << today << std::endl;
        current_et_date_ = today;
        current_state_ = state_store_.load(current_et_date_);
        liquidation_in_progress_ = false;
    }
}

void EodGuardian::log_decision(const EodDecision& decision) const {
    std::cout << "[EodGuardian] in_window=" << decision.in_window
              << " has_pos=" << decision.has_positions
              << " should_liq=" << decision.should_liquidate
              << " | " << decision.reason << std::endl;
}

} // namespace sentio
```

---

## File: `include/common/eod_state.h`

**Path**: `include/common/eod_state.h`

```cpp
#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace sentio {

/**
 * @brief EOD execution status
 */
enum class EodStatus {
    PENDING,      // Not started yet for this day
    IN_PROGRESS,  // Liquidation in progress
    DONE          // Verified flat and complete
};

/**
 * @brief Complete EOD state for a trading day
 */
struct EodState {
    EodStatus status{EodStatus::PENDING};
    std::string positions_hash;  // SHA1 of sorted positions (for verification)
    int64_t last_attempt_epoch{0};  // Unix timestamp of last liquidation attempt

    EodState() = default;
    EodState(EodStatus s, std::string hash, int64_t epoch)
        : status(s), positions_hash(hash), last_attempt_epoch(epoch) {}
};

/**
 * @brief Persistent state tracking for End-of-Day (EOD) liquidation
 *
 * Production-hardened implementation with:
 * - Status-based state machine (PENDING → IN_PROGRESS → DONE)
 * - Position hash verification (detects corruption/stale state)
 * - Timestamp tracking (enables retry logic)
 * - Safety-first: Always liquidate if positions detected in window
 *
 * File format (plain text, one line per field):
 *   date=YYYY-MM-DD
 *   status=PENDING|IN_PROGRESS|DONE
 *   positions_hash=<sha1_hex>
 *   last_attempt_epoch=<unix_timestamp>
 */
class EodStateStore {
public:
    /**
     * @brief Construct state store with file path
     * @param state_file_path Full path to state file
     */
    explicit EodStateStore(std::string state_file_path);

    /**
     * @brief Load complete EOD state for given date
     * @param et_date ET date in YYYY-MM-DD format
     * @return EodState with status, hash, timestamp
     */
    EodState load(const std::string& et_date) const;

    /**
     * @brief Save complete EOD state atomically
     * @param et_date ET date in YYYY-MM-DD format
     * @param state Complete state to persist
     */
    void save(const std::string& et_date, const EodState& state);

    /**
     * @brief DEPRECATED: Check if EOD completed (use load() instead)
     * @param et_date ET date in YYYY-MM-DD format
     * @return true if status == DONE
     */
    [[deprecated("Use load() and check status instead")]]
    bool is_eod_complete(const std::string& et_date) const {
        return load(et_date).status == EodStatus::DONE;
    }

    /**
     * @brief DEPRECATED: Mark EOD complete (use save() instead)
     * @param et_date ET date in YYYY-MM-DD format
     */
    [[deprecated("Use save() with full EodState instead")]]
    void mark_eod_complete(const std::string& et_date);

    /**
     * @brief Get the ET date of the last saved state
     * @return ET date string if available, nullopt if no state recorded
     */
    std::optional<std::string> last_eod_date() const;

private:
    std::string state_file_;

    // Parse status from string
    static EodStatus parse_status(const std::string& s);

    // Convert status to string
    static std::string status_to_string(EodStatus s);
};

/**
 * @brief Convert status to string for logging
 */
inline const char* to_string(EodStatus status) {
    switch (status) {
        case EodStatus::PENDING: return "PENDING";
        case EodStatus::IN_PROGRESS: return "IN_PROGRESS";
        case EodStatus::DONE: return "DONE";
    }
    return "UNKNOWN";
}

} // namespace sentio
```

---

## File: `src/common/eod_state.cpp`

**Path**: `src/common/eod_state.cpp`

```cpp
#include "common/eod_state.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace sentio {

EodStateStore::EodStateStore(std::string state_file_path)
    : state_file_(std::move(state_file_path)) {}

EodState EodStateStore::load(const std::string& et_date) const {
    std::ifstream file(state_file_);
    if (!file.is_open()) {
        // No file = fresh start = PENDING
        return EodState{};
    }

    std::string stored_date, status_str, hash;
    int64_t epoch = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("date=", 0) == 0) {
            stored_date = line.substr(5);
        } else if (line.rfind("status=", 0) == 0) {
            status_str = line.substr(7);
        } else if (line.rfind("positions_hash=", 0) == 0) {
            hash = line.substr(15);
        } else if (line.rfind("last_attempt_epoch=", 0) == 0) {
            epoch = std::stoll(line.substr(19));
        }
    }

    // If stored date doesn't match, return fresh PENDING state
    if (stored_date != et_date) {
        return EodState{};
    }

    return EodState{parse_status(status_str), hash, epoch};
}

void EodStateStore::save(const std::string& et_date, const EodState& state) {
    // Atomic write: write to temp file, then rename
    std::string temp_file = state_file_ + ".tmp";

    std::ofstream file(temp_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open EOD state file for writing: " + temp_file);
    }

    file << "date=" << et_date << "\n";
    file << "status=" << status_to_string(state.status) << "\n";
    file << "positions_hash=" << state.positions_hash << "\n";
    file << "last_attempt_epoch=" << state.last_attempt_epoch << "\n";

    file.flush();
    file.close();

    // Atomic rename
    if (std::rename(temp_file.c_str(), state_file_.c_str()) != 0) {
        throw std::runtime_error("Failed to atomically update EOD state file");
    }
}

void EodStateStore::mark_eod_complete(const std::string& et_date) {
    // Deprecated method - for backwards compatibility
    save(et_date, EodState{EodStatus::DONE, "", 0});
}

std::optional<std::string> EodStateStore::last_eod_date() const {
    std::ifstream file(state_file_);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("date=", 0) == 0) {
            return line.substr(5);
        }
    }

    return std::nullopt;
}

EodStatus EodStateStore::parse_status(const std::string& s) {
    if (s == "PENDING") return EodStatus::PENDING;
    if (s == "IN_PROGRESS") return EodStatus::IN_PROGRESS;
    if (s == "DONE") return EodStatus::DONE;
    return EodStatus::PENDING;  // Default to safe state
}

std::string EodStateStore::status_to_string(EodStatus s) {
    switch (s) {
        case EodStatus::PENDING: return "PENDING";
        case EodStatus::IN_PROGRESS: return "IN_PROGRESS";
        case EodStatus::DONE: return "DONE";
    }
    return "PENDING";
}

} // namespace sentio
```

---

## File: `include/strategy/online_ensemble_strategy.h`

**Path**: `include/strategy/online_ensemble_strategy.h`

```cpp
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "strategy/market_regime_detector.h"
#include "strategy/regime_parameter_manager.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Regime detection parameters
        bool enable_regime_detection = false;  // Enable regime-aware parameter switching
        int regime_check_interval = 100;       // Check regime every N bars
        int regime_lookback_period = 100;      // Bars to analyze for regime detection

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Predictor training (for warmup)
    void train_predictor(const std::vector<double>& features, double realized_return);
    std::vector<double> extract_features(const Bar& current_bar);

    // Feature caching support (for Optuna optimization speedup)
    void set_external_features(const std::vector<double>* features) {
        external_features_ = features;
        skip_feature_engine_update_ = (features != nullptr);
    }

    // Runtime configuration update (for mid-day optimization)
    void update_config(const OnlineEnsembleConfig& new_config) {
        config_ = new_config;
    }

    // Learning state management
    struct LearningState {
        int64_t last_trained_bar_id = -1;      // Global bar ID of last training
        int last_trained_bar_index = -1;       // Index of last trained bar
        int64_t last_trained_timestamp_ms = 0; // Timestamp of last training
        bool is_warmed_up = false;              // Feature engine ready
        bool is_learning_current = true;        // Learning is up-to-date
        int bars_behind = 0;                    // How many bars behind
    };

    LearningState get_learning_state() const { return learning_state_; }
    bool ensure_learning_current(const Bar& bar);  // Catch up if needed
    bool is_learning_current() const { return learning_state_.is_learning_current; }

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering (production-grade with O(1) updates, 45 features)
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::shared_ptr<const std::vector<double>> features;  // Shared, immutable
        double entry_price;
        bool is_long;
    };

    struct PendingUpdate {
        std::array<HorizonPrediction, 3> horizons;  // Fixed size for 3 horizons
        uint8_t count = 0;  // Track actual count (1-3)
    };

    std::map<int, PendingUpdate> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Learning state tracking
    LearningState learning_state_;
    std::deque<Bar> missed_bars_;  // Queue of bars that need training

    // External feature support for caching
    const std::vector<double>* external_features_ = nullptr;
    bool skip_feature_engine_update_ = false;

    // Regime detection (optional)
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<RegimeParameterManager> regime_param_manager_;
    MarketRegime current_regime_;
    int bars_since_regime_check_;

    // Private methods
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);
    void check_and_update_regime();  // Regime detection method

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio
```

---

## File: `src/strategy/online_ensemble_strategy.cpp`

**Path**: `src/strategy/online_ensemble_strategy.cpp`

```cpp
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0),
      current_regime_(MarketRegime::CHOPPY),
      bars_since_regime_check_(0) {

    // Initialize feature engine V2 (production-grade with O(1) updates)
    features::EngineConfig engine_config;
    engine_config.momentum = true;
    engine_config.volatility = true;
    engine_config.volume = true;
    engine_config.normalize = true;
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>(engine_config);

    // Get feature count from V2 engine schema
    size_t num_features = feature_engine_->names().size();
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    // Initialize regime detection if enabled
    if (config_.enable_regime_detection) {
        // Use new adaptive detector with default params (vol_window=96, slope_window=120, chop_window=48)
        regime_detector_ = std::make_unique<MarketRegimeDetector>();
        regime_param_manager_ = std::make_unique<RegimeParameterManager>();
        utils::log_info("Regime detection enabled with adaptive thresholds - check interval: " +
                       std::to_string(config_.regime_check_interval) + " bars");
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    // CRITICAL: Ensure learning is current before generating signal
    if (!ensure_learning_current(bar)) {
        throw std::runtime_error(
            "[OnlineEnsemble] FATAL: Cannot generate signal - learning state is not current. "
            "Bar ID: " + std::to_string(bar.bar_id) +
            ", Last trained: " + std::to_string(learning_state_.last_trained_bar_id) +
            ", Bars behind: " + std::to_string(learning_state_.bars_behind));
    }

    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check and update regime if enabled
    check_and_update_regime();

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // DEBUG: Log prediction
    static int signal_count = 0;
    signal_count++;
    if (signal_count <= 10) {
// DEBUG:         std::cout << "[OES] generate_signal #" << signal_count
// DEBUG:                   << ": predicted_return=" << prediction.predicted_return
// DEBUG:                   << ", confidence=" << prediction.confidence
// DEBUG:                   << std::endl;
    }

    // Check for NaN in prediction
    if (!std::isfinite(prediction.predicted_return) || !std::isfinite(prediction.confidence)) {
        std::cerr << "[OES] WARNING: NaN in prediction! pred_return=" << prediction.predicted_return
                  << ", confidence=" << prediction.confidence << " - returning neutral" << std::endl;
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        return output;
    }

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    if (signal_count <= 10) {
// DEBUG:         std::cout << "[OES]   → base_prob=" << base_prob << std::endl;
    }

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.confidence = prediction.confidence;  // FIX: Set confidence from prediction
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine V2 (skip if using external cached features)
    if (!skip_feature_engine_update_) {
        feature_engine_->update(bar);
    }

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);

    // Update learning state after processing this bar
    learning_state_.last_trained_bar_id = bar.bar_id;
    learning_state_.last_trained_bar_index = samples_seen_ - 1;  // 0-indexed
    learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
    learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    // Use external features if provided (for feature caching optimization)
    if (external_features_ != nullptr) {
        return *external_features_;
    }

    // DEBUG: Track why features might be empty
    static int extract_count = 0;
    extract_count++;

    if (bar_history_.size() < MIN_FEATURES_BARS) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": bar_history_.size()=" << bar_history_.size()
// DEBUG:                       << " < MIN_FEATURES_BARS=" << MIN_FEATURES_BARS
// DEBUG:                       << " → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_seeded()) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": feature_engine_v2 NOT ready → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};
    }

    // Get features from V2 engine (returns const vector& - no copy)
    const auto& features_view = feature_engine_->features_view();
    std::vector<double> features(features_view.begin(), features_view.end());
    if (extract_count <= 10 || features.empty()) {
// DEBUG:         std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                   << ": got " << features.size() << " features from engine"
// DEBUG:                   << std::endl;
    }

    return features;
}

void OnlineEnsembleStrategy::train_predictor(const std::vector<double>& features, double realized_return) {
    if (features.empty()) {
        return;  // Nothing to train on
    }

    // Train all horizon predictors with the same realized return
    // (In practice, each horizon would use its own future return, but for warmup we use next-bar return)
    for (int horizon : config_.prediction_horizons) {
        ensemble_predictor_->update(horizon, features, realized_return);
    }
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    // Create shared_ptr only once per bar (reuse for all horizons)
    static std::shared_ptr<const std::vector<double>> shared_features;
    static int last_bar_index = -1;

    if (bar_index != last_bar_index) {
        // New bar - create new shared features
        shared_features = std::make_shared<const std::vector<double>>(features);
        last_bar_index = bar_index;
    }

    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = shared_features;  // Share, don't copy
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    // Use fixed array instead of vector
    auto& update = pending_updates_[pred.target_bar_index];
    if (update.count < 3) {
        update.horizons[update.count++] = std::move(pred);  // Move, don't copy
    }
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& update = it->second;

        // Process only the valid predictions (0 to count-1)
        for (uint8_t i = 0; i < update.count; ++i) {
            const auto& pred = update.horizons[i];

            double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
            if (!pred.is_long) {
                actual_return = -actual_return;
            }

            // Dereference shared_ptr only when needed
            ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
        }

        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed " + std::to_string(static_cast<int>(update.count)) +
                           " updates at bar " + std::to_string(samples_seen_) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

// ============================================================================
// Learning State Management - Ensures model is always current before signals
// ============================================================================

bool OnlineEnsembleStrategy::ensure_learning_current(const Bar& bar) {
    // Check if this is the first bar (initial state)
    if (learning_state_.last_trained_bar_id == -1) {
        // First bar - just update state, don't train yet
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // Check if we're already current with this bar
    if (learning_state_.last_trained_bar_id == bar.bar_id) {
        return true;  // Already trained on this bar
    }

    // Calculate how many bars behind we are
    int64_t bars_behind = bar.bar_id - learning_state_.last_trained_bar_id;

    if (bars_behind < 0) {
        // Going backwards in time - this should only happen during replay/testing
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Bar ID went backwards! "
                  << "Current: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << " (replaying historical data)" << std::endl;

        // Reset learning state for replay
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    if (bars_behind == 0) {
        return true;  // Current bar
    }

    if (bars_behind == 1) {
        // Normal case: exactly 1 bar behind (typical sequential processing)
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // We're more than 1 bar behind - need to catch up
    learning_state_.bars_behind = static_cast<int>(bars_behind);
    learning_state_.is_learning_current = false;

    // Only warn if feature engine is warmed up
    // (during warmup, it's normal to skip bars)
    if (learning_state_.is_warmed_up) {
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Learning engine is " << bars_behind << " bars behind!"
                  << std::endl;
        std::cerr << "    Current bar ID: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << std::endl;
        std::cerr << "    This should only happen during warmup. Once warmed up, "
                  << "the system must stay fully updated." << std::endl;

        // In production live trading, this is FATAL
        // Cannot generate signals without being current
        return false;
    }

    // During warmup, it's OK to be behind
    // Mark as current and continue
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
    return true;
}

void OnlineEnsembleStrategy::check_and_update_regime() {
    if (!config_.enable_regime_detection || !regime_detector_) {
        return;
    }

    // Check regime periodically
    bars_since_regime_check_++;
    if (bars_since_regime_check_ < config_.regime_check_interval) {
        return;
    }

    bars_since_regime_check_ = 0;

    // Need sufficient history
    if (bar_history_.size() < static_cast<size_t>(config_.regime_lookback_period)) {
        return;
    }

    // Detect current regime
    std::vector<Bar> recent_bars(bar_history_.end() - config_.regime_lookback_period,
                                 bar_history_.end());
    MarketRegime new_regime = regime_detector_->detect_regime(recent_bars);

    // Switch parameters if regime changed
    if (new_regime != current_regime_) {
        MarketRegime old_regime = current_regime_;
        current_regime_ = new_regime;

        RegimeParams params = regime_param_manager_->get_params_for_regime(new_regime);

        // Apply new thresholds
        current_buy_threshold_ = params.buy_threshold;
        current_sell_threshold_ = params.sell_threshold;

        // Log regime transition
        utils::log_info("Regime transition: " +
                       MarketRegimeDetector::regime_to_string(old_regime) + " -> " +
                       MarketRegimeDetector::regime_to_string(new_regime) +
                       " | buy=" + std::to_string(current_buy_threshold_) +
                       " sell=" + std::to_string(current_sell_threshold_) +
                       " lambda=" + std::to_string(params.ewrls_lambda) +
                       " bb=" + std::to_string(params.bb_amplification_factor));

        // Note: For full regime switching, we would also update:
        // - config_.ewrls_lambda (requires rebuilding predictor)
        // - config_.bb_amplification_factor
        // - config_.horizon_weights
        // For now, only threshold switching is implemented (most impactful)
    }
}

} // namespace sentio
```

---

## File: `include/backend/adaptive_trading_mechanism.h`

**Path**: `include/backend/adaptive_trading_mechanism.h`

```cpp
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "common/types.h"
#include "strategy/signal_output.h"

// Forward declarations to avoid circular dependencies
namespace sentio {
    class BackendComponent;
}

namespace sentio {

// ===================================================================
// THRESHOLD PAIR STRUCTURE
// ===================================================================

/**
 * @brief Represents a pair of buy and sell thresholds for trading decisions
 * 
 * The ThresholdPair encapsulates the core decision boundaries for the adaptive
 * trading system. Buy threshold determines when signals trigger buy orders,
 * sell threshold determines sell orders, with a neutral zone between them.
 */
struct ThresholdPair {
    double buy_threshold = 0.6;   // Probability threshold for buy orders
    double sell_threshold = 0.4;  // Probability threshold for sell orders
    
    ThresholdPair() = default;
    ThresholdPair(double buy, double sell) : buy_threshold(buy), sell_threshold(sell) {}
    
    /**
     * @brief Validates that thresholds are within acceptable bounds
     * @return true if thresholds are valid, false otherwise
     */
    bool is_valid() const {
        return buy_threshold > sell_threshold + 0.05 && // Min 5% gap
               buy_threshold >= 0.51 && buy_threshold <= 0.90 &&
               sell_threshold >= 0.10 && sell_threshold <= 0.49;
    }
    
    /**
     * @brief Gets the size of the neutral zone between thresholds
     * @return Size of neutral zone (buy_threshold - sell_threshold)
     */
    double get_neutral_zone_size() const {
        return buy_threshold - sell_threshold;
    }
};

// ===================================================================
// MARKET STATE AND REGIME DETECTION
// ===================================================================

/**
 * @brief Enumeration of different market regimes for adaptive threshold selection
 * @deprecated Use MarketRegime from market_regime_detector.h instead
 */
enum class AdaptiveMarketRegime {
    BULL_LOW_VOL,     // Rising prices, low volatility - aggressive thresholds
    BULL_HIGH_VOL,    // Rising prices, high volatility - moderate thresholds
    BEAR_LOW_VOL,     // Falling prices, low volatility - moderate thresholds
    BEAR_HIGH_VOL,    // Falling prices, high volatility - conservative thresholds
    SIDEWAYS_LOW_VOL, // Range-bound, low volatility - balanced thresholds
    SIDEWAYS_HIGH_VOL // Range-bound, high volatility - conservative thresholds
};

/**
 * @brief Comprehensive market state information for adaptive decision making
 */
struct MarketState {
    double current_price = 0.0;
    double volatility = 0.0;          // 20-day volatility measure
    double trend_strength = 0.0;      // -1 (strong bear) to +1 (strong bull)
    double volume_ratio = 1.0;        // Current volume / average volume
    AdaptiveMarketRegime regime = AdaptiveMarketRegime::SIDEWAYS_LOW_VOL;
    
    // Signal distribution statistics
    double avg_signal_strength = 0.5;
    double signal_volatility = 0.1;
    
    // Portfolio state
    int open_positions = 0;
    double cash_utilization = 0.0;    // 0.0 to 1.0
};

/**
 * @brief Detects and classifies market regimes for adaptive threshold optimization
 * @deprecated Use MarketRegimeDetector from market_regime_detector.h instead
 *
 * The AdaptiveMarketRegimeDetector analyzes price history, volatility, and trend patterns
 * to classify current market conditions. This enables regime-specific threshold
 * optimization for improved performance across different market environments.
 */
class AdaptiveMarketRegimeDetector {
private:
    std::vector<double> price_history_;
    std::vector<double> volume_history_;
    const size_t LOOKBACK_PERIOD = 20;
    
public:
    /**
     * @brief Analyzes current market conditions and returns comprehensive market state
     * @param current_bar Current market data bar
     * @param recent_history Vector of recent bars for trend analysis
     * @param signal Current signal for context
     * @return MarketState with regime classification and metrics
     */
    MarketState analyze_market_state(const Bar& current_bar, 
                                   const std::vector<Bar>& recent_history,
                                   const SignalOutput& signal);
    
private:
    double calculate_volatility();
    double calculate_trend_strength();
    double calculate_volume_ratio();
    AdaptiveMarketRegime classify_market_regime(double volatility, double trend_strength);
};

// ===================================================================
// PERFORMANCE TRACKING AND EVALUATION
// ===================================================================

/**
 * @brief Represents the outcome of a completed trade for learning feedback
 */
struct TradeOutcome {
    // Store essential trade information instead of full TradeOrder to avoid circular dependency
    std::string symbol;
    TradeAction action = TradeAction::HOLD;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    double actual_pnl = 0.0;
    double pnl_percentage = 0.0;
    bool was_profitable = false;
    int bars_to_profit = 0;
    double max_adverse_move = 0.0;
    double sharpe_contribution = 0.0;
    std::chrono::system_clock::time_point outcome_timestamp;
};

/**
 * @brief Comprehensive performance metrics for adaptive learning evaluation
 */
struct PerformanceMetrics {
    double win_rate = 0.0;              // Percentage of profitable trades
    double profit_factor = 1.0;         // Gross profit / Gross loss
    double sharpe_ratio = 0.0;          // Risk-adjusted return
    double max_drawdown = 0.0;          // Maximum peak-to-trough decline
    double trade_frequency = 0.0;       // Trades per day
    double capital_efficiency = 0.0;    // Return on deployed capital
    double opportunity_cost = 0.0;      // Estimated missed profits
    std::vector<double> returns;        // Historical returns
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;
};

/**
 * @brief Evaluates trading performance and generates learning signals
 * 
 * The PerformanceEvaluator tracks trade outcomes, calculates comprehensive
 * performance metrics, and generates reward signals for the learning algorithms.
 * It maintains rolling windows of performance data for adaptive optimization.
 */
class PerformanceEvaluator {
private:
    std::vector<TradeOutcome> trade_history_;
    std::vector<double> portfolio_values_;
    const size_t MAX_HISTORY = 1000;
    const size_t PERFORMANCE_WINDOW = 100;
    
public:
    /**
     * @brief Adds a completed trade outcome for performance tracking
     * @param outcome TradeOutcome with P&L and timing information
     */
    void add_trade_outcome(const TradeOutcome& outcome);
    
    /**
     * @brief Adds portfolio value snapshot for drawdown calculation
     * @param value Current total portfolio value
     */
    void add_portfolio_value(double value);
    
    /**
     * @brief Calculates comprehensive performance metrics from recent trades
     * @return PerformanceMetrics with win rate, Sharpe ratio, drawdown, etc.
     */
    PerformanceMetrics calculate_performance_metrics();
    
    /**
     * @brief Calculates reward signal for learning algorithms
     * @param metrics Current performance metrics
     * @return Reward value for reinforcement learning
     */
    double calculate_reward_signal(const PerformanceMetrics& metrics);
    
private:
    double calculate_sharpe_ratio(const std::vector<double>& returns);
    double calculate_max_drawdown();
    double calculate_capital_efficiency();
};

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER
// ===================================================================

/**
 * @brief State-action pair for Q-learning lookup table
 */
struct StateActionPair {
    int state_hash;
    int action_index;
    
    bool operator<(const StateActionPair& other) const {
        return std::tie(state_hash, action_index) < std::tie(other.state_hash, other.action_index);
    }
};

/**
 * @brief Available actions for threshold adjustment in Q-learning
 */
enum class ThresholdAction {
    INCREASE_BUY_SMALL,      // +0.01
    INCREASE_BUY_MEDIUM,     // +0.03
    DECREASE_BUY_SMALL,      // -0.01
    DECREASE_BUY_MEDIUM,     // -0.03
    INCREASE_SELL_SMALL,     // +0.01
    INCREASE_SELL_MEDIUM,    // +0.03
    DECREASE_SELL_SMALL,     // -0.01
    DECREASE_SELL_MEDIUM,    // -0.03
    MAINTAIN_THRESHOLDS,     // No change
    COUNT
};

/**
 * @brief Q-Learning based threshold optimizer for adaptive trading
 * 
 * Implements reinforcement learning to find optimal buy/sell thresholds.
 * Uses epsilon-greedy exploration and Q-value updates to learn from
 * trading outcomes and maximize long-term performance.
 */
class QLearningThresholdOptimizer {
private:
    std::map<StateActionPair, double> q_table_;
    std::map<int, int> state_visit_count_;
    
    // Hyperparameters
    double learning_rate_ = 0.1;
    double discount_factor_ = 0.95;
    double exploration_rate_ = 0.1;
    double exploration_decay_ = 0.995;
    double min_exploration_ = 0.01;
    
    // State discretization
    const int THRESHOLD_BINS = 20;
    const int PERFORMANCE_BINS = 10;
    
    std::mt19937 rng_;
    
public:
    QLearningThresholdOptimizer();
    
    /**
     * @brief Selects next action using epsilon-greedy policy
     * @param state Current market state
     * @param current_thresholds Current threshold values
     * @param performance Recent performance metrics
     * @return Selected threshold action
     */
    ThresholdAction select_action(const MarketState& state, 
                                 const ThresholdPair& current_thresholds,
                                 const PerformanceMetrics& performance);
    
    /**
     * @brief Updates Q-value based on observed reward
     * @param prev_state Previous market state
     * @param prev_thresholds Previous thresholds
     * @param prev_performance Previous performance
     * @param action Action taken
     * @param reward Observed reward
     * @param new_state New market state
     * @param new_thresholds New thresholds
     * @param new_performance New performance
     */
    void update_q_value(const MarketState& prev_state,
                       const ThresholdPair& prev_thresholds,
                       const PerformanceMetrics& prev_performance,
                       ThresholdAction action,
                       double reward,
                       const MarketState& new_state,
                       const ThresholdPair& new_thresholds,
                       const PerformanceMetrics& new_performance);
    
    /**
     * @brief Applies selected action to current thresholds
     * @param current_thresholds Current threshold values
     * @param action Action to apply
     * @return New threshold values after action
     */
    ThresholdPair apply_action(const ThresholdPair& current_thresholds, ThresholdAction action);
    
    /**
     * @brief Gets current learning progress (1.0 - exploration_rate)
     * @return Learning progress from 0.0 to 1.0
     */
    double get_learning_progress() const;
    
private:
    int discretize_state(const MarketState& state, 
                        const ThresholdPair& thresholds,
                        const PerformanceMetrics& performance);
    double get_q_value(const StateActionPair& sa_pair);
    double get_max_q_value(int state_hash);
    ThresholdAction get_best_action(int state_hash);
};

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER
// ===================================================================

/**
 * @brief Represents a bandit arm (threshold combination) with statistics
 */
struct BanditArm {
    ThresholdPair thresholds;
    double estimated_reward = 0.0;
    int pull_count = 0;
    double confidence_bound = 0.0;
    
    BanditArm(const ThresholdPair& t) : thresholds(t) {}
};

/**
 * @brief Multi-Armed Bandit optimizer for threshold selection
 * 
 * Implements UCB1 algorithm to balance exploration and exploitation
 * across different threshold combinations. Maintains confidence bounds
 * for each arm and selects based on upper confidence bounds.
 */
class MultiArmedBanditOptimizer {
private:
    std::vector<BanditArm> arms_;
    int total_pulls_ = 0;
    std::mt19937 rng_;
    
public:
    MultiArmedBanditOptimizer();
    
    /**
     * @brief Selects threshold pair using UCB1 algorithm
     * @return Selected threshold pair
     */
    ThresholdPair select_thresholds();
    
    /**
     * @brief Updates reward for selected threshold pair
     * @param thresholds Threshold pair that was used
     * @param reward Observed reward
     */
    void update_reward(const ThresholdPair& thresholds, double reward);
    
private:
    void initialize_arms();
    void update_confidence_bounds();
};

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER - Main Orchestrator
// ===================================================================

/**
 * @brief Learning algorithm selection for adaptive threshold optimization
 */
enum class LearningAlgorithm {
    Q_LEARNING,           // Reinforcement learning approach
    MULTI_ARMED_BANDIT,   // UCB1 bandit algorithm
    ENSEMBLE              // Combination of multiple algorithms
};

/**
 * @brief Configuration parameters for adaptive threshold system
 */
struct AdaptiveConfig {
    LearningAlgorithm algorithm = LearningAlgorithm::Q_LEARNING;
    double learning_rate = 0.1;
    double exploration_rate = 0.1;
    int performance_window = 50;
    int feedback_delay = 5;
    double max_drawdown_limit = 0.05;
    bool enable_regime_adaptation = true;
    bool conservative_mode = false;
};

/**
 * @brief Main orchestrator for adaptive threshold management
 * 
 * The AdaptiveThresholdManager coordinates all components of the adaptive
 * trading system. It manages learning algorithms, performance evaluation,
 * market regime detection, and provides the main interface for getting
 * optimal thresholds and processing trade outcomes.
 */
class AdaptiveThresholdManager {
private:
    // Current state
    ThresholdPair current_thresholds_;
    MarketState current_market_state_;
    PerformanceMetrics current_performance_;
    
    // Learning components
    std::unique_ptr<QLearningThresholdOptimizer> q_learner_;
    std::unique_ptr<MultiArmedBanditOptimizer> bandit_optimizer_;
    std::unique_ptr<AdaptiveMarketRegimeDetector> regime_detector_;
    std::unique_ptr<PerformanceEvaluator> performance_evaluator_;

    // Configuration
    AdaptiveConfig config_;

    // State tracking
    std::queue<std::pair<TradeOutcome, std::chrono::system_clock::time_point>> pending_trades_;
    std::vector<Bar> recent_bars_;
    bool learning_enabled_ = true;
    bool circuit_breaker_active_ = false;

    // Regime-specific thresholds
    std::map<AdaptiveMarketRegime, ThresholdPair> regime_thresholds_;
    
public:
    /**
     * @brief Constructs adaptive threshold manager with configuration
     * @param config Configuration parameters for the adaptive system
     */
    AdaptiveThresholdManager(const AdaptiveConfig& config = AdaptiveConfig());
    
    /**
     * @brief Gets current optimal thresholds for given market conditions
     * @param signal Current signal output
     * @param bar Current market data bar
     * @return Optimal threshold pair for current conditions
     */
    ThresholdPair get_current_thresholds(const SignalOutput& signal, const Bar& bar);
    
    /**
     * @brief Processes trade outcome for learning feedback
     * @param symbol Trade symbol
     * @param action Trade action (BUY/SELL)
     * @param quantity Trade quantity
     * @param price Trade price
     * @param trade_value Trade value
     * @param fees Trade fees
     * @param actual_pnl Actual profit/loss from trade
     * @param pnl_percentage P&L as percentage of trade value
     * @param was_profitable Whether trade was profitable
     */
    void process_trade_outcome(const std::string& symbol, TradeAction action, 
                              double quantity, double price, double trade_value, double fees,
                              double actual_pnl, double pnl_percentage, bool was_profitable);
    
    /**
     * @brief Updates portfolio value for performance tracking
     * @param value Current total portfolio value
     */
    void update_portfolio_value(double value);
    
    // Control methods
    void enable_learning(bool enabled) { learning_enabled_ = enabled; }
    void reset_circuit_breaker() { circuit_breaker_active_ = false; }
    bool is_circuit_breaker_active() const { return circuit_breaker_active_; }
    
    // Analytics methods
    PerformanceMetrics get_current_performance() const { return current_performance_; }
    MarketState get_current_market_state() const { return current_market_state_; }
    double get_learning_progress() const;
    
    /**
     * @brief Generates comprehensive performance report
     * @return Formatted string with performance metrics and insights
     */
    std::string generate_performance_report() const;
    
private:
    void initialize_regime_thresholds();
    void update_performance_and_learn();
    ThresholdPair get_regime_adapted_thresholds();
    ThresholdPair get_conservative_thresholds();
    void check_circuit_breaker();
};

} // namespace sentio
```

---

## File: `src/backend/adaptive_trading_mechanism.cpp`

**Path**: `src/backend/adaptive_trading_mechanism.cpp`

```cpp
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <numeric>
#include <filesystem>

namespace sentio {

// ===================================================================
// MARKET REGIME DETECTOR IMPLEMENTATION
// ===================================================================

MarketState AdaptiveMarketRegimeDetector::analyze_market_state(const Bar& current_bar, 
                                                      const std::vector<Bar>& recent_history,
                                                      const SignalOutput& signal) {
    MarketState state;
    
    // Update price history
    price_history_.push_back(current_bar.close);
    if (price_history_.size() > LOOKBACK_PERIOD) {
        price_history_.erase(price_history_.begin());
    }
    
    // Update volume history
    volume_history_.push_back(current_bar.volume);
    if (volume_history_.size() > LOOKBACK_PERIOD) {
        volume_history_.erase(volume_history_.begin());
    }
    
    // Calculate market metrics
    state.current_price = current_bar.close;
    state.volatility = calculate_volatility();
    state.trend_strength = calculate_trend_strength();
    state.volume_ratio = calculate_volume_ratio();
    state.regime = classify_market_regime(state.volatility, state.trend_strength);
    
    // Signal statistics
    state.avg_signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    utils::log_debug("Market Analysis: Price=" + std::to_string(state.current_price) + 
                    ", Vol=" + std::to_string(state.volatility) + 
                    ", Trend=" + std::to_string(state.trend_strength) + 
                    ", Regime=" + std::to_string(static_cast<int>(state.regime)));
    
    return state;
}

double AdaptiveMarketRegimeDetector::calculate_volatility() {
    if (price_history_.size() < 2) return 0.1; // Default volatility
    
    std::vector<double> returns;
    for (size_t i = 1; i < price_history_.size(); ++i) {
        double ret = std::log(price_history_[i] / price_history_[i-1]);
        returns.push_back(ret);
    }
    
    // Calculate standard deviation of returns
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double sq_sum = 0.0;
    for (double ret : returns) {
        sq_sum += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sq_sum / returns.size()) * std::sqrt(252); // Annualized
}

double AdaptiveMarketRegimeDetector::calculate_trend_strength() {
    if (price_history_.size() < 10) return 0.0;
    
    // Linear regression slope over recent prices
    double n = static_cast<double>(price_history_.size());
    double sum_x = n * (n - 1) / 2;
    double sum_y = std::accumulate(price_history_.begin(), price_history_.end(), 0.0);
    double sum_xy = 0.0;
    double sum_x2 = n * (n - 1) * (2 * n - 1) / 6;
    
    for (size_t i = 0; i < price_history_.size(); ++i) {
        sum_xy += static_cast<double>(i) * price_history_[i];
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to [-1, 1] range
    double price_range = *std::max_element(price_history_.begin(), price_history_.end()) -
                        *std::min_element(price_history_.begin(), price_history_.end());
    
    if (price_range > 0) {
        return std::clamp(slope / price_range * 100, -1.0, 1.0);
    }
    
    return 0.0;
}

double AdaptiveMarketRegimeDetector::calculate_volume_ratio() {
    if (volume_history_.empty()) return 1.0;
    
    double current_volume = volume_history_.back();
    double avg_volume = std::accumulate(volume_history_.begin(), volume_history_.end(), 0.0) / volume_history_.size();
    
    return (avg_volume > 0) ? current_volume / avg_volume : 1.0;
}

AdaptiveMarketRegime AdaptiveMarketRegimeDetector::classify_market_regime(double volatility, double trend_strength) {
    bool high_vol = volatility > 0.25; // 25% annualized volatility threshold

    if (trend_strength > 0.3) {
        return high_vol ? AdaptiveMarketRegime::BULL_HIGH_VOL : AdaptiveMarketRegime::BULL_LOW_VOL;
    } else if (trend_strength < -0.3) {
        return high_vol ? AdaptiveMarketRegime::BEAR_HIGH_VOL : AdaptiveMarketRegime::BEAR_LOW_VOL;
    } else {
        return high_vol ? AdaptiveMarketRegime::SIDEWAYS_HIGH_VOL : AdaptiveMarketRegime::SIDEWAYS_LOW_VOL;
    }
}

// ===================================================================
// PERFORMANCE EVALUATOR IMPLEMENTATION
// ===================================================================

void PerformanceEvaluator::add_trade_outcome(const TradeOutcome& outcome) {
    trade_history_.push_back(outcome);
    
    // Maintain rolling window
    if (trade_history_.size() > MAX_HISTORY) {
        trade_history_.erase(trade_history_.begin());
    }
    
    utils::log_debug("Trade outcome added: PnL=" + std::to_string(outcome.actual_pnl) + 
                    ", Profitable=" + (outcome.was_profitable ? "YES" : "NO"));
}

void PerformanceEvaluator::add_portfolio_value(double value) {
    portfolio_values_.push_back(value);
    
    if (portfolio_values_.size() > MAX_HISTORY) {
        portfolio_values_.erase(portfolio_values_.begin());
    }
}

PerformanceMetrics PerformanceEvaluator::calculate_performance_metrics() {
    PerformanceMetrics metrics;
    
    if (trade_history_.empty()) {
        return metrics;
    }
    
    // Get recent trades for analysis
    size_t start_idx = trade_history_.size() > PERFORMANCE_WINDOW ? 
                      trade_history_.size() - PERFORMANCE_WINDOW : 0;
    
    std::vector<TradeOutcome> recent_trades(
        trade_history_.begin() + start_idx, trade_history_.end());
    
    // Calculate basic metrics
    metrics.total_trades = static_cast<int>(recent_trades.size());
    metrics.winning_trades = 0;
    metrics.losing_trades = 0;
    metrics.gross_profit = 0.0;
    metrics.gross_loss = 0.0;
    
    for (const auto& trade : recent_trades) {
        if (trade.was_profitable) {
            metrics.winning_trades++;
            metrics.gross_profit += trade.actual_pnl;
        } else {
            metrics.losing_trades++;
            metrics.gross_loss += std::abs(trade.actual_pnl);
        }
        
        metrics.returns.push_back(trade.pnl_percentage);
    }
    
    // Calculate derived metrics
    metrics.win_rate = metrics.total_trades > 0 ? 
                      static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;
    
    metrics.profit_factor = metrics.gross_loss > 0 ? 
                           metrics.gross_profit / metrics.gross_loss : 1.0;
    
    metrics.sharpe_ratio = calculate_sharpe_ratio(metrics.returns);
    metrics.max_drawdown = calculate_max_drawdown();
    metrics.capital_efficiency = calculate_capital_efficiency();
    
    return metrics;
}

double PerformanceEvaluator::calculate_reward_signal(const PerformanceMetrics& metrics) {
    // Multi-objective reward function
    double profit_component = metrics.gross_profit - metrics.gross_loss;
    double risk_component = metrics.sharpe_ratio * 0.5;
    double drawdown_penalty = metrics.max_drawdown * -2.0;
    double overtrading_penalty = std::max(0.0, metrics.trade_frequency - 10.0) * -0.1;
    
    double total_reward = profit_component + risk_component + drawdown_penalty + overtrading_penalty;
    
    utils::log_debug("Reward calculation: Profit=" + std::to_string(profit_component) + 
                    ", Risk=" + std::to_string(risk_component) + 
                    ", Drawdown=" + std::to_string(drawdown_penalty) + 
                    ", Total=" + std::to_string(total_reward));
    
    return total_reward;
}

double PerformanceEvaluator::calculate_sharpe_ratio(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    double std_dev = std::sqrt(variance);
    return std_dev > 0 ? mean_return / std_dev : 0.0;
}

double PerformanceEvaluator::calculate_max_drawdown() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double peak = portfolio_values_[0];
    double max_dd = 0.0;
    
    for (double value : portfolio_values_) {
        if (value > peak) {
            peak = value;
        }
        
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    return max_dd;
}

double PerformanceEvaluator::calculate_capital_efficiency() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double initial_value = portfolio_values_.front();
    double final_value = portfolio_values_.back();
    
    return initial_value > 0 ? (final_value - initial_value) / initial_value : 0.0;
}

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER IMPLEMENTATION
// ===================================================================

QLearningThresholdOptimizer::QLearningThresholdOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    utils::log_info("Q-Learning Threshold Optimizer initialized with learning_rate=" + 
                   std::to_string(learning_rate_) + ", exploration_rate=" + std::to_string(exploration_rate_));
}

ThresholdAction QLearningThresholdOptimizer::select_action(const MarketState& state, 
                                                          const ThresholdPair& current_thresholds,
                                                          const PerformanceMetrics& performance) {
    int state_hash = discretize_state(state, current_thresholds, performance);
    
    // Epsilon-greedy action selection
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    if (dis(rng_) < exploration_rate_) {
        // Explore: random action
        std::uniform_int_distribution<int> action_dis(0, static_cast<int>(ThresholdAction::COUNT) - 1);
        ThresholdAction action = static_cast<ThresholdAction>(action_dis(rng_));
        utils::log_debug("Q-Learning: EXPLORE action=" + std::to_string(static_cast<int>(action)));
        return action;
    } else {
        // Exploit: best known action
        ThresholdAction action = get_best_action(state_hash);
        utils::log_debug("Q-Learning: EXPLOIT action=" + std::to_string(static_cast<int>(action)));
        return action;
    }
}

void QLearningThresholdOptimizer::update_q_value(const MarketState& prev_state,
                                                 const ThresholdPair& prev_thresholds,
                                                 const PerformanceMetrics& prev_performance,
                                                 ThresholdAction action,
                                                 double reward,
                                                 const MarketState& new_state,
                                                 const ThresholdPair& new_thresholds,
                                                 const PerformanceMetrics& new_performance) {
    
    int prev_state_hash = discretize_state(prev_state, prev_thresholds, prev_performance);
    int new_state_hash = discretize_state(new_state, new_thresholds, new_performance);
    
    StateActionPair sa_pair{prev_state_hash, static_cast<int>(action)};
    
    // Get current Q-value
    double current_q = get_q_value(sa_pair);
    
    // Get maximum Q-value for next state
    double max_next_q = get_max_q_value(new_state_hash);
    
    // Q-learning update
    double target = reward + discount_factor_ * max_next_q;
    double new_q = current_q + learning_rate_ * (target - current_q);
    
    q_table_[sa_pair] = new_q;
    state_visit_count_[prev_state_hash]++;
    
    // Decay exploration rate
    exploration_rate_ = std::max(min_exploration_, exploration_rate_ * exploration_decay_);
    
    utils::log_debug("Q-Learning update: State=" + std::to_string(prev_state_hash) + 
                    ", Action=" + std::to_string(static_cast<int>(action)) + 
                    ", Reward=" + std::to_string(reward) + 
                    ", Q_old=" + std::to_string(current_q) + 
                    ", Q_new=" + std::to_string(new_q));
}

ThresholdPair QLearningThresholdOptimizer::apply_action(const ThresholdPair& current_thresholds, ThresholdAction action) {
    ThresholdPair new_thresholds = current_thresholds;
    
    switch (action) {
        case ThresholdAction::INCREASE_BUY_SMALL:
            new_thresholds.buy_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_BUY_SMALL:
            new_thresholds.buy_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold -= 0.03;
            break;
        case ThresholdAction::INCREASE_SELL_SMALL:
            new_thresholds.sell_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_SELL_SMALL:
            new_thresholds.sell_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold -= 0.03;
            break;
        case ThresholdAction::MAINTAIN_THRESHOLDS:
        default:
            // No change
            break;
    }
    
    // Ensure thresholds remain valid
    new_thresholds.buy_threshold = std::clamp(new_thresholds.buy_threshold, 0.51, 0.90);
    new_thresholds.sell_threshold = std::clamp(new_thresholds.sell_threshold, 0.10, 0.49);
    
    // Ensure minimum gap
    if (new_thresholds.buy_threshold - new_thresholds.sell_threshold < 0.05) {
        new_thresholds.buy_threshold = new_thresholds.sell_threshold + 0.05;
        new_thresholds.buy_threshold = std::min(new_thresholds.buy_threshold, 0.90);
    }
    
    return new_thresholds;
}

double QLearningThresholdOptimizer::get_learning_progress() const {
    return 1.0 - exploration_rate_;
}

int QLearningThresholdOptimizer::discretize_state(const MarketState& state, 
                                                 const ThresholdPair& thresholds,
                                                 const PerformanceMetrics& performance) {
    // Create a hash of the discretized state
    int buy_bin = static_cast<int>((thresholds.buy_threshold - 0.5) / 0.4 * THRESHOLD_BINS);
    int sell_bin = static_cast<int>((thresholds.sell_threshold - 0.1) / 0.4 * THRESHOLD_BINS);
    int vol_bin = static_cast<int>(std::min(state.volatility / 0.5, 1.0) * 5);
    int trend_bin = static_cast<int>((state.trend_strength + 1.0) / 2.0 * 5);
    int perf_bin = static_cast<int>(std::clamp(performance.win_rate, 0.0, 1.0) * PERFORMANCE_BINS);
    
    // Combine bins into a single hash
    return buy_bin * 10000 + sell_bin * 1000 + vol_bin * 100 + trend_bin * 10 + perf_bin;
}

double QLearningThresholdOptimizer::get_q_value(const StateActionPair& sa_pair) {
    auto it = q_table_.find(sa_pair);
    return (it != q_table_.end()) ? it->second : 0.0; // Optimistic initialization
}

double QLearningThresholdOptimizer::get_max_q_value(int state_hash) {
    double max_q = 0.0;
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        max_q = std::max(max_q, get_q_value(sa_pair));
    }
    
    return max_q;
}

ThresholdAction QLearningThresholdOptimizer::get_best_action(int state_hash) {
    ThresholdAction best_action = ThresholdAction::MAINTAIN_THRESHOLDS;
    double best_q = get_q_value({state_hash, static_cast<int>(best_action)});
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        double q_val = get_q_value(sa_pair);
        
        if (q_val > best_q) {
            best_q = q_val;
            best_action = static_cast<ThresholdAction>(action);
        }
    }
    
    return best_action;
}

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER IMPLEMENTATION
// ===================================================================

MultiArmedBanditOptimizer::MultiArmedBanditOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_arms();
    utils::log_info("Multi-Armed Bandit Optimizer initialized with " + std::to_string(arms_.size()) + " arms");
}

ThresholdPair MultiArmedBanditOptimizer::select_thresholds() {
    if (arms_.empty()) {
        return ThresholdPair(); // Default thresholds
    }
    
    // UCB1 algorithm
    update_confidence_bounds();
    
    auto best_arm = std::max_element(arms_.begin(), arms_.end(),
        [](const BanditArm& a, const BanditArm& b) {
            return (a.estimated_reward + a.confidence_bound) < 
                   (b.estimated_reward + b.confidence_bound);
        });
    
    utils::log_debug("Bandit selected: Buy=" + std::to_string(best_arm->thresholds.buy_threshold) + 
                    ", Sell=" + std::to_string(best_arm->thresholds.sell_threshold) + 
                    ", UCB=" + std::to_string(best_arm->estimated_reward + best_arm->confidence_bound));
    
    return best_arm->thresholds;
}

void MultiArmedBanditOptimizer::update_reward(const ThresholdPair& thresholds, double reward) {
    // Find the arm that was pulled
    auto arm_it = std::find_if(arms_.begin(), arms_.end(),
        [&thresholds](const BanditArm& arm) {
            return std::abs(arm.thresholds.buy_threshold - thresholds.buy_threshold) < 0.005 &&
                   std::abs(arm.thresholds.sell_threshold - thresholds.sell_threshold) < 0.005;
        });
    
    if (arm_it != arms_.end()) {
        // Update arm's estimated reward using incremental mean
        arm_it->pull_count++;
        total_pulls_++;
        
        double old_estimate = arm_it->estimated_reward;
        arm_it->estimated_reward = old_estimate + (reward - old_estimate) / arm_it->pull_count;
        
        utils::log_debug("Bandit reward update: Buy=" + std::to_string(thresholds.buy_threshold) + 
                        ", Sell=" + std::to_string(thresholds.sell_threshold) + 
                        ", Reward=" + std::to_string(reward) + 
                        ", New_Est=" + std::to_string(arm_it->estimated_reward));
    }
}

void MultiArmedBanditOptimizer::initialize_arms() {
    // Create a grid of threshold combinations
    for (double buy = 0.55; buy <= 0.85; buy += 0.05) {
        for (double sell = 0.15; sell <= 0.45; sell += 0.05) {
            if (buy > sell + 0.05) { // Ensure minimum gap
                arms_.emplace_back(ThresholdPair(buy, sell));
            }
        }
    }
}

void MultiArmedBanditOptimizer::update_confidence_bounds() {
    for (auto& arm : arms_) {
        if (arm.pull_count == 0) {
            arm.confidence_bound = std::numeric_limits<double>::max();
        } else {
            arm.confidence_bound = std::sqrt(2.0 * std::log(total_pulls_) / arm.pull_count);
        }
    }
}

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER IMPLEMENTATION
// ===================================================================

AdaptiveThresholdManager::AdaptiveThresholdManager(const AdaptiveConfig& config) 
    : config_(config), current_thresholds_(0.55, 0.45) {
    
    // Initialize components
    q_learner_ = std::make_unique<QLearningThresholdOptimizer>();
    bandit_optimizer_ = std::make_unique<MultiArmedBanditOptimizer>();
    regime_detector_ = std::make_unique<AdaptiveMarketRegimeDetector>();
    performance_evaluator_ = std::make_unique<PerformanceEvaluator>();
    
    // Initialize regime-specific thresholds
    initialize_regime_thresholds();
    
    utils::log_info("AdaptiveThresholdManager initialized: Algorithm=" + 
                   std::to_string(static_cast<int>(config_.algorithm)) + 
                   ", LearningRate=" + std::to_string(config_.learning_rate) + 
                   ", ConservativeMode=" + (config_.conservative_mode ? "YES" : "NO"));
}

ThresholdPair AdaptiveThresholdManager::get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
    // Update market state
    current_market_state_ = regime_detector_->analyze_market_state(bar, recent_bars_, signal);
    recent_bars_.push_back(bar);
    if (recent_bars_.size() > 100) {
        recent_bars_.erase(recent_bars_.begin());
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_) {
        utils::log_warning("Circuit breaker active - using conservative thresholds");
        return get_conservative_thresholds();
    }
    
    // Update performance and potentially adjust thresholds
    update_performance_and_learn();
    
    // Get regime-adapted thresholds if enabled
    if (config_.enable_regime_adaptation) {
        return get_regime_adapted_thresholds();
    }
    
    return current_thresholds_;
}

void AdaptiveThresholdManager::process_trade_outcome(const std::string& symbol, TradeAction action, 
                                                    double quantity, double price, double trade_value, double fees,
                                                    double actual_pnl, double pnl_percentage, bool was_profitable) {
    TradeOutcome outcome;
    outcome.symbol = symbol;
    outcome.action = action;
    outcome.quantity = quantity;
    outcome.price = price;
    outcome.trade_value = trade_value;
    outcome.fees = fees;
    outcome.actual_pnl = actual_pnl;
    outcome.pnl_percentage = pnl_percentage;
    outcome.was_profitable = was_profitable;
    outcome.outcome_timestamp = std::chrono::system_clock::now();
    
    performance_evaluator_->add_trade_outcome(outcome);
    
    // Update learning algorithms with reward feedback
    if (learning_enabled_) {
        current_performance_ = performance_evaluator_->calculate_performance_metrics();
        double reward = performance_evaluator_->calculate_reward_signal(current_performance_);
        
        // Update based on algorithm type
        switch (config_.algorithm) {
            case LearningAlgorithm::Q_LEARNING:
                // Q-learning update will happen in next call to update_performance_and_learn()
                break;
                
            case LearningAlgorithm::MULTI_ARMED_BANDIT:
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
                
            case LearningAlgorithm::ENSEMBLE:
                // Update both algorithms
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
        }
    }
    
    // Check for circuit breaker conditions
    check_circuit_breaker();
}

void AdaptiveThresholdManager::update_portfolio_value(double value) {
    performance_evaluator_->add_portfolio_value(value);
}

double AdaptiveThresholdManager::get_learning_progress() const {
    return q_learner_->get_learning_progress();
}

std::string AdaptiveThresholdManager::generate_performance_report() const {
    std::ostringstream report;
    
    report << "=== ADAPTIVE TRADING PERFORMANCE REPORT ===\n";
    report << "Current Thresholds: Buy=" << std::fixed << std::setprecision(3) << current_thresholds_.buy_threshold 
           << ", Sell=" << current_thresholds_.sell_threshold << "\n";
    report << "Market Regime: " << static_cast<int>(current_market_state_.regime) << "\n";
    report << "Total Trades: " << current_performance_.total_trades << "\n";
    report << "Win Rate: " << std::fixed << std::setprecision(1) << (current_performance_.win_rate * 100) << "%\n";
    report << "Profit Factor: " << std::fixed << std::setprecision(2) << current_performance_.profit_factor << "\n";
    report << "Sharpe Ratio: " << std::fixed << std::setprecision(2) << current_performance_.sharpe_ratio << "\n";
    report << "Max Drawdown: " << std::fixed << std::setprecision(1) << (current_performance_.max_drawdown * 100) << "%\n";
    report << "Learning Progress: " << std::fixed << std::setprecision(1) << (get_learning_progress() * 100) << "%\n";
    report << "Circuit Breaker: " << (circuit_breaker_active_ ? "ACTIVE" : "INACTIVE") << "\n";
    
    return report.str();
}

void AdaptiveThresholdManager::initialize_regime_thresholds() {
    // Conservative thresholds for volatile markets
    regime_thresholds_[AdaptiveMarketRegime::BULL_HIGH_VOL] = ThresholdPair(0.65, 0.35);
    regime_thresholds_[AdaptiveMarketRegime::BEAR_HIGH_VOL] = ThresholdPair(0.70, 0.30);
    regime_thresholds_[AdaptiveMarketRegime::SIDEWAYS_HIGH_VOL] = ThresholdPair(0.68, 0.32);
    
    // More aggressive thresholds for stable markets
    regime_thresholds_[AdaptiveMarketRegime::BULL_LOW_VOL] = ThresholdPair(0.58, 0.42);
    regime_thresholds_[AdaptiveMarketRegime::BEAR_LOW_VOL] = ThresholdPair(0.62, 0.38);
    regime_thresholds_[AdaptiveMarketRegime::SIDEWAYS_LOW_VOL] = ThresholdPair(0.60, 0.40);
}

void AdaptiveThresholdManager::update_performance_and_learn() {
    if (!learning_enabled_ || circuit_breaker_active_) {
        return;
    }
    
    // Update performance metrics
    PerformanceMetrics new_performance = performance_evaluator_->calculate_performance_metrics();
    
    // Only learn if we have enough data
    if (new_performance.total_trades < config_.performance_window / 2) {
        return;
    }
    
    // Q-Learning update
    if (config_.algorithm == LearningAlgorithm::Q_LEARNING || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        double reward = performance_evaluator_->calculate_reward_signal(new_performance);
        
        // Select and apply action
        ThresholdAction action = q_learner_->select_action(
            current_market_state_, current_thresholds_, current_performance_);
        
        ThresholdPair new_thresholds = q_learner_->apply_action(current_thresholds_, action);
        
        // Update Q-values if we have previous state
        if (current_performance_.total_trades > 0) {
            q_learner_->update_q_value(
                current_market_state_, current_thresholds_, current_performance_,
                action, reward,
                current_market_state_, new_thresholds, new_performance);
        }
        
        current_thresholds_ = new_thresholds;
    }
    
    // Multi-Armed Bandit update
    if (config_.algorithm == LearningAlgorithm::MULTI_ARMED_BANDIT || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        current_thresholds_ = bandit_optimizer_->select_thresholds();
    }
    
    current_performance_ = new_performance;
}

ThresholdPair AdaptiveThresholdManager::get_regime_adapted_thresholds() {
    auto regime_it = regime_thresholds_.find(current_market_state_.regime);
    if (regime_it != regime_thresholds_.end()) {
        // Blend learned thresholds with regime-specific ones
        ThresholdPair regime_thresholds = regime_it->second;
        double blend_factor = config_.conservative_mode ? 0.7 : 0.3;
        
        return ThresholdPair(
            current_thresholds_.buy_threshold * (1.0 - blend_factor) + 
            regime_thresholds.buy_threshold * blend_factor,
            current_thresholds_.sell_threshold * (1.0 - blend_factor) + 
            regime_thresholds.sell_threshold * blend_factor
        );
    }
    
    return current_thresholds_;
}

ThresholdPair AdaptiveThresholdManager::get_conservative_thresholds() {
    // Return very conservative thresholds during circuit breaker
    return ThresholdPair(0.75, 0.25);
}

void AdaptiveThresholdManager::check_circuit_breaker() {
    // Only activate circuit breaker if we have sufficient trading history
    if (current_performance_.total_trades < 10) {
        return; // Not enough data to make circuit breaker decisions
    }
    
    if (current_performance_.max_drawdown > config_.max_drawdown_limit ||
        current_performance_.win_rate < 0.3 ||
        (current_performance_.total_trades > 20 && current_performance_.profit_factor < 0.8)) {
        
        circuit_breaker_active_ = true;
        learning_enabled_ = false;
        
        utils::log_error("CIRCUIT BREAKER ACTIVATED: Drawdown=" + std::to_string(current_performance_.max_drawdown) + 
                        ", WinRate=" + std::to_string(current_performance_.win_rate) + 
                        ", ProfitFactor=" + std::to_string(current_performance_.profit_factor));
    }
}

} // namespace sentio
```

---

## File: `include/common/time_utils.h`

**Path**: `include/common/time_utils.h`

```cpp
#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>

namespace sentio {

/**
 * @brief Trading session configuration with timezone support
 *
 * Handles market hours, weekends, and timezone conversions.
 * Uses system timezone API for DST-aware calculations.
 */
struct TradingSession {
    std::string timezone_name;  // IANA timezone (e.g., "America/New_York")
    int market_open_hour{9};
    int market_open_minute{30};
    int market_close_hour{16};
    int market_close_minute{0};

    TradingSession(const std::string& tz_name = "America/New_York")
        : timezone_name(tz_name) {}

    /**
     * @brief Check if given time is during regular trading hours
     * @param tp System clock time point
     * @return true if within market hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a weekday
     * @param tp System clock time point
     * @return true if Monday-Friday
     */
    bool is_weekday(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a trading day (weekday, not holiday)
     * @param tp System clock time point
     * @return true if trading day
     * @note Holiday calendar not yet implemented - returns weekday check only
     */
    bool is_trading_day(const std::chrono::system_clock::time_point& tp) const {
        // TODO: Add holiday calendar check
        return is_weekday(tp);
    }

    /**
     * @brief Get local time string in timezone
     * @param tp System clock time point
     * @return Formatted time string "YYYY-MM-DD HH:MM:SS TZ"
     */
    std::string to_local_string(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Convert system time to local time in configured timezone
     * @param tp System clock time point
     * @return Local time struct
     */
    std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const;
};

/**
 * @brief Get current time (always uses system UTC, convert to ET via TradingSession)
 * @return System clock time point
 */
inline std::chrono::system_clock::time_point now() {
    return std::chrono::system_clock::now();
}

/**
 * @brief Format timestamp to ISO 8601 string
 * @param tp System clock time point
 * @return ISO formatted string "YYYY-MM-DDTHH:MM:SSZ"
 */
std::string to_iso_string(const std::chrono::system_clock::time_point& tp);

/**
 * @brief Centralized ET Time Manager - ALL time operations should use this
 *
 * This class ensures consistent ET timezone handling across the entire system.
 * No direct time conversions should be done elsewhere.
 */
class ETTimeManager {
public:
    ETTimeManager() : session_("America/New_York") {}

    /**
     * @brief Get current ET time as formatted string
     * @return "YYYY-MM-DD HH:MM:SS ET"
     */
    std::string get_current_et_string() const {
        return session_.to_local_string(now());
    }

    /**
     * @brief Get current ET time components
     * @return struct tm in ET timezone
     */
    std::tm get_current_et_tm() const {
        return session_.to_local_time(now());
    }

    /**
     * @brief Get current ET date as string (YYYY-MM-DD format)
     * @return Date string in format "2025-10-09"
     */
    std::string get_current_et_date() const {
        auto et_tm = get_current_et_tm();
        char buffer[11];  // "YYYY-MM-DD\0"
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                     et_tm.tm_year + 1900,
                     et_tm.tm_mon + 1,
                     et_tm.tm_mday);
        return std::string(buffer);
    }

    /**
     * @brief Check if current time is during regular trading hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours() const {
        return session_.is_regular_hours(now()) && session_.is_trading_day(now());
    }

    /**
     * @brief Check if current time is in EOD liquidation window (3:55 PM - 4:00 PM ET)
     * Uses a 5-minute window instead of exact time to ensure liquidation happens
     */
    bool is_eod_liquidation_window() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // EOD window: 3:55 PM - 4:00 PM ET
        if (hour == 15 && minute >= 55) return true;  // 3:55-3:59 PM
        if (hour == 16 && minute == 0) return true;   // 4:00 PM exactly

        return false;
    }

    /**
     * @brief Check if current time is mid-day optimization window (15:15 PM ET exactly)
     * Used for adaptive parameter tuning based on comprehensive data (historical + today's bars)
     */
    bool is_midday_optimization_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Mid-day optimization: 15:15 PM ET (3:15pm) - during trading hours
        return (hour == 15 && minute == 15);
    }

    /**
     * @brief Check if we should liquidate positions on startup (started outside trading hours with open positions)
     */
    bool should_liquidate_on_startup(bool has_positions) const {
        if (!has_positions) return false;

        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;

        // If started after market close (after 4 PM) or before market open (before 9:30 AM),
        // and we have positions, we should liquidate
        bool after_hours = (hour >= 16) || (hour < 9) || (hour == 9 && et_tm.tm_min < 30);

        return after_hours;
    }

    /**
     * @brief Get minutes since midnight ET
     */
    int get_et_minutes_since_midnight() const {
        auto et_tm = get_current_et_tm();
        return et_tm.tm_hour * 60 + et_tm.tm_min;
    }

    /**
     * @brief Access to underlying TradingSession
     */
    const TradingSession& session() const { return session_; }

private:
    TradingSession session_;
};

/**
 * @brief Get Unix timestamp in microseconds
 * @param tp System clock time point
 * @return Microseconds since epoch
 */
inline uint64_t to_unix_micros(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()
    ).count();
}

} // namespace sentio
```

---

## File: `src/common/time_utils.cpp`

**Path**: `src/common/time_utils.cpp`

```cpp
#include "common/time_utils.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace sentio {

std::tm TradingSession::to_local_time(const std::chrono::system_clock::time_point& tp) const {
    // C++20 thread-safe timezone conversion using zoned_time
    // This replaces the unsafe setenv("TZ") approach

    #if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        // Use C++20 timezone database
        try {
            const auto* tz = std::chrono::locate_zone(timezone_name);
            std::chrono::zoned_time zt{tz, tp};

            // Convert zoned_time to std::tm
            auto local_time = zt.get_local_time();
            auto local_dp = std::chrono::floor<std::chrono::days>(local_time);
            auto ymd = std::chrono::year_month_day{local_dp};
            auto tod = std::chrono::hh_mm_ss{local_time - local_dp};

            std::tm result{};
            result.tm_year = static_cast<int>(ymd.year()) - 1900;
            result.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
            result.tm_mday = static_cast<unsigned>(ymd.day());
            result.tm_hour = tod.hours().count();
            result.tm_min = tod.minutes().count();
            result.tm_sec = tod.seconds().count();

            // Calculate day of week
            auto dp_sys = std::chrono::sys_days{ymd};
            auto weekday = std::chrono::weekday{dp_sys};
            result.tm_wday = weekday.c_encoding();

            // DST info
            auto info = zt.get_info();
            result.tm_isdst = (info.save != std::chrono::minutes{0}) ? 1 : 0;

            return result;

        } catch (const std::exception& e) {
            // Fallback: if timezone not found, use UTC
            auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm result;
            gmtime_r(&tt, &result);
            return result;
        }
    #else
        // Fallback for C++17: use old setenv approach (NOT thread-safe)
        // This should not happen since we require C++20
        #warning "C++20 chrono timezone database not available - using unsafe setenv fallback"

        auto tt = std::chrono::system_clock::to_time_t(tp);

        const char* old_tz = getenv("TZ");
        setenv("TZ", timezone_name.c_str(), 1);
        tzset();

        std::tm local_tm;
        localtime_r(&tt, &local_tm);

        if (old_tz) {
            setenv("TZ", old_tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        return local_tm;
    #endif
}

bool TradingSession::is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    // Calculate minutes since midnight
    int open_mins = market_open_hour * 60 + market_open_minute;
    int close_mins = market_close_hour * 60 + market_close_minute;
    int current_mins = hour * 60 + minute;

    return current_mins >= open_mins && current_mins < close_mins;
}

bool TradingSession::is_weekday(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    // tm_wday: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int wday = local_tm.tm_wday;

    return wday >= 1 && wday <= 5;  // Monday - Friday
}

std::string TradingSession::to_local_string(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    ss << " " << timezone_name;

    return ss.str();
}

std::string to_iso_string(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm;
    gmtime_r(&tt, &utc_tm);

    std::stringstream ss;
    ss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");

    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms << "Z";

    return ss.str();
}

} // namespace sentio
```

---

## File: `include/common/types.h`

**Path**: `include/common/types.h`

```cpp
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// System Constants
// -----------------------------------------------------------------------------

/// Standard block size for backtesting and signal processing
/// One block represents approximately 8 hours of trading (480 minutes)
/// This constant ensures consistency across strattest, trade, and audit commands
static constexpr size_t STANDARD_BLOCK_SIZE = 480;

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp_ms and symbol at load time
    uint64_t bar_id = 0;
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
    // Derived fields for traceability/debugging (filled by loader)
    uint32_t sequence_num = 0;   // Position in original dataset
    uint16_t block_num = 0;      // STANDARD_BLOCK_SIZE partition index
    std::string date_str;        // e.g. "2025-09-09" for human-readable logs
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio
```

---

## File: `src/common/types.cpp`

**Path**: `src/common/types.cpp`

```cpp
#include "common/types.h"
#include "common/utils.h"

// =============================================================================
// Implementation: common/types.cpp
// Provides serialization helpers for PortfolioState.
// =============================================================================

namespace sentio {

// Serialize a PortfolioState to a minimal JSON representation.
// The structure is designed for audit logs and DB storage via adapters.
std::string PortfolioState::to_json() const {
    // Flatten positions into a simple key/value map for lightweight JSON.
    // For a richer schema, replace with a full JSON library in adapters.
    std::map<std::string, std::string> m;
    m["cash_balance"] = std::to_string(cash_balance);
    m["total_equity"] = std::to_string(total_equity);
    m["unrealized_pnl"] = std::to_string(unrealized_pnl);
    m["realized_pnl"] = std::to_string(realized_pnl);
    m["timestamp_ms"] = std::to_string(timestamp_ms);

    // Encode position count; individual positions can be stored elsewhere
    // or serialized as a separate artifact for brevity in logs.
    m["position_count"] = std::to_string(positions.size());
    return utils::to_json(m);
}

// Parse JSON into PortfolioState. Only top-level numeric fields are restored.
PortfolioState PortfolioState::from_json(const std::string& json_str) {
    PortfolioState s;
    auto m = utils::from_json(json_str);
    if (m.count("cash_balance")) s.cash_balance = std::stod(m["cash_balance"]);
    if (m.count("total_equity")) s.total_equity = std::stod(m["total_equity"]);
    if (m.count("unrealized_pnl")) s.unrealized_pnl = std::stod(m["unrealized_pnl"]);
    if (m.count("realized_pnl")) s.realized_pnl = std::stod(m["realized_pnl"]);
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    return s;
}

} // namespace sentio


```

---

