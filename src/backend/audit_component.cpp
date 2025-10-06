#include "backend/audit_component.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>

namespace sentio {

TradeSummary AuditComponent::analyze_equity_curve(const std::vector<double>& equity_curve) const {
    TradeSummary s;
    if (equity_curve.size() < 2) return s;

    // Calculate total trades and basic metrics
    s.total_trades = static_cast<int>(equity_curve.size());
    
    // Approximate returns from equity curve deltas
    std::vector<double> returns;
    returns.reserve(equity_curve.size() - 1);
    
    int positive_returns = 0;
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double prev = equity_curve[i - 1];
        double curr = equity_curve[i];
        if (prev != 0.0) {
            double return_val = (curr - prev) / prev;
            returns.push_back(return_val);
            if (return_val > 0) {
                positive_returns++;
                s.wins++;
            } else if (return_val < 0) {
                s.losses++;
            }
        }
    }
    
    // Calculate win rate
    if (s.wins + s.losses > 0) {
        s.win_rate = static_cast<double>(s.wins) / (s.wins + s.losses);
    }

    // Calculate Sharpe ratio and max drawdown
    s.sharpe = calculate_sharpe_ratio(returns);
    s.max_drawdown = calculate_max_drawdown(equity_curve);
    
    return s;
}

double AuditComponent::calculate_sharpe_ratio(const std::vector<double>& returns) const {
    if (returns.empty()) return 0.0;
    
    // Calculate mean return
    double mean_return = 0.0;
    for (double ret : returns) {
        mean_return += ret;
    }
    mean_return /= returns.size();
    
    // Calculate standard deviation
    double variance = 0.0;
    for (double ret : returns) {
        variance += std::pow(ret - mean_return, 2);
    }
    variance /= returns.size();
    double std_dev = std::sqrt(variance);
    
    // Sharpe ratio (assuming risk-free rate = 0)
    if (std_dev == 0.0) return 0.0;
    return mean_return / std_dev;
}

double AuditComponent::calculate_max_drawdown(const std::vector<double>& equity_curve) const {
    if (equity_curve.size() < 2) return 0.0;
    
    double max_drawdown = 0.0;
    double peak = equity_curve[0];
    
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        if (equity_curve[i] > peak) {
            peak = equity_curve[i];
        } else {
            double drawdown = (peak - equity_curve[i]) / peak;
            max_drawdown = std::max(max_drawdown, drawdown);
        }
    }
    
    return max_drawdown;
}

} // namespace sentio


