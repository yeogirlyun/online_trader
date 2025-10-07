# Production Hardening Implementation Plan

**Date**: 2025-10-08
**Status**: Implementation Ready
**Based on**: User's production-grade code review + PRODUCTION_READINESS_CODE_REVIEW.md

---

## Executive Summary

This document provides **line-by-line implementation guidance** for hardening the OnlineTrader live trading system for real capital deployment. All patches are drop-in ready with concrete code snippets.

**Critical Areas**:
1. Exception-safe trading loop with watchdog
2. Order reconciliation & position truth
3. Time/session correctness (no hardcoded EDT)
4. Feature engine hygiene (NaN guards, schema validation)
5. Predictor numerical stability
6. Config-driven risk parameters
7. Circuit breakers & kill-switches
8. Observability (metrics, structured logs)
9. Comprehensive test suite
10. High-leverage cleanups

**Timeline**: 5 days (40 hours) for P0+P1 items
**Risk Reduction**: Eliminates 6 P0 and 6 P1 issues from production readiness review

---

## 1. Exception-Safe Trading Loop with Watchdog (P0-1)

### Current Problem
```cpp
// src/cli/live_trade_command.cpp:330-378
void on_new_bar(const Bar& bar) {
    strategy_.on_bar(bar);  // Could throw
    auto signal = generate_signal(bar);  // Could throw
    // NO TRY-CATCH → crash on any exception
}
```

**Impact**: Single exception tears down entire trading system

### Fix Implementation

**Step 1**: Create exception types and utilities

File: `include/common/exceptions.h` (NEW)
```cpp
#pragma once
#include <stdexcept>
#include <string>

namespace sentio {

// Transient errors (reconnect/retry)
class TransientError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class FeedDisconnectError : public TransientError {
public:
    using TransientError::TransientError;
};

class BrokerApiError : public TransientError {
public:
    int status_code;
    BrokerApiError(const std::string& msg, int code)
        : TransientError(msg), status_code(code) {}
};

// Fatal errors (flatten + exit)
class FatalTradingError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class PositionReconciliationError : public FatalTradingError {
public:
    using FatalTradingError::FatalTradingError;
};

} // namespace sentio
```

**Step 2**: Add bounded backoff helper

File: `include/common/backoff.h` (NEW)
```cpp
#pragma once
#include <chrono>
#include <thread>
#include <algorithm>

namespace sentio {

class ExponentialBackoff {
public:
    ExponentialBackoff(std::chrono::milliseconds initial = std::chrono::milliseconds{250},
                       std::chrono::seconds max = std::chrono::seconds{15})
        : current_(initial), max_(max) {}

    void sleep() {
        std::this_thread::sleep_for(
            std::min(current_, std::chrono::duration_cast<std::chrono::milliseconds>(max_))
        );
        current_ *= 2;
    }

    void reset() {
        current_ = std::chrono::milliseconds{250};
    }

private:
    std::chrono::milliseconds current_;
    std::chrono::seconds max_;
};

} // namespace sentio
```

**Step 3**: Rewrite main trading loop

