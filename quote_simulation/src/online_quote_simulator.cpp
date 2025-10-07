#include "online_quote_simulator.hpp"
#include "sentio/runner.hpp"
#include "sentio/symbol_table.hpp"
#include "sentio/audit.hpp"
#include "audit/audit_db_recorder.hpp"
#include "sentio/base_strategy.hpp"
#include "sentio/mars_data_loader.hpp"
#include "sentio/run_id_generator.hpp"
#include "sentio/dataset_metadata.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

namespace sentio {

OnlineQuoteSimulator::OnlineQuoteSimulator() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      streaming_active_(false) {
    initialize_market_regimes();
    
    // Initialize base prices for common symbols
    base_prices_["QQQ"] = 458.0;
    base_prices_["SPY"] = 450.0;
    base_prices_["AAPL"] = 175.0;
    base_prices_["MSFT"] = 350.0;
    base_prices_["TSLA"] = 250.0;
    base_prices_["TQQQ"] = 120.0; // 3x QQQ
    base_prices_["SQQQ"] = 120.0; // 3x inverse QQQ
}

void OnlineQuoteSimulator::initialize_market_regimes() {
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

void OnlineQuoteSimulator::select_new_regime() {
    // Weight regimes by probability (normal market is most common)
    std::vector<double> weights = {0.15, 0.10, 0.20, 0.15, 0.05, 0.05, 0.30};
    
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    int regime_index = dist(rng_);
    
    current_regime_ = market_regimes_[regime_index];
    regime_start_time_ = std::chrono::system_clock::now();
}

bool OnlineQuoteSimulator::should_change_regime() {
    if (current_regime_.name.empty()) return true;
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - regime_start_time_);
    return elapsed.count() >= current_regime_.duration_minutes;
}

std::vector<OnlineQuoteSimulator::QuoteData> OnlineQuoteSimulator::generate_realtime_quotes(
    const std::string& symbol, 
    int duration_minutes,
    int interval_ms) {
    
    std::vector<QuoteData> quotes;
    quotes.reserve(duration_minutes * 60 * 1000 / interval_ms);
    
    auto current_time = std::chrono::system_clock::now();
    
    for (int i = 0; i < duration_minutes * 60 * 1000 / interval_ms; ++i) {
        // Change regime periodically
        if (i % 1000 == 0) {
            select_new_regime();
        }
        
        QuoteData quote = generate_quote(symbol, current_time);
        quotes.push_back(quote);
        
        current_time += std::chrono::milliseconds(interval_ms);
    }
    
    std::cout << "✅ Generated " << quotes.size() << " real-time quotes for " << symbol << std::endl;
    return quotes;
}

OnlineQuoteSimulator::QuoteData OnlineQuoteSimulator::generate_quote(
    const std::string& symbol, 
    std::chrono::system_clock::time_point timestamp) {
    
    double current_price = base_prices_[symbol];
    
    // Calculate price movement
    double price_move = calculate_price_movement(symbol, current_price, current_regime_);
    double new_price = current_price * (1 + price_move);
    
    // Generate realistic bid/ask spread
    double spread = new_price * 0.0001; // 0.01% spread
    double bid_price = new_price - spread / 2;
    double ask_price = new_price + spread / 2;
    
    // Generate realistic sizes
    int base_size = 1000 + (rng_() % 5000);
    double size_multiplier = current_regime_.volume_multiplier;
    int bid_size = static_cast<int>(base_size * size_multiplier);
    int ask_size = static_cast<int>(base_size * size_multiplier);
    
    // Generate last trade
    double last_price = new_price;
    int last_size = 100 + (rng_() % 1000);
    
    // Generate volume
    int base_volume = 50000 + (rng_() % 150000);
    double volume_multiplier = current_regime_.volume_multiplier * (1 + std::abs(price_move) * 2.0);
    int volume = static_cast<int>(base_volume * volume_multiplier);
    
    QuoteData quote;
    quote.timestamp = std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
    quote.symbol = symbol;
    quote.bid_price = bid_price;
    quote.ask_price = ask_price;
    quote.bid_size = bid_size;
    quote.ask_size = ask_size;
    quote.last_price = last_price;
    quote.last_size = last_size;
    quote.volume = volume;
    
    return quote;
}

double OnlineQuoteSimulator::calculate_price_movement(const std::string& symbol, 
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

std::vector<OnlineQuoteSimulator::SimulationResult> OnlineQuoteSimulator::run_online_simulation(
    const OnlineTestConfig& config,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg) {
    
    if (config.use_mars) {
        return run_mars_online_simulation(config, std::move(strategy), runner_cfg);
    }
    
    // Fallback to basic simulation
    std::vector<SimulationResult> results;
    results.reserve(config.simulations);
    
    std::cout << "🚀 Starting Online Learning Simulation..." << std::endl;
    std::cout << "📊 Strategy: " << config.strategy_name << std::endl;
    std::cout << "📈 Symbol: " << config.symbol << std::endl;
    std::cout << "⏱️  Duration: " << config.duration_minutes << " minutes" << std::endl;
    std::cout << "🎲 Simulations: " << config.simulations << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < config.simulations; ++i) {
        // Progress reporting
        if (config.simulations >= 5 && ((i + 1) % std::max(1, config.simulations / 2) == 0 || i == config.simulations - 1)) {
            double progress_pct = (100.0 * (i + 1)) / config.simulations;
            std::cout << "📊 Progress: " << std::fixed << std::setprecision(0) << progress_pct 
                      << "% (" << (i + 1) << "/" << config.simulations << ")" << std::endl;
        }
        
        try {
            // Generate market data
            auto quotes = generate_realtime_quotes(config.symbol, config.duration_minutes, config.quote_interval_ms);
            
            // Convert quotes to bars
            std::vector<Bar> market_data;
            market_data.reserve(quotes.size() / 60); // Convert to 1-minute bars
            
            for (size_t j = 0; j < quotes.size(); j += 60) {
                if (j + 59 < quotes.size()) {
                    Bar bar;
                    bar.ts_utc_epoch = quotes[j].timestamp;
                    bar.open = quotes[j].bid_price;
                    bar.close = quotes[j + 59].ask_price;
                    bar.high = std::max_element(quotes.begin() + j, quotes.begin() + j + 60, 
                        [](const QuoteData& a, const QuoteData& b) { return a.ask_price < b.ask_price; })->ask_price;
                    bar.low = std::min_element(quotes.begin() + j, quotes.begin() + j + 60, 
                        [](const QuoteData& a, const QuoteData& b) { return a.bid_price < b.bid_price; })->bid_price;
                    bar.volume = std::accumulate(quotes.begin() + j, quotes.begin() + j + 60, 0,
                        [](int sum, const QuoteData& q) { return sum + static_cast<int>(q.volume); });
                    market_data.push_back(bar);
                }
            }
            
            // Create new strategy instance for this simulation
            auto strategy_instance = StrategyFactory::instance().create_strategy(config.strategy_name);
            
            if (!strategy_instance) {
                std::cerr << "Error: Could not create strategy instance for simulation " << (i + 1) << std::endl;
                results.push_back(SimulationResult{}); // Add empty result
                continue;
            }
            
            // Run simulation
            auto result = run_single_online_simulation(
                market_data, std::move(strategy_instance), runner_cfg, config.initial_capital
            );
            
            result.market_regime = config.market_regime;
            result.simulation_duration_minutes = config.duration_minutes;
            
            results.push_back(result);
            
        } catch (const std::exception& e) {
            std::cerr << "Error in online simulation " << (i + 1) << ": " << e.what() << std::endl;
            results.push_back(SimulationResult{}); // Add empty result
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    
    std::cout << "⏱️  Completed " << config.simulations << " online simulations in " 
              << total_time << " seconds" << std::endl;
    
    // Print comprehensive report
    print_online_simulation_report(results, config);
    
    return results;
}

std::vector<OnlineQuoteSimulator::SimulationResult> OnlineQuoteSimulator::run_mars_online_simulation(
    const OnlineTestConfig& config,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg) {
    
    std::cout << "🔄 Using MarS for realistic online learning simulation..." << std::endl;
    
    // Use MarS data loader for realistic data
    std::vector<SimulationResult> results;
    results.reserve(config.simulations);
    
    for (int i = 0; i < config.simulations; ++i) {
        try {
            // Generate MarS data
            auto mars_data = MarsDataLoader::load_mars_data(
                config.symbol,
                config.duration_minutes,
                60, // 1-minute bars
                1,  // Single simulation per call
                config.market_regime
            );
            
            if (mars_data.empty()) {
                std::cerr << "Warning: No MarS data loaded for simulation " << (i + 1) << std::endl;
                results.push_back(SimulationResult{}); // Add empty result
                continue;
            }
            
            // Create new strategy instance
            auto strategy_instance = StrategyFactory::instance().create_strategy(config.strategy_name);
            
            if (!strategy_instance) {
                std::cerr << "Error: Could not create strategy instance for simulation " << (i + 1) << std::endl;
                results.push_back(SimulationResult{}); // Add empty result
                continue;
            }
            
            // Run simulation with MarS data
            auto result = run_single_online_simulation(
                mars_data, std::move(strategy_instance), runner_cfg, config.initial_capital
            );
            
            result.market_regime = config.market_regime;
            result.simulation_duration_minutes = config.duration_minutes;
            
            results.push_back(result);
            
        } catch (const std::exception& e) {
            std::cerr << "Error in MarS online simulation " << (i + 1) << ": " << e.what() << std::endl;
            results.push_back(SimulationResult{}); // Add empty result
        }
    }
    
    return results;
}

OnlineQuoteSimulator::SimulationResult OnlineQuoteSimulator::run_single_online_simulation(
    const std::vector<Bar>& market_data,
    std::unique_ptr<BaseStrategy> strategy,
    const RunnerCfg& runner_cfg,
    double initial_capital) {
    
    SimulationResult result;
    
    try {
        // Create SymbolTable for QQQ family
        SymbolTable ST;
        int qqq_id = ST.intern("QQQ");
        int tqqq_id = ST.intern("TQQQ");  // 3x leveraged long
        int sqqq_id = ST.intern("SQQQ");  // 3x leveraged short
        
        // Create series data structure
        std::vector<std::vector<Bar>> series(3);
        series[qqq_id] = market_data;  // Base QQQ data
        
        // Generate theoretical leverage data
        series[tqqq_id] = generate_theoretical_leverage_series(market_data, 3.0);   // 3x leveraged long
        series[sqqq_id] = generate_theoretical_leverage_series(market_data, -3.0);  // 3x leveraged short
        
        // Create audit recorder
        std::string run_id = generate_run_id();
        std::string note = "Strategy: " + runner_cfg.strategy_name + ", Test: online_simulation";
        
        std::string db_path = (runner_cfg.audit_level == AuditLevel::MetricsOnly) ? ":memory:" : "audit/sentio_audit.sqlite3";
        audit::AuditDBRecorder audit(db_path, run_id, note);
        
        // Run backtest using actual Runner
        DatasetMetadata dataset_meta;
        dataset_meta.source_type = "online_quote_simulation";
        dataset_meta.file_path = "quote_simulation/generated_data";
        dataset_meta.track_id = "online_sim";
        dataset_meta.regime = "online_learning";
        dataset_meta.bars_count = static_cast<int>(market_data.size());
        if (!market_data.empty()) {
            dataset_meta.time_range_start = market_data.front().ts_utc_epoch * 1000;
            dataset_meta.time_range_end = market_data.back().ts_utc_epoch * 1000;
        }
        
        BacktestOutput backtest_output = run_backtest(audit, ST, series, qqq_id, runner_cfg, dataset_meta);
        
        // Store raw output and calculate unified metrics
        result.raw_output = backtest_output;
        result.unified_metrics = UnifiedMetricsCalculator::calculate_metrics(backtest_output);
        result.run_id = run_id;
        
        // Populate metrics for backward compatibility
        result.total_return = result.unified_metrics.total_return;
        result.final_capital = result.unified_metrics.final_equity;
        result.sharpe_ratio = result.unified_metrics.sharpe_ratio;
        result.max_drawdown = result.unified_metrics.max_drawdown;
        result.win_rate = 0.0; // Not calculated in unified metrics yet
        result.total_trades = result.unified_metrics.total_fills;
        result.monthly_projected_return = result.unified_metrics.monthly_projected_return;
        result.daily_trades = result.unified_metrics.avg_daily_trades;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Online simulation failed: " << e.what() << std::endl;
        
        // Return zero results on error
        result.raw_output = BacktestOutput{};
        result.unified_metrics = UnifiedMetricsReport{};
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

void OnlineQuoteSimulator::start_realtime_streaming(const std::string& symbol,
                                                   std::function<void(const QuoteData&)> quote_callback,
                                                   int interval_ms) {
    if (streaming_active_) {
        std::cout << "⚠️ Streaming already active" << std::endl;
        return;
    }
    
    streaming_active_ = true;
    streaming_thread_ = std::thread(&OnlineQuoteSimulator::streaming_worker, this, symbol, quote_callback, interval_ms);
    
    std::cout << "🚀 Started real-time quote streaming for " << symbol << std::endl;
}

void OnlineQuoteSimulator::stop_realtime_streaming() {
    if (!streaming_active_) {
        return;
    }
    
    streaming_active_ = false;
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
    
    std::cout << "⏹️ Stopped real-time quote streaming" << std::endl;
}

void OnlineQuoteSimulator::streaming_worker(const std::string& symbol,
                                           std::function<void(const QuoteData&)> quote_callback,
                                           int interval_ms) {
    while (streaming_active_) {
        auto quote = generate_quote(symbol, std::chrono::system_clock::now());
        quote_callback(quote);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void OnlineQuoteSimulator::print_online_simulation_report(const std::vector<SimulationResult>& results,
                                                        const OnlineTestConfig& config) {
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
    
    // Print report
    std::cout << "\n📊 Online Learning Simulation Report" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Strategy: " << config.strategy_name << std::endl;
    std::cout << "Symbol: " << config.symbol << std::endl;
    std::cout << "Duration: " << config.duration_minutes << " minutes" << std::endl;
    std::cout << "Simulations: " << config.simulations << std::endl;
    std::cout << "Market Regime: " << config.market_regime << std::endl;
    std::cout << "MarS Integration: " << (config.use_mars ? "Enabled" : "Disabled") << std::endl;
    std::cout << std::endl;
    
    std::cout << "📈 Performance Statistics:" << std::endl;
    std::cout << "  Mean Return: " << std::fixed << std::setprecision(2) << mean_return * 100 << "%" << std::endl;
    std::cout << "  Median Return: " << std::fixed << std::setprecision(2) << median_return * 100 << "%" << std::endl;
    std::cout << "  Std Dev: " << std::fixed << std::setprecision(2) << std_return * 100 << "%" << std::endl;
    std::cout << "  Mean Sharpe: " << std::fixed << std::setprecision(3) << mean_sharpe << std::endl;
    std::cout << "  Mean MPR: " << std::fixed << std::setprecision(2) << mean_mpr * 100 << "%" << std::endl;
    std::cout << "  Mean Daily Trades: " << std::fixed << std::setprecision(1) << mean_daily_trades << std::endl;
    std::cout << "  Probability of Profit: " << std::fixed << std::setprecision(1) << prob_profit << "%" << std::endl;
    
    // Percentile analysis
    if (returns.size() >= 10) {
        std::cout << "\n📊 Return Percentiles:" << std::endl;
        std::cout << "  10th: " << std::fixed << std::setprecision(2) << returns[returns.size() / 10] * 100 << "%" << std::endl;
        std::cout << "  25th: " << std::fixed << std::setprecision(2) << returns[returns.size() / 4] * 100 << "%" << std::endl;
        std::cout << "  75th: " << std::fixed << std::setprecision(2) << returns[3 * returns.size() / 4] * 100 << "%" << std::endl;
        std::cout << "  90th: " << std::fixed << std::setprecision(2) << returns[9 * returns.size() / 10] * 100 << "%" << std::endl;
    }
}

// MockPolygonAPI implementation
MockPolygonAPI::MockPolygonAPI(const std::string& api_key) 
    : api_key_(api_key), mars_enabled_(true), current_regime_("normal"), subscription_active_(false) {
    quote_simulator_ = std::make_unique<OnlineQuoteSimulator>();
}

MockPolygonAPI::~MockPolygonAPI() {
    if (subscription_active_) {
        unsubscribe_from_quotes("ALL");
    }
}

MockPolygonAPI::PolygonQuote MockPolygonAPI::get_realtime_quote(const std::string& symbol) {
    if (mars_enabled_) {
        auto quote_data = quote_simulator_->generate_quote(symbol, std::chrono::system_clock::now());
        
        PolygonQuote quote;
        quote.symbol = symbol;
        quote.bid = quote_data.bid_price;
        quote.ask = quote_data.ask_price;
        quote.bid_size = quote_data.bid_size;
        quote.ask_size = quote_data.ask_size;
        quote.last = quote_data.last_price;
        quote.last_size = quote_data.last_size;
        quote.timestamp = quote_data.timestamp;
        
        return quote;
    }
    
    // Fallback to basic mock data
    return generate_mock_quote(symbol);
}

void MockPolygonAPI::subscribe_to_quotes(const std::string& symbol,
                                        std::function<void(const PolygonQuote&)> callback) {
    subscriptions_[symbol] = callback;
    
    if (!subscription_active_) {
        subscription_active_ = true;
        subscription_thread_ = std::thread(&MockPolygonAPI::subscription_worker, this);
    }
}

void MockPolygonAPI::unsubscribe_from_quotes(const std::string& symbol) {
    if (symbol == "ALL") {
        subscriptions_.clear();
        subscription_active_ = false;
        if (subscription_thread_.joinable()) {
            subscription_thread_.join();
        }
    } else {
        subscriptions_.erase(symbol);
    }
}

void MockPolygonAPI::subscription_worker() {
    while (subscription_active_) {
        for (auto& [symbol, callback] : subscriptions_) {
            auto quote = get_realtime_quote(symbol);
            callback(quote);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1 second updates
    }
}

MockPolygonAPI::PolygonQuote MockPolygonAPI::generate_mock_quote(const std::string& symbol) {
    PolygonQuote quote;
    quote.symbol = symbol;
    quote.bid = 450.0;
    quote.ask = 450.1;
    quote.bid_size = 1000;
    quote.ask_size = 1000;
    quote.last = 450.05;
    quote.last_size = 500;
    quote.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return quote;
}

} // namespace sentio
