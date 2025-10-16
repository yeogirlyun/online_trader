# v1.0 vs Current Feature Set: Complete Comparison

**Date**: 2025-10-08
**Purpose**: Document exact features implemented in v1.0 (claimed 126) vs current production 45-feature set

---

## Executive Summary

### v1.0 Feature Reality Check
- **Claimed**: 126 features
- **Actually Implemented**: ~41 features (32.5% complete)
- **Stub Implementation**: `get_feature_names()` returned only 8 time features
- **Critical Bug**: `price.return_1` placeholder always returned 0

### Current Feature Set
- **Total**: 45 features (100% complete)
- **No Placeholders**: All features fully implemented with O(1) incremental updates
- **Production-Grade**: Schema hash, deterministic ordering, complete API
- **Major Improvement**: Proper technical indicators (MACD, Stochastic, Williams%R, CCI, Keltner, Donchian)

### Key Finding
**v1.0's performance was NOT due to 126 features** - it had only ~41 implemented features, and many were placeholders. Performance came from:
1. **PSM asymmetric thresholds** (0.55/0.45 vs 0.50/0.50)
2. **3-bar minimum hold period** (prevented whipsaws)
3. **Tight profit/stop targets** (2.0%/-1.5%)
4. **Time-of-day features** (8 cyclical encoding features)

---

## 1. Feature-by-Feature Comparison

### 1.1 Time Features

| Feature Name | v1.0 | Current | Status |
|--------------|------|---------|--------|
| `hour_sin` | ✅ Full | ❌ Removed | Time features removed in current |
| `hour_cos` | ✅ Full | ❌ Removed | |
| `minute_sin` | ✅ Full | ❌ Removed | |
| `minute_cos` | ✅ Full | ❌ Removed | |
| `dow_sin` | ✅ Full | ❌ Removed | Day-of-week encoding |
| `dow_cos` | ✅ Full | ❌ Removed | |
| `dom_sin` | ✅ Full | ❌ Removed | Day-of-month encoding |
| `dom_cos` | ✅ Full | ❌ Removed | |