File: `src/cli/live_trade_command.cpp:330-378` (REPLACE)
```cpp
void LiveTradeCommand::run_trading_loop() {
    using clock = std::chrono::steady_clock;
    constexpr auto TICK_BUDGET = std::chrono::milliseconds{400};  // 1min bars
    constexpr auto WATCHDOG_TIMEOUT = std::chrono::minutes{3};

    ExponentialBackoff backoff;
    auto last_heartbeat = clock::now();

    log_system("Starting exception-safe trading loop with watchdog");

    while (running_) {
        try {
            // Check for panic file (operator kill-switch)
            if (std::filesystem::exists("/tmp/sentio_panic")) {
                log_error("Panic file detected - initiating emergency flatten");
                throw FatalTradingError("Operator panic triggered");
            }

            // Normal bar processing
            auto t0 = clock::now();

            // This will be called by WebSocket callback
            // For now, wait for next bar (blocks until new bar arrives)
            if (!has_new_bar_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            Bar bar = get_next_bar();  // Thread-safe queue pop

            // Validate bar before processing
            if (!is_valid_bar(bar)) {
                log_warn("Invalid bar dropped: " + bar.to_string());
                continue;
            }

            // Process bar: ingest → predict → decide → act
            on_new_bar(bar);

            // Update heartbeat and reset backoff
            last_heartbeat = clock::now();
            backoff.reset();

            // Check if we exceeded tick budget
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now() - t0
            );
            if (elapsed > TICK_BUDGET) {
                log_warn("Tick over budget: " + std::to_string(elapsed.count()) + "ms");
            }

        } catch (const TransientError& e) {
            log_warn("Transient error (will retry): " + std::string(e.what()));
            backoff.sleep();

        } catch (const FatalTradingError& e) {
            log_error("FATAL ERROR: " + std::string(e.what()));
            panic_flatten();
            return;  // Exit clean for supervisor restart

        } catch (const std::exception& e) {
            log_error("UNHANDLED EXCEPTION: " + std::string(e.what()));
            panic_flatten();
            return;
        }

        // Watchdog: no progress in 3 minutes → panic
        if (clock::now() - last_heartbeat > WATCHDOG_TIMEOUT) {
            log_error("WATCHDOG TIMEOUT: No heartbeat in 3 minutes");
            panic_flatten();
            return;
        }
    }

    log_system("Trading loop exited cleanly");
}

// Panic flatten: emergency position close
void LiveTradeCommand::panic_flatten() {
    log_error("=== PANIC FLATTEN INITIATED ===");

    try {
        // Close all positions immediately
        auto positions = alpaca_client_->get_positions();
        for (const auto& pos : positions) {
            log_system("Emergency close: " + pos.symbol + " (" +
                      std::to_string(pos.qty) + " shares)");
            alpaca_client_->close_position(pos.symbol);
        }

        // Cancel all open orders
        alpaca_client_->cancel_all_orders();

        log_system("=== PANIC FLATTEN COMPLETE ===");

    } catch (const std::exception& e) {
        log_error("PANIC FLATTEN FAILED: " + std::string(e.what()));
        // Best effort - log and exit anyway
    }
}
```

**Step 4**: Add bar validation

File: `include/common/bar_validator.h` (NEW)
```cpp
#pragma once
#include "common/types.h"
#include <cmath>

namespace sentio {

inline bool is_valid_bar(const Bar& bar) {
    // Price sanity checks
    if (!std::isfinite(bar.open) || !std::isfinite(bar.high) ||
        !std::isfinite(bar.low) || !std::isfinite(bar.close)) {
        return false;
    }

    // Volume sanity
    if (!std::isfinite(bar.volume) || bar.volume < 0) {
        return false;
    }

    // OHLC relationships
    if (!(bar.high >= bar.low)) return false;
    if (!(bar.high >= bar.open && bar.high >= bar.close)) return false;
    if (!(bar.low <= bar.open && bar.low <= bar.close)) return false;

    // Reasonable price ranges (reject obvious errors)
    if (bar.high <= 0 || bar.low <= 0) return false;
    if (bar.high / bar.low > 1.5) return false;  // >50% intrabar move = suspicious

    return true;
}

} // namespace sentio
```

---

## 2. Order Reconciliation & Position Truth (P0-3, P0-4)

### Current Problem
```cpp
// src/cli/live_trade_command.cpp:542-545
void close_position(const std::string& symbol) {
    alpaca_client_->close_position(symbol);
    // NO VALIDATION - silent failure possible
}
```

**Impact**: Position drift, unrealized P&L errors, silent trade failures

### Fix Implementation

**Step 1**: Create position book with reconciliation

