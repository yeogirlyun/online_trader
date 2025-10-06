#include "cli/online_trade_command.h"
#include "strategy/online_strategy_60sa.h"
#include "strategy/online_strategy_8detector.h"
#include "strategy/online_strategy_25intraday.h"
#include "backend/ensemble_position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"
#include "backend/portfolio_manager.h"
#include "common/utils.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace sentio {
namespace cli {

std::string OnlineTradeCommand::get_name() const {
    return "online-trade";
}

std::string OnlineTradeCommand::get_description() const {
    return "Trade with online learning strategy using ensemble PSM (warmup + trading)";
}

std::vector<std::string> OnlineTradeCommand::get_usage() const {
    return {
        "online-trade <strategy> <data_path> <output_path> [options]",
        "",
        "Strategies:",
        "  60sa        - 60SA feature set",
        "  8detector   - 8-detector feature set",
        "  25intraday  - 25-intraday feature set",
        "",
        "Options:",
        "  --warmup-blocks N      Number of blocks for convergence (default: 1)",
        "  --trade-blocks N       Number of blocks to trade (default: 20)",
        "  --capital N            Starting capital (default: 100000)",
        "  --no-leverage          Disable leverage",
        "  --min-agreement N      Minimum horizon agreement (default: 0.6)",
        "",
        "Examples:",
        "  # Warmup 1 block, trade 20 blocks",
        "  online-trade 60sa data/equities/QQQ_RTH_NH.csv results/online_60sa.json",
        "",
        "  # Custom warmup and trading periods",
        "  online-trade 8detector data/equities/QQQ_RTH_NH.csv results/online_8det.json \\",
        "    --warmup-blocks 2 --trade-blocks 15",
        "",
        "Workflow:",
        "  1. Warmup Phase (Block 1): Learn from data, no trading",
        "  2. Trading Phase (Blocks 2-21): Trade with ensemble PSM",
        "  3. Results: MRB, Sharpe, drawdown, horizon performance"
    };
}

void OnlineTradeCommand::show_help() const {
    std::cout << "\nUsage: " << get_name() << " - " << get_description() << "\n\n";
    for (const auto& line : get_usage()) {
        std::cout << line << "\n";
    }
    std::cout << std::endl;
}

void OnlineTradeCommand::print_usage() const {
    show_help();
}

OnlineTradeCommand::OnlineTradeConfig OnlineTradeCommand::parse_args(
    const std::vector<std::string>& args) {
    
    OnlineTradeConfig config;
    
    if (args.size() < 3) {
        throw std::runtime_error("Insufficient arguments");
    }
    
    config.strategy_name = args[0];
    config.data_path = args[1];
    config.output_path = args[2];
    
    // Parse optional arguments
    for (size_t i = 3; i < args.size(); ++i) {
        if (args[i] == "--warmup-blocks" && i + 1 < args.size()) {
            config.warmup_blocks = std::stoi(args[++i]);
        } else if (args[i] == "--trade-blocks" && i + 1 < args.size()) {
            config.trade_blocks = std::stoi(args[++i]);
        } else if (args[i] == "--capital" && i + 1 < args.size()) {
            config.starting_capital = std::stod(args[++i]);
        } else if (args[i] == "--no-leverage") {
            config.use_leverage = false;
        } else if (args[i] == "--min-agreement" && i + 1 < args.size()) {
            config.min_agreement = std::stod(args[++i]);
        }
    }
    
    // Set derived paths
    config.signals_path = config.output_path + ".signals.jsonl";
    config.trades_path = config.output_path + ".trades.jsonl";
    
    return config;
}

int OnlineTradeCommand::execute(const std::vector<std::string>& args) {
    try {
        auto config = parse_args(args);
        auto results = run_online_trade(config);
        print_results(results, config);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        print_usage();
        return 1;
    }
}

OnlineTradeCommand::OnlineTradeResults OnlineTradeCommand::run_online_trade(
    const OnlineTradeConfig& config) {
    
    OnlineTradeResults results;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ONLINE LEARNING TRADING WITH ENSEMBLE PSM         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nStrategy: " << config.strategy_name << std::endl;
    std::cout << "Data: " << config.data_path << std::endl;
    std::cout << "Warmup: " << config.warmup_blocks << " blocks" << std::endl;
    std::cout << "Trading: " << config.trade_blocks << " blocks" << std::endl;
    std::cout << "Starting Capital: $" << std::fixed << std::setprecision(2) 
              << config.starting_capital << std::endl;
    
    // Create strategy
    std::unique_ptr<IStrategy> strategy;
    
    if (config.strategy_name == "60sa") {
        strategy = std::make_unique<OnlineStrategy60SA>();
    } else if (config.strategy_name == "8detector") {
        strategy = std::make_unique<OnlineStrategy8Detector>();
    } else if (config.strategy_name == "25intraday") {
        strategy = std::make_unique<OnlineStrategy25Intraday>();
    } else {
        throw std::runtime_error("Unknown strategy: " + config.strategy_name);
    }
    
    // Initialize strategy
    StrategyComponent::StrategyConfig strat_config;
    strat_config.metadata["market_data_path"] = config.data_path;
    
    if (!strategy->initialize(strat_config)) {
        throw std::runtime_error("Failed to initialize strategy");
    }
    
    // Load market data
    std::cout << "\n=== LOADING DATA ===" << std::endl;
    auto market_data = utils::read_csv_data(config.data_path);
    
    if (market_data.empty()) {
        throw std::runtime_error("No data loaded");
    }
    
    std::cout << "Loaded " << market_data.size() << " bars" << std::endl;
    
    // Calculate warmup and trading ranges
    const int BARS_PER_BLOCK = 480;
    int warmup_bars = config.warmup_blocks * BARS_PER_BLOCK;
    int trade_start = warmup_bars;
    int trade_end = std::min(
        static_cast<int>(market_data.size()),
        warmup_bars + config.trade_blocks * BARS_PER_BLOCK
    );
    
    results.warmup_bars_used = warmup_bars;
    results.trading_bars = trade_end - trade_start;
    
    std::cout << "Warmup bars: 0 - " << warmup_bars << std::endl;
    std::cout << "Trading bars: " << trade_start << " - " << trade_end << std::endl;
    
    // ========== PHASE 1: WARMUP (CONVERGENCE) ==========
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    PHASE 1: WARMUP                         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nProcessing warmup data (learning, no trading)..." << std::endl;
    
    // Process warmup data
    std::vector<Bar> warmup_data(market_data.begin(), market_data.begin() + warmup_bars);
    auto warmup_signals = strategy->process_data(warmup_data);
    
    // Measure warmup accuracy
    int warmup_correct = 0, warmup_total = 0;
    for (size_t i = 0; i < warmup_signals.size() && i + 1 < warmup_data.size(); ++i) {
        const auto& sig = warmup_signals[i];
        if (sig.signal_type == SignalType::NEUTRAL) continue;
        
        double actual_return = (warmup_data[i+1].close - warmup_data[i].close) / warmup_data[i].close;
        bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                      (sig.signal_type == SignalType::SHORT && actual_return < 0);
        
        if (correct) warmup_correct++;
        warmup_total++;
    }
    
    results.warmup_final_accuracy = warmup_total > 0 ? 
        static_cast<double>(warmup_correct) / warmup_total : 0.0;
    results.converged = results.warmup_final_accuracy > 0.51;
    
    std::cout << "\n┌─ WARMUP RESULTS ──────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Bars Processed:     " << warmup_bars << std::endl;
    std::cout << "│ Final Accuracy:     " << std::fixed << std::setprecision(2) 
              << (results.warmup_final_accuracy * 100) << "%" << std::endl;
    std::cout << "│ Convergence Status: " 
              << (results.converged ? "✅ CONVERGED" : "⚠️  NOT CONVERGED") << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    if (!results.converged) {
        std::cout << "\n⚠️  WARNING: Strategy has not converged. Consider more warmup blocks." << std::endl;
    }
    
    // ========== PHASE 2: TRADING WITH ENSEMBLE PSM ==========
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                  PHASE 2: TRADING                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nProcessing trading data with ensemble PSM..." << std::endl;
    
    // Process trading data
    std::vector<Bar> trading_data(market_data.begin() + trade_start, market_data.begin() + trade_end);
    auto trading_signals = strategy->process_data(trading_data);
    
    // Initialize ensemble PSM
    auto ensemble_psm = std::make_unique<EnsemblePositionStateMachine>();
    
    // Initialize portfolio
    PortfolioState portfolio;
    portfolio.cash_balance = config.starting_capital;
    portfolio.total_equity = config.starting_capital;
    
    // Track trades
    std::vector<std::string> trade_log;
    double peak_value = config.starting_capital;
    double max_dd = 0.0;
    std::vector<double> daily_returns;
    
    // Process each signal with ensemble PSM
    int long_count = 0, short_count = 0, neutral_count = 0;
    double total_confidence = 0.0;
    int confident_signals = 0;
    
    for (size_t i = 0; i < trading_signals.size(); ++i) {
        const auto& signal = trading_signals[i];
        const auto& bar = trading_data[i];
        
        // Count signal types
        if (signal.signal_type == SignalType::LONG) long_count++;
        else if (signal.signal_type == SignalType::SHORT) short_count++;
        else neutral_count++;
        
        if (signal.signal_type != SignalType::NEUTRAL) {
            total_confidence += std::abs(signal.probability - 0.5) * 2.0;
            confident_signals++;
        }
        
        // For ensemble PSM, we need to aggregate horizon signals
        // Since OnlineStrategyBase generates ensemble predictions internally,
        // we'll create a simple ensemble signal from the aggregated output
        
        EnsemblePositionStateMachine::EnsembleSignal ensemble_signal;
        ensemble_signal.horizon_signals = {signal};  // Single aggregated signal
        ensemble_signal.horizon_bars = {5};          // Dominant horizon
        ensemble_signal.horizon_weights = {1.0};
        ensemble_signal.weighted_probability = signal.probability;
        ensemble_signal.consensus_signal = signal.signal_type;
        ensemble_signal.signal_agreement = 1.0;  // Already aggregated
        ensemble_signal.confidence = std::abs(signal.probability - 0.5) * 2.0;
        
        // Get market state
        MarketState market_state;
        market_state.current_price = bar.close;
        market_state.volatility = 0.01;  // Simplified
        
        // Get ensemble transition
        auto transition = ensemble_psm->get_ensemble_transition(
            portfolio,
            ensemble_signal,
            market_state,
            bar.bar_id
        );
        
        // Execute transition (simplified - would need full backend integration)
        // For now, just track that we would trade
        if (transition.target_state != PositionStateMachine::State::CASH_ONLY) {
            results.total_trades++;
            
            // Log trade
            trade_log.push_back(
                "Bar " + std::to_string(i + trade_start) + ": " +
                (signal.signal_type == SignalType::LONG ? "LONG" : "SHORT") +
                " @ $" + std::to_string(bar.close) +
                " (conf=" + std::to_string(ensemble_signal.confidence) + ")"
            );
        }
        
        // Update portfolio value (simplified)
        if (i > 0 && i < trading_data.size()) {
            double return_pct = (trading_data[i].close - trading_data[i-1].close) / 
                               trading_data[i-1].close;
            
            // Apply return if we have position
            if (transition.target_state != PositionStateMachine::State::CASH_ONLY) {
                double position_return = return_pct;
                if (signal.signal_type == SignalType::SHORT) {
                    position_return = -return_pct;
                }
                
                portfolio.total_equity *= (1.0 + position_return);
                daily_returns.push_back(position_return);
            }
            
            // Track drawdown
            if (portfolio.total_equity > peak_value) {
                peak_value = portfolio.total_equity;
            }
            double current_dd = (peak_value - portfolio.total_equity) / peak_value;
            max_dd = std::max(max_dd, current_dd);
        }
        
        // Update PSM with current bar
        ensemble_psm->update_horizon_positions(bar.bar_id, bar.close);
    }
    
    // Calculate final metrics
    results.final_capital = portfolio.total_equity;
    results.total_return = (portfolio.total_equity - config.starting_capital) / 
                          config.starting_capital;
    results.max_drawdown = max_dd;
    
    // Calculate MRB (vs buy-and-hold)
    double market_return = (trading_data.back().close - trading_data.front().close) / 
                          trading_data.front().close;
    results.mrb = (results.total_return - market_return) * 100.0;
    
    // Calculate Sharpe ratio
    if (!daily_returns.empty()) {
        double mean_return = 0.0;
        for (double r : daily_returns) mean_return += r;
        mean_return /= daily_returns.size();
        
        double variance = 0.0;
        for (double r : daily_returns) {
            variance += (r - mean_return) * (r - mean_return);
        }
        double std_dev = std::sqrt(variance / daily_returns.size());
        
        results.sharpe_ratio = std_dev > 0 ? (mean_return / std_dev) * std::sqrt(252) : 0.0;
    }
    
    // Signal metrics during trading
    results.signal_coverage = confident_signals > 0 ? 
        static_cast<double>(confident_signals) / trading_signals.size() : 0.0;
    results.avg_confidence = confident_signals > 0 ? 
        total_confidence / confident_signals : 0.0;
    
    // Calculate trading signal accuracy
    int trading_correct = 0, trading_total = 0;
    for (size_t i = 0; i < trading_signals.size() && i + 1 < trading_data.size(); ++i) {
        const auto& sig = trading_signals[i];
        if (sig.signal_type == SignalType::NEUTRAL) continue;
        
        double actual_return = (trading_data[i+1].close - trading_data[i].close) / 
                              trading_data[i].close;
        bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                      (sig.signal_type == SignalType::SHORT && actual_return < 0);
        
        if (correct) trading_correct++;
        trading_total++;
    }
    results.trading_signal_accuracy = trading_total > 0 ? 
        static_cast<double>(trading_correct) / trading_total : 0.0;
    
    // Save outputs
    if (config.save_signals) {
        std::cout << "\nSaving signals to: " << config.signals_path << std::endl;
        std::ofstream sig_file(config.signals_path);
        for (const auto& sig : trading_signals) {
            sig_file << sig.to_json() << "\n";
        }
        sig_file.close();
    }
    
    if (config.save_trades && !trade_log.empty()) {
        std::cout << "Saving trades to: " << config.trades_path << std::endl;
        std::ofstream trade_file(config.trades_path);
        for (const auto& trade : trade_log) {
            trade_file << trade << "\n";
        }
        trade_file.close();
    }
    
    return results;
}

void OnlineTradeCommand::print_results(
    const OnlineTradeResults& results,
    const OnlineTradeConfig& config) {
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                   TRADING RESULTS                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\n┌─ WARMUP PHASE ────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Bars Used:          " << results.warmup_bars_used << std::endl;
    std::cout << "│ Final Accuracy:     " << std::fixed << std::setprecision(2) 
              << (results.warmup_final_accuracy * 100) << "%" << std::endl;
    std::cout << "│ Status:             " 
              << (results.converged ? "✅ CONVERGED" : "⚠️  NOT CONVERGED") << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ TRADING PERFORMANCE ─────────────────────────────────────┐" << std::endl;
    std::cout << "│ Trading Bars:       " << results.trading_bars << std::endl;
    std::cout << "│ Total Trades:       " << results.total_trades << std::endl;
    std::cout << "│ Starting Capital:   $" << std::fixed << std::setprecision(2) 
              << config.starting_capital << std::endl;
    std::cout << "│ Final Capital:      $" << std::fixed << std::setprecision(2) 
              << results.final_capital << std::endl;
    std::cout << "│ Total Return:       " << std::fixed << std::setprecision(2) 
              << (results.total_return * 100) << "%" << std::endl;
    std::cout << "│ MRB:                " << std::fixed << std::setprecision(2) 
              << results.mrb << "%" 
              << (results.mrb > 0 ? " ✅" : " ❌") << std::endl;
    std::cout << "│ Sharpe Ratio:       " << std::fixed << std::setprecision(2) 
              << results.sharpe_ratio << std::endl;
    std::cout << "│ Max Drawdown:       " << std::fixed << std::setprecision(2) 
              << (results.max_drawdown * 100) << "%" << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ SIGNAL QUALITY (TRADING PERIOD) ────────────────────────┐" << std::endl;
    std::cout << "│ Signal Accuracy:    " << std::fixed << std::setprecision(2) 
              << (results.trading_signal_accuracy * 100) << "%" << std::endl;
    std::cout << "│ Signal Coverage:    " << std::fixed << std::setprecision(2) 
              << (results.signal_coverage * 100) << "%" << std::endl;
    std::cout << "│ Avg Confidence:     " << std::fixed << std::setprecision(2) 
              << (results.avg_confidence * 100) << "%" << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    if (results.mrb > 0 && results.converged) {
        std::cout << "║                    ✅ TRADING SUCCESSFUL                   ║" << std::endl;
        std::cout << "║                                                            ║" << std::endl;
        std::cout << "║  Strategy outperformed market after convergence           ║" << std::endl;
    } else if (!results.converged) {
        std::cout << "║                    ⚠️  INSUFFICIENT WARMUP                 ║" << std::endl;
        std::cout << "║                                                            ║" << std::endl;
        std::cout << "║  Strategy needs more warmup blocks to converge            ║" << std::endl;
    } else {
        std::cout << "║                    ❌ UNDERPERFORMED MARKET                ║" << std::endl;
        std::cout << "║                                                            ║" << std::endl;
        std::cout << "║  Strategy did not beat buy-and-hold                       ║" << std::endl;
    }
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\n📁 Output Files:" << std::endl;
    std::cout << "  • Signals: " << config.signals_path << std::endl;
    std::cout << "  • Trades:  " << config.trades_path << std::endl;
    std::cout << "  • Results: " << config.output_path << std::endl;
    std::cout << std::endl;
}

} // namespace cli
} // namespace sentio
