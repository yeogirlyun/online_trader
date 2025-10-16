#include "strategy/regime_parameter_manager.h"
#include "strategy/online_ensemble_strategy.h"
#include <fstream>
#include <iostream>

namespace sentio {

RegimeParameterManager::RegimeParameterManager() {
    load_default_params();
}

RegimeParams RegimeParameterManager::get_params_for_regime(MarketRegime regime) const {
    auto it = regime_params_.find(regime);
    if (it != regime_params_.end()) {
        return it->second;
    }

    // Fallback to CHOPPY params if regime not found
    auto fallback = regime_params_.find(MarketRegime::CHOPPY);
    if (fallback != regime_params_.end()) {
        return fallback->second;
    }

    // Ultimate fallback: default constructor
    return RegimeParams();
}

void RegimeParameterManager::set_params_for_regime(MarketRegime regime, const RegimeParams& params) {
    if (params.is_valid()) {
        regime_params_[regime] = params;
    } else {
        std::cerr << "Warning: Invalid parameters for regime "
                  << MarketRegimeDetector::regime_to_string(regime) << std::endl;
    }
}

void RegimeParameterManager::load_default_params() {
    init_trending_up_params();
    init_trending_down_params();
    init_choppy_params();
    init_high_volatility_params();
    init_low_volatility_params();
}

// Default parameters for TRENDING_UP regime
// Optimized for capturing upward momentum
void RegimeParameterManager::init_trending_up_params() {
    RegimeParams params(
        0.55,   // buy_threshold (wide gap to capture trends)
        0.43,   // sell_threshold
        0.992,  // ewrls_lambda (moderate adaptation)
        0.08,   // bb_amplification_factor
        0.15,   // h1_weight (favor longer horizons in trends)
        0.60,   // h5_weight
        0.25,   // h10_weight
        20,     // bb_period
        2.25,   // bb_std_dev
        0.30,   // bb_proximity
        0.016   // regularization
    );
    regime_params_[MarketRegime::TRENDING_UP] = params;
}

// Default parameters for TRENDING_DOWN regime
// Similar to trending up but with slightly different thresholds
void RegimeParameterManager::init_trending_down_params() {
    RegimeParams params(
        0.56,   // buy_threshold (slightly higher to avoid catching falling knives)
        0.42,   // sell_threshold (more aggressive shorts)
        0.992,  // ewrls_lambda
        0.08,   // bb_amplification_factor
        0.15,   // h1_weight
        0.60,   // h5_weight
        0.25,   // h10_weight
        20,     // bb_period
        2.25,   // bb_std_dev
        0.30,   // bb_proximity
        0.016   // regularization
    );
    regime_params_[MarketRegime::TRENDING_DOWN] = params;
}

// Default parameters for CHOPPY regime
// Narrower thresholds to avoid whipsaws
void RegimeParameterManager::init_choppy_params() {
    RegimeParams params(
        0.57,   // buy_threshold (narrower gap to reduce trades)
        0.45,   // sell_threshold
        0.995,  // ewrls_lambda (slower adaptation in noise)
        0.05,   // bb_amplification_factor (less aggressive)
        0.20,   // h1_weight (favor shorter horizons in chop)
        0.50,   // h5_weight
        0.30,   // h10_weight
        25,     // bb_period (longer period for smoothing)
        2.5,    // bb_std_dev (wider bands)
        0.35,   // bb_proximity
        0.025   // regularization (higher to reduce overfitting)
    );
    regime_params_[MarketRegime::CHOPPY] = params;
}

// Default parameters for HIGH_VOLATILITY regime
// Wider thresholds and faster adaptation
void RegimeParameterManager::init_high_volatility_params() {
    RegimeParams params(
        0.58,   // buy_threshold (wider gap for volatile moves)
        0.40,   // sell_threshold
        0.990,  // ewrls_lambda (faster adaptation)
        0.12,   // bb_amplification_factor (more aggressive)
        0.25,   // h1_weight (favor short horizons in volatility)
        0.45,   // h5_weight
        0.30,   // h10_weight
        15,     // bb_period (shorter for responsiveness)
        2.0,    // bb_std_dev
        0.25,   // bb_proximity
        0.010   // regularization (lower to be more responsive)
    );
    regime_params_[MarketRegime::HIGH_VOLATILITY] = params;
}

// Default parameters for LOW_VOLATILITY regime
// Tighter thresholds, more conservative
void RegimeParameterManager::init_low_volatility_params() {
    RegimeParams params(
        0.54,   // buy_threshold (tighter gap for small moves)
        0.46,   // sell_threshold
        0.996,  // ewrls_lambda (very slow adaptation)
        0.04,   // bb_amplification_factor (conservative)
        0.20,   // h1_weight
        0.50,   // h5_weight
        0.30,   // h10_weight
        30,     // bb_period (longer for stable regime)
        2.5,    // bb_std_dev
        0.40,   // bb_proximity
        0.030   // regularization (higher for stability)
    );
    regime_params_[MarketRegime::LOW_VOLATILITY] = params;
}

bool RegimeParameterManager::load_from_file(const std::string& config_path) {
    // TODO: Implement JSON/YAML config file loading
    // For now, just use defaults
    std::cerr << "Config file loading not yet implemented: " << config_path << std::endl;
    return false;
}

bool RegimeParameterManager::save_to_file(const std::string& config_path) const {
    // TODO: Implement JSON/YAML config file saving
    std::cerr << "Config file saving not yet implemented: " << config_path << std::endl;
    return false;
}

} // namespace sentio
