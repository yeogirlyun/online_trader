#include "features/xgboost_feature_engine.h"
#include "features/xgboost_feature_order.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace sentio {
namespace features {

XGBoostFeatureEngine::XGBoostFeatureEngine(const Config& config) : config_(config) {
    // Calculate expected total features based on enabled categories
    // FIX: Adjusted to match Python trainer's 105 features
    total_features_ = 0;
    if (config_.enable_momentum) total_features_ += 16;
    if (config_.enable_trend) total_features_ += 12;  // Reduced from 20
    if (config_.enable_volume_temporal) total_features_ += 13; // Reduced from 15
    if (config_.enable_price_action) total_features_ += 18;   // Reduced from 20
    if (config_.enable_state_transitions) total_features_ += 13; // Reduced from 15
    if (config_.enable_market_context) total_features_ += 10;
    if (config_.enable_adaptive_lags) total_features_ += 23;   // Reduced from 25
    
    utils::log_info("🔧 XGBoostFeatureEngine (Focused Temporal) configured:");
    utils::log_info("   Momentum: " + std::to_string(config_.enable_momentum ? 16 : 0) + " features");
    utils::log_info("   Trend: " + std::to_string(config_.enable_trend ? 12 : 0) + " features");  
    utils::log_info("   Volume Temporal: " + std::to_string(config_.enable_volume_temporal ? 13 : 0) + " features");
    utils::log_info("   Price Action: " + std::to_string(config_.enable_price_action ? 18 : 0) + " features");
    utils::log_info("   State Transitions: " + std::to_string(config_.enable_state_transitions ? 13 : 0) + " features");
    utils::log_info("   Market Context: " + std::to_string(config_.enable_market_context ? 10 : 0) + " features");
    utils::log_info("   Adaptive Lags: " + std::to_string(config_.enable_adaptive_lags ? 23 : 0) + " features");
    utils::log_info("   Total features: " + std::to_string(total_features_));
    
    if (total_features_ != config_.expected_total_features) {
        utils::log_error("⚠️  Feature count mismatch: calculated=" + std::to_string(total_features_) + 
                          ", expected=" + std::to_string(config_.expected_total_features));
    }
}

bool XGBoostFeatureEngine::initialize() {
    try {
        if (config_.include_feature_names) {
            initialize_feature_names();
        }
        
        initialized_ = true;
        utils::log_info("✅ XGBoostFeatureEngine (Focused) initialized with " + std::to_string(total_features_) + " features");
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("❌ XGBoostFeatureEngine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void XGBoostFeatureEngine::update(const Bar& bar) {
    if (!initialized_) {
        utils::log_error("XGBoostFeatureEngine not initialized");
        return;
    }
    
    // Add new bar to history
    bar_history_.push_back(bar);
    maintain_history_size();
    
    // Invalidate cache since we have new data
    invalidate_cache();
    
    // Update readiness status
    ready_ = has_sufficient_history(50);  // Need at least 50 bars for all temporal features
}

std::vector<double> XGBoostFeatureEngine::extract_features() {
    if (!initialized_ || !is_ready()) {
        // Return neutral features if not ready
        return std::vector<double>(XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT, 0.0);
    }
    
    try {
        // Update cached calculations
        update_feature_cache();
        
        // CRITICAL FIX: Build feature map with ALL calculations  
        std::unordered_map<std::string, double> feature_map;
        compute_all_features_to_map(feature_map);
        
        // CRITICAL FIX: Extract features in EXACT order from training
        std::vector<double> ordered_features;
        ordered_features.reserve(XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT);
        
        for (const auto& feature_name : XGBoostFeatureOrder::FEATURE_ORDER) {
            auto it = feature_map.find(feature_name);
            if (it != feature_map.end()) {
                ordered_features.push_back(it->second);
            } else {
                utils::log_error("🚨 Missing feature: " + feature_name + " - using 0.0");
                ordered_features.push_back(0.0);
            }
        }
        
        // Validate we have exactly 105 features
        if (ordered_features.size() != XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT) {
            utils::log_error("🚨 CRITICAL: Feature count mismatch: expected " + 
                            std::to_string(XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT) + 
                            ", got " + std::to_string(ordered_features.size()));
        }
        
        // CRITICAL: Validate and sanitize all features
        size_t nan_features = 0;
        double min_val = std::numeric_limits<double>::max();
        double max_val = std::numeric_limits<double>::lowest();
        double sum_val = 0.0;
        
        for (size_t i = 0; i < ordered_features.size(); ++i) {
            double val = ordered_features[i];
            
            // Check for NaN or Inf
            if (std::isnan(val) || std::isinf(val)) {
                ordered_features[i] = 0.0;  // Replace with neutral value
                nan_features++;
                val = 0.0;
            }
            
            // Track range and sum
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
            sum_val += val;
        }
        
        return ordered_features;
        
    } catch (const std::exception& e) {
        utils::log_error("❌ CRITICAL: Ordered feature extraction failed: " + std::string(e.what()));
        return std::vector<double>(XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT, 0.0);
    }
}

bool XGBoostFeatureEngine::is_ready() const {
    return initialized_ && ready_ && bar_history_.size() >= 50;
}

std::vector<std::string> XGBoostFeatureEngine::get_feature_names() const {
    return feature_names_;
}

XGBoostFeatureEngine::FeatureCategoryBreakdown XGBoostFeatureEngine::get_feature_breakdown() const {
    FeatureCategoryBreakdown breakdown;
    
    if (!config_.enable_momentum) breakdown.momentum_count = 0;
    if (!config_.enable_trend) breakdown.trend_count = 0;
    if (!config_.enable_volume_temporal) breakdown.volume_temporal_count = 0;
    if (!config_.enable_price_action) breakdown.price_action_count = 0;
    if (!config_.enable_state_transitions) breakdown.state_transitions_count = 0;
    if (!config_.enable_market_context) breakdown.market_context_count = 0;
    if (!config_.enable_adaptive_lags) breakdown.adaptive_lags_count = 0;
    
    return breakdown;
}

// ============================================================================
// FOCUSED FEATURE EXTRACTION METHODS (matching Python trainer)
// ============================================================================

std::vector<double> XGBoostFeatureEngine::compute_momentum_features() {
    std::vector<double> features;
    features.reserve(16);
    
    // CRITICAL FIX: Ensure we have enough history
    if (bar_history_.size() < 31) {
        return std::vector<double>(16, 0.0);
    }
    
    const Bar& current = bar_history_.back();
    size_t current_idx = bar_history_.size() - 1;
    
    // FIX: Use safer indexing with bounds checking
    auto safe_get_close = [&](size_t offset) -> double {
        if (offset >= current_idx) return current.close;  // Fallback to current if invalid
        return bar_history_[current_idx - offset].close;
    };
    
    // Immediate momentum (1-5 bars) - FIXED indexing
    features.push_back(pct_change(current.close, safe_get_close(1)));   // mom_1m
    features.push_back(pct_change(current.close, safe_get_close(3)));   // mom_3m  
    features.push_back(pct_change(current.close, safe_get_close(5)));   // mom_5m
    
    // Short-term momentum (10-30 bars) - FIXED indexing
    features.push_back(pct_change(current.close, safe_get_close(10)));  // mom_10m
    features.push_back(pct_change(current.close, safe_get_close(15)));  // mom_15m
    features.push_back(pct_change(current.close, safe_get_close(30)));  // mom_30m
    
    // Momentum acceleration
    double mom_3m = features[1];
    double mom_5m = features[2];
    double mom_10m = features[3];
    double mom_15m = features[4];
    
    features.push_back(mom_3m - mom_5m);   // mom_accel_5m
    features.push_back(mom_10m - mom_15m); // mom_accel_15m
    
    // Momentum consistency (alignment checks)
    double mom_1m = features[0];
    features.push_back((mom_1m > 0 && mom_5m > 0 && mom_10m > 0) ? 1.0 : 0.0); // mom_alignment_short
    features.push_back((mom_5m > 0 && mom_15m > 0 && features[5] > 0) ? 1.0 : 0.0); // mom_alignment_long
    
    // Momentum strength
    features.push_back(std::abs(mom_5m));  // mom_strength_5m
    features.push_back(std::abs(mom_15m)); // mom_strength_15m
    
    // Momentum direction - FIXED to use proper sign function
    features.push_back(mom_5m > 1e-9 ? 1.0 : (mom_5m < -1e-9 ? -1.0 : 0.0)); // mom_direction
    
    // Momentum persistence - FIXED indexing
    double mom_5m_prev = pct_change(safe_get_close(1), safe_get_close(6));
    features.push_back((std::signbit(mom_5m) == std::signbit(mom_5m_prev)) ? 1.0 : 0.0); // mom_persistence
    
    // FIXED: Volatility calculation with proper bounds and statistics
    double vol_5m = 0.0, vol_15m = 0.0;
    
    if (bar_history_.size() >= 6) {
        std::vector<double> returns_5m;
        for (size_t i = 1; i <= 5 && i <= current_idx; ++i) {
            double ret = pct_change(safe_get_close(i-1), safe_get_close(i));
            returns_5m.push_back(ret);
        }
        if (!returns_5m.empty()) {
            double sum = std::accumulate(returns_5m.begin(), returns_5m.end(), 0.0);
            double mean = sum / returns_5m.size();
            double sq_sum = 0.0;
            for (double r : returns_5m) {
                sq_sum += (r - mean) * (r - mean);
            }
            vol_5m = std::sqrt(sq_sum / std::max(1.0, static_cast<double>(returns_5m.size() - 1)));
        }
    }
    
    if (bar_history_.size() >= 16) {
        std::vector<double> returns_15m;
        for (size_t i = 1; i <= 15 && i <= current_idx; ++i) {
            double ret = pct_change(safe_get_close(i-1), safe_get_close(i));
            returns_15m.push_back(ret);
        }
        if (!returns_15m.empty()) {
            double sum = std::accumulate(returns_15m.begin(), returns_15m.end(), 0.0);
            double mean = sum / returns_15m.size();
            double sq_sum = 0.0;
            for (double r : returns_15m) {
                sq_sum += (r - mean) * (r - mean);
            }
            vol_15m = std::sqrt(sq_sum / std::max(1.0, static_cast<double>(returns_15m.size() - 1)));
        }
    }
    
    features.push_back(vol_5m);  // mom_volatility_5m
    features.push_back(vol_15m); // mom_volatility_15m
    
    return features; // Should be exactly 16 features
}

std::vector<double> XGBoostFeatureEngine::compute_trend_features() {
    std::vector<double> features;
    if (!has_sufficient_history(31)) return std::vector<double>(12, 0.0);

    const Bar& current = bar_history_.back();
    
    // Linear regression slopes at different windows (4 features)
    for (int window : {5, 10, 20, 30}) {
        if (bar_history_.size() >= window) {
            auto prices = get_price_series("close", window);
            double slope = calculate_linear_regression_slope(prices);
            features.push_back(slope);
        } else {
            features.push_back(0.0);
        }
    }
    
    // R-squared values (4 features)  
    for (int window : {5, 10, 20, 30}) {
        if (bar_history_.size() >= window) {
            auto prices = get_price_series("close", window);
            double r2 = calculate_r_squared(prices);
            features.push_back(r2);
        } else {
            features.push_back(0.0);
        }
    }
    
    // Trend persistence (2 features)
    int uptrend_bars = 0, downtrend_bars = 0;
    if (bar_history_.size() >= 10) {
        for (size_t i = bar_history_.size() - 9; i < bar_history_.size(); ++i) {
            if (bar_history_[i].close > bar_history_[i-1].close) {
                uptrend_bars++;
                downtrend_bars = 0;
            } else if (bar_history_[i].close < bar_history_[i-1].close) {
                downtrend_bars++;
                uptrend_bars = 0;
            }
        }
    }
    features.push_back(uptrend_bars);
    features.push_back(downtrend_bars);
    
    // Trend strength vs volatility (2 features)
    if (bar_history_.size() >= 20) {
        auto prices_10 = get_price_series("close", 10);
        auto prices_20 = get_price_series("close", 20);
        double slope_10 = calculate_linear_regression_slope(prices_10);
        double slope_20 = calculate_linear_regression_slope(prices_20);
        
        // Calculate volatility for normalization
        double vol_5m = 0.0, vol_15m = 0.0;
        if (bar_history_.size() >= 5) {
            for (size_t i = bar_history_.size() - 5; i < bar_history_.size() - 1; i++) {
                double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
                vol_5m += ret * ret;
            }
            vol_5m = std::sqrt(vol_5m / 4.0);
        }
        
        if (bar_history_.size() >= 15) {
            for (size_t i = bar_history_.size() - 15; i < bar_history_.size() - 1; i++) {
                double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
                vol_15m += ret * ret;
            }
            vol_15m = std::sqrt(vol_15m / 14.0);
        }
        
        features.push_back(safe_divide(std::abs(slope_10), vol_5m + 1e-8));
        features.push_back(safe_divide(std::abs(slope_20), vol_15m + 1e-8));
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    return features; // Should be exactly 12 features
}

std::vector<double> XGBoostFeatureEngine::compute_volume_temporal_features() {
    if (!has_sufficient_history(21)) return std::vector<double>(13, 0.0);
    std::vector<double> features;

    const Bar& current = bar_history_.back();
    
    // Volume ratios (2 features)
    double mean_vol_5 = 0.0, mean_vol_20 = 0.0;
    if (bar_history_.size() >= 5) {
        for (size_t i = bar_history_.size() - 5; i < bar_history_.size(); ++i) {
            mean_vol_5 += bar_history_[i].volume;
        }
        mean_vol_5 /= 5.0;
    }
    if (bar_history_.size() >= 20) {
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            mean_vol_20 += bar_history_[i].volume;
        }
        mean_vol_20 /= 20.0;
    }
    
    features.push_back(safe_divide(current.volume, mean_vol_5 + 1));
    features.push_back(safe_divide(current.volume, mean_vol_20 + 1));

    // VWAP momentum (2 features)
    auto vwap_5 = calculate_vwap(5);
    auto vwap_20 = calculate_vwap(20);
    features.push_back(safe_divide(current.close - vwap_5, vwap_5));
    features.push_back(safe_divide(current.close - vwap_20, vwap_20));
    
    // Volume-price correlation (1 feature)
    if (bar_history_.size() >= 10) {
        auto prices = get_price_series("close", 10);
        auto volumes = get_volume_series(10);
        
        double price_mean = 0.0, vol_mean = 0.0;
        for (double p : prices) price_mean += p;
        for (double v : volumes) vol_mean += v;
        price_mean /= prices.size();
        vol_mean /= volumes.size();
        
        double covariance = 0.0, price_var = 0.0, vol_var = 0.0;
        for (size_t i = 0; i < prices.size(); ++i) {
            double p_diff = prices[i] - price_mean;
            double v_diff = volumes[i] - vol_mean;
            covariance += p_diff * v_diff;
            price_var += p_diff * p_diff;
            vol_var += v_diff * v_diff;
        }
        
        double correlation = safe_divide(covariance, std::sqrt(price_var * vol_var) + 1e-8);
        features.push_back(correlation);
    } else {
        features.push_back(0.0);
    }
    
    // Volume z-score (1 feature)
    if (bar_history_.size() >= 20) {
        auto volumes = get_volume_series(20);
        double vol_mean = 0.0, vol_var = 0.0;
        for (double v : volumes) vol_mean += v;
        vol_mean /= volumes.size();
        
        for (double v : volumes) vol_var += (v - vol_mean) * (v - vol_mean);
        double vol_std = std::sqrt(vol_var / (volumes.size() - 1));
        
        features.push_back(safe_divide(current.volume - vol_mean, vol_std + 1e-8));
    } else {
        features.push_back(0.0);
    }
    
    // Volume acceleration (2 features)
    if (bar_history_.size() >= 11) {
        double vol_5m_ago = bar_history_[bar_history_.size() - 6].volume;
        double vol_10m_ago = bar_history_[bar_history_.size() - 11].volume;
        features.push_back(pct_change(current.volume, vol_5m_ago));
        features.push_back(pct_change(current.volume, vol_10m_ago));
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // Volume-momentum alignment (1 feature)
    double vol_ratio = safe_divide(current.volume, mean_vol_5 + 1);
    if (bar_history_.size() >= 6) {
        double mom_5m = pct_change(current.close, bar_history_[bar_history_.size() - 6].close);
        features.push_back((vol_ratio > 1.0 && mom_5m > 0) ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
    }
    
    // Volume efficiency (1 feature)
    if (bar_history_.size() >= 6) {
        double price_move_5m = std::abs(pct_change(current.close, bar_history_[bar_history_.size() - 6].close));
        features.push_back(safe_divide(vol_ratio, price_move_5m + 1e-8));
    } else {
        features.push_back(0.0);
    }
    
    // Volume trend (1 feature)
    if (bar_history_.size() >= 5) {
        auto volumes = get_volume_series(5);
        features.push_back(calculate_linear_regression_slope(volumes));
    } else {
        features.push_back(0.0);
    }
    
    // Volume percentile (1 feature)
    if (bar_history_.size() >= 20) {
        auto volumes = get_volume_series(20);
        std::vector<double> sorted_volumes = volumes;
        std::sort(sorted_volumes.begin(), sorted_volumes.end());
        auto it = std::lower_bound(sorted_volumes.begin(), sorted_volumes.end(), current.volume);
        double percentile = static_cast<double>(std::distance(sorted_volumes.begin(), it)) / sorted_volumes.size();
        features.push_back(percentile);
    } else {
        features.push_back(0.5);
    }
    
    // Volume-price divergence (1 feature)
    if (bar_history_.size() >= 10) {
        auto volumes = get_volume_series(10);
        auto prices = get_price_series("close", 10);
        double vol_trend = calculate_linear_regression_slope(volumes);
        double price_trend = calculate_linear_regression_slope(prices);
        features.push_back((vol_trend > 0) != (price_trend > 0) ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
    }
    
    return features; // Should be exactly 13 features
}

std::vector<double> XGBoostFeatureEngine::compute_price_action_features() {
    if (!has_sufficient_history(21)) return std::vector<double>(18, 0.0);
    std::vector<double> features;

    const Bar& current = bar_history_.back();
    
    // Breakout detection (4 features)
    double high_5m = 0.0, low_5m = 1e9, high_20m = 0.0, low_20m = 1e9;
    
    if (bar_history_.size() >= 6) {
        for (size_t i = bar_history_.size() - 6; i < bar_history_.size() - 1; ++i) {
            high_5m = std::max(high_5m, bar_history_[i].high);
            low_5m = std::min(low_5m, bar_history_[i].low);
        }
    }
    
    if (bar_history_.size() >= 21) {
        for (size_t i = bar_history_.size() - 21; i < bar_history_.size() - 1; ++i) {
            high_20m = std::max(high_20m, bar_history_[i].high);
            low_20m = std::min(low_20m, bar_history_[i].low);
        }
    }
    
    features.push_back(current.close > high_5m ? 1.0 : 0.0);
    features.push_back(current.close < low_5m ? 1.0 : 0.0);
    features.push_back(current.close > high_20m ? 1.0 : 0.0);
    features.push_back(current.close < low_20m ? 1.0 : 0.0);
    
    // Range position (2 features)
    double range_5m = high_5m - low_5m;
    double range_20m = high_20m - low_20m;
    features.push_back(safe_divide(current.close - low_5m, range_5m));
    features.push_back(safe_divide(current.close - low_20m, range_20m));
    
    // Volatility calculations (3 features)
    double vol_5m = 0.0, vol_20m = 0.0;
    if (bar_history_.size() >= 5) {
        for (size_t i = bar_history_.size() - 5; i < bar_history_.size() - 1; i++) {
            double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
            vol_5m += ret * ret;
        }
        vol_5m = std::sqrt(vol_5m / 4.0);
    }
    
    if (bar_history_.size() >= 20) {
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size() - 1; i++) {
            double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
            vol_20m += ret * ret;
        }
        vol_20m = std::sqrt(vol_20m / 19.0);
    }
    
    features.push_back(vol_5m);
    features.push_back(vol_20m);
    features.push_back(safe_divide(vol_5m, vol_20m + 1e-8));
    
    // Price efficiency (1 feature)
    features.push_back(calculate_efficiency(10));
    
    // Gap detection (2 features)
    if (bar_history_.size() >= 2) {
        const Bar& prev = bar_history_[bar_history_.size() - 2];
        features.push_back(current.open > prev.high ? 1.0 : 0.0);
        features.push_back(current.open < prev.low ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // Inside/Outside bars (2 features)
    if (bar_history_.size() >= 2) {
        const Bar& prev = bar_history_[bar_history_.size() - 2];
        bool inside_bar = (current.high <= prev.high) && (current.low >= prev.low);
        bool outside_bar = (current.high >= prev.high) && (current.low <= prev.low);
        features.push_back(inside_bar ? 1.0 : 0.0);
        features.push_back(outside_bar ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // Price acceleration (1 feature)
    if (bar_history_.size() >= 11) {
        double mom_5m = pct_change(current.close, bar_history_[bar_history_.size() - 6].close);
        double prev_mom_5m = pct_change(bar_history_[bar_history_.size() - 2].close, 
                                       bar_history_[bar_history_.size() - 7].close);
        features.push_back(mom_5m - prev_mom_5m);
    } else {
        features.push_back(0.0);
    }
    
    // Support/resistance levels (2 features)
    features.push_back(safe_divide(current.close, high_20m) > 0.98 ? 1.0 : 0.0);
    features.push_back(safe_divide(current.close, low_20m) < 1.02 ? 1.0 : 0.0);
    
    // Volatility expansion (1 feature)
    if (bar_history_.size() >= 10) {
        double prev_vol_5m = 0.0;
        for (size_t i = bar_history_.size() - 10; i < bar_history_.size() - 6; i++) {
            double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
            prev_vol_5m += ret * ret;
        }
        prev_vol_5m = std::sqrt(prev_vol_5m / 4.0);
        features.push_back(vol_5m > prev_vol_5m ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
    }
    
    return features; // Should be exactly 18 features
}

std::vector<double> XGBoostFeatureEngine::compute_state_transition_features() {
    if (!has_sufficient_history(51)) return std::vector<double>(13, 0.0);
    std::vector<double> features;

    // MA crosses and alignments (6 features)  
    if (bar_history_.size() >= 20) {
        double ma_5 = 0.0, ma_10 = 0.0, ma_20 = 0.0;
        
        for (size_t i = bar_history_.size() - 5; i < bar_history_.size(); ++i) {
            ma_5 += bar_history_[i].close;
        }
        ma_5 /= 5.0;
        
        for (size_t i = bar_history_.size() - 10; i < bar_history_.size(); ++i) {
            ma_10 += bar_history_[i].close;
        }
        ma_10 /= 10.0;
        
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            ma_20 += bar_history_[i].close;
        }
        ma_20 /= 20.0;
        
        // Cross detection (simplified)
        features.push_back(ma_5 > ma_10 ? 1.0 : 0.0);
        features.push_back(ma_10 > ma_20 ? 1.0 : 0.0);
        
        // MA alignment
        features.push_back((ma_5 > ma_10 && ma_10 > ma_20) ? 1.0 : 0.0); // bull alignment
        features.push_back((ma_5 < ma_10 && ma_10 < ma_20) ? 1.0 : 0.0); // bear alignment
        
        // Time since cross (2 features)
        features.push_back(compute_bars_since_cross(5, 10));   // bars_since_5_10_cross
        features.push_back(compute_bars_since_cross(10, 20));  // bars_since_10_20_cross
    } else {
        for (int i = 0; i < 6; ++i) features.push_back(0.0);
    }
    
    // RSI state changes (3 features)
    if (bar_history_.size() >= 50) {
        auto prices = get_price_series("close", 50);
        double rsi = calculate_rsi(prices, 14);
        
        // Get previous RSI
        if (bar_history_.size() >= 51) {
            auto prev_prices = get_price_series("close", 50);
            prev_prices.erase(prev_prices.end() - 1); // Remove last element
            prev_prices.insert(prev_prices.begin(), bar_history_[bar_history_.size() - 51].close);
            double prev_rsi = calculate_rsi(prev_prices, 14);
            
            features.push_back((rsi > 30 && prev_rsi <= 30) ? 1.0 : 0.0); // oversold to neutral
            features.push_back((rsi < 70 && prev_rsi >= 70) ? 1.0 : 0.0); // overbought to neutral
            
            // RSI momentum (simplified)
            features.push_back(rsi - prev_rsi);
        } else {
            features.push_back(0.0);
            features.push_back(0.0);
            features.push_back(0.0);
        }
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // Volatility regime changes (1 feature)
    if (bar_history_.size() >= 70) {
        // Calculate current volatility
        double current_vol = 0.0;
        if (bar_history_.size() >= 5) {
            for (size_t i = bar_history_.size() - 5; i < bar_history_.size() - 1; i++) {
                double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
                current_vol += ret * ret;
            }
            current_vol = std::sqrt(current_vol / 4.0);
        }
        
        // Calculate 50-bar volatility for regime detection
        auto vol_series = get_returns_series(50);
        std::sort(vol_series.begin(), vol_series.end());
        double vol_80th = vol_series[static_cast<size_t>(vol_series.size() * 0.8)];
        
        bool vol_regime_high = current_vol > vol_80th;
        
        // Get previous volatility regime
        double prev_vol = 0.0;
        if (bar_history_.size() >= 10) {
            for (size_t i = bar_history_.size() - 10; i < bar_history_.size() - 6; i++) {
                double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
                prev_vol += ret * ret;
            }
            prev_vol = std::sqrt(prev_vol / 4.0);
        }
        bool prev_vol_regime_high = prev_vol > vol_80th;
        
        features.push_back(vol_regime_high != prev_vol_regime_high ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
    }
    
    // Trend regime changes (1 feature)
    if (bar_history_.size() >= 41) {
        auto prices_20 = get_price_series("close", 20);
        double trend_slope = calculate_linear_regression_slope(prices_20);
        bool trend_up = trend_slope > 0;
        
        // Get previous trend
        std::vector<double> prev_prices_20;
        for (size_t i = bar_history_.size() - 21; i < bar_history_.size() - 1; ++i) {
            prev_prices_20.push_back(bar_history_[i].close);
        }
        double prev_trend_slope = calculate_linear_regression_slope(prev_prices_20);
        bool prev_trend_up = prev_trend_slope > 0;
        
        features.push_back(trend_up != prev_trend_up ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
    }
    
    // Price level breaks (2 features)
    if (bar_history_.size() >= 21) {
        double current_high_20 = 0.0, current_low_20 = 1e9;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            current_high_20 = std::max(current_high_20, bar_history_[i].high);
            current_low_20 = std::min(current_low_20, bar_history_[i].low);
        }
        
        double prev_high_20 = 0.0, prev_low_20 = 1e9;
        for (size_t i = bar_history_.size() - 21; i < bar_history_.size() - 1; ++i) {
            prev_high_20 = std::max(prev_high_20, bar_history_[i].high);
            prev_low_20 = std::min(prev_low_20, bar_history_[i].low);
        }
        
        features.push_back(bar_history_.back().close > prev_high_20 ? 1.0 : 0.0);
        features.push_back(bar_history_.back().close < prev_low_20 ? 1.0 : 0.0);
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    return features; // Should be exactly 13 features
}

std::vector<double> XGBoostFeatureEngine::compute_market_context_features() {
    std::vector<double> features;
    
    // Time-based features (5 features)
    int minutes_from_open = get_minutes_from_market_open();
    features.push_back(minutes_from_open);
    features.push_back(config_.session_length_minutes - minutes_from_open);
    features.push_back(minutes_from_open < 120 ? 1.0 : 0.0); // morning session
    features.push_back(minutes_from_open >= 120 && minutes_from_open < 240 ? 1.0 : 0.0); // lunch session
    features.push_back(config_.session_length_minutes - minutes_from_open < 60 ? 1.0 : 0.0); // power hour
    
    // Session return (1 feature)
    if (!bar_history_.empty()) {
        double session_start_price = bar_history_[0].open;
        double session_return = pct_change(bar_history_.back().close, session_start_price);
        features.push_back(session_return);
    } else {
        features.push_back(0.0);
    }
    
    // Time volatility factor (1 feature)
    double time_vol_factor = 1.0;
    bool morning_session = minutes_from_open < 120;
    bool lunch_session = minutes_from_open >= 120 && minutes_from_open < 240;
    bool power_hour = config_.session_length_minutes - minutes_from_open < 60;
    
    if (morning_session) time_vol_factor = 1.2;
    else if (power_hour) time_vol_factor = 1.3;
    else if (lunch_session) time_vol_factor = 0.8;
    features.push_back(time_vol_factor);
    
    // End of day approaching (1 feature)
    bool eod_approaching = config_.session_length_minutes - minutes_from_open < 30;
    features.push_back(eod_approaching ? 1.0 : 0.0);
    
    // Synthetic day (1 feature)
    double synthetic_day = static_cast<double>((bar_history_.size() / 390) % 5);
    features.push_back(synthetic_day);
    
    // Reserved for future expansion (1 feature)
    features.push_back(0.0);
    
    return features; // Should be exactly 10 features
}

std::vector<double> XGBoostFeatureEngine::compute_adaptive_lag_features() {
    if (!has_sufficient_history(6)) return std::vector<double>(23, 0.0);
    std::vector<double> features;

    // Create lag features based on key indicators
    // Note: This is a simplified implementation - a full version would cache previous feature values
    
    // mom_5m lags (4 features: lag1, lag3, lag5, delta_3m)
    if (bar_history_.size() >= 11) {
        double current_mom_5m = pct_change(bar_history_.back().close, bar_history_[bar_history_.size() - 6].close);
        double lag1_mom_5m = bar_history_.size() >= 7 ? 
            pct_change(bar_history_[bar_history_.size() - 2].close, bar_history_[bar_history_.size() - 7].close) : 0.0;
        double lag3_mom_5m = bar_history_.size() >= 9 ? 
            pct_change(bar_history_[bar_history_.size() - 4].close, bar_history_[bar_history_.size() - 9].close) : 0.0;
        double lag5_mom_5m = bar_history_.size() >= 11 ? 
            pct_change(bar_history_[bar_history_.size() - 6].close, bar_history_[bar_history_.size() - 11].close) : 0.0;
        
        features.push_back(lag1_mom_5m);
        features.push_back(lag3_mom_5m);
        features.push_back(lag5_mom_5m);
        features.push_back(current_mom_5m - lag3_mom_5m); // delta_3m
    } else {
        for (int i = 0; i < 4; ++i) features.push_back(0.0);
    }
    
    // mom_15m lags (2 features: lag1, lag3)
    if (bar_history_.size() >= 19) {
        double lag1_mom_15m = pct_change(bar_history_[bar_history_.size() - 2].close, bar_history_[bar_history_.size() - 17].close);
        double lag3_mom_15m = pct_change(bar_history_[bar_history_.size() - 4].close, bar_history_[bar_history_.size() - 19].close);
        features.push_back(lag1_mom_15m);
        features.push_back(lag3_mom_15m);
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // trend_slope_10m lags (3 features: lag1, lag3, lag5)
    if (bar_history_.size() >= 15) {
        auto current_prices = get_price_series("close", 10);
        double current_slope = calculate_linear_regression_slope(current_prices);
        
        std::vector<double> lag1_prices, lag3_prices, lag5_prices;
        for (size_t i = bar_history_.size() - 11; i < bar_history_.size() - 1; ++i) {
            lag1_prices.push_back(bar_history_[i].close);
        }
        for (size_t i = bar_history_.size() - 13; i < bar_history_.size() - 3; ++i) {
            lag3_prices.push_back(bar_history_[i].close);
        }
        for (size_t i = bar_history_.size() - 15; i < bar_history_.size() - 5; ++i) {
            lag5_prices.push_back(bar_history_[i].close);
        }
        
        features.push_back(calculate_linear_regression_slope(lag1_prices));
        features.push_back(calculate_linear_regression_slope(lag3_prices));
        features.push_back(calculate_linear_regression_slope(lag5_prices));
    } else {
        for (int i = 0; i < 3; ++i) features.push_back(0.0);
    }
    
    // trend_slope_20m lags (2 features: lag1, lag3)
    if (bar_history_.size() >= 23) {
        std::vector<double> lag1_prices, lag3_prices;
        for (size_t i = bar_history_.size() - 21; i < bar_history_.size() - 1; ++i) {
            lag1_prices.push_back(bar_history_[i].close);
        }
        for (size_t i = bar_history_.size() - 23; i < bar_history_.size() - 3; ++i) {
            lag3_prices.push_back(bar_history_[i].close);
        }
        
        features.push_back(calculate_linear_regression_slope(lag1_prices));
        features.push_back(calculate_linear_regression_slope(lag3_prices));
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // Volume ratio lags (3 features: lag1, lag3, delta_3m)
    if (bar_history_.size() >= 8) {
        double current_vol_ratio = safe_divide(bar_history_.back().volume, 
                                             (bar_history_[bar_history_.size()-2].volume + 
                                              bar_history_[bar_history_.size()-3].volume + 
                                              bar_history_[bar_history_.size()-4].volume + 
                                              bar_history_[bar_history_.size()-5].volume + 
                                              bar_history_[bar_history_.size()-6].volume) / 5.0 + 1);
        double lag1_vol_ratio = safe_divide(bar_history_[bar_history_.size()-2].volume,
                                           (bar_history_[bar_history_.size()-3].volume + 
                                            bar_history_[bar_history_.size()-4].volume + 
                                            bar_history_[bar_history_.size()-5].volume + 
                                            bar_history_[bar_history_.size()-6].volume + 
                                            bar_history_[bar_history_.size()-7].volume) / 5.0 + 1);
        double lag3_vol_ratio = bar_history_.size() >= 10 ? 
                                safe_divide(bar_history_[bar_history_.size()-4].volume,
                                          (bar_history_[bar_history_.size()-5].volume + 
                                           bar_history_[bar_history_.size()-6].volume + 
                                           bar_history_[bar_history_.size()-7].volume + 
                                           bar_history_[bar_history_.size()-8].volume + 
                                           bar_history_[bar_history_.size()-9].volume) / 5.0 + 1) : 0.0;
        
        features.push_back(lag1_vol_ratio);
        features.push_back(lag3_vol_ratio);
        features.push_back(current_vol_ratio - lag3_vol_ratio);
    } else {
        for (int i = 0; i < 3; ++i) features.push_back(0.0);
    }
    
    // VWAP momentum lags (3 features: lag1, lag3, lag5)
    if (bar_history_.size() >= 10) {
        double current_vwap = calculate_vwap(5);
        double current_close = bar_history_.back().close;
        double current_vwap_momentum = safe_divide(current_close - current_vwap, current_vwap);
        
        // Simplified lag calculation
        double lag1_close = bar_history_[bar_history_.size()-2].close;
        double lag3_close = bar_history_.size() >= 4 ? bar_history_[bar_history_.size()-4].close : current_close;
        double lag5_close = bar_history_.size() >= 6 ? bar_history_[bar_history_.size()-6].close : current_close;
        
        features.push_back(safe_divide(lag1_close - current_vwap, current_vwap));
        features.push_back(safe_divide(lag3_close - current_vwap, current_vwap));
        features.push_back(safe_divide(lag5_close - current_vwap, current_vwap));
    } else {
        for (int i = 0; i < 3; ++i) features.push_back(0.0);
    }
    
    // range_position_5m lags (3 features: lag1, lag3, delta_3m)
    if (bar_history_.size() >= 8) {
        // Simplified range position calculation
        double current_high = bar_history_.back().high;
        double current_low = bar_history_.back().low;
        double current_close = bar_history_.back().close;
        double current_range_pos = safe_divide(current_close - current_low, current_high - current_low);
        
        double lag1_high = bar_history_[bar_history_.size()-2].high;
        double lag1_low = bar_history_[bar_history_.size()-2].low;
        double lag1_close = bar_history_[bar_history_.size()-2].close;
        double lag1_range_pos = safe_divide(lag1_close - lag1_low, lag1_high - lag1_low);
        
        double lag3_high = bar_history_.size() >= 4 ? bar_history_[bar_history_.size()-4].high : current_high;
        double lag3_low = bar_history_.size() >= 4 ? bar_history_[bar_history_.size()-4].low : current_low;
        double lag3_close = bar_history_.size() >= 4 ? bar_history_[bar_history_.size()-4].close : current_close;
        double lag3_range_pos = safe_divide(lag3_close - lag3_low, lag3_high - lag3_low);
        
        features.push_back(lag1_range_pos);
        features.push_back(lag3_range_pos);
        features.push_back(current_range_pos - lag3_range_pos); // delta_3m
    } else {
        for (int i = 0; i < 3; ++i) features.push_back(0.0);
    }
    
    // volatility_ratio lags (2 features: lag1, lag3)
    if (bar_history_.size() >= 25) {
        // Simplified volatility ratio calculation using price changes
        double vol_5m = 0.0, vol_20m = 0.0;
        
        // Calculate 5m volatility
        for (size_t i = bar_history_.size() - 5; i < bar_history_.size() - 1; i++) {
            double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
            vol_5m += ret * ret;
        }
        vol_5m = std::sqrt(vol_5m / 4.0);
        
        // Calculate 20m volatility
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size() - 1; i++) {
            double ret = pct_change(bar_history_[i+1].close, bar_history_[i].close);
            vol_20m += ret * ret;
        }
        vol_20m = std::sqrt(vol_20m / 19.0);
        
        double current_vol_ratio = safe_divide(vol_5m, vol_20m);
        
        // Use simplified lag calculations
        features.push_back(current_vol_ratio * 0.9); // lag1 approximation
        features.push_back(current_vol_ratio * 0.8); // lag3 approximation
    } else {
        features.push_back(0.0);
        features.push_back(0.0);
    }
    
    // efficiency_10m_lag1 (1 feature)
    if (bar_history_.size() >= 12) {
        double efficiency_lag1 = calculate_efficiency(10);
        features.push_back(efficiency_lag1);
    } else {
        features.push_back(0.0);
    }
    
    return features; // Should be exactly 23 features
}

// ============================================================================
// PROFESSIONAL TECHNICAL INDICATOR IMPLEMENTATIONS
// ============================================================================

double XGBoostFeatureEngine::calculate_linear_regression_slope(const std::vector<double>& y_values) {
    if (y_values.size() < 2) return 0.0;
    
    // FIX: Check for constant values
    double first = y_values[0];
    bool all_same = std::all_of(y_values.begin(), y_values.end(), 
        [first](double v) { return std::abs(v - first) < 1e-10; });
    if (all_same) return 0.0;  // No slope if all values are the same
    
    int n = y_values.size();
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    
    for (int i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = y_values[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double denominator = n * sum_x2 - sum_x * sum_x;
    if (std::abs(denominator) < 1e-10) return 0.0;
    
    double slope = (n * sum_xy - sum_x * sum_y) / denominator;
    
    // FIX: Normalize slope by price level to avoid scale issues
    double avg_y = sum_y / n;
    if (std::abs(avg_y) > 1e-10) {
        slope = slope / avg_y;  // Percentage slope
    }
    
    return slope;
}

double XGBoostFeatureEngine::calculate_linear_regression_r2(const std::vector<double>& y_values) {
    return calculate_r_squared(y_values);
}

double XGBoostFeatureEngine::calculate_r_squared(const std::vector<double>& y_values) {
    if (y_values.size() < 2) return 0.0;
    
    double slope = calculate_linear_regression_slope(y_values);
    
    // Calculate mean
    double mean_y = 0.0;
    for (double y : y_values) {
        mean_y += y;
    }
    mean_y /= y_values.size();
    
    // Calculate R-squared
    double ss_tot = 0.0, ss_res = 0.0;
    for (size_t i = 0; i < y_values.size(); ++i) {
        double x = static_cast<double>(i);
        double y = y_values[i];
        double y_pred = mean_y + slope * (x - (y_values.size() - 1) / 2.0);
        
        ss_tot += (y - mean_y) * (y - mean_y);
        ss_res += (y - y_pred) * (y - y_pred);
    }
    
    if (ss_tot < 1e-10) return 0.0;
    
    return 1.0 - (ss_res / ss_tot);
}

double XGBoostFeatureEngine::calculate_vwap(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    double sum_pv = 0.0, sum_v = 0.0;
    
    for (size_t i = bar_history_.size() - window; i < bar_history_.size(); ++i) {
        const Bar& bar = bar_history_[i];
        double typical_price = (bar.high + bar.low + bar.close) / 3.0;
        sum_pv += typical_price * bar.volume;
        sum_v += bar.volume;
    }
    
    return safe_divide(sum_pv, sum_v, 0.0);
}

double XGBoostFeatureEngine::calculate_efficiency(int window) {
    if (bar_history_.size() < window + 1) return 0.0;
    
    double price_displacement = std::abs(bar_history_.back().close - bar_history_[bar_history_.size() - window - 1].close);
    double total_movement = 0.0;
    
    for (size_t i = bar_history_.size() - window; i < bar_history_.size(); ++i) {
        total_movement += std::abs(bar_history_[i].close - bar_history_[i-1].close);
    }
    
    return safe_divide(price_displacement, total_movement, 0.0);
}

std::vector<double> XGBoostFeatureEngine::calculate_moving_average_series(const std::string& price_type, int window) {
    std::vector<double> prices = get_price_series(price_type, -1);  // Get all prices
    std::vector<double> ma_series;
    
    for (size_t i = window - 1; i < prices.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < window; ++j) {
            sum += prices[i - j];
        }
        ma_series.push_back(sum / window);
    }
    
    return ma_series;
}

double XGBoostFeatureEngine::calculate_rsi(const std::vector<double>& prices, int period) {
    if (prices.size() < period + 1) return 50.0;  // Neutral RSI
    
    std::vector<double> gains, losses;
    
    for (size_t i = 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i-1];
        gains.push_back(change > 0 ? change : 0.0);
        losses.push_back(change < 0 ? -change : 0.0);
    }
    
    // Calculate average gains and losses
    double avg_gain = 0.0, avg_loss = 0.0;
    
    // Initial averages
    for (int i = 0; i < period && i < gains.size(); ++i) {
        avg_gain += gains[i];
        avg_loss += losses[i];
    }
    avg_gain /= period;
    avg_loss /= period;
    
    // Wilder's smoothing
    for (size_t i = period; i < gains.size(); ++i) {
        avg_gain = ((avg_gain * (period - 1)) + gains[i]) / period;
        avg_loss = ((avg_loss * (period - 1)) + losses[i]) / period;
    }
    
    if (avg_loss < 1e-10) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

void XGBoostFeatureEngine::maintain_history_size() {
    while (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
        invalidate_cache(); // Cache needs to be recalculated
    }
}


void XGBoostFeatureEngine::invalidate_cache() {
    cache_valid_ = false;
    cached_.cache_valid = false;
}

bool XGBoostFeatureEngine::has_sufficient_history(size_t required_bars) const {
    return bar_history_.size() >= required_bars;
}

double XGBoostFeatureEngine::safe_divide(double numerator, double denominator, double default_value) const {
    if (std::abs(denominator) < 1e-10) {
        // Instead of always returning default_value, return meaningful values based on numerator
        if (std::abs(numerator) < 1e-10) {
            return default_value;  // 0/0 case
        } else if (numerator > 0) {
            return 1.0;  // Positive large value normalized to 1
        } else {
            return -1.0;  // Negative large value normalized to -1
        }
    }
    
    double result = numerator / denominator;
    
    // Clamp extreme values to prevent model instability
    result = std::max(-10.0, std::min(10.0, result));
    
    return result;
}

double XGBoostFeatureEngine::pct_change(double current, double previous) const {
    return safe_divide(current - previous, std::abs(previous), 0.0);
}

int XGBoostFeatureEngine::get_minutes_from_market_open() const {
    // For CSV data without timestamps, create synthetic time based on bar count
    // Assuming each bar represents 1 minute and market opens at 9:30 AM
    int synthetic_minutes = static_cast<int>(bar_history_.size()) % config_.session_length_minutes;
    return synthetic_minutes;
}

bool XGBoostFeatureEngine::validate_feature_count(const std::vector<double>& features, size_t expected) const {
    if (features.size() != expected) {
        utils::log_error("Feature count mismatch: got " + std::to_string(features.size()) + 
                         ", expected " + std::to_string(expected));
        return false;
    }
    return true;
}

void XGBoostFeatureEngine::initialize_feature_names() {
    feature_names_.clear();
    
    if (config_.enable_momentum) {
        feature_names_.insert(feature_names_.end(), {
            "mom_1m", "mom_3m", "mom_5m", "mom_10m", "mom_15m", "mom_30m",
            "mom_accel_5m", "mom_accel_15m", "mom_alignment_short", "mom_alignment_long",
            "mom_strength_5m", "mom_strength_15m", "mom_direction", "mom_persistence",
            "mom_volatility_5m", "mom_volatility_15m"
        });
    }
    
    if (config_.enable_trend) {
        feature_names_.insert(feature_names_.end(), {
            "trend_slope_5m", "trend_slope_10m", "trend_slope_20m", "trend_slope_30m",
            "trend_r2_5m", "trend_r2_10m", "trend_r2_20m", "trend_r2_30m",
            "uptrend_bars", "downtrend_bars", "trend_strength_10m", "trend_strength_20m"
        });
    }
    
    if (config_.enable_volume_temporal) {
        feature_names_.insert(feature_names_.end(), {
            "vol_ratio_5m", "vol_ratio_20m", "vwap_momentum_5m", "vwap_momentum_20m",
            "volume_price_corr_10m", "volume_zscore", "volume_accel_5m", "volume_accel_10m",
            "vol_mom_alignment", "volume_efficiency_5m", "volume_trend_5m", 
            "volume_percentile_20m", "vol_price_divergence"
        });
    }
    
    if (config_.enable_price_action) {
        feature_names_.insert(feature_names_.end(), {
            "above_high_5m", "below_low_5m", "above_high_20m", "below_low_20m",
            "range_position_5m", "range_position_20m", "volatility_5m", "volatility_20m",
            "volatility_ratio", "efficiency_10m", "gap_up", "gap_down", "inside_bar",
            "outside_bar", "price_accel_5m", "near_high_20m", "near_low_20m", "vol_expansion"
        });
    }
    
    if (config_.enable_state_transitions) {
        feature_names_.insert(feature_names_.end(), {
            "ma_5_10_cross", "ma_10_20_cross", "bars_since_5_10_cross", "bars_since_10_20_cross",
            "ma_alignment_bull", "ma_alignment_bear", "rsi_oversold_to_neutral", 
            "rsi_overbought_to_neutral", "rsi_momentum", "vol_regime_change_high",
            "trend_regime_change", "price_level_break_up", "price_level_break_down"
        });
    }
    
    if (config_.enable_market_context) {
        feature_names_.insert(feature_names_.end(), {
            "minutes_from_open", "minutes_to_close", "morning_session", "lunch_session",
            "power_hour", "session_return", "time_vol_factor", "eod_approaching",
            "synthetic_day", "reserved_context"
        });
    }
    
    if (config_.enable_adaptive_lags) {
        feature_names_.insert(feature_names_.end(), {
            "mom_5m_lag1", "mom_5m_lag3", "mom_5m_lag5", "mom_5m_delta_3m",
            "mom_15m_lag1", "mom_15m_lag3", "trend_slope_10m_lag1", "trend_slope_10m_lag3",
            "trend_slope_10m_lag5", "trend_slope_20m_lag1", "trend_slope_20m_lag3",
            "vol_ratio_5m_lag1", "vol_ratio_5m_lag3", "vol_ratio_5m_delta_3m",
            "vwap_momentum_5m_lag1", "vwap_momentum_5m_lag3", "vwap_momentum_5m_lag5",
            "range_position_5m_lag1", "range_position_5m_lag3", "range_position_5m_delta_3m",
            "volatility_ratio_lag1", "volatility_ratio_lag3", "efficiency_10m_lag1"
        });
    }
}

void XGBoostFeatureEngine::update_feature_cache() {
    if (cache_valid_) return;
    // Proper feature cache implementation
    if (bar_history_.size() < 2) {
        cache_valid_ = false;
        return;
    }
    
    // Cache key momentum features as vectors
    feature_cache_["mom_5m"] = {compute_single_momentum(5)};
    feature_cache_["mom_15m"] = {compute_single_momentum(15)};
    
    // Cache trend features
    if (bar_history_.size() >= 10) {
        auto prices = get_price_series("close", 10);
        feature_cache_["trend_slope_10m"] = {calculate_linear_regression_slope(prices)};
        feature_cache_["trend_r2_10m"] = {calculate_linear_regression_r2(prices)};
    }
    
    // Cache volume features
    if (bar_history_.size() >= 5) {
        feature_cache_["vol_ratio_5m"] = {compute_volume_ratio(5)};
    }
    
    cache_valid_ = true;
}

std::vector<double> XGBoostFeatureEngine::get_price_series(const std::string& price_type, int lookback, int offset) const {
    if (bar_history_.empty()) {
        return std::vector<double>{}; // Slightly different return to avoid ODR violation
    }
    
    std::vector<double> prices;
    int end_idx = static_cast<int>(bar_history_.size()) - offset;
    int start_idx = 0;
    
    if (lookback > 0) {
        start_idx = std::max(0, end_idx - lookback);
    }
    
    for (int i = start_idx; i < end_idx; ++i) {
        if (i < 0 || i >= static_cast<int>(bar_history_.size())) continue;
        
        if (price_type == "open") {
            prices.push_back(bar_history_[i].open);
        } else if (price_type == "high") {
            prices.push_back(bar_history_[i].high);
        } else if (price_type == "low") {
            prices.push_back(bar_history_[i].low);
        } else { // default to close
            prices.push_back(bar_history_[i].close);
        }
    }
    
    return prices;
}

std::vector<double> XGBoostFeatureEngine::get_volume_series(int lookback, int offset) const {
    // Check if we have enough data for volume extraction
    if (bar_history_.empty()) return {};
    
    std::vector<double> volumes;
    volumes.reserve(lookback > 0 ? lookback : bar_history_.size()); // Add optimization
    int end_idx = static_cast<int>(bar_history_.size()) - offset;
    int start_idx = 0;
    
    if (lookback > 0) {
        start_idx = std::max(0, end_idx - lookback);
    }
    
    for (int i = start_idx; i < end_idx; ++i) {
        if (i < 0 || i >= static_cast<int>(bar_history_.size())) continue;
        volumes.push_back(bar_history_[i].volume);
    }
    
    return volumes;
}

std::vector<double> XGBoostFeatureEngine::get_returns_series(int lookback, int offset) const {
    if (bar_history_.empty() || bar_history_.size() < 2) return {};
    
    std::vector<double> returns;
    int end_idx = static_cast<int>(bar_history_.size()) - offset;
    int start_idx = std::max(1, end_idx - lookback);
    
    for (int i = start_idx; i < end_idx; ++i) {
        if (i <= 0 || i >= static_cast<int>(bar_history_.size())) continue;
        double ret = pct_change(bar_history_[i].close, bar_history_[i-1].close);
        returns.push_back(ret);
    }
    
    return returns;
}

double XGBoostFeatureEngine::lag_feature(const std::string& feature_name, int periods) {
    // Simplified lag implementation - in production this would use cached feature values
    if (periods <= 0 || bar_history_.size() < periods + 1) return 0.0;
    
    // Basic lag calculation for common features
    if (feature_name == "mom_5m" && bar_history_.size() >= 6 + periods) {
        return pct_change(bar_history_[bar_history_.size() - 1 - periods].close, 
                         bar_history_[bar_history_.size() - 6 - periods].close);
    } else if (feature_name == "mom_15m" && bar_history_.size() >= 16 + periods) {
        return pct_change(bar_history_[bar_history_.size() - 1 - periods].close, 
                         bar_history_[bar_history_.size() - 16 - periods].close);
    }
    // Add more feature lag calculations as needed
    
    return 0.0;
}

// CRITICAL FIX: Compute all features individually and store in map for ordered extraction
void XGBoostFeatureEngine::compute_all_features_to_map(std::unordered_map<std::string, double>& features) {
    if (!is_ready() || bar_history_.empty()) {
        // Fill map with zeros if not ready
        for (const auto& feature_name : XGBoostFeatureOrder::FEATURE_ORDER) {
            features[feature_name] = 0.0;
        }
        return;
    }
    
    const Bar& current = bar_history_.back();
    size_t hist_size = bar_history_.size();
    
    // MOMENTUM FEATURES (16) - EXACT Python implementation
    features["mom_1m"] = compute_single_momentum(1);
    features["mom_3m"] = compute_single_momentum(3);
    features["mom_5m"] = compute_single_momentum(5);
    features["mom_10m"] = compute_single_momentum(10);
    features["mom_15m"] = compute_single_momentum(15);
    features["mom_30m"] = compute_single_momentum(30);
    
    // Momentum acceleration
    features["mom_accel_5m"] = features["mom_3m"] - features["mom_5m"];
    features["mom_accel_15m"] = features["mom_10m"] - features["mom_15m"];
    
    // Momentum alignment
    features["mom_alignment_short"] = 
        (features["mom_1m"] > 0 && features["mom_5m"] > 0 && features["mom_10m"] > 0) ? 1.0 : 0.0;
    features["mom_alignment_long"] = 
        (features["mom_5m"] > 0 && features["mom_15m"] > 0 && features["mom_30m"] > 0) ? 1.0 : 0.0;
    
    features["mom_strength_5m"] = std::abs(features["mom_5m"]);
    features["mom_strength_15m"] = std::abs(features["mom_15m"]);
    features["mom_direction"] = features["mom_5m"] > 0 ? 1.0 : -1.0;
    
    // Momentum persistence
    double mom_5m_prev = compute_single_momentum_at_offset(5, 1);
    features["mom_persistence"] = ((features["mom_5m"] > 0) == (mom_5m_prev > 0)) ? 1.0 : 0.0;
    
    // Momentum volatility
    features["mom_volatility_5m"] = compute_rolling_volatility(5);
    features["mom_volatility_15m"] = compute_rolling_volatility(15);
    
    // TREND FEATURES (12) - Match Python linear regression slopes
    features["trend_slope_5m"] = compute_regression_slope(5);
    features["trend_r2_5m"] = compute_regression_r2(5);
    features["trend_slope_10m"] = compute_regression_slope(10);
    features["trend_r2_10m"] = compute_regression_r2(10);
    features["trend_slope_20m"] = compute_regression_slope(20);
    features["trend_r2_20m"] = compute_regression_r2(20);
    features["trend_slope_30m"] = compute_regression_slope(30);
    features["trend_r2_30m"] = compute_regression_r2(30);
    
    // Trend bars
    features["uptrend_bars"] = compute_consecutive_trend_bars(true);
    features["downtrend_bars"] = compute_consecutive_trend_bars(false);
    features["trend_strength_10m"] = std::abs(features["trend_slope_10m"]) * features["trend_r2_10m"];
    features["trend_strength_20m"] = std::abs(features["trend_slope_20m"]) * features["trend_r2_20m"];
    
    // VOLUME TEMPORAL FEATURES (13)
    features["vol_ratio_5m"] = compute_volume_ratio(5);
    features["vol_ratio_20m"] = compute_volume_ratio(20);
    features["vwap_momentum_5m"] = compute_vwap_momentum(5);
    features["vwap_momentum_20m"] = compute_vwap_momentum(20);
    features["volume_price_corr_10m"] = compute_volume_price_correlation(10);
    features["volume_zscore"] = compute_volume_zscore(20);
    features["volume_accel_5m"] = compute_volume_acceleration(5);
    features["volume_accel_10m"] = compute_volume_acceleration(10);
    features["vol_mom_alignment"] = (features["volume_accel_5m"] > 0) == (features["mom_5m"] > 0) ? 1.0 : 0.0;
    features["volume_efficiency_5m"] = compute_volume_efficiency(5);
    features["volume_trend_5m"] = compute_volume_trend_slope(5);
    features["volume_percentile_20m"] = compute_volume_percentile_rank(20);
    features["vol_price_divergence"] = (std::signbit(features["volume_trend_5m"]) != std::signbit(features["trend_slope_10m"])) ? 1.0 : 0.0;
    
    // PRICE ACTION FEATURES (18)
    auto high_low_ranges = compute_high_low_ranges();
    features["above_high_5m"] = high_low_ranges["above_high_5m"];
    features["below_low_5m"] = high_low_ranges["below_low_5m"];
    features["above_high_20m"] = high_low_ranges["above_high_20m"];
    features["below_low_20m"] = high_low_ranges["below_low_20m"];
    features["range_position_5m"] = compute_range_position_at_offset(5, 0);
    features["range_position_20m"] = compute_range_position_at_offset(20, 0);
    
    features["volatility_5m"] = features["mom_volatility_5m"];  // Reuse calculated volatility
    features["volatility_20m"] = compute_rolling_volatility(20);
    features["volatility_ratio"] = safe_divide(features["volatility_5m"], features["volatility_20m"]);
    features["efficiency_10m"] = compute_price_efficiency(10);
    
    auto gap_features = compute_gap_features();
    features["gap_up"] = gap_features["gap_up"];
    features["gap_down"] = gap_features["gap_down"];
    features["inside_bar"] = compute_bar_pattern("inside_bar");
    features["outside_bar"] = compute_bar_pattern("outside_bar");
    
    features["price_accel_5m"] = features["mom_5m"] - compute_single_momentum_at_offset(5, 1);
    features["near_high_20m"] = compute_support_resistance("near_resistance");
    features["near_low_20m"] = compute_support_resistance("near_support");
    features["vol_expansion"] = (features["volatility_5m"] > compute_rolling_volatility(5)) ? 1.0 : 0.0;
    
    // STATE TRANSITION FEATURES (13)
    features["ma_5_10_cross"] = compute_ma_cross(5, 10);
    features["ma_10_20_cross"] = compute_ma_cross(10, 20);
    features["bars_since_5_10_cross"] = compute_actual_bars_since_cross(5, 10);
    features["bars_since_10_20_cross"] = compute_actual_bars_since_cross(10, 20);
    features["ma_alignment_bull"] = compute_ma_alignment("bull");
    features["ma_alignment_bear"] = compute_ma_alignment("bear");
    
    // RSI features
    features["rsi_oversold_to_neutral"] = compute_rsi_transition("oversold_to_neutral");
    features["rsi_overbought_to_neutral"] = compute_rsi_transition("overbought_to_neutral");
    features["rsi_momentum"] = compute_rsi_transition("momentum");
    
    features["vol_regime_change_high"] = compute_regime_change("volatility");
    features["trend_regime_change"] = compute_regime_change("trend");
    features["price_level_break_up"] = compute_price_level_break(true);
    features["price_level_break_down"] = compute_price_level_break(false);
    
    // MARKET CONTEXT FEATURES (10) - Session timing - simplified implementation
    int minutes_from_open = get_minutes_from_market_open();
    features["minutes_from_open"] = static_cast<double>(minutes_from_open);
    features["minutes_to_close"] = static_cast<double>(390 - minutes_from_open);
    features["morning_session"] = compute_session_indicator("morning");
    features["lunch_session"] = compute_session_indicator("lunch");
    features["power_hour"] = compute_session_indicator("power_hour");
    features["session_return"] = compute_real_market_context("session_return");
    features["time_vol_factor"] = compute_real_market_context("time_volatility_factor");
    features["eod_approaching"] = (390 - minutes_from_open < 30) ? 1.0 : 0.0;
    features["synthetic_day"] = static_cast<double>((hist_size / 390) % 5);
    
    // ADAPTIVE LAG FEATURES (23) - Lagged versions of key features
    features["mom_5m_lag1"] = compute_single_momentum_at_offset(5, 1);
    features["mom_5m_lag3"] = compute_single_momentum_at_offset(5, 3);
    features["mom_5m_lag5"] = compute_single_momentum_at_offset(5, 5);
    features["mom_5m_delta_3m"] = features["mom_5m"] - features["mom_5m_lag3"];
    
    features["mom_15m_lag1"] = compute_single_momentum_at_offset(15, 1);
    features["mom_15m_lag3"] = compute_single_momentum_at_offset(15, 3);
    
    features["trend_slope_10m_lag1"] = compute_regression_slope_at_offset(10, 1);
    features["trend_slope_10m_lag3"] = compute_regression_slope_at_offset(10, 3);
    features["trend_slope_10m_lag5"] = compute_regression_slope_at_offset(10, 5);
    
    features["trend_slope_20m_lag1"] = compute_regression_slope_at_offset(20, 1);
    features["trend_slope_20m_lag3"] = compute_regression_slope_at_offset(20, 3);
    
    features["vol_ratio_5m_lag1"] = compute_volume_ratio_at_offset(5, 1);
    features["vol_ratio_5m_lag3"] = compute_volume_ratio_at_offset(5, 3);
    features["vol_ratio_5m_delta_3m"] = features["vol_ratio_5m"] - features["vol_ratio_5m_lag3"];
    
    features["vwap_momentum_5m_lag1"] = compute_vwap_momentum_at_offset(5, 1);
    features["vwap_momentum_5m_lag3"] = compute_vwap_momentum_at_offset(5, 3);
    features["vwap_momentum_5m_lag5"] = compute_vwap_momentum_at_offset(5, 5);
    
    features["range_position_5m_lag1"] = compute_range_position_at_offset(5, 1);
    features["range_position_5m_lag3"] = compute_range_position_at_offset(5, 3);
    features["range_position_5m_delta_3m"] = features["range_position_5m"] - features["range_position_5m_lag3"];
    
    features["volatility_ratio_lag1"] = compute_volatility_ratio_at_offset(1);
    features["volatility_ratio_lag3"] = compute_volatility_ratio_at_offset(3);
    
    features["efficiency_10m_lag1"] = compute_price_efficiency_at_offset(10, 1);
    // Removed: Not present in final 105-feature order; prevents count mismatch and C-API issues
    // features["efficiency_10m_lag3"] = compute_price_efficiency_at_offset(10, 3);
}

// HELPER METHODS: Individual feature calculations for ordered extraction
double XGBoostFeatureEngine::compute_single_momentum(int period) {
    if (bar_history_.size() <= period) return 0.0;
    
    size_t current_idx = bar_history_.size() - 1;
    size_t past_idx = current_idx - period;
    
    return pct_change(bar_history_[current_idx].close, bar_history_[past_idx].close);
}

double XGBoostFeatureEngine::compute_single_momentum_at_offset(int period, int offset) {
    if (bar_history_.size() <= period + offset) return 0.0;
    
    size_t current_idx = bar_history_.size() - 1 - offset;
    size_t past_idx = current_idx - period;
    
    return pct_change(bar_history_[current_idx].close, bar_history_[past_idx].close);
}

// Additional offset-based helper functions



double XGBoostFeatureEngine::compute_rolling_volatility(int window) {
    if (bar_history_.size() < window + 1) return 0.0;
    
    std::vector<double> returns;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        if (i > 0) {
            double ret = pct_change(bar_history_[i].close, bar_history_[i-1].close);
            returns.push_back(ret);
        }
    }
    
    if (returns.empty()) return 0.0;
    
    // Calculate standard deviation (matching pandas)
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double sq_sum = 0.0;
    for (double r : returns) {
        sq_sum += (r - mean) * (r - mean);
    }
    
    // Use N-1 for sample standard deviation (matching pandas)
    return std::sqrt(sq_sum / (returns.size() - 1));
}


double XGBoostFeatureEngine::compute_regression_slope(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    std::vector<double> prices;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        prices.push_back(bar_history_[i].close);
    }
    
    return calculate_linear_regression_slope(prices);
}

double XGBoostFeatureEngine::compute_regression_slope_at_offset(int window, int offset) {
    if (bar_history_.size() < window + offset) return 0.0;
    
    std::vector<double> prices;
    size_t end_idx = bar_history_.size() - offset;
    size_t start_idx = end_idx - window;
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        prices.push_back(bar_history_[i].close);
    }
    
    return calculate_linear_regression_slope(prices);
}

double XGBoostFeatureEngine::compute_regression_r2(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    std::vector<double> prices;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        prices.push_back(bar_history_[i].close);
    }
    
    return calculate_r_squared(prices);
}

double XGBoostFeatureEngine::compute_consecutive_trend_bars(bool upward) {
    if (bar_history_.size() < 2) return 0.0;
    
    double count = 0;
    for (int i = bar_history_.size() - 1; i >= 1; --i) {
        bool is_up = bar_history_[i].close > bar_history_[i-1].close;
        if ((upward && is_up) || (!upward && !is_up)) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

// Simplified stubs for other features (can be enhanced later)
double XGBoostFeatureEngine::compute_volume_ratio(int window) {
    if (bar_history_.size() < window) return 1.0;
    
    double current_vol = bar_history_.back().volume;
    double avg_vol = 0.0;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        avg_vol += bar_history_[i].volume;
    }
    avg_vol /= window;
    
    return safe_divide(current_vol, avg_vol, 1.0);
}

double XGBoostFeatureEngine::compute_volume_ratio_at_offset(int window, int offset) {
    if (bar_history_.size() < window + offset) return 1.0;
    
    double target_vol = bar_history_[bar_history_.size() - 1 - offset].volume;
    double avg_vol = 0.0;
    size_t end_idx = bar_history_.size() - offset;
    size_t start_idx = end_idx - window;
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        avg_vol += bar_history_[i].volume;
    }
    avg_vol /= window;
    
    return safe_divide(target_vol, avg_vol, 1.0);
}

double XGBoostFeatureEngine::compute_vwap_momentum(int window) {
    return calculate_vwap(window); // Use existing method
}

double XGBoostFeatureEngine::compute_vwap_momentum_at_offset(int window, int offset) {
    // Simplified: just return momentum at offset
    return compute_single_momentum_at_offset(window, offset);
}

double XGBoostFeatureEngine::compute_volume_price_correlation(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    std::vector<double> prices, volumes;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        prices.push_back(bar_history_[i].close);
        volumes.push_back(bar_history_[i].volume);
    }
    
    // Simple correlation calculation
    if (prices.empty() || volumes.empty()) return 0.0;
    
    double price_mean = std::accumulate(prices.begin(), prices.end(), 0.0) / prices.size();
    double vol_mean = std::accumulate(volumes.begin(), volumes.end(), 0.0) / volumes.size();
    
    double numerator = 0.0, price_var = 0.0, vol_var = 0.0;
    
    for (size_t i = 0; i < prices.size(); ++i) {
        double price_diff = prices[i] - price_mean;
        double vol_diff = volumes[i] - vol_mean;
        numerator += price_diff * vol_diff;
        price_var += price_diff * price_diff;
        vol_var += vol_diff * vol_diff;
    }
    
    double denominator = std::sqrt(price_var * vol_var);
    return safe_divide(numerator, denominator, 0.0);
}

// Continue with more stub implementations for the remaining methods...
// (Adding minimum implementations to get compilation working)

std::map<std::string, double> XGBoostFeatureEngine::compute_high_low_ranges() {
    std::map<std::string, double> ranges;
    if (bar_history_.size() < 21) return ranges;
    
    // Calculate 5m and 20m high/low ranges
    double high_5m = -std::numeric_limits<double>::infinity();
    double low_5m = std::numeric_limits<double>::infinity();
    double high_20m = -std::numeric_limits<double>::infinity();
    double low_20m = std::numeric_limits<double>::infinity();
    
    // Get 5m high/low
    size_t start_5m = bar_history_.size() >= 5 ? bar_history_.size() - 5 : 0;
    for (size_t i = start_5m; i < bar_history_.size() - 1; ++i) {
        high_5m = std::max(high_5m, bar_history_[i].high);
        low_5m = std::min(low_5m, bar_history_[i].low);
    }
    
    // Get 20m high/low
    size_t start_20m = bar_history_.size() >= 20 ? bar_history_.size() - 20 : 0;
    for (size_t i = start_20m; i < bar_history_.size() - 1; ++i) {
        high_20m = std::max(high_20m, bar_history_[i].high);
        low_20m = std::min(low_20m, bar_history_[i].low);
    }
    
    double current_close = bar_history_.back().close;
    
    ranges["above_high_5m"] = (current_close > high_5m) ? 1.0 : 0.0;
    ranges["below_low_5m"] = (current_close < low_5m) ? 1.0 : 0.0;
    ranges["above_high_20m"] = (current_close > high_20m) ? 1.0 : 0.0;
    ranges["below_low_20m"] = (current_close < low_20m) ? 1.0 : 0.0;
    
    ranges["range_position_5m"] = safe_divide(current_close - low_5m, high_5m - low_5m, 0.5);
    ranges["range_position_20m"] = safe_divide(current_close - low_20m, high_20m - low_20m, 0.5);
    
    ranges["high_20m"] = high_20m;
    ranges["low_20m"] = low_20m;
    
    return ranges;
}

double XGBoostFeatureEngine::compute_range_position_at_offset(int window, int offset) {
    // Simplified implementation
    if (bar_history_.size() < window + offset) return 0.5;
    
    size_t target_idx = bar_history_.size() - 1 - offset;
    size_t start_idx = target_idx >= window ? target_idx - window : 0;
    
    double high_val = -std::numeric_limits<double>::infinity();
    double low_val = std::numeric_limits<double>::infinity();
    
    for (size_t i = start_idx; i < target_idx; ++i) {
        high_val = std::max(high_val, bar_history_[i].high);
        low_val = std::min(low_val, bar_history_[i].low);
    }
    
    double target_close = bar_history_[target_idx].close;
    return safe_divide(target_close - low_val, high_val - low_val, 0.5);
}

// === COMPREHENSIVE UTILITY METHODS IMPLEMENTATIONS ===
void XGBoostFeatureEngine::reset() {
    // Reset all state for new training run
    bar_history_.clear();
    feature_cache_.clear();
    ready_ = false;
    initialized_ = false;
}

// === FEATURE COMPUTATION HELPER METHODS IMPLEMENTATIONS ===
// NOTE: These are basic stub implementations to allow compilation
// Full implementations with exact Python parity would be implemented in actual C++ training

double XGBoostFeatureEngine::compute_volume_zscore(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    std::vector<double> volumes;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        volumes.push_back(bar_history_[i].volume);
    }
    
    double mean = vec_stats::mean(volumes);
    double std = vec_stats::stddev(volumes);
    double current_volume = bar_history_.back().volume;
    
    return safe_divide(current_volume - mean, std, 0.0);
}

double XGBoostFeatureEngine::compute_volume_acceleration(int window) {
    if (bar_history_.size() < window + 2) return 0.0;
    
    // Simple implementation: difference in volume ratios
    double current_ratio = compute_volume_ratio(window);
    
    if (bar_history_.size() < window + window + 1) return 0.0;
    
    // Calculate past volume ratio
    double past_volume = bar_history_[bar_history_.size() - window - 1].volume;
    double past_avg_volume = 0.0;
    
    for (size_t i = bar_history_.size() - window - window - 1; i < bar_history_.size() - window - 1; ++i) {
        past_avg_volume += bar_history_[i].volume;
    }
    past_avg_volume /= window;
    double past_ratio = safe_divide(past_volume, past_avg_volume, 1.0);
    
    return current_ratio - past_ratio;
}

double XGBoostFeatureEngine::compute_volume_efficiency(int window) {
    if (bar_history_.size() < window) return 1.0;
    
    // Simple implementation: volume-weighted price change efficiency
    double total_volume = 0.0;
    double volume_weighted_return = 0.0;
    
    size_t start_idx = bar_history_.size() - window;
    for (size_t i = start_idx + 1; i < bar_history_.size(); ++i) {
        double ret = pct_change(bar_history_[i].close, bar_history_[i-1].close);
        double vol = bar_history_[i].volume;
        volume_weighted_return += std::abs(ret) * vol;
        total_volume += vol;
    }
    
    return safe_divide(volume_weighted_return, total_volume, 1.0);
}

double XGBoostFeatureEngine::compute_volume_trend_slope(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    std::vector<double> volumes;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        volumes.push_back(bar_history_[i].volume);
    }
    
    return calculate_linear_regression_slope(volumes);
}

double XGBoostFeatureEngine::compute_volume_percentile_rank(int window) {
    if (bar_history_.size() < window) return 0.5;
    
    std::vector<double> volumes;
    size_t start_idx = bar_history_.size() - window;
    
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        volumes.push_back(bar_history_[i].volume);
    }
    
    double current_volume = bar_history_.back().volume;
    return vec_stats::rank_percentile(volumes, current_volume);
}

// Price action helpers
double XGBoostFeatureEngine::compute_high_low_range(int window) {
    if (bar_history_.size() < window) return 0.0;
    
    double max_high = -std::numeric_limits<double>::infinity();
    double min_low = std::numeric_limits<double>::infinity();
    
    size_t start_idx = bar_history_.size() - window;
    for (size_t i = start_idx; i < bar_history_.size(); ++i) {
        max_high = std::max(max_high, bar_history_[i].high);
        min_low = std::min(min_low, bar_history_[i].low);
    }
    
    return max_high - min_low;
}


double XGBoostFeatureEngine::compute_price_level_break(bool break_up) {
    if (bar_history_.size() < 20) return 0.0;
    
    // Simple implementation: check if current price broke recent high/low
    double current_price = bar_history_.back().close;
    
    double extreme_price = break_up ? -std::numeric_limits<double>::infinity() : 
                                     std::numeric_limits<double>::infinity();
    
    for (size_t i = bar_history_.size() - 20; i < bar_history_.size() - 1; ++i) {
        if (break_up) {
            extreme_price = std::max(extreme_price, bar_history_[i].high);
        } else {
            extreme_price = std::min(extreme_price, bar_history_[i].low);
        }
    }
    
    return (break_up && current_price > extreme_price) || 
           (!break_up && current_price < extreme_price) ? 1.0 : 0.0;
}


double XGBoostFeatureEngine::compute_price_efficiency(int window) {
    if (bar_history_.size() < window) return 1.0;
    
    // Price movement efficiency: net price change vs total price movement
    double net_change = std::abs(bar_history_.back().close - bar_history_[bar_history_.size() - window].close);
    double total_movement = 0.0;
    
    for (size_t i = bar_history_.size() - window + 1; i < bar_history_.size(); ++i) {
        total_movement += std::abs(bar_history_[i].close - bar_history_[i-1].close);
    }
    
    return safe_divide(net_change, total_movement, 1.0);
}

std::map<std::string, double> XGBoostFeatureEngine::compute_gap_features() {
    std::map<std::string, double> gaps;
    
    if (bar_history_.size() < 2) {
        gaps["gap_up"] = 0.0;
        gaps["gap_down"] = 0.0;
        return gaps;
    }
    
    // Simple gap detection: current open vs previous close
    double prev_close = bar_history_[bar_history_.size() - 2].close;
    double current_open = bar_history_.back().open;
    
    double gap_pct = pct_change(current_open, prev_close);
    
    gaps["gap_up"] = (gap_pct > 0.002) ? 1.0 : 0.0;    // 0.2% threshold
    gaps["gap_down"] = (gap_pct < -0.002) ? 1.0 : 0.0; // -0.2% threshold
    
    return gaps;
}

// State transition helpers
double XGBoostFeatureEngine::compute_ma_cross(int short_period, int long_period) {
    if (bar_history_.size() < long_period) return 0.0;
    
    auto short_ma = calculate_moving_average_series("close", short_period);
    auto long_ma = calculate_moving_average_series("close", long_period);
    
    if (short_ma.size() < 2 || long_ma.size() < 2) return 0.0;
    
    bool current_above = short_ma.back() > long_ma.back();
    bool previous_above = short_ma[short_ma.size()-2] > long_ma[long_ma.size()-2];
    
    if (current_above && !previous_above) return 1.0;  // Bullish cross
    if (!current_above && previous_above) return -1.0; // Bearish cross
    return 0.0;
}

double XGBoostFeatureEngine::compute_bars_since_cross(int short_period, int long_period) {
    return compute_actual_bars_since_cross(short_period, long_period);
}

double XGBoostFeatureEngine::compute_regime_change(const std::string& regime_type) {
    if (bar_history_.size() < 50) return 0.0;
    
    if (regime_type == "volatility") {
        // Detect high volatility regime (current vol > 1.5x average)
        double current_vol = compute_rolling_volatility(10);
        double avg_vol = 0.0;
        for (size_t i = bar_history_.size() - 50; i < bar_history_.size() - 10; ++i) {
            std::vector<double> prices;
            for (size_t j = i; j < i + 10 && j < bar_history_.size(); ++j) {
                prices.push_back(bar_history_[j].close);
            }
            avg_vol += compute_rolling_volatility(10);
        }
        avg_vol /= 40.0; // 40 periods of 10-bar volatility
        return (current_vol > 1.5 * avg_vol) ? 1.0 : 0.0;
    } else if (regime_type == "trend") {
        // Detect trend regime change (slope direction change)
        if (bar_history_.size() < 30) return 0.0;
        auto current_prices = get_price_series("close", 15);
        auto prev_prices = get_price_series("close", 15, 15); // 15 bars ago
        
        double current_slope = calculate_linear_regression_slope(current_prices);
        double prev_slope = calculate_linear_regression_slope(prev_prices);
        
        // Detect slope direction change
        bool current_up = current_slope > 0;
        bool prev_up = prev_slope > 0;
        return (current_up != prev_up) ? 1.0 : 0.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_rsi_transition(const std::string& transition_type) {
    if (bar_history_.size() < 50) return 0.0;
    auto prices = get_price_series("close", 50);
    double current_rsi = calculate_rsi(prices, 14);
    std::vector<double> prev_prices;
    for (size_t i = bar_history_.size() - 50; i < bar_history_.size() - 1; ++i) {
        prev_prices.push_back(bar_history_[i].close);
    }
    double prev_rsi = calculate_rsi(prev_prices, 14);
    if (transition_type == "oversold_to_neutral") {
        return (prev_rsi < 30.0 && current_rsi >= 30.0) ? 1.0 : 0.0;
    } else if (transition_type == "overbought_to_neutral") {
        return (prev_rsi > 70.0 && current_rsi <= 70.0) ? 1.0 : 0.0;
    } else if (transition_type == "momentum") {
        return current_rsi - prev_rsi;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_ma_alignment(const std::string& direction) {
    if (bar_history_.size() < 50) return 0.0;
    double ma_5 = 0.0, ma_10 = 0.0, ma_20 = 0.0, ma_50 = 0.0;
    for (size_t i = bar_history_.size() - 5; i < bar_history_.size(); ++i) ma_5 += bar_history_[i].close; ma_5 /= 5.0;
    for (size_t i = bar_history_.size() - 10; i < bar_history_.size(); ++i) ma_10 += bar_history_[i].close; ma_10 /= 10.0;
    for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) ma_20 += bar_history_[i].close; ma_20 /= 20.0;
    for (size_t i = bar_history_.size() - 50; i < bar_history_.size(); ++i) ma_50 += bar_history_[i].close; ma_50 /= 50.0;
    if (direction == "bull") {
        return (ma_5 > ma_10 && ma_10 > ma_20 && ma_20 > ma_50) ? 1.0 : 0.0;
    } else if (direction == "bear") {
        return (ma_5 < ma_10 && ma_10 < ma_20 && ma_20 < ma_50) ? 1.0 : 0.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_bar_pattern(const std::string& pattern_type) {
    if (bar_history_.size() < 2) return 0.0;
    const Bar& current = bar_history_.back();
    const Bar& prev = bar_history_[bar_history_.size() - 2];
    if (pattern_type == "inside_bar") {
        bool inside = (current.high <= prev.high) && (current.low >= prev.low);
        return inside ? 1.0 : 0.0;
    } else if (pattern_type == "outside_bar") {
        bool outside = (current.high > prev.high) && (current.low < prev.low);
        return outside ? 1.0 : 0.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_support_resistance(const std::string& level_type) {
    if (bar_history_.size() < 50) return 0.0;
    const Bar& current = bar_history_.back();
    std::vector<double> highs, lows;
    for (size_t i = bar_history_.size() - 50; i < bar_history_.size() - 1; ++i) {
        highs.push_back(bar_history_[i].high);
        lows.push_back(bar_history_[i].low);
    }
    std::sort(highs.begin(), highs.end());
    std::sort(lows.begin(), lows.end());
    if (level_type == "near_resistance") {
        double level = highs[static_cast<size_t>(highs.size() * 0.9)];
        double dist = std::abs(current.close - level) / std::max(1e-12, level);
        return (dist < 0.002) ? 1.0 : 0.0;
    } else if (level_type == "near_support") {
        double level = lows[static_cast<size_t>(lows.size() * 0.1)];
        double dist = std::abs(current.close - level) / std::max(1e-12, level);
        return (dist < 0.002) ? 1.0 : 0.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_real_market_context(const std::string& context_type) {
    int bar_in_day = static_cast<int>(bar_history_.size()) % 390;
    int minutes_from_open = bar_in_day;
    if (context_type == "session_return") {
        if (bar_in_day == 0 || bar_history_.size() < 2) return 0.0;
        size_t session_start_idx = bar_history_.size() - bar_in_day - 1;
        if (session_start_idx < bar_history_.size()) {
            double open_price = bar_history_[session_start_idx].open;
            double current_price = bar_history_.back().close;
            return pct_change(current_price, open_price);
        }
    } else if (context_type == "time_volatility_factor") {
        if (minutes_from_open < 30) return 1.5;
        else if (minutes_from_open < 60) return 1.3;
        else if (minutes_from_open < 120) return 1.1;
        else if (minutes_from_open < 240) return 0.8;
        else if (390 - minutes_from_open < 30) return 1.6;
        else if (390 - minutes_from_open < 60) return 1.4;
        else return 1.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_actual_bars_since_cross(int short_period, int long_period) {
    if (bar_history_.size() < static_cast<size_t>(long_period + 50)) return 50.0;
    for (int bars_ago = 0; bars_ago < 50; ++bars_ago) {
        size_t idx = bar_history_.size() - 1 - bars_ago;
        if (idx < static_cast<size_t>(long_period + 1)) break;
        double short_ma_curr = 0.0, long_ma_curr = 0.0;
        double short_ma_prev = 0.0, long_ma_prev = 0.0;
        for (int i = 0; i < short_period; ++i) short_ma_curr += bar_history_[idx - i].close; short_ma_curr /= short_period;
        for (int i = 0; i < long_period; ++i) long_ma_curr += bar_history_[idx - i].close; long_ma_curr /= long_period;
        for (int i = 0; i < short_period; ++i) short_ma_prev += bar_history_[idx - 1 - i].close; short_ma_prev /= short_period;
        for (int i = 0; i < long_period; ++i) long_ma_prev += bar_history_[idx - 1 - i].close; long_ma_prev /= long_period;
        bool curr_above = short_ma_curr > long_ma_curr;
        bool prev_above = short_ma_prev > long_ma_prev;
        if (curr_above != prev_above) return static_cast<double>(bars_ago);
    }
    return 50.0;
}

// Market context helpers
double XGBoostFeatureEngine::compute_session_indicator(const std::string& session_type) {
    int minutes_from_open = get_minutes_from_market_open();
    
    if (session_type == "morning") {
        // First 90 minutes (9:30-11:00)
        return (minutes_from_open >= 0 && minutes_from_open < 90) ? 1.0 : 0.0;
    } else if (session_type == "lunch") {
        // Lunch period (11:30-13:00)
        return (minutes_from_open >= 120 && minutes_from_open < 210) ? 1.0 : 0.0;
    } else if (session_type == "power_hour") {
        // Last 60 minutes (15:00-16:00)
        return (minutes_from_open >= 330 && minutes_from_open < 390) ? 1.0 : 0.0;
    }
    return 0.0;
}

double XGBoostFeatureEngine::compute_time_factor(const std::string& factor_type) {
    // Simplified implementation
    return 1.0;
}

// Adaptive lag helpers
double XGBoostFeatureEngine::compute_feature_lag(const std::string& feature_name, int lag) {
    if (bar_history_.size() < lag + 1) return 0.0;
    
    // Store current bar count for restoration
    size_t current_size = bar_history_.size();
    
    // Temporarily remove recent bars to simulate lag
    std::vector<Bar> temp_bars;
    for (int i = 0; i < lag; ++i) {
        temp_bars.push_back(bar_history_.back());
        bar_history_.pop_back();
    }
    
    double lagged_value = 0.0;
    if (feature_name == "mom_5m") {
        lagged_value = compute_single_momentum(5);
    } else if (feature_name == "mom_15m") {
        lagged_value = compute_single_momentum(15);
    } else if (feature_name == "trend_slope_10m") {
        auto prices = get_price_series("close", 10);
        lagged_value = calculate_linear_regression_slope(prices);
    } else if (feature_name == "vol_ratio_5m") {
        lagged_value = compute_volume_ratio(5);
    } else if (feature_name == "vwap_momentum_5m") {
        lagged_value = compute_vwap_momentum(5);
    } else if (feature_name == "range_position_5m") {
        lagged_value = compute_range_position_at_offset(5, 0);
    }
    
    // Restore bars
    for (int i = temp_bars.size() - 1; i >= 0; --i) {
        bar_history_.push_back(temp_bars[i]);
    }
    
    return lagged_value;
}

double XGBoostFeatureEngine::compute_feature_delta(const std::string& feature_name, int period) {
    if (bar_history_.size() < period + 1) return 0.0;
    
    double current_value = 0.0;
    double past_value = 0.0;
    
    // Get current value
    if (feature_name == "mom_5m") {
        current_value = compute_single_momentum(5);
    } else if (feature_name == "mom_15m") {
        current_value = compute_single_momentum(15);
    } else if (feature_name == "trend_slope_10m") {
        auto prices = get_price_series("close", 10);
        current_value = calculate_linear_regression_slope(prices);
    } else if (feature_name == "vol_ratio_5m") {
        current_value = compute_volume_ratio(5);
    }
    
    // Get past value using lag
    past_value = compute_feature_lag(feature_name, period);
    
    return current_value - past_value;
}

// Additional offset-based helpers


double XGBoostFeatureEngine::compute_volatility_ratio_at_offset(int offset) {
    if (bar_history_.size() < 20 + offset) return 1.0;
    
    // Current volatility (recent 5 bars)
    double current_vol = 0.0;
    if (bar_history_.size() >= 5) {
        std::vector<double> returns;
        for (size_t i = bar_history_.size() - 5; i < bar_history_.size(); ++i) {
            if (i > 0) {
                returns.push_back(pct_change(bar_history_[i].close, bar_history_[i-1].close));
            }
        }
        if (!returns.empty()) {
            current_vol = vec_stats::stddev(returns);
        }
    }
    
    // Past volatility (20 bars ago, 5-bar window)
    double past_vol = 0.0;
    if (bar_history_.size() >= 20 + offset + 5) {
        std::vector<double> past_returns;
        size_t start_idx = bar_history_.size() - 20 - offset - 5;
        for (size_t i = start_idx; i < start_idx + 5; ++i) {
            if (i > 0) {
                past_returns.push_back(pct_change(bar_history_[i].close, bar_history_[i-1].close));
            }
        }
        if (!past_returns.empty()) {
            past_vol = vec_stats::stddev(past_returns);
        }
    }
    
    return safe_divide(current_vol, past_vol, 1.0);
}

double XGBoostFeatureEngine::compute_price_efficiency_at_offset(int window, int offset) {
    if (bar_history_.size() < window + offset) return 0.0;
    
    size_t end_idx = bar_history_.size() - 1 - offset;
    size_t start_idx = (end_idx >= window) ? end_idx - window : 0;
    
    if (start_idx >= end_idx) return 0.0;
    
    // Calculate net price movement
    double net_movement = std::abs(bar_history_[end_idx].close - bar_history_[start_idx].close);
    
    // Calculate total movement (sum of absolute returns)
    double total_movement = 0.0;
    for (size_t i = start_idx + 1; i <= end_idx; ++i) {
        total_movement += std::abs(pct_change(bar_history_[i].close, bar_history_[i-1].close));
    }
    
    return safe_divide(net_movement, total_movement, 0.0);
}

// === VEC_STATS NAMESPACE IMPLEMENTATIONS ===
namespace vec_stats {

double sum(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0);
}

double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return sum(v) / v.size();
}

double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    
    double mean_val = mean(v);
    double sum_sq_diff = 0.0;
    
    for (double val : v) {
        double diff = val - mean_val;
        sum_sq_diff += diff * diff;
    }
    
    return std::sqrt(sum_sq_diff / (v.size() - 1));  // Sample standard deviation
}

double correlation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    
    double mean_x = mean(x);
    double mean_y = mean(y);
    
    double numerator = 0.0;
    double sum_sq_x = 0.0;
    double sum_sq_y = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;
        
        numerator += dx * dy;
        sum_sq_x += dx * dx;
        sum_sq_y += dy * dy;
    }
    
    double denominator = std::sqrt(sum_sq_x * sum_sq_y);
    return (denominator > 1e-10) ? (numerator / denominator) : 0.0;
}

double rank_percentile(const std::vector<double>& v, double value) {
    if (v.empty()) return 0.5;
    
    // Count values less than the target value
    size_t count_less = 0;
    for (double val : v) {
        if (val < value) count_less++;
    }
    
    return static_cast<double>(count_less) / v.size();
}

} // namespace vec_stats

} // namespace features
} // namespace sentio
