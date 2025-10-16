# Production Readiness Code Review - Source Analysis

**Generated**: 2025-10-08 00:47:40
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (14 files)
**Description**: Comprehensive code review with all related source modules for OnlineTrader live trading system

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [include/backend/ensemble_position_state_machine.h](#file-1)
2. [include/backend/position_state_machine.h](#file-2)
3. [include/features/unified_feature_engine.h](#file-3)
4. [include/learning/online_predictor.h](#file-4)
5. [include/strategy/online_ensemble_strategy.h](#file-5)
6. [megadocs/PRODUCTION_READINESS_CODE_REVIEW.md](#file-6)
7. [src/backend/ensemble_position_state_machine.cpp](#file-7)
8. [src/backend/position_state_machine.cpp](#file-8)
9. [src/cli/live_trade_command.cpp](#file-9)
10. [src/features/unified_feature_engine.cpp](#file-10)
11. [src/learning/online_predictor.cpp](#file-11)
12. [src/live/alpaca_client.cpp](#file-12)
13. [src/live/polygon_websocket.cpp](#file-13)
14. [src/strategy/online_ensemble_strategy.cpp](#file-14)

---

## 📄 **FILE 1 of 14**: include/backend/ensemble_position_state_machine.h

**File Information**:
- **Path**: `include/backend/ensemble_position_state_machine.h`

- **Size**: 122 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <memory>

namespace sentio {

/**
 * Enhanced PSM that handles multiple prediction horizons simultaneously
 * Manages overlapping positions from different time horizons
 */
class EnsemblePositionStateMachine : public PositionStateMachine {
public:
    struct EnsembleSignal {
        std::vector<SignalOutput> horizon_signals;  // Signals from each horizon
        std::vector<double> horizon_weights;        // Weight for each horizon
        std::vector<int> horizon_bars;              // Horizon length (1, 5, 10)
        
        // Aggregated results
        double weighted_probability;
        double signal_agreement;  // How much horizons agree (0-1)
        sentio::SignalType consensus_signal;  // Use global SignalType from signal_output.h
        double confidence;
        
        EnsembleSignal()
            : weighted_probability(0.5),
              signal_agreement(0.0),
              consensus_signal(sentio::SignalType::NEUTRAL),
              confidence(0.0) {}
    };
    
    struct HorizonPosition {
        std::string symbol;
        int horizon_bars;
        uint64_t entry_bar_id;
        uint64_t exit_bar_id;     // Expected exit
        double entry_price;
        double predicted_return;
        double position_weight;    // Fraction of capital for this horizon
        sentio::SignalType signal_type;  // Use global SignalType from signal_output.h
        bool is_active;
        
        HorizonPosition()
            : horizon_bars(0),
              entry_bar_id(0),
              exit_bar_id(0),
              entry_price(0.0),
              predicted_return(0.0),
              position_weight(0.0),
              signal_type(sentio::SignalType::NEUTRAL),
              is_active(true) {}
    };
    
    struct EnsembleTransition : public StateTransition {
        // Enhanced transition with multi-horizon awareness
        std::vector<HorizonPosition> horizon_positions;
        double total_position_size;  // Sum across all horizons
        std::map<int, double> horizon_allocations;  // horizon -> % allocation
        bool has_consensus;  // Do horizons agree?
        int dominant_horizon;    // Which horizon has strongest signal
        
        EnsembleTransition()
            : total_position_size(0.0),
              has_consensus(false),
              dominant_horizon(0) {}
    };

    EnsemblePositionStateMachine();
    
    // Main ensemble interface
    EnsembleTransition get_ensemble_transition(
        const PortfolioState& current_portfolio,
        const EnsembleSignal& ensemble_signal,
        const MarketState& market_conditions,
        uint64_t current_bar_id
    );
    
    // Aggregate multiple horizon signals into consensus
    EnsembleSignal aggregate_signals(
        const std::map<int, SignalOutput>& horizon_signals,
        const std::map<int, double>& horizon_weights
    );
    
    // Position management for multiple horizons
    void update_horizon_positions(uint64_t current_bar_id, double current_price);
    std::vector<HorizonPosition> get_active_positions() const;
    std::vector<HorizonPosition> get_closeable_positions(uint64_t current_bar_id) const;
    
    // Dynamic allocation based on signal agreement
    std::map<int, double> calculate_horizon_allocations(const EnsembleSignal& signal);
    
    // Risk management with overlapping positions
    double calculate_ensemble_risk(const std::vector<HorizonPosition>& positions) const;
    double get_maximum_position_size() const;
    
private:
    // Track positions by horizon
    std::map<int, std::vector<HorizonPosition>> positions_by_horizon_;
    
    // Performance tracking by horizon
    std::map<int, double> horizon_accuracy_;
    std::map<int, double> horizon_pnl_;
    std::map<int, int> horizon_trade_count_;
    
    // Ensemble configuration
    static constexpr double BASE_ALLOCATION = 0.3;  // Base allocation per horizon
    static constexpr double CONSENSUS_BONUS = 0.4;  // Extra allocation for agreement
    static constexpr double MIN_AGREEMENT = 0.6;    // Minimum agreement to trade
    
    // Helper methods
    sentio::SignalType determine_consensus(const std::vector<SignalOutput>& signals,
                                          const std::vector<double>& weights) const;
    double calculate_agreement(const std::vector<SignalOutput>& signals) const;
    bool should_override_hold(const EnsembleSignal& signal, uint64_t current_bar_id) const;
    void update_horizon_performance(int horizon, double pnl);
};

} // namespace sentio

```

## 📄 **FILE 2 of 14**: include/backend/position_state_machine.h

**File Information**:
- **Path**: `include/backend/position_state_machine.h`

- **Size**: 139 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

struct MarketState;

class PositionStateMachine {
public:
    enum class State {
        CASH_ONLY,      
        QQQ_ONLY,       
        TQQQ_ONLY,      
        PSQ_ONLY,       
        SQQQ_ONLY,      
        QQQ_TQQQ,       
        PSQ_SQQQ,       
        INVALID         
    };

    enum class SignalType {
        STRONG_BUY,     
        WEAK_BUY,       
        WEAK_SELL,      
        STRONG_SELL,    
        NEUTRAL         
    };

    struct StateTransition {
        State current_state;
        SignalType signal_type;
        State target_state;
        std::string optimal_action;
        std::string theoretical_basis;
        double expected_return = 0.0;      
        double risk_score = 0.0;           
        double confidence = 0.0;           
        
        // NEW: Multi-bar prediction support
        int prediction_horizon = 1;           // How many bars ahead this predicts
        uint64_t position_open_bar_id = 0;    // When position was opened
        uint64_t earliest_exit_bar_id = 0;    // When position can be closed
        bool is_hold_enforced = false;        // Currently in minimum hold period
        int bars_held = 0;                    // How many bars position has been held
        int bars_remaining = 0;               // Bars until hold period ends
    };
    
    struct PositionTracking {
        uint64_t open_bar_id;
        int horizon;
        double entry_price;
        std::string symbol;
    };

    PositionStateMachine();

    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    );

    std::pair<double, double> get_state_aware_thresholds(
        double base_buy_threshold,
        double base_sell_threshold,
        State current_state
    ) const;

    bool validate_transition(
        const StateTransition& transition,
        const PortfolioState& current_portfolio,
        double available_capital
    ) const;
    
    // NEW: Multi-bar support methods
    bool can_close_position(uint64_t current_bar_id, const std::string& symbol) const;
    void record_position_entry(const std::string& symbol, uint64_t bar_id, 
                              int horizon, double entry_price);
    void record_position_exit(const std::string& symbol);
    void clear_position_tracking();
    int get_bars_held(const std::string& symbol, uint64_t current_bar_id) const;
    int get_bars_remaining(const std::string& symbol, uint64_t current_bar_id) const;
    bool is_in_hold_period(const PortfolioState& portfolio, uint64_t current_bar_id) const;

    static std::string state_to_string(State s);
    static std::string signal_type_to_string(SignalType st);
    
    State determine_current_state(const PortfolioState& portfolio) const;

protected:
    StateTransition get_base_transition(State current, SignalType signal) const;

private:
    SignalType classify_signal(
        const SignalOutput& signal,
        double buy_threshold,
        double sell_threshold,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    ) const;
    
    void initialize_transition_matrix();
    double apply_state_risk_adjustment(State state, double base_value) const;
    double calculate_kelly_position_size(
        double signal_probability,
        double expected_return,
        double risk_estimate,
        double available_capital
    ) const;
    
    void update_position_tracking(const SignalOutput& signal, 
                                 const StateTransition& transition);

    std::map<std::pair<State, SignalType>, StateTransition> transition_matrix_;
    
    // NEW: Multi-bar position tracking
    std::map<std::string, PositionTracking> position_tracking_;

    static constexpr double DEFAULT_BUY_THRESHOLD = 0.55;
    static constexpr double DEFAULT_SELL_THRESHOLD = 0.45;
    static constexpr double CONFIDENCE_THRESHOLD = 0.7;
    static constexpr double STRONG_MARGIN = 0.15;
    static constexpr double MAX_LEVERAGE_EXPOSURE = 0.8;
    static constexpr double MAX_POSITION_SIZE = 0.6;
    static constexpr double MIN_CASH_BUFFER = 0.1;
};

using PSM = PositionStateMachine;

} // namespace sentio
```

## 📄 **FILE 3 of 14**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`

- **Size**: 254 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <string>

namespace sentio {
namespace features {

/**
 * @brief Configuration for Unified Feature Engine
 */
struct UnifiedFeatureEngineConfig {
    // Feature categories
    bool enable_time_features = true;
    bool enable_price_action = true;
    bool enable_moving_averages = true;
    bool enable_momentum = true;
    bool enable_volatility = true;
    bool enable_volume = true;
    bool enable_statistical = true;
    bool enable_pattern_detection = true;
    bool enable_price_lags = true;
    bool enable_return_lags = true;
    
    // Normalization
    bool normalize_features = true;
    bool use_robust_scaling = false;  // Use median/IQR instead of mean/std
    
    // Performance optimization
    bool enable_caching = true;
    bool enable_incremental_updates = true;
    int max_history_size = 1000;
    
    // Feature dimensions (matching Kochi analysis)
    int time_features = 8;
    int price_action_features = 12;
    int moving_average_features = 16;
    int momentum_features = 20;
    int volatility_features = 15;
    int volume_features = 12;
    int statistical_features = 10;
    int pattern_features = 8;
    int price_lag_features = 10;
    int return_lag_features = 15;
    
    // Total: Variable based on enabled features  
    int total_features() const {
        int total = 0;
        if (enable_time_features) total += time_features;
        if (enable_price_action) total += price_action_features;
        if (enable_moving_averages) total += moving_average_features;
        if (enable_momentum) total += momentum_features;
        if (enable_volatility) total += volatility_features;
        if (enable_volume) total += volume_features;
        if (enable_statistical) total += statistical_features;
        if (enable_pattern_detection) total += pattern_features;
        if (enable_price_lags) total += price_lag_features;
        if (enable_return_lags) total += return_lag_features;
        return total;
    }
};

/**
 * @brief Incremental calculator for O(1) moving averages
 */
class IncrementalSMA {
public:
    explicit IncrementalSMA(int period);
    double update(double value);
    double get_value() const { return sum_ / std::min(period_, static_cast<int>(values_.size())); }
    bool is_ready() const { return static_cast<int>(values_.size()) >= period_; }

private:
    int period_;
    std::deque<double> values_;
    double sum_ = 0.0;
};

/**
 * @brief Incremental calculator for O(1) EMA
 */
class IncrementalEMA {
public:
    IncrementalEMA(int period, double alpha = -1.0);
    double update(double value);
    double get_value() const { return ema_value_; }
    bool is_ready() const { return initialized_; }

private:
    double alpha_;
    double ema_value_ = 0.0;
    bool initialized_ = false;
};

/**
 * @brief Incremental calculator for O(1) RSI
 */
class IncrementalRSI {
public:
    explicit IncrementalRSI(int period);
    double update(double price);
    double get_value() const;
    bool is_ready() const { return gain_sma_.is_ready() && loss_sma_.is_ready(); }

private:
    double prev_price_ = 0.0;
    bool first_update_ = true;
    IncrementalSMA gain_sma_;
    IncrementalSMA loss_sma_;
};

/**
 * @brief Unified Feature Engine implementing Kochi's 126-feature set
 * 
 * This engine provides a comprehensive set of technical indicators optimized
 * for machine learning applications. It implements all features identified
 * in the Kochi analysis with proper normalization and O(1) incremental updates.
 * 
 * Feature Categories:
 * 1. Time Features (8): Cyclical encoding of time components
 * 2. Price Action (12): OHLC patterns, gaps, shadows
 * 3. Moving Averages (16): SMA/EMA ratios at multiple periods
 * 4. Momentum (20): RSI, MACD, Stochastic, Williams %R
 * 5. Volatility (15): ATR, Bollinger Bands, Keltner Channels
 * 6. Volume (12): VWAP, OBV, A/D Line, Volume ratios
 * 7. Statistical (10): Correlation, regression, distribution metrics
 * 8. Patterns (8): Candlestick pattern detection
 * 9. Price Lags (10): Historical price references
 * 10. Return Lags (15): Historical return references
 */
class UnifiedFeatureEngine {
public:
    using Config = UnifiedFeatureEngineConfig;
    
    explicit UnifiedFeatureEngine(const Config& config = Config{});
    ~UnifiedFeatureEngine() = default;

    /**
     * @brief Update engine with new bar data
     * @param bar New OHLCV bar
     */
    void update(const Bar& bar);

    /**
     * @brief Get current feature vector
     * @return Vector of 126 normalized features
     */
    std::vector<double> get_features() const;

    /**
     * @brief Get specific feature category
     */
    std::vector<double> get_time_features() const;
    std::vector<double> get_price_action_features() const;
    std::vector<double> get_moving_average_features() const;
    std::vector<double> get_momentum_features() const;
    std::vector<double> get_volatility_features() const;
    std::vector<double> get_volume_features() const;
    std::vector<double> get_statistical_features() const;
    std::vector<double> get_pattern_features() const;
    std::vector<double> get_price_lag_features() const;
    std::vector<double> get_return_lag_features() const;

    /**
     * @brief Get feature names for debugging/analysis
     */
    std::vector<std::string> get_feature_names() const;

    /**
     * @brief Reset engine state
     */
    void reset();

    /**
     * @brief Check if engine has enough data for all features
     */
    bool is_ready() const;

    /**
     * @brief Get number of bars processed
     */
    size_t get_bar_count() const { return bar_history_.size(); }

private:
    Config config_;
    
    // Data storage
    std::deque<Bar> bar_history_;
    std::deque<double> returns_;
    std::deque<double> log_returns_;
    
    // Incremental calculators
    std::unordered_map<std::string, std::unique_ptr<IncrementalSMA>> sma_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalEMA>> ema_calculators_;
    std::unordered_map<std::string, std::unique_ptr<IncrementalRSI>> rsi_calculators_;
    
    // Cached features
    mutable std::vector<double> cached_features_;
    mutable bool cache_valid_ = false;
    
    // Normalization parameters
    struct NormalizationParams {
        double mean = 0.0;
        double std = 1.0;
        double median = 0.0;
        double iqr = 1.0;
        std::deque<double> history;
        bool initialized = false;
    };
    mutable std::unordered_map<std::string, NormalizationParams> norm_params_;
    
    // Private methods
    void initialize_calculators();
    void update_returns(const Bar& bar);
    void update_calculators(const Bar& bar);
    void invalidate_cache();
    
    // Feature calculation methods
    std::vector<double> calculate_time_features(const Bar& bar) const;
    std::vector<double> calculate_price_action_features() const;
    std::vector<double> calculate_moving_average_features() const;
    std::vector<double> calculate_momentum_features() const;
    std::vector<double> calculate_volatility_features() const;
    std::vector<double> calculate_volume_features() const;
    std::vector<double> calculate_statistical_features() const;
    std::vector<double> calculate_pattern_features() const;
    std::vector<double> calculate_price_lag_features() const;
    std::vector<double> calculate_return_lag_features() const;
    
    // Utility methods
    double normalize_feature(const std::string& name, double value) const;
    void update_normalization_params(const std::string& name, double value) const;
    double safe_divide(double numerator, double denominator, double fallback = 0.0) const;
    double calculate_atr(int period) const;
    double calculate_true_range(size_t index) const;
    
    // Pattern detection helpers
    bool is_doji(const Bar& bar) const;
    bool is_hammer(const Bar& bar) const;
    bool is_shooting_star(const Bar& bar) const;
    bool is_engulfing_bullish(size_t index) const;
    bool is_engulfing_bearish(size_t index) const;
    
    // Statistical helpers
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y, int period) const;
    double calculate_linear_regression_slope(const std::vector<double>& values, int period) const;
};

} // namespace features
} // namespace sentio
```

## 📄 **FILE 4 of 14**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`

- **Size**: 133 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <deque>
#include <memory>
#include <cmath>

namespace sentio {
namespace learning {

/**
 * Online learning predictor that eliminates train/inference parity issues
 * Uses Exponentially Weighted Recursive Least Squares (EWRLS)
 */
class OnlinePredictor {
public:
    struct Config {
        double lambda;
        double initial_variance;
        double regularization;
        int warmup_samples;
        bool adaptive_learning;
        double min_lambda;
        double max_lambda;
        
        Config()
            : lambda(0.995),
              initial_variance(100.0),
              regularization(0.01),
              warmup_samples(100),
              adaptive_learning(true),
              min_lambda(0.990),
              max_lambda(0.999) {}
    };
    
    struct PredictionResult {
        double predicted_return;
        double confidence;
        double volatility_estimate;
        bool is_ready;
        
        PredictionResult()
            : predicted_return(0.0),
              confidence(0.0),
              volatility_estimate(0.0),
              is_ready(false) {}
    };
    
    explicit OnlinePredictor(size_t num_features, const Config& config = Config());
    
    // Main interface - predict and optionally update
    PredictionResult predict(const std::vector<double>& features);
    void update(const std::vector<double>& features, double actual_return);
    
    // Combined predict-then-update for efficiency
    PredictionResult predict_and_update(const std::vector<double>& features, 
                                        double actual_return);
    
    // Adaptive learning rate based on recent volatility
    void adapt_learning_rate(double market_volatility);
    
    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);
    
    // Diagnostics
    double get_recent_rmse() const;
    double get_directional_accuracy() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }
    
private:
    Config config_;
    size_t num_features_;
    int samples_seen_;
    
    // EWRLS parameters
    Eigen::VectorXd theta_;      // Model parameters
    Eigen::MatrixXd P_;          // Covariance matrix
    double current_lambda_;      // Adaptive forgetting factor
    
    // Performance tracking
    std::deque<double> recent_errors_;
    std::deque<bool> recent_directions_;
    static constexpr size_t HISTORY_SIZE = 100;
    
    // Volatility estimation for adaptive learning
    std::deque<double> recent_returns_;
    double estimate_volatility() const;
    
    // Numerical stability
    void ensure_positive_definite();
    static constexpr double EPSILON = 1e-8;
};

/**
 * Ensemble of online predictors for different time horizons
 */
class MultiHorizonPredictor {
public:
    struct HorizonConfig {
        int horizon_bars;
        double weight;
        OnlinePredictor::Config predictor_config;
        
        HorizonConfig()
            : horizon_bars(1),
              weight(1.0),
              predictor_config() {}
    };
    
    explicit MultiHorizonPredictor(size_t num_features);
    
    // Add predictors for different horizons
    void add_horizon(int bars, double weight = 1.0);
    
    // Ensemble prediction
    OnlinePredictor::PredictionResult predict(const std::vector<double>& features);
    
    // Update all predictors
    void update(int bars_ago, const std::vector<double>& features, double actual_return);
    
private:
    size_t num_features_;
    std::vector<std::unique_ptr<OnlinePredictor>> predictors_;
    std::vector<HorizonConfig> configs_;
};

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 5 of 14**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 191 lines
- **Modified**: 2025-10-08 00:12:00

- **Type**: .h

```text
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

    // Predictor training (for warmup)
    void train_predictor(const std::vector<double>& features, double realized_return);
    std::vector<double> extract_features(const Bar& current_bar);

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

```

## 📄 **FILE 6 of 14**: megadocs/PRODUCTION_READINESS_CODE_REVIEW.md

**File Information**:
- **Path**: `megadocs/PRODUCTION_READINESS_CODE_REVIEW.md`

- **Size**: 1867 lines
- **Modified**: 2025-10-08 00:32:26

- **Type**: .md

```text
# OnlineTrader Production Readiness Code Review

**Date**: 2025-10-08
**Version**: v1.1 (Post Live Trading Fix)
**Status**: COMPREHENSIVE ANALYSIS
**Purpose**: Ensure system is production-ready for real capital deployment

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Issues (P0)](#critical-issues-p0)
3. [High Priority Issues (P1)](#high-priority-issues-p1)
4. [Learning Continuity Analysis](#learning-continuity-analysis)
5. [Performance Optimization Opportunities](#performance-optimization-opportunities)
6. [MRB Improvement Opportunities](#mrb-improvement-opportunities)
7. [Complete Source Code Modules](#complete-source-code-modules)
8. [Recommended Fix Priority](#recommended-fix-priority)
9. [Testing & Validation Requirements](#testing-validation-requirements)

---

## Executive Summary

### Current System Status

**✅ Working Components**:
- Live trading connection (Alpaca Paper Trading)
- Real-time data feed (Polygon IEX WebSocket)
- Feature extraction (126 features)
- Predictor training (860 warmup samples)
- Signal generation (varying probabilities)
- Order placement and tracking

**❌ Critical Deficiencies Found**:
- **6 Critical (P0)**: Could cause financial loss or system crash
- **6 High Priority (P1)**: Degrade performance or learning effectiveness
- **7 Medium Priority (P2)**: Technical debt and edge cases
- **4 Low Priority (P3)**: Code quality improvements

### Overall Verdict

**🚫 NOT READY FOR REAL CAPITAL** until P0 and P1 issues are resolved.

**Estimated Time to Production-Ready**: 40-60 hours of focused development:
- P0 fixes: 16-20 hours
- P1 fixes: 16-20 hours
- Testing & validation: 8-20 hours

---

## Critical Issues (P0)

### P0-1: No Exception Handling in Main Trading Loop

**File**: `src/cli/live_trade_command.cpp:330-378`
**Severity**: CRITICAL
**Impact**: Single exception crashes entire trading session

**Current Code**:
```cpp
void on_new_bar(const Bar& bar) {
    auto timestamp = get_timestamp_readable();

    if (is_end_of_day_liquidation_time()) {
        liquidate_all_positions();
        return;
    }

    if (!is_regular_hours()) {
        return;
    }

    strategy_.on_bar(bar);  // Could throw
    auto signal = generate_signal(bar);  // Could throw
    auto decision = make_decision(signal, bar);  // Could throw

    if (decision.should_trade) {
        execute_transition(decision);  // Could throw
    }
}
```

**Problem**: Any exception in:
- Feature extraction
- Predictor inference
- Decision logic
- Order placement
- API calls

Will crash the entire process. No graceful degradation, no error logging with context.

**Recommended Fix**:
```cpp
void on_new_bar(const Bar& bar) {
    try {
        auto timestamp = get_timestamp_readable();

        if (is_end_of_day_liquidation_time()) {
            liquidate_all_positions();
            return;
        }

        if (!is_regular_hours()) {
            // NEW: Still learn from after-hours data
            strategy_.on_bar(bar);
            return;
        }

        // Main trading logic
        strategy_.on_bar(bar);
        auto signal = generate_signal(bar);
        auto decision = make_decision(signal, bar);

        if (decision.should_trade) {
            execute_transition(decision);
        }

        log_portfolio_state();

    } catch (const std::exception& e) {
        log_error("EXCEPTION in on_new_bar: " + std::string(e.what()));
        log_error("Bar context: timestamp=" + std::to_string(bar.timestamp_ms) +
                 ", close=" + std::to_string(bar.close) +
                 ", volume=" + std::to_string(bar.volume));

        // Enter safe mode - no new trades until cleared
        is_in_error_state_ = true;
        error_count_++;

        // If multiple consecutive errors, stop trading
        if (error_count_ > 3) {
            log_error("Multiple consecutive errors - STOPPING TRADING");
            // Close all positions and halt
            liquidate_all_positions();
            should_continue_trading_ = false;
        }

    } catch (...) {
        log_error("UNKNOWN EXCEPTION in on_new_bar - STOPPING");
        is_in_error_state_ = true;
        liquidate_all_positions();
        should_continue_trading_ = false;
    }
}
```

**Required New Members**:
```cpp
// In LiveTrader class
bool is_in_error_state_ = false;
int error_count_ = 0;
bool should_continue_trading_ = true;
```

**Test Plan**:
1. Inject API failure → verify graceful handling
2. Inject invalid bar data → verify error recovery
3. Inject 4 consecutive errors → verify emergency stop
4. Verify logs contain full context

---

### P0-2: Hardcoded $100,000 Capital Assumption

**Files Affected**:
- `src/strategy/online_ensemble_strategy.cpp:146`
- `src/backend/position_state_machine.cpp:389`
- `src/cli/live_trade_command.cpp:65,718-719`

**Severity**: CRITICAL
**Impact**: All financial calculations wrong for non-$100k accounts

**Current Code Locations**:
```cpp
// 1. Return calculation (online_ensemble_strategy.cpp:146)
double return_pct = realized_pnl / 100000.0;  // Assuming $100k base

// 2. Risk management (position_state_machine.cpp:389)
if (available_capital < MIN_CASH_BUFFER * 100000) {
    // ...
}

// 3. Constructor (live_trade_command.cpp:65)
entry_equity_(100000.0)

// 4. Performance tracking (live_trade_command.cpp:718-719)
j["total_return"] = account->portfolio_value - 100000.0;
j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;
```

**Problem**: If account has $50k:
- Return % is 2x too low (10% shows as 5%)
- Risk checks use wrong threshold ($5k buffer instead of $2.5k)
- Performance tracking shows wrong P&L

If account has $200k:
- Return % is 2x too high (5% shows as 10%)
- Risk checks are too conservative
- Could under-utilize capital

**Recommended Fix**:

**Step 1**: Add `initial_equity_` member, initialize from real account:
```cpp
// In LiveTrader constructor
LiveTrader(...)
    : alpaca_(alpaca_key, alpaca_secret, true)
    , polygon_(polygon_url, polygon_key)
    , log_dir_(log_dir)
    , strategy_(create_v1_config())
    , psm_()
    , current_state_(PositionStateMachine::State::CASH_ONLY)
    , bars_held_(0)
{
    open_log_files();

    // Get actual starting capital
    auto account = alpaca_.get_account();
    if (!account) {
        throw std::runtime_error("Failed to get account info during initialization");
    }

    initial_equity_ = account->portfolio_value;
    entry_equity_ = initial_equity_;

    log_system("Starting equity: $" + std::to_string(initial_equity_));

    warmup_strategy();
}
```

**Step 2**: Pass to strategy for return calculations:
```cpp
// In OnlineEnsembleStrategy
void update(const Bar& bar, double realized_pnl, double base_equity) {
    if (std::abs(realized_pnl) > 1e-6) {
        double return_pct = realized_pnl / base_equity;  // Use actual equity
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }
    process_pending_updates(bar);
}
```

**Step 3**: Fix risk management:
```cpp
// In position_state_machine.cpp
if (available_capital < MIN_CASH_BUFFER * current_portfolio_value) {
    // Use current portfolio value, not hardcoded
}
```

**Step 4**: Fix performance tracking:
```cpp
j["total_return"] = account->portfolio_value - initial_equity_;
j["total_return_pct"] = (account->portfolio_value - initial_equity_) / initial_equity_;
```

**Test Plan**:
1. Test with $50k account → verify correct return %
2. Test with $200k account → verify correct return %
3. Test with $100k account → verify unchanged behavior
4. Verify risk thresholds scale correctly

---

### P0-3: No Order Fill Validation

**File**: `src/cli/live_trade_command.cpp:571-588`
**Severity**: CRITICAL
**Impact**: State machine assumes filled but positions don't exist

**Current Code**:
```cpp
for (const auto& [symbol, quantity] : quantities) {
    log_system("  Buying " + std::to_string(quantity) + " shares of " + symbol);

    auto order = alpaca_.place_market_order(symbol, quantity, "gtc");
    if (order) {
        log_system("  ✓ Order placed: " + order->order_id + " (" + order->status + ")");
        log_trade(*order);
    } else {
        log_error("  ✗ Failed to place order for " + symbol);
    }
}

// Continues without checking if orders filled!
log_system("✓ Transition complete");
current_state_ = target_state;  // Updates state assuming success
bars_held_ = 0;
entry_equity_ = account->portfolio_value;
```

**Problem**: Order could be:
- `pending_new` → not yet accepted
- `pending_replace` → being modified
- `rejected` → never executed
- `canceled` → cancelled by broker
- `partially_filled` → only partial position

System updates `current_state_` to `BASE_BULL_3X` but might have zero shares!

**Example Failure Scenario**:
```
1. Signal: LONG (p=0.68) → transition to BASE_BULL_3X
2. Place order for 100 shares SPY
3. Order status: "pending_new"
4. Update current_state_ = BASE_BULL_3X
5. Actual position: 0 shares (order not filled yet)
6. Next signal: NEUTRAL (p=0.50) → hold position
7. System thinks it has 100 shares, actually has 0
8. Missing out on trades because "holding phantom position"
```

**Recommended Fix**:
```cpp
struct OrderResult {
    std::string symbol;
    double requested_qty;
    double filled_qty;
    std::string status;
    std::string order_id;
    bool success;
};

std::vector<OrderResult> place_orders_and_wait(
    const std::map<std::string, double>& quantities) {

    std::vector<OrderResult> results;

    for (const auto& [symbol, quantity] : quantities) {
        OrderResult result;
        result.symbol = symbol;
        result.requested_qty = quantity;
        result.filled_qty = 0;
        result.success = false;

        log_system("  Placing order: " + std::to_string(quantity) + " shares of " + symbol);

        auto order = alpaca_.place_market_order(symbol, quantity, "ioc");  // immediate-or-cancel
        if (!order) {
            log_error("  ✗ Failed to place order for " + symbol);
            result.status = "failed_to_place";
            results.push_back(result);
            continue;
        }

        result.order_id = order->order_id;
        result.status = order->status;

        // Wait for fill (with timeout)
        int retries = 0;
        while (retries < 20 && order->status != "filled" && order->status != "rejected") {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            order = alpaca_.get_order(order->order_id);
            retries++;
        }

        result.status = order->status;
        result.filled_qty = order->filled_qty;
        result.success = (order->status == "filled" && order->filled_qty > 0);

        if (result.success) {
            log_system("  ✓ Order filled: " + order->order_id +
                      " (" + std::to_string(order->filled_qty) + " shares)");
        } else {
            log_error("  ✗ Order not filled: " + order->order_id +
                     " (status=" + order->status + ")");
        }

        results.push_back(result);
    }

    return results;
}

// In execute_transition:
auto results = place_orders_and_wait(quantities);

// Verify all orders succeeded
bool all_filled = std::all_of(results.begin(), results.end(),
    [](const OrderResult& r) { return r.success; });

if (!all_filled) {
    log_error("Not all orders filled - reconciling positions");

    // Get actual positions from broker
    auto actual_positions = alpaca_.get_positions();
    current_state_ = determine_state_from_positions(actual_positions);

    log_error("Actual state after partial fill: " + state_to_string(current_state_));
    return;
}

// Only update state if ALL orders filled
log_system("✓ Transition complete - all orders filled");
current_state_ = target_state;
bars_held_ = 0;
entry_equity_ = account->portfolio_value;
```

**Test Plan**:
1. Mock rejected order → verify state not updated
2. Mock partial fill → verify reconciliation
3. Mock slow fill (10+ seconds) → verify timeout handling
4. Verify actual positions match expected state

---

### P0-4: Silent Failure on Position Close

**File**: `src/cli/live_trade_command.cpp:542-545`
**Severity**: CRITICAL
**Impact**: Incorrect state leads to double positions or failed exits

**Current Code**:
```cpp
if (!alpaca_.close_all_positions()) {
    log_error("Failed to close positions - aborting transition");
    return;  // Leaves system in inconsistent state
}
```

**Problem**: If `close_all_positions()` fails:
- API timeout
- Network error
- Rejected close orders
- Partial closes (some closed, some not)

The function returns, but:
- `current_state_` still shows old position (e.g., `BASE_BULL_3X`)
- `bars_held_` continues incrementing
- `entry_equity_` is stale
- Next transition tries to add to non-closed position

**Example Failure Scenario**:
```
1. State: BASE_BULL_3X (holding 100 shares SPY)
2. Signal: NEUTRAL → transition to CASH_ONLY
3. Try to close 100 shares SPY
4. API timeout → close_all_positions() returns false
5. Abort transition, current_state_ = BASE_BULL_3X (but did some shares close?)
6. Actual position: 50 shares SPY (partial close before timeout)
7. System thinks: 100 shares
8. Next signal: LONG → try to add 100 more shares
9. Result: 150 shares instead of 100 (over-leveraged)
```

**Recommended Fix**:
```cpp
bool close_result = alpaca_.close_all_positions();

if (!close_result) {
    log_error("Failed to close positions - entering RECONCILIATION mode");

    // Force reconciliation with broker
    auto actual_positions = alpaca_.get_positions();

    if (actual_positions.empty()) {
        log_system("Reconciliation: No positions found - close succeeded despite error");
        current_state_ = PositionStateMachine::State::CASH_ONLY;
        bars_held_ = 0;
        return;
    }

    // Log what positions still exist
    for (const auto& pos : actual_positions) {
        log_error("Reconciliation: Still holding " +
                 std::to_string(pos.quantity) + " shares of " + pos.symbol);
    }

    // Determine actual state from remaining positions
    current_state_ = determine_state_from_positions(actual_positions);

    log_error("Reconciliation complete: actual state = " +
             state_to_string(current_state_));

    // Set error flag - don't allow new transitions until cleared
    is_in_error_state_ = true;

    // Try to close remaining positions individually
    for (const auto& pos : actual_positions) {
        log_system("Attempting individual close: " + pos.symbol);
        alpaca_.close_position(pos.symbol);
    }

    return;
}

// Only proceed if close succeeded
log_system("✓ All positions closed");
auto account = alpaca_.get_account();
// ... continue with transition
```

**Add Helper Function**:
```cpp
PositionStateMachine::State determine_state_from_positions(
    const std::vector<Position>& positions) const {

    if (positions.empty()) {
        return PositionStateMachine::State::CASH_ONLY;
    }

    // Count position types
    int spy_qty = 0, spxl_qty = 0, sh_qty = 0, sds_qty = 0;

    for (const auto& pos : positions) {
        if (pos.symbol == "SPY") spy_qty = pos.quantity;
        else if (pos.symbol == "SPXL") spxl_qty = pos.quantity;
        else if (pos.symbol == "SH") sh_qty = pos.quantity;
        else if (pos.symbol == "SDS") sds_qty = pos.quantity;
    }

    // Determine state from holdings
    if (spxl_qty > 0 && spy_qty == 0) return PositionStateMachine::State::BASE_BULL_3X;
    if (spy_qty > 0 && spxl_qty == 0) return PositionStateMachine::State::BASE_BULL_1X;
    if (sds_qty > 0 && sh_qty == 0) return PositionStateMachine::State::BASE_BEAR_2X;
    if (sh_qty > 0 && sds_qty == 0) return PositionStateMachine::State::BASE_BEAR_1X;

    // Mixed positions - log warning
    log_error("Unexpected position mix: SPY=" + std::to_string(spy_qty) +
             " SPXL=" + std::to_string(spxl_qty) +
             " SH=" + std::to_string(sh_qty) +
             " SDS=" + std::to_string(sds_qty));

    return PositionStateMachine::State::CASH_ONLY;  // Default to safe state
}
```

**Test Plan**:
1. Mock close_all failure → verify reconciliation
2. Mock partial close → verify state determination
3. Mock API timeout → verify retry logic
4. Verify no trades execute while in error state

---

### P0-5: No Continuous Learning During Live Trading

**File**: `src/cli/live_trade_command.cpp:330-378`
**Severity**: CRITICAL
**Impact**: Model stagnates, doesn't adapt to current market

**Current Code**:
```cpp
void on_new_bar(const Bar& bar) {
    // ...
    strategy_.on_bar(bar);  // Only updates features, not predictor
    auto signal = generate_signal(bar);
    // ...
    // NO PREDICTOR TRAINING HERE
}
```

**Problem**: During live trading, predictor only learns via `process_pending_updates()` which triggers when horizons complete (every 1, 5, or 10 bars). Between updates:
- Model doesn't see new data
- Doesn't adapt to sudden regime changes
- Stagnates between horizon boundaries

**Warmup trains on every bar** (line 319), but **live trading doesn't**.

**Recommended Fix**:
```cpp
// Add member to track previous bar
std::optional<Bar> previous_bar_;

void on_new_bar(const Bar& bar) {
    try {
        auto timestamp = get_timestamp_readable();

        if (is_end_of_day_liquidation_time()) {
            liquidate_all_positions();
            return;
        }

        // Learn from after-hours data
        strategy_.on_bar(bar);

        // NEW: Continuous learning from bar-to-bar returns
        if (previous_bar_.has_value() && strategy_.is_ready()) {
            auto features = strategy_.extract_features(previous_bar_.value());

            if (!features.empty()) {
                // Calculate realized return from previous bar to current
                double prev_close = previous_bar_->close;
                double curr_close = bar.close;
                double realized_return = (curr_close - prev_close) / prev_close;

                // Train predictor on this observation
                strategy_.train_predictor(features, realized_return);

                // Log periodically
                static int train_count = 0;
                train_count++;
                if (train_count % 100 == 0) {
                    log_system("Continuous learning: " + std::to_string(train_count) +
                             " live training samples processed");
                }
            }
        }

        // Store current bar for next iteration
        previous_bar_ = bar;

        // Only trade during regular hours
        if (!is_regular_hours()) {
            log_system("[" + timestamp + "] Outside regular hours - learning only");
            return;
        }

        // Generate signal and trade
        auto signal = generate_signal(bar);
        auto decision = make_decision(signal, bar);

        if (decision.should_trade) {
            execute_transition(decision);
        }

        log_portfolio_state();

    } catch (...) {
        // exception handling from P0-1
    }
}
```

**Expected Impact**:
- Model adapts to intraday regime changes
- Faster response to new market conditions
- More consistent with warmup training methodology
- **Potential MRB improvement: +0.1% to +0.3%** (from faster adaptation)

**Test Plan**:
1. Run live trading for 100 bars → verify 100 training calls
2. Compare signal evolution with vs without continuous learning
3. Measure prediction error over time (should decrease)
4. Verify no performance degradation from extra training

---

### P0-6: Timezone Hardcoded to EDT

**File**: `src/cli/live_trade_command.cpp:196-213`
**Severity**: HIGH (promoted to CRITICAL for live trading)
**Impact**: Trading hours wrong during EST months (Nov-Mar)

**Current Code**:
```cpp
// Convert GMT to ET (EDT = GMT-4)
int et_hour = gmt_hour - 4;
if (et_hour < 0) et_hour += 24;

// Check if within regular trading hours (9:30am - 4:00pm ET)
if (et_hour < 9 || et_hour >= 16) {
    return false;
}
if (et_hour == 9 && gmt_minute < 30) {
    return false;
}
```

**Problem**: EDT (GMT-4) is only valid Apr-Nov. During EST (Nov-Mar), offset is GMT-5. This means:
- During EST, thinks it's 1 hour later than actual
- Would start trading at 8:30am instead of 9:30am
- Would stop trading at 3:00pm instead of 4:00pm

**Example**: On January 15 (EST):
- Actual GMT time: 14:00 (2pm)
- Actual ET time: 09:00 (9am EST)
- Code calculates: 14:00 - 4 = 10:00 (thinks 10am)
- Result: Trading starts 1 hour early

**Recommended Fix**:
```cpp
bool is_regular_hours() const {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);

    // Use localtime which handles DST automatically
    auto local_tm = *std::localtime(&time_t_now);

    // If system is configured for ET timezone, this works directly
    int local_hour = local_tm.tm_hour;
    int local_minute = local_tm.tm_min;

    // If system is NOT in ET timezone, need to convert
    // Option 1: Set TZ environment variable at startup
    // Option 2: Use explicit DST check

    // Check DST status
    bool is_dst = (local_tm.tm_isdst > 0);

    // Convert from GMT to ET
    auto gmt_tm = *std::gmtime(&time_t_now);
    int gmt_hour = gmt_tm.tm_hour;
    int gmt_minute = gmt_tm.tm_min;

    int et_offset = is_dst ? -4 : -5;  // EDT=-4, EST=-5
    int et_hour = (gmt_hour + et_offset + 24) % 24;

    // Regular trading hours: 9:30am - 4:00pm ET
    if (et_hour < 9 || et_hour >= 16) {
        return false;
    }
    if (et_hour == 9 && gmt_minute < 30) {
        return false;
    }

    return true;
}
```

**Better Solution**: Use environment variable:
```cpp
// At program startup (in main or constructor)
setenv("TZ", "America/New_York", 1);  // Auto-handles DST
tzset();

bool is_regular_hours() const {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t_now);  // Now in ET

    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    // 9:30am - 4:00pm ET
    if (hour < 9 || hour >= 16) return false;
    if (hour == 9 && minute < 30) return false;

    return true;
}
```

**Test Plan**:
1. Test in July (EDT) → verify 9:30am-4pm
2. Test in January (EST) → verify 9:30am-4pm
3. Test at DST transition dates → verify correct handling
4. Log timezone info at startup for verification

---

## High Priority Issues (P1)

### P1-1: Learning Gap Outside Trading Hours

**File**: `src/cli/live_trade_command.cpp:341-344`
**Severity**: HIGH
**Impact**: Misses ~30% of market data

**Current Code**:
```cpp
if (!is_regular_hours()) {
    log_system("[" + timestamp + "] Outside regular hours - skipping");
    return;  // Completely skips the bar
}
```

**Problem**: Pre-market (4am-9:30am) and after-hours (4pm-8pm) data is discarded. Never calls `strategy_.on_bar()`, so:
- Feature engine doesn't update
- Predictor doesn't learn from these returns
- Gap in historical continuity

After-hours often has significant price movements that the model never learns about.

**Fix**: Already included in P0-5 continuous learning fix. Just need to move `strategy_.on_bar()` before the hours check.

**Expected Impact**: Better handling of overnight gaps, improved adaptation to extended-hours movements.

---

### P1-2: Train/Test Methodology Mismatch

**Files**: `src/cli/live_trade_command.cpp:310-323` (warmup) vs `330-378` (live)
**Severity**: HIGH
**Impact**: Model overfits to 1-bar patterns

**Problem**:
- **Warmup**: Trains on 1-bar returns (bar[i] → bar[i+1])
- **Live**: Uses multi-horizon returns (1-bar, 5-bar, 10-bar)

This creates train/test distribution mismatch. Model learns short-term patterns but is tested on multi-horizon predictions.

**Recommended Fix**: Use consistent multi-horizon training in warmup:
```cpp
// In warmup_strategy()
for (size_t i = start_idx + 64; i < all_bars.size(); ++i) {
    strategy_.on_bar(all_bars[i]);
    auto signal = generate_signal(all_bars[i]);  // This tracks predictions

    // Multi-horizon training (like live trading)
    auto features = strategy_.extract_features(all_bars[i]);
    if (!features.empty()) {
        // Train on multiple horizons
        for (int horizon : {1, 5, 10}) {
            if (i + horizon < all_bars.size()) {
                double h_return = (all_bars[i + horizon].close - all_bars[i].close)
                                / all_bars[i].close;
                strategy_.train_predictor_horizon(features, horizon, h_return);
            }
        }
    }
}
```

**Required**: Add `train_predictor_horizon()` method:
```cpp
void OnlineEnsembleStrategy::train_predictor_horizon(
    const std::vector<double>& features,
    int horizon,
    double realized_return) {

    if (features.empty()) return;
    ensemble_predictor_->update(horizon, features, realized_return);
}
```

**Expected Impact**:
- More consistent predictions between warmup and live
- **Potential MRB improvement: +0.05% to +0.15%** (from better training)

---

### P1-3: Predictor Update Skipped on Numerical Instability

**File**: `src/learning/online_predictor.cpp:87-90`
**Severity**: HIGH
**Impact**: Learning discontinuity during volatile periods

**Current Code**:
```cpp
double denominator = lambda + x.transpose() * Px;

if (std::abs(denominator) < EPSILON) {
    utils::log_warning("Near-zero denominator in EWRLS update, skipping");
    return;  // NO LEARNING FOR THIS BAR
}

Eigen::VectorXd k = Px / denominator;
```

**Problem**: When market is highly volatile or features are unusual, EWRLS can encounter numerical instability (denominator ≈ 0). Current code skips the entire update, discarding that bar's information permanently.

This is precisely when the model needs to adapt most - during unusual market conditions.

**Recommended Fix**: Regularize instead of skipping:
```cpp
double denominator = lambda + x.transpose() * Px;

if (std::abs(denominator) < EPSILON) {
    utils::log_warning("Near-zero denominator in EWRLS update - applying regularization");

    // Add small regularization to denominator
    denominator = std::copysign(EPSILON * 10, denominator);  // Preserve sign

    // Or add diagonal regularization to P
    // P = P + EPSILON * Eigen::MatrixXd::Identity(P.rows(), P.cols());
}

Eigen::VectorXd k = Px / denominator;
// Continue with update
```

**Alternative**: Add adaptive regularization:
```cpp
// If numerical issues occur frequently, increase forgetting factor
if (numerical_issues_count_ > 10) {
    lambda_ = std::min(lambda_ * 1.1, 0.999);  // Increase smoothing
    numerical_issues_count_ = 0;
}
```

**Expected Impact**: More robust learning, fewer "lost" observations during volatile periods.

---

### P1-4: Magic Number Thresholds Not Configurable

**File**: `src/cli/live_trade_command.cpp:477-493`
**Severity**: HIGH
**Impact**: Can't adapt strategy without code changes

**Current Code**:
```cpp
if (signal.probability >= 0.68) {
    target_state = PositionStateMachine::State::TQQQ_ONLY;
} else if (signal.probability >= 0.60) {
    target_state = PositionStateMachine::State::QQQ_TQQQ;
} else if (signal.probability >= 0.55) {
    target_state = PositionStateMachine::State::QQQ_ONLY;
}
// ... etc
```

**Problem**: These thresholds determine:
- Entry/exit points
- Leverage usage (1x, 2x, 3x)
- Risk exposure

Can't adjust for different market regimes without recompiling.

**Recommended Fix**: Move to configuration:
```cpp
struct TradingThresholds {
    double bull_3x = 0.68;      // SPXL entry (3x long)
    double bull_mixed = 0.60;   // SPY+SPXL (mixed)
    double bull_1x = 0.55;      // SPY entry (1x long)
    double neutral_upper = 0.51;
    double neutral_lower = 0.49;
    double bear_1x = 0.45;      // SH entry (1x short)
    double bear_mixed = 0.35;   // SH+SDS (mixed)
    double bear_2x = 0.32;      // SDS entry (2x short)

    // Load from config file
    static TradingThresholds from_config(const std::string& path);

    // Validate thresholds are monotonic
    bool is_valid() const {
        return bull_3x > bull_mixed &&
               bull_mixed > bull_1x &&
               bull_1x > neutral_upper &&
               neutral_upper > neutral_lower &&
               neutral_lower > bear_1x &&
               bear_1x > bear_mixed &&
               bear_mixed > bear_2x;
    }
};

// In LiveTrader class
TradingThresholds thresholds_;

// Use in make_decision:
if (signal.probability >= thresholds_.bull_3x) {
    target_state = PositionStateMachine::State::BASE_BULL_3X;
}
// ... etc
```

**Load from JSON config**:
```json
{
  "thresholds": {
    "bull_3x": 0.68,
    "bull_mixed": 0.60,
    "bull_1x": 0.55,
    "neutral_upper": 0.51,
    "neutral_lower": 0.49,
    "bear_1x": 0.45,
    "bear_mixed": 0.35,
    "bear_2x": 0.32
  }
}
```

**Expected Impact**:
- Can optimize thresholds without recompiling
- Can adjust for different volatility regimes
- **Potential MRB improvement: +0.1% to +0.5%** (from threshold optimization)

---

### P1-5: No Bar Validation

**File**: `src/strategy/online_ensemble_strategy.cpp:155-176`
**Severity**: HIGH
**Impact**: Corrupt data causes learning errors

**Current Code**:
```cpp
void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    feature_engine_->update(bar);
    samples_seen_++;
    // NO VALIDATION
}
```

**Problem**: No checks that:
- Bar timestamps are sequential
- Bar IDs are increasing
- OHLCV values are valid (positive prices, non-negative volume)
- No duplicate bars

If feed delivers bad data, model learns from garbage.

**Recommended Fix**:
```cpp
bool is_valid_bar(const Bar& bar, const Bar* previous) const {
    // Check price validity
    if (bar.close <= 0 || bar.open <= 0 || bar.high <= 0 || bar.low <= 0) {
        utils::log_error("Invalid prices in bar: close=" + std::to_string(bar.close));
        return false;
    }

    // Check volume validity
    if (bar.volume < 0) {
        utils::log_error("Invalid volume: " + std::to_string(bar.volume));
        return false;
    }

    // Check OHLC consistency
    if (bar.high < bar.low || bar.high < bar.open || bar.high < bar.close ||
        bar.low > bar.open || bar.low > bar.close) {
        utils::log_error("Inconsistent OHLC: O=" + std::to_string(bar.open) +
                        " H=" + std::to_string(bar.high) +
                        " L=" + std::to_string(bar.low) +
                        " C=" + std::to_string(bar.close));
        return false;
    }

    // Check sequentiality (if we have a previous bar)
    if (previous) {
        if (bar.bar_id <= previous->bar_id) {
            utils::log_error("Out-of-order or duplicate bar: " +
                           std::to_string(bar.bar_id) + " after " +
                           std::to_string(previous->bar_id));
            return false;
        }

        if (bar.timestamp_ms <= previous->timestamp_ms) {
            utils::log_error("Non-sequential timestamps");
            return false;
        }

        // Sanity check: price change shouldn't be > 50% per bar
        double price_change_pct = std::abs(bar.close - previous->close) / previous->close;
        if (price_change_pct > 0.5) {
            utils::log_error("Suspicious price change: " +
                           std::to_string(price_change_pct * 100) + "%");
            return false;
        }
    }

    return true;
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Validate bar
    const Bar* prev = bar_history_.empty() ? nullptr : &bar_history_.back();

    if (!is_valid_bar(bar, prev)) {
        utils::log_error("Rejecting invalid bar at timestamp " +
                        std::to_string(bar.timestamp_ms));
        return;  // Don't process invalid data
    }

    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    feature_engine_->update(bar);
    samples_seen_++;

    // ... rest of method
}
```

**Expected Impact**: Prevents learning from corrupt data, improves model reliability.

---

### P1-6: Volatility Filter Disabled by Default

**File**: `include/strategy/online_ensemble_strategy.h:77`
**Severity**: MEDIUM (promoted to HIGH for production)
**Impact**: Trading in low-signal environments

**Current Code**:
```cpp
bool enable_volatility_filter = false;  // Disabled
```

**Problem**: Implementation exists (lines 79-85 in cpp) to skip trading when ATR is too low (flat markets), but it's disabled. Trading occurs even when market is range-bound with no exploitable patterns.

**Recommended Fix**: Enable with permissive threshold:
```cpp
bool enable_volatility_filter = true;
double min_atr_ratio = 0.0005;  // 0.05% - very permissive
```

Or make it configurable:
```json
{
  "strategy": {
    "enable_volatility_filter": true,
    "min_atr_ratio": 0.0005,
    "atr_period": 14
  }
}
```

**Expected Impact**:
- Fewer false signals in flat markets
- **Potential MRB improvement: +0.05% to +0.1%** (from avoiding low-signal trades)

---

## Learning Continuity Analysis

### Current Learning Flow

**Warmup Phase** (960 bars):
```
for each historical bar:
    1. strategy_.on_bar(bar)          ✅ Updates features
    2. generate_signal(bar)            ✅ Generates prediction
    3. train_predictor(features, return)  ✅ Learns from return
```
**Result**: 860 training samples (bars 64-959)

**Live Trading Phase**:
```
for each live bar:
    1. strategy_.on_bar(bar)          ✅ Updates features
    2. generate_signal(bar)            ✅ Generates prediction
    3. process_pending_updates(bar)    ⚠️  Only on horizon boundaries
    4. NO immediate training           ❌ Gap between horizons
```
**Result**: ~5-10 training samples per 100 bars (only when horizons complete)

### Learning Gaps Identified

1. **After-Hours Gap**: Bars outside 9:30am-4pm are completely skipped
2. **Intraday Gap**: Only learns when horizons complete, not on every bar
3. **Methodology Gap**: Warmup uses 1-bar returns, live uses multi-horizon
4. **Numerical Instability Gap**: Skips learning when denominator near zero

### Recommended Complete Learning Flow

```cpp
void on_new_bar(const Bar& bar) {
    // 1. Always update features (even after-hours)
    strategy_.on_bar(bar);

    // 2. Continuous learning from bar-to-bar
    if (previous_bar_) {
        auto features = strategy_.extract_features(*previous_bar_);
        if (!features.empty()) {
            double return_1bar = (bar.close - previous_bar_->close) / previous_bar_->close;
            strategy_.train_predictor(features, return_1bar);
        }
    }
    previous_bar_ = bar;

    // 3. Multi-horizon learning (existing)
    // This happens automatically via process_pending_updates()

    // 4. Only trade during regular hours
    if (!is_regular_hours()) {
        return;  // Learn but don't trade
    }

    // 5. Generate signal and trade
    auto signal = generate_signal(bar);
    // ... trading logic
}
```

### Expected Improvements

With continuous learning:
- **Training samples per 100 bars**: 5-10 → 100
- **Adaptation time**: 10-50 bars → 1-5 bars
- **MRB improvement**: +0.1% to +0.3% (from faster adaptation)
- **Sharpe ratio improvement**: +0.05 to +0.1 (from better market regime tracking)

---

## Performance Optimization Opportunities

### Computational Performance

#### Opp-1: Feature Caching

**File**: `src/features/unified_feature_engine.cpp`
**Current**: Recalculates all 126 features on every `get_features()` call
**Impact**: ~1-2ms per bar (negligible for 1-minute bars, but wasteful)

**Optimization**:
```cpp
// Already has cache, just ensure it's used
mutable std::vector<double> cached_features_;
mutable bool cache_valid_ = false;

std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_) {
        return cached_features_;  // Return cached
    }

    // Calculate features
    cached_features_ = calculate_all_features();
    cache_valid_ = true;

    return cached_features_;
}

void UnifiedFeatureEngine::invalidate_cache() {
    cache_valid_ = false;  // Called by update()
}
```

**Expected Impact**: Minimal (already implemented), but ensure it's working correctly.

---

#### Opp-2: Incremental Feature Updates

**Current**: Some features recalculated from scratch using full history
**Opportunity**: Use incremental calculators (SMA, EMA, RSI already incremental)

**Example - Bollinger Bands**:
```cpp
// Current: Recalculates mean and std over 20-bar window
BollingerBands calculate_bollinger_bands() const {
    std::vector<double> closes;
    for (size_t i = size - 20; i < size; i++) {
        closes.push_back(bar_history_[i].close);
    }
    double mean = std::accumulate(closes) / 20.0;
    double std = calculate_std(closes);
    // ...
}

// Optimized: Use incremental mean and variance
class IncrementalBollingerBands {
    IncrementalSMA sma_{20};
    IncrementalVariance variance_{20};

    void update(double value) {
        sma_.update(value);
        variance_.update(value);
    }

    BollingerBands get() const {
        double mean = sma_.get_value();
        double std = std::sqrt(variance_.get_value());
        return {mean - 2*std, mean, mean + 2*std};
    }
};
```

**Expected Impact**:
- Feature calculation time: 1-2ms → 0.1-0.2ms
- Not critical for 1-minute bars, but enables higher-frequency trading

---

#### Opp-3: Parallel Feature Calculation

**Current**: All features calculated sequentially
**Opportunity**: Calculate independent feature groups in parallel

```cpp
std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_) return cached_features_;

    std::vector<double> features;
    features.reserve(126);

    // Calculate feature groups in parallel
    std::future<std::vector<double>> time_features_future =
        std::async(std::launch::async, [this]() {
            return calculate_time_features();
        });

    std::future<std::vector<double>> price_features_future =
        std::async(std::launch::async, [this]() {
            return calculate_price_action_features();
        });

    // ... more groups

    // Collect results
    auto time_features = time_features_future.get();
    auto price_features = price_features_future.get();

    features.insert(features.end(), time_features.begin(), time_features.end());
    features.insert(features.end(), price_features.begin(), price_features.end());

    return features;
}
```

**Expected Impact**:
- With 4 cores: 1-2ms → 0.3-0.5ms
- Not critical for current use case
- **Recommendation**: Defer until moving to higher frequency (< 10s bars)

---

### MRB Improvement Opportunities

#### Opp-4: Adaptive Threshold Calibration

**Current**: Thresholds calibrated every 100 bars
**Opportunity**: Adapt calibration frequency based on market regime

```cpp
void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // ...

    // Adaptive calibration
    if (config_.enable_threshold_calibration) {
        int calibration_interval = detect_market_regime() == Regime::VOLATILE
                                    ? 20   // Calibrate more frequently in volatile markets
                                    : 100; // Less frequently in stable markets

        if (samples_seen_ % calibration_interval == 0 && is_ready()) {
            calibrate_thresholds();
        }
    }
}

enum class Regime { STABLE, VOLATILE, TRENDING };

Regime detect_market_regime() const {
    // Use ATR to detect volatility
    double atr = calculate_atr(14);
    double avg_price = bar_history_.back().close;
    double atr_ratio = atr / avg_price;

    if (atr_ratio > 0.02) return Regime::VOLATILE;    // > 2% daily range
    if (atr_ratio < 0.01) return Regime::STABLE;      // < 1% daily range
    return Regime::TRENDING;
}
```

**Expected Impact**:
- **MRB improvement: +0.05% to +0.15%** (from better threshold adaptation)
- Faster response to regime changes

---

#### Opp-5: Multi-Timeframe Features

**Current**: Only 1-minute features
**Opportunity**: Add features from multiple timeframes (5min, 15min, 1hour)

```cpp
class MultiTimeframeFeatureEngine {
    UnifiedFeatureEngine tf_1min_;   // 1-minute
    UnifiedFeatureEngine tf_5min_;   // 5-minute
    UnifiedFeatureEngine tf_15min_;  // 15-minute
    UnifiedFeatureEngine tf_1hour_;  // 1-hour

    std::vector<double> get_features() {
        auto f1 = tf_1min_.get_features();   // 126 features
        auto f5 = tf_5min_.get_features();   // 126 features
        auto f15 = tf_15min_.get_features(); // 126 features
        auto f1h = tf_1hour_.get_features(); // 126 features

        // Concatenate: 504 total features
        std::vector<double> all_features;
        all_features.reserve(504);
        all_features.insert(all_features.end(), f1.begin(), f1.end());
        all_features.insert(all_features.end(), f5.begin(), f5.end());
        all_features.insert(all_features.end(), f15.begin(), f15.end());
        all_features.insert(all_features.end(), f1h.begin(), f1h.end());

        return all_features;
    }
};
```

**Expected Impact**:
- Captures both short-term and long-term patterns
- Better trend identification
- **MRB improvement: +0.2% to +0.5%** (from multi-scale analysis)
- **Trade-off**: More features = more data needed for training

---

#### Opp-6: Feature Selection / Importance Analysis

**Current**: Uses all 126 features, including ~40 placeholders
**Opportunity**: Identify and remove low-importance features

```cpp
// 1. Implement get_feature_importance() (currently stubbed)
std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    return ensemble_predictor_->get_feature_importance();
}

// 2. Periodically analyze importance
void analyze_feature_importance() {
    auto importance = strategy_.get_feature_importance();
    auto feature_names = feature_engine_->get_feature_names();

    // Sort by importance
    std::vector<std::pair<std::string, double>> ranked;
    for (size_t i = 0; i < importance.size(); ++i) {
        ranked.push_back({feature_names[i], importance[i]});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](auto& a, auto& b) { return a.second > b.second; });

    // Log top 20 features
    log_system("Top 20 Most Important Features:");
    for (int i = 0; i < 20; ++i) {
        log_system("  " + std::to_string(i+1) + ". " +
                  ranked[i].first + " (" +
                  std::to_string(ranked[i].second) + ")");
    }

    // Identify features to remove (importance < threshold)
    std::vector<int> features_to_keep;
    for (size_t i = 0; i < importance.size(); ++i) {
        if (importance[i] > 0.001) {  // Keep if importance > 0.1%
            features_to_keep.push_back(i);
        }
    }
}

// 3. Use feature selection mask
std::vector<double> extract_features_masked(const Bar& bar) {
    auto all_features = feature_engine_->get_features();

    std::vector<double> selected_features;
    for (int idx : feature_selection_mask_) {
        selected_features.push_back(all_features[idx]);
    }

    return selected_features;
}
```

**Expected Impact**:
- Reduce noise from irrelevant features
- Faster training and inference
- **MRB improvement: +0.1% to +0.3%** (from noise reduction)
- Better generalization

---

#### Opp-7: Ensemble Multiple Models

**Current**: Single EWRLS model
**Opportunity**: Ensemble multiple model types

```cpp
class HybridPredictor {
    std::unique_ptr<EWRLSPredictor> ewrls_;
    std::unique_ptr<OnlineRandomForest> forest_;
    std::unique_ptr<OnlineSVM> svm_;

    double predict(const std::vector<double>& features) {
        double pred_ewrls = ewrls_->predict(features);
        double pred_forest = forest_->predict(features);
        double pred_svm = svm_->predict(features);

        // Weighted ensemble based on recent performance
        double w1 = ewrls_->get_recent_accuracy();
        double w2 = forest_->get_recent_accuracy();
        double w3 = svm_->get_recent_accuracy();
        double total_weight = w1 + w2 + w3;

        return (pred_ewrls * w1 + pred_forest * w2 + pred_svm * w3) / total_weight;
    }
};
```

**Expected Impact**:
- More robust predictions
- Better handling of different market regimes
- **MRB improvement: +0.3% to +0.8%** (from model diversity)
- **Trade-off**: More complex, harder to debug

---

#### Opp-8: Risk-Adjusted Position Sizing

**Current**: Fixed allocation (e.g., 70 shares SPY)
**Opportunity**: Size positions based on predicted return and uncertainty

```cpp
double calculate_position_size(
    double signal_probability,
    double prediction_confidence,
    double current_volatility,
    double available_capital) {

    // Kelly criterion with confidence adjustment
    double edge = std::abs(signal_probability - 0.5) * 2.0;  // 0 to 1
    double win_prob = signal_probability;

    // Reduce position size when confidence is low
    double confidence_factor = prediction_confidence;  // 0 to 1

    // Reduce position size when volatility is high
    double volatility_factor = std::clamp(0.01 / current_volatility, 0.5, 1.0);

    // Kelly fraction
    double kelly_fraction = (win_prob * edge) / (1 - win_prob);

    // Apply safety factors
    double fraction = kelly_fraction * confidence_factor * volatility_factor * 0.5;  // Half-Kelly

    // Clamp to reasonable range
    fraction = std::clamp(fraction, 0.1, 0.5);  // 10% to 50% of capital

    return available_capital * fraction;
}
```

**Expected Impact**:
- Larger positions when high confidence + low volatility
- Smaller positions when uncertain or volatile
- **MRB improvement: +0.2% to +0.6%** (from better risk management)
- **Sharpe ratio improvement: +0.1 to +0.3**

---

#### Opp-9: Regime-Specific Models

**Current**: Single model for all market conditions
**Opportunity**: Train separate models for different regimes

```cpp
class RegimeAwareStrategy {
    std::map<Regime, std::unique_ptr<OnlineEnsembleStrategy>> strategies_;
    Regime current_regime_;

    SignalOutput generate_signal(const Bar& bar) {
        // Detect current regime
        current_regime_ = detect_regime(bar);

        // Use regime-specific strategy
        return strategies_[current_regime_]->generate_signal(bar);
    }

    Regime detect_regime(const Bar& bar) {
        double trend = calculate_trend(50);  // 50-bar trend
        double volatility = calculate_atr(14) / bar.close;

        if (std::abs(trend) > 0.05 && volatility < 0.015) {
            return Regime::TRENDING_STABLE;
        }
        if (std::abs(trend) > 0.05 && volatility >= 0.015) {
            return Regime::TRENDING_VOLATILE;
        }
        if (std::abs(trend) < 0.02 && volatility < 0.015) {
            return Regime::RANGING_STABLE;
        }
        return Regime::RANGING_VOLATILE;
    }
};
```

**Expected Impact**:
- Specialized models perform better than generalist
- **MRB improvement: +0.3% to +1.0%** (from regime specialization)
- More complex to maintain

---

#### Opp-10: Stop-Loss Optimization

**Current**: Fixed 1.5% stop-loss
**Opportunity**: Adaptive stop-loss based on volatility and holding period

```cpp
double calculate_adaptive_stop_loss(
    double entry_price,
    double current_volatility,  // ATR
    int bars_held) {

    // Base stop at 2x ATR
    double volatility_stop = entry_price - (2.0 * current_volatility);

    // Tighten stop-loss over time (trailing stop)
    double time_factor = std::min(1.0, bars_held / 10.0);  // Tighten over 10 bars
    double trailing_stop = entry_price * (1.0 - 0.01 * time_factor);  // 0% to 1%

    // Use the higher of the two (less aggressive)
    return std::max(volatility_stop, trailing_stop);
}
```

**Expected Impact**:
- Wider stops in volatile markets (fewer false stops)
- Tighter stops as position ages (lock in profits)
- **MRB improvement: +0.1% to +0.3%** (from better exit timing)

---

## Recommended Fix Priority

### Phase 1: Critical Safety (Week 1)

**Must fix before real capital deployment**:

1. ✅ **P0-1**: Exception handling in main loop (4 hours)
2. ✅ **P0-2**: Remove hardcoded $100k capital (6 hours)
3. ✅ **P0-3**: Order fill validation (8 hours)
4. ✅ **P0-4**: Position close failure handling (4 hours)
5. ✅ **P0-6**: Fix timezone/DST handling (2 hours)

**Total**: ~24 hours
**Outcome**: System won't crash, financial calculations correct

---

### Phase 2: Learning Effectiveness (Week 2)

**Significant MRB improvements expected**:

1. ✅ **P0-5**: Continuous learning during live trading (4 hours)
2. ✅ **P1-1**: Learn from after-hours data (included in P0-5)
3. ✅ **P1-2**: Consistent train/test methodology (6 hours)
4. ✅ **P1-3**: Fix predictor numerical instability (2 hours)
5. ✅ **P1-5**: Bar validation (4 hours)

**Total**: ~16 hours
**Outcome**: Model learns continuously, adapts faster
**Expected MRB**: +0.2% to +0.5%

---

### Phase 3: Configuration & Robustness (Week 3)

**Operational improvements**:

1. ✅ **P1-4**: Configurable thresholds (6 hours)
2. ✅ **P1-6**: Enable volatility filter (2 hours)
3. ⚠️ **P2 issues**: Feature importance, debug logging cleanup (8 hours)

**Total**: ~16 hours
**Outcome**: Easier to tune, more robust
**Expected MRB**: +0.1% to +0.2%

---

### Phase 4: Performance Optimization (Week 4)

**MRB enhancement opportunities**:

1. ✅ **Opp-4**: Adaptive threshold calibration (4 hours)
2. ✅ **Opp-6**: Feature selection/importance (6 hours)
3. ✅ **Opp-8**: Risk-adjusted position sizing (8 hours)
4. ⚠️ **Opp-10**: Adaptive stop-loss (4 hours)

**Total**: ~22 hours
**Outcome**: Higher MRB, better risk management
**Expected MRB**: +0.4% to +1.2%

---

### Phase 5: Advanced Features (Future)

**Long-term improvements**:

1. ⚠️ **Opp-5**: Multi-timeframe features (16 hours)
2. ⚠️ **Opp-7**: Model ensembling (20 hours)
3. ⚠️ **Opp-9**: Regime-specific models (24 hours)

**Total**: ~60 hours
**Expected MRB**: +0.5% to +2.0% (cumulative)

---

## Testing & Validation Requirements

### Unit Tests Required

```cpp
// Test bar validation
TEST(BarValidation, RejectsInvalidPrices) {
    Bar invalid_bar;
    invalid_bar.close = -10.0;  // Negative price

    OnlineEnsembleStrategy strategy;
    EXPECT_FALSE(strategy.on_bar(invalid_bar));  // Should reject
}

// Test order fill validation
TEST(OrderExecution, WaitsForFill) {
    MockAlpacaClient alpaca;
    alpaca.set_order_status("pending_new");  // Simulate slow fill

    LiveTrader trader(alpaca);
    trader.execute_transition(/* CASH → LONG */);

    EXPECT_EQ(trader.current_state(), CASH_ONLY);  // Should not update state
}

// Test exception handling
TEST(ExceptionHandling, CatchesFeatureExtractionError) {
    MockFeatureEngine engine;
    engine.set_throw_on_get_features(true);

    LiveTrader trader;
    trader.on_new_bar(bar);  // Should not crash

    EXPECT_TRUE(trader.is_in_error_state());
}

// Test capital scaling
TEST(CapitalScaling, Scales Return Calculation) {
    OnlineEnsembleStrategy strategy;
    strategy.set_base_equity(50000.0);  // $50k account

    double pnl = 500.0;  // $500 profit
    strategy.update(bar, pnl, 50000.0);

    auto metrics = strategy.get_performance_metrics();
    EXPECT_NEAR(metrics.avg_return, 0.01, 1e-6);  // 1% return
}
```

---

### Integration Tests Required

```cpp
// Test full warmup→live pipeline
TEST(Integration, WarmupToLiveTransition) {
    LiveTrader trader;

    // Run warmup
    trader.warmup_strategy();
    EXPECT_TRUE(trader.is_ready());

    // Process 100 live bars
    for (int i = 0; i < 100; ++i) {
        Bar bar = generate_test_bar(i);
        trader.on_new_bar(bar);
    }

    // Verify continuous learning
    EXPECT_GT(trader.get_training_count(), 100);

    // Verify trades executed
    EXPECT_GT(trader.get_trade_count(), 0);
}

// Test error recovery
TEST(Integration, RecoversFromAPIFailure) {
    MockAlpacaClient alpaca;
    LiveTrader trader(alpaca);

    // Simulate API failure
    alpaca.set_fail_next_request(true);
    trader.on_new_bar(bar);

    // Should be in error state
    EXPECT_TRUE(trader.is_in_error_state());

    // Recover
    alpaca.set_fail_next_request(false);
    trader.on_new_bar(bar);

    // Should clear error after successful bar
    EXPECT_FALSE(trader.is_in_error_state());
}
```

---

### Validation Criteria

**Before Real Capital Deployment**:

1. ✅ All P0 issues fixed
2. ✅ All unit tests passing
3. ✅ All integration tests passing
4. ✅ 48+ hours of stable paper trading
5. ✅ No crashes or exceptions during paper trading
6. ✅ Order fills confirmed 100% of the time
7. ✅ Position reconciliation working correctly
8. ✅ Performance metrics tracking correctly
9. ✅ MRB > 0% over 100-bar test period
10. ✅ Max drawdown < 5% during paper trading

**Deployment Checklist**:

```markdown
## Pre-Deployment Checklist

### Code Quality
- [ ] All P0 issues resolved
- [ ] All P1 issues resolved
- [ ] Unit test coverage > 80%
- [ ] Integration tests passing
- [ ] No TODOs in critical paths
- [ ] No hardcoded values (capital, thresholds)
- [ ] Exception handling in all critical paths
- [ ] Logging comprehensive and informative

### Paper Trading Validation
- [ ] 48+ hours runtime without crashes
- [ ] 0 unhandled exceptions
- [ ] 100% order fill confirmation
- [ ] Position state matches broker 100%
- [ ] P&L tracking accurate within $1
- [ ] All trades logged and auditable
- [ ] Error recovery tested and working

### Performance Validation
- [ ] MRB > 0% over test period
- [ ] Win rate > 45%
- [ ] Sharpe ratio > 0.5
- [ ] Max drawdown < 5%
- [ ] Average trade frequency: 1-3 per day
- [ ] No excessive churning (< 10 trades/day)

### Operational Readiness
- [ ] Monitoring dashboard working
- [ ] Alerts configured (errors, large losses)
- [ ] Backup connectivity configured
- [ ] Kill switch tested
- [ ] Recovery procedures documented
- [ ] Contact information for emergencies

### Financial Validation
- [ ] Starting capital confirmed with broker
- [ ] Position sizing appropriate (< 50% capital per position)
- [ ] Stop-losses configured
- [ ] Profit targets configured
- [ ] Risk limits enforced (daily/monthly loss limits)

### Final Sign-Off
- [ ] Code review completed
- [ ] System architect approval
- [ ] Risk manager approval
- [ ] Operations approval
- [ ] Legal/compliance approval (if required)
```

---

## Conclusion

**Current Status**: System is **functional for paper trading** but has **6 critical deficiencies** that must be fixed before real capital deployment.

**Timeline to Production**:
- **Phase 1 (Critical)**: 1 week → Makes system safe
- **Phase 2 (Learning)**: 1 week → Makes system effective
- **Phase 3 (Config)**: 1 week → Makes system tunable
- **Validation**: 1 week → Proves system ready

**Total**: 4-5 weeks to production-ready

**Expected Performance After All Fixes**:
- Current MRB: +0.046% per block (from live trading test)
- After Phase 2 fixes: +0.25% to +0.55% per block
- After Phase 4 optimizations: +0.45% to +1.65% per block
- Annualized: 5.5% to 20% (vs current 0.55%)

**Recommendation**: Complete Phase 1 and Phase 2 before risking real capital. Phase 3 and 4 can be iterative improvements during live operation.

---

**Document Status**: COMPREHENSIVE REVIEW COMPLETE
**Next Action**: Implement Phase 1 fixes (P0 issues)
**Review Date**: 2025-10-08
**Reviewer**: Claude Code + User Review


```

## 📄 **FILE 7 of 14**: src/backend/ensemble_position_state_machine.cpp

**File Information**:
- **Path**: `src/backend/ensemble_position_state_machine.cpp`

- **Size**: 477 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "backend/ensemble_position_state_machine.h"
#include "common/utils.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sentio {

EnsemblePositionStateMachine::EnsemblePositionStateMachine() 
    : PositionStateMachine() {
    
    // Initialize performance tracking
    for (int horizon : {1, 5, 10}) {
        horizon_accuracy_[horizon] = 0.5;  // Start neutral
        horizon_pnl_[horizon] = 0.0;
        horizon_trade_count_[horizon] = 0;
    }
    
    utils::log_info("EnsemblePSM initialized with multi-horizon support");
}

EnsemblePositionStateMachine::EnsembleTransition 
EnsemblePositionStateMachine::get_ensemble_transition(
    const PortfolioState& current_portfolio,
    const EnsembleSignal& ensemble_signal,
    const MarketState& market_conditions,
    uint64_t current_bar_id) {
    
    EnsembleTransition transition;
    
    // First, check if we need to close any positions
    auto closeable = get_closeable_positions(current_bar_id);
    if (!closeable.empty()) {
        utils::log_info("Closing " + std::to_string(closeable.size()) + 
                       " positions at target horizons");
        
        // Determine current state for base PSM
        State current_state = determine_current_state(current_portfolio);
        
        transition.current_state = current_state;
        transition.target_state = State::CASH_ONLY;  // Temporary, will recalculate
        transition.optimal_action = "Close matured positions";
        
        // Mark positions for closing
        for (auto pos : closeable) {
            transition.horizon_positions.push_back(pos);
            pos.is_active = false;
        }
    }
    
    // Check signal agreement across horizons
    transition.has_consensus = ensemble_signal.signal_agreement >= MIN_AGREEMENT;
    
    if (!transition.has_consensus && get_active_positions().empty()) {
        // No consensus and no positions - stay in cash
        transition.current_state = State::CASH_ONLY;
        transition.target_state = State::CASH_ONLY;
        transition.optimal_action = "No consensus - hold cash";
        transition.theoretical_basis = "Disagreement across horizons (" + 
                                      std::to_string(ensemble_signal.signal_agreement) + ")";
        return transition;
    }
    
    // Calculate allocations based on signal strength and agreement
    auto allocations = calculate_horizon_allocations(ensemble_signal);
    transition.horizon_allocations = allocations;
    
    // Determine dominant horizon (strongest signal)
    int dominant_horizon = 0;
    double max_weight = 0.0;
    for (size_t i = 0; i < ensemble_signal.horizon_bars.size(); ++i) {
        double weight = ensemble_signal.horizon_weights[i] * 
                       std::abs(ensemble_signal.horizon_signals[i].probability - 0.5);
        if (weight > max_weight) {
            max_weight = weight;
            dominant_horizon = ensemble_signal.horizon_bars[i];
        }
    }
    transition.dominant_horizon = dominant_horizon;
    
    // Create positions for each horizon that agrees
    for (size_t i = 0; i < ensemble_signal.horizon_signals.size(); ++i) {
        const auto& signal = ensemble_signal.horizon_signals[i];
        int horizon = ensemble_signal.horizon_bars[i];
        
        // Skip if this horizon disagrees with consensus
        if (signal.signal_type != ensemble_signal.consensus_signal) {
            continue;
        }
        
        // Check if we already have a position for this horizon
        bool has_existing = false;
        for (const auto& pos : positions_by_horizon_[horizon]) {
            if (pos.is_active && pos.exit_bar_id > current_bar_id) {
                has_existing = true;
                break;
            }
        }
        
        if (!has_existing && allocations[horizon] > 0) {
            HorizonPosition new_pos;
            new_pos.symbol = signal.symbol;
            new_pos.horizon_bars = horizon;
            new_pos.entry_bar_id = current_bar_id;
            new_pos.exit_bar_id = current_bar_id + horizon;
            new_pos.predicted_return = (signal.probability - 0.5) * 2.0 / std::sqrt(horizon);
            new_pos.position_weight = allocations[horizon];
            new_pos.signal_type = signal.signal_type;
            new_pos.is_active = true;
            
            transition.horizon_positions.push_back(new_pos);
        }
    }
    
    // Calculate total position size
    transition.total_position_size = 0.0;
    for (const auto& [horizon, allocation] : allocations) {
        transition.total_position_size += allocation;
    }
    
    // Determine target state based on consensus and positions
    if (ensemble_signal.consensus_signal == sentio::SignalType::LONG) {
        if (transition.total_position_size > 0.6) {
            transition.target_state = State::TQQQ_ONLY;  // High conviction long
        } else {
            transition.target_state = State::QQQ_ONLY;   // Normal long
        }
    } else if (ensemble_signal.consensus_signal == sentio::SignalType::SHORT) {
        if (transition.total_position_size > 0.6) {
            transition.target_state = State::SQQQ_ONLY;  // High conviction short
        } else {
            transition.target_state = State::PSQ_ONLY;   // Normal short
        }
    } else {
        transition.target_state = State::CASH_ONLY;
    }
    
    // Set metadata
    transition.confidence = ensemble_signal.confidence;
    transition.expected_return = ensemble_signal.weighted_probability - 0.5;
    transition.risk_score = calculate_ensemble_risk(transition.horizon_positions);
    
    std::string action_detail = "Ensemble: ";
    for (const auto& [h, a] : allocations) {
        action_detail += std::to_string(h) + "bar=" + 
                        std::to_string(static_cast<int>(a * 100)) + "% ";
    }
    transition.optimal_action = action_detail;
    
    return transition;
}

EnsemblePositionStateMachine::EnsembleSignal 
EnsemblePositionStateMachine::aggregate_signals(
    const std::map<int, SignalOutput>& horizon_signals,
    const std::map<int, double>& horizon_weights) {
    
    EnsembleSignal ensemble;
    
    // Extract signals and weights
    for (const auto& [horizon, signal] : horizon_signals) {
        ensemble.horizon_signals.push_back(signal);
        ensemble.horizon_bars.push_back(horizon);
        
        double weight = horizon_weights.count(horizon) ? horizon_weights.at(horizon) : 1.0;
        // Adjust weight by historical performance
        if (horizon_trade_count_[horizon] > 10) {
            weight *= (0.5 + horizon_accuracy_[horizon]);  // Scale by accuracy
        }
        ensemble.horizon_weights.push_back(weight);
    }
    
    // Calculate weighted probability
    double total_weight = 0.0;
    ensemble.weighted_probability = 0.0;
    
    for (size_t i = 0; i < ensemble.horizon_signals.size(); ++i) {
        double w = ensemble.horizon_weights[i];
        ensemble.weighted_probability += ensemble.horizon_signals[i].probability * w;
        total_weight += w;
    }
    
    if (total_weight > 0) {
        ensemble.weighted_probability /= total_weight;
    }
    
    // Determine consensus signal
    ensemble.consensus_signal = determine_consensus(ensemble.horizon_signals, 
                                                   ensemble.horizon_weights);
    
    // Calculate agreement (0-1)
    ensemble.signal_agreement = calculate_agreement(ensemble.horizon_signals);
    
    // Calculate confidence based on agreement and signal strength
    double signal_strength = std::abs(ensemble.weighted_probability - 0.5) * 2.0;
    ensemble.confidence = signal_strength * ensemble.signal_agreement;
    
    utils::log_debug("Ensemble aggregation: prob=" + 
                    std::to_string(ensemble.weighted_probability) +
                    ", agreement=" + std::to_string(ensemble.signal_agreement) +
                    ", consensus=" + (ensemble.consensus_signal == sentio::SignalType::LONG ? "LONG" :
                                     ensemble.consensus_signal == sentio::SignalType::SHORT ? "SHORT" : 
                                     "NEUTRAL"));
    
    return ensemble;
}

std::map<int, double> EnsemblePositionStateMachine::calculate_horizon_allocations(
    const EnsembleSignal& signal) {
    
    std::map<int, double> allocations;
    
    // Start with base allocation for each agreeing horizon
    for (size_t i = 0; i < signal.horizon_signals.size(); ++i) {
        int horizon = signal.horizon_bars[i];
        
        if (signal.horizon_signals[i].signal_type == signal.consensus_signal) {
            // Base allocation weighted by historical performance
            double perf_weight = 0.5;  // Default
            if (horizon_trade_count_[horizon] > 5) {
                perf_weight = horizon_accuracy_[horizon];
            }
            
            allocations[horizon] = BASE_ALLOCATION * perf_weight;
        } else {
            allocations[horizon] = 0.0;
        }
    }
    
    // Add consensus bonus if high agreement
    if (signal.signal_agreement > 0.8) {
        double bonus_per_horizon = CONSENSUS_BONUS / allocations.size();
        for (auto& [horizon, alloc] : allocations) {
            alloc += bonus_per_horizon;
        }
    }
    
    // Normalize to not exceed max position
    double total = 0.0;
    for (const auto& [h, a] : allocations) {
        total += a;
    }
    
    double max_position = get_maximum_position_size();
    if (total > max_position) {
        double scale = max_position / total;
        for (auto& [h, a] : allocations) {
            a *= scale;
        }
    }
    
    return allocations;
}

void EnsemblePositionStateMachine::update_horizon_positions(uint64_t current_bar_id, 
                                                           double current_price) {
    // Update each horizon's positions
    for (auto& [horizon, positions] : positions_by_horizon_) {
        for (auto& pos : positions) {
            if (pos.is_active && current_bar_id >= pos.exit_bar_id) {
                // Calculate realized return
                double realized_return = (current_price - pos.entry_price) / pos.entry_price;
                
                // If SHORT position, reverse the return
                if (pos.signal_type == sentio::SignalType::SHORT) {
                    realized_return = -realized_return;
                }
                
                // Update performance tracking
                update_horizon_performance(horizon, realized_return);
                
                // Mark as inactive
                pos.is_active = false;
                
                utils::log_info("Closed " + std::to_string(horizon) + "-bar position: " +
                              "return=" + std::to_string(realized_return * 100) + "%");
            }
        }
        
        // Remove inactive positions
        positions.erase(
            std::remove_if(positions.begin(), positions.end(),
                          [](const HorizonPosition& p) { return !p.is_active; }),
            positions.end()
        );
    }
}

std::vector<EnsemblePositionStateMachine::HorizonPosition> 
EnsemblePositionStateMachine::get_active_positions() const {
    std::vector<HorizonPosition> active;
    
    for (const auto& [horizon, positions] : positions_by_horizon_) {
        for (const auto& pos : positions) {
            if (pos.is_active) {
                active.push_back(pos);
            }
        }
    }
    
    return active;
}

std::vector<EnsemblePositionStateMachine::HorizonPosition> 
EnsemblePositionStateMachine::get_closeable_positions(uint64_t current_bar_id) const {
    std::vector<HorizonPosition> closeable;
    
    for (const auto& [horizon, positions] : positions_by_horizon_) {
        for (const auto& pos : positions) {
            if (pos.is_active && current_bar_id >= pos.exit_bar_id) {
                closeable.push_back(pos);
            }
        }
    }
    
    return closeable;
}

double EnsemblePositionStateMachine::calculate_ensemble_risk(
    const std::vector<HorizonPosition>& positions) const {
    
    if (positions.empty()) return 0.0;
    
    // Risk increases with:
    // 1. Total position size
    // 2. Disagreement across horizons
    // 3. Longer horizons (more uncertainty)
    
    double total_weight = 0.0;
    double weighted_horizon = 0.0;
    
    for (const auto& pos : positions) {
        total_weight += pos.position_weight;
        weighted_horizon += pos.horizon_bars * pos.position_weight;
    }
    
    double avg_horizon = weighted_horizon / std::max(0.01, total_weight);
    
    // Risk score (0-1)
    double position_risk = total_weight;  // Already 0-1
    double horizon_risk = avg_horizon / 10.0;  // Normalize by max horizon
    
    return std::min(1.0, position_risk * 0.7 + horizon_risk * 0.3);
}

double EnsemblePositionStateMachine::get_maximum_position_size() const {
    // Dynamic max position based on recent performance
    double base_max = 0.8;  // 80% max
    
    // Calculate recent win rate across all horizons
    double total_accuracy = 0.0;
    double total_trades = 0.0;
    
    for (const auto& [horizon, accuracy] : horizon_accuracy_) {
        if (horizon_trade_count_.at(horizon) > 0) {
            total_accuracy += accuracy * horizon_trade_count_.at(horizon);
            total_trades += horizon_trade_count_.at(horizon);
        }
    }
    
    if (total_trades > 10) {
        double avg_accuracy = total_accuracy / total_trades;
        // Scale max position by performance
        if (avg_accuracy > 0.55) {
            base_max = std::min(0.95, base_max + (avg_accuracy - 0.55) * 2.0);
        } else if (avg_accuracy < 0.45) {
            base_max = std::max(0.5, base_max - (0.45 - avg_accuracy) * 2.0);
        }
    }
    
    return base_max;
}

sentio::SignalType EnsemblePositionStateMachine::determine_consensus(
    const std::vector<SignalOutput>& signals,
    const std::vector<double>& weights) const {
    
    double long_weight = 0.0;
    double short_weight = 0.0;
    double neutral_weight = 0.0;
    
    for (size_t i = 0; i < signals.size(); ++i) {
        double w = weights[i];
        switch (signals[i].signal_type) {
            case sentio::SignalType::LONG:
                long_weight += w;
                break;
            case sentio::SignalType::SHORT:
                short_weight += w;
                break;
            case sentio::SignalType::NEUTRAL:
                neutral_weight += w;
                break;
        }
    }
    
    // Require clear majority
    double total = long_weight + short_weight + neutral_weight;
    if (long_weight / total > 0.5) return sentio::SignalType::LONG;
    if (short_weight / total > 0.5) return sentio::SignalType::SHORT;
    return sentio::SignalType::NEUTRAL;
}

double EnsemblePositionStateMachine::calculate_agreement(
    const std::vector<SignalOutput>& signals) const {
    
    if (signals.size() <= 1) return 1.0;
    
    // Count how many signals agree with each other
    int agreements = 0;
    int comparisons = 0;
    
    for (size_t i = 0; i < signals.size(); ++i) {
        for (size_t j = i + 1; j < signals.size(); ++j) {
            comparisons++;
            if (signals[i].signal_type == signals[j].signal_type) {
                agreements++;
            }
        }
    }
    
    return comparisons > 0 ? static_cast<double>(agreements) / comparisons : 0.0;
}

void EnsemblePositionStateMachine::update_horizon_performance(int horizon, double pnl) {
    horizon_pnl_[horizon] += pnl;
    horizon_trade_count_[horizon]++;
    
    // Update accuracy (exponentially weighted)
    double was_correct = pnl > 0 ? 1.0 : 0.0;
    double alpha = 0.1;  // Learning rate
    horizon_accuracy_[horizon] = (1 - alpha) * horizon_accuracy_[horizon] + alpha * was_correct;
    
    utils::log_info("Horizon " + std::to_string(horizon) + " performance: " +
                   "accuracy=" + std::to_string(horizon_accuracy_[horizon]) +
                   ", total_pnl=" + std::to_string(horizon_pnl_[horizon]) +
                   ", trades=" + std::to_string(horizon_trade_count_[horizon]));
}

bool EnsemblePositionStateMachine::should_override_hold(
    const EnsembleSignal& signal, uint64_t current_bar_id) const {
    
    // Override hold if:
    // 1. Very high agreement (>90%)
    // 2. Strong signal from best-performing horizon
    // 3. No conflicting positions
    
    if (signal.signal_agreement > 0.9 && signal.confidence > 0.7) {
        return true;
    }
    
    // Check if best performer strongly agrees
    int best_horizon = 0;
    double best_accuracy = 0.0;
    for (const auto& [h, acc] : horizon_accuracy_) {
        if (horizon_trade_count_.at(h) > 10 && acc > best_accuracy) {
            best_accuracy = acc;
            best_horizon = h;
        }
    }
    
    if (best_accuracy > 0.6) {
        // Find signal from best horizon
        for (size_t i = 0; i < signal.horizon_bars.size(); ++i) {
            if (signal.horizon_bars[i] == best_horizon) {
                double signal_strength = std::abs(signal.horizon_signals[i].probability - 0.5);
                if (signal_strength > 0.3) {  // Strong signal from best performer
                    return true;
                }
            }
        }
    }
    
    return false;
}

} // namespace sentio

```

## 📄 **FILE 8 of 14**: src/backend/position_state_machine.cpp

**File Information**:
- **Path**: `src/backend/position_state_machine.cpp`

- **Size**: 685 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "backend/position_state_machine.h"
#include "common/utils.h"
#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <algorithm>

// Constructor for the PositionStateMachine.
sentio::PositionStateMachine::PositionStateMachine() {
    initialize_transition_matrix();
    utils::log_info("PositionStateMachine initialized with 32 state transitions");
    // In a full implementation, you would also initialize:
    // optimization_engine_ = std::make_unique<OptimizationEngine>();
    // risk_manager_ = std::make_unique<RiskManager>();
}

// REQ-PSM-002: Maps all signal scenarios to optimal state transitions.
void sentio::PositionStateMachine::initialize_transition_matrix() {
    utils::log_debug("Initializing Position State Machine transition matrix with 32 scenarios");
    
    // This function populates all 32 decision scenarios from the requirements document.
    
    // 1. CASH_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::CASH_ONLY, SignalType::STRONG_BUY}] = {
        State::CASH_ONLY, SignalType::STRONG_BUY, State::TQQQ_ONLY, 
        "Initiate TQQQ position", "Maximize leverage on strong signal",
        0.15, 0.8, 0.9  // expected_return, risk_score, confidence
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::WEAK_BUY}] = {
        State::CASH_ONLY, SignalType::WEAK_BUY, State::QQQ_ONLY, 
        "Initiate QQQ position", "Conservative entry",
        0.08, 0.4, 0.7
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::WEAK_SELL}] = {
        State::CASH_ONLY, SignalType::WEAK_SELL, State::PSQ_ONLY, 
        "Initiate PSQ position", "Conservative short entry",
        0.06, 0.4, 0.6
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::STRONG_SELL}] = {
        State::CASH_ONLY, SignalType::STRONG_SELL, State::SQQQ_ONLY, 
        "Initiate SQQQ position", "Maximize short leverage",
        0.12, 0.8, 0.85
    };

    // 2. QQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::QQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::QQQ_ONLY, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Scale up with TQQQ", "Leverage profitable position",
        0.18, 0.6, 0.85
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::QQQ_ONLY, SignalType::WEAK_BUY, State::QQQ_ONLY, 
        "Add to QQQ position", "Conservative scaling",
        0.05, 0.3, 0.6
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::QQQ_ONLY, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Partial QQQ liquidation", "Risk reduction",
        0.02, 0.2, 0.5
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::QQQ_ONLY, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full QQQ liquidation", "Capital preservation",
        0.0, 0.1, 0.9
    };

    // 3. TQQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::TQQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::TQQQ_ONLY, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Add QQQ for stability", "Diversify leverage risk",
        0.12, 0.5, 0.8
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::TQQQ_ONLY, SignalType::WEAK_BUY, State::TQQQ_ONLY, 
        "Scale up TQQQ", "Maintain leverage",
        0.08, 0.7, 0.6
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::TQQQ_ONLY, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Partial TQQQ -> QQQ", "De-leverage gradually",
        0.03, 0.3, 0.7
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::TQQQ_ONLY, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full TQQQ liquidation", "Rapid de-risking",
        0.0, 0.1, 0.95
    };

    // 4. PSQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::PSQ_ONLY, SignalType::STRONG_BUY}] = {
        State::PSQ_ONLY, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full PSQ liquidation", "Directional reversal",
        0.0, 0.2, 0.9
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::WEAK_BUY}] = {
        State::PSQ_ONLY, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Partial PSQ liquidation", "Gradual unwinding",
        0.02, 0.3, 0.6
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::WEAK_SELL}] = {
        State::PSQ_ONLY, SignalType::WEAK_SELL, State::PSQ_ONLY, 
        "Add to PSQ position", "Reinforce position",
        0.04, 0.4, 0.6
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::STRONG_SELL}] = {
        State::PSQ_ONLY, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Scale up with SQQQ", "Amplify short exposure",
        0.15, 0.7, 0.8
    };

    // 5. SQQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::SQQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::SQQQ_ONLY, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full SQQQ liquidation", "Rapid directional reversal",
        0.0, 0.1, 0.95
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::SQQQ_ONLY, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Partial SQQQ -> PSQ", "Gradual de-leveraging",
        0.02, 0.4, 0.7
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::SQQQ_ONLY, SignalType::WEAK_SELL, State::SQQQ_ONLY, 
        "Scale up SQQQ", "Maintain leverage",
        0.06, 0.8, 0.6
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::SQQQ_ONLY, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Add PSQ for stability", "Diversify short risk",
        0.10, 0.6, 0.8
    };

    // 6. QQQ_TQQQ State Transitions (4 scenarios)
    transition_matrix_[{State::QQQ_TQQQ, SignalType::STRONG_BUY}] = {
        State::QQQ_TQQQ, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Scale both positions", "Amplify winning strategy",
        0.20, 0.8, 0.9
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::WEAK_BUY}] = {
        State::QQQ_TQQQ, SignalType::WEAK_BUY, State::QQQ_TQQQ, 
        "Add to QQQ only", "Conservative scaling",
        0.06, 0.4, 0.6
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::WEAK_SELL}] = {
        State::QQQ_TQQQ, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Liquidate TQQQ first", "De-leverage gradually",
        0.02, 0.3, 0.7
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::STRONG_SELL}] = {
        State::QQQ_TQQQ, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full liquidation", "Rapid risk reduction",
        0.0, 0.1, 0.95
    };

    // 7. PSQ_SQQQ State Transitions (4 scenarios)
    transition_matrix_[{State::PSQ_SQQQ, SignalType::STRONG_BUY}] = {
        State::PSQ_SQQQ, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full liquidation", "Complete directional reversal",
        0.0, 0.1, 0.95
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::WEAK_BUY}] = {
        State::PSQ_SQQQ, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Liquidate SQQQ first", "Gradual de-leveraging",
        0.02, 0.4, 0.7
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::WEAK_SELL}] = {
        State::PSQ_SQQQ, SignalType::WEAK_SELL, State::PSQ_SQQQ, 
        "Add to PSQ only", "Conservative scaling",
        0.05, 0.5, 0.6
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::STRONG_SELL}] = {
        State::PSQ_SQQQ, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Scale both positions", "Amplify short strategy",
        0.18, 0.8, 0.85
    };
    
    utils::log_info("Position State Machine transition matrix initialized with " + 
                   std::to_string(transition_matrix_.size()) + " transitions");
}

// REQ-PSM-005: Provide real-time state transition recommendations.
sentio::PositionStateMachine::StateTransition sentio::PositionStateMachine::get_optimal_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions,
    double confidence_threshold) {
    
    // 1. Determine the current state from the portfolio.
    State current_state = determine_current_state(current_portfolio);

    // CRITICAL NEW LOGIC: Check hold period enforcement FIRST
    if (is_in_hold_period(current_portfolio, signal.bar_id)) {
        int max_bars_remaining = 0;
        std::string held_symbols;
        
        for (const auto& [symbol, position] : current_portfolio.positions) {
            if (position.quantity > 1e-6) {
                int bars_remaining = get_bars_remaining(symbol, signal.bar_id);
                if (bars_remaining > 0) {
                    max_bars_remaining = std::max(max_bars_remaining, bars_remaining);
                    if (!held_symbols.empty()) held_symbols += ", ";
                    held_symbols += symbol + "(" + std::to_string(bars_remaining) + " bars)";
                }
            }
        }
        
        StateTransition hold_transition;
        hold_transition.current_state = current_state;
        hold_transition.target_state = current_state;
        hold_transition.signal_type = SignalType::NEUTRAL;
        hold_transition.optimal_action = "HOLD - Minimum period enforced";
        hold_transition.theoretical_basis = "Positions held: " + held_symbols;
        hold_transition.confidence = 1.0;
        hold_transition.expected_return = 0.0;
        hold_transition.risk_score = 0.1;
        hold_transition.is_hold_enforced = true;
        hold_transition.prediction_horizon = signal.prediction_horizon;
        hold_transition.bars_remaining = max_bars_remaining;
        
        utils::log_debug("Hold period enforced: " + held_symbols);
        return hold_transition;
    }

    // Handle the INVALID state immediately.
    if (current_state == State::INVALID) {
        utils::log_warning("INVALID portfolio state detected - triggering emergency liquidation");
        StateTransition invalid_transition;
        invalid_transition.current_state = State::INVALID;
        invalid_transition.signal_type = SignalType::NEUTRAL;
        invalid_transition.target_state = State::CASH_ONLY;
        invalid_transition.optimal_action = "Emergency liquidation";
        invalid_transition.theoretical_basis = "Risk containment";
        invalid_transition.expected_return = 0.0;
        invalid_transition.risk_score = 0.0;
        invalid_transition.confidence = 1.0;
        invalid_transition.prediction_horizon = signal.prediction_horizon;
        invalid_transition.position_open_bar_id = signal.bar_id;
        invalid_transition.earliest_exit_bar_id = signal.bar_id + signal.prediction_horizon;
        return invalid_transition;
    }

    // 2. Classify the incoming signal using dynamic thresholds
    SignalType signal_type = classify_signal(signal, DEFAULT_BUY_THRESHOLD, DEFAULT_SELL_THRESHOLD, confidence_threshold);

    // Handle NEUTRAL signal (no action).
    if (signal_type == SignalType::NEUTRAL) {
        utils::log_debug("NEUTRAL signal (" + std::to_string(signal.probability) + 
                        ") - maintaining current state: " + state_to_string(current_state));
        StateTransition neutral_transition;
        neutral_transition.current_state = current_state;
        neutral_transition.signal_type = signal_type;
        neutral_transition.target_state = current_state;
        neutral_transition.optimal_action = "Hold position";
        neutral_transition.theoretical_basis = "Signal in neutral zone";
        neutral_transition.expected_return = 0.0;
        neutral_transition.risk_score = 0.0;
        neutral_transition.confidence = 0.5;
        neutral_transition.prediction_horizon = signal.prediction_horizon;
        return neutral_transition;
    }
    
    // 3. Look up the transition in the matrix.
    auto it = transition_matrix_.find({current_state, signal_type});

    if (it != transition_matrix_.end()) {
        StateTransition transition = it->second;
        
        // Apply market condition adjustments
        transition.risk_score = apply_state_risk_adjustment(current_state, transition.risk_score);
        
        // NEW: Enhance with multi-bar information
        transition.prediction_horizon = signal.prediction_horizon;
        transition.position_open_bar_id = signal.bar_id;
        transition.earliest_exit_bar_id = signal.bar_id + signal.prediction_horizon;
        transition.is_hold_enforced = false;
        
        // Update position tracking if entering new positions
        if (transition.target_state != State::CASH_ONLY && 
            transition.target_state != current_state) {
            update_position_tracking(signal, transition);
        }
        
        utils::log_debug("PSM Transition: " + state_to_string(current_state) + 
                        " + " + signal_type_to_string(signal_type) + 
                        " -> " + state_to_string(transition.target_state) + 
                        " (horizon=" + std::to_string(signal.prediction_horizon) + " bars)");
        
        return transition;
    }

    // Fallback if a transition is not defined (should not happen with a complete matrix).
    utils::log_error("Undefined transition for state=" + state_to_string(current_state) + 
                     ", signal=" + signal_type_to_string(signal_type));
    return {current_state, signal_type, current_state, 
            "Hold (Undefined Transition)", "No valid action defined for this state/signal pair",
            0.0, 1.0, 0.0};
}

// REQ-PSM-004: Integration with adaptive threshold mechanism
std::pair<double, double> sentio::PositionStateMachine::get_state_aware_thresholds(
    double base_buy_threshold,
    double base_sell_threshold,
    State current_state
) const {
    
    double buy_adjustment = 1.0;
    double sell_adjustment = 1.0;
    
    // State-specific threshold adjustments based on risk profile
    switch (current_state) {
        case State::QQQ_TQQQ:
        case State::PSQ_SQQQ:
            // More conservative thresholds for leveraged positions
            buy_adjustment = 0.95;  // Slightly higher buy threshold
            sell_adjustment = 1.05; // Slightly lower sell threshold
            break;
            
        case State::TQQQ_ONLY:
        case State::SQQQ_ONLY:
            // Very conservative for high-leverage single positions
            buy_adjustment = 0.90;
            sell_adjustment = 1.10;
            break;
            
        case State::CASH_ONLY:
            // More aggressive thresholds for cash deployment
            buy_adjustment = 1.05;  // Slightly lower buy threshold
            sell_adjustment = 0.95; // Slightly higher sell threshold
            break;
            
        case State::QQQ_ONLY:
        case State::PSQ_ONLY:
            // Standard adjustments for single unleveraged positions
            buy_adjustment = 1.0;
            sell_adjustment = 1.0;
            break;
            
        case State::INVALID:
            // Emergency conservative thresholds
            buy_adjustment = 0.80;
            sell_adjustment = 1.20;
            break;
    }
    
    double adjusted_buy = base_buy_threshold * buy_adjustment;
    double adjusted_sell = base_sell_threshold * sell_adjustment;
    
    // Ensure thresholds maintain minimum gap
    if (adjusted_buy - adjusted_sell < 0.05) {
        double gap = 0.05;
        double midpoint = (adjusted_buy + adjusted_sell) / 2.0;
        adjusted_buy = midpoint + gap / 2.0;
        adjusted_sell = midpoint - gap / 2.0;
    }
    
    // Clamp to reasonable bounds
    adjusted_buy = std::clamp(adjusted_buy, 0.51, 0.90);
    adjusted_sell = std::clamp(adjusted_sell, 0.10, 0.49);
    
    utils::log_debug("State-aware thresholds for " + state_to_string(current_state) + 
                    ": buy=" + std::to_string(adjusted_buy) + 
                    ", sell=" + std::to_string(adjusted_sell));
    
    return {adjusted_buy, adjusted_sell};
}

// REQ-PSM-003: Theoretical optimization framework validation
bool sentio::PositionStateMachine::validate_transition(
    const StateTransition& transition,
    const PortfolioState& current_portfolio,
    double available_capital
) const {
    
    // Basic validation checks
    if (transition.risk_score > 0.9) {
        utils::log_warning("High risk transition rejected: risk_score=" + 
                          std::to_string(transition.risk_score));
        return false;
    }
    
    if (transition.confidence < 0.3) {
        utils::log_warning("Low confidence transition rejected: confidence=" + 
                          std::to_string(transition.confidence));
        return false;
    }
    
    // Check capital requirements (simplified)
    if (available_capital < MIN_CASH_BUFFER * 100000) { // Assuming $100k base capital
        utils::log_warning("Insufficient capital for transition: available=" + 
                          std::to_string(available_capital));
        return false;
    }
    
    // Validate state transition logic
    if (transition.current_state == State::INVALID && 
        transition.target_state != State::CASH_ONLY) {
        utils::log_error("Invalid state must transition to CASH_ONLY");
        return false;
    }
    
    utils::log_debug("Transition validation passed for " + 
                    state_to_string(transition.current_state) + " -> " + 
                    state_to_string(transition.target_state));
    
    return true;
}

sentio::PositionStateMachine::State sentio::PositionStateMachine::determine_current_state(const PortfolioState& portfolio) const {
    std::set<std::string> symbols;
    for (const auto& [symbol, pos] : portfolio.positions) {
        if (pos.quantity > 1e-6) { // Consider only positions with meaningful quantity
            symbols.insert(symbol);
        }
    }

    if (symbols.empty()) {
        return State::CASH_ONLY;
    }
    
    bool has_qqq = symbols.count("QQQ");
    bool has_tqqq = symbols.count("TQQQ");
    bool has_psq = symbols.count("PSQ");
    bool has_sqqq = symbols.count("SQQQ");

    // Single Instrument States
    if (symbols.size() == 1) {
        if (has_qqq) return State::QQQ_ONLY;
        if (has_tqqq) return State::TQQQ_ONLY;
        if (has_psq) return State::PSQ_ONLY;
        if (has_sqqq) return State::SQQQ_ONLY;
    }

    // Dual Instrument States (valid combinations only)
    if (symbols.size() == 2) {
        if (has_qqq && has_tqqq) return State::QQQ_TQQQ;
        if (has_psq && has_sqqq) return State::PSQ_SQQQ;
    }

    // Any other combination is considered invalid (e.g., QQQ + PSQ, TQQQ + SQQQ)
    utils::log_warning("Invalid portfolio state detected with symbols: " + 
                      [&symbols]() {
                          std::string result;
                          for (const auto& s : symbols) {
                              if (!result.empty()) result += ", ";
                              result += s;
                          }
                          return result;
                      }());
    return State::INVALID;
}

sentio::PositionStateMachine::SignalType sentio::PositionStateMachine::classify_signal(
    const SignalOutput& signal,
    double buy_threshold,
    double sell_threshold,
    double confidence_threshold
) const {
    // Confidence filtering removed - using signal strength from probability only
    double p = signal.probability;
    
    if (p > (buy_threshold + STRONG_MARGIN)) return SignalType::STRONG_BUY;
    if (p > buy_threshold) return SignalType::WEAK_BUY;
    if (p < (sell_threshold - STRONG_MARGIN)) return SignalType::STRONG_SELL;
    if (p < sell_threshold) return SignalType::WEAK_SELL;
    
    return SignalType::NEUTRAL;
}

double sentio::PositionStateMachine::apply_state_risk_adjustment(State state, double base_risk) const {
    double adjustment = 1.0;
    
    switch (state) {
        case State::TQQQ_ONLY:
        case State::SQQQ_ONLY:
            adjustment = 1.3; // Higher risk for leveraged single positions
            break;
        case State::QQQ_TQQQ:
        case State::PSQ_SQQQ:
            adjustment = 1.2; // Moderate increase for dual positions
            break;
        case State::CASH_ONLY:
            adjustment = 0.5; // Lower risk for cash positions
            break;
        default:
            adjustment = 1.0; // No adjustment for standard positions
            break;
    }
    
    return std::clamp(base_risk * adjustment, 0.0, 1.0);
}

double sentio::PositionStateMachine::calculate_kelly_position_size(
    double signal_probability,
    double expected_return,
    double risk_estimate,
    double available_capital
) const {
    // Kelly Criterion: f* = (bp - q) / b
    // where b = odds, p = win probability, q = loss probability
    
    if (risk_estimate <= 0.0 || expected_return <= 0.0) {
        return 0.0;
    }
    
    double win_prob = std::clamp(signal_probability, 0.1, 0.9);
    double loss_prob = 1.0 - win_prob;
    double odds = expected_return / risk_estimate;
    
    double kelly_fraction = (odds * win_prob - loss_prob) / odds;
    kelly_fraction = std::clamp(kelly_fraction, 0.0, MAX_POSITION_SIZE);
    
    return available_capital * kelly_fraction;
}

// Helper function to convert State enum to string for logging and debugging.
std::string sentio::PositionStateMachine::state_to_string(State s) {
    switch (s) {
        case State::CASH_ONLY: return "CASH_ONLY";
        case State::QQQ_ONLY: return "BASE_ONLY";           // 1x base (QQQ/SPY)
        case State::TQQQ_ONLY: return "BULL_3X_ONLY";       // 3x bull (TQQQ/SPXL)
        case State::PSQ_ONLY: return "BEAR_1X_ONLY";        // -1x bear (PSQ/SH)
        case State::SQQQ_ONLY: return "BEAR_NX_ONLY";       // -2x/-3x bear (SQQQ/SDS/SPXS)
        case State::QQQ_TQQQ: return "BASE_BULL_3X";        // 50% base + 50% bull
        case State::PSQ_SQQQ: return "BEAR_1X_NX";          // 50% bear_1x + 50% bear_nx
        case State::INVALID: return "INVALID";
        default: return "UNKNOWN_STATE";
    }
}

// Helper function to convert SignalType enum to string for logging and debugging.
std::string sentio::PositionStateMachine::signal_type_to_string(SignalType st) {
    switch (st) {
        case SignalType::STRONG_BUY: return "STRONG_BUY";
        case SignalType::WEAK_BUY: return "WEAK_BUY";
        case SignalType::WEAK_SELL: return "WEAK_SELL";
        case SignalType::STRONG_SELL: return "STRONG_SELL";
        case SignalType::NEUTRAL: return "NEUTRAL";
        default: return "UNKNOWN_SIGNAL";
    }
}

// NEW: Multi-bar support methods
bool sentio::PositionStateMachine::can_close_position(uint64_t current_bar_id, 
                                                      const std::string& symbol) const {
    auto it = position_tracking_.find(symbol);
    if (it == position_tracking_.end()) {
        return true; // No tracking = can close
    }
    
    const auto& tracking = it->second;
    uint64_t minimum_exit_bar = tracking.open_bar_id + tracking.horizon;
    
    return current_bar_id >= minimum_exit_bar;
}

void sentio::PositionStateMachine::record_position_entry(const std::string& symbol, 
                                                         uint64_t bar_id, 
                                                         int horizon, 
                                                         double entry_price) {
    PositionTracking tracking;
    tracking.open_bar_id = bar_id;
    tracking.horizon = horizon;
    tracking.entry_price = entry_price;
    tracking.symbol = symbol;
    
    position_tracking_[symbol] = tracking;
    
    utils::log_info("Position opened: " + symbol + 
                   " at bar " + std::to_string(bar_id) + 
                   " with " + std::to_string(horizon) + "-bar horizon");
}

void sentio::PositionStateMachine::record_position_exit(const std::string& symbol) {
    auto it = position_tracking_.find(symbol);
    if (it != position_tracking_.end()) {
        utils::log_info("Position closed: " + symbol);
        position_tracking_.erase(it);
    }
}

void sentio::PositionStateMachine::clear_position_tracking() {
    position_tracking_.clear();
    utils::log_info("Position tracking cleared");
}

int sentio::PositionStateMachine::get_bars_held(const std::string& symbol, 
                                                uint64_t current_bar_id) const {
    auto it = position_tracking_.find(symbol);
    if (it == position_tracking_.end()) {
        return 0;
    }
    return current_bar_id - it->second.open_bar_id;
}

int sentio::PositionStateMachine::get_bars_remaining(const std::string& symbol, 
                                                     uint64_t current_bar_id) const {
    auto it = position_tracking_.find(symbol);
    if (it == position_tracking_.end()) {
        return 0;
    }
    
    uint64_t minimum_exit_bar = it->second.open_bar_id + it->second.horizon;
    if (current_bar_id >= minimum_exit_bar) {
        return 0;
    }
    
    return minimum_exit_bar - current_bar_id;
}

bool sentio::PositionStateMachine::is_in_hold_period(const PortfolioState& portfolio, 
                                                     uint64_t current_bar_id) const {
    for (const auto& [symbol, position] : portfolio.positions) {
        if (position.quantity > 1e-6) {
            if (get_bars_remaining(symbol, current_bar_id) > 0) {
                return true;
            }
        }
    }
    return false;
}

void sentio::PositionStateMachine::update_position_tracking(const SignalOutput& signal, 
                                                            const StateTransition& transition) {
    // Determine which symbols are being traded
    std::vector<std::string> symbols;
    switch (transition.target_state) {
        case State::QQQ_ONLY: symbols = {"QQQ"}; break;
        case State::TQQQ_ONLY: symbols = {"TQQQ"}; break;
        case State::PSQ_ONLY: symbols = {"PSQ"}; break;
        case State::SQQQ_ONLY: symbols = {"SQQQ"}; break;
        case State::QQQ_TQQQ: symbols = {"QQQ", "TQQQ"}; break;
        case State::PSQ_SQQQ: symbols = {"PSQ", "SQQQ"}; break;
        default: break;
    }
    
    // Determine hold period: use metadata override if present, otherwise use prediction_horizon
    int hold_period = signal.prediction_horizon;
    auto it = signal.metadata.find("minimum_hold_bars");
    if (it != signal.metadata.end()) {
        try {
            hold_period = std::stoi(it->second);
            static int debug_count = 0;
            if (debug_count < 3) {
                utils::log_info("PSM: Using minimum_hold_bars=" + std::to_string(hold_period) + 
                               " from metadata (prediction_horizon=" + std::to_string(signal.prediction_horizon) + ")");
                debug_count++;
            }
        } catch (...) {
            // If parsing fails, use default
            utils::log_warning("PSM: Failed to parse minimum_hold_bars, using prediction_horizon=" + 
                             std::to_string(hold_period));
        }
    }
    
    // Record entry for each symbol with custom hold period
    for (const auto& sym : symbols) {
        record_position_entry(sym, signal.bar_id, hold_period, 0.0);
    }
}

// Protected method for derived classes (EnhancedPSM) to get base transition logic
sentio::PositionStateMachine::StateTransition 
sentio::PositionStateMachine::get_base_transition(State current, SignalType signal) const {
    auto key = std::make_pair(current, signal);
    auto it = transition_matrix_.find(key);
    
    if (it != transition_matrix_.end()) {
        return it->second;
    }
    
    // Fallback: return neutral transition (stay in current state)
    StateTransition fallback;
    fallback.current_state = current;
    fallback.signal_type = signal;
    fallback.target_state = current;
    fallback.optimal_action = "No action (unknown state/signal combination)";
    fallback.theoretical_basis = "Fallback transition";
    fallback.expected_return = 0.0;
    fallback.risk_score = 0.0;
    fallback.confidence = 0.0;
    
    return fallback;
}


```

## 📄 **FILE 9 of 14**: src/cli/live_trade_command.cpp

**File Information**:
- **Path**: `src/cli/live_trade_command.cpp`

- **Size**: 788 lines
- **Modified**: 2025-10-08 00:11:35

- **Type**: .cpp

```text
#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>

namespace sentio {
namespace cli {

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config() {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // v1.0 asymmetric thresholds (from optimization)
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;

    // EWRLS parameters
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 960;  // 2 days of 1-min bars

    // Enable BB amplification
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;

    // Adaptive learning
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    return config;
}

/**
 * Live Trading Runner for OnlineEnsemble Strategy v1.0
 *
 * - Trades SPY/SDS/SPXL/SH during regular hours (9:30am - 4:00pm ET)
 * - Uses OnlineEnsemble EWRLS with asymmetric thresholds
 * - Comprehensive logging of all decisions and trades
 */
class LiveTrader {
public:
    LiveTrader(const std::string& alpaca_key,
               const std::string& alpaca_secret,
               const std::string& polygon_url,
               const std::string& polygon_key,
               const std::string& log_dir)
        : alpaca_(alpaca_key, alpaca_secret, true /* paper */)
        , polygon_(polygon_url, polygon_key)
        , log_dir_(log_dir)
        , strategy_(create_v1_config())
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
    {
        // Initialize log files
        init_logs();

        // SPY trading configuration (maps to sentio PSM states)
        symbol_map_ = {
            {"SPY", "SPY"},      // Base 1x
            {"SPXL", "SPXL"},    // Bull 3x
            {"SH", "SH"},        // Bear -1x
            {"SDS", "SDS"}       // Bear -2x
        };
    }

    void run() {
        log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to Alpaca
        log_system("Connecting to Alpaca Paper Trading...");
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to connect to Alpaca");
            return;
        }
        log_system("✓ Connected - Account: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to Polygon
        log_system("Connecting to Polygon proxy...");
        if (!polygon_.connect()) {
            log_error("Failed to connect to Polygon");
            return;
        }
        log_system("✓ Connected to Polygon");

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!polygon_.subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        log_system("Loading warmup data (960 bars = 2 trading days)...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Start main trading loop
        polygon_.start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Keep running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

private:
    AlpacaClient alpaca_;
    PolygonClient polygon_;
    std::string log_dir_;
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        auto timestamp = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + timestamp + ".log");
        log_signals_.open(log_dir_ + "/signals_" + timestamp + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + timestamp + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + timestamp + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + timestamp + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    bool is_regular_hours() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        // Use GMT time and convert to ET (GMT-4 for EDT, GMT-5 for EST)
        // For simplicity, assuming EDT (GMT-4) - adjust for DST if needed
        auto gmt_tm = *std::gmtime(&time_t_now);
        int gmt_hour = gmt_tm.tm_hour;
        int gmt_minute = gmt_tm.tm_min;

        // Convert GMT to ET (EDT = GMT-4)
        int et_hour = gmt_hour - 4;
        if (et_hour < 0) et_hour += 24;

        int time_minutes = et_hour * 60 + gmt_minute;

        // Regular hours: 9:30am - 3:58pm ET
        int market_open = 9 * 60 + 30;   // 9:30am = 570 minutes
        int market_close = 15 * 60 + 58;  // 3:58pm = 958 minutes

        return time_minutes >= market_open && time_minutes < market_close;
    }

    bool is_end_of_day_liquidation_time() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        // Use GMT time and convert to ET (EDT = GMT-4)
        auto gmt_tm = *std::gmtime(&time_t_now);
        int gmt_hour = gmt_tm.tm_hour;
        int gmt_minute = gmt_tm.tm_min;

        // Convert GMT to ET (EDT = GMT-4)
        int et_hour = gmt_hour - 4;
        if (et_hour < 0) et_hour += 24;

        int time_minutes = et_hour * 60 + gmt_minute;

        // Liquidate at 3:58pm ET (2 minutes before close)
        int liquidation_time = 15 * 60 + 58;  // 3:58pm = 958 minutes

        return time_minutes == liquidation_time;
    }

    void log_system(const std::string& message) {
        auto timestamp = get_timestamp_readable();
        std::cout << "[" << timestamp << "] " << message << std::endl;
        log_system_ << "[" << timestamp << "] " << message << std::endl;
        log_system_.flush();
    }

    void log_error(const std::string& message) {
        log_system("ERROR: " + message);
    }

    void warmup_strategy() {
        // Load recent historical data from CSV for warmup (last 960 bars = 2 trading days)
        std::string warmup_file = "data/equities/SPY_RTH_NH.csv";

        // Try relative path first, then from parent directory
        std::ifstream file(warmup_file);
        if (!file.is_open()) {
            warmup_file = "../data/equities/SPY_RTH_NH.csv";
            file.open(warmup_file);
        }

        if (!file.is_open()) {
            log_system("WARNING: Could not open warmup file: data/equities/SPY_RTH_NH.csv");
            log_system("         Strategy will learn from first few live bars");
            return;
        }

        // Read all bars
        std::vector<Bar> all_bars;
        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string ts_utc, ts_epoch_str, open_str, high_str, low_str, close_str, volume_str;

            if (std::getline(iss, ts_utc, ',') &&
                std::getline(iss, ts_epoch_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_epoch_str) * 1000LL; // Convert sec to ms
                bar.open = std::stod(open_str);
                bar.high = std::stod(high_str);
                bar.low = std::stod(low_str);
                bar.close = std::stod(close_str);
                bar.volume = std::stoll(volume_str);
                all_bars.push_back(bar);
            }
        }
        file.close();

        if (all_bars.empty()) {
            log_system("WARNING: No bars loaded from warmup file");
            return;
        }

        // Take last 960 bars
        size_t warmup_count = std::min(size_t(960), all_bars.size());
        size_t start_idx = all_bars.size() - warmup_count;

        log_system("Warming up with " + std::to_string(warmup_count) + " historical bars...");

        int predictor_training_count = 0;
        for (size_t i = start_idx; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]); // Feed bar to strategy (increments sample counter)
            generate_signal(all_bars[i]); // Generate signal (after features are ready)

            // Train predictor on bar-to-bar returns (after feature engine is ready at bar 64)
            if (i > start_idx + 64 && i + 1 < all_bars.size()) {
                // Extract features for bar i (current bar)
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    // Calculate realized return from bar i to bar i+1
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }
        }

        log_system("✓ Warmup complete - processed " + std::to_string(warmup_count) + " bars");
        log_system("  Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("  Predictor trained on " + std::to_string(predictor_training_count) + " historical returns");
    }

    void on_new_bar(const Bar& bar) {
        auto timestamp = get_timestamp_readable();

        // Check for end-of-day liquidation (3:58pm ET)
        if (is_end_of_day_liquidation_time()) {
            log_system("[" + timestamp + "] END OF DAY - Liquidating all positions");
            liquidate_all_positions();
            return;
        }

        // Check if we should be trading
        if (!is_regular_hours()) {
            log_system("[" + timestamp + "] Outside regular hours - skipping");
            return;
        }

        log_system("[" + timestamp + "] New bar received - processing...");

        // Feed bar to strategy FIRST (updates features, increments counter)
        strategy_.on_bar(bar);

        // Generate signal using OnlineEnsemble strategy (now features are ready)
        auto signal = generate_signal(bar);

        // Log signal to console (for monitoring)
        std::cout << "[" << timestamp << "] Signal: "
                  << signal.prediction << " "
                  << "(p=" << std::fixed << std::setprecision(4) << signal.probability
                  << ", conf=" << std::setprecision(2) << signal.confidence << ")"
                  << " [DBG: ready=" << (strategy_.is_ready() ? "YES" : "NO") << "]"
                  << std::endl;

        // Log signal
        log_signal(bar, signal);

        // Make trading decision
        auto decision = make_decision(signal, bar);

        // Log decision
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
        }

        // Log current portfolio state
        log_portfolio_state();
    }

    struct Signal {
        double probability;
        double confidence;
        std::string prediction;  // "LONG", "SHORT", "NEUTRAL"
        double prob_1bar;
        double prob_5bar;
        double prob_10bar;
    };

    Signal generate_signal(const Bar& bar) {
        // Call OnlineEnsemble strategy to generate real signal
        auto strategy_signal = strategy_.generate_signal(bar);

        // DEBUG: Check why we're getting 0.5
        if (strategy_signal.probability == 0.5) {
            std::string reason = "unknown";
            if (strategy_signal.metadata.count("skip_reason")) {
                reason = strategy_signal.metadata.at("skip_reason");
            }
            std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
        }

        Signal signal;
        signal.probability = strategy_signal.probability;

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Set confidence based on distance from neutral (0.5)
        signal.confidence = std::abs(strategy_signal.probability - 0.5) * 2.0;

        // Use same probability for all horizons (OnlineEnsemble provides single probability)
        signal.prob_1bar = strategy_signal.probability;
        signal.prob_5bar = strategy_signal.probability;
        signal.prob_10bar = strategy_signal.probability;

        return signal;
    }

    struct Decision {
        bool should_trade;
        PositionStateMachine::State target_state;
        std::string reason;
        double current_equity;
        double position_pnl_pct;
        bool profit_target_hit;
        bool stop_loss_hit;
        bool min_hold_violated;
    };

    Decision make_decision(const Signal& signal, const Bar& bar) {
        Decision decision;
        decision.should_trade = false;

        // Get current portfolio state
        auto account = alpaca_.get_account();
        if (!account) {
            decision.reason = "Failed to get account info";
            return decision;
        }

        decision.current_equity = account->portfolio_value;
        decision.position_pnl_pct = (decision.current_equity - entry_equity_) / entry_equity_;

        // Check profit target / stop loss
        decision.profit_target_hit = (decision.position_pnl_pct >= PROFIT_TARGET &&
                                      current_state_ != PositionStateMachine::State::CASH_ONLY);
        decision.stop_loss_hit = (decision.position_pnl_pct <= STOP_LOSS &&
                                  current_state_ != PositionStateMachine::State::CASH_ONLY);

        // Check minimum hold period
        decision.min_hold_violated = (bars_held_ < MIN_HOLD_BARS);

        // Force exit to cash if profit/stop hit
        if (decision.profit_target_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "PROFIT_TARGET (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        if (decision.stop_loss_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "STOP_LOSS (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        // Map signal probability to PSM state (v1.0 asymmetric thresholds)
        PositionStateMachine::State target_state;

        if (signal.probability >= 0.68) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Maps to SPXL
        } else if (signal.probability >= 0.60) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Mixed
        } else if (signal.probability >= 0.55) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Maps to SPY
        } else if (signal.probability >= 0.49) {
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= 0.45) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Maps to SH
        } else if (signal.probability >= 0.35) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Mixed
        } else if (signal.probability < 0.32) {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Maps to SDS
        } else {
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        decision.target_state = target_state;

        // Check if state transition needed
        if (target_state != current_state_) {
            // Check minimum hold period
            if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
                decision.should_trade = false;
                decision.reason = "MIN_HOLD_PERIOD (held " + std::to_string(bars_held_) + " bars)";
            } else {
                decision.should_trade = true;
                decision.reason = "STATE_TRANSITION (prob=" + std::to_string(signal.probability) + ")";
            }
        } else {
            decision.should_trade = false;
            decision.reason = "NO_CHANGE";
        }

        return decision;
    }

    void liquidate_all_positions() {
        log_system("Closing all positions for end of day...");

        if (alpaca_.close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = alpaca_.get_account();
            if (account) {
                log_system("Final portfolio value: $" + std::to_string(account->portfolio_value));
                entry_equity_ = account->portfolio_value;
            }
        } else {
            log_error("Failed to close all positions");
        }

        log_portfolio_state();
    }

    void execute_transition(const Decision& decision) {
        log_system("Executing transition: " + psm_.state_to_string(current_state_) +
                   " → " + psm_.state_to_string(decision.target_state));
        log_system("Reason: " + decision.reason);

        // Step 1: Close all current positions
        log_system("Step 1: Closing current positions...");
        if (!alpaca_.close_all_positions()) {
            log_error("Failed to close positions - aborting transition");
            return;
        }
        log_system("✓ All positions closed");

        // Wait a moment for orders to settle
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Step 2: Get current account info
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        log_system("Available capital: $" + std::to_string(available_capital));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("Step 2: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  Buying " + std::to_string(quantity) + " shares of " + symbol);

                    auto order = alpaca_.place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order placed: " + order->order_id + " (" + order->status + ")");
                        log_trade(*order);
                    } else {
                        log_error("  ✗ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("Target state is CASH_ONLY - no positions to open");
        }

        // Update state
        current_state_ = decision.target_state;
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        log_system("✓ Transition complete");
    }

    // Calculate position allocations based on PSM state
    std::map<std::string, double> calculate_target_allocations(
        PositionStateMachine::State state, double capital) {

        std::map<std::string, double> allocations;

        // Map PSM states to SPY instrument allocations
        switch (state) {
            case PositionStateMachine::State::TQQQ_ONLY:
                // 3x bull → SPXL only
                allocations["SPXL"] = capital;
                break;

            case PositionStateMachine::State::QQQ_TQQQ:
                // Blended long → SPY (50%) + SPXL (50%)
                allocations["SPY"] = capital * 0.5;
                allocations["SPXL"] = capital * 0.5;
                break;

            case PositionStateMachine::State::QQQ_ONLY:
                // 1x base → SPY only
                allocations["SPY"] = capital;
                break;

            case PositionStateMachine::State::CASH_ONLY:
                // No positions
                break;

            case PositionStateMachine::State::PSQ_ONLY:
                // -1x bear → SH only
                allocations["SH"] = capital;
                break;

            case PositionStateMachine::State::PSQ_SQQQ:
                // Blended short → SH (50%) + SDS (50%)
                allocations["SH"] = capital * 0.5;
                allocations["SDS"] = capital * 0.5;
                break;

            case PositionStateMachine::State::SQQQ_ONLY:
                // -2x bear → SDS only
                allocations["SDS"] = capital;
                break;

            default:
                break;
        }

        // Convert dollar allocations to share quantities
        std::map<std::string, double> quantities;
        for (const auto& [symbol, dollar_amount] : allocations) {
            // Get current price from recent bars
            auto bars = polygon_.get_recent_bars(symbol, 1);
            if (!bars.empty() && bars[0].close > 0) {
                double shares = std::floor(dollar_amount / bars[0].close);
                if (shares > 0) {
                    quantities[symbol] = shares;
                }
            }
        }

        return quantities;
    }

    void log_trade(const AlpacaClient::Order& order) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;

        log_trades_ << j.dump() << std::endl;
        log_trades_.flush();
    }

    void log_signal(const Bar& bar, const Signal& signal) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_timestamp_ms"] = bar.timestamp_ms;
        j["probability"] = signal.probability;
        j["confidence"] = signal.confidence;
        j["prediction"] = signal.prediction;
        j["prob_1bar"] = signal.prob_1bar;
        j["prob_5bar"] = signal.prob_5bar;
        j["prob_10bar"] = signal.prob_10bar;

        log_signals_ << j.dump() << std::endl;
        log_signals_.flush();
    }

    void log_decision(const Decision& decision) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["should_trade"] = decision.should_trade;
        j["current_state"] = psm_.state_to_string(current_state_);
        j["target_state"] = psm_.state_to_string(decision.target_state);
        j["reason"] = decision.reason;
        j["current_equity"] = decision.current_equity;
        j["position_pnl_pct"] = decision.position_pnl_pct;
        j["bars_held"] = bars_held_;

        log_decisions_ << j.dump() << std::endl;
        log_decisions_.flush();
    }

    void log_portfolio_state() {
        auto account = alpaca_.get_account();
        if (!account) return;

        auto positions = alpaca_.get_positions();

        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["cash"] = account->cash;
        j["buying_power"] = account->buying_power;
        j["portfolio_value"] = account->portfolio_value;
        j["equity"] = account->equity;
        j["total_return"] = account->portfolio_value - 100000.0;
        j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;

        nlohmann::json positions_json = nlohmann::json::array();
        for (const auto& pos : positions) {
            nlohmann::json p;
            p["symbol"] = pos.symbol;
            p["quantity"] = pos.quantity;
            p["avg_entry_price"] = pos.avg_entry_price;
            p["current_price"] = pos.current_price;
            p["market_value"] = pos.market_value;
            p["unrealized_pl"] = pos.unrealized_pl;
            p["unrealized_pl_pct"] = pos.unrealized_pl_pct;
            positions_json.push_back(p);
        }
        j["positions"] = positions_json;

        log_positions_ << j.dump() << std::endl;
        log_positions_.flush();
    }
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // PAPER TRADING CREDENTIALS (your new paper account)
    // Use these for paper trading validation before switching to live
    const std::string ALPACA_KEY = "PK3NCBT07OJZJULDJR5V";
    const std::string ALPACA_SECRET = "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

    // Polygon API key from config.env
    const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
    // Use Alpaca's IEX WebSocket for real-time market data (FREE tier)
    // IEX provides ~3% of market volume, sufficient for paper trading
    // Upgrade to SIP ($49/mo) for 100% market coverage in production
    const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";

    // Polygon key (kept for future use, currently using Alpaca IEX)
    const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

    // NOTE: Only switch to LIVE credentials after paper trading success is confirmed!
    // LIVE credentials are in config.env (ALPACA_LIVE_API_KEY / ALPACA_LIVE_SECRET_KEY)

    // Log directory
    std::string log_dir = "logs/live_trading";

    LiveTrader trader(ALPACA_KEY, ALPACA_SECRET, ALPACA_MARKET_DATA_URL, POLYGON_KEY, log_dir);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade\n\n";
    std::cout << "Run OnlineTrader v1.0 with paper account\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Data: Real-time via Alpaca (upgradeable to Polygon WebSocket)\n";
    std::cout << "  Account: Paper trading (PK3NCBT07OJZJULDJR5V)\n\n";
    std::cout << "Logs: logs/live_trading/\n";
    std::cout << "  - system_*.log: Human-readable events\n";
    std::cout << "  - signals_*.jsonl: Every prediction\n";
    std::cout << "  - decisions_*.jsonl: Trading decisions\n";
    std::cout << "  - trades_*.jsonl: Order executions\n";
    std::cout << "  - positions_*.jsonl: Portfolio snapshots\n\n";
    std::cout << "Example:\n";
    std::cout << "  sentio_cli live-trade\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 10 of 14**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`

- **Size**: 680 lines
- **Modified**: 2025-10-08 00:06:48

- **Type**: .cpp

```text
#include "features/unified_feature_engine.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace sentio {
namespace features {

// IncrementalSMA implementation
IncrementalSMA::IncrementalSMA(int period) : period_(period) {
    // Note: std::deque doesn't have reserve(), but that's okay
}

double IncrementalSMA::update(double value) {
    if (values_.size() >= period_) {
        sum_ -= values_.front();
        values_.pop_front();
    }
    
    values_.push_back(value);
    sum_ += value;
    
    return get_value();
}

// IncrementalEMA implementation
IncrementalEMA::IncrementalEMA(int period, double alpha) {
    if (alpha < 0) {
        alpha_ = 2.0 / (period + 1.0);  // Standard EMA alpha
    } else {
        alpha_ = alpha;
    }
}

double IncrementalEMA::update(double value) {
    if (!initialized_) {
        ema_value_ = value;
        initialized_ = true;
    } else {
        ema_value_ = alpha_ * value + (1.0 - alpha_) * ema_value_;
    }
    return ema_value_;
}

// IncrementalRSI implementation
IncrementalRSI::IncrementalRSI(int period) 
    : gain_sma_(period), loss_sma_(period) {
}

double IncrementalRSI::update(double price) {
    if (first_update_) {
        prev_price_ = price;
        first_update_ = false;
        return 50.0;  // Neutral RSI
    }
    
    double change = price - prev_price_;
    double gain = std::max(0.0, change);
    double loss = std::max(0.0, -change);
    
    gain_sma_.update(gain);
    loss_sma_.update(loss);
    
    prev_price_ = price;
    
    return get_value();
}

double IncrementalRSI::get_value() const {
    if (!is_ready()) return 50.0;
    
    double avg_gain = gain_sma_.get_value();
    double avg_loss = loss_sma_.get_value();
    
    if (avg_loss == 0.0) return 100.0;
    
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// UnifiedFeatureEngine implementation
UnifiedFeatureEngine::UnifiedFeatureEngine(const Config& config) : config_(config) {
    initialize_calculators();
    cached_features_.resize(config_.total_features(), 0.0);
}

void UnifiedFeatureEngine::initialize_calculators() {
    // Initialize SMA calculators for various periods
    std::vector<int> sma_periods = {5, 10, 20, 50, 100, 200};
    for (int period : sma_periods) {
        sma_calculators_["sma_" + std::to_string(period)] = 
            std::make_unique<IncrementalSMA>(period);
    }
    
    // Initialize EMA calculators
    std::vector<int> ema_periods = {5, 10, 20, 50};
    for (int period : ema_periods) {
        ema_calculators_["ema_" + std::to_string(period)] = 
            std::make_unique<IncrementalEMA>(period);
    }
    
    // Initialize RSI calculators
    std::vector<int> rsi_periods = {14, 21};
    for (int period : rsi_periods) {
        rsi_calculators_["rsi_" + std::to_string(period)] = 
            std::make_unique<IncrementalRSI>(period);
    }
}

void UnifiedFeatureEngine::update(const Bar& bar) {
    // DEBUG: Track bar history size WITH INSTANCE POINTER
    size_t size_before = bar_history_.size();

    // Add to history
    bar_history_.push_back(bar);

    size_t size_after_push = bar_history_.size();

    // Maintain max history size
    if (bar_history_.size() > config_.max_history_size) {
        bar_history_.pop_front();
    }

    size_t size_final = bar_history_.size();

    // DEBUG: Log size changes (every 100 bars to avoid spam)
    static int update_count = 0;
    update_count++;
    if (update_count <= 10 || update_count % 100 == 0 || size_final < 70) {
        std::cout << "[UFE@" << (void*)this << "] update #" << update_count
                  << ": before=" << size_before
                  << " → after_push=" << size_after_push
                  << " → final=" << size_final
                  << " (max=" << config_.max_history_size
                  << ", ready=" << (size_final >= 64 ? "YES" : "NO") << ")"
                  << std::endl;
    }

    // Update returns
    update_returns(bar);

    // Update incremental calculators
    update_calculators(bar);

    // Invalidate cache
    invalidate_cache();
}

void UnifiedFeatureEngine::update_returns(const Bar& bar) {
    if (!bar_history_.empty() && bar_history_.size() > 1) {
        double prev_close = bar_history_[bar_history_.size() - 2].close;
        double current_close = bar.close;
        
        double return_val = (current_close - prev_close) / prev_close;
        double log_return = std::log(current_close / prev_close);
        
        returns_.push_back(return_val);
        log_returns_.push_back(log_return);
        
        // Maintain max size
        if (returns_.size() > config_.max_history_size) {
            returns_.pop_front();
            log_returns_.pop_front();
        }
    }
}

void UnifiedFeatureEngine::update_calculators(const Bar& bar) {
    double close = bar.close;
    
    // Update SMA calculators
    for (auto& [name, calc] : sma_calculators_) {
        calc->update(close);
    }
    
    // Update EMA calculators
    for (auto& [name, calc] : ema_calculators_) {
        calc->update(close);
    }
    
    // Update RSI calculators
    for (auto& [name, calc] : rsi_calculators_) {
        calc->update(close);
    }
}

void UnifiedFeatureEngine::invalidate_cache() {
    cache_valid_ = false;
}

std::vector<double> UnifiedFeatureEngine::get_features() const {
    if (cache_valid_ && config_.enable_caching) {
        return cached_features_;
    }
    
    std::vector<double> features;
    features.reserve(config_.total_features());
    
    if (bar_history_.empty()) {
        // Return zeros if no data
        features.resize(config_.total_features(), 0.0);
        return features;
    }
    
    const Bar& current_bar = bar_history_.back();
    
    // 1. Time features (8)
    if (config_.enable_time_features) {
        auto time_features = calculate_time_features(current_bar);
        features.insert(features.end(), time_features.begin(), time_features.end());
    }
    
    // 2. Price action features (12)
    if (config_.enable_price_action) {
        auto price_features = calculate_price_action_features();
        features.insert(features.end(), price_features.begin(), price_features.end());
    }
    
    // 3. Moving average features (16)
    if (config_.enable_moving_averages) {
        auto ma_features = calculate_moving_average_features();
        features.insert(features.end(), ma_features.begin(), ma_features.end());
    }
    
    // 4. Momentum features (20)
    if (config_.enable_momentum) {
        auto momentum_features = calculate_momentum_features();
        features.insert(features.end(), momentum_features.begin(), momentum_features.end());
    }
    
    // 5. Volatility features (15)
    if (config_.enable_volatility) {
        auto vol_features = calculate_volatility_features();
        features.insert(features.end(), vol_features.begin(), vol_features.end());
    }
    
    // 6. Volume features (12)
    if (config_.enable_volume) {
        auto volume_features = calculate_volume_features();
        features.insert(features.end(), volume_features.begin(), volume_features.end());
    }
    
    // 7. Statistical features (10)
    if (config_.enable_statistical) {
        auto stat_features = calculate_statistical_features();
        features.insert(features.end(), stat_features.begin(), stat_features.end());
    }
    
    // 8. Pattern features (8)
    if (config_.enable_pattern_detection) {
        auto pattern_features = calculate_pattern_features();
        features.insert(features.end(), pattern_features.begin(), pattern_features.end());
    }
    
    // 9. Price lag features (10)
    if (config_.enable_price_lags) {
        auto price_lag_features = calculate_price_lag_features();
        features.insert(features.end(), price_lag_features.begin(), price_lag_features.end());
    }
    
    // 10. Return lag features (15)
    if (config_.enable_return_lags) {
        auto return_lag_features = calculate_return_lag_features();
        features.insert(features.end(), return_lag_features.begin(), return_lag_features.end());
    }
    
    // Ensure we have the right number of features
    features.resize(config_.total_features(), 0.0);
    
    // Cache results
    if (config_.enable_caching) {
        cached_features_ = features;
        cache_valid_ = true;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_time_features(const Bar& bar) const {
    std::vector<double> features(config_.time_features, 0.0);
    
    // Convert timestamp to time components
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);
    
    if (time_info) {
        // Cyclical encoding of time components
        double hour = time_info->tm_hour;
        double minute = time_info->tm_min;
        double day_of_week = time_info->tm_wday;
        double day_of_month = time_info->tm_mday;
        
        // Hour cyclical encoding (24 hours)
        features[0] = std::sin(2 * M_PI * hour / 24.0);
        features[1] = std::cos(2 * M_PI * hour / 24.0);
        
        // Minute cyclical encoding (60 minutes)
        features[2] = std::sin(2 * M_PI * minute / 60.0);
        features[3] = std::cos(2 * M_PI * minute / 60.0);
        
        // Day of week cyclical encoding (7 days)
        features[4] = std::sin(2 * M_PI * day_of_week / 7.0);
        features[5] = std::cos(2 * M_PI * day_of_week / 7.0);
        
        // Day of month cyclical encoding (31 days)
        features[6] = std::sin(2 * M_PI * day_of_month / 31.0);
        features[7] = std::cos(2 * M_PI * day_of_month / 31.0);
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_action_features() const {
    std::vector<double> features(config_.price_action_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    const Bar& previous = bar_history_[bar_history_.size() - 2];
    
    // Basic OHLC ratios
    features[0] = safe_divide(current.high - current.low, current.close);  // Range/Close
    features[1] = safe_divide(current.close - current.open, current.close);  // Body/Close
    features[2] = safe_divide(current.high - std::max(current.open, current.close), current.close);  // Upper shadow
    features[3] = safe_divide(std::min(current.open, current.close) - current.low, current.close);  // Lower shadow
    
    // Gap analysis
    features[4] = safe_divide(current.open - previous.close, previous.close);  // Gap
    
    // Price position within range
    features[5] = safe_divide(current.close - current.low, current.high - current.low);  // Close position
    
    // Volatility measures
    features[6] = safe_divide(current.high - current.low, previous.close);  // True range ratio
    
    // Price momentum
    features[7] = safe_divide(current.close - previous.close, previous.close);  // Return
    features[8] = safe_divide(current.high - previous.high, previous.high);  // High momentum
    features[9] = safe_divide(current.low - previous.low, previous.low);  // Low momentum
    
    // Volume-price relationship
    features[10] = safe_divide(current.volume, previous.volume + 1.0) - 1.0;  // Volume change
    features[11] = safe_divide(current.volume * std::abs(current.close - current.open), current.close);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_moving_average_features() const {
    std::vector<double> features(config_.moving_average_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    int idx = 0;
    
    // SMA ratios
    for (const auto& [name, calc] : sma_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    // EMA ratios
    for (const auto& [name, calc] : ema_calculators_) {
        if (idx >= config_.moving_average_features) break;
        if (calc->is_ready()) {
            features[idx] = safe_divide(current_price, calc->get_value()) - 1.0;
        }
        idx++;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_momentum_features() const {
    std::vector<double> features(config_.momentum_features, 0.0);
    
    int idx = 0;
    
    // RSI features
    for (const auto& [name, calc] : rsi_calculators_) {
        if (idx >= config_.momentum_features) break;
        if (calc->is_ready()) {
            features[idx] = calc->get_value() / 100.0;  // Normalize to [0,1]
        }
        idx++;
    }
    
    // Simple momentum features
    if (bar_history_.size() >= 10 && idx < config_.momentum_features) {
        double current = bar_history_.back().close;
        double past_5 = bar_history_[bar_history_.size() - 6].close;
        double past_10 = bar_history_[bar_history_.size() - 11].close;
        
        features[idx++] = safe_divide(current - past_5, past_5);  // 5-period momentum
        features[idx++] = safe_divide(current - past_10, past_10);  // 10-period momentum
    }
    
    // Rate of change features
    if (returns_.size() >= 5 && idx < config_.momentum_features) {
        // Recent return statistics
        auto recent_returns = std::vector<double>(returns_.end() - 5, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 5.0;
        features[idx++] = mean_return;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volatility_features() const {
    std::vector<double> features(config_.volatility_features, 0.0);
    
    if (bar_history_.size() < 20) return features;
    
    // ATR-based features
    double atr_14 = calculate_atr(14);
    double atr_20 = calculate_atr(20);
    double current_price = bar_history_.back().close;
    
    features[0] = safe_divide(atr_14, current_price);  // ATR ratio
    features[1] = safe_divide(atr_20, current_price);  // ATR 20 ratio
    
    // Return volatility
    if (returns_.size() >= 20) {
        auto recent_returns = std::vector<double>(returns_.end() - 20, returns_.end());
        double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / 20.0;
        
        double variance = 0.0;
        for (double ret : recent_returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= 19.0;  // Sample variance
        
        features[2] = std::sqrt(variance);  // Return volatility
        features[3] = std::sqrt(variance * 252);  // Annualized volatility
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_volume_features() const {
    std::vector<double> features(config_.volume_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Volume ratios and trends
    if (bar_history_.size() >= 20) {
        // Calculate average volume over last 20 periods
        double avg_volume = 0.0;
        for (size_t i = bar_history_.size() - 20; i < bar_history_.size(); ++i) {
            avg_volume += bar_history_[i].volume;
        }
        avg_volume /= 20.0;
        
        features[0] = safe_divide(current.volume, avg_volume) - 1.0;  // Volume ratio
    }
    
    // Volume-price correlation
    features[1] = current.volume * std::abs(current.close - current.open);  // Volume intensity
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_statistical_features() const {
    std::vector<double> features(config_.statistical_features, 0.0);
    
    if (returns_.size() < 20) return features;
    
    // Return distribution statistics
    auto recent_returns = std::vector<double>(returns_.end() - std::min(20UL, returns_.size()), returns_.end());
    
    // Skewness approximation
    double mean_return = std::accumulate(recent_returns.begin(), recent_returns.end(), 0.0) / recent_returns.size();
    double variance = 0.0;
    double skew_sum = 0.0;
    
    for (double ret : recent_returns) {
        double diff = ret - mean_return;
        variance += diff * diff;
        skew_sum += diff * diff * diff;
    }
    
    variance /= (recent_returns.size() - 1);
    double std_dev = std::sqrt(variance);
    
    if (std_dev > 1e-8) {
        features[0] = skew_sum / (recent_returns.size() * std_dev * std_dev * std_dev);  // Skewness
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_pattern_features() const {
    std::vector<double> features(config_.pattern_features, 0.0);
    
    if (bar_history_.size() < 2) return features;
    
    const Bar& current = bar_history_.back();
    
    // Basic candlestick patterns
    features[0] = is_doji(current) ? 1.0 : 0.0;
    features[1] = is_hammer(current) ? 1.0 : 0.0;
    features[2] = is_shooting_star(current) ? 1.0 : 0.0;
    
    if (bar_history_.size() >= 2) {
        features[3] = is_engulfing_bullish(bar_history_.size() - 1) ? 1.0 : 0.0;
        features[4] = is_engulfing_bearish(bar_history_.size() - 1) ? 1.0 : 0.0;
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_price_lag_features() const {
    std::vector<double> features(config_.price_lag_features, 0.0);
    
    if (bar_history_.empty()) return features;
    
    double current_price = bar_history_.back().close;
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(bar_history_.size()) > lag) {
            double lagged_price = bar_history_[bar_history_.size() - 1 - lag].close;
            features[i] = safe_divide(current_price, lagged_price) - 1.0;
        }
    }
    
    return features;
}

std::vector<double> UnifiedFeatureEngine::calculate_return_lag_features() const {
    std::vector<double> features(config_.return_lag_features, 0.0);
    
    std::vector<int> lags = {1, 2, 3, 5, 10, 15, 20, 30, 50, 100, 150, 200, 250, 300, 500};
    
    for (size_t i = 0; i < lags.size() && i < features.size(); ++i) {
        int lag = lags[i];
        if (static_cast<int>(returns_.size()) > lag) {
            features[i] = returns_[returns_.size() - 1 - lag];
        }
    }
    
    return features;
}

// Utility methods
double UnifiedFeatureEngine::safe_divide(double numerator, double denominator, double fallback) const {
    if (std::abs(denominator) < 1e-10) {
        return fallback;
    }
    return numerator / denominator;
}

double UnifiedFeatureEngine::calculate_atr(int period) const {
    if (static_cast<int>(bar_history_.size()) < period + 1) return 0.0;
    
    double atr_sum = 0.0;
    for (int i = 0; i < period; ++i) {
        size_t idx = bar_history_.size() - 1 - i;
        atr_sum += calculate_true_range(idx);
    }
    
    return atr_sum / period;
}

double UnifiedFeatureEngine::calculate_true_range(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return 0.0;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    
    return std::max({tr1, tr2, tr3});
}

// Pattern detection helpers
bool UnifiedFeatureEngine::is_doji(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double range = bar.high - bar.low;
    return range > 0 && (body / range) < 0.1;
}

bool UnifiedFeatureEngine::is_hammer(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return lower_shadow > 2 * body && upper_shadow < body;
}

bool UnifiedFeatureEngine::is_shooting_star(const Bar& bar) const {
    double body = std::abs(bar.close - bar.open);
    double lower_shadow = std::min(bar.open, bar.close) - bar.low;
    double upper_shadow = bar.high - std::max(bar.open, bar.close);
    
    return upper_shadow > 2 * body && lower_shadow < body;
}

bool UnifiedFeatureEngine::is_engulfing_bullish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bearish = previous.close < previous.open;
    bool curr_bullish = current.close > current.open;
    bool engulfs = current.open < previous.close && current.close > previous.open;
    
    return prev_bearish && curr_bullish && engulfs;
}

bool UnifiedFeatureEngine::is_engulfing_bearish(size_t index) const {
    if (index == 0 || index >= bar_history_.size()) return false;
    
    const Bar& current = bar_history_[index];
    const Bar& previous = bar_history_[index - 1];
    
    bool prev_bullish = previous.close > previous.open;
    bool curr_bearish = current.close < current.open;
    bool engulfs = current.open > previous.close && current.close < previous.open;
    
    return prev_bullish && curr_bearish && engulfs;
}

void UnifiedFeatureEngine::reset() {
    bar_history_.clear();
    returns_.clear();
    log_returns_.clear();
    
    // Reset calculators
    initialize_calculators();
    
    invalidate_cache();
}

bool UnifiedFeatureEngine::is_ready() const {
    // Need at least 64 bars to align with FeatureSequenceManager requirement
    // This ensures both feature engine and sequence manager become ready together
    bool ready = bar_history_.size() >= 64;

    // DEBUG: Log first few checks and any checks that fail WITH INSTANCE POINTER
    static int check_count = 0;
    check_count++;
    if (check_count <= 10 || !ready) {
        std::cout << "[UFE@" << (void*)this << "] is_ready() check #" << check_count
                  << ": bar_history_.size()=" << bar_history_.size()
                  << " (need 64) → " << (ready ? "READY" : "NOT_READY")
                  << std::endl;
    }

    return ready;
}

std::vector<std::string> UnifiedFeatureEngine::get_feature_names() const {
    std::vector<std::string> names;
    names.reserve(config_.total_features());
    
    // Time features
    names.insert(names.end(), {"hour_sin", "hour_cos", "minute_sin", "minute_cos", 
                              "dow_sin", "dow_cos", "dom_sin", "dom_cos"});
    
    // Add more feature names as needed...
    // (This is a simplified version for now)
    
    return names;
}

} // namespace features
} // namespace sentio


```

## 📄 **FILE 11 of 14**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`

- **Size**: 340 lines
- **Modified**: 2025-10-07 19:30:09

- **Type**: .cpp

```text
#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>

namespace sentio {
namespace learning {

OnlinePredictor::OnlinePredictor(size_t num_features, const Config& config)
    : config_(config), num_features_(num_features), samples_seen_(0),
      current_lambda_(config.lambda) {
    
    // Initialize parameters to zero
    theta_ = Eigen::VectorXd::Zero(num_features);
    
    // Initialize covariance with high uncertainty
    P_ = Eigen::MatrixXd::Identity(num_features, num_features) * config.initial_variance;
    
    utils::log_info("OnlinePredictor initialized with " + std::to_string(num_features) + 
                   " features, lambda=" + std::to_string(config.lambda));
}

OnlinePredictor::PredictionResult OnlinePredictor::predict(const std::vector<double>& features) {
    PredictionResult result;
    result.is_ready = is_ready();
    
    if (!result.is_ready) {
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }
    
    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Linear prediction
    result.predicted_return = theta_.dot(x);
    
    // Confidence from prediction variance
    double prediction_variance = x.transpose() * P_ * x;
    result.confidence = 1.0 / (1.0 + std::sqrt(prediction_variance));
    
    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();
    
    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;

    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }

    // Use Eigen::Map to avoid copy (zero-copy view of std::vector)
    Eigen::Map<const Eigen::VectorXd> x(features.data(), features.size());

    // Current prediction
    double predicted = theta_.dot(x);
    double error = actual_return - predicted;
    
    // Store error for diagnostics
    recent_errors_.push_back(error);
    if (recent_errors_.size() > HISTORY_SIZE) {
        recent_errors_.pop_front();
    }
    
    // Store direction accuracy
    bool correct_direction = (predicted > 0 && actual_return > 0) || 
                           (predicted < 0 && actual_return < 0);
    recent_directions_.push_back(correct_direction);
    if (recent_directions_.size() > HISTORY_SIZE) {
        recent_directions_.pop_front();
    }
    
    // EWRLS update with regularization
    double lambda_reg = current_lambda_ + config_.regularization;
    
    // Kalman gain
    Eigen::VectorXd Px = P_ * x;
    double denominator = lambda_reg + x.dot(Px);
    
    if (std::abs(denominator) < EPSILON) {
        utils::log_warning("Near-zero denominator in EWRLS update, skipping");
        return;
    }
    
    Eigen::VectorXd k = Px / denominator;

    // Update parameters
    theta_.noalias() += k * error;

    // Update covariance (optimized: reuse Px, avoid k * x.transpose() * P_)
    // P = (P - k * x' * P) / lambda = (P - k * Px') / lambda
    P_.noalias() -= k * Px.transpose();
    P_ /= current_lambda_;
    
    // Ensure numerical stability
    ensure_positive_definite();
    
    // Adapt learning rate if enabled
    if (config_.adaptive_learning && samples_seen_ % 10 == 0) {
        adapt_learning_rate(estimate_volatility());
    }
}

OnlinePredictor::PredictionResult OnlinePredictor::predict_and_update(
    const std::vector<double>& features, double actual_return) {
    
    auto result = predict(features);
    update(features, actual_return);
    return result;
}

void OnlinePredictor::adapt_learning_rate(double market_volatility) {
    // Higher volatility -> faster adaptation (lower lambda)
    // Lower volatility -> slower adaptation (higher lambda)
    
    double baseline_vol = 0.001;  // 0.1% baseline volatility
    double vol_ratio = market_volatility / baseline_vol;
    
    // Map volatility ratio to lambda
    // High vol (ratio=2) -> lambda=0.990
    // Low vol (ratio=0.5) -> lambda=0.999
    double target_lambda = config_.lambda - 0.005 * std::log(vol_ratio);
    target_lambda = std::clamp(target_lambda, config_.min_lambda, config_.max_lambda);
    
    // Smooth transition
    current_lambda_ = 0.9 * current_lambda_ + 0.1 * target_lambda;
    
    utils::log_debug("Adapted lambda: " + std::to_string(current_lambda_) + 
                    " (volatility=" + std::to_string(market_volatility) + ")");
}

bool OnlinePredictor::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Save config
        file.write(reinterpret_cast<const char*>(&config_), sizeof(Config));
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_lambda_), sizeof(double));
        
        // Save theta
        file.write(reinterpret_cast<const char*>(theta_.data()), 
                  sizeof(double) * theta_.size());
        
        // Save P (covariance)
        file.write(reinterpret_cast<const char*>(P_.data()), 
                  sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Saved predictor state to: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlinePredictor::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Load config
        file.read(reinterpret_cast<char*>(&config_), sizeof(Config));
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_lambda_), sizeof(double));
        
        // Load theta
        theta_.resize(num_features_);
        file.read(reinterpret_cast<char*>(theta_.data()), 
                 sizeof(double) * theta_.size());
        
        // Load P
        P_.resize(num_features_, num_features_);
        file.read(reinterpret_cast<char*>(P_.data()), 
                 sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Loaded predictor state from: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

double OnlinePredictor::get_recent_rmse() const {
    if (recent_errors_.empty()) return 0.0;
    
    double sum_sq = 0.0;
    for (double error : recent_errors_) {
        sum_sq += error * error;
    }
    return std::sqrt(sum_sq / recent_errors_.size());
}

double OnlinePredictor::get_directional_accuracy() const {
    if (recent_directions_.empty()) return 0.5;
    
    int correct = std::count(recent_directions_.begin(), recent_directions_.end(), true);
    return static_cast<double>(correct) / recent_directions_.size();
}

std::vector<double> OnlinePredictor::get_feature_importance() const {
    // Feature importance based on parameter magnitude * covariance
    std::vector<double> importance(num_features_);
    
    for (size_t i = 0; i < num_features_; ++i) {
        // Combine parameter magnitude with certainty (inverse variance)
        double param_importance = std::abs(theta_[i]);
        double certainty = 1.0 / (1.0 + std::sqrt(P_(i, i)));
        importance[i] = param_importance * certainty;
    }
    
    // Normalize
    double max_imp = *std::max_element(importance.begin(), importance.end());
    if (max_imp > 0) {
        for (double& imp : importance) {
            imp /= max_imp;
        }
    }
    
    return importance;
}

double OnlinePredictor::estimate_volatility() const {
    if (recent_returns_.size() < 20) return 0.001;  // Default 0.1%
    
    double mean = std::accumulate(recent_returns_.begin(), recent_returns_.end(), 0.0) 
                 / recent_returns_.size();
    
    double sum_sq = 0.0;
    for (double ret : recent_returns_) {
        sum_sq += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sum_sq / recent_returns_.size());
}

void OnlinePredictor::ensure_positive_definite() {
    // Eigenvalue decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);
    Eigen::VectorXd eigenvalues = solver.eigenvalues();
    
    // Ensure all eigenvalues are positive
    bool needs_correction = false;
    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (eigenvalues[i] < EPSILON) {
            eigenvalues[i] = EPSILON;
            needs_correction = true;
        }
    }
    
    if (needs_correction) {
        // Reconstruct with corrected eigenvalues
        P_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        utils::log_debug("Corrected covariance matrix for positive definiteness");
    }
}

// MultiHorizonPredictor Implementation

MultiHorizonPredictor::MultiHorizonPredictor(size_t num_features) 
    : num_features_(num_features) {
}

void MultiHorizonPredictor::add_horizon(int bars, double weight) {
    HorizonConfig config;
    config.horizon_bars = bars;
    config.weight = weight;

    // Adjust learning rate based on horizon
    config.predictor_config.lambda = 0.995 + 0.001 * std::log(bars);
    config.predictor_config.lambda = std::clamp(config.predictor_config.lambda, 0.990, 0.999);

    // Reduce warmup for multi-horizon learning
    // Updates arrive delayed by horizon length, so effective warmup is longer
    config.predictor_config.warmup_samples = 20;

    predictors_.emplace_back(std::make_unique<OnlinePredictor>(num_features_, config.predictor_config));
    configs_.push_back(config);

    utils::log_info("Added predictor for " + std::to_string(bars) + "-bar horizon");
}

OnlinePredictor::PredictionResult MultiHorizonPredictor::predict(const std::vector<double>& features) {
    OnlinePredictor::PredictionResult ensemble_result;
    ensemble_result.predicted_return = 0.0;
    ensemble_result.confidence = 0.0;
    ensemble_result.volatility_estimate = 0.0;
    
    double total_weight = 0.0;
    int ready_count = 0;
    
    for (size_t i = 0; i < predictors_.size(); ++i) {
        auto result = predictors_[i]->predict(features);
        
        if (result.is_ready) {
            double weight = configs_[i].weight * result.confidence;
            ensemble_result.predicted_return += result.predicted_return * weight;
            ensemble_result.confidence += result.confidence * configs_[i].weight;
            ensemble_result.volatility_estimate += result.volatility_estimate * configs_[i].weight;
            total_weight += weight;
            ready_count++;
        }
    }
    
    if (total_weight > 0) {
        ensemble_result.predicted_return /= total_weight;
        ensemble_result.confidence /= configs_.size();
        ensemble_result.volatility_estimate /= configs_.size();
        ensemble_result.is_ready = true;
    }
    
    return ensemble_result;
}

void MultiHorizonPredictor::update(int bars_ago, const std::vector<double>& features, 
                                   double actual_return) {
    // Update the appropriate predictor
    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (configs_[i].horizon_bars == bars_ago) {
            predictors_[i]->update(features, actual_return);
            break;
        }
    }
}

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 12 of 14**: src/live/alpaca_client.cpp

**File Information**:
- **Path**: `src/live/alpaca_client.cpp`

- **Size**: 327 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "live/alpaca_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace sentio {

// Callback for libcurl to capture response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

AlpacaClient::AlpacaClient(const std::string& api_key,
                           const std::string& secret_key,
                           bool paper_trading)
    : api_key_(api_key)
    , secret_key_(secret_key)
{
    if (paper_trading) {
        base_url_ = "https://paper-api.alpaca.markets/v2";
    } else {
        base_url_ = "https://api.alpaca.markets/v2";
    }
}

std::map<std::string, std::string> AlpacaClient::get_headers() {
    return {
        {"APCA-API-KEY-ID", api_key_},
        {"APCA-API-SECRET-KEY", secret_key_},
        {"Content-Type", "application/json"}
    };
}

std::string AlpacaClient::http_get(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP GET failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_post(const std::string& endpoint, const std::string& json_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP POST failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_delete(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP DELETE failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::optional<AlpacaClient::AccountInfo> AlpacaClient::get_account() {
    try {
        std::string response = http_get("/account");
        return parse_account_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting account: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::get_positions() {
    try {
        std::string response = http_get("/positions");
        return parse_positions_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting positions: " << e.what() << std::endl;
        return {};
    }
}

std::optional<AlpacaClient::Position> AlpacaClient::get_position(const std::string& symbol) {
    try {
        std::string response = http_get("/positions/" + symbol);
        return parse_position_json(response);
    } catch (const std::exception& e) {
        // Position not found is not an error
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::place_market_order(const std::string& symbol,
                                                                    double quantity,
                                                                    const std::string& time_in_force) {
    try {
        json order_json;
        order_json["symbol"] = symbol;
        order_json["qty"] = std::abs(quantity);
        order_json["side"] = (quantity > 0) ? "buy" : "sell";
        order_json["type"] = "market";
        order_json["time_in_force"] = time_in_force;

        std::string json_body = order_json.dump();
        std::string response = http_post("/orders", json_body);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error placing order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::close_position(const std::string& symbol) {
    try {
        http_delete("/positions/" + symbol);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing position: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::close_all_positions() {
    try {
        http_delete("/positions");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing all positions: " << e.what() << std::endl;
        return false;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::get_order(const std::string& order_id) {
    try {
        std::string response = http_get("/orders/" + order_id);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::cancel_order(const std::string& order_id) {
    try {
        http_delete("/orders/" + order_id);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error canceling order: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::is_market_open() {
    try {
        std::string response = http_get("/clock");
        json clock = json::parse(response);
        return clock["is_open"].get<bool>();
    } catch (const std::exception& e) {
        std::cerr << "Error checking market status: " << e.what() << std::endl;
        return false;
    }
}

// JSON parsing helpers

std::optional<AlpacaClient::AccountInfo> AlpacaClient::parse_account_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        AccountInfo info;
        info.account_number = j["account_number"].get<std::string>();
        info.buying_power = std::stod(j["buying_power"].get<std::string>());
        info.cash = std::stod(j["cash"].get<std::string>());
        info.portfolio_value = std::stod(j["portfolio_value"].get<std::string>());
        info.equity = std::stod(j["equity"].get<std::string>());
        info.last_equity = std::stod(j["last_equity"].get<std::string>());
        info.pattern_day_trader = j.value("pattern_day_trader", false);
        info.trading_blocked = j.value("trading_blocked", false);
        info.account_blocked = j.value("account_blocked", false);
        return info;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing account JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::parse_positions_json(const std::string& json_str) {
    std::vector<Position> positions;
    try {
        json j = json::parse(json_str);
        for (const auto& item : j) {
            Position pos;
            pos.symbol = item["symbol"].get<std::string>();
            pos.quantity = std::stod(item["qty"].get<std::string>());
            pos.avg_entry_price = std::stod(item["avg_entry_price"].get<std::string>());
            pos.current_price = std::stod(item["current_price"].get<std::string>());
            pos.market_value = std::stod(item["market_value"].get<std::string>());
            pos.unrealized_pl = std::stod(item["unrealized_pl"].get<std::string>());
            pos.unrealized_pl_pct = std::stod(item["unrealized_plpc"].get<std::string>());
            positions.push_back(pos);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing positions JSON: " << e.what() << std::endl;
    }
    return positions;
}

std::optional<AlpacaClient::Position> AlpacaClient::parse_position_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Position pos;
        pos.symbol = j["symbol"].get<std::string>();
        pos.quantity = std::stod(j["qty"].get<std::string>());
        pos.avg_entry_price = std::stod(j["avg_entry_price"].get<std::string>());
        pos.current_price = std::stod(j["current_price"].get<std::string>());
        pos.market_value = std::stod(j["market_value"].get<std::string>());
        pos.unrealized_pl = std::stod(j["unrealized_pl"].get<std::string>());
        pos.unrealized_pl_pct = std::stod(j["unrealized_plpc"].get<std::string>());
        return pos;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing position JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::parse_order_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Order order;
        order.order_id = j["id"].get<std::string>();
        order.symbol = j["symbol"].get<std::string>();
        order.quantity = std::stod(j["qty"].get<std::string>());
        order.side = j["side"].get<std::string>();
        order.type = j["type"].get<std::string>();
        order.time_in_force = j["time_in_force"].get<std::string>();
        order.status = j["status"].get<std::string>();
        order.filled_qty = std::stod(j["filled_qty"].get<std::string>());
        if (!j["filled_avg_price"].is_null()) {
            order.filled_avg_price = std::stod(j["filled_avg_price"].get<std::string>());
        } else {
            order.filled_avg_price = 0.0;
        }
        return order;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing order JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace sentio

```

## 📄 **FILE 13 of 14**: src/live/polygon_websocket.cpp

**File Information**:
- **Path**: `src/live/polygon_websocket.cpp`

- **Size**: 304 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
// Alpaca IEX WebSocket Client - Real-time market data
// URL: wss://stream.data.alpaca.markets/v2/iex
// Docs: https://docs.alpaca.markets/docs/streaming-market-data

#include "live/polygon_client.hpp"
#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

using json = nlohmann::json;

namespace sentio {

// WebSocket callback context
struct ws_context {
    PolygonClient* client;
    PolygonClient::BarCallback callback;
    bool connected;
    std::string buffer;
};

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    ws_context* ctx = (ws_context*)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✓ WebSocket connected to Alpaca IEX" << std::endl;
            ctx->connected = true;

            // Alpaca requires authentication first
            // Auth key format: "KEY|SECRET"
            {
                // Parse API key and secret from auth_key
                std::string auth_key_pair = ctx->client ? "" : "";  // Will be passed properly
                size_t delimiter = auth_key_pair.find('|');
                std::string api_key, api_secret;

                // Get keys from environment
                const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
                const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

                // Use hardcoded keys if environment not set (paper trading)
                api_key = alpaca_key_env ? alpaca_key_env : "PK3NCBT07OJZJULDJR5V";
                api_secret = alpaca_secret_env ? alpaca_secret_env : "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

                // Send authentication message
                json auth;
                auth["action"] = "auth";
                auth["key"] = api_key;
                auth["secret"] = api_secret;
                std::string auth_msg = auth.dump();
                unsigned char auth_buf[LWS_PRE + 1024];
                memcpy(&auth_buf[LWS_PRE], auth_msg.c_str(), auth_msg.length());
                lws_write(wsi, &auth_buf[LWS_PRE], auth_msg.length(), LWS_WRITE_TEXT);
                std::cout << "→ Sent authentication to Alpaca" << std::endl;

                // Subscribe to minute bars for SPY instruments
                // Alpaca format: {"action":"subscribe","bars":["SPY","SPXL","SH","SDS"]}
                json sub;
                sub["action"] = "subscribe";
                sub["bars"] = json::array({"SPY", "SPXL", "SH", "SDS"});
                std::string sub_msg = sub.dump();
                unsigned char sub_buf[LWS_PRE + 512];
                memcpy(&sub_buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
                lws_write(wsi, &sub_buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
                std::cout << "→ Subscribed to bars: SPY, SPXL, SH, SDS" << std::endl;
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            // Accumulate message
            ctx->buffer.append((char*)in, len);

            // Check if message is complete
            if (lws_is_final_fragment(wsi)) {
                try {
                    json j = json::parse(ctx->buffer);

                    // Alpaca sends arrays of messages: [{...}, {...}]
                    if (j.is_array()) {
                        for (const auto& msg : j) {
                            // Control messages (authentication, subscriptions, errors)
                            if (msg.contains("T")) {
                                std::string msg_type = msg["T"];

                                // Success/error messages
                                if (msg_type == "success") {
                                    std::cout << "Alpaca: " << msg.value("msg", "Success") << std::endl;
                                } else if (msg_type == "error") {
                                    std::cerr << "Alpaca Error: " << msg.value("msg", "Unknown error") << std::endl;
                                } else if (msg_type == "subscription") {
                                    std::cout << "Alpaca: Subscriptions confirmed" << std::endl;
                                }

                                // Minute bar message (T="b")
                                else if (msg_type == "b") {
                                    Bar bar;
                                    // Alpaca timestamp format: "2025-10-06T13:30:00Z" (ISO 8601)
                                    // Need to convert to milliseconds since epoch
                                    std::string timestamp_str = msg.value("t", "");
                                    // For now, use current time (will implement proper ISO parsing if needed)
                                    bar.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch()
                                    ).count();

                                    bar.open = msg.value("o", 0.0);
                                    bar.high = msg.value("h", 0.0);
                                    bar.low = msg.value("l", 0.0);
                                    bar.close = msg.value("c", 0.0);
                                    bar.volume = msg.value("v", 0LL);

                                    std::string symbol = msg.value("S", "");

                                    if (bar.close > 0 && !symbol.empty()) {
                                        std::cout << "✓ Bar: " << symbol << " $" << bar.close
                                                  << " (O:" << bar.open << " H:" << bar.high
                                                  << " L:" << bar.low << " V:" << bar.volume << ")" << std::endl;

                                        // Store bar
                                        if (ctx->client) {
                                            ctx->client->store_bar(symbol, bar);
                                        }

                                        // Callback (only for SPY to trigger strategy)
                                        if (ctx->callback && symbol == "SPY") {
                                            ctx->callback(symbol, bar);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
                    std::cerr << "Message was: " << ctx->buffer.substr(0, 200) << std::endl;
                }

                ctx->buffer.clear();
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "❌ WebSocket connection error" << std::endl;
            ctx->connected = false;
            break;

        case LWS_CALLBACK_CLOSED:
            std::cout << "WebSocket connection closed" << std::endl;
            ctx->connected = false;
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "polygon-protocol",
        websocket_callback,
        sizeof(ws_context),
        4096,
    },
    { NULL, NULL, 0, 0 } // terminator
};

PolygonClient::PolygonClient(const std::string& proxy_url, const std::string& auth_key)
    : proxy_url_(proxy_url)
    , auth_key_(auth_key)
    , connected_(false)
    , running_(false)
{
}

PolygonClient::~PolygonClient() {
    stop();
}

bool PolygonClient::connect() {
    std::cout << "Connecting to Polygon WebSocket proxy..." << std::endl;
    std::cout << "URL: " << proxy_url_ << std::endl;
    connected_ = true;  // Will be updated by WebSocket callback
    return true;
}

bool PolygonClient::subscribe(const std::vector<std::string>& symbols) {
    std::cout << "Subscribing to: ";
    for (const auto& s : symbols) std::cout << s << " ";
    std::cout << std::endl;
    return true;
}

void PolygonClient::start(BarCallback callback) {
    if (running_) return;

    running_ = true;
    std::thread ws_thread([this, callback]() {
        receive_loop(callback);
    });
    ws_thread.detach();
}

void PolygonClient::stop() {
    running_ = false;
    connected_ = false;
}

void PolygonClient::receive_loop(BarCallback callback) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info conn_info;
    struct lws_context *context;
    struct lws *wsi;
    ws_context ctx;

    memset(&info, 0, sizeof(info));
    memset(&conn_info, 0, sizeof(conn_info));
    memset(&ctx, 0, sizeof(ctx));

    ctx.client = this;
    ctx.callback = callback;
    ctx.connected = false;

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) {
        std::cerr << "❌ Failed to create WebSocket context" << std::endl;
        return;
    }

    // Connect to Alpaca IEX WebSocket directly
    conn_info.context = context;
    conn_info.address = "stream.data.alpaca.markets";
    conn_info.port = 443;
    conn_info.path = "/v2/iex";
    conn_info.host = conn_info.address;
    conn_info.origin = conn_info.address;
    conn_info.protocol = protocols[0].name;
    conn_info.ssl_connection = LCCSCF_USE_SSL;
    conn_info.userdata = &ctx;

    std::cout << "Connecting to wss://" << conn_info.address << ":" << conn_info.port << conn_info.path << std::endl;

    wsi = lws_client_connect_via_info(&conn_info);
    if (!wsi) {
        std::cerr << "❌ Failed to connect to WebSocket" << std::endl;
        lws_context_destroy(context);
        return;
    }

    // Service loop
    while (running_) {
        lws_service(context, 50);  // 50ms timeout
    }

    lws_context_destroy(context);
    std::cout << "WebSocket loop ended" << std::endl;
}

void PolygonClient::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto& history = bars_history_[symbol];
    history.push_back(bar);

    if (history.size() > MAX_BARS_HISTORY) {
        history.pop_front();
    }
}

std::vector<Bar> PolygonClient::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto it = bars_history_.find(symbol);
    if (it == bars_history_.end()) {
        return {};
    }

    const auto& history = it->second;
    size_t start = (history.size() > count) ? (history.size() - count) : 0;

    std::vector<Bar> result;
    for (size_t i = start; i < history.size(); ++i) {
        result.push_back(history[i]);
    }

    return result;
}

bool PolygonClient::is_connected() const {
    return connected_;
}

} // namespace sentio

```

## 📄 **FILE 14 of 14**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 598 lines
- **Modified**: 2025-10-08 00:11:07

- **Type**: .cpp

```text
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // Initialize feature engine first to get actual feature count
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>();

    // Initialize multi-horizon EWRLS predictor with correct feature count
    size_t num_features = feature_engine_->get_features().size();
    if (num_features == 0) {
        // Feature engine not ready yet, use default config total
        features::UnifiedFeatureEngineConfig default_config;
        num_features = default_config.total_features();  // 126 features
    }
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check volatility filter (skip trading in flat markets)
    if (config_.enable_volatility_filter && !has_sufficient_volatility()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.metadata["skip_reason"] = "insufficient_volatility";
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // DEBUG: Log prediction
    static int signal_count = 0;
    signal_count++;
    if (signal_count <= 10) {
        std::cout << "[OES] generate_signal #" << signal_count
                  << ": predicted_return=" << prediction.predicted_return
                  << ", confidence=" << prediction.confidence
                  << std::endl;
    }

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    if (signal_count <= 10) {
        std::cout << "[OES]   → base_prob=" << base_prob << std::endl;
    }

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    // DEBUG: Track why features might be empty
    static int extract_count = 0;
    extract_count++;

    if (bar_history_.size() < MIN_FEATURES_BARS) {
        if (extract_count <= 10) {
            std::cout << "[OES] extract_features #" << extract_count
                      << ": bar_history_.size()=" << bar_history_.size()
                      << " < MIN_FEATURES_BARS=" << MIN_FEATURES_BARS
                      << " → returning empty"
                      << std::endl;
        }
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_ready()) {
        if (extract_count <= 10) {
            std::cout << "[OES] extract_features #" << extract_count
                      << ": feature_engine NOT ready → returning empty"
                      << std::endl;
        }
        return {};
    }

    auto features = feature_engine_->get_features();
    if (extract_count <= 10 || features.empty()) {
        std::cout << "[OES] extract_features #" << extract_count
                  << ": got " << features.size() << " features from engine"
                  << std::endl;
    }

    return features;
}

void OnlineEnsembleStrategy::train_predictor(const std::vector<double>& features, double realized_return) {
    if (features.empty()) {
        return;  // Nothing to train on
    }

    // Train all horizon predictors with the same realized return
    // (In practice, each horizon would use its own future return, but for warmup we use next-bar return)
    for (int horizon : config_.prediction_horizons) {
        ensemble_predictor_->update(horizon, features, realized_return);
    }
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    // Create shared_ptr only once per bar (reuse for all horizons)
    static std::shared_ptr<const std::vector<double>> shared_features;
    static int last_bar_index = -1;

    if (bar_index != last_bar_index) {
        // New bar - create new shared features
        shared_features = std::make_shared<const std::vector<double>>(features);
        last_bar_index = bar_index;
    }

    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = shared_features;  // Share, don't copy
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    // Use fixed array instead of vector
    auto& update = pending_updates_[pred.target_bar_index];
    if (update.count < 3) {
        update.horizons[update.count++] = std::move(pred);  // Move, don't copy
    }
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& update = it->second;

        // Process only the valid predictions (0 to count-1)
        for (uint8_t i = 0; i < update.count; ++i) {
            const auto& pred = update.horizons[i];

            double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
            if (!pred.is_long) {
                actual_return = -actual_return;
            }

            // Dereference shared_ptr only when needed
            ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
        }

        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed " + std::to_string(static_cast<int>(update.count)) +
                           " updates at bar " + std::to_string(samples_seen_) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

double OnlineEnsembleStrategy::calculate_atr(int period) const {
    if (bar_history_.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    // Calculate True Range for each bar and average
    double sum_tr = 0.0;
    for (size_t i = bar_history_.size() - period; i < bar_history_.size(); ++i) {
        const auto& curr = bar_history_[i];
        const auto& prev = bar_history_[i - 1];

        // True Range = max(high-low, |high-prev_close|, |low-prev_close|)
        double hl = curr.high - curr.low;
        double hc = std::abs(curr.high - prev.close);
        double lc = std::abs(curr.low - prev.close);

        double tr = std::max({hl, hc, lc});
        sum_tr += tr;
    }

    return sum_tr / period;
}

bool OnlineEnsembleStrategy::has_sufficient_volatility() const {
    if (bar_history_.empty()) {
        return false;
    }

    // Calculate ATR
    double atr = calculate_atr(config_.atr_period);

    // Get current price
    double current_price = bar_history_.back().close;

    // Calculate ATR as percentage of price
    double atr_ratio = (current_price > 0) ? (atr / current_price) : 0.0;

    // Check if ATR ratio meets minimum threshold
    return atr_ratio >= config_.min_atr_ratio;
}

} // namespace sentio

```

