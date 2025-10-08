#pragma once

#include <unordered_set>
#include <string>

namespace sentio {

/**
 * @brief NYSE market holiday and half-day calendar
 *
 * Provides trading day checks for NYSE regular hours (9:30 AM - 4:00 PM ET).
 * Includes full holidays and half-days (1:00 PM close) for 2025-2027.
 *
 * Future enhancement: Load from JSON file for 10+ year coverage.
 */
class NyseCalendar {
public:
    NyseCalendar();

    /**
     * @brief Check if given ET date is a trading day
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return true if not a full holiday (may be half-day)
     */
    bool is_trading_day(const std::string& et_date_ymd) const;

    /**
     * @brief Check if given ET date is a half-day (1:00 PM close)
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return true if early close at 1:00 PM ET
     */
    bool is_half_day(const std::string& et_date_ymd) const;

    /**
     * @brief Get market close hour for given ET date
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return 13 for half-days, 16 for normal days
     */
    int market_close_hour(const std::string& et_date_ymd) const;

    /**
     * @brief Get market close minute for given ET date
     * @param et_date_ymd ET date in YYYY-MM-DD format
     * @return 0 (always on the hour)
     */
    int market_close_minute(const std::string& et_date_ymd) const;

private:
    std::unordered_set<std::string> full_holidays_;
    std::unordered_set<std::string> half_days_;
};

} // namespace sentio
