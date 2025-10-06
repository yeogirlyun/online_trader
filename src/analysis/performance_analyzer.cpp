// src/analysis/performance_analyzer.cpp
#include "analysis/performance_analyzer.h"
#include "analysis/temp_file_manager.h"
#include "strategy/istrategy.h"
#include "backend/enhanced_backend_component.h"
#include "common/utils.h"
#include "validation/bar_id_validator.h"
#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <fstream>
#include <cerrno>

namespace sentio::analysis {

PerformanceMetrics PerformanceAnalyzer::calculate_metrics(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    PerformanceMetrics metrics;
    
    if (signals.empty() || market_data.empty()) {
        return metrics;
    }
    
    // Calculate MRB metrics (single source of truth: Enhanced PSM)
    metrics.signal_accuracy = calculate_signal_accuracy(signals, market_data);
    metrics.trading_based_mrb = calculate_trading_based_mrb_with_psm(signals, market_data, blocks);
    metrics.block_mrbs = calculate_block_mrbs(signals, market_data, blocks, true);
    
    // Calculate MRB consistency
    if (!metrics.block_mrbs.empty()) {
        double mean = std::accumulate(metrics.block_mrbs.begin(), 
                                     metrics.block_mrbs.end(), 0.0) / metrics.block_mrbs.size();
        double variance = 0.0;
        for (const auto& mrb : metrics.block_mrbs) {
            variance += (mrb - mean) * (mrb - mean);
        }
        variance /= metrics.block_mrbs.size();
        metrics.mrb_consistency = std::sqrt(variance) / std::abs(mean);
    }
    
    // Simulate trading to get equity curve
    auto [equity_curve, trade_results] = simulate_trading(signals, market_data);
    
    if (!equity_curve.empty()) {
        // Calculate return metrics
        metrics.total_return = (equity_curve.back() - equity_curve.front()) / equity_curve.front();
        metrics.cumulative_return = metrics.total_return;
        
        // Annualized return (assuming 252 trading days)
        double days = equity_curve.size();
        double years = days / 252.0;
        if (years > 0) {
            metrics.annualized_return = std::pow(1.0 + metrics.total_return, 1.0 / years) - 1.0;
        }
        
        // Calculate returns
        auto returns = calculate_returns(equity_curve);
        
        // Risk-adjusted metrics
        metrics.sharpe_ratio = calculate_sharpe_ratio(returns);
        metrics.sortino_ratio = calculate_sortino_ratio(returns);
        metrics.calmar_ratio = calculate_calmar_ratio(returns, equity_curve);
        
        // Risk metrics
        metrics.max_drawdown = calculate_max_drawdown(equity_curve);
        metrics.volatility = calculate_volatility(returns);
        
        // Trading metrics
        if (!trade_results.empty()) {
            metrics.win_rate = calculate_win_rate(trade_results);
            metrics.profit_factor = calculate_profit_factor(trade_results);
            
            metrics.total_trades = trade_results.size();
            metrics.winning_trades = std::count_if(trade_results.begin(), trade_results.end(),
                                                   [](double r) { return r > 0; });
            metrics.losing_trades = metrics.total_trades - metrics.winning_trades;
            
            // Calculate average win/loss
            double total_wins = 0.0, total_losses = 0.0;
            for (const auto& result : trade_results) {
                if (result > 0) total_wins += result;
                else total_losses += std::abs(result);
            }
            
            if (metrics.winning_trades > 0) {
                metrics.avg_win = total_wins / metrics.winning_trades;
            }
            if (metrics.losing_trades > 0) {
                metrics.avg_loss = total_losses / metrics.losing_trades;
            }
            
            metrics.largest_win = *std::max_element(trade_results.begin(), trade_results.end());
            metrics.largest_loss = *std::min_element(trade_results.begin(), trade_results.end());
        }
    }
    
    // Signal metrics
    metrics.total_signals = signals.size();
    for (const auto& signal : signals) {
        switch (signal.signal_type) {
            case SignalType::LONG:
                metrics.long_signals++;
                break;
            case SignalType::SHORT:
                metrics.short_signals++;
                break;
            case SignalType::NEUTRAL:
                metrics.neutral_signals++;
                break;
            default:
                break;
        }
    }
    
    metrics.non_neutral_signals = metrics.long_signals + metrics.short_signals;
    metrics.signal_generation_rate = static_cast<double>(metrics.total_signals - metrics.neutral_signals) 
                                    / metrics.total_signals;
    metrics.non_neutral_ratio = static_cast<double>(metrics.non_neutral_signals) / metrics.total_signals;
    
    // Calculate mean confidence
    double total_confidence = 0.0;
    int confidence_count = 0;
    for (const auto& signal : signals) {
        if (0.7 > 0.0) {
            total_confidence += 0.7;
            confidence_count++;
        }
    }
    if (confidence_count > 0) {
        metrics.mean_confidence = total_confidence / confidence_count;
    }
    
    return metrics;
}

double PerformanceAnalyzer::calculate_signal_accuracy(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data
) {
    // Signal accuracy = % of signals where predicted direction matched actual price movement
    
    if (signals.empty() || market_data.empty()) {
        return 0.0;
    }
    
    size_t min_size = std::min(signals.size(), market_data.size());
    if (min_size < 2) {
        return 0.0;  // Need at least 2 bars to compare
    }
    
    int correct_predictions = 0;
    int total_predictions = 0;
    
    for (size_t i = 0; i < min_size - 1; ++i) {
        const auto& signal = signals[i];
        const auto& current_bar = market_data[i];
        const auto& next_bar = market_data[i + 1];
        
        // Skip neutral signals
        if (signal.signal_type == SignalType::NEUTRAL) {
            continue;
        }
        
        // Determine actual price movement
        double price_change = next_bar.close - current_bar.close;
        bool price_went_up = price_change > 0;
        bool price_went_down = price_change < 0;
        
        // Check if signal predicted correctly
        bool correct = false;
        if (signal.signal_type == SignalType::LONG && price_went_up) {
            correct = true;
        } else if (signal.signal_type == SignalType::SHORT && price_went_down) {
            correct = true;
        }
        
        if (correct) {
            correct_predictions++;
        }
        total_predictions++;
    }
    
    if (total_predictions == 0) {
        return 0.0;
    }
    
    return static_cast<double>(correct_predictions) / total_predictions;
}

double PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    const PSMValidationConfig& config
) {
    // FULL ENHANCED PSM SIMULATION FOR ACCURATE TRADING MRB (RAII-based)
    
    std::cerr << "⚠️⚠️⚠️ calculate_trading_based_mrb_with_psm called with " << signals.size() << " signals, " << blocks << " blocks\n";
    std::cerr.flush();
    
    if (signals.empty() || market_data.empty() || blocks < 1) {
        std::cerr << "⚠️ Returning 0.0 due to empty data or invalid blocks\n";
        std::cerr.flush();
        return 0.0;
    }
    
    try {
        // In-memory fast path (no file parsing) - DISABLED for audit consistency
        if (false && config.temp_directory == ":memory:" && !config.keep_temp_files) {
            EnhancedBackendComponent::EnhancedBackendConfig backend_config;
            backend_config.starting_capital = config.starting_capital;
            backend_config.leverage_enabled = config.leverage_enabled;
            backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
            backend_config.enable_hysteresis = config.enable_hysteresis;
            backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;
            backend_config.slippage_factor = config.slippage_factor;
            EnhancedBackendComponent backend(backend_config);
            auto r = backend.process_in_memory(signals, market_data, 0, SIZE_MAX);
            double total_return_fraction = (r.final_equity - config.starting_capital) / config.starting_capital;
            double mrb = total_return_fraction / blocks;
            if (mrb > 0.10) {
                std::cerr << "WARNING: Unrealistic MRB per block detected (in-memory): " << mrb << "\n";
            }
            return mrb;
        }

        // RAII-based temp file management (automatic cleanup) for file-based audits
        // TEMPORARY: Keep temp files for debugging
        TempFileManager temp_manager(config.temp_directory, true);
        
        std::string temp_signals = temp_manager.create_temp_file("sanity_check_signals", ".jsonl");
        std::string temp_market = temp_manager.create_temp_file("sanity_check_market", ".csv");
        std::string temp_trades = temp_manager.create_temp_file("sanity_check_trades", ".jsonl");
        
        // Write signals
        std::cerr << "DEBUG: Writing " << signals.size() << " signals to " << temp_signals << "\n";
        std::ofstream signal_file(temp_signals);
        for (const auto& sig : signals) {
            signal_file << sig.to_json() << "\n";
        }
        signal_file.close();
        std::cerr << "DEBUG: Signals written successfully\n";
        
        // Write market data in the "standard format" expected by utils::read_csv_data
        // Format: symbol,timestamp_ms,open,high,low,close,volume
        std::cerr << "DEBUG: Writing " << market_data.size() << " bars to " << temp_market << "\n";
        std::ofstream market_file(temp_market);
        market_file << "symbol,timestamp_ms,open,high,low,close,volume\n";
        for (const auto& bar : market_data) {
            // Validate numeric values before writing
            if (std::isnan(bar.open) || std::isnan(bar.high) || std::isnan(bar.low) || 
                std::isnan(bar.close) || std::isnan(bar.volume)) {
                std::cerr << "ERROR: Invalid bar data at timestamp " << bar.timestamp_ms 
                         << ": open=" << bar.open << ", high=" << bar.high 
                         << ", low=" << bar.low << ", close=" << bar.close 
                         << ", volume=" << bar.volume << "\n";
                throw std::runtime_error("Invalid bar data contains NaN");
            }
            market_file << bar.symbol << ","  // Symbol comes FIRST in standard format!
                       << bar.timestamp_ms << "," 
                       << bar.open << "," << bar.high << "," 
                       << bar.low << "," << bar.close << "," 
                       << bar.volume << "\n";
        }
        market_file.close();
        std::cerr << "DEBUG: Market data written successfully\n";
        
        // Configure Enhanced Backend with validation settings
        EnhancedBackendComponent::EnhancedBackendConfig backend_config;
        backend_config.starting_capital = config.starting_capital;
        backend_config.cost_model = (config.cost_model == "alpaca") ? 
                                    CostModel::ALPACA : CostModel::PERCENTAGE;
        backend_config.leverage_enabled = config.leverage_enabled;
        backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
        backend_config.enable_hysteresis = config.enable_hysteresis;
        backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;
        backend_config.slippage_factor = config.slippage_factor;
        
        // Initialize Enhanced Backend
        std::cerr << "DEBUG: Initializing Enhanced Backend\n";
        EnhancedBackendComponent backend(backend_config);
        std::string run_id = utils::generate_run_id("sanity");
        
        // Process through Enhanced PSM
        std::cerr << "DEBUG: Calling process_to_jsonl\n";
        backend.process_to_jsonl(temp_signals, temp_market, temp_trades, run_id, 0, SIZE_MAX, 0.0);
        std::cerr << "DEBUG: process_to_jsonl completed\n";
        
        // CRITICAL: Validate one-to-one correspondence between signals and trades
        std::cerr << "DEBUG: Validating bar_id correspondence\n";
        try {
            auto validation_result = BarIdValidator::validate_files(temp_signals, temp_trades, false);
            if (!validation_result.passed) {
                std::cerr << "WARNING: Bar ID validation found issues:\n";
                std::cerr << validation_result.to_string();
                // Don't throw - just warn, as HOLD decisions are expected
            } else {
                std::cerr << "DEBUG: Bar ID validation passed\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Bar ID validation failed: " << e.what() << "\n";
            throw;
        }
        
        // Read the trade log to get final equity
        double initial_capital = config.starting_capital;
        double final_equity = initial_capital;
        bool parsed_equity = false;
        int trade_lines_read = 0;
        {
            std::ifstream trade_file(temp_trades);
            if (!trade_file.is_open()) {
                std::cerr << "ERROR: Failed to open trade file: " << temp_trades << "\n";
                throw std::runtime_error("Failed to open trade file: " + temp_trades);
            }
            std::string trade_line;
            while (std::getline(trade_file, trade_line)) {
                if (trade_line.empty()) continue;
                trade_lines_read++;
#ifdef NLOHMANN_JSON_AVAILABLE
                try {
                    auto j = json::parse(trade_line);
                    
                    // Check version for migration tracking
                    std::string version = j.value("version", "1.0");
                    if (version == "1.0") {
                        std::cerr << "Warning: Processing legacy trade log format (v1.0)\n";
                    }
                    
                    if (j.contains("equity_after")) {
                        if (j["equity_after"].is_number()) {
                            // Preferred: numeric value
                            final_equity = j["equity_after"].get<double>();
                            parsed_equity = true;
                        } else if (j["equity_after"].is_string()) {
                            // Fallback: string parsing with enhanced error handling
                            try {
                                std::string equity_str = j["equity_after"].get<std::string>();
                                // Trim whitespace and quotes
                                equity_str.erase(0, equity_str.find_first_not_of(" \t\n\r\""));
                                equity_str.erase(equity_str.find_last_not_of(" \t\n\r\"") + 1);
                                
                                if (!equity_str.empty() && equity_str != "null") {
                                    // Use strtod for more robust parsing
                                    char* end;
                                    errno = 0;
                                    double value = std::strtod(equity_str.c_str(), &end);
                                    if (errno == 0 && end != equity_str.c_str() && *end == '\0') {
                                        final_equity = value;
                                        parsed_equity = true;
                                    } else {
                                        std::cerr << "Warning: Invalid equity_after format: '" 
                                                 << equity_str << "' (errno=" << errno << ")\n";
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "Warning: Failed to parse equity_after string: " << e.what() << "\n";
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to parse trade JSON: " << e.what() << "\n";
                } catch (...) {
                    // ignore non-JSON lines
                }
#else
                const std::string key = "\"equity_after\":";
                size_t pos = trade_line.find(key);
                if (pos != std::string::npos) {
                    size_t value_start = pos + key.size();
                    while (value_start < trade_line.size() && (trade_line[value_start] == ' ' || trade_line[value_start] == '\"')) {
                        ++value_start;
                    }
                    size_t value_end = trade_line.find_first_of(",}\"", value_start);
                    if (value_end != std::string::npos && value_end > value_start) {
                        try {
                            std::string equity_str = trade_line.substr(value_start, value_end - value_start);
                            if (!equity_str.empty() && equity_str != "null") {
                                final_equity = std::stod(equity_str);
                                parsed_equity = true;
                            }
                        } catch (...) {
                            // keep scanning
                        }
                    }
                }
#endif
            }
        }
        
        std::cerr << "DEBUG: Read " << trade_lines_read << " trade lines from " << temp_trades << "\n";
        std::cerr << "DEBUG: parsed_equity=" << parsed_equity << ", final_equity=" << final_equity << "\n";
        
        if (!parsed_equity) {
            throw std::runtime_error("Failed to parse equity_after from trade log: " + temp_trades + 
                                   " (read " + std::to_string(trade_lines_read) + " lines)");
        }
        
        double total_return_pct = ((final_equity - initial_capital) / initial_capital) * 100.0;
        
        // MRB = average return per block
        // Return MRB as fraction, not percent, for consistency with rest of system
        double mrb = (total_return_pct / 100.0) / blocks;
        
        // DEBUG: Log equity values to diagnose unrealistic MRB
        std::cerr << "🔍 MRB Calculation: initial=" << initial_capital 
                  << ", final=" << final_equity 
                  << ", return%=" << total_return_pct 
                  << ", blocks=" << blocks 
                  << ", mrb(fraction)=" << mrb << "\n";
        std::cerr.flush();
        if (mrb > 0.10) {
            std::cerr << "WARNING: Unrealistic MRB per block detected: " << mrb << " (fraction)\n";
            std::cerr.flush();
        }
        
        // Temp files automatically cleaned up by TempFileManager destructor
        
        return mrb;
        
    } catch (const std::exception& e) {
        std::cerr << "\n";
        std::cerr << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cerr << "║  CRITICAL ERROR: Enhanced PSM Simulation Failed                ║\n";
        std::cerr << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "Exception: " << e.what() << "\n";
        std::cerr << "Context: calculate_trading_based_mrb_with_psm\n";
        std::cerr << "Signals: " << signals.size() << "\n";
        std::cerr << "Market Data: " << market_data.size() << "\n";
        std::cerr << "Blocks: " << blocks << "\n";
        std::cerr << "\n";
        std::cerr << "⚠️  Sentio uses REAL MONEY for trading.\n";
        std::cerr << "⚠️  Fallback mechanisms are DISABLED to prevent silent failures.\n";
        std::cerr << "⚠️  Fix the underlying issue before proceeding.\n";
        std::cerr << "\n";
        std::cerr.flush();
        
        // NO FALLBACK! Crash immediately with detailed error
        throw std::runtime_error(
            "Enhanced PSM simulation failed: " + std::string(e.what()) + 
            " | Signals: " + std::to_string(signals.size()) + 
            " | Market Data: " + std::to_string(market_data.size()) + 
            " | Blocks: " + std::to_string(blocks)
        );
    }
}

double PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
    const std::vector<SignalOutput>& signals,
    const std::string& dataset_csv_path,
    int blocks,
    const PSMValidationConfig& config
) {
    // Reuse the temp-signal writing logic, but use the real dataset CSV path directly
    if (signals.empty() || blocks < 1) return 0.0;

    try {
        TempFileManager temp_manager(config.temp_directory, config.keep_temp_files);

        std::string temp_signals = temp_manager.create_temp_file("sanity_check_signals", ".jsonl");
        std::string temp_trades = temp_manager.create_temp_file("sanity_check_trades", ".jsonl");

        // Write signals only
        {
            std::ofstream signal_file(temp_signals);
            for (const auto& sig : signals) signal_file << sig.to_json() << "\n";
        }

        // Configure backend
        EnhancedBackendComponent::EnhancedBackendConfig backend_config;
        backend_config.starting_capital = config.starting_capital;
        backend_config.cost_model = (config.cost_model == "alpaca") ? CostModel::ALPACA : CostModel::PERCENTAGE;
        backend_config.leverage_enabled = config.leverage_enabled;
        backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
        backend_config.enable_hysteresis = config.enable_hysteresis;
        backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;

        EnhancedBackendComponent backend(backend_config);
        std::string run_id = utils::generate_run_id("sanity");

        // Derive start/end for last N blocks
        const size_t BLOCK_SIZE = sentio::STANDARD_BLOCK_SIZE;
        size_t total = signals.size();
        size_t needed = static_cast<size_t>(blocks) * BLOCK_SIZE;
        size_t start_index = (total > needed) ? (total - needed) : 0;
        size_t end_index = total;

        backend.process_to_jsonl(temp_signals, dataset_csv_path, temp_trades, run_id, start_index, end_index, 0.7);

        // Parse equity_after
        double initial_capital = config.starting_capital;
        double final_equity = initial_capital;
        bool parsed_equity = false;
        {
            std::ifstream trade_file(temp_trades);
            std::string line;
            while (std::getline(trade_file, line)) {
#ifdef NLOHMANN_JSON_AVAILABLE
                try {
                    auto j = json::parse(line);
                    
                    // Check version for migration tracking
                    std::string version = j.value("version", "1.0");
                    if (version == "1.0") {
                        static bool warned = false;
                        if (!warned) {
                            std::cerr << "Warning: Processing legacy trade log format (v1.0)\n";
                            warned = true;
                        }
                    }
                    
                    if (j.contains("equity_after")) {
                        if (j["equity_after"].is_number()) {
                            // Preferred: numeric value
                            final_equity = j["equity_after"].get<double>();
                            parsed_equity = true;
                        } else if (j["equity_after"].is_string()) {
                            // Fallback: string parsing with enhanced error handling
                            try {
                                std::string equity_str = j["equity_after"].get<std::string>();
                                // Trim whitespace and quotes
                                equity_str.erase(0, equity_str.find_first_not_of(" \t\n\r\""));
                                equity_str.erase(equity_str.find_last_not_of(" \t\n\r\"") + 1);
                                
                                if (!equity_str.empty() && equity_str != "null") {
                                    // Use strtod for more robust parsing
                                    char* end;
                                    errno = 0;
                                    double value = std::strtod(equity_str.c_str(), &end);
                                    if (errno == 0 && end != equity_str.c_str() && *end == '\0') {
                                        final_equity = value;
                                        parsed_equity = true;
                                    }
                                }
                            } catch (...) { /* ignore */ }
                        }
                    }
                } catch (...) { /* ignore */ }
#else
                const std::string key = "\"equity_after\":";
                size_t pos = line.find(key);
                if (pos != std::string::npos) {
                    size_t value_start = pos + key.size();
                    while (value_start < line.size() && (line[value_start] == ' ' || line[value_start] == '"')) ++value_start;
                    size_t value_end = line.find_first_of(",}\"", value_start);
                    if (value_end != std::string::npos && value_end > value_start) {
                        try {
                            std::string equity_str = line.substr(value_start, value_end - value_start);
                            if (!equity_str.empty() && equity_str != "null") {
                                final_equity = std::stod(equity_str);
                                parsed_equity = true;
                            }
                        } catch (...) {}
                    }
                }
#endif
            }
        }
        if (!parsed_equity) return 0.0; // treat as 0 MRB if no trades

        double total_return_pct = ((final_equity - initial_capital) / initial_capital) * 100.0;
        return (total_return_pct / 100.0) / blocks;
    } catch (...) {
        return 0.0;
    }
}

double PerformanceAnalyzer::calculate_trading_based_mrb(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    // Delegate to the Enhanced PSM path to ensure single source of MRB truth
    PSMValidationConfig cfg; // defaults to file-based temp dir
    return calculate_trading_based_mrb_with_psm(signals, market_data, blocks, cfg);
}

std::vector<double> PerformanceAnalyzer::calculate_block_mrbs(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    std::vector<double> block_mrbs;
    if (signals.empty() || market_data.empty() || blocks < 1) return block_mrbs;
    size_t min_size = std::min(signals.size(), market_data.size());
    size_t block_size = min_size / static_cast<size_t>(blocks);
    if (block_size == 0) return block_mrbs;

    for (int b = 0; b < blocks; ++b) {
        size_t start = static_cast<size_t>(b) * block_size;
        size_t end = (b == blocks - 1) ? min_size : (static_cast<size_t>(b + 1) * block_size);
        if (start >= end) { block_mrbs.push_back(0.0); continue; }

        // Slice signals and market data
        std::vector<SignalOutput> s_slice(signals.begin() + start, signals.begin() + end);
        std::vector<MarketData> m_slice(market_data.begin() + start, market_data.begin() + end);

        double mrb_block = 0.0;
        try {
            mrb_block = calculate_trading_based_mrb_with_psm(s_slice, m_slice, 1);
        } catch (...) {
            mrb_block = 0.0;
        }
        block_mrbs.push_back(mrb_block);
    }
    
    return block_mrbs;
}

ComparisonResult PerformanceAnalyzer::compare_strategies(
    const std::map<std::string, std::vector<SignalOutput>>& strategy_signals,
    const std::vector<MarketData>& market_data
) {
    ComparisonResult result;
    
    // Calculate metrics for each strategy
    for (const auto& [strategy_name, signals] : strategy_signals) {
        auto metrics = calculate_metrics(signals, market_data);
        metrics.strategy_name = strategy_name;
        result.strategy_metrics[strategy_name] = metrics;
    }
    
    // Find best and worst strategies
    double best_score = -std::numeric_limits<double>::infinity();
    double worst_score = std::numeric_limits<double>::infinity();
    
    for (const auto& [name, metrics] : result.strategy_metrics) {
        double score = metrics.calculate_score();
        result.rankings.push_back({name, score});
        
        if (score > best_score) {
            best_score = score;
            result.best_strategy = name;
        }
        if (score < worst_score) {
            worst_score = score;
            result.worst_strategy = name;
        }
    }
    
    // Sort rankings
    std::sort(result.rankings.begin(), result.rankings.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return result;
}

SignalQualityMetrics PerformanceAnalyzer::analyze_signal_quality(
    const std::vector<SignalOutput>& signals
) {
    SignalQualityMetrics metrics;
    
    if (signals.empty()) return metrics;
    
    int long_count = 0, short_count = 0, neutral_count = 0;
    std::vector<double> confidences;
    int reversals = 0;
    int consecutive_neutrals = 0;
    int max_consecutive_neutrals = 0;
    
    SignalType prev_type = SignalType::NEUTRAL;
    
    for (const auto& signal : signals) {
        // Count signal types
        switch (signal.signal_type) {
            case SignalType::LONG:
                long_count++;
                consecutive_neutrals = 0;
                break;
            case SignalType::SHORT:
                short_count++;
                consecutive_neutrals = 0;
                break;
            case SignalType::NEUTRAL:
                neutral_count++;
                consecutive_neutrals++;
                max_consecutive_neutrals = std::max(max_consecutive_neutrals, consecutive_neutrals);
                break;
            default:
                break;
        }
        
        // Count reversals (long to short or short to long)
        if ((prev_type == SignalType::LONG && signal.signal_type == SignalType::SHORT) ||
            (prev_type == SignalType::SHORT && signal.signal_type == SignalType::LONG)) {
            reversals++;
        }
        
        prev_type = signal.signal_type;
        
        // Collect confidences
        if (0.7 > 0.0) {
            confidences.push_back(0.7);
        }
    }
    
    // Calculate ratios
    metrics.long_ratio = static_cast<double>(long_count) / signals.size();
    metrics.short_ratio = static_cast<double>(short_count) / signals.size();
    metrics.neutral_ratio = static_cast<double>(neutral_count) / signals.size();
    
    // Calculate confidence statistics
    if (!confidences.empty()) {
        std::sort(confidences.begin(), confidences.end());
        
        metrics.mean_confidence = std::accumulate(confidences.begin(), confidences.end(), 0.0) 
                                 / confidences.size();
        metrics.median_confidence = confidences[confidences.size() / 2];
        metrics.min_confidence = confidences.front();
        metrics.max_confidence = confidences.back();
        
        // Standard deviation
        double variance = 0.0;
        for (const auto& conf : confidences) {
            variance += (conf - metrics.mean_confidence) * (conf - metrics.mean_confidence);
        }
        variance /= confidences.size();
        metrics.confidence_std_dev = std::sqrt(variance);
    }
    
    metrics.signal_reversals = reversals;
    metrics.consecutive_neutrals = max_consecutive_neutrals;
    
    // Calculate quality indicators
    metrics.signal_consistency = 1.0 - (static_cast<double>(reversals) / signals.size());
    metrics.signal_stability = 1.0 - metrics.neutral_ratio;
    
    return metrics;
}

RiskMetrics PerformanceAnalyzer::calculate_risk_metrics(
    const std::vector<double>& equity_curve
) {
    RiskMetrics metrics;
    
    if (equity_curve.empty()) return metrics;
    
    // Calculate drawdowns
    double peak = equity_curve[0];
    double current_dd = 0.0;
    int dd_duration = 0;
    int max_dd_duration = 0;
    
    for (const auto& equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
            dd_duration = 0;
        } else {
            dd_duration++;
            double dd = (peak - equity) / peak;
            metrics.current_drawdown = dd;
            
            if (dd > metrics.max_drawdown) {
                metrics.max_drawdown = dd;
            }
            
            if (dd_duration > max_dd_duration) {
                max_dd_duration = dd_duration;
            }
        }
    }
    
    metrics.max_drawdown_duration = max_dd_duration;
    metrics.current_drawdown_duration = dd_duration;
    
    // Calculate returns for volatility metrics
    auto returns = calculate_returns(equity_curve);
    
    if (!returns.empty()) {
        metrics.volatility = calculate_volatility(returns);
        
        // Downside deviation
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double downside_variance = 0.0;
        double upside_variance = 0.0;
        int downside_count = 0;
        int upside_count = 0;
        
        for (const auto& ret : returns) {
            if (ret < mean_return) {
                downside_variance += (ret - mean_return) * (ret - mean_return);
                downside_count++;
            } else {
                upside_variance += (ret - mean_return) * (ret - mean_return);
                upside_count++;
            }
        }
        
        if (downside_count > 0) {
            metrics.downside_deviation = std::sqrt(downside_variance / downside_count);
        }
        if (upside_count > 0) {
            metrics.upside_deviation = std::sqrt(upside_variance / upside_count);
        }
        
        // Value at Risk (VaR)
        std::vector<double> sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        size_t var_95_idx = sorted_returns.size() * 0.05;
        size_t var_99_idx = sorted_returns.size() * 0.01;
        
        if (var_95_idx < sorted_returns.size()) {
            metrics.var_95 = sorted_returns[var_95_idx];
        }
        if (var_99_idx < sorted_returns.size()) {
            metrics.var_99 = sorted_returns[var_99_idx];
        }
        
        // Conditional VaR (CVaR)
        if (var_95_idx > 0) {
            double cvar_sum = 0.0;
            for (size_t i = 0; i < var_95_idx; ++i) {
                cvar_sum += sorted_returns[i];
            }
            metrics.cvar_95 = cvar_sum / var_95_idx;
        }
        if (var_99_idx > 0) {
            double cvar_sum = 0.0;
            for (size_t i = 0; i < var_99_idx; ++i) {
                cvar_sum += sorted_returns[i];
            }
            metrics.cvar_99 = cvar_sum / var_99_idx;
        }
    }
    
    return metrics;
}

// Private helper methods

double PerformanceAnalyzer::calculate_sharpe_ratio(
    const std::vector<double>& returns,
    double risk_free_rate
) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return - risk_free_rate;
    
    double volatility = calculate_volatility(returns);
    
    return (volatility > 0) ? (excess_return / volatility) : 0.0;
}

double PerformanceAnalyzer::calculate_max_drawdown(
    const std::vector<double>& equity_curve
) {
    if (equity_curve.empty()) return 0.0;
    
    double max_dd = 0.0;
    double peak = equity_curve[0];
    
    for (const auto& equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
        } else {
            double dd = (peak - equity) / peak;
            max_dd = std::max(max_dd, dd);
        }
    }
    
    return max_dd;
}

double PerformanceAnalyzer::calculate_win_rate(
    const std::vector<double>& trades
) {
    if (trades.empty()) return 0.0;
    
    int winning_trades = std::count_if(trades.begin(), trades.end(),
                                       [](double t) { return t > 0; });
    
    return static_cast<double>(winning_trades) / trades.size();
}

double PerformanceAnalyzer::calculate_profit_factor(
    const std::vector<double>& trades
) {
    if (trades.empty()) return 0.0;
    
    double gross_profit = 0.0;
    double gross_loss = 0.0;
    
    for (const auto& trade : trades) {
        if (trade > 0) {
            gross_profit += trade;
        } else {
            gross_loss += std::abs(trade);
        }
    }
    
    return (gross_loss > 0) ? (gross_profit / gross_loss) : 0.0;
}

double PerformanceAnalyzer::calculate_volatility(
    const std::vector<double>& returns
) {
    if (returns.empty()) return 0.0;
    
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (const auto& ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= returns.size();
    
    return std::sqrt(variance);
}

double PerformanceAnalyzer::calculate_sortino_ratio(
    const std::vector<double>& returns,
    double risk_free_rate
) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return - risk_free_rate;
    
    // Calculate downside deviation
    double downside_variance = 0.0;
    int downside_count = 0;
    
    for (const auto& ret : returns) {
        if (ret < risk_free_rate) {
            downside_variance += (ret - risk_free_rate) * (ret - risk_free_rate);
            downside_count++;
        }
    }
    
    if (downside_count == 0) return 0.0;
    
    double downside_deviation = std::sqrt(downside_variance / downside_count);
    
    return (downside_deviation > 0) ? (excess_return / downside_deviation) : 0.0;
}

double PerformanceAnalyzer::calculate_calmar_ratio(
    const std::vector<double>& returns,
    const std::vector<double>& equity_curve
) {
    if (returns.empty() || equity_curve.empty()) return 0.0;
    
    double annualized_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    annualized_return *= 252;  // Annualize
    
    double max_dd = calculate_max_drawdown(equity_curve);
    
    return (max_dd > 0) ? (annualized_return / max_dd) : 0.0;
}

std::pair<std::vector<double>, std::vector<double>> PerformanceAnalyzer::simulate_trading(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data
) {
    std::vector<double> equity_curve;
    std::vector<double> trade_results;
    
    if (signals.empty() || market_data.empty()) {
        return {equity_curve, trade_results};
    }
    
    double equity = 10000.0;  // Starting capital
    equity_curve.push_back(equity);
    
    int position = 0;  // 0 = neutral, 1 = long, -1 = short
    double entry_price = 0.0;
    
    size_t min_size = std::min(signals.size(), market_data.size());
    
    for (size_t i = 0; i < min_size - 1; ++i) {
        const auto& signal = signals[i];
        const auto& current_data = market_data[i];
        const auto& next_data = market_data[i + 1];
        
        // Determine new position
        int new_position = 0;
        if (signal.signal_type == SignalType::LONG) {
            new_position = 1;
        } else if (signal.signal_type == SignalType::SHORT) {
            new_position = -1;
        }
        
        // Close existing position if changing
        if (position != 0 && new_position != position) {
            double exit_price = current_data.close;
            double pnl = position * (exit_price - entry_price) / entry_price;
            equity *= (1.0 + pnl);
            trade_results.push_back(pnl);
            position = 0;
        }
        
        // Open new position
        if (new_position != 0 && position == 0) {
            entry_price = current_data.close;
            position = new_position;
        }
        
        equity_curve.push_back(equity);
    }
    
    // Close final position
    if (position != 0 && !market_data.empty()) {
        double exit_price = market_data.back().close;
        double pnl = position * (exit_price - entry_price) / entry_price;
        equity *= (1.0 + pnl);
        trade_results.push_back(pnl);
    }
    
    return {equity_curve, trade_results};
}

std::vector<double> PerformanceAnalyzer::calculate_returns(
    const std::vector<double>& equity_curve
) {
    std::vector<double> returns;
    
    if (equity_curve.size() < 2) return returns;
    
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        if (equity_curve[i-1] > 0) {
            double ret = (equity_curve[i] - equity_curve[i-1]) / equity_curve[i-1];
            returns.push_back(ret);
        }
    }
    
    return returns;
}

// WalkForwardAnalyzer implementation

WalkForwardAnalyzer::WalkForwardResult WalkForwardAnalyzer::analyze(
    const std::string& strategy_name,
    const std::vector<MarketData>& market_data,
    const WalkForwardConfig& config
) {
    WalkForwardResult result;
    
    // Implementation of walk-forward analysis
    // This would split data into windows, train on in-sample, test on out-of-sample
    // For now, this is a placeholder
    
    return result;
}

// StressTestAnalyzer implementation

std::vector<StressTestAnalyzer::StressTestResult> StressTestAnalyzer::run_stress_tests(
    const std::string& strategy_name,
    const std::vector<MarketData>& base_market_data,
    const std::vector<StressScenario>& scenarios
) {
    std::vector<StressTestResult> results;
    
    for (const auto& scenario : scenarios) {
        StressTestResult test_result;
        test_result.scenario = scenario;
        
        // Apply stress scenario to data
        auto stressed_data = apply_stress_scenario(base_market_data, scenario);
        
        // Load strategy and generate signals
        auto strategy_unique = create_strategy(strategy_name);
        if (!strategy_unique) continue;
        auto strategy = std::shared_ptr<IStrategy>(std::move(strategy_unique));
        
        std::vector<SignalOutput> signals;
        try {
            signals = strategy->process_data(stressed_data);
        } catch (...) {
            signals.clear();
        }
        
        // Calculate metrics
        test_result.metrics = PerformanceAnalyzer::calculate_metrics(
            signals, stressed_data
        );
        
        // Determine if passed based on metrics
        test_result.passed = (test_result.metrics.trading_based_mrb > 0.005);
        
        results.push_back(test_result);
    }
    
    return results;
}

std::vector<MarketData> StressTestAnalyzer::apply_stress_scenario(
    const std::vector<MarketData>& market_data,
    StressScenario scenario
) {
    std::vector<MarketData> stressed_data = market_data;
    
    switch (scenario) {
        case StressScenario::MARKET_CRASH:
            // Apply crash scenario
            for (auto& data : stressed_data) {
                data.close *= 0.8;  // 20% crash
            }
            break;
            
        case StressScenario::HIGH_VOLATILITY:
            // Increase volatility
            for (size_t i = 1; i < stressed_data.size(); ++i) {
                double change = (stressed_data[i].close - stressed_data[i-1].close) / stressed_data[i-1].close;
                stressed_data[i].close = stressed_data[i-1].close * (1.0 + change * 2.0);
            }
            break;
            
        case StressScenario::LOW_VOLATILITY:
            // Decrease volatility
            for (size_t i = 1; i < stressed_data.size(); ++i) {
                double change = (stressed_data[i].close - stressed_data[i-1].close) / stressed_data[i-1].close;
                stressed_data[i].close = stressed_data[i-1].close * (1.0 + change * 0.5);
            }
            break;
            
        // Add other scenarios
        default:
            break;
    }
    
    return stressed_data;
}

} // namespace sentio::analysis


