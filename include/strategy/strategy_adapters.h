#pragma once

#include "strategy/istrategy.h"
// PPO headers removed for this project scope
#include "common/utils.h"
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <chrono>

#ifdef MOMENTUM_SCALPER_AVAILABLE
#include "strategy/momentum_scalper.h"
#endif

#include "strategy/catboost_strategy.h"
#include "strategy/tft_strategy.h"
#include "strategy/intraday_ml_strategies.h"
#include "strategy/intraday_catboost_strategy.h"
#include "strategy/optimized_sigor_strategy.h"
#include "strategy/williams_rsi_bb_strategy.h"
#include "strategy/williams_rsi_strategy.h"
#include "strategy/deterministic_test_strategy.h"
#include "strategy/cheat_strategy.h"
#include "strategy/config_resolver.h"

namespace sentio {

/**
 * @brief Base Strategy Adapter Template (eliminates ODR violations)
 */
template<typename StrategyType, typename ConfigType>
class BaseStrategyAdapter : public IStrategy {
protected:
    std::unique_ptr<StrategyType> strategy_;
    ConfigType config_;
    
public:
    
    bool validate() const override {
        return strategy_ != nullptr;
    }
};

/**
 * Adapter class for Optimized SGO Strategy (Enhanced SGO - now default)
 */
class OptimizedSigorStrategyAdapter : public IStrategy {
public:
    OptimizedSigorStrategyAdapter() = default;
    ~OptimizedSigorStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        // Load configuration from JSON file
        OptimizedSigorConfig opt_config = OptimizedSigorConfig::from_file("config/sgo_optimized_config.json");
        strategy_ = std::make_unique<OptimizedSigorStrategy>(opt_config);
        utils::log_info("OptimizedSigorStrategy initialized from config/sgo_optimized_config.json");
        return true;
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("OptimizedSigorStrategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        // Process each bar
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->generate_signal(market_data[i], i);
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override { return "optimized_sgo"; }
    std::string get_strategy_version() const override { return "1.0"; }
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 50; }  // Need more warmup for regime detection
    
    bool validate() const override {
        return strategy_ != nullptr;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"detectors", "7_enhanced"},
            {"aggregation", "soft_fusion"},
            {"type", "optimized_rule_based"},
            {"filtering", "production_grade"},
            {"min_confidence", "0.80"},
            {"consecutive_bars", "3"},
            {"hysteresis", "enabled"},
            {"whipsaw_protection", "enabled"},
            {"target_mrb", ">0.5%"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<OptimizedSigorStrategy> strategy_;
};

// PPO adapters removed for this project scope

/**
 * Adapter class to make IntradayXGBoostStrategy compatible with IStrategy interface
 */
class XGBoostStrategyAdapter : public IStrategy {
public:
    XGBoostStrategyAdapter() = default;
    ~XGBoostStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        utils::log_info("🔧 Initializing XGBoost Intraday Strategy...");
        try {
            std::string target_symbol = "QQQ";
            
            auto xgb_config = intraday::IntradayXGBoostStrategy::Config{};
            xgb_config.model_path = "artifacts/Intraday/xgboost_intraday.json";
            
            strategy_ = std::make_unique<intraday::IntradayXGBoostStrategy>(
                config, xgb_config, target_symbol
            );
            
            if (strategy_->initialize()) {
                utils::log_info("✅ XGBoost Intraday Strategy initialized successfully.");
                return true;
            } else {
                utils::log_error("❌ XGBoost initialization failed.");
                return false;
            }
        } catch (const std::exception& e) {
            utils::log_error("❌ Exception during XGBoost initialization: " + std::string(e.what()));
            return false;
        }
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("XGBoost strategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->process_bar(market_data[i], static_cast<int>(i));
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override { return "xgb"; }
    std::string get_strategy_version() const override { return "2.0"; }
    bool requires_warmup() const override { return true; }
    bool validate() const override { return strategy_ != nullptr; }
    int get_warmup_bars() const override { return 100; }
    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"model_type", "xgboost_intraday"},
            {"features", "25"},
            {"feature_type", "intraday"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<intraday::IntradayXGBoostStrategy> strategy_;
};

// LightGBMStrategyAdapter removed - redundant with XGBoost

// LightGBM adapter completely removed - use XGBoost instead
// XGBoost and LightGBM are both gradient boosting, XGBoost has proven C++ API

/**
 * Adapter class to make CatBoostStrategy compatible with IStrategy interface
 */
class CatBoostStrategyAdapter : public IStrategy {
public:
    CatBoostStrategyAdapter() = default;
    ~CatBoostStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        utils::log_info("🔧 Initializing CatBoost Intraday Strategy (Real Trees)...");
        try {
            std::string target_symbol = "QQQ";
            
            auto cb_config = intraday::IntradayCatBoostStrategy::Config{};
            cb_config.model_path = "artifacts/CatBoost/catboost_model.cpp";
            
            strategy_ = std::make_unique<intraday::IntradayCatBoostStrategy>(
                config, cb_config, target_symbol
            );
            
            if (strategy_->initialize()) {
                utils::log_info("✅ CatBoost Intraday Strategy initialized successfully.");
                return true;
            } else {
                utils::log_error("❌ CatBoost initialization failed.");
                return false;
            }
        } catch (const std::exception& e) {
            utils::log_error("❌ Exception during CatBoost initialization: " + std::string(e.what()));
            return false;
        }
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("CatBoostStrategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        // Process bars with internal warmup tracking
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->process_bar(market_data[i], static_cast<int>(i));
            
            // Validate signal has required fields
            if (signal.symbol.empty() || signal.symbol == "UNKNOWN") {
                utils::log_error("CatBoost produced invalid symbol at bar " + std::to_string(i));
                throw std::runtime_error("CatBoost symbol configuration failed");
            }
            
            if (signal.strategy_name.empty()) {
                utils::log_error("CatBoost missing strategy_name at bar " + std::to_string(i));
                throw std::runtime_error("CatBoost metadata configuration failed");
            }
            
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override { return "ctb"; }  // Renamed from catboost
    std::string get_strategy_version() const override { return "3.0"; }  // v3.0 = Real CatBoost trees
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 100; }
    
