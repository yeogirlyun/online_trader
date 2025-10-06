#pragma once

#include <string>
#include <vector>

namespace sentio {

/**
 * @brief Advanced ML Signal structure for multi-task model outputs
 * 
 * This structure holds the rich output from ML models,
 * including direction probability, magnitude prediction, confidence,
 * volatility forecast, and market regime classification.
 */
struct AdvancedMLSignal {
    // Primary prediction outputs
    double direction = 0.5;    ///< Buy/sell probability (0.0 to 1.0)
    double magnitude = 0.0;    ///< Expected return magnitude
    double confidence = 0.0;   ///< Model's prediction confidence (0.0 to 1.0)
    double volatility = 0.0;   ///< Expected market volatility
    
    // Market regime classification (probabilities for each regime)
    std::vector<double> regime; ///< Market regime probabilities [bull_low_vol, bull_high_vol, bear_low_vol, bear_high_vol]
    
    // Metadata
    std::string model_version = "ml_v1";
    long long timestamp_ms = 0;
    
    /**
     * @brief Default constructor - creates neutral signal
     */
    AdvancedMLSignal() {
        regime.resize(4, 0.25); // Equal probability for all regimes initially
    }
    
    /**
     * @brief Check if the signal is valid
     * @return true if all values are within expected ranges
     */
    bool is_valid() const {
        return (direction >= 0.0 && direction <= 1.0) &&
               (confidence >= 0.0 && confidence <= 1.0) &&
               (volatility >= 0.0) &&
               (regime.size() == 4);
    }
    
    /**
     * @brief Get the dominant market regime
     * @return Index of the regime with highest probability (0-3)
     */
    int get_dominant_regime() const {
        if (regime.empty()) return 0;
        
        int max_idx = 0;
        double max_prob = regime[0];
        for (size_t i = 1; i < regime.size(); ++i) {
            if (regime[i] > max_prob) {
                max_prob = regime[i];
                max_idx = static_cast<int>(i);
            }
        }
        return max_idx;
    }
    
    /**
     * @brief Get regime name as string
     * @return Human-readable regime name
     */
    std::string get_regime_name() const {
        const std::vector<std::string> regime_names = {
            "Bull Low Vol", "Bull High Vol", "Bear Low Vol", "Bear High Vol"
        };
        
        int dominant = get_dominant_regime();
        if (dominant >= 0 && dominant < static_cast<int>(regime_names.size())) {
            return regime_names[dominant];
        }
        return "Unknown";
    }
    
    /**
     * @brief Convert to simple probability for backward compatibility
     * @return Direction probability adjusted by confidence
     */
    double to_simple_probability() const {
        // Weight the direction by confidence for more reliable signals
        double base_prob = direction;
        double confidence_weight = std::max(0.1, confidence); // Minimum 10% confidence
        
        // Adjust probability based on confidence
        if (base_prob > 0.5) {
            return 0.5 + (base_prob - 0.5) * confidence_weight;
        } else {
            return 0.5 - (0.5 - base_prob) * confidence_weight;
        }
    }
};

/**
 * @brief Market regime enumeration for easier handling
 */
enum class MarketRegime {
    BULL_LOW_VOL = 0,
    BULL_HIGH_VOL = 1,
    BEAR_LOW_VOL = 2,
    BEAR_HIGH_VOL = 3
};

/**
 * @brief Convert MarketRegime enum to string
 */
inline std::string regime_to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::BULL_LOW_VOL: return "Bull Low Vol";
        case MarketRegime::BULL_HIGH_VOL: return "Bull High Vol";
        case MarketRegime::BEAR_LOW_VOL: return "Bear Low Vol";
        case MarketRegime::BEAR_HIGH_VOL: return "Bear High Vol";
        default: return "Unknown";
    }
}

} // namespace sentio
