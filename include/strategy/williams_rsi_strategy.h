// williams_rsi_strategy.h
// Simplified Williams %R + RSI strategy (no TSI gating)

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

struct WilliamsRSIConfig {
    // Indicator parameters
    int williams_period = 10;
    int rsi_period = 20;
    int bb_period = 20;
    double bb_std_dev = 2.0;
    
    // Trading parameters
    double cross_margin = 1.0;
    int cross_persist_bars = 1;      // require separation persists N bars
    
    // Bollinger Band gating
    double bb_proximity = 0.40;  // 40% proximity to BB bands
    
    // Anticipatory detection parameters (NEW)
    bool enable_anticipatory_detection = true;
    int anticipatory_lookback = 5;           // bars to look back for velocity
    double convergence_threshold = 0.14;     // slightly looser
    double velocity_weight = 0.6;            // weight for velocity vs closeness
    double closeness_weight = 0.4;
    double distance_normalization = 10.0;    // normalize closeness by this
    double anticipatory_probability_scale = 0.5;  // scale factor for anticipatory signals [0.3, 1.0]
    double min_anticipatory_strength = 0.35; // slightly looser
    int cross_confirmation_bars = 3;         // bars to confirm cross materialized
    
    // Directional amplification parameters (NEW)
    bool enable_directional_amplification = true;
    double amplification_factor = 1.0;       // 1.0 = up to 1.5x amplification
    std::string amplification_method = "directional";  // "directional" or "selective"
    
    // Signal quality filters
    double min_williams_strength = 0.3;      // only trade when Williams is extreme (0-100 scale)
    bool enable_rsi_confirmation = false;    // boost when RSI confirms Williams
    double rsi_confirmation_boost = 0.2;     // boost factor when RSI extreme
    
    // Signal throttling
    bool enable_signal_throttling = false;   // prevent too many signals in short period
    int throttle_window_size = 10;           // bars to look back
    int throttle_max_signals = 3;            // max signals in window
    
    // Legacy signal strength multiplier (for backward compatibility)
    double signal_strength_multiplier = 1.2;
    
    // Signal generation parameters
    int lookback_bars = 5;
    double base_confidence = 0.5;
    
    // Confirmation parameters
    bool require_volume_confirmation = false;
    double volume_threshold = 1.2;
    int confirmation_window = 3;
    
    // Debug mode
    bool debug_mode = false;
    
    // Static factory methods
    static WilliamsRSIConfig from_file(const std::string& path);
    static WilliamsRSIConfig defaults() {
        return WilliamsRSIConfig();
    }
};

class WilliamsRSIStrategy {
public:
    explicit WilliamsRSIStrategy(const WilliamsRSIConfig& config = WilliamsRSIConfig())
        : config_(config) {
        reset();
        
        if (config_.debug_mode) {
            std::cout << "[WR] Strategy initialized with:" << std::endl;
            std::cout << "  Williams period: " << config_.williams_period << std::endl;
            std::cout << "  RSI period: " << config_.rsi_period << std::endl;
            std::cout << "  Cross margin: " << config_.cross_margin << std::endl;
        }
    }
    
