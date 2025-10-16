#pragma once

#include "common/types.h"
#include <map>
#include <chrono>
#include <string>

namespace sentio {

/**
 * @brief Data quality validation for market data bars
 *
 * Validates incoming bar data to prevent:
 * - Price anomalies (excessive moves)
 * - Stale data
 * - Invalid OHLC relationships
 * - Zero/negative prices
 */
class DataValidator {
public:
    struct ValidationConfig {
        double max_price_move_pct = 0.10;     // 10% max move per minute
        double max_spread_pct = 0.05;         // 5% max high-low spread
        int max_staleness_seconds = 60;       // 60 seconds max staleness
        double min_volume = 100;              // Minimum volume threshold
        bool strict_mode = true;              // Fail on any violation
    };

    explicit DataValidator(ValidationConfig config = {})
        : config_(config) {}

    // Validate single bar
    bool validate_bar(const std::string& symbol, const Bar& bar);

    // Validate snapshot (multi-symbol)
    bool validate_snapshot(const std::map<std::string, Bar>& snapshot);

    // Get validation report
    std::string get_last_error() const { return last_error_; }

    // Reset validator state
    void reset();

private:
    ValidationConfig config_;
    std::map<std::string, Bar> prev_bars_;
    std::string last_error_;

    bool check_price_anomaly(const std::string& symbol, const Bar& bar);
    bool check_spread(const Bar& bar);
    bool check_staleness(const Bar& bar);
    bool check_volume(const Bar& bar);
    bool check_ohlc_consistency(const Bar& bar);
};

} // namespace sentio
