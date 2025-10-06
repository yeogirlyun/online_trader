#include "strategy/strategy_component.h"
#include "common/utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>

namespace sentio {

// -----------------------------------------------------------------------------
// StrategyComponent: orchestrates ingestion of Bars and generation of signals.
// -----------------------------------------------------------------------------
StrategyComponent::StrategyComponent(const StrategyConfig& config)
    : config_(config), bars_processed_(0), warmup_complete_(false) {
    historical_bars_.reserve(static_cast<size_t>(std::max(0, config_.warmup_bars)));
}

std::vector<SignalOutput> StrategyComponent::process_dataset(
    const std::string& dataset_path,
    const std::string& strategy_name,
    const std::map<std::string, std::string>& /*strategy_params*/) {

    std::vector<SignalOutput> signals;
    auto bars = utils::read_csv_data(dataset_path);

    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& bar = bars[i];
        update_indicators(bar);

        if (is_warmed_up()) {
            auto signal = generate_signal(bar, static_cast<int>(i));
            signal.strategy_name = strategy_name;
            signal.strategy_version = config_.version;
            signals.push_back(signal);
        }

        bars_processed_++;
    }

    return signals;
}

std::vector<SignalOutput> StrategyComponent::process_dataset_range(
    const std::string& dataset_path,
    const std::string& strategy_name,
    const std::map<std::string, std::string>& /*strategy_params*/,
    uint64_t start_index,
    uint64_t count) {

    std::vector<SignalOutput> signals;
    
    // Use high-performance index-based loading (binary format preferred)
    auto bars = utils::read_market_data_range(dataset_path, start_index, count);
    
    if (bars.empty()) {
        utils::log_error("Failed to load market data range: start=" + std::to_string(start_index) + 
                        ", count=" + std::to_string(count) + ", path=" + dataset_path);
        return signals;
    }
    
    utils::log_info("Processing " + std::to_string(bars.size()) + " bars from index " + 
                   std::to_string(start_index) + " (strategy=" + strategy_name + ")");

    // Process bars with adjusted bar indices (maintain original indexing)
    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& bar = bars[i];
        update_indicators(bar);

        if (is_warmed_up()) {
            auto signal = generate_signal(bar, static_cast<int>(start_index + i));
            signal.strategy_name = strategy_name;
            signal.strategy_version = config_.version;
            signals.push_back(signal);
        }

        bars_processed_++;
    }

    return signals;
}

bool StrategyComponent::export_signals(
    const std::vector<SignalOutput>& signals,
    const std::string& output_path,
    const std::string& format) {

    if (format == "jsonl") {
        std::vector<std::string> lines;
        lines.reserve(signals.size());
        for (const auto& signal : signals) {
            lines.push_back(signal.to_json());
        }
        return utils::write_jsonl(output_path, lines);
    } else if (format == "csv") {
        std::vector<std::vector<std::string>> data;
        // Header row
        data.push_back({"timestamp_ms", "bar_index", "symbol", "probability",
                       "strategy_name", "strategy_version"});
        for (const auto& signal : signals) {
            data.push_back({
                std::to_string(signal.timestamp_ms),
                std::to_string(signal.bar_index),
                signal.symbol,
                std::to_string(signal.probability),
                signal.strategy_name,
                signal.strategy_version
            });
        }
        return utils::write_csv(output_path, data);
    }

    return false;
}

SignalOutput StrategyComponent::generate_signal(const Bar& bar, int bar_index) {
    // Default implementation: neutral placeholder
    SignalOutput signal;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.symbol = bar.symbol;
    signal.probability = 0.5;
    signal.metadata["warmup_complete"] = warmup_complete_ ? "true" : "false";
    return signal;
}

void StrategyComponent::update_indicators(const Bar& bar) {
    historical_bars_.push_back(bar);
    if (historical_bars_.size() > static_cast<size_t>(std::max(0, config_.warmup_bars))) {
        historical_bars_.erase(historical_bars_.begin());
    }

    // Example: rolling simple moving average of 20 bars
    if (historical_bars_.size() >= 20) {
        double sum = 0;
        for (size_t i = historical_bars_.size() - 20; i < historical_bars_.size(); ++i) {
            sum += historical_bars_[i].close;
        }
        moving_average_.push_back(sum / 20.0);
    }
}

bool StrategyComponent::is_warmed_up() const {
    return bars_processed_ >= config_.warmup_bars;
}

// SigorStrategy implementation moved to strategy/sigor_strategy.cpp

} // namespace sentio


