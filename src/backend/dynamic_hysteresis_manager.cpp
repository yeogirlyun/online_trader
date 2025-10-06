// File: src/backend/dynamic_hysteresis_manager.cpp
#include "backend/dynamic_hysteresis_manager.h"
#include "common/utils.h"
#include <numeric>
#include <stdexcept>

namespace backend {

DynamicHysteresisManager::DynamicHysteresisManager(const HysteresisConfig& config)
    : config_(config) {
    signal_history_.clear();
}

void DynamicHysteresisManager::update_signal_history(const SignalOutput& signal) {
    signal_history_.push_back(signal);
    
    // Maintain window size
    while (signal_history_.size() > static_cast<size_t>(config_.signal_history_window)) {
        signal_history_.pop_front();
    }
}

DynamicHysteresisManager::DynamicThresholds DynamicHysteresisManager::get_thresholds(
    PositionStateMachine::State current_state,
    const SignalOutput& signal,
    int bars_in_position) const {
    
    DynamicThresholds thresholds;
    
    // Base thresholds
    double base_buy = config_.base_buy_threshold;
    double base_sell = config_.base_sell_threshold;
    
    // Initialize with base values
    thresholds.buy_threshold = base_buy;
    thresholds.sell_threshold = base_sell;
    
    // State-dependent adjustments (HYSTERESIS)
    switch (current_state) {
        case PositionStateMachine::State::CASH_ONLY:
            // Normal entry thresholds - no adjustment needed
            break;
            
        case PositionStateMachine::State::QQQ_ONLY:
        case PositionStateMachine::State::TQQQ_ONLY:
            // In long position - harder to add more, harder to exit
            thresholds.buy_threshold = base_buy + config_.entry_bias;  // 0.55 → 0.57
            thresholds.sell_threshold = base_sell - config_.exit_bias; // 0.45 → 0.40
            break;
            
        case PositionStateMachine::State::PSQ_ONLY:
        case PositionStateMachine::State::SQQQ_ONLY:
            // In short position - harder to add more, harder to exit
            thresholds.sell_threshold = base_sell - config_.entry_bias; // 0.45 → 0.43
            thresholds.buy_threshold = base_buy + config_.exit_bias;    // 0.55 → 0.60
            break;
            
        case PositionStateMachine::State::QQQ_TQQQ:
            // Already leveraged long - VERY HARD to add more
            thresholds.buy_threshold = base_buy + config_.dual_state_entry_multiplier * config_.entry_bias;  // 0.55 → 0.59
            thresholds.sell_threshold = base_sell - config_.exit_bias;     // 0.45 → 0.40
            break;
            
        case PositionStateMachine::State::PSQ_SQQQ:
            // Already leveraged short - VERY HARD to add more
            thresholds.sell_threshold = base_sell - config_.dual_state_entry_multiplier * config_.entry_bias; // 0.45 → 0.41
            thresholds.buy_threshold = base_buy + config_.exit_bias;        // 0.55 → 0.60
            break;
            
        default:
            // INVALID state or unknown - use base thresholds
            break;
    }
    
    // Time-in-position adjustment (longer in position = harder to exit)
    if (bars_in_position > 5 && bars_in_position < 50) {
        double time_factor = std::min(0.02, bars_in_position * 0.001);
        if (is_long_state(current_state)) {
            thresholds.sell_threshold -= time_factor;  // Even harder to exit
        } else if (is_short_state(current_state)) {
            thresholds.buy_threshold += time_factor;   // Even harder to exit
        }
    }
    
    // Variance-based adjustment (widen neutral zone if volatile)
    double variance_adj = get_variance_adjustment();
    thresholds.buy_threshold += variance_adj;
    thresholds.sell_threshold -= variance_adj;
    
    // Momentum-based adjustment (follow the trend)
    if (config_.momentum_factor > 0) {
        double momentum_adj = get_momentum_adjustment();
        thresholds.buy_threshold += momentum_adj;
        thresholds.sell_threshold += momentum_adj;
    }
    
    // Clamp to bounds
    thresholds.buy_threshold = std::clamp(thresholds.buy_threshold, 
                                         config_.min_threshold, config_.max_threshold);
    thresholds.sell_threshold = std::clamp(thresholds.sell_threshold,
                                          config_.min_threshold, config_.max_threshold);
    
    // Ensure buy > sell (maintain neutral zone)
    if (thresholds.buy_threshold <= thresholds.sell_threshold) {
        double mid = (thresholds.buy_threshold + thresholds.sell_threshold) / 2.0;
        thresholds.buy_threshold = mid + 0.05;
        thresholds.sell_threshold = mid - 0.05;
    }
    
    // Strong signal thresholds
    thresholds.strong_buy_threshold = thresholds.buy_threshold + config_.strong_margin;
    thresholds.strong_sell_threshold = thresholds.sell_threshold - config_.strong_margin;
    
    // Confidence threshold (could also be adaptive based on regime)
    thresholds.confidence_threshold = config_.confidence_threshold;
    std::string regime = determine_market_regime();
    if (regime == "VOLATILE") {
        thresholds.confidence_threshold = std::min(0.85, config_.confidence_threshold + 0.10);
    }
    
    // Calculate diagnostic info
    auto stats = calculate_statistics();
    thresholds.signal_variance = stats.variance;
    thresholds.signal_mean = stats.mean;
    thresholds.signal_momentum = stats.momentum;
    thresholds.regime = regime;
    thresholds.neutral_zone_width = thresholds.buy_threshold - thresholds.sell_threshold;
    thresholds.hysteresis_strength = std::abs(base_buy - thresholds.buy_threshold) + 
                                     std::abs(base_sell - thresholds.sell_threshold);
    thresholds.bars_in_position = bars_in_position;
    
    // Log threshold adjustments
    sentio::utils::log_debug("DYNAMIC THRESHOLDS: state=" + std::to_string(static_cast<int>(current_state)) +
                    ", buy=" + std::to_string(thresholds.buy_threshold) +
                    ", sell=" + std::to_string(thresholds.sell_threshold) +
                    ", variance=" + std::to_string(thresholds.signal_variance) +
                    ", momentum=" + std::to_string(thresholds.signal_momentum) +
                    ", regime=" + thresholds.regime);
    
    return thresholds;
}

double DynamicHysteresisManager::calculate_signal_variance() const {
    auto stats = calculate_statistics();
    return stats.variance;
}

double DynamicHysteresisManager::calculate_signal_mean() const {
    auto stats = calculate_statistics();
    return stats.mean;
}

double DynamicHysteresisManager::calculate_signal_momentum() const {
    auto stats = calculate_statistics();
    return stats.momentum;
}

std::string DynamicHysteresisManager::determine_market_regime() const {
    if (signal_history_.size() < 5) return "UNKNOWN";
    
    auto stats = calculate_statistics();
    
    // Determine regime based on variance and momentum
    if (stats.variance > 0.01) {
        return "VOLATILE";
    } else if (stats.momentum > 0.02) {
        return "TRENDING_UP";
    } else if (stats.momentum < -0.02) {
        return "TRENDING_DOWN";
    } else {
        return "STABLE";
    }
}

void DynamicHysteresisManager::reset() {
    signal_history_.clear();
}

void DynamicHysteresisManager::update_config(const HysteresisConfig& new_config) {
    config_ = new_config;
}

double DynamicHysteresisManager::get_entry_adjustment(PositionStateMachine::State state) const {
    // Make it harder to enter positions when already positioned
    if (state == PositionStateMachine::State::CASH_ONLY) {
        return 0.0;  // No adjustment for cash
    } else if (is_dual_state(state)) {
        return config_.entry_bias * config_.dual_state_entry_multiplier;  // Very hard to add
    } else {
        return config_.entry_bias;  // Moderately harder to add
    }
}

double DynamicHysteresisManager::get_exit_adjustment(PositionStateMachine::State state) const {
    // Make it harder to exit positions (prevent whipsaw)
    if (state == PositionStateMachine::State::CASH_ONLY) {
        return 0.0;  // No adjustment for cash
    } else {
        return config_.exit_bias;  // Harder to exit any position
    }
}

double DynamicHysteresisManager::get_variance_adjustment() const {
    if (signal_history_.size() < 10) return 0.0;
    
    double variance = calculate_signal_variance();
    
    // High variance → wider neutral zone (reduce trades)
    // Low variance → narrower neutral zone (allow more trades)
    // Cap adjustment to prevent extreme widening
    return std::min(0.10, variance * config_.variance_sensitivity);
}

double DynamicHysteresisManager::get_momentum_adjustment() const {
    if (signal_history_.size() < 10) return 0.0;
    
    double momentum = calculate_signal_momentum();
    
    // Follow the trend: if trending up, make it easier to go long
    // If trending down, make it easier to go short
    return momentum * config_.momentum_factor;
}

bool DynamicHysteresisManager::is_long_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::QQQ_ONLY ||
           state == PositionStateMachine::State::TQQQ_ONLY ||
           state == PositionStateMachine::State::QQQ_TQQQ;
}

