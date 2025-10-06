#pragma once

#include "common/types.h"
#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <cmath>
#include <map>
#include <unordered_map>

namespace sentio {
namespace features {

/**
 * @brief Professional-Grade XGBoost Feature Engine
 * 
 * Complete 1-to-1 feature parity with Python trainer for production-ready
 * temporal feature engineering. Implements 105 focused features designed
 * for minute-bar trading with optimal performance characteristics.
 * 
 * Feature Categories (105 total):
 * - Multi-Scale Momentum (16): 1m to 30m momentum capture with acceleration
 * - Trend Strength (12): Linear regression slopes, R-squared, persistence  
 * - Volume Temporal (13): Volume momentum, VWAP, correlations, efficiency
 * - Price Action (18): Breakouts, range position, volatility regimes, patterns
 * - State Transitions (13): MA crosses, RSI changes, regime shifts
 * - Market Context (10): Time-of-day effects, session indicators
 * - Adaptive Lags (23): Smart lagging of key features only
 * 
 * Designed for perfect matching with xgboost_trainer_focused.py.
 */
class XGBoostFeatureEngine {
public:
    /**
     * @brief Configuration for professional temporal feature engineering
     */
    struct Config {
        // Feature categories (enable/disable groups)
        bool enable_momentum = true;           // 16 features
        bool enable_trend = true;              // 12 features  
        bool enable_volume_temporal = true;    // 13 features
        bool enable_price_action = true;       // 18 features
        bool enable_state_transitions = true;  // 13 features
        bool enable_market_context = true;     // 10 features
        bool enable_adaptive_lags = true;      // 23 features
        
        // History management
        size_t max_history_size = 500;  // Sufficient for all temporal calculations
        
        // Feature naming and validation
        bool include_feature_names = true;
        size_t expected_total_features = 105;  // Professional feature set
        
        // Market session configuration (for context features)
        int market_open_hour = 9;
        int market_open_minute = 30;
        int session_length_minutes = 390;  // 6.5 hours
    };

    explicit XGBoostFeatureEngine(const Config& config);
    ~XGBoostFeatureEngine() = default;

    /**
     * @brief Initialize the focused temporal feature engine
     */
    bool initialize();

    /**
     * @brief Update with new market data bar
     */
    void update(const Bar& bar);

    /**
     * @brief Extract focused temporal feature vector (~130 features)
     * @return Vector of features matching xgboost_trainer_focused.py exactly
     */
    std::vector<double> extract_features();

    /**
     * @brief Check if engine has enough history for full feature extraction
     */
    bool is_ready() const;

    /**
     * @brief Get total number of features
     */
    size_t get_feature_count() const { return total_features_; }
    
    /**
     * @brief Reset feature engine state for new training run
     */
    void reset();

    /**
     * @brief Get feature names for model inspection and debugging
     */
    std::vector<std::string> get_feature_names() const;

    /**
     * @brief Get feature breakdown by category for analysis
     */
    struct FeatureCategoryBreakdown {
        size_t momentum_count = 15;
        size_t trend_count = 20;
        size_t volume_temporal_count = 15;
        size_t price_action_count = 20;
        size_t state_transitions_count = 15;
        size_t market_context_count = 10;
        size_t adaptive_lags_count = 25;
        size_t total_count() const {
            return momentum_count + trend_count + volume_temporal_count + 
                   price_action_count + state_transitions_count + 
                   market_context_count + adaptive_lags_count;
        }
    };
    FeatureCategoryBreakdown get_feature_breakdown() const;

private:
    Config config_;
    
    // Historical data storage for temporal calculations
    std::deque<Bar> bar_history_;
    
    // Feature computation state
    bool initialized_ = false;
    bool ready_ = false;
    size_t total_features_ = 0;
    
    // Feature names storage (for debugging and model inspection)
    std::vector<std::string> feature_names_;
    
    // Professional-grade caching system for performance optimization
    std::map<std::string, std::vector<double>> feature_cache_;
    bool cache_valid_ = false;
    
    // Cached technical indicators to avoid recomputation
    std::map<int, std::vector<double>> ma_cache_;  // Moving averages by window
    std::map<int, std::vector<double>> rsi_cache_; // RSI by period
    std::vector<double> atr_cache_;
    std::map<std::pair<int,int>, std::vector<double>> macd_cache_; // MACD by (fast, slow) params
    
    // Legacy cached calculations (for backward compatibility)
    struct CachedCalculations {
        std::vector<double> price_changes;
        std::vector<double> ma_5, ma_10, ma_20;
        std::vector<double> volatility_5m, volatility_15m, volatility_20m;
        double rsi_14;
        bool cache_valid = false;
        
        // Extended cache for professional features
        std::vector<double> momentum_features;
        std::vector<double> trend_features;
        std::vector<double> volume_features;
        std::vector<double> price_action_features;
        std::vector<double> state_transition_features;
        std::vector<double> market_context_features;
        std::vector<double> adaptive_lag_features;
    } cached_;
    
    // === FOCUSED FEATURE EXTRACTION METHODS ===
    
