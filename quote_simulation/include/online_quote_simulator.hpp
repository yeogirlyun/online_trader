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

/**
 * Online Quote Simulation Engine
 * 
 * Provides real-time quote simulation for online learning algorithms
 * with integration to MarS (Microsoft Research Market Simulation Engine)
 * for realistic market data generation.
 */
class OnlineQuoteSimulator {
public:
    struct MarketRegime {
        std::string name;
        double volatility;      // Daily volatility (0.01 = 1%)
        double trend;          // Daily trend (-0.05 to +0.05)
        double mean_reversion; // Mean reversion strength (0.0 to 1.0)
        double volume_multiplier; // Volume scaling factor
        int duration_minutes;   // How long this regime lasts
    };

    struct QuoteData {
        std::time_t timestamp;
        std::string symbol;
        double bid_price;
        double ask_price;
        int bid_size;
        int ask_size;
        double last_price;
        int last_size;
        double volume;
    };

    struct SimulationResult {
        // Performance metrics
        double total_return;
        double final_capital;
        double sharpe_ratio;
        double max_drawdown;
        double win_rate;
        int total_trades;
        double monthly_projected_return;
        double daily_trades;
        
        // Raw backtest output for unified metrics calculation
        BacktestOutput raw_output;
        
        // Unified metrics report
        UnifiedMetricsReport unified_metrics;
        
        // Simulation metadata
        std::string run_id;
        std::string market_regime;
        int simulation_duration_minutes;
    };

    struct OnlineTestConfig {
        std::string strategy_name;
        std::string symbol;
        int duration_minutes = 60;
        int simulations = 10;
        bool use_mars = true;
        std::string params_json = "{}";
        double initial_capital = 100000.0;
        std::string market_regime = "normal";
        bool real_time_mode = false;
        int quote_interval_ms = 1000; // 1 second quotes
    };

    OnlineQuoteSimulator();
    ~OnlineQuoteSimulator() = default;

    /**
     * Generate real-time quotes for online learning
     */
    std::vector<QuoteData> generate_realtime_quotes(const std::string& symbol, 
                                                   int duration_minutes,
                                                   int interval_ms = 1000);

    /**
     * Run online learning simulation with MarS integration
     */
    std::vector<SimulationResult> run_online_simulation(
        const OnlineTestConfig& config,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg);
    
    /**
     * Run MarS-powered realistic simulation for online learning
     */
    std::vector<SimulationResult> run_mars_online_simulation(
        const OnlineTestConfig& config,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg);
    
    /**
     * Run single online learning test
     */
    std::vector<SimulationResult> run_online_test(const std::string& strategy_name,
                                                  const std::string& symbol,
                                                  int duration_minutes,
                                                  int simulations,
                                                  const std::string& market_regime = "normal",
                                                  const std::string& params_json = "");

    /**
     * Run historical continuation test (start from real data, continue with simulation)
     */
    std::vector<SimulationResult> run_historical_continuation_test(
        const std::string& strategy_name,
        const std::string& symbol,
        const std::string& historical_data_file,
        int continuation_minutes,
        int simulations,
        const std::string& params_json = "");

    /**
     * Run single simulation with given market data
     */
    SimulationResult run_single_online_simulation(
        const std::vector<Bar>& market_data,
        std::unique_ptr<BaseStrategy> strategy,
        const RunnerCfg& runner_cfg,
        double initial_capital);

    /**
     * Generate comprehensive online learning report
     */
    void print_online_simulation_report(const std::vector<SimulationResult>& results,
                                       const OnlineTestConfig& config);

    /**
     * Start real-time quote streaming (for live testing)
     */
    void start_realtime_streaming(const std::string& symbol,
                                 std::function<void(const QuoteData&)> quote_callback,
                                 int interval_ms = 1000);

    /**
     * Stop real-time quote streaming
     */
    void stop_realtime_streaming();

private:
    std::mt19937 rng_;
    std::vector<MarketRegime> market_regimes_;
    std::unordered_map<std::string, double> base_prices_;
    
    MarketRegime current_regime_;
    std::chrono::system_clock::time_point regime_start_time_;
    
    // Real-time streaming
    std::atomic<bool> streaming_active_;
    std::thread streaming_thread_;
    
    void initialize_market_regimes();
    void select_new_regime();
    bool should_change_regime();
    
    QuoteData generate_quote(const std::string& symbol, 
                           std::chrono::system_clock::time_point timestamp);
    
    double calculate_price_movement(const std::string& symbol, 
                                   double current_price,
                                   const MarketRegime& regime);
    
    SimulationResult calculate_online_performance_metrics(
        const std::vector<double>& returns,
        const std::vector<int>& trades,
        double initial_capital);
    
    void streaming_worker(const std::string& symbol,
                         std::function<void(const QuoteData&)> quote_callback,
                         int interval_ms);
};

/**
 * Mock Polygon API for Online Learning
 * 
 * Provides a drop-in replacement for Polygon API calls
 * using MarS-generated realistic market data
 */
class MockPolygonAPI {
public:
    struct PolygonQuote {
        std::string symbol;
        double bid;
        double ask;
        int bid_size;
        int ask_size;
        double last;
        int last_size;
        std::time_t timestamp;
    };

    MockPolygonAPI(const std::string& api_key = "mock_key");
    ~MockPolygonAPI() = default;

    /**
     * Get real-time quote (replaces Polygon API call)
     */
    PolygonQuote get_realtime_quote(const std::string& symbol);

    /**
     * Get historical bars (replaces Polygon API call)
     */
    std::vector<Bar> get_historical_bars(const std::string& symbol,
                                         const std::string& start_date,
                                         const std::string& end_date,
                                         const std::string& timespan = "minute");

    /**
     * Subscribe to real-time quotes (replaces Polygon WebSocket)
     */
    void subscribe_to_quotes(const std::string& symbol,
                            std::function<void(const PolygonQuote&)> callback);

    /**
     * Unsubscribe from quotes
     */
    void unsubscribe_from_quotes(const std::string& symbol);

    /**
     * Enable MarS integration for realistic data
     */
    void enable_mars_integration(bool enable = true);

    /**
     * Set market regime for simulation
     */
    void set_market_regime(const std::string& regime);

private:
    std::string api_key_;
    bool mars_enabled_;
    std::string current_regime_;
    std::unique_ptr<OnlineQuoteSimulator> quote_simulator_;
    std::unordered_map<std::string, std::function<void(const PolygonQuote&)>> subscriptions_;
    std::thread subscription_thread_;
    std::atomic<bool> subscription_active_;
    
    void subscription_worker();
    PolygonQuote generate_mock_quote(const std::string& symbol);
};

/**
 * Online Learning Test Runner
 * 
 * High-level interface for running online learning tests
 * with quote simulation
 */
class OnlineTestRunner {
public:
    OnlineTestRunner() = default;
    ~OnlineTestRunner() = default;

    /**
     * Run online learning test with quote simulation
     */
    int run_online_test(const OnlineQuoteSimulator::OnlineTestConfig& config);

    /**
     * Run real-time online learning test
     */
    int run_realtime_test(const std::string& strategy_name,
                         const std::string& symbol,
                         int duration_minutes);

private:
    OnlineQuoteSimulator quote_simulator_;
    
    std::unique_ptr<BaseStrategy> create_strategy(const std::string& strategy_name,
                                                  const std::string& params_json);
    
    RunnerCfg load_strategy_config(const std::string& strategy_name);
    
    void print_progress(int current, int total, double elapsed_time);
};

} // namespace sentio