bool DynamicHysteresisManager::is_short_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::PSQ_ONLY ||
           state == PositionStateMachine::State::SQQQ_ONLY ||
           state == PositionStateMachine::State::PSQ_SQQQ;
}

bool DynamicHysteresisManager::is_dual_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::QQQ_TQQQ ||
           state == PositionStateMachine::State::PSQ_SQQQ;
}

DynamicHysteresisManager::SignalStatistics DynamicHysteresisManager::calculate_statistics() const {
    SignalStatistics stats = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    
    if (signal_history_.empty()) {
        return stats;
    }
    
    // Calculate mean
    double sum = 0.0;
    for (const auto& signal : signal_history_) {
        sum += signal.probability;
        stats.min_value = std::min(stats.min_value, signal.probability);
        stats.max_value = std::max(stats.max_value, signal.probability);
    }
    stats.mean = sum / signal_history_.size();
    
    // Calculate variance and standard deviation
    if (signal_history_.size() > 1) {
        double sum_squared_diff = 0.0;
        for (const auto& signal : signal_history_) {
            double diff = signal.probability - stats.mean;
            sum_squared_diff += diff * diff;
        }
        stats.variance = sum_squared_diff / signal_history_.size();
        stats.std_dev = std::sqrt(stats.variance);
    }
    
    // Calculate momentum (trend)
    if (signal_history_.size() >= 5) {
        // Simple linear regression slope
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        int n = signal_history_.size();
        
        for (int i = 0; i < n; ++i) {
            double x = i;
            double y = signal_history_[i].probability;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }
        
        double denominator = n * sum_x2 - sum_x * sum_x;
        if (std::abs(denominator) > 0.0001) {
            stats.momentum = (n * sum_xy - sum_x * sum_y) / denominator;
        }
    }
    
    return stats;
}

} // namespace backend

