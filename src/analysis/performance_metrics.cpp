// src/analysis/performance_metrics.cpp
#include "analysis/performance_metrics.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace sentio::analysis {

// PerformanceMetrics implementation

std::string PerformanceMetrics::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"strategy_name\": \"" << strategy_name << "\",\n";
    ss << "  \"dataset_name\": \"" << dataset_name << "\",\n";
    
    // MRB metrics
    ss << "  \"mrb_metrics\": {\n";
    ss << "    \"signal_accuracy\": " << signal_accuracy << ",\n";
    ss << "    \"trading_based_mrb\": " << trading_based_mrb << ",\n";
    ss << "    \"mrb_consistency\": " << mrb_consistency << "\n";
    ss << "  },\n";
    
    // Return metrics
    ss << "  \"return_metrics\": {\n";
    ss << "    \"total_return\": " << total_return << ",\n";
    ss << "    \"annualized_return\": " << annualized_return << ",\n";
    ss << "    \"cumulative_return\": " << cumulative_return << "\n";
    ss << "  },\n";
    
    // Risk-adjusted metrics
    ss << "  \"risk_adjusted_metrics\": {\n";
    ss << "    \"sharpe_ratio\": " << sharpe_ratio << ",\n";
    ss << "    \"sortino_ratio\": " << sortino_ratio << ",\n";
    ss << "    \"calmar_ratio\": " << calmar_ratio << ",\n";
    ss << "    \"information_ratio\": " << information_ratio << "\n";
    ss << "  },\n";
    
    // Risk metrics
    ss << "  \"risk_metrics\": {\n";
    ss << "    \"max_drawdown\": " << max_drawdown << ",\n";
    ss << "    \"avg_drawdown\": " << avg_drawdown << ",\n";
    ss << "    \"volatility\": " << volatility << ",\n";
    ss << "    \"downside_deviation\": " << downside_deviation << "\n";
    ss << "  },\n";
    
    // Trading metrics
    ss << "  \"trading_metrics\": {\n";
    ss << "    \"win_rate\": " << win_rate << ",\n";
    ss << "    \"profit_factor\": " << profit_factor << ",\n";
    ss << "    \"avg_win\": " << avg_win << ",\n";
    ss << "    \"avg_loss\": " << avg_loss << ",\n";
    ss << "    \"total_trades\": " << total_trades << ",\n";
    ss << "    \"winning_trades\": " << winning_trades << ",\n";
    ss << "    \"losing_trades\": " << losing_trades << "\n";
    ss << "  },\n";
    
    // Signal metrics
    ss << "  \"signal_metrics\": {\n";
    ss << "    \"total_signals\": " << total_signals << ",\n";
    ss << "    \"non_neutral_signals\": " << non_neutral_signals << ",\n";
    ss << "    \"long_signals\": " << long_signals << ",\n";
    ss << "    \"short_signals\": " << short_signals << ",\n";
    ss << "    \"signal_generation_rate\": " << signal_generation_rate << ",\n";
    ss << "    \"non_neutral_ratio\": " << non_neutral_ratio << ",\n";
    ss << "    \"mean_confidence\": " << mean_confidence << "\n";
    ss << "  },\n";
    
    // Execution metrics
    ss << "  \"execution_metrics\": {\n";
    ss << "    \"execution_time_ms\": " << execution_time_ms << ",\n";
    ss << "    \"avg_inference_time_ms\": " << avg_inference_time_ms << ",\n";
    ss << "    \"memory_usage_mb\": " << memory_usage_mb << "\n";
    ss << "  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string PerformanceMetrics::get_summary() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "Strategy: " << strategy_name;
    if (!dataset_name.empty()) {
        ss << " (" << dataset_name << ")";
    }
    ss << "\n";
    
    ss << "MRB: " << trading_based_mrb;
    ss << " | Sharpe: " << std::setprecision(3) << sharpe_ratio;
    ss << " | Drawdown: " << std::setprecision(2) << (max_drawdown * 100.0) << "%";
    ss << " | Win Rate: " << std::setprecision(1) << (win_rate * 100.0) << "%";
    
    return ss.str();
}

