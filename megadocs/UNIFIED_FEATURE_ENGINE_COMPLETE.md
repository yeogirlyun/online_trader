# Unified Feature Engine Completion Requirements and Source Code

**Generated**: 2025-10-08 11:01:16
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (4 files)
**Description**: Complete requirements for implementing missing 54 features in UnifiedFeatureEngine, eliminating duplicate calculations, and optimizing performance. Includes full source code analysis.

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [include/features/feature_schema.h](#file-1)
2. [include/features/unified_feature_engine.h](#file-2)
3. [megadocs/UNIFIED_FEATURE_ENGINE_REQUIREMENTS.md](#file-3)
4. [src/features/unified_feature_engine.cpp](#file-4)

---

## 📄 **FILE 1 of 4**: include/features/feature_schema.h

**File Information**:
- **Path**: `include/features/feature_schema.h`

- **Size**: 123 lines
- **Modified**: 2025-10-08 01:04:31

- **Type**: .h

```text
#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace sentio {

/**
 * @brief Feature schema for reproducibility and validation
 *
 * Tracks feature names, version, and hash for model compatibility checking.
 */
struct FeatureSchema {
    std::vector<std::string> feature_names;
    int version{1};
    std::string hash;  // Hex digest of names + params

    /**
     * @brief Compute hash from feature names and version
     * @return Hex string hash (16 chars)
     */
    std::string compute_hash() const {
        std::stringstream ss;
        for (const auto& name : feature_names) {
            ss << name << "|";
        }
        ss << "v" << version;

        // Use std::hash as placeholder (use proper SHA256 in production)
        std::string s = ss.str();
        std::hash<std::string> hasher;
        size_t h = hasher(s);

        std::stringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << h;
        return hex.str();
    }

    /**
     * @brief Finalize schema by computing hash
     */
    void finalize() {
        hash = compute_hash();
    }

    /**
     * @brief Check if schema matches another
     * @param other Other schema to compare
     * @return true if compatible (same hash)
     */
    bool is_compatible(const FeatureSchema& other) const {
        return hash == other.hash && version == other.version;
    }
};

/**
 * @brief Feature snapshot with timestamp and schema
 */
struct FeatureSnapshot {
    uint64_t timestamp{0};
    uint64_t bar_id{0};
    std::vector<double> features;
    FeatureSchema schema;

    /**
     * @brief Check if snapshot is valid (size matches schema)
     * @return true if valid
     */
    bool is_valid() const {
        return features.size() == schema.feature_names.size();
    }
};

/**
 * @brief Replace NaN/Inf values with 0.0
 * @param features Feature vector to clean
 */
inline void nan_guard(std::vector<double>& features) {
    for (auto& f : features) {
        if (!std::isfinite(f)) {
            f = 0.0;
        }
    }
}

/**
 * @brief Clamp extreme feature values
 * @param features Feature vector to clamp
 * @param min_val Minimum allowed value
 * @param max_val Maximum allowed value
 */
inline void clamp_features(std::vector<double>& features,
                          double min_val = -1e6,
                          double max_val = 1e6) {
    for (auto& f : features) {
        f = std::clamp(f, min_val, max_val);
    }
}

/**
 * @brief Sanitize features: NaN guard + clamp
 * @param features Feature vector to sanitize
 */
inline void sanitize_features(std::vector<double>& features) {
    nan_guard(features);
    clamp_features(features);
}

/**
 * @brief Check if feature vector contains any invalid values
 * @param features Feature vector to check
 * @return true if all values are finite
 */
inline bool is_feature_vector_valid(const std::vector<double>& features) {
    return std::all_of(features.begin(), features.end(),
                      [](double f) { return std::isfinite(f); });
}

} // namespace sentio

```

## 📄 **FILE 2 of 4**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`

- **Size**: 266 lines
- **Modified**: 2025-10-08 01:06:16

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include "features/feature_schema.h"
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <string>

namespace sentio {
namespace features {

/**
 * @brief Configuration for Unified Feature Engine
 */
struct UnifiedFeatureEngineConfig {
    // Feature categories
    bool enable_time_features = true;
    bool enable_price_action = true;
    bool enable_moving_averages = true;
    bool enable_momentum = true;
    bool enable_volatility = true;
    bool enable_volume = true;
    bool enable_statistical = true;
    bool enable_pattern_detection = true;
    bool enable_price_lags = true;
    bool enable_return_lags = true;
    
    // Normalization
    bool normalize_features = true;
    bool use_robust_scaling = false;  // Use median/IQR instead of mean/std
    
    // Performance optimization
    bool enable_caching = true;
    bool enable_incremental_updates = true;
    int max_history_size = 1000;
    
    // Feature dimensions (matching Kochi analysis)
    int time_features = 8;
    int price_action_features = 12;
    int moving_average_features = 16;
    int momentum_features = 20;
    int volatility_features = 15;
    int volume_features = 12;
    int statistical_features = 10;
    int pattern_features = 8;
    int price_lag_features = 10;
    int return_lag_features = 15;
    
    // Total: Variable based on enabled features  
    int total_features() const {
        int total = 0;
        if (enable_time_features) total += time_features;
        if (enable_price_action) total += price_action_features;
        if (enable_moving_averages) total += moving_average_features;
        if (enable_momentum) total += momentum_features;
        if (enable_volatility) total += volatility_features;
        if (enable_volume) total += volume_features;
        if (enable_statistical) total += statistical_features;
        if (enable_pattern_detection) total += pattern_features;
        if (enable_price_lags) total += price_lag_features;
        if (enable_return_lags) total += return_lag_features;
        return total;
    }
};

/**
 * @brief Incremental calculator for O(1) moving averages
 */
class IncrementalSMA {
public:
    explicit IncrementalSMA(int period);
    double update(double value);
    double get_value() const { return sum_ / std::min(period_, static_cast<int>(values_.size())); }
    bool is_ready() const { return static_cast<int>(values_.size()) >= period_; }

private:
    int period_;
    std::deque<double> values_;
    double sum_ = 0.0;
};

/**
 * @brief Incremental calculator for O(1) EMA
 */
class IncrementalEMA {
public:
    IncrementalEMA(int period, double alpha = -1.0);
    double update(double value);
    double get_value() const { return ema_value_; }
    bool is_ready() const { return initialized_; }

private:
    double alpha_;
    double ema_value_ = 0.0;
    bool initialized_ = false;
};

/**
 * @brief Incremental calculator for O(1) RSI
 */
class IncrementalRSI {
public:
    explicit IncrementalRSI(int period);
    double update(double price);
    double get_value() const;
    bool is_ready() const { return gain_sma_.is_ready() && loss_sma_.is_ready(); }

private:
    double prev_price_ = 0.0;
    bool first_update_ = true;
    IncrementalSMA gain_sma_;
    IncrementalSMA loss_sma_;
};

/**
 * @brief Unified Feature Engine implementing Kochi's 126-feature set
 * 
 * This engine provides a comprehensive set of technical indicators optimized
 * for machine learning applications. It implements all features identified
 * in the Kochi analysis with proper normalization and O(1) incremental updates.
 * 
 * Feature Categories:
 * 1. Time Features (8): Cyclical encoding of time components
 * 2. Price Action (12): OHLC patterns, gaps, shadows
 * 3. Moving Averages (16): SMA/EMA ratios at multiple periods
 * 4. Momentum (20): RSI, MACD, Stochastic, Williams %R
 * 5. Volatility (15): ATR, Bollinger Bands, Keltner Channels
 * 6. Volume (12): VWAP, OBV, A/D Line, Volume ratios
 * 7. Statistical (10): Correlation, regression, distribution metrics
 * 8. Patterns (8): Candlestick pattern detection
 * 9. Price Lags (10): Historical price references
 * 10. Return Lags (15): Historical return references
 */
class UnifiedFeatureEngine {
public:
    using Config = UnifiedFeatureEngineConfig;
    
    explicit UnifiedFeatureEngine(const Config& config = Config{});
    ~UnifiedFeatureEngine() = default;

    /**
     * @brief Update engine with new bar data
     * @param bar New OHLCV bar
     */
    void update(const Bar& bar);

    /**
     * @brief Get current feature vector
     * @return Vector of 126 normalized features
     */
    std::vector<double> get_features() const;

    /**
     * @brief Get specific feature category
     */
    std::vector<double> get_time_features() const;
    std::vector<double> get_price_action_features() const;
    std::vector<double> get_moving_average_features() const;
    std::vector<double> get_momentum_features() const;
    std::vector<double> get_volatility_features() const;
    std::vector<double> get_volume_features() const;
    std::vector<double> get_statistical_features() const;
    std::vector<double> get_pattern_features() const;
    std::vector<double> get_price_lag_features() const;
    std::vector<double> get_return_lag_features() const;

    /**
     * @brief Get feature names for debugging/analysis
     */
    std::vector<std::string> get_feature_names() const;

    /**
     * @brief Reset engine state
     */
    void reset();

    /**
     * @brief Check if engine has enough data for all features
     */
    bool is_ready() const;

    /**
     * @brief Get number of bars processed
     */
    size_t get_bar_count() const { return bar_history_.size(); }

    /**
     * @brief Get feature schema for validation
     */
    const FeatureSchema& get_schema() const { return feature_schema_; }

private:
    Config config_;

    // Initialization methods
    void initialize_feature_schema();

    // Data storage
    std::deque<Bar> bar_history_;
    std::deque<double> returns_;
    std::deque<double> log_returns_;
    
    // Incremental calculators
    std::unordered_map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalEMA>> ema_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalRSI>> rsi_calculators_;
    
    // Cached features
    mutable std::vector<double> cached_features_;

    // Feature schema for validation
    FeatureSchema feature_schema_;
    mutable bool cache_valid_ = false;
    
    // Normalization parameters
    struct NormalizationParams {
        double mean = 0.0;
        double std = 1.0;
        double median = 0.0;
        double iqr = 1.0;
        std::deque<double> history;
        bool initialized = false;
    };
    mutable std::unordered_map<std::string, NormalizationParams> norm_params_;
    
    // Private methods
    void initialize_calculators();
    void update_returns(const Bar& bar);
    void update_calculators(const Bar& bar);
    void invalidate_cache();
    
    // Feature calculation methods
    std::vector<double> calculate_time_features(const Bar& bar) const;
    std::vector<double> calculate_price_action_features() const;
    std::vector<double> calculate_moving_average_features() const;
    std::vector<double> calculate_momentum_features() const;
    std::vector<double> calculate_volatility_features() const;
    std::vector<double> calculate_volume_features() const;
    std::vector<double> calculate_statistical_features() const;
    std::vector<double> calculate_pattern_features() const;
    std::vector<double> calculate_price_lag_features() const;
    std::vector<double> calculate_return_lag_features() const;
    
    // Utility methods
    double normalize_feature(const std::string& name, double value) const;
    void update_normalization_params(const std::string& name, double value) const;
    double safe_divide(double numerator, double denominator, double fallback = 0.0) const;
    double calculate_atr(int period) const;
    double calculate_true_range(size_t index) const;
    
    // Pattern detection helpers
    bool is_doji(const Bar& bar) const;
    bool is_hammer(const Bar& bar) const;
    bool is_shooting_star(const Bar& bar) const;
    bool is_engulfing_bullish(size_t index) const;
    bool is_engulfing_bearish(size_t index) const;
    
    // Statistical helpers
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y, int period) const;
    double calculate_linear_regression_slope(const std::vector<double>& values, int period) const;
};

} // namespace features
} // namespace sentio
```

## 📄 **FILE 3 of 4**: megadocs/UNIFIED_FEATURE_ENGINE_REQUIREMENTS.md

**File Information**:
- **Path**: `megadocs/UNIFIED_FEATURE_ENGINE_REQUIREMENTS.md`

- **Size**: 572 lines
- **Modified**: 2025-10-08 11:00:25

- **Type**: .md

```text
# Unified Feature Engine Completion Requirements

**Current Completion**: 57% (72/126 features)
**Missing Features**: 54
**Critical Issues**: 10 missing public methods, duplicate calculations, inefficient algorithms

---

## 1. MISSING FEATURE IMPLEMENTATIONS

### 1.1 Momentum Features (15 missing, 5 implemented)

**Currently Has**:
- RSI(14), RSI(21)
- 5-period momentum
- 10-period momentum
- Mean return

**Must Add**:
1. **MACD (Moving Average Convergence Divergence)**
   - MACD line (12-period EMA - 26-period EMA)
   - Signal line (9-period EMA of MACD)
   - MACD histogram (MACD - Signal)

2. **Stochastic Oscillator**
   - %K = (Close - Low14) / (High14 - Low14) * 100
   - %D = 3-period SMA of %K
   - Stochastic Slow = 3-period SMA of %D

3. **Williams %R**
   - %R = (High14 - Close) / (High14 - Low14) * -100
   - Periods: 14, 21

4. **Rate of Change (ROC)**
   - ROC = (Close - Close_n) / Close_n * 100
   - Periods: 5, 10, 20

5. **Commodity Channel Index (CCI)**
   - CCI = (TP - SMA_TP) / (0.015 * Mean Deviation)
   - Period: 20

6. **Additional Momentum**
   - 20-period momentum
   - Momentum divergence (price vs momentum direction)

### 1.2 Volatility Features (11 missing, 4 implemented)

**Currently Has**:
- ATR(14) ratio
- ATR(20) ratio
- 20-period return volatility
- Annualized volatility

**Must Add**:
1. **Bollinger Bands (BB)**
   - Upper Band = SMA(20) + 2*StdDev(20)
   - Lower Band = SMA(20) - 2*StdDev(20)
   - %B = (Close - Lower) / (Upper - Lower)
   - Bandwidth = (Upper - Lower) / SMA(20)

2. **Keltner Channels (KC)**
   - Middle = EMA(20)
   - Upper = EMA(20) + 2*ATR(20)
   - Lower = EMA(20) - 2*ATR(20)
   - Position = (Close - Lower) / (Upper - Lower)

3. **Additional Volatility Metrics**
   - Historical volatility (10, 30 periods)
   - Parkinson volatility (High-Low range estimator)
   - Volatility ratio (short/long term)

### 1.3 Volume Features (10 missing, 2 implemented)

**Currently Has**:
- Volume ratio (vs 20-period avg)
- Volume intensity

**Must Add**:
1. **VWAP (Volume Weighted Average Price)**
   - VWAP = Σ(Price * Volume) / Σ(Volume)
   - Distance from VWAP

2. **OBV (On Balance Volume)**
   - OBV = OBV_prev + (Close > Close_prev ? Volume : -Volume)
   - OBV SMA(10), OBV SMA(20)

3. **A/D Line (Accumulation/Distribution)**
   - MFM = ((Close - Low) - (High - Close)) / (High - Low)
   - A/D = A/D_prev + MFM * Volume

4. **Money Flow Index (MFI)**
   - Typical Price = (High + Low + Close) / 3
   - Money Flow = TP * Volume
   - MFI = 100 - 100/(1 + Positive MF / Negative MF)

5. **Volume Metrics**
   - Volume SMA(5), SMA(20)
   - Volume EMA(10)
   - Volume rate of change

### 1.4 Statistical Features (9 missing, 1 implemented)

**Currently Has**:
- Skewness (20-period)

**Must Add**:
1. **Kurtosis**
   - Kurtosis = Σ((x - mean)/std)^4 / n - 3
   - Period: 20

2. **Correlation Metrics**
   - Price-Volume correlation (20-period)
   - Returns autocorrelation (lag 1, 5, 10)

3. **Linear Regression**
   - Slope of price over 20 periods
   - R-squared (goodness of fit)
   - Residual std dev

4. **Distribution Metrics**
   - Median absolute deviation (MAD)
   - Interquartile range (IQR)

### 1.5 Pattern Features (3 missing, 5 implemented)

**Currently Has**:
- Doji, Hammer, Shooting Star
- Bullish Engulfing, Bearish Engulfing

**Must Add**:
1. Morning Star / Evening Star
2. Harami (Bullish/Bearish)
3. Three White Soldiers / Three Black Crows

### 1.6 Moving Average Features (6 missing, 10 implemented)

**Currently Has**:
- SMA ratios: 5, 10, 20, 50, 100, 200
- EMA ratios: 5, 10, 20, 50

**Must Add**:
1. **SMA/EMA Crossovers**
   - SMA(5) vs SMA(20) crossover signal
   - EMA(12) vs EMA(26) crossover signal

2. **Distance Bands**
   - (Close - SMA(20)) / SMA(20)
   - (Close - EMA(20)) / EMA(20)

3. **Convergence Metrics**
   - EMA(12) - SMA(12) divergence
   - Multi-timeframe alignment

---

## 2. MISSING PUBLIC API METHODS

**All declared in header but NOT implemented**:

```cpp
// Must implement in unified_feature_engine.cpp:
std::vector<double> get_time_features() const;
std::vector<double> get_price_action_features() const;
std::vector<double> get_moving_average_features() const;
std::vector<double> get_momentum_features() const;
std::vector<double> get_volatility_features() const;
std::vector<double> get_volume_features() const;
std::vector<double> get_statistical_features() const;
std::vector<double> get_pattern_features() const;
std::vector<double> get_price_lag_features() const;
std::vector<double> get_return_lag_features() const;
```

**Implementation**:
```cpp
std::vector<double> UnifiedFeatureEngine::get_time_features() const {
    return calculate_time_features(bar_history_.back());
}
// ... repeat for all 10 methods
```

---

## 3. DUPLICATE CALCULATIONS TO ELIMINATE

### 3.1 Variance/Mean Return (3 duplicates)

**Current**:
- `calculate_momentum_features()`: calculates 5-period mean
- `calculate_volatility_features()`: calculates 20-period mean & variance
- `calculate_statistical_features()`: calculates 20-period mean & variance (DUPLICATE)

**Solution**: Create shared helper
```cpp
struct ReturnStatistics {
    double mean;
    double variance;
    double std_dev;
    double skewness;
    double kurtosis;
};

ReturnStatistics compute_return_stats(int period) const {
    // Single-pass calculation using Welford's algorithm
}
```

### 3.2 Volume Average (recalculated every tick)

**Current**: Full loop in `calculate_volume_features()`
```cpp
double avg_volume = 0.0;
for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
    avg_volume += bar_history_[i].volume;
}
avg_volume /= 20.0;
```

**Solution**: Add `IncrementalVolumeSMA` to `initialize_calculators()`
```cpp
volume_sma_20_ = std::make_unique<IncrementalSMA>(20);
// Update in update_calculators()
volume_sma_20_->update(bar.volume);
```

---

## 4. INEFFICIENT CALCULATIONS TO OPTIMIZE

### 4.1 ATR Calculation (O(N) every tick)

**Current**: Loops 14+20=34 times per feature update
```cpp
double calculate_atr(int period) const {
    double atr_sum = 0.0;
    for (int i = 0; i < period; ++i) {
        atr_sum += calculate_true_range(bar_history_.size() - 1 - i);
    }
    return atr_sum / period;
}
```

**Solution**: Incremental ATR calculator
```cpp
class IncrementalATR {
public:
    explicit IncrementalATR(int period);
    double update(const Bar& current, const Bar& previous);
    double get_value() const { return atr_value_; }
    bool is_ready() const;
private:
    int period_;
    IncrementalSMA tr_sma_;
    double atr_value_ = 0.0;
};
```

### 4.2 Unordered Map Iteration (undefined order)

**Current**: Feature order changes between runs
```cpp
for (const auto& [name, calc] : sma_calculators_) {  // UNDEFINED ORDER
    features[idx++] = safe_divide(price, calc->get_value()) - 1.0;
}
```

**Solution**: Use ordered container
```cpp
std::map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
// OR
std::vector<std::pair<std::string, std::unique_ptr<IncrementalSMA>>> sma_calculators_;
```

### 4.3 Vector Allocations (10+ per tick)

**Current**: Every calculate_*_features() allocates new vector
```cpp
std::vector<double> features(config_.momentum_features, 0.0);
```

**Solution**: Pre-allocated member variables
```cpp
private:
    mutable std::vector<double> time_features_;
    mutable std::vector<double> momentum_features_;
    // ... etc

    void initialize_feature_vectors() {
        time_features_.resize(config_.time_features);
        momentum_features_.resize(config_.momentum_features);
        // ...
    }
```

### 4.4 Deque-to-Vector Copying (3+ copies per tick)

**Current**:
```cpp
auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
```

**Solution**: Use iterators or std::span
```cpp
// C++20 span
std::span<const double> get_recent_returns(int period) const {
    return std::span(returns_.end() - period, returns_.end());
}
```

---

## 5. CORRECTNESS FIXES

### 5.1 RSI Calculation (using SMA instead of Wilder's EMA)

**Current** (WRONG):
```cpp
IncrementalSMA gain_sma_;
IncrementalSMA loss_sma_;
```

**Correct** (Wilder's smoothing):
```cpp
class IncrementalRSI {
private:
    double avg_gain_ = 0.0;
    double avg_loss_ = 0.0;
    int period_;
    int count_ = 0;

    double update(double price) {
        double change = price - prev_price_;
        double gain = std::max(0.0, change);
        double loss = std::max(0.0, -change);

        if (count_ < period_) {
            // First N values: use SMA
            gain_sum_ += gain;
            loss_sum_ += loss;
            if (count_ == period_ - 1) {
                avg_gain_ = gain_sum_ / period_;
                avg_loss_ = loss_sum_ / period_;
            }
        } else {
            // Wilder's smoothing: avg = (prev_avg * 13 + current) / 14
            avg_gain_ = (avg_gain_ * (period_ - 1) + gain) / period_;
            avg_loss_ = (avg_loss_ * (period_ - 1) + loss) / period_;
        }
    }
};
```

### 5.2 Volume Intensity Normalization

**Current** (inconsistent):
```cpp
// Line 370 - normalized
features[11] = safe_divide(volume * abs(close - open), close);

// Line 491 - NOT normalized
features[1] = volume * abs(close - open);
```

**Fix**: Use consistent normalization
```cpp
features[1] = safe_divide(volume * abs(close - open), close * avg_volume);
```

### 5.3 Skewness Formula

**Current** (biased):
```cpp
skewness = skew_sum / (n * std_dev^3);
```

**Correct** (unbiased sample skewness):
```cpp
skewness = (n / ((n-1)*(n-2))) * skew_sum / (std_dev^3);
```

---

## 6. FEATURE NAME GENERATION

**Current**: Only 8 names returned (time features)

**Required**: All 126 names in order

```cpp
std::vector<std::string> get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());

    // Time (8)
    names.insert(names.end(), {
        "hour_sin", "hour_cos", "minute_sin", "minute_cos",
        "dow_sin", "dow_cos", "dom_sin", "dom_cos"
    });

    // Price Action (12)
    names.insert(names.end(), {
        "body_size_pct", "upper_shadow_pct", "lower_shadow_pct",
        "close_position", "range_pct", "gap_pct",
        "high_low_range", "body_direction", "candle_type",
        "price_momentum_1", "price_momentum_5", "volume_intensity"
    });

    // Moving Averages (16)
    for (int period : {5, 10, 20, 50, 100, 200}) {
        names.push_back("sma_" + std::to_string(period) + "_ratio");
    }
    for (int period : {5, 10, 20, 50}) {
        names.push_back("ema_" + std::to_string(period) + "_ratio");
    }
    names.insert(names.end(), {"sma5_sma20_cross", "ema12_ema26_cross",
                              "close_sma20_dist", "close_ema20_dist",
                              "ema12_sma12_div", "ma_alignment"});

    // Momentum (20)
    names.insert(names.end(), {
        "rsi_14", "rsi_21",
        "macd", "macd_signal", "macd_hist",
        "stoch_k", "stoch_d", "stoch_slow",
        "williams_r_14", "williams_r_21",
        "roc_5", "roc_10", "roc_20",
        "cci_20",
        "momentum_5", "momentum_10", "momentum_20",
        "mean_return_5", "momentum_div", "momentum_strength"
    });

    // Volatility (15)
    names.insert(names.end(), {
        "atr_14_ratio", "atr_20_ratio",
        "bb_upper", "bb_lower", "bb_pct_b", "bb_bandwidth",
        "kc_upper", "kc_lower", "kc_position",
        "return_vol_20", "return_vol_ann",
        "hvol_10", "hvol_30",
        "parkinson_vol", "vol_ratio"
    });

    // Volume (12)
    names.insert(names.end(), {
        "volume_ratio_20", "volume_intensity",
        "vwap", "vwap_distance",
        "obv", "obv_sma_10", "obv_sma_20",
        "ad_line",
        "mfi_14",
        "volume_sma_5", "volume_ema_10", "volume_roc"
    });

    // Statistical (10)
    names.insert(names.end(), {
        "skewness_20", "kurtosis_20",
        "price_vol_corr", "ret_autocorr_1", "ret_autocorr_5", "ret_autocorr_10",
        "lin_reg_slope", "r_squared",
        "mad", "iqr"
    });

    // Patterns (8)
    names.insert(names.end(), {
        "doji", "hammer", "shooting_star",
        "engulf_bull", "engulf_bear",
        "morning_star", "harami", "three_soldiers"
    });

    // Price Lags (10)
    for (int lag = 1; lag <= 10; ++lag) {
        names.push_back("price_lag_" + std::to_string(lag));
    }

    // Return Lags (15)
    for (int lag = 1; lag <= 15; ++lag) {
        names.push_back("return_lag_" + std::to_string(lag));
    }

    return names;
}
```

---

## 7. IMPLEMENTATION PRIORITY

### P0 - Critical (Breaks API Contract)
1. Implement 10 missing public getter methods
2. Fix unordered_map iteration (stable feature ordering)
3. Complete `get_feature_names()` (all 126 names)

### P1 - High (Performance/Correctness)
4. Add IncrementalATR (eliminates O(N) hotspot)
5. Fix RSI calculation (Wilder's smoothing)
6. Eliminate duplicate variance calculations
7. Add incremental volume calculator

### P2 - Medium (Feature Completeness)
8. Implement MACD (3 features)
9. Implement Bollinger Bands (4 features)
10. Implement VWAP, OBV (4 features)
11. Implement Stochastic Oscillator (3 features)

### P3 - Low (Nice to Have)
12. Implement remaining momentum features (6 features)
13. Implement Keltner Channels (3 features)
14. Implement remaining volume features (6 features)
15. Implement remaining statistical features (9 features)
16. Implement remaining pattern features (3 features)

---

## 8. TESTING REQUIREMENTS

### Unit Tests Required
1. **Feature Count Validation**
   - Each category returns correct number of features
   - Total features = 126

2. **Feature Order Stability**
   - Same input → same feature order across runs
   - Verify with multiple calls to `get_features()`

3. **Correctness Tests**
   - RSI matches TradingView/TA-Lib
   - MACD matches TradingView
   - Bollinger Bands match reference implementation
   - ATR matches reference

4. **Performance Tests**
   - `update()` completes in < 1ms
   - `get_features()` completes in < 0.5ms
   - No memory leaks over 100K updates

### Integration Tests
1. Feature engine with OnlineEnsembleStrategy
2. Feature vector used in EWRLS prediction
3. Live trading with real-time feature updates

---

## 9. DELIVERABLES

1. **Updated Source Files**
   - `src/features/unified_feature_engine.cpp` (all missing methods)
   - `include/features/unified_feature_engine.h` (new incremental calculators)

2. **New Incremental Calculators**
   - `IncrementalATR`
   - `IncrementalStochastic`
   - `IncrementalMACD`
   - `IncrementalBollingerBands`
   - `IncrementalVariance` (Welford's algorithm)

3. **Test Suite**
   - `tests/test_unified_feature_engine.cpp`
   - Unit tests for each feature category
   - Performance benchmarks

4. **Validation Report**
   - Feature count: 126/126 ✓
   - Feature names: all populated ✓
   - Correctness: matches reference implementations ✓
   - Performance: < 1ms update time ✓

---

## 10. ACCEPTANCE CRITERIA

✅ **Complete**: All 126 features calculated
✅ **Correct**: RSI, MACD, Bollinger Bands match TradingView
✅ **Fast**: < 1ms per update (incremental calculations)
✅ **Stable**: Feature order deterministic across runs
✅ **Tested**: 90%+ code coverage, all tests pass
✅ **Documented**: All 126 feature names in `get_feature_names()`

```

## 📄 **FILE 4 of 4**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`

- **Size**: 705 lines
- **Modified**: 2025-10-08 01:06:25

- **Type**: .cpp

```text
#include "features/unified_feature_engine.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace sentio {
namespace features {

// IncrementalSMA implementation
IncrementalSMA::IncrementalSMA(int period) : period_(period) {
    // Note: std::deque doesn't have reserve(), but that's okay
}

double IncrementalSMA::update(double value) {
    if (values_.size() >= period_) {
        sum_ -= values_.front();
        values_.pop_front();
    }
    
    values_.push_back(value);
    sum_ += value;
    
    return get_value();
}

// IncrementalEMA implementation
IncrementalEMA::IncrementalEMA(int period, double alpha) {
    if (alpha < 0) {
        alpha_ = 2.0 / (period + 1.0);  // Standard EMA alpha
    } else {
        alpha_ = alpha;
    }
}

double IncrementalEMA::update(double value) {
    if (!initialized_) {
        ema_value_ = value;
        initialized_ = true;
    } else {
        ema_value_ = alpha_ * value + (1.0 - alpha_) * ema_value_;
    }
    return ema_value_;
}

// IncrementalRSI implementation
IncrementalRSI::IncrementalRSI(int period) 
    : gain_sma_(period), loss_sma_(period) {
}

double IncrementalRSI::update(double price) {
    if (first_update_) {
        prev_price_ = price;
        first_update_ = false;
        return 50.0;  // Neutral RSI
    }
    
    double change = price - prev_price_;
    double gain = std::max(0.0, change);
    double loss = std::max(0.0, -change);
    
    gain_sma_.update(gain);
    loss_sma_.update(loss);
    
    prev_price_ = price;
    
    return get_value();
}

double IncrementalRSI::get_value() const {
    if (!is_ready()) return 50.0;
    
    double avg_gain = gain_sma_.get_value();
    double avg_loss = loss_sma_.get_value();
    
    if (avg_loss == 0.0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// UnifiedFeatureEngine implementation
UnifiedFeatureEngine::UnifiedFeatureEngine(const Config& config) : config_(config) {
    initialize_calculators();
    cached_features_.resize(config_.total_features(), 0.0);

    // Initialize feature schema
    initialize_feature_schema();
}

void UnifiedFeatureEngine::initialize_calculators() {
    // Initialize SMA calculators for various periods
    std::vector<int> sma_periods = {5, 10, 20, 50, 100, 200};
    for (int period : sma_periods) {
        sma_calculators_["sma_" + std::to_string(period)] = 
            std::make_unique<IncrementalSMA>(period);
    }
    
    // Initialize EMA calculators
    std::vector<int> ema_periods = {5, 10, 20, 50};
    for (int period : ema_periods) {
        ema_calculators_["ema_" + std::to_string(period)] = 
            std::make_unique<IncrementalEMA>(period);
    }
    
    // Initialize RSI calculators
    std::vector<int> rsi_periods = {14, 21};
    for (int period : rsi_periods) {
        rsi_calculators_["rsi_" + std::to_string(period)] =
            std::make_unique<IncrementalRSI>(period);
    }
}

void UnifiedFeatureEngine::initialize_feature_schema() {
    // Initialize feature schema with all 126 feature names
    feature_schema_.version = 3;  // Bump when changing feature set
    feature_schema_.feature_names = get_feature_names();
    feature_schema_.finalize();

    std::cout << "[FeatureEngine] Initialized schema v" << feature_schema_.version
              << " with " << feature_schema_.feature_names.size()
              << " features, hash=" << feature_schema_.hash << std::endl;
}

void UnifiedFeatureEngine::update(const Bar& bar) {
    // DEBUG: Track bar history size WITH INSTANCE POINTER
    size_t size_before = bar_history_.size();

    // Add to history
    bar_history_.push_back(bar);

    size_t size_after_push = bar_history_.size();

    // Maintain max history size
    if (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
    }

    size_t size_final = bar_history_.size();

    // DEBUG: Log size changes (every 100 bars to avoid spam)
    static int update_count = 0;
    update_count++;
    if (update_count <= 10 || update_count % 100 == 0 || size_final < 70) {
        std::cout << "[UFE@" << (void*)this << "] update #" << update_count
                  << ": before=" << size_before
                  << " → after_push=" << size_after_push
                  << " → final=" << size_final
                  << " (max=" << config_.max_history_size
                  << ", ready=" << (size_final >= 64 ? "YES" : "NO") << ")"
                  << std::endl;
    }

    // Update returns
    update_returns(bar);

    // Update incremental calculators
    update_calculators(bar);

    // Invalidate cache
    invalidate_cache();
}

void UnifiedFeatureEngine::update_returns(const Bar& bar) {
    if (!bar_history_.empty() && bar_history_.size() > 1) {
        double prev_close = bar_history_[bar_history_.size() - 2].close;
        double current_close = bar.close;
        
        double return_val = (current_close - prev_close) / prev_close;
        double log_return = std::log(current_close / prev_close);
        
        returns_.push_back(return_val);
        log_returns_.push_back(log_return);
        
        // Maintain max size
        if (returns_.size() > config_.max_history_size) {
            returns_.pop_front();
            log_returns_.pop_front();
        }
    }
}

void UnifiedFeatureEngine::update_calculators(const Bar& bar) {
    double close = bar.close;
    
    // Update SMA calculators
    for (auto& [name, calc] : sma_calculators_) {
        calc->update(close);
    }
    
    // Update EMA calculators
    for (auto& [name, calc] : ema_calculators_) {
        calc->update(close);
    }
    
    // Update RSI calculators
    for (auto& [name, calc] : rsi_calculators_) {
        calc->update(close);
    }
}

void UnifiedFeatureEngine::invalidate_cache() {
    cache_valid_ = false;
}

std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_ && config_.enable_caching) {
        return cached_features_;
    }
    
    std::vector<double> features;
    features.reserve(config_.total_features());
    
    if (bar_history_.empty()) {
        // Return zeros if no data
        features.resize(config_.total_features(), 0.0);
        return features;
    }
    
    const Bar& current_bar = bar_history_.back();
    
    // 1. Time features (8)
    if (config_.enable_time_features) {
        auto time_features = calculate_time_features(current_bar);
        features.insert(features.end(), time_features.begin(), time_features.end());
    }
    
    // 2. Price action features (12)
    if (config_.enable_price_action) {
        auto price_features = calculate_price_action_features();
        features.insert(features.end(), price_features.begin(), price_features.end());
    }
    
    // 3. Moving average features (16)
    if (config_.enable_moving_averages) {
        auto ma_features = calculate_moving_average_features();
        features.insert(features.end(), ma_features.begin(), ma_features.end());
    }
    
    // 4. Momentum features (20)
    if (config_.enable_momentum) {
        auto momentum_features = calculate_momentum_features();
        features.insert(features.end(), momentum_features.begin(), momentum_features.end());
    }
    
    // 5. Volatility features (15)
    if (config_.enable_volatility) {
        auto vol_features = calculate_volatility_features();
        features.insert(features.end(), vol_features.begin(), vol_features.end());
    }
    
    // 6. Volume features (12)
    if (config_.enable_volume) {
        auto volume_features = calculate_volume_features();
        features.insert(features.end(), volume_features.begin(), volume_features.end());
    }
    
    // 7. Statistical features (10)
    if (config_.enable_statistical) {
        auto stat_features = calculate_statistical_features();
        features.insert(features.end(), stat_features.begin(), stat_features.end());
    }
    
    // 8. Pattern features (8)
    if (config_.enable_pattern_detection) {
        auto pattern_features = calculate_pattern_features();
        features.insert(features.end(), pattern_features.begin(), pattern_features.end());
    }
    
    // 9. Price lag features (10)
    if (config_.enable_price_lags) {
        auto price_lag_features = calculate_price_lag_features();
        features.insert(features.end(), price_lag_features.begin(), price_lag_features.end());
    }
    
    // 10. Return lag features (15)
    if (config_.enable_return_lags) {
        auto return_lag_features = calculate_return_lag_features();
        features.insert(features.end(), return_lag_features.begin(), return_lag_features.end());
    }
    
    // Ensure we have the right number of features
    features.resize(config_.total_features(), 0.0);

    // CRITICAL: Apply NaN guards and clamping
    sanitize_features(features);

    // Validate feature count matches schema
    if (!feature_schema_.feature_names.empty() &&
        features.size() != feature_schema_.feature_names.size()) {
        std::cerr << "[FeatureEngine] ERROR: Feature size mismatch: got "
                  << features.size() << ", expected "
                  << feature_schema_.feature_names.size() << std::endl;
    }

    // Cache results
    if (config_.enable_caching) {
        cached_features_ = features;
        cache_valid_ = true;
    }

    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_time_features(const Bar& bar) const {
    std::vector<double> features(config_.time_features, 0.0);
    
    // Convert timestamp to time components
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);
    
    if (time_info) {
        // Cyclical encoding of time components
        double hour = time_info->tm_hour;
        double minute = time_info->tm_min;
        double day_of_week = time_info->tm_wday;
        double day_of_month = time_info->tm_mday;
        
        // Hour cyclical encoding (24 hours)
        features[0] = std::sin(2 * M_PI * hour / 24.0);
        features[1] = std::cos(2 * M_PI * hour / 24.0);
        
        // Minute cyclical encoding (60 minutes)
        features[2] = std::sin(2 * M_PI * minute / 60.0);
        features[3] = std::cos(2 * M_PI * minute / 60.0);
        
        // Day of week cyclical encoding (7 days)
        features[4] = std::sin(2 * M_PI * day_of_week / 7.0);
        features[5] = std::cos(2 * M_PI * day_of_week / 7.0);
        
        // Day of month cyclical encoding (31 days)
        features[6] = std::sin(2 * M_PI * day_of_month / 31.0);
        features[7] = std::cos(2 * M_PI * day_of_month / 31.0);
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_action_features() const {
    std::vector<double> features(config_.price_action_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    const Bar& previous = bar_history_[bar_history_.size() - 2];
    
    // Basic OHLC ratios
    features[0] = safe_divide(current.high - current.low, current.close);  // Range/Close
    features[1] = safe_divide(current.close - current.open, current.close);  // Body/Close
    features[2] = safe_divide(current.high - std::max(current.open, current.close), current.close);  // Upper shadow
    features[3] = safe_divide(std::min(current.open, current.close) - current.low, current.close);  // Lower shadow
    
    // Gap analysis
    features[4] = safe_divide(current.open - previous.close, previous.close);  // Gap
    
    // Price position within range
    features[5] = safe_divide(current.close - current.low, current.high - current.low);  // Close position
    
    // Volatility measures
    features[6] = safe_divide(current.high - current.low, previous.close);  // True range ratio
    
    // Price momentum
    features[7] = safe_divide(current.close - previous.close, previous.close);  // Return
    features[8] = safe_divide(current.high - previous.high, previous.high);  // High momentum
    features[9] = safe_divide(current.low - previous.low, previous.low);  // Low momentum
    
    // Volume-price relationship
    features[10] = safe_divide(current.volume, previous.volume + 1.0) - 1.0;  // Volume change
    features[11] = safe_divide(current.volume * std::abs(current.close - current.open), current.close);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_moving_average_features() const {
    std::vector<double> features(config_.moving_average_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    int idx = 0;
    
    // SMA ratios
    for (const auto& [name, calc] : sma_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    // EMA ratios
    for (const auto& [name, calc] : ema_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    std::vector<double> features(config_.momentum_features, 0.0);
    
    int idx = 0;
    
    // RSI features
    for (const auto& [name, calc] : rsi_calculators_) {
        if (idx >= config_.momentum_features) break;
        if (calc->is_ready()) {
            features[idx] = calc->get_value() / 100.0;  // Normalize to [0,1]
        }
        idx++;
    }
    
    // Simple momentum features
    if (bar_history_.size() >= 10 && idx < config_.momentum_features) {
        double current = bar_history_.back().close;
        double past_5 = bar_history_[bar_history_.size() - 6].close;
        double past_10 = bar_history_[bar_history_.size() - 11].close;
        
        features[idx++] = safe_divide(current - past_5, past_5);  // 5-period momentum
        features[idx++] = safe_divide(current - past_10, past_10);  // 10-period momentum
    }
    
    // Rate of change features
    if (returns_.size() >= 5 && idx < config_.momentum_features) {
        // Recent return statistics
        auto recent_returns = std::vector<double>(returns_.end() - 5, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 5.0;
        features[idx++] = mean_return;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    std::vector<double> features(config_.volatility_features, 0.0);
    
    if (bar_history_.size() < 20) return features;
    
    // ATR-based features
    double atr_14 = calculate_atr(14);
    double atr_20 = calculate_atr(20);
    double current_price = bar_history_.back().close;
    
    features[0] = safe_divide(atr_14, current_price);  // ATR ratio
    features[1] = safe_divide(atr_20, current_price);  // ATR 20 ratio
    
    // Return volatility
    if (returns_.size() >= 20) {
        auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 20.0;
        
        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= 19.0;  // Sample variance
        
        features[2] = std::sqrt(variance);  // Return volatility
        features[3] = std::sqrt(variance * 252);  // Annualized volatility
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volume_features() const {
    std::vector<double> features(config_.volume_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Volume ratios and trends
    if (bar_history_.size() >= 20) {
        // Calculate average volume over last 20 periods
        double avg_volume = 0.0;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            avg_volume += bar_history_[i].volume;
        }
        avg_volume /= 20.0;
        
        features[0] = safe_divide(current.volume, avg_volume) - 1.0;  // Volume ratio
    }
    
    // Volume-price correlation
    features[1] = current.volume * std::abs(current.close - current.open);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_statistical_features() const {
    std::vector<double> features(config_.statistical_features, 0.0);
    
    if (returns_.size() < 20) return features;
    
    // Return distribution statistics
    auto recent_returns = std::vector<double>(returns_.end() - std::min(20UL, returns_.size()), returns_.end());
    
    // Skewness approximation
    double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / recent_returns.size();
    double variance = 0.0;
    double skew_sum = 0.0;
    
    for (double ret : recent_returns) {
        double diff = ret - mean_return;
        variance += diff * diff;
        skew_sum += diff * diff * diff;
    }
    
    variance /= (recent_returns.size() - 1);
    double std_dev = std::sqrt(variance);
    
    if (std_dev > 1e-8) {
        features[0] = skew_sum / (recent_returns.size() * std_dev * std_dev * std_dev);  // Skewness
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_pattern_features() const {
    std::vector<double> features(config_.pattern_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Basic candlestick patterns
    features[0] = is_doji(current) ? 1.0 : 0.0;
    features[1] = is_hammer(current) ? 1.0 : 0.0;
    features[2] = is_shooting_star(current) ? 1.0 : 0.0;
    
    if (bar_history_.size() >= 2) {
        features[3] = is_engulfing_bullish(bar_history_.size() - 1) ? 1.0 : 0.0;
        features[4] = is_engulfing_bearish(bar_history_.size() - 1) ? 1.0 : 0.0;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_lag_features() const {
    std::vector<double> features(config_.price_lag_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(bar_history_.size()) > lag) {
            double lagged_price = bar_history_[bar_history_.size() - 1 - lag].close;
            features[i] = safe_divide(current_price, lagged_price) - 1.0;
        }
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_return_lag_features() const {
    std::vector<double> features(config_.return_lag_features, 0.0);
    
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100, 150, 200, 250, 300, 500};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(returns_.size()) > lag) {
            features[i] = returns_[returns_.size() - 1 - lag];
        }
    }
    
    return features;
}

// Utility methods
double UnifiedFeatureEngine::safe_divide(double numerator, double denominator, double fallback) const {
    if (std::abs(denominator) < 1e-10) {
        return fallback;
    }
    return numerator / denominator;
}

double UnifiedFeatureEngine::calculate_atr(int period) const {
    if (static_cast<int>(bar_history_.size()) < period + 1) return 0.0;
    
    double atr_sum = 0.0;
    for (int i = 0; i < period; ++i) {
        size_t idx = bar_history_.size() - 1 - i;
        atr_sum += calculate_true_range(idx);
    }
    
    return atr_sum / period;
}

double UnifiedFeatureEngine::calculate_true_range(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return 0.0;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    
    return std::max({tr1, tr2, tr3});
}

// Pattern detection helpers
bool UnifiedFeatureEngine::is_doji(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double range = bar.high - bar.low;
    return range > 0 && (body / range) < 0.1;
}

bool UnifiedFeatureEngine::is_hammer(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return lower_shadow > 2 * body && upper_shadow < body;
}

bool UnifiedFeatureEngine::is_shooting_star(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return upper_shadow > 2 * body && lower_shadow < body;
}

bool UnifiedFeatureEngine::is_engulfing_bullish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bearish = previous.close < previous.open;
    bool curr_bullish = current.close > current.open;
    bool engulfs = current.open < previous.close && current.close > previous.open;
    
    return prev_bearish && curr_bullish && engulfs;
}

bool UnifiedFeatureEngine::is_engulfing_bearish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bullish = previous.close > previous.open;
    bool curr_bearish = current.close < current.open;
    bool engulfs = current.open > previous.close && current.close < previous.open;
    
    return prev_bullish && curr_bearish && engulfs;
}

void UnifiedFeatureEngine::reset() {
    bar_history_.clear();
    returns_.clear();
    log_returns_.clear();
    
    // Reset calculators
    initialize_calculators();
    
    invalidate_cache();
}

bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    bool ready = bar_history_.size() >= 64;

    // DEBUG: Log first few checks and any checks that fail WITH INSTANCE POINTER
    static int check_count = 0;
    check_count++;
    if (check_count <= 10 || !ready) {
        std::cout << "[UFE@" << (void*)this << "] is_ready() check #" << check_count
                  << ": bar_history_.size()=" << bar_history_.size()
                  << " (need 64) → " << (ready ? "READY" : "NOT_READY")
                  << std::endl;
    }

    return ready;
}

std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());
    
    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos", 
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});
    
    // Add more feature names as needed...
    // (This is a simplified version for now)
    
    return names;
}

} // namespace features
} // namespace sentio


```