    bool validate() const override {
        if (!strategy_) {
            utils::log_error("CatBoost strategy is null");
            return false;
        }
        
        // Just check if initialized, not if ready (warmup happens during processing)
        return true;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"model_type", "catboost_intraday"},
            {"model_path", "artifacts/CatBoost/catboost_intraday_25f.cbm"},
            {"feature_count", "25"},
            {"warmup_bars", "100"},
            {"type", "gradient_boosting_ml"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<intraday::IntradayCatBoostStrategy> strategy_;
};

// LightGBMStrategyAdapter removed - will be replaced with new implementation

/**
 * Adapter class to make TFTStrategy compatible with IStrategy interface
 */
class TFTStrategyAdapter : public IStrategy {
public:
    TFTStrategyAdapter() = default;
    ~TFTStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        utils::log_info("🔧 Initializing TFTStrategyAdapter...");
        try {
            // Default to QQQ for standard market data
            std::string target_symbol = "QQQ";
            
            // Create strategy with symbol parameter
            strategy_ = std::make_unique<TFTStrategy>(config, target_symbol);
            
            if (strategy_->initialize()) {
                utils::log_info("✅ TFTStrategyAdapter initialized successfully.");
                return true;
            } else {
                utils::log_error("❌ TFTStrategy initialization failed.");
                return false;
            }
        } catch (const std::exception& e) {
            utils::log_error("❌ Exception during TFTStrategyAdapter initialization: " + std::string(e.what()));
            return false;
        }
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("TFTStrategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        // Process bars with internal warmup tracking
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->process_bar(market_data[i], static_cast<int>(i));
            
            // Validate signal has required fields
            if (signal.symbol.empty() || signal.symbol == "UNKNOWN") {
                utils::log_error("TFT produced invalid symbol at bar " + std::to_string(i));
                throw std::runtime_error("TFT symbol configuration failed");
            }
            
            if (signal.strategy_name.empty()) {
                utils::log_error("TFT missing strategy_name at bar " + std::to_string(i));
                throw std::runtime_error("TFT metadata configuration failed");
            }
            
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override { return "tft"; }
    std::string get_strategy_version() const override { return "1.0"; }
    bool requires_warmup() const override { return true; }
    int get_warmup_bars() const override { return 100; }  // TFT needs more warmup
    
    bool validate() const override {
        if (!strategy_) {
            utils::log_error("TFT strategy is null");
            return false;
        }
        
        // Just check if initialized, not if ready (warmup happens during processing)
        return true;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"model_type", "tft"},
            {"model_path", "artifacts/TFT/tft_signal.pt"},
            {"feature_count", "20"},
            {"warmup_bars", "100"},
            {"prediction_length", "5"},
            {"context_length", "50"},
            {"architecture", "temporal_fusion_transformer"},
            {"type", "deep_learning_time_series"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<TFTStrategy> strategy_;
};

/**
 * Adapter class for Williams/RSI/TSI strategy
 */
// WRT strategy removed

// Williams/RSI/Bollinger Bands Strategy Adapter
class WilliamsRSIBBStrategyAdapter : public IStrategy {
public:
    WilliamsRSIBBStrategyAdapter() = default;
    ~WilliamsRSIBBStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        // Try to load configuration from JSON file if it exists
        std::string config_path = "config/williams_rsi_bb_config.json";
        
        try {
            if (std::filesystem::exists(config_path)) {
                WilliamsRSIBBConfig wrb_config = WilliamsRSIBBConfig::from_file(config_path);
                strategy_ = std::make_unique<WilliamsRSIBBStrategy>(wrb_config);
                utils::log_info("WilliamsRSIBBStrategy initialized from " + config_path);
            } else {
                // Use default configuration
                strategy_ = std::make_unique<WilliamsRSIBBStrategy>();
                utils::log_info("WilliamsRSIBBStrategy initialized with default configuration");
            }
        } catch (const std::exception& e) {
            utils::log_error("Failed to load config from " + config_path + ": " + e.what());
            utils::log_info("Falling back to default configuration");
            strategy_ = std::make_unique<WilliamsRSIBBStrategy>();
        }
        
        return true;
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("WilliamsRSIBBStrategy not initialized");
        }

        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());