File: `include/live/position_book.h` (NEW)
```cpp
#pragma once
#include "common/types.h"
#include <map>
#include <string>
#include <optional>

namespace sentio {

struct Position {
    std::string symbol;
    int64_t qty{0};
    double avg_entry_price{0.0};
    double unrealized_pnl{0.0};
    double current_price{0.0};

    bool is_flat() const { return qty == 0; }
};

struct ExecutionReport {
    std::string order_id;
    std::string client_order_id;
    std::string symbol;
    std::string side;  // "buy" or "sell"
    int64_t filled_qty{0};
    double avg_fill_price{0.0};
    std::string status;  // "filled", "partial_fill", "pending", etc.
    uint64_t timestamp{0};
};

struct ReconcileResult {
    double realized_pnl{0.0};
    int64_t filled_qty{0};
    bool flat{false};
    std::string status;
};

class PositionBook {
public:
    // Update position from execution report
    void on_execution(const ExecutionReport& exec);

    // Get current position
    Position get_position(const std::string& symbol) const;

    // Reconcile against broker truth
    void reconcile_with_broker(const std::vector<Position>& broker_positions);

    // Get all positions
    std::map<std::string, Position> get_all_positions() const;

    // Get realized P&L since timestamp
    double get_realized_pnl_since(uint64_t since_ts) const;

private:
    std::map<std::string, Position> positions_;
    std::vector<ExecutionReport> execution_history_;
    double total_realized_pnl_{0.0};

    void update_position_on_fill(const ExecutionReport& exec);
    double calculate_realized_pnl(const Position& old_pos,
                                   const ExecutionReport& exec);
};

} // namespace sentio
```

File: `src/live/position_book.cpp` (NEW)
```cpp
#include "live/position_book.h"
#include "common/logging.h"
#include <cmath>

namespace sentio {

void PositionBook::on_execution(const ExecutionReport& exec) {
    execution_history_.push_back(exec);

    if (exec.filled_qty == 0) {
        return;  // No fill, nothing to update
    }

    auto& pos = positions_[exec.symbol];

    // Calculate realized P&L if reducing position
    double realized_pnl = calculate_realized_pnl(pos, exec);
    total_realized_pnl_ += realized_pnl;

    // Update position
    update_position_on_fill(exec);

    log_info("Position update: " + exec.symbol +
             " qty=" + std::to_string(pos.qty) +
             " avg_px=" + std::to_string(pos.avg_entry_price) +
             " realized_pnl=" + std::to_string(realized_pnl));
}

void PositionBook::update_position_on_fill(const ExecutionReport& exec) {
    auto& pos = positions_[exec.symbol];

    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") fill_qty = -fill_qty;

    int64_t new_qty = pos.qty + fill_qty;

    if (pos.qty == 0) {
        // Opening new position
        pos.avg_entry_price = exec.avg_fill_price;
    } else if ((pos.qty > 0 && fill_qty > 0) || (pos.qty < 0 && fill_qty < 0)) {
        // Adding to position - update avg entry price
        double total_cost = pos.qty * pos.avg_entry_price +
                           fill_qty * exec.avg_fill_price;
        pos.avg_entry_price = total_cost / new_qty;
    }
    // If reducing/reversing, keep old avg_entry_price for P&L calc

    pos.qty = new_qty;
    pos.symbol = exec.symbol;

    if (pos.qty == 0) {
        pos.avg_entry_price = 0.0;
    }
}

double PositionBook::calculate_realized_pnl(const Position& old_pos,
                                            const ExecutionReport& exec) {
    if (old_pos.qty == 0) return 0.0;

    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") fill_qty = -fill_qty;

    // Only calculate P&L if reducing position
    if ((old_pos.qty > 0 && fill_qty >= 0) || (old_pos.qty < 0 && fill_qty <= 0)) {
        return 0.0;  // Adding to position
    }

    int64_t closed_qty = std::min(std::abs(fill_qty), std::abs(old_pos.qty));
    double pnl_per_share = exec.avg_fill_price - old_pos.avg_entry_price;

    if (old_pos.qty < 0) {
        pnl_per_share = -pnl_per_share;  // Short position
    }

    return closed_qty * pnl_per_share;
}

Position PositionBook::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return Position{.symbol = symbol};
    }
    return it->second;
}

void PositionBook::reconcile_with_broker(const std::vector<Position>& broker_positions) {
    log_info("=== Position Reconciliation ===");

    std::map<std::string, Position> broker_map;
    for (const auto& bp : broker_positions) {
        broker_map[bp.symbol] = bp;
    }

    // Check for discrepancies
    bool has_drift = false;
    for (const auto& [symbol, local_pos] : positions_) {
        auto bit = broker_map.find(symbol);

        if (bit == broker_map.end()) {
            if (local_pos.qty != 0) {
                log_error("DRIFT: Local has " + symbol + " (" +
                         std::to_string(local_pos.qty) + "), broker has 0");
                has_drift = true;
            }
        } else {
            const auto& broker_pos = bit->second;
            if (local_pos.qty != broker_pos.qty) {
                log_error("DRIFT: " + symbol +
                         " local=" + std::to_string(local_pos.qty) +
                         " broker=" + std::to_string(broker_pos.qty));
                has_drift = true;
            }
        }
    }

    // Check for positions broker has but we don't
    for (const auto& [symbol, broker_pos] : broker_map) {
        if (positions_.find(symbol) == positions_.end() && broker_pos.qty != 0) {
            log_error("DRIFT: Broker has " + symbol + " (" +
                     std::to_string(broker_pos.qty) + "), local has none");
            has_drift = true;
        }
    }

    if (has_drift) {
        log_error("=== POSITION DRIFT DETECTED ===");
        // In production, this should trigger panic_flatten()
        throw PositionReconciliationError("Position drift detected");
    } else {
        log_info("Position reconciliation: OK");
    }
}

double PositionBook::get_realized_pnl_since(uint64_t since_ts) const {
    double pnl = 0.0;
    for (const auto& exec : execution_history_) {
        if (exec.timestamp >= since_ts && exec.status == "filled") {
            // Recalculate P&L for this execution
            // (This is simplified - in production, track per-exec P&L)
            pnl += 0.0;  // TODO: Store per-exec P&L in execution_history_
        }
    }
    return pnl;
}

std::map<std::string, Position> PositionBook::get_all_positions() const {
    return positions_;
}

} // namespace sentio
```

