#pragma once

#include "strategy/online_strategy_base.h"
#include "strategy/xgb_feature_set_25intraday.h"
#include <memory>

namespace sentio {

/**
 * Online learning strategy using 25-intraday feature set
 * Multi-horizon ensemble: 1-bar, 5-bar, 10-bar predictions
 * Continuous learning with EWRLS - no training/inference split
 */
class OnlineStrategy25Intraday : public OnlineStrategyBase {
public:
    OnlineStrategy25Intraday()
        : OnlineStrategyBase(
            std::make_unique<XGBFeatureSet25Intraday>(),
            create_default_config()
        ) {}
    
    explicit OnlineStrategy25Intraday(const OnlineConfig& config)
        : OnlineStrategyBase(
            std::make_unique<XGBFeatureSet25Intraday>(),
            config
        ) {}
    
    std::string get_strategy_name() const override {
        return "online_25intraday";
    }
    
    static OnlineConfig create_default_config() {
        OnlineConfig config;
        config.prediction_horizon = 5;
        config.signal_interval = 3;
        config.use_ensemble = true;
        config.ensemble_horizons = {1, 5, 10};
        config.confidence_threshold = 0.6;
        config.signal_threshold = 0.0001;
        config.state_dir = "artifacts/online/25intraday/";
        config.save_state_periodically = true;
        config.save_interval_bars = 1000;
        return config;
    }
};

} // namespace sentio
