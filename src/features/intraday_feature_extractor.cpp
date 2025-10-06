#include "strategy/intraday_ml_strategies.h"
#include "common/utils.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sentio {
namespace intraday {

IntradayFeatureExtractor::IntradayFeatureExtractor() {
    reset();
}

void IntradayFeatureExtractor::reset() {
    bars_1m_.clear();
    bars_5m_.clear();
    bars_15m_.clear();
    session_.reset();
}

void IntradayFeatureExtractor::update(const Bar& bar) {
    // Add to 1-minute bars
    bars_1m_.push_back(bar);
    if (bars_1m_.size() > max_history_size_) {
        bars_1m_.pop_front();
    }
    
    // Update session data
    update_session_data(bar);
    
    // Aggregate to higher timeframes
    aggregate_to_higher_timeframe(bar);
}

IntradayFeatureExtractor::Features IntradayFeatureExtractor::extract_features() const {
    Features features;
    
    if (!is_ready()) {
        return features;  // Return zeros if not ready
    }
    
    const Bar& current_bar = bars_1m_.back();
    
    // 1. Microstructure Features
    features.tick_direction = calculate_tick_direction();
    features.spread_ratio = (current_bar.high - current_bar.low) / current_bar.close;
    features.wicks_ratio = calculate_wicks_ratio(current_bar);
    features.volume_imbalance = calculate_volume_imbalance();
    features.trade_intensity = current_bar.volume / 
        (current_bar.high - current_bar.low + 1e-10);
    
    // Price acceleration (second derivative)
    if (bars_1m_.size() >= 3) {
        double ret1 = (bars_1m_[bars_1m_.size()-1].close - 
                      bars_1m_[bars_1m_.size()-2].close) / 
                      bars_1m_[bars_1m_.size()-2].close;
        double ret2 = (bars_1m_[bars_1m_.size()-2].close - 
                      bars_1m_[bars_1m_.size()-3].close) / 
                      bars_1m_[bars_1m_.size()-3].close;
        features.price_acceleration = ret1 - ret2;
    }
    
    // 2. Short-term Momentum
    if (bars_1m_.size() >= 2) {
        features.micro_momentum_1 = (current_bar.close - bars_1m_[bars_1m_.size()-2].close) / 
                                    bars_1m_[bars_1m_.size()-2].close;
    }
    if (bars_1m_.size() >= 4) {
        features.micro_momentum_3 = (current_bar.close - bars_1m_[bars_1m_.size()-4].close) / 
                                    bars_1m_[bars_1m_.size()-4].close;
    }
    features.rsi_3 = calculate_rsi(3);
    features.ema_cross_fast = calculate_ema_cross(2, 5);
    
    // Velocity (rate of change of returns)
    if (bars_1m_.size() >= 3) {
        double ret_current = features.micro_momentum_1;
        double ret_prev = (bars_1m_[bars_1m_.size()-2].close - 
                          bars_1m_[bars_1m_.size()-3].close) / 
                          bars_1m_[bars_1m_.size()-3].close;
        features.velocity = ret_current - ret_prev;
    }
    
    // 3. Multi-timeframe Confluence
    features.mtf_trend_5m = calculate_mtf_trend(bars_1m_, 5);
    features.mtf_trend_15m = calculate_mtf_trend(bars_1m_, 15);
    features.mtf_volume_5m = calculate_mtf_trend(bars_5m_, 1);  // Use 5m bars
    
    // Volatility ratio (1m vs 5m)
    if (bars_1m_.size() >= 5 && bars_5m_.size() >= 2) {
        double vol_1m = 0.0;
        double vol_5m = 0.0;
        
        // Calculate 1m volatility
        for (size_t i = bars_1m_.size() - 5; i < bars_1m_.size() - 1; ++i) {
            double ret = (bars_1m_[i+1].close - bars_1m_[i].close) / bars_1m_[i].close;
            vol_1m += ret * ret;
        }
        vol_1m = std::sqrt(vol_1m / 4);
        
        // Calculate 5m volatility
        if (bars_5m_.size() >= 2) {
            double ret_5m = (bars_5m_.back().close - bars_5m_[bars_5m_.size()-2].close) / 
                           bars_5m_[bars_5m_.size()-2].close;
            vol_5m = std::abs(ret_5m);
        }
        
        features.mtf_volatility_ratio = vol_1m / (vol_5m + 1e-10);
    }
    
    // Session range position
    if (session_.high > session_.low) {
        features.session_range_position = 
            (current_bar.close - session_.low) / (session_.high - session_.low);
    }
    
    // 4. Session Patterns
    features.vwap_distance = calculate_vwap_distance(current_bar);
    features.opening_range_breakout = calculate_opening_range_breakout(current_bar);
    
    auto [time_sin, time_cos] = calculate_time_features(current_bar.timestamp_ms);
    features.time_of_day_sin = time_sin;
    features.time_of_day_cos = time_cos;
    
    // 5. Volume Profile
    features.volume_percentile = calculate_volume_percentile(current_bar.volume);
    features.delta_cumulative = session_.cumulative_delta;
    
    // VWAP standard deviation bands
    if (session_.cumulative_volume > 0) {
        double vwap = session_.vwap;
        double std_dev = std::sqrt(session_.vwap_variance);
        features.vwap_stdev_bands = (current_bar.close - vwap) / (std_dev + 1e-10);
    }
    
    // Relative volume
    if (bars_1m_.size() >= 20) {
        double avg_volume = 0.0;
        for (size_t i = bars_1m_.size() - 20; i < bars_1m_.size(); ++i) {
            avg_volume += bars_1m_[i].volume;
        }
        avg_volume /= 20.0;
        features.relative_volume = current_bar.volume / (avg_volume + 1.0);
    }
    
    // Volume Price Trend
    double vpt = 0.0;
    for (size_t i = 1; i < std::min(size_t(100), bars_1m_.size()); ++i) {
        double price_change = (bars_1m_[i].close - bars_1m_[i-1].close) / bars_1m_[i-1].close;
        vpt += price_change * bars_1m_[i].volume;
    }
    features.volume_price_trend = vpt / 1e6;  // Scale for numerical stability
    
    return features;
}

// Helper method implementations

double IntradayFeatureExtractor::calculate_tick_direction() const {
    if (bars_1m_.size() < 10) return 0.5;
    
    int upticks = 0, downticks = 0;
    for (size_t i = bars_1m_.size() - 10; i < bars_1m_.size() - 1; ++i) {
        if (bars_1m_[i+1].close > bars_1m_[i].close) upticks++;
        else if (bars_1m_[i+1].close < bars_1m_[i].close) downticks++;
    }
    
    return static_cast<double>(upticks) / (upticks + downticks + 1);
}

double IntradayFeatureExtractor::calculate_wicks_ratio(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double upper_wick = bar.high - std::max(bar.open, bar.close);
    double lower_wick = std::min(bar.open, bar.close) - bar.low;
    double total_wick = upper_wick + lower_wick;
    
    return total_wick / (body + 1e-10);
}

double IntradayFeatureExtractor::calculate_volume_imbalance() const {
    if (bars_1m_.size() < 10) return 0.0;
    
    double buy_volume = 0.0, sell_volume = 0.0;
    for (size_t i = bars_1m_.size() - 10; i < bars_1m_.size(); ++i) {
        const Bar& bar = bars_1m_[i];
        if (bar.close > bar.open) {
            buy_volume += bar.volume;
        } else {
            sell_volume += bar.volume;
        }
    }
    
    double total_volume = buy_volume + sell_volume;
    return (buy_volume - sell_volume) / (total_volume + 1.0);
}

double IntradayFeatureExtractor::calculate_rsi(int period) const {
    if (bars_1m_.size() < static_cast<size_t>(period + 1)) return 50.0;
    
    double gains = 0.0, losses = 0.0;
    size_t start = bars_1m_.size() - period - 1;
    
    for (size_t i = start; i < bars_1m_.size() - 1; ++i) {
        double change = bars_1m_[i+1].close - bars_1m_[i].close;
        if (change > 0) gains += change;
        else losses += -change;
    }
    
    double avg_gain = gains / period;
    double avg_loss = losses / period;
    
    if (avg_loss == 0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

double IntradayFeatureExtractor::calculate_ema_cross(int fast, int slow) const {
    if (bars_1m_.size() < static_cast<size_t>(slow)) return 0.0;
    
    // Simple EMA calculation
    double ema_fast = bars_1m_.back().close;
    double ema_slow = bars_1m_.back().close;
    
    double alpha_fast = 2.0 / (fast + 1.0);
    double alpha_slow = 2.0 / (slow + 1.0);
    
    size_t start = bars_1m_.size() - slow;
    for (size_t i = start; i < bars_1m_.size(); ++i) {
        ema_fast = alpha_fast * bars_1m_[i].close + (1 - alpha_fast) * ema_fast;
        ema_slow = alpha_slow * bars_1m_[i].close + (1 - alpha_slow) * ema_slow;
    }
    
    return (ema_fast - ema_slow) / ema_slow;
}

double IntradayFeatureExtractor::calculate_mtf_trend(const std::deque<Bar>& bars, int period) const {
    if (bars.size() < static_cast<size_t>(period)) return 0.5;
    
    double sum = 0.0;
    size_t start = bars.size() - period;
    for (size_t i = start; i < bars.size(); ++i) {
        sum += bars[i].close;
    }
    double sma = sum / period;
    
    return (bars.back().close > sma) ? 1.0 : 0.0;
}

double IntradayFeatureExtractor::calculate_vwap_distance(const Bar& bar) const {
    if (session_.vwap == 0) return 0.0;
    return (bar.close - session_.vwap) / session_.vwap;
}

double IntradayFeatureExtractor::calculate_opening_range_breakout(const Bar& bar) const {
    if (bars_1m_.size() < 30) return 0.0;
    
    // Calculate opening range (first 30 minutes)
    double opening_high = bars_1m_[0].high;
    double opening_low = bars_1m_[0].low;
    
    for (size_t i = 0; i < std::min(size_t(30), bars_1m_.size()); ++i) {
        opening_high = std::max(opening_high, bars_1m_[i].high);
        opening_low = std::min(opening_low, bars_1m_[i].low);
    }
    
    double opening_mid = (opening_high + opening_low) / 2.0;
    return (bar.close - opening_mid) / (opening_mid + 1e-10);
}

std::pair<double, double> IntradayFeatureExtractor::calculate_time_features(int64_t timestamp_ms) const {
    // Assuming 8-hour trading session (480 minutes)
    const int minutes_in_session = 480;
    const int64_t ms_per_minute = 60000;
    
    // Calculate minute of day (simplified - in production would use actual market hours)
    int64_t session_elapsed_ms = timestamp_ms - session_.session_start_ms;
    int minute_of_session = static_cast<int>(session_elapsed_ms / ms_per_minute) % minutes_in_session;
    
    double angle = 2.0 * M_PI * minute_of_session / minutes_in_session;
    return std::make_pair(std::sin(angle), std::cos(angle));
}

double IntradayFeatureExtractor::calculate_volume_percentile(double volume) const {
    if (bars_1m_.size() < 100) return 0.5;
    
    // Count how many recent volumes are less than current
    int count_below = 0;
    size_t start = bars_1m_.size() - 100;
    
    for (size_t i = start; i < bars_1m_.size(); ++i) {
        if (bars_1m_[i].volume < volume) count_below++;
    }
    
    return static_cast<double>(count_below) / 100.0;
}

void IntradayFeatureExtractor::update_session_data(const Bar& bar) {
    // Initialize session on first bar
    if (session_.session_start_ms == 0) {
        session_.session_start_ms = bar.timestamp_ms;
        session_.open = bar.open;
        session_.high = bar.high;
        session_.low = bar.low;
    } else {
        session_.high = std::max(session_.high, bar.high);
        session_.low = std::min(session_.low, bar.low);
    }
    
    // Update VWAP
    double typical_price = (bar.high + bar.low + bar.close) / 3.0;
    session_.cumulative_pv += typical_price * bar.volume;
    session_.cumulative_volume += bar.volume;
    
    if (session_.cumulative_volume > 0) {
        double old_vwap = session_.vwap;
        session_.vwap = session_.cumulative_pv / session_.cumulative_volume;
        
        // Update VWAP variance (for standard deviation bands)
        double diff = typical_price - session_.vwap;
        session_.vwap_variance += diff * diff * bar.volume / session_.cumulative_volume;
    }
    
    // Update cumulative delta
    if (bar.close > bar.open) {
        session_.cumulative_delta += bar.volume;
    } else {
        session_.cumulative_delta -= bar.volume;
    }
}

void IntradayFeatureExtractor::aggregate_to_higher_timeframe(const Bar& bar) {
    // Simple aggregation to 5-minute bars
    static int bar_count = 0;
    static Bar temp_5m_bar;
    
    if (bar_count == 0) {
        temp_5m_bar = bar;
    } else {
        temp_5m_bar.high = std::max(temp_5m_bar.high, bar.high);
        temp_5m_bar.low = std::min(temp_5m_bar.low, bar.low);
        temp_5m_bar.close = bar.close;
        temp_5m_bar.volume += bar.volume;
    }
    
    bar_count++;
    if (bar_count >= 5) {
        bars_5m_.push_back(temp_5m_bar);
        if (bars_5m_.size() > 100) {
            bars_5m_.pop_front();
        }
        bar_count = 0;
        
        // Aggregate to 15-minute bars
        static int bar_5m_count = 0;
        static Bar temp_15m_bar;
        
        if (bar_5m_count == 0) {
            temp_15m_bar = temp_5m_bar;
        } else {
            temp_15m_bar.high = std::max(temp_15m_bar.high, temp_5m_bar.high);
            temp_15m_bar.low = std::min(temp_15m_bar.low, temp_5m_bar.low);
            temp_15m_bar.close = temp_5m_bar.close;
            temp_15m_bar.volume += temp_5m_bar.volume;
        }
        
        bar_5m_count++;
        if (bar_5m_count >= 3) {
            bars_15m_.push_back(temp_15m_bar);
            if (bars_15m_.size() > 50) {
                bars_15m_.pop_front();
            }
            bar_5m_count = 0;
        }
    }
}

} // namespace intraday
} // namespace sentio

