#pragma once

#include "strategy_component.h"
#include "common/utils.h"
#include <vector>
#include <string>
#include <memory>
#include <deque>

namespace sentio {

/**
 * @brief CatBoost strategy configuration
 */
struct CatBoostConfig {
    std::string model_path = "artifacts/CatBoost/catboost_signal.cbm";
    std::string target_symbol = "QQQ";
    int warmup_bars = 50;  // Need 50 bars for feature extraction
    bool normalize_features = true;
    
    // Feature extraction parameters
    double price_scale = 1.0;
    double volume_scale = 1e6;
    int sma_fast = 5;
    int sma_slow = 20;
    int rsi_period = 14;
    int atr_period = 14;
    
    // Adaptive thresholds for expert-recommended signal generation
    struct Thresholds {
        double momentum_min = 0.001;     // Minimum momentum to consider
        double momentum_scale = 100.0;   // Momentum scaling factor
        double return_min = 0.0002;      // Minimum return to consider
        double return_scale_up = 50.0;   // Upside return scaling
        double return_scale_down = 60.0; // Downside return scaling (asymmetric)
        double volume_low = 0.8;         // Low volume threshold
        double volume_high = 1.2;        // High volume threshold
        double volatility_low = 0.005;   // Low volatility threshold
        double volatility_high = 0.015;  // High volatility threshold
        double ma_divergence_min = 0.001; // Minimum MA divergence to consider
        double ma_divergence_scale = 200.0; // MA divergence scaling factor
    } thresholds;
    
    // Signal weights for transparency and fine-tuning (based on GBM success)
    struct Weights {
        double momentum_primary = 0.35;     // Primary momentum weight
        double momentum_secondary = 0.25;   // Secondary momentum weight
        double returns = 0.20;              // Return signal weight
        double ma_divergence = 0.10;        // Moving average divergence weight
        double volume_impact = 0.15;        // Volume confirmation impact
        double volatility_impact = 0.05;    // Volatility adjustment impact
    } weights;
};

/**
 * @brief CatBoost strategy using Python subprocess for inference
 * 
 * This strategy uses the trained CatBoost model via Python subprocess
 * to generate trading signals. It extracts 17 features from market data
 * and calls the Python inference script.
 */
class CatBoostStrategy : public StrategyComponent {
public:
    explicit CatBoostStrategy(const StrategyConfig& config, const std::string& symbol = "QQQ");
    virtual ~CatBoostStrategy() = default;

    /**
     * @brief Initialize the CatBoost strategy
     */
    bool initialize();

    /**
     * @brief Check if strategy is ready for inference
     */
    bool is_ready() const;


    /**
     * @brief Set CatBoost-specific configuration
     */
    void set_catboost_config(const CatBoostConfig& config) { catboost_config_ = config; }

    /**
     * @brief Public interface for signal generation (for adapter)
     */

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;

private:
    CatBoostConfig catboost_config_;
    std::string symbol_;
    std::deque<Bar> market_history_;
    bool initialized_ = false;

    /**
     * @brief Extract features from current bar and history
     */
    std::vector<double> extract_features(const Bar& current_bar, const std::deque<Bar>& history);

    /**
     * @brief Run CatBoost inference via Python subprocess
     */
    double run_catboost_inference(const std::vector<double>& features);

    /**
     * @brief Validate model file exists
     */
    bool validate_model_file() const;

    // Feature calculation helpers
    double calculate_sma(const std::deque<Bar>& history, int period) const;
    double calculate_std(const std::deque<Bar>& history, int period) const;
    double calculate_momentum(const std::deque<Bar>& history, int period) const;
    double calculate_volume_ratio(const Bar& bar, const std::deque<Bar>& history) const;
    
    // Signal health monitoring and performance optimization
    struct SignalMonitor {
        struct Stats {
            double sum = 0.0;
            double sum_sq = 0.0;
            double min_val = 1.0;
            double max_val = 0.0;
            int neutral_count = 0;
            int total_count = 0;
            
            double mean() const { 
                return total_count > 0 ? sum / total_count : 0.5; 
            }
            
            double std_dev() const {
                if (total_count <= 1) return 0.0;
                double variance = (sum_sq / total_count) - (mean() * mean());
                return std::sqrt(std::max(0.0, variance));
            }
            
            double neutral_ratio() const {
                return total_count > 0 ? 
                    static_cast<double>(neutral_count) / total_count : 0.0;
            }
            
            bool is_healthy() const {
                return (std_dev() > 0.05) &&           // Sufficient diversity
                       (neutral_ratio() < 0.60) &&      // Not too many neutrals
                       (max_val - min_val > 0.20) &&   // Good range
                       (total_count > 50);             // Enough samples
            }
        };
        
        mutable Stats stats_;
        mutable int unhealthy_streak_ = 0;
        
        void update(double signal) const {
            stats_.sum += signal;
            stats_.sum_sq += signal * signal;
            stats_.min_val = std::min(stats_.min_val, signal);
            stats_.max_val = std::max(stats_.max_val, signal);
            stats_.total_count++;
            
            if (std::abs(signal - 0.5) < 0.01) {
                stats_.neutral_count++;
            }
            
            // Health check every 100 signals
            if (stats_.total_count % 100 == 0) {
                if (!stats_.is_healthy()) {
                    unhealthy_streak_++;
                    if (unhealthy_streak_ >= 3) {
                        utils::log_error(
                            "CatBoost health check failed! "
                            "Mean: " + std::to_string(stats_.mean()) + 
                            ", StdDev: " + std::to_string(stats_.std_dev()) +
                            ", NeutralRatio: " + std::to_string(stats_.neutral_ratio())
                        );
                    }
                } else {
                    unhealthy_streak_ = 0;
                }
            }
        }
        
        void reset() const {
            stats_ = Stats();
            unhealthy_streak_ = 0;
        }
    };
    
    mutable SignalMonitor monitor_;
};

} // namespace sentio
