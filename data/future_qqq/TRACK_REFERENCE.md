# Quick Track Reference

## 🎯 Track Selection Guide

### For Standard Testing
- **Track 1, 4, 7, 10**: Normal market conditions
- **Best for**: Baseline performance, consistency validation

### For Stress Testing  
- **Track 2, 5, 8**: Volatile market conditions
- **Best for**: Risk management, extreme scenario testing

### For Trend Analysis
- **Track 3, 6, 9**: Trending market conditions  
- **Best for**: Momentum strategies, directional bias testing

## 📊 Quick Stats

| Track | Regime | Daily Volume | Price Range | Best Use Case |
|-------|--------|-------------|-------------|---------------|
| **1** | Normal | 39.5M | $409-$466 | Baseline testing |
| **2** | Volatile | 58.0M | $409-$466 | Volatility stress test |
| **3** | Trending | 52.0M | $409-$466 | Trend validation |
| **4** | Normal | 45.1M | $409-$466 | Consistency check |
| **5** | Volatile | 65.9M | $409-$466 | High vol scenarios |
| **6** | Trending | 58.8M | $409-$466 | Momentum testing |
| **7** | Normal | 50.9M | $409-$466 | Statistical robustness |
| **8** | Volatile | 73.8M | $409-$466 | Extreme conditions |
| **9** | Trending | 65.5M | $409-$466 | Directional bias |
| **10** | Normal | 56.5M | $409-$466 | Final validation |

## 🚀 Quick Load Commands

```bash
# Load specific track (CSV)
head -5 data/future_qqq/future_qqq_track_01.csv

# Check track statistics  
python3 tools/generate_future_qqq.py list

# Get track file path
python3 tools/generate_future_qqq.py get 5 --format csv
```

## ⚡ Integration Example

```cpp
// Monte Carlo with all tracks
std::vector<std::string> tracks = {
    "future_qqq_track_01.csv",  // Normal
    "future_qqq_track_02.csv",  // Volatile  
    "future_qqq_track_03.csv",  // Trending
    // ... add more as needed
};

for (size_t i = 0; i < tracks.size(); ++i) {
    auto data = load_csv("data/future_qqq/" + tracks[i]);
    auto result = test_strategy(strategy, data);
    results.push_back(result);
}
```
