# Expert AI Feedback Implementation Plan
**Version**: v2.7.0 (Target)
**Date**: 2025-10-15
**Status**: Implementation Plan
**Based On**: Expert AI Model Review of v2.6.0

---

## Executive Summary

Expert AI feedback identified critical improvements across **5 key areas**: Risk Management, Architecture, Production Readiness, Performance, and Testing. This document provides a prioritized implementation plan with detailed technical specifications.

**Implementation Scope**: All recommendations EXCEPT fallback strategy (as requested by user)

---

## Priority Matrix

| Priority | Category | Item | Impact | Effort | Timeline |
|----------|----------|------|--------|--------|----------|
| 🔴 P0 | Risk | Volatility-Adjusted Position Sizing | High | Medium | Week 1 |
| 🔴 P0 | Production | Data Quality Validation | High | Low | Week 1 |
| 🔴 P0 | Production | Monitoring & Alerting | High | Medium | Week 1 |
| 🟡 P1 | Architecture | Transaction Cost Modeling | High | Low | Week 2 |
| 🟡 P1 | Risk | Capital Tracking Refactor | Medium | High | Week 2 |
| 🟡 P1 | Performance | Feature Caching | Medium | Medium | Week 2 |
| 🟢 P2 | Architecture | Feature Selection | Medium | High | Week 3 |
| 🟢 P2 | Architecture | Adaptive Horizon Weighting | Medium | Medium | Week 3 |
| 🟢 P2 | Risk | Learning State Queue | Low | Medium | Week 3 |
| 🟢 P2 | Performance | Reduce Warmup Data | Low | Low | Week 3 |
| 🟢 P2 | Testing | Unit Tests | Medium | High | Week 4 |
| 🟢 P2 | Testing | Stress Tests | Medium | Medium | Week 4 |

---

## 1. Critical Risk Management Improvements (P0)

### 1.1 Volatility-Adjusted Position Sizing

**Current Issue**:
```cpp
// src/backend/rotation_trading_backend.cpp
double fixed_allocation = (current_equity * 0.95) / max_positions;
// PROBLEM: All symbols get same allocation regardless of volatility
```

**Expert Recommendation**: Scale position size inversely with volatility

**Implementation Plan**:

**Step 1**: Add volatility calculation to feature engine
```cpp
// include/features/unified_feature_engine.h
class UnifiedFeatureEngine {
public:
    double get_realized_volatility(int lookback = 20) const;
    double get_annualized_volatility() const;

private:
    RollingStatistics<double> returns_std_{20};  // 20-bar rolling std dev
};
```

**Step 2**: Implement in feature engine
```cpp
// src/features/unified_feature_engine.cpp
double UnifiedFeatureEngine::get_realized_volatility(int lookback) const {
    if (returns_std_.size() < lookback) return 0.0;

    // Calculate standard deviation of returns
    double sum_sq = 0.0;
    double mean = 0.0;
    int count = 0;

    for (const auto& ret : recent_returns_) {
        if (count >= lookback) break;
        mean += ret;
        count++;
    }
    mean /= count;

    count = 0;
    for (const auto& ret : recent_returns_) {
        if (count >= lookback) break;
        sum_sq += (ret - mean) * (ret - mean);
        count++;
    }

    return std::sqrt(sum_sq / (count - 1));
}

double UnifiedFeatureEngine::get_annualized_volatility() const {
    double daily_vol = get_realized_volatility(20);
    return daily_vol * std::sqrt(252);  // Annualize (252 trading days)
}
```

**Step 3**: Update position sizing in backend
```cpp
// include/backend/rotation_trading_backend.h
class RotationTradingBackend {
private:
    struct VolatilityProfile {
        double current_vol;
        double baseline_vol;     // Portfolio average
        double vol_scalar;       // Inverse scaling factor
        std::chrono::system_clock::time_point last_update;
    };

    std::map<std::string, VolatilityProfile> volatility_profiles_;

    void update_volatility_profiles();
    double calculate_risk_adjusted_size(const std::string& symbol, double base_size);
};
```

