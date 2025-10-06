// include/analysis/performance_metrics.h
#pragma once

#include <string>
#include <vector>
#include <map>

namespace sentio::analysis {

/**
 * @brief Comprehensive performance metrics structure
 * 
 * Contains all metrics for evaluating strategy performance including
 * MRB, risk-adjusted returns, trading metrics, and signal quality.
 */
struct PerformanceMetrics {
    // Strategy identification
    std::string strategy_name;
    std::string dataset_name;
    
    // MRB metrics
    double signal_accuracy = 0.0;  // Directional accuracy (0.0-1.0)
    double trading_based_mrb = 0.0;
    double mrb_consistency = 0.0;
    std::vector<double> block_mrbs;
    
    // Return metrics
    double total_return = 0.0;
    double annualized_return = 0.0;
    double cumulative_return = 0.0;
    
    // Risk-adjusted metrics
    double sharpe_ratio = 0.0;
    double sortino_ratio = 0.0;
    double calmar_ratio = 0.0;
    double information_ratio = 0.0;
    
    // Risk metrics
    double max_drawdown = 0.0;
    double avg_drawdown = 0.0;
    double volatility = 0.0;
    double downside_deviation = 0.0;
    
    // Trading metrics
    double win_rate = 0.0;
    double profit_factor = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0;
    double largest_win = 0.0;
    double largest_loss = 0.0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    
    // Signal metrics
    int total_signals = 0;
    int non_neutral_signals = 0;
    int long_signals = 0;
    int short_signals = 0;
    int neutral_signals = 0;
    double signal_generation_rate = 0.0;
    double non_neutral_ratio = 0.0;
    double mean_confidence = 0.0;
    
    // Execution metrics
    double execution_time_ms = 0.0;
    double avg_inference_time_ms = 0.0;
    double memory_usage_mb = 0.0;
    
    // Additional metrics
    std::map<std::string, double> custom_metrics;
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
    
    /**
     * @brief Get summary string
     */
    std::string get_summary() const;
    
    /**
     * @brief Calculate overall score (0-100)
     */
    double calculate_score() const;
};

/**
 * @brief Signal quality metrics
 */
struct SignalQualityMetrics {
    // Distribution metrics
    double long_ratio = 0.0;
    double short_ratio = 0.0;
    double neutral_ratio = 0.0;
    
    // Confidence metrics
    double mean_confidence = 0.0;
    double median_confidence = 0.0;
    double confidence_std_dev = 0.0;
    double min_confidence = 0.0;
    double max_confidence = 0.0;
    
    // Quality indicators
    double signal_consistency = 0.0;
    double signal_stability = 0.0;
    int signal_reversals = 0;
    int consecutive_neutrals = 0;
    
    // Confidence distribution by bins
    std::map<std::string, int> confidence_distribution;
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
};

/**
 * @brief Risk metrics structure
 */
struct RiskMetrics {
    // Drawdown metrics
    double max_drawdown = 0.0;
    double avg_drawdown = 0.0;
    double current_drawdown = 0.0;
    int max_drawdown_duration = 0;
    int current_drawdown_duration = 0;
    
    // Volatility metrics
    double volatility = 0.0;
    double downside_deviation = 0.0;
    double upside_deviation = 0.0;
    
    // Value at Risk
    double var_95 = 0.0;
    double var_99 = 0.0;
    double cvar_95 = 0.0;
    double cvar_99 = 0.0;
    
    // Additional risk metrics
    double beta = 0.0;
    double alpha = 0.0;
    double tracking_error = 0.0;
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
};

/**
 * @brief Comparison result for multiple strategies
 */
struct ComparisonResult {
    std::map<std::string, PerformanceMetrics> strategy_metrics;
    std::string best_strategy;
    std::string worst_strategy;
    std::vector<std::pair<std::string, double>> rankings;  // strategy name, score
    std::map<std::string, std::string> comparisons;
    
    // Statistical comparison
    std::map<std::string, double> statistical_significance;
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
    
    /**
     * @brief Convert to comparison report
     */
    std::string to_report() const;
};

} // namespace sentio::analysis


