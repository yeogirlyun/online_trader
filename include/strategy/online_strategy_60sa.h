#pragma once

#include "strategy/online_strategy_base.h"
#include "strategy/xgb_feature_set_60sa.h"
#include <memory>

namespace sentio {

/**
 * Online learning strategy using 60SA feature set
 * Multi-horizon ensemble: 1-bar, 5-bar, 10-bar predictions
 * Continuous learning with EWRLS - no training/inference split
 */
class OnlineStrategy60SA : public OnlineStrategyBase {
public:
    OnlineStrategy60SA()
        : OnlineStrategyBase(
            std::make_unique<XGBFeatureSet60SA>(),
            create_default_config()
        ) {}
    
    explicit OnlineStrategy60SA(const OnlineConfig& config)
        : OnlineStrategyBase(
            std::make_unique<XGBFeatureSet60SA>(),
            config
        ) {}
    
    std::string get_strategy_name() const override {
        return "online_60sa";
    }
    
    static OnlineConfig create_default_config() {
        OnlineConfig config;
        config.prediction_horizon = 5;
        config.signal_interval = 3;
        config.use_ensemble = true;
        config.ensemble_horizons = {1, 5, 10};
        config.confidence_threshold = 0.1;  // Lower threshold for warmup phase
        config.signal_threshold = 0.00001;  // Very low threshold
        config.state_dir = "artifacts/online/60sa/";
        config.save_state_periodically = true;
        config.save_interval_bars = 1000;
        return config;
    }
};

} // namespace sentio