**Step 2**: Implement reconciled flatten

File: `src/cli/live_trade_command.cpp` (ADD METHOD)
```cpp
ReconcileResult LiveTradeCommand::flatten_symbol_with_reconciliation(
    const std::string& symbol) {

    const auto local_pos = position_book_.get_position(symbol);

    if (local_pos.is_flat()) {
        log_info("Position already flat: " + symbol);
        return {.flat = true, .status = "already_flat"};
    }

    const std::string side = local_pos.qty > 0 ? "sell" : "buy";
    const int qty = std::abs(local_pos.qty);

    // Generate unique client order ID for tracking
    const std::string client_order_id = "FLAT-" + symbol + "-" +
                                        std::to_string(current_bar_id_);

    log_system("Flattening " + symbol + ": " + side + " " +
               std::to_string(qty) + " shares");

    // Place market order
    auto order_response = alpaca_client_->place_order(
        symbol, side, qty, "market", 0.0, client_order_id
    );

    // Wait for fill confirmation (bounded timeout)
    const auto deadline = std::chrono::steady_clock::now() +
                         std::chrono::seconds{5};

    while (std::chrono::steady_clock::now() < deadline) {
        // Poll order status
        auto order_status = alpaca_client_->get_order_status(order_response.order_id);

        if (order_status.status == "filled") {
            // Update position book
            ExecutionReport exec{
                .order_id = order_response.order_id,
                .client_order_id = client_order_id,
                .symbol = symbol,
                .side = side,
                .filled_qty = order_status.filled_qty,
                .avg_fill_price = order_status.avg_fill_price,
                .status = "filled",
                .timestamp = current_timestamp_
            };
            position_book_.on_execution(exec);

            const auto final_pos = position_book_.get_position(symbol);
            if (!final_pos.is_flat()) {
                throw PositionReconciliationError(
                    "Position not flat after fill: " + symbol
                );
            }

            log_system("✓ Position flattened: " + symbol);
            return {
                .realized_pnl = exec.avg_fill_price * qty,  // Simplified
                .filled_qty = qty,
                .flat = true,
                .status = "success"
            };
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }

    // Timeout - position may be partially filled
    log_error("Flatten timeout: " + symbol);
    throw PositionReconciliationError("Flatten timeout: " + symbol);
}

// Add periodic reconciliation to main loop
void LiveTradeCommand::run_trading_loop() {
    // ... existing loop code ...

    // Every 60 bars (60 minutes), reconcile positions
    if (current_bar_id_ % 60 == 0) {
        try {
            auto broker_positions = alpaca_client_->get_positions();
            position_book_.reconcile_with_broker(broker_positions);
        } catch (const PositionReconciliationError& e) {
            log_error("Reconciliation failed: " + std::string(e.what()));
            panic_flatten();
            throw;
        }
    }
}
```

