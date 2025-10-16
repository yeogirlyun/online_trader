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
