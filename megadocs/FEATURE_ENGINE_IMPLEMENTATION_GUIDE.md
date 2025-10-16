# UnifiedFeatureEngine Implementation Guide

**Based on**: Production code review feedback
**Target**: 126-feature completion with O(1) incremental updates
**Approach**: Surgical fixes to existing codebase (not full rewrite)

---

## Phase 1: Fix Critical Infrastructure (P0)

### 1.1 Replace Unordered Map with Stable Ordering

**Problem**: Feature indices change between runs due to undefined iteration order

**Current** (src/features/unified_feature_engine.cpp:384-399):
```cpp
std::unordered_map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;

for (const auto& [name, calc] : sma_calculators_) {  // UNDEFINED ORDER
    features[idx++] = safe_divide(current_price, calc->get_value()) - 1.0;
}
```

**Fix**: Use std::map or std::vector with fixed ordering
```cpp
// In header: replace unordered_map
std::map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
std::map<std::string, std::unique_ptr<IncrementalEMA>> ema_calculators_;
std::map<std::string, std::unique_ptr<IncrementalRSI>> rsi_calculators_;

// OR use vector with deterministic initialization order:
struct CalculatorPair {
    std::string name;
    std::unique_ptr<IncrementalSMA> calc;
};
std::vector<CalculatorPair> sma_calculators_;
```

### 1.2 Implement Missing Public Getters

**Add to src/features/unified_feature_engine.cpp**:
```cpp
std::vector<double> UnifiedFeatureEngine::get_time_features() const {
    if (bar_history_.empty()) return std::vector<double>(config_.time_features, 0.0);
    return calculate_time_features(bar_history_.back());
}

std::vector<double> UnifiedFeatureEngine::get_price_action_features() const {
    return calculate_price_action_features();
}

std::vector<double> UnifiedFeatureEngine::get_moving_average_features() const {
    return calculate_moving_average_features();
}

std::vector<double> UnifiedFeatureEngine::get_momentum_features() const {
    return calculate_momentum_features();
}

std::vector<double> UnifiedFeatureEngine::get_volatility_features() const {
    return calculate_volatility_features();
}

std::vector<double> UnifiedFeatureEngine::get_volume_features() const {
    return calculate_volume_features();
}

std::vector<double> UnifiedFeatureEngine::get_statistical_features() const {
    return calculate_statistical_features();
}

std::vector<double> UnifiedFeatureEngine::get_pattern_features() const {
    return calculate_pattern_features();
}

std::vector<double> UnifiedFeatureEngine::get_price_lag_features() const {
    return calculate_price_lag_features();
}

std::vector<double> UnifiedFeatureEngine::get_return_lag_features() const {
    return calculate_return_lag_features();
}
```

### 1.3 Add Welford's One-Pass Variance Calculator

**Add to include/features/unified_feature_engine.h**:
```cpp
/**
 * @brief Incremental variance calculator using Welford's algorithm
 * Single-pass, numerically stable, O(1) updates
 */
class IncrementalVariance {
public:
    explicit IncrementalVariance(int period);

    void update(double value);
    double get_mean() const { return mean_; }
    double get_variance() const;
    double get_std_dev() const;
    bool is_ready() const { return count_ >= period_; }

    // For shared calculation reuse
    struct Stats {
        double mean;
        double variance;
        double std_dev;
        int count;
    };
    Stats get_stats() const;

private:
    int period_;
    int count_ = 0;
    double mean_ = 0.0;
    double m2_ = 0.0;  // Sum of squared deviations
    std::deque<double> values_;
};
```

**Add to src/features/unified_feature_engine.cpp**:
```cpp
IncrementalVariance::IncrementalVariance(int period) : period_(period) {}

void IncrementalVariance::update(double value) {
    values_.push_back(value);

    if (values_.size() > period_) {
        // Remove oldest value using Welford's algorithm in reverse
        double old_val = values_.front();
        values_.pop_front();

        double old_mean = mean_;
        count_--;
        if (count_ > 0) {
            mean_ = (count_ * old_mean - old_val) / count_;
            m2_ -= (old_val - old_mean) * (old_val - mean_);
        } else {
            mean_ = 0.0;
            m2_ = 0.0;
        }
    }

    // Add new value using Welford's algorithm
    count_ = values_.size();
    double delta = value - mean_;
    mean_ += delta / count_;
    m2_ += delta * (value - mean_);
}

double IncrementalVariance::get_variance() const {
    if (count_ < 2) return 0.0;
    return m2_ / (count_ - 1);  // Sample variance
}

double IncrementalVariance::get_std_dev() const {
    return std::sqrt(get_variance());
}

IncrementalVariance::Stats IncrementalVariance::get_stats() const {
    return {mean_, get_variance(), get_std_dev(), count_};
}
```

---