**Step 4**: Implementation in backend
```cpp
// src/backend/rotation_trading_backend.cpp
void RotationTradingBackend::update_volatility_profiles() {
    // Calculate baseline (average) volatility across all symbols
    double total_vol = 0.0;
    int count = 0;

    for (const auto& [symbol, oes] : oes_managers_) {
        double vol = oes->get_feature_engine()->get_realized_volatility(20);
        if (vol > 0.0) {
            volatility_profiles_[symbol].current_vol = vol;
            total_vol += vol;
            count++;
        }
    }

    double baseline = (count > 0) ? total_vol / count : 0.02;  // Default 2%

    // Calculate scalars for each symbol
    for (auto& [symbol, profile] : volatility_profiles_) {
        profile.baseline_vol = baseline;
        // Inverse scaling: high vol = smaller position
        profile.vol_scalar = baseline / std::max(profile.current_vol, 0.005);
        // Cap scaling between 0.5x and 2.0x
        profile.vol_scalar = std::clamp(profile.vol_scalar, 0.5, 2.0);
        profile.last_update = std::chrono::system_clock::now();
    }
}

double RotationTradingBackend::calculate_risk_adjusted_size(
    const std::string& symbol,
    double base_size
) {
    auto it = volatility_profiles_.find(symbol);
    if (it == volatility_profiles_.end()) {
        return base_size;  // No data, use base size
    }

    const auto& profile = it->second;

    // Check if data is stale (older than 1 hour)
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - profile.last_update);
    if (age.count() > 60) {
        update_volatility_profiles();  // Refresh stale data
    }

    return base_size * profile.vol_scalar;
}
```

**Expected Impact**:
- UVXY (very high vol): 0.5x position size → 7.5% instead of 15%
- TQQQ (high vol): 0.7x position size → 10.5% instead of 15%
- SSO (medium vol): 1.0x position size → 15% (unchanged)
- Result: **Reduce losses from high-volatility symbols, improve Sharpe ratio**

**Testing**: Backtest October with volatility adjustment enabled vs disabled

---

### 1.2 Capital Tracking Refactor (P1)

**Current Issue**: Dual tracking system is error-prone
```cpp
double current_cash_;
double allocated_capital_;
// Risk: Can become desynchronized, leading to over/under-allocation
```

**Expert Recommendation**: Single source of truth with derived values

**Implementation Plan**:

**Step 1**: Create Capital Manager class
```cpp
// include/backend/capital_manager.h
#ifndef SENTIO_CAPITAL_MANAGER_H
#define SENTIO_CAPITAL_MANAGER_H

#include <map>
#include <string>
#include <stdexcept>

namespace sentio {

class CapitalManager {
public:
    explicit CapitalManager(double initial_capital)
        : total_capital_(initial_capital),
          initial_capital_(initial_capital) {}

    // Allocate capital to a position
    void allocate(const std::string& symbol, double amount);

    // Deallocate capital from a position (on exit)
    void deallocate(const std::string& symbol, double amount);

    // Update position value (mark-to-market)
    void update_position_value(const std::string& symbol, double new_value);

    // Getters
    double get_total_capital() const { return total_capital_; }
    double get_allocated_capital() const;
    double get_available_cash() const;
    double get_total_equity() const;
    double get_position_value(const std::string& symbol) const;

    // Validation
    bool can_allocate(double amount) const;
    double get_max_allocation() const;

    // Reconciliation
    void reconcile(double expected_equity);
    bool is_balanced() const;

private:
    double total_capital_;           // Initial capital + realized P&L
    double initial_capital_;
    std::map<std::string, double> position_values_;  // Current market value of each position

    // Validation
    static constexpr double RECONCILIATION_TOLERANCE = 0.01;  // 1% tolerance
};

} // namespace sentio

#endif // SENTIO_CAPITAL_MANAGER_H
```

