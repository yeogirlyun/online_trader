#pragma once

/**
 * @file feature_config_standard.h
 * @brief Standardized 91-feature configuration for all ML strategies
 * 
 * This header provides the proven, stable 91-feature configuration discovered
 * through TFM model collapse resolution. This configuration eliminates
 * problematic features that cause training instability while maintaining
 * all meaningful predictive features.
 * 
 * Usage:
 *   auto config = sentio::features::get_standard_91_feature_config();
 *   auto engine = std::make_unique<UnifiedFeatureEngine>(config);
 * 
 * Applied to: TFM Strategy, PPO Strategy, and all future ML strategies
 */

#include "features/unified_feature_engine.h"

namespace sentio {
namespace features {

/**
 * @brief Get the standardized 91-feature configuration
 * 
 * This configuration has been validated through TFM training and prevents
 * model collapse by removing problematic feature categories:
 * - Statistical features (unstable skewness/kurtosis)
 * - Price lags (unbounded momentum) 
 * - Return lags (noisy historical returns)
 * 
 * @return UnifiedFeatureEngine::Config with proven 91-feature setup
 */
inline UnifiedFeatureEngine::Config get_standard_91_feature_config() {
    UnifiedFeatureEngine::Config config;
    
    // ✅ STABLE FEATURES (91 total)
    config.enable_time_features = true;           // Time-based features (5)
    config.enable_price_action = true;            // Price ratios and patterns (20)  
    config.enable_moving_averages = true;         // Trend indicators (15)
    config.enable_momentum = true;                // Controlled momentum (15)
    config.enable_volatility = true;              // Normalized volatility (10)
    config.enable_volume = true;                  // Volume ratios (20)
    config.enable_pattern_detection = true;       // Binary patterns (6)
    
    // 🚨 PROBLEMATIC FEATURES - REMOVED (35 total removed)
    config.enable_statistical = false;           // ❌ Unstable skewness/kurtosis (10)
    config.enable_price_lags = false;            // ❌ Unbounded momentum features (15)
    config.enable_return_lags = false;           // ❌ Noisy historical returns (10)
    
    // Standard settings
    config.normalize_features = true;
    
    return config;
}

/**
 * @brief Validate that the configuration produces exactly 91 features
 * 
 * This function should be called during initialization to ensure
 * the configuration remains consistent.
 * 
 * @param config The feature configuration to validate
 * @return true if configuration produces exactly 91 features
 */
inline bool validate_91_feature_config(const UnifiedFeatureEngine::Config& config) {
    return config.total_features() == 91;
}

/**
 * @brief Get a human-readable description of the 91-feature set
 * 
 * @return String describing the feature composition
 */
inline std::string get_91_feature_description() {
    return "Standardized 91-Feature Set: "
           "Time(5) + PriceAction(20) + MovingAvg(15) + Momentum(15) + "
           "Volatility(10) + Volume(20) + Patterns(6) = 91 features. "
           "Problematic features removed: Statistical(10) + PriceLags(15) + ReturnLags(10) = 35 removed.";
}

} // namespace features
} // namespace sentio
