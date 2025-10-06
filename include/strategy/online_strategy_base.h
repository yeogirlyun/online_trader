#pragma once

#include "strategy/istrategy.h"
#include "strategy/xgb_feature_set.h"
#include "learning/online_predictor.h"
#include <memory>
#include <deque>
#include <map>

namespace sentio {

class OnlineStrategyBase : public IStrategy {
public:
    struct OnlineConfig {
        int prediction_horizon;
        int signal_interval;
        bool use_ensemble;
        std::vector<int> ensemble_horizons;
        double confidence_threshold;
        double signal_threshold;
        std::string state_dir;
        bool save_state_periodically;
        int save_interval_bars;
        
        OnlineConfig() 
            : prediction_horizon(5),
              signal_interval(3),
              use_ensemble(true),
              ensemble_horizons({1, 5, 10}),
              confidence_threshold(0.6),
              signal_threshold(0.0001),
              state_dir("artifacts/online/"),
              save_state_periodically(true),
              save_interval_bars(1000) {}
    };
    
    explicit OnlineStrategyBase(std::unique_ptr<XGBFeatureSet> featureSet,
                               const OnlineConfig& config = OnlineConfig());
    ~OnlineStrategyBase() override;
    
    bool initialize(const StrategyComponent::StrategyConfig& config) override;
    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override;
    std::string get_strategy_name() const override { return strategy_name_; }
    std::string get_strategy_version() const override { return "2.0-online"; }
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 100; }
    bool validate() const override { return initialized_; }
    std::map<std::string, std::string> get_metadata() const override;
    void reset() override;
    
    // Online-specific methods
    void save_state() const;
    bool load_state();
    double get_current_accuracy() const;
    
private:
    std::unique_ptr<XGBFeatureSet> feature_set_;
    std::unique_ptr<learning::MultiHorizonPredictor> predictor_;
    OnlineConfig online_config_;
    StrategyComponent::StrategyConfig strategy_config_;
    std::string strategy_name_;
    bool initialized_{false};
    
    struct FeatureSnapshot {
        std::vector<double> features;
        uint64_t bar_id;
        int bar_index;
        double close_price;
    };
    std::deque<FeatureSnapshot> feature_history_;
    
    SignalOutput create_signal(const Bar& bar, int bar_index,
                              const learning::OnlinePredictor::PredictionResult& prediction);
    void update_with_realized_returns(const Bar& current_bar, int current_index);
    std::map<int, SignalOutput> create_ensemble_signals(const Bar& bar, int bar_index,
                                                        const std::vector<double>& features);
};

} // namespace sentio