    SignalOutput generate_signal(const Bar& bar, int bar_index) {
        // Debug: Print configuration on first signal  
        if (bar_index == 0) {
            std::cout << "[AnticipatoryWR] Loaded configuration:" << std::endl;
            std::cout << "  Anticipatory detection: " << (config_.enable_anticipatory_detection ? "ENABLED" : "DISABLED") << std::endl;
            std::cout << "  Directional amplification: " << (config_.enable_directional_amplification ? "ENABLED" : "DISABLED") << std::endl;
            std::cout << "  Amplification factor: " << std::fixed << std::setprecision(2) 
                      << config_.amplification_factor << std::endl;
        }
        
        // Update indicators with new bar (also stores williams/rsi history)
        update_indicators(bar);
        
        // Initialize output
        SignalOutput signal;
        signal.timestamp_ms = bar.timestamp_ms;
        signal.bar_index = bar_index;
        signal.symbol = bar.symbol;
        signal.strategy_name = "AnticipatoryWR";
        signal.strategy_version = "2.0";
        
        // Need minimum bars for calculation
        size_t min_bars = std::max((size_t)config_.williams_period, (size_t)config_.rsi_period);
        
        if (price_history_.size() < min_bars + config_.anticipatory_lookback + 2) {
            signal.probability = 0.5;
            signal.signal_type = SignalType::NEUTRAL;
            return signal;
        }
        
        // Calculate current indicator values
        double williams_norm = williams_history_.back();  // Already calculated in update_indicators
        double rsi = rsi_history_.back();
        BollingerBands bb = calculate_bollinger_bands();
        
        double probability = 0.5;
        double original_probability = 0.5;
        double velocity = 0.0;
        double closeness = 0.0;
        bool is_anticipatory = false;
        
        // ========== ANTICIPATORY CROSS DETECTION ==========
        if (config_.enable_anticipatory_detection) {
            // Calculate anticipatory metrics
            velocity = calculate_convergence_velocity();
            closeness = calculate_closeness_to_cross();
            
            // Check for actual cross first
            CrossState cross = detect_cross();
            
            if (cross != CrossState::NO_CROSS) {
                // Actual cross detected → use cross-based probability (strong signal)
                probability = calculate_probability_with_bb_gating(
                    williams_norm, rsi, cross, bar, false, false);
                is_anticipatory = false;
            } else {
                // No cross yet → use anticipatory logic
                probability = calculate_anticipatory_probability(
                    williams_norm, rsi, velocity, closeness);
                is_anticipatory = true;
            }
        } else {
            // Legacy mode: only detect actual crosses
            CrossState cross = detect_cross();
            probability = calculate_probability_with_bb_gating(
                williams_norm, rsi, cross, bar, false, false);
        }
        
        original_probability = probability;
        
        // ========== DIRECTIONAL AMPLIFICATION ==========
        if (probability != 0.5 && config_.enable_directional_amplification) {
            double bb_position = calculate_bb_position(bar.close, bb);
            double bb_amplifier = calculate_bb_amplifier_directional(bb_position);
            
            // Apply amplifier to signal strength (distance from 0.5)
            double signal_strength = std::abs(probability - 0.5);
            double amplified_strength = signal_strength * bb_amplifier;
            
            // Reconstruct probability with amplified strength
            double direction = (probability > 0.5) ? 1.0 : -1.0;
            probability = 0.5 + direction * amplified_strength;
            
            // Clamp to valid range [0, 1]
            probability = std::clamp(probability, 0.0, 1.0);
            
            // Debug first 5 amplifications
            static int amp_count = 0;
            if (amp_count < 5 && original_probability != 0.5) {
                std::cout << "[AnticipatoryWR-AMP] Bar " << bar_index 
                          << ": " << std::fixed << std::setprecision(4)
                          << original_probability << " → " << probability 
                          << " (BB_pos=" << bb_position << ", amp=" << bb_amplifier << ")"
                          << (is_anticipatory ? " [ANTICIPATORY]" : " [CROSS]")
                          << std::endl;
                amp_count++;
            }
        }
        
        // Filters removed: rely on Enhanced PSM hysteresis for trade quality control
        
        signal.probability = probability;
        
        // Determine signal type directly from probability
        if (probability > 0.5) {
            signal.signal_type = SignalType::LONG;
        } else if (probability < 0.5) {
            signal.signal_type = SignalType::SHORT;
        } else {
            signal.signal_type = SignalType::NEUTRAL;
        }
        
        // ========== ANTICIPATORY TRACKING ==========
        
        // Record anticipatory predictions
        if (is_anticipatory && signal.signal_type != SignalType::NEUTRAL) {
            bool williams_below_rsi = (williams_norm < rsi);
            anticipatory_tracker_.record_prediction(bar_index, williams_below_rsi);
        }
        
        // Check if crosses materialized
        CrossState current_cross = detect_cross();
        if (current_cross != CrossState::NO_CROSS) {
            bool actual_cross_up = (current_cross == CrossState::CROSSED_UP);
            anticipatory_tracker_.check_materialization(bar_index, actual_cross_up);
        }
        
        // Add comprehensive metadata
        signal.metadata["williams_norm"] = std::to_string(williams_norm);
        signal.metadata["rsi"] = std::to_string(rsi);
        signal.metadata["bb_upper"] = std::to_string(bb.upper);
        signal.metadata["bb_middle"] = std::to_string(bb.middle);
        signal.metadata["bb_lower"] = std::to_string(bb.lower);
        signal.metadata["bb_position"] = std::to_string(calculate_bb_position(bar.close, bb));
        signal.metadata["original_probability"] = std::to_string(original_probability);
        signal.metadata["amplified_probability"] = std::to_string(probability);
        signal.metadata["is_anticipatory"] = is_anticipatory ? "true" : "false";
        signal.metadata["convergence_velocity"] = std::to_string(velocity);
        signal.metadata["closeness_to_cross"] = std::to_string(closeness);
        signal.metadata["amplification_method"] = config_.amplification_method;
        
        // Anticipatory tracker stats
        signal.metadata["anticipatory_predictions"] = std::to_string(anticipatory_tracker_.get_predictions_made());
        signal.metadata["anticipatory_materialized"] = std::to_string(anticipatory_tracker_.get_crosses_materialized());
        signal.metadata["anticipatory_accuracy"] = std::to_string(anticipatory_tracker_.get_accuracy());
        
        return signal;
    }
    