## Phase 2: Add Missing Incremental Calculators (P1)

### 2.1 IncrementalATR (Wilder's Smoothing)

**Add to include/features/unified_feature_engine.h**:
```cpp
class IncrementalATR {
public:
    explicit IncrementalATR(int period);
    double update(double high, double low, double close);
    double get_value() const { return atr_value_; }
    bool is_ready() const { return count_ >= period_; }

private:
    int period_;
    int count_ = 0;
    double prev_close_ = 0.0;
    double atr_value_ = 0.0;
    double tr_sum_ = 0.0;  // For initial SMA
    bool first_update_ = true;
};
```

**Implementation**:
```cpp
IncrementalATR::IncrementalATR(int period) : period_(period) {}

double IncrementalATR::update(double high, double low, double close) {
    double true_range;

    if (first_update_) {
        true_range = high - low;
        first_update_ = false;
    } else {
        // TR = max(H-L, |H-prev_close|, |L-prev_close|)
        true_range = std::max({
            high - low,
            std::abs(high - prev_close_),
            std::abs(low - prev_close_)
        });
    }

    prev_close_ = close;
    count_++;

    if (count_ < period_) {
        // Accumulate for initial SMA
        tr_sum_ += true_range;
        if (count_ == period_) {
            atr_value_ = tr_sum_ / period_;
        }
    } else if (count_ == period_) {
        atr_value_ = tr_sum_ / period_;
    } else {
        // Wilder's smoothing: ATR = (ATR_prev * (n-1) + TR) / n
        atr_value_ = (atr_value_ * (period_ - 1) + true_range) / period_;
    }

    return atr_value_;
}
```

### 2.2 MACD (Triple EMA)

**Add to include/features/unified_feature_engine.h**:
```cpp
class IncrementalMACD {
public:
    IncrementalMACD(int fast_period = 12, int slow_period = 26, int signal_period = 9);

    struct Values {
        double macd_line;
        double signal_line;
        double histogram;
    };

    Values update(double price);
    Values get_values() const { return values_; }
    bool is_ready() const { return signal_ema_.is_ready(); }

private:
    IncrementalEMA fast_ema_;
    IncrementalEMA slow_ema_;
    IncrementalEMA signal_ema_;
    Values values_{0.0, 0.0, 0.0};
};
```

**Implementation**:
```cpp
IncrementalMACD::IncrementalMACD(int fast, int slow, int signal)
    : fast_ema_(fast), slow_ema_(slow), signal_ema_(signal) {}

IncrementalMACD::Values IncrementalMACD::update(double price) {
    double fast = fast_ema_.update(price);
    double slow = slow_ema_.update(price);

    values_.macd_line = fast - slow;
    values_.signal_line = signal_ema_.update(values_.macd_line);
    values_.histogram = values_.macd_line - values_.signal_line;

    return values_;
}
```

### 2.3 Stochastic Oscillator

**Add to include/features/unified_feature_engine.h**:
```cpp
class IncrementalStochastic {
public:
    explicit IncrementalStochastic(int k_period = 14, int d_period = 3);

    struct Values {
        double k;      // Fast %K
        double d;      // Slow %D (3-period SMA of %K)
        double slow;   // Slow Stochastic (3-period SMA of %D)
    };

    Values update(double high, double low, double close);
    Values get_values() const { return values_; }
    bool is_ready() const;

private:
    int k_period_;
    std::deque<double> highs_;
    std::deque<double> lows_;
    IncrementalSMA d_sma_;
    IncrementalSMA slow_sma_;
    Values values_{0.0, 0.0, 0.0};
};
```

**Implementation**:
```cpp
IncrementalStochastic::IncrementalStochastic(int k_period, int d_period)
    : k_period_(k_period), d_sma_(d_period), slow_sma_(d_period) {}

IncrementalStochastic::Values IncrementalStochastic::update(double high, double low, double close) {
    highs_.push_back(high);
    lows_.push_back(low);

    if (highs_.size() > k_period_) {
        highs_.pop_front();
        lows_.pop_front();
    }

    if (highs_.size() < k_period_) {
        return values_;  // Not ready yet
    }

    double highest_high = *std::max_element(highs_.begin(), highs_.end());
    double lowest_low = *std::min_element(lows_.begin(), lows_.end());

    double range = highest_high - lowest_low;
    values_.k = (range == 0.0) ? 50.0 : ((close - lowest_low) / range) * 100.0;
    values_.d = d_sma_.update(values_.k);
    values_.slow = slow_sma_.update(values_.d);

    return values_;
}

bool IncrementalStochastic::is_ready() const {
    return highs_.size() >= k_period_ && d_sma_.is_ready() && slow_sma_.is_ready();
}
```

### 2.4 Bollinger Bands