---

## 3. Time & Session Correctness (P0-6, P1-3)

### Current Problem
```cpp
// src/cli/live_trade_command.cpp:203
const auto ny_time = datetime("America/New_York");  // HARDCODED
const int hour = ny_time.tm_hour;
const int minute = ny_time.tm_min;

// WRONG: Assumes EDT, breaks during DST transitions
if (hour < 9 || hour >= 16) return false;
```

**Impact**: Trading during wrong hours, DST bugs, timezone errors

### Fix Implementation

**Step 1**: Add proper timezone support (requires `date` library)

File: `CMakeLists.txt` (ADD)
```cmake
# Find date library for timezone support
find_package(date QUIET)
if(date_FOUND)
    message(STATUS "date library found - timezone support enabled")
    target_link_libraries(online_common PUBLIC date::date date::date-tz)
    add_compile_definitions(TIMEZONE_SUPPORT_ENABLED)
else()
    message(WARNING "date library not found - install with: brew install howard-hinnant-date")
endif()
```

File: `include/common/time_utils.h` (NEW)
```cpp
#pragma once
#include <chrono>
#include <string>

#ifdef TIMEZONE_SUPPORT_ENABLED
#include <date/tz.h>
#endif

namespace sentio {

struct TradingSession {
    std::string timezone;  // IANA timezone (e.g., "America/New_York")
    int market_open_hour{9};
    int market_open_minute{30};
    int market_close_hour{16};
    int market_close_minute{0};

#ifdef TIMEZONE_SUPPORT_ENABLED
    const date::time_zone* tz{nullptr};

    TradingSession(const std::string& tz_name = "America/New_York")
        : timezone(tz_name) {
        try {
            tz = date::locate_zone(tz_name);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid timezone: " + tz_name);
        }
    }

    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
        auto zoned = date::make_zoned(tz, tp);
        auto local = zoned.get_local_time();
        auto dp = date::floor<date::days>(local);
        auto time = date::make_time(local - dp);

        int hour = time.hours().count();
        int minute = time.minutes().count();

        // Check if within trading hours
        int open_mins = market_open_hour * 60 + market_open_minute;
        int close_mins = market_close_hour * 60 + market_close_minute;
        int current_mins = hour * 60 + minute;

        return current_mins >= open_mins && current_mins < close_mins;
    }

    bool is_weekday(const std::chrono::system_clock::time_point& tp) const {
        auto zoned = date::make_zoned(tz, tp);
        auto ymd = date::year_month_day{date::floor<date::days>(zoned.get_local_time())};
        auto weekday = date::weekday{ymd};

        return weekday >= date::Monday && weekday <= date::Friday;
    }

    bool is_trading_day(const std::chrono::system_clock::time_point& tp) const {
        // TODO: Add holiday calendar check
        return is_weekday(tp);
    }
#else
    // Fallback to UTC-based check (less accurate)
    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
        auto tt = std::chrono::system_clock::to_time_t(tp);
        auto tm = *std::gmtime(&tt);

        // Very rough EDT approximation (WRONG during DST!)
        int hour = (tm.tm_hour - 4 + 24) % 24;
        return hour >= market_open_hour && hour < market_close_hour;
    }
#endif
};

} // namespace sentio
```