    void reset() {
        price_history_.clear();
        high_history_.clear();
        low_history_.clear();
        williams_history_.clear();
        rsi_history_.clear();
        rsi_gains_.clear();
        rsi_losses_.clear();
        avg_gain_ = 0.0;
        avg_loss_ = 0.0;
        throttler_.reset();
        anticipatory_tracker_.reset();
    }

private:
    WilliamsRSIConfig config_;
    
    // Price history
    std::deque<double> price_history_;
    std::deque<double> high_history_;
    std::deque<double> low_history_;
    
    // Indicator history for cross detection
    std::deque<double> williams_history_;
    std::deque<double> rsi_history_;
    
    // RSI calculation state
    std::deque<double> rsi_gains_;
    std::deque<double> rsi_losses_;
    double avg_gain_ = 0.0;
    double avg_loss_ = 0.0;
    
    // Signal throttling helper
    class SignalThrottler {
    private:
        std::deque<int> recent_signal_bars_;
        
    public:
        bool should_accept_signal(int current_bar, int window_size, int max_signals) {
            // Remove old signals outside window
            while (!recent_signal_bars_.empty() && 
                   current_bar - recent_signal_bars_.front() > window_size) {
                recent_signal_bars_.pop_front();
            }
            
            // Check if under limit
            if (recent_signal_bars_.size() >= (size_t)max_signals) {
                return false;
            }
            
            recent_signal_bars_.push_back(current_bar);
            return true;
        }
        
        void reset() {
            recent_signal_bars_.clear();
        }
    } throttler_;
    
    // Anticipatory accuracy tracker
    class AnticipatoryTracker {
    private:
        int predictions_made_ = 0;
        int crosses_materialized_ = 0;
        std::deque<std::pair<int, bool>> pending_predictions_;  // <bar_index, williams_below_rsi>
        
    public:
        void record_prediction(int bar_index, bool williams_below_rsi) {
            predictions_made_++;
            pending_predictions_.push_back({bar_index, williams_below_rsi});
        }
        
