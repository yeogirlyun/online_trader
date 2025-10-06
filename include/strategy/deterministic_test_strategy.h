// include/strategy/deterministic_test_strategy.h
#pragma once

#include "common/types.h"
#include "strategy/signal_output.h"
#include <cmath>
#include <limits>
#include <ostream>
#include <istream>

namespace sentio {
namespace strategy {

/**
 * @brief Deterministic test strategy with known, verifiable outcomes
 * 
 * Generates signals with mathematically predictable accuracy for
 * comprehensive validation of trading system calculations.
 */
class DeterministicTestStrategy {
public:
    enum Mode {
        PERFECT_PREDICTION,    // 100% accuracy for validation
        KNOWN_ACCURACY,        // Specific accuracy (e.g., 60%)
        ALTERNATING,           // LONG/SHORT/LONG/SHORT pattern
        THRESHOLD_BASED,       // Signal when price crosses thresholds
        ALL_NEUTRAL            // All signals neutral (edge case test)
    };

    struct Config {
        Mode mode = PERFECT_PREDICTION;
        double target_accuracy = 0.6;     // For KNOWN_ACCURACY mode
        double long_threshold = 102.0;    // For THRESHOLD_BASED
        double short_threshold = 98.0;
        double signal_probability = 0.75; // Consistent probability
        uint32_t seed = 42;               // Fixed seed for reproducibility
    };

private:
    Config config_;
    mutable int signal_count_;
    mutable uint64_t rng_state_;
    
public:
    DeterministicTestStrategy() : config_(), signal_count_(0), rng_state_(static_cast<uint64_t>(config_.seed) | 1ULL) {}
    
    explicit DeterministicTestStrategy(const Config& config)
        : config_(config), signal_count_(0), rng_state_(static_cast<uint64_t>(config.seed) | 1ULL) {}
    
    SignalOutput generate_signal(const Bar& bar, int bar_index) {
        SignalOutput signal;
        signal.timestamp_ms = bar.timestamp_ms;
        signal.bar_index = bar_index;
        signal.symbol = bar.symbol;
        signal.strategy_name = "DeterministicTest";
        signal.strategy_version = "1.0.0";
        
        double current_price = bar.close;
        
        switch (config_.mode) {
            case PERFECT_PREDICTION:
                generate_perfect_signal(signal, current_price, bar_index);
                break;
                
            case KNOWN_ACCURACY:
                generate_known_accuracy_signal(signal, current_price, bar_index);
                break;
                
            case ALTERNATING:
                generate_alternating_signal(signal);
                break;
                
            case THRESHOLD_BASED:
                generate_threshold_signal(signal, current_price);
                break;
                
            case ALL_NEUTRAL:
                signal.signal_type = SignalType::NEUTRAL;
                signal.probability = 0.5;
                break;
        }
        
        // Add deterministic metadata
        signal.metadata["signal_number"] = std::to_string(signal_count_);
        signal.metadata["mode"] = get_mode_string(config_.mode);
        signal.metadata["current_price"] = std::to_string(current_price);
        signal.metadata["bar_index"] = std::to_string(bar_index);
        
        signal_count_++;
        return signal;
    }
    
    std::string get_name() const {
        return "DeterministicTest";
    }
    
    std::string get_version() const {
        return "1.0.0";
    }
    
    void reset() {
        signal_count_ = 0;
        rng_state_ = static_cast<uint64_t>(config_.seed) | 1ULL;
    }

private:
    void generate_perfect_signal(SignalOutput& signal, double current_price, int bar_index) const {
        // Use sine wave pattern knowledge for perfect prediction
        // Assuming period=20 sine wave (default in synthetic generator)
        // Formula: price = base + amplitude * sin(2*pi * i / period)
        // Derivative: amplitude * (2*pi/period) * cos(2*pi * i / period)
        // Price rises when cos > 0, falls when cos < 0
        
        int bar_in_cycle = bar_index % 20;
        
        // cos(2*pi * i / 20) is positive from i=0 to i=5 and i=15 to i=20
        // cos is negative from i=5 to i=15
        // This means price rises: 0-5 and 15-20, falls: 5-15
        bool price_will_rise = (bar_in_cycle < 5) || (bar_in_cycle >= 15);
        
        if (price_will_rise) {
            signal.signal_type = SignalType::LONG;
            signal.probability = config_.signal_probability;
        } else {
            signal.signal_type = SignalType::SHORT;
            signal.probability = 1.0 - config_.signal_probability;
        }
    }
    
    void generate_known_accuracy_signal(SignalOutput& signal, double current_price, int bar_index) const {
        // Generate signals with specific accuracy using pattern knowledge
        bool should_be_correct = next_uniform_01() < config_.target_accuracy;
        
        // Use sine wave pattern knowledge (same as perfect prediction)
        int bar_in_cycle = bar_index % 20;
        bool price_will_rise = (bar_in_cycle < 5) || (bar_in_cycle >= 15);
        
        if (should_be_correct) {
            // Correct prediction
            signal.signal_type = price_will_rise ? SignalType::LONG : SignalType::SHORT;
            signal.probability = price_will_rise ? config_.signal_probability : (1.0 - config_.signal_probability);
        } else {
            // Incorrect prediction
            signal.signal_type = price_will_rise ? SignalType::SHORT : SignalType::LONG;
            signal.probability = price_will_rise ? (1.0 - config_.signal_probability) : config_.signal_probability;
        }
    }
    
    void generate_alternating_signal(SignalOutput& signal) const {
        // Simple alternating pattern for testing
        if (signal_count_ % 2 == 0) {
            signal.signal_type = SignalType::LONG;
            signal.probability = config_.signal_probability;
        } else {
            signal.signal_type = SignalType::SHORT;
            signal.probability = 1.0 - config_.signal_probability;
        }
    }
    
    void generate_threshold_signal(SignalOutput& signal, double current_price) const {
        // Signal based on price thresholds (mean reversion)
        if (current_price > config_.long_threshold) {
            // Overbought - expect reversion down
            signal.signal_type = SignalType::SHORT;
            signal.probability = 0.2;
        } else if (current_price < config_.short_threshold) {
            // Oversold - expect reversion up
            signal.signal_type = SignalType::LONG;
            signal.probability = 0.8;
        } else {
            signal.signal_type = SignalType::NEUTRAL;
            signal.probability = 0.5;
        }
    }
    
    std::string get_mode_string(Mode mode) const {
        switch (mode) {
            case PERFECT_PREDICTION: return "perfect";
            case KNOWN_ACCURACY: return "known_accuracy";
            case ALTERNATING: return "alternating";
            case THRESHOLD_BASED: return "threshold";
            case ALL_NEUTRAL: return "all_neutral";
            default: return "unknown";
        }
    }

    // Simple xorshift-based PRNG producing [0,1) doubles with 53-bit precision
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

