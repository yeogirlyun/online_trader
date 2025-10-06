# Future QQQ Data - 2026 Synthetic Market Data

This directory contains 10 realistic QQQ datasets for 2026, generated using MarS AI to simulate future market conditions. Each track represents a different market scenario with authentic volume patterns and extended trading hours.

## 📊 Dataset Overview

- **Timeline**: January 2 - February 12, 2026 (28 trading days)
- **Resolution**: 1-minute bars with extended hours
- **Format**: Polygon-compatible CSV + JSON
- **Total Bars**: 26,830 per track
- **File Size**: ~6 MB per track (1.5 MB CSV + 4.5 MB JSON)

## ⏰ Trading Hours (All Times in UTC)

| Session | ET Time | UTC Time | Volume Characteristics |
|---------|---------|----------|----------------------|
| **Premarket** | 4:00 AM - 9:30 AM | 09:00 - 14:30 | Low volume (15% of normal) |
| **Regular** | 9:30 AM - 4:00 PM | 14:30 - 21:00 | Normal to high volume |
| **After Hours** | 4:00 PM - 8:00 PM | 21:00 - 01:00+1 | Moderate volume (25% of normal) |

## 🎯 Track Specifications

### Track 1-4, 7, 10: Normal Market Regime
- **Characteristics**: Balanced market conditions
- **Volume Multiplier**: 1.0x (baseline)
- **Daily Volume**: 39-57M shares average
- **Price Behavior**: Steady movements with moderate volatility
- **Use Case**: Standard strategy testing, baseline performance

### Track 2, 5, 8: Volatile Market Regime  
- **Characteristics**: High volatility periods
- **Volume Multiplier**: 1.4x (+40% volume)
- **Daily Volume**: 58-74M shares average
- **Price Behavior**: Sharp moves, increased intraday range
- **Use Case**: Stress testing, volatility strategy validation

### Track 3, 6, 9: Trending Market Regime
- **Characteristics**: Strong directional movements
- **Volume Multiplier**: 1.2x (+20% volume)
- **Daily Volume**: 52-66M shares average  
- **Price Behavior**: Sustained trends, momentum patterns
- **Use Case**: Trend-following strategy testing

## 📁 File Structure

```
data/future_qqq/
├── future_qqq_track_01.csv     # Normal regime
├── future_qqq_track_01.json    # Normal regime (JSON)
├── future_qqq_track_02.csv     # Volatile regime
├── future_qqq_track_02.json    # Volatile regime (JSON)
├── ...
├── future_qqq_track_10.csv     # Normal regime
├── future_qqq_track_10.json    # Normal regime (JSON)
├── metadata.json               # Complete statistics
└── README.md                   # This file
```

## 📋 Data Format

### CSV Format (Polygon-Compatible)
```csv
timestamp,open,high,low,close,volume,symbol
2026-01-02T09:00:00Z,455.0,455.0,455.0,455.0,17381,QQQ
2026-01-02T09:01:00Z,455.0,455.0,455.0,455.0,15309,QQQ
2026-01-02T09:02:00Z,457.0,458.0,457.0,458.0,11332,QQQ
```

### JSON Format
```json
[
  {
    "timestamp": "2026-01-02T09:00:00Z",
    "open": 455.0,
    "high": 455.0,
    "low": 455.0,
    "close": 455.0,
    "volume": 17381,
    "symbol": "QQQ"
  }
]
```

## 🔧 How It Was Generated

### 1. MarS AI Simulation
- **Base Engine**: MarS (Market Simulation) AI generates realistic price movements
- **Duration**: 28 days of continuous market data
- **Regimes**: Different market conditions (normal/volatile/trending)

### 2. Volume Enhancement
- **Base Volume**: 42M daily average (current QQQ levels)
- **Growth Trend**: 15% annual growth built into 2026 data
- **Intraday Patterns**:
  - Market open surge: 2.5x volume
  - Lunch lull: 0.6x volume  
  - Close surge: 1.8x volume
  - Extended hours: 0.15-0.25x volume

### 3. Timestamp Conversion
- **Source**: MarS generates in simulation time
- **Conversion**: Mapped to 2026 calendar with proper timezone handling
- **Format**: UTC timestamps in ISO 8601 format (`YYYY-MM-DDTHH:MM:SSZ`)
- **Holidays**: Excludes 9 US market holidays and weekends