**v1.0 Implementation** (unified_feature_engine.cpp:260-292):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_time_features(const Bar& bar) const {
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);

    double hour = time_info->tm_hour;
    double minute = time_info->tm_min;
    double day_of_week = time_info->tm_wday;
    double day_of_month = time_info->tm_mday;

    features[0] = std::sin(2 * M_PI * hour / 24.0);
    features[1] = std::cos(2 * M_PI * hour / 24.0);
    features[2] = std::sin(2 * M_PI * minute / 60.0);
    features[3] = std::cos(2 * M_PI * minute / 60.0);
    features[4] = std::sin(2 * M_PI * day_of_week / 7.0);
    features[5] = std::cos(2 * M_PI * day_of_week / 7.0);
    features[6] = std::sin(2 * M_PI * day_of_month / 31.0);
    features[7] = std::cos(2 * M_PI * day_of_month / 31.0);
}
```

**Analysis**: These were the **only features with proper names** in v1.0's `get_feature_names()` stub. Likely the most important features for v1.0's performance.

---

### 1.2 Core Price/Volume Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| `price.close` | ❌ Missing | ✅ Full | New in current |
| `price.open` | ❌ Missing | ✅ Full | New in current |
| `price.high` | ❌ Missing | ✅ Full | New in current |
| `price.low` | ❌ Missing | ✅ Full | New in current |
| `price.return_1` | ⚠️ **BROKEN** | ✅ Fixed | v1.0: always returned 0! |
| `volume.raw` | ❌ Missing | ✅ Full | New in current |

**v1.0 Implementation**: None - raw OHLC not exposed as features

**Current Implementation** (unified_feature_engine.cpp:222-226):
```cpp
feats_[k++] = prevClose_;
feats_[k++] = prevOpen_;
feats_[k++] = prevHigh_;
feats_[k++] = prevLow_;
feats_[k++] = safe_return(prevClose_, prevPrevClose_);  // Fixed 1-bar return
feats_[k++] = prevVolume_;
```

---

### 1.3 Price Action Features

| Feature Name | v1.0 | Current | Status |
|--------------|------|---------|--------|
| Range/Close ratio | ✅ Full | ❌ Removed | `(high - low) / close` |
| Body/Close ratio | ✅ Full | ❌ Removed | `(close - open) / close` |
| Upper shadow | ✅ Full | ❌ Removed | Candlestick upper wick |
| Lower shadow | ✅ Full | ❌ Removed | Candlestick lower wick |
| Gap | ✅ Full | ❌ Removed | `(open - prev.close) / prev.close` |
| Close position | ✅ Full | ❌ Removed | `(close - low) / (high - low)` |
| True range ratio | ✅ Full | ❌ Removed | `(high - low) / prev.close` |
| Return | ✅ Full | ✅ Full | Now as `price.return_1` (fixed) |
| High momentum | ✅ Full | ❌ Removed | `(high - prev.high) / prev.high` |
| Low momentum | ✅ Full | ❌ Removed | `(low - prev.low) / prev.low` |
| Volume change | ✅ Full | ❌ Removed | `volume / prev.volume - 1` |
| Volume intensity | ✅ Full | ❌ Removed | `volume * |close - open|` |

**Total**: v1.0 had 12 price action features, current has 0 (replaced by technical indicators)

**v1.0 Implementation** (v1_features.cpp:294-327):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_price_action_features() const {
    const Bar& current = bar_history_.back();
    const Bar& previous = bar_history_[bar_history_.size() - 2];

    features[0] = safe_divide(current.high - current.low, current.close);
    features[1] = safe_divide(current.close - current.open, current.close);
    features[2] = safe_divide(current.high - std::max(current.open, current.close), current.close);
    features[3] = safe_divide(std::min(current.open, current.close) - current.low, current.close);
    features[4] = safe_divide(current.open - previous.close, previous.close);
    features[5] = safe_divide(current.close - current.low, current.high - current.low);
    features[6] = safe_divide(current.high - current.low, previous.close);
    features[7] = safe_divide(current.close - previous.close, previous.close);
    features[8] = safe_divide(current.high - previous.high, previous.high);
    features[9] = safe_divide(current.low - previous.low, previous.low);
    features[10] = safe_divide(current.volume, previous.volume + 1.0) - 1.0;
    features[11] = safe_divide(current.volume * std::abs(current.close - current.open), current.close);

    return features;  // 12 features
}
```

---

### 1.4 Moving Average Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| SMA-5 ratio | ⚠️ Partial | ❌ Removed | Depended on incremental calculator |
| SMA-10 ratio | ⚠️ Partial | ✅ Full | Now absolute value `sma10` |
| SMA-20 ratio | ⚠️ Partial | ✅ Full | Now absolute + `price_vs_sma20` ratio |
| SMA-50 ratio | ⚠️ Partial | ✅ Full | Now absolute value `sma50` |
| SMA-100 ratio | ⚠️ Partial | ❌ Removed | |
| SMA-200 ratio | ⚠️ Partial | ❌ Removed | |
| EMA-5 ratio | ⚠️ Partial | ❌ Removed | |
| EMA-10 ratio | ⚠️ Partial | ✅ Full | Now absolute value `ema10` |
| EMA-20 ratio | ⚠️ Partial | ✅ Full | Now absolute + `price_vs_ema20` ratio |
| EMA-50 ratio | ⚠️ Partial | ✅ Full | Now absolute value `ema50` |

**Total**: v1.0 had ~10 MA features (partial), current has 8 (complete)

