#pragma once

#include "strategy/istrategy.h"
#include "strategy/xgb_feature_set.h"
#include "training/feature_normalizer.h"
#include <memory>

namespace sentio {

class XGBStrategyBase : public IStrategy {
public:
    explicit XGBStrategyBase(std::unique_ptr<XGBFeatureSet> featureSet);
    ~XGBStrategyBase() override;

    bool initialize(const StrategyComponent::StrategyConfig& config) override;
    std::vector<SignalOutput> process_data(const std::vector<::sentio::Bar>& market_data) override;
    std::string get_strategy_name() const override { return strategy_name_; }
    std::string get_strategy_version() const override { return "1.0"; }
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 100; }
    bool validate() const override { return initialized_; }
    std::map<std::string, std::string> get_metadata() const override;
    void reset() override;

protected:
    virtual std::string feature_set_name() const { return feature_set_->name(); }
    virtual std::string model_dir() const;
    virtual std::string model_path() const;
    virtual std::string checksum_path() const;

private:
    std::unique_ptr<XGBFeatureSet> feature_set_;
    std::unique_ptr<training::FeatureNormalizer> feature_normalizer_;
    StrategyComponent::StrategyConfig config_{};
    std::string strategy_name_;
    bool initialized_{false};
    void* booster_{nullptr};
};

} // namespace sentio





