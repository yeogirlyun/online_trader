# Unified Live/Mock Trading Implementation Plan

**Date:** 2025-10-09
**Goal:** One script, one codebase, perfect consistency between live and mock
**Principle:** Mock mode = Live mode + different data/broker layers

## Current State

### Live Trading (Working)
```cpp
// src/cli/live_trade_command.cpp
class LiveTrader {
    AlpacaClient alpaca_;      // Hardcoded - talks to real broker
    PolygonClient polygon_;    // Hardcoded - real-time WebSocket
    // ... trading logic ...
};

int LiveTradeCommand::execute() {
    LiveTrader trader(alpaca_key, alpaca_secret, polygon_url, polygon_key, log_dir);
    trader.run();
}
```

### Mock Infrastructure (Created, Not Integrated)
```cpp
// include/live/*.h (Already exists!)
class IBrokerClient { /* interface */ };
class IBarFeed { /* interface */ };
class MockBroker : public IBrokerClient { /* mock broker */ };
class MockBarFeedReplay : public IBarFeed { /* CSV replay */ };
class AlpacaClientAdapter : public IBrokerClient { /* wraps AlpacaClient */ };
class PolygonClientAdapter : public IBarFeed { /* wraps PolygonClient */ };
class TradingInfrastructureFactory { /* creates mock or live */ };
```

## Required Changes

### 1. Modify `LiveTrader` to Use Interfaces

**Before (Hardcoded):**
```cpp
class LiveTrader {
private:
    AlpacaClient alpaca_;          // Hardcoded!
    PolygonClient polygon_;        // Hardcoded!
};
```

**After (Interface-Based):**
```cpp
class LiveTrader {
private:
    std::unique_ptr<IBrokerClient> broker_;     // Interface!
    std::unique_ptr<IBarFeed> bar_feed_;        // Interface!
};
```

### 2. Factory-Based Construction

**Before:**
```cpp
int LiveTradeCommand::execute() {
    LiveTrader trader(alpaca_key, alpaca_secret, polygon_url, polygon_key, log_dir);
    trader.run();
}
```

**After:**
```cpp
int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse --mock flag
    bool is_mock = has_flag(args, "--mock");
    std::string mock_data_file = get_arg(args, "--mock-data", "");

    // Create broker and bar feed based on mode
    std::unique_ptr<IBrokerClient> broker;
    std::unique_ptr<IBarFeed> bar_feed;

    if (is_mock) {
        // MOCK MODE
        MockConfig config;
        config.mode = MockMode::REPLAY_HISTORICAL;
        config.initial_capital = 100000.0;
        config.speed_multiplier = 39.0;  // Fast replay
        config.data_file = mock_data_file;

        broker = TradingInfrastructureFactory::create_broker(config);
        bar_feed = TradingInfrastructureFactory::create_bar_feed(config);

        std::cout << "🎭 MOCK MODE: Replaying " << mock_data_file << " at 39x speed\n";
    } else {
        // LIVE MODE
        MockConfig config;  // Use same config struct
        config.mode = MockMode::LIVE;
        config.alpaca_key = alpaca_key;
        config.alpaca_secret = alpaca_secret;
        config.polygon_url = polygon_url;
        config.polygon_key = polygon_key;

        broker = TradingInfrastructureFactory::create_broker(config);
        bar_feed = TradingInfrastructureFactory::create_bar_feed(config);

        std::cout << "📈 LIVE MODE: Connected to Alpaca + Polygon\n";
    }

    // Create trader with injected dependencies
    LiveTrader trader(std::move(broker), std::move(bar_feed), log_dir);
    trader.run();
}
```

### 3. Update `TradingInfrastructureFactory`

