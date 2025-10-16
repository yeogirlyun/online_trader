# EOD Production Hardening Plan

**Date**: 2025-10-08
**Author**: System Design Review
**Status**: Implementation Plan

---

## Executive Summary

The current ET time management system (v1.0) has critical gaps that could cause:
- **Double liquidation** if process restarts during EOD window
- **Missed liquidation** on NYSE holidays (trades when market closed)
- **Half-day failures** (e.g., 1 PM close on Black Friday)
- **DST edge cases** during spring-forward/fall-back transitions

This plan addresses these risks through 3 implementation phases.

---

## Phase 1: Critical Fixes (C++17 Compatible)

**Timeline**: Immediate (1-2 hours)
**Risk**: HIGH → MEDIUM

### 1.1 Idempotent EOD State Tracking

**Problem**: No persistence of EOD execution - process restart could double-liquidate or miss entirely.

**Solution**: Simple file-based state marker

```cpp
// include/common/eod_state.h
#pragma once
#include <string>
#include <optional>
#include <fstream>

namespace sentio {

class EodStateStore {
public:
    explicit EodStateStore(std::string state_file_path);

    // Returns ET date (YYYY-MM-DD) of last completed EOD
    std::optional<std::string> last_eod_date() const;

    // Mark EOD complete for given ET date
    void mark_eod_complete(const std::string& et_date);

    // Check if EOD already done for given date
    bool is_eod_complete(const std::string& et_date) const;

private:
    std::string state_file_;
};

} // namespace sentio
```

**Implementation**:
```cpp
// src/common/eod_state.cpp
#include "common/eod_state.h"
#include <sstream>
#include <iostream>

namespace sentio {

EodStateStore::EodStateStore(std::string path) : state_file_(std::move(path)) {}

std::optional<std::string> EodStateStore::last_eod_date() const {
    std::ifstream in(state_file_);
    if (!in) return std::nullopt;

    std::string line;
    if (std::getline(in, line) && !line.empty()) {
        return line;
    }
    return std::nullopt;
}

void EodStateStore::mark_eod_complete(const std::string& et_date) {
    std::ofstream out(state_file_, std::ios::trunc);
    if (!out) {
        std::cerr << "ERROR: Failed to write EOD state to " << state_file_ << "\n";
        return;
    }
    out << et_date << "\n";
    out.flush();
}

bool EodStateStore::is_eod_complete(const std::string& et_date) const {
    auto last = last_eod_date();
    return last.has_value() && last.value() == et_date;
}

} // namespace sentio
```

**Usage in live_trade_command.cpp**:
```cpp
// Add member variable
EodStateStore eod_state_{"/tmp/sentio_eod_state.txt"};  // TODO: use proper app data dir

// In on_bar() before EOD check:
std::string today_et = et_time_.get_current_et_string().substr(0, 10);  // YYYY-MM-DD

if (eod_state_.is_eod_complete(today_et)) {
    // Already liquidated today - skip
    return;
}

if (is_end_of_day_liquidation_time()) {
    log_system("[" + timestamp + "] END OF DAY - Liquidating all positions");
    liquidate_all_positions();
    eod_state_.mark_eod_complete(today_et);
    return;
}
```

### 1.2 NYSE Holiday Calendar (2025-2027)

**Problem**: `is_trading_day()` only checks weekdays - will trade on holidays.

**Solution**: Hardcoded holiday list (acceptable short-term, replace with JSON later)

```cpp
// include/common/nyse_calendar.h
#pragma once
#include <unordered_set>
#include <string>
#include <chrono>

namespace sentio {

class NyseCalendar {
public:
    NyseCalendar();

    bool is_trading_day(const std::string& et_date_ymd) const;
    bool is_half_day(const std::string& et_date_ymd) const;
    int market_close_hour(const std::string& et_date_ymd) const;
    int market_close_minute(const std::string& et_date_ymd) const;

private:
    std::unordered_set<std::string> full_holidays_;
    std::unordered_set<std::string> half_days_;
};

} // namespace sentio
```

