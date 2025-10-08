#include "common/nyse_calendar.h"

namespace sentio {

NyseCalendar::NyseCalendar() {
    // ============================================================================
    // 2025 NYSE Holidays (Source: NYSE official calendar)
    // ============================================================================
    full_holidays_ = {
        // 2025
        "2025-01-01",  // New Year's Day (Wednesday)
        "2025-01-20",  // Martin Luther King Jr. Day (Monday)
        "2025-02-17",  // Presidents' Day (Monday)
        "2025-04-18",  // Good Friday (Friday)
        "2025-05-26",  // Memorial Day (Monday)
        "2025-06-19",  // Juneteenth National Independence Day (Thursday)
        "2025-07-04",  // Independence Day (Friday)
        "2025-09-01",  // Labor Day (Monday)
        "2025-11-27",  // Thanksgiving Day (Thursday)
        "2025-12-25",  // Christmas Day (Thursday)

        // 2026
        "2026-01-01",  // New Year's Day (Thursday)
        "2026-01-19",  // Martin Luther King Jr. Day (Monday)
        "2026-02-16",  // Presidents' Day (Monday)
        "2026-04-03",  // Good Friday (Friday)
        "2026-05-25",  // Memorial Day (Monday)
        "2026-06-19",  // Juneteenth National Independence Day (Friday)
        "2026-07-03",  // Independence Day observed (Friday, July 4 is Saturday)
        "2026-09-07",  // Labor Day (Monday)
        "2026-11-26",  // Thanksgiving Day (Thursday)
        "2026-12-25",  // Christmas Day (Friday)

        // 2027
        "2027-01-01",  // New Year's Day (Friday)
        "2027-01-18",  // Martin Luther King Jr. Day (Monday)
        "2027-02-15",  // Presidents' Day (Monday)
        "2027-03-26",  // Good Friday (Friday)
        "2027-05-31",  // Memorial Day (Monday)
        "2027-06-18",  // Juneteenth observed (Friday, June 19 is Saturday)
        "2027-07-05",  // Independence Day observed (Monday, July 4 is Sunday)
        "2027-09-06",  // Labor Day (Monday)
        "2027-11-25",  // Thanksgiving Day (Thursday)
        "2027-12-24",  // Christmas observed (Friday, Dec 25 is Saturday)
    };

    // ============================================================================
    // Half-Days (1:00 PM ET close)
    // ============================================================================
    half_days_ = {
        // 2025
        "2025-07-03",  // Day before Independence Day (Thursday)
        "2025-11-28",  // Day after Thanksgiving (Friday - Black Friday)
        "2025-12-24",  // Christmas Eve (Wednesday)

        // 2026
        "2026-11-27",  // Day after Thanksgiving (Friday - Black Friday)
        "2026-12-24",  // Christmas Eve (Thursday)

        // 2027
        "2027-11-26",  // Day after Thanksgiving (Friday - Black Friday)
        "2027-12-23",  // Day before Christmas Eve (Thursday, since Dec 24 is holiday)
    };
}

bool NyseCalendar::is_trading_day(const std::string& et_date_ymd) const {
    // Not a trading day if it's a full holiday
    return full_holidays_.find(et_date_ymd) == full_holidays_.end();
}

bool NyseCalendar::is_half_day(const std::string& et_date_ymd) const {
    return half_days_.find(et_date_ymd) != half_days_.end();
}

int NyseCalendar::market_close_hour(const std::string& et_date_ymd) const {
    return is_half_day(et_date_ymd) ? 13 : 16;  // 1:00 PM or 4:00 PM ET
}

int NyseCalendar::market_close_minute(const std::string& et_date_ymd) const {
    return 0;  // Always close on the hour
}

} // namespace sentio
