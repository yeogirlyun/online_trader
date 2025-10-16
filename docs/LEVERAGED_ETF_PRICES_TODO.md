# TODO: Load Leveraged ETF Prices in Mock Mode

## Status
✅ **Downloaded data from Polygon**: SH, SDS, SPXL for Oct 7, 2025
⏳ **Integration needed**: Load these prices into MockBroker

## Downloaded Files
Located in `/tmp/`:
- `SH_yesterday.csv` - 369 RTH bars (9:30-4:00 PM)
- `SDS_yesterday.csv` - 377 RTH bars
- `SPXL_yesterday.csv` - 386 RTH bars
- `SPY_yesterday.csv` - 391 RTH bars (already in use)

Format: `date_str,timestamp_sec,open,high,low,close,volume`

## Current Problem
Mock mode currently uses dummy prices for SH, SDS, SPXL:
```cpp
// src/cli/live_trade_command.cpp:551-554
mock_broker->update_market_price("SPY", bar.close);
mock_broker->update_market_price("SPXL", bar.close * 1.0);  // WRONG - dummy price
mock_broker->update_market_price("SH", bar.close * 1.0);    // WRONG - dummy price
mock_broker->update_market_price("SDS", bar.close * 1.0);   // WRONG - dummy price
```

This causes all BEAR state transitions to fail:
```
ERROR: ❌ CRITICAL: Target state is BEAR_1X_NX but failed to calculate positions
       (likely price fetch failure)
```

## Solution Required

### Step 1: Add member variable to LiveTrader class
**File**: `src/cli/live_trade_command.cpp:196` (after `entry_equity_`)

```cpp
// Mock mode: Leveraged ETF prices loaded from CSV
std::unordered_map<uint64_t, std::unordered_map<std::string, double>> leveraged_prices_;
```

### Step 2: Load CSVs during initialization
**File**: `src/cli/live_trade_command.cpp` (in constructor or `run()` method)

```cpp
// In mock mode, load leveraged ETF prices
if (is_mock_mode_) {
    log_system("Loading leveraged ETF prices for mock mode...");
    leveraged_prices_ = load_leveraged_prices("/tmp");
}
```

### Step 3: Add helper function to load CSV files
**File**: `src/cli/live_trade_command.cpp` (before `class LiveTrader`)

```cpp
std::unordered_map<uint64_t, std::unordered_map<std::string, double>>
load_leveraged_prices(const std::string& base_path) {
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> prices;

    std::vector<std::string> symbols = {"SH", "SDS", "SPXL"};

    for (const auto& symbol : symbols) {
        std::string filepath = base_path + "/" + symbol + "_yesterday.csv";
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "⚠️  Warning: Could not load " + filepath << std::endl;
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string date_str, ts_str, o, h, l, close_str, v;

            if (std::getline(iss, date_str, ',') &&
                std::getline(iss, ts_str, ',') &&
                std::getline(iss, o, ',') &&
                std::getline(iss, h, ',') &&
                std::getline(iss, l, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, v)) {

                uint64_t timestamp_sec = std::stoull(ts_str);
                double close_price = std::stod(close_str);

                prices[timestamp_sec][symbol] = close_price;
            }
        }
    }

    return prices;
}
```

### Step 4: Update prices in on_new_bar()
**File**: `src/cli/live_trade_command.cpp:551-554`

**Replace**:
```cpp
mock_broker->update_market_price("SPY", bar.close);
mock_broker->update_market_price("SPXL", bar.close * 1.0);
mock_broker->update_market_price("SH", bar.close * 1.0);
mock_broker->update_market_price("SDS", bar.close * 1.0);
```

**With**:
```cpp
// Update SPY price from bar
mock_broker->update_market_price("SPY", bar.close);

// Update leveraged ETF prices from loaded CSV data
uint64_t bar_ts_sec = bar.timestamp_ms / 1000;

if (leveraged_prices_.count(bar_ts_sec)) {
    const auto& prices_at_ts = leveraged_prices_[bar_ts_sec];

    if (prices_at_ts.count("SPXL")) {
        mock_broker->update_market_price("SPXL", prices_at_ts.at("SPXL"));
    }
    if (prices_at_ts.count("SH")) {
        mock_broker->update_market_price("SH", prices_at_ts.at("SH"));
    }
    if (prices_at_ts.count("SDS")) {
        mock_broker->update_market_price("SDS", prices_at_ts.at("SDS"));
    }
} else {
    // Fallback to SPY price if timestamp not found
    mock_broker->update_market_price("SPXL", bar.close);
    mock_broker->update_market_price("SH", bar.close);
    mock_broker->update_market_price("SDS", bar.close);
}
```

## Testing
After implementation:
1. Rebuild: `cmake --build build --target sentio_cli`
2. Run mock session: `build/sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv --mock-speed 0`
3. Verify:
   - No "failed to calculate positions" errors
   - BEAR state transitions work (SH/SDS positions)
   - EOD liquidation at 3:58 PM
   - No trades after 3:58 PM

## Alternative: Download Script
The download script is available at `/tmp/download_leveraged_data_polygon.py`

Usage:
```bash
python3 /tmp/download_leveraged_data_polygon.py
```

This will download fresh data for any target date by changing `TARGET_DATE` in the script.

## Priority
**HIGH** - This blocks proper mock session testing. Without this fix:
- All BEAR states fail
- Can't test full strategy behavior
- Mock session incomplete