**Implementation**:
```cpp
// src/common/nyse_calendar.cpp
#include "common/nyse_calendar.h"

namespace sentio {

NyseCalendar::NyseCalendar() {
    // 2025 NYSE Holidays
    full_holidays_ = {
        "2025-01-01",  // New Year's Day
        "2025-01-20",  // Martin Luther King Jr. Day
        "2025-02-17",  // Presidents' Day
        "2025-04-18",  // Good Friday
        "2025-05-26",  // Memorial Day
        "2025-06-19",  // Juneteenth
        "2025-07-04",  // Independence Day
        "2025-09-01",  // Labor Day
        "2025-11-27",  // Thanksgiving
        "2025-12-25",  // Christmas

        // 2026 (add as needed)
        "2026-01-01",  // New Year's Day
        "2026-01-19",  // MLK Day
        // ... (complete list)

        // 2027
        "2027-01-01",
        // ... (complete list)
    };

    // Half-days (1:00 PM ET close)
    half_days_ = {
        "2025-07-03",  // Day before Independence Day
        "2025-11-28",  // Day after Thanksgiving
        "2025-12-24",  // Christmas Eve

        "2026-07-03",
        "2026-11-27",
        "2026-12-24",

        "2027-07-02",
        "2027-11-26",
        "2027-12-24",
    };
}

bool NyseCalendar::is_trading_day(const std::string& et_date_ymd) const {
    // Check if it's a full holiday
    return full_holidays_.find(et_date_ymd) == full_holidays_.end();
}

bool NyseCalendar::is_half_day(const std::string& et_date_ymd) const {
    return half_days_.find(et_date_ymd) != half_days_.end();
}

int NyseCalendar::market_close_hour(const std::string& et_date_ymd) const {
    return is_half_day(et_date_ymd) ? 13 : 16;  // 1:00 PM or 4:00 PM
}

int NyseCalendar::market_close_minute(const std::string& et_date_ymd) const {
    return 0;
}

} // namespace sentio
```

### 1.3 Startup Catch-Up Logic

**Problem**: If process starts after 4 PM with open positions, no liquidation happens.

**Solution**: Check on startup if previous trading day's EOD was missed

```cpp
// In LiveTrader constructor or init():
void check_startup_eod_catch_up() {
    auto et_tm = et_time_.get_current_et_tm();
    std::string today = format_et_date(et_tm);  // YYYY-MM-DD

    // Get previous trading day
    std::string prev_trading_day = get_previous_trading_day(today);

    // Check if we missed previous day's EOD
    if (!eod_state_.is_eod_complete(prev_trading_day)) {
        // Check if we have open positions
        if (has_open_positions()) {
            log_system("STARTUP CATCH-UP: Missed EOD on " + prev_trading_day +
                      " - liquidating now");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(prev_trading_day);
        }
    }

    // Also check if started outside trading hours TODAY with positions
    if (et_time_.should_liquidate_on_startup(has_open_positions())) {
        log_system("STARTUP: Started outside trading hours with positions - liquidating");
        liquidate_all_positions();
        eod_state_.mark_eod_complete(today);
    }
}

std::string get_previous_trading_day(const std::string& et_date) {
    // Walk back day-by-day until we find a trading day
    auto tm = parse_et_date(et_date);
    for (int i = 1; i <= 10; ++i) {
        // Subtract i days
        std::time_t t = std::mktime(&tm) - (i * 86400);
        std::tm* prev_tm = std::localtime(&t);
        std::string prev_date = format_et_date(*prev_tm);

        // Check if weekday and not holiday
        if (prev_tm->tm_wday >= 1 && prev_tm->tm_wday <= 5) {
            if (nyse_calendar_.is_trading_day(prev_date)) {
                return prev_date;
            }
        }
    }
    return et_date;  // Fallback
}

bool has_open_positions() const {
    return position_book_.get_current_position() != PositionState::NEUTRAL_CASH;
}
```

---

## Phase 2: C++20 Upgrade & Robust Timezone (Medium Priority)

**Timeline**: 1-2 days
**Risk**: MEDIUM → LOW

### 2.1 Upgrade to C++20

**CMakeLists.txt**:
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**Test tzdb availability**:
```cpp
#if __has_include(<chrono>)
  #include <chrono>
  #if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    #define HAS_TZDB 1
  #endif
#endif

#ifndef HAS_TZDB
  #error "C++20 chrono timezone database not available - use Howard Hinnath's date library"
#endif
```

### 2.2 Replace Manual TZ Conversion

**Current (BROKEN)**:
```cpp
// Uses setenv("TZ") which is NOT thread-safe
std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const {
    const char* old_tz = std::getenv("TZ");
    setenv("TZ", timezone_name.c_str(), 1);  // RACE CONDITION
    tzset();

    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm local_tm;
    localtime_r(&time_t_val, &local_tm);

    if (old_tz) {
        setenv("TZ", old_tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    return local_tm;
}
```

**New (C++20)**:
```cpp
#include <chrono>

std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const {
    using namespace std::chrono;

    const auto* tz = locate_zone(timezone_name);
    zoned_time zt{tz, tp};
    auto local_tp = zt.get_local_time();

    // Convert to time_t for std::tm conversion
    auto sys_tp = zt.get_sys_time();
    std::time_t t = system_clock::to_time_t(floor<seconds>(sys_tp));

    std::tm result;
    localtime_r(&t, &result);
    return result;
}
```