        for (size_t i = 0; i < market_data.size(); ++i) {
            SignalOutput signal = strategy_->generate_signal(market_data[i], static_cast<int>(i));
            signals.push_back(signal);
        }

        return signals;
    }

    std::string get_strategy_name() const override {
        return strategy_ ? strategy_->get_name() : "williams_rsi_bb";
    }

    std::string get_strategy_version() const override {
        return strategy_ ? strategy_->get_version() : "3.0";
    }

    bool requires_warmup() const override {
        return true;
    }

    int get_warmup_bars() const override {
        return 50;
    }

    bool validate() const override {
        return strategy_ != nullptr;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"strategy_type", "williams_rsi_bb"},
            {"version", "3.0"},
            {"indicators", "Williams %R, RSI, Bollinger Bands"},
            {"signal_logic", "Cross + BB mean reversion"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<WilliamsRSIBBStrategy> strategy_;
};

/**
 * Adapter for Williams/RSI Strategy (no TSI gating)
 */
class WilliamsRsiStrategyAdapter : public IStrategy {
public:
    WilliamsRsiStrategyAdapter() = default;
    ~WilliamsRsiStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        // Use ConfigResolver to get config path (supports custom override)
        std::string default_path = "config/williams_rsi_config.json";
        std::string config_path = ConfigResolver::get_config_path("wr", default_path);
        
        try {
            if (std::filesystem::exists(config_path)) {
                WilliamsRSIConfig wr_config = WilliamsRSIConfig::from_file(config_path);
                strategy_ = std::make_unique<WilliamsRSIStrategy>(wr_config);
                utils::log_info("WilliamsRSIStrategy initialized from " + config_path);
            } else {
                // Use default configuration
                strategy_ = std::make_unique<WilliamsRSIStrategy>();
                utils::log_info("WilliamsRSIStrategy initialized with default configuration");
            }
        } catch (const std::exception& e) {
            utils::log_error("Failed to load config from " + config_path + ": " + e.what());
            utils::log_info("Falling back to default configuration");
            strategy_ = std::make_unique<WilliamsRSIStrategy>();
        }
        
        return true;
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("WilliamsRSIStrategy not initialized");
        }

        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());

        for (size_t i = 0; i < market_data.size(); ++i) {
            SignalOutput signal = strategy_->generate_signal(market_data[i], static_cast<int>(i));
            signals.push_back(signal);
        }

        return signals;
    }

    std::string get_strategy_name() const override {
        return "williams_rsi";
    }

    std::string get_strategy_version() const override {
        return "1.0";
    }

    bool requires_warmup() const override {
        return true;
    }

    int get_warmup_bars() const override {
        return 30;
    }

    bool validate() const override {
        return strategy_ != nullptr;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"strategy_type", "williams_rsi"},
            {"version", "1.0"},
            {"indicators", "Williams %R, RSI"},
            {"signal_logic", "Simple cross detection"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<WilliamsRSIStrategy> strategy_;
};

/**
 * @brief Adapter for Deterministic Test Strategy
 */
class DeterministicTestStrategyAdapter : public IStrategy {
public:
    DeterministicTestStrategyAdapter() = default;
    ~DeterministicTestStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        // Load configuration from JSON file or use defaults
        strategy::DeterministicTestStrategy::Config test_config;
        
