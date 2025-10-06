#pragma once

#include "strategy_component.h"
#include "common/types.h"
#include <memory>
#include <vector>
#include <deque>
#include <array>
#include <cmath>

// Forward declarations for ML libraries
#ifdef XGBOOST_AVAILABLE
extern "C" {
    typedef void* BoosterHandle;
    typedef void* DMatrixHandle;
}
#endif

#ifdef LIGHTGBM_AVAILABLE
extern "C" {
    typedef void* LGBM_BoosterHandle;
}
#endif

namespace sentio {
namespace intraday {

/**
 * @brief Intraday feature extractor with 25 optimized features
 */
class IntradayFeatureExtractor {
public:
    static constexpr size_t FEATURE_COUNT = 25;
    
    struct Features {
        // Microstructure (6)
        double tick_direction = 0.0;
        double spread_ratio = 0.0;
        double wicks_ratio = 0.0;
        double volume_imbalance = 0.0;
        double trade_intensity = 0.0;
        double price_acceleration = 0.0;
        
        // Short-term momentum (5)
        double micro_momentum_1 = 0.0;
        double micro_momentum_3 = 0.0;
        double rsi_3 = 0.0;
        double ema_cross_fast = 0.0;
        double velocity = 0.0;
        
        // Multi-timeframe (5)
        double mtf_trend_5m = 0.0;
        double mtf_trend_15m = 0.0;
        double mtf_volume_5m = 0.0;
        double mtf_volatility_ratio = 0.0;
        double session_range_position = 0.0;
        
        // Session patterns (4)
        double vwap_distance = 0.0;
        double opening_range_breakout = 0.0;
        double time_of_day_sin = 0.0;
        double time_of_day_cos = 0.0;
        
        // Volume profile (5)
        double volume_percentile = 0.0;
        double delta_cumulative = 0.0;
        double vwap_stdev_bands = 0.0;
        double relative_volume = 0.0;
        double volume_price_trend = 0.0;
        
        std::array<float, FEATURE_COUNT> to_array() const {
            return {
                static_cast<float>(tick_direction),
                static_cast<float>(spread_ratio),
                static_cast<float>(wicks_ratio),
                static_cast<float>(volume_imbalance),
                static_cast<float>(trade_intensity),
                static_cast<float>(price_acceleration),
                static_cast<float>(micro_momentum_1),
                static_cast<float>(micro_momentum_3),
                static_cast<float>(rsi_3),
                static_cast<float>(ema_cross_fast),
                static_cast<float>(velocity),
                static_cast<float>(mtf_trend_5m),
                static_cast<float>(mtf_trend_15m),
                static_cast<float>(mtf_volume_5m),
                static_cast<float>(mtf_volatility_ratio),
                static_cast<float>(session_range_position),
                static_cast<float>(vwap_distance),
                static_cast<float>(opening_range_breakout),
                static_cast<float>(time_of_day_sin),
                static_cast<float>(time_of_day_cos),
                static_cast<float>(volume_percentile),
                static_cast<float>(delta_cumulative),
                static_cast<float>(vwap_stdev_bands),
                static_cast<float>(relative_volume),
                static_cast<float>(volume_price_trend)
            };
        }
    };
    
    IntradayFeatureExtractor();
    ~IntradayFeatureExtractor() = default;
    
    void update(const Bar& bar);
    Features extract_features() const;
    bool is_ready() const { return bars_1m_.size() >= min_bars_required_; }
    void reset();
    
private:
    static constexpr size_t min_bars_required_ = 100;
    static constexpr size_t max_history_size_ = 500;
    
    // Market data storage
    std::deque<Bar> bars_1m_;
    std::deque<Bar> bars_5m_;
    std::deque<Bar> bars_15m_;
    
    // Session tracking
    struct SessionData {
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double vwap = 0.0;
        double vwap_variance = 0.0;
        double cumulative_pv = 0.0;
        double cumulative_volume = 0.0;
        double cumulative_delta = 0.0;
        int64_t session_start_ms = 0;
        
        void reset() {
            *this = SessionData{};
        }
    } session_;
    
    // Helper methods
    double calculate_tick_direction() const;
    double calculate_wicks_ratio(const Bar& bar) const;
    double calculate_volume_imbalance() const;
    double calculate_rsi(int period) const;
    double calculate_ema_cross(int fast, int slow) const;
    double calculate_mtf_trend(const std::deque<Bar>& bars, int period) const;
    double calculate_vwap_distance(const Bar& bar) const;
    double calculate_opening_range_breakout(const Bar& bar) const;
    std::pair<double, double> calculate_time_features(int64_t timestamp_ms) const;
    double calculate_volume_percentile(double volume) const;
    void update_session_data(const Bar& bar);
    void aggregate_to_higher_timeframe(const Bar& bar);
};

/**
 * @brief XGBoost strategy with proper model inference
 */
class IntradayXGBoostStrategy : public StrategyComponent {
public:
    struct Config {
        std::string model_path = "artifacts/Intraday/xgboost_intraday.json";
        double confidence_threshold = 0.05;  // Lower for intraday
        double position_size_multiplier = 1.0;
        bool enable_risk_management = true;
        double max_drawdown_percent = 5.0;  // 5% max drawdown
        double stop_loss_percent = 0.5;     // 0.5% stop loss
        double take_profit_percent = 1.0;   // 1% take profit
    };
    
    IntradayXGBoostStrategy(const StrategyConfig& base_config,
                           const Config& xgb_config,
                           const std::string& symbol);
    ~IntradayXGBoostStrategy();
    
    bool initialize();
    void cleanup();
    void reset();
    
protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;
    
private:
    Config config_;
    std::string symbol_;
    std::unique_ptr<IntradayFeatureExtractor> feature_extractor_;
    
#ifdef XGBOOST_AVAILABLE
    BoosterHandle booster_ = nullptr;
    DMatrixHandle dmatrix_ = nullptr;
#endif
    
    bool initialized_ = false;
    std::array<float, IntradayFeatureExtractor::FEATURE_COUNT> feature_buffer_;
    
    // Risk management
    struct RiskManager {
        double current_drawdown = 0.0;
        double peak_equity = 0.0;
        double current_position = 0.0;
        int consecutive_losses = 0;
        
        bool should_trade() const {
            return current_drawdown < 5.0 && consecutive_losses < 3;
        }
        
        void update_equity(double equity) {
            peak_equity = std::max(peak_equity, equity);
            current_drawdown = (peak_equity - equity) / peak_equity * 100.0;
        }
    } risk_;
    
    bool load_model();
    double run_inference(const IntradayFeatureExtractor::Features& features);
    SignalOutput create_signal(double probability, const Bar& bar, int bar_index);
};

// LightGBM removed - redundant with XGBoost (both use same gradient boosting approach)

} // namespace intraday
} // namespace sentio

