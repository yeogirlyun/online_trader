#pragma once

#include "strategy/istrategy.h"
#include "detectors/awr_detector.h"
#include <memory>

namespace sentio {

class AWRStrategy : public IStrategy {
public:
    AWRStrategy() = default;
    ~AWRStrategy() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override;
    std::vector<SignalOutput> process_data(const std::vector<::sentio::Bar>& market_data) override;
    std::string get_strategy_name() const override { return "awr"; }
    std::string get_strategy_version() const override { return "1.0"; }
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 5; }
    bool validate() const override { return detector_ != nullptr; }
    std::map<std::string, std::string> get_metadata() const override;
    void reset() override;

private:
    std::unique_ptr<AWRDetector> detector_;
    StrategyComponent::StrategyConfig config_{};
};

} // namespace sentio