**Add to include/features/unified_feature_engine.h**:
```cpp
class IncrementalBollingerBands {
public:
    IncrementalBollingerBands(int period = 20, double num_std = 2.0);

    struct Bands {
        double middle;    // SMA
        double upper;     // Middle + k*std
        double lower;     // Middle - k*std
        double pct_b;     // Position within bands
        double bandwidth; // (Upper - Lower) / Middle
    };

    Bands update(double price);
    Bands get_bands() const { return bands_; }
    bool is_ready() const { return variance_.is_ready(); }

private:
    IncrementalSMA sma_;
    IncrementalVariance variance_;
    double num_std_;
    Bands bands_{0.0, 0.0, 0.0, 0.0, 0.0};
};
```

**Implementation**:
```cpp
IncrementalBollingerBands::IncrementalBollingerBands(int period, double num_std)
    : sma_(period), variance_(period), num_std_(num_std) {}

IncrementalBollingerBands::Bands IncrementalBollingerBands::update(double price) {
    bands_.middle = sma_.update(price);
    variance_.update(price);

    if (!is_ready()) return bands_;

    double std_dev = variance_.get_std_dev();
    bands_.upper = bands_.middle + num_std_ * std_dev;
    bands_.lower = bands_.middle - num_std_ * std_dev;

    double range = bands_.upper - bands_.lower;
    if (range > 0) {
        bands_.pct_b = (price - bands_.lower) / range;
        bands_.bandwidth = range / bands_.middle;
    } else {
        bands_.pct_b = 0.5;
        bands_.bandwidth = 0.0;
    }

    return bands_;
}
```

### 2.5 Volume-Based Indicators

**Add VWAP, OBV**:
```cpp
class IncrementalVWAP {
public:
    IncrementalVWAP();
    double update(double price, double volume);
    double get_value() const { return vwap_; }
    void reset();  // Call at start of each session

private:
    double sum_pv_ = 0.0;
    double sum_v_ = 0.0;
    double vwap_ = 0.0;
};

class IncrementalOBV {
public:
    IncrementalOBV();
    double update(double close, double volume);
    double get_value() const { return obv_; }

private:
    double prev_close_ = 0.0;
    double obv_ = 0.0;
    bool first_update_ = true;
};
```

**Implementations**:
```cpp
IncrementalVWAP::IncrementalVWAP() {}

double IncrementalVWAP::update(double price, double volume) {
    sum_pv_ += price * volume;
    sum_v_ += volume;
    vwap_ = (sum_v_ > 0) ? (sum_pv_ / sum_v_) : price;
    return vwap_;
}

void IncrementalVWAP::reset() {
    sum_pv_ = 0.0;
    sum_v_ = 0.0;
    vwap_ = 0.0;
}

IncrementalOBV::IncrementalOBV() {}

double IncrementalOBV::update(double close, double volume) {
    if (first_update_) {
        prev_close_ = close;
        first_update_ = false;
        return obv_;
    }

    if (close > prev_close_) {
        obv_ += volume;
    } else if (close < prev_close_) {
        obv_ -= volume;
    }
    // If close == prev_close, OBV unchanged

    prev_close_ = close;
    return obv_;
}
```

---

## Phase 3: Eliminate Duplicate Calculations (P1)

### 3.1 Shared Return Statistics Helper

**Add to private section of UnifiedFeatureEngine**:
```cpp
private:
    // Shared statistics cache (updated once per bar)
    struct ReturnStatistics {
        double mean_5 = 0.0;
        double mean_20 = 0.0;
        double variance_20 = 0.0;
        double std_dev_20 = 0.0;
        double skewness_20 = 0.0;
        bool valid = false;
    };
    mutable ReturnStatistics return_stats_cache_;

    void update_return_statistics();
```

**Implementation**:
```cpp
void UnifiedFeatureEngine::update_return_statistics() {
    if (returns_.size() < 5) {
        return_stats_cache_.valid = false;
        return;
    }

    // Calculate 5-period mean
    auto recent_5 = std::vector<double>(returns_.end() - 5, returns_.end());
    return_stats_cache_.mean_5 = std::accumulate(recent_5.begin(), recent_5.end(), 0.0) / 5.0;

    if (returns_.size() < 20) {
        return_stats_cache_.valid = true;
        return;
    }

    // Calculate 20-period statistics in one pass
    auto recent_20 = std::vector<double>(returns_.end() - 20, returns_.end());
    double sum = 0.0;
    for (double r : recent_20) sum += r;
    return_stats_cache_.mean_20 = sum / 20.0;

    // Variance and skewness in same loop
    double m2 = 0.0, m3 = 0.0;
    for (double r : recent_20) {
        double diff = r - return_stats_cache_.mean_20;
        double diff2 = diff * diff;
        m2 += diff2;
        m3 += diff2 * diff;
    }

    return_stats_cache_.variance_20 = m2 / 19.0;
    return_stats_cache_.std_dev_20 = std::sqrt(return_stats_cache_.variance_20);

    if (return_stats_cache_.std_dev_20 > 1e-8) {
        double std3 = return_stats_cache_.std_dev_20 * return_stats_cache_.std_dev_20 * return_stats_cache_.std_dev_20;
        return_stats_cache_.skewness_20 = m3 / (20.0 * std3);
    } else {
        return_stats_cache_.skewness_20 = 0.0;
    }

    return_stats_cache_.valid = true;
}
```

