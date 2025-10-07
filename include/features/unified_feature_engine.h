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