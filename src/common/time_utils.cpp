#include "common/time_utils.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace sentio {

std::tm TradingSession::to_local_time(const std::chrono::system_clock::time_point& tp) const {
    // C++20 thread-safe timezone conversion using zoned_time
    // This replaces the unsafe setenv("TZ") approach

    #if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        // Use C++20 timezone database
        try {
            const auto* tz = std::chrono::locate_zone(timezone_name);
            std::chrono::zoned_time zt{tz, tp};

            // Convert zoned_time to std::tm
            auto local_time = zt.get_local_time();
            auto local_dp = std::chrono::floor<std::chrono::days>(local_time);
            auto ymd = std::chrono::year_month_day{local_dp};
            auto tod = std::chrono::hh_mm_ss{local_time - local_dp};

            std::tm result{};
            result.tm_year = static_cast<int>(ymd.year()) - 1900;
            result.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
            result.tm_mday = static_cast<unsigned>(ymd.day());
            result.tm_hour = tod.hours().count();
            result.tm_min = tod.minutes().count();
            result.tm_sec = tod.seconds().count();

            // Calculate day of week
            auto dp_sys = std::chrono::sys_days{ymd};
            auto weekday = std::chrono::weekday{dp_sys};
            result.tm_wday = weekday.c_encoding();

            // DST info
            auto info = zt.get_info();
            result.tm_isdst = (info.save != std::chrono::minutes{0}) ? 1 : 0;

            return result;

        } catch (const std::exception& e) {
            // Fallback: if timezone not found, use UTC
            auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm result;
            gmtime_r(&tt, &result);
            return result;
        }
    #else
        // Fallback for C++17: use old setenv approach (NOT thread-safe)
        // This should not happen since we require C++20
        #warning "C++20 chrono timezone database not available - using unsafe setenv fallback"

        auto tt = std::chrono::system_clock::to_time_t(tp);

        const char* old_tz = getenv("TZ");
        setenv("TZ", timezone_name.c_str(), 1);
        tzset();

        std::tm local_tm;
        localtime_r(&tt, &local_tm);

        if (old_tz) {
            setenv("TZ", old_tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        return local_tm;
    #endif
}

bool TradingSession::is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    // Calculate minutes since midnight
    int open_mins = market_open_hour * 60 + market_open_minute;
    int close_mins = market_close_hour * 60 + market_close_minute;
    int current_mins = hour * 60 + minute;

    return current_mins >= open_mins && current_mins < close_mins;
}

bool TradingSession::is_weekday(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    // tm_wday: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int wday = local_tm.tm_wday;

    return wday >= 1 && wday <= 5;  // Monday - Friday
}

std::string TradingSession::to_local_string(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    ss << " " << timezone_name;

    return ss.str();
}

std::string to_iso_string(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm;
    gmtime_r(&tt, &utc_tm);

    std::stringstream ss;
    ss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");

    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms << "Z";

    return ss.str();
}

} // namespace sentio