### 2.3 Add EodController Class

Your proposed `EodController` design is excellent - integrate after C++20 upgrade.

---

## Phase 3: Observability & Monitoring (Low Priority)

**Timeline**: 1 week
**Risk**: LOW

### 3.1 Structured Logging

Add JSON-formatted logs for EOD events:

```cpp
void log_eod_event(const std::string& event_type, const json& data) {
    json log_entry = {
        {"timestamp_et", et_time_.get_current_et_string()},
        {"event", event_type},
        {"data", data}
    };

    std::ofstream eod_log("logs/eod_events.jsonl", std::ios::app);
    eod_log << log_entry.dump() << "\n";
}

// Usage:
log_eod_event("eod_liquidation", {
    {"trading_date", today_et},
    {"positions_closed", num_positions},
    {"trigger", "scheduled_window"},
    {"window_start_et", "15:55"},
    {"window_end_et", "16:00"}
});
```

### 3.2 Clock Health Monitoring

```cpp
void log_clock_health() {
    auto now = std::chrono::system_clock::now();
    auto et_tm = et_time_.get_current_et_tm();

    json health = {
        {"system_time_utc", to_iso_string(now)},
        {"et_time", et_time_.get_current_et_string()},
        {"tz_database_version", "2024a"},  // From tzdb
        {"is_dst", et_tm.tm_isdst == 1},
        {"ntp_offset_ms", get_ntp_offset()}  // Optional
    };

    log_system("Clock Health: " + health.dump());
}
```

### 3.3 Alerting

- **Missed EOD Alert**: If 4:10 PM ET and EOD not complete → send alert
- **Holiday Trading Alert**: If positions opened on holiday → send alert
- **Clock Drift Alert**: If NTP offset > 100ms → send alert

---

## Implementation Priority

### Must-Have (This Week)
1. ✅ Idempotent EOD state tracking
2. ✅ NYSE holiday calendar (2025-2027)
3. ✅ Startup catch-up logic

### Should-Have (Next Week)
4. C++20 upgrade
5. Thread-safe timezone handling
6. EodController refactor
7. Unit tests

### Nice-to-Have (Future)
8. Structured logging
9. Clock health monitoring
10. NTP validation
11. JSON holiday calendar (10+ years)

---

## Testing Strategy

### Unit Tests
```cpp
TEST(EodStateStore, IdempotentSameDay) {
    EodStateStore store("/tmp/test_eod.txt");
    store.mark_eod_complete("2025-10-08");
    EXPECT_TRUE(store.is_eod_complete("2025-10-08"));
    EXPECT_FALSE(store.is_eod_complete("2025-10-09"));
}

TEST(NyseCalendar, ThanksgivingHoliday) {
    NyseCalendar cal;
    EXPECT_FALSE(cal.is_trading_day("2025-11-27"));  // Thanksgiving
    EXPECT_TRUE(cal.is_half_day("2025-11-28"));       // Black Friday
}

TEST(LiveTrader, NoDoubleLiquidation) {
    // Simulate EOD trigger twice in same day
    // Assert liquidate_all_positions() called only once
}
```

### Integration Tests
1. **Startup at 10 PM with positions** → immediate liquidation
2. **Process restart during EOD window** → no double liquidation
3. **Holiday trading attempt** → positions rejected
4. **Half-day (1 PM close)** → EOD at 12:55-13:00

---

## Risk Mitigation

### What Could Still Go Wrong?

1. **File system failure** → EOD state file not writable
   - Mitigation: Fallback to in-memory state + loud alert

2. **Emergency market closure** (e.g., 9/11-style event)
   - Mitigation: Manual override command to force liquidate

3. **Extended hours trading** (if added later)
   - Mitigation: Separate EOD window for extended hours

4. **Leap second** (rare but possible)
   - Mitigation: C++20 chrono handles this correctly

---

## Appendix: File Checklist

### New Files to Create
- `include/common/eod_state.h`
- `src/common/eod_state.cpp`
- `include/common/nyse_calendar.h`
- `src/common/nyse_calendar.cpp`
- `tests/test_eod_state.cpp` (future)
- `tests/test_nyse_calendar.cpp` (future)

### Files to Modify
- `src/cli/live_trade_command.cpp` - integrate EodStateStore
- `include/common/time_utils.h` - add calendar integration
- `CMakeLists.txt` - add new source files
- `CMakeLists.txt` - upgrade to C++20 (Phase 2)

---

## Approval & Sign-off

- [ ] Phase 1 implementation reviewed
- [ ] Integration tests passing
- [ ] Deployed to paper trading environment
- [ ] Monitored for 1 week without issues
- [ ] Approved for live trading

**Reviewer**: _______________
**Date**: _______________