**Step 2**: Implementation
```cpp
// src/backend/capital_manager.cpp
#include "backend/capital_manager.h"
#include <numeric>
#include <cmath>

namespace sentio {

void CapitalManager::allocate(const std::string& symbol, double amount) {
    if (amount < 0) {
        throw std::invalid_argument("Cannot allocate negative amount");
    }

    if (!can_allocate(amount)) {
        throw std::runtime_error("Insufficient capital for allocation");
    }

    position_values_[symbol] += amount;
}

void CapitalManager::deallocate(const std::string& symbol, double amount) {
    auto it = position_values_.find(symbol);
    if (it == position_values_.end()) {
        throw std::runtime_error("Symbol not found in positions");
    }

    it->second -= amount;

    // Remove if zero
    if (std::abs(it->second) < 0.01) {
        position_values_.erase(it);
    }

    // Update total capital with realized P&L
    double realized_pnl = amount - amount;  // Will be calculated properly in backend
    total_capital_ += realized_pnl;
}

void CapitalManager::update_position_value(const std::string& symbol, double new_value) {
    position_values_[symbol] = new_value;
}

double CapitalManager::get_allocated_capital() const {
    return std::accumulate(
        position_values_.begin(),
        position_values_.end(),
        0.0,
        [](double sum, const auto& p) { return sum + p.second; }
    );
}

double CapitalManager::get_available_cash() const {
    double allocated = get_allocated_capital();
    return total_capital_ - allocated;
}

double CapitalManager::get_total_equity() const {
    // Total equity = cash + market value of positions
    return total_capital_ + std::accumulate(
        position_values_.begin(),
        position_values_.end(),
        0.0,
        [](double sum, const auto& p) {
            return sum + p.second;  // Unrealized P&L included in position_values_
        }
    );
}

double CapitalManager::get_position_value(const std::string& symbol) const {
    auto it = position_values_.find(symbol);
    return (it != position_values_.end()) ? it->second : 0.0;
}

bool CapitalManager::can_allocate(double amount) const {
    return get_available_cash() >= amount;
}

double CapitalManager::get_max_allocation() const {
    return get_available_cash() * 0.95;  // Leave 5% buffer
}

void CapitalManager::reconcile(double expected_equity) {
    double actual_equity = get_total_equity();
    double diff = std::abs(actual_equity - expected_equity);
    double tolerance = expected_equity * RECONCILIATION_TOLERANCE;

    if (diff > tolerance) {
        // Log warning - reconciliation mismatch
        // In production, this would trigger an alert
    }
}

bool CapitalManager::is_balanced() const {
    // Check that total_capital + allocated = expected
    double allocated = get_allocated_capital();
    double total = total_capital_ + allocated;
    return std::abs(total - initial_capital_) < (initial_capital_ * 0.01);
}

} // namespace sentio
```

**Step 3**: Integrate into backend
```cpp
// include/backend/rotation_trading_backend.h
#include "backend/capital_manager.h"

class RotationTradingBackend {
private:
    std::unique_ptr<CapitalManager> capital_manager_;

    // Remove:
    // double current_cash_;
    // double allocated_capital_;
};
```

**Expected Impact**:
- Eliminate capital tracking bugs
- Easier to audit and reconcile
- Clear separation of concerns

---

## 2. Production Readiness Enhancements (P0)

### 2.1 Data Quality Validation

**Current Issue**: No validation of incoming bar data

**Implementation Plan**:

**Step 1**: Create data validator
```cpp
// include/common/data_validator.h
#ifndef SENTIO_DATA_VALIDATOR_H
#define SENTIO_DATA_VALIDATOR_H

#include "common/types.h"
#include <map>
#include <chrono>
#include <string>

namespace sentio {

class DataValidator {
public:
    struct ValidationConfig {
        double max_price_move_pct = 0.10;     // 10% max move per minute
        double max_spread_pct = 0.05;         // 5% max bid-ask spread
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

private:
    ValidationConfig config_;
    std::map<std::string, Bar> prev_bars_;
    std::string last_error_;

    bool check_price_anomaly(const std::string& symbol, const Bar& bar);
    bool check_spread(const Bar& bar);
    bool check_staleness(const Bar& bar);
    bool check_volume(const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_DATA_VALIDATOR_H
```