```cpp
// src/live/mock_config.cpp

std::unique_ptr<IBrokerClient> TradingInfrastructureFactory::create_broker(const MockConfig& config) {
    switch (config.mode) {
        case MockMode::LIVE:
            // Wrap real AlpacaClient in adapter
            return std::make_unique<AlpacaClientAdapter>(
                config.alpaca_key, config.alpaca_secret, true /* paper */
            );

        case MockMode::REPLAY_HISTORICAL:
        case MockMode::STRESS_TEST:
        case MockMode::PARAMETER_SWEEP:
        case MockMode::REGRESSION_TEST:
            // Use MockBroker
            return std::make_unique<MockBroker>(
                config.initial_capital,
                config.commission_per_share,
                config.fill_behavior
            );
    }
}

std::unique_ptr<IBarFeed> TradingInfrastructureFactory::create_bar_feed(const MockConfig& config) {
    switch (config.mode) {
        case MockMode::LIVE:
            // Wrap real PolygonClient in adapter
            return std::make_unique<PolygonClientAdapter>(
                config.polygon_url, config.polygon_key
            );

        case MockMode::REPLAY_HISTORICAL:
            // Use CSV replay
            return std::make_unique<MockBarFeedReplay>(
                config.data_file,
                config.speed_multiplier
            );

        // ... other modes ...
    }
}
```

### 4. Update `LiveTrader` Constructor

**Before:**
```cpp
LiveTrader::LiveTrader(
    const std::string& alpaca_key,
    const std::string& alpaca_secret,
    const std::string& polygon_url,
    const std::string& polygon_key,
    const std::string& log_dir
) : alpaca_(alpaca_key, alpaca_secret, true)
  , polygon_(polygon_url, polygon_key)
  , log_dir_(log_dir)
  // ...
{ }
```

**After:**
```cpp
LiveTrader::LiveTrader(
    std::unique_ptr<IBrokerClient> broker,
    std::unique_ptr<IBarFeed> bar_feed,
    const std::string& log_dir
) : broker_(std::move(broker))
  , bar_feed_(std::move(bar_feed))
  , log_dir_(log_dir)
  // ...
{ }
```

### 5. Update Broker/Feed Calls

**Before (Direct Calls):**
```cpp
void LiveTrader::run() {
    auto account = alpaca_.get_account();          // Direct call
    polygon_.subscribe(symbols);                    // Direct call
    polygon_.start([this](const Bar& bar) { ... }); // Direct call
}

void LiveTrader::execute_trade(...) {
    alpaca_.place_order(...);                       // Direct call
}
```

**After (Interface Calls):**
```cpp
void LiveTrader::run() {
    auto account = broker_->get_account();            // Interface call
    bar_feed_->subscribe(symbols);                    // Interface call
    bar_feed_->start([this](const std::string& symbol, const Bar& bar) {
        on_new_bar(bar);
    });
}

void LiveTrader::execute_trade(...) {
    broker_->place_order(...);                        // Interface call
}
```

## Usage Examples

### Live Trading (As Before)
```bash
# Source credentials
source config.env

# Run live trading (no changes to workflow!)
./build/sentio_cli live-trade
```

Output:
```
📈 LIVE MODE: Connected to Alpaca + Polygon
=== OnlineTrader v1.0 Live Paper Trading Started ===
Connecting to Alpaca Paper Trading...
✓ Connected - Account: PA3NCBT07OJZJULDJR5V
  Starting Capital: $100000.00
Connecting to Polygon proxy...
✓ Connected to Polygon
✓ Subscribed to SPY, SPXL, SH, SDS

Initializing OnlineEnsemble strategy...
Loaded 7864 bars from warmup file
=== Starting Warmup Process ===
...
```

### Mock Trading (New!)
```bash
# Extract yesterday's data
tail -391 data/equities/SPY_RTH_NH.csv > /tmp/SPY_yesterday.csv

# Run mock session (replays yesterday)
./build/sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv
```

Output:
```
🎭 MOCK MODE: Replaying /tmp/SPY_yesterday.csv at 39x speed
=== OnlineTrader v1.0 Mock Trading Started ===
Mock Broker initialized with $100,000.00
Mock Bar Feed: 390 bars loaded (39x speed)
✓ Mock account created

Initializing OnlineEnsemble strategy...
Loaded 7864 bars from warmup file
=== Starting Warmup Process ===
  Target: 3900 bars
  Available: 7864 bars

✓ Feature Engine Warmup Complete (64 bars)
✓ Strategy Warmup Complete (3964 bars)
✓ Predictor trained: 3899 samples

=== Live trading active (Mock Replay) ===
📊 BAR #7865 Received (Replay)
  Time: 2025-10-07 09:30:00 ET (at 39x speed)
  OHLC: O=$641.20 H=$641.45 L=$641.10 C=$641.30
...
[10 minutes later - full day replayed]
...
=== Trading Session Complete ===
Final Capital: $100,046.00
Total Return: +0.046%
```

