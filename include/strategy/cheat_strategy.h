// include/strategy/cheat_strategy.h
#pragma once

#include "common/types.h"
#include "strategy/signal_output.h"
#include <limits>
#include <vector>
#include <deque>

namespace sentio {
namespace strategy {

/**
 * @brief Cheat strategy that looks ahead at future prices
 * 
 * This strategy "cheats" by looking at the next bar's price to determine
 * if the price will go up or down, then generates signals with a specified
 * accuracy rate. Used for validating the walk-forward framework.
 * 
 * If this strategy shows 0% MRB in walk-forward testing, it indicates
 * a bug in the validation framework, not the strategy logic.
 */
class CheatStrategy {
public:
    struct Config {
        double target_accuracy = 0.60;      // 60% accuracy
        double signal_probability = 0.75;   // Probability value for signals
        uint32_t seed = 42;                 // Fixed seed for reproducibility
        int lookback_bars = 1;              // How many bars ahead to look
    };

private:
    Config config_;
    std::deque<Bar> bar_history_;
    mutable uint64_t rng_state_;
    mutable int signal_count_;
    
public:
    CheatStrategy() : config_(), rng_state_(static_cast<uint64_t>(config_.seed) | 1ULL), signal_count_(0) {}
    
    explicit CheatStrategy(const Config& config)
        : config_(config), rng_state_(static_cast<uint64_t>(config.seed) | 1ULL), signal_count_(0) {}
    
    /**
     * @brief Generate signal by looking at future price
     * 
     * For walk-forward testing, we process ALL data including future bars,
     * so we can "cheat" by looking at what happens next.
     */
    SignalOutput generate_signal(const Bar& bar, int bar_index, 
                                 const std::vector<Bar>& all_data) {
        SignalOutput signal;
        signal.timestamp_ms = bar.timestamp_ms;
        signal.bar_index = bar_index;
        signal.symbol = bar.symbol;
        signal.strategy_name = "CheatStrategy";
        signal.strategy_version = "1.0.0";
        
        // Store current bar
        bar_history_.push_back(bar);
        if (bar_history_.size() > 100) {
            bar_history_.pop_front();
        }
        
        // Default to neutral
        signal.signal_type = SignalType::NEUTRAL;
        signal.probability = 0.5;
        
        // Need at least next bar to cheat
        if ((size_t)bar_index + config_.lookback_bars >= all_data.size()) {
            signal.metadata["reason"] = "insufficient_future_data";
            signal_count_++;
            return signal;
        }
        
        // Look at future price
        double current_price = bar.close;
        double future_price = all_data[bar_index + config_.lookback_bars].close;
        bool price_will_rise = future_price > current_price;
        
        // Decide if this prediction should be correct or not
        bool should_be_correct = next_uniform_01() < config_.target_accuracy;
        
        if (should_be_correct) {
            // Make correct prediction
            if (price_will_rise) {
                signal.signal_type = SignalType::LONG;
                signal.probability = config_.signal_probability;
            } else {
                signal.signal_type = SignalType::SHORT;
                signal.probability = 1.0 - config_.signal_probability;
            }
        } else {
            // Make incorrect prediction (intentionally wrong 40% of the time)
            if (price_will_rise) {
                signal.signal_type = SignalType::SHORT;
                signal.probability = 1.0 - config_.signal_probability;
            } else {
                signal.signal_type = SignalType::LONG;
                signal.probability = config_.signal_probability;
            }
        }
        
        // Add metadata for debugging
        signal.metadata["current_price"] = std::to_string(current_price);
        signal.metadata["future_price"] = std::to_string(future_price);
        signal.metadata["price_change"] = std::to_string((future_price - current_price) / current_price * 100.0);
        signal.metadata["correct_prediction"] = should_be_correct ? "yes" : "no";
        signal.metadata["actual_direction"] = price_will_rise ? "up" : "down";
        signal.metadata["signal_number"] = std::to_string(signal_count_);
        
        signal_count_++;
        return signal;
    }
    
    std::string get_name() const {
        return "CheatStrategy";
    }
    
    std::string get_version() const {
        return "1.0.0";
    }
    
    void reset() {
        signal_count_ = 0;
        rng_state_ = static_cast<uint64_t>(config_.seed) | 1ULL;
        bar_history_.clear();
    }

private:
    double next_uniform_01() const {
        uint64_t x = rng_state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        rng_state_ = x;
        uint64_t y = x * 2685821657736338717ULL;
        return static_cast<double>(y >> 11) * (1.0 / 9007199254740992.0);
    }
};

} // namespace strategy
} // namespace sentio

