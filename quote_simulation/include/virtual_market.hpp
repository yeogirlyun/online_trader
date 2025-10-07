#pragma once

#include "sentio/core.hpp"
#include "sentio/base_strategy.hpp"
#include "sentio/runner.hpp"
#include "sentio/unified_metrics.hpp"
#include <vector>
#include <random>
#include <memory>
#include <chrono>

namespace sentio {

// **PROFIT MAXIMIZATION**: Generate theoretical leverage series for maximum capital deployment
std::vector<Bar> generate_theoretical_leverage_series(const std::vector<Bar>& base_series, double leverage_factor);

/**
 * Virtual Market Engine for Monte Carlo Strategy Testing
 * 
 * Provides synthetic market data generation and Monte Carlo simulation
 * capabilities integrated with the real Sentio strategy components.
 */
class VirtualMarketEngine {
public:
    struct MarketRegime {
        std::string name;
        double volatility;      // Daily volatility (0.01 = 1%)
        double trend;          // Daily trend (-0.05 to +0.05)
        double mean_reversion; // Mean reversion strength (0.0 to 1.0)
        double volume_multiplier; // Volume scaling factor
        int duration_minutes;   // How long this regime lasts
    };

    struct VMSimulationResult {
        // LEGACY: Old calculated metrics (for backward compatibility)
        double total_return;
        double final_capital;
        double sharpe_ratio;
        double max_drawdown;
        double win_rate;
        int total_trades;
        double monthly_projected_return;
        double daily_trades;
        
        // NEW: Raw backtest output for unified metrics calculation
        BacktestOutput raw_output;
        
        // NEW: Unified metrics report (calculated from raw_output)
        UnifiedMetricsReport unified_metrics;
        
        // Audit Information
        std::string run_id;            // Run ID for audit verification
    };

    struct VMTestConfig {
        std::string strategy_name;
        std::string symbol;
        int days = 30;
        int hours = 0;
        int simulations = 100;
        bool fast_mode = true;
        std::string params_json = "{}";
        double initial_capital = 100000.0;
    };

    VirtualMarketEngine();
    ~VirtualMarketEngine() = default;

    /**
     * Generate synthetic market data for a symbol
     */
    std::vector<Bar> generate_market_data(const std::string& symbol, 
                                         int periods, 
                                         int interval_seconds = 60);

    /**
     * Run Monte Carlo simulation with real strategy components
     */
    std::vector<VMSimulationResult> run_monte_carlo_simulation(
        const VMTestConfig& config,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg);
    
    /**
     * Run Monte Carlo simulation using MarS-generated realistic data
     */
    std::vector<VMSimulationResult> run_mars_monte_carlo_simulation(
        const VMTestConfig& config,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg,
        const std::string& market_regime = "normal");
    
    /**
     * Run MarS virtual market test (convenience method)
     */
    std::vector<VMSimulationResult> run_mars_vm_test(const std::string& strategy_name,
                                                    const std::string& symbol,
                                                    int days,
                                                    int simulations,
                                                    const std::string& market_regime = "normal",
                                                    const std::string& params_json = "");

    /**
     * Run fast historical test using optimized historical patterns
     */
    std::vector<VMSimulationResult> run_fast_historical_test(const std::string& strategy_name,
                                                            const std::string& symbol,
                                                            const std::string& historical_data_file,
                                                            int continuation_minutes,
                                                            int simulations,
                                                            const std::string& params_json = "");

    /**
     * Run single historical test on actual historical data (no simulations)
     */
    std::vector<VMSimulationResult> run_single_historical_test(const std::string& strategy_name,
                                                              const std::string& symbol,
                                                              const std::string& historical_data_file,
                                                              int continuation_minutes,
                                                              const std::string& params_json = "");

    /**
     * Run future QQQ regime test using pre-generated future data
     */
    std::vector<VMSimulationResult> run_future_qqq_regime_test(const std::string& strategy_name,
                                                              const std::string& symbol,
                                                              int simulations,
                                                              const std::string& regime,
                                                              const std::string& params_json = "");

    /**
     * Run single simulation with given market data
     */
    VMSimulationResult run_single_simulation(
        const std::vector<Bar>& market_data,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg,
        double initial_capital);

    /**
     * Generate comprehensive test report
     */
    void print_simulation_report(const std::vector<VMSimulationResult>& results,
                                const VMTestConfig& config);

private:
    std::mt19937 rng_;
    std::vector<MarketRegime> market_regimes_;
    std::unordered_map<std::string, double> base_prices_;
    
    MarketRegime current_regime_;
    std::chrono::system_clock::time_point regime_start_time_;
    
    void initialize_market_regimes();
    void select_new_regime();
    bool should_change_regime();
    
    Bar generate_market_bar(const std::string& symbol, 
                           std::chrono::system_clock::time_point timestamp);
    
    double calculate_price_movement(const std::string& symbol, 
                                   double current_price,
                                   const MarketRegime& regime);
    
    VMSimulationResult calculate_performance_metrics(
        const std::vector<double>& returns,
        const std::vector<int>& trades,
        double initial_capital);
};

/**
 * Virtual Market Test Runner
 * 
 * High-level interface for running VM tests with real Sentio components
 */
class VMTestRunner {
public:
    VMTestRunner() = default;
    ~VMTestRunner() = default;

    /**
     * Run virtual market test with real strategy components
     */
    int run_vmtest(const VirtualMarketEngine::VMTestConfig& config);

private:
    VirtualMarketEngine vm_engine_;
    
    std::unique_ptr<BaseStrategy> create_strategy(const std::string& strategy_name,
                                                  const std::string& params_json);
    
    RunnerCfg load_strategy_config(const std::string& strategy_name);
    
    void print_progress(int current, int total, double elapsed_time);
};

} // namespace sentio
