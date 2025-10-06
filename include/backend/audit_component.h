#pragma once

// =============================================================================
// Module: backend/audit_component.h
// Purpose: Post-run analysis of trades and portfolio performance metrics.
//
// Core idea:
// - Consume executed trades and produce risk/performance analytics such as
//   Sharpe ratio, max drawdown, win rate, and conflict detection summaries.
// =============================================================================

#include <vector>
#include <string>
#include <map>

namespace sentio {

struct TradeSummary {
    int total_trades = 0;
    int wins = 0;
    int losses = 0;
    double win_rate = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
};

class AuditComponent {
public:
    /**
     * @brief Analyze equity curve and generate comprehensive trading summary
     * @param equity_curve Vector of equity values over time
     * @return TradeSummary with performance metrics
     */
    TradeSummary analyze_equity_curve(const std::vector<double>& equity_curve) const;
    
private:
    /**
     * @brief Calculate Sharpe ratio from returns
     * @param returns Vector of period returns
     * @return Sharpe ratio (risk-free rate assumed to be 0)
     */
    double calculate_sharpe_ratio(const std::vector<double>& returns) const;
    
    /**
     * @brief Calculate maximum drawdown from equity curve
     * @param equity_curve Vector of equity values over time  
     * @return Maximum drawdown as percentage
     */
    double calculate_max_drawdown(const std::vector<double>& equity_curve) const;
};

} // namespace sentio


