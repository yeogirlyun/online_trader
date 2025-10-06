/**
 * Standalone test for online learning trading
 * Warmup 1 block, trade 20 blocks, report MRB
 */

#include "strategy/online_strategy_60sa.h"
#include "backend/ensemble_position_state_machine.h"
#include "common/utils.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace sentio;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_path>" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/equities/QQQ_RTH_NH.csv" << std::endl;
        return 1;
    }
    
    std::string data_path = argv[1];
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║    ONLINE LEARNING TRADE TEST (1 BLOCK WARMUP + 20 TRADE) ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        // Create strategy
        std::cout << "\n=== INITIALIZING STRATEGY ===" << std::endl;
        auto strategy = std::make_unique<OnlineStrategy60SA>();
        
        StrategyComponent::StrategyConfig config;
        config.metadata["market_data_path"] = data_path;
        
        if (!strategy->initialize(config)) {
            std::cerr << "Failed to initialize strategy" << std::endl;
            return 1;
        }
        
        std::cout << "Strategy: " << strategy->get_strategy_name() << std::endl;
        std::cout << "Version: " << strategy->get_strategy_version() << std::endl;
        
        // Load data
        std::cout << "\n=== LOADING DATA ===" << std::endl;
        auto market_data = utils::read_csv_data(data_path);
        
        if (market_data.empty()) {
            std::cerr << "No data loaded" << std::endl;
            return 1;
        }
        
        std::cout << "Loaded " << market_data.size() << " bars" << std::endl;
        
        // Calculate ranges - USE MOST RECENT BLOCKS (warmup + trading)
        const int BARS_PER_BLOCK = 480;
        const int WARMUP_BLOCKS = 10;  // Full warmup for proper convergence
        const int TRADE_BLOCKS = 20;   // Test on most recent 20 blocks
        const int TOTAL_BLOCKS = WARMUP_BLOCKS + TRADE_BLOCKS;
        
        int total_bars_needed = TOTAL_BLOCKS * BARS_PER_BLOCK;
        
        if (market_data.size() < static_cast<size_t>(total_bars_needed)) {
            std::cerr << "Error: Not enough data. Need " << total_bars_needed 
                      << " bars, have " << market_data.size() << std::endl;
            return 1;
        }
        
        // Use the LAST 21 blocks
        int data_start = market_data.size() - total_bars_needed;
        int warmup_start = data_start;
        int warmup_end = warmup_start + WARMUP_BLOCKS * BARS_PER_BLOCK;
        int trade_start = warmup_end;
        int trade_end = market_data.size();
        
        std::cout << "\n🎯 USING MOST RECENT DATA:" << std::endl;
        std::cout << "Total bars available: " << market_data.size() << std::endl;
        std::cout << "Using bars " << data_start << " to " << market_data.size() 
                  << " (last " << TOTAL_BLOCKS << " blocks)" << std::endl;
        std::cout << "Warmup: bars " << warmup_start << "-" << warmup_end 
                  << " (" << WARMUP_BLOCKS << " blocks)" << std::endl;
        std::cout << "Trading: bars " << trade_start << "-" << trade_end 
                  << " (" << TRADE_BLOCKS << " blocks)" << std::endl;
        
        // PHASE 1: WARMUP
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                    PHASE 1: WARMUP                         ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        std::cout << "\nProcessing warmup data (learning continuously)..." << std::endl;
        
        // Process ALL data continuously (warmup + trading together)
        // The strategy learns from every bar, including during "trading"
        std::vector<Bar> all_data(market_data.begin() + warmup_start, market_data.begin() + trade_end);
        auto all_signals = strategy->process_data(all_data);
        
        // Split signals into warmup and trading for analysis
        std::vector<SignalOutput> warmup_signals(all_signals.begin(), all_signals.begin() + (warmup_end - warmup_start));
        std::vector<SignalOutput> trading_signals(all_signals.begin() + (warmup_end - warmup_start), all_signals.end());
        
        // Also split the data for reference
        std::vector<Bar> warmup_data(all_data.begin(), all_data.begin() + (warmup_end - warmup_start));
        std::vector<Bar> trading_data(all_data.begin() + (warmup_end - warmup_start), all_data.end());
        
        // Debug: Check signal metadata
        int warmup_non_neutral = 0;
        for (const auto& sig : warmup_signals) {
            if (sig.signal_type != SignalType::NEUTRAL) warmup_non_neutral++;
        }
        std::cout << "Warmup non-neutral signals: " << warmup_non_neutral << " / " << warmup_signals.size() << std::endl;
        
        // Sample some signals to see confidence
        for (size_t i = 0; i < warmup_signals.size() && i < 200; ++i) {
            const auto& sig = warmup_signals[i];
            if (sig.signal_type != SignalType::NEUTRAL && sig.metadata.count("confidence")) {
                std::cout << "Sample signal (bar " << i << "):" << std::endl;
                std::cout << "  Confidence: " << sig.metadata.at("confidence") << std::endl;
                std::cout << "  Predicted return: " << sig.metadata.at("predicted_return") << std::endl;
                std::cout << "  Signal type: " << (sig.signal_type == SignalType::LONG ? "LONG" : "SHORT") << std::endl;
                break;
            }
        }
        
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
        
        double warmup_accuracy = warmup_total > 0 ? 
            static_cast<double>(warmup_correct) / warmup_total : 0.0;
        bool converged = warmup_accuracy > 0.51;
        
        std::cout << "\n┌─ WARMUP RESULTS ──────────────────────────────────────────┐" << std::endl;
        std::cout << "│ Bars Processed:     " << (WARMUP_BLOCKS * BARS_PER_BLOCK) << std::endl;
        std::cout << "│ Final Accuracy:     " << std::fixed << std::setprecision(2) 
                  << (warmup_accuracy * 100) << "%" << std::endl;
        std::cout << "│ Status:             " 
                  << (converged ? "✅ CONVERGED" : "⚠️  NOT CONVERGED") << std::endl;
        std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
        
        // PHASE 2: TRADING (already processed above with continuous learning)
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                  PHASE 2: TRADING                          ║" << std::endl;
        std::cout << "║            (with continuous learning)                      ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        // Calculate trading signal accuracy
        int trading_correct = 0, trading_total = 0;
        int long_count = 0, short_count = 0, neutral_count = 0;
        
        for (size_t i = 0; i < trading_signals.size(); ++i) {
            const auto& sig = trading_signals[i];
            
            if (sig.signal_type == SignalType::LONG) long_count++;
            else if (sig.signal_type == SignalType::SHORT) short_count++;
            else neutral_count++;
            
            if (i + 1 < trading_data.size() && sig.signal_type != SignalType::NEUTRAL) {
                double actual_return = (trading_data[i+1].close - trading_data[i].close) / 
                                      trading_data[i].close;
                bool correct = (sig.signal_type == SignalType::LONG && actual_return > 0) ||
                              (sig.signal_type == SignalType::SHORT && actual_return < 0);
                
                if (correct) trading_correct++;
                trading_total++;
            }
        }
        
        double trading_accuracy = trading_total > 0 ? 
            static_cast<double>(trading_correct) / trading_total : 0.0;
        double signal_coverage = static_cast<double>(long_count + short_count) / trading_signals.size();
        
        // Calculate MRB (simplified - assumes we hold position for each signal)
        // Market return: buy-and-hold over the 20 blocks
        double market_return = (trading_data.back().close - trading_data.front().close) / 
                              trading_data.front().close;
        
        // Strategy return: simulate following all signals with equal capital allocation
        // Simplified model: each signal gets equal weight, compounded
        double portfolio_value = 1.0;  // Start with $1
        int trade_count = 0;
        
        for (size_t i = 0; i < trading_signals.size() && i + 1 < trading_data.size(); ++i) {
            const auto& sig = trading_signals[i];
            if (sig.signal_type == SignalType::NEUTRAL) continue;
            
            // Calculate bar return
            double bar_return = (trading_data[i+1].close - trading_data[i].close) / 
                               trading_data[i].close;
            
            // Reverse for SHORT
            if (sig.signal_type == SignalType::SHORT) {
                bar_return = -bar_return;
            }
            
            // Compound the return (simplified: full capital on each signal)
            portfolio_value *= (1.0 + bar_return);
            trade_count++;
        }
        
        // Strategy total return
        double strategy_return = portfolio_value - 1.0;
        
        // MRB = Strategy return - Market return (in percentage points)
        double mrb = (strategy_return - market_return) * 100.0;
        
        // RESULTS
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                   TRADING RESULTS                          ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        std::cout << "\n┌─ SIGNAL STATISTICS ───────────────────────────────────────┐" << std::endl;
        std::cout << "│ Total Signals:      " << trading_signals.size() << std::endl;
        std::cout << "│ LONG:               " << long_count << " (" 
                  << std::fixed << std::setprecision(1) << (100.0 * long_count / trading_signals.size()) << "%)" << std::endl;
        std::cout << "│ SHORT:              " << short_count << " (" 
                  << std::fixed << std::setprecision(1) << (100.0 * short_count / trading_signals.size()) << "%)" << std::endl;
        std::cout << "│ NEUTRAL:            " << neutral_count << " (" 
                  << std::fixed << std::setprecision(1) << (100.0 * neutral_count / trading_signals.size()) << "%)" << std::endl;
        std::cout << "│ Signal Coverage:    " << std::fixed << std::setprecision(2) 
                  << (signal_coverage * 100) << "%" << std::endl;
        std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
        
        std::cout << "\n┌─ ACCURACY METRICS ────────────────────────────────────────┐" << std::endl;
        std::cout << "│ Warmup Accuracy:    " << std::fixed << std::setprecision(2) 
                  << (warmup_accuracy * 100) << "%" << std::endl;
        std::cout << "│ Trading Accuracy:   " << std::fixed << std::setprecision(2) 
                  << (trading_accuracy * 100) << "%" 
                  << (trading_accuracy > 0.52 ? " ✅" : " ⚠️") << std::endl;
        std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
        
        std::cout << "\n┌─ PERFORMANCE (20 BLOCKS) ─────────────────────────────────┐" << std::endl;
        std::cout << "│ Market Return:      " << std::fixed << std::setprecision(2) 
                  << (market_return * 100) << "% (buy-and-hold)" << std::endl;
        std::cout << "│ Strategy Return:    " << std::fixed << std::setprecision(2) 
                  << (strategy_return * 100) << "% (compounded)" << std::endl;
        std::cout << "│ MRB:                " << std::fixed << std::setprecision(2) 
                  << mrb << "%" 
                  << (mrb > 0 ? " ✅" : " ❌") << std::endl;
        std::cout << "│ Trade Count:        " << trade_count << std::endl;
        std::cout << "│ Portfolio Value:    $" << std::fixed << std::setprecision(2) 
                  << portfolio_value << " (from $1.00)" << std::endl;
        std::cout << "└───────────────────────────────────────────────────────────┘" << std::endl;
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        if (converged && trading_accuracy > 0.52 && mrb > 0) {
            std::cout << "║                    ✅ TEST SUCCESSFUL                      ║" << std::endl;
            std::cout << "║                                                            ║" << std::endl;
            std::cout << "║  Strategy converged and outperformed market                ║" << std::endl;
        } else if (!converged) {
            std::cout << "║                    ⚠️  INSUFFICIENT WARMUP                 ║" << std::endl;
            std::cout << "║                                                            ║" << std::endl;
            std::cout << "║  Strategy needs more warmup to converge                   ║" << std::endl;
        } else if (trading_accuracy <= 0.52) {
            std::cout << "║                    ⚠️  LOW ACCURACY                        ║" << std::endl;
            std::cout << "║                                                            ║" << std::endl;
            std::cout << "║  Trading accuracy below 52% threshold                     ║" << std::endl;
        } else {
            std::cout << "║                    ❌ UNDERPERFORMED MARKET                ║" << std::endl;
            std::cout << "║                                                            ║" << std::endl;
            std::cout << "║  Strategy did not beat buy-and-hold                       ║" << std::endl;
        }
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        std::cout << "\n📊 NOTE: This is a simplified backtest." << std::endl;
        std::cout << "   - Assumes full capital on each signal (unrealistic)" << std::endl;
        std::cout << "   - No transaction costs or slippage" << std::endl;
        std::cout << "   - For realistic backtest with ensemble PSM, use: online-trade command" << std::endl;
        std::cout << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