**Usage in calculate_momentum_features()**:
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    std::vector<double> features(config_.momentum_features, 0.0);

    // Use cached mean instead of recalculating
    if (return_stats_cache_.valid) {
        features[/* appropriate index */] = return_stats_cache_.mean_5;
    }
    // ... rest of momentum features
}
```

**Usage in calculate_volatility_features()**:
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    std::vector<double> features(config_.volatility_features, 0.0);

    // Use cached variance/std_dev instead of recalculating
    if (return_stats_cache_.valid) {
        features[2] = return_stats_cache_.std_dev_20;
        features[3] = return_stats_cache_.std_dev_20 * std::sqrt(252);  // Annualized
    }
    // ... rest of volatility features
}
```

**Usage in calculate_statistical_features()**:
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_statistical_features() const {
    std::vector<double> features(config_.statistical_features, 0.0);

    // Use cached skewness instead of recalculating
    if (return_stats_cache_.valid) {
        features[0] = return_stats_cache_.skewness_20;
    }
    // ... rest of statistical features
}
```

---

## Phase 4: Integration into update() Flow

**Modify update() to use new calculators**:
```cpp
void UnifiedFeatureEngine::update(const Bar& bar) {
    // Existing bar history update
    bar_history_.push_back(bar);
    if (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
    }

    // Update returns
    update_returns(bar);

    // NEW: Update shared statistics cache
    update_return_statistics();

    // Update all incremental calculators
    update_calculators(bar);

    // Invalidate cache
    invalidate_cache();
}
```

**Modify update_calculators() to include new calculators**:
```cpp
void UnifiedFeatureEngine::update_calculators(const Bar& bar) {
    double price = bar.close;

    // Existing SMA/EMA/RSI updates
    for (auto& [name, calc] : sma_calculators_) {
        calc->update(price);
    }
    for (auto& [name, calc] : ema_calculators_) {
        calc->update(price);
    }
    for (auto& [name, calc] : rsi_calculators_) {
        calc->update(price);
    }

    // NEW: Update ATR calculators
    for (auto& [name, calc] : atr_calculators_) {
        calc->update(bar.high, bar.low, bar.close);
    }

    // NEW: Update MACD
    macd_12_26_9_.update(price);

    // NEW: Update Stochastic
    stoch_14_.update(bar.high, bar.low, bar.close);

    // NEW: Update Bollinger Bands
    bb_20_.update(price);

    // NEW: Update volume indicators
    vwap_.update(price, bar.volume);
    obv_.update(bar.close, bar.volume);
}
```

---

## Summary of Changes

### New Files (None - all modifications to existing files)

### Modified Files

**include/features/unified_feature_engine.h**:
- Change `std::unordered_map` → `std::map`
- Add `IncrementalVariance` class
- Add `IncrementalATR` class
- Add `IncrementalMACD` class
- Add `IncrementalStochastic` class
- Add `IncrementalBollingerBands` class
- Add `IncrementalVWAP` class
- Add `IncrementalOBV` class
- Add `ReturnStatistics` struct
- Add `update_return_statistics()` method
- Add new calculator members

**src/features/unified_feature_engine.cpp**:
- Implement 10 missing public getter methods
- Implement all new incremental calculator classes
- Implement `update_return_statistics()`
- Modify `initialize_calculators()` to use std::map
- Modify `update_calculators()` to update new calculators
- Modify `calculate_momentum_features()` to use cached stats
- Modify `calculate_volatility_features()` to use cached stats
- Modify `calculate_statistical_features()` to use cached stats
- Complete `get_feature_names()` with all 126 names

### Performance Impact

**Before**:
- ~300+ redundant calculations per bar
- O(N) ATR recalculation
- Undefined feature ordering

**After**:
- ~50 incremental O(1) updates per bar
- One-pass statistics calculation
- Deterministic stable feature ordering
- 5-6x speedup estimated

### Testing Checklist

- [ ] All 126 features return correct count
- [ ] Feature order identical across runs
- [ ] ATR matches reference implementation
- [ ] MACD matches TradingView
- [ ] Bollinger Bands match reference
- [ ] No duplicate variance calculations
- [ ] Update time < 1ms per bar
- [ ] Integration test with OnlineEnsembleStrategy
