#include "strategy/istrategy.h"
#include "strategy/strategy_adapters.h"
#include "strategy/xgb_strategy_base.h"
#include "strategy/xgb_feature_set_25intraday.h"
#include "strategy/xgb_feature_set_8detector.h"
#include "strategy/xgb_feature_set_60sa.h"
#include "strategy/awr_strategy.h"
#include "common/utils.h"
#include <stdexcept>
#include <algorithm>

namespace sentio {

std::unique_ptr<IStrategy> create_strategy(const std::string& strategy_name) {
    std::string lower = strategy_name; 
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    // Direct-name overrides for XGB feature-set variants
    if (lower == "xgb-8" || lower == "xgb8" || lower == "xgb_8detector") {
        utils::log_info("  - Creating XGBStrategyBase (8Detector)");
        return std::make_unique<XGBStrategyBase>(std::make_unique<XGBFeatureSet8Detector>());
    }
    if (lower == "xgb-25" || lower == "xgb25" || lower == "xgb_25intraday") {
        utils::log_info("  - Creating XGBStrategyBase (25Intraday)");
        return std::make_unique<XGBStrategyBase>(std::make_unique<XGBFeatureSet25Intraday>());
    }
    if (lower == "xgb-60sa" || lower == "xgb60sa" || lower == "xgb_60sa") {
        utils::log_info("  - Creating XGBStrategyBase (60SA)");
        return std::make_unique<XGBStrategyBase>(std::make_unique<XGBFeatureSet60SA>());
    }
    return create_strategy(string_to_strategy_type(strategy_name));
}

std::unique_ptr<IStrategy> create_strategy(StrategyType type) {
    utils::log_info("🏭 StrategyFactory creating strategy type: " + std::to_string(static_cast<int>(type)));
    
    switch (type) {
        case StrategyType::OPTIMIZED_SGO:
            utils::log_info("  - Creating OptimizedSigorStrategyAdapter (Enhanced SGO - Default)");
            return std::make_unique<OptimizedSigorStrategyAdapter>();
            
        case StrategyType::XGBOOST:
            utils::log_info("  - Creating XGBStrategyBase with 25Intraday features");
            return std::make_unique<XGBStrategyBase>(std::make_unique<XGBFeatureSet25Intraday>());
            
        // LightGBM removed - use XGBoost instead (same gradient boosting approach)
            
        case StrategyType::CATBOOST:
            utils::log_info("  - Creating CatBoostStrategyAdapter");
            return std::make_unique<CatBoostStrategyAdapter>();
            
        case StrategyType::TFT:
            utils::log_info("  - Creating TFTStrategyAdapter");
            return std::make_unique<TFTStrategyAdapter>();
            
        case StrategyType::WILLIAMS_RSI_TSI:
            throw std::invalid_argument("WRT strategy removed");
            
        case StrategyType::WILLIAMS_RSI_BB:
            utils::log_info("  - Creating WilliamsRSIBBStrategyAdapter");
            return std::make_unique<WilliamsRSIBBStrategyAdapter>();
            
        case StrategyType::WILLIAMS_RSI:
            utils::log_info("  - Creating WilliamsRsiStrategyAdapter");
            return std::make_unique<WilliamsRsiStrategyAdapter>();
        // MOMENTUM removed (deprecated)
            
        case StrategyType::DETERMINISTIC_TEST:
            utils::log_info("  - Creating DeterministicTestStrategyAdapter");
            return std::make_unique<DeterministicTestStrategyAdapter>();
            
        case StrategyType::CHEAT:
            utils::log_info("  - Creating CheatStrategyAdapter (looks ahead for validation)");
            return std::make_unique<CheatStrategyAdapter>();

        // MOMENTUM_SCALPER removed (deprecated)

        default:
            throw std::invalid_argument("Unknown strategy type: " + std::to_string(static_cast<int>(type)));
    }
}

std::vector<std::string> get_available_strategies() {
    std::vector<std::string> strategies = {"sgo", "optimized_sgo"};

    // ML strategies with short names
    strategies.push_back("xgb");      // Default maps to 25Intraday
    strategies.push_back("xgb-8");    // XGBoost over 8-detector feature set
    strategies.push_back("xgb-25");   // XGBoost over 25 intraday features
    strategies.push_back("xgb-60sa"); // XGBoost over 45-feature 60% SA target set
    strategies.push_back("ctb");      // CatBoost
    strategies.push_back("tft");      // Temporal Fusion Transformer
    
    // Technical indicator strategies
    // WRT removed
    strategies.push_back("wr");     // Williams/RSI (no TSI)
    
    // Test strategies
    strategies.push_back("deterministic_test");  // Deterministic test strategy
    strategies.push_back("cheat");  // Cheat strategy for validation

    // momentum removed (deprecated)

    return strategies;
}

bool is_valid_strategy(const std::string& strategy_name) {
    std::string lower = strategy_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Check for direct XGB feature set variants first
    if (lower == "xgb-8" || lower == "xgb8" || lower == "xgb_8detector" ||
        lower == "xgb-25" || lower == "xgb25" || lower == "xgb_25intraday" ||
        lower == "xgb-60sa" || lower == "xgb60sa" || lower == "xgb_60sa") {
        return true;
    }
    
    try {
        string_to_strategy_type(strategy_name);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

StrategyType string_to_strategy_type(const std::string& strategy_name) {
    std::string lower_name = strategy_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name == "sgo" || lower_name == "sigor" || 
        lower_name == "optimized_sgo" || lower_name == "opt_sgo" || lower_name == "sgo_opt") {
        return StrategyType::OPTIMIZED_SGO;
    } else if (lower_name == "xgb" || lower_name == "xgboost" || lower_name == "xgb-8" || lower_name == "xgb8" || lower_name == "xgb_8detector" || lower_name == "xgb-25" || lower_name == "xgb25" || lower_name == "xgb_25intraday") {
        return StrategyType::XGBOOST;  // Intraday XGBoost
    } else if (lower_name == "ctb" || lower_name == "catboost" || lower_name == "cb") {
        return StrategyType::CATBOOST;  // Accept both old and new names
    } else if (lower_name == "tft" || lower_name == "temporal_fusion_transformer") {
        return StrategyType::TFT;
    } else if (lower_name == "wrt" || lower_name == "williams_rsi_tsi" || 
               lower_name == "williamsrsitsi") {
        throw std::invalid_argument("WRT strategy removed");
    } else if (lower_name == "wrb" || lower_name == "williams_rsi_bb" || 
               lower_name == "williamsrsibb" || lower_name == "williams_bb") {
        return StrategyType::WILLIAMS_RSI_BB;
    } else if (lower_name == "wr" || lower_name == "williams_rsi" || 
               lower_name == "williams" || lower_name == "williamsrsi") {
        return StrategyType::WILLIAMS_RSI;
    } else if (lower_name == "deterministic_test" || lower_name == "det_test" || 
               lower_name == "test" || lower_name == "dt") {
        return StrategyType::DETERMINISTIC_TEST;
    } else if (lower_name == "cheat" || lower_name == "cheat_strategy") {
        return StrategyType::CHEAT;
    } else {
        throw std::invalid_argument("Unknown strategy name: " + strategy_name);
    }
}

std::string strategy_type_to_string(StrategyType type) {
    switch (type) {
        case StrategyType::OPTIMIZED_SGO: return "sgo";  // Enhanced SGO is now default
        case StrategyType::XGBOOST: return "xgb";       // Intraday XGBoost
        case StrategyType::CATBOOST: return "ctb";      // Renamed from catboost
        case StrategyType::TFT: return "tft";
        case StrategyType::WILLIAMS_RSI_TSI: return "removed";
        case StrategyType::WILLIAMS_RSI_BB: return "wrb";
        case StrategyType::WILLIAMS_RSI: return "wr";
        case StrategyType::DETERMINISTIC_TEST: return "deterministic_test";
        case StrategyType::CHEAT: return "cheat";
        default: return "unknown";
    }
}

} // namespace sentio