        // Use default config file
        std::string config_file = "config/deterministic_test_config.json";
        
        // Parse mode from config if available
        if (std::filesystem::exists(config_file)) {
            try {
                std::ifstream file(config_file);
                nlohmann::json j;
                file >> j;
                
                // Parse mode
                if (j.contains("mode")) {
                    std::string mode_str = j["mode"];
                    if (mode_str == "perfect") {
                        test_config.mode = strategy::DeterministicTestStrategy::PERFECT_PREDICTION;
                    } else if (mode_str == "known_accuracy") {
                        test_config.mode = strategy::DeterministicTestStrategy::KNOWN_ACCURACY;
                    } else if (mode_str == "alternating") {
                        test_config.mode = strategy::DeterministicTestStrategy::ALTERNATING;
                    } else if (mode_str == "threshold") {
                        test_config.mode = strategy::DeterministicTestStrategy::THRESHOLD_BASED;
                    } else if (mode_str == "all_neutral") {
                        test_config.mode = strategy::DeterministicTestStrategy::ALL_NEUTRAL;
                    }
                }
                
                if (j.contains("target_accuracy")) test_config.target_accuracy = j["target_accuracy"];
                if (j.contains("long_threshold")) test_config.long_threshold = j["long_threshold"];
                if (j.contains("short_threshold")) test_config.short_threshold = j["short_threshold"];
                if (j.contains("signal_probability")) test_config.signal_probability = j["signal_probability"];
                if (j.contains("seed")) test_config.seed = j["seed"];
                
                utils::log_info("DeterministicTestStrategy loaded from " + config_file);
            } catch (const std::exception& e) {
                utils::log_warning("Failed to load deterministic test config: " + std::string(e.what()));
                utils::log_info("Using default deterministic test configuration");
            }
        } else {
            utils::log_info("Using default deterministic test configuration (perfect prediction)");
        }
        
        strategy_ = std::make_unique<strategy::DeterministicTestStrategy>(test_config);
        return true;
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("DeterministicTestStrategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->generate_signal(market_data[i], i);
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override {
        return "deterministic_test";
    }

    std::string get_strategy_version() const override {
        return "1.0.0";
    }

    bool requires_warmup() const override {
        return false;  // No warmup needed for test strategy
    }

    int get_warmup_bars() const override {
        return 0;
    }

    bool validate() const override {
        return strategy_ != nullptr;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"strategy_type", "deterministic_test"},
            {"version", "1.0.0"},
            {"purpose", "Validation with known outcomes"},
            {"reproducible", "true"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<strategy::DeterministicTestStrategy> strategy_;
};

/**
 * @brief Adapter for Cheat Strategy (looks ahead for validation)
 */
class CheatStrategyAdapter : public IStrategy {
public:
    CheatStrategyAdapter() = default;
    ~CheatStrategyAdapter() override = default;

    bool initialize(const StrategyComponent::StrategyConfig& config) override {
        strategy::CheatStrategy::Config cheat_config;
        cheat_config.target_accuracy = 0.60;  // 60% accuracy
        cheat_config.signal_probability = 0.75;
        cheat_config.seed = 42;
        cheat_config.lookback_bars = 1;
        
        strategy_ = std::make_unique<strategy::CheatStrategy>(cheat_config);
        utils::log_info("CheatStrategy initialized with 60% target accuracy");
        return true;
    }

    std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) override {
        if (!strategy_) {
            throw std::runtime_error("CheatStrategy not initialized");
        }
        
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        // Process each bar - CheatStrategy will look ahead in market_data
        for (size_t i = 0; i < market_data.size(); ++i) {
            auto signal = strategy_->generate_signal(market_data[i], i, market_data);
            signals.push_back(signal);
        }
        
        return signals;
    }

    std::string get_strategy_name() const override {
        return "cheat";
    }

    std::string get_strategy_version() const override {
        return "1.0.0";
    }

    bool requires_warmup() const override {
        return false;  // No warmup needed, we cheat!
    }

    int get_warmup_bars() const override {
        return 0;
    }

    bool validate() const override {
        return strategy_ != nullptr;
    }

    std::map<std::string, std::string> get_metadata() const override {
        return {
            {"strategy_type", "cheat"},
            {"version", "1.0.0"},
            {"purpose", "Validation by looking ahead at future prices"},
            {"target_accuracy", "60%"},
            {"reproducible", "true"}
        };
    }
    
    void reset() override {
        if (strategy_) {
            strategy_->reset();
        }
    }

private:
    std::unique_ptr<strategy::CheatStrategy> strategy_;
};

} // namespace sentio
