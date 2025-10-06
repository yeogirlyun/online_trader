#include "cli/online_sanity_check_command.h"
#include "strategy/online_strategy_60sa.h"
#include "strategy/online_strategy_8detector.h"
#include "strategy/online_strategy_25intraday.h"
#include "common/utils.h"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace sentio {
namespace cli {

std::string OnlineSanityCheckCommand::get_name() const {
    return "online-sanity";
}

std::string OnlineSanityCheckCommand::get_description() const {
    return "Sanity check for online learning strategies (convergence, signal quality, stability)";
}

std::vector<std::string> OnlineSanityCheckCommand::get_usage() const {
    return {
        "online-sanity <strategy> <data_path> [--backtest]",
        "",
        "Strategies:",
        "  60sa        - 60SA feature set",
        "  8detector   - 8-detector feature set",
        "  25intraday  - 25-intraday feature set",
        "",
        "Options:",
        "  --backtest  - Run full backtest with ensemble PSM (optional)",
        "",
        "Examples:",
        "  online-sanity 60sa data/equities/QQQ_RTH_NH.csv",
        "  online-sanity 8detector data/equities/QQQ_RTH_NH.csv --backtest",
        "",
        "Metrics Measured:",
        "  - Warmup accuracy (first 100 bars)",
        "  - Convergence accuracy (after 500 bars)",
        "  - Signal coverage and confidence",
        "  - Ensemble horizon agreement",
        "  - Learning stability",
        "  - Trading performance (if --backtest enabled)"
    };
}

void OnlineSanityCheckCommand::show_help() const {
    std::cout << "\nUsage: " << get_name() << " - " << get_description() << "\n\n";
    for (const auto& line : get_usage()) {
        std::cout << line << "\n";
    }
    std::cout << std::endl;
}

void OnlineSanityCheckCommand::print_usage() const {
    show_help();
}

int OnlineSanityCheckCommand::execute(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Error: Insufficient arguments\n";
        print_usage();
        return 1;
    }
    
    std::string strategy_name = args[0];
    std::string data_path = args[1];
    bool run_backtest = (args.size() > 2 && args[2] == "--backtest");
    
    auto metrics = run_sanity_check(strategy_name, data_path, run_backtest);
    print_metrics(metrics, strategy_name);
    
    return metrics.passed_overall ? 0 : 1;
}

