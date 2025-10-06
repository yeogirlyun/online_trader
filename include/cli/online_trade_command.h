#pragma once

#include "cli/command_interface.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * Trade with online learning strategy using ensemble PSM
 * Supports warmup period followed by live trading
 */
class OnlineTradeCommand : public Command {
public:
    struct OnlineTradeConfig {
        std::string strategy_name;
        std::string data_path;
        std::string output_path;
        
        // Warmup/convergence settings
        int warmup_blocks = 1;              // Number of blocks for convergence (default: 1 block = 480 bars)
        int warmup_bars = 0;                // Alternative: specify exact bars
        bool skip_warmup_trades = true;     // Don't trade during warmup
        
        // Trading settings
        int trade_blocks = 20;              // Number of blocks to trade (default: 20)
        int trade_bars = 0;                 // Alternative: specify exact bars
        double starting_capital = 100000.0;
        bool use_leverage = true;
        
        // Ensemble PSM settings
        bool use_ensemble_psm = true;
        double min_agreement = 0.6;         // Minimum horizon agreement to trade
        double base_allocation = 0.3;       // Base allocation per horizon
        double consensus_bonus = 0.4;       // Extra allocation for agreement
        
        // Output settings
        bool save_signals = true;
        bool save_trades = true;
        bool save_state = true;
        std::string signals_path;
        std::string trades_path;
    };
    
    struct OnlineTradeResults {
        // Warmup metrics
        int warmup_bars_used = 0;
        double warmup_final_accuracy = 0.0;
        bool converged = false;
        
        // Trading metrics
        int trading_bars = 0;
        int total_trades = 0;
        double final_capital = 0.0;
        double total_return = 0.0;
        double mrb = 0.0;
        double sharpe_ratio = 0.0;
        double max_drawdown = 0.0;
        
        // Signal metrics during trading
        double trading_signal_accuracy = 0.0;
        double signal_coverage = 0.0;
        double avg_confidence = 0.0;
        
        // Ensemble metrics
        double horizon_1bar_trades = 0;
        double horizon_5bar_trades = 0;
        double horizon_10bar_trades = 0;
        double avg_ensemble_agreement = 0.0;
        
        // Performance by horizon
        double horizon_1bar_return = 0.0;
        double horizon_5bar_return = 0.0;
        double horizon_10bar_return = 0.0;
    };
    
    std::string get_name() const override;
    std::string get_description() const override;
    std::vector<std::string> get_usage() const;
    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    OnlineTradeResults run_online_trade(const OnlineTradeConfig& config);
    void print_results(const OnlineTradeResults& results, const OnlineTradeConfig& config);
    void print_usage() const;
    
    OnlineTradeConfig parse_args(const std::vector<std::string>& args);
};

} // namespace cli
} // namespace sentio
