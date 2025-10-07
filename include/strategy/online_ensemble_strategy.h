#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Volatility filter (prevent trading in flat markets)
        bool enable_volatility_filter = false;  // Enable ATR-based volatility filter
        int atr_period = 20;                    // ATR calculation period
        double min_atr_ratio = 0.001;           // Minimum ATR as % of price (0.1%)

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::shared_ptr<const std::vector<double>> features;  // Shared, immutable
        double entry_price;
        bool is_long;
    };

    struct PendingUpdate {
        std::array<HorizonPrediction, 3> horizons;  // Fixed size for 3 horizons
        uint8_t count = 0;  // Track actual count (1-3)
    };

    std::map<int, PendingUpdate> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Private methods
    std::vector<double> extract_features(const Bar& current_bar);
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Volatility filter
    double calculate_atr(int period) const;
    bool has_sufficient_volatility() const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio
