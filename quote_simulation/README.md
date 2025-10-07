# Quote Simulation Module
# =======================

A comprehensive quote simulation system for online learning algorithms, featuring integration with Microsoft Research's MarS (Market Simulation Engine) for realistic market data generation.

## 🚀 Features

### **Real-Time Quote Generation**
- **Live Quote Streaming**: Generate real-time quotes for online learning algorithms
- **Multiple Market Regimes**: Normal, volatile, trending, bear, bull, sideways markets
- **Configurable Intervals**: From milliseconds to minutes
- **Realistic Spreads**: Bid/ask spreads and volume modeling

### **MarS Integration**
- **Microsoft Research MarS**: Integration with state-of-the-art market simulation engine
- **Order-Level Simulation**: Realistic market behavior through order generation
- **AI-Powered Continuation**: Use MarS AI model for realistic market continuation
- **Historical Context**: Start from real data, continue with AI simulation

### **Mock Polygon API**
- **Drop-in Replacement**: Replace Polygon API calls with simulated data
- **WebSocket Simulation**: Real-time quote subscriptions
- **Historical Data**: Generate historical bars compatible with Polygon format
- **No API Limits**: Unlimited testing without API costs

### **Online Learning Support**
- **Strategy Testing**: Test online learning algorithms with realistic data
- **Performance Metrics**: Comprehensive performance analysis
- **Monte Carlo Simulation**: Multiple simulation runs for robust testing
- **Real-time Adaptation**: Test algorithms that adapt in real-time

## 📁 Structure

```
quote_simulation/
├── include/                    # C++ headers
│   ├── online_quote_simulator.hpp
│   ├── virtual_market.hpp
│   └── mars_data_loader.hpp
├── src/                        # C++ implementation
│   ├── online_quote_simulator.cpp
│   ├── virtual_market.cpp
│   └── mars_data_loader.cpp
├── tools/                      # Python tools
│   ├── online_quote_simulator.py
│   └── mars_bridge.py
├── MarS/                       # Microsoft Research MarS package
├── examples/                   # Example code
│   ├── quote_simulation_example.cpp
│   └── quote_simulation_test.cpp
├── data/                       # Generated data
└── CMakeLists.txt             # Build configuration
```

## 🛠️ Installation

### **Prerequisites**
- C++17 compiler
- Eigen3
- nlohmann_json (optional)
- Python 3.8+ (for MarS integration)

### **Build**
```bash
cd quote_simulation
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### **Python Dependencies**
```bash
pip install pandas numpy
# Optional: MarS dependencies
pip install torch transformers ray streamlit
```

## 🚀 Usage

### **C++ API**

#### **Basic Quote Generation**
```cpp
#include "online_quote_simulator.hpp"

OnlineQuoteSimulator simulator;
auto quotes = simulator.generate_realtime_quotes("QQQ", 60, 1000); // 60 min, 1s intervals
```

#### **MarS Integration**
```cpp
OnlineTestConfig config;
config.strategy_name = "online_sgd";
config.symbol = "QQQ";
config.duration_minutes = 60;
config.simulations = 10;
config.use_mars = true;
config.market_regime = "normal";

auto results = simulator.run_mars_online_simulation(config, strategy, runner_cfg);
```

#### **Mock Polygon API**
```cpp
MockPolygonAPI mock_api("mock_key");
auto quote = mock_api.get_realtime_quote("QQQ");

// Subscribe to real-time quotes
mock_api.subscribe_to_quotes("QQQ", [](const PolygonQuote& quote) {
    std::cout << "Quote: " << quote.bid << " / " << quote.ask << std::endl;
});
```

### **Python Tools**

#### **Basic Quote Generation**
```bash
python tools/online_quote_simulator.py \
    --symbol QQQ \
    --duration 60 \
    --interval 1000 \
    --regime normal \
    --output quotes.json
```

#### **MarS Integration**
```bash
python tools/online_quote_simulator.py \
    --symbol QQQ \
    --duration 60 \
    --mars \
    --regime volatile \
    --simulations 10 \
    --output mars_quotes.json
```

#### **Real-Time Streaming**
```bash
python tools/online_quote_simulator.py \
    --symbol QQQ \
    --realtime \
    --regime normal \
    --interval 1000
```

#### **MarS Bridge**
```bash
python tools/mars_bridge.py \
    --symbol QQQ \
    --duration 60 \
    --regime normal \
    --simulations 5 \
    --output mars_data.json