        void check_materialization(int current_bar, bool actual_cross_up) {
            // Check pending predictions (within confirmation window)
            auto it = pending_predictions_.begin();
            while (it != pending_predictions_.end()) {
                int pred_bar = it->first;
                bool predicted_cross_up = it->second;  // Williams was below, predict cross up
                
                // If enough bars have passed, check if cross materialized
                if (current_bar - pred_bar >= 3) {  // confirmation window
                    if (predicted_cross_up == actual_cross_up) {
                        crosses_materialized_++;
                    }
                    it = pending_predictions_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        double get_accuracy() const {
            return predictions_made_ > 0 ? 
                   (double)crosses_materialized_ / predictions_made_ : 0.0;
        }
        
        int get_predictions_made() const { return predictions_made_; }
        int get_crosses_materialized() const { return crosses_materialized_; }
        
        void reset() {
            predictions_made_ = 0;
            crosses_materialized_ = 0;
            pending_predictions_.clear();
        }
    } anticipatory_tracker_;
    
    enum class CrossState {
        NO_CROSS,
        CROSSED_UP,
        CROSSED_DOWN
    };
    
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
    };
    
    void update_indicators(const Bar& bar) {
        if (price_history_.size() >= 1000) price_history_.pop_front();
        if (high_history_.size() >= 1000) high_history_.pop_front();
        if (low_history_.size() >= 1000) low_history_.pop_front();
        
        price_history_.push_back(bar.close);
        high_history_.push_back(bar.high);
        low_history_.push_back(bar.low);
        
        // Calculate and store Williams and RSI for anticipatory detection
        double williams = calculate_williams_normalized();
        double rsi = calculate_rsi();
        
        if (williams_history_.size() >= 100) williams_history_.pop_front();
        if (rsi_history_.size() >= 100) rsi_history_.pop_front();
        
        williams_history_.push_back(williams);
        rsi_history_.push_back(rsi);
    }
    
    double calculate_williams_normalized() {
        if (high_history_.size() < (size_t)config_.williams_period) return 50.0;
        
        size_t start = high_history_.size() - config_.williams_period;
        double highest = *std::max_element(high_history_.begin() + start, high_history_.end());
        double lowest = *std::min_element(low_history_.begin() + start, low_history_.end());
        double current = price_history_.back();
        
        if (highest == lowest) return 50.0;
        
        double williams = ((highest - current) / (highest - lowest)) * 100.0;
        return 100.0 - williams;  // Normalize to 0-100 (inverted so high = bullish)
    }
    
    double calculate_rsi() {
        if (price_history_.size() < (size_t)config_.rsi_period + 1) return 50.0;
        
        if (rsi_gains_.size() < (size_t)config_.rsi_period) {
            // Initial calculation
            for (size_t i = price_history_.size() - config_.rsi_period; i < price_history_.size(); ++i) {
                double change = price_history_[i] - price_history_[i-1];
                rsi_gains_.push_back(std::max(0.0, change));
                rsi_losses_.push_back(std::max(0.0, -change));
            }
            avg_gain_ = std::accumulate(rsi_gains_.begin(), rsi_gains_.end(), 0.0) / config_.rsi_period;
            avg_loss_ = std::accumulate(rsi_losses_.begin(), rsi_losses_.end(), 0.0) / config_.rsi_period;
        } else {
            // Smoothed calculation
            double change = price_history_.back() - price_history_[price_history_.size() - 2];
            double gain = std::max(0.0, change);
            double loss = std::max(0.0, -change);
            
            avg_gain_ = (avg_gain_ * (config_.rsi_period - 1) + gain) / config_.rsi_period;
            avg_loss_ = (avg_loss_ * (config_.rsi_period - 1) + loss) / config_.rsi_period;
        }
        
        if (avg_loss_ == 0.0) return 100.0;
        double rs = avg_gain_ / avg_loss_;
        return 100.0 - (100.0 / (1.0 + rs));
    }
    
    BollingerBands calculate_bollinger_bands() {
        BollingerBands bb;
        
        if (price_history_.size() < (size_t)config_.bb_period) {
            double current = price_history_.empty() ? 0.0 : price_history_.back();
            bb.middle = current;
            bb.upper = current;
            bb.lower = current;
            return bb;
        }
        
        // Calculate SMA (middle band)
        size_t start = price_history_.size() - config_.bb_period;
        double sum = 0.0;
        for (size_t i = start; i < price_history_.size(); ++i) {
            sum += price_history_[i];
        }
        bb.middle = sum / config_.bb_period;
        
        // Calculate standard deviation
        double variance = 0.0;
        for (size_t i = start; i < price_history_.size(); ++i) {
            double diff = price_history_[i] - bb.middle;
            variance += diff * diff;
        }
        double std_dev = std::sqrt(variance / config_.bb_period);
        
        // Calculate upper and lower bands
        bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
        bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
        
        return bb;
    }
    
    bool is_in_bb_buy_zone(double price, const BollingerBands& bb) {
        // Buy zone: [BB_low, BB_low + 20% of BB width]
        // This is the lower 20% of the BB range
        double bb_width = bb.upper - bb.lower;
        double buy_zone_upper = bb.lower + (bb_width * config_.bb_proximity);
        return price >= bb.lower && price <= buy_zone_upper;
    }
    
    bool is_in_bb_sell_zone(double price, const BollingerBands& bb) {
        // Sell zone: [BB_high - 20% of BB width, BB_high]
        // This is the upper 20% of the BB range
        double bb_width = bb.upper - bb.lower;
        double sell_zone_lower = bb.upper - (bb_width * config_.bb_proximity);
        return price >= sell_zone_lower && price <= bb.upper;
    }
    
    CrossState detect_cross() {
        if (williams_history_.size() < 2 || rsi_history_.size() < 2) {
            return CrossState::NO_CROSS;
        }
        
        double w_prev = williams_history_[williams_history_.size() - 2];
        double w_curr = williams_history_.back();
        double r_prev = rsi_history_[rsi_history_.size() - 2];
        double r_curr = rsi_history_.back();
        
        // Simple cross detection first
        bool williams_crossed_above_rsi = (w_prev <= r_prev) && (w_curr > r_curr);
        bool williams_crossed_below_rsi = (w_prev >= r_prev) && (w_curr < r_curr);
        
        // Enforce cross margin/persistence to avoid micro crosses
        if (williams_crossed_above_rsi) {
            double separation = w_curr - r_curr;
            if (separation < config_.cross_margin) return CrossState::NO_CROSS;
            // require persistence: previous N bars maintain separation sign and near margin
            int ok = 0;
            for (int k = 0; k < std::min<int>(config_.cross_persist_bars, (int)williams_history_.size()-1); ++k) {
                size_t idx = williams_history_.size() - 1 - k;
                double sep = williams_history_[idx] - rsi_history_[idx];
                if (sep >= 0.0) ok++;
            }
            if (ok < std::min(config_.cross_persist_bars, (int)williams_history_.size()-1)) return CrossState::NO_CROSS;
            return CrossState::CROSSED_UP;
        }
        if (williams_crossed_below_rsi) {
            double separation = r_curr - w_curr;
            if (separation < config_.cross_margin) return CrossState::NO_CROSS;
            int ok = 0;
            for (int k = 0; k < std::min<int>(config_.cross_persist_bars, (int)williams_history_.size()-1); ++k) {
                size_t idx = williams_history_.size() - 1 - k;
                double sep = rsi_history_[idx] - williams_history_[idx];
                if (sep >= 0.0) ok++;
            }
            if (ok < std::min(config_.cross_persist_bars, (int)williams_history_.size()-1)) return CrossState::NO_CROSS;
            return CrossState::CROSSED_DOWN;
        }
        
        return CrossState::NO_CROSS;
    }
    
    // ========== ANTICIPATORY CROSS DETECTION (NEW) ==========
    
    // Calculate convergence velocity over lookback period (FIXED - Bug #1)
    double calculate_convergence_velocity() {
        if (williams_history_.size() < (size_t)config_.anticipatory_lookback + 1 || 
            rsi_history_.size() < (size_t)config_.anticipatory_lookback + 1) {
            return 0.0;
        }
        
        size_t current_idx = williams_history_.size() - 1;
        size_t past_idx = current_idx - config_.anticipatory_lookback;
        
        double distance_now = williams_history_[current_idx] - rsi_history_[current_idx];
        double distance_past = williams_history_[past_idx] - rsi_history_[past_idx];
        
        // Key fix: Check if actually converging (distance decreasing)
        double abs_distance_now = std::abs(distance_now);
        double abs_distance_past = std::abs(distance_past);
        
        // Convergence = reduction in absolute distance
        double convergence = abs_distance_past - abs_distance_now;
        
        // CRITICAL FIX: Only return positive values for TRUE convergence
        if (convergence <= 0.0) {
            return 0.0;  // Diverging or parallel - no signal
        }
        
        // Also check they're on track to cross (require sign change across lookback)
        bool will_cross = (distance_past * distance_now < 0);
        if (!will_cross) return 0.0;
        
        // Optional: enforce monotonic decrease of |W-R| over recent K bars
        int monotonic_len = std::min(3, (int)current_idx);
        for (int k = 0; k < monotonic_len; ++k) {
            size_t i2 = current_idx - k;
            size_t i1 = i2 > 0 ? i2 - 1 : i2;
            double d2 = std::abs(williams_history_[i2] - rsi_history_[i2]);
            double d1 = std::abs(williams_history_[i1] - rsi_history_[i1]);
            if (d2 > d1 + 1e-9) {
                return 0.0;  // not consistently converging
            }
        }
        
        // Normalize only positive convergence
        return std::min(convergence / 20.0, 1.0);
    }
    
    // Calculate closeness to cross
    double calculate_closeness_to_cross() {
        if (williams_history_.empty() || rsi_history_.empty()) {
            return 0.0;
        }
        
        double current_distance = std::abs(williams_history_.back() - rsi_history_.back());
        
        // Invert and normalize: close = high score
        return 1.0 - std::min(current_distance / config_.distance_normalization, 1.0);
    }
    
    // Generate anticipatory signal (BEFORE actual cross) - ENHANCED
    double calculate_anticipatory_probability(double williams, double rsi, 
                                               double velocity_normalized, double closeness) {
        bool williams_below_rsi = (williams < rsi);
        
        // Only generate signals when converging sufficiently
        if (velocity_normalized < config_.convergence_threshold) {
            return 0.5;  // Not converging enough
        }
        
        // Anticipatory strength (weighted combination)
        double anticipatory_strength = (velocity_normalized * config_.velocity_weight) + 
                                        (closeness * config_.closeness_weight);
        
        // Check minimum anticipatory strength
        if (anticipatory_strength < config_.min_anticipatory_strength) {
            return 0.5;  // Signal too weak
        }
        
        // Williams position context (already in [0, 100])
        double probability = 0.5;
        if (williams_below_rsi) {
            // Approaching from below → anticipate BUY
            // Low Williams (oversold) = stronger buy signal
            double williams_strength = (100.0 - williams) / 100.0;
            probability = 0.5 + (anticipatory_strength * config_.anticipatory_probability_scale * williams_strength);
        } else {
            // Approaching from above → anticipate SELL
            // High Williams (overbought) = stronger sell signal
            double williams_strength = williams / 100.0;
            probability = 0.5 - (anticipatory_strength * config_.anticipatory_probability_scale * williams_strength);
        }
        
        // Apply strict BB proximity gating for anticipatory signals
        BollingerBands bb = calculate_bollinger_bands();
        double bb_position = calculate_bb_position(price_history_.back(), bb);
        bool is_buy_signal = (probability > 0.5);
        double gate = config_.bb_proximity;
        if (is_buy_signal && !(bb_position <= gate)) return 0.5;
        if (!is_buy_signal && !(bb_position >= (1.0 - gate))) return 0.5;
        
        return std::clamp(probability, 0.0, 1.0);
    }
    
    // ========== DIRECTIONAL AMPLIFICATION (NEW) ==========
    
    // Helper: Calculate BB position (0.0 = at lower, 0.5 = at middle, 1.0 = at upper)
    double calculate_bb_position(double price, const BollingerBands& bb) {
        if (bb.upper == bb.lower) return 0.5;  // Degenerate case
        return (price - bb.lower) / (bb.upper - bb.lower);
    }
    
    // Directional amplification: ALWAYS amplify (never dampen)
    double calculate_bb_amplifier_directional(double bb_position) {
        // Pure directional: amplify based on distance from middle
        // ALWAYS amplifies (never dampens), regardless of signal direction
        
        double bb_distance_from_middle = std::abs(bb_position - 0.5);
        double amplifier = 1.0 + (bb_distance_from_middle * config_.amplification_factor);
        
        // amplifier range: [1.0, 1.0 + 0.5 * amplification_factor]
        // With amplification_factor = 1.0: [1.0, 1.5]
        // With amplification_factor = 2.0: [1.0, 2.0]
        
        return amplifier;
    }
    
    // Legacy selective amplifier (for backward compatibility)
    double calculate_bb_amplifier(double bb_position, bool is_buy_signal) {
        // bb_position: 0.0 = at lower band, 1.0 = at upper band, 0.5 = middle
        
        if (is_buy_signal) {
            // BUY signals: amplify near lower band, dampen near upper band
            // At lower (0.0): amplifier = 1.5x (strengthen)
            // At middle (0.5): amplifier = 1.0x (no change)
            // At upper (1.0): amplifier = 0.5x (dampen)
            return 1.5 - bb_position;
            
        } else {
            // SELL signals: amplify near upper band, dampen near lower band
            // At lower (0.0): amplifier = 0.5x (dampen)
            // At middle (0.5): amplifier = 1.0x (no change)
            // At upper (1.0): amplifier = 1.5x (strengthen)
            return 0.5 + bb_position;
        }
    }
    
    double calculate_probability_with_bb_gating(
        double williams_norm, double rsi, CrossState cross, 
        const Bar& bar, bool in_buy_zone, bool in_sell_zone) {
        
        // Williams Score (WS) is in range [0, 100]
        // Low WS = oversold = good for buying
        // High WS = overbought = good for selling
        
        double probability = 0.5;  // Default neutral
        
        if (cross == CrossState::CROSSED_UP) {
            // Williams crossed ABOVE RSI = SELL signal
            // Williams moving higher (more overbought) = bearish
            // Probability = 0.5 - (WS/100) / 2
            // When WS is HIGH (overbought, e.g., 80): 0.5 - 0.4 = 0.1 (strong sell)
            // When WS is LOW (e.g., 20): 0.5 - 0.1 = 0.4 (weak sell)
            probability = 0.5 - (williams_norm / 100.0) / 2.0;
            
        } else if (cross == CrossState::CROSSED_DOWN) {
            // Williams crossed BELOW RSI = BUY signal
            // Williams moving lower (more oversold) = bullish
            // Probability = 0.5 + (1 - WS/100) / 2
            // When WS is LOW (oversold, e.g., 20): 0.5 + (1-0.2)/2 = 0.9 (strong buy)
            // When WS is HIGH (e.g., 80): 0.5 + (1-0.8)/2 = 0.6 (weak buy)
            probability = 0.5 + (1.0 - williams_norm / 100.0) / 2.0;
        }
        // else: No cross -> neutral (0.5)
        
        // Strict BB proximity gating for cross-based signals
        if (probability != 0.5) {
            BollingerBands bb = calculate_bollinger_bands();
            double bb_position = calculate_bb_position(bar.close, bb);
            bool is_buy_signal = (probability > 0.5);
            double gate = config_.bb_proximity;
            if (is_buy_signal && !(bb_position <= gate)) return 0.5;
            if (!is_buy_signal && !(bb_position >= (1.0 - gate))) return 0.5;
        }
        
        // Apply BB amplifier if signal is non-neutral
        if (probability != 0.5) {
            BollingerBands bb = calculate_bollinger_bands();
            double bb_position = calculate_bb_position(bar.close, bb);
            bool is_buy_signal = (probability > 0.5);
            double bb_amplifier = calculate_bb_amplifier(bb_position, is_buy_signal);
            
            // Apply amplifier to signal strength (distance from 0.5)
            double signal_strength = std::abs(probability - 0.5);
            double amplified_strength = signal_strength * bb_amplifier;
            
            // Reconstruct probability with amplified strength
            double direction = (probability > 0.5) ? 1.0 : -1.0;
            probability = 0.5 + direction * amplified_strength;
        }
        
        // Clamp to valid range
        return std::clamp(probability, 0.0, 1.0);
    }
};

// Config loader implementation
inline WilliamsRSIConfig WilliamsRSIConfig::from_file(const std::string& path) {
    WilliamsRSIConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << path << std::endl;
        return config;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        if (j.contains("williams_period")) config.williams_period = j["williams_period"];
        if (j.contains("rsi_period")) config.rsi_period = j["rsi_period"];
        if (j.contains("bb_period")) config.bb_period = j["bb_period"];
        if (j.contains("bb_std_dev")) config.bb_std_dev = j["bb_std_dev"];
        if (j.contains("bb_proximity")) config.bb_proximity = j["bb_proximity"];
        if (j.contains("signal_strength_multiplier")) config.signal_strength_multiplier = j["signal_strength_multiplier"];
        if (j.contains("cross_margin")) config.cross_margin = j["cross_margin"];
        if (j.contains("cross_persist_bars")) config.cross_persist_bars = j["cross_persist_bars"];
        if (j.contains("lookback_bars")) config.lookback_bars = j["lookback_bars"];
        if (j.contains("base_confidence")) config.base_confidence = j["base_confidence"];
        if (j.contains("require_volume_confirmation")) config.require_volume_confirmation = j["require_volume_confirmation"];
        if (j.contains("volume_threshold")) config.volume_threshold = j["volume_threshold"];
        if (j.contains("confirmation_window")) config.confirmation_window = j["confirmation_window"];
        if (j.contains("debug_mode")) config.debug_mode = j["debug_mode"];
        
        // Anticipatory detection parameters
        if (j.contains("anticipation")) {
            auto ant = j["anticipation"];
            if (ant.contains("enable_anticipatory_detection")) 
                config.enable_anticipatory_detection = ant["enable_anticipatory_detection"];
            if (ant.contains("anticipatory_lookback")) 
                config.anticipatory_lookback = ant["anticipatory_lookback"];
            if (ant.contains("convergence_threshold")) 
                config.convergence_threshold = ant["convergence_threshold"];
            if (ant.contains("velocity_weight")) 
                config.velocity_weight = ant["velocity_weight"];
            if (ant.contains("closeness_weight")) 
                config.closeness_weight = ant["closeness_weight"];
            if (ant.contains("distance_normalization")) 
                config.distance_normalization = ant["distance_normalization"];
            if (ant.contains("anticipatory_probability_scale"))
                config.anticipatory_probability_scale = ant["anticipatory_probability_scale"];
            if (ant.contains("min_anticipatory_strength"))
                config.min_anticipatory_strength = ant["min_anticipatory_strength"];
            if (ant.contains("cross_confirmation_bars"))
                config.cross_confirmation_bars = ant["cross_confirmation_bars"];
        }
        
        // Directional amplification parameters
        if (j.contains("amplification")) {
            auto amp = j["amplification"];
            if (amp.contains("enable_directional_amplification")) 
                config.enable_directional_amplification = amp["enable_directional_amplification"];
            if (amp.contains("amplification_factor")) 
                config.amplification_factor = amp["amplification_factor"];
            if (amp.contains("amplification_method")) 
                config.amplification_method = amp["amplification_method"];
            if (amp.contains("signal_strength_multiplier")) 
                config.signal_strength_multiplier = amp["signal_strength_multiplier"];
        }
        
        // Signal quality filters
        if (j.contains("filters")) {
            auto filters = j["filters"];
            if (filters.contains("min_williams_strength"))
                config.min_williams_strength = filters["min_williams_strength"];
            if (filters.contains("enable_rsi_confirmation"))
                config.enable_rsi_confirmation = filters["enable_rsi_confirmation"];
            if (filters.contains("rsi_confirmation_boost"))
                config.rsi_confirmation_boost = filters["rsi_confirmation_boost"];
        }
        
        // Signal throttling
        if (j.contains("throttling")) {
            auto throttle = j["throttling"];
            if (throttle.contains("enable_signal_throttling"))
                config.enable_signal_throttling = throttle["enable_signal_throttling"];
            if (throttle.contains("throttle_window_size"))
                config.throttle_window_size = throttle["throttle_window_size"];
            if (throttle.contains("throttle_max_signals"))
                config.throttle_max_signals = throttle["throttle_max_signals"];
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
    }
    
    return config;
}

} // namespace sentio

