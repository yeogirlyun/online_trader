# Mock Infrastructure Quick Start Guide

## Quick Reference

### 1. Basic Replay Mode (Fastest Way to Test)

```cpp
#include "live/mock_config.h"

// Setup
MockConfig config;
config.mode = MockMode::REPLAY_HISTORICAL;
config.csv_data_path = "data/equities/SPY_4blocks.csv";
config.speed_multiplier = 39.0;  // 39x speed

// Create clients
auto broker = TradingInfrastructureFactory::create_broker(config);
auto feed = TradingInfrastructureFactory::create_bar_feed(config);

// Use normally (no changes to existing code needed)
```

### 2. EOD Testing with Crash Simulation

```cpp
#include "live/mock_session_state.h"

MockLiveSession session("eod_test", "data/equities/SPY_4blocks.csv", 39.0);
session.simulate_crash_at_time("15:00:00");  // Crash at 3:00 PM ET
session.start();

// After restart
auto state = session.state();
session.verify_eod_idempotency();  // Check if EOD is idempotent
```

### 3. Market Impact Simulation

```cpp
MockConfig config;
config.mode = MockMode::REPLAY_HISTORICAL;
config.enable_market_impact = true;
config.market_impact_bps = 5.0;  // 5 bps temporary impact
config.bid_ask_spread_bps = 2.0;  // 2 bps spread

auto broker = TradingInfrastructureFactory::create_broker(config);
// Orders will now include realistic slippage
```

### 4. Fill Behavior Options

```cpp
// Immediate (unrealistic but fast)
config.fill_behavior = FillBehavior::IMMEDIATE_FULL;

// Delayed full fills (realistic)
config.fill_behavior = FillBehavior::DELAYED_FULL;

// Partial fills (most realistic)
config.fill_behavior = FillBehavior::DELAYED_PARTIAL;
```

### 5. Performance Profiling

```cpp
MockLiveSession session("profile_test", "data.csv", 39.0);
session.start();

auto metrics = session.state().metrics();
metrics.print_performance_report();

// Output:
// Total Time: 1234.56 ms
// - Strategy: 800.0 ms (64.8%)
// - Broker: 300.0 ms (24.3%)
// - Feed: 134.56 ms (10.9%)
// Avg Time/Bar: 0.79 ms
```

## Mock Modes Explained

### LIVE (Production)
```cpp
config.mode = MockMode::LIVE;
// Uses real Alpaca + Polygon
```

### REPLAY_HISTORICAL (Testing)
```cpp
config.mode = MockMode::REPLAY_HISTORICAL;
// Exact replay of historical data
```

### STRESS_TEST (Robustness)
```cpp
config.mode = MockMode::STRESS_TEST;
config.enable_random_gaps = true;
config.enable_high_volatility = true;
config.volatility_multiplier = 2.0;
// Tests strategy under extreme conditions
```

### PARAMETER_SWEEP (Optimization)
```cpp
config.mode = MockMode::PARAMETER_SWEEP;
config.speed_multiplier = 100.0;  // Maximum speed
config.enable_market_impact = false;  // Pure signal testing
// For rapid parameter optimization
```

### REGRESSION_TEST (CI/CD)
```cpp
config.mode = MockMode::REGRESSION_TEST;
// Verify bug fixes and features
```

## Speed Multiplier Guide

| Multiplier | Use Case | Time for 4 Blocks |
|-----------|----------|-------------------|
| 1x | Real-time testing | ~8 hours |
| 10x | Slow validation | ~48 minutes |
| 39x | **Recommended** | ~12 minutes |
| 100x | Parameter sweep | ~5 minutes |
| 1000x | Limited by CPU | ~30 seconds |

## Common Patterns

### Pattern 1: Test Strategy Changes
```cpp
// 1. Make strategy changes
// 2. Run quick replay
MockConfig config;
config.mode = MockMode::REPLAY_HISTORICAL;
config.csv_data_path = "data/equities/SPY_4blocks.csv";
config.speed_multiplier = 39.0;

// 3. Compare results with baseline
```

### Pattern 2: Verify Bug Fix
```cpp
// 1. Reproduce bug with historical data
MockConfig config;
config.mode = MockMode::REGRESSION_TEST;
config.checkpoint_file = "bug_scenario_checkpoint.json";

// 2. Apply fix
// 3. Re-run and verify resolution
```

### Pattern 3: Optimize Parameters
```cpp
// In Optuna objective function:
MockConfig config;
config.mode = MockMode::PARAMETER_SWEEP;
config.speed_multiplier = 100.0;

// Run 1000 trials in < 1 hour
```

