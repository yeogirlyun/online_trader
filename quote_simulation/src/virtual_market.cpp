#include "sentio/virtual_market.hpp"
#include "sentio/runner.hpp"
// #include "sentio/temporal_analysis.hpp" // Removed during cleanup
#include "sentio/symbol_table.hpp"
#include "sentio/audit.hpp"
#include "audit/audit_db_recorder.hpp"
#include "sentio/base_strategy.hpp"
#include "sentio/mars_data_loader.hpp"
#include "sentio/future_qqq_loader.hpp"
#include "sentio/leverage_aware_csv_loader.hpp"
#include "sentio/run_id_generator.hpp"
#include "sentio/dataset_metadata.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <chrono>

namespace sentio {

// **PROFIT MAXIMIZATION**: Generate theoretical leverage series for maximum capital deployment
std::vector<Bar> generate_theoretical_leverage_series(const std::vector<Bar>& base_series, double leverage_factor) {
    std::vector<Bar> leverage_series;
    leverage_series.reserve(base_series.size());
    
    if (base_series.empty()) return leverage_series;
    
    // Initialize with first bar (no leverage effect on first bar)
    Bar first_bar = base_series[0];
    leverage_series.push_back(first_bar);
    
    // Generate subsequent bars with leverage effect
    for (size_t i = 1; i < base_series.size(); ++i) {
        const Bar& prev_base = base_series[i-1];
        const Bar& curr_base = base_series[i];
        const Bar& prev_leverage = leverage_series[i-1];
        
        // Calculate base return
        double base_return = (curr_base.close - prev_base.close) / prev_base.close;
        
        // Apply leverage factor
        double leverage_return = base_return * leverage_factor;
        
        // Calculate new leverage prices
        Bar leverage_bar;
        leverage_bar.ts_utc_epoch = curr_base.ts_utc_epoch;
        leverage_bar.close = prev_leverage.close * (1.0 + leverage_return);
        
        // Approximate OHLV based on close price movement
        double price_ratio = leverage_bar.close / prev_leverage.close;
        leverage_bar.open = prev_leverage.close;  // Open at previous close
        leverage_bar.high = std::max(leverage_bar.open, leverage_bar.close) * 1.001;  // Small spread
        leverage_bar.low = std::min(leverage_bar.open, leverage_bar.close) * 0.999;   // Small spread
        leverage_bar.volume = curr_base.volume;  // Use base volume
        
        leverage_series.push_back(leverage_bar);
    }
    
    return leverage_series;
}

VirtualMarketEngine::VirtualMarketEngine() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_market_regimes();
    
    // Initialize base prices for common symbols (updated to match real market levels)
    base_prices_["QQQ"] = 458.0; // Updated to match real QQQ levels
    base_prices_["SPY"] = 450.0;
    base_prices_["AAPL"] = 175.0;
    base_prices_["MSFT"] = 350.0;
    base_prices_["TSLA"] = 250.0;
    // PSQ removed - moderate sell signals now use SHORT QQQ
    base_prices_["TQQQ"] = 120.0; // 3x QQQ
    base_prices_["SQQQ"] = 120.0; // 3x inverse QQQ
}

void VirtualMarketEngine::initialize_market_regimes() {
    market_regimes_ = {
        {"BULL_TRENDING", 0.015, 0.02, 0.3, 1.2, 60},
        {"BEAR_TRENDING", 0.025, -0.015, 0.2, 1.5, 45},
        {"SIDEWAYS_LOW_VOL", 0.008, 0.001, 0.8, 0.8, 90},
        {"SIDEWAYS_HIGH_VOL", 0.020, 0.002, 0.6, 1.3, 30},
        {"VOLATILE_BREAKOUT", 0.035, 0.025, 0.1, 2.0, 15},
        {"VOLATILE_BREAKDOWN", 0.040, -0.030, 0.1, 2.2, 20},
        {"NORMAL_MARKET", 0.008, 0.001, 0.5, 1.0, 120}
    };
    
    // Select initial regime
    select_new_regime();
}

void VirtualMarketEngine::select_new_regime() {
    // Weight regimes by probability (normal market is most common)
    std::vector<double> weights = {0.15, 0.10, 0.20, 0.15, 0.05, 0.05, 0.30};
    
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    int regime_index = dist(rng_);
    
    current_regime_ = market_regimes_[regime_index];
    regime_start_time_ = std::chrono::system_clock::now();
    
    // Debug: Regime switching (commented out to reduce console noise)
    // std::cout << "🔄 Switched to market regime: " << current_regime_.name 
    //           << " (vol=" << std::fixed << std::setprecision(3) << current_regime_.volatility
    //           << ", trend=" << current_regime_.trend << ")" << std::endl;
}

