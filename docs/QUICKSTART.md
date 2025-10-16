# Online Trader - Quick Start Guide

Get up and running with Online Trader in 5 minutes!

## Prerequisites

### macOS (recommended)

```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required dependencies
brew install cmake eigen

# Optional: Install additional dependencies
brew install nlohmann-json googletest
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y cmake libeigen3-dev
sudo apt-get install -y nlohmann-json3-dev googletest  # optional
```

## Build

### Option 1: Using the build script (recommended)

```bash
./build.sh Release
```

Or for a clean build:

```bash
./build.sh Release clean
```

### Option 2: Manual build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

## Verify Installation

```bash
# Check that executables were built
ls -lh build/sentio_cli
ls -lh build/test_online_trade

# Test CLI
./build/sentio_cli --help
```

## First Run

### 1. Prepare Sample Data

If you don't have data yet, you can use the CSV to binary converter:

```bash
# Convert CSV market data to binary format
./build/csv_to_binary_converter \
  --input path/to/market_data.csv \
  --output data/futures.bin
```

Expected CSV format:
```
timestamp,open,high,low,close,volume
2024-01-01 09:30:00,100.0,101.0,99.0,100.5,1000
...
```

### 2. Run Sanity Check

```bash
./build/sentio_cli online-sanity-check \
  --config config/enhanced_psm_config.json
```

### 3. Test Online Learning

```bash
./build/test_online_trade \
  --data data/futures.bin \
  --window 1000 \
  --learning-rate 0.01
```

### 4. Run Full Backtest

```bash
./build/sentio_cli online-trade \
  --config config/enhanced_psm_config.json \
  --data data/futures.bin \
  --output results/online_backtest.json
```

## Configuration

Edit configuration files in `config/` directory:

```bash
# Main configuration for online learning with Ensemble PSM
config/enhanced_psm_config.json

# SGO-optimized hysteresis configuration
config/sgo_optimized_config.json

# Walk-forward validation configuration
config/walk_forward.json
```

Example configuration:

```json
{
  "strategy": "online_ensemble",
  "learning_rate": 0.01,
  "ensemble_size": 5,
  "window_size": 1000,
  "adaptation_rate": 0.1,
  "hysteresis_threshold": 0.02,
  "max_position_size": 1.0,
  "risk_per_trade": 0.02
}
```

## Development Workflow

### 1. Create a New Online Learning Strategy

Create header file `include/strategy/my_strategy.h`:

```cpp
#pragma once
#include "strategy/online_strategy_base.h"

class MyOnlineStrategy : public OnlineStrategyBase {
public:
    MyOnlineStrategy() = default;
    ~MyOnlineStrategy() override = default;
    
    // Initialize strategy
    void initialize() override;
    
    // Generate prediction
    SignalOutput predict(const Bar& bar) override;
    
    // Update model with new data
    void update(const Bar& bar, double realized_pnl) override;
    
private:
    // Your online learning model state
    Eigen::VectorXd weights_;
    double learning_rate_;
};
```

Create source file `src/strategy/my_strategy.cpp`:

```cpp
#include "strategy/my_strategy.h"

void MyOnlineStrategy::initialize() {
    weights_ = Eigen::VectorXd::Zero(91);  // 91 features
    learning_rate_ = 0.01;
}

SignalOutput MyOnlineStrategy::predict(const Bar& bar) {
    // Extract features
    Eigen::VectorXd features = extract_features(bar);
    
    // Compute prediction
    double signal = weights_.dot(features);
    
    return SignalOutput{signal, confidence};
}

void MyOnlineStrategy::update(const Bar& bar, double realized_pnl) {
    // Online gradient descent update
    Eigen::VectorXd features = extract_features(bar);
    double error = realized_pnl - weights_.dot(features);
    weights_ += learning_rate_ * error * features;
}
```

### 2. Add to CMakeLists.txt

```cmake
# Add to STRATEGY_SOURCES
list(APPEND STRATEGY_SOURCES src/strategy/my_strategy.cpp)
```

### 3. Register CLI Command

Create `src/cli/my_strategy_command.cpp` and register in command registry.

### 4. Rebuild and Test

```bash
./build.sh Release
./build/sentio_cli my-strategy --config config/my_config.json
```

## Troubleshooting

### Build Errors

**Error: Eigen3 not found**
```bash
brew install eigen
# or for Linux
sudo apt-get install libeigen3-dev
```

**Error: CMake version too old**
```bash
brew upgrade cmake  # macOS
# or download from https://cmake.org/download/
```

### Runtime Errors

**Error: Config file not found**
- Check that you're running from project root
- Use absolute paths: `./build/sentio_cli --config $(pwd)/config/enhanced_psm_config.json`

**Error: Data file not found**
- Convert your CSV data using `csv_to_binary_converter`
- Ensure binary data is in `data/` directory

## Performance Tips

### 1. Release Build (Recommended)
```bash
./build.sh Release
```
- Enables O3 optimization
- Uses `-march=native` for CPU-specific optimizations
- ~10-50x faster than Debug builds

### 2. OpenMP Parallelization
Already enabled in Release builds for multi-core processing.

### 3. Eigen Optimizations
Eigen automatically uses SIMD instructions (SSE, AVX) when available.

## Testing

### Unit Tests (if GTest installed)

```bash
cd build
ctest --output-on-failure
```

### Performance Benchmarks

```bash
./build/test_online_trade \
  --data data/futures.bin \
  --benchmark \
  --iterations 10000
```

## Next Steps

1. **Read the README**: `cat README.md`
2. **Check Migration Summary**: `cat MIGRATION_SUMMARY.md`
3. **Explore Examples**: Look in `tools/test_online_trade.cpp`
4. **Study Ensemble PSM**: Review `include/backend/ensemble_position_state_machine.h`
5. **Join Development**: Implement your own online learning algorithms!

## Common Use Cases

### Backtesting
```bash
./build/sentio_cli online-trade \
  --data data/historical.bin \
  --config config/enhanced_psm_config.json \
  --output results/backtest_$(date +%Y%m%d).json
```

### Walk-Forward Validation
```bash
./build/sentio_cli walk-forward \
  --data data/historical.bin \
  --config config/walk_forward.json \
  --train-size 5000 \
  --test-size 1000
```

### Live Trading Simulation
```bash
./build/sentio_cli online-trade \
  --data data/live_feed.bin \
  --config config/enhanced_psm_config.json \
  --mode simulation \
  --output results/live_sim.json
```

## Resources

- **Documentation**: See `README.md` for comprehensive guide
- **Architecture**: Review `MIGRATION_SUMMARY.md` for system overview
- **Examples**: Check `tools/` directory for example programs
- **Configs**: Explore `config/` for configuration templates

## Support

For issues or questions:
1. Check troubleshooting section above
2. Review build output for specific errors
3. Examine CMakeLists.txt for dependency configuration
4. Read inline code documentation in header files

---

**Happy Trading! 📈**
