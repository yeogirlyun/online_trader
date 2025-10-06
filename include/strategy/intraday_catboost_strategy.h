#pragma once

#include "strategy/strategy_component.h"
#include "strategy/intraday_ml_strategies.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <string>

namespace sentio {
namespace intraday {

/**
 * @brief CatBoost strategy using 25 advanced intraday features
 * 
 * This strategy uses the REAL CatBoost model exported to C++ code.
 * No approximations - actual tree traversal logic!
 */
class IntradayCatBoostStrategy : public StrategyComponent {
public:
    struct Config {
        std::string model_path = "artifacts/CatBoost/catboost_model.cpp";
        double confidence_threshold = 0.05;
        double position_size_multiplier = 1.0;
        bool enable_risk_management = true;
        double max_drawdown_percent = 5.0;
        double stop_loss_percent = 0.5;
        double take_profit_percent = 1.0;
        
        // Hysteresis parameters to reduce whipsaw movements
        bool enable_hysteresis = true;   // Re-enabled with simplified logic
        double hysteresis_margin = 0.05;        // Required probability margin to switch signal (reduced)
        int hysteresis_cooldown_bars = 2;       // Bars to wait after a signal change (reduced)
        double probability_hysteresis = 0.05;   // Extra margin for probability changes (reduced)
    };
    
    IntradayCatBoostStrategy(const StrategyConfig& base_config,
                            const Config& cb_config,
                            const std::string& symbol);
    
    ~IntradayCatBoostStrategy() override;
    
    bool initialize();
    
    // Public interface for adapter
    SignalOutput process_bar(const Bar& bar, int bar_index) {
        update_indicators(bar);
        return generate_signal(bar, bar_index);
    }
    
    // Reset strategy state (for walk-forward validation)
    void reset() {
        risk_ = RiskManager{};
        hysteresis_state_ = HysteresisState{};
        if (feature_extractor_) {
            feature_extractor_->reset();
        }
    }

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;

private:
    Config config_;
    std::string symbol_;
    std::unique_ptr<IntradayFeatureExtractor> feature_extractor_;
    bool initialized_ = false;
    
    // Risk management
    struct RiskManager {
        double current_drawdown = 0.0;
        double peak_equity = 0.0;
        double current_position = 0.0;
        int consecutive_losses = 0;
        
        bool should_trade() const {
            return current_drawdown < 5.0 && consecutive_losses < 3;
        }
        
        void update_equity(double equity) {
            peak_equity = std::max(peak_equity, equity);
            current_drawdown = (peak_equity - equity) / peak_equity * 100.0;
        }
    } risk_;
    
    // Hysteresis state tracking
    struct HysteresisState {
        double last_probability = 0.5;          // Last signal probability
        double last_confidence = 0.0;           // Last signal confidence
        int cooldown_bars_remaining = 0;        // Bars remaining in cooldown
        int bars_since_last_change = 0;         // Bars since last signal change
        bool last_was_long = false;             // Last signal type (for directional hysteresis)
        bool last_was_short = false;
        double probability_threshold_long = 0.65;   // Dynamic threshold for LONG signals
        double probability_threshold_short = 0.35;  // Dynamic threshold for SHORT signals
        
        void reset() {
            last_probability = 0.5;
            last_confidence = 0.0;
            cooldown_bars_remaining = 0;
            bars_since_last_change = 0;
            last_was_long = false;
            last_was_short = false;
            probability_threshold_long = 0.65;
            probability_threshold_short = 0.35;
        }
        
        void update_thresholds() {
            // Gradually return thresholds to neutral as time passes
            double decay_factor = 0.95;
            probability_threshold_long = 0.5 + (probability_threshold_long - 0.5) * decay_factor;
            probability_threshold_short = 0.5 - (0.5 - probability_threshold_short) * decay_factor;
        }
    } hysteresis_state_;
    
    double run_catboost_model(const IntradayFeatureExtractor::Features& features);
    SignalOutput create_signal(double probability, const Bar& bar, int bar_index);
    
    // Hysteresis methods
    double apply_hysteresis(double raw_probability, double confidence);
    bool should_change_signal(double new_probability, double new_confidence);
};

} // namespace intraday
} // namespace sentio