### Pattern 4: Production Validation
```cpp
// 1. Collect today's live data
// 2. Replay at EOD
MockConfig config;
config.mode = MockMode::REPLAY_HISTORICAL;
config.csv_data_path = "today_live_data.csv";

// 3. Verify trades match live session
```

## Integration with Existing Code

### Before (Hard-coded Alpaca)
```cpp
AlpacaClient alpaca(api_key, secret, true);
PolygonClient polygon(url, key);
```

### After (Polymorphic with Mock Support)
```cpp
auto broker = TradingInfrastructureFactory::create_broker(
    config, api_key, secret);
auto feed = TradingInfrastructureFactory::create_bar_feed(
    config, url, key);

// Rest of code unchanged
```

## Checkpoint/Restore Example

```cpp
// Session 1: Run and crash
MockLiveSession session1("test", "data.csv", 39.0);
session1.simulate_crash_at_time("14:00:00");
session1.start();

// Save state
session1.state().save_to_file("checkpoint.json");

// Session 2: Restore and continue
auto restored_state = MockSessionState::load_from_file("checkpoint.json");
MockLiveSession session2("test_resume", "data.csv", 39.0);
session2.simulate_restart_with_state(restored_state);
session2.start();
```

## Performance Metrics Breakdown

```cpp
auto metrics = session.state().metrics();

// Timing
std::cout << "Strategy time: " << metrics.total_strategy_time.count() / 1e6 << " ms\n";
std::cout << "Broker time: " << metrics.total_broker_time.count() / 1e6 << " ms\n";

// Throughput
std::cout << "Bars/sec: " << (metrics.bars_processed / total_time_sec) << "\n";

// Trading
std::cout << "Total trades: " << metrics.total_trades << "\n";
std::cout << "Total slippage: $" << metrics.total_slippage << "\n";
std::cout << "Total commission: $" << metrics.total_commission << "\n";
```

## CLI Integration (Proposed)

```bash
# Replay mode
./build/sentio_cli live-trade \
  --mock-mode replay \
  --mock-data data/equities/SPY_4blocks.csv \
  --speed 39

# Stress test mode
./build/sentio_cli live-trade \
  --mock-mode stress \
  --mock-data data/equities/SPY_4blocks.csv \
  --enable-gaps \
  --volatility-multiplier 2.0

# EOD crash test
./build/sentio_cli live-trade \
  --mock-mode regression \
  --mock-data data/equities/SPY_4blocks.csv \
  --crash-at "15:00:00"
```

## Troubleshooting

### Issue: Replay too slow
```cpp
// Increase speed multiplier
config.speed_multiplier = 100.0;

// Disable expensive features
config.enable_market_impact = false;
config.fill_behavior = FillBehavior::IMMEDIATE_FULL;
```

### Issue: Unrealistic fills
```cpp
// Enable realistic simulation
config.fill_behavior = FillBehavior::DELAYED_PARTIAL;
config.enable_market_impact = true;
config.market_impact_bps = 5.0;
```

### Issue: State not saving
```cpp
// Ensure checkpoints are enabled
config.enable_checkpoints = true;
config.checkpoint_file = "checkpoint.json";

// Manually save after each phase
session.state().save_to_file("manual_checkpoint.json");
```

## Best Practices

1. **Always validate data first**
   ```cpp
   auto feed = std::make_unique<MockBarFeedReplay>(csv_path);
   if (!feed->validate_data_integrity()) {
       throw std::runtime_error("Invalid data");
   }
   ```

2. **Use appropriate speed for task**
   - Development/debugging: 10-39x
   - Parameter optimization: 100x+
   - Final validation: 1x (real-time)

3. **Enable market impact for realistic testing**
   ```cpp
   config.enable_market_impact = true;  // Unless pure signal testing
   ```

4. **Save checkpoints at critical points**
   ```cpp
   // Before EOD
   session.state().save_checkpoint(create_checkpoint("pre_eod"));

   // After EOD
   session.state().save_checkpoint(create_checkpoint("post_eod"));
   ```

5. **Profile before optimizing**
   ```cpp
   // Run once with profiling
   session.start();
   session.state().metrics().print_performance_report();

   // Optimize bottlenecks
   ```

## Testing Checklist

- [ ] Replay 4-block session at 39x speed
- [ ] Verify trades match expected output
- [ ] Test EOD crash at 3:00 PM
- [ ] Verify EOD idempotency
- [ ] Run stress test with gaps
- [ ] Profile performance (target: <1ms/bar)
- [ ] Test checkpoint save/restore
- [ ] Validate position reconciliation

---

**Last Updated:** 2025-10-09
**Status:** Ready for integration