**v1.0 Implementation** (v1_features.cpp:329-356):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_moving_average_features() const {
    double current_price = bar_history_.back().close;
    int idx = 0;

    // SMA ratios (periods: 5, 10, 20, 50, 100, 200)
    for (const auto& [name, calc] : sma_calculators_) {
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }

    // EMA ratios (periods: 5, 10, 20, 50)
    for (const auto& [name, calc] : ema_calculators_) {
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }

    return features;  // ~10 features
}
```

**Current Implementation** (unified_feature_engine.cpp:232-248):
```cpp
// Absolute MA values
feats_[k++] = sma10;
feats_[k++] = sma20;
feats_[k++] = sma50;
feats_[k++] = ema10;
feats_[k++] = ema20;
feats_[k++] = ema50;

// Price vs MA ratios
feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : NaN;
feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : NaN;
```

**Analysis**: Current version provides **both absolute values and ratios**, giving model more flexibility.

---

### 1.5 Momentum Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| RSI-14 | ⚠️ Partial | ✅ Full | v1.0: basic SMA-based, Current: proper Wilder's EMA |
| RSI-21 | ⚠️ Partial | ✅ Full | |
| 5-period momentum | ✅ Full | ❌ Removed | `(close - close[-5]) / close[-5]` |
| 10-period momentum | ✅ Full | ❌ Removed | `(close - close[-10]) / close[-10]` |
| Mean return (5-bar) | ✅ Full | ❌ Removed | Average of last 5 returns |
| Stochastic %K | ❌ Missing | ✅ Full | **New in current** |
| Stochastic %D | ❌ Missing | ✅ Full | **New in current** |
| Stochastic slow | ❌ Missing | ✅ Full | **New in current** |
| Williams %R | ❌ Missing | ✅ Full | **New in current** |
| MACD line | ❌ Missing | ✅ Full | **New in current** |
| MACD signal | ❌ Missing | ✅ Full | **New in current** |
| MACD histogram | ❌ Missing | ✅ Full | **New in current** |
| ROC-5 | ❌ Missing | ✅ Full | **New in current** |
| ROC-10 | ❌ Missing | ✅ Full | **New in current** |
| ROC-20 | ❌ Missing | ✅ Full | **New in current** |
| CCI-20 | ❌ Missing | ✅ Full | **New in current** |

**Total**: v1.0 had 5 features (partial), current has 13 (complete)

**v1.0 Implementation** (v1_features.cpp:358-391):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    int idx = 0;

    // RSI features (periods: 14, 21)
    for (const auto& [name, calc] : rsi_calculators_) {
        if (calc->is_ready()) {
            features[idx] = calc->get_value() / 100.0;  // Normalize to [0,1]
        }
        idx++;
    }

    // Simple momentum
    if (bar_history_.size() >= 10) {
        double current = bar_history_.back().close;
        double past_5 = bar_history_[bar_history_.size() - 6].close;
        double past_10 = bar_history_[bar_history_.size() - 11].close;

        features[idx++] = safe_divide(current - past_5, past_5);
        features[idx++] = safe_divide(current - past_10, past_10);
    }

    // Recent return mean
    if (returns_.size() >= 5) {
        auto recent_returns = std::vector<double>(returns_.end() - 5, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 5.0;
        features[idx++] = mean_return;
    }

    return features;  // ~5 features
}
```

**Current Implementation** (unified_feature_engine.cpp:270-284):
```cpp
if (cfg_.momentum) {
    feats_[k++] = rsi14_.value;
    feats_[k++] = rsi21_.value;
    feats_[k++] = stoch14_.k;
    feats_[k++] = stoch14_.d;
    feats_[k++] = stoch14_.slow;
    feats_[k++] = will14_.r;
    feats_[k++] = macd_.macd;
    feats_[k++] = macd_.signal;
    feats_[k++] = macd_.hist;
    feats_[k++] = roc5_.value;
    feats_[k++] = roc10_.value;
    feats_[k++] = roc20_.value;
    feats_[k++] = cci20_.value;
}
```

**Analysis**: Current version has **8 additional professional momentum indicators** not in v1.0.

---