**Step 2**: Implementation
```cpp
// src/common/data_validator.cpp
#include "common/data_validator.h"
#include <cmath>
#include <sstream>

namespace sentio {

bool DataValidator::validate_bar(const std::string& symbol, const Bar& bar) {
    last_error_.clear();

    // Check for NaN or invalid prices
    if (std::isnan(bar.open) || std::isnan(bar.high) ||
        std::isnan(bar.low) || std::isnan(bar.close)) {
        last_error_ = "NaN price detected for " + symbol;
        return false;
    }

    // Check OHLC consistency
    if (bar.high < bar.low || bar.high < bar.open ||
        bar.high < bar.close || bar.low > bar.open || bar.low > bar.close) {
        last_error_ = "OHLC inconsistency for " + symbol;
        return false;
    }

    // Check price anomaly
    if (!check_price_anomaly(symbol, bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check spread (high-low range)
    if (!check_spread(bar)) {
        return config_.strict_mode ? false : true;
    }

    // Check staleness
    if (!check_staleness(bar)) {
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

bool DataValidator::check_price_anomaly(const std::string& symbol, const Bar& bar) {
    auto it = prev_bars_.find(symbol);
    if (it == prev_bars_.end()) {
        return true;  // First bar, no comparison
    }

    const Bar& prev = it->second;
    double price_move = std::abs(bar.close - prev.close) / prev.close;

    if (price_move > config_.max_price_move_pct) {
        std::ostringstream oss;
        oss << "Price anomaly for " << symbol
            << ": " << (price_move * 100) << "% move"
            << " (prev=" << prev.close << ", curr=" << bar.close << ")";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_spread(const Bar& bar) {
    double spread = (bar.high - bar.low) / bar.close;

    if (spread > config_.max_spread_pct) {
        std::ostringstream oss;
        oss << "Excessive spread: " << (spread * 100) << "%";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_staleness(const Bar& bar) {
    auto now = std::chrono::system_clock::now();
    auto bar_time = std::chrono::system_clock::from_time_t(bar.timestamp_utc / 1000);
    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - bar_time).count();

    if (age_seconds > config_.max_staleness_seconds) {
        std::ostringstream oss;
        oss << "Stale data: " << age_seconds << " seconds old";
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::check_volume(const Bar& bar) {
    if (bar.volume < config_.min_volume) {
        std::ostringstream oss;
        oss << "Low volume: " << bar.volume;
        last_error_ = oss.str();
        return false;
    }

    return true;
}

bool DataValidator::validate_snapshot(const std::map<std::string, Bar>& snapshot) {
    for (const auto& [symbol, bar] : snapshot) {
        if (!validate_bar(symbol, bar)) {
            return false;
        }
    }
    return true;
}

} // namespace sentio
```

**Step 3**: Integrate into backend
```cpp
// src/backend/rotation_trading_backend.cpp
#include "common/data_validator.h"

class RotationTradingBackend {
private:
    std::unique_ptr<DataValidator> data_validator_;
};

void RotationTradingBackend::on_bar(const std::string& symbol, const Bar& bar) {
    // Validate before processing
    if (!data_validator_->validate_bar(symbol, bar)) {
        std::cerr << "[WARNING] Data validation failed for " << symbol
                  << ": " << data_validator_->get_last_error() << std::endl;
        // Log to file
        // Optionally skip this bar
        return;
    }

    // Continue with normal processing...
}
```

**Expected Impact**:
- Prevent bad data from corrupting learning models
- Early detection of feed issues
- Improved system reliability

---

### 2.2 Monitoring & Alerting System

**Implementation Plan**:

