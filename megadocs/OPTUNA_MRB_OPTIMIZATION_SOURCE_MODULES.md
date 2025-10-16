# Optuna MRB Optimization - Source Code & Requirements

**Generated**: 2025-10-08 13:35:18
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (10 files)
**Description**: Complete source code modules for Optuna parameter optimization to maximize Mean Return per Block (MRB). Includes OnlineEnsembleStrategy, EWRLS learning, feature engineering, backtest infrastructure, and optimization requirements.

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [include/cli/backtest_command.h](#file-1)
2. [include/features/unified_engine_v2.h](#file-2)
3. [include/learning/online_predictor.h](#file-3)
4. [include/strategy/online_ensemble_strategy.h](#file-4)
5. [megadocs/OPTUNA_MRB_OPTIMIZATION_REQUIREMENTS.md](#file-5)
6. [src/cli/analyze_trades_command.cpp](#file-6)
7. [src/cli/backtest_command.cpp](#file-7)
8. [src/features/unified_engine_v2.cpp](#file-8)
9. [src/learning/online_predictor.cpp](#file-9)
10. [src/strategy/online_ensemble_strategy.cpp](#file-10)

---

## 📄 **FILE 1 of 10**: include/cli/backtest_command.h

**File Information**:
- **Path**: `include/cli/backtest_command.h`

- **Size**: 43 lines
- **Modified**: 2025-10-08 11:48:10

- **Type**: .h

```text
#pragma once

#include "cli/command_interface.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * @brief Backtest command - Run end-to-end backtest on last N blocks
 *
 * Usage:
 *   sentio_cli backtest --blocks 20
 *   sentio_cli backtest --blocks 100 --data custom.csv
 *   sentio_cli backtest --blocks 50 --warmup 200
 *
 * Features:
 * - Extracts last N blocks from binary or CSV data
 * - Integrated pipeline: generate-signals → execute-trades → analyze-trades
 * - Defaults to SPY_5years.bin for fast loading
 * - Auto-detects CSV vs BIN format
 * - No temp files created
 */
class BacktestCommand : public Command {
public:
    BacktestCommand() = default;
    virtual ~BacktestCommand() = default;

    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "backtest"; }
    std::string get_description() const override {
        return "Run end-to-end backtest on last N blocks of data";
    }
    void show_help() const override;

private:
    static constexpr int BARS_PER_BLOCK = 480;
    static constexpr const char* DEFAULT_DATA_PATH = "data/equities/SPY_5years.bin";
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 2 of 10**: include/features/unified_engine_v2.h

**File Information**:
- **Path**: `include/features/unified_engine_v2.h`

- **Size**: 222 lines
- **Modified**: 2025-10-08 11:16:25

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include "features/indicators.h"
#include "features/scaler.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace features {

// =============================================================================
// Configuration for Production-Grade Unified Feature Engine
// =============================================================================

struct EngineConfig {
    // Feature toggles
    bool momentum = true;
    bool volatility = true;
    bool volume = true;
    bool statistics = true;
    bool patterns = false;

    // Indicator periods
    int rsi14 = 14;
    int rsi21 = 21;
    int atr14 = 14;
    int bb20 = 20;
    int bb_k = 2;
    int stoch14 = 14;
    int will14 = 14;
    int macd_fast = 12;
    int macd_slow = 26;
    int macd_sig = 9;
    int roc5 = 5;
    int roc10 = 10;
    int roc20 = 20;
    int cci20 = 20;
    int don20 = 20;
    int keltner_ema = 20;
    int keltner_atr = 10;
    double keltner_mult = 2.0;

    // Moving averages
    int sma10 = 10;
    int sma20 = 20;
    int sma50 = 50;
    int ema10 = 10;
    int ema20 = 20;
    int ema50 = 50;

    // Normalization
    bool normalize = true;
    bool robust = false;
};

// =============================================================================
// Feature Schema with Hash for Model Compatibility
// =============================================================================

struct Schema {
    std::vector<std::string> names;
    std::string sha1_hash;  // Hash of (names + config) for version control
};

// =============================================================================
// Production-Grade Unified Feature Engine
//
// Key Features:
// - Stable, deterministic feature ordering (std::map, not unordered_map)
// - O(1) incremental updates using Welford's algorithm and ring buffers
// - Schema hash for model compatibility checks
// - Complete public API: update(), features_view(), names(), schema()
// - Serialization/restoration for online learning
// - Zero duplicate calculations (shared statistics cache)
// =============================================================================

class UnifiedFeatureEngineV2 {
public:
    explicit UnifiedFeatureEngineV2(EngineConfig cfg = {});

    // ==========================================================================
    // Core API
    // ==========================================================================

    /**
     * Idempotent update with new bar. Returns true if state advanced.
     */
    bool update(const Bar& b);

    /**
     * Get contiguous feature vector in stable order (ready for model input).
     * Values may contain NaN until warmup complete for each feature.
     */
    const std::vector<double>& features_view() const { return feats_; }

    /**
     * Get canonical feature names in fixed, deterministic order.
     */
    const std::vector<std::string>& names() const { return schema_.names; }

    /**
     * Get schema with hash for model compatibility checks.
     */
    const Schema& schema() const { return schema_; }

    /**
     * Count of bars remaining before all features are non-NaN.
     */
    int warmup_remaining() const;

    /**
     * Reset engine to initial state.
     */
    void reset();

    /**
     * Serialize engine state for persistence (online learning resume).
     */
    std::string serialize() const;

    /**
     * Restore engine state from serialized blob.
     */
    void restore(const std::string& blob);

    /**
     * Check if engine has processed at least one bar.
     */
    bool is_seeded() const { return seeded_; }

    /**
     * Get number of bars processed.
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Get normalization scaler (for external persistence).
     */
    const Scaler& get_scaler() const { return scaler_; }

    /**
     * Set scaler from external source (for trained models).
     */
    void set_scaler(const Scaler& s) { scaler_ = s; }

private:
    void build_schema_();
    void recompute_vector_();
    std::string compute_schema_hash_(const std::string& concatenated_names);

    EngineConfig cfg_;
    Schema schema_;

    // ==========================================================================
    // Indicators (all O(1) incremental)
    // ==========================================================================

    ind::RSI rsi14_;
    ind::RSI rsi21_;
    ind::ATR atr14_;
    ind::Boll bb20_;
    ind::Stoch stoch14_;
    ind::WilliamsR will14_;
    ind::MACD macd_;
    ind::ROC roc5_, roc10_, roc20_;
    ind::CCI cci20_;
    ind::Donchian don20_;
    ind::Keltner keltner_;
    ind::OBV obv_;
    ind::VWAP vwap_;

    // Moving averages
    roll::EMA ema10_, ema20_, ema50_;
    roll::Ring<double> sma10_ring_, sma20_ring_, sma50_ring_;

    // ==========================================================================
    // State
    // ==========================================================================

    bool seeded_ = false;
    size_t bar_count_ = 0;
    double prevClose_ = std::numeric_limits<double>::quiet_NaN();
    double prevOpen_ = std::numeric_limits<double>::quiet_NaN();
    double prevHigh_ = std::numeric_limits<double>::quiet_NaN();
    double prevLow_ = std::numeric_limits<double>::quiet_NaN();
    double prevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // Feature vector (stable order, contiguous for model input)
    std::vector<double> feats_;

    // Normalization
    Scaler scaler_;
    std::vector<std::vector<double>> normalization_buffer_;  // For fit()
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Compute SHA1 hash of string (for schema versioning).
 */
std::string sha1_hex(const std::string& s);

/**
 * Safe return calculation (handles NaN and division by zero).
 */
inline double safe_return(double current, double previous) {
    if (std::isnan(previous) || previous == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (current / previous) - 1.0;
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 3 of 10**: include/learning/online_predictor.h

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

## 📄 **FILE 4 of 10**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 200 lines
- **Modified**: 2025-10-08 13:24:00

- **Type**: .h

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "learning/online_predictor.h"
#include "features/unified_engine_v2.h"
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

    // Learning state management
    struct LearningState {
        int64_t last_trained_bar_id = -1;      // Global bar ID of last training
        int last_trained_bar_index = -1;       // Index of last trained bar
        int64_t last_trained_timestamp_ms = 0; // Timestamp of last training
        bool is_warmed_up = false;              // Feature engine ready
        bool is_learning_current = true;        // Learning is up-to-date
        int bars_behind = 0;                    // How many bars behind
    };

    LearningState get_learning_state() const { return learning_state_; }
    bool ensure_learning_current(const Bar& bar);  // Catch up if needed
    bool is_learning_current() const { return learning_state_.is_learning_current; }

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

    // Feature engineering (V2 - production-grade with O(1) updates)
    std::unique_ptr<features::UnifiedFeatureEngineV2> feature_engine_v2_;

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

    // Learning state tracking
    LearningState learning_state_;
    std::deque<Bar> missed_bars_;  // Queue of bars that need training

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

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

## 📄 **FILE 5 of 10**: megadocs/OPTUNA_MRB_OPTIMIZATION_REQUIREMENTS.md

**File Information**:
- **Path**: `megadocs/OPTUNA_MRB_OPTIMIZATION_REQUIREMENTS.md`

- **Size**: 401 lines
- **Modified**: 2025-10-08 13:29:56

- **Type**: .md

```text
# Optuna MRB Optimization Requirements & Parameter Ranges

**Date:** 2025-10-08
**Objective:** Define proper parameter settings and ranges for Optuna to optimize OnlineEnsembleStrategy (OES) on Mean Return per Block (MRB)
**Current Baseline:** MRB = -0.165% per block (4-block test, 2-block warmup)

---

## 1. OPTIMIZATION OBJECTIVE

### Primary Metric: Mean Return per Block (MRB)
```
MRB = Total Return % / Number of Test Blocks
```

**Why MRB over Total Return:**
- Normalizes performance across different test periods
- Allows fair comparison between 10-block vs 100-block runs
- Represents expected return per trading day (480 bars = 1 day)
- More stable metric than total return for parameter selection

**Target:** Maximize MRB while maintaining:
- Win rate ≥ 55%
- Max drawdown ≤ 5%
- Trade frequency: 50-200% per block (reasonable activity)

---

## 2. PARAMETER CATEGORIES & RANGES

### Category A: EWRLS Learning Parameters (HIGH PRIORITY)
These control how fast/slow the model adapts to new data.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `ewrls_lambda` | 0.995 | 0.990 | 0.999 | float | Forgetting factor: higher = slower adaptation |
| `initial_variance` | 100.0 | 10.0 | 500.0 | float | Initial parameter uncertainty |
| `regularization` | 0.01 | 0.001 | 0.1 | float | L2 penalty to prevent overfitting |
| `warmup_samples` | 100 | 50 | 200 | int | Bars before trading starts |

**Rationale:**
- `ewrls_lambda`: 0.995 means ~140 bars half-life. SPY volatility changes over weeks/months, so range 0.990-0.999 covers 70-700 bars.
- `initial_variance`: Higher = more aggressive initial learning. Range allows 10x variation.
- `regularization`: Prevents feature weights from exploding. Log-scale search recommended.
- `warmup_samples`: Too low = noisy predictions, too high = missed opportunities.

---

### Category B: Multi-Horizon Ensemble (MEDIUM PRIORITY)
Controls how different time horizons are weighted.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `horizon_1_weight` | 0.3 | 0.1 | 0.5 | float | Weight for 1-bar predictor |
| `horizon_5_weight` | 0.5 | 0.2 | 0.6 | float | Weight for 5-bar predictor |
| `horizon_10_weight` | 0.2 | 0.1 | 0.4 | float | Weight for 10-bar predictor |

**Constraint:** `horizon_1_weight + horizon_5_weight + horizon_10_weight = 1.0`

**Rationale:**
- Current weights favor 5-bar (medium-term) predictions
- 1-bar: captures immediate momentum
- 5-bar: ~1 hour of trading (most reliable?)
- 10-bar: ~2 hours (longer trends)

**Alternative Approach:** Make horizons themselves tunable:
- `horizon_1`: 1-3 bars
- `horizon_2`: 5-10 bars
- `horizon_3`: 10-20 bars

---

### Category C: Signal Generation Thresholds (HIGH PRIORITY)
Controls when LONG/SHORT signals are triggered.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `buy_threshold` | 0.53 | 0.51 | 0.60 | float | Probability threshold for LONG |
| `sell_threshold` | 0.47 | 0.40 | 0.49 | float | Probability threshold for SHORT |

**Constraint:** `buy_threshold > 0.50 > sell_threshold` and `buy_threshold - sell_threshold >= 0.02`

**Rationale:**
- Current neutral zone: 0.47-0.53 (6% wide)
- Wider neutral zone = fewer trades, higher quality
- Narrower neutral zone = more trades, potentially noisier
- Asymmetric thresholds possible (e.g., 0.55/0.48 if market has upward bias)

---

### Category D: Bollinger Bands Amplification (MEDIUM PRIORITY)
Boosts signal strength near BB extremes.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `bb_period` | 20 | 10 | 50 | int | BB calculation window |
| `bb_std_dev` | 2.0 | 1.5 | 3.0 | float | Standard deviations for bands |
| `bb_proximity_threshold` | 0.30 | 0.20 | 0.50 | float | Within X% of band triggers boost |
| `bb_amplification_factor` | 0.10 | 0.05 | 0.20 | float | How much to boost probability |

**Rationale:**
- BB amplification should help catch mean-reversion opportunities
- `bb_period=20`: Standard setting, but 10-50 covers intraday to multi-day
- `bb_std_dev=2.0`: Standard 95% coverage, but 1.5-3.0 allows tuning sensitivity
- Current amplification is conservative (0.10), range allows 0.05-0.20

---

### Category E: Adaptive Calibration (LOW PRIORITY - Consider Disabling)
Auto-adjusts thresholds based on recent win rate.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `enable_threshold_calibration` | true | - | - | bool | Enable/disable feature |
| `calibration_window` | 200 | 100 | 500 | int | Bars for calibration |
| `target_win_rate` | 0.60 | 0.55 | 0.65 | float | Target accuracy |
| `threshold_step` | 0.005 | 0.002 | 0.010 | float | Adjustment size |

**WARNING:** This feature may interfere with Optuna optimization by changing thresholds during test period.

**Recommendation:** DISABLE during optimization (`enable_threshold_calibration=false`), then re-enable if needed.

---

### Category F: Risk Management (MEDIUM PRIORITY)
Controls position sizing and Kelly criterion.

| Parameter | Current | Min | Max | Type | Impact |
|-----------|---------|-----|-----|------|--------|
| `kelly_fraction` | 0.25 | 0.10 | 0.50 | float | Fraction of Kelly to use |
| `max_position_size` | 0.50 | 0.25 | 1.00 | float | Max capital per position |

**Rationale:**
- `kelly_fraction=0.25`: Conservative (1/4 Kelly). Range 0.10-0.50 covers very conservative to half-Kelly.
- `max_position_size=0.50`: Allows 50% of capital per trade. SPY is liquid, so could go higher, but keep 0.25-1.0 for safety.

---

## 3. PARAMETER INTERACTION ANALYSIS

### Critical Interactions:

**1. `ewrls_lambda` ↔ `warmup_samples`**
- Higher lambda (slower learning) → needs more warmup
- Lower lambda (faster learning) → can use less warmup
- **Suggestion:** If `ewrls_lambda > 0.997`, require `warmup_samples >= 150`

**2. `buy_threshold` ↔ `sell_threshold` ↔ `bb_amplification_factor`**
- Higher BB amplification → can use stricter thresholds (higher buy, lower sell)
- Wider neutral zone → less dependent on BB amplification
- **Suggestion:** Sample `neutral_zone_width = buy_threshold - sell_threshold` as single parameter (0.02-0.15)

**3. `horizon_weights` ↔ `ewrls_lambda`**
- Longer horizons (10-bar) work better with higher lambda (smoother trends)
- Shorter horizons (1-bar) work better with lower lambda (faster reaction)
- **Suggestion:** Adjust per-horizon lambda based on horizon length

**4. `kelly_fraction` ↔ Signal Quality**
- If win rate is high (>60%), can increase Kelly fraction
- If win rate is low (<55%), should decrease Kelly fraction
- **Suggestion:** Make Kelly fraction adaptive to recent win rate

---

## 4. OPTUNA SEARCH STRATEGY

### Phase 1: Coarse Grid Search (50 trials)
Explore wide ranges to identify promising regions.

```python
# Optuna trial suggestions
lambda_val = trial.suggest_float('ewrls_lambda', 0.990, 0.999)
buy_thresh = trial.suggest_float('buy_threshold', 0.51, 0.60)
sell_thresh = trial.suggest_float('sell_threshold', 0.40, 0.49)
bb_amp = trial.suggest_float('bb_amplification_factor', 0.05, 0.20)
kelly_frac = trial.suggest_float('kelly_fraction', 0.10, 0.50)

# Ensure constraint
if buy_thresh - sell_thresh < 0.02:
    raise optuna.TrialPruned()
```

### Phase 2: Fine-Tuning (100 trials)
Narrow ranges around top 10 parameter sets from Phase 1.

```python
# Example: If Phase 1 found best lambda=0.995, narrow to 0.993-0.997
lambda_val = trial.suggest_float('ewrls_lambda', 0.993, 0.997)
buy_thresh = trial.suggest_float('buy_threshold', 0.52, 0.56)
# ... etc
```

### Phase 3: Validation (20 trials)
Test top 3 parameter sets on different time periods (walk-forward).

---

## 5. EVALUATION PROTOCOL

### Test Configuration:
- **Data:** SPY_RTH_NH_cpp.bin (2020-2025, 1018 blocks total)
- **Train Period:** First 800 blocks (80%)
- **Test Period:** Last 218 blocks (20%)
- **Warmup:** 10 blocks before each test period
- **Objective:** Maximize MRB on test period

### Optimization Objective Function:
```python
def objective(trial):
    # Sample parameters
    params = sample_parameters(trial)

    # Run backtest
    mrb, win_rate, max_dd, trades_per_block = run_backtest(params)

    # Penalty for poor constraints
    penalty = 0
    if win_rate < 0.50:
        penalty += (0.50 - win_rate) * 10  # Severe penalty for <50% win rate
    if max_dd > 0.10:
        penalty += (max_dd - 0.10) * 5     # Penalty for >10% drawdown
    if trades_per_block < 50 or trades_per_block > 300:
        penalty += 0.01  # Light penalty for extreme trade frequency

    # Return MRB with penalties
    return mrb - penalty
```

### Constraints to Enforce:
1. **Minimum trades:** ≥50 trades total (avoid overly conservative configs)
2. **Win rate:** ≥50% (break-even baseline)
3. **Max drawdown:** ≤10% (risk management)
4. **Valid parameter ranges:** As defined above

---

## 6. RECOMMENDED PARAMETER PRIORITIES

### Tier 1 (Optimize First):
1. `buy_threshold` / `sell_threshold` - Controls trade frequency and quality
2. `ewrls_lambda` - Controls learning speed
3. `bb_amplification_factor` - Signal enhancement

### Tier 2 (Optimize Second):
4. `horizon_weights` - Multi-horizon blending
5. `kelly_fraction` - Position sizing
6. `regularization` - Overfitting control

### Tier 3 (Fine-tune Last):
7. `bb_period`, `bb_std_dev` - BB parameters
8. `warmup_samples` - Warmup length
9. `initial_variance` - Initial learning aggressiveness

### Disabled During Optimization:
- `enable_threshold_calibration` = false (interferes with optimization)
- `enable_adaptive_learning` = true (keep enabled, part of EWRLS design)

---

## 7. BASELINE EXPERIMENT DESIGN

### Experiment 1: Threshold-Only Optimization
Fix all other parameters, optimize only `buy_threshold` and `sell_threshold`.

**Purpose:** Establish baseline improvement from signal filtering alone.

### Experiment 2: EWRLS-Only Optimization
Fix thresholds, optimize `ewrls_lambda`, `regularization`, `initial_variance`.

**Purpose:** Measure impact of learning rate tuning.

### Experiment 3: Full Optimization
Optimize all Tier 1 + Tier 2 parameters simultaneously.

**Purpose:** Find global optimum.

### Experiment 4: Ensemble Weights
Fix best params from Exp 3, optimize `horizon_weights`.

**Purpose:** Test if ensemble weighting matters.

---

## 8. SUCCESS CRITERIA

### Minimum Viable Improvement:
- MRB improvement: ≥0.10% per block (from -0.165% to -0.065% or better)
- Win rate: ≥55%
- Max drawdown: ≤5%

### Stretch Goal:
- MRB: ≥+0.05% per block (profitable on average)
- Win rate: ≥60%
- Sharpe ratio: ≥0.5 (assuming 252 trading days/year)

### Red Flags:
- If MRB degrades below -0.20%, parameter ranges may be too wide
- If all trials have <50% win rate, signal quality is fundamentally poor
- If optimization finds edge cases (e.g., lambda=0.999, buy_threshold=0.60), ranges are wrong

---

## 9. IMPLEMENTATION CHECKLIST

### Before Running Optuna:
- [ ] Verify backtest command accepts parameter overrides
- [ ] Implement parameter validation in OES constructor
- [ ] Add MRB calculation to analyze-trades output
- [ ] Create Optuna study database (SQLite or in-memory)
- [ ] Set up logging for each trial (save to `data/tmp/optuna_trials/`)

### During Optimization:
- [ ] Monitor trial progress (expected ~2 min per trial)
- [ ] Check for early pruning opportunities (e.g., if MRB < -0.50% after 2 blocks)
- [ ] Save intermediate best parameters every 10 trials
- [ ] Plot parameter importance after 50 trials

### After Optimization:
- [ ] Validate best parameters on holdout period (separate 100 blocks)
- [ ] Compare best vs baseline on multiple metrics (not just MRB)
- [ ] Test robustness: run best params on QQQ data
- [ ] Document winning parameter set in configuration file

---

## 10. SOURCE CODE MODULES REFERENCE

### Strategy Configuration:
- `include/strategy/online_ensemble_strategy.h:33-80` - OnlineEnsembleConfig struct
- `src/strategy/online_ensemble_strategy.cpp:10-53` - Constructor with config initialization

### EWRLS Learning:
- `include/learning/online_predictor.h:20-37` - OnlinePredictor::Config
- `src/learning/online_predictor.cpp:50-109` - EWRLS update logic
- `src/learning/online_predictor.cpp:119-137` - Adaptive learning rate

### Multi-Horizon Ensemble:
- `include/learning/online_predictor.h:102-130` - MultiHorizonPredictor
- `src/learning/online_predictor.cpp:277-294` - add_horizon() with auto lambda adjustment
- `src/learning/online_predictor.cpp:296-326` - Ensemble prediction weighting

### Signal Generation:
- `src/strategy/online_ensemble_strategy.cpp:55-145` - generate_signal() with BB amplification
- `src/strategy/online_ensemble_strategy.cpp:294-302` - determine_signal() threshold logic
- `src/strategy/online_ensemble_strategy.cpp:516-566` - BB calculation and amplification

### Delayed Learning Updates:
- `src/strategy/online_ensemble_strategy.cpp:238-264` - track_prediction() for horizon tracking
- `src/strategy/online_ensemble_strategy.cpp:266-292` - process_pending_updates() for delayed EWRLS updates

### Risk Management:
- `src/backend/adaptive_portfolio_manager.cpp` - Kelly sizing implementation
- `src/backend/ensemble_position_state_machine.cpp` - Multi-instrument PSM

### Backtest Infrastructure:
- `src/cli/backtest_command.cpp:51-315` - End-to-end backtest workflow
- `src/cli/analyze_trades_command.cpp` - Performance analysis and MRB calculation

### Feature Engineering:
- `include/features/unified_engine_v2.h` - 45-feature production engine
- `src/features/unified_engine_v2.cpp` - O(1) feature updates

---

## 11. EXPECTED RUNTIME

### Single Trial:
- Signal generation: ~30 seconds (2880 bars)
- Trade execution: ~10 seconds
- Analysis: ~5 seconds
- **Total: ~45-60 seconds per trial**

### Full Optimization:
- 50 trials (Phase 1): ~45 minutes
- 100 trials (Phase 2): ~90 minutes
- 20 trials (Phase 3): ~20 minutes
- **Total: ~2.5-3 hours for complete optimization**

### Parallelization:
- Can run 4-8 trials in parallel if using multi-process Optuna
- Reduces total time to ~30-45 minutes

---

## 12. NEXT STEPS

1. **Implement Optuna wrapper script** (`tools/optimize_oes_params.py`)
2. **Add parameter override CLI flags** to `sentio_cli backtest`
3. **Run Experiment 1** (threshold-only) to validate framework
4. **Iterate** based on results
5. **Deploy best parameters** to production config

---

## CONCLUSION

This document provides comprehensive parameter ranges and optimization strategy for tuning OnlineEnsembleStrategy to maximize MRB. The key insight is to focus on **signal quality** (thresholds, BB amplification) and **learning dynamics** (EWRLS lambda) before fine-tuning ensemble weights and risk management.

**Critical Success Factor:** Optuna must search intelligently in the high-dimensional parameter space. Use constraints, early pruning, and multi-phase optimization to avoid wasting trials on obviously bad configs.

**Risk Mitigation:** Always validate on holdout data and test robustness across different market regimes (2020 COVID crash, 2021 rally, 2022 bear, 2023-2024 recovery).

```

## 📄 **FILE 6 of 10**: src/cli/analyze_trades_command.cpp

**File Information**:
- **Path**: `src/cli/analyze_trades_command.cpp`

- **Size**: 324 lines
- **Modified**: 2025-10-08 09:51:20

- **Type**: .cpp

```text
#include "cli/ensemble_workflow_command.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace sentio {
namespace cli {

// Per-instrument performance tracking
struct InstrumentMetrics {
    std::string symbol;
    int num_trades = 0;
    int buy_count = 0;
    int sell_count = 0;
    double total_buy_value = 0.0;
    double total_sell_value = 0.0;
    double realized_pnl = 0.0;
    double avg_allocation_pct = 0.0;  // Average % of portfolio allocated
    double win_rate = 0.0;
    int winning_trades = 0;
    int losing_trades = 0;
};

int AnalyzeTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string trades_path = get_arg(args, "--trades", "");
    std::string output_path = get_arg(args, "--output", "analysis_report.json");
    int num_blocks = std::stoi(get_arg(args, "--blocks", "0"));  // Number of blocks for MRB calculation
    bool show_detailed = !has_flag(args, "--summary-only");
    bool show_trades = has_flag(args, "--show-trades");
    bool export_csv = has_flag(args, "--csv");
    bool export_json = !has_flag(args, "--no-json");

    if (trades_path.empty()) {
        std::cerr << "Error: --trades is required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Trade Analysis ===\n";
    std::cout << "Trade file: " << trades_path << "\n\n";

    // Load trades from JSONL
    std::cout << "Loading trade history...\n";
    std::vector<ExecuteTradesCommand::TradeRecord> trades;

    std::ifstream file(trades_path);
    if (!file) {
        std::cerr << "Error: Could not open trade file\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json j = json::parse(line);
            ExecuteTradesCommand::TradeRecord trade;

            trade.bar_id = j["bar_id"];
            trade.timestamp_ms = j["timestamp_ms"];
            trade.bar_index = j["bar_index"];
            trade.symbol = j["symbol"];

            std::string action_str = j["action"];
            trade.action = (action_str == "BUY") ? TradeAction::BUY : TradeAction::SELL;

            trade.quantity = j["quantity"];
            trade.price = j["price"];
            trade.trade_value = j["trade_value"];
            trade.fees = j["fees"];
            trade.reason = j["reason"];

            trade.cash_balance = j["cash_balance"];
            trade.portfolio_value = j["portfolio_value"];
            trade.position_quantity = j["position_quantity"];
            trade.position_avg_price = j["position_avg_price"];

            trades.push_back(trade);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse line: " << e.what() << "\n";
        }
    }

    std::cout << "Loaded " << trades.size() << " trades\n\n";

    if (trades.empty()) {
        std::cerr << "Error: No trades loaded\n";
        return 1;
    }

    // Calculate per-instrument metrics
    std::cout << "Calculating per-instrument metrics...\n";
    std::map<std::string, InstrumentMetrics> instrument_metrics;
    std::map<std::string, std::vector<std::pair<double, double>>> position_tracking;  // symbol -> [(buy_price, quantity)]

    double starting_capital = 100000.0;  // Assume standard starting capital
    double total_allocation_samples = 0;

    for (const auto& trade : trades) {
        auto& metrics = instrument_metrics[trade.symbol];
        metrics.symbol = trade.symbol;
        metrics.num_trades++;

        if (trade.action == TradeAction::BUY) {
            metrics.buy_count++;
            metrics.total_buy_value += trade.trade_value;

            // Track position for P/L calculation
            position_tracking[trade.symbol].push_back({trade.price, trade.quantity});

            // Track allocation
            double allocation_pct = (trade.trade_value / trade.portfolio_value) * 100.0;
            metrics.avg_allocation_pct += allocation_pct;
            total_allocation_samples++;

        } else {  // SELL
            metrics.sell_count++;
            metrics.total_sell_value += trade.trade_value;

            // Calculate realized P/L using FIFO
            auto& positions = position_tracking[trade.symbol];
            double remaining_qty = trade.quantity;
            double trade_pnl = 0.0;

            while (remaining_qty > 0 && !positions.empty()) {
                auto& pos = positions.front();
                double qty_to_close = std::min(remaining_qty, pos.second);

                // P/L = (sell_price - buy_price) * quantity
                trade_pnl += (trade.price - pos.first) * qty_to_close;

                pos.second -= qty_to_close;
                remaining_qty -= qty_to_close;

                if (pos.second <= 0) {
                    positions.erase(positions.begin());
                }
            }

            metrics.realized_pnl += trade_pnl;

            // Track win/loss
            if (trade_pnl > 0) {
                metrics.winning_trades++;
            } else if (trade_pnl < 0) {
                metrics.losing_trades++;
            }
        }
    }

    // Calculate averages and win rates
    for (auto& [symbol, metrics] : instrument_metrics) {
        if (metrics.buy_count > 0) {
            metrics.avg_allocation_pct /= metrics.buy_count;
        }
        int completed_trades = metrics.winning_trades + metrics.losing_trades;
        if (completed_trades > 0) {
            metrics.win_rate = (double)metrics.winning_trades / completed_trades * 100.0;
        }
    }

    // Calculate overall metrics
    std::cout << "Calculating overall performance metrics...\n";
    PerformanceReport report = calculate_metrics(trades);

    // Print instrument analysis
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         PER-INSTRUMENT PERFORMANCE ANALYSIS                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Sort instruments by realized P/L (descending)
    std::vector<std::pair<std::string, InstrumentMetrics>> sorted_instruments;
    for (const auto& [symbol, metrics] : instrument_metrics) {
        sorted_instruments.push_back({symbol, metrics});
    }
    std::sort(sorted_instruments.begin(), sorted_instruments.end(),
              [](const auto& a, const auto& b) { return a.second.realized_pnl > b.second.realized_pnl; });

    std::cout << std::fixed << std::setprecision(2);

    for (const auto& [symbol, m] : sorted_instruments) {
        std::string pnl_indicator = (m.realized_pnl > 0) ? "✅" : (m.realized_pnl < 0) ? "❌" : "  ";

        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << symbol << " " << pnl_indicator << "\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "  Trades:           " << m.num_trades << " (" << m.buy_count << " BUY, " << m.sell_count << " SELL)\n";
        std::cout << "  Total Buy Value:  $" << std::setw(12) << m.total_buy_value << "\n";
        std::cout << "  Total Sell Value: $" << std::setw(12) << m.total_sell_value << "\n";
        std::cout << "  Realized P/L:     $" << std::setw(12) << m.realized_pnl
                  << "  (" << std::showpos << (m.realized_pnl / starting_capital * 100.0)
                  << std::noshowpos << "% of capital)\n";
        std::cout << "  Avg Allocation:   " << std::setw(12) << m.avg_allocation_pct << "%\n";
        std::cout << "  Win Rate:         " << std::setw(12) << m.win_rate << "%  ("
                  << m.winning_trades << "W / " << m.losing_trades << "L)\n";
        std::cout << "\n";
    }

    // Summary table
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              INSTRUMENT SUMMARY TABLE                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << std::left << std::setw(8) << "Symbol"
              << std::right << std::setw(10) << "Trades"
              << std::setw(12) << "Alloc %"
              << std::setw(15) << "P/L ($)"
              << std::setw(12) << "P/L (%)"
              << std::setw(12) << "Win Rate"
              << "\n";
    std::cout << "────────────────────────────────────────────────────────────────\n";

    for (const auto& [symbol, m] : sorted_instruments) {
        double pnl_pct = (m.realized_pnl / starting_capital) * 100.0;
        std::cout << std::left << std::setw(8) << symbol
                  << std::right << std::setw(10) << m.num_trades
                  << std::setw(12) << m.avg_allocation_pct
                  << std::setw(15) << m.realized_pnl
                  << std::setw(12) << std::showpos << pnl_pct << std::noshowpos
                  << std::setw(12) << m.win_rate
                  << "\n";
    }

    // Calculate total realized P/L from instruments
    double total_realized_pnl = 0.0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_realized_pnl += m.realized_pnl;
    }

    std::cout << "────────────────────────────────────────────────────────────────\n";
    std::cout << std::left << std::setw(8) << "TOTAL"
              << std::right << std::setw(10) << trades.size()
              << std::setw(12) << ""
              << std::setw(15) << total_realized_pnl
              << std::setw(12) << std::showpos << (total_realized_pnl / starting_capital * 100.0) << std::noshowpos
              << std::setw(12) << ""
              << "\n\n";

    // Calculate and print MRB if blocks specified
    double total_return_pct = (total_realized_pnl / starting_capital) * 100.0;
    if (num_blocks > 0) {
        double mrb = total_return_pct / num_blocks;
        std::cout << "Mean Return per Block (MRB): " << std::showpos << std::fixed << std::setprecision(4)
                  << mrb << std::noshowpos << "% (" << num_blocks << " blocks)\n\n";
    }

    // Print overall report
    print_report(report);

    // Save report
    if (export_json) {
        std::cout << "\nSaving report to " << output_path << "...\n";
        save_report_json(report, output_path);
    }

    std::cout << "\n✅ Analysis complete!\n";
    return 0;
}

AnalyzeTradesCommand::PerformanceReport
AnalyzeTradesCommand::calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades) {
    PerformanceReport report;

    if (trades.empty()) {
        return report;
    }

    // Basic counts
    report.total_trades = static_cast<int>(trades.size());

    // Extract equity curve from trades
    std::vector<double> equity;
    for (const auto& trade : trades) {
        equity.push_back(trade.portfolio_value);
    }

    // Calculate returns (stub - would need full implementation)
    report.total_return_pct = 0.0;
    report.annualized_return = 0.0;

    return report;
}

void AnalyzeTradesCommand::print_report(const PerformanceReport& report) {
    // Stub - basic implementation
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Total Trades: " << report.total_trades << "\n";
}

void AnalyzeTradesCommand::save_report_json(const PerformanceReport& report, const std::string& path) {
    // Stub
}

void AnalyzeTradesCommand::show_help() const {
    std::cout << "Usage: sentio_cli analyze-trades --trades <file> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --trades <file>     Trade history file (JSONL format)\n";
    std::cout << "  --output <file>     Output report file (default: analysis_report.json)\n";
    std::cout << "  --blocks <N>        Number of blocks traded (for MRB calculation)\n";
    std::cout << "  --summary-only      Show only summary metrics\n";
    std::cout << "  --show-trades       Show individual trade details\n";
    std::cout << "  --csv               Export to CSV format\n";
    std::cout << "  --no-json           Disable JSON export\n";
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 7 of 10**: src/cli/backtest_command.cpp

**File Information**:
- **Path**: `src/cli/backtest_command.cpp`

- **Size**: 318 lines
- **Modified**: 2025-10-08 12:55:08

- **Type**: .cpp

```text
#include "cli/backtest_command.h"
#include "cli/command_registry.h"
#include "common/binary_data.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <memory>

namespace sentio {
namespace cli {

void BacktestCommand::show_help() const {
    std::cout << "Backtest Command - Run end-to-end strategy backtest\n";
    std::cout << "======================================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  sentio_cli backtest --blocks <N> [OPTIONS]\n\n";
    std::cout << "Required Arguments:\n";
    std::cout << "  --blocks <N>           Number of blocks to test (1 block = 480 bars = 1 trading day)\n\n";
    std::cout << "Optional Arguments:\n";
    std::cout << "  --data <path>          Data file path (default: " << DEFAULT_DATA_PATH << ")\n";
    std::cout << "                         Supports both CSV and BIN formats (auto-detected)\n";
    std::cout << "  --warmup-blocks <N>    Warmup blocks before test period (default: 10)\n";
    std::cout << "                         These blocks are used for learning but not counted in results\n";
    std::cout << "  --warmup <N>           Additional warmup bars within first warmup block (default: 100)\n";
    std::cout << "  --output-dir <dir>     Output directory for results (default: data/tmp)\n";
    std::cout << "  --verbose, -v          Verbose output\n\n";
    std::cout << "Warmup Behavior:\n";
    std::cout << "  The strategy uses TWO warmup phases:\n";
    std::cout << "  1. Block warmup: --warmup-blocks N loads N extra blocks before test period\n";
    std::cout << "     - Strategy learns from these blocks (continuous online learning)\n";
    std::cout << "     - Trades executed during warmup blocks are NOT counted in results\n";
    std::cout << "  2. Bar warmup: --warmup N skips first N bars of the warmup period\n";
    std::cout << "     - Allows feature calculation to stabilize before starting learning\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Test 20 blocks with 10-block warmup (default)\n";
    std::cout << "  sentio_cli backtest --blocks 20\n\n";
    std::cout << "  # Test 20 blocks with 5-block warmup + 200 bar warmup\n";
    std::cout << "  sentio_cli backtest --blocks 20 --warmup-blocks 5 --warmup 200\n\n";
    std::cout << "  # Test 100 blocks with 20-block warmup (for extensive learning)\n";
    std::cout << "  sentio_cli backtest --blocks 100 --warmup-blocks 20\n\n";
    std::cout << "Output:\n";
    std::cout << "  - Signals JSONL file\n";
    std::cout << "  - Trades JSONL file\n";
    std::cout << "  - Performance analysis report\n";
    std::cout << "  - MRB (Mean Return per Block) calculation\n";
}

int BacktestCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string blocks_str = get_arg(args, "--blocks", "");
    if (blocks_str.empty()) {
        std::cerr << "❌ Error: --blocks is required\n\n";
        show_help();
        return 1;
    }

    int num_blocks = std::stoi(blocks_str);
    if (num_blocks <= 0) {
        std::cerr << "❌ Error: --blocks must be positive (got " << num_blocks << ")\n";
        return 1;
    }

    std::string data_path = get_arg(args, "--data", DEFAULT_DATA_PATH);
    int warmup_blocks = std::stoi(get_arg(args, "--warmup-blocks", "10"));
    int warmup_bars = std::stoi(get_arg(args, "--warmup", "100"));
    std::string output_dir = get_arg(args, "--output-dir", "data/tmp");
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");

    if (warmup_blocks < 0) {
        std::cerr << "❌ Error: --warmup-blocks must be non-negative (got " << warmup_blocks << ")\n";
        return 1;
    }

    // Create output directory
    std::filesystem::create_directories(output_dir);

    // Output file paths
    std::string signals_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_signals.jsonl";
    std::string trades_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_trades.jsonl";
    std::string analysis_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_analysis.txt";

    // Print header
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  🎯 BACKTEST - " << num_blocks << " Blocks + " << warmup_blocks << " Warmup Blocks\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "Data:          " << data_path << "\n";
    std::cout << "Test Blocks:   " << num_blocks << " (=" << (num_blocks * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Warmup Blocks: " << warmup_blocks << " (=" << (warmup_blocks * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Warmup Bars:   " << warmup_bars << " bars (initial feature stabilization)\n";
    std::cout << "Total Data:    " << (num_blocks + warmup_blocks) << " blocks (="
              << ((num_blocks + warmup_blocks) * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Output:        " << output_dir << "/\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";

    // Step 1: Load data (test blocks + warmup blocks)
    std::cout << "📊 Step 1: Loading data...\n";
    std::vector<Bar> bars;
    int total_blocks = num_blocks + warmup_blocks;

    // Auto-detect format (CSV vs BIN)
    std::filesystem::path p(data_path);
    bool is_binary = (p.extension() == ".bin");

    if (is_binary) {
        // Load from binary file
        binary_data::BinaryDataReader reader(data_path);
        if (!reader.open()) {
            std::cerr << "❌ Failed to open binary data file: " << data_path << "\n";
            return 1;
        }

        uint64_t total_bars = reader.get_bar_count();
        uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;

        if (bars_needed > total_bars) {
            std::cerr << "⚠️  Warning: Requested " << bars_needed << " bars but only "
                      << total_bars << " available\n";
            std::cerr << "   Using all " << total_bars << " bars ("
                      << (total_bars / BARS_PER_BLOCK) << " blocks)\n";
            bars_needed = total_bars;
        }

        bars = reader.read_last_n_bars(bars_needed);

        if (verbose) {
            std::cout << "   Symbol: " << reader.get_symbol() << "\n";
            std::cout << "   Total available: " << total_bars << " bars\n";
        }
        std::cout << "   Loaded: " << bars.size() << " bars ("
                  << std::fixed << std::setprecision(2)
                  << (bars.size() / static_cast<double>(BARS_PER_BLOCK)) << " blocks)\n";
    } else {
        // Load from CSV file
        bars = utils::read_csv_data(data_path);
        if (bars.empty()) {
            std::cerr << "❌ Failed to load CSV data from: " << data_path << "\n";
            return 1;
        }

        // Extract last N blocks (warmup + test)
        uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;
        if (bars.size() > bars_needed) {
            bars.erase(bars.begin(), bars.end() - bars_needed);
        }

        std::cout << "   Loaded: " << bars.size() << " bars ("
                  << std::fixed << std::setprecision(2)
                  << (bars.size() / static_cast<double>(BARS_PER_BLOCK)) << " blocks)\n";
    }

    if (bars.empty()) {
        std::cerr << "❌ No data loaded\n";
        return 1;
    }

    // Prepare data for workflow commands
    // Extract symbol from binary file (or use SPY as default for CSV)
    std::string symbol = "SPY";
    if (is_binary) {
        binary_data::BinaryDataReader reader_for_symbol(data_path);
        if (reader_for_symbol.open()) {
            symbol = reader_for_symbol.get_symbol();
        }
    }

    // Determine source directory and required instruments
    std::string data_source_dir = std::filesystem::current_path().parent_path().string() + "/data/equities";
    std::vector<std::string> required_symbols;
    if (symbol == "SPY") {
        required_symbols = {"SPY", "SPXL", "SH", "SDS"};
    } else if (symbol == "QQQ") {
        required_symbols = {"QQQ", "TQQQ", "PSQ", "SQQQ"};
    } else {
        std::cerr << "❌ Unsupported symbol: " << symbol << "\n";
        std::cerr << "   Only SPY and QQQ are supported for backtesting\n";
        return 1;
    }

    // Write truncated CSV files for all instruments
    // Execute-trades needs ALL 4 instruments with matching timestamps
    std::cout << "Preparing " << required_symbols.size() << " instrument CSV files...\n";
    uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;

    for (const auto& sym : required_symbols) {
        std::string source_file = data_source_dir + "/" + sym + "_RTH_NH.csv";
        std::string target_file = output_dir + "/" + sym + "_RTH_NH.csv";

        // Load and truncate data for this instrument
        auto instrument_bars = utils::read_csv_data(source_file);
        if (instrument_bars.empty()) {
            std::cerr << "❌ Failed to load " << sym << " data from " << source_file << "\n";
            return 1;
        }

        // Extract last N blocks (warmup + test)
        if (instrument_bars.size() > bars_needed) {
            instrument_bars.erase(instrument_bars.begin(),
                                 instrument_bars.end() - bars_needed);
        }

        // Write truncated CSV
        std::ofstream csv_out(target_file);
        if (!csv_out.is_open()) {
            std::cerr << "❌ Failed to create file: " << target_file << "\n";
            return 1;
        }

        csv_out << "ts_utc,ts_nyt_epoch,open,high,low,close,volume\n";
        for (const auto& bar : instrument_bars) {
            csv_out << utils::ms_to_timestamp(bar.timestamp_ms) << ","
                    << (bar.timestamp_ms / 1000) << ","
                    << std::fixed << std::setprecision(4)
                    << bar.open << "," << bar.high << "," << bar.low << "," << bar.close << ","
                    << bar.volume << "\n";
        }
        csv_out.close();

        std::cout << "  " << sym << ": " << instrument_bars.size() << " bars written\n";
    }

    std::string temp_data_file = output_dir + "/" + symbol + "_RTH_NH.csv";
    std::cout << "✅ Data prepared: " << total_blocks << " blocks (" << bars_needed << " bars) for 4 instruments\n\n";

    // Step 2: Generate signals (delegate to generate-signals command)
    std::cout << "🔧 Step 2: Generating signals...\n";
    auto& registry = CommandRegistry::instance();
    auto generate_cmd = registry.get_command("generate-signals");
    if (!generate_cmd) {
        std::cerr << "❌ Failed to get generate-signals command\n";
        return 1;
    }

    std::vector<std::string> gen_args = {
        "--data", temp_data_file,
        "--output", signals_file,
        "--warmup", std::to_string(warmup_bars)
    };
    if (verbose) gen_args.push_back("--verbose");

    int ret = generate_cmd->execute(gen_args);
    if (ret != 0) {
        std::cerr << "❌ Signal generation failed\n";
        return ret;
    }
    std::cout << "✅ Signals generated\n\n";

    // Step 3: Execute trades (delegate to execute-trades command)
    std::cout << "💼 Step 3: Executing trades...\n";
    auto execute_cmd = registry.get_command("execute-trades");
    if (!execute_cmd) {
        std::cerr << "❌ Failed to get execute-trades command\n";
        return 1;
    }

    // Calculate total warmup: warmup blocks + warmup bars
    // This tells execute-trades to skip the warmup period when calculating results
    int total_warmup_bars = (warmup_blocks * BARS_PER_BLOCK) + warmup_bars;

    std::vector<std::string> exec_args = {
        "--signals", signals_file,
        "--data", temp_data_file,
        "--output", trades_file,
        "--warmup", std::to_string(total_warmup_bars)
    };
    if (verbose) exec_args.push_back("--verbose");

    ret = execute_cmd->execute(exec_args);
    if (ret != 0) {
        std::cerr << "❌ Trade execution failed\n";
        return ret;
    }
    std::cout << "✅ Trades executed\n\n";

    // Step 4: Analyze performance (delegate to analyze-trades command)
    std::cout << "📈 Step 4: Analyzing performance...\n";
    auto analyze_cmd = registry.get_command("analyze-trades");
    if (!analyze_cmd) {
        std::cerr << "❌ Failed to get analyze-trades command\n";
        return 1;
    }

    std::vector<std::string> analyze_args = {
        "--trades", trades_file,
        "--data", temp_data_file,
        "--output", analysis_file
    };

    ret = analyze_cmd->execute(analyze_args);
    if (ret != 0) {
        std::cerr << "❌ Performance analysis failed\n";
        return ret;
    }
    std::cout << "✅ Analysis complete\n\n";

    // Clean up temp data files
    for (const auto& sym : required_symbols) {
        std::string temp_file = output_dir + "/" + sym + "_RTH_NH.csv";
        std::filesystem::remove(temp_file);
    }

    // Final summary
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  ✅ BACKTEST COMPLETE - " << num_blocks << " Blocks\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "📁 Results:\n";
    std::cout << "   Signals:  " << signals_file << "\n";
    std::cout << "   Trades:   " << trades_file << "\n";
    std::cout << "   Analysis: " << analysis_file << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";

    return 0;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 8 of 10**: src/features/unified_engine_v2.cpp

**File Information**:
- **Path**: `src/features/unified_engine_v2.cpp`

- **Size**: 363 lines
- **Modified**: 2025-10-08 11:17:20

- **Type**: .cpp

```text
#include "features/unified_engine_v2.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

// OpenSSL for SHA1 hashing (already in dependencies)
#include <openssl/sha.h>

namespace sentio {
namespace features {

// =============================================================================
// SHA1 Hash Utility
// =============================================================================

std::string sha1_hex(const std::string& s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);

    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        os << std::setw(2) << static_cast<int>(c);
    }
    return os.str();
}

// =============================================================================
// UnifiedFeatureEngineV2 Implementation
// =============================================================================

UnifiedFeatureEngineV2::UnifiedFeatureEngineV2(EngineConfig cfg)
    : cfg_(cfg),
      rsi14_(cfg.rsi14),
      rsi21_(cfg.rsi21),
      atr14_(cfg.atr14),
      bb20_(cfg.bb20, cfg.bb_k),
      stoch14_(cfg.stoch14),
      will14_(cfg.will14),
      macd_(),  // Uses default periods 12/26/9
      roc5_(cfg.roc5),
      roc10_(cfg.roc10),
      roc20_(cfg.roc20),
      cci20_(cfg.cci20),
      don20_(cfg.don20),
      keltner_(cfg.keltner_ema, cfg.keltner_atr, cfg.keltner_mult),
      obv_(),
      vwap_(),
      ema10_(cfg.ema10),
      ema20_(cfg.ema20),
      ema50_(cfg.ema50),
      sma10_ring_(cfg.sma10),
      sma20_ring_(cfg.sma20),
      sma50_ring_(cfg.sma50),
      scaler_(cfg.robust ? Scaler::Type::ROBUST : Scaler::Type::STANDARD)
{
    build_schema_();
    feats_.assign(schema_.names.size(), std::numeric_limits<double>::quiet_NaN());
}

void UnifiedFeatureEngineV2::build_schema_() {
    std::vector<std::string> n;

    // ==========================================================================
    // Core price/volume features (always included)
    // ==========================================================================
    n.push_back("price.close");
    n.push_back("price.open");
    n.push_back("price.high");
    n.push_back("price.low");
    n.push_back("price.return_1");
    n.push_back("volume.raw");

    // ==========================================================================
    // Moving Averages (always included for baseline)
    // ==========================================================================
    n.push_back("sma10");
    n.push_back("sma20");
    n.push_back("sma50");
    n.push_back("ema10");
    n.push_back("ema20");
    n.push_back("ema50");
    n.push_back("price_vs_sma20");  // (close - sma20) / sma20
    n.push_back("price_vs_ema20");  // (close - ema20) / ema20

    // ==========================================================================
    // Volatility Features
    // ==========================================================================
    if (cfg_.volatility) {
        n.push_back("atr14");
        n.push_back("atr14_pct");  // ATR / close
        n.push_back("bb20.mean");
        n.push_back("bb20.sd");
        n.push_back("bb20.upper");
        n.push_back("bb20.lower");
        n.push_back("bb20.percent_b");
        n.push_back("bb20.bandwidth");
        n.push_back("keltner.middle");
        n.push_back("keltner.upper");
        n.push_back("keltner.lower");
    }

    // ==========================================================================
    // Momentum Features
    // ==========================================================================
    if (cfg_.momentum) {
        n.push_back("rsi14");
        n.push_back("rsi21");
        n.push_back("stoch14.k");
        n.push_back("stoch14.d");
        n.push_back("stoch14.slow");
        n.push_back("will14");
        n.push_back("macd.line");
        n.push_back("macd.signal");
        n.push_back("macd.hist");
        n.push_back("roc5");
        n.push_back("roc10");
        n.push_back("roc20");
        n.push_back("cci20");
    }

    // ==========================================================================
    // Volume Features
    // ==========================================================================
    if (cfg_.volume) {
        n.push_back("obv");
        n.push_back("vwap");
        n.push_back("vwap_dist");  // (close - vwap) / vwap
    }

    // ==========================================================================
    // Donchian Channels (pattern/breakout detection)
    // ==========================================================================
    n.push_back("don20.up");
    n.push_back("don20.mid");
    n.push_back("don20.dn");
    n.push_back("don20.position");  // (close - dn) / (up - dn)

    // ==========================================================================
    // Finalize schema and compute hash
    // ==========================================================================
    schema_.names = std::move(n);

    // Concatenate names and critical config for hash
    std::ostringstream cat;
    for (const auto& name : schema_.names) {
        cat << name << "\n";
    }
    cat << "cfg:"
        << cfg_.rsi14 << ","
        << cfg_.bb20 << ","
        << cfg_.bb_k << ","
        << cfg_.macd_fast << ","
        << cfg_.macd_slow << ","
        << cfg_.macd_sig;

    schema_.sha1_hash = sha1_hex(cat.str());
}

bool UnifiedFeatureEngineV2::update(const Bar& b) {
    // ==========================================================================
    // Update all indicators (O(1) incremental)
    // ==========================================================================

    // Volatility
    atr14_.update(b.high, b.low, b.close);
    bb20_.update(b.close);
    keltner_.update(b.high, b.low, b.close);

    // Momentum
    rsi14_.update(b.close);
    rsi21_.update(b.close);
    stoch14_.update(b.high, b.low, b.close);
    will14_.update(b.high, b.low, b.close);
    macd_.update(b.close);
    roc5_.update(b.close);
    roc10_.update(b.close);
    roc20_.update(b.close);
    cci20_.update(b.high, b.low, b.close);

    // Channels
    don20_.update(b.high, b.low);

    // Volume
    obv_.update(b.close, b.volume);
    vwap_.update(b.close, b.volume);

    // Moving averages
    ema10_.update(b.close);
    ema20_.update(b.close);
    ema50_.update(b.close);
    sma10_ring_.push(b.close);
    sma20_ring_.push(b.close);
    sma50_ring_.push(b.close);

    // Store previous values for derived features
    prevClose_ = b.close;
    prevOpen_ = b.open;
    prevHigh_ = b.high;
    prevLow_ = b.low;
    prevVolume_ = b.volume;

    // Recompute feature vector
    recompute_vector_();

    seeded_ = true;
    ++bar_count_;
    return true;
}

void UnifiedFeatureEngineV2::recompute_vector_() {
    size_t k = 0;

    // ==========================================================================
    // Core price/volume
    // ==========================================================================
    feats_[k++] = prevClose_;
    feats_[k++] = prevOpen_;
    feats_[k++] = prevHigh_;
    feats_[k++] = prevLow_;
    feats_[k++] = safe_return(prevClose_, prevClose_);  // Placeholder (need prev bar)
    feats_[k++] = prevVolume_;

    // ==========================================================================
    // Moving Averages
    // ==========================================================================
    double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double ema10 = ema10_.get_value();
    double ema20 = ema20_.get_value();
    double ema50 = ema50_.get_value();

    feats_[k++] = sma10;
    feats_[k++] = sma20;
    feats_[k++] = sma50;
    feats_[k++] = ema10;
    feats_[k++] = ema20;
    feats_[k++] = ema50;

    // Price vs MA ratios
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : std::numeric_limits<double>::quiet_NaN();

    // ==========================================================================
    // Volatility
    // ==========================================================================
    if (cfg_.volatility) {
        feats_[k++] = atr14_.value;
        feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = bb20_.mean;
        feats_[k++] = bb20_.sd;
        feats_[k++] = bb20_.upper;
        feats_[k++] = bb20_.lower;
        feats_[k++] = bb20_.percent_b;
        feats_[k++] = bb20_.bandwidth;
        feats_[k++] = keltner_.middle;
        feats_[k++] = keltner_.upper;
        feats_[k++] = keltner_.lower;
    }

    // ==========================================================================
    // Momentum
    // ==========================================================================
    if (cfg_.momentum) {
        feats_[k++] = rsi14_.value;
        feats_[k++] = rsi21_.value;
        feats_[k++] = stoch14_.k;
        feats_[k++] = stoch14_.d;
        feats_[k++] = stoch14_.slow;
        feats_[k++] = will14_.r;
        feats_[k++] = macd_.macd;
        feats_[k++] = macd_.signal;
        feats_[k++] = macd_.hist;
        feats_[k++] = roc5_.value;
        feats_[k++] = roc10_.value;
        feats_[k++] = roc20_.value;
        feats_[k++] = cci20_.value;
    }

    // ==========================================================================
    // Volume
    // ==========================================================================
    if (cfg_.volume) {
        feats_[k++] = obv_.value;
        feats_[k++] = vwap_.value;
        double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                           ? (prevClose_ - vwap_.value) / vwap_.value
                           : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = vwap_dist;
    }

    // ==========================================================================
    // Donchian
    // ==========================================================================
    feats_[k++] = don20_.up;
    feats_[k++] = don20_.mid;
    feats_[k++] = don20_.dn;

    // Donchian position: (close - dn) / (up - dn)
    double don_range = don20_.up - don20_.dn;
    double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                     ? (prevClose_ - don20_.dn) / don_range
                     : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = don_pos;
}

int UnifiedFeatureEngineV2::warmup_remaining() const {
    // Conservative: max lookback across all indicators
    int max_period = std::max({
        cfg_.rsi14, cfg_.rsi21, cfg_.atr14, cfg_.bb20,
        cfg_.stoch14, cfg_.will14, cfg_.macd_slow, cfg_.don20,
        cfg_.sma50, cfg_.ema50
    });

    return std::max(0, max_period - static_cast<int>(bar_count_));
}

void UnifiedFeatureEngineV2::reset() {
    *this = UnifiedFeatureEngineV2(cfg_);
}

std::string UnifiedFeatureEngineV2::serialize() const {
    std::ostringstream os;
    os << std::setprecision(17);

    os << "prevClose " << prevClose_ << "\n";
    os << "prevOpen " << prevOpen_ << "\n";
    os << "prevHigh " << prevHigh_ << "\n";
    os << "prevLow " << prevLow_ << "\n";
    os << "prevVolume " << prevVolume_ << "\n";
    os << "bar_count " << bar_count_ << "\n";
    os << "obv " << obv_.value << "\n";
    os << "vwap " << vwap_.sumPV << " " << vwap_.sumV << "\n";

    // Add EMA/indicator states if exact resume needed
    // (Omitted for brevity; can be extended)

    return os.str();
}

void UnifiedFeatureEngineV2::restore(const std::string& blob) {
    reset();

    std::istringstream is(blob);
    std::string key;

    while (is >> key) {
        if (key == "prevClose") is >> prevClose_;
        else if (key == "prevOpen") is >> prevOpen_;
        else if (key == "prevHigh") is >> prevHigh_;
        else if (key == "prevLow") is >> prevLow_;
        else if (key == "prevVolume") is >> prevVolume_;
        else if (key == "bar_count") is >> bar_count_;
        else if (key == "obv") is >> obv_.value;
        else if (key == "vwap") is >> vwap_.sumPV >> vwap_.sumV;
    }
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 9 of 10**: src/learning/online_predictor.cpp

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

## 📄 **FILE 10 of 10**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 647 lines
- **Modified**: 2025-10-08 13:24:17

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

    // Initialize feature engine V2 (production-grade with O(1) updates)
    features::EngineConfig engine_config;
    engine_config.momentum = true;
    engine_config.volatility = true;
    engine_config.volume = true;
    engine_config.normalize = true;
    feature_engine_v2_ = std::make_unique<features::UnifiedFeatureEngineV2>(engine_config);

    // Get feature count from V2 engine schema
    size_t num_features = feature_engine_v2_->names().size();
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
    // CRITICAL: Ensure learning is current before generating signal
    if (!ensure_learning_current(bar)) {
        throw std::runtime_error(
            "[OnlineEnsemble] FATAL: Cannot generate signal - learning state is not current. "
            "Bar ID: " + std::to_string(bar.bar_id) +
            ", Last trained: " + std::to_string(learning_state_.last_trained_bar_id) +
            ", Bars behind: " + std::to_string(learning_state_.bars_behind));
    }

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

    // Update feature engine V2
    feature_engine_v2_->update(bar);

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);

    // Update learning state after processing this bar
    learning_state_.last_trained_bar_id = bar.bar_id;
    learning_state_.last_trained_bar_index = samples_seen_ - 1;  // 0-indexed
    learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
    learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
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

    // UnifiedFeatureEngineV2 maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_v2_->is_seeded()) {
        if (extract_count <= 10) {
            std::cout << "[OES] extract_features #" << extract_count
                      << ": feature_engine_v2 NOT ready → returning empty"
                      << std::endl;
        }
        return {};
    }

    // Get features from V2 engine (returns const vector& - no copy)
    const auto& features_view = feature_engine_v2_->features_view();
    std::vector<double> features(features_view.begin(), features_view.end());
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

// ============================================================================
// Learning State Management - Ensures model is always current before signals
// ============================================================================

bool OnlineEnsembleStrategy::ensure_learning_current(const Bar& bar) {
    // Check if this is the first bar (initial state)
    if (learning_state_.last_trained_bar_id == -1) {
        // First bar - just update state, don't train yet
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // Check if we're already current with this bar
    if (learning_state_.last_trained_bar_id == bar.bar_id) {
        return true;  // Already trained on this bar
    }

    // Calculate how many bars behind we are
    int64_t bars_behind = bar.bar_id - learning_state_.last_trained_bar_id;

    if (bars_behind < 0) {
        // Going backwards in time - this should only happen during replay/testing
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Bar ID went backwards! "
                  << "Current: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << " (replaying historical data)" << std::endl;

        // Reset learning state for replay
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    if (bars_behind == 0) {
        return true;  // Current bar
    }

    if (bars_behind == 1) {
        // Normal case: exactly 1 bar behind (typical sequential processing)
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // We're more than 1 bar behind - need to catch up
    learning_state_.bars_behind = static_cast<int>(bars_behind);
    learning_state_.is_learning_current = false;

    // Only warn if feature engine is warmed up
    // (during warmup, it's normal to skip bars)
    if (learning_state_.is_warmed_up) {
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Learning engine is " << bars_behind << " bars behind!"
                  << std::endl;
        std::cerr << "    Current bar ID: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << std::endl;
        std::cerr << "    This should only happen during warmup. Once warmed up, "
                  << "the system must stay fully updated." << std::endl;

        // In production live trading, this is FATAL
        // Cannot generate signals without being current
        return false;
    }

    // During warmup, it's OK to be behind
    // Mark as current and continue
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
    return true;
}

} // namespace sentio

```

