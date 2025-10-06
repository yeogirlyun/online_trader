#pragma once

#include <cmath>

namespace sentio {
namespace signal {

// Calculate confidence from probability using tanh scaling
inline double calculate_confidence(double probability, double k = 3.0) {
    // Confidence = tanh(k * |p - 0.5|)
    // Maps probability distance from 0.5 to [0, 1] with smooth scaling
    return std::tanh(k * std::abs(probability - 0.5));
}

// Apply temperature calibration to probability
inline double temperature_calibrate(double probability, double temperature = 1.0) {
    if (temperature <= 0.0 || temperature == 1.0) {
        return probability;
    }
    
    // Convert probability to logit
    double eps = 1e-7;
    probability = std::max(eps, std::min(1.0 - eps, probability));
    double logit = std::log(probability / (1.0 - probability));
    
    // Apply temperature scaling
    double scaled_logit = logit / temperature;
    
    // Convert back to probability
    return 1.0 / (1.0 + std::exp(-scaled_logit));
}

} // namespace signal
} // namespace sentio