double PerformanceMetrics::calculate_score() const {
    double score = 0.0;
    
    // MRB component (40 points)
    if (trading_based_mrb >= 0.025) score += 40.0;
    else if (trading_based_mrb >= 0.020) score += 35.0;
    else if (trading_based_mrb >= 0.015) score += 30.0;
    else if (trading_based_mrb >= 0.010) score += 20.0;
    else if (trading_based_mrb >= 0.005) score += 10.0;
    
    // Sharpe ratio component (30 points)
    if (sharpe_ratio >= 1.0) score += 30.0;
    else if (sharpe_ratio >= 0.8) score += 25.0;
    else if (sharpe_ratio >= 0.6) score += 20.0;
    else if (sharpe_ratio >= 0.4) score += 15.0;
    else if (sharpe_ratio >= 0.2) score += 10.0;
    
    // Risk management component (20 points)
    if (max_drawdown <= 0.10) score += 20.0;
    else if (max_drawdown <= 0.15) score += 15.0;
    else if (max_drawdown <= 0.20) score += 10.0;
    else if (max_drawdown <= 0.30) score += 5.0;
    
    // Trading performance component (10 points)
    if (win_rate >= 0.60) score += 10.0;
    else if (win_rate >= 0.55) score += 8.0;
    else if (win_rate >= 0.50) score += 6.0;
    else if (win_rate >= 0.45) score += 4.0;
    
    return score;
}

// SignalQualityMetrics implementation

std::string SignalQualityMetrics::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"distribution\": {\n";
    ss << "    \"long_ratio\": " << long_ratio << ",\n";
    ss << "    \"short_ratio\": " << short_ratio << ",\n";
    ss << "    \"neutral_ratio\": " << neutral_ratio << "\n";
    ss << "  },\n";
    
    ss << "  \"confidence\": {\n";
    ss << "    \"mean\": " << mean_confidence << ",\n";
    ss << "    \"median\": " << median_confidence << ",\n";
    ss << "    \"std_dev\": " << confidence_std_dev << ",\n";
    ss << "    \"min\": " << min_confidence << ",\n";
    ss << "    \"max\": " << max_confidence << "\n";
    ss << "  },\n";
    
    ss << "  \"quality_indicators\": {\n";
    ss << "    \"consistency\": " << signal_consistency << ",\n";
    ss << "    \"stability\": " << signal_stability << ",\n";
    ss << "    \"reversals\": " << signal_reversals << ",\n";
    ss << "    \"consecutive_neutrals\": " << consecutive_neutrals << "\n";
    ss << "  },\n";
    
    ss << "  \"confidence_distribution\": {\n";
    bool first = true;
    for (const auto& [bin, count] : confidence_distribution) {
        if (!first) ss << ",\n";
        ss << "    \"" << bin << "\": " << count;
        first = false;
    }
    ss << "\n  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

// RiskMetrics implementation

std::string RiskMetrics::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"drawdown\": {\n";
    ss << "    \"max_drawdown\": " << max_drawdown << ",\n";
    ss << "    \"avg_drawdown\": " << avg_drawdown << ",\n";
    ss << "    \"current_drawdown\": " << current_drawdown << ",\n";
    ss << "    \"max_drawdown_duration\": " << max_drawdown_duration << ",\n";
    ss << "    \"current_drawdown_duration\": " << current_drawdown_duration << "\n";
    ss << "  },\n";
    
    ss << "  \"volatility\": {\n";
    ss << "    \"volatility\": " << volatility << ",\n";
    ss << "    \"downside_deviation\": " << downside_deviation << ",\n";
    ss << "    \"upside_deviation\": " << upside_deviation << "\n";
    ss << "  },\n";
    
    ss << "  \"value_at_risk\": {\n";
    ss << "    \"var_95\": " << var_95 << ",\n";
    ss << "    \"var_99\": " << var_99 << ",\n";
    ss << "    \"cvar_95\": " << cvar_95 << ",\n";
    ss << "    \"cvar_99\": " << cvar_99 << "\n";
    ss << "  },\n";
    
    ss << "  \"market_exposure\": {\n";
    ss << "    \"beta\": " << beta << ",\n";
    ss << "    \"alpha\": " << alpha << ",\n";
    ss << "    \"tracking_error\": " << tracking_error << "\n";
    ss << "  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

// ComparisonResult implementation (already in test_result.cpp, but adding more methods)

} // namespace sentio::analysis