## Files to Modify

### 1. **src/cli/live_trade_command.cpp**
- Change `LiveTrader` to use `IBrokerClient*` and `IBarFeed*`
- Update constructor to accept injected dependencies
- Replace all `alpaca_.method()` with `broker_->method()`
- Replace all `polygon_.method()` with `bar_feed_->method()`
- Add `--mock` flag parsing in `execute()`

### 2. **include/cli/live_trade_command.hpp**
- Update `LiveTrader` class declaration
- Add `#include "live/broker_client_interface.h"`
- Add `#include "live/bar_feed_interface.h"`

### 3. **src/live/mock_config.cpp**
- Enhance `TradingInfrastructureFactory::create_broker()` to support LIVE mode
- Enhance `TradingInfrastructureFactory::create_bar_feed()` to support LIVE mode
- Add `AlpacaClientAdapter` and `PolygonClientAdapter` creation

### 4. **CMakeLists.txt** (if needed)
- Ensure mock infrastructure is linked to `sentio_cli`

## Benefits

### ✅ Perfect Consistency
- **Same warmup** (7,864 bars loaded and processed identically)
- **Same strategy initialization** (feature engine + EWRLS predictor)
- **Same trading logic** (PSM, risk management, EOD)
- **Same logging** (signals, trades, positions, decisions)
- **Same dashboard** (use same tool for both modes)

### ✅ Zero Duplication
- One `LiveTrader` class for both modes
- One warmup function `warmup_strategy()`
- One trading loop `on_new_bar()`
- One PSM logic
- One risk management

### ✅ Easy Testing
```bash
# Test mock first (fast!)
./build/sentio_cli live-trade --mock --mock-data yesterday.csv
# Takes 10 minutes, validates everything

# If mock looks good, run live (with confidence!)
./build/sentio_cli live-trade
# Same code, just different data source
```

### ✅ Debugging
If live trading has unexpected behavior:
1. Capture live bars to CSV
2. Replay in mock mode with `--mock`
3. Debug with same exact code path
4. Fix issue once, fixes both modes

## Implementation Order

1. ✅ **Already Done:** Mock infrastructure created
   - IBrokerClient, IBarFeed interfaces
   - MockBroker, MockBarFeedReplay implementations
   - AlpacaClientAdapter, PolygonClientAdapter wrappers
   - TradingInfrastructureFactory

2. **Next:** Modify `LiveTrader` to use interfaces
   - Change member variables to `std::unique_ptr<I*>`
   - Update constructor signature
   - Replace direct calls with interface calls

3. **Next:** Add `--mock` flag support
   - Parse `--mock` and `--mock-data` arguments
   - Create broker/feed via factory based on mode
   - Inject into LiveTrader constructor

4. **Next:** Test mock mode
   - Extract yesterday's 390 bars
   - Run `sentio_cli live-trade --mock --mock-data ...`
   - Verify warmup, trading, EOD all work

5. **Next:** Verify live mode still works
   - Run `sentio_cli live-trade` (no flags)
   - Ensure adapters work correctly
   - Confirm no regressions

6. **Final:** Document and deploy
   - Update README with `--mock` usage
   - Add to launch scripts
   - Deploy to production

## Expected Timeline

- **Interface integration:** 2-3 hours (modify LiveTrader)
- **Factory setup:** 1 hour (enhance create_broker/create_bar_feed)
- **Testing mock mode:** 1 hour (run and debug)
- **Testing live mode:** 1 hour (verify no regressions)
- **Documentation:** 30 minutes

**Total:** ~6 hours of focused work

## Success Criteria

- [ ] `sentio_cli live-trade` runs unchanged (live mode)
- [ ] `sentio_cli live-trade --mock --mock-data file.csv` replays CSV
- [ ] Both modes use identical warmup process
- [ ] Both modes produce identical log formats
- [ ] Both modes generate same dashboard
- [ ] Mock mode runs at 39x speed (~10 min for 6.5 hour day)
- [ ] Live mode runs at 1x speed (real-time)

---

**Status:** Ready to implement
**Owner:** Claude + User
**Priority:** High (enables rapid iteration and testing)
