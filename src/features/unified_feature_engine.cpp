#include "features/unified_feature_engine.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace sentio {
namespace features {

// IncrementalSMA implementation
IncrementalSMA::IncrementalSMA(int period) : period_(period) {
    // Note: std::deque doesn't have reserve(), but that's okay
}

double IncrementalSMA::update(double value) {
    if (values_.size() >= period_) {
        sum_ -= values_.front();
        values_.pop_front();
    }
    
    values_.push_back(value);
    sum_ += value;
    
    return get_value();
}

// IncrementalEMA implementation
IncrementalEMA::IncrementalEMA(int period, double alpha) {
    if (alpha < 0) {
        alpha_ = 2.0 / (period + 1.0);  // Standard EMA alpha
    } else {
        alpha_ = alpha;
    }
}

double IncrementalEMA::update(double value) {
    if (!initialized_) {
        ema_value_ = value;
        initialized_ = true;
    } else {
        ema_value_ = alpha_ * value + (1.0 - alpha_) * ema_value_;
    }
    return ema_value_;
}

// IncrementalRSI implementation
IncrementalRSI::IncrementalRSI(int period) 
    : gain_sma_(period), loss_sma_(period) {
}

double IncrementalRSI::update(double price) {
    if (first_update_) {
        prev_price_ = price;
        first_update_ = false;
        return 50.0;  // Neutral RSI
    }
    
    double change = price - prev_price_;
    double gain = std::max(0.0, change);
    double loss = std::max(0.0, -change);
    
    gain_sma_.update(gain);
    loss_sma_.update(loss);
    
    prev_price_ = price;
    
    return get_value();
}

double IncrementalRSI::get_value() const {
    if (!is_ready()) return 50.0;
    
    double avg_gain = gain_sma_.get_value();
    double avg_loss = loss_sma_.get_value();
    
    if (avg_loss == 0.0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// UnifiedFeatureEngine implementation
UnifiedFeatureEngine::UnifiedFeatureEngine(const Config& config) : config_(config) {
    initialize_calculators();
    cached_features_.resize(config_.total_features(), 0.0);

    // Initialize feature schema
    initialize_feature_schema();
}

void UnifiedFeatureEngine::initialize_calculators() {
    // Initialize SMA calculators for various periods
    std::vector<int> sma_periods = {5, 10, 20, 50, 100, 200};
    for (int period : sma_periods) {
        sma_calculators_["sma_" + std::to_string(period)] = 
            std::make_unique<IncrementalSMA>(period);
    }
    
    // Initialize EMA calculators
    std::vector<int> ema_periods = {5, 10, 20, 50};
    for (int period : ema_periods) {
        ema_calculators_["ema_" + std::to_string(period)] = 
            std::make_unique<IncrementalEMA>(period);
    }
    
    // Initialize RSI calculators
    std::vector<int> rsi_periods = {14, 21};
    for (int period : rsi_periods) {
        rsi_calculators_["rsi_" + std::to_string(period)] =
            std::make_unique<IncrementalRSI>(period);
    }
}

void UnifiedFeatureEngine::initialize_feature_schema() {
    // Initialize feature schema with all 126 feature names
    feature_schema_.version = 3;  // Bump when changing feature set
    feature_schema_.feature_names = get_feature_names();
    feature_schema_.finalize();

    std::cout << "[FeatureEngine] Initialized schema v" << feature_schema_.version
              << " with " << feature_schema_.feature_names.size()
              << " features, hash=" << feature_schema_.hash << std::endl;
}

void UnifiedFeatureEngine::update(const Bar& bar) {
    // DEBUG: Track bar history size WITH INSTANCE POINTER
    size_t size_before = bar_history_.size();

    // Add to history
    bar_history_.push_back(bar);

    size_t size_after_push = bar_history_.size();

    // Maintain max history size
    if (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
    }

    size_t size_final = bar_history_.size();

    // DEBUG: Log size changes (every 100 bars to avoid spam)
    static int update_count = 0;
    update_count++;
    if (update_count <= 10 || update_count % 100 == 0 || size_final < 70) {
        std::cout << "[UFE@" << (void*)this << "] update #" << update_count
                  << ": before=" << size_before
                  << " → after_push=" << size_after_push
                  << " → final=" << size_final
                  << " (max=" << config_.max_history_size
                  << ", ready=" << (size_final >= 64 ? "YES" : "NO") << ")"
                  << std::endl;
    }

    // Update returns
    update_returns(bar);

    // Update incremental calculators
    update_calculators(bar);

    // Invalidate cache
    invalidate_cache();
}

void UnifiedFeatureEngine::update_returns(const Bar& bar) {
    if (!bar_history_.empty() && bar_history_.size() > 1) {
        double prev_close = bar_history_[bar_history_.size() - 2].close;
        double current_close = bar.close;
        
        double return_val = (current_close - prev_close) / prev_close;
        double log_return = std::log(current_close / prev_close);
        
        returns_.push_back(return_val);
        log_returns_.push_back(log_return);
        
        // Maintain max size
        if (returns_.size() > config_.max_history_size) {
            returns_.pop_front();
            log_returns_.pop_front();
        }
    }
}

void UnifiedFeatureEngine::update_calculators(const Bar& bar) {
    double close = bar.close;
    
    // Update SMA calculators
    for (auto& [name, calc] : sma_calculators_) {
        calc->update(close);
    }
    
    // Update EMA calculators
    for (auto& [name, calc] : ema_calculators_) {
        calc->update(close);
    }
    
    // Update RSI calculators
    for (auto& [name, calc] : rsi_calculators_) {
        calc->update(close);
    }
}

void UnifiedFeatureEngine::invalidate_cache() {
    cache_valid_ = false;
}

std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_ && config_.enable_caching) {
        return cached_features_;
    }
    
    std::vector<double> features;
    features.reserve(config_.total_features());
    
    if (bar_history_.empty()) {
        // Return zeros if no data
        features.resize(config_.total_features(), 0.0);
        return features;
    }
    
    const Bar& current_bar = bar_history_.back();
    
    // 1. Time features (8)
    if (config_.enable_time_features) {
        auto time_features = calculate_time_features(current_bar);
        features.insert(features.end(), time_features.begin(), time_features.end());
    }
    
    // 2. Price action features (12)
    if (config_.enable_price_action) {
        auto price_features = calculate_price_action_features();
        features.insert(features.end(), price_features.begin(), price_features.end());
    }
    
    // 3. Moving average features (16)
    if (config_.enable_moving_averages) {
        auto ma_features = calculate_moving_average_features();
        features.insert(features.end(), ma_features.begin(), ma_features.end());
    }
    
    // 4. Momentum features (20)
    if (config_.enable_momentum) {
        auto momentum_features = calculate_momentum_features();
        features.insert(features.end(), momentum_features.begin(), momentum_features.end());
    }
    
    // 5. Volatility features (15)
    if (config_.enable_volatility) {
        auto vol_features = calculate_volatility_features();
        features.insert(features.end(), vol_features.begin(), vol_features.end());
    }
    
    // 6. Volume features (12)
    if (config_.enable_volume) {
        auto volume_features = calculate_volume_features();
        features.insert(features.end(), volume_features.begin(), volume_features.end());
    }
    
    // 7. Statistical features (10)
    if (config_.enable_statistical) {
        auto stat_features = calculate_statistical_features();
        features.insert(features.end(), stat_features.begin(), stat_features.end());
    }
    
    // 8. Pattern features (8)
    if (config_.enable_pattern_detection) {
        auto pattern_features = calculate_pattern_features();
        features.insert(features.end(), pattern_features.begin(), pattern_features.end());
    }
    
    // 9. Price lag features (10)
    if (config_.enable_price_lags) {
        auto price_lag_features = calculate_price_lag_features();
        features.insert(features.end(), price_lag_features.begin(), price_lag_features.end());
    }
    
    // 10. Return lag features (15)
    if (config_.enable_return_lags) {
        auto return_lag_features = calculate_return_lag_features();
        features.insert(features.end(), return_lag_features.begin(), return_lag_features.end());
    }
    
    // Ensure we have the right number of features
    features.resize(config_.total_features(), 0.0);

    // CRITICAL: Apply NaN guards and clamping
    sanitize_features(features);

    // Validate feature count matches schema
    if (!feature_schema_.feature_names.empty() &&
        features.size() != feature_schema_.feature_names.size()) {
        std::cerr << "[FeatureEngine] ERROR: Feature size mismatch: got "
                  << features.size() << ", expected "
                  << feature_schema_.feature_names.size() << std::endl;
    }

    // Cache results
    if (config_.enable_caching) {
        cached_features_ = features;
        cache_valid_ = true;
    }

    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_time_features(const Bar& bar) const {
    std::vector<double> features(config_.time_features, 0.0);
    
    // Convert timestamp to time components
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);
    
    if (time_info) {
        // Cyclical encoding of time components
        double hour = time_info->tm_hour;
        double minute = time_info->tm_min;
        double day_of_week = time_info->tm_wday;
        double day_of_month = time_info->tm_mday;
        
        // Hour cyclical encoding (24 hours)
        features[0] = std::sin(2 * M_PI * hour / 24.0);
        features[1] = std::cos(2 * M_PI * hour / 24.0);
        
        // Minute cyclical encoding (60 minutes)
        features[2] = std::sin(2 * M_PI * minute / 60.0);
        features[3] = std::cos(2 * M_PI * minute / 60.0);
        
        // Day of week cyclical encoding (7 days)
        features[4] = std::sin(2 * M_PI * day_of_week / 7.0);
        features[5] = std::cos(2 * M_PI * day_of_week / 7.0);
        
        // Day of month cyclical encoding (31 days)
        features[6] = std::sin(2 * M_PI * day_of_month / 31.0);
        features[7] = std::cos(2 * M_PI * day_of_month / 31.0);
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_action_features() const {
    std::vector<double> features(config_.price_action_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    const Bar& previous = bar_history_[bar_history_.size() - 2];
    
    // Basic OHLC ratios
    features[0] = safe_divide(current.high - current.low, current.close);  // Range/Close
    features[1] = safe_divide(current.close - current.open, current.close);  // Body/Close
    features[2] = safe_divide(current.high - std::max(current.open, current.close), current.close);  // Upper shadow
    features[3] = safe_divide(std::min(current.open, current.close) - current.low, current.close);  // Lower shadow
    
    // Gap analysis
    features[4] = safe_divide(current.open - previous.close, previous.close);  // Gap
    
    // Price position within range
    features[5] = safe_divide(current.close - current.low, current.high - current.low);  // Close position
    
    // Volatility measures
    features[6] = safe_divide(current.high - current.low, previous.close);  // True range ratio
    
    // Price momentum
    features[7] = safe_divide(current.close - previous.close, previous.close);  // Return
    features[8] = safe_divide(current.high - previous.high, previous.high);  // High momentum
    features[9] = safe_divide(current.low - previous.low, previous.low);  // Low momentum
    
    // Volume-price relationship
    features[10] = safe_divide(current.volume, previous.volume + 1.0) - 1.0;  // Volume change
    features[11] = safe_divide(current.volume * std::abs(current.close - current.open), current.close);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_moving_average_features() const {
    std::vector<double> features(config_.moving_average_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    int idx = 0;
    
    // SMA ratios
    for (const auto& [name, calc] : sma_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    // EMA ratios
    for (const auto& [name, calc] : ema_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    std::vector<double> features(config_.momentum_features, 0.0);
    
    int idx = 0;
    
    // RSI features
    for (const auto& [name, calc] : rsi_calculators_) {
        if (idx >= config_.momentum_features) break;
        if (calc->is_ready()) {
            features[idx] = calc->get_value() / 100.0;  // Normalize to [0,1]
        }
        idx++;
    }
    
    // Simple momentum features
    if (bar_history_.size() >= 10 && idx < config_.momentum_features) {
        double current = bar_history_.back().close;
        double past_5 = bar_history_[bar_history_.size() - 6].close;
        double past_10 = bar_history_[bar_history_.size() - 11].close;
        
        features[idx++] = safe_divide(current - past_5, past_5);  // 5-period momentum
        features[idx++] = safe_divide(current - past_10, past_10);  // 10-period momentum
    }
    
    // Rate of change features
    if (returns_.size() >= 5 && idx < config_.momentum_features) {
        // Recent return statistics
        auto recent_returns = std::vector<double>(returns_.end() - 5, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 5.0;
        features[idx++] = mean_return;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    std::vector<double> features(config_.volatility_features, 0.0);
    
    if (bar_history_.size() < 20) return features;
    
    // ATR-based features
    double atr_14 = calculate_atr(14);
    double atr_20 = calculate_atr(20);
    double current_price = bar_history_.back().close;
    
    features[0] = safe_divide(atr_14, current_price);  // ATR ratio
    features[1] = safe_divide(atr_20, current_price);  // ATR 20 ratio
    
    // Return volatility
    if (returns_.size() >= 20) {
        auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 20.0;
        
        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= 19.0;  // Sample variance
        
        features[2] = std::sqrt(variance);  // Return volatility
        features[3] = std::sqrt(variance * 252);  // Annualized volatility
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volume_features() const {
    std::vector<double> features(config_.volume_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Volume ratios and trends
    if (bar_history_.size() >= 20) {
        // Calculate average volume over last 20 periods
        double avg_volume = 0.0;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            avg_volume += bar_history_[i].volume;
        }
        avg_volume /= 20.0;
        
        features[0] = safe_divide(current.volume, avg_volume) - 1.0;  // Volume ratio
    }
    
    // Volume-price correlation
    features[1] = current.volume * std::abs(current.close - current.open);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_statistical_features() const {
    std::vector<double> features(config_.statistical_features, 0.0);
    
    if (returns_.size() < 20) return features;
    
    // Return distribution statistics
    auto recent_returns = std::vector<double>(returns_.end() - std::min(20UL, returns_.size()), returns_.end());
    
    // Skewness approximation
    double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / recent_returns.size();
    double variance = 0.0;
    double skew_sum = 0.0;
    
    for (double ret : recent_returns) {
        double diff = ret - mean_return;
        variance += diff * diff;
        skew_sum += diff * diff * diff;
    }
    
    variance /= (recent_returns.size() - 1);
    double std_dev = std::sqrt(variance);
    
    if (std_dev > 1e-8) {
        features[0] = skew_sum / (recent_returns.size() * std_dev * std_dev * std_dev);  // Skewness
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_pattern_features() const {
    std::vector<double> features(config_.pattern_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Basic candlestick patterns
    features[0] = is_doji(current) ? 1.0 : 0.0;
    features[1] = is_hammer(current) ? 1.0 : 0.0;
    features[2] = is_shooting_star(current) ? 1.0 : 0.0;
    
    if (bar_history_.size() >= 2) {
        features[3] = is_engulfing_bullish(bar_history_.size() - 1) ? 1.0 : 0.0;
        features[4] = is_engulfing_bearish(bar_history_.size() - 1) ? 1.0 : 0.0;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_lag_features() const {
    std::vector<double> features(config_.price_lag_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(bar_history_.size()) > lag) {
            double lagged_price = bar_history_[bar_history_.size() - 1 - lag].close;
            features[i] = safe_divide(current_price, lagged_price) - 1.0;
        }
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_return_lag_features() const {
    std::vector<double> features(config_.return_lag_features, 0.0);
    
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100, 150, 200, 250, 300, 500};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(returns_.size()) > lag) {
            features[i] = returns_[returns_.size() - 1 - lag];
        }
    }
    
    return features;
}

// Utility methods
double UnifiedFeatureEngine::safe_divide(double numerator, double denominator, double fallback) const {
    if (std::abs(denominator) < 1e-10) {
        return fallback;
    }
    return numerator / denominator;
}

double UnifiedFeatureEngine::calculate_atr(int period) const {
    if (static_cast<int>(bar_history_.size()) < period + 1) return 0.0;
    
    double atr_sum = 0.0;
    for (int i = 0; i < period; ++i) {
        size_t idx = bar_history_.size() - 1 - i;
        atr_sum += calculate_true_range(idx);
    }
    
    return atr_sum / period;
}

double UnifiedFeatureEngine::calculate_true_range(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return 0.0;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    
    return std::max({tr1, tr2, tr3});
}

// Pattern detection helpers
bool UnifiedFeatureEngine::is_doji(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double range = bar.high - bar.low;
    return range > 0 && (body / range) < 0.1;
}

bool UnifiedFeatureEngine::is_hammer(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return lower_shadow > 2 * body && upper_shadow < body;
}

bool UnifiedFeatureEngine::is_shooting_star(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return upper_shadow > 2 * body && lower_shadow < body;
}

bool UnifiedFeatureEngine::is_engulfing_bullish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bearish = previous.close < previous.open;
    bool curr_bullish = current.close > current.open;
    bool engulfs = current.open < previous.close && current.close > previous.open;
    
    return prev_bearish && curr_bullish && engulfs;
}

bool UnifiedFeatureEngine::is_engulfing_bearish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bullish = previous.close > previous.open;
    bool curr_bearish = current.close < current.open;
    bool engulfs = current.open > previous.close && current.close < previous.open;
    
    return prev_bullish && curr_bearish && engulfs;
}

void UnifiedFeatureEngine::reset() {
    bar_history_.clear();
    returns_.clear();
    log_returns_.clear();
    
    // Reset calculators
    initialize_calculators();
    
    invalidate_cache();
}

bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    bool ready = bar_history_.size() >= 64;

    // DEBUG: Log first few checks and any checks that fail WITH INSTANCE POINTER
    static int check_count = 0;
    check_count++;
    if (check_count <= 10 || !ready) {
        std::cout << "[UFE@" << (void*)this << "] is_ready() check #" << check_count
                  << ": bar_history_.size()=" << bar_history_.size()
                  << " (need 64) → " << (ready ? "READY" : "NOT_READY")
                  << std::endl;
    }

    return ready;
}

std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());
    
    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos", 
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});
    
    // Add more feature names as needed...
    // (This is a simplified version for now)
    
    return names;
}

} // namespace features
} // namespace sentio