**Step 1**: Create monitoring class
```cpp
// include/backend/trading_monitor.h
#ifndef SENTIO_TRADING_MONITOR_H
#define SENTIO_TRADING_MONITOR_H

#include "common/types.h"
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace sentio {

class TradingMonitor {
public:
    enum class AlertLevel {
        INFO,
        WARNING,
        CRITICAL
    };

    struct Alert {
        AlertLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    struct MonitorConfig {
        int max_consecutive_losses = 5;
        double max_drawdown_pct = 0.10;
        int max_data_staleness_seconds = 60;
        double min_equity_pct = 0.90;  // Alert if equity drops below 90% of starting
    };

    using AlertCallback = std::function<void(const Alert&)>;

    explicit TradingMonitor(MonitorConfig config = {})
        : config_(config) {}

    // Register callback for alerts
    void register_alert_handler(AlertCallback callback);

    // Update metrics
    void update_trade_result(bool is_win, double pnl);
    void update_equity(double current_equity, double starting_equity);
    void update_data_staleness(int seconds);

    // Manual checks
    void check_health();

    // Get metrics
    int get_consecutive_losses() const { return consecutive_losses_; }
    double get_current_drawdown() const { return current_drawdown_; }
    std::vector<Alert> get_recent_alerts(int count = 10) const;

private:
    MonitorConfig config_;
    std::vector<AlertCallback> alert_handlers_;
    std::vector<Alert> alert_history_;

    // Metrics
    int consecutive_losses_ = 0;
    int consecutive_wins_ = 0;
    double current_drawdown_ = 0.0;
    double peak_equity_ = 0.0;
    int data_staleness_seconds_ = 0;

    void send_alert(AlertLevel level, const std::string& message);
};

} // namespace sentio

#endif // SENTIO_TRADING_MONITOR_H
```

**Step 2**: Implementation
```cpp
// src/backend/trading_monitor.cpp
#include "backend/trading_monitor.h"
#include <iostream>

namespace sentio {

void TradingMonitor::register_alert_handler(AlertCallback callback) {
    alert_handlers_.push_back(callback);
}

void TradingMonitor::update_trade_result(bool is_win, double pnl) {
    if (is_win) {
        consecutive_wins_++;
        consecutive_losses_ = 0;
    } else {
        consecutive_losses_++;
        consecutive_wins_ = 0;

        if (consecutive_losses_ >= config_.max_consecutive_losses) {
            send_alert(AlertLevel::WARNING,
                      "Consecutive losses exceeded: " + std::to_string(consecutive_losses_));
        }
    }
}

void TradingMonitor::update_equity(double current_equity, double starting_equity) {
    // Update peak equity
    if (current_equity > peak_equity_) {
        peak_equity_ = current_equity;
    }

    // Calculate drawdown
    current_drawdown_ = (peak_equity_ - current_equity) / peak_equity_;

    if (current_drawdown_ >= config_.max_drawdown_pct) {
        send_alert(AlertLevel::CRITICAL,
                  "Maximum drawdown reached: " +
                  std::to_string(current_drawdown_ * 100) + "%");
    }

    // Check minimum equity
    double equity_pct = current_equity / starting_equity;
    if (equity_pct < config_.min_equity_pct) {
        send_alert(AlertLevel::WARNING,
                  "Equity dropped below " +
                  std::to_string(config_.min_equity_pct * 100) + "%");
    }
}

void TradingMonitor::update_data_staleness(int seconds) {
    data_staleness_seconds_ = seconds;

    if (seconds > config_.max_data_staleness_seconds) {
        send_alert(AlertLevel::WARNING,
                  "Data feed issues: " + std::to_string(seconds) + " seconds stale");
    }
}

void TradingMonitor::check_health() {
    // Perform comprehensive health check
    std::vector<std::string> issues;

    if (consecutive_losses_ >= config_.max_consecutive_losses) {
        issues.push_back("High consecutive losses");
    }

    if (current_drawdown_ >= config_.max_drawdown_pct) {
        issues.push_back("Excessive drawdown");
    }

    if (data_staleness_seconds_ > config_.max_data_staleness_seconds) {
        issues.push_back("Stale data feed");
    }

    if (!issues.empty()) {
        std::string combined = "Health check failed: ";
        for (const auto& issue : issues) {
            combined += issue + "; ";
        }
        send_alert(AlertLevel::CRITICAL, combined);
    }
}

void TradingMonitor::send_alert(AlertLevel level, const std::string& message) {
    Alert alert{level, message, std::chrono::system_clock::now()};

    // Store in history
    alert_history_.push_back(alert);
    if (alert_history_.size() > 1000) {
        alert_history_.erase(alert_history_.begin());
    }

    // Call all registered handlers
    for (const auto& handler : alert_handlers_) {
        handler(alert);
    }

    // Default console logging
    std::string level_str = (level == AlertLevel::CRITICAL) ? "CRITICAL" :
                           (level == AlertLevel::WARNING) ? "WARNING" : "INFO";
    std::cerr << "[" << level_str << "] " << message << std::endl;
}

std::vector<TradingMonitor::Alert> TradingMonitor::get_recent_alerts(int count) const {
    int start = std::max(0, static_cast<int>(alert_history_.size()) - count);
    return std::vector<Alert>(alert_history_.begin() + start, alert_history_.end());
}

} // namespace sentio
```

