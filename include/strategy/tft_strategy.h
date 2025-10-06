#pragma once

#include "strategy_component.h"
#include "common/utils.h"
#include <vector>
#include <string>
#include <memory>
#include <deque>

namespace sentio {

/**
 * @brief TFT (Temporal Fusion Transformer) strategy configuration
 */
struct TFTConfig {
    std::string model_path = "artifacts/TFT/tft_signal.pt";
    std::string target_symbol = "QQQ";
    int warmup_bars = 100;  // TFT needs more history for temporal patterns
    bool normalize_features = true;
    
    // TFT specific parameters
    int prediction_length = 5;    // Multi-step ahead prediction
    int context_length = 50;      // Historical context window
    int hidden_size = 128;        // Hidden dimension
    int num_attention_heads = 8;  // Multi-head attention
    int num_layers = 6;           // Transformer layers
    double dropout = 0.1;         // Dropout rate
    
    // Time series specific features
    bool include_time_features = true;
    bool include_volatility = true;
    bool include_volume_profile = true;
    bool multi_horizon = true;    // Predict multiple time steps
};

/**
 * @brief TFT strategy for advanced time series forecasting
 * 
 * This strategy uses Temporal Fusion Transformer, a state-of-the-art
 * architecture designed specifically for time series forecasting with
 * interpretability and multi-horizon predictions.
 */
class TFTStrategy : public StrategyComponent {
public:
    explicit TFTStrategy(const StrategyConfig& config, const std::string& symbol = "QQQ");
    virtual ~TFTStrategy() = default;

    /**
     * @brief Initialize the TFT strategy
     */
    bool initialize();

    /**
     * @brief Check if strategy is ready for inference
     */
    bool is_ready() const;


    /**
     * @brief Set TFT-specific configuration
     */
    void set_tft_config(const TFTConfig& config) { tft_config_ = config; }

    /**
     * @brief Public interface for signal generation (for adapter)
     */
    
    /**
     * @brief Reset strategy state (for walk-forward validation)
     */
    void reset() {
        market_history_.clear();
        initialized_ = false;
    }

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;

private:
    TFTConfig tft_config_;
    std::string symbol_;
    std::deque<Bar> market_history_;
    bool initialized_ = false;
    
    // TFT-specific state
    std::vector<std::vector<double>> temporal_features_;
    std::vector<double> attention_weights_;
    std::vector<double> multi_horizon_predictions_;

    /**
     * @brief Extract temporal features for TFT
     */
    std::vector<std::vector<double>> extract_temporal_features(
        const std::deque<Bar>& history, int sequence_length);

    /**
     * @brief Run TFT inference via Python subprocess
     */
    std::vector<double> run_tft_inference(const std::vector<std::vector<double>>& temporal_features);

    /**
     * @brief Convert multi-horizon predictions to single trading signal
     */
    double aggregate_multi_horizon_predictions(const std::vector<double>& predictions);

    /**
     * @brief Validate model file exists
     */
    bool validate_model_file() const;

    // Time series feature calculation helpers
    double calculate_volatility(const std::deque<Bar>& history, int period) const;
    double calculate_volume_profile(const std::deque<Bar>& history, int period) const;
    double calculate_price_acceleration(const std::deque<Bar>& history, int period) const;
    double calculate_trend_strength(const std::deque<Bar>& history, int period) const;
    double calculate_mean_reversion_indicator(const std::deque<Bar>& history, int period) const;
    
    // Time-based features
    double get_time_of_day_feature(int64_t timestamp_ms) const;
    double get_day_of_week_feature(int64_t timestamp_ms) const;
    double get_market_session_feature(int64_t timestamp_ms) const;
};

} // namespace sentio