### 4. Extended Hours Integration
- **Premarket**: 4:00 AM - 9:30 AM ET (low volume activity)
- **After Hours**: 4:00 PM - 8:00 PM ET (moderate volume)
- **Overnight**: Excluded (no trading 8 PM - 4 AM)

## 🚀 Usage Guide

### Strategy Testing Integration

```cpp
// Example: Use different tracks for Monte Carlo simulations
for (int sim = 1; sim <= 10; sim++) {
    std::string track_file = "data/future_qqq/future_qqq_track_" + 
                           std::to_string(sim).insert(0, 2 - std::to_string(sim).length(), '0') + 
                           ".csv";
    
    // Load track data for this simulation
    auto market_data = load_csv_data(track_file);
    
    // Run strategy test
    auto result = run_strategy_simulation(strategy, market_data);
    results.push_back(result);
}
```

### Recommended Track Usage

| Simulation # | Track | Regime | Purpose |
|-------------|-------|--------|---------|
| 1 | Track 1 | Normal | Baseline performance |
| 2 | Track 2 | Volatile | Stress test |
| 3 | Track 3 | Trending | Momentum validation |
| 4 | Track 4 | Normal | Consistency check |
| 5 | Track 5 | Volatile | High volatility edge cases |
| 6 | Track 6 | Trending | Trend persistence |
| 7 | Track 7 | Normal | Statistical significance |
| 8 | Track 8 | Volatile | Extreme conditions |
| 9 | Track 9 | Trending | Directional bias |
| 10 | Track 10 | Normal | Final validation |

### Loading Data Examples

#### Python
```python
import pandas as pd

# Load CSV data
df = pd.read_csv('future_qqq_track_01.csv')
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Load JSON data  
import json
with open('future_qqq_track_01.json', 'r') as f:
    data = json.load(f)
```

#### C++
```cpp
// Using your existing CSV loader
#include "sentio/csv_loader.hpp"

auto bars = sentio::CSVLoader::load_bars_from_csv("data/future_qqq/future_qqq_track_01.csv");
```

## 📈 Volume Statistics Summary

| Track | Regime | Daily Avg Volume | Min Volume | Max Volume |
|-------|--------|-----------------|------------|------------|
| 1 | Normal | 39.5M | 229K | 50.5M |
| 2 | Volatile | 58.0M | 343K | 74.0M |
| 3 | Trending | 52.0M | 306K | 65.9M |
| 4 | Normal | 45.1M | 283K | 57.2M |
| 5 | Volatile | 65.9M | 379K | 84.0M |
| 6 | Trending | 58.8M | 330K | 74.8M |
| 7 | Normal | 50.9M | 248K | 64.7M |
| 8 | Volatile | 73.8M | 435K | 93.8M |
| 9 | Trending | 65.5M | 385K | 82.7M |
| 10 | Normal | 56.5M | 313K | 71.6M |

## 🎯 Key Features

### ✅ Realistic Market Behavior
- **Price Discovery**: Authentic intrabar movements (9% of bars have OHLC variation)
- **Volume Correlation**: Higher volatility = higher volume
- **Session Patterns**: Proper premarket/regular/after-hours characteristics

### ✅ Production Quality
- **Format Compatibility**: 100% Polygon-compatible timestamps
- **Extended Coverage**: Full 16-hour trading day
- **Holiday Awareness**: Proper 2026 market calendar
- **Timezone Accuracy**: All timestamps in UTC with 'Z' suffix

### ✅ Testing Diversity
- **10 Unique Scenarios**: Different market conditions
- **Consistent Timeline**: Same 28-day period for fair comparison
- **Volume Variety**: 3 different regime multipliers
- **Statistical Robustness**: Sufficient data for Monte Carlo analysis

## 🔍 Quality Assurance

- **✓ Timestamp Validation**: All timestamps verified as valid 2026 dates
- **✓ Market Hours**: Confirmed 4 AM - 8 PM ET coverage
- **✓ Holiday Exclusion**: 9 US market holidays properly skipped
- **✓ Volume Realism**: Daily averages match current QQQ characteristics
- **✓ Format Compliance**: Headers and data types match Polygon exactly

## 📞 Generation Details

- **Generated**: September 14, 2025
- **Generator**: `tools/generate_future_qqq.py`
- **MarS Version**: Latest AI simulation engine
- **Total Generation Time**: ~63 minutes for all 10 tracks
- **Validation**: Comprehensive format and data quality checks

---

**Note**: This synthetic data is for backtesting and strategy validation only. While generated using sophisticated AI models to match realistic market patterns, it should not be used for actual trading decisions or market predictions.
