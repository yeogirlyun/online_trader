// williams_rsi_bb_strategy.h
// Williams/RSI crossover with Bollinger Bands gating

#pragma once

#include "strategy/istrategy.h"
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace sentio {

struct WilliamsRSIBBConfig {
    // Indicator parameters
    int williams_period = 10;
    int rsi_period = 20;
    int bb_period = 20;
    double bb_std_dev = 2.0;
    
    // Trading gates
    double bb_proximity_pct = 0.40;  // Within 40% of BB bands (more permissive)
    double cross_margin = 0.0;  // No margin required for testing
    
    // Signal generation parameters
    double base_confidence = 0.6;
    
    // Confirmation parameters
    bool require_volume_confirmation = true;
    double volume_threshold = 1.2;
    
    // Sentio integration
    double min_confidence_threshold = 0.55;
    double probability_threshold = 0.05;
    
    // Debug mode
    bool debug_mode = false;
    
    // Static factory methods
    static WilliamsRSIBBConfig from_file(const std::string& path);
    static WilliamsRSIBBConfig defaults() {
        return WilliamsRSIBBConfig();
    }
};

class WilliamsRSIBBStrategy {
public:
    explicit WilliamsRSIBBStrategy(const WilliamsRSIBBConfig& config = WilliamsRSIBBConfig())
        : config_(config) {
        reset();
        
        if (config_.debug_mode) {
            std::cout << "[WRB] Strategy initialized with:" << std::endl;
            std::cout << "  Williams period: " << config_.williams_period << std::endl;
            std::cout << "  RSI period: " << config_.rsi_period << std::endl;
            std::cout << "  BB period: " << config_.bb_period << std::endl;
            std::cout << "  BB proximity: " << config_.bb_proximity_pct * 100 << "%" << std::endl;
        }
    }
    
    SignalOutput generate_signal(const Bar& bar, int bar_index) {
        // Update indicators with new bar
        update_indicators(bar);
        
        // Initialize output
        SignalOutput signal;
        signal.timestamp_ms = bar.timestamp_ms;
        signal.bar_index = bar_index;
        signal.symbol = bar.symbol;
        signal.strategy_name = "WilliamsRSIBB";
        signal.strategy_version = "3.0";
        
        // Need minimum bars for calculation
        size_t min_bars = std::max({
            (size_t)config_.williams_period,
            (size_t)config_.rsi_period,
            (size_t)config_.bb_period
        });
        
        if (price_history_.size() < min_bars + 2) {
            signal.probability = 0.5;
            // confidence removed
            signal.signal_type = SignalType::NEUTRAL;
            return signal;
        }
        
        // Calculate current indicator values
        double williams_norm = calculate_williams_normalized();
        double rsi = calculate_rsi();
        BollingerBands bb = calculate_bollinger_bands();
        
        // Store current values into history BEFORE cross detection
        if (williams_history_.size() >= 100) williams_history_.pop_front();
        if (rsi_history_.size() >= 100) rsi_history_.pop_front();
        williams_history_.push_back(williams_norm);
        rsi_history_.push_back(rsi);
        
        // Detect crosses using history
        CrossState cross = detect_cross();
        
        // Calculate probability and confidence
        ProbabilityConfidence pc = calculate_probability_confidence(
            williams_norm, rsi, bb, cross, bar
        );
        
        signal.probability = pc.probability;
        // confidence removed
        
        // Use config parameters for thresholds
        double prob_long_threshold = 0.5 + config_.probability_threshold;
        double prob_short_threshold = 0.5 - config_.probability_threshold;
        
        double signal_strength = std::abs(pc.probability - 0.5) * 2.0;
        if (pc.probability > prob_long_threshold && 
            signal_strength >= config_.min_confidence_threshold) {
            signal.signal_type = SignalType::LONG;
        } else if (pc.probability < prob_short_threshold && 
                   signal_strength >= config_.min_confidence_threshold) {
            signal.signal_type = SignalType::SHORT;
        } else {
            signal.signal_type = SignalType::NEUTRAL;
        }
        
        // Debug output
        if (config_.debug_mode && bar_index % 100 == 0) {
            std::cout << "[WRB] Bar " << bar_index << ":"
                      << " W=" << williams_norm 
                      << " R=" << rsi
                      << " BB=[" << bb.lower << "," << bb.middle << "," << bb.upper << "]"
                      << " Price=" << bar.close
                      << " Cross=" << (cross == CrossState::NO_CROSS ? "NO" : 
                                      cross == CrossState::CROSSED_UP ? "UP" : "DOWN")
                      << " P=" << pc.probability
                      << " C=" << pc.confidence
                      << " Signal=" << (signal.signal_type == SignalType::NEUTRAL ? "NEUTRAL" :
                                       signal.signal_type == SignalType::LONG ? "LONG" : "SHORT")
                      << std::endl;
        }
        
        // Add metadata
        signal.metadata["williams_norm"] = std::to_string(williams_norm);
        signal.metadata["rsi"] = std::to_string(rsi);
        signal.metadata["bb_upper"] = std::to_string(bb.upper);
        signal.metadata["bb_middle"] = std::to_string(bb.middle);
        signal.metadata["bb_lower"] = std::to_string(bb.lower);
        signal.metadata["bb_position"] = std::to_string(bb.position_pct);
        signal.metadata["cross_type"] = cross_type_to_string(cross);
        signal.metadata["cross_strength"] = std::to_string(cross_strength_);
        signal.metadata["bb_aligned"] = bb_aligned_ ? "true" : "false";
        
        return signal;
    }
    
    void reset() {
        price_history_.clear();
        high_history_.clear();
        low_history_.clear();
        volume_history_.clear();
        
        williams_history_.clear();
        rsi_history_.clear();
        
        rsi_avg_gain_ = 0.0;
        rsi_avg_loss_ = 0.0;
        
        cross_strength_ = 0.0;
        bb_aligned_ = false;
    }
    
    std::string get_name() const {
        return "WilliamsRSIBB";
    }
    
    std::string get_version() const {
        return "3.0";
    }

private:
    enum class CrossState {
        NO_CROSS,
        CROSSED_UP,
        CROSSED_DOWN
    };
    
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // Where price is relative to bands (0=lower, 1=upper)
    };
    
    struct ProbabilityConfidence {
        double probability;
        double confidence;
    };
    
    // Update all indicators with new bar
    void update_indicators(const Bar& bar) {
        price_history_.push_back(bar.close);
        high_history_.push_back(bar.high);
        low_history_.push_back(bar.low);
        volume_history_.push_back(bar.volume);
        
        // Maintain reasonable window sizes
        size_t max_lookback = std::max({
            (size_t)config_.williams_period,
            (size_t)config_.rsi_period,
            (size_t)config_.bb_period
        }) * 3;
        
        if (price_history_.size() > max_lookback) {
            price_history_.pop_front();
            high_history_.pop_front();
            low_history_.pop_front();
            volume_history_.pop_front();
        }
    }
    
    // Williams %R calculation (0-100 normalized)
    double calculate_williams_normalized() {
        if (high_history_.size() < config_.williams_period) {
            return 50.0;
        }
        
        size_t start_idx = high_history_.size() - config_.williams_period;
        
        double hh = *std::max_element(
            high_history_.begin() + start_idx,
            high_history_.end()
        );
        
        double ll = *std::min_element(
            low_history_.begin() + start_idx,
            low_history_.end()
        );
        
        double range = hh - ll;
        if (range < 0.0001) return 50.0;
        
        double williams_r = -100.0 * (hh - price_history_.back()) / range;
        double normalized = 100.0 + williams_r;
        
        return std::max(0.0, std::min(100.0, normalized));
    }
    
    // RSI calculation with Wilder's smoothing
    double calculate_rsi() {
        if (price_history_.size() < config_.rsi_period + 1) {
            return 50.0;
        }
        
        if (rsi_avg_gain_ == 0.0 && rsi_avg_loss_ == 0.0) {
            double sum_gain = 0.0, sum_loss = 0.0;
            
            size_t start = price_history_.size() - config_.rsi_period - 1;
            for (size_t i = start + 1; i <= start + config_.rsi_period; i++) {
                double change = price_history_[i] - price_history_[i-1];
                if (change > 0) {
                    sum_gain += change;
                } else {
                    sum_loss += -change;
                }
            }
            
            rsi_avg_gain_ = sum_gain / config_.rsi_period;
            rsi_avg_loss_ = sum_loss / config_.rsi_period;
        } else {
            size_t idx = price_history_.size() - 1;
            double change = price_history_[idx] - price_history_[idx-1];
            
            double gain = change > 0 ? change : 0;
            double loss = change < 0 ? -change : 0;
            
            rsi_avg_gain_ = (rsi_avg_gain_ * (config_.rsi_period - 1) + gain) / config_.rsi_period;
            rsi_avg_loss_ = (rsi_avg_loss_ * (config_.rsi_period - 1) + loss) / config_.rsi_period;
        }
        
        if (rsi_avg_loss_ < 0.0001) {
            return 100.0;
        }
        
        double rs = rsi_avg_gain_ / rsi_avg_loss_;
        double rsi = 100.0 - (100.0 / (1.0 + rs));
        
        return std::max(0.0, std::min(100.0, rsi));
    }
    
    // Bollinger Bands calculation
    BollingerBands calculate_bollinger_bands() {
        BollingerBands bb;
        bb.upper = 0.0;
        bb.middle = 0.0;
        bb.lower = 0.0;
        bb.bandwidth = 0.0;
        bb.position_pct = 0.5;
        
        if (price_history_.size() < config_.bb_period) {
            return bb;
        }
        
        // Calculate SMA (middle band)
        size_t start = price_history_.size() - config_.bb_period;
        double sum = 0.0;
        for (size_t i = start; i < price_history_.size(); i++) {
            sum += price_history_[i];
        }
        bb.middle = sum / config_.bb_period;
        
        // Calculate standard deviation
        double variance = 0.0;
        for (size_t i = start; i < price_history_.size(); i++) {
            double diff = price_history_[i] - bb.middle;
            variance += diff * diff;
        }
        double std_dev = std::sqrt(variance / config_.bb_period);
        
        // Calculate bands
        bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
        bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
        bb.bandwidth = bb.upper - bb.lower;
        
        // Calculate position within bands (0=lower, 1=upper)
        double current_price = price_history_.back();
        if (bb.bandwidth > 0.0001) {
            bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
            bb.position_pct = std::max(0.0, std::min(1.0, bb.position_pct));
        }
        
        return bb;
    }
    
    // Cross detection
    CrossState detect_cross() {
        if (williams_history_.size() < 2 || rsi_history_.size() < 2) {
            return CrossState::NO_CROSS;
        }
        
        size_t curr_idx = williams_history_.size() - 1;
        size_t prev_idx = curr_idx - 1;
        
        double curr_williams = williams_history_[curr_idx];
        double prev_williams = williams_history_[prev_idx];
        double curr_rsi = rsi_history_[curr_idx];
        double prev_rsi = rsi_history_[prev_idx];
        
        if (config_.debug_mode && curr_idx % 50 == 0) {
            std::cout << "[CROSS] Prev: W=" << prev_williams << " R=" << prev_rsi
                      << " | Curr: W=" << curr_williams << " R=" << curr_rsi
                      << " | Margin=" << config_.cross_margin << std::endl;
        }
        
        // Check for cross up
        if (prev_williams <= prev_rsi && curr_williams > curr_rsi) {
            double separation = curr_williams - curr_rsi;
            
            if (separation >= config_.cross_margin) {
                cross_strength_ = separation / std::max(1.0, config_.cross_margin);
                
                if (config_.debug_mode) {
                    std::cout << "[CROSS] UP detected! Separation=" << separation 
                              << " Strength=" << cross_strength_ << std::endl;
                }
                
                return CrossState::CROSSED_UP;
            }
        }
        
        // Check for cross down
        if (prev_williams >= prev_rsi && curr_williams < curr_rsi) {
            double separation = curr_rsi - curr_williams;
            
            if (separation >= config_.cross_margin) {
                cross_strength_ = separation / std::max(1.0, config_.cross_margin);
                
                if (config_.debug_mode) {
                    std::cout << "[CROSS] DOWN detected! Separation=" << separation 
                              << " Strength=" << cross_strength_ << std::endl;
                }
                
                return CrossState::CROSSED_DOWN;
            }
        }
        
        cross_strength_ = 0.0;
        return CrossState::NO_CROSS;
    }
    
    // Calculate probability and confidence
    ProbabilityConfidence calculate_probability_confidence(
        double williams_norm, double rsi, const BollingerBands& bb, 
        CrossState cross, const Bar& bar) {
        
        ProbabilityConfidence result;
        result.probability = 0.5;
        result.confidence = 0.0;
        
        // Check Bollinger Band alignment
        // BUY: Cross UP + near lower band (position < 0.20)
        // SELL: Cross DOWN + near upper band (position > 0.80)
        bool bb_buy_aligned = (cross == CrossState::CROSSED_UP && 
                               bb.position_pct <= config_.bb_proximity_pct);
        bool bb_sell_aligned = (cross == CrossState::CROSSED_DOWN && 
                                bb.position_pct >= (1.0 - config_.bb_proximity_pct));
        bb_aligned_ = bb_buy_aligned || bb_sell_aligned;
        
        // Base probability from cross and BB position
        if (bb_buy_aligned) {
            // Strong buy signal: cross up near lower band
            result.probability = 0.60 + 0.10 * std::min(2.0, cross_strength_);
            
            // Boost for extreme oversold
            if (bb.position_pct < 0.10) {
                result.probability += 0.05;
            }
        } else if (bb_sell_aligned) {
            // Strong sell signal: cross down near upper band
            result.probability = 0.40 - 0.10 * std::min(2.0, cross_strength_);
            
            // Boost for extreme overbought
            if (bb.position_pct > 0.90) {
                result.probability -= 0.05;
            }
        } else if (cross != CrossState::NO_CROSS) {
            // Cross without BB alignment - weak signal
            if (cross == CrossState::CROSSED_UP) {
                result.probability = 0.52 + 0.03 * std::min(1.0, cross_strength_);
            } else {
                result.probability = 0.48 - 0.03 * std::min(1.0, cross_strength_);
            }
        }
        
        // Calculate confidence
        if (cross != CrossState::NO_CROSS) {
            result.confidence = config_.base_confidence;
            
            // Factor 1: Cross strength bonus
            result.confidence += 0.1 * std::min(2.0, cross_strength_);
            
            // Factor 2: BB alignment bonus
            if (bb_aligned_) {
                result.confidence += 0.15;
            }
            
            // Factor 3: Williams-RSI divergence strength
            double divergence = std::abs(williams_norm - rsi);
            if (divergence > 10) {
                result.confidence += 0.10;
            } else if (divergence > 5) {
                result.confidence += 0.05;
            }
            
            // Factor 4: Volume confirmation
            if (config_.require_volume_confirmation && volume_history_.size() >= 20) {
                double avg_volume = calculate_average_volume();
                if (bar.volume > avg_volume * config_.volume_threshold) {
                    result.confidence += 0.10;
                }
            }
            
            // Factor 5: BB squeeze (narrow bands = higher confidence for breakout)
            if (bb.bandwidth > 0) {
                double bb_width_pct = bb.bandwidth / bb.middle;
                if (bb_width_pct < 0.05) {  // Narrow bands
                    result.confidence += 0.05;
                }
            }
            
            // Cap confidence
            result.confidence = std::min(0.95, result.confidence);
        }
        
        // Ensure probability bounds
        result.probability = std::max(0.05, std::min(0.95, result.probability));
        
        return result;
    }
    
    // Calculate average volume
    double calculate_average_volume() {
        if (volume_history_.empty()) return 1.0;
        
        size_t count = std::min(size_t(20), volume_history_.size());
        double sum = 0.0;
        
        size_t start = volume_history_.size() - count;
        for (size_t i = start; i < volume_history_.size(); i++) {
            sum += volume_history_[i];
        }
        
        return sum / count;
    }
    
    std::string cross_type_to_string(CrossState cross) {
        switch (cross) {
            case CrossState::CROSSED_UP: return "UP";
            case CrossState::CROSSED_DOWN: return "DOWN";
            default: return "NONE";
        }
    }
    
    // Configuration
    WilliamsRSIBBConfig config_;
    
    // Price and volume data
    std::deque<double> price_history_;
    std::deque<double> high_history_;
    std::deque<double> low_history_;
    std::deque<double> volume_history_;
    
    // Indicator value histories
    std::deque<double> williams_history_;
    std::deque<double> rsi_history_;
    
    // RSI calculation state
    double rsi_avg_gain_ = 0.0;
    double rsi_avg_loss_ = 0.0;
    
    // Signal state
    double cross_strength_ = 0.0;
    bool bb_aligned_ = false;
};