bool VirtualMarketEngine::should_change_regime() {
    if (current_regime_.name.empty()) return true;
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - regime_start_time_);
    return elapsed.count() >= current_regime_.duration_minutes;
}

std::vector<Bar> VirtualMarketEngine::generate_market_data(const std::string& symbol, 
                                                         int periods, 
                                                         int interval_seconds) {
    std::vector<Bar> bars;
    bars.reserve(periods);
    
    auto current_time = std::chrono::system_clock::now();
    double current_price = base_prices_[symbol];
    
    for (int i = 0; i < periods; ++i) {
        // Change regime periodically
        if (i % 100 == 0) {
            select_new_regime();
        }
        
        Bar bar = generate_market_bar(symbol, current_time);
        bars.push_back(bar);
        
        current_price = bar.close;
        current_time += std::chrono::seconds(interval_seconds);
    }
    
    std::cout << "✅ Generated " << bars.size() << " bars for " << symbol << std::endl;
    return bars;
}

Bar VirtualMarketEngine::generate_market_bar(const std::string& symbol, 
                                           std::chrono::system_clock::time_point timestamp) {
    double current_price = base_prices_[symbol];
    
    // Calculate price movement
    double price_move = calculate_price_movement(symbol, current_price, current_regime_);
    double new_price = current_price * (1 + price_move);
    
    // Generate OHLC from price movement
    double open_price = current_price;
    double close_price = new_price;
    
    // High and low with realistic intrabar movement
    double intrabar_range = std::abs(price_move) * (0.3 + (rng_() % 50) / 100.0);
    double high_price = std::max(open_price, close_price) + current_price * intrabar_range * (rng_() % 100) / 100.0;
    double low_price = std::min(open_price, close_price) - current_price * intrabar_range * (rng_() % 100) / 100.0;
    
    // Realistic volume model based on real QQQ data
    int base_volume = 80000 + (rng_() % 120000); // 80K-200K base volume
    double volume_multiplier = current_regime_.volume_multiplier * (1 + std::abs(price_move) * 2.0);
    
    // Add time-of-day volume patterns (higher volume during market open/close)
    double time_multiplier = 1.0;
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm* tm_info = std::localtime(&time_t);
    int hour = tm_info->tm_hour;
    
    // Market hours volume patterns (9:30-16:00 ET)
    if (hour >= 9 && hour <= 16) {
        if (hour == 9 || hour == 16) {
            time_multiplier = 1.5; // Higher volume at open/close
        } else if (hour == 10 || hour == 15) {
            time_multiplier = 1.2; // Moderate volume
        } else {
            time_multiplier = 1.0; // Normal volume
        }
    } else {
        time_multiplier = 0.3; // Lower volume outside market hours
    }
    
    int volume = static_cast<int>(base_volume * volume_multiplier * time_multiplier);
    
    Bar bar;
    bar.ts_utc_epoch = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
    bar.open = open_price;
    bar.high = high_price;
    bar.low = low_price;
    bar.close = close_price;
    bar.volume = volume;
    
    return bar;
}

double VirtualMarketEngine::calculate_price_movement(const std::string& symbol, 
                                                    double current_price,
                                                    const MarketRegime& regime) {
    // Calculate price movement components
    double dt = 1.0 / (252 * 390); // 1 minute as fraction of trading year
    
    // Trend component
    double trend_move = regime.trend * dt;
    
    // Random walk component
    std::normal_distribution<double> normal_dist(0.0, 1.0);
    double volatility_move = regime.volatility * std::sqrt(dt) * normal_dist(rng_);
    
    // Mean reversion component
    double base_price = base_prices_[symbol];
    double reversion_move = regime.mean_reversion * (base_price - current_price) * dt * 0.01;
    
    // Microstructure noise
    double microstructure_noise = normal_dist(rng_) * 0.0005;
    
    return trend_move + volatility_move + reversion_move + microstructure_noise;
}

