#include "strategy/online_strategy_base.h"
#include "common/utils.h"
#include <filesystem>
#include <cmath>

namespace sentio {

OnlineStrategyBase::OnlineStrategyBase(std::unique_ptr<XGBFeatureSet> featureSet,
                                       const OnlineConfig& config)
    : feature_set_(std::move(featureSet)), online_config_(config) {
    strategy_name_ = "online_" + feature_set_->name();
    
    // Create multi-horizon predictor
    predictor_ = std::make_unique<learning::MultiHorizonPredictor>(feature_set_->featureCount());
    
    if (online_config_.use_ensemble) {
        for (int horizon : online_config_.ensemble_horizons) {
            predictor_->add_horizon(horizon, 1.0 / online_config_.ensemble_horizons.size());
        }
    } else {
        predictor_->add_horizon(online_config_.prediction_horizon, 1.0);
    }
    
    utils::log_info("OnlineStrategy initialized with " + feature_set_->name() + 
                   " feature set, " + std::to_string(feature_set_->featureCount()) + " features");
}

OnlineStrategyBase::~OnlineStrategyBase() = default;

bool OnlineStrategyBase::initialize(const StrategyComponent::StrategyConfig& config) {
    strategy_config_ = config;
    
    // Create state directory
    std::string state_dir = online_config_.state_dir + feature_set_->name();
    std::filesystem::create_directories(state_dir);
    
    // Try to load saved state for continuous learning
    if (load_state()) {
        utils::log_info("Loaded previous state - continuing learning from checkpoint");
    } else {
        utils::log_info("No saved state found - starting fresh");
    }
    
    initialized_ = true;
    utils::log_info("OnlineStrategy ready for real-time learning and prediction");
    return true;
}

std::vector<SignalOutput> OnlineStrategyBase::process_data(const std::vector<Bar>& market_data) {
    if (!initialized_) {
        throw std::runtime_error("OnlineStrategy not initialized");
    }
    
    std::vector<SignalOutput> signals;
    signals.reserve(market_data.size());
    
    std::vector<float> feat_float;
    feat_float.resize(feature_set_->featureCount());
    
    int non_neutral_count = 0;
    
    for (size_t i = 0; i < market_data.size(); ++i) {
        const auto& bar = market_data[i];
        feature_set_->update(bar);
        
        // Default neutral signal
        SignalOutput signal;
        signal.bar_id = bar.bar_id;
        signal.timestamp_ms = bar.timestamp_ms;
        signal.bar_index = i;
        signal.symbol = bar.symbol;
        signal.probability = 0.5;
        signal.signal_type = SignalType::NEUTRAL;
        signal.strategy_name = get_strategy_name();
        signal.strategy_version = get_strategy_version();
        signal.prediction_horizon = online_config_.prediction_horizon;
        signal.target_bar_id = bar.bar_id + online_config_.prediction_horizon;
        signal.requires_hold = true;
        signal.signal_generation_interval = online_config_.signal_interval;
        signal.metadata["minimum_hold_bars"] = "3";
        
        // Update with realized returns from past predictions
        update_with_realized_returns(bar, i);
        
        // Generate signal at intervals
        if (feature_set_->isReady() && i % online_config_.signal_interval == 0) {
            // Extract features
            feature_set_->extract(feat_float);
            std::vector<double> features(feat_float.begin(), feat_float.end());
            
            // Store for future update
            FeatureSnapshot snapshot;
            snapshot.features = features;
            snapshot.bar_id = bar.bar_id;
            snapshot.bar_index = i;
            snapshot.close_price = bar.close;
            feature_history_.push_back(snapshot);
            
            // Limit history size
            while (feature_history_.size() > 1000) {
                feature_history_.pop_front();
            }
            
            // Get prediction
            auto prediction = predictor_->predict(features);
            
            if (prediction.is_ready) {
                signal = create_signal(bar, i, prediction);
                
                if (signal.signal_type != SignalType::NEUTRAL) {
                    non_neutral_count++;
                }
                
                // Debug logging
                if (i < 20 || i % 100 == 0) {
                    utils::log_debug("Bar " + std::to_string(i) + 
                                   ": pred=" + std::to_string(prediction.predicted_return) +
                                   ", conf=" + std::to_string(prediction.confidence) +
                                   ", signal=" + (signal.signal_type == SignalType::LONG ? "LONG" :
                                                  signal.signal_type == SignalType::SHORT ? "SHORT" : "NEUTRAL"));
                }
            }
        }
        
        signals.push_back(signal);
        
        // Save state periodically
        if (online_config_.save_state_periodically && 
            i > 0 && i % online_config_.save_interval_bars == 0) {
            save_state();
        }
    }
    
    utils::log_info("OnlineStrategy processed " + std::to_string(signals.size()) + 
                   " bars, generated " + std::to_string(non_neutral_count) + 
                   " actionable signals (" + 
                   std::to_string(100.0 * non_neutral_count / signals.size()) + "%)");
    
    // Final state save
    save_state();
    
    return signals;
}

void OnlineStrategyBase::update_with_realized_returns(const Bar& current_bar, int current_index) {
    // Check each horizon for realized returns
    for (int horizon : online_config_.ensemble_horizons) {
        // Find the feature snapshot from 'horizon' bars ago
        for (const auto& snapshot : feature_history_) {
            if (snapshot.bar_index == current_index - horizon) {
                // Calculate realized return
                double realized_return = (current_bar.close - snapshot.close_price) / 
                                       snapshot.close_price;
                
                // Update predictor
                predictor_->update(horizon, snapshot.features, realized_return);
                
                // Debug logging
                if (current_index % 100 == 0) {
                    utils::log_debug("Updated " + std::to_string(horizon) + 
                                   "-bar predictor with return=" + 
                                   std::to_string(realized_return));
                }
                break;
            }
        }
    }
}

SignalOutput OnlineStrategyBase::create_signal(const Bar& bar, int bar_index,
                                              const learning::OnlinePredictor::PredictionResult& prediction) {
    SignalOutput signal;
    signal.bar_id = bar.bar_id;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.symbol = bar.symbol;
    signal.strategy_name = get_strategy_name();
    signal.strategy_version = get_strategy_version();
    signal.prediction_horizon = online_config_.prediction_horizon;
    signal.target_bar_id = bar.bar_id + online_config_.prediction_horizon;
    signal.requires_hold = true;
    signal.signal_generation_interval = online_config_.signal_interval;
    signal.metadata["minimum_hold_bars"] = "3";
    
    // Convert prediction to probability (0-1 range)
    // Use aggressive scaling like XGB for PSM compatibility
    double scaling = 50.0;
    signal.probability = 0.5 + 0.5 * std::tanh(prediction.predicted_return * scaling);
    
    // Determine signal type with confidence weighting
    if (prediction.confidence < online_config_.confidence_threshold) {
        signal.signal_type = SignalType::NEUTRAL;
    } else if (std::abs(prediction.predicted_return) < online_config_.signal_threshold) {
        signal.signal_type = SignalType::NEUTRAL;
    } else if (prediction.predicted_return > 0) {
        signal.signal_type = SignalType::LONG;
    } else {
        signal.signal_type = SignalType::SHORT;
    }
    
    // Add metadata
    signal.metadata["predicted_return"] = std::to_string(prediction.predicted_return);
    signal.metadata["confidence"] = std::to_string(prediction.confidence);
    signal.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    signal.metadata["learning_type"] = "online_ewrls";
    
    return signal;
}

std::map<int, SignalOutput> OnlineStrategyBase::create_ensemble_signals(
    const Bar& bar, int bar_index, const std::vector<double>& features) {
    
    std::map<int, SignalOutput> ensemble_signals;
    
    // Get prediction for each horizon
    for (int horizon : online_config_.ensemble_horizons) {
        auto prediction = predictor_->predict(features);
        
        if (prediction.is_ready) {
            SignalOutput signal = create_signal(bar, bar_index, prediction);
            signal.prediction_horizon = horizon;
            signal.target_bar_id = bar.bar_id + horizon;
            ensemble_signals[horizon] = signal;
        }
    }
    
    return ensemble_signals;
}

std::map<std::string, std::string> OnlineStrategyBase::get_metadata() const {
    auto metadata = strategy_config_.metadata;
    metadata["feature_set"] = feature_set_->name();
    metadata["prediction_horizon"] = std::to_string(online_config_.prediction_horizon);
    metadata["learning_type"] = "online_ewrls";
    metadata["accuracy"] = std::to_string(get_current_accuracy());
    
    if (online_config_.use_ensemble) {
        std::string horizons;
        for (int h : online_config_.ensemble_horizons) {
            if (!horizons.empty()) horizons += ",";
            horizons += std::to_string(h);
        }
        metadata["ensemble_horizons"] = horizons;
    }
    
    return metadata;
}

void OnlineStrategyBase::reset() {
    feature_set_->reset();
    feature_history_.clear();
    
    // Recreate predictor
    predictor_ = std::make_unique<learning::MultiHorizonPredictor>(feature_set_->featureCount());
    if (online_config_.use_ensemble) {
        for (int horizon : online_config_.ensemble_horizons) {
            predictor_->add_horizon(horizon, 1.0 / online_config_.ensemble_horizons.size());
        }
    } else {
        predictor_->add_horizon(online_config_.prediction_horizon, 1.0);
    }
    
    initialized_ = false;
}

void OnlineStrategyBase::save_state() const {
    std::string state_dir = online_config_.state_dir + feature_set_->name();
    std::filesystem::create_directories(state_dir);
    
    std::string state_path = state_dir + "/predictor_state.bin";
    
    // Note: MultiHorizonPredictor doesn't have save_state yet
    // This would need to be implemented to save all predictors
    utils::log_debug("State save requested (not yet implemented for MultiHorizonPredictor)");
}

bool OnlineStrategyBase::load_state() {
    std::string state_dir = online_config_.state_dir + feature_set_->name();
    std::string state_path = state_dir + "/predictor_state.bin";
    
    if (!std::filesystem::exists(state_path)) {
        return false;
    }
    
    // Note: MultiHorizonPredictor doesn't have load_state yet
    // This would need to be implemented to load all predictors
    utils::log_debug("State load requested (not yet implemented for MultiHorizonPredictor)");
    return false;
}

double OnlineStrategyBase::get_current_accuracy() const {
    // Get accuracy from first predictor (they should be similar)
    // This is a placeholder - would need proper implementation
    return 0.5;  // Default to random until we track this properly
}

} // namespace sentio