OnlineSanityCheckCommand::OnlineSanityMetrics OnlineSanityCheckCommand::run_sanity_check(
    const std::string& strategy_name,
    const std::string& data_path,
    bool run_backtest) {
    
    OnlineSanityMetrics metrics;
    
    try {
        std::cout << "\n=== ONLINE LEARNING SANITY CHECK ===" << std::endl;
        std::cout << "Strategy: " << strategy_name << std::endl;
        std::cout << "Data: " << data_path << std::endl;
        
        // Create strategy
        std::unique_ptr<IStrategy> strategy;
        
        if (strategy_name == "60sa") {
            strategy = std::make_unique<OnlineStrategy60SA>();
        } else if (strategy_name == "8detector") {
            strategy = std::make_unique<OnlineStrategy8Detector>();
        } else if (strategy_name == "25intraday") {
            strategy = std::make_unique<OnlineStrategy25Intraday>();
        } else {
            throw std::runtime_error("Unknown strategy: " + strategy_name);
        }
        
        // Initialize
        StrategyComponent::StrategyConfig config;
        config.metadata["market_data_path"] = data_path;
        
        if (!strategy->initialize(config)) {
            throw std::runtime_error("Failed to initialize strategy");
        }
        
        // Load data
        std::cout << "\nLoading market data..." << std::endl;
        auto market_data = utils::read_csv_data(data_path);
        
        if (market_data.empty()) {
            throw std::runtime_error("No data loaded");
        }
        
        std::cout << "Loaded " << market_data.size() << " bars" << std::endl;
        
        // Process data
        std::cout << "\n=== PROCESSING (Measuring Convergence) ===" << std::endl;
        auto signals = strategy->process_data(market_data);
        
        // Calculate metrics
        std::cout << "\n=== CALCULATING METRICS ===" << std::endl;
        
        // 1. Warmup accuracy (bars 0-100)
        int warmup_correct = 0, warmup_total = 0;
        for (size_t i = 0; i < std::min(size_t(100), signals.size()); ++i) {
            if (i + 1 >= market_data.size()) break;
            
            const auto& sig = signals[i];
            if (sig.signal_type == SignalType::NEUTRAL) continue;
            
            double actual_return = (market_data[i+1].close - market_data[i].close) / market_data[i].close;
            bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                          (sig.signal_type == SignalType::SHORT && actual_return < 0);
            
            if (correct) warmup_correct++;
            warmup_total++;
        }
        metrics.warmup_accuracy = warmup_total > 0 ? 
            static_cast<double>(warmup_correct) / warmup_total : 0.0;
        
        // 2. Converged accuracy (bars 500+)
        int converged_correct = 0, converged_total = 0;
        for (size_t i = 500; i < signals.size(); ++i) {
            if (i + 1 >= market_data.size()) break;
            
            const auto& sig = signals[i];
            if (sig.signal_type == SignalType::NEUTRAL) continue;
            
            double actual_return = (market_data[i+1].close - market_data[i].close) / market_data[i].close;
            bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                          (sig.signal_type == SignalType::SHORT && actual_return < 0);
            
            if (correct) converged_correct++;
            converged_total++;
        }
        metrics.converged_accuracy = converged_total > 0 ? 
            static_cast<double>(converged_correct) / converged_total : 0.0;
        
        // 3. Overall signal accuracy
        int total_correct = 0, total_signals = 0;
        for (size_t i = 0; i < signals.size(); ++i) {
            if (i + 1 >= market_data.size()) break;
            
            const auto& sig = signals[i];
            if (sig.signal_type == SignalType::NEUTRAL) continue;
            
            double actual_return = (market_data[i+1].close - market_data[i].close) / market_data[i].close;
            bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                          (sig.signal_type == SignalType::SHORT && actual_return < 0);
            
            if (correct) total_correct++;
            total_signals++;
        }
        metrics.signal_accuracy = total_signals > 0 ? 
            static_cast<double>(total_correct) / total_signals : 0.0;
        
        // 4. Signal coverage
        int non_neutral = 0;
        double total_confidence = 0.0;
        for (const auto& sig : signals) {
            if (sig.signal_type != SignalType::NEUTRAL) {
                non_neutral++;
                total_confidence += std::abs(sig.probability - 0.5) * 2.0;
            }
        }
        metrics.signal_coverage = static_cast<double>(non_neutral) / signals.size();
        metrics.avg_confidence = non_neutral > 0 ? total_confidence / non_neutral : 0.0;
        
        // 5. Find convergence point (where accuracy stabilizes)
        std::vector<double> rolling_accuracy;
        const int window = 50;
        for (size_t i = window; i < signals.size(); ++i) {
            int window_correct = 0, window_total = 0;
            for (size_t j = i - window; j < i; ++j) {
                if (j + 1 >= market_data.size()) break;
                const auto& sig = signals[j];
                if (sig.signal_type == SignalType::NEUTRAL) continue;
                
                double actual_return = (market_data[j+1].close - market_data[j].close) / market_data[j].close;
                bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                              (sig.signal_type == SignalType::SHORT && actual_return < 0);
                
                if (correct) window_correct++;
                window_total++;
            }
            if (window_total > 0) {
                rolling_accuracy.push_back(static_cast<double>(window_correct) / window_total);
            }
        }
        
        // Find where rolling accuracy stabilizes above 51%
        for (size_t i = 0; i < rolling_accuracy.size(); ++i) {
            if (rolling_accuracy[i] > 0.51) {
                // Check if it stays above 50% for next 100 bars
                bool stable = true;
                for (size_t j = i; j < std::min(i + 100, rolling_accuracy.size()); ++j) {
                    if (rolling_accuracy[j] < 0.50) {
                        stable = false;
                        break;
                    }
                }
                if (stable) {
                    metrics.convergence_bar = i + window;
                    break;
                }
            }
        }
        
        // 6. Prediction volatility (how stable are predictions)
        double sum_prob_changes = 0.0;
        int prob_changes = 0;
        for (size_t i = 1; i < signals.size(); ++i) {
            if (signals[i].signal_type != SignalType::NEUTRAL && 
                signals[i-1].signal_type != SignalType::NEUTRAL) {
                sum_prob_changes += std::abs(signals[i].probability - signals[i-1].probability);
                prob_changes++;
            }
        }
        metrics.prediction_volatility = prob_changes > 0 ? sum_prob_changes / prob_changes : 0.0;
        
        // 7. Pass/fail criteria
        metrics.passed_warmup = metrics.warmup_accuracy > 0.48;  // Better than random
        metrics.passed_convergence = metrics.converged_accuracy > 0.52;  // Clearly learning
        metrics.passed_signal_quality = metrics.signal_coverage > 0.10 && metrics.avg_confidence > 0.3;
        metrics.passed_overall = metrics.passed_warmup && 
                                metrics.passed_convergence && 
                                metrics.passed_signal_quality;
        
        // 8. Backtest (optional)
        if (run_backtest) {
            std::cout << "\n=== RUNNING BACKTEST ===" << std::endl;
            std::cout << "Note: Ensemble PSM backtest not yet implemented" << std::endl;
            std::cout << "      This would measure actual trading performance" << std::endl;
            // TODO: Integrate with EnsemblePositionStateMachine
            metrics.mrb = 0.0;
            metrics.total_trades = 0;
            metrics.win_rate = 0.0;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error during sanity check: " << e.what() << std::endl;
        metrics.passed_overall = false;
    }
    
    return metrics;
}

void OnlineSanityCheckCommand::print_metrics(
    const OnlineSanityMetrics& metrics,
    const std::string& strategy_name) {
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║       ONLINE LEARNING SANITY CHECK RESULTS                ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nStrategy: " << strategy_name << std::endl;
    
    std::cout << "\n┌─ CONVERGENCE METRICS ─────────────────────────────────────┐" << std::endl;
    std::cout << "│ Warmup Accuracy (0-100 bars):    " << std::fixed << std::setprecision(2) 
              << (metrics.warmup_accuracy * 100) << "% " 
              << (metrics.passed_warmup ? "✅ PASS" : "❌ FAIL (need > 48%)") << std::endl;
    std::cout << "│ Converged Accuracy (500+ bars):  " << std::fixed << std::setprecision(2) 
              << (metrics.converged_accuracy * 100) << "% " 
              << (metrics.passed_convergence ? "✅ PASS" : "❌ FAIL (need > 52%)") << std::endl;
    std::cout << "│ Convergence Point:               Bar " << metrics.convergence_bar 
              << (metrics.convergence_bar > 0 && metrics.convergence_bar < 1000 ? " ✅" : " ⚠️") << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ SIGNAL QUALITY METRICS ──────────────────────────────────┐" << std::endl;
    std::cout << "│ Overall Signal Accuracy:         " << std::fixed << std::setprecision(2) 
              << (metrics.signal_accuracy * 100) << "%" << std::endl;
    std::cout << "│ Signal Coverage (non-neutral):   " << std::fixed << std::setprecision(2) 
              << (metrics.signal_coverage * 100) << "%" << std::endl;
    std::cout << "│ Average Confidence:              " << std::fixed << std::setprecision(2) 
              << (metrics.avg_confidence * 100) << "%" << std::endl;
    std::cout << "│ Quality Check:                   " 
              << (metrics.passed_signal_quality ? "✅ PASS" : "❌ FAIL (need > 10% coverage, > 30% conf)") << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    std::cout << "\n┌─ LEARNING STABILITY ──────────────────────────────────────┐" << std::endl;
    std::cout << "│ Prediction Volatility:           " << std::fixed << std::setprecision(4) 
              << metrics.prediction_volatility 
              << (metrics.prediction_volatility < 0.1 ? " ✅ Stable" : " ⚠️ Volatile") << std::endl;
    std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    
    if (metrics.total_trades > 0) {
        std::cout << "\n┌─ TRADING PERFORMANCE ─────────────────────────────────────┐" << std::endl;
        std::cout << "│ Market-Relative Benchmark (MRB): " << std::fixed << std::setprecision(2) 
                  << metrics.mrb << "%" << std::endl;
        std::cout << "│ Total Trades:                    " << metrics.total_trades << std::endl;
        std::cout << "│ Win Rate:                        " << std::fixed << std::setprecision(2) 
                  << (metrics.win_rate * 100) << "%" << std::endl;
        std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
    }
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    if (metrics.passed_overall) {
        std::cout << "║                    ✅ SANITY CHECK PASSED                  ║" << std::endl;
        std::cout << "║                                                            ║" << std::endl;
        std::cout << "║  Strategy is learning and generating quality signals      ║" << std::endl;
    } else {
        std::cout << "║                    ❌ SANITY CHECK FAILED                  ║" << std::endl;
        std::cout << "║                                                            ║" << std::endl;
        std::cout << "║  Review metrics above for specific issues                 ║" << std::endl;
    }
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\n📊 INTERPRETATION:" << std::endl;
    std::cout << "  • Warmup: Should be ~48-52% (learning from scratch)" << std::endl;
    std::cout << "  • Convergence: Should reach 52-58% after 500 bars" << std::endl;
    std::cout << "  • Coverage: 10-30% non-neutral signals is healthy" << std::endl;
    std::cout << "  • Confidence: Higher is better, > 30% indicates conviction" << std::endl;
    std::cout << "  • Volatility: Lower is better, < 0.1 means stable learning" << std::endl;
    std::cout << std::endl;
}

} // namespace cli
} // namespace sentio