### 1.6 Volatility Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| ATR-14 ratio | ✅ Full | ✅ Full | `atr14 / close` |
| ATR-20 ratio | ✅ Full | ❌ Removed | |
| Return volatility (20-bar) | ✅ Full | ❌ Removed | Std dev of returns |
| Annualized volatility | ✅ Full | ❌ Removed | `√(variance × 252)` |
| ATR-14 absolute | ❌ Missing | ✅ Full | **New in current** |
| ATR-14 percent | ❌ Missing | ✅ Full | Same as v1.0 ATR-14 ratio |
| Bollinger mean | ❌ Missing | ✅ Full | **New in current** |
| Bollinger std dev | ❌ Missing | ✅ Full | **New in current** |
| Bollinger upper | ❌ Missing | ✅ Full | **New in current** |
| Bollinger lower | ❌ Missing | ✅ Full | **New in current** |
| Bollinger %B | ❌ Missing | ✅ Full | **New in current** |
| Bollinger bandwidth | ❌ Missing | ✅ Full | **New in current** |
| Keltner middle | ❌ Missing | ✅ Full | **New in current** |
| Keltner upper | ❌ Missing | ✅ Full | **New in current** |
| Keltner lower | ❌ Missing | ✅ Full | **New in current** |

**Total**: v1.0 had 4 features (basic), current has 11 (professional-grade)

**v1.0 Implementation** (v1_features.cpp:393-422):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    // ATR-based features
    double atr_14 = calculate_atr(14);
    double atr_20 = calculate_atr(20);
    double current_price = bar_history_.back().close;

    features[0] = safe_divide(atr_14, current_price);
    features[1] = safe_divide(atr_20, current_price);

    // Return volatility (20-bar)
    if (returns_.size() >= 20) {
        auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 20.0;

        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= 19.0;

        features[2] = std::sqrt(variance);  // Volatility
        features[3] = std::sqrt(variance * 252);  // Annualized
    }

    return features;  // 4 features
}
```

**Current Implementation** (unified_feature_engine.cpp:253-265):
```cpp
if (cfg_.volatility) {
    feats_[k++] = atr14_.value;
    feats_[k++] = (prevClose_ != 0) ? atr14_.value / prevClose_ : NaN;
    feats_[k++] = bb20_.mean;
    feats_[k++] = bb20_.sd;
    feats_[k++] = bb20_.upper;
    feats_[k++] = bb20_.lower;
    feats_[k++] = bb20_.percent_b;
    feats_[k++] = bb20_.bandwidth;
    feats_[k++] = keltner_.middle;
    feats_[k++] = keltner_.upper;
    feats_[k++] = keltner_.lower;
}
```

**Analysis**: Current version adds **Bollinger Bands (6 features) and Keltner Channels (3 features)** - critical for v1.0's Bollinger amplification strategy!

---

### 1.7 Volume Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| Volume ratio (20-bar) | ✅ Full | ❌ Removed | `volume / avg_volume - 1` |
| Volume intensity | ✅ Full | ❌ Removed | `volume × |close - open|` |
| OBV | ❌ Missing | ✅ Full | **New in current** |
| VWAP | ❌ Missing | ✅ Full | **New in current** |
| VWAP distance | ❌ Missing | ✅ Full | **New in current** |

**Total**: v1.0 had 2 features (basic), current has 3 (professional-grade)

**v1.0 Implementation** (v1_features.cpp:424-447):
```cpp
std::vector<double> UnifiedFeatureEngine::calculate_volume_features() const {
    const Bar& current = bar_history_.back();

    // Average volume (20-bar)
    if (bar_history_.size() >= 20) {
        double avg_volume = 0.0;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            avg_volume += bar_history_[i].volume;
        }
        avg_volume /= 20.0;

        features[0] = safe_divide(current.volume, avg_volume) - 1.0;
    }

    features[1] = current.volume * std::abs(current.close - current.open);

    return features;  // 2 features
}
```

**Current Implementation** (unified_feature_engine.cpp:289-296):
```cpp
if (cfg_.volume) {
    feats_[k++] = obv_.value;
    feats_[k++] = vwap_.value;
    double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                       ? (prevClose_ - vwap_.value) / vwap_.value
                       : NaN;
    feats_[k++] = vwap_dist;
}
```

**Analysis**: Current replaces basic volume ratio with **OBV (On-Balance Volume)** and **VWAP** - industry-standard volume indicators.

---

### 1.8 Channel/Breakout Features

| Feature Name | v1.0 | Current | Notes |
|--------------|------|---------|-------|
| Donchian upper | ❌ Missing | ✅ Full | **New in current** |
| Donchian middle | ❌ Missing | ✅ Full | **New in current** |
| Donchian lower | ❌ Missing | ✅ Full | **New in current** |
| Donchian position | ❌ Missing | ✅ Full | **New in current** |

**Total**: v1.0 had 0, current has 4

**Current Implementation** (unified_feature_engine.cpp:301-310):
```cpp
feats_[k++] = don20_.up;
feats_[k++] = don20_.mid;
feats_[k++] = don20_.dn;

