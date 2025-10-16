#include "backend/trading_monitor.h"
#include <iostream>
#include <iomanip>
#include <sstream>

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
            std::ostringstream oss;
            oss << "Consecutive losses exceeded: " << consecutive_losses_
                << " (threshold: " << config_.max_consecutive_losses << ")";
            send_alert(AlertLevel::WARNING, oss.str());
        }
    }
}

void TradingMonitor::update_equity(double current_equity, double starting_equity) {
    // Update peak equity
    if (current_equity > peak_equity_) {
        peak_equity_ = current_equity;
    }

    // Calculate drawdown from peak
    if (peak_equity_ > 0) {
        current_drawdown_ = (peak_equity_ - current_equity) / peak_equity_;
    }

    if (current_drawdown_ >= config_.max_drawdown_pct) {
        std::ostringstream oss;
        oss << "Maximum drawdown reached: "
            << std::fixed << std::setprecision(2)
            << (current_drawdown_ * 100) << "%"
            << " (threshold: " << (config_.max_drawdown_pct * 100) << "%)";
        send_alert(AlertLevel::CRITICAL, oss.str());
    }

    // Check minimum equity
    double equity_pct = current_equity / starting_equity;
    if (equity_pct < config_.min_equity_pct) {
        std::ostringstream oss;
        oss << "Equity dropped below "
            << std::fixed << std::setprecision(1)
            << (config_.min_equity_pct * 100) << "%"
            << " (current: " << (equity_pct * 100) << "%)";
        send_alert(AlertLevel::WARNING, oss.str());
    }
}

void TradingMonitor::update_data_staleness(int seconds) {
    data_staleness_seconds_ = seconds;

    if (seconds > config_.max_data_staleness_seconds) {
        std::ostringstream oss;
        oss << "Data feed issues: " << seconds << " seconds stale"
            << " (threshold: " << config_.max_data_staleness_seconds << ")";
        send_alert(AlertLevel::WARNING, oss.str());
    }
}

void TradingMonitor::check_health() {
    std::vector<std::string> issues;

    if (consecutive_losses_ >= config_.max_consecutive_losses) {
        issues.push_back("High consecutive losses (" +
                        std::to_string(consecutive_losses_) + ")");
    }

    if (current_drawdown_ >= config_.max_drawdown_pct) {
        std::ostringstream oss;
        oss << "Excessive drawdown ("
            << std::fixed << std::setprecision(2)
            << (current_drawdown_ * 100) << "%)";
        issues.push_back(oss.str());
    }

    if (data_staleness_seconds_ > config_.max_data_staleness_seconds) {
        issues.push_back("Stale data feed (" +
                        std::to_string(data_staleness_seconds_) + "s)");
    }

    if (!issues.empty()) {
        std::string combined = "Health check failed: ";
        for (size_t i = 0; i < issues.size(); ++i) {
            combined += issues[i];
            if (i < issues.size() - 1) combined += "; ";
        }
        send_alert(AlertLevel::CRITICAL, combined);
    } else {
        send_alert(AlertLevel::INFO, "Health check passed - all systems normal");
    }
}

void TradingMonitor::send_alert(AlertLevel level, const std::string& message) {
    Alert alert{level, message, std::chrono::system_clock::now()};

    // Store in history (limited to 1000 alerts)
    alert_history_.push_back(alert);
    if (alert_history_.size() > 1000) {
        alert_history_.pop_front();
    }

    // Call all registered handlers
    for (const auto& handler : alert_handlers_) {
        handler(alert);
    }

    // Console logging (if enabled)
    if (config_.enable_console_alerts) {
        std::string level_prefix;
        switch (level) {
            case AlertLevel::INFO:
                level_prefix = "[INFO] ";
                break;
            case AlertLevel::WARNING:
                level_prefix = "[⚠️  WARNING] ";
                break;
            case AlertLevel::CRITICAL:
                level_prefix = "[🚨 CRITICAL] ";
                break;
        }
        std::cerr << level_prefix << message << std::endl;
    }
}

std::vector<TradingMonitor::Alert> TradingMonitor::get_recent_alerts(int count) const {
    int start = std::max(0, static_cast<int>(alert_history_.size()) - count);
    return std::vector<Alert>(alert_history_.begin() + start, alert_history_.end());
}

void TradingMonitor::reset() {
    peak_equity_ = 0.0;
    current_drawdown_ = 0.0;
    consecutive_losses_ = 0;
    consecutive_wins_ = 0;
    data_staleness_seconds_ = 0;
    alert_history_.clear();
}

} // namespace sentio