std::vector<VirtualMarketEngine::VMSimulationResult> VirtualMarketEngine::run_monte_carlo_simulation(
    const VMTestConfig& config,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg) {
    
    std::cout << "🔄 FIXED DATA: Using Future QQQ tracks instead of random Monte Carlo..." << std::endl;
    
    // **FIXED DATA REPLACEMENT**: Use Future QQQ regime test instead of random generation
    return run_future_qqq_regime_test(
        config.strategy_name, 
        config.symbol, 
        config.simulations, 
        "normal",  // Default to normal regime for Monte Carlo replacement
        config.params_json
    );
}

std::vector<VirtualMarketEngine::VMSimulationResult> VirtualMarketEngine::run_mars_monte_carlo_simulation(
    const VMTestConfig& config,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg,
    const std::string& market_regime) {
    
    std::cout << "🔄 FIXED DATA: Using Future QQQ tracks instead of random MarS generation..." << std::endl;
    
    // **FIXED DATA REPLACEMENT**: Use Future QQQ regime test instead of random MarS generation
    return run_future_qqq_regime_test(
        config.strategy_name, 
        config.symbol, 
        config.simulations, 
        market_regime,  // Use the requested regime
        config.params_json
    );
}

std::vector<VirtualMarketEngine::VMSimulationResult> VirtualMarketEngine::run_mars_vm_test(
    const std::string& strategy_name,
    const std::string& symbol,
    int days,
    int simulations,
    const std::string& market_regime,
    const std::string& params_json) {
    
    // Create VM test configuration
    VMTestConfig config;
    config.strategy_name = strategy_name;
    config.symbol = symbol;
    config.days = days;
    config.simulations = simulations;
    config.params_json = params_json;
    config.initial_capital = 100000.0;
    
    // Create strategy instance
    auto strategy = StrategyFactory::instance().create_strategy(strategy_name);
    if (!strategy) {
        std::cerr << "Error: Could not create strategy " << strategy_name << std::endl;
        return {};
    }
    
    // Create runner configuration
    RunnerCfg runner_cfg;
    runner_cfg.strategy_name = strategy_name;
    runner_cfg.strategy_params["buy_hi"] = "0.6";
    runner_cfg.strategy_params["sell_lo"] = "0.4";
    
    // Run MarS Monte Carlo simulation
    return run_mars_monte_carlo_simulation(config, std::move(strategy), runner_cfg, market_regime);
}