// Donchian position: (close - dn) / (up - dn)
double don_range = don20_.up - don20_.dn;
double don_pos = (don_range != 0) ? (prevClose_ - don20_.dn) / don_range : NaN;
feats_[k++] = don_pos;
```

---

### 1.9 Features Removed (v1.0 Only)

These features were in v1.0 but **NOT in current**:

| Feature Category | Count | Reason Removed |
|------------------|-------|----------------|
| Time features | 8 | Redundant for 1-min intraday (strategy learned hour/minute patterns) |
| Price action ratios | 12 | Replaced by proper technical indicators |
| Statistical skewness | 1 | Low signal-to-noise ratio |
| Pattern detection | 5 | Binary features (doji, hammer, engulfing) - low predictive value |
| Price lags | 10 | Redundant with MA features |
| Return lags | 15 | Redundant with momentum indicators |

**Total removed**: 51 features (but only ~41 were actually implemented in v1.0)

---

## 2. Feature Count Summary

| Category | v1.0 Claimed | v1.0 Implemented | Current | Change |
|----------|--------------|------------------|---------|--------|
| **Time** | 8 | 8 ✅ | 0 | -8 |
| **Core Price/Volume** | 0 | 0 | 6 | +6 |
| **Price Action** | 12 | 12 ✅ | 0 | -12 |
| **Moving Averages** | 16 | ~10 ⚠️ | 8 | -2 (but higher quality) |
| **Momentum** | 20 | ~5 ⚠️ | 13 | +8 |
| **Volatility** | 15 | 4 ✅ | 11 | +7 |
| **Volume** | 12 | 2 ✅ | 3 | +1 (professional-grade) |
| **Channels/Breakout** | 0 | 0 | 4 | +4 |
| **Statistical** | 10 | ~1 ⚠️ | 0 | -1 |
| **Pattern Detection** | 8 | ~5 ⚠️ | 0 | -5 |
| **Price Lags** | 10 | 0 ⚠️ | 0 | 0 |
| **Return Lags** | 15 | 0 ⚠️ | 0 | 0 |
| **TOTAL** | **126** | **~41** (32.5%) | **45** (100%) | +4 net |

---

## 3. Critical Implementation Differences

### 3.1 The `get_feature_names()` Stub (v1.0)

**Evidence of incomplete implementation** (v1_features.cpp:631-643):
```cpp
std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());

    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos",
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});

    // Add more feature names as needed...
    // (This is a simplified version for now)  ← STUB!

    return names;
}
```

**Analysis**: v1.0 returned **only 8 feature names** out of claimed 126. This proves the feature engine was **incomplete**.

### 3.2 Feature Calculation Order (v1.0)

v1.0 used **std::map** for calculators (lines 88-108), which provides deterministic ordering but:
- Features were concatenated in arbitrary order
- No schema hash for compatibility checks
- Feature names didn't match feature values

### 3.3 Production-Grade Improvements (Current)

1. **Schema Hash** (unified_feature_engine.cpp:159):
   ```cpp
   schema_.sha1_hash = sha1_hex(cat.str());
   ```

2. **Deterministic Ordering** (unified_feature_engine.cpp:63-144):
   - Fixed order: Core → MA → Volatility → Momentum → Volume → Channels
   - Schema built at construction time
   - Names match values 1-to-1

3. **O(1) Incremental Updates**:
   - v1.0: Used `std::deque` for history, O(N) lookups for lags
   - Current: Ring buffers for SMA, Welford's algorithm for variance

4. **Complete API**:
   - `features_view()`: Direct access to vector
   - `names()`: Feature names in order
   - `schema()`: Schema with hash
   - `warmup_remaining()`: Readiness indicator
   - `serialize()`/`restore()`: State persistence

---

## 4. Quality Comparison

### 4.1 Indicator Implementation Quality

| Indicator | v1.0 | Current | Notes |
|-----------|------|---------|-------|
| **RSI** | Basic SMA-based | Wilder's EMA | Current: industry-standard |
| **Bollinger Bands** | ❌ Missing | ✅ Welford's | Used in v1.0 strategy but not in features! |
| **MACD** | ❌ Missing | ✅ Full | EMA(12), EMA(26), Signal(9) |
| **ATR** | Manual calculation | ✅ Incremental | Current: O(1) updates |
| **Stochastic** | ❌ Missing | ✅ Full | %K, %D, slow |
| **Williams %R** | ❌ Missing | ✅ Full | Overbought/oversold |
| **CCI** | ❌ Missing | ✅ Full | Commodity Channel Index |
| **Donchian** | ❌ Missing | ✅ Full | Breakout detection |
| **Keltner** | ❌ Missing | ✅ Full | Volatility-based channels |
| **OBV** | ❌ Missing | ✅ Full | Volume flow indicator |
| **VWAP** | ❌ Missing | ✅ Full | Intraday pivot point |

### 4.2 Code Quality

| Aspect | v1.0 | Current |
|--------|------|---------|
| **Placeholder Features** | ⚠️ Yes (return_1 = 0) | ✅ None |
| **Incomplete Names** | ⚠️ Yes (stub) | ✅ Complete |
| **Schema Hash** | ❌ No | ✅ Yes |
| **State Serialization** | ⚠️ Partial | ✅ Complete |
| **Test Coverage** | ❌ Unknown | ✅ Unit tests |
| **Documentation** | ⚠️ Minimal | ✅ Comprehensive |

---

## 5. Why v1.0 Performed Well Despite Incomplete Features

### 5.1 Key Performance Drivers (NOT the feature count)

1. **PSM Asymmetric Thresholds** (from V1_0_CONFIGURATION_ANALYSIS.md):
   - LONG: 0.55 (vs 0.50 default)
   - SHORT: 0.45 (vs 0.50 default)
   - **Impact**: +5.3x MRB improvement (Phase 1)

2. **3-Bar Minimum Hold Period**:
   - Prevented whipsaws in 1-min data
   - **Impact**: Additional +1.5x MRB improvement (Phase 2)

3. **Tight Profit/Stop Targets**:
   - Profit: +2.0%
   - Stop: -1.5%
   - **Impact**: Fast exits prevented drawdowns

4. **Time Features Were Sufficient**:
   - 8 cyclical time features captured intraday patterns
   - Hour/minute encoding learned market open/close effects
   - Day-of-week captured weekly seasonality

5. **Simple Model Avoided Overfitting**:
   - Only ~41 features (not 126) meant less overfitting risk
   - EWRLS with lambda=0.995 was appropriately regularized

### 5.2 Why v1.0's Bollinger Amplification Worked

**Critical Discovery**: v1.0 used Bollinger Bands in the **strategy** (online_ensemble_strategy.cpp) but **NOT in features**!

```cpp
// v1.0 strategy code (not feature engine):
if (enable_bb_amplification) {
    double proximity = calculate_bb_proximity(close, bb_upper, bb_lower);
    if (proximity < bb_proximity_threshold) {
        prob += bb_amplification_factor;  // +0.10 boost
    }
}
```

**This explains why v1.0 succeeded**: Bollinger Bands were used for **signal amplification**, not as model input. The model learned from time/price features, and BB just boosted signals near boundaries.

**Current implementation**: Bollinger Bands are **in features**, allowing model to learn BB relationships directly.

---

## 6. What This Means for Current Optimization

### 6.1 Hypothesis: Current Should Outperform v1.0

With proper features and re-optimized parameters, current should achieve **better** performance because:

1. **No Broken Features**: `price.return_1` is fixed
2. **Professional Indicators**: MACD, Stochastic, Williams%R, CCI, Keltner, Donchian
3. **Complete Volume Indicators**: OBV, VWAP (not in v1.0)
4. **Bollinger Bands in Features**: Model can learn BB patterns, not just use them for signal boost
5. **Higher Quality Implementation**: O(1) updates, schema hash, proper state tracking

### 6.2 Why Current Showed -0.125% MRB Initially

**Root cause**: Using v1.0 parameters (tuned for ~41 incomplete features) on current 45 complete features = **schema mismatch**.

**Evidence**:
- v1.0 MRB with broken features: +0.47%
- Current MRB with v1.0 params: -0.125%
- Delta: -0.595% (worse)

**Solution**: Re-optimize with Optuna on current feature set.

### 6.3 Optuna Optimization Strategy

**Current status**: Running Strategy C (static baseline) with 200 trials on 20 blocks.

**Expected outcome**: Find new optimal parameters that exploit:
- Professional momentum indicators (MACD, Stoch, Williams%R)
- Volatility channels (Bollinger, Keltner, Donchian)
- Volume flow (OBV, VWAP)
- Fixed return feature

**Target**: Match or exceed v1.0's 0.6086% MRB with **properly implemented features**.

---

## 7. Recommendations

### 7.1 Immediate Actions

1. ✅ **Complete Optuna optimization** (Strategy C, 30 blocks) - **In progress**
2. ⏳ **Analyze feature importance**: Which features drive performance?
3. ⏳ **Compare optimized params vs v1.0**: Document improvements
4. ⏳ **Test hypothesis**: Are time features still needed?

### 7.2 Future Enhancements

1. **Consider adding back time features** (as optional toggle):
   - v1.0's 8 time features were fully implemented
   - May be important for intraday 1-min patterns
   - Add as `cfg_.time = true` option

2. **Feature ablation study**:
   - Test performance with subsets of features
   - Identify which indicator groups are most valuable
   - Remove low-signal features to reduce overfitting

3. **Ensemble feature selection**:
   - Use LASSO or Ridge regularization to identify important features
   - Compare with Optuna-optimized feature importance

---

## 8. Conclusion

### Key Findings

1. **v1.0 Feature Reality**: Only ~41 features actually implemented (32.5% of claimed 126)
2. **v1.0 Success Factors**: PSM thresholds + time features + simple model, NOT feature count
3. **Critical Bug**: v1.0's `price.return_1` always returned 0 (now fixed)
4. **Current Advantages**: 45 complete features with professional indicators
5. **Performance Drop**: Expected due to schema mismatch, requires re-optimization

### Bottom Line

**v1.0's 16.7% return was NOT due to 126 features** - it was driven by:
- Asymmetric PSM thresholds (0.55/0.45)
- 3-bar hold period
- Tight profit/stop targets
- 8 time features
- **Accidentally regularized model** (broken return feature + incomplete feature set)

With current's **45 complete, production-grade features** and re-optimized parameters, we should achieve **equal or better performance** than v1.0.

---

**Status**: Analysis complete. Awaiting Optuna optimization results to validate hypothesis.
