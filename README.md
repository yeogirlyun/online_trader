# Online Trader

A C++17 trading system focused on **online learning algorithms** with **ensemble position state machine (PSM)** backend for adaptive algorithmic trading.

## Overview

Online Trader is a streamlined trading framework extracted from sentio_trader, focusing exclusively on online learning strategies with real-time model adaptation. This project removes offline ML strategies (XGBoost, CatBoost, PPO) and concentrates on:

- **Online Learning Algorithms**: Real-time adaptive learning using incremental updates
- **Ensemble Position State Machine**: Advanced backend PSM that manages multiple trading signals with dynamic allocation
- **Strategy Framework**: Modular architecture for implementing online learning strategies
- **Performance Analysis**: Comprehensive backtesting and validation framework

## Key Features

### 🎯 Online Learning
- Incremental model updates without full retraining
- Adaptive feature normalization and scaling
- Real-time prediction with ensemble methods
- Support for multiple online learning algorithms (SGD, Online Gradient Boosting, etc.)

### 🔄 Ensemble Position State Machine
- Dynamic signal weighting based on recent performance
- Adaptive allocation management
- Hysteresis control for stable position transitions
- Multi-strategy ensemble support
- Risk-aware position sizing

### 📊 Strategy Framework
- Base classes for implementing online learning strategies
- Signal generation and output management
- Trading state management
- Strategy component architecture for modular design

### 🧪 Testing & Validation
- Enhanced testing framework with statistical analysis
- Walk-forward validation for online learning strategies
- Performance metrics and analysis
- Bar ID validation for data integrity

## Architecture

```
online_trader/
├── include/
│   ├── common/          # Common utilities and types
│   ├── core/            # Core data management and I/O
│   ├── strategy/        # Strategy framework (base classes)
│   ├── backend/         # Ensemble PSM and portfolio management
│   ├── learning/        # Online learning algorithms
│   ├── cli/             # Command-line interface
│   ├── testing/         # Testing framework
│   ├── validation/      # Validation framework
│   └── analysis/        # Performance analysis
├── src/                 # Implementation files (mirrors include/)
├── config/              # Configuration files
├── data/                # Market data (binary format)
├── tools/               # Utility tools
└── CMakeLists.txt       # Build configuration
```

## Dependencies

### Required
- **CMake** >= 3.16
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **Eigen3** >= 3.3 (for online learning algorithms)

### Optional
- **nlohmann/json** (for JSON parsing, fallback to header-only if not found)
- **OpenMP** (for parallel processing in Release builds)
- **GTest** (for unit testing, optional)

## Building

### Install Dependencies (macOS)

```bash
# Install Eigen3
brew install eigen

# Install nlohmann/json (optional)
brew install nlohmann-json

# Install GTest (optional, for testing)
brew install googletest
```

### Build the Project

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Optional: Run tests
ctest --output-on-failure
```

## Usage

### Command-Line Interface

The `sentio_cli` executable provides commands for online learning:

```bash
# Run online learning sanity check
./sentio_cli online-sanity-check --config config/online_config.json

# Run online learning backtest
./sentio_cli online-trade --config config/online_config.json --data data/futures.bin

# Test online learning strategy
./test_online_trade --data data/futures.bin
```

### Configuration

Configuration files are stored in `config/` directory. Example configuration:

```json
{
  "strategy": "online_ensemble",
  "learning_rate": 0.01,
  "ensemble_size": 5,
  "window_size": 1000,
  "adaptation_rate": 0.1,
  "hysteresis_threshold": 0.02,
  "max_position_size": 1.0
}
```

## Online Learning Strategies

### Base Framework

All online learning strategies inherit from `OnlineStrategyBase`:

```cpp
class OnlineStrategyBase : public MLStrategyBase {
public:
    virtual void update(const Bar& bar, double realized_pnl) = 0;
    virtual SignalOutput predict(const Bar& bar) = 0;
};
```

### Ensemble PSM

The Ensemble Position State Machine (`EnsemblePositionStateMachine`) manages multiple online learning models:

- **Dynamic Weighting**: Recent performance-based signal weighting
- **Adaptive Allocation**: Risk-adjusted position sizing
- **Hysteresis Management**: Smooth position transitions
- **Model Selection**: Automatic best-model selection

## Performance

The online learning framework is optimized for:
- **Low Latency**: < 1ms prediction time
- **Memory Efficiency**: Incremental updates without full history
- **Scalability**: Parallel processing with OpenMP
- **Robustness**: Statistical validation and walk-forward testing

## Comparison with Sentio Trader

| Feature | Online Trader | Sentio Trader |
|---------|---------------|---------------|
| Online Learning | ✅ Primary Focus | ⚠️ Limited Support |
| Ensemble PSM | ✅ Advanced | ⚠️ Basic |
| XGBoost/ML Strategies | ❌ Removed | ✅ Supported |
| PPO/RL Strategies | ❌ Removed | ✅ Supported |
| Real-time Adaptation | ✅ Core Feature | ⚠️ Experimental |
| Codebase Size | 📦 Streamlined | 📚 Comprehensive |

## Development

### Adding a New Online Learning Strategy

1. Create header in `include/strategy/`
2. Inherit from `OnlineStrategyBase`
3. Implement `update()` and `predict()` methods
4. Add to CMakeLists.txt
5. Create CLI command in `include/cli/`

Example:

```cpp
// include/strategy/my_online_strategy.h
#pragma once
#include "strategy/online_strategy_base.h"

class MyOnlineStrategy : public OnlineStrategyBase {
public:
    void update(const Bar& bar, double realized_pnl) override;
    SignalOutput predict(const Bar& bar) override;
private:
    // Your online learning model state
};
```

### Testing

Write unit tests in `tests/` directory using GTest:

```cpp
#include <gtest/gtest.h>
#include "strategy/my_online_strategy.h"

TEST(MyOnlineStrategyTest, UpdateAndPredict) {
    MyOnlineStrategy strategy;
    // Test your strategy
}
```

## Contributing

This project is part of the sentio trading system family. Contributions focused on:
- New online learning algorithms
- Ensemble PSM improvements
- Performance optimizations
- Documentation improvements

## License

[Specify your license here]

## Acknowledgments

Derived from the sentio_trader project with focus on online learning and ensemble methods.

## Contact

[Your contact information]

---

**Note**: This is a specialized trading framework for research and development. Use at your own risk in production environments.
