#pragma once

// =============================================================================
// Module: strategy/strategy_component.h
// Purpose: Base strategy abstraction and a concrete example (SigorStrategy).
//
// Core idea:
// - A strategy processes a stream of Bars, maintains internal indicators, and
//   emits SignalOutput records once warm-up is complete.
// - The base class provides the ingest/export orchestration; derived classes
//   implement indicator updates and signal generation.
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "common/types.h"
#include "signal_output.h"

namespace sentio {

class StrategyComponent {
public:
    struct StrategyConfig {
        std::string name = "default";
        std::string version = "1.0";
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        int warmup_bars = 250;
        std::map<std::string, double> params;
        
        // NEW: Multi-bar prediction metadata
        std::map<std::string, std::string> metadata;
    };

    explicit StrategyComponent(const StrategyConfig& config);
    virtual ~StrategyComponent() = default;

    // Process a dataset file of Bars and return generated signals.
    virtual std::vector<SignalOutput> process_dataset(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params
    );

    // Process a specific range of bars from dataset (index-based, high-performance)
    virtual std::vector<SignalOutput> process_dataset_range(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params,
        uint64_t start_index = 0,
        uint64_t count = 0  // 0 = process from start_index to end
    );

    // Export signals to file in jsonl or csv format.
    virtual bool export_signals(
        const std::vector<SignalOutput>& signals,
        const std::string& output_path,
        const std::string& format = "jsonl"
    );
    
    // Public interface to process a single bar (eliminates duplicates)
    SignalOutput process_bar(const Bar& bar, int bar_index) {
        update_indicators(bar);
        return generate_signal(bar, bar_index);
    }

protected:
    // Hooks for strategy authors to implement
    virtual SignalOutput generate_signal(const Bar& bar, int bar_index);
    virtual void update_indicators(const Bar& bar);
    virtual bool is_warmed_up() const;

protected:
    StrategyConfig config_;
    std::vector<Bar> historical_bars_;
    int bars_processed_ = 0;
    bool warmup_complete_ = false;

    // Example internal indicators
    std::vector<double> moving_average_;
    std::vector<double> volatility_;
    std::vector<double> momentum_;
};

// Note: SigorStrategy is defined in `strategy/sigor_strategy.h`.

} // namespace sentio


