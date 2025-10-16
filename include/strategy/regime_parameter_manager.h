#pragma once

#include "strategy/market_regime_detector.h"
#include <map>
#include <string>

namespace sentio {

// Forward declaration to avoid circular dependency
class OnlineEnsembleStrategy;

// Parameter set for a specific market regime
struct RegimeParams {
    // Primary strategy parameters
    double buy_threshold;
    double sell_threshold;
    double ewrls_lambda;
    double bb_amplification_factor;

    // Secondary parameters
    double h1_weight;
    double h5_weight;
    double h10_weight;
    double bb_period;
    double bb_std_dev;
    double bb_proximity;
    double regularization;

    RegimeParams()
        : buy_threshold(0.53),
          sell_threshold(0.48),
          ewrls_lambda(0.992),
          bb_amplification_factor(0.05),
          h1_weight(0.20),
          h5_weight(0.50),
          h10_weight(0.30),
          bb_period(20),
          bb_std_dev(2.0),
          bb_proximity(0.30),
          regularization(0.01) {}

    RegimeParams(double buy, double sell, double lambda, double bb_amp,
                 double h1, double h5, double h10,
                 double bb_per, double bb_std, double bb_prox, double reg)
        : buy_threshold(buy),
          sell_threshold(sell),
          ewrls_lambda(lambda),
          bb_amplification_factor(bb_amp),
          h1_weight(h1),
          h5_weight(h5),
          h10_weight(h10),
          bb_period(bb_per),
          bb_std_dev(bb_std),
          bb_proximity(bb_prox),
          regularization(reg) {}

    // Validate parameters
    bool is_valid() const {
        if (buy_threshold <= sell_threshold) return false;
        if (buy_threshold < 0.5 || buy_threshold > 0.7) return false;
        if (sell_threshold < 0.3 || sell_threshold > 0.5) return false;
        if (ewrls_lambda < 0.98 || ewrls_lambda > 1.0) return false;
        if (bb_amplification_factor < 0.0 || bb_amplification_factor > 0.3) return false;

        double weight_sum = h1_weight + h5_weight + h10_weight;
        if (std::abs(weight_sum - 1.0) > 0.01) return false;

        return true;
    }
};

// Manage regime-specific parameters
class RegimeParameterManager {
public:
    RegimeParameterManager();

    // Get parameters for a specific regime
    RegimeParams get_params_for_regime(MarketRegime regime) const;

    // Set parameters for a specific regime
    void set_params_for_regime(MarketRegime regime, const RegimeParams& params);

    // Load default parameter sets (optimized for each regime)
    void load_default_params();

    // Load from config file
    bool load_from_file(const std::string& config_path);

    // Save to config file
    bool save_to_file(const std::string& config_path) const;

private:
    std::map<MarketRegime, RegimeParams> regime_params_;

    // Default parameter sets for each regime (from Optuna optimization)
    void init_trending_up_params();
    void init_trending_down_params();
    void init_choppy_params();
    void init_high_volatility_params();
    void init_low_volatility_params();
};

} // namespace sentio