**Expected Impact**:
- Real-time issue detection
- Prevent catastrophic losses
- Operational visibility

---

## 3. Architecture Improvements (P1-P2)

### 3.1 Transaction Cost Modeling

**Implementation Plan**:

**Step 1**: Create cost model
```cpp
// include/backend/transaction_costs.h
#ifndef SENTIO_TRANSACTION_COSTS_H
#define SENTIO_TRANSACTION_COSTS_H

#include <string>
#include <map>
#include <cmath>

namespace sentio {

class TransactionCosts {
public:
    struct CostConfig {
        double spread_bps = 1.0;          // 1 basis point spread
        double market_impact_bps = 2.0;   // 2 basis points for leveraged ETFs
        double commission = 0.0;          // Per-trade commission (if applicable)
        double sec_fee_bps = 0.0;         // SEC fee (if applicable)
    };

    explicit TransactionCosts(CostConfig config = {})
        : config_(config) {}

    // Calculate total cost for a trade
    double calculate_entry_cost(double position_value) const;
    double calculate_exit_cost(double position_value) const;
    double calculate_round_trip_cost(double position_value) const;

    // Get cost as percentage of position
    double get_cost_percentage(double position_value) const;

    // Check if trade is profitable after costs
    bool is_profitable_after_costs(double gross_pnl, double position_value) const;

private:
    CostConfig config_;

    double bps_to_decimal(double bps) const { return bps / 10000.0; }
};

} // namespace sentio

#endif // SENTIO_TRANSACTION_COSTS_H
```

**Step 2**: Implementation
```cpp
// src/backend/transaction_costs.cpp
#include "backend/transaction_costs.h"

namespace sentio {

double TransactionCosts::calculate_entry_cost(double position_value) const {
    // Spread cost
    double spread_cost = position_value * bps_to_decimal(config_.spread_bps);

    // Market impact (sqrt scaling with position size)
    double impact_cost = position_value * bps_to_decimal(config_.market_impact_bps) *
                        std::sqrt(position_value / 10000.0);  // Normalize by $10k

    // Fixed costs
    double fixed_cost = config_.commission + position_value * bps_to_decimal(config_.sec_fee_bps);

    return spread_cost + impact_cost + fixed_cost;
}

double TransactionCosts::calculate_exit_cost(double position_value) const {
    return calculate_entry_cost(position_value);  // Symmetric for now
}

double TransactionCosts::calculate_round_trip_cost(double position_value) const {
    return calculate_entry_cost(position_value) + calculate_exit_cost(position_value);
}

double TransactionCosts::get_cost_percentage(double position_value) const {
    double cost = calculate_round_trip_cost(position_value);
    return cost / position_value;
}

bool TransactionCosts::is_profitable_after_costs(double gross_pnl, double position_value) const {
    double round_trip_cost = calculate_round_trip_cost(position_value);
    return gross_pnl > round_trip_cost;
}

} // namespace sentio
```

