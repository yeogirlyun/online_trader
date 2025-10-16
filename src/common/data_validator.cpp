#include "common/data_validator.h"
#include <cmath>
#include <sstream>

namespace sentio {

bool DataValidator::validate_bar(const std::string& symbol, const Bar& bar) {
    last_error_.clear();

    // Check for NaN or invalid prices
    if (std::isnan(bar.open) || std::isnan(bar.high) ||
        std::isnan(bar.low) || std::isnan(bar.close)) {
        last_error_ = "[DataValidator] NaN price detected for " + symbol;
        return false;
    }

    // Check for zero or negative prices
    if (bar.open <= 0 || bar.high <= 0 || bar.low <= 0 || bar.close <= 0) {
        last_error_ = "[DataValidator] Zero/negative price detected for " + symbol;
        return false;
    }

    // Check OHLC consistency
    if (!check_ohlc_consistency(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check price anomaly (compared to previous bar)
    if (!check_price_anomaly(symbol, bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check spread (high-low range)
    if (!check_spread(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check volume
    if (!check_volume(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Store for next comparison
    prev_bars_[symbol] = bar;

    return true;
}

bool DataValidator::check_ohlc_consistency(const Bar& bar) {
    // High must be >= all other prices
    if (bar.high < bar.low || bar.high < bar.open || bar.high < bar.close) {
        last_error_ = "[DataValidator] High price inconsistency (high < low/open/close)";
        return false;
    }

    // Low must be <= all other prices
    if (bar.low > bar.open || bar.low > bar.close) {
        last_error_ = "[DataValidator] Low price inconsistency (low > open/close)";
        return false;
    }

    return true;
}

bool DataValidator::check_price_anomaly(const std::string& symbol, const Bar& bar) {
    auto it = prev_bars_.find(symbol);
    if (it == prev_bars_.end()) {
        return true;  // First bar, no comparison
    }

    const Bar& prev = it->second;
    double price_move = std::abs(bar.close - prev.close) / prev.close;

    if (price_move > config_.max_price_move_pct) {
        std::ostringstream oss;
        oss << "[DataValidator] Price anomaly for " << symbol
            << ": " << (price_move * 100.0) << "% move in 1 minute"
            << " (prev=" << prev.close << ", curr=" << bar.close << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_spread(const Bar& bar) {
    if (bar.close <= 0) return false;

    double spread = (bar.high - bar.low) / bar.close;

    if (spread > config_.max_spread_pct) {
        std::ostringstream oss;
        oss << "[DataValidator] Excessive spread: " << (spread * 100.0) << "%"
            << " (high=" << bar.high << ", low=" << bar.low << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_staleness(const Bar& bar) {
    auto now = std::chrono::system_clock::now();
    auto bar_time = std::chrono::system_clock::from_time_t(bar.timestamp_ms / 1000);
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - bar_time).count();

    if (age_seconds > config_.max_staleness_seconds) {
        std::ostringstream oss;
        oss << "[DataValidator] Stale data: " << age_seconds << " seconds old";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_volume(const Bar& bar) {
    if (bar.volume < config_.min_volume) {
        std::ostringstream oss;
        oss << "[DataValidator] Low volume: " << bar.volume
            << " (min=" << config_.min_volume << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::validate_snapshot(const std::map<std::string, Bar>& snapshot) {
    bool all_valid = true;

    for (const auto& [symbol, bar] : snapshot) {
        if (!validate_bar(symbol, bar)) {
            all_valid = false;
            if (config_.strict_mode) {
                return false;  // Fail fast in strict mode
            }
        }
    }

    return all_valid;
}

void DataValidator::reset() {
    prev_bars_.clear();
    last_error_.clear();
}

} // namespace sentio
