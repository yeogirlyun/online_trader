// include/analysis/performance_analyzer.h
#pragma once

#include "performance_metrics.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

// Forward declaration for Enhanced PSM integration
namespace sentio::analysis {
    class EnhancedPerformanceAnalyzer;
}

namespace sentio::analysis {
using MarketData = sentio::Bar;

/**
 * @brief Configuration for PSM-based validation
 */
struct PSMValidationConfig {
    double starting_capital = 100000.0;
    std::string cost_model = "alpaca";  // "alpaca" or "percentage"
    bool leverage_enabled = true;
    bool enable_dynamic_psm = true;
    bool enable_hysteresis = true;
    bool enable_dynamic_allocation = true;
    double slippage_factor = 0.0;
    bool keep_temp_files = false;  // For debugging
    // Default to file-based validation to ensure single source of truth via Enhanced PSM
    // Use a local artifacts directory managed by TempFileManager
    std::string temp_directory = "artifacts/tmp";
};

/**
 * @brief Comprehensive performance analysis engine
 * 
 * Provides detailed analysis of strategy performance including:
 * - MRB calculations (signal-based and trading-based)
 * - Risk-adjusted return metrics
 * - Drawdown analysis
 * - Statistical significance testing
 */
class PerformanceAnalyzer {
public:
    /**
     * @brief Calculate comprehensive performance metrics
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return PerformanceMetrics structure with all metrics
     */
    static PerformanceMetrics calculate_metrics(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Calculate signal directional accuracy
     * @param signals Generated strategy signals
     * @param market_data Market data to compare against
     * @return Signal accuracy (0.0-1.0)
     */
    static double calculate_signal_accuracy(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Calculate trading-based MRB with actual Enhanced PSM simulation
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param config PSM validation configuration (optional)
     * @return Trading-based MRB with full Enhanced PSM
     */
    static double calculate_trading_based_mrb_with_psm(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        const PSMValidationConfig& config = PSMValidationConfig{}
    );

    // Dataset-path overload: preferred for sanity-check to avoid temp CSV schema/index mismatches
    static double calculate_trading_based_mrb_with_psm(
        const std::vector<SignalOutput>& signals,
        const std::string& dataset_csv_path,
        int blocks = 20,
        const PSMValidationConfig& config = PSMValidationConfig{}
    );
    
    /**
     * @brief Calculate trading-based MRB
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return Trading-based Mean Reversion Baseline
     */
    static double calculate_trading_based_mrb(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Calculate MRB across multiple blocks
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return Vector of MRB values for each block
     */
    static std::vector<double> calculate_block_mrbs(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Compare performance across multiple strategies
     * @param strategy_signals Map of strategy name to signals
     * @param market_data Market data for comparison
     * @return ComparisonResult with rankings and comparisons
     */
    static ComparisonResult compare_strategies(
        const std::map<std::string, std::vector<SignalOutput>>& strategy_signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Analyze signal quality
     * @param signals Generated strategy signals
     * @return SignalQualityMetrics structure
     */
    static SignalQualityMetrics analyze_signal_quality(
        const std::vector<SignalOutput>& signals
    );
    
    /**
     * @brief Calculate risk metrics
     * @param equity_curve Equity curve from trading simulation
     * @return RiskMetrics structure
     */
    static RiskMetrics calculate_risk_metrics(
        const std::vector<double>& equity_curve
    );

protected:
    /**
     * @brief Enhanced PSM instance for advanced analysis
     */
    static std::unique_ptr<EnhancedPerformanceAnalyzer> enhanced_analyzer_;

private:
    /**
     * @brief Calculate Sharpe ratio
     * @param returns Vector of returns
     * @param risk_free_rate Risk-free rate (default 0.0)
     * @return Sharpe ratio
     */
    static double calculate_sharpe_ratio(
        const std::vector<double>& returns,
        double risk_free_rate = 0.0
    );
    
    /**
     * @brief Calculate maximum drawdown
     * @param equity_curve Equity curve
     * @return Maximum drawdown as percentage
     */
    static double calculate_max_drawdown(
        const std::vector<double>& equity_curve
    );
    
    /**
     * @brief Calculate win rate
     * @param trades Vector of trade results
     * @return Win rate as percentage
     */
    static double calculate_win_rate(
        const std::vector<double>& trades
    );
    
    /**
     * @brief Calculate profit factor
     * @param trades Vector of trade results
     * @return Profit factor
     */
    static double calculate_profit_factor(
        const std::vector<double>& trades
    );
    
    /**
     * @brief Calculate volatility (standard deviation of returns)
     * @param returns Vector of returns
     * @return Volatility
     */
    static double calculate_volatility(
        const std::vector<double>& returns
    );
    
    /**
     * @brief Calculate Sortino ratio
     * @param returns Vector of returns
     * @param risk_free_rate Risk-free rate
     * @return Sortino ratio
     */
    static double calculate_sortino_ratio(
        const std::vector<double>& returns,
        double risk_free_rate = 0.0
    );
    
    /**
     * @brief Calculate Calmar ratio
     * @param returns Vector of returns
     * @param equity_curve Equity curve
     * @return Calmar ratio
     */
    static double calculate_calmar_ratio(
        const std::vector<double>& returns,
        const std::vector<double>& equity_curve
    );
    
    /**
     * @brief Simulate trading based on signals
     * @param signals Strategy signals
     * @param market_data Market data
     * @return Equity curve and trade results
     */
    static std::pair<std::vector<double>, std::vector<double>> simulate_trading(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Calculate returns from equity curve
     * @param equity_curve Equity curve
     * @return Vector of returns
     */
    static std::vector<double> calculate_returns(
        const std::vector<double>& equity_curve
    );
};

/**
 * @brief Walk-forward analysis engine
 */
class WalkForwardAnalyzer {
public:
    struct WalkForwardConfig {
        int window_size = 252;      // Training window size
        int step_size = 21;          // Step size for rolling
        int min_window_size = 126;   // Minimum window size
    };
    
    struct WalkForwardResult {
        std::vector<PerformanceMetrics> in_sample_metrics;
        std::vector<PerformanceMetrics> out_of_sample_metrics;
        double avg_in_sample_mrb = 0.0;
        double avg_out_of_sample_mrb = 0.0;
        double stability_ratio = 0.0;  // out-of-sample / in-sample
        int num_windows = 0;
    };
    
    /**
     * @brief Perform walk-forward analysis
     */
    static WalkForwardResult analyze(
        const std::string& strategy_name,
        const std::vector<MarketData>& market_data,
        const WalkForwardConfig& config
    );
};

/**
 * @brief Stress testing engine
 */
class StressTestAnalyzer {
public:
    enum class StressScenario {
        MARKET_CRASH,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        TRENDING_UP,
        TRENDING_DOWN,
        SIDEWAYS,
        MISSING_DATA,
        EXTREME_OUTLIERS
    };
    
    struct StressTestResult {
        StressScenario scenario;
        std::string scenario_name;
        PerformanceMetrics metrics;
        bool passed;
        std::string description;
    };
    
    /**
     * @brief Run stress tests
     */
    static std::vector<StressTestResult> run_stress_tests(
        const std::string& strategy_name,
        const std::vector<MarketData>& base_market_data,
        const std::vector<StressScenario>& scenarios
    );
    
private:
    /**
     * @brief Apply stress scenario to market data
     */
    static std::vector<MarketData> apply_stress_scenario(
        const std::vector<MarketData>& market_data,
        StressScenario scenario
    );
};

} // namespace sentio::analysis