    // Multi-Scale Momentum (15 features)
    std::vector<double> compute_momentum_features();
    
    // Trend Strength Indicators (20 features)  
    std::vector<double> compute_trend_features();
    
    // Volume Temporal Patterns (15 features)
    std::vector<double> compute_volume_temporal_features();
    
    // Price Action Patterns (20 features)
    std::vector<double> compute_price_action_features();
    
    // Temporal State Transitions (15 features)
    std::vector<double> compute_state_transition_features();
    
    // Market Context (10 features)
    std::vector<double> compute_market_context_features();
    
    // Adaptive Lag Features (23 features)
    std::vector<double> compute_adaptive_lag_features();
    
    // CRITICAL FIX: Ordered feature computation for training-inference consistency
    void compute_all_features_to_map(std::unordered_map<std::string, double>& features);
    
    // === PROFESSIONAL TECHNICAL INDICATOR CALCULATIONS ===
    double calculate_rsi(const std::vector<double>& prices, int period = 14);
    double calculate_linear_regression_slope(const std::vector<double>& y_values);
    double calculate_linear_regression_r2(const std::vector<double>& y_values);
    double calculate_r_squared(const std::vector<double>& y_values);
    double calculate_vwap(int window);
    double calculate_efficiency(int window);
    
    // Advanced indicators for complete parity
    std::vector<double> calculate_moving_average_series(const std::string& price_type, int window);
    std::vector<double> calculate_bollinger_bands(int window, double num_std = 2.0);
    double calculate_atr(int window);
    std::vector<double> calculate_macd(int fast = 12, int slow = 26, int signal = 9);
    
    // === FEATURE COMPUTATION HELPER METHODS ===
    // Momentum calculation helpers
    double compute_single_momentum(int period);
    double compute_single_momentum_at_offset(int period, int offset);
    double compute_rolling_volatility(int window);
    
    // Trend calculation helpers  
    double compute_regression_slope(int window);
    double compute_regression_r2(int window);
    double compute_consecutive_trend_bars(bool uptrend);
    double compute_regression_slope_at_offset(int window, int offset);
    double compute_volume_ratio_at_offset(int window, int offset);
    double compute_vwap_momentum_at_offset(int window, int offset);
    double compute_volatility_ratio_at_offset(int offset);
    double compute_price_efficiency_at_offset(int window, int offset);
    
    // Volume calculation helpers
    double compute_volume_ratio(int window);
    double compute_vwap_momentum(int window);
    double compute_volume_price_correlation(int window);
    double compute_volume_zscore(int window);
    double compute_volume_acceleration(int window);
    double compute_volume_efficiency(int window);
    double compute_volume_trend_slope(int window);
    double compute_volume_percentile_rank(int window);
    
    // Price action helpers
    double compute_high_low_range(int window);
    double compute_range_position_at_offset(int window, int offset);
    double compute_price_level_break(bool break_up);
    std::map<std::string, double> compute_high_low_ranges();
    double compute_price_efficiency(int window);
    std::map<std::string, double> compute_gap_features();
    
    // State transition helpers
    double compute_ma_cross(int short_period, int long_period);
    double compute_bars_since_cross(int short_period, int long_period);
    double compute_actual_bars_since_cross(int short_period, int long_period);
    double compute_regime_change(const std::string& regime_type);
    double compute_rsi_transition(const std::string& transition_type);
    double compute_ma_alignment(const std::string& direction);
    double compute_bar_pattern(const std::string& pattern_type);
    double compute_support_resistance(const std::string& level_type);
    double compute_real_market_context(const std::string& context_type);
    
    // Market context helpers
    double compute_session_indicator(const std::string& session_type);
    double compute_time_factor(const std::string& factor_type);
    
    // Adaptive lag helpers
    double compute_feature_lag(const std::string& feature_name, int lag);
    double compute_feature_delta(const std::string& feature_name, int period);

    // === COMPREHENSIVE UTILITY METHODS ===
    void maintain_history_size();
    void update_feature_cache();
    void invalidate_cache();
    bool has_sufficient_history(size_t required_bars) const;
    double safe_divide(double numerator, double denominator, double default_value = 0.0) const;
    std::vector<double> get_price_series(const std::string& price_type, int lookback, int offset = 0) const;
    std::vector<double> get_volume_series(int lookback, int offset = 0) const;
    std::vector<double> get_returns_series(int lookback, int offset = 0) const;
    double pct_change(double current, double previous) const;
    int get_minutes_from_market_open() const;
    double lag_feature(const std::string& feature_name, int periods);
    
    // Feature validation and initialization
    bool validate_feature_count(const std::vector<double>& features, size_t expected) const;
    void initialize_feature_names();
};

// Statistical helpers for feature calculations (outside class)
namespace vec_stats {
    double sum(const std::vector<double>& v);
    double mean(const std::vector<double>& v);
    double stddev(const std::vector<double>& v);
    double correlation(const std::vector<double>& x, const std::vector<double>& y);
    double rank_percentile(const std::vector<double>& v, double value);
}

} // namespace features
} // namespace sentio