**Step 2**: Replace hardcoded checks

File: `src/cli/live_trade_command.cpp` (REPLACE)
```cpp
// Add member variable
class LiveTradeCommand {
    TradingSession session_{"America/New_York"};  // Configurable
    // ...
};

// Replace is_regular_hours()
bool LiveTradeCommand::is_regular_hours() const {
    auto now = std::chrono::system_clock::now();

    if (!session_.is_trading_day(now)) {
        return false;  // Weekend or holiday
    }

    return session_.is_regular_hours(now);
}

// In main loop, allow after-hours learning but gate trading
void LiveTradeCommand::on_new_bar(const Bar& bar) {
    // ALWAYS feed to strategy for learning (even after-hours)
    strategy_.on_bar(bar);

    // Train predictor on all bars
    if (previous_bar_.has_value()) {
        auto features = strategy_.extract_features(*previous_bar_);
        if (!features.empty()) {
            double return_1bar = (bar.close - previous_bar_->close) /
                                previous_bar_->close;
            strategy_.train_predictor(features, return_1bar);
        }
    }
    previous_bar_ = bar;

    // ONLY trade during regular hours
    if (!is_regular_hours()) {
        log_debug("Outside trading hours - learning only, no trades");
        return;
    }

    // Generate signal and execute trades
    auto signal = generate_signal(bar);
    execute_signal(signal, bar);
}
```

This addresses **P1-1 (After-hours learning gap)** from the production review!

---

## 4. Feature Engine Hygiene (P1-4)

### Current Problem
- No NaN guards
- No schema versioning
- No reproducibility guarantees

### Fix Implementation

File: `include/features/feature_schema.h` (NEW)
```cpp
#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace sentio {

struct FeatureSchema {
    std::vector<std::string> feature_names;
    int version{1};
    std::string hash;  // SHA256 of names + params

    std::string compute_hash() const {
        // Simplified hash (use proper SHA256 in production)
        std::stringstream ss;
        for (const auto& name : feature_names) {
            ss << name << "|";
        }
        ss << "v" << version;

        // Return first 16 chars of hex digest (placeholder)
        std::string s = ss.str();
        std::hash<std::string> hasher;
        size_t h = hasher(s);

        std::stringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << h;
        return hex.str();
    }

    void finalize() {
        hash = compute_hash();
    }
};

struct FeatureSnapshot {
    uint64_t timestamp{0};
    uint64_t bar_id{0};
    std::vector<double> features;
    FeatureSchema schema;

    bool is_valid() const {
        return features.size() == schema.feature_names.size();
    }
};

// NaN guard: replace NaN/Inf with 0
inline void nan_guard(std::vector<double>& features) {
    for (auto& f : features) {
        if (!std::isfinite(f)) {
            f = 0.0;
        }
    }
}

// Clamp extreme values
inline void clamp_features(std::vector<double>& features,
                          double min_val = -1e6,
                          double max_val = 1e6) {
    for (auto& f : features) {
        f = std::clamp(f, min_val, max_val);
    }
}

} // namespace sentio
```