std::vector<VirtualMarketEngine::VMSimulationResult> VirtualMarketEngine::run_future_qqq_regime_test(
    const std::string& strategy_name,
    const std::string& symbol,
    int simulations,
    const std::string& market_regime,
    const std::string& params_json) {
    
    std::vector<VMSimulationResult> results;
    results.reserve(simulations);
    
    std::cout << "🚀 Starting Future QQQ Regime Test..." << std::endl;
    std::cout << "📊 Strategy: " << strategy_name << std::endl;
    std::cout << "📈 Symbol: " << symbol << std::endl;
    std::cout << "🎯 Market Regime: " << market_regime << std::endl;
    std::cout << "🎲 Simulations: " << simulations << std::endl;
    
    // Validate future QQQ tracks are available
    if (!FutureQQQLoader::validate_tracks()) {
        std::cerr << "❌ Future QQQ tracks validation failed" << std::endl;
        return results;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < simulations; ++i) {
        // Progress reporting
        if (simulations >= 5 && ((i + 1) % std::max(1, simulations / 2) == 0 || i == simulations - 1)) {
            double progress_pct = (100.0 * (i + 1)) / simulations;
            std::cout << "📊 Progress: " << std::fixed << std::setprecision(0) << progress_pct 
                      << "% (" << (i + 1) << "/" << simulations << ")" << std::endl;
        }
        
        try {
            // Load a random track for the specified regime
            // Use simulation index as seed for reproducible results
            auto future_data = FutureQQQLoader::load_regime_track(market_regime, i);
            
            if (future_data.empty()) {
                std::cerr << "Warning: No future QQQ data loaded for simulation " << (i + 1) << std::endl;
                results.push_back(VMSimulationResult{}); // Add empty result
                continue;
            }
            
            // Create new strategy instance for this simulation
            auto strategy_instance = StrategyFactory::instance().create_strategy(strategy_name);
            
            if (!strategy_instance) {
                std::cerr << "Error: Could not create strategy instance for simulation " << (i + 1) << std::endl;
                results.push_back(VMSimulationResult{}); // Add empty result
                continue;
            }
            
            // Create runner configuration
            RunnerCfg runner_cfg;
            runner_cfg.strategy_name = strategy_name;
            runner_cfg.strategy_params["buy_hi"] = "0.6";
            runner_cfg.strategy_params["sell_lo"] = "0.4";
            runner_cfg.audit_level = AuditLevel::Full; // Ensure full audit logging
            
            // Run simulation with future QQQ data
            auto result = run_single_simulation(
                future_data, std::move(strategy_instance), runner_cfg, 100000.0
            );
            
            results.push_back(result);
            
        } catch (const std::exception& e) {
            std::cerr << "Error in future QQQ simulation " << (i + 1) << ": " << e.what() << std::endl;
            results.push_back(VMSimulationResult{}); // Add empty result
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    
    std::cout << "⏱️  Completed " << simulations << " future QQQ simulations in " 
              << total_time << " seconds" << std::endl;
    
    // Print comprehensive report
    VMTestConfig config;
    config.strategy_name = strategy_name;
    config.symbol = symbol;
    config.simulations = simulations;
    config.params_json = params_json;
    config.initial_capital = 100000.0;
    print_simulation_report(results, config);
    
    return results;
}

std::vector<VirtualMarketEngine::VMSimulationResult> VirtualMarketEngine::run_fast_historical_test(
    const std::string& strategy_name,
    const std::string& symbol,
    const std::string& historical_data_file,
    int continuation_minutes,
    int simulations,
    const std::string& params_json) {
    
    std::cout << "🔄 FIXED DATA: Using Future QQQ tracks instead of random fast historical generation..." << std::endl;
    std::cout << "📊 Strategy: " << strategy_name << std::endl;
    std::cout << "📈 Symbol: " << symbol << std::endl;
    std::cout << "🎲 Simulations: " << simulations << std::endl;
    
    // **FIXED DATA REPLACEMENT**: Use Future QQQ regime test instead of random fast historical generation
    return run_future_qqq_regime_test(
        strategy_name, 
        symbol, 
        simulations, 
        "normal",  // Default to normal regime for historical replacement
        params_json
    );
}

VirtualMarketEngine::VMSimulationResult VirtualMarketEngine::run_single_simulation(
    const std::vector<Bar>& market_data,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg,
    double initial_capital) {
    
    // REAL INTEGRATION: Use actual Runner component
    VMSimulationResult result;
    
    try {
        // 1. Create SymbolTable for QQQ family (profit maximization requires all leverage instruments)
        SymbolTable ST;
        int qqq_id = ST.intern("QQQ");
        int tqqq_id = ST.intern("TQQQ");  // 3x leveraged long
        int sqqq_id = ST.intern("SQQQ");  // 3x leveraged short
        int psq_id = ST.intern("PSQ");    // 1x inverse
        
        // 2. Create series data structure (QQQ family for maximum leverage)
        std::vector<std::vector<Bar>> series(4);
        series[qqq_id] = market_data;  // Base QQQ data
        
        // Generate theoretical leverage data for profit maximization
        std::cout << "🚀 Generating theoretical leverage data for maximum profit..." << std::endl;
        
        // Generate theoretical leverage series directly from QQQ data
        series[tqqq_id] = generate_theoretical_leverage_series(market_data, 3.0);   // 3x leveraged long
        series[sqqq_id] = generate_theoretical_leverage_series(market_data, -3.0);  // 3x leveraged short  
        series[psq_id] = generate_theoretical_leverage_series(market_data, -1.0);   // 1x inverse
        
        std::cout << "✅ TQQQ theoretical data generated (" << series[tqqq_id].size() << " bars, 3x leverage)" << std::endl;
        std::cout << "✅ SQQQ theoretical data generated (" << series[sqqq_id].size() << " bars, -3x leverage)" << std::endl;
        std::cout << "✅ PSQ theoretical data generated (" << series[psq_id].size() << " bars, -1x leverage)" << std::endl;
        
        // 3. Create audit recorder - use in-memory database if audit logging is disabled
        std::string run_id = generate_run_id();
        std::string note = "Strategy: " + runner_cfg.strategy_name + ", Test: vm_test, Generated by strattest";
        
        // Use in-memory database if audit logging is disabled to prevent conflicts
        std::string db_path = (runner_cfg.audit_level == AuditLevel::MetricsOnly) ? ":memory:" : "audit/sentio_audit.sqlite3";
        audit::AuditDBRecorder audit(db_path, run_id, note);
        
        // 4. Run REAL backtest using actual Runner (now returns raw BacktestOutput)
        // **DATASET TRACEABILITY**: Pass comprehensive dataset metadata
        DatasetMetadata dataset_meta;
        dataset_meta.source_type = "future_qqq_track";
        dataset_meta.file_path = "data/future_qqq/future_qqq_track_01.csv";
        dataset_meta.track_id = "track_01";
        dataset_meta.regime = "normal"; // Default regime for fixed data
        dataset_meta.bars_count = static_cast<int>(market_data.size());
        if (!market_data.empty()) {
            dataset_meta.time_range_start = market_data.front().ts_utc_epoch * 1000;
            dataset_meta.time_range_end = market_data.back().ts_utc_epoch * 1000;
        }
        BacktestOutput backtest_output = run_backtest(audit, ST, series, qqq_id, runner_cfg, dataset_meta);
        
        // Suppress debug output for cleaner console
        
        // 5. NEW: Store raw output and calculate unified metrics
        result.raw_output = backtest_output;
        result.unified_metrics = UnifiedMetricsCalculator::calculate_metrics(backtest_output);
        result.run_id = run_id;  // Store run ID for audit verification
        
        // 6. Populate metrics for backward compatibility
        result.total_return = result.unified_metrics.total_return;
        result.final_capital = result.unified_metrics.final_equity;
        result.sharpe_ratio = result.unified_metrics.sharpe_ratio;
        result.max_drawdown = result.unified_metrics.max_drawdown;
        result.win_rate = 0.0; // Not calculated in unified metrics yet
        result.total_trades = result.unified_metrics.total_fills;
        result.monthly_projected_return = result.unified_metrics.monthly_projected_return;
        result.daily_trades = result.unified_metrics.avg_daily_trades;
        
        // Suppress individual simulation output for cleaner console
        
    } catch (const std::exception& e) {
        std::cerr << "❌ VM simulation failed: " << e.what() << std::endl;
        
        // Return zero results on error
        result.raw_output = BacktestOutput{}; // Empty raw output
        result.unified_metrics = UnifiedMetricsReport{}; // Empty unified metrics
        result.total_return = 0.0;
        result.final_capital = initial_capital;
        result.sharpe_ratio = 0.0;
        result.max_drawdown = 0.0;
        result.win_rate = 0.0;
        result.total_trades = 0;
        result.monthly_projected_return = 0.0;
        result.daily_trades = 0.0;
    }
    
    return result;
}

VirtualMarketEngine::VMSimulationResult VirtualMarketEngine::calculate_performance_metrics(
    const std::vector<double>& returns,
    const std::vector<int>& trades,
    double initial_capital) {
    
    VMSimulationResult result;
    
    if (returns.empty()) {
        result.total_return = 0.0;
        result.final_capital = initial_capital;
        result.sharpe_ratio = 0.0;
        result.max_drawdown = 0.0;
        result.win_rate = 0.0;
        result.total_trades = 0;
        result.monthly_projected_return = 0.0;
        result.daily_trades = 0.0;
        return result;
    }
    
    // Calculate basic metrics
    result.total_return = returns.back();
    result.final_capital = initial_capital * (1 + result.total_return);
    result.total_trades = trades.size();
    
    // Calculate Sharpe ratio
    if (returns.size() > 1) {
        std::vector<double> daily_returns;
        for (size_t i = 1; i < returns.size(); ++i) {
            daily_returns.push_back(returns[i] - returns[i-1]);
        }
        
        double mean_return = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) / daily_returns.size();
        double variance = 0.0;
        for (double ret : daily_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        double std_dev = std::sqrt(variance / daily_returns.size());
        
        result.sharpe_ratio = (std_dev > 0) ? (mean_return / std_dev * std::sqrt(252)) : 0.0;
    }
    
    // Calculate max drawdown
    double peak = initial_capital;
    double max_dd = 0.0;
    for (double ret : returns) {
        double current_value = initial_capital * (1 + ret);
        if (current_value > peak) {
            peak = current_value;
        }
        double dd = (peak - current_value) / peak;
        max_dd = std::max(max_dd, dd);
    }
    result.max_drawdown = max_dd;
    
    // Calculate win rate
    int winning_trades = std::count_if(trades.begin(), trades.end(), [](int trade) { return trade > 0; });
    result.win_rate = (trades.empty()) ? 0.0 : static_cast<double>(winning_trades) / trades.size();
    
    // Monthly projected return is already correctly calculated by run_backtest
    // using UnifiedMetricsCalculator - no need to recalculate here
    
    // Calculate daily trades
    result.daily_trades = static_cast<double>(result.total_trades) / returns.size();
    
    return result;
}

void VirtualMarketEngine::print_simulation_report(const std::vector<VMSimulationResult>& results,
                                                 const VMTestConfig& config) {
    if (results.empty()) {
        std::cout << "❌ No simulation results to report" << std::endl;
        return;
    }
    
    // Calculate statistics
    std::vector<double> returns;
    std::vector<double> sharpe_ratios;
    std::vector<double> monthly_returns;
    std::vector<double> daily_trades;
    
    for (const auto& result : results) {
        returns.push_back(result.total_return);
        sharpe_ratios.push_back(result.sharpe_ratio);
        monthly_returns.push_back(result.monthly_projected_return);
        daily_trades.push_back(result.daily_trades);
    }
    
    // Sort for percentiles
    std::sort(returns.begin(), returns.end());
    std::sort(sharpe_ratios.begin(), sharpe_ratios.end());
    std::sort(monthly_returns.begin(), monthly_returns.end());
    std::sort(daily_trades.begin(), daily_trades.end());
    
    // Calculate statistics
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double median_return = returns[returns.size() / 2];
    double std_return = 0.0;
    for (double ret : returns) {
        std_return += (ret - mean_return) * (ret - mean_return);
    }
    std_return = std::sqrt(std_return / returns.size());
    
    double mean_sharpe = std::accumulate(sharpe_ratios.begin(), sharpe_ratios.end(), 0.0) / sharpe_ratios.size();
    double mean_mpr = std::accumulate(monthly_returns.begin(), monthly_returns.end(), 0.0) / monthly_returns.size();
    double mean_daily_trades = std::accumulate(daily_trades.begin(), daily_trades.end(), 0.0) / daily_trades.size();
    
    // Probability analysis
    int profitable_sims = std::count_if(returns.begin(), returns.end(), [](double ret) { return ret > 0; });
    double prob_profit = static_cast<double>(profitable_sims) / returns.size() * 100;
    
    // Report generation moved to UnifiedStrategyTester for consistency
}

// VMTestRunner implementation
int VMTestRunner::run_vmtest(const VirtualMarketEngine::VMTestConfig& config) {
    std::cout << "🚀 Starting Virtual Market Test..." << std::endl;
    std::cout << "📊 Strategy: " << config.strategy_name << std::endl;
    std::cout << "📈 Symbol: " << config.symbol << std::endl;
    std::cout << "⏱️  Duration: " << (config.hours > 0 ? std::to_string(config.hours) + " hours" : std::to_string(config.days) + " days") << std::endl;
    std::cout << "🎲 Simulations: " << config.simulations << std::endl;
    std::cout << "⚡ Fast Mode: " << (config.fast_mode ? "enabled" : "disabled") << std::endl;
    
    // Create strategy
    auto strategy = create_strategy(config.strategy_name, config.params_json);
    if (!strategy) {
        std::cerr << "❌ Failed to create strategy: " << config.strategy_name << std::endl;
        return 1;
    }
    
    // Load strategy configuration
    RunnerCfg runner_cfg = load_strategy_config(config.strategy_name);
    
    // Run Monte Carlo simulation
    auto results = vm_engine_.run_monte_carlo_simulation(config, std::move(strategy), runner_cfg);
    
    // Print comprehensive report
    vm_engine_.print_simulation_report(results, config);
    
    return 0;
}

std::unique_ptr<BaseStrategy> VMTestRunner::create_strategy(const std::string& strategy_name,
                                                           const std::string& params_json) {
    // Use StrategyFactory to create strategy (same as Runner)
    return StrategyFactory::instance().create_strategy(strategy_name);
}

RunnerCfg VMTestRunner::load_strategy_config(const std::string& strategy_name) {
    RunnerCfg cfg;
    cfg.strategy_name = strategy_name;
    cfg.audit_level = AuditLevel::Full;
    cfg.snapshot_stride = 100;
    
    // Load configuration from JSON files
    // TODO: Implement JSON configuration loading
    
    return cfg;
}

} // namespace sentio
