#pragma once

#include "cli/command_interface.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * Sanity check for online learning strategies
 * Measures convergence, signal quality, and learning stability
 */
class OnlineSanityCheckCommand : public Command {
public:
    struct OnlineSanityMetrics {
        // Convergence metrics
        double warmup_accuracy = 0.0;      // Accuracy in first 100 bars
        double converged_accuracy = 0.0;   // Accuracy after 500 bars
        int convergence_bar = 0;           // Bar where accuracy stabilized
        
        // Signal quality metrics
        double signal_accuracy = 0.0;      // Overall directional accuracy
        double signal_coverage = 0.0;      // % non-neutral signals
        double avg_confidence = 0.0;       // Average confidence of signals
        
        // Ensemble metrics
        double horizon_1bar_accuracy = 0.0;
        double horizon_5bar_accuracy = 0.0;
        double horizon_10bar_accuracy = 0.0;
        double ensemble_agreement = 0.0;   // How often horizons agree
        
        // Learning stability
        double prediction_volatility = 0.0; // Stability of predictions
        double weight_change_rate = 0.0;    // How fast weights are changing
        
        // Trading metrics (if backtest enabled)
        double mrb = 0.0;                   // Market-relative benchmark
        int total_trades = 0;
        double win_rate = 0.0;
        
        // Quality assessment
        bool passed_warmup = false;         // > 48% after 100 bars
        bool passed_convergence = false;    // > 52% after 500 bars
        bool passed_signal_quality = false; // > 10% coverage, > 0.3 confidence
        bool passed_overall = false;
    };
    
    std::string get_name() const override;
    std::string get_description() const override;
    std::vector<std::string> get_usage() const;
    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    OnlineSanityMetrics run_sanity_check(
        const std::string& strategy_name,
        const std::string& data_path,
        bool run_backtest = false
    );
    
    void print_metrics(const OnlineSanityMetrics& metrics, const std::string& strategy_name);
    void print_usage() const;
};

} // namespace cli
} // namespace sentio