File: `src/features/unified_feature_engine.cpp` (MODIFY extract_features())
```cpp
std::vector<double> UnifiedFeatureEngine::extract_features(
    const std::string& symbol,
    uint64_t bar_id) const {

    if (!is_ready()) {
        return {};
    }

    std::vector<double> features;
    features.reserve(126);  // Known size

    // ... existing feature extraction code ...

    // CRITICAL: Apply NaN guards before returning
    nan_guard(features);
    clamp_features(features);

    // Validate schema match
    if (features.size() != feature_schema_.feature_names.size()) {
        throw std::runtime_error(
            "Feature size mismatch: got " + std::to_string(features.size()) +
            ", expected " + std::to_string(feature_schema_.feature_names.size())
        );
    }

    return features;
}

// Add schema initialization in constructor
UnifiedFeatureEngine::UnifiedFeatureEngine(const Config& config) {
    // ... existing initialization ...

    // Initialize feature schema
    feature_schema_.version = 3;  // Bump on any structural change
    feature_schema_.feature_names = {
        "returns_1", "returns_2", "returns_3", "returns_5", "returns_10",
        "sma_5", "sma_10", "sma_20", "sma_50",
        "ema_5", "ema_10", "ema_20",
        "bb_upper", "bb_middle", "bb_lower", "bb_width",
        "rsi_14", "macd", "macd_signal", "macd_hist",
        // ... all 126 features ...
    };
    feature_schema_.finalize();

    log_info("Feature schema: v" + std::to_string(feature_schema_.version) +
             " hash=" + feature_schema_.hash);
}
```

---

## 5. Predictor Numerical Stability (P2-4)

### Current Problem
```cpp
// EWRLS can produce unbounded predictions
double predicted_return = dot_product(weights, features);
// No clamping, no confidence measure
```

### Fix Implementation

File: `src/learning/online_predictor.cpp` (MODIFY predict())
```cpp
std::pair<double, double> OnlinePredictor::predict(
    const std::vector<double>& features) const {

    if (features.empty() || weights_.size() != features.size()) {
        return {0.0, 0.0};  // No prediction
    }

    // Standardize features (frozen μ/σ from warmup)
    std::vector<double> z_features = features;
    for (size_t i = 0; i < z_features.size(); ++i) {
        double z = (z_features[i] - feature_means_[i]) /
                   (feature_stds_[i] + 1e-6);

        // Clamp extreme z-scores (beyond ±8 sigma is noise)
        z_features[i] = std::clamp(z, -8.0, 8.0);

        // Final NaN guard
        if (!std::isfinite(z_features[i])) {
            z_features[i] = 0.0;
        }
    }

    // Compute prediction
    double raw_prediction = 0.0;
    for (size_t i = 0; i < weights_.size(); ++i) {
        raw_prediction += weights_[i] * z_features[i];
    }

    // Clamp prediction to reasonable return range
    // SPY rarely moves >5% in one bar
    double prediction = std::clamp(raw_prediction, -0.05, 0.05);

    // Compute confidence from weight entropy or recent error
    double confidence = compute_confidence();

    return {prediction, confidence};
}

double OnlinePredictor::compute_confidence() const {
    // Method 1: Weight magnitude (higher = more confident)
    double weight_norm = 0.0;
    for (double w : weights_) {
        weight_norm += w * w;
    }
    weight_norm = std::sqrt(weight_norm);

    // Normalize to [0, 1] using sigmoid
    double conf = 1.0 / (1.0 + std::exp(-weight_norm / 10.0));

    // Method 2: Recent prediction error (lower = more confident)
    // TODO: Track rolling prediction error

    return std::clamp(conf, 0.0, 1.0);
}
```

---

## 6. Circuit Breakers & Kill-Switches (P0-NEW)

### Implementation

File: `include/risk/circuit_breaker.h` (NEW)
```cpp
#pragma once
#include <chrono>

namespace sentio {

struct CircuitBreakerConfig {
    double daily_loss_limit_bps{150};  // -1.5% max daily loss
    int max_open_positions{4};
    double max_leverage{1.2};
    int halt_duration_minutes{45};
};

class CircuitBreaker {
public:
    CircuitBreaker(const CircuitBreakerConfig& config, double starting_equity)
        : config_(config), starting_equity_(starting_equity) {}

    void on_realized_pnl(double pnl) {
        daily_realized_pnl_ += pnl;

        double pnl_bps = (daily_realized_pnl_ / starting_equity_) * 10000;

        if (pnl_bps < -config_.daily_loss_limit_bps) {
            trip("Daily loss limit exceeded: " +
                 std::to_string(pnl_bps) + " bps");
        }
    }

    void check_position_count(int count) {
        if (count > config_.max_open_positions) {
            trip("Too many open positions: " + std::to_string(count));
        }
    }

    bool is_halted() const {
        if (!tripped_) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - trip_time_
        );

        return elapsed < std::chrono::minutes{config_.halt_duration_minutes};
    }

    void reset_daily() {
        daily_realized_pnl_ = 0.0;
        tripped_ = false;
    }

private:
    CircuitBreakerConfig config_;
    double starting_equity_;
    double daily_realized_pnl_{0.0};
    bool tripped_{false};
    std::chrono::steady_clock::time_point trip_time_;
    std::string trip_reason_;

    void trip(const std::string& reason) {
        tripped_ = true;
        trip_time_ = std::chrono::steady_clock::now();
        trip_reason_ = reason;
        // Log to alerts/Slack in production
    }
};

} // namespace sentio
```

