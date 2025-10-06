// File: include/backend/dynamic_hysteresis_manager.h
#ifndef DYNAMIC_HYSTERESIS_MANAGER_H
#define DYNAMIC_HYSTERESIS_MANAGER_H

#include <deque>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"

// Use sentio namespace types
using sentio::SignalOutput;
using sentio::MarketState;
using sentio::PositionStateMachine;

namespace backend {

class DynamicHysteresisManager {
public:
    struct HysteresisConfig {
        double base_buy_threshold = 0.55;
        double base_sell_threshold = 0.45;
        double strong_margin = 0.15;
        double confidence_threshold = 0.70;
        
        // Hysteresis parameters
        double entry_bias = 0.02;      // Harder to enter new position
        double exit_bias = 0.05;       // Harder to exit existing position
        double variance_sensitivity = 0.10;  // Adjust based on signal variance
        
        // Adaptive parameters
        int signal_history_window = 20;  // Bars to track
        double min_threshold = 0.35;     // Minimum threshold bounds
        double max_threshold = 0.65;     // Maximum threshold bounds
        
        // Advanced hysteresis
        double dual_state_entry_multiplier = 2.0;  // Extra difficulty for dual states
        double momentum_factor = 0.03;              // Trend following adjustment
        bool enable_regime_detection = true;        // Enable market regime detection
    };
    
    struct DynamicThresholds {
        double buy_threshold;
        double sell_threshold;
        double strong_buy_threshold;
        double strong_sell_threshold;
        double confidence_threshold;
        
        // Diagnostic info
        double signal_variance;
        double signal_mean;
        double signal_momentum;
        std::string regime;  // "STABLE", "VOLATILE", "TRENDING_UP", "TRENDING_DOWN"
        
        // Additional metrics
        double neutral_zone_width;
        double hysteresis_strength;
        int bars_in_position;
    };
    
    explicit DynamicHysteresisManager(const HysteresisConfig& config);
    
    // Update signal history
    void update_signal_history(const SignalOutput& signal);
    
    // Get state-dependent thresholds
    DynamicThresholds get_thresholds(
        PositionStateMachine::State current_state,
        const SignalOutput& signal,
        int bars_in_position = 0
    ) const;
    
    // Calculate signal statistics
    double calculate_signal_variance() const;
    double calculate_signal_mean() const;
    double calculate_signal_momentum() const;
    std::string determine_market_regime() const;
    
    // Reset history (for testing or new sessions)
    void reset();
    
    // Get current config
    const HysteresisConfig& get_config() const { return config_; }
    
    // Update config dynamically
    void update_config(const HysteresisConfig& new_config);
    
private:
    HysteresisConfig config_;
    mutable std::deque<SignalOutput> signal_history_;
    
    // State-dependent threshold adjustments
    double get_entry_adjustment(PositionStateMachine::State state) const;
    double get_exit_adjustment(PositionStateMachine::State state) const;
    
    // Variance-based threshold widening
    double get_variance_adjustment() const;
    
    // Momentum-based adjustment
    double get_momentum_adjustment() const;
    
    // Helper functions
    bool is_long_state(PositionStateMachine::State state) const;
    bool is_short_state(PositionStateMachine::State state) const;
    bool is_dual_state(PositionStateMachine::State state) const;
    
    // Calculate rolling statistics
    struct SignalStatistics {
        double mean;
        double variance;
        double std_dev;
        double momentum;
        double min_value;
        double max_value;
    };
    
    SignalStatistics calculate_statistics() const;
};

} // namespace backend

#endif // DYNAMIC_HYSTERESIS_MANAGER_H

