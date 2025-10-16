#pragma once

#include "common/types.h"
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <deque>

namespace sentio {

/**
 * @brief Real-time trading system monitoring and alerting
 *
 * Monitors critical metrics and sends alerts when thresholds are exceeded:
 * - Consecutive losses
 * - Drawdown
 * - Data staleness
 * - Equity drops
 */
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

        std::string level_string() const {
            switch (level) {
                case AlertLevel::INFO: return "INFO";
                case AlertLevel::WARNING: return "WARNING";
                case AlertLevel::CRITICAL: return "CRITICAL";
                default: return "UNKNOWN";
            }
        }
    };

    struct MonitorConfig {
        int max_consecutive_losses = 5;
        double max_drawdown_pct = 0.10;       // 10% max drawdown
        int max_data_staleness_seconds = 60;
        double min_equity_pct = 0.90;         // Alert if equity drops below 90%
        bool enable_console_alerts = true;    // Print alerts to console
    };

    using AlertCallback = std::function<void(const Alert&)>;

    TradingMonitor()
        : config_(), peak_equity_(0.0), current_drawdown_(0.0),
          consecutive_losses_(0), consecutive_wins_(0), data_staleness_seconds_(0) {}

    explicit TradingMonitor(MonitorConfig config)
        : config_(config), peak_equity_(0.0), current_drawdown_(0.0),
          consecutive_losses_(0), consecutive_wins_(0), data_staleness_seconds_(0) {}

    // Register callback for alerts
    void register_alert_handler(AlertCallback callback);

    // Update metrics
    void update_trade_result(bool is_win, double pnl);
    void update_equity(double current_equity, double starting_equity);
    void update_data_staleness(int seconds);

    // Manual health check
    void check_health();

    // Get metrics
    int get_consecutive_losses() const { return consecutive_losses_; }
    int get_consecutive_wins() const { return consecutive_wins_; }
    double get_current_drawdown() const { return current_drawdown_; }
    double get_peak_equity() const { return peak_equity_; }
    int get_data_staleness() const { return data_staleness_seconds_; }
    std::vector<Alert> get_recent_alerts(int count = 10) const;

    // Reset monitor
    void reset();

private:
    MonitorConfig config_;
    std::vector<AlertCallback> alert_handlers_;
    std::deque<Alert> alert_history_;

    // Metrics
    double peak_equity_;
    double current_drawdown_;
    int consecutive_losses_;
    int consecutive_wins_;
    int data_staleness_seconds_;

    void send_alert(AlertLevel level, const std::string& message);
};

} // namespace sentio