**Step 3**: Integrate into signal evaluation
```cpp
// In adaptive_trading_mechanism.cpp
bool AdaptiveTradingMechanism::should_enter_position(
    const Signal& signal,
    double position_value
) {
    // Existing logic...

    // Check if expected profit exceeds transaction costs
    double expected_profit = signal.confidence * position_value * 0.02;  // 2% target
    double round_trip_cost = transaction_costs_->calculate_round_trip_cost(position_value);

    if (expected_profit < round_trip_cost * 1.5) {
        // Expected profit not enough to cover costs + margin
        return false;
    }

    return true;
}
```

**Expected Impact**:
- Filter out low-profit trades
- Improve profit factor by avoiding costly small trades
- More realistic P&L estimates

---

### 3.2 Feature Caching (P1)

**Implementation Plan**:

**Step 1**: Add caching to feature engine
```cpp
// include/features/unified_feature_engine.h
class UnifiedFeatureEngine {
private:
    struct FeatureCache {
        uint64_t bar_id;
        std::vector<double> features;
        std::chrono::system_clock::time_point timestamp;
    };

    std::deque<FeatureCache> feature_cache_;
    static constexpr size_t MAX_CACHE_SIZE = 1000;

    bool try_get_cached_features(uint64_t bar_id, std::vector<double>& features);
    void cache_features(uint64_t bar_id, const std::vector<double>& features);
};
```

**Step 2**: Implementation
```cpp
// src/features/unified_feature_engine.cpp
bool UnifiedFeatureEngine::try_get_cached_features(
    uint64_t bar_id,
    std::vector<double>& features
) {
    for (const auto& cached : feature_cache_) {
        if (cached.bar_id == bar_id) {
            features = cached.features;
            return true;
        }
    }
    return false;
}

void UnifiedFeatureEngine::cache_features(
    uint64_t bar_id,
    const std::vector<double>& features
) {
    // Add to cache
    feature_cache_.push_back({
        bar_id,
        features,
        std::chrono::system_clock::now()
    });

    // Limit cache size
    if (feature_cache_.size() > MAX_CACHE_SIZE) {
        feature_cache_.pop_front();
    }
}

std::vector<double> UnifiedFeatureEngine::get_features() {
    uint64_t bar_id = latest_bar_.timestamp_utc;

    // Try cache first
    std::vector<double> features;
    if (try_get_cached_features(bar_id, features)) {
        return features;
    }

    // Calculate features
    features = calculate_features();

    // Cache result
    cache_features(bar_id, features);

    return features;
}
```

**Expected Impact**:
- Reduce CPU usage by 20-30%
- Faster signal generation
- Better scalability for more symbols

---

## 4. Performance Optimizations (P2)

### 4.1 Reduce Warmup Data

**Current**: 1560 bars (6.5 trading days) for mock warmup
**Proposed**: 200-500 bars (0.8-2 trading days)

**Implementation**:
```cpp
// In rotation_trade_command.cpp
constexpr int MINIMAL_WARMUP_BARS = 200;   // For indicators (MA, RSI, etc.)
constexpr int STANDARD_WARMUP_BARS = 500;  // For learning stability

// Update warmup loading
int warmup_bars = config.minimal_mode ? MINIMAL_WARMUP_BARS : STANDARD_WARMUP_BARS;
```

**Expected Impact**:
- Faster warmup (from ~30 seconds to ~10 seconds)
- Reduced memory usage
- Minimal impact on model accuracy

---

## 5. Testing Strategy (P2)

### 5.1 Unit Tests

**Required Test Coverage**:

1. **Capital Manager Tests**
```cpp
TEST(CapitalManagerTest, AllocateAndDeallocate) {
    CapitalManager cm(100000.0);
    cm.allocate("TQQQ", 15000.0);
    EXPECT_EQ(cm.get_allocated_capital(), 15000.0);
    EXPECT_EQ(cm.get_available_cash(), 85000.0);

    cm.deallocate("TQQQ", 15500.0);  // With profit
    EXPECT_EQ(cm.get_allocated_capital(), 0.0);
    EXPECT_GT(cm.get_total_capital(), 100000.0);  // Profit added
}

TEST(CapitalManagerTest, OverAllocation) {
    CapitalManager cm(100000.0);
    EXPECT_THROW(cm.allocate("TQQQ", 150000.0), std::runtime_error);
}
```

2. **Data Validator Tests**
```cpp
TEST(DataValidatorTest, PriceAnomaly) {
    DataValidator validator;
    Bar bar1{.close = 100.0, .timestamp_utc = 1000};
    Bar bar2{.close = 120.0, .timestamp_utc = 2000};  // 20% move

    EXPECT_TRUE(validator.validate_bar("TEST", bar1));
    EXPECT_FALSE(validator.validate_bar("TEST", bar2));  // Should fail
}
```

3. **Transaction Cost Tests**
```cpp
TEST(TransactionCostsTest, RoundTripCost) {
    TransactionCosts costs;
    double position_value = 10000.0;
    double round_trip = costs.calculate_round_trip_cost(position_value);
    EXPECT_GT(round_trip, 0.0);
    EXPECT_LT(round_trip, position_value * 0.01);  // Less than 1%
}
```

### 5.2 Stress Tests

**Test Scenarios**:

1. **Rapid Market Movements**: Simulate circuit breaker conditions
2. **Data Feed Interruptions**: Test graceful degradation
3. **High-Frequency Rotations**: Verify no race conditions
4. **Memory Leaks**: Run for extended periods (24+ hours)

---

## 6. Implementation Timeline

### Week 1: Critical Fixes (P0)
- Day 1-2: Data Quality Validation
- Day 3-4: Monitoring & Alerting
- Day 5: Volatility-Adjusted Position Sizing
- Weekend: Testing & Validation

### Week 2: Architecture (P1)
- Day 1-2: Transaction Cost Modeling
- Day 3-4: Capital Manager Refactor
- Day 5: Feature Caching
- Weekend: Integration Testing

### Week 3: Advanced (P2)
- Day 1-2: Feature Selection (optional)
- Day 3-4: Adaptive Horizon Weighting (optional)
- Day 5: Reduce Warmup Data
- Weekend: Performance Testing

### Week 4: Testing & Validation
- Day 1-2: Unit Tests
- Day 3-4: Stress Tests
- Day 5: Paper Trading Setup
- Weekend: Documentation

---

## 7. Success Metrics

### Week 1 Targets:
- Zero data quality issues in backtests
- Monitoring system operational
- Volatility adjustment reduces high-vol losses by 20%

### Week 2 Targets:
- Transaction costs reduce trade frequency by 15%
- Capital tracking: 100% accuracy in reconciliation
- Feature caching improves performance by 25%

### Week 3 Targets:
- Warmup time reduced from 30s to 10s
- Memory usage reduced by 30%

### Week 4 Targets:
- 80%+ unit test coverage for critical paths
- Pass all stress tests
- Ready for paper trading

---

## 8. Exclusions (As Requested)

**NOT Implementing**:
- Fallback Strategy (expert recommended, user excluded)
  - Simple momentum signals when ML fails
  - Reason: User prefers ML-only approach

**Deferred to Later**:
- Learning State Queue (P2, low priority)
- Feature Selection (P2, requires extensive research)
- Adaptive Horizon Weighting (P2, needs validation)

---

## Conclusion

This implementation plan addresses expert feedback systematically, prioritizing:
1. **Risk Management** (prevent losses)
2. **Production Readiness** (system reliability)
3. **Architecture** (code quality)
4. **Performance** (scalability)
5. **Testing** (confidence)

Expected outcome: **Improved MRD, Profit Factor, and System Reliability** within 4 weeks.

---

**End of Document**