```

## 📊 Market Regimes

| Regime | Volatility | Trend | Volume | Description |
|--------|------------|-------|--------|-------------|
| **normal** | 0.008 | 0.001 | 1.0x | Standard market conditions |
| **volatile** | 0.025 | 0.005 | 1.5x | High volatility market |
| **trending** | 0.015 | 0.02 | 1.2x | Strong upward trend |
| **bear** | 0.025 | -0.015 | 1.3x | Bear market conditions |
| **bull** | 0.015 | 0.02 | 1.1x | Bull market conditions |
| **sideways** | 0.008 | 0.001 | 0.8x | Low volatility sideways |

## 🤖 MarS Integration

### **What is MarS?**
MarS (Market Simulation Engine) is Microsoft Research's state-of-the-art market simulation system powered by a Large Market Model (LMM). It generates realistic market behavior through order-level simulation.

### **Key Features**
- **Order-Level Simulation**: Generates individual orders with realistic properties
- **AI-Powered**: Uses transformer-based models for realistic market behavior
- **Multiple Regimes**: Supports various market conditions
- **Historical Continuation**: Can continue from real historical data

### **Usage with MarS**
```python
from mars_bridge import MarsDataGenerator

generator = MarsDataGenerator(symbol="QQQ", base_price=458.0)
data = generator.generate_market_data(
    duration_minutes=60,
    bar_interval_seconds=60,
    num_simulations=5,
    market_regime="normal"
)
```

## 🔌 Mock Polygon API

### **Features**
- **Drop-in Replacement**: Same interface as Polygon API
- **Real-time Quotes**: WebSocket-like subscription system
- **Historical Data**: Generate historical bars
- **No Rate Limits**: Unlimited testing

### **Example**
```python
from online_quote_simulator import MockPolygonAPI

api = MockPolygonAPI("mock_key")

# Get real-time quote
quote = api.get_realtime_quote("QQQ")
print(f"Bid: {quote['bid']}, Ask: {quote['ask']}")

# Subscribe to quotes
def quote_callback(quote):
    print(f"New quote: {quote['bid']} / {quote['ask']}")

api.subscribe_to_quotes("QQQ", quote_callback)
```

## 📈 Performance Testing

### **Online Learning Algorithms**
The quote simulation system is specifically designed for testing online learning algorithms that:
- Adapt in real-time
- Learn from streaming data
- Require continuous market data
- Need realistic market behavior

### **Example Test**
```cpp
// Test online SGD strategy
OnlineTestConfig config;
config.strategy_name = "online_sgd";
config.symbol = "QQQ";
config.duration_minutes = 60;
config.simulations = 100;
config.use_mars = true;
config.market_regime = "normal";

auto results = simulator.run_mars_online_simulation(config, strategy, runner_cfg);
simulator.print_online_simulation_report(results, config);
```

## 🔧 Configuration

### **Environment Variables**
```bash
export MARS_MODEL_PATH="/path/to/mars/model"
export MARS_SERVER_IP="localhost"
export MARS_SERVER_PORT="8000"
```

### **Configuration Files**
- `MarS/market_simulation/conf.py`: MarS configuration
- `config.env`: Environment configuration
- `CMakeLists.txt`: Build configuration

## 📚 Examples

### **1. Basic Quote Generation**
```cpp
// Generate 1000 quotes for QQQ over 10 minutes
auto quotes = simulator.generate_realtime_quotes("QQQ", 10, 600);
```

### **2. MarS-Powered Simulation**
```cpp
// Use MarS for realistic market simulation
auto results = simulator.run_mars_online_simulation(config, strategy, runner_cfg);
```

### **3. Real-Time Streaming**
```cpp
// Start real-time quote streaming
simulator.start_realtime_streaming("QQQ", [](const QuoteData& quote) {
    // Process quote in real-time
    process_quote(quote);
}, 1000);
```

### **4. Mock Polygon Integration**
```cpp
// Replace Polygon API calls
MockPolygonAPI mock_api("mock_key");
auto quote = mock_api.get_realtime_quote("QQQ");
```

## 🎯 Use Cases

### **Online Learning Research**
- Test online learning algorithms with realistic data
- Compare different online learning approaches
- Validate algorithm performance across market regimes

### **Strategy Development**
- Develop and test trading strategies
- Validate strategy performance
- Test strategy robustness

### **System Testing**
- Test trading systems without live data
- Validate system performance
- Stress test with various market conditions

### **Education and Research**
- Learn about market behavior
- Research market microstructure
- Study algorithmic trading

## 🔍 Troubleshooting

### **Common Issues**

#### **MarS Not Available**
```
Warning: MarS not available, falling back to basic simulation
```
**Solution**: Install MarS dependencies or use basic simulation mode.

#### **Build Errors**
```
Error: Eigen3 not found
```
**Solution**: Install Eigen3 development package.

#### **Python Import Errors**
```
ImportError: No module named 'pandas'
```
**Solution**: Install required Python packages.

### **Performance Tips**
- Use MarS for realistic simulation when available
- Adjust quote intervals based on algorithm requirements
- Use appropriate market regimes for testing
- Monitor memory usage during long simulations

## 📄 License

This module is part of the online_trader project and follows the same license terms.

## 🤝 Contributing

Contributions are welcome! Please see the main project contributing guidelines.

## 📞 Support

For questions and support, please refer to the main project documentation or create an issue in the project repository.