**Operator Kill-Switch**: Already implemented in main loop as panic file check

---

## 7. Structured Logging & Metrics (P1-5)

### Implementation

File: `include/common/structured_log.h` (NEW)
```cpp
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <fstream>

namespace sentio {

class StructuredLogger {
public:
    void log_bar_processed(uint64_t bar_id, const std::string& symbol,
                          double prob, double conf, const std::string& signal_type,
                          bool fe_ready, int latency_ms) {
        std::stringstream ss;
        ss << "{"
           << "\"ts\":\"" << get_iso_timestamp() << "\","
           << "\"bar_id\":" << bar_id << ","
           << "\"symbol\":\"" << symbol << "\","
           << "\"prob\":" << std::fixed << std::setprecision(4) << prob << ","
           << "\"conf\":" << conf << ","
           << "\"signal\":\"" << signal_type << "\","
           << "\"fe_ready\":" << (fe_ready ? "true" : "false") << ","
           << "\"lat_ms\":" << latency_ms
           << "}\n";

        append_to_log(ss.str());
    }

private:
    std::string log_file_{"logs/structured/bars.jsonl"};

    std::string get_iso_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    void append_to_log(const std::string& line) {
        std::ofstream ofs(log_file_, std::ios::app);
        ofs << line;
    }
};

} // namespace sentio
```

---

## Implementation Timeline

### Phase 1 (8 hours) - Critical Safety
- [ ] Exception-safe trading loop with watchdog
- [ ] Panic flatten + operator kill-switch
- [ ] Bar validation
- [ ] Circuit breaker (daily loss limit)

### Phase 2 (16 hours) - Position Truth
- [ ] Position book implementation
- [ ] Order reconciliation
- [ ] Periodic broker reconciliation
- [ ] Fill validation and timeout handling

### Phase 3 (8 hours) - Time & Features
- [ ] Timezone support (date library)
- [ ] Session gates (RTH only for trades)
- [ ] After-hours learning
- [ ] Feature schema + NaN guards

### Phase 4 (8 hours) - Observability
- [ ] Structured logging (JSON lines)
- [ ] Metrics collection
- [ ] Test suite (bar validation, reconciliation, circuit breaker)

**Total: 40 hours (5 days)**

---

## Testing Requirements

All fixes must include:

1. **Unit tests** (Catch2/GTest)
   - Bar validation fuzzer
   - Position reconciliation scenarios
   - Circuit breaker trip conditions

2. **Integration tests**
   - Full warmup → live bar → reconcile cycle
   - Panic flatten under various position states
   - Watchdog timeout recovery

3. **Paper trading validation**
   - 24-hour run with all fixes
   - Verify no exceptions, no drift
   - Metrics dashboard review

---

## Next Steps

**User Decision Required**:
1. Which phase to implement first? (Recommend: Phase 1 critical safety)
2. Test framework preference? (Catch2 vs GTest)
3. Metrics backend? (Prometheus, InfluxDB, or simple file-based)
4. Deploy timeline? (When to cut over from v1.1 to v2.0 hardened?)

Ready to implement - just specify priority order and I'll start with concrete code patches.