// Implementation of WilliamsRSIBBConfig::from_file
inline WilliamsRSIBBConfig WilliamsRSIBBConfig::from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    
    nlohmann::json j;
    file >> j;
    
    WilliamsRSIBBConfig config;
    
    // Load indicator parameters
    if (j.contains("indicators")) {
        auto ind = j["indicators"];
        if (ind.contains("williams_period")) config.williams_period = ind["williams_period"];
        if (ind.contains("rsi_period")) config.rsi_period = ind["rsi_period"];
        if (ind.contains("bb_period")) config.bb_period = ind["bb_period"];
        if (ind.contains("bb_std_dev")) config.bb_std_dev = ind["bb_std_dev"];
    }
    
    // Load trading gates
    if (j.contains("trading_gates")) {
        auto gates = j["trading_gates"];
        if (gates.contains("bb_proximity_pct")) config.bb_proximity_pct = gates["bb_proximity_pct"];
        if (gates.contains("cross_margin")) config.cross_margin = gates["cross_margin"];
    }
    
    // Load signal generation
    if (j.contains("signal_generation")) {
        auto sig = j["signal_generation"];
        if (sig.contains("base_confidence")) config.base_confidence = sig["base_confidence"];
        if (sig.contains("require_volume_confirmation")) {
            config.require_volume_confirmation = sig["require_volume_confirmation"];
        }
        if (sig.contains("volume_threshold")) config.volume_threshold = sig["volume_threshold"];
    }
    
    // Load Sentio integration parameters
    if (j.contains("sentio_integration")) {
        auto sentio = j["sentio_integration"];
        if (sentio.contains("min_confidence")) config.min_confidence_threshold = sentio["min_confidence"];
        if (sentio.contains("min_signal_strength")) config.probability_threshold = sentio["min_signal_strength"];
    }
    
    return config;
}

} // namespace sentio

