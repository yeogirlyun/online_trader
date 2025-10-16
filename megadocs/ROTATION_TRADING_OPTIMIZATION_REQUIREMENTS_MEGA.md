# ROTATION_TRADING_OPTIMIZATION_REQUIREMENTS - Complete Analysis

**Generated**: 2025-10-15 23:57:00
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: /Volumes/ExternalSSD/Dev/C++/online_trader/megadocs/ROTATION_TRADING_OPTIMIZATION_REQUIREMENTS.md
**Total Files**: 32

---

## 📋 **TABLE OF CONTENTS**

1. [config/best_params.json](#file-1)
2. [config/rotation_strategy.json](#file-2)
3. [include/backend/adaptive_trading_mechanism.h](#file-3)
4. [include/backend/rotation_trading_backend.h](#file-4)
5. [include/cli/rotation_trade_command.h](#file-5)
6. [include/common/config_loader.h](#file-6)
7. [include/common/time_utils.h](#file-7)
8. [include/common/types.h](#file-8)
9. [include/data/mock_multi_symbol_feed.h](#file-9)
10. [include/data/multi_symbol_data_manager.h](#file-10)
11. [include/features/unified_feature_engine.h](#file-11)
12. [include/learning/online_predictor.h](#file-12)
13. [include/live/alpaca_client.hpp](#file-13)
14. [include/live/polygon_client.hpp](#file-14)
15. [include/live/position_book.h](#file-15)
16. [include/strategy/online_ensemble_strategy.h](#file-16)
17. [scripts/rotation_trading_aggregate_dashboard.py](#file-17)
18. [scripts/rotation_trading_dashboard.py](#file-18)
19. [src/backend/adaptive_trading_mechanism.cpp](#file-19)
20. [src/backend/rotation_trading_backend.cpp](#file-20)
21. [src/cli/command_registry.cpp](#file-21)
22. [src/cli/rotation_trade_command.cpp](#file-22)
23. [src/common/config_loader.cpp](#file-23)
24. [src/common/time_utils.cpp](#file-24)
25. [src/data/mock_multi_symbol_feed.cpp](#file-25)
26. [src/data/multi_symbol_data_manager.cpp](#file-26)
27. [src/features/unified_feature_engine.cpp](#file-27)
28. [src/learning/online_predictor.cpp](#file-28)
29. [src/live/alpaca_client.cpp](#file-29)
30. [src/live/position_book.cpp](#file-30)
31. [src/strategy/market_regime_detector.cpp](#file-31)
32. [src/strategy/online_ensemble_strategy.cpp](#file-32)

---

## 📄 **FILE 1 of 32**: config/best_params.json

**File Information**:
- **Path**: `config/best_params.json`
- **Size**: 23 lines
- **Modified**: 2025-10-10 09:21:21
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "last_updated": "2025-10-10T09:21:21Z",
  "optimization_source": "2phase_optuna_premarket",
  "optimization_date": "2025-10-10",
  "data_used": "SPY_RTH_NH_5years.csv",
  "n_trials_phase1": 50,
  "n_trials_phase2": 0,
  "best_mrd": 0.0,
  "parameters": {
    "buy_threshold": 0.501,
    "sell_threshold": 0.49,
    "ewrls_lambda": 0.988,
    "bb_amplification_factor": 0.29000000000000004,
    "h1_weight": 0.5,
    "h5_weight": 0.3,
    "h10_weight": 0.2,
    "bb_period": 20,
    "bb_std_dev": 2.0,
    "bb_proximity": 0.01,
    "regularization": 0.001
  },
  "note": "Optimized for live trading session on 2025-10-10"
}
```

## 📄 **FILE 2 of 32**: config/rotation_strategy.json

**File Information**:
- **Path**: `config/rotation_strategy.json`
- **Size**: 103 lines
- **Modified**: 2025-10-15 20:42:06
- **Type**: json
- **Permissions**: -rw-r--r--

```text
{
  "name": "Multi-Symbol Rotation Strategy v2.0",
  "description": "6-symbol leveraged ETF rotation with OnlineEnsemble learning + VIX exposure",
  "version": "2.0.1",

  "symbols": {
    "active": ["TQQQ", "SQQQ", "SPXL", "SDS", "UVXY", "SVIX"],
    "leverage_boosts": {
      "TQQQ": 1.5,
      "SQQQ": 1.5,
      "SPXL": 1.5,
      "SDS": 1.4,
      "UVXY": 1.6,
      "SVIX": 1.3
    }
  },

  "oes_config": {
    "ewrls_lambda": 0.98,
    "initial_variance": 10.0,
    "regularization": 0.1,
    "warmup_samples": 100,

    "prediction_horizons": [1, 5, 10],
    "horizon_weights": [0.3, 0.5, 0.2],

    "buy_threshold": 0.53,
    "sell_threshold": 0.47,
    "neutral_zone": 0.06,

    "enable_bb_amplification": true,
    "bb_period": 20,
    "bb_std_dev": 2.0,
    "bb_proximity_threshold": 0.30,
    "bb_amplification_factor": 0.10,

    "enable_threshold_calibration": true,
    "calibration_window": 200,
    "target_win_rate": 0.60,
    "threshold_step": 0.005,

    "enable_kelly_sizing": false,
    "kelly_fraction": 0.25,
    "max_position_size": 0.50
  },

  "signal_aggregator_config": {
    "min_probability": 0.51,
    "min_confidence": 0.01,
    "min_strength": 0.005,

    "enable_correlation_filter": false,
    "max_correlation": 0.85,

    "filter_stale_signals": true,
    "max_staleness_seconds": 120.0
  },

  "rotation_manager_config": {
    "max_positions": 3,
    "min_rank_to_hold": 5,
    "min_strength_to_enter": 0.008,
    "min_strength_to_hold": 0.006,
    "min_strength_to_exit": 0.004,

    "rotation_strength_delta": 0.15,
    "rotation_cooldown_bars": 10,
    "minimum_hold_bars": 5,

    "equal_weight": true,
    "volatility_weight": false,
    "capital_per_position": 0.33,

    "enable_profit_target": true,
    "profit_target_pct": 0.03,
    "enable_stop_loss": true,
    "stop_loss_pct": 0.015,

    "eod_liquidation": true,
    "eod_exit_time_minutes": 388
  },

  "data_manager_config": {
    "max_forward_fills": 5,
    "max_staleness_seconds": 300.0,
    "history_size": 500,
    "log_data_quality": true
  },

  "performance_targets": {
    "target_mrd": 0.005,
    "target_win_rate": 0.60,
    "target_sharpe": 2.0,
    "max_drawdown": 0.05
  },

  "notes": {
    "eod_liquidation": "All positions closed at 3:58 PM ET - eliminates overnight decay risk",
    "leverage_boost": "Prioritizes leveraged ETFs due to higher profit potential with EOD exit",
    "rotation_logic": "Capital flows to strongest signals - simpler than PSM",
    "independence": "Each symbol learns independently - no cross-contamination"
  }
}

```

## 📄 **FILE 3 of 32**: include/backend/adaptive_trading_mechanism.h

**File Information**:
- **Path**: `include/backend/adaptive_trading_mechanism.h`
- **Size**: 504 lines
- **Modified**: 2025-10-08 07:44:51
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "common/types.h"
#include "strategy/signal_output.h"

// Forward declarations to avoid circular dependencies
namespace sentio {
    class BackendComponent;
}

namespace sentio {

// ===================================================================
// THRESHOLD PAIR STRUCTURE
// ===================================================================

/**
 * @brief Represents a pair of buy and sell thresholds for trading decisions
 * 
 * The ThresholdPair encapsulates the core decision boundaries for the adaptive
 * trading system. Buy threshold determines when signals trigger buy orders,
 * sell threshold determines sell orders, with a neutral zone between them.
 */
struct ThresholdPair {
    double buy_threshold = 0.6;   // Probability threshold for buy orders
    double sell_threshold = 0.4;  // Probability threshold for sell orders
    
    ThresholdPair() = default;
    ThresholdPair(double buy, double sell) : buy_threshold(buy), sell_threshold(sell) {}
    
    /**
     * @brief Validates that thresholds are within acceptable bounds
     * @return true if thresholds are valid, false otherwise
     */
    bool is_valid() const {
        return buy_threshold > sell_threshold + 0.05 && // Min 5% gap
               buy_threshold >= 0.51 && buy_threshold <= 0.90 &&
               sell_threshold >= 0.10 && sell_threshold <= 0.49;
    }
    
    /**
     * @brief Gets the size of the neutral zone between thresholds
     * @return Size of neutral zone (buy_threshold - sell_threshold)
     */
    double get_neutral_zone_size() const {
        return buy_threshold - sell_threshold;
    }
};

// ===================================================================
// MARKET STATE AND REGIME DETECTION
// ===================================================================

/**
 * @brief Enumeration of different market regimes for adaptive threshold selection
 * @deprecated Use MarketRegime from market_regime_detector.h instead
 */
enum class AdaptiveMarketRegime {
    BULL_LOW_VOL,     // Rising prices, low volatility - aggressive thresholds
    BULL_HIGH_VOL,    // Rising prices, high volatility - moderate thresholds
    BEAR_LOW_VOL,     // Falling prices, low volatility - moderate thresholds
    BEAR_HIGH_VOL,    // Falling prices, high volatility - conservative thresholds
    SIDEWAYS_LOW_VOL, // Range-bound, low volatility - balanced thresholds
    SIDEWAYS_HIGH_VOL // Range-bound, high volatility - conservative thresholds
};

/**
 * @brief Comprehensive market state information for adaptive decision making
 */
struct MarketState {
    double current_price = 0.0;
    double volatility = 0.0;          // 20-day volatility measure
    double trend_strength = 0.0;      // -1 (strong bear) to +1 (strong bull)
    double volume_ratio = 1.0;        // Current volume / average volume
    AdaptiveMarketRegime regime = AdaptiveMarketRegime::SIDEWAYS_LOW_VOL;
    
    // Signal distribution statistics
    double avg_signal_strength = 0.5;
    double signal_volatility = 0.1;
    
    // Portfolio state
    int open_positions = 0;
    double cash_utilization = 0.0;    // 0.0 to 1.0
};

/**
 * @brief Detects and classifies market regimes for adaptive threshold optimization
 * @deprecated Use MarketRegimeDetector from market_regime_detector.h instead
 *
 * The AdaptiveMarketRegimeDetector analyzes price history, volatility, and trend patterns
 * to classify current market conditions. This enables regime-specific threshold
 * optimization for improved performance across different market environments.
 */
class AdaptiveMarketRegimeDetector {
private:
    std::vector<double> price_history_;
    std::vector<double> volume_history_;
    const size_t LOOKBACK_PERIOD = 20;
    
public:
    /**
     * @brief Analyzes current market conditions and returns comprehensive market state
     * @param current_bar Current market data bar
     * @param recent_history Vector of recent bars for trend analysis
     * @param signal Current signal for context
     * @return MarketState with regime classification and metrics
     */
    MarketState analyze_market_state(const Bar& current_bar, 
                                   const std::vector<Bar>& recent_history,
                                   const SignalOutput& signal);
    
private:
    double calculate_volatility();
    double calculate_trend_strength();
    double calculate_volume_ratio();
    AdaptiveMarketRegime classify_market_regime(double volatility, double trend_strength);
};

// ===================================================================
// PERFORMANCE TRACKING AND EVALUATION
// ===================================================================

/**
 * @brief Represents the outcome of a completed trade for learning feedback
 */
struct TradeOutcome {
    // Store essential trade information instead of full TradeOrder to avoid circular dependency
    std::string symbol;
    TradeAction action = TradeAction::HOLD;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    double actual_pnl = 0.0;
    double pnl_percentage = 0.0;
    bool was_profitable = false;
    int bars_to_profit = 0;
    double max_adverse_move = 0.0;
    double sharpe_contribution = 0.0;
    std::chrono::system_clock::time_point outcome_timestamp;
};

/**
 * @brief Comprehensive performance metrics for adaptive learning evaluation
 */
struct PerformanceMetrics {
    double win_rate = 0.0;              // Percentage of profitable trades
    double profit_factor = 1.0;         // Gross profit / Gross loss
    double sharpe_ratio = 0.0;          // Risk-adjusted return
    double max_drawdown = 0.0;          // Maximum peak-to-trough decline
    double trade_frequency = 0.0;       // Trades per day
    double capital_efficiency = 0.0;    // Return on deployed capital
    double opportunity_cost = 0.0;      // Estimated missed profits
    std::vector<double> returns;        // Historical returns
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;
};

/**
 * @brief Evaluates trading performance and generates learning signals
 * 
 * The PerformanceEvaluator tracks trade outcomes, calculates comprehensive
 * performance metrics, and generates reward signals for the learning algorithms.
 * It maintains rolling windows of performance data for adaptive optimization.
 */
class PerformanceEvaluator {
private:
    std::vector<TradeOutcome> trade_history_;
    std::vector<double> portfolio_values_;
    const size_t MAX_HISTORY = 1000;
    const size_t PERFORMANCE_WINDOW = 100;
    
public:
    /**
     * @brief Adds a completed trade outcome for performance tracking
     * @param outcome TradeOutcome with P&L and timing information
     */
    void add_trade_outcome(const TradeOutcome& outcome);
    
    /**
     * @brief Adds portfolio value snapshot for drawdown calculation
     * @param value Current total portfolio value
     */
    void add_portfolio_value(double value);
    
    /**
     * @brief Calculates comprehensive performance metrics from recent trades
     * @return PerformanceMetrics with win rate, Sharpe ratio, drawdown, etc.
     */
    PerformanceMetrics calculate_performance_metrics();
    
    /**
     * @brief Calculates reward signal for learning algorithms
     * @param metrics Current performance metrics
     * @return Reward value for reinforcement learning
     */
    double calculate_reward_signal(const PerformanceMetrics& metrics);
    
private:
    double calculate_sharpe_ratio(const std::vector<double>& returns);
    double calculate_max_drawdown();
    double calculate_capital_efficiency();
};

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER
// ===================================================================

/**
 * @brief State-action pair for Q-learning lookup table
 */
struct StateActionPair {
    int state_hash;
    int action_index;
    
    bool operator<(const StateActionPair& other) const {
        return std::tie(state_hash, action_index) < std::tie(other.state_hash, other.action_index);
    }
};

/**
 * @brief Available actions for threshold adjustment in Q-learning
 */
enum class ThresholdAction {
    INCREASE_BUY_SMALL,      // +0.01
    INCREASE_BUY_MEDIUM,     // +0.03
    DECREASE_BUY_SMALL,      // -0.01
    DECREASE_BUY_MEDIUM,     // -0.03
    INCREASE_SELL_SMALL,     // +0.01
    INCREASE_SELL_MEDIUM,    // +0.03
    DECREASE_SELL_SMALL,     // -0.01
    DECREASE_SELL_MEDIUM,    // -0.03
    MAINTAIN_THRESHOLDS,     // No change
    COUNT
};

/**
 * @brief Q-Learning based threshold optimizer for adaptive trading
 * 
 * Implements reinforcement learning to find optimal buy/sell thresholds.
 * Uses epsilon-greedy exploration and Q-value updates to learn from
 * trading outcomes and maximize long-term performance.
 */
class QLearningThresholdOptimizer {
private:
    std::map<StateActionPair, double> q_table_;
    std::map<int, int> state_visit_count_;
    
    // Hyperparameters
    double learning_rate_ = 0.1;
    double discount_factor_ = 0.95;
    double exploration_rate_ = 0.1;
    double exploration_decay_ = 0.995;
    double min_exploration_ = 0.01;
    
    // State discretization
    const int THRESHOLD_BINS = 20;
    const int PERFORMANCE_BINS = 10;
    
    std::mt19937 rng_;
    
public:
    QLearningThresholdOptimizer();
    
    /**
     * @brief Selects next action using epsilon-greedy policy
     * @param state Current market state
     * @param current_thresholds Current threshold values
     * @param performance Recent performance metrics
     * @return Selected threshold action
     */
    ThresholdAction select_action(const MarketState& state, 
                                 const ThresholdPair& current_thresholds,
                                 const PerformanceMetrics& performance);
    
    /**
     * @brief Updates Q-value based on observed reward
     * @param prev_state Previous market state
     * @param prev_thresholds Previous thresholds
     * @param prev_performance Previous performance
     * @param action Action taken
     * @param reward Observed reward
     * @param new_state New market state
     * @param new_thresholds New thresholds
     * @param new_performance New performance
     */
    void update_q_value(const MarketState& prev_state,
                       const ThresholdPair& prev_thresholds,
                       const PerformanceMetrics& prev_performance,
                       ThresholdAction action,
                       double reward,
                       const MarketState& new_state,
                       const ThresholdPair& new_thresholds,
                       const PerformanceMetrics& new_performance);
    
    /**
     * @brief Applies selected action to current thresholds
     * @param current_thresholds Current threshold values
     * @param action Action to apply
     * @return New threshold values after action
     */
    ThresholdPair apply_action(const ThresholdPair& current_thresholds, ThresholdAction action);
    
    /**
     * @brief Gets current learning progress (1.0 - exploration_rate)
     * @return Learning progress from 0.0 to 1.0
     */
    double get_learning_progress() const;
    
private:
    int discretize_state(const MarketState& state, 
                        const ThresholdPair& thresholds,
                        const PerformanceMetrics& performance);
    double get_q_value(const StateActionPair& sa_pair);
    double get_max_q_value(int state_hash);
    ThresholdAction get_best_action(int state_hash);
};

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER
// ===================================================================

/**
 * @brief Represents a bandit arm (threshold combination) with statistics
 */
struct BanditArm {
    ThresholdPair thresholds;
    double estimated_reward = 0.0;
    int pull_count = 0;
    double confidence_bound = 0.0;
    
    BanditArm(const ThresholdPair& t) : thresholds(t) {}
};

/**
 * @brief Multi-Armed Bandit optimizer for threshold selection
 * 
 * Implements UCB1 algorithm to balance exploration and exploitation
 * across different threshold combinations. Maintains confidence bounds
 * for each arm and selects based on upper confidence bounds.
 */
class MultiArmedBanditOptimizer {
private:
    std::vector<BanditArm> arms_;
    int total_pulls_ = 0;
    std::mt19937 rng_;
    
public:
    MultiArmedBanditOptimizer();
    
    /**
     * @brief Selects threshold pair using UCB1 algorithm
     * @return Selected threshold pair
     */
    ThresholdPair select_thresholds();
    
    /**
     * @brief Updates reward for selected threshold pair
     * @param thresholds Threshold pair that was used
     * @param reward Observed reward
     */
    void update_reward(const ThresholdPair& thresholds, double reward);
    
private:
    void initialize_arms();
    void update_confidence_bounds();
};

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER - Main Orchestrator
// ===================================================================

/**
 * @brief Learning algorithm selection for adaptive threshold optimization
 */
enum class LearningAlgorithm {
    Q_LEARNING,           // Reinforcement learning approach
    MULTI_ARMED_BANDIT,   // UCB1 bandit algorithm
    ENSEMBLE              // Combination of multiple algorithms
};

/**
 * @brief Configuration parameters for adaptive threshold system
 */
struct AdaptiveConfig {
    LearningAlgorithm algorithm = LearningAlgorithm::Q_LEARNING;
    double learning_rate = 0.1;
    double exploration_rate = 0.1;
    int performance_window = 50;
    int feedback_delay = 5;
    double max_drawdown_limit = 0.05;
    bool enable_regime_adaptation = true;
    bool conservative_mode = false;
};

/**
 * @brief Main orchestrator for adaptive threshold management
 * 
 * The AdaptiveThresholdManager coordinates all components of the adaptive
 * trading system. It manages learning algorithms, performance evaluation,
 * market regime detection, and provides the main interface for getting
 * optimal thresholds and processing trade outcomes.
 */
class AdaptiveThresholdManager {
private:
    // Current state
    ThresholdPair current_thresholds_;
    MarketState current_market_state_;
    PerformanceMetrics current_performance_;
    
    // Learning components
    std::unique_ptr<QLearningThresholdOptimizer> q_learner_;
    std::unique_ptr<MultiArmedBanditOptimizer> bandit_optimizer_;
    std::unique_ptr<AdaptiveMarketRegimeDetector> regime_detector_;
    std::unique_ptr<PerformanceEvaluator> performance_evaluator_;

    // Configuration
    AdaptiveConfig config_;

    // State tracking
    std::queue<std::pair<TradeOutcome, std::chrono::system_clock::time_point>> pending_trades_;
    std::vector<Bar> recent_bars_;
    bool learning_enabled_ = true;
    bool circuit_breaker_active_ = false;

    // Regime-specific thresholds
    std::map<AdaptiveMarketRegime, ThresholdPair> regime_thresholds_;
    
public:
    /**
     * @brief Constructs adaptive threshold manager with configuration
     * @param config Configuration parameters for the adaptive system
     */
    AdaptiveThresholdManager(const AdaptiveConfig& config = AdaptiveConfig());
    
    /**
     * @brief Gets current optimal thresholds for given market conditions
     * @param signal Current signal output
     * @param bar Current market data bar
     * @return Optimal threshold pair for current conditions
     */
    ThresholdPair get_current_thresholds(const SignalOutput& signal, const Bar& bar);
    
    /**
     * @brief Processes trade outcome for learning feedback
     * @param symbol Trade symbol
     * @param action Trade action (BUY/SELL)
     * @param quantity Trade quantity
     * @param price Trade price
     * @param trade_value Trade value
     * @param fees Trade fees
     * @param actual_pnl Actual profit/loss from trade
     * @param pnl_percentage P&L as percentage of trade value
     * @param was_profitable Whether trade was profitable
     */
    void process_trade_outcome(const std::string& symbol, TradeAction action, 
                              double quantity, double price, double trade_value, double fees,
                              double actual_pnl, double pnl_percentage, bool was_profitable);
    
    /**
     * @brief Updates portfolio value for performance tracking
     * @param value Current total portfolio value
     */
    void update_portfolio_value(double value);
    
    // Control methods
    void enable_learning(bool enabled) { learning_enabled_ = enabled; }
    void reset_circuit_breaker() { circuit_breaker_active_ = false; }
    bool is_circuit_breaker_active() const { return circuit_breaker_active_; }
    
    // Analytics methods
    PerformanceMetrics get_current_performance() const { return current_performance_; }
    MarketState get_current_market_state() const { return current_market_state_; }
    double get_learning_progress() const;
    
    /**
     * @brief Generates comprehensive performance report
     * @return Formatted string with performance metrics and insights
     */
    std::string generate_performance_report() const;
    
private:
    void initialize_regime_thresholds();
    void update_performance_and_learn();
    ThresholdPair get_regime_adapted_thresholds();
    ThresholdPair get_conservative_thresholds();
    void check_circuit_breaker();
};

} // namespace sentio

```

## 📄 **FILE 4 of 32**: include/backend/rotation_trading_backend.h

**File Information**:
- **Path**: `include/backend/rotation_trading_backend.h`
- **Size**: 364 lines
- **Modified**: 2025-10-15 15:31:55
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/multi_symbol_oes_manager.h"
#include "strategy/signal_aggregator.h"
#include "strategy/rotation_position_manager.h"
#include "data/multi_symbol_data_manager.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace sentio {

/**
 * @brief Complete trading backend for multi-symbol rotation strategy
 *
 * This backend integrates all Phase 1 and Phase 2 components into a
 * unified trading system:
 *
 * Phase 1 (Data):
 * - MultiSymbolDataManager (async data handling)
 * - IBarFeed (live or mock data source)
 *
 * Phase 2 (Strategy):
 * - MultiSymbolOESManager (6 independent learners)
 * - SignalAggregator (rank by strength)
 * - RotationPositionManager (hold top N, rotate)
 *
 * Integration:
 * - Broker interface (Alpaca for live, mock for testing)
 * - Performance tracking (MRD, Sharpe, win rate)
 * - Trade logging (signals, decisions, executions)
 * - EOD management (liquidate at 3:58 PM ET)
 *
 * Usage:
 *   RotationTradingBackend backend(config);
 *   backend.warmup(historical_data);
 *   backend.start_trading();
 *
 *   // Each bar:
 *   backend.on_bar();
 *
 *   backend.stop_trading();
 */
class RotationTradingBackend {
public:
    struct Config {
        // Symbols to trade
        std::vector<std::string> symbols = {
            "TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"
        };

        // Component configurations
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        SignalAggregator::Config aggregator_config;
        RotationPositionManager::Config rotation_config;
        data::MultiSymbolDataManager::Config data_config;

        // Trading parameters
        double starting_capital = 100000.0;
        bool enable_trading = true;        // If false, only log signals
        bool log_all_signals = true;       // Log all signals (not just trades)
        bool log_all_decisions = true;     // Log all position decisions

        // Output paths
        std::string signal_log_path = "logs/live_trading/signals.jsonl";
        std::string decision_log_path = "logs/live_trading/decisions.jsonl";
        std::string trade_log_path = "logs/live_trading/trades.jsonl";
        std::string position_log_path = "logs/live_trading/positions.jsonl";

        // Performance tracking
        bool enable_performance_tracking = true;
        int performance_window = 200;      // Bars for rolling metrics

        // Broker (for live trading)
        std::string alpaca_api_key = "";
        std::string alpaca_secret_key = "";
        bool paper_trading = true;
    };

    /**
     * @brief Trading session statistics
     */
    struct SessionStats {
        int bars_processed = 0;
        int signals_generated = 0;
        int trades_executed = 0;
        int positions_opened = 0;
        int positions_closed = 0;
        int rotations = 0;

        double total_pnl = 0.0;
        double total_pnl_pct = 0.0;
        double current_equity = 0.0;
        double max_equity = 0.0;
        double min_equity = 0.0;
        double max_drawdown = 0.0;

        double win_rate = 0.0;
        double avg_win_pct = 0.0;
        double avg_loss_pct = 0.0;
        double sharpe_ratio = 0.0;
        double mrd = 0.0;  // Mean Return per Day

        std::chrono::system_clock::time_point session_start;
        std::chrono::system_clock::time_point session_end;
    };

    /**
     * @brief Construct backend
     *
     * @param config Configuration
     * @param data_mgr Data manager (optional, will create if not provided)
     * @param broker Broker client (optional, for live trading)
     */
    explicit RotationTradingBackend(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr = nullptr,
        std::shared_ptr<AlpacaClient> broker = nullptr
    );

    ~RotationTradingBackend();

    // === Trading Session Management ===

    /**
     * @brief Warmup strategy with historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Start trading session
     *
     * Opens log files, initializes session stats.
     *
     * @return true if started successfully
     */
    bool start_trading();

    /**
     * @brief Stop trading session
     *
     * Closes all positions, finalizes logs, prints summary.
     */
    void stop_trading();

    /**
     * @brief Process new bar (main trading loop)
     *
     * This is the core method called each minute:
     * 1. Update data manager
     * 2. Generate signals (6 symbols)
     * 3. Rank signals by strength
     * 4. Make position decisions
     * 5. Execute trades
     * 6. Update learning
     * 7. Log everything
     *
     * @return true if processing successful
     */
    bool on_bar();

    /**
     * @brief Check if EOD time reached
     *
     * @param current_time_minutes Minutes since market open (9:30 AM ET)
     * @return true if at or past EOD exit time
     */
    bool is_eod(int current_time_minutes) const;

    /**
     * @brief Liquidate all positions (EOD or emergency)
     *
     * @param reason Reason for liquidation
     * @return true if liquidation successful
     */
    bool liquidate_all_positions(const std::string& reason = "EOD");

    // === State Access ===

    /**
     * @brief Check if backend is ready to trade
     *
     * @return true if all components warmed up
     */
    bool is_ready() const;

    /**
     * @brief Get current session statistics
     *
     * @return Session stats
     */
    const SessionStats& get_session_stats() const { return session_stats_; }

    /**
     * @brief Get current equity
     *
     * @return Current equity (cash + unrealized P&L)
     */
    double get_current_equity() const;

    /**
     * @brief Get current positions
     *
     * @return Map of symbol → position
     */
    const std::map<std::string, RotationPositionManager::Position>& get_positions() const {
        return rotation_manager_->get_positions();
    }

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config);

private:
    /**
     * @brief Generate all signals
     *
     * @return Map of symbol → signal
     */
    std::map<std::string, SignalOutput> generate_signals();

    /**
     * @brief Rank signals by strength
     *
     * @param signals Map of symbol → signal
     * @return Ranked signals (strongest first)
     */
    std::vector<SignalAggregator::RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals
    );

    /**
     * @brief Make position decisions
     *
     * @param ranked_signals Ranked signals
     * @return Position decisions
     */
    std::vector<RotationPositionManager::PositionDecision> make_decisions(
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals
    );

    /**
     * @brief Execute position decision
     *
     * @param decision Position decision
     * @return true if execution successful
     */
    bool execute_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Get execution price for symbol
     *
     * @param symbol Symbol ticker
     * @param side BUY or SELL
     * @return Execution price
     */
    double get_execution_price(const std::string& symbol, const std::string& side);

    /**
     * @brief Calculate position size
     *
     * @param decision Position decision
     * @return Number of shares
     */
    int calculate_position_size(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Update learning with realized P&L
     */
    void update_learning();

    /**
     * @brief Log signal
     *
     * @param symbol Symbol
     * @param signal Signal output
     */
    void log_signal(const std::string& symbol, const SignalOutput& signal);

    /**
     * @brief Log position decision
     *
     * @param decision Position decision
     */
    void log_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Log trade execution
     *
     * @param decision Position decision
     * @param execution_price Price
     * @param shares Shares traded
     * @param realized_pnl Realized P&L for EXIT trades (optional)
     * @param realized_pnl_pct Realized P&L % for EXIT trades (optional)
     */
    void log_trade(
        const RotationPositionManager::PositionDecision& decision,
        double execution_price,
        int shares,
        double realized_pnl = std::numeric_limits<double>::quiet_NaN(),
        double realized_pnl_pct = std::numeric_limits<double>::quiet_NaN()
    );

    /**
     * @brief Log current positions
     */
    void log_positions();

    /**
     * @brief Update session statistics
     */
    void update_session_stats();

    /**
     * @brief Calculate current time in minutes since market open
     *
     * @return Minutes since 9:30 AM ET
     */
    int get_current_time_minutes() const;

    Config config_;

    // Core components
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::unique_ptr<MultiSymbolOESManager> oes_manager_;
    std::unique_ptr<SignalAggregator> signal_aggregator_;
    std::unique_ptr<RotationPositionManager> rotation_manager_;

    // Broker (for live trading)
    std::shared_ptr<AlpacaClient> broker_;

    // State
    bool trading_active_{false};
    bool is_warmup_{true};  // True during warmup, false during actual trading
    bool circuit_breaker_triggered_{false};  // CRITICAL FIX: Circuit breaker to stop trading after large losses
    double current_cash_;
    double allocated_capital_{0.0};  // Track capital in open positions for validation
    std::map<std::string, double> position_entry_costs_;  // CRITICAL FIX: Track entry cost per position for accurate exit accounting
    std::map<std::string, int> position_shares_;  // CRITICAL FIX: Track shares per position
    std::map<std::string, double> realized_pnls_;  // For learning updates

    // Logging
    std::ofstream signal_log_;
    std::ofstream decision_log_;
    std::ofstream trade_log_;
    std::ofstream position_log_;

    // Statistics
    SessionStats session_stats_;
    std::vector<double> equity_curve_;
    std::vector<double> returns_;
};

} // namespace sentio

```

## 📄 **FILE 5 of 32**: include/cli/rotation_trade_command.h

**File Information**:
- **Path**: `include/cli/rotation_trade_command.h`
- **Size**: 201 lines
- **Modified**: 2025-10-15 23:38:26
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "cli/command_interface.h"
#include "backend/rotation_trading_backend.h"
#include "data/multi_symbol_data_manager.h"
#include "data/mock_multi_symbol_feed.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include <string>
#include <memory>
#include <atomic>

namespace sentio {
namespace cli {

/**
 * @brief CLI command for multi-symbol rotation trading
 *
 * Supports two modes:
 * 1. Live trading: Real-time trading with Alpaca paper/live account
 * 2. Mock trading: Backtest replay from CSV files
 *
 * Usage:
 *   ./sentio_cli rotation-trade --mode live
 *   ./sentio_cli rotation-trade --mode mock --data-dir data/equities
 */
class RotationTradeCommand : public Command {
public:
    struct Options {
        // Mode selection
        bool is_mock_mode = false;
        std::string data_dir = "data/equities";

        // Symbols to trade (12 instruments - removed gold miners NUGT/DUST)
        std::vector<std::string> symbols = {
            "ERX", "ERY", "FAS", "FAZ", "SDS", "SSO", "SQQQ", "SVXY", "TNA", "TQQQ", "TZA", "UVXY"
        };

        // Capital
        double starting_capital = 100000.0;

        // Warmup configuration
        int warmup_blocks = 20;  // For live mode
        int warmup_days = 4;      // Days of historical data before test_date for warmup
        std::string warmup_dir = "data/equities";

        // Date filtering (for single-day tests)
        std::string test_date;    // YYYY-MM-DD format (empty = run all data)

        // Test period (for multi-day batch testing)
        std::string start_date;   // YYYY-MM-DD format (empty = single day mode)
        std::string end_date;     // YYYY-MM-DD format (empty = single day mode)
        bool generate_dashboards = false;  // Generate HTML dashboards for each day

        // Output paths
        std::string log_dir = "logs/rotation_trading";
        std::string dashboard_output_dir;  // For batch test dashboards (default: log_dir + "/dashboards")

        // Alpaca credentials (for live mode)
        std::string alpaca_api_key;
        std::string alpaca_secret_key;
        bool paper_trading = true;

        // Configuration file
        std::string config_file = "config/rotation_strategy.json";
    };

    RotationTradeCommand();
    explicit RotationTradeCommand(const Options& options);
    ~RotationTradeCommand() override;

    // Command interface
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "rotation-trade"; }
    std::string get_description() const override {
        return "Multi-symbol rotation trading (live or mock mode)";
    }
    void show_help() const override;

    /**
     * @brief Execute with pre-configured options
     *
     * @return 0 on success, non-zero on error
     */
    int execute_with_options();

private:
    /**
     * @brief Load configuration from JSON file
     *
     * @return Backend configuration
     */
    RotationTradingBackend::Config load_config();

    /**
     * @brief Load warmup data for all symbols
     *
     * For live mode: Loads recent historical data (warmup_blocks)
     * For mock mode: Loads from CSV files
     *
     * @return Map of symbol → bars
     */
    std::map<std::string, std::vector<Bar>> load_warmup_data();

    /**
     * @brief Execute mock trading (backtest)
     *
     * @return 0 on success, non-zero on error
     */
    int execute_mock_trading();

    /**
     * @brief Execute batch mock trading across multiple days
     *
     * Runs mock trading for each trading day in the specified range,
     * generates dashboards, and creates a summary report.
     *
     * @return 0 on success, non-zero on error
     */
    int execute_batch_mock_trading();

    /**
     * @brief Extract trading days from data within date range
     *
     * @param start_date Start date (YYYY-MM-DD)
     * @param end_date End date (YYYY-MM-DD)
     * @return Vector of trading dates
     */
    std::vector<std::string> extract_trading_days(
        const std::string& start_date,
        const std::string& end_date
    );

    /**
     * @brief Generate dashboard for a specific day's results
     *
     * @param date Trading date (YYYY-MM-DD)
     * @param output_dir Output directory for the day
     * @return 0 on success, non-zero on error
     */
    int generate_daily_dashboard(
        const std::string& date,
        const std::string& output_dir
    );

    /**
     * @brief Generate summary dashboard across all test days
     *
     * @param daily_results Results from each day
     * @param output_dir Output directory for summary
     * @return 0 on success, non-zero on error
     */
    int generate_summary_dashboard(
        const std::vector<std::map<std::string, std::string>>& daily_results,
        const std::string& output_dir
    );

    /**
     * @brief Execute live trading
     *
     * @return 0 on success, non-zero on error
     */
    int execute_live_trading();

    /**
     * @brief Setup signal handlers for graceful shutdown
     */
    void setup_signal_handlers();

    /**
     * @brief Check if EOD time reached
     *
     * @return true if at or past 3:58 PM ET
     */
    bool is_eod() const;

    /**
     * @brief Get minutes since market open (9:30 AM ET)
     *
     * @return Minutes since open
     */
    int get_minutes_since_open() const;

    /**
     * @brief Print session summary
     */
    void print_summary(const RotationTradingBackend::SessionStats& stats);

    /**
     * @brief Log system message
     */
    void log_system(const std::string& msg);

    Options options_;
    std::unique_ptr<RotationTradingBackend> backend_;
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::shared_ptr<AlpacaClient> broker_;
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 6 of 32**: include/common/config_loader.h

**File Information**:
- **Path**: `include/common/config_loader.h`
- **Size**: 30 lines
- **Modified**: 2025-10-08 03:32:46
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <string>
#include <optional>
#include "strategy/online_ensemble_strategy.h"

namespace sentio {
namespace config {

/**
 * Load best parameters from JSON file.
 *
 * This function loads optimized parameters from config/best_params.json
 * which is updated by Optuna optimization runs.
 *
 * @param config_file Path to best_params.json (default: config/best_params.json)
 * @return OnlineEnsembleConfig with loaded parameters, or std::nullopt if file not found
 */
std::optional<OnlineEnsembleStrategy::OnlineEnsembleConfig>
load_best_params(const std::string& config_file = "config/best_params.json");

/**
 * Get default config with fallback to hardcoded values.
 *
 * Tries to load from config/best_params.json first, falls back to defaults.
 */
OnlineEnsembleStrategy::OnlineEnsembleConfig get_production_config();

} // namespace config
} // namespace sentio

```

## 📄 **FILE 7 of 32**: include/common/time_utils.h

**File Information**:
- **Path**: `include/common/time_utils.h`
- **Size**: 246 lines
- **Modified**: 2025-10-10 10:35:57
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <cstdio>

namespace sentio {

/**
 * @brief Trading session configuration with timezone support
 *
 * Handles market hours, weekends, and timezone conversions.
 * Uses system timezone API for DST-aware calculations.
 */
struct TradingSession {
    std::string timezone_name;  // IANA timezone (e.g., "America/New_York")
    int market_open_hour{9};
    int market_open_minute{30};
    int market_close_hour{16};
    int market_close_minute{0};

    TradingSession(const std::string& tz_name = "America/New_York")
        : timezone_name(tz_name) {}

    /**
     * @brief Check if given time is during regular trading hours
     * @param tp System clock time point
     * @return true if within market hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a weekday
     * @param tp System clock time point
     * @return true if Monday-Friday
     */
    bool is_weekday(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Check if given time is a trading day (weekday, not holiday)
     * @param tp System clock time point
     * @return true if trading day
     * @note Holiday calendar not yet implemented - returns weekday check only
     */
    bool is_trading_day(const std::chrono::system_clock::time_point& tp) const {
        // TODO: Add holiday calendar check
        return is_weekday(tp);
    }

    /**
     * @brief Get local time string in timezone
     * @param tp System clock time point
     * @return Formatted time string "YYYY-MM-DD HH:MM:SS TZ"
     */
    std::string to_local_string(const std::chrono::system_clock::time_point& tp) const;

    /**
     * @brief Convert system time to local time in configured timezone
     * @param tp System clock time point
     * @return Local time struct
     */
    std::tm to_local_time(const std::chrono::system_clock::time_point& tp) const;
};

/**
 * @brief Get current time (always uses system UTC, convert to ET via TradingSession)
 * @return System clock time point
 */
inline std::chrono::system_clock::time_point now() {
    return std::chrono::system_clock::now();
}

/**
 * @brief Format timestamp to ISO 8601 string
 * @param tp System clock time point
 * @return ISO formatted string "YYYY-MM-DDTHH:MM:SSZ"
 */
std::string to_iso_string(const std::chrono::system_clock::time_point& tp);

/**
 * @brief Centralized ET Time Manager - ALL time operations should use this
 *
 * This class ensures consistent ET timezone handling across the entire system.
 * No direct time conversions should be done elsewhere.
 */
class ETTimeManager {
public:
    ETTimeManager() : session_("America/New_York"), use_mock_time_(false) {}

    /**
     * @brief Enable mock time mode (for replay/testing)
     * @param timestamp_ms Simulated time in milliseconds
     */
    void set_mock_time(uint64_t timestamp_ms) {
        use_mock_time_ = true;
        mock_time_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms)
        );
    }

    /**
     * @brief Disable mock time mode (return to wall-clock time)
     */
    void disable_mock_time() {
        use_mock_time_ = false;
    }

    /**
     * @brief Get current ET time as formatted string
     * @return "YYYY-MM-DD HH:MM:SS ET"
     */
    std::string get_current_et_string() const {
        return session_.to_local_string(get_time());
    }

    /**
     * @brief Get current ET time components
     * @return struct tm in ET timezone
     */
    std::tm get_current_et_tm() const {
        return session_.to_local_time(get_time());
    }

    /**
     * @brief Get current ET date as string (YYYY-MM-DD format)
     * @return Date string in format "2025-10-09"
     */
    std::string get_current_et_date() const {
        auto et_tm = get_current_et_tm();
        char buffer[11];  // "YYYY-MM-DD\0"
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                     et_tm.tm_year + 1900,
                     et_tm.tm_mon + 1,
                     et_tm.tm_mday);
        return std::string(buffer);
    }

    /**
     * @brief Check if current time is during regular trading hours (9:30 AM - 4:00 PM ET)
     */
    bool is_regular_hours() const {
        return session_.is_regular_hours(get_time()) && session_.is_trading_day(get_time());
    }

    /**
     * @brief Check if current time is in EOD liquidation window (3:58 PM - 4:00 PM ET)
     * Uses a 2-minute window to liquidate positions before market close
     */
    bool is_eod_liquidation_window() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // EOD window: 3:58 PM - 4:00 PM ET
        if (hour == 15 && minute >= 58) return true;  // 3:58-3:59 PM
        if (hour == 16 && minute == 0) return true;   // 4:00 PM exactly

        return false;
    }

    /**
     * @brief Check if current time is hourly optimization window (top of each hour during trading)
     * Used for adaptive parameter tuning based on comprehensive data (historical + today's bars)
     * Triggers at: 10:00, 11:00, 12:00, 13:00, 14:00, 15:00 (every hour during 9:30-16:00 trading)
     */
    bool is_hourly_optimization_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Hourly optimization: top of each hour (XX:00) during trading hours
        // Skip 9:00 (before market open) and 16:00 (market close)
        if (minute != 0) return false;

        // Trigger at 10:00, 11:00, 12:00, 13:00, 14:00, 15:00
        return (hour >= 10 && hour <= 15);
    }

    /**
     * @brief Check if we should liquidate positions on startup (started outside trading hours with open positions)
     */
    bool should_liquidate_on_startup(bool has_positions) const {
        if (!has_positions) return false;

        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;

        // If started after market close (after 4 PM) or before market open (before 9:30 AM),
        // and we have positions, we should liquidate
        bool after_hours = (hour >= 16) || (hour < 9) || (hour == 9 && et_tm.tm_min < 30);

        return after_hours;
    }

    /**
     * @brief Check if market has closed (>= 4:00 PM ET)
     * Used to trigger automatic shutdown after EOD liquidation
     */
    bool is_market_close_time() const {
        auto et_tm = get_current_et_tm();
        int hour = et_tm.tm_hour;
        int minute = et_tm.tm_min;

        // Market closes at 4:00 PM ET - shutdown at 4:00 PM or later
        return (hour >= 16);
    }

    /**
     * @brief Get minutes since midnight ET
     */
    int get_et_minutes_since_midnight() const {
        auto et_tm = get_current_et_tm();
        return et_tm.tm_hour * 60 + et_tm.tm_min;
    }

    /**
     * @brief Access to underlying TradingSession
     */
    const TradingSession& session() const { return session_; }

private:
    /**
     * @brief Get current time (mock or wall-clock)
     */
    std::chrono::system_clock::time_point get_time() const {
        return use_mock_time_ ? mock_time_ : now();
    }

    TradingSession session_;
    bool use_mock_time_;
    std::chrono::system_clock::time_point mock_time_;
};

/**
 * @brief Get Unix timestamp in microseconds
 * @param tp System clock time point
 * @return Microseconds since epoch
 */
inline uint64_t to_unix_micros(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        tp.time_since_epoch()
    ).count();
}

} // namespace sentio

```

## 📄 **FILE 8 of 32**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`
- **Size**: 113 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// System Constants
// -----------------------------------------------------------------------------

/// Standard block size for backtesting and signal processing
/// One block represents approximately 8 hours of trading (480 minutes)
/// This constant ensures consistency across strattest, trade, and audit commands
static constexpr size_t STANDARD_BLOCK_SIZE = 480;

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp_ms and symbol at load time
    uint64_t bar_id = 0;
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
    // Derived fields for traceability/debugging (filled by loader)
    uint32_t sequence_num = 0;   // Position in original dataset
    uint16_t block_num = 0;      // STANDARD_BLOCK_SIZE partition index
    std::string date_str;        // e.g. "2025-09-09" for human-readable logs
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio

```

## 📄 **FILE 9 of 32**: include/data/mock_multi_symbol_feed.h

**File Information**:
- **Path**: `include/data/mock_multi_symbol_feed.h`
- **Size**: 210 lines
- **Modified**: 2025-10-15 10:36:54
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "data/bar_feed_interface.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <thread>
#include <atomic>

namespace sentio {
namespace data {

/**
 * @brief Mock data feed for testing - replays historical CSV data
 *
 * Reads CSV files for multiple symbols and replays them synchronously.
 * Useful for backtesting and integration testing.
 *
 * CSV Format (per symbol):
 *   timestamp,open,high,low,close,volume
 *   1696464600000,432.15,432.89,431.98,432.56,12345678
 *
 * Usage:
 *   MockMultiSymbolFeed feed(data_manager, {
 *       {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
 *       {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"}
 *   });
 *   feed.connect();  // Load CSV files
 *   feed.start();    // Start replay in background thread
 */
class MockMultiSymbolFeed : public IBarFeed {
public:
    struct Config {
        std::map<std::string, std::string> symbol_files;  // Symbol → CSV path
        double replay_speed = 39.0;                       // Speed multiplier (0=instant)
        bool loop = false;                                // Loop replay?
        bool sync_timestamps = true;                      // Synchronize timestamps?
        std::string filter_date;                          // Filter to specific date (YYYY-MM-DD, empty = all)
    };

    /**
     * @brief Construct mock feed
     *
     * @param data_manager Data manager to feed
     * @param config Configuration
     */
    MockMultiSymbolFeed(std::shared_ptr<MultiSymbolDataManager> data_manager,
                       const Config& config);

    ~MockMultiSymbolFeed() override;

    // === IBarFeed Interface ===

    /**
     * @brief Connect to data source (loads CSV files)
     * @return true if all CSV files loaded successfully
     */
    bool connect() override;

    /**
     * @brief Disconnect from data source
     */
    void disconnect() override;

    /**
     * @brief Check if connected (data loaded)
     */
    bool is_connected() const override;

    /**
     * @brief Start feeding data (begins replay in background thread)
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop feeding data
     */
    void stop() override;

    /**
     * @brief Check if feed is active
     */
    bool is_active() const override;

    /**
     * @brief Get feed type identifier
     */
    std::string get_type() const override;

    /**
     * @brief Get symbols being fed
     */
    std::vector<std::string> get_symbols() const override;

    /**
     * @brief Set callback for bar updates
     */
    void set_bar_callback(BarCallback callback) override;

    /**
     * @brief Set callback for errors
     */
    void set_error_callback(ErrorCallback callback) override;

    /**
     * @brief Set callback for connection state changes
     */
    void set_connection_callback(ConnectionCallback callback) override;

    /**
     * @brief Get feed statistics
     */
    FeedStats get_stats() const override;

    // === Additional Mock-Specific Methods ===

    /**
     * @brief Get total bars loaded per symbol
     */
    std::map<std::string, int> get_bar_counts() const;

    /**
     * @brief Get replay progress
     */
    struct Progress {
        int bars_replayed;
        int total_bars;
        double progress_pct;
        std::string current_symbol;
        uint64_t current_timestamp;
    };

    Progress get_progress() const;

private:
    /**
     * @brief Load CSV file for a symbol
     *
     * @param symbol Symbol ticker
     * @param filepath Path to CSV file
     * @return Number of bars loaded
     */
    int load_csv(const std::string& symbol, const std::string& filepath);

    /**
     * @brief Parse CSV line into Bar
     *
     * @param line CSV line
     * @param bar Output bar
     * @return true if parsed successfully
     */
    bool parse_csv_line(const std::string& line, Bar& bar);

    /**
     * @brief Sleep for replay speed
     *
     * @param bars Number of bars to sleep for (1 = 1 minute real-time)
     */
    void sleep_for_replay(int bars = 1);

    /**
     * @brief Replay loop (runs in background thread)
     */
    void replay_loop();

    /**
     * @brief Replay single bar for all symbols
     * @return true if bars available, false if EOF
     */
    bool replay_next_bar();

    std::shared_ptr<MultiSymbolDataManager> data_manager_;
    Config config_;

    // Data storage
    struct SymbolData {
        std::deque<Bar> bars;
        size_t current_index;

        SymbolData() : current_index(0) {}
    };

    std::map<std::string, SymbolData> symbol_data_;

    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> should_stop_{false};

    // Background thread
    std::thread replay_thread_;

    // Callbacks
    BarCallback bar_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;

    // Replay state
    std::atomic<int> bars_replayed_{0};
    int total_bars_{0};
    std::atomic<int> errors_{0};
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 10 of 32**: include/data/multi_symbol_data_manager.h

**File Information**:
- **Path**: `include/data/multi_symbol_data_manager.h`
- **Size**: 259 lines
- **Modified**: 2025-10-14 22:52:31
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace sentio {
namespace data {

/**
 * @brief Snapshot of a symbol's latest data with staleness tracking
 *
 * Each symbol maintains its own update timeline. Staleness weighting
 * reduces influence of old data in signal ranking.
 */
struct SymbolSnapshot {
    Bar latest_bar;                     // Most recent bar
    uint64_t last_update_ms;            // Timestamp of last update
    double staleness_seconds;           // Age of data (seconds)
    double staleness_weight;            // Exponential decay weight: exp(-age/60)
    int forward_fill_count;             // How many times forward-filled
    bool is_valid;                      // Data is usable

    SymbolSnapshot()
        : last_update_ms(0)
        , staleness_seconds(0.0)
        , staleness_weight(1.0)
        , forward_fill_count(0)
        , is_valid(false) {}

    // Calculate staleness based on current time
    void update_staleness(uint64_t current_time_ms) {
        staleness_seconds = (current_time_ms - last_update_ms) / 1000.0;

        // Exponential decay: fresh = 1.0, 60s = 0.37, 120s = 0.14
        staleness_weight = std::exp(-staleness_seconds / 60.0);

        is_valid = (staleness_seconds < 300.0);  // Invalid after 5 minutes
    }
};

/**
 * @brief Synchronized snapshot of all symbols at a logical timestamp
 *
 * Not all symbols may be present (async data arrival). Missing symbols
 * are forward-filled from last known data.
 */
struct MultiSymbolSnapshot {
    uint64_t logical_timestamp_ms;                     // Logical time (aligned)
    std::map<std::string, SymbolSnapshot> snapshots;   // Symbol → data
    std::vector<std::string> missing_symbols;          // Symbols not updated
    int total_forward_fills;                           // Total symbols forward-filled
    bool is_complete;                                  // All symbols present
    double avg_staleness_seconds;                      // Average age across symbols

    MultiSymbolSnapshot()
        : logical_timestamp_ms(0)
        , total_forward_fills(0)
        , is_complete(false)
        , avg_staleness_seconds(0.0) {}

    // Check if snapshot is usable
    bool is_usable() const {
        // Must have at least 50% of symbols with fresh data
        int valid_count = 0;
        for (const auto& [symbol, snap] : snapshots) {
            if (snap.is_valid && snap.staleness_seconds < 120.0) {
                valid_count++;
            }
        }
        return valid_count >= (snapshots.size() / 2);
    }
};

/**
 * @brief Asynchronous multi-symbol data manager
 *
 * Core design principles:
 * 1. NON-BLOCKING: Never wait for all symbols, use latest available
 * 2. STALENESS WEIGHTING: Reduce influence of old data
 * 3. FORWARD FILL: Use last known price for missing data (max 5 fills)
 * 4. THREAD-SAFE: WebSocket updates from background thread
 *
 * Usage:
 *   auto snapshot = data_mgr->get_latest_snapshot();
 *   for (const auto& [symbol, data] : snapshot.snapshots) {
 *       double adjusted_strength = base_strength * data.staleness_weight;
 *   }
 */
class MultiSymbolDataManager {
public:
    struct Config {
        std::vector<std::string> symbols;          // Active symbols to track
        int max_forward_fills = 5;                 // Max consecutive forward fills
        double max_staleness_seconds = 300.0;      // Max age before invalid (5 min)
        int history_size = 500;                    // Bars to keep per symbol
        bool log_data_quality = true;              // Log missing/stale data
        bool backtest_mode = false;                // Disable timestamp validation for backtesting
    };

    explicit MultiSymbolDataManager(const Config& config);
    virtual ~MultiSymbolDataManager() = default;

    // === Main Interface ===

    /**
     * @brief Get latest snapshot for all symbols (non-blocking)
     *
     * Returns immediately with whatever data is available. Applies
     * staleness weighting and forward-fill for missing symbols.
     *
     * @return MultiSymbolSnapshot with latest data
     */
    MultiSymbolSnapshot get_latest_snapshot();

    /**
     * @brief Update a symbol's data (called from WebSocket/feed thread)
     *
     * Thread-safe update of symbol data. Validates and stores bar.
     *
     * @param symbol Symbol ticker
     * @param bar New bar data
     * @return true if update successful, false if validation failed
     */
    bool update_symbol(const std::string& symbol, const Bar& bar);

    /**
     * @brief Bulk update multiple symbols (for mock replay)
     *
     * @param bars Map of symbol → bar
     * @return Number of successful updates
     */
    int update_all(const std::map<std::string, Bar>& bars);

    // === History Access ===

    /**
     * @brief Get recent bars for a symbol (for volatility calculation)
     *
     * @param symbol Symbol ticker
     * @param count Number of bars to retrieve
     * @return Vector of recent bars (newest first)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, int count) const;

    /**
     * @brief Get all history for a symbol
     *
     * @param symbol Symbol ticker
     * @return Deque of all bars (oldest first)
     */
    std::deque<Bar> get_all_bars(const std::string& symbol) const;

    // === Statistics & Monitoring ===

    /**
     * @brief Get data quality stats
     */
    struct DataQualityStats {
        std::map<std::string, int> update_counts;          // Symbol → updates
        std::map<std::string, double> avg_staleness;       // Symbol → avg age
        std::map<std::string, int> forward_fill_counts;    // Symbol → fills
        int total_updates;
        int total_forward_fills;
        int total_rejections;
        double overall_avg_staleness;
    };

    DataQualityStats get_quality_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get configured symbols
     */
    const std::vector<std::string>& get_symbols() const { return config_.symbols; }

protected:
    /**
     * @brief Validate incoming bar data
     *
     * @param symbol Symbol ticker
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    bool validate_bar(const std::string& symbol, const Bar& bar);

    /**
     * @brief Forward-fill missing symbol from last known bar
     *
     * @param symbol Symbol ticker
     * @param logical_time Timestamp to use for forward-filled bar
     * @return SymbolSnapshot with forward-filled data
     */
    SymbolSnapshot forward_fill_symbol(const std::string& symbol,
                                       uint64_t logical_time);

private:
    struct SymbolState {
        std::deque<Bar> history;           // Bar history
        Bar latest_bar;                    // Most recent bar
        uint64_t last_update_ms;           // Last update timestamp
        int update_count;                  // Total updates received
        int forward_fill_count;            // Consecutive forward fills
        int rejection_count;               // Rejected updates
        double cumulative_staleness;       // For avg calculation

        SymbolState()
            : last_update_ms(0)
            , update_count(0)
            , forward_fill_count(0)
            , rejection_count(0)
            , cumulative_staleness(0.0) {}
    };

    Config config_;
    std::map<std::string, SymbolState> symbol_states_;
    mutable std::mutex data_mutex_;

    // Statistics
    std::atomic<int> total_updates_{0};
    std::atomic<int> total_forward_fills_{0};
    std::atomic<int> total_rejections_{0};

    // Current time (for testing - can be injected)
    std::function<uint64_t()> time_provider_;

    uint64_t get_current_time_ms() const {
        if (time_provider_) {
            return time_provider_();
        }
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

public:
    // For testing: inject time provider
    void set_time_provider(std::function<uint64_t()> provider) {
        time_provider_ = provider;
    }

private:
    // Helper functions
    std::string join_symbols() const;
    std::string join_vector(const std::vector<std::string>& vec) const;
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 11 of 32**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`
- **Size**: 232 lines
- **Modified**: 2025-10-15 03:28:55
- **Type**: h
- **Permissions**: -rw-r--r--

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
    bool time = true;         // Time-of-day features (8 features)
    bool patterns = true;     // Candlestick patterns (5 features)
    bool momentum = true;
    bool volatility = true;
    bool volume = true;
    bool statistics = true;

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

class UnifiedFeatureEngine {
public:
    explicit UnifiedFeatureEngine(EngineConfig cfg = {});

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
     * Get list of indicator names that are not yet ready (for debugging).
     */
    std::vector<std::string> get_unready_indicators() const;

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
    uint64_t prevTimestamp_ = 0;  // For time features
    double prevClose_ = std::numeric_limits<double>::quiet_NaN();
    double prevOpen_ = std::numeric_limits<double>::quiet_NaN();
    double prevHigh_ = std::numeric_limits<double>::quiet_NaN();
    double prevLow_ = std::numeric_limits<double>::quiet_NaN();
    double prevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // For computing 1-bar return (current close vs previous close)
    double prevPrevClose_ = std::numeric_limits<double>::quiet_NaN();

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

## 📄 **FILE 12 of 32**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`
- **Size**: 133 lines
- **Modified**: 2025-10-07 00:37:12
- **Type**: h
- **Permissions**: -rw-r--r--

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

## 📄 **FILE 13 of 32**: include/live/alpaca_client.hpp

**File Information**:
- **Path**: `include/live/alpaca_client.hpp`
- **Size**: 216 lines
- **Modified**: 2025-10-09 10:39:21
- **Type**: hpp
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_ALPACA_CLIENT_HPP
#define SENTIO_ALPACA_CLIENT_HPP

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace sentio {

/**
 * Alpaca Paper Trading API Client
 *
 * REST API client for Alpaca Markets paper trading.
 * Supports account info, positions, and order execution.
 */
class AlpacaClient {
public:
    struct Position {
        std::string symbol;
        double quantity;           // Positive for long, negative for short
        double avg_entry_price;
        double current_price;
        double market_value;
        double unrealized_pl;
        double unrealized_pl_pct;
    };

    struct AccountInfo {
        std::string account_number;
        double buying_power;
        double cash;
        double portfolio_value;
        double equity;
        double last_equity;
        bool pattern_day_trader;
        bool trading_blocked;
        bool account_blocked;
    };

    struct Order {
        std::string symbol;
        double quantity;
        std::string side;          // "buy" or "sell"
        std::string type;          // "market", "limit", etc.
        std::string time_in_force; // "day", "gtc", "ioc", "fok"
        std::optional<double> limit_price;

        // Response fields
        std::string order_id;
        std::string status;        // "new", "filled", "canceled", etc.
        double filled_qty;
        double filled_avg_price;
    };

    /**
     * Constructor
     * @param api_key Alpaca API key (APCA-API-KEY-ID)
     * @param secret_key Alpaca secret key (APCA-API-SECRET-KEY)
     * @param paper_trading Use paper trading endpoint (default: true)
     */
    AlpacaClient(const std::string& api_key,
                 const std::string& secret_key,
                 bool paper_trading = true);

    ~AlpacaClient() = default;

    /**
     * Get account information
     * GET /v2/account
     */
    std::optional<AccountInfo> get_account();

    /**
     * Get all open positions
     * GET /v2/positions
     */
    std::vector<Position> get_positions();

    /**
     * Get position for specific symbol
     * GET /v2/positions/{symbol}
     */
    std::optional<Position> get_position(const std::string& symbol);

    /**
     * Place a market order
     * POST /v2/orders
     *
     * @param symbol Stock symbol (e.g., "QQQ", "TQQQ")
     * @param quantity Number of shares (positive for buy, negative for sell)
     * @param time_in_force "day" or "gtc" (good till canceled)
     * @return Order details if successful
     */
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc");

    /**
     * Close position for a symbol
     * DELETE /v2/positions/{symbol}
     */
    bool close_position(const std::string& symbol);

    /**
     * Close all positions
     * DELETE /v2/positions
     */
    bool close_all_positions();

    /**
     * Get order by ID
     * GET /v2/orders/{order_id}
     */
    std::optional<Order> get_order(const std::string& order_id);

    /**
     * Cancel order by ID
     * DELETE /v2/orders/{order_id}
     */
    bool cancel_order(const std::string& order_id);

    /**
     * Get all open orders
     * GET /v2/orders?status=open
     */
    std::vector<Order> get_open_orders();

    /**
     * Cancel all open orders (idempotent)
     * DELETE /v2/orders
     */
    bool cancel_all_orders();

    /**
     * Check if market is open
     * GET /v2/clock
     */
    bool is_market_open();

    /**
     * Bar data structure
     */
    struct BarData {
        std::string symbol;
        uint64_t timestamp_ms;  // Unix timestamp in milliseconds
        double open;
        double high;
        double low;
        double close;
        uint64_t volume;
    };

    /**
     * Get latest bars for symbols (real-time quotes via REST API)
     * GET /v2/stocks/bars/latest
     *
     * @param symbols Vector of symbols to fetch (e.g., {"SPY", "SPXL", "SH", "SDS"})
     * @return Vector of bar data
     */
    std::vector<BarData> get_latest_bars(const std::vector<std::string>& symbols);

    /**
     * Get historical bars for a symbol
     * GET /v2/stocks/{symbol}/bars
     *
     * @param symbol Stock symbol
     * @param timeframe Timeframe (e.g., "1Min", "5Min", "1Hour", "1Day")
     * @param start Start time in RFC3339 format (e.g., "2025-01-01T09:30:00Z")
     * @param end End time in RFC3339 format
     * @param limit Maximum number of bars to return (default: 1000)
     * @return Vector of bar data
     */
    std::vector<BarData> get_bars(const std::string& symbol,
                                   const std::string& timeframe = "1Min",
                                   const std::string& start = "",
                                   const std::string& end = "",
                                   int limit = 1000);

private:
    std::string api_key_;
    std::string secret_key_;
    std::string base_url_;

    /**
     * Make HTTP GET request
     */
    std::string http_get(const std::string& endpoint);

    /**
     * Make HTTP POST request with JSON body
     */
    std::string http_post(const std::string& endpoint, const std::string& json_body);

    /**
     * Make HTTP DELETE request
     */
    std::string http_delete(const std::string& endpoint);

    /**
     * Add authentication headers
     */
    std::map<std::string, std::string> get_headers();

    /**
     * Parse JSON response
     */
    static std::optional<AccountInfo> parse_account_json(const std::string& json);
    static std::vector<Position> parse_positions_json(const std::string& json);
    static std::optional<Position> parse_position_json(const std::string& json);
    static std::optional<Order> parse_order_json(const std::string& json);
};

} // namespace sentio

#endif // SENTIO_ALPACA_CLIENT_HPP

```

## 📄 **FILE 14 of 32**: include/live/polygon_client.hpp

**File Information**:
- **Path**: `include/live/polygon_client.hpp`
- **Size**: 106 lines
- **Modified**: 2025-10-08 11:17:58
- **Type**: hpp
- **Permissions**: -rw-r--r--

```text
#ifndef SENTIO_POLYGON_CLIENT_HPP
#define SENTIO_POLYGON_CLIENT_HPP

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <deque>
#include <mutex>
#include <chrono>
#include <atomic>

namespace sentio {

/**
 * Polygon.io WebSocket Client for Real-Time Market Data
 *
 * Connects to Polygon proxy server and receives 1-minute aggregated bars
 * for SPY, SDS, SPXL, and SH in real-time.
 */
class PolygonClient {
public:
    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Constructor
     * @param proxy_url WebSocket URL for Polygon proxy (e.g., "ws://proxy.example.com:8080")
     * @param auth_key Authentication key for proxy
     */
    PolygonClient(const std::string& proxy_url, const std::string& auth_key);
    ~PolygonClient();

    /**
     * Connect to Polygon proxy and authenticate
     */
    bool connect();

    /**
     * Subscribe to symbols for 1-minute aggregates
     */
    bool subscribe(const std::vector<std::string>& symbols);

    /**
     * Start receiving data (runs in separate thread)
     * @param callback Function called when new bar arrives
     */
    void start(BarCallback callback);

    /**
     * Stop receiving data and disconnect
     */
    void stop();

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const;

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Store a bar in history (public for WebSocket callback access)
     */
    void store_bar(const std::string& symbol, const Bar& bar);

    /**
     * Update last message timestamp (called by WebSocket callback)
     */
    void update_last_message_time();

    /**
     * Check if connection is healthy (received message recently)
     */
    bool is_connection_healthy() const;

    /**
     * Get seconds since last message
     */
    int get_seconds_since_last_message() const;

private:
    std::string proxy_url_;
    std::string auth_key_;
    bool connected_;
    bool running_;

    // Health monitoring
    std::atomic<std::chrono::steady_clock::time_point> last_message_time_;
    static constexpr int HEALTH_CHECK_TIMEOUT_SECONDS = 120;  // 2 minutes

    // Thread-safe storage of recent bars (per symbol)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // WebSocket implementation
    void receive_loop(BarCallback callback);
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_HPP

```

## 📄 **FILE 15 of 32**: include/live/position_book.h

**File Information**:
- **Path**: `include/live/position_book.h`
- **Size**: 146 lines
- **Modified**: 2025-10-09 00:56:18
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "common/types.h"
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace sentio {

struct BrokerPosition {
    std::string symbol;
    int64_t qty{0};                     // For position_book internal use
    double quantity{0.0};               // For broker interface (can be fractional)
    double avg_entry_price{0.0};
    double current_price{0.0};
    double market_value{0.0};
    double unrealized_pnl{0.0};         // Note: pnl not pl
    double unrealized_pl{0.0};          // Alias for compatibility
    double unrealized_pl_pct{0.0};

    bool is_flat() const { return qty == 0 && quantity == 0.0; }
};

struct ExecutionReport {
    std::string order_id;
    std::string client_order_id;
    std::string symbol;
    std::string side;  // "buy" or "sell"
    int64_t filled_qty{0};                // Integer quantity (for position_book)
    double quantity{0.0};                 // Decimal quantity (for broker interface)
    double filled_qty_decimal{0.0};       // Filled decimal quantity
    double avg_fill_price{0.0};
    double filled_avg_price{0.0};         // Alias for compatibility
    std::string status;  // "filled", "partial_fill", "pending", etc.
    uint64_t timestamp{0};
    uint64_t timestamp_ms{0};             // Alias for compatibility
    std::string fill_type;                // "full", "partial"
};

struct ReconcileResult {
    double realized_pnl{0.0};
    int64_t filled_qty{0};
    bool flat{false};
    std::string status;
};

/**
 * @brief Position book that tracks positions and reconciles with broker
 *
 * This class maintains local position state and provides reconciliation
 * against broker truth to detect position drift.
 */
class PositionBook {
public:
    PositionBook() = default;

    /**
     * @brief Update position from execution report
     * @param exec Execution report from broker
     */
    void on_execution(const ExecutionReport& exec);

    /**
     * @brief Get current position for symbol
     * @param symbol Symbol to query
     * @return BrokerPosition (returns flat position if symbol not found)
     */
    BrokerPosition get_position(const std::string& symbol) const;

    /**
     * @brief Reconcile local positions against broker truth
     * @param broker_positions Positions from broker API
     * @throws PositionReconciliationError if drift detected
     */
    void reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions);

    /**
     * @brief Get all non-flat positions
     * @return Map of symbol -> position
     */
    std::map<std::string, BrokerPosition> get_all_positions() const;

    /**
     * @brief Get total realized P&L since timestamp
     * @param since_ts Unix timestamp in microseconds
     * @return Realized P&L in dollars
     */
    double get_realized_pnl_since(uint64_t since_ts) const;

    /**
     * @brief Get total realized P&L today
     * @return Realized P&L in dollars
     */
    double get_total_realized_pnl() const { return total_realized_pnl_; }

    /**
     * @brief Reset daily P&L (call at market open)
     */
    void reset_daily_pnl() { total_realized_pnl_ = 0.0; }

    /**
     * @brief Update current market prices for unrealized P&L calculation
     * @param symbol Symbol
     * @param price Current market price
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * @brief Set position directly (for startup reconciliation)
     * @param symbol Symbol
     * @param qty Quantity
     * @param avg_price Average entry price
     */
    void set_position(const std::string& symbol, int64_t qty, double avg_price);

    /**
     * @brief Check if all positions are flat (for EOD safety)
     * @return true if no positions held
     */
    bool is_flat() const {
        for (const auto& [symbol, pos] : positions_) {
            if (pos.qty != 0) return false;
        }
        return true;
    }

    /**
     * @brief Calculate SHA1 hash of positions (for EOD verification)
     * @return Hex string of sorted positions hash (empty string if flat)
     *
     * Format: sorted by symbol, "SYMBOL:QTY|SYMBOL:QTY|..."
     * Example: "SPY:100|TQQQ:-50" → SHA1 → hex string
     */
    std::string positions_hash() const;

private:
    std::map<std::string, BrokerPosition> positions_;
    std::vector<ExecutionReport> execution_history_;
    double total_realized_pnl_{0.0};

    void update_position_on_fill(const ExecutionReport& exec);
    double calculate_realized_pnl(const BrokerPosition& old_pos, const ExecutionReport& exec);
};

} // namespace sentio

```

## 📄 **FILE 16 of 32**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`
- **Size**: 240 lines
- **Modified**: 2025-10-15 03:27:23
- **Type**: h
- **Permissions**: -rw-r--r--

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "strategy/market_regime_detector.h"
#include "strategy/regime_parameter_manager.h"
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

        // Regime detection parameters
        bool enable_regime_detection = false;  // Enable regime-aware parameter switching
        int regime_check_interval = 100;       // Check regime every N bars
        int regime_lookback_period = 100;      // Bars to analyze for regime detection

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

    // Feature caching support (for Optuna optimization speedup)
    void set_external_features(const std::vector<double>* features) {
        external_features_ = features;
        skip_feature_engine_update_ = (features != nullptr);
    }

    // Runtime configuration update (for mid-day optimization)
    void update_config(const OnlineEnsembleConfig& new_config) {
        config_ = new_config;
        // CRITICAL: Update member variables used by determine_signal()
        current_buy_threshold_ = new_config.buy_threshold;
        current_sell_threshold_ = new_config.sell_threshold;
    }

    // Get current thresholds (for PSM decision logic)
    double get_current_buy_threshold() const { return current_buy_threshold_; }
    double get_current_sell_threshold() const { return current_sell_threshold_; }

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
    bool is_ready() const {
        // Check both predictor warmup AND feature engine warmup
        return samples_seen_ >= config_.warmup_samples &&
               feature_engine_->warmup_remaining() == 0;
    }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering (production-grade with O(1) updates, 45 features)
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

    // Learning state tracking
    LearningState learning_state_;
    std::deque<Bar> missed_bars_;  // Queue of bars that need training

    // External feature support for caching
    const std::vector<double>* external_features_ = nullptr;
    bool skip_feature_engine_update_ = false;

    // Regime detection (optional)
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<RegimeParameterManager> regime_param_manager_;
    MarketRegime current_regime_;
    int bars_since_regime_check_;

    // Private methods
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);
    void check_and_update_regime();  // Regime detection method

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

## 📄 **FILE 17 of 32**: scripts/rotation_trading_aggregate_dashboard.py

**File Information**:
- **Path**: `scripts/rotation_trading_aggregate_dashboard.py`
- **Size**: 723 lines
- **Modified**: 2025-10-15 23:41:54
- **Type**: py
- **Permissions**: -rw-r--r--

```text
#!/usr/bin/env python3
"""
Rotation Trading Aggregate Summary Dashboard
===========================================

Aggregates trading data across multiple days to create comprehensive summary:
- Overall performance summary
- Per-symbol performance breakdown
- Daily performance table
- Price charts with trades across all days for each symbol
- Complete trade statements for each symbol

Usage:
    python rotation_trading_aggregate_dashboard.py \
        --batch-dir logs/october_2025 \
        --output logs/october_2025/dashboards/aggregate_summary.html \
        --start-date 2025-10-01 \
        --end-date 2025-10-15
"""

import argparse
import json
import sys
import os
from datetime import datetime
from collections import defaultdict
import pandas as pd
import numpy as np

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    import plotly.express as px
except ImportError:
    print("❌ Plotly not installed. Install with: pip install plotly pandas")
    sys.exit(1)


class AggregateDashboard:
    """Aggregate summary dashboard across multiple trading days"""

    def __init__(self, batch_dir, start_date, end_date, start_equity=100000.0, data_dir='data/equities'):
        self.batch_dir = batch_dir
        self.start_date = start_date
        self.end_date = end_date
        self.start_equity = start_equity
        self.data_dir = data_dir

        # All expected symbols (12 instruments - removed gold miners NUGT/DUST)
        self.all_symbols = ['ERX', 'ERY', 'FAS', 'FAZ', 'SDS', 'SSO', 'SQQQ', 'SVXY', 'TNA', 'TQQQ', 'TZA', 'UVXY']

        # Data structures
        self.daily_data = {}  # date -> {trades, equity_curve, pnl, etc}
        self.all_trades = []
        self.trades_by_symbol = defaultdict(list)

        # Aggregate metrics
        self.daily_metrics = []
        self.symbol_metrics = {}
        self.portfolio_metrics = {}

    def load_all_data(self):
        """Load trade data from all days in batch directory"""
        print(f"📊 Loading aggregate data from {self.batch_dir}...")

        # Find all day subdirectories
        trading_days = []
        for item in sorted(os.listdir(self.batch_dir)):
            day_path = os.path.join(self.batch_dir, item)
            if os.path.isdir(day_path) and item.startswith('2025-'):
                trading_days.append(item)

        print(f"✓ Found {len(trading_days)} trading days: {trading_days[0]} to {trading_days[-1]}")

        # Load data from each day
        cumulative_equity = self.start_equity

        for day in trading_days:
            day_path = os.path.join(self.batch_dir, day)
            trades_file = os.path.join(day_path, 'trades.jsonl')

            if not os.path.exists(trades_file):
                print(f"⚠️  No trades file for {day}")
                continue

            # Load day's trades
            day_trades = []
            with open(trades_file, 'r') as f:
                for line in f:
                    if line.strip():
                        trade = json.loads(line)
                        trade['date'] = day  # Tag with date
                        day_trades.append(trade)
                        self.all_trades.append(trade)

                        symbol = trade.get('symbol', 'UNKNOWN')
                        self.trades_by_symbol[symbol].append(trade)

            # Calculate day's P&L
            day_pnl = sum(t.get('pnl', 0.0) for t in day_trades if t.get('action') == 'EXIT')
            cumulative_equity += day_pnl

            # Store day data
            self.daily_data[day] = {
                'trades': day_trades,
                'pnl': day_pnl,
                'equity': cumulative_equity,
                'num_trades': len(day_trades)
            }

            print(f"   {day}: {len(day_trades)} trades, P&L: ${day_pnl:+,.2f}, Equity: ${cumulative_equity:,.2f}")

        print(f"\n✓ Loaded {len(self.all_trades)} total trades across {len(trading_days)} days")

    def calculate_metrics(self):
        """Calculate aggregate metrics"""
        print(f"\n📊 Calculating aggregate metrics...")

        # Daily metrics for chart
        for day in sorted(self.daily_data.keys()):
            data = self.daily_data[day]
            self.daily_metrics.append({
                'date': day,
                'pnl': data['pnl'],
                'equity': data['equity'],
                'num_trades': data['num_trades']
            })

        # Per-symbol aggregate metrics
        for symbol in self.all_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            self.symbol_metrics[symbol] = self._calculate_symbol_metrics(symbol, symbol_trades)

        # Portfolio aggregate metrics
        self.portfolio_metrics = self._calculate_portfolio_metrics()

        print(f"✓ Aggregate metrics calculated")

    def _calculate_symbol_metrics(self, symbol, symbol_trades):
        """Calculate metrics for a symbol across all days"""
        entries = [t for t in symbol_trades if t.get('action') == 'ENTRY']
        exits = [t for t in symbol_trades if t.get('action') == 'EXIT']

        total_pnl = sum(t.get('pnl', 0.0) for t in exits)
        winning_trades = [t for t in exits if t.get('pnl', 0.0) > 0]
        losing_trades = [t for t in exits if t.get('pnl', 0.0) < 0]

        return {
            'symbol': symbol,
            'total_trades': len(symbol_trades),
            'entries': len(entries),
            'exits': len(exits),
            'total_pnl': total_pnl,
            'winning_trades': len(winning_trades),
            'losing_trades': len(losing_trades),
            'win_rate': len(winning_trades) / len(exits) * 100 if exits else 0.0,
            'avg_win': np.mean([t.get('pnl', 0.0) for t in winning_trades]) if winning_trades else 0.0,
            'avg_loss': np.mean([t.get('pnl', 0.0) for t in losing_trades]) if losing_trades else 0.0,
        }

    def _calculate_portfolio_metrics(self):
        """Calculate overall portfolio metrics"""
        all_exits = [t for t in self.all_trades if t.get('action') == 'EXIT']
        total_pnl = sum(t.get('pnl', 0.0) for t in all_exits)
        winning_trades = [t for t in all_exits if t.get('pnl', 0.0) > 0]
        losing_trades = [t for t in all_exits if t.get('pnl', 0.0) < 0]

        final_equity = self.daily_metrics[-1]['equity'] if self.daily_metrics else self.start_equity
        total_return = (final_equity - self.start_equity) / self.start_equity * 100

        num_days = len(self.daily_data)

        # Calculate MRD (Mean Return per Day) from daily metrics
        daily_returns = []
        if len(self.daily_metrics) > 0:
            prev_equity = self.start_equity
            for day_metric in self.daily_metrics:
                day_equity = day_metric['equity']
                day_return = (day_equity - prev_equity) / prev_equity * 100
                daily_returns.append(day_return)
                prev_equity = day_equity
        avg_mrd = np.mean(daily_returns) if daily_returns else 0.0

        return {
            'total_trades': len(self.all_trades),
            'total_exits': len(all_exits),
            'total_pnl': total_pnl,
            'total_return': total_return,
            'final_equity': final_equity,
            'winning_trades': len(winning_trades),
            'losing_trades': len(losing_trades),
            'win_rate': len(winning_trades) / len(all_exits) * 100 if all_exits else 0.0,
            'avg_win': np.mean([t.get('pnl', 0.0) for t in winning_trades]) if winning_trades else 0.0,
            'avg_loss': np.mean([t.get('pnl', 0.0) for t in losing_trades]) if losing_trades else 0.0,
            'profit_factor': abs(sum(t.get('pnl', 0.0) for t in winning_trades) / sum(t.get('pnl', 0.0) for t in losing_trades)) if losing_trades else float('inf'),
            'num_days': num_days,
            'avg_pnl_per_day': total_pnl / num_days if num_days > 0 else 0.0,
            'avg_mrd': avg_mrd
        }

    def _load_price_data(self, symbol, date_list):
        """Load historical price data for a symbol across multiple dates (RTH only)"""
        csv_path = os.path.join(self.data_dir, f'{symbol}_RTH_NH.csv')

        if not os.path.exists(csv_path):
            print(f"⚠️  Price data not found for {symbol}: {csv_path}")
            return [], []

        try:
            df = pd.read_csv(csv_path)
            df['datetime'] = pd.to_datetime(df['ts_utc'])
            df['date'] = df['datetime'].dt.date

            # Filter for dates in our range
            date_objs = [pd.to_datetime(d).date() for d in date_list]
            df_filtered = df[df['date'].isin(date_objs)].copy()

            if df_filtered.empty:
                print(f"⚠️  No price data for {symbol} in date range")
                return [], []

            # Filter for RTH only (9:30 AM - 4:00 PM ET)
            # Convert to ET timezone for hour filtering
            if df_filtered['datetime'].dt.tz is None:
                df_filtered['datetime_et'] = df_filtered['datetime'].dt.tz_localize('UTC').dt.tz_convert('America/New_York')
            else:
                df_filtered['datetime_et'] = df_filtered['datetime'].dt.tz_convert('America/New_York')

            df_filtered['hour'] = df_filtered['datetime_et'].dt.hour
            df_filtered['minute'] = df_filtered['datetime_et'].dt.minute

            # RTH filter: 9:30 AM (09:30) to 4:00 PM (16:00) ET
            mask = (
                ((df_filtered['hour'] == 9) & (df_filtered['minute'] >= 30)) |
                ((df_filtered['hour'] > 9) & (df_filtered['hour'] < 16)) |
                ((df_filtered['hour'] == 16) & (df_filtered['minute'] == 0))
            )
            df_rth = df_filtered[mask]

            if df_rth.empty:
                print(f"⚠️  No RTH price data for {symbol} in date range")
                return [], []

            times = df_rth['datetime'].tolist()
            prices = df_rth['close'].tolist()
            return times, prices
        except Exception as e:
            print(f"❌ Error loading price data for {symbol}: {e}")
            return [], []

    def generate_dashboard(self, output_path):
        """Generate comprehensive aggregate dashboard"""
        print(f"\n📈 Generating aggregate dashboard...")

        # Sort symbols by total P&L descending
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )

        # Calculate layout: Portfolio Summary + Per-Symbol Summary + Daily Performance + (Chart + Table) per symbol
        # Single column layout
        total_rows = 3 + (len(sorted_symbols) * 2)  # 3 summary tables + 2 rows per symbol

        subplot_specs = []
        subplot_titles = []
        row_heights = []

        # Row 1: Portfolio Summary (transposed: 2 rows - header + data row)
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Portfolio Performance Summary</b>")
        row_heights.append(150)

        # Row 2: Per-Symbol Performance Summary
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Per-Symbol Performance Summary</b>")
        row_heights.append(350)

        # Row 3: Daily Performance
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Daily Performance</b>")
        row_heights.append(450)

        # Rows for each symbol (chart + table)
        for symbol in sorted_symbols:
            # Chart row
            subplot_specs.append([{"type": "xy"}])
            num_trades = len(self.trades_by_symbol.get(symbol, []))
            subplot_titles.append(f"<b>{symbol}</b> Price & Trades ({self.start_date} to {self.end_date})")
            row_heights.append(500)

            # Table row
            subplot_specs.append([{"type": "table"}])
            subplot_titles.append(f"<b>{symbol}</b> Trade Statement (ALL {num_trades} trades)")
            row_heights.append(550)

        # Create figure
        fig = make_subplots(
            rows=total_rows,
            cols=1,
            subplot_titles=subplot_titles,
            specs=subplot_specs,
            vertical_spacing=0.015,
            row_heights=row_heights
        )

        current_row = 1

        # Add Portfolio Summary Table
        self._add_portfolio_summary(fig, current_row)
        current_row += 1

        # Add Per-Symbol Performance Summary
        self._add_symbol_summary_table(fig, current_row)
        current_row += 1

        # Add Daily Performance Table
        self._add_daily_performance_table(fig, current_row)
        current_row += 1

        # Add symbol charts and tables
        for symbol in sorted_symbols:
            self._add_symbol_chart_and_table(fig, symbol, current_row)
            current_row += 2

        # Update layout
        total_height = sum(row_heights) + (total_rows * 30)  # Add spacing
        fig.update_layout(
            title=dict(
                text=f'<b>Aggregate Trading Summary: {self.start_date} to {self.end_date}</b>',
                font=dict(size=36, family='Arial Black'),
                x=0.5,
                xanchor='center'
            ),
            showlegend=False,
            height=total_height,
            plot_bgcolor='white',
            paper_bgcolor='white',
            font=dict(family='Arial', size=18),
            margin=dict(t=120, b=40, l=40, r=40)
        )

        # Increase caption font sizes and make them bold
        for annotation in fig['layout']['annotations']:
            annotation['font'] = dict(size=24, family='Arial', color='#333')

        # Save
        fig.write_html(output_path)
        print(f"✅ Aggregate dashboard saved: {output_path}")

        return output_path

    def _add_portfolio_summary(self, fig, row):
        """Add portfolio summary table (transposed: metrics as columns)"""
        pm = self.portfolio_metrics

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Test Period</b>', '<b>Trading Days</b>', '<b>Total Trades</b>', '<b>Total Exits</b>',
                            '<b>Win Rate</b>', '<b>Total P&L</b>', '<b>Total Return</b>', '<b>Average MRD</b>',
                            '<b>Final Equity</b>', '<b>Avg P&L/Day</b>', '<b>Profit Factor</b>'],
                    fill_color='#667eea',
                    font=dict(color='white', size=16, family='Arial'),
                    align='center',
                    height=45,
                    line=dict(color='#667eea', width=0)
                ),
                cells=dict(
                    values=[
                        [f"{self.start_date} to {self.end_date}"],
                        [f"{pm['num_days']}"],
                        [f"{pm['total_trades']}"],
                        [f"{pm['total_exits']}"],
                        [f"{pm['win_rate']:.1f}%"],
                        [f"${pm['total_pnl']:+,.2f}"],
                        [f"{pm['total_return']:+.2f}%"],
                        [f"{pm['avg_mrd']:+.3f}%"],
                        [f"${pm['final_equity']:,.2f}"],
                        [f"${pm['avg_pnl_per_day']:+,.2f}"],
                        [f"{pm['profit_factor']:.2f}"]
                    ],
                    fill_color='white',
                    font=dict(size=15, family='Arial'),
                    align='center',
                    height=40
                )
            ),
            row=row, col=1
        )

    def _add_symbol_summary_table(self, fig, row):
        """Add per-symbol performance summary table"""
        # Sort symbols by total P&L descending
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )

        symbols = []
        total_trades_list = []
        exits_list = []
        win_rates = []
        total_pnls = []
        avg_wins = []
        avg_losses = []

        for symbol in sorted_symbols:
            metrics = self.symbol_metrics[symbol]
            symbols.append(f"<b>{symbol}</b>")
            total_trades_list.append(str(metrics['total_trades']))
            exits_list.append(str(metrics['exits']))
            win_rates.append(f"{metrics['win_rate']:.1f}%")
            total_pnls.append(f"${metrics['total_pnl']:+,.2f}")
            avg_wins.append(f"${metrics['avg_win']:+,.2f}" if metrics['avg_win'] != 0 else "$0.00")
            avg_losses.append(f"${metrics['avg_loss']:+,.2f}" if metrics['avg_loss'] != 0 else "$0.00")

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Symbol</b>', '<b>Total Trades</b>', '<b>Exits</b>', '<b>Win Rate</b>',
                            '<b>Total P&L</b>', '<b>Avg Win</b>', '<b>Avg Loss</b>'],
                    fill_color='#667eea',
                    font=dict(color='white', size=18, family='Arial'),
                    align='center',
                    height=50,
                    line=dict(color='#667eea', width=0)
                ),
                cells=dict(
                    values=[symbols, total_trades_list, exits_list, win_rates, total_pnls, avg_wins, avg_losses],
                    fill_color='white',
                    font=dict(size=16, family='Arial'),
                    align=['center', 'center', 'center', 'center', 'right', 'right', 'right'],
                    height=38
                )
            ),
            row=row, col=1
        )

    def _add_daily_performance_table(self, fig, row):
        """Add daily performance breakdown table"""
        dates = []
        pnls = []
        returns = []
        equities = []
        num_trades = []

        prev_equity = self.start_equity
        for daily in self.daily_metrics:
            dates.append(daily['date'])
            pnls.append(f"${daily['pnl']:+,.2f}")
            daily_return = (daily['pnl'] / prev_equity) * 100
            returns.append(f"{daily_return:+.2f}%")
            equities.append(f"${daily['equity']:,.2f}")
            num_trades.append(str(daily['num_trades']))
            prev_equity = daily['equity']

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Date</b>', '<b>P&L</b>', '<b>Return</b>', '<b>Equity</b>', '<b>Trades</b>'],
                    fill_color='#764ba2',
                    font=dict(color='white', size=18, family='Arial'),
                    align='center',
                    height=50,
                    line=dict(color='#764ba2', width=0)
                ),
                cells=dict(
                    values=[dates, pnls, returns, equities, num_trades],
                    fill_color='white',
                    font=dict(size=16, family='Arial'),
                    align=['center', 'right', 'right', 'right', 'center'],
                    height=38
                )
            ),
            row=row, col=1
        )

    def _add_symbol_chart_and_table(self, fig, symbol, chart_row):
        """Add price chart and trade table for a symbol"""
        symbol_trades = self.trades_by_symbol.get(symbol, [])

        # Load price data across all dates
        trading_dates = sorted(self.daily_data.keys())
        times, prices = self._load_price_data(symbol, trading_dates)

        # Color mapping (12 distinct colors - removed gold miners NUGT/DUST)
        colors = {
            'ERX': '#FF4500',   # Orange Red
            'ERY': '#8B0000',   # Dark Red
            'FAS': '#00CED1',   # Dark Turquoise
            'FAZ': '#4169E1',   # Royal Blue
            'SDS': '#FF6B6B',   # Red
            'SSO': '#32CD32',   # Lime Green
            'SQQQ': '#FF1493',  # Deep Pink
            'SVXY': '#7FFF00',  # Chartreuse
            'TNA': '#FF8C00',   # Dark Orange
            'TQQQ': '#00BFFF',  # Deep Sky Blue
            'TZA': '#DC143C',   # Crimson
            'UVXY': '#9370DB',  # Medium Purple
        }
        color = colors.get(symbol, '#6366f1')

        # Add price line
        if times and prices:
            fig.add_trace(
                go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                ),
                row=chart_row, col=1
            )

        # Add entry markers
        entries = [t for t in symbol_trades if t.get('action') == 'ENTRY']
        if entries:
            entry_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in entries]
            entry_prices = [t['exec_price'] for t in entries]
            entry_texts = []
            for t in entries:
                text = f"<b>ENTRY</b><br>{t['date']}<br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>Value: ${t['value']:.2f}"
                entry_texts.append(text)

            fig.add_trace(
                go.Scatter(
                    x=entry_times,
                    y=entry_prices,
                    mode='markers',
                    name=f'{symbol} ENTRY',
                    marker=dict(
                        symbol='triangle-up',
                        size=15,
                        color='green',
                        line=dict(width=2, color='white')
                    ),
                    text=entry_texts,
                    hoverinfo='text',
                    showlegend=False
                ),
                row=chart_row, col=1
            )

        # Add exit markers
        exits = [t for t in symbol_trades if t.get('action') == 'EXIT']
        if exits:
            exit_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in exits]
            exit_prices = [t['exec_price'] for t in exits]
            exit_texts = []
            for t in exits:
                pnl = t.get('pnl', 0)
                pnl_pct = t.get('pnl_pct', 0)
                text = f"<b>EXIT</b><br>{t['date']}<br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>P&L: ${pnl:+,.2f} ({pnl_pct*100:+.2f}%)"
                exit_texts.append(text)

            fig.add_trace(
                go.Scatter(
                    x=exit_times,
                    y=exit_prices,
                    mode='markers',
                    name=f'{symbol} EXIT',
                    marker=dict(
                        symbol='triangle-down',
                        size=15,
                        color='red',
                        line=dict(width=2, color='white')
                    ),
                    text=exit_texts,
                    hoverinfo='text',
                    showlegend=False
                ),
                row=chart_row, col=1
            )

        # Update chart axes with rangebreaks to hide non-trading periods
        fig.update_xaxes(
            title_text='Date/Time',
            row=chart_row,
            col=1,
            tickfont=dict(size=14),
            rangebreaks=[
                dict(bounds=[16, 9.5], pattern="hour"),  # Hide 4:00 PM to 9:30 AM (non-RTH)
                dict(bounds=["sat", "mon"]),  # Hide weekends
            ]
        )
        fig.update_yaxes(title_text='Price ($)', row=chart_row, col=1, tickfont=dict(size=14))

        # Add trade statement table
        table_row = chart_row + 1
        self._add_trade_statement_table(fig, symbol, symbol_trades, table_row)

    def _add_trade_statement_table(self, fig, symbol, symbol_trades, row):
        """Add trade statement table with same format as single-day dashboard"""

        if not symbol_trades:
            # Empty table with message
            fig.add_trace(
                go.Table(
                    header=dict(
                        values=['<b>Timestamp</b>', '<b>Action</b>', '<b>Price</b>', '<b>Shares</b>',
                                '<b>Value</b>', '<b>P&L</b>', '<b>P&L %</b>', '<b>Bars Held</b>', '<b>Reason</b>'],
                        fill_color='#764ba2',
                        font=dict(color='white', size=16, family='Arial'),
                        align='center',
                        height=48,
                        line=dict(color='#764ba2', width=0)
                    ),
                    cells=dict(
                        values=[['N/A'], ['NO TRADES'], ['N/A'], ['0'], ['$0.00'], ['-'], ['-'], ['-'],
                                ['Symbol not selected by rotation strategy']],
                        fill_color='white',
                        font=dict(size=15, family='Arial'),
                        align=['left', 'center', 'right', 'right', 'right', 'right', 'right', 'center', 'left'],
                        height=35
                    )
                ),
                row=row, col=1
            )
            return

        # Sort trades by timestamp
        sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])

        # Format trade data
        timestamps = []
        actions = []
        prices = []
        shares = []
        values = []
        pnls = []
        pnl_pcts = []
        bars_held = []
        reasons = []
        row_colors = []

        for trade in sorted_trades:
            timestamp = datetime.fromtimestamp(trade['timestamp_ms'] / 1000).strftime('%Y-%m-%d %H:%M')
            timestamps.append(timestamp)

            action = trade.get('action', 'N/A')
            actions.append(action)

            prices.append(f"${trade.get('exec_price', 0):.2f}")
            shares.append(str(trade.get('shares', 0)))
            values.append(f"${trade.get('value', 0):.2f}")

            if action == 'EXIT':
                pnl = trade.get('pnl', 0)
                pnl_pct = trade.get('pnl_pct', 0)
                pnls.append(f"${pnl:+,.2f}")
                pnl_pcts.append(f"{pnl_pct*100:+.2f}%")
                bars_held.append(str(trade.get('bars_held', 0)))
                row_colors.append('#ffebee')  # Light red
            else:
                pnls.append('-')
                pnl_pcts.append('-')
                bars_held.append('-')
                row_colors.append('#e8f5e9')  # Light green

            reason = trade.get('reason', '')[:50]  # Truncate long reasons
            reasons.append(reason)

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Timestamp</b>', '<b>Action</b>', '<b>Price</b>', '<b>Shares</b>',
                            '<b>Value</b>', '<b>P&L</b>', '<b>P&L %</b>', '<b>Bars Held</b>', '<b>Reason</b>'],
                    fill_color='#764ba2',
                    font=dict(color='white', size=16, family='Arial'),
                    align='center',
                    height=48,
                    line=dict(color='#764ba2', width=0)
                ),
                cells=dict(
                    values=[timestamps, actions, prices, shares, values, pnls, pnl_pcts, bars_held, reasons],
                    fill_color=[row_colors],
                    font=dict(size=15, family='Arial'),
                    align=['left', 'center', 'right', 'right', 'right', 'right', 'right', 'center', 'left'],
                    height=35
                )
            ),
            row=row, col=1
        )


def main():
    parser = argparse.ArgumentParser(description='Generate aggregate rotation trading summary dashboard')
    parser.add_argument('--batch-dir', required=True, help='Batch test directory (e.g., logs/october_2025)')
    parser.add_argument('--output', required=True, help='Output HTML file path')
    parser.add_argument('--start-date', required=True, help='Start date (YYYY-MM-DD)')
    parser.add_argument('--end-date', required=True, help='End date (YYYY-MM-DD)')
    parser.add_argument('--capital', type=float, default=100000.0, help='Starting capital (default: 100000)')
    parser.add_argument('--data-dir', default='data/equities', help='Historical data directory')

    args = parser.parse_args()

    # Create dashboard
    dashboard = AggregateDashboard(
        batch_dir=args.batch_dir,
        start_date=args.start_date,
        end_date=args.end_date,
        start_equity=args.capital,
        data_dir=args.data_dir
    )

    # Load and process data
    dashboard.load_all_data()
    dashboard.calculate_metrics()

    # Generate HTML
    dashboard.generate_dashboard(args.output)

    print("\n✅ Aggregate summary dashboard complete!")


if __name__ == '__main__':
    main()

```

## 📄 **FILE 18 of 32**: scripts/rotation_trading_dashboard.py

**File Information**:
- **Path**: `scripts/rotation_trading_dashboard.py`
- **Size**: 818 lines
- **Modified**: 2025-10-15 23:39:09
- **Type**: py
- **Permissions**: -rw-r--r--

```text
#!/usr/bin/env python3
"""
Rotation Trading Multi-Symbol Dashboard
=======================================

Professional dashboard for multi-symbol rotation trading system.
Shows all 6 instruments with:
- Price charts with entry/exit markers
- Complete trade statement tables
- Per-symbol performance metrics
- Combined portfolio summary

Usage:
    python rotation_trading_dashboard.py \
        --trades logs/rotation_trading/trades.jsonl \
        --output data/dashboards/rotation_report.html
"""

import argparse
import json
import sys
from datetime import datetime
from collections import defaultdict
import pandas as pd
import numpy as np

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    import plotly.express as px
except ImportError:
    print("❌ Plotly not installed. Install with: pip install plotly pandas")
    sys.exit(1)


class RotationTradingDashboard:
    """Multi-symbol rotation trading dashboard"""

    def __init__(self, trades_path, signals_path=None, positions_path=None,
                 decisions_path=None, start_equity=100000.0, data_dir='data/equities'):
        self.trades_path = trades_path
        self.signals_path = signals_path
        self.positions_path = positions_path
        self.decisions_path = decisions_path
        self.start_equity = start_equity
        self.data_dir = data_dir

        # All expected symbols (rotation strategy uses 12 instruments - removed gold miners NUGT/DUST)
        self.all_symbols = ['ERX', 'ERY', 'FAS', 'FAZ', 'SDS', 'SSO', 'SQQQ', 'SVXY', 'TNA', 'TQQQ', 'TZA', 'UVXY']

        # Data structures
        self.trades = []
        self.trades_by_symbol = defaultdict(list)
        self.equity_by_symbol = defaultdict(list)
        self.portfolio_equity = []

        # Performance metrics
        self.symbol_metrics = {}
        self.portfolio_metrics = {}

    def load_data(self):
        """Load all trade data"""
        print(f"📊 Loading rotation trading data...")
        print(f"   Trades: {self.trades_path}")

        # Load trades
        with open(self.trades_path, 'r') as f:
            for line in f:
                if line.strip():
                    trade = json.loads(line)
                    self.trades.append(trade)
                    symbol = trade.get('symbol', 'UNKNOWN')
                    self.trades_by_symbol[symbol].append(trade)

        print(f"✓ Loaded {len(self.trades)} total trades")
        for symbol, symbol_trades in sorted(self.trades_by_symbol.items()):
            print(f"   {symbol}: {len(symbol_trades)} trades")

    def calculate_metrics(self):
        """Calculate performance metrics for each symbol and portfolio"""
        print(f"\n📊 Calculating performance metrics...")

        # Calculate per-symbol metrics for ALL symbols (including those with 0 trades)
        for symbol in self.all_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            self.symbol_metrics[symbol] = self._calculate_symbol_metrics(symbol, symbol_trades)

        # Calculate portfolio-level metrics
        self.portfolio_metrics = self._calculate_portfolio_metrics()

        print(f"✓ Metrics calculated")

    def _calculate_symbol_metrics(self, symbol, symbol_trades):
        """Calculate performance metrics for a single symbol"""
        metrics = {
            'symbol': symbol,
            'total_trades': len(symbol_trades),
            'entries': sum(1 for t in symbol_trades if t.get('action') == 'ENTRY'),
            'exits': sum(1 for t in symbol_trades if t.get('action') == 'EXIT'),
            'total_pnl': 0.0,
            'total_pnl_pct': 0.0,
            'wins': 0,
            'losses': 0,
            'avg_win': 0.0,
            'avg_loss': 0.0,
            'largest_win': 0.0,
            'largest_loss': 0.0,
            'win_rate': 0.0,
            'avg_bars_held': 0.0,
        }

        # Calculate from EXIT trades (which have P&L)
        exit_trades = [t for t in symbol_trades if t.get('action') == 'EXIT']

        if not exit_trades:
            return metrics

        wins = []
        losses = []
        bars_held = []

        for trade in exit_trades:
            pnl = trade.get('pnl', 0.0)
            if pnl is not None:
                metrics['total_pnl'] += pnl

                if pnl > 0:
                    wins.append(pnl)
                    metrics['wins'] += 1
                elif pnl < 0:
                    losses.append(pnl)
                    metrics['losses'] += 1

                if pnl > metrics['largest_win']:
                    metrics['largest_win'] = pnl
                if pnl < metrics['largest_loss']:
                    metrics['largest_loss'] = pnl

            pnl_pct = trade.get('pnl_pct', 0.0)
            if pnl_pct is not None:
                metrics['total_pnl_pct'] += pnl_pct

            bars = trade.get('bars_held', 0)
            if bars > 0:
                bars_held.append(bars)

        # Calculate averages
        if wins:
            metrics['avg_win'] = np.mean(wins)
        if losses:
            metrics['avg_loss'] = np.mean(losses)
        if bars_held:
            metrics['avg_bars_held'] = np.mean(bars_held)

        # Win rate
        total_closed = metrics['wins'] + metrics['losses']
        if total_closed > 0:
            metrics['win_rate'] = (metrics['wins'] / total_closed) * 100

        return metrics

    def _calculate_portfolio_metrics(self):
        """Calculate portfolio-level metrics"""
        # Sum metrics across all symbols
        metrics = {
            'total_trades': len(self.trades),
            'symbols_traded': len(self.trades_by_symbol),
            'total_pnl': 0.0,
            'total_pnl_pct': 0.0,
            'total_wins': 0,
            'total_losses': 0,
            'portfolio_win_rate': 0.0,
            'final_equity': self.start_equity,
            'return_pct': 0.0,
        }

        for symbol, sym_metrics in self.symbol_metrics.items():
            metrics['total_pnl'] += sym_metrics['total_pnl']
            metrics['total_pnl_pct'] += sym_metrics['total_pnl_pct']
            metrics['total_wins'] += sym_metrics['wins']
            metrics['total_losses'] += sym_metrics['losses']

        # Final equity and return
        metrics['final_equity'] = self.start_equity + metrics['total_pnl']
        metrics['return_pct'] = (metrics['total_pnl'] / self.start_equity) * 100

        # Win rate
        total_closed = metrics['total_wins'] + metrics['total_losses']
        if total_closed > 0:
            metrics['portfolio_win_rate'] = (metrics['total_wins'] / total_closed) * 100

        return metrics

    def _load_price_data(self, symbol, target_date):
        """Load historical price data for a symbol on a specific date"""
        import os
        csv_path = os.path.join(self.data_dir, f'{symbol}_RTH_NH.csv')

        if not os.path.exists(csv_path):
            print(f"⚠️  Price data not found for {symbol}: {csv_path}")
            return [], []

        try:
            df = pd.read_csv(csv_path)
            # Filter for target date
            df['date'] = pd.to_datetime(df['ts_utc']).dt.date
            target_date_obj = pd.to_datetime(target_date).date()
            df_filtered = df[df['date'] == target_date_obj]

            if df_filtered.empty:
                print(f"⚠️  No price data for {symbol} on {target_date}")
                return [], []

            times = pd.to_datetime(df_filtered['ts_utc']).tolist()
            prices = df_filtered['close'].tolist()
            return times, prices
        except Exception as e:
            print(f"❌ Error loading price data for {symbol}: {e}")
            return [], []

    def _create_symbol_chart_and_table(self, symbol, symbol_trades, color, test_date=None):
        """Create price chart and trade table for a single symbol"""

        # Load continuous historical price data for ALL symbols
        if test_date:
            times, prices = self._load_price_data(symbol, test_date)
            if times and prices:
                price_trace = go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                )
            else:
                # Fallback: empty price trace if data not available
                price_trace = go.Scatter(
                    x=[],
                    y=[],
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    showlegend=False
                )
        else:
            # No test date: use trade data if available
            if symbol_trades:
                sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])
                times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in sorted_trades]
                prices = [t['exec_price'] for t in sorted_trades]
                price_trace = go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                )
            else:
                # Empty trace
                price_trace = go.Scatter(
                    x=[],
                    y=[],
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    showlegend=False
                )

        # Handle symbols with no trades
        if not symbol_trades:
            # Create a single row table with "No trades" message
            table_rows = [['N/A', 'NO TRADES', 'N/A', '0', '$0.00', '-', '-', '-', 'Symbol not selected by rotation strategy']]
            return price_trace, None, None, table_rows

        # Sort trades by timestamp
        sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])

        # Separate entries and exits
        entries = [t for t in sorted_trades if t.get('action') == 'ENTRY']
        exits = [t for t in sorted_trades if t.get('action') == 'EXIT']

        # Entry markers
        entry_trace = None
        if entries:
            entry_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in entries]
            entry_prices = [t['exec_price'] for t in entries]
            entry_texts = []
            for t in entries:
                text = f"<b>ENTRY</b><br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>Value: ${t['value']:.2f}"
                if 'signal_probability' in t:
                    text += f"<br>Signal Prob: {t['signal_probability']:.3f}"
                if 'signal_rank' in t:
                    text += f"<br>Rank: {t['signal_rank']}"
                entry_texts.append(text)

            entry_trace = go.Scatter(
                x=entry_times,
                y=entry_prices,
                mode='markers',
                name=f'{symbol} ENTRY',
                marker=dict(
                    symbol='triangle-up',
                    size=15,
                    color='green',
                    line=dict(width=2, color='white')
                ),
                text=entry_texts,
                hoverinfo='text',
                showlegend=False
            )

        # Exit markers
        exit_trace = None
        if exits:
            exit_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in exits]
            exit_prices = [t['exec_price'] for t in exits]
            exit_texts = []
            for t in exits:
                pnl = t.get('pnl', 0)
                pnl_pct = t.get('pnl_pct', 0)
                text = f"<b>EXIT</b><br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}"
                text += f"<br>P&L: ${pnl:.2f} ({pnl_pct*100:.2f}%)<br>Bars: {t.get('bars_held', 0)}"
                if 'reason' in t:
                    text += f"<br>Reason: {t['reason']}"
                exit_texts.append(text)

            exit_trace = go.Scatter(
                x=exit_times,
                y=exit_prices,
                mode='markers',
                name=f'{symbol} EXIT',
                marker=dict(
                    symbol='triangle-down',
                    size=15,
                    color='red',
                    line=dict(width=2, color='white')
                ),
                text=exit_texts,
                hoverinfo='text',
                showlegend=False
            )

        # Create trade table data
        table_rows = []
        for trade in sorted_trades:
            timestamp = datetime.fromtimestamp(trade['timestamp_ms'] / 1000).strftime('%Y-%m-%d %H:%M')
            action = trade.get('action', 'N/A')
            price = f"${trade.get('exec_price', 0):.2f}"
            shares = str(trade.get('shares', 0))
            value = f"${trade.get('value', 0):.2f}"

            if action == 'EXIT':
                pnl = trade.get('pnl', 0)
                pnl_pct = trade.get('pnl_pct', 0)
                pnl_str = f"${pnl:.2f}"
                pnl_pct_str = f"{pnl_pct*100:.2f}%"
                bars_held = str(trade.get('bars_held', 0))
                reason = trade.get('reason', '')[:50]  # Truncate long reasons
            else:
                pnl_str = '-'
                pnl_pct_str = '-'
                bars_held = '-'
                reason = trade.get('reason', '')[:50]

            table_rows.append([timestamp, action, price, shares, value, pnl_str, pnl_pct_str, bars_held, reason])

        return price_trace, entry_trace, exit_trace, table_rows

    def create_dashboard(self, test_date="Unknown Date"):
        """Create comprehensive multi-symbol dashboard"""
        print(f"\n🎨 Creating dashboard...")

        # Use ALL configured symbols sorted by total P&L (descending)
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )
        num_symbols = len(sorted_symbols)

        # Create subplots: 2 rows per symbol (chart + table) + 1 portfolio equity + 1 summary table
        total_rows = (num_symbols * 2) + 2

        # Build subplot specs and calculate dynamic row heights
        subplot_specs = []
        subplot_titles = []
        row_heights = []

        for symbol in sorted_symbols:
            # Chart row
            subplot_specs.append([{"type": "xy"}])
            subplot_titles.append(f"<b>{symbol}</b> Price & Trades")
            row_heights.append(500)  # Fixed chart height at 500px

            # Table row - fixed height for 6 rows with vertical scrollbar
            subplot_specs.append([{"type": "table"}])
            num_trades = len(self.trades_by_symbol.get(symbol, []))
            if num_trades == 0:
                subplot_titles.append(f"<b>{symbol}</b> Trade Statement (NO TRADES - 0% allocation)")
            else:
                subplot_titles.append(f"<b>{symbol}</b> Trade Statement (ALL {num_trades} trades)")

            # Fixed table height to ensure minimum 6 visible rows with scrollbar for overflow
            # Header (60px) + 6 rows (6 * 38px = 228px) + generous padding for scrollbar and spacing
            row_heights.append(550)  # Fixed at 550px to ensure 6 rows are visible

        # Portfolio equity row
        subplot_specs.append([{"type": "xy"}])
        subplot_titles.append("<b>Portfolio Equity Curve</b>")
        row_heights.append(400)

        # Summary table row (6 symbols + 1 portfolio row = 7 rows)
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Performance Summary</b>")
        # Calculate height: header (70px) + (7 rows * 45px) + padding = 70 + 315 + 80 = 465px
        row_heights.append(520)  # Enough for all 7 rows without scrolling (increased to accommodate taller header)

        # Calculate safe vertical spacing for any number of symbols
        # Max spacing = 1/(rows-1), use 80% of max to be safe
        max_spacing = 1.0 / (total_rows - 1) if total_rows > 1 else 0.05
        vertical_spacing = min(0.025, max_spacing * 0.8)

        fig = make_subplots(
            rows=total_rows,
            cols=1,
            subplot_titles=subplot_titles,
            vertical_spacing=vertical_spacing,
            specs=subplot_specs,
            row_heights=row_heights
        )

        # Color map for symbols (12 distinct colors - removed gold miners NUGT/DUST)
        colors = {
            'ERX': '#FF4500',   # Orange Red
            'ERY': '#8B0000',   # Dark Red
            'FAS': '#00CED1',   # Dark Turquoise
            'FAZ': '#4169E1',   # Royal Blue
            'SDS': '#FF6B6B',   # Red
            'SSO': '#32CD32',   # Lime Green
            'SQQQ': '#FF1493',  # Deep Pink
            'SVXY': '#7FFF00',  # Chartreuse
            'TNA': '#FF8C00',   # Dark Orange
            'TQQQ': '#00BFFF',  # Deep Sky Blue
            'TZA': '#DC143C',   # Crimson
            'UVXY': '#9370DB',  # Medium Purple
        }

        # Plot each symbol (chart + table)
        row = 1
        for symbol in sorted_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            color = colors.get(symbol, '#888888')

            # Create chart and table
            price_trace, entry_trace, exit_trace, table_rows = self._create_symbol_chart_and_table(
                symbol, symbol_trades, color, test_date
            )

            # Add price line
            fig.add_trace(price_trace, row=row, col=1)

            # Add entry markers (only if they exist)
            if entry_trace is not None:
                fig.add_trace(entry_trace, row=row, col=1)

            # Add exit markers (only if they exist)
            if exit_trace is not None:
                fig.add_trace(exit_trace, row=row, col=1)

            # Update chart axes
            fig.update_yaxes(title_text=f"Price ($)", row=row, col=1)
            fig.update_xaxes(title_text="Time (ET)", row=row, col=1)

            row += 1

            # Add trade statement table
            # NOTE: Plotly tables don't support vertical text alignment (valign).
            # Headers may appear top-aligned. This is a known Plotly limitation.
            # See: megadocs/PLOTLY_TABLE_VERTICAL_CENTERING_BUG.md for details
            table_headers = ['Timestamp', 'Action', 'Price', 'Shares', 'Value', 'P&L', 'P&L %', 'Bars', 'Reason']
            table_values = list(zip(*table_rows)) if table_rows else [[] for _ in table_headers]

            fig.add_trace(
                go.Table(
                    header=dict(
                        values=table_headers,
                        fill_color=color,
                        font=dict(color='white', size=16, family='Arial'),  # Reduced from 18 for better visual balance
                        align='center',
                        height=48,  # Reduced from 60 to minimize visual misalignment impact
                        line=dict(color=color, width=0)  # Clean look without borders
                    ),
                    cells=dict(
                        values=table_values,
                        fill_color=['white', '#f9f9f9'] * (len(table_rows) // 2 + 1),
                        font=dict(size=15),
                        align='left',
                        height=38
                    )
                ),
                row=row, col=1
            )

            row += 1

        # Portfolio equity curve
        equity_times = []
        equity_values = []
        current_equity = self.start_equity

        for trade in sorted(self.trades, key=lambda t: t['timestamp_ms']):
            trade_time = datetime.fromtimestamp(trade['timestamp_ms'] / 1000)

            # Update equity on EXIT (when P&L is realized)
            if trade.get('action') == 'EXIT':
                pnl = trade.get('pnl', 0.0)
                if pnl is not None:
                    current_equity += pnl

            equity_times.append(trade_time)
            equity_values.append(current_equity)

        if equity_times:
            # Add starting equity reference line as a scatter trace
            fig.add_trace(
                go.Scatter(
                    x=[equity_times[0], equity_times[-1]],
                    y=[self.start_equity, self.start_equity],
                    mode='lines',
                    name='Start Equity',
                    line=dict(color='gray', width=2, dash='dash'),
                    hovertemplate=f'Start Equity: ${self.start_equity:,.2f}<extra></extra>',
                    showlegend=False
                ),
                row=row, col=1
            )

            # Add portfolio equity curve
            fig.add_trace(
                go.Scatter(
                    x=equity_times,
                    y=equity_values,
                    mode='lines',
                    name='Portfolio Equity',
                    line=dict(color='#667eea', width=3),
                    fill='tonexty',
                    fillcolor='rgba(102, 126, 234, 0.1)',
                    hovertemplate='%{x}<br>Equity: $%{y:,.2f}<extra></extra>',
                    showlegend=False
                ),
                row=row, col=1
            )

        fig.update_yaxes(title_text="Portfolio Equity ($)", row=row, col=1)
        fig.update_xaxes(title_text="Time (ET)", row=row, col=1)

        row += 1

        # Performance summary table
        table_data = []

        # Header
        headers = ['Symbol', 'Total Trades', 'Wins', 'Losses', 'Win Rate', 'Total P&L', 'Avg Win', 'Avg Loss', 'Avg Hold']

        # Per-symbol rows
        for symbol in sorted_symbols:
            metrics = self.symbol_metrics[symbol]
            table_data.append([
                symbol,
                f"{metrics['total_trades']} ({metrics['entries']}E / {metrics['exits']}X)",
                metrics['wins'],
                metrics['losses'],
                f"{metrics['win_rate']:.1f}%",
                f"${metrics['total_pnl']:.2f}",
                f"${metrics['avg_win']:.2f}",
                f"${metrics['avg_loss']:.2f}",
                f"{metrics['avg_bars_held']:.1f}",
            ])

        # Portfolio totals row
        pm = self.portfolio_metrics
        table_data.append([
            '<b>PORTFOLIO</b>',
            f"<b>{pm['total_trades']}</b>",
            f"<b>{pm['total_wins']}</b>",
            f"<b>{pm['total_losses']}</b>",
            f"<b>{pm['portfolio_win_rate']:.1f}%</b>",
            f"<b>${pm['total_pnl']:.2f}</b>",
            '',
            '',
            '',
        ])

        # Transpose for table
        table_values = list(zip(*table_data))

        # NOTE: Plotly tables don't support vertical text alignment (valign).
        # See: megadocs/PLOTLY_TABLE_VERTICAL_CENTERING_BUG.md for details
        fig.add_trace(
            go.Table(
                header=dict(
                    values=headers,
                    fill_color='#667eea',
                    font=dict(color='white', size=18, family='Arial'),  # Reduced from 21 for better visual balance
                    align='center',
                    height=50,  # Reduced from 70 to minimize visual misalignment impact
                    line=dict(color='#667eea', width=0)  # Clean look without borders
                ),
                cells=dict(
                    values=table_values,
                    fill_color=[['white', '#f0f0f0'] * (len(table_data) - 1) + ['#ffeaa7']],  # Highlight totals
                    font=dict(size=18),
                    align='center',
                    height=45
                )
            ),
            row=row, col=1
        )

        # Update layout
        # Calculate total height: sum of rows + spacing between rows + extra padding
        total_figure_height = sum(row_heights) + (total_rows * 50) + 100  # 50px per title/spacing + 100px top margin

        fig.update_layout(
            height=total_figure_height,
            showlegend=False,
            hovermode='closest',
            plot_bgcolor='white',
            paper_bgcolor='#f8f9fa',
            font=dict(family='Arial', size=17, color='#2c3e50'),
            margin=dict(t=80, b=20, l=50, r=50)  # Increased top margin for caption visibility
        )

        # Update subplot title annotations (20% smaller than 32px = 26px, non-bold)
        # and adjust y position to add more space above charts
        for annotation in fig.layout.annotations:
            annotation.font.size = 26
            annotation.font.family = 'Arial'
            # Move annotations up to ensure visibility (yshift in pixels)
            annotation.yshift = 15

        return fig

    def generate(self, output_path):
        """Generate the complete dashboard"""
        self.load_data()
        self.calculate_metrics()

        # Extract test date from trades
        test_date = "Unknown Date"
        if self.trades:
            # Get timestamp from first trade and convert to date
            first_timestamp_ms = self.trades[0].get('timestamp_ms', 0)
            if first_timestamp_ms:
                test_date = datetime.fromtimestamp(first_timestamp_ms / 1000).strftime('%Y-%m-%d')

        fig = self.create_dashboard(test_date)

        # Add summary statistics at the top
        pm = self.portfolio_metrics

        html_header = f"""
        <div style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white; padding: 30px; margin-bottom: 20px; border-radius: 10px;">
            <h1 style="margin: 0; text-align: center;">🎯 Rotation Trading Report</h1>
            <p style="text-align: center; font-size: 27px; margin: 10px 0 0 0;">
                Multi-Symbol Position Rotation Strategy
            </p>
            <p style="text-align: center; font-size: 24px; margin: 10px 0 0 0; opacity: 0.9;">
                Test Date: {test_date}
            </p>
        </div>

        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
                    gap: 20px; margin-bottom: 30px;">
            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Starting Equity</div>
                <div style="font-size: 42px; font-weight: bold; color: #2c3e50;">
                    ${self.start_equity:,.2f}
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Final Equity</div>
                <div style="font-size: 42px; font-weight: bold; color: {'#27ae60' if pm['total_pnl'] >= 0 else '#e74c3c'};">
                    ${pm['final_equity']:,.2f}
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Total P&L</div>
                <div style="font-size: 42px; font-weight: bold; color: {'#27ae60' if pm['total_pnl'] >= 0 else '#e74c3c'};">
                    ${pm['total_pnl']:+,.2f}
                </div>
                <div style="font-size: 24px; color: #666;">
                    ({pm['return_pct']:+.2f}%)
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Total Trades</div>
                <div style="font-size: 42px; font-weight: bold; color: #2c3e50;">
                    {pm['total_trades']}
                </div>
                <div style="font-size: 24px; color: #666;">
                    {pm['symbols_traded']} symbols
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Win Rate</div>
                <div style="font-size: 42px; font-weight: bold; color: #3498db;">
                    {pm['portfolio_win_rate']:.1f}%
                </div>
                <div style="font-size: 24px; color: #666;">
                    {pm['total_wins']}W / {pm['total_losses']}L
                </div>
            </div>
        </div>
        """

        # Save with custom HTML wrapper
        html_str = fig.to_html(include_plotlyjs='cdn', full_html=False)

        full_html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            <title>Rotation Trading Dashboard</title>
            <style>
                body {{
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
                    margin: 0;
                    padding: 20px;
                    background-color: #f8f9fa;
                }}
                .container {{
                    max-width: 1600px;
                    margin: 0 auto;
                }}
            </style>
        </head>
        <body>
            <div class="container">
                {html_header}
                {html_str}
                <div style="text-align: center; margin-top: 30px; padding: 20px; color: #666; border-top: 1px solid #ddd;">
                    <p>🤖 Generated by OnlineTrader Rotation System v2.0</p>
                    <p style="font-size: 18px;">Generated at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
                </div>
            </div>
        </body>
        </html>
        """

        with open(output_path, 'w') as f:
            f.write(full_html)

        print(f"\n✅ Dashboard saved: {output_path}")

        # Print summary
        print(f"\n{'='*60}")
        print(f"ROTATION TRADING SUMMARY")
        print(f"{'='*60}")
        print(f"Starting Equity:  ${self.start_equity:,.2f}")
        print(f"Final Equity:     ${pm['final_equity']:,.2f}")
        print(f"Total P&L:        ${pm['total_pnl']:+,.2f} ({pm['return_pct']:+.2f}%)")
        print(f"Total Trades:     {pm['total_trades']} ({pm['symbols_traded']} symbols)")
        print(f"Win Rate:         {pm['portfolio_win_rate']:.1f}% ({pm['total_wins']}W / {pm['total_losses']}L)")
        print(f"{'='*60}")
        print(f"\nPer-Symbol Breakdown:")
        for symbol in sorted(self.symbol_metrics.keys()):
            metrics = self.symbol_metrics[symbol]
            print(f"  {symbol:6s}: {metrics['total_trades']:2d} trades, "
                  f"P&L ${metrics['total_pnl']:+8.2f}, "
                  f"WR {metrics['win_rate']:5.1f}%")
        print(f"{'='*60}\n")


def main():
    parser = argparse.ArgumentParser(description='Generate rotation trading dashboard')
    parser.add_argument('--trades', required=True, help='Path to trades.jsonl')
    parser.add_argument('--signals', help='Path to signals.jsonl')
    parser.add_argument('--positions', help='Path to positions.jsonl')
    parser.add_argument('--decisions', help='Path to decisions.jsonl')
    parser.add_argument('--output', required=True, help='Output HTML path')
    parser.add_argument('--start-equity', type=float, default=100000.0, help='Starting equity')

    args = parser.parse_args()

    print("="*60)
    print("ROTATION TRADING DASHBOARD GENERATOR")
    print("="*60)

    dashboard = RotationTradingDashboard(
        trades_path=args.trades,
        signals_path=args.signals,
        positions_path=args.positions,
        decisions_path=args.decisions,
        start_equity=args.start_equity
    )

    dashboard.generate(args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())

```

## 📄 **FILE 19 of 32**: src/backend/adaptive_trading_mechanism.cpp

**File Information**:
- **Path**: `src/backend/adaptive_trading_mechanism.cpp`
- **Size**: 702 lines
- **Modified**: 2025-10-08 07:45:04
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <numeric>
#include <filesystem>

namespace sentio {

// ===================================================================
// MARKET REGIME DETECTOR IMPLEMENTATION
// ===================================================================

MarketState AdaptiveMarketRegimeDetector::analyze_market_state(const Bar& current_bar, 
                                                      const std::vector<Bar>& recent_history,
                                                      const SignalOutput& signal) {
    MarketState state;
    
    // Update price history
    price_history_.push_back(current_bar.close);
    if (price_history_.size() > LOOKBACK_PERIOD) {
        price_history_.erase(price_history_.begin());
    }
    
    // Update volume history
    volume_history_.push_back(current_bar.volume);
    if (volume_history_.size() > LOOKBACK_PERIOD) {
        volume_history_.erase(volume_history_.begin());
    }
    
    // Calculate market metrics
    state.current_price = current_bar.close;
    state.volatility = calculate_volatility();
    state.trend_strength = calculate_trend_strength();
    state.volume_ratio = calculate_volume_ratio();
    state.regime = classify_market_regime(state.volatility, state.trend_strength);
    
    // Signal statistics
    state.avg_signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    utils::log_debug("Market Analysis: Price=" + std::to_string(state.current_price) + 
                    ", Vol=" + std::to_string(state.volatility) + 
                    ", Trend=" + std::to_string(state.trend_strength) + 
                    ", Regime=" + std::to_string(static_cast<int>(state.regime)));
    
    return state;
}

double AdaptiveMarketRegimeDetector::calculate_volatility() {
    if (price_history_.size() < 2) return 0.1; // Default volatility
    
    std::vector<double> returns;
    for (size_t i = 1; i < price_history_.size(); ++i) {
        double ret = std::log(price_history_[i] / price_history_[i-1]);
        returns.push_back(ret);
    }
    
    // Calculate standard deviation of returns
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double sq_sum = 0.0;
    for (double ret : returns) {
        sq_sum += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sq_sum / returns.size()) * std::sqrt(252); // Annualized
}

double AdaptiveMarketRegimeDetector::calculate_trend_strength() {
    if (price_history_.size() < 10) return 0.0;
    
    // Linear regression slope over recent prices
    double n = static_cast<double>(price_history_.size());
    double sum_x = n * (n - 1) / 2;
    double sum_y = std::accumulate(price_history_.begin(), price_history_.end(), 0.0);
    double sum_xy = 0.0;
    double sum_x2 = n * (n - 1) * (2 * n - 1) / 6;
    
    for (size_t i = 0; i < price_history_.size(); ++i) {
        sum_xy += static_cast<double>(i) * price_history_[i];
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to [-1, 1] range
    double price_range = *std::max_element(price_history_.begin(), price_history_.end()) -
                        *std::min_element(price_history_.begin(), price_history_.end());
    
    if (price_range > 0) {
        return std::clamp(slope / price_range * 100, -1.0, 1.0);
    }
    
    return 0.0;
}

double AdaptiveMarketRegimeDetector::calculate_volume_ratio() {
    if (volume_history_.empty()) return 1.0;
    
    double current_volume = volume_history_.back();
    double avg_volume = std::accumulate(volume_history_.begin(), volume_history_.end(), 0.0) / volume_history_.size();
    
    return (avg_volume > 0) ? current_volume / avg_volume : 1.0;
}

AdaptiveMarketRegime AdaptiveMarketRegimeDetector::classify_market_regime(double volatility, double trend_strength) {
    bool high_vol = volatility > 0.25; // 25% annualized volatility threshold

    if (trend_strength > 0.3) {
        return high_vol ? AdaptiveMarketRegime::BULL_HIGH_VOL : AdaptiveMarketRegime::BULL_LOW_VOL;
    } else if (trend_strength < -0.3) {
        return high_vol ? AdaptiveMarketRegime::BEAR_HIGH_VOL : AdaptiveMarketRegime::BEAR_LOW_VOL;
    } else {
        return high_vol ? AdaptiveMarketRegime::SIDEWAYS_HIGH_VOL : AdaptiveMarketRegime::SIDEWAYS_LOW_VOL;
    }
}

// ===================================================================
// PERFORMANCE EVALUATOR IMPLEMENTATION
// ===================================================================

void PerformanceEvaluator::add_trade_outcome(const TradeOutcome& outcome) {
    trade_history_.push_back(outcome);
    
    // Maintain rolling window
    if (trade_history_.size() > MAX_HISTORY) {
        trade_history_.erase(trade_history_.begin());
    }
    
    utils::log_debug("Trade outcome added: PnL=" + std::to_string(outcome.actual_pnl) + 
                    ", Profitable=" + (outcome.was_profitable ? "YES" : "NO"));
}

void PerformanceEvaluator::add_portfolio_value(double value) {
    portfolio_values_.push_back(value);
    
    if (portfolio_values_.size() > MAX_HISTORY) {
        portfolio_values_.erase(portfolio_values_.begin());
    }
}

PerformanceMetrics PerformanceEvaluator::calculate_performance_metrics() {
    PerformanceMetrics metrics;
    
    if (trade_history_.empty()) {
        return metrics;
    }
    
    // Get recent trades for analysis
    size_t start_idx = trade_history_.size() > PERFORMANCE_WINDOW ? 
                      trade_history_.size() - PERFORMANCE_WINDOW : 0;
    
    std::vector<TradeOutcome> recent_trades(
        trade_history_.begin() + start_idx, trade_history_.end());
    
    // Calculate basic metrics
    metrics.total_trades = static_cast<int>(recent_trades.size());
    metrics.winning_trades = 0;
    metrics.losing_trades = 0;
    metrics.gross_profit = 0.0;
    metrics.gross_loss = 0.0;
    
    for (const auto& trade : recent_trades) {
        if (trade.was_profitable) {
            metrics.winning_trades++;
            metrics.gross_profit += trade.actual_pnl;
        } else {
            metrics.losing_trades++;
            metrics.gross_loss += std::abs(trade.actual_pnl);
        }
        
        metrics.returns.push_back(trade.pnl_percentage);
    }
    
    // Calculate derived metrics
    metrics.win_rate = metrics.total_trades > 0 ? 
                      static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;
    
    metrics.profit_factor = metrics.gross_loss > 0 ? 
                           metrics.gross_profit / metrics.gross_loss : 1.0;
    
    metrics.sharpe_ratio = calculate_sharpe_ratio(metrics.returns);
    metrics.max_drawdown = calculate_max_drawdown();
    metrics.capital_efficiency = calculate_capital_efficiency();
    
    return metrics;
}

double PerformanceEvaluator::calculate_reward_signal(const PerformanceMetrics& metrics) {
    // Multi-objective reward function
    double profit_component = metrics.gross_profit - metrics.gross_loss;
    double risk_component = metrics.sharpe_ratio * 0.5;
    double drawdown_penalty = metrics.max_drawdown * -2.0;
    double overtrading_penalty = std::max(0.0, metrics.trade_frequency - 10.0) * -0.1;
    
    double total_reward = profit_component + risk_component + drawdown_penalty + overtrading_penalty;
    
    utils::log_debug("Reward calculation: Profit=" + std::to_string(profit_component) + 
                    ", Risk=" + std::to_string(risk_component) + 
                    ", Drawdown=" + std::to_string(drawdown_penalty) + 
                    ", Total=" + std::to_string(total_reward));
    
    return total_reward;
}

double PerformanceEvaluator::calculate_sharpe_ratio(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    double std_dev = std::sqrt(variance);
    return std_dev > 0 ? mean_return / std_dev : 0.0;
}

double PerformanceEvaluator::calculate_max_drawdown() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double peak = portfolio_values_[0];
    double max_dd = 0.0;
    
    for (double value : portfolio_values_) {
        if (value > peak) {
            peak = value;
        }
        
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    return max_dd;
}

double PerformanceEvaluator::calculate_capital_efficiency() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double initial_value = portfolio_values_.front();
    double final_value = portfolio_values_.back();
    
    return initial_value > 0 ? (final_value - initial_value) / initial_value : 0.0;
}

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER IMPLEMENTATION
// ===================================================================

QLearningThresholdOptimizer::QLearningThresholdOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    utils::log_info("Q-Learning Threshold Optimizer initialized with learning_rate=" + 
                   std::to_string(learning_rate_) + ", exploration_rate=" + std::to_string(exploration_rate_));
}

ThresholdAction QLearningThresholdOptimizer::select_action(const MarketState& state, 
                                                          const ThresholdPair& current_thresholds,
                                                          const PerformanceMetrics& performance) {
    int state_hash = discretize_state(state, current_thresholds, performance);
    
    // Epsilon-greedy action selection
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    if (dis(rng_) < exploration_rate_) {
        // Explore: random action
        std::uniform_int_distribution<int> action_dis(0, static_cast<int>(ThresholdAction::COUNT) - 1);
        ThresholdAction action = static_cast<ThresholdAction>(action_dis(rng_));
        utils::log_debug("Q-Learning: EXPLORE action=" + std::to_string(static_cast<int>(action)));
        return action;
    } else {
        // Exploit: best known action
        ThresholdAction action = get_best_action(state_hash);
        utils::log_debug("Q-Learning: EXPLOIT action=" + std::to_string(static_cast<int>(action)));
        return action;
    }
}

void QLearningThresholdOptimizer::update_q_value(const MarketState& prev_state,
                                                 const ThresholdPair& prev_thresholds,
                                                 const PerformanceMetrics& prev_performance,
                                                 ThresholdAction action,
                                                 double reward,
                                                 const MarketState& new_state,
                                                 const ThresholdPair& new_thresholds,
                                                 const PerformanceMetrics& new_performance) {
    
    int prev_state_hash = discretize_state(prev_state, prev_thresholds, prev_performance);
    int new_state_hash = discretize_state(new_state, new_thresholds, new_performance);
    
    StateActionPair sa_pair{prev_state_hash, static_cast<int>(action)};
    
    // Get current Q-value
    double current_q = get_q_value(sa_pair);
    
    // Get maximum Q-value for next state
    double max_next_q = get_max_q_value(new_state_hash);
    
    // Q-learning update
    double target = reward + discount_factor_ * max_next_q;
    double new_q = current_q + learning_rate_ * (target - current_q);
    
    q_table_[sa_pair] = new_q;
    state_visit_count_[prev_state_hash]++;
    
    // Decay exploration rate
    exploration_rate_ = std::max(min_exploration_, exploration_rate_ * exploration_decay_);
    
    utils::log_debug("Q-Learning update: State=" + std::to_string(prev_state_hash) + 
                    ", Action=" + std::to_string(static_cast<int>(action)) + 
                    ", Reward=" + std::to_string(reward) + 
                    ", Q_old=" + std::to_string(current_q) + 
                    ", Q_new=" + std::to_string(new_q));
}

ThresholdPair QLearningThresholdOptimizer::apply_action(const ThresholdPair& current_thresholds, ThresholdAction action) {
    ThresholdPair new_thresholds = current_thresholds;
    
    switch (action) {
        case ThresholdAction::INCREASE_BUY_SMALL:
            new_thresholds.buy_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_BUY_SMALL:
            new_thresholds.buy_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold -= 0.03;
            break;
        case ThresholdAction::INCREASE_SELL_SMALL:
            new_thresholds.sell_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_SELL_SMALL:
            new_thresholds.sell_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold -= 0.03;
            break;
        case ThresholdAction::MAINTAIN_THRESHOLDS:
        default:
            // No change
            break;
    }
    
    // Ensure thresholds remain valid
    new_thresholds.buy_threshold = std::clamp(new_thresholds.buy_threshold, 0.51, 0.90);
    new_thresholds.sell_threshold = std::clamp(new_thresholds.sell_threshold, 0.10, 0.49);
    
    // Ensure minimum gap
    if (new_thresholds.buy_threshold - new_thresholds.sell_threshold < 0.05) {
        new_thresholds.buy_threshold = new_thresholds.sell_threshold + 0.05;
        new_thresholds.buy_threshold = std::min(new_thresholds.buy_threshold, 0.90);
    }
    
    return new_thresholds;
}

double QLearningThresholdOptimizer::get_learning_progress() const {
    return 1.0 - exploration_rate_;
}

int QLearningThresholdOptimizer::discretize_state(const MarketState& state, 
                                                 const ThresholdPair& thresholds,
                                                 const PerformanceMetrics& performance) {
    // Create a hash of the discretized state
    int buy_bin = static_cast<int>((thresholds.buy_threshold - 0.5) / 0.4 * THRESHOLD_BINS);
    int sell_bin = static_cast<int>((thresholds.sell_threshold - 0.1) / 0.4 * THRESHOLD_BINS);
    int vol_bin = static_cast<int>(std::min(state.volatility / 0.5, 1.0) * 5);
    int trend_bin = static_cast<int>((state.trend_strength + 1.0) / 2.0 * 5);
    int perf_bin = static_cast<int>(std::clamp(performance.win_rate, 0.0, 1.0) * PERFORMANCE_BINS);
    
    // Combine bins into a single hash
    return buy_bin * 10000 + sell_bin * 1000 + vol_bin * 100 + trend_bin * 10 + perf_bin;
}

double QLearningThresholdOptimizer::get_q_value(const StateActionPair& sa_pair) {
    auto it = q_table_.find(sa_pair);
    return (it != q_table_.end()) ? it->second : 0.0; // Optimistic initialization
}

double QLearningThresholdOptimizer::get_max_q_value(int state_hash) {
    double max_q = 0.0;
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        max_q = std::max(max_q, get_q_value(sa_pair));
    }
    
    return max_q;
}

ThresholdAction QLearningThresholdOptimizer::get_best_action(int state_hash) {
    ThresholdAction best_action = ThresholdAction::MAINTAIN_THRESHOLDS;
    double best_q = get_q_value({state_hash, static_cast<int>(best_action)});
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        double q_val = get_q_value(sa_pair);
        
        if (q_val > best_q) {
            best_q = q_val;
            best_action = static_cast<ThresholdAction>(action);
        }
    }
    
    return best_action;
}

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER IMPLEMENTATION
// ===================================================================

MultiArmedBanditOptimizer::MultiArmedBanditOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_arms();
    utils::log_info("Multi-Armed Bandit Optimizer initialized with " + std::to_string(arms_.size()) + " arms");
}

ThresholdPair MultiArmedBanditOptimizer::select_thresholds() {
    if (arms_.empty()) {
        return ThresholdPair(); // Default thresholds
    }
    
    // UCB1 algorithm
    update_confidence_bounds();
    
    auto best_arm = std::max_element(arms_.begin(), arms_.end(),
        [](const BanditArm& a, const BanditArm& b) {
            return (a.estimated_reward + a.confidence_bound) < 
                   (b.estimated_reward + b.confidence_bound);
        });
    
    utils::log_debug("Bandit selected: Buy=" + std::to_string(best_arm->thresholds.buy_threshold) + 
                    ", Sell=" + std::to_string(best_arm->thresholds.sell_threshold) + 
                    ", UCB=" + std::to_string(best_arm->estimated_reward + best_arm->confidence_bound));
    
    return best_arm->thresholds;
}

void MultiArmedBanditOptimizer::update_reward(const ThresholdPair& thresholds, double reward) {
    // Find the arm that was pulled
    auto arm_it = std::find_if(arms_.begin(), arms_.end(),
        [&thresholds](const BanditArm& arm) {
            return std::abs(arm.thresholds.buy_threshold - thresholds.buy_threshold) < 0.005 &&
                   std::abs(arm.thresholds.sell_threshold - thresholds.sell_threshold) < 0.005;
        });
    
    if (arm_it != arms_.end()) {
        // Update arm's estimated reward using incremental mean
        arm_it->pull_count++;
        total_pulls_++;
        
        double old_estimate = arm_it->estimated_reward;
        arm_it->estimated_reward = old_estimate + (reward - old_estimate) / arm_it->pull_count;
        
        utils::log_debug("Bandit reward update: Buy=" + std::to_string(thresholds.buy_threshold) + 
                        ", Sell=" + std::to_string(thresholds.sell_threshold) + 
                        ", Reward=" + std::to_string(reward) + 
                        ", New_Est=" + std::to_string(arm_it->estimated_reward));
    }
}

void MultiArmedBanditOptimizer::initialize_arms() {
    // Create a grid of threshold combinations
    for (double buy = 0.55; buy <= 0.85; buy += 0.05) {
        for (double sell = 0.15; sell <= 0.45; sell += 0.05) {
            if (buy > sell + 0.05) { // Ensure minimum gap
                arms_.emplace_back(ThresholdPair(buy, sell));
            }
        }
    }
}

void MultiArmedBanditOptimizer::update_confidence_bounds() {
    for (auto& arm : arms_) {
        if (arm.pull_count == 0) {
            arm.confidence_bound = std::numeric_limits<double>::max();
        } else {
            arm.confidence_bound = std::sqrt(2.0 * std::log(total_pulls_) / arm.pull_count);
        }
    }
}

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER IMPLEMENTATION
// ===================================================================

AdaptiveThresholdManager::AdaptiveThresholdManager(const AdaptiveConfig& config) 
    : config_(config), current_thresholds_(0.55, 0.45) {
    
    // Initialize components
    q_learner_ = std::make_unique<QLearningThresholdOptimizer>();
    bandit_optimizer_ = std::make_unique<MultiArmedBanditOptimizer>();
    regime_detector_ = std::make_unique<AdaptiveMarketRegimeDetector>();
    performance_evaluator_ = std::make_unique<PerformanceEvaluator>();
    
    // Initialize regime-specific thresholds
    initialize_regime_thresholds();
    
    utils::log_info("AdaptiveThresholdManager initialized: Algorithm=" + 
                   std::to_string(static_cast<int>(config_.algorithm)) + 
                   ", LearningRate=" + std::to_string(config_.learning_rate) + 
                   ", ConservativeMode=" + (config_.conservative_mode ? "YES" : "NO"));
}

ThresholdPair AdaptiveThresholdManager::get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
    // Update market state
    current_market_state_ = regime_detector_->analyze_market_state(bar, recent_bars_, signal);
    recent_bars_.push_back(bar);
    if (recent_bars_.size() > 100) {
        recent_bars_.erase(recent_bars_.begin());
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_) {
        utils::log_warning("Circuit breaker active - using conservative thresholds");
        return get_conservative_thresholds();
    }
    
    // Update performance and potentially adjust thresholds
    update_performance_and_learn();
    
    // Get regime-adapted thresholds if enabled
    if (config_.enable_regime_adaptation) {
        return get_regime_adapted_thresholds();
    }
    
    return current_thresholds_;
}

void AdaptiveThresholdManager::process_trade_outcome(const std::string& symbol, TradeAction action, 
                                                    double quantity, double price, double trade_value, double fees,
                                                    double actual_pnl, double pnl_percentage, bool was_profitable) {
    TradeOutcome outcome;
    outcome.symbol = symbol;
    outcome.action = action;
    outcome.quantity = quantity;
    outcome.price = price;
    outcome.trade_value = trade_value;
    outcome.fees = fees;
    outcome.actual_pnl = actual_pnl;
    outcome.pnl_percentage = pnl_percentage;
    outcome.was_profitable = was_profitable;
    outcome.outcome_timestamp = std::chrono::system_clock::now();
    
    performance_evaluator_->add_trade_outcome(outcome);
    
    // Update learning algorithms with reward feedback
    if (learning_enabled_) {
        current_performance_ = performance_evaluator_->calculate_performance_metrics();
        double reward = performance_evaluator_->calculate_reward_signal(current_performance_);
        
        // Update based on algorithm type
        switch (config_.algorithm) {
            case LearningAlgorithm::Q_LEARNING:
                // Q-learning update will happen in next call to update_performance_and_learn()
                break;
                
            case LearningAlgorithm::MULTI_ARMED_BANDIT:
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
                
            case LearningAlgorithm::ENSEMBLE:
                // Update both algorithms
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
        }
    }
    
    // Check for circuit breaker conditions
    check_circuit_breaker();
}

void AdaptiveThresholdManager::update_portfolio_value(double value) {
    performance_evaluator_->add_portfolio_value(value);
}

double AdaptiveThresholdManager::get_learning_progress() const {
    return q_learner_->get_learning_progress();
}

std::string AdaptiveThresholdManager::generate_performance_report() const {
    std::ostringstream report;
    
    report << "=== ADAPTIVE TRADING PERFORMANCE REPORT ===\n";
    report << "Current Thresholds: Buy=" << std::fixed << std::setprecision(3) << current_thresholds_.buy_threshold 
           << ", Sell=" << current_thresholds_.sell_threshold << "\n";
    report << "Market Regime: " << static_cast<int>(current_market_state_.regime) << "\n";
    report << "Total Trades: " << current_performance_.total_trades << "\n";
    report << "Win Rate: " << std::fixed << std::setprecision(1) << (current_performance_.win_rate * 100) << "%\n";
    report << "Profit Factor: " << std::fixed << std::setprecision(2) << current_performance_.profit_factor << "\n";
    report << "Sharpe Ratio: " << std::fixed << std::setprecision(2) << current_performance_.sharpe_ratio << "\n";
    report << "Max Drawdown: " << std::fixed << std::setprecision(1) << (current_performance_.max_drawdown * 100) << "%\n";
    report << "Learning Progress: " << std::fixed << std::setprecision(1) << (get_learning_progress() * 100) << "%\n";
    report << "Circuit Breaker: " << (circuit_breaker_active_ ? "ACTIVE" : "INACTIVE") << "\n";
    
    return report.str();
}

void AdaptiveThresholdManager::initialize_regime_thresholds() {
    // Conservative thresholds for volatile markets
    regime_thresholds_[AdaptiveMarketRegime::BULL_HIGH_VOL] = ThresholdPair(0.65, 0.35);
    regime_thresholds_[AdaptiveMarketRegime::BEAR_HIGH_VOL] = ThresholdPair(0.70, 0.30);
    regime_thresholds_[AdaptiveMarketRegime::SIDEWAYS_HIGH_VOL] = ThresholdPair(0.68, 0.32);
    
    // More aggressive thresholds for stable markets
    regime_thresholds_[AdaptiveMarketRegime::BULL_LOW_VOL] = ThresholdPair(0.58, 0.42);
    regime_thresholds_[AdaptiveMarketRegime::BEAR_LOW_VOL] = ThresholdPair(0.62, 0.38);
    regime_thresholds_[AdaptiveMarketRegime::SIDEWAYS_LOW_VOL] = ThresholdPair(0.60, 0.40);
}

void AdaptiveThresholdManager::update_performance_and_learn() {
    if (!learning_enabled_ || circuit_breaker_active_) {
        return;
    }
    
    // Update performance metrics
    PerformanceMetrics new_performance = performance_evaluator_->calculate_performance_metrics();
    
    // Only learn if we have enough data
    if (new_performance.total_trades < config_.performance_window / 2) {
        return;
    }
    
    // Q-Learning update
    if (config_.algorithm == LearningAlgorithm::Q_LEARNING || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        double reward = performance_evaluator_->calculate_reward_signal(new_performance);
        
        // Select and apply action
        ThresholdAction action = q_learner_->select_action(
            current_market_state_, current_thresholds_, current_performance_);
        
        ThresholdPair new_thresholds = q_learner_->apply_action(current_thresholds_, action);
        
        // Update Q-values if we have previous state
        if (current_performance_.total_trades > 0) {
            q_learner_->update_q_value(
                current_market_state_, current_thresholds_, current_performance_,
                action, reward,
                current_market_state_, new_thresholds, new_performance);
        }
        
        current_thresholds_ = new_thresholds;
    }
    
    // Multi-Armed Bandit update
    if (config_.algorithm == LearningAlgorithm::MULTI_ARMED_BANDIT || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        current_thresholds_ = bandit_optimizer_->select_thresholds();
    }
    
    current_performance_ = new_performance;
}

ThresholdPair AdaptiveThresholdManager::get_regime_adapted_thresholds() {
    auto regime_it = regime_thresholds_.find(current_market_state_.regime);
    if (regime_it != regime_thresholds_.end()) {
        // Blend learned thresholds with regime-specific ones
        ThresholdPair regime_thresholds = regime_it->second;
        double blend_factor = config_.conservative_mode ? 0.7 : 0.3;
        
        return ThresholdPair(
            current_thresholds_.buy_threshold * (1.0 - blend_factor) + 
            regime_thresholds.buy_threshold * blend_factor,
            current_thresholds_.sell_threshold * (1.0 - blend_factor) + 
            regime_thresholds.sell_threshold * blend_factor
        );
    }
    
    return current_thresholds_;
}

ThresholdPair AdaptiveThresholdManager::get_conservative_thresholds() {
    // Return very conservative thresholds during circuit breaker
    return ThresholdPair(0.75, 0.25);
}

void AdaptiveThresholdManager::check_circuit_breaker() {
    // Only activate circuit breaker if we have sufficient trading history
    if (current_performance_.total_trades < 10) {
        return; // Not enough data to make circuit breaker decisions
    }
    
    if (current_performance_.max_drawdown > config_.max_drawdown_limit ||
        current_performance_.win_rate < 0.3 ||
        (current_performance_.total_trades > 20 && current_performance_.profit_factor < 0.8)) {
        
        circuit_breaker_active_ = true;
        learning_enabled_ = false;
        
        utils::log_error("CIRCUIT BREAKER ACTIVATED: Drawdown=" + std::to_string(current_performance_.max_drawdown) + 
                        ", WinRate=" + std::to_string(current_performance_.win_rate) + 
                        ", ProfitFactor=" + std::to_string(current_performance_.profit_factor));
    }
}

} // namespace sentio

```

## 📄 **FILE 20 of 32**: src/backend/rotation_trading_backend.cpp

**File Information**:
- **Path**: `src/backend/rotation_trading_backend.cpp`
- **Size**: 1069 lines
- **Modified**: 2025-10-15 15:33:18
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "backend/rotation_trading_backend.h"
#include "common/utils.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>

using json = nlohmann::json;

namespace sentio {

RotationTradingBackend::RotationTradingBackend(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr,
    std::shared_ptr<AlpacaClient> broker
)
    : config_(config)
    , data_manager_(data_mgr)
    , broker_(broker)
    , current_cash_(config.starting_capital) {

    utils::log_info("========================================");
    utils::log_info("RotationTradingBackend Initializing");
    utils::log_info("========================================");

    // Create data manager if not provided
    if (!data_manager_) {
        data::MultiSymbolDataManager::Config dm_config = config_.data_config;
        dm_config.symbols = config_.symbols;
        data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);
        utils::log_info("Created MultiSymbolDataManager");
    }

    // Create OES manager
    MultiSymbolOESManager::Config oes_config;
    oes_config.symbols = config_.symbols;
    oes_config.base_config = config_.oes_config;
    oes_manager_ = std::make_unique<MultiSymbolOESManager>(oes_config, data_manager_);
    utils::log_info("Created MultiSymbolOESManager");

    // Create signal aggregator
    signal_aggregator_ = std::make_unique<SignalAggregator>(config_.aggregator_config);
    utils::log_info("Created SignalAggregator");

    // Create rotation manager
    rotation_manager_ = std::make_unique<RotationPositionManager>(config_.rotation_config);
    utils::log_info("Created RotationPositionManager");

    utils::log_info("Symbols: " + std::to_string(config_.symbols.size()));
    utils::log_info("Starting capital: $" + std::to_string(config_.starting_capital));
    utils::log_info("Max positions: " + std::to_string(config_.rotation_config.max_positions));
    utils::log_info("Backend initialization complete");
}

RotationTradingBackend::~RotationTradingBackend() {
    if (trading_active_) {
        stop_trading();
    }
}

// === Trading Session Management ===

bool RotationTradingBackend::warmup(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("========================================");
    utils::log_info("Warmup Phase");
    utils::log_info("========================================");
    std::cout << "[Backend] Starting warmup with " << symbol_bars.size() << " symbols" << std::endl;

    // Log warmup data sizes
    for (const auto& [symbol, bars] : symbol_bars) {
        utils::log_info("  " + symbol + ": " + std::to_string(bars.size()) + " warmup bars");
        std::cout << "[Backend]   " << symbol << ": " << bars.size() << " warmup bars" << std::endl;
    }

    bool success = oes_manager_->warmup_all(symbol_bars);

    // Check individual readiness
    auto ready_status = oes_manager_->get_ready_status();
    for (const auto& [symbol, is_ready] : ready_status) {
        utils::log_info("  " + symbol + ": " + (is_ready ? "READY" : "NOT READY"));
        std::cout << "[Backend]   " << symbol << ": " << (is_ready ? "READY" : "NOT READY") << std::endl;
    }

    if (success) {
        utils::log_info("✓ Warmup complete - all OES instances ready");
        std::cout << "[Backend] ✓ Warmup complete - all OES instances ready" << std::endl;
    } else {
        utils::log_error("Warmup failed - some OES instances not ready");
        std::cout << "[Backend] ❌ Warmup failed - some OES instances not ready" << std::endl;
    }

    return success;
}

bool RotationTradingBackend::start_trading() {
    utils::log_info("========================================");
    utils::log_info("Starting Trading Session");
    utils::log_info("========================================");
    std::cout << "[Backend] Checking if ready..." << std::endl;

    // Check if ready
    if (!is_ready()) {
        utils::log_error("Cannot start trading - backend not ready");
        std::cout << "[Backend] ❌ Cannot start trading - backend not ready" << std::endl;

        // Debug: Check which OES instances are not ready
        auto ready_status = oes_manager_->get_ready_status();
        for (const auto& [symbol, is_ready] : ready_status) {
            if (!is_ready) {
                utils::log_error("  " + symbol + " is NOT READY");
                std::cout << "[Backend]   " << symbol << " is NOT READY" << std::endl;
            }
        }

        return false;
    }

    std::cout << "[Backend] ✓ Backend is ready" << std::endl;

    // Open log files
    std::cout << "[Backend] Opening log files..." << std::endl;

    if (config_.log_all_signals) {
        std::cout << "[Backend]   Signal log: " << config_.signal_log_path << std::endl;
        signal_log_.open(config_.signal_log_path, std::ios::out | std::ios::trunc);
        if (!signal_log_.is_open()) {
            utils::log_error("Failed to open signal log: " + config_.signal_log_path);
            std::cout << "[Backend] ❌ Failed to open signal log: " << config_.signal_log_path << std::endl;
            return false;
        }
    }

    if (config_.log_all_decisions) {
        std::cout << "[Backend]   Decision log: " << config_.decision_log_path << std::endl;
        decision_log_.open(config_.decision_log_path, std::ios::out | std::ios::trunc);
        if (!decision_log_.is_open()) {
            utils::log_error("Failed to open decision log: " + config_.decision_log_path);
            std::cout << "[Backend] ❌ Failed to open decision log: " << config_.decision_log_path << std::endl;
            return false;
        }
    }

    std::cout << "[Backend]   Trade log: " << config_.trade_log_path << std::endl;
    trade_log_.open(config_.trade_log_path, std::ios::out | std::ios::trunc);
    if (!trade_log_.is_open()) {
        utils::log_error("Failed to open trade log: " + config_.trade_log_path);
        std::cout << "[Backend] ❌ Failed to open trade log: " << config_.trade_log_path << std::endl;
        return false;
    }

    std::cout << "[Backend]   Position log: " << config_.position_log_path << std::endl;
    position_log_.open(config_.position_log_path, std::ios::out | std::ios::trunc);
    if (!position_log_.is_open()) {
        utils::log_error("Failed to open position log: " + config_.position_log_path);
        std::cout << "[Backend] ❌ Failed to open position log: " << config_.position_log_path << std::endl;
        return false;
    }

    // Initialize session stats
    session_stats_ = SessionStats();
    session_stats_.session_start = std::chrono::system_clock::now();
    session_stats_.current_equity = config_.starting_capital;
    session_stats_.max_equity = config_.starting_capital;
    session_stats_.min_equity = config_.starting_capital;

    trading_active_ = true;
    is_warmup_ = false;  // End warmup mode, start actual trading

    utils::log_info("✓ Trading session started");
    utils::log_info("✓ Warmup mode disabled - trades will now execute");
    utils::log_info("  Signal log: " + config_.signal_log_path);
    utils::log_info("  Decision log: " + config_.decision_log_path);
    utils::log_info("  Trade log: " + config_.trade_log_path);
    utils::log_info("  Position log: " + config_.position_log_path);

    return true;
}

void RotationTradingBackend::stop_trading() {
    if (!trading_active_) {
        return;
    }

    utils::log_info("========================================");
    utils::log_info("Stopping Trading Session");
    utils::log_info("========================================");

    // DIAGNOSTIC: Pre-liquidation state
    utils::log_info("========================================");
    utils::log_info("Pre-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Cash: $" + std::to_string(current_cash_));
    utils::log_info("Allocated Capital: $" + std::to_string(allocated_capital_));

    auto positions = rotation_manager_->get_positions();
    double unrealized_total = 0.0;

    for (const auto& [symbol, pos] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_[symbol];
            double entry_cost = position_entry_costs_[symbol];
            double current_value = shares * pos.current_price;
            double unrealized = current_value - entry_cost;
            unrealized_total += unrealized;

            utils::log_info("Position " + symbol + ": " +
                          std::to_string(shares) + " shares, " +
                          "entry_cost=$" + std::to_string(entry_cost) +
                          ", current_value=$" + std::to_string(current_value) +
                          ", unrealized=$" + std::to_string(unrealized) +
                          " (" + std::to_string(unrealized / entry_cost * 100.0) + "%)");
        }
    }

    utils::log_info("Total Unrealized P&L: $" + std::to_string(unrealized_total));
    double pre_liquidation_equity = current_cash_ + allocated_capital_ + unrealized_total;
    utils::log_info("Pre-liquidation Equity: $" + std::to_string(pre_liquidation_equity) +
                   " (" + std::to_string(pre_liquidation_equity / config_.starting_capital * 100.0) + "%)");

    // Liquidate all positions
    if (rotation_manager_->get_position_count() > 0) {
        utils::log_info("========================================");
        utils::log_info("Liquidating " + std::to_string(positions.size()) + " positions...");
        liquidate_all_positions("Session End");
    }

    // Update session stats after liquidation
    update_session_stats();

    // DIAGNOSTIC: Post-liquidation state
    utils::log_info("========================================");
    utils::log_info("Post-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Final Cash: $" + std::to_string(current_cash_));
    utils::log_info("Final Allocated: $" + std::to_string(allocated_capital_) +
                   " (should be ~$0)");
    utils::log_info("Final Equity (from stats): $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");

    // Close log files
    if (signal_log_.is_open()) signal_log_.close();
    if (decision_log_.is_open()) decision_log_.close();
    if (trade_log_.is_open()) trade_log_.close();
    if (position_log_.is_open()) position_log_.close();

    // Finalize session stats
    session_stats_.session_end = std::chrono::system_clock::now();

    trading_active_ = false;

    // Print summary
    utils::log_info("========================================");
    utils::log_info("Session Summary");
    utils::log_info("========================================");
    utils::log_info("Bars processed: " + std::to_string(session_stats_.bars_processed));
    utils::log_info("Signals generated: " + std::to_string(session_stats_.signals_generated));
    utils::log_info("Trades executed: " + std::to_string(session_stats_.trades_executed));
    utils::log_info("Positions opened: " + std::to_string(session_stats_.positions_opened));
    utils::log_info("Positions closed: " + std::to_string(session_stats_.positions_closed));
    utils::log_info("Rotations: " + std::to_string(session_stats_.rotations));
    utils::log_info("");
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");
    utils::log_info("Final equity: $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Max drawdown: " + std::to_string(session_stats_.max_drawdown * 100.0) + "%");
    utils::log_info("Win rate: " + std::to_string(session_stats_.win_rate * 100.0) + "%");
    utils::log_info("Sharpe ratio: " + std::to_string(session_stats_.sharpe_ratio));
    utils::log_info("MRD: " + std::to_string(session_stats_.mrd * 100.0) + "%");
    utils::log_info("========================================");
}

bool RotationTradingBackend::on_bar() {
    if (!trading_active_) {
        utils::log_error("Cannot process bar - trading not active");
        return false;
    }

    session_stats_.bars_processed++;

    // Step 1: Update OES on_bar (updates feature engines)
    oes_manager_->on_bar();

    // Step 2: Generate signals
    auto signals = generate_signals();
    session_stats_.signals_generated += signals.size();

    // Log signals
    if (config_.log_all_signals) {
        for (const auto& [symbol, signal] : signals) {
            log_signal(symbol, signal);
        }
    }

    // Step 3: Rank signals
    auto ranked_signals = rank_signals(signals);

    // CRITICAL FIX: Circuit breaker - check for large losses or minimum capital
    // IMPORTANT: Calculate total unrealized P&L using current position values
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();
    for (const auto& [symbol, position] : positions) {
        if (position_entry_costs_.count(symbol) > 0 && position_shares_.count(symbol) > 0) {
            double entry_cost = position_entry_costs_.at(symbol);
            int shares = position_shares_.at(symbol);
            double current_value = shares * position.current_price;
            double pnl = current_value - entry_cost;
            unrealized_pnl += pnl;
        }
    }
    double current_equity = current_cash_ + allocated_capital_ + unrealized_pnl;
    double equity_pct = current_equity / config_.starting_capital;
    const double MIN_TRADING_CAPITAL = 10000.0;  // $10k minimum to continue trading

    // Use stderr to ensure this shows up
    std::cerr << "[EQUITY] cash=$" << current_cash_
              << ", allocated=$" << allocated_capital_
              << ", unrealized=$" << unrealized_pnl
              << ", equity=$" << current_equity
              << " (" << (equity_pct * 100.0) << "%)" << std::endl;

    if (!circuit_breaker_triggered_) {
        if (equity_pct < 0.60) {  // 40% loss threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - LARGE LOSS DETECTED          ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (" + std::to_string(equity_pct * 100.0) + "% of start)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        } else if (current_equity < MIN_TRADING_CAPITAL) {  // Minimum capital threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - MINIMUM CAPITAL BREACH       ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (below $" + std::to_string(MIN_TRADING_CAPITAL) + " minimum)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        }
    }

    // Step 4: Check for EOD
    int current_time_minutes = get_current_time_minutes();

    if (is_eod(current_time_minutes)) {
        utils::log_info("EOD reached - liquidating all positions");
        liquidate_all_positions("EOD");
        return true;
    }

    // Step 5: Make position decisions
    auto decisions = make_decisions(ranked_signals);

    // DIAGNOSTIC: Log received decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ BACKEND RECEIVED " + std::to_string(decisions.size()) + " DECISIONS FROM make_decisions()     ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");

    // Log decisions
    if (config_.log_all_decisions) {
        for (const auto& decision : decisions) {
            log_decision(decision);
        }
    }

    // Step 6: Execute decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ EXECUTING DECISIONS (skipping HOLDs)                      ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");
    int executed_count = 0;
    for (const auto& decision : decisions) {
        if (decision.decision != RotationPositionManager::Decision::HOLD) {
            utils::log_info(">>> EXECUTING decision #" + std::to_string(executed_count + 1) +
                          ": " + decision.symbol);
            execute_decision(decision);
            executed_count++;
        }
    }
    utils::log_info(">>> EXECUTED " + std::to_string(executed_count) + " decisions (skipped " +
                   std::to_string(decisions.size() - executed_count) + " HOLDs)");

    // Step 7: Update learning
    update_learning();

    // Step 8: Log positions
    log_positions();

    // Step 9: Update statistics
    update_session_stats();

    return true;
}

bool RotationTradingBackend::is_eod(int current_time_minutes) const {
    return current_time_minutes >= config_.rotation_config.eod_exit_time_minutes;
}

bool RotationTradingBackend::liquidate_all_positions(const std::string& reason) {
    auto positions = rotation_manager_->get_positions();

    utils::log_info("[EOD] Liquidating " + std::to_string(positions.size()) +
                   " positions. Reason: " + reason);
    utils::log_info("[EOD] Cash before: $" + std::to_string(current_cash_) +
                   ", Allocated: $" + std::to_string(allocated_capital_));

    for (const auto& [symbol, position] : positions) {
        // Get tracking info for logging
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double exit_value = shares * position.current_price;
            double realized_pnl = exit_value - entry_cost;

            utils::log_info("[EOD] Liquidating " + symbol + ": " +
                          std::to_string(shares) + " shares @ $" +
                          std::to_string(position.current_price) +
                          ", proceeds=$" + std::to_string(exit_value) +
                          ", P&L=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl / entry_cost * 100.0) + "%)");
        }

        // Create EOD exit decision
        RotationPositionManager::PositionDecision decision;
        decision.symbol = symbol;
        decision.decision = RotationPositionManager::Decision::EOD_EXIT;
        decision.position = position;
        decision.reason = reason;

        // Execute (this handles all accounting via execute_decision)
        execute_decision(decision);
    }

    utils::log_info("[EOD] Liquidation complete. Final cash: $" +
                   std::to_string(current_cash_) +
                   ", Final allocated: $" + std::to_string(allocated_capital_));

    // Verify accounting - allocated should be 0 or near-0 after liquidation
    if (std::abs(allocated_capital_) > 0.01) {
        utils::log_error("[EOD] WARNING: Allocated capital should be ~0 but is $" +
                        std::to_string(allocated_capital_) +
                        " after liquidation!");
    }

    return true;
}

// === State Access ===

bool RotationTradingBackend::is_ready() const {
    return oes_manager_->all_ready();
}

double RotationTradingBackend::get_current_equity() const {
    // CRITICAL FIX: Calculate proper unrealized P&L using tracked positions
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();

    for (const auto& [symbol, position] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double current_value = shares * position.current_price;
            unrealized_pnl += (current_value - entry_cost);
        }
    }

    // CRITICAL FIX: Include allocated_capital_ which represents entry costs of positions
    return current_cash_ + allocated_capital_ + unrealized_pnl;
}

void RotationTradingBackend::update_config(const Config& new_config) {
    config_ = new_config;

    // Update component configs
    oes_manager_->update_config(new_config.oes_config);
    signal_aggregator_->update_config(new_config.aggregator_config);
    rotation_manager_->update_config(new_config.rotation_config);
}

// === Private Methods ===

std::map<std::string, SignalOutput> RotationTradingBackend::generate_signals() {
    return oes_manager_->generate_all_signals();
}

std::vector<SignalAggregator::RankedSignal> RotationTradingBackend::rank_signals(
    const std::map<std::string, SignalOutput>& signals
) {
    // Get staleness weights from data manager
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> staleness_weights;

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        staleness_weights[symbol] = snap.staleness_weight;
    }

    return signal_aggregator_->rank_signals(signals, staleness_weights);
}

std::vector<RotationPositionManager::PositionDecision>
RotationTradingBackend::make_decisions(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) {
    // Get current prices
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> current_prices;

    // FIX 1: Diagnostic logging to identify data synchronization issues
    static int call_count = 0;
    if (call_count++ % 100 == 0) {  // Log every 100 calls to avoid spam
        utils::log_info("[DEBUG] make_decisions() call #" + std::to_string(call_count) +
                       ": Snapshot has " + std::to_string(snapshot.snapshots.size()) + " symbols");
    }

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_prices[symbol] = snap.latest_bar.close;

        if (call_count % 100 == 0) {
            utils::log_info("[DEBUG]   " + symbol + " price: " +
                           std::to_string(snap.latest_bar.close) +
                           " (bar_id: " + std::to_string(snap.latest_bar.bar_id) + ")");
        }
    }

    if (current_prices.empty()) {
        utils::log_error("[CRITICAL] No current prices available for position decisions!");
        utils::log_error("  Snapshot size: " + std::to_string(snapshot.snapshots.size()));
        utils::log_error("  Data manager appears to have no data");
    }

    int current_time_minutes = get_current_time_minutes();

    return rotation_manager_->make_decisions(
        ranked_signals,
        current_prices,
        current_time_minutes
    );
}

bool RotationTradingBackend::execute_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!config_.enable_trading) {
        // Dry run mode - just log
        utils::log_info("[DRY RUN] " + decision.symbol + ": " +
                       std::to_string(static_cast<int>(decision.decision)));
        return true;
    }

    // WARMUP FIX: Skip trade execution during warmup phase
    if (is_warmup_) {
        utils::log_info("[WARMUP] Skipping trade execution for " + decision.symbol +
                       " (warmup mode active)");
        return true;  // Return success but don't execute
    }

    // CRITICAL FIX: Circuit breaker - block new entries if triggered
    bool is_entry = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                     decision.decision == RotationPositionManager::Decision::ENTER_SHORT);

    if (circuit_breaker_triggered_ && is_entry) {
        utils::log_warning("[CIRCUIT BREAKER] Blocking new entry for " + decision.symbol +
                          " - circuit breaker active due to large losses");
        return false;  // Block entry
    }

    // Get execution price
    std::string side = (decision.decision == RotationPositionManager::Decision::ENTER_LONG) ?
                       "BUY" : "SELL";
    double execution_price = get_execution_price(decision.symbol, side);

    // Calculate position size
    int shares = 0;
    double position_cost = 0.0;

    if (is_entry) {

        shares = calculate_position_size(decision);

        if (shares == 0) {
            utils::log_warning("Position size is 0 for " + decision.symbol + " - skipping");
            return false;
        }

        // CRITICAL FIX: Validate we have sufficient cash BEFORE proceeding
        position_cost = shares * execution_price;

        if (position_cost > current_cash_) {
            utils::log_error("INSUFFICIENT FUNDS: Need $" + std::to_string(position_cost) +
                           " but only have $" + std::to_string(current_cash_) +
                           " for " + decision.symbol);
            return false;
        }

        // PRE-DEDUCT cash to prevent over-allocation race condition
        current_cash_ -= position_cost;
        utils::log_info("Pre-deducted $" + std::to_string(position_cost) +
                       " for " + decision.symbol +
                       " (remaining cash: $" + std::to_string(current_cash_) + ")");

    }

    // Execute with rotation manager
    bool success = rotation_manager_->execute_decision(decision, execution_price);

    // Variables for tracking realized P&L (for EXIT trades)
    double realized_pnl = std::numeric_limits<double>::quiet_NaN();
    double realized_pnl_pct = std::numeric_limits<double>::quiet_NaN();

    if (success) {
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            // Cash already deducted above, track allocated capital
            allocated_capital_ += position_cost;

            // CRITICAL FIX: Track entry cost and shares for this position
            position_entry_costs_[decision.symbol] = position_cost;
            position_shares_[decision.symbol] = shares;

            session_stats_.positions_opened++;
            session_stats_.trades_executed++;

            utils::log_info("Entry: allocated $" + std::to_string(position_cost) +
                          " for " + decision.symbol + " (" + std::to_string(shares) + " shares)");

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }
        } else {
            // Exit - return cash and release allocated capital
            // CRITICAL FIX: Use tracked entry cost and shares
            if (position_entry_costs_.count(decision.symbol) == 0) {
                utils::log_error("CRITICAL: No entry cost tracked for " + decision.symbol);
                return false;
            }

            double entry_cost = position_entry_costs_[decision.symbol];
            int exit_shares = position_shares_[decision.symbol];
            double exit_value = exit_shares * execution_price;

            current_cash_ += exit_value;
            allocated_capital_ -= entry_cost;  // Remove the original allocation

            // Remove from tracking maps
            position_entry_costs_.erase(decision.symbol);
            position_shares_.erase(decision.symbol);

            session_stats_.positions_closed++;
            session_stats_.trades_executed++;

            // Calculate realized P&L for this exit
            realized_pnl = exit_value - entry_cost;
            realized_pnl_pct = realized_pnl / entry_cost;

            utils::log_info("Exit: " + decision.symbol +
                          " - entry_cost=$" + std::to_string(entry_cost) +
                          ", exit_value=$" + std::to_string(exit_value) +
                          ", realized_pnl=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl_pct * 100.0) + "%)");

            // Track realized P&L for learning
            realized_pnls_[decision.symbol] = realized_pnl;

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error after exit: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }

            // Update shares for logging
            shares = exit_shares;
        }

        // Track rotations
        if (decision.decision == RotationPositionManager::Decision::ROTATE_OUT) {
            session_stats_.rotations++;
        }

        // Log trade (with actual realized P&L for exits)
        log_trade(decision, execution_price, shares, realized_pnl, realized_pnl_pct);
    } else {
        // ROLLBACK on failure for entry positions
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            current_cash_ += position_cost;  // Restore cash
            utils::log_error("Failed to execute " + decision.symbol +
                           " - rolled back $" + std::to_string(position_cost) +
                           " (cash now: $" + std::to_string(current_cash_) + ")");
        }
    }

    return success;
}

double RotationTradingBackend::get_execution_price(
    const std::string& symbol,
    const std::string& side
) {
    auto snapshot = data_manager_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        utils::log_error("CRITICAL: No data for " + symbol + " - cannot get price");
        // Return last known price if available, or throw
        if (rotation_manager_->has_position(symbol)) {
            auto& positions = rotation_manager_->get_positions();
            return positions.at(symbol).current_price;
        }
        throw std::runtime_error("No price available for " + symbol);
    }

    double price = snapshot.snapshots.at(symbol).latest_bar.close;
    if (price <= 0.0) {
        throw std::runtime_error("Invalid price for " + symbol + ": " + std::to_string(price));
    }

    return price;
}

int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    // CRITICAL FIX: Use current equity (not starting capital) to prevent over-allocation
    // This adapts position sizing to account for current P&L
    double current_equity = current_cash_ + allocated_capital_;
    int max_positions = config_.rotation_config.max_positions;
    double fixed_allocation = (current_equity * 0.95) / max_positions;

    // But still check against available cash
    double available_cash = current_cash_;
    double allocation = std::min(fixed_allocation, available_cash * 0.95);

    if (allocation <= 100.0) {
        utils::log_warning("Insufficient cash for position: $" +
                          std::to_string(available_cash) +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) + ")");
        return 0;  // Don't trade with less than $100
    }

    // Get execution price
    double price = get_execution_price(decision.symbol, "BUY");
    if (price <= 0) {
        utils::log_error("Invalid price for position sizing: " +
                        std::to_string(price));
        return 0;
    }

    int shares = static_cast<int>(allocation / price);

    // Final validation - ensure position doesn't exceed available cash
    double position_value = shares * price;
    if (position_value > available_cash) {
        shares = static_cast<int>(available_cash / price);
    }

    // Validate we got non-zero shares
    if (shares == 0) {
        utils::log_warning("[POSITION SIZE] Calculated 0 shares for " + decision.symbol +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) +
                          ", available=$" + std::to_string(available_cash) +
                          ", allocation=$" + std::to_string(allocation) +
                          ", price=$" + std::to_string(price) + ")");

        // Force minimum 1 share if we have enough capital
        if (allocation >= price) {
            utils::log_info("[POSITION SIZE] Forcing minimum 1 share");
            shares = 1;
        } else {
            utils::log_error("[POSITION SIZE] Insufficient capital even for 1 share - skipping");
            return 0;
        }
    }

    utils::log_info("Position sizing for " + decision.symbol +
                   ": fixed_alloc=$" + std::to_string(fixed_allocation) +
                   ", available=$" + std::to_string(available_cash) +
                   ", allocation=$" + std::to_string(allocation) +
                   ", price=$" + std::to_string(price) +
                   ", shares=" + std::to_string(shares) +
                   ", value=$" + std::to_string(shares * price));

    return shares;
}

void RotationTradingBackend::update_learning() {
    // FIX #1: Continuous Learning Feedback
    // Predictor now receives bar-to-bar returns EVERY bar, not just on exits
    // This is critical for learning - predictor needs frequent feedback

    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> bar_returns;

    // Calculate bar-to-bar return for each symbol
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        auto history = data_manager_->get_recent_bars(symbol, 2);
        if (history.size() >= 2) {
            // Return = (current_close - previous_close) / previous_close
            double bar_return = (history[0].close - history[1].close) / history[1].close;
            bar_returns[symbol] = bar_return;
        }
    }

    // Update all predictors with bar-to-bar returns (weight = 1.0)
    if (!bar_returns.empty()) {
        oes_manager_->update_all(bar_returns);
    }

    // ALSO update with realized P&L when positions exit (weight = 10.0)
    // Realized P&L is more important than bar-to-bar noise
    if (!realized_pnls_.empty()) {
        // Scale realized P&L by 10x to give it more weight in learning
        std::map<std::string, double> weighted_pnls;
        for (const auto& [symbol, pnl] : realized_pnls_) {
            // Convert P&L to return percentage
            double return_pct = pnl / config_.starting_capital;
            weighted_pnls[symbol] = return_pct * 10.0;  // 10x weight
        }
        oes_manager_->update_all(weighted_pnls);
        realized_pnls_.clear();
    }
}

void RotationTradingBackend::log_signal(
    const std::string& symbol,
    const SignalOutput& signal
) {
    if (!signal_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = signal.timestamp_ms;
    j["bar_id"] = signal.bar_id;
    j["symbol"] = symbol;
    j["signal"] = static_cast<int>(signal.signal_type);
    j["probability"] = signal.probability;
    j["confidence"] = signal.confidence;

    signal_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!decision_log_.is_open()) {
        return;
    }

    json j;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["reason"] = decision.reason;

    if (decision.decision != RotationPositionManager::Decision::HOLD) {
        j["rank"] = decision.signal.rank;
        j["strength"] = decision.signal.strength;
    }

    decision_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_trade(
    const RotationPositionManager::PositionDecision& decision,
    double execution_price,
    int shares,
    double realized_pnl,
    double realized_pnl_pct
) {
    if (!trade_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = data_manager_->get_latest_snapshot().logical_timestamp_ms;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["exec_price"] = execution_price;
    j["shares"] = shares;
    j["action"] = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                   decision.decision == RotationPositionManager::Decision::ENTER_SHORT) ?
                  "ENTRY" : "EXIT";
    j["direction"] = (decision.signal.signal.signal_type == SignalType::LONG) ?
                     "LONG" : "SHORT";
    j["price"] = execution_price;
    j["value"] = execution_price * shares;
    j["reason"] = decision.reason;  // Add reason for entry/exit

    // Add P&L for exits
    if (decision.decision != RotationPositionManager::Decision::ENTER_LONG &&
        decision.decision != RotationPositionManager::Decision::ENTER_SHORT) {
        // CRITICAL FIX: Use actual realized P&L passed from execute_decision (exit_value - entry_cost)
        if (!std::isnan(realized_pnl) && !std::isnan(realized_pnl_pct)) {
            j["pnl"] = realized_pnl;
            j["pnl_pct"] = realized_pnl_pct;
        } else {
            // Fallback to position P&L (should not happen for EXIT trades)
            j["pnl"] = decision.position.pnl * shares;
            j["pnl_pct"] = decision.position.pnl_pct;
        }
        j["bars_held"] = decision.position.bars_held;
    } else {
        // For ENTRY trades, add signal metadata
        j["signal_probability"] = decision.signal.signal.probability;
        j["signal_confidence"] = decision.signal.signal.confidence;
        j["signal_rank"] = decision.signal.rank;
    }

    trade_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_positions() {
    if (!position_log_.is_open()) {
        return;
    }

    json j;
    j["bar"] = session_stats_.bars_processed;
    j["positions"] = json::array();

    for (const auto& [symbol, position] : rotation_manager_->get_positions()) {
        json pos_j;
        pos_j["symbol"] = symbol;
        pos_j["direction"] = (position.direction == SignalType::LONG) ? "LONG" : "SHORT";
        pos_j["entry_price"] = position.entry_price;
        pos_j["current_price"] = position.current_price;
        pos_j["pnl"] = position.pnl;
        pos_j["pnl_pct"] = position.pnl_pct;
        pos_j["bars_held"] = position.bars_held;
        pos_j["current_rank"] = position.current_rank;
        pos_j["current_strength"] = position.current_strength;

        j["positions"].push_back(pos_j);
    }

    j["total_unrealized_pnl"] = rotation_manager_->get_total_unrealized_pnl();
    j["current_equity"] = get_current_equity();

    position_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::update_session_stats() {
    // Calculate current equity using CORRECT formula (cash + allocated + unrealized)
    session_stats_.current_equity = get_current_equity();

    // Track equity curve
    equity_curve_.push_back(session_stats_.current_equity);

    // Update max/min equity
    if (session_stats_.current_equity > session_stats_.max_equity) {
        session_stats_.max_equity = session_stats_.current_equity;
    }
    if (session_stats_.current_equity < session_stats_.min_equity) {
        session_stats_.min_equity = session_stats_.current_equity;
    }

    // Calculate drawdown
    double drawdown = (session_stats_.max_equity - session_stats_.current_equity) /
                     session_stats_.max_equity;
    if (drawdown > session_stats_.max_drawdown) {
        session_stats_.max_drawdown = drawdown;
    }

    // Calculate total P&L from FULL equity (not just cash!)
    session_stats_.total_pnl = session_stats_.current_equity - config_.starting_capital;
    session_stats_.total_pnl_pct = session_stats_.total_pnl / config_.starting_capital;

    // Diagnostic logging every 100 bars
    if (session_stats_.bars_processed % 100 == 0) {
        // Calculate unrealized P&L for logging
        double unrealized_pnl = 0.0;
        auto positions = rotation_manager_->get_positions();
        for (const auto& [symbol, position] : positions) {
            if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
                int shares = position_shares_.at(symbol);
                double entry_cost = position_entry_costs_.at(symbol);
                double current_value = shares * position.current_price;
                unrealized_pnl += (current_value - entry_cost);
            }
        }

        utils::log_info("[STATS] Bar " + std::to_string(session_stats_.bars_processed) +
                       ": Cash=$" + std::to_string(current_cash_) +
                       ", Allocated=$" + std::to_string(allocated_capital_) +
                       ", Unrealized=$" + std::to_string(unrealized_pnl) +
                       ", Equity=$" + std::to_string(session_stats_.current_equity) +
                       ", P&L=" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%");
    }

    // Calculate returns for Sharpe
    if (equity_curve_.size() > 1) {
        double ret = (equity_curve_.back() - equity_curve_[equity_curve_.size() - 2]) /
                     equity_curve_[equity_curve_.size() - 2];
        returns_.push_back(ret);
    }

    // Calculate Sharpe ratio (if enough data)
    if (returns_.size() >= 20) {
        double mean_return = 0.0;
        for (double r : returns_) {
            mean_return += r;
        }
        mean_return /= returns_.size();

        double variance = 0.0;
        for (double r : returns_) {
            variance += (r - mean_return) * (r - mean_return);
        }
        variance /= returns_.size();

        double std_dev = std::sqrt(variance);
        if (std_dev > 0.0) {
            // Annualize: 390 bars per day, ~252 trading days
            session_stats_.sharpe_ratio = (mean_return / std_dev) * std::sqrt(390.0 * 252.0);
        }
    }

    // Calculate MRD (Mean Return per Day)
    // Assume 390 bars per day
    if (session_stats_.bars_processed >= 390) {
        int trading_days = session_stats_.bars_processed / 390;
        session_stats_.mrd = session_stats_.total_pnl_pct / trading_days;
    }
}

int RotationTradingBackend::get_current_time_minutes() const {
    // Calculate minutes since market open (9:30 AM ET)
    // Works for both mock and live modes

    auto snapshot = data_manager_->get_latest_snapshot();
    if (snapshot.snapshots.empty()) {
        return 0;
    }

    // Get first symbol's timestamp
    auto first_snap = snapshot.snapshots.begin()->second;
    int64_t timestamp_ms = first_snap.latest_bar.timestamp_ms;

    // Convert to time-of-day (assuming ET timezone)
    int64_t timestamp_sec = timestamp_ms / 1000;
    std::time_t t = timestamp_sec;
    std::tm* tm_info = std::localtime(&t);

    if (!tm_info) {
        utils::log_error("Failed to convert timestamp to local time");
        return 0;
    }

    // Calculate minutes since market open (9:30 AM)
    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;
    int minutes_since_midnight = hour * 60 + minute;
    constexpr int market_open_minutes = 9 * 60 + 30;  // 9:30 AM = 570 minutes
    int minutes_since_open = minutes_since_midnight - market_open_minutes;

    return minutes_since_open;
}

} // namespace sentio

```

## 📄 **FILE 21 of 32**: src/cli/command_registry.cpp

**File Information**:
- **Path**: `src/cli/command_registry.cpp`
- **Size**: 619 lines
- **Modified**: 2025-10-15 09:43:25
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/command_registry.h"
// Production trading system - only mock and live modes
#include "cli/rotation_trade_command.h"
#ifdef XGBOOST_AVAILABLE
#include "cli/train_command.h"
#endif
#ifdef TORCH_AVAILABLE
// PPO training command removed from this project scope
#endif
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sentio::cli {

// ================================================================================================
// COMMAND REGISTRY IMPLEMENTATION
// ================================================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::register_command(const std::string& name, 
                                      std::shared_ptr<Command> command,
                                      const CommandInfo& info) {
    CommandInfo cmd_info = info;
    cmd_info.command = command;
    if (cmd_info.description.empty()) {
        cmd_info.description = command->get_description();
    }
    
    commands_[name] = cmd_info;
    
    // Register aliases
    for (const auto& alias : cmd_info.aliases) {
        AliasInfo alias_info;
        alias_info.target_command = name;
        aliases_[alias] = alias_info;
    }
}

void CommandRegistry::register_alias(const std::string& alias, 
                                    const std::string& target_command,
                                    const AliasInfo& info) {
    AliasInfo alias_info = info;
    alias_info.target_command = target_command;
    aliases_[alias] = alias_info;
}

void CommandRegistry::deprecate_command(const std::string& name, 
                                       const std::string& replacement,
                                       const std::string& message) {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        it->second.deprecated = true;
        it->second.replacement_command = replacement;
        it->second.deprecation_message = message.empty() ? 
            "This command is deprecated. Use '" + replacement + "' instead." : message;
    }
}

std::shared_ptr<Command> CommandRegistry::get_command(const std::string& name) {
    // Check direct command first
    auto cmd_it = commands_.find(name);
    if (cmd_it != commands_.end()) {
        if (cmd_it->second.deprecated) {
            show_deprecation_warning(name, cmd_it->second);
        }
        return cmd_it->second.command;
    }
    
    // Check aliases
    auto alias_it = aliases_.find(name);
    if (alias_it != aliases_.end()) {
        if (alias_it->second.deprecated) {
            show_alias_warning(name, alias_it->second);
        }
        
        auto target_it = commands_.find(alias_it->second.target_command);
        if (target_it != commands_.end()) {
            return target_it->second.command;
        }
    }
    
    return nullptr;
}

bool CommandRegistry::has_command(const std::string& name) const {
    return commands_.find(name) != commands_.end() || 
           aliases_.find(name) != aliases_.end();
}

std::vector<std::string> CommandRegistry::get_available_commands() const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

std::vector<std::string> CommandRegistry::get_commands_by_category(const std::string& category) const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (info.category == category && !info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

const CommandRegistry::CommandInfo* CommandRegistry::get_command_info(const std::string& name) const {
    auto it = commands_.find(name);
    return (it != commands_.end()) ? &it->second : nullptr;
}

void CommandRegistry::show_help() const {
    std::cout << "Sentio CLI - Advanced Trading System Command Line Interface\n\n";
    std::cout << "Usage: sentio_cli <command> [options]\n\n";
    
    // Group commands by category
    std::map<std::string, std::vector<std::string>> categories;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            categories[info.category].push_back(name);
        }
    }
    
    // Show each category
    for (const auto& [category, commands] : categories) {
        std::cout << category << " Commands:\n";
        for (const auto& cmd : commands) {
            const auto& info = commands_.at(cmd);
            std::cout << "  " << std::left << std::setw(15) << cmd 
                     << info.description << "\n";
        }
        std::cout << "\n";
    }
    
    std::cout << "Global Options:\n";
    std::cout << "  --help, -h         Show this help message\n";
    std::cout << "  --version, -v      Show version information\n\n";
    
    std::cout << "Use 'sentio_cli <command> --help' for detailed command help.\n";
    std::cout << "Use 'sentio_cli --migration' to see deprecated command alternatives.\n\n";
    
    EnhancedCommandDispatcher::show_usage_examples();
}

void CommandRegistry::show_category_help(const std::string& category) const {
    auto commands = get_commands_by_category(category);
    if (commands.empty()) {
        std::cout << "No commands found in category: " << category << "\n";
        return;
    }
    
    std::cout << category << " Commands:\n\n";
    for (const auto& cmd : commands) {
        const auto& info = commands_.at(cmd);
        std::cout << "  " << cmd << " - " << info.description << "\n";
        
        if (!info.aliases.empty()) {
            std::cout << "    Aliases: " << format_command_list(info.aliases) << "\n";
        }
        
        if (!info.tags.empty()) {
            std::cout << "    Tags: " << format_command_list(info.tags) << "\n";
        }
        std::cout << "\n";
    }
}

void CommandRegistry::show_migration_guide() const {
    std::cout << "Migration Guide - Deprecated Commands\n";
    std::cout << "=====================================\n\n";
    
    bool has_deprecated = false;
    
    for (const auto& [name, info] : commands_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "❌ " << name << " (deprecated)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            if (!info.replacement_command.empty()) {
                std::cout << "   ✅ Use instead: " << info.replacement_command << "\n";
            }
            std::cout << "\n";
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "⚠️  " << alias << " (deprecated alias)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            std::cout << "   ✅ Use instead: " << info.target_command << "\n";
            if (!info.migration_guide.empty()) {
                std::cout << "   📖 Migration: " << info.migration_guide << "\n";
            }
            std::cout << "\n";
        }
    }
    
    if (!has_deprecated) {
        std::cout << "✅ No deprecated commands or aliases found.\n";
        std::cout << "All commands are up-to-date!\n";
    }
}

int CommandRegistry::execute_command(const std::string& name, const std::vector<std::string>& args) {
    auto command = get_command(name);
    if (!command) {
        std::cerr << "❌ Unknown command: " << name << "\n\n";
        
        auto suggestions = suggest_commands(name);
        if (!suggestions.empty()) {
            std::cerr << "💡 Did you mean:\n";
            for (const auto& suggestion : suggestions) {
                std::cerr << "  " << suggestion << "\n";
            }
            std::cerr << "\n";
        }
        
        std::cerr << "Use 'sentio_cli --help' to see available commands.\n";
        return 1;
    }
    
    try {
        return command->execute(args);
    } catch (const std::exception& e) {
        std::cerr << "❌ Command execution failed: " << e.what() << "\n";
        return 1;
    }
}

std::vector<std::string> CommandRegistry::suggest_commands(const std::string& input) const {
    std::vector<std::pair<std::string, int>> candidates;
    
    // Check all commands and aliases
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, name);
            if (distance <= 2 && distance < static_cast<int>(name.length())) {
                candidates.emplace_back(name, distance);
            }
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, alias);
            if (distance <= 2 && distance < static_cast<int>(alias.length())) {
                candidates.emplace_back(alias, distance);
            }
        }
    }
    
    // Sort by distance and return top suggestions
    std::sort(candidates.begin(), candidates.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::vector<std::string> suggestions;
    for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
        suggestions.push_back(candidates[i].first);
    }
    
    return suggestions;
}

void CommandRegistry::initialize_default_commands() {
    // Canonical commands and legacy commands commented out - not implemented yet
    // TODO: Implement these commands when needed

    /* COMMENTED OUT - NOT IMPLEMENTED YET
    // Register canonical commands (new interface)
    CommandInfo generate_info;
    generate_info.category = "Signal Generation";
    generate_info.version = "2.0";
    generate_info.description = "Generate trading signals (canonical interface)";
    generate_info.tags = {"signals", "generation", "canonical"};
    register_command("generate", std::make_shared<GenerateCommand>(), generate_info);

    CommandInfo analyze_info;
    analyze_info.category = "Performance Analysis";
    analyze_info.version = "2.0";
    analyze_info.description = "Analyze trading performance (canonical interface)";
    analyze_info.tags = {"analysis", "performance", "canonical"};
    register_command("analyze", std::make_shared<AnalyzeCommand>(), analyze_info);

    CommandInfo execute_info;
    execute_info.category = "Trade Execution";
    execute_info.version = "2.0";
    execute_info.description = "Execute trades from signals (canonical interface)";
    execute_info.tags = {"trading", "execution", "canonical"};
    register_command("execute", std::make_shared<TradeCanonicalCommand>(), execute_info);

    CommandInfo pipeline_info;
    pipeline_info.category = "Workflows";
    pipeline_info.version = "2.0";
    pipeline_info.description = "Run multi-step trading workflows";
    pipeline_info.tags = {"workflow", "automation", "canonical"};
    register_command("pipeline", std::make_shared<PipelineCommand>(), pipeline_info);

    // Register legacy commands (backward compatibility)
    CommandInfo strattest_info;
    strattest_info.category = "Legacy";
    strattest_info.version = "1.0";
    strattest_info.description = "Generate trading signals (legacy interface)";
    strattest_info.deprecated = false;  // Keep for now
    strattest_info.tags = {"signals", "legacy"};
    register_command("strattest", std::make_shared<StrattestCommand>(), strattest_info);

    CommandInfo audit_info;
    audit_info.category = "Legacy";
    audit_info.version = "1.0";
    audit_info.description = "Analyze performance with reports (legacy interface)";
    audit_info.deprecated = false;  // Keep for now
    audit_info.tags = {"analysis", "legacy"};
    register_command("audit", std::make_shared<AuditCommand>(), audit_info);
    END OF COMMENTED OUT SECTION */

    // Production Trading System - Rotation Strategy Only
    // Supports both mock (date replay) and live (paper/real) modes

    CommandInfo mock_info;
    mock_info.category = "Trading";
    mock_info.version = "2.0";
    mock_info.description = "Run mock trading session (date replay for testing MRD)";
    mock_info.tags = {"mock", "testing", "mrd", "6-instruments"};
    register_command("mock", std::make_shared<RotationTradeCommand>(), mock_info);

    CommandInfo live_info;
    live_info.category = "Trading";
    live_info.version = "2.0";
    live_info.description = "Run live paper trading (Alpaca paper account)";
    live_info.tags = {"live", "paper", "production", "6-instruments"};
    register_command("live", std::make_shared<RotationTradeCommand>(), live_info);

    // Register training commands if available
// XGBoost training now handled by Python scripts (tools/train_xgboost_binary.py)
// C++ train command disabled

#ifdef TORCH_AVAILABLE
    // PPO training command intentionally removed
#endif
}

void CommandRegistry::setup_canonical_aliases() {
    // Canonical command aliases commented out - canonical commands not implemented yet
    /* COMMENTED OUT - CANONICAL COMMANDS NOT IMPLEMENTED
    // Setup helpful aliases for canonical commands
    AliasInfo gen_alias;
    gen_alias.target_command = "generate";
    gen_alias.migration_guide = "Use 'generate' instead of 'strattest' for consistent interface";
    register_alias("gen", "generate", gen_alias);

    AliasInfo report_alias;
    report_alias.target_command = "analyze";
    report_alias.migration_guide = "Use 'analyze report' instead of 'audit report'";
    register_alias("report", "analyze", report_alias);

    AliasInfo run_alias;
    run_alias.target_command = "execute";
    register_alias("run", "execute", run_alias);

    // Deprecate old patterns
    AliasInfo strattest_alias;
    strattest_alias.target_command = "generate";
    strattest_alias.deprecated = true;
    strattest_alias.deprecation_message = "The 'strattest' command interface is being replaced";
    strattest_alias.migration_guide = "Use 'generate --strategy <name> --data <path>' for the new canonical interface";
    // Don't register as alias yet - keep original command for compatibility
    */
}

// ================================================================================================
// PRIVATE HELPER METHODS
// ================================================================================================

void CommandRegistry::show_deprecation_warning(const std::string& command_name, const CommandInfo& info) {
    std::cerr << "⚠️  WARNING: Command '" << command_name << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    if (!info.replacement_command.empty()) {
        std::cerr << "   Use '" << info.replacement_command << "' instead.\n";
    }
    std::cerr << "\n";
}

void CommandRegistry::show_alias_warning(const std::string& alias, const AliasInfo& info) {
    std::cerr << "⚠️  WARNING: Alias '" << alias << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    std::cerr << "   Use '" << info.target_command << "' instead.\n";
    if (!info.migration_guide.empty()) {
        std::cerr << "   Migration: " << info.migration_guide << "\n";
    }
    std::cerr << "\n";
}

std::string CommandRegistry::format_command_list(const std::vector<std::string>& commands) const {
    std::ostringstream ss;
    for (size_t i = 0; i < commands.size(); ++i) {
        ss << commands[i];
        if (i < commands.size() - 1) ss << ", ";
    }
    return ss.str();
}

int CommandRegistry::levenshtein_distance(const std::string& s1, const std::string& s2) const {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = static_cast<int>(j);
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    
    return dp[len1][len2];
}

// ================================================================================================
// ENHANCED COMMAND DISPATCHER IMPLEMENTATION
// ================================================================================================

int EnhancedCommandDispatcher::execute(int argc, char** argv) {
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    // Handle global flags
    if (handle_global_flags(args)) {
        return 0;
    }
    
    std::string command_name = argv[1];
    
    // Handle special cases
    if (command_name == "--help" || command_name == "-h") {
        show_help();
        return 0;
    }
    
    if (command_name == "--version" || command_name == "-v") {
        show_version();
        return 0;
    }
    
    if (command_name == "--migration") {
        CommandRegistry::instance().show_migration_guide();
        return 0;
    }
    
    // Execute command through registry
    auto& registry = CommandRegistry::instance();
    return registry.execute_command(command_name, args);
}

void EnhancedCommandDispatcher::show_help() {
    CommandRegistry::instance().show_help();
}

void EnhancedCommandDispatcher::show_version() {
    std::cout << "Sentio CLI " << get_version_string() << "\n";
    std::cout << "Advanced Trading System Command Line Interface\n";
    std::cout << "Copyright (c) 2024 Sentio Trading Systems\n\n";
    
    std::cout << "Features:\n";
    std::cout << "  • Multi-strategy signal generation (SGO, AWR, XGBoost, CatBoost)\n";
    std::cout << "  • Advanced portfolio management with leverage\n";
    std::cout << "  • Comprehensive performance analysis\n";
    std::cout << "  • Automated trading workflows\n";
    std::cout << "  • Machine learning model training (Python-side for XGB/CTB)\n\n";
    
    std::cout << "Build Information:\n";
#ifdef TORCH_AVAILABLE
    std::cout << "  • PyTorch/LibTorch: Enabled\n";
#else
    std::cout << "  • PyTorch/LibTorch: Disabled\n";
#endif
#ifdef XGBOOST_AVAILABLE
    std::cout << "  • XGBoost: Enabled\n";
#else
    std::cout << "  • XGBoost: Disabled\n";
#endif
    std::cout << "  • Compiler: " << __VERSION__ << "\n";
    std::cout << "  • Build Date: " << __DATE__ << " " << __TIME__ << "\n";
}

bool EnhancedCommandDispatcher::handle_global_flags(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            show_help();
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            show_version();
            return true;
        }
        if (arg == "--migration") {
            CommandRegistry::instance().show_migration_guide();
            return true;
        }
    }
    return false;
}

void EnhancedCommandDispatcher::show_command_not_found_help(const std::string& command_name) {
    std::cerr << "Command '" << command_name << "' not found.\n\n";
    
    auto& registry = CommandRegistry::instance();
    auto suggestions = registry.suggest_commands(command_name);
    
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:\n";
        for (const auto& suggestion : suggestions) {
            std::cerr << "  " << suggestion << "\n";
        }
        std::cerr << "\n";
    }
    
    std::cerr << "Use 'sentio_cli --help' to see all available commands.\n";
}

void EnhancedCommandDispatcher::show_usage_examples() {
    std::cout << "Production Trading Workflow:\n";
    std::cout << "============================\n\n";

    std::cout << "Step 1: Test with Mock Mode (Date Replay)\n";
    std::cout << "  sentio_cli mock --date 2025-10-14\n";
    std::cout << "  sentio_cli mock --start-date 2025-10-01 --end-date 2025-10-14\n\n";

    std::cout << "Step 2: Verify MRD Performance\n";
    std::cout << "  Target: MRD ≥ 0.5% (positive daily returns)\n";
    std::cout << "  Check: logs/mock_trading/analysis_report.txt\n\n";

    std::cout << "Step 3: Go Live (Paper Trading)\n";
    std::cout << "  sentio_cli live --paper\n";
    std::cout << "  sentio_cli live --paper --dry-run  (no orders, logging only)\n\n";

    std::cout << "Step 4: Production (Real Trading)\n";
    std::cout << "  sentio_cli live --real  (use with caution)\n\n";

    std::cout << "Notes:\n";
    std::cout << "  • All modes use 6-instrument rotation (TQQQ/SQQQ/SPXL/SDS/UVXY/SVXY)\n";
    std::cout << "  • Mock mode: Test strategy on historical dates\n";
    std::cout << "  • Live mode: Execute on Alpaca paper/real account\n";
    std::cout << "  • MRD (Mean Return per Day) is the ultimate performance metric\n\n";
}

std::string EnhancedCommandDispatcher::get_version_string() {
    return "2.0.0-beta";  // Update as needed
}

// ================================================================================================
// COMMAND FACTORY IMPLEMENTATION
// ================================================================================================

std::map<std::string, CommandFactory::CommandCreator> CommandFactory::factories_;

void CommandFactory::register_factory(const std::string& name, CommandCreator creator) {
    factories_[name] = creator;
}

std::shared_ptr<Command> CommandFactory::create_command(const std::string& name) {
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        return it->second();
    }
    return nullptr;
}

void CommandFactory::register_builtin_commands() {
    // Canonical commands and legacy commands not implemented - commented out
    /* COMMENTED OUT - NOT IMPLEMENTED
    // Register factory functions for lazy loading
    register_factory("generate", []() { return std::make_shared<GenerateCommand>(); });
    register_factory("analyze", []() { return std::make_shared<AnalyzeCommand>(); });
    register_factory("execute", []() { return std::make_shared<TradeCanonicalCommand>(); });
    register_factory("pipeline", []() { return std::make_shared<PipelineCommand>(); });

    register_factory("strattest", []() { return std::make_shared<StrattestCommand>(); });
    register_factory("audit", []() { return std::make_shared<AuditCommand>(); });
    register_factory("trade", []() { return std::make_shared<TradeCommand>(); });
    register_factory("full-test", []() { return std::make_shared<FullTestCommand>(); });
    */

    // Production Trading System
    register_factory("mock", []() { return std::make_shared<RotationTradeCommand>(); });
    register_factory("live", []() { return std::make_shared<RotationTradeCommand>(); });
    
// XGBoost training now handled by Python scripts

#ifdef TORCH_AVAILABLE
    register_factory("train_ppo", []() { return std::make_shared<TrainPpoCommand>(); });
#endif
}

} // namespace sentio::cli

```

## 📄 **FILE 22 of 32**: src/cli/rotation_trade_command.cpp

**File Information**:
- **Path**: `src/cli/rotation_trade_command.cpp`
- **Size**: 833 lines
- **Modified**: 2025-10-15 22:41:54
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "cli/rotation_trade_command.h"
#include "common/utils.h"
#include "common/time_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <chrono>
#include <filesystem>
#include <set>
#include <cstdlib>

namespace sentio {
namespace cli {

// Static member for signal handling
static std::atomic<bool> g_shutdown_requested{false};

// Signal handler
static void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << " - initiating graceful shutdown...\n";
    g_shutdown_requested = true;
}

RotationTradeCommand::RotationTradeCommand()
    : options_() {
}

RotationTradeCommand::RotationTradeCommand(const Options& options)
    : options_(options) {
}

RotationTradeCommand::~RotationTradeCommand() {
}

int RotationTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--mode" && i + 1 < args.size()) {
            std::string mode = args[++i];
            options_.is_mock_mode = (mode == "mock" || mode == "backtest");
        } else if (arg == "--data-dir" && i + 1 < args.size()) {
            options_.data_dir = args[++i];
        } else if (arg == "--warmup-dir" && i + 1 < args.size()) {
            options_.warmup_dir = args[++i];
        } else if (arg == "--warmup-days" && i + 1 < args.size()) {
            options_.warmup_days = std::stoi(args[++i]);
        } else if (arg == "--date" && i + 1 < args.size()) {
            options_.test_date = args[++i];
        } else if (arg == "--start-date" && i + 1 < args.size()) {
            options_.start_date = args[++i];
        } else if (arg == "--end-date" && i + 1 < args.size()) {
            options_.end_date = args[++i];
        } else if (arg == "--generate-dashboards") {
            options_.generate_dashboards = true;
        } else if (arg == "--dashboard-dir" && i + 1 < args.size()) {
            options_.dashboard_output_dir = args[++i];
        } else if (arg == "--log-dir" && i + 1 < args.size()) {
            options_.log_dir = args[++i];
        } else if (arg == "--config" && i + 1 < args.size()) {
            options_.config_file = args[++i];
        } else if (arg == "--capital" && i + 1 < args.size()) {
            options_.starting_capital = std::stod(args[++i]);
        } else if (arg == "--help" || arg == "-h") {
            show_help();
            return 0;
        }
    }

    // Get Alpaca credentials from environment if in live mode
    if (!options_.is_mock_mode) {
        const char* api_key = std::getenv("ALPACA_PAPER_API_KEY");
        const char* secret_key = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (api_key) options_.alpaca_api_key = api_key;
        if (secret_key) options_.alpaca_secret_key = secret_key;
    }

    return execute_with_options();
}

void RotationTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli rotation-trade [OPTIONS]\n\n";
    std::cout << "Multi-symbol rotation trading system\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mode <mode>         Trading mode: 'live' or 'mock' (default: live)\n";
    std::cout << "  --date <YYYY-MM-DD>   Test specific date only (mock mode)\n";
    std::cout << "                        Warmup data loaded from prior days\n";
    std::cout << "  --start-date <date>   Start date for batch testing (YYYY-MM-DD)\n";
    std::cout << "  --end-date <date>     End date for batch testing (YYYY-MM-DD)\n";
    std::cout << "                        When specified, runs mock tests for each trading day\n";
    std::cout << "  --generate-dashboards Generate HTML dashboards for batch tests\n";
    std::cout << "  --dashboard-dir <dir> Dashboard output directory (default: <log-dir>/dashboards)\n";
    std::cout << "  --warmup-days <N>     Days of historical data for warmup (default: 4)\n";
    std::cout << "  --data-dir <dir>      Data directory for CSV files (default: data/equities)\n";
    std::cout << "  --warmup-dir <dir>    Warmup data directory (default: data/equities)\n";
    std::cout << "  --log-dir <dir>       Log output directory (default: logs/rotation_trading)\n";
    std::cout << "  --config <file>       Configuration file (default: config/rotation_strategy.json)\n";
    std::cout << "  --capital <amount>    Starting capital (default: 100000.0)\n";
    std::cout << "  --help, -h            Show this help message\n\n";
    std::cout << "Environment Variables (for live mode):\n";
    std::cout << "  ALPACA_PAPER_API_KEY      Alpaca API key\n";
    std::cout << "  ALPACA_PAPER_SECRET_KEY   Alpaca secret key\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Mock trading (backtest)\n";
    std::cout << "  sentio_cli rotation-trade --mode mock --data-dir data/equities\n\n";
    std::cout << "  # Live paper trading\n";
    std::cout << "  export ALPACA_PAPER_API_KEY=your_key\n";
    std::cout << "  export ALPACA_PAPER_SECRET_KEY=your_secret\n";
    std::cout << "  sentio_cli rotation-trade --mode live\n";
}

int RotationTradeCommand::execute_with_options() {
    log_system("========================================");
    log_system("Multi-Symbol Rotation Trading System");
    log_system("========================================");
    log_system("");

    log_system("Mode: " + std::string(options_.is_mock_mode ? "MOCK (Backtest)" : "LIVE (Paper)"));
    log_system("Symbols: " + std::to_string(options_.symbols.size()) + " instruments");
    for (const auto& symbol : options_.symbols) {
        log_system("  - " + symbol);
    }
    log_system("");

    // Setup signal handlers
    setup_signal_handlers();

    // Execute based on mode
    if (options_.is_mock_mode) {
        // Check if batch mode (date range specified)
        if (!options_.start_date.empty() && !options_.end_date.empty()) {
            return execute_batch_mock_trading();
        } else {
            return execute_mock_trading();
        }
    } else {
        return execute_live_trading();
    }
}

RotationTradingBackend::Config RotationTradeCommand::load_config() {
    RotationTradingBackend::Config config;

    // Load from JSON if available
    std::ifstream file(options_.config_file);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load OES config
            if (j.contains("oes_config")) {
                auto oes = j["oes_config"];
                config.oes_config.buy_threshold = oes.value("buy_threshold", 0.53);
                config.oes_config.sell_threshold = oes.value("sell_threshold", 0.47);
                config.oes_config.neutral_zone = oes.value("neutral_zone", 0.06);
                config.oes_config.ewrls_lambda = oes.value("ewrls_lambda", 0.995);
                config.oes_config.warmup_samples = oes.value("warmup_samples", 100);
                config.oes_config.enable_bb_amplification = oes.value("enable_bb_amplification", true);
                config.oes_config.bb_amplification_factor = oes.value("bb_amplification_factor", 0.10);
                config.oes_config.bb_period = oes.value("bb_period", 20);
                config.oes_config.bb_std_dev = oes.value("bb_std_dev", 2.0);
                config.oes_config.bb_proximity_threshold = oes.value("bb_proximity_threshold", 0.30);
                config.oes_config.regularization = oes.value("regularization", 0.01);

                if (oes.contains("horizon_weights")) {
                    config.oes_config.horizon_weights = oes["horizon_weights"].get<std::vector<double>>();
                }
            }

            // Load signal aggregator config
            if (j.contains("signal_aggregator_config")) {
                auto agg = j["signal_aggregator_config"];
                config.aggregator_config.min_probability = agg.value("min_probability", 0.51);
                config.aggregator_config.min_confidence = agg.value("min_confidence", 0.55);
                config.aggregator_config.min_strength = agg.value("min_strength", 0.40);
            }

            // Load leverage boosts
            if (j.contains("symbols") && j["symbols"].contains("leverage_boosts")) {
                auto boosts = j["symbols"]["leverage_boosts"];
                for (const auto& symbol : options_.symbols) {
                    if (boosts.contains(symbol)) {
                        config.aggregator_config.leverage_boosts[symbol] = boosts[symbol];
                    }
                }
            }

            // Load rotation config
            if (j.contains("rotation_manager_config")) {
                auto rot = j["rotation_manager_config"];
                config.rotation_config.max_positions = rot.value("max_positions", 3);
                config.rotation_config.min_strength_to_enter = rot.value("min_strength_to_enter", 0.50);
                config.rotation_config.rotation_strength_delta = rot.value("rotation_strength_delta", 0.10);
                config.rotation_config.profit_target_pct = rot.value("profit_target_pct", 0.03);
                config.rotation_config.stop_loss_pct = rot.value("stop_loss_pct", 0.015);
                config.rotation_config.eod_liquidation = rot.value("eod_liquidation", true);
                config.rotation_config.eod_exit_time_minutes = rot.value("eod_exit_time_minutes", 388);
            }

            log_system("✓ Loaded configuration from: " + options_.config_file);
        } catch (const std::exception& e) {
            log_system("⚠️  Failed to load config: " + std::string(e.what()));
            log_system("   Using default configuration");
        }
    } else {
        log_system("⚠️  Config file not found: " + options_.config_file);
        log_system("   Using default configuration");
    }

    // Set symbols and capital
    config.symbols = options_.symbols;
    config.starting_capital = options_.starting_capital;

    // Set output paths
    config.signal_log_path = options_.log_dir + "/signals.jsonl";
    config.decision_log_path = options_.log_dir + "/decisions.jsonl";
    config.trade_log_path = options_.log_dir + "/trades.jsonl";
    config.position_log_path = options_.log_dir + "/positions.jsonl";

    // Set broker credentials
    config.alpaca_api_key = options_.alpaca_api_key;
    config.alpaca_secret_key = options_.alpaca_secret_key;
    config.paper_trading = options_.paper_trading;

    return config;
}

std::map<std::string, std::vector<Bar>> RotationTradeCommand::load_warmup_data() {
    std::map<std::string, std::vector<Bar>> warmup_data;

    log_system("Loading warmup data...");

    for (const auto& symbol : options_.symbols) {
        std::string filename = options_.warmup_dir + "/" + symbol + "_RTH_NH.csv";

        std::ifstream file(filename);
        if (!file.is_open()) {
            log_system("⚠️  Could not open warmup file for " + symbol + ": " + filename);
            continue;
        }

        std::vector<Bar> bars;
        std::string line;

        // Skip header
        std::getline(file, line);

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::vector<std::string> tokens;
            std::string token;

            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }

            try {
                Bar bar;

                // Support both 6-column and 7-column formats
                if (tokens.size() == 7) {
                    // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert epoch seconds to ms
                    bar.open = std::stod(tokens[2]);
                    bar.high = std::stod(tokens[3]);
                    bar.low = std::stod(tokens[4]);
                    bar.close = std::stod(tokens[5]);
                    bar.volume = std::stoll(tokens[6]);
                } else if (tokens.size() >= 6) {
                    // Format: timestamp,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[0]);
                    bar.open = std::stod(tokens[1]);
                    bar.high = std::stod(tokens[2]);
                    bar.low = std::stod(tokens[3]);
                    bar.close = std::stod(tokens[4]);
                    bar.volume = std::stoll(tokens[5]);
                } else {
                    continue;  // Skip malformed lines
                }

                bars.push_back(bar);
            } catch (const std::exception& e) {
                // Skip malformed lines
                continue;
            }
        }

        if (options_.is_mock_mode && !options_.test_date.empty()) {
            // Date-specific test: Load warmup_days before test_date
            // Parse test_date (YYYY-MM-DD)
            int test_year, test_month, test_day;
            if (std::sscanf(options_.test_date.c_str(), "%d-%d-%d", &test_year, &test_month, &test_day) == 3) {
                // Calculate warmup start date (test_date - warmup_days)
                std::tm test_tm = {};
                test_tm.tm_year = test_year - 1900;
                test_tm.tm_mon = test_month - 1;
                test_tm.tm_mday = test_day - options_.warmup_days;  // Go back warmup_days
                std::mktime(&test_tm);  // Normalize the date

                std::tm end_tm = {};
                end_tm.tm_year = test_year - 1900;
                end_tm.tm_mon = test_month - 1;
                end_tm.tm_mday = test_day - 1;  // Day before test_date
                end_tm.tm_hour = 23;
                end_tm.tm_min = 59;
                std::mktime(&end_tm);

                // Filter bars between warmup_start and test_date-1
                std::vector<Bar> warmup_bars;
                for (const auto& bar : bars) {
                    std::time_t bar_time = bar.timestamp_ms / 1000;
                    std::tm* bar_tm = std::localtime(&bar_time);

                    if (bar_tm &&
                        std::mktime(bar_tm) >= std::mktime(&test_tm) &&
                        std::mktime(bar_tm) <= std::mktime(&end_tm)) {
                        warmup_bars.push_back(bar);
                    }
                }

                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) +
                          " bars (" + std::to_string(options_.warmup_days) + " days warmup before " +
                          options_.test_date + ")");
            } else {
                log_system("⚠️  Invalid date format: " + options_.test_date);
                warmup_data[symbol] = bars;
            }
        } else if (options_.is_mock_mode) {
            // For mock mode (no specific date), use last 1560 bars (4 blocks) for warmup
            // This ensures 50+ bars for indicator warmup (max_period=50) plus 100+ for predictor training
            if (bars.size() > 1560) {
                std::vector<Bar> warmup_bars(bars.end() - 1560, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (4 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        } else {
            // For live mode, use last 7800 bars (20 blocks) for warmup
            if (bars.size() > 7800) {
                std::vector<Bar> warmup_bars(bars.end() - 7800, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (20 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        }
    }

    log_system("");
    return warmup_data;
}

int RotationTradeCommand::execute_mock_trading() {
    log_system("========================================");
    log_system("Mock Trading Mode (Backtest)");
    log_system("========================================");
    log_system("");

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    dm_config.backtest_mode = true;  // Disable timestamp validation for mock trading
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create backend (no broker for mock mode)
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, nullptr);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // Create mock feed
    data::MockMultiSymbolFeed::Config feed_config;
    for (const auto& symbol : options_.symbols) {
        feed_config.symbol_files[symbol] = options_.data_dir + "/" + symbol + "_RTH_NH.csv";
    }
    feed_config.replay_speed = 0.0;  // Instant replay for testing
    feed_config.filter_date = options_.test_date;  // Apply date filter if specified

    if (!options_.test_date.empty()) {
        log_system("Starting mock trading session...");
        log_system("Test date: " + options_.test_date + " (single-day mode)");
        log_system("");
    } else {
        log_system("Starting mock trading session...");
        log_system("");
    }

    auto feed = std::make_shared<data::MockMultiSymbolFeed>(data_manager_, feed_config);

    // Set callback to trigger backend on each bar
    feed->set_bar_callback([this](const std::string& symbol, const Bar& bar) {
        if (g_shutdown_requested) {
            return;
        }
        backend_->on_bar();
    });

    // Create log directory if it doesn't exist
    std::string mkdir_cmd = "mkdir -p " + options_.log_dir;
    int result = system(mkdir_cmd.c_str());
    if (result != 0) {
        log_system("⚠️  Failed to create log directory: " + options_.log_dir);
    }

    // Start trading
    log_system("Starting mock trading session...");
    log_system("");

    if (!backend_->start_trading()) {
        log_system("❌ Failed to start trading!");
        return 1;
    }

    // Connect and start feed
    if (!feed->connect()) {
        log_system("❌ Failed to connect feed!");
        return 1;
    }

    if (!feed->start()) {
        log_system("❌ Failed to start feed!");
        return 1;
    }

    // Wait for replay to complete
    log_system("Waiting for backtest to complete...");
    while (feed->is_active() && !g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop trading
    backend_->stop_trading();
    log_system("");
    log_system("Mock trading session complete");
    log_system("");

    // Print summary
    print_summary(backend_->get_session_stats());

    return 0;
}

int RotationTradeCommand::execute_live_trading() {
    log_system("========================================");
    log_system("Live Trading Mode (Paper)");
    log_system("========================================");
    log_system("");

    // Check credentials
    if (options_.alpaca_api_key.empty() || options_.alpaca_secret_key.empty()) {
        log_system("❌ Missing Alpaca credentials!");
        log_system("   Set ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY");
        return 1;
    }

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create Alpaca client
    broker_ = std::make_shared<AlpacaClient>(
        options_.alpaca_api_key,
        options_.alpaca_secret_key,
        options_.paper_trading
    );

    // Test connection
    log_system("Testing Alpaca connection...");
    auto account = broker_->get_account();
    if (!account || account->cash < 0) {
        log_system("❌ Failed to connect to Alpaca!");
        return 1;
    }
    log_system("✓ Connected to Alpaca");
    log_system("  Cash: $" + std::to_string(account->cash));
    log_system("");

    // Create backend
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, broker_);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // TODO: Create Alpaca WebSocket feed for live bars
    // For now, this is a placeholder - the actual WebSocket integration
    // would go here using AlpacaMultiSymbolFeed

    log_system("⚠️  Live WebSocket feed not yet implemented");
    log_system("   Use mock mode for now");

    return 0;
}

void RotationTradeCommand::setup_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

bool RotationTradeCommand::is_eod() const {
    int minutes = get_minutes_since_open();
    return minutes >= 358;  // 3:58 PM ET
}

int RotationTradeCommand::get_minutes_since_open() const {
    // Get current ET time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Convert to ET (this is simplified - production should use proper timezone handling)
    std::tm* now_tm = std::localtime(&now_time);

    // Calculate minutes since 9:30 AM
    int minutes = (now_tm->tm_hour - 9) * 60 + (now_tm->tm_min - 30);

    return minutes;
}

void RotationTradeCommand::print_summary(const RotationTradingBackend::SessionStats& stats) {
    log_system("========================================");
    log_system("Session Summary");
    log_system("========================================");

    log_system("Bars processed: " + std::to_string(stats.bars_processed));
    log_system("Signals generated: " + std::to_string(stats.signals_generated));
    log_system("Trades executed: " + std::to_string(stats.trades_executed));
    log_system("Positions opened: " + std::to_string(stats.positions_opened));
    log_system("Positions closed: " + std::to_string(stats.positions_closed));
    log_system("Rotations: " + std::to_string(stats.rotations));
    log_system("");

    log_system("Total P&L: $" + std::to_string(stats.total_pnl) +
               " (" + std::to_string(stats.total_pnl_pct * 100.0) + "%)");
    log_system("Final equity: $" + std::to_string(stats.current_equity));
    log_system("Max drawdown: " + std::to_string(stats.max_drawdown * 100.0) + "%");
    log_system("");

    log_system("Win rate: " + std::to_string(stats.win_rate * 100.0) + "%");
    log_system("Avg win: " + std::to_string(stats.avg_win_pct * 100.0) + "%");
    log_system("Avg loss: " + std::to_string(stats.avg_loss_pct * 100.0) + "%");
    log_system("Sharpe ratio: " + std::to_string(stats.sharpe_ratio));
    log_system("MRD: " + std::to_string(stats.mrd * 100.0) + "%");

    log_system("========================================");

    // Highlight MRD performance
    if (stats.mrd >= 0.005) {
        log_system("");
        log_system("🎯 TARGET ACHIEVED! MRD >= 0.5%");
    } else if (stats.mrd >= 0.0) {
        log_system("");
        log_system("✓ Positive MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    } else {
        log_system("");
        log_system("⚠️  Negative MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    }
}

void RotationTradeCommand::log_system(const std::string& msg) {
    std::cout << msg << std::endl;
}

int RotationTradeCommand::execute_batch_mock_trading() {
    log_system("========================================");
    log_system("Batch Mock Trading Mode");
    log_system("========================================");
    log_system("Start Date: " + options_.start_date);
    log_system("End Date: " + options_.end_date);
    log_system("");

    // Set dashboard output directory if not specified
    if (options_.dashboard_output_dir.empty()) {
        options_.dashboard_output_dir = options_.log_dir + "/dashboards";
    }

    // Extract trading days from data
    auto trading_days = extract_trading_days(options_.start_date, options_.end_date);

    if (trading_days.empty()) {
        log_system("❌ No trading days found in date range");
        return 1;
    }

    log_system("Found " + std::to_string(trading_days.size()) + " trading days");
    for (const auto& day : trading_days) {
        log_system("  - " + day);
    }
    log_system("");

    // Results tracking
    std::vector<std::map<std::string, std::string>> daily_results;
    int success_count = 0;

    // Run mock trading for each day
    for (size_t i = 0; i < trading_days.size(); ++i) {
        const auto& date = trading_days[i];

        log_system("");
        log_system("========================================");
        log_system("[" + std::to_string(i+1) + "/" + std::to_string(trading_days.size()) + "] " + date);
        log_system("========================================");
        log_system("");

        // Set test date for this iteration
        options_.test_date = date;

        // Create day-specific output directory
        std::string day_output = options_.log_dir + "/" + date;
        std::filesystem::create_directories(day_output);

        // Temporarily redirect log_dir for this day
        std::string original_log_dir = options_.log_dir;
        options_.log_dir = day_output;

        // Execute single day mock trading
        int result = execute_mock_trading();

        // Restore log_dir
        options_.log_dir = original_log_dir;

        if (result == 0) {
            success_count++;

            // Generate dashboard if requested
            if (options_.generate_dashboards) {
                generate_daily_dashboard(date, day_output);
            }

            // Store results for summary
            std::map<std::string, std::string> day_result;
            day_result["date"] = date;
            day_result["output_dir"] = day_output;
            daily_results.push_back(day_result);
        }
    }

    log_system("");
    log_system("========================================");
    log_system("BATCH TEST COMPLETE");
    log_system("========================================");
    log_system("Successful days: " + std::to_string(success_count) + "/" + std::to_string(trading_days.size()));
    log_system("");

    // Generate summary dashboard
    if (!daily_results.empty() && options_.generate_dashboards) {
        generate_summary_dashboard(daily_results, options_.dashboard_output_dir);
    }

    return (success_count > 0) ? 0 : 1;
}

std::vector<std::string> RotationTradeCommand::extract_trading_days(
    const std::string& start_date,
    const std::string& end_date
) {
    std::vector<std::string> trading_days;
    std::set<std::string> unique_dates;

    // Read SPY data file to extract trading days
    std::string spy_file = options_.data_dir + "/SPY_RTH_NH.csv";
    std::ifstream file(spy_file);

    if (!file.is_open()) {
        log_system("❌ Could not open " + spy_file);
        return trading_days;
    }

    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Extract date from timestamp (format: YYYY-MM-DDTHH:MM:SS)
        size_t t_pos = line.find('T');
        if (t_pos != std::string::npos) {
            std::string date_str = line.substr(0, t_pos);

            // Check if within range
            if (date_str >= start_date && date_str <= end_date) {
                unique_dates.insert(date_str);
            }
        }
    }

    file.close();

    // Convert set to vector
    trading_days.assign(unique_dates.begin(), unique_dates.end());
    std::sort(trading_days.begin(), trading_days.end());

    return trading_days;
}

int RotationTradeCommand::generate_daily_dashboard(
    const std::string& date,
    const std::string& output_dir
) {
    log_system("📈 Generating dashboard for " + date + "...");

    // Build command
    std::string cmd = "python3 scripts/rotation_trading_dashboard.py";
    cmd += " --trades " + output_dir + "/trades.jsonl";
    cmd += " --output " + output_dir + "/dashboard.html";

    // Add optional files if they exist
    std::string signals_file = output_dir + "/signals.jsonl";
    std::string positions_file = output_dir + "/positions.jsonl";
    std::string decisions_file = output_dir + "/decisions.jsonl";

    if (std::filesystem::exists(signals_file)) {
        cmd += " --signals " + signals_file;
    }
    if (std::filesystem::exists(positions_file)) {
        cmd += " --positions " + positions_file;
    }
    if (std::filesystem::exists(decisions_file)) {
        cmd += " --decisions " + decisions_file;
    }

    // Execute
    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✓ Dashboard generated: " + output_dir + "/dashboard.html");
    } else {
        log_system("❌ Dashboard generation failed");
    }

    return result;
}

int RotationTradeCommand::generate_summary_dashboard(
    const std::vector<std::map<std::string, std::string>>& daily_results,
    const std::string& output_dir
) {
    log_system("");
    log_system("========================================");
    log_system("Generating Summary Dashboard");
    log_system("========================================");

    std::filesystem::create_directories(output_dir);

    // Create summary markdown
    std::string summary_file = output_dir + "/SUMMARY.md";
    std::ofstream out(summary_file);

    out << "# Rotation Trading Batch Test Summary\n\n";
    out << "## Test Period\n";
    out << "- **Start Date**: " << options_.start_date << "\n";
    out << "- **End Date**: " << options_.end_date << "\n";
    out << "- **Trading Days**: " << daily_results.size() << "\n\n";

    out << "## Daily Results\n\n";
    out << "| Date | Dashboard | Trades | Signals | Decisions |\n";
    out << "|------|-----------|--------|---------|----------|\n";

    for (const auto& result : daily_results) {
        std::string date = result.at("date");
        std::string dir = result.at("output_dir");

        out << "| " << date << " ";
        out << "| [View](" << dir << "/dashboard.html) ";
        out << "| [trades.jsonl](" << dir << "/trades.jsonl) ";
        out << "| [signals.jsonl](" << dir << "/signals.jsonl) ";
        out << "| [decisions.jsonl](" << dir << "/decisions.jsonl) ";
        out << "|\n";
    }

    out << "\n---\n\n";
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << "Generated on: " << std::ctime(&time);

    out.close();

    log_system("✅ Summary saved: " + summary_file);

    // Generate aggregate HTML dashboard
    log_system("");
    log_system("📊 Generating aggregate HTML dashboard...");

    std::string aggregate_html = output_dir + "/aggregate_summary.html";
    std::string cmd = "python3 scripts/rotation_trading_aggregate_dashboard.py";
    cmd += " --batch-dir " + options_.log_dir;
    cmd += " --output " + aggregate_html;
    cmd += " --start-date " + options_.start_date;
    cmd += " --end-date " + options_.end_date;

    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✅ Aggregate dashboard generated: " + aggregate_html);
    } else {
        log_system("❌ Aggregate dashboard generation failed");
    }

    return result;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 23 of 32**: src/common/config_loader.cpp

**File Information**:
- **Path**: `src/common/config_loader.cpp`
- **Size**: 117 lines
- **Modified**: 2025-10-08 03:33:05
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "common/config_loader.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>

namespace sentio {
namespace config {

std::optional<OnlineEnsembleStrategy::OnlineEnsembleConfig>
load_best_params(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        utils::log_warning("Could not open config file: " + config_file);
        return std::nullopt;
    }

    // Parse JSON manually (simple key-value extraction)
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    // Helper to extract double value from JSON
    auto extract_double = [&json_content](const std::string& key) -> std::optional<double> {
        std::string search_key = "\"" + key + "\":";
        size_t pos = json_content.find(search_key);
        if (pos == std::string::npos) return std::nullopt;

        // Move past the key
        pos += search_key.length();

        // Skip whitespace
        while (pos < json_content.length() && std::isspace(json_content[pos])) {
            pos++;
        }

        // Extract number
        size_t end = pos;
        while (end < json_content.length() &&
               (std::isdigit(json_content[end]) || json_content[end] == '.' ||
                json_content[end] == '-' || json_content[end] == 'e' || json_content[end] == 'E')) {
            end++;
        }

        if (end == pos) return std::nullopt;

        try {
            return std::stod(json_content.substr(pos, end - pos));
        } catch (...) {
            return std::nullopt;
        }
    };

    // Extract parameters
    auto buy_threshold = extract_double("buy_threshold");
    auto sell_threshold = extract_double("sell_threshold");
    auto ewrls_lambda = extract_double("ewrls_lambda");
    auto bb_amplification_factor = extract_double("bb_amplification_factor");

    if (!buy_threshold || !sell_threshold || !ewrls_lambda || !bb_amplification_factor) {
        utils::log_error("Failed to parse parameters from " + config_file);
        return std::nullopt;
    }

    // Create config with loaded parameters
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.buy_threshold = *buy_threshold;
    config.sell_threshold = *sell_threshold;
    config.ewrls_lambda = *ewrls_lambda;
    config.bb_amplification_factor = *bb_amplification_factor;

    // Set other defaults
    config.neutral_zone = config.buy_threshold - config.sell_threshold;
    config.warmup_samples = 960;  // 2 days of 1-min bars
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_bb_amplification = true;
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    utils::log_info("Loaded best parameters from " + config_file);
    utils::log_info("  buy_threshold: " + std::to_string(config.buy_threshold));
    utils::log_info("  sell_threshold: " + std::to_string(config.sell_threshold));
    utils::log_info("  ewrls_lambda: " + std::to_string(config.ewrls_lambda));
    utils::log_info("  bb_amplification_factor: " + std::to_string(config.bb_amplification_factor));

    return config;
}

OnlineEnsembleStrategy::OnlineEnsembleConfig get_production_config() {
    // Try to load from config file
    auto loaded_config = load_best_params();
    if (loaded_config) {
        utils::log_info("✅ Using optimized parameters from config/best_params.json");
        return *loaded_config;
    }

    // Fallback to hardcoded defaults
    utils::log_warning("⚠️  Using hardcoded default parameters (config/best_params.json not found)");

    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 960;
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    return config;
}

} // namespace config
} // namespace sentio

```

## 📄 **FILE 24 of 32**: src/common/time_utils.cpp

**File Information**:
- **Path**: `src/common/time_utils.cpp`
- **Size**: 126 lines
- **Modified**: 2025-10-07 21:46:56
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "common/time_utils.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

namespace sentio {

std::tm TradingSession::to_local_time(const std::chrono::system_clock::time_point& tp) const {
    // C++20 thread-safe timezone conversion using zoned_time
    // This replaces the unsafe setenv("TZ") approach

    #if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        // Use C++20 timezone database
        try {
            const auto* tz = std::chrono::locate_zone(timezone_name);
            std::chrono::zoned_time zt{tz, tp};

            // Convert zoned_time to std::tm
            auto local_time = zt.get_local_time();
            auto local_dp = std::chrono::floor<std::chrono::days>(local_time);
            auto ymd = std::chrono::year_month_day{local_dp};
            auto tod = std::chrono::hh_mm_ss{local_time - local_dp};

            std::tm result{};
            result.tm_year = static_cast<int>(ymd.year()) - 1900;
            result.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
            result.tm_mday = static_cast<unsigned>(ymd.day());
            result.tm_hour = tod.hours().count();
            result.tm_min = tod.minutes().count();
            result.tm_sec = tod.seconds().count();

            // Calculate day of week
            auto dp_sys = std::chrono::sys_days{ymd};
            auto weekday = std::chrono::weekday{dp_sys};
            result.tm_wday = weekday.c_encoding();

            // DST info
            auto info = zt.get_info();
            result.tm_isdst = (info.save != std::chrono::minutes{0}) ? 1 : 0;

            return result;

        } catch (const std::exception& e) {
            // Fallback: if timezone not found, use UTC
            auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm result;
            gmtime_r(&tt, &result);
            return result;
        }
    #else
        // Fallback for C++17: use old setenv approach (NOT thread-safe)
        // This should not happen since we require C++20
        #warning "C++20 chrono timezone database not available - using unsafe setenv fallback"

        auto tt = std::chrono::system_clock::to_time_t(tp);

        const char* old_tz = getenv("TZ");
        setenv("TZ", timezone_name.c_str(), 1);
        tzset();

        std::tm local_tm;
        localtime_r(&tt, &local_tm);

        if (old_tz) {
            setenv("TZ", old_tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        return local_tm;
    #endif
}

bool TradingSession::is_regular_hours(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    int hour = local_tm.tm_hour;
    int minute = local_tm.tm_min;

    // Calculate minutes since midnight
    int open_mins = market_open_hour * 60 + market_open_minute;
    int close_mins = market_close_hour * 60 + market_close_minute;
    int current_mins = hour * 60 + minute;

    return current_mins >= open_mins && current_mins < close_mins;
}

bool TradingSession::is_weekday(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    // tm_wday: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int wday = local_tm.tm_wday;

    return wday >= 1 && wday <= 5;  // Monday - Friday
}

std::string TradingSession::to_local_string(const std::chrono::system_clock::time_point& tp) const {
    auto local_tm = to_local_time(tp);

    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    ss << " " << timezone_name;

    return ss.str();
}

std::string to_iso_string(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm;
    gmtime_r(&tt, &utc_tm);

    std::stringstream ss;
    ss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S");

    // Add milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms << "Z";

    return ss.str();
}

} // namespace sentio

```

## 📄 **FILE 25 of 32**: src/data/mock_multi_symbol_feed.cpp

**File Information**:
- **Path**: `src/data/mock_multi_symbol_feed.cpp`
- **Size**: 485 lines
- **Modified**: 2025-10-15 11:31:55
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <set>

namespace sentio {
namespace data {

MockMultiSymbolFeed::MockMultiSymbolFeed(
    std::shared_ptr<MultiSymbolDataManager> data_manager,
    const Config& config
)
    : data_manager_(data_manager)
    , config_(config)
    , total_bars_(0) {

    utils::log_info("MockMultiSymbolFeed initialized with " +
                   std::to_string(config_.symbol_files.size()) + " symbols, " +
                   "speed=" + std::to_string(config_.replay_speed) + "x");
}

MockMultiSymbolFeed::~MockMultiSymbolFeed() {
    stop();
    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }
}

// === IBarFeed Interface Implementation ===

bool MockMultiSymbolFeed::connect() {
    if (connected_) {
        utils::log_warning("MockMultiSymbolFeed already connected");
        return true;
    }

    utils::log_info("Loading CSV data for " +
                   std::to_string(config_.symbol_files.size()) + " symbols...");
    std::cout << "[Feed] Loading CSV data for " << config_.symbol_files.size() << " symbols..." << std::endl;

    int total_loaded = 0;

    for (const auto& [symbol, filepath] : config_.symbol_files) {
        std::cout << "[Feed] Loading " << symbol << " from " << filepath << std::endl;
        int loaded = load_csv(symbol, filepath);
        if (loaded == 0) {
            utils::log_error("Failed to load data for " + symbol + " from " + filepath);
            std::cout << "[Feed] ❌ Failed to load " << symbol << " (0 bars loaded)" << std::endl;
            if (error_callback_) {
                error_callback_("Failed to load " + symbol + ": " + filepath);
            }
            return false;
        }

        total_loaded += loaded;
        utils::log_info("  " + symbol + ": " + std::to_string(loaded) + " bars");
        std::cout << "[Feed] ✓ Loaded " << symbol << ": " << loaded << " bars" << std::endl;
    }

    total_bars_ = total_loaded;
    connected_ = true;

    utils::log_info("Total bars loaded: " + std::to_string(total_loaded));

    if (connection_callback_) {
        connection_callback_(true);
    }

    return true;
}

void MockMultiSymbolFeed::disconnect() {
    if (!connected_) {
        return;
    }

    stop();

    symbol_data_.clear();
    bars_replayed_ = 0;
    total_bars_ = 0;
    connected_ = false;

    utils::log_info("MockMultiSymbolFeed disconnected");

    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool MockMultiSymbolFeed::is_connected() const {
    return connected_;
}

bool MockMultiSymbolFeed::start() {
    if (!connected_) {
        utils::log_error("Cannot start - not connected. Call connect() first.");
        return false;
    }

    if (active_) {
        utils::log_warning("MockMultiSymbolFeed already active");
        return true;
    }

    if (symbol_data_.empty()) {
        utils::log_error("No data loaded - call connect() first");
        return false;
    }

    utils::log_info("Starting replay (" +
                   std::to_string(config_.replay_speed) + "x speed)...");

    bars_replayed_ = 0;
    should_stop_ = false;
    active_ = true;

    // Start replay thread
    replay_thread_ = std::thread(&MockMultiSymbolFeed::replay_loop, this);

    return true;
}

void MockMultiSymbolFeed::stop() {
    if (!active_) {
        return;
    }

    utils::log_info("Stopping replay...");
    should_stop_ = true;
    active_ = false;

    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }

    utils::log_info("Replay stopped: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::is_active() const {
    return active_;
}

std::string MockMultiSymbolFeed::get_type() const {
    return "MockMultiSymbolFeed";
}

std::vector<std::string> MockMultiSymbolFeed::get_symbols() const {
    std::vector<std::string> symbols;
    for (const auto& [symbol, _] : config_.symbol_files) {
        symbols.push_back(symbol);
    }
    return symbols;
}

void MockMultiSymbolFeed::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void MockMultiSymbolFeed::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void MockMultiSymbolFeed::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = callback;
}

IBarFeed::FeedStats MockMultiSymbolFeed::get_stats() const {
    FeedStats stats;
    stats.total_bars_received = bars_replayed_.load();
    stats.errors = errors_.load();
    stats.reconnects = 0;  // Mock doesn't reconnect
    stats.avg_latency_ms = 0.0;  // Mock has no latency

    // Per-symbol counts
    int i = 0;
    for (const auto& [symbol, data] : symbol_data_) {
        if (i < 10) {
            stats.bars_per_symbol[i] = static_cast<int>(data.current_index);
            i++;
        }
    }

    return stats;
}

// === Additional Mock-Specific Methods ===

void MockMultiSymbolFeed::replay_loop() {
    utils::log_info("Replay loop started");

    // Reset current_index for all symbols
    for (auto& [symbol, data] : symbol_data_) {
        data.current_index = 0;
    }

    // Replay until all symbols exhausted or stop requested
    while (!should_stop_ && replay_next_bar()) {
        // Sleep for replay speed
        if (config_.replay_speed > 0) {
            sleep_for_replay();
        }
    }

    active_ = false;
    utils::log_info("Replay loop complete: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::replay_next_bar() {
    // Check if any symbol has bars remaining
    bool has_data = false;
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        return false;  // All symbols exhausted
    }

    // If syncing timestamps, find common timestamp
    uint64_t target_timestamp = 0;

    if (config_.sync_timestamps) {
        // Find minimum timestamp across all symbols
        target_timestamp = UINT64_MAX;

        for (const auto& [symbol, data] : symbol_data_) {
            if (data.current_index < data.bars.size()) {
                uint64_t ts = data.bars[data.current_index].timestamp_ms;
                target_timestamp = std::min(target_timestamp, ts);
            }
        }
    }

    // Update all symbols at this timestamp
    std::string last_symbol;
    Bar last_bar;
    bool any_updated = false;

    // FIX 3: Track which symbols are updated for validation
    std::set<std::string> updated_symbols;
    int expected_symbols = symbol_data_.size();

    for (auto& [symbol, data] : symbol_data_) {
        if (data.current_index >= data.bars.size()) {
            continue;  // Symbol exhausted
        }

        const auto& bar = data.bars[data.current_index];

        // If syncing, only update if timestamp matches
        if (config_.sync_timestamps && bar.timestamp_ms != target_timestamp) {
            continue;
        }

        // Feed bar to data manager (direct update)
        if (data_manager_) {
            data_manager_->update_symbol(symbol, bar);
            updated_symbols.insert(symbol);
        }

        data.current_index++;
        bars_replayed_++;

        last_symbol = symbol;
        last_bar = bar;
        any_updated = true;
    }

    // FIX 3: Validate all expected symbols were updated
    static int validation_counter = 0;
    if (validation_counter++ % 100 == 0) {  // Check every 100 bars
        if (updated_symbols.size() != static_cast<size_t>(expected_symbols)) {
            utils::log_warning("[FEED VALIDATION] Only updated " +
                             std::to_string(updated_symbols.size()) +
                             " of " + std::to_string(expected_symbols) + " symbols at bar " +
                             std::to_string(bars_replayed_.load()));

            // Log which symbols are missing
            for (const auto& [symbol, data] : symbol_data_) {
                if (updated_symbols.count(symbol) == 0) {
                    utils::log_warning("  Missing update for: " + symbol +
                                     " (index: " + std::to_string(data.current_index) +
                                     "/" + std::to_string(data.bars.size()) + ")");
                }
            }
        }
    }

    // Call callback ONCE after all symbols updated (not for each symbol!)
    if (bar_callback_ && any_updated) {
        bar_callback_(last_symbol, last_bar);
    }

    return true;
}

std::map<std::string, int> MockMultiSymbolFeed::get_bar_counts() const {
    std::map<std::string, int> counts;
    for (const auto& [symbol, data] : symbol_data_) {
        counts[symbol] = static_cast<int>(data.bars.size());
    }
    return counts;
}

MockMultiSymbolFeed::Progress MockMultiSymbolFeed::get_progress() const {
    Progress prog;
    prog.bars_replayed = bars_replayed_;
    prog.total_bars = total_bars_;
    prog.progress_pct = (total_bars_ > 0) ?
        (static_cast<double>(bars_replayed_) / total_bars_ * 100.0) : 0.0;

    // Find current symbol/timestamp
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            prog.current_symbol = symbol;
            prog.current_timestamp = data.bars[data.current_index].timestamp_ms;
            break;
        }
    }

    return prog;
}

// === Private methods ===

int MockMultiSymbolFeed::load_csv(const std::string& symbol, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        utils::log_error("Cannot open file: " + filepath);
        return 0;
    }

    SymbolData data;
    std::string line;
    int line_num = 0;
    int loaded = 0;
    int parse_failures = 0;

    // Skip header
    if (std::getline(file, line)) {
        line_num++;
        utils::log_info("  Header: " + line.substr(0, std::min(100, (int)line.size())));
    }

    // Read data
    while (std::getline(file, line)) {
        line_num++;

        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty/comment lines
        }

        Bar bar;
        if (parse_csv_line(line, bar)) {
            data.bars.push_back(bar);
            loaded++;

            // Log first bar as sample
            if (loaded == 1) {
                utils::log_info("  First bar: timestamp=" + std::to_string(bar.timestamp_ms) +
                              " close=" + std::to_string(bar.close));
            }
        } else {
            parse_failures++;
            if (parse_failures <= 3) {
                utils::log_warning("Failed to parse line " + std::to_string(line_num) +
                                  " in " + filepath + ": " + line.substr(0, 80));
            }
        }
    }

    file.close();

    if (parse_failures > 3) {
        utils::log_warning("  Total parse failures: " + std::to_string(parse_failures));
    }

    // Apply date filter if specified
    if (!config_.filter_date.empty() && loaded > 0) {
        // Parse filter_date (YYYY-MM-DD)
        int year, month, day;
        if (std::sscanf(config_.filter_date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
            // Filter bars to only include this specific date
            std::deque<Bar> filtered_bars;
            for (const auto& bar : data.bars) {
                std::time_t bar_time = bar.timestamp_ms / 1000;
                std::tm* bar_tm = std::localtime(&bar_time);

                if (bar_tm &&
                    bar_tm->tm_year + 1900 == year &&
                    bar_tm->tm_mon + 1 == month &&
                    bar_tm->tm_mday == day) {
                    filtered_bars.push_back(bar);
                }
            }

            data.bars = std::move(filtered_bars);
            int filtered_count = static_cast<int>(data.bars.size());
            utils::log_info("  Date-filtered to " + std::to_string(filtered_count) +
                          " bars for " + symbol + " on " + config_.filter_date);
            loaded = filtered_count;
        } else {
            utils::log_warning("  Invalid filter_date format: " + config_.filter_date);
        }
    }

    if (loaded > 0) {
        symbol_data_[symbol] = std::move(data);
        utils::log_info("  Successfully loaded " + std::to_string(loaded) + " bars for " + symbol);
    } else {
        utils::log_error("  No bars loaded for " + symbol);
    }

    return loaded;
}

bool MockMultiSymbolFeed::parse_csv_line(const std::string& line, Bar& bar) {
    std::istringstream ss(line);
    std::string token;

    try {
        // Format: timestamp,open,high,low,close,volume
        // OR: ts_utc,ts_nyt_epoch,open,high,low,close,volume (7 columns)
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        // Support both 6-column and 7-column formats
        if (tokens.size() == 7) {
            // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert seconds to ms
            bar.open = std::stod(tokens[2]);
            bar.high = std::stod(tokens[3]);
            bar.low = std::stod(tokens[4]);
            bar.close = std::stod(tokens[5]);
            bar.volume = std::stoll(tokens[6]);
        } else if (tokens.size() >= 6) {
            // Format: timestamp,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[0]);
            bar.open = std::stod(tokens[1]);
            bar.high = std::stod(tokens[2]);
            bar.low = std::stod(tokens[3]);
            bar.close = std::stod(tokens[4]);
            bar.volume = std::stoll(tokens[5]);
        } else {
            return false;
        }

        // Set bar_id (not in CSV, use timestamp)
        bar.bar_id = bar.timestamp_ms / 60000;  // Minutes since epoch

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

void MockMultiSymbolFeed::sleep_for_replay(int bars) {
    if (config_.replay_speed <= 0.0) {
        return;  // Instant replay
    }

    // 1 minute real-time = 60000 ms
    // At 39x speed: 60000 / 39 = 1538 ms per bar
    double ms_per_bar = 60000.0 / config_.replay_speed;
    int sleep_ms = static_cast<int>(ms_per_bar * bars);

    if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 26 of 32**: src/data/multi_symbol_data_manager.cpp

**File Information**:
- **Path**: `src/data/multi_symbol_data_manager.cpp`
- **Size**: 375 lines
- **Modified**: 2025-10-14 23:00:21
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "data/multi_symbol_data_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

namespace sentio {
namespace data {

MultiSymbolDataManager::MultiSymbolDataManager(const Config& config)
    : config_(config)
    , time_provider_(nullptr) {

    // Initialize state for each symbol
    for (const auto& symbol : config_.symbols) {
        symbol_states_[symbol] = SymbolState();
    }

    utils::log_info("MultiSymbolDataManager initialized with " +
                   std::to_string(config_.symbols.size()) + " symbols: " +
                   join_symbols());
}

MultiSymbolSnapshot MultiSymbolDataManager::get_latest_snapshot() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    MultiSymbolSnapshot snapshot;

    // In backtest mode, use the latest bar timestamp instead of wall-clock time
    // This prevents historical data from being marked as "stale"
    if (config_.backtest_mode) {
        // Find the latest timestamp across all symbols
        uint64_t max_ts = 0;
        for (const auto& [symbol, state] : symbol_states_) {
            if (state.update_count > 0 && state.last_update_ms > max_ts) {
                max_ts = state.last_update_ms;
            }
        }
        snapshot.logical_timestamp_ms = (max_ts > 0) ? max_ts : get_current_time_ms();
    } else {
        snapshot.logical_timestamp_ms = get_current_time_ms();
    }

    double total_staleness = 0.0;
    int stale_count = 0;

    // Build snapshot for each symbol
    for (const auto& symbol : config_.symbols) {
        auto it = symbol_states_.find(symbol);
        if (it == symbol_states_.end()) {
            continue;  // Symbol not tracked
        }

        const auto& state = it->second;

        if (state.update_count == 0) {
            // Never received data - skip this symbol
            snapshot.missing_symbols.push_back(symbol);
            continue;
        }

        SymbolSnapshot sym_snap;
        sym_snap.latest_bar = state.latest_bar;
        sym_snap.last_update_ms = state.last_update_ms;
        sym_snap.forward_fill_count = state.forward_fill_count;

        // Calculate staleness
        sym_snap.update_staleness(snapshot.logical_timestamp_ms);

        // Check if we need to forward-fill
        if (sym_snap.staleness_seconds > 60.0 &&
            state.forward_fill_count < config_.max_forward_fills) {

            // Forward-fill (use last known bar, update timestamp)
            sym_snap = forward_fill_symbol(symbol, snapshot.logical_timestamp_ms);
            snapshot.total_forward_fills++;
            total_forward_fills_++;

            if (config_.log_data_quality) {
                utils::log_warning("Forward-filling " + symbol +
                                 " (stale: " + std::to_string(sym_snap.staleness_seconds) +
                                 "s, fill #" + std::to_string(sym_snap.forward_fill_count) + ")");
            }
        }

        snapshot.snapshots[symbol] = sym_snap;

        total_staleness += sym_snap.staleness_seconds;
        stale_count++;

        // Track missing if too stale
        if (!sym_snap.is_valid) {
            snapshot.missing_symbols.push_back(symbol);
        }
    }

    // Calculate aggregate stats
    snapshot.avg_staleness_seconds = (stale_count > 0) ?
        (total_staleness / stale_count) : 0.0;

    snapshot.is_complete = snapshot.missing_symbols.empty();

    // Log quality issues
    if (config_.log_data_quality && !snapshot.is_complete) {
        utils::log_warning("Snapshot incomplete: " +
                          std::to_string(snapshot.missing_symbols.size()) +
                          "/" + std::to_string(config_.symbols.size()) +
                          " missing: " + join_vector(snapshot.missing_symbols));
    }

    return snapshot;
}

bool MultiSymbolDataManager::update_symbol(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    static int update_count = 0;
    if (update_count < 3) {
        std::cout << "[DataMgr] update_symbol called for " << symbol << " (bar timestamp: " << bar.timestamp_ms << ")" << std::endl;
        update_count++;
    }

    // Check if symbol is tracked
    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        utils::log_warning("Ignoring update for untracked symbol: " + symbol);
        std::cout << "[DataMgr] ❌ Ignoring update for untracked symbol: " << symbol << std::endl;
        return false;
    }

    // Validate bar
    if (!validate_bar(symbol, bar)) {
        it->second.rejection_count++;
        total_rejections_++;
        return false;
    }

    auto& state = it->second;

    // Add to history
    state.history.push_back(bar);
    if (state.history.size() > static_cast<size_t>(config_.history_size)) {
        state.history.pop_front();
    }

    // Update latest
    state.latest_bar = bar;
    state.last_update_ms = bar.timestamp_ms;
    state.update_count++;
    state.forward_fill_count = 0;  // Reset forward fill counter

    total_updates_++;

    return true;
}

int MultiSymbolDataManager::update_all(const std::map<std::string, Bar>& bars) {
    int success_count = 0;
    for (const auto& [symbol, bar] : bars) {
        if (update_symbol(symbol, bar)) {
            success_count++;
        }
    }
    return success_count;
}

std::vector<Bar> MultiSymbolDataManager::get_recent_bars(
    const std::string& symbol,
    int count
) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.history.empty()) {
        return {};
    }

    const auto& history = it->second.history;
    int available = static_cast<int>(history.size());
    int to_return = std::min(count, available);

    std::vector<Bar> result;
    result.reserve(to_return);

    // Return newest bars first
    auto start_it = history.end() - to_return;
    for (auto it = start_it; it != history.end(); ++it) {
        result.push_back(*it);
    }

    std::reverse(result.begin(), result.end());  // Newest first

    return result;
}

std::deque<Bar> MultiSymbolDataManager::get_all_bars(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        return {};
    }

    return it->second.history;
}

MultiSymbolDataManager::DataQualityStats
MultiSymbolDataManager::get_quality_stats() const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    DataQualityStats stats;
    stats.total_updates = total_updates_.load();
    stats.total_forward_fills = total_forward_fills_.load();
    stats.total_rejections = total_rejections_.load();

    double total_avg_staleness = 0.0;
    int count = 0;

    for (const auto& [symbol, state] : symbol_states_) {
        stats.update_counts[symbol] = state.update_count;
        stats.forward_fill_counts[symbol] = state.forward_fill_count;

        if (state.update_count > 0) {
            double avg = state.cumulative_staleness / state.update_count;
            stats.avg_staleness[symbol] = avg;
            total_avg_staleness += avg;
            count++;
        }
    }

    stats.overall_avg_staleness = (count > 0) ?
        (total_avg_staleness / count) : 0.0;

    return stats;
}

void MultiSymbolDataManager::reset_stats() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    total_updates_ = 0;
    total_forward_fills_ = 0;
    total_rejections_ = 0;

    for (auto& [symbol, state] : symbol_states_) {
        state.update_count = 0;
        state.forward_fill_count = 0;
        state.rejection_count = 0;
        state.cumulative_staleness = 0.0;
    }
}

bool MultiSymbolDataManager::validate_bar(const std::string& symbol, const Bar& bar) {
    // Check 1: Timestamp is reasonable (not in future, not too old)
    // SKIP timestamp validation in backtest mode (historical data is expected to be old)
    if (!config_.backtest_mode) {
        uint64_t now = get_current_time_ms();
        if (bar.timestamp_ms > now + 60000) {  // Future by > 1 minute
            utils::log_error("Rejected " + symbol + " bar: timestamp in future (" +
                            std::to_string(bar.timestamp_ms) + " vs " +
                            std::to_string(now) + ")");
            return false;
        }

        if (bar.timestamp_ms < now - 86400000) {  // Older than 24 hours
            utils::log_warning("Rejected " + symbol + " bar: timestamp too old (" +
                              std::to_string((now - bar.timestamp_ms) / 1000) + "s)");
            return false;
        }
    }

    // Check 2: Price sanity (0.01 < price < 10000)
    if (bar.close <= 0.01 || bar.close > 10000.0) {
        utils::log_error("Rejected " + symbol + " bar: invalid price (" +
                        std::to_string(bar.close) + ")");
        return false;
    }

    // Check 3: OHLC consistency
    if (bar.low > bar.close || bar.high < bar.close ||
        bar.low > bar.open || bar.high < bar.open) {
        utils::log_warning("Rejected " + symbol + " bar: OHLC inconsistent (O=" +
                          std::to_string(bar.open) + " H=" +
                          std::to_string(bar.high) + " L=" +
                          std::to_string(bar.low) + " C=" +
                          std::to_string(bar.close) + ")");
        return false;
    }

    // Check 4: Volume non-negative
    if (bar.volume < 0) {
        utils::log_warning("Rejected " + symbol + " bar: negative volume (" +
                          std::to_string(bar.volume) + ")");
        return false;
    }

    // Check 5: Duplicate detection (same timestamp as last bar)
    auto it = symbol_states_.find(symbol);
    if (it != symbol_states_.end() && it->second.update_count > 0) {
        if (bar.timestamp_ms == it->second.last_update_ms) {
            // Duplicate - not necessarily an error, just skip
            return false;
        }

        // Check timestamp ordering (must be after last update)
        if (bar.timestamp_ms < it->second.last_update_ms) {
            utils::log_warning("Rejected " + symbol + " bar: out-of-order timestamp (" +
                              std::to_string(bar.timestamp_ms) + " < " +
                              std::to_string(it->second.last_update_ms) + ")");
            return false;
        }
    }

    return true;
}

SymbolSnapshot MultiSymbolDataManager::forward_fill_symbol(
    const std::string& symbol,
    uint64_t logical_time
) {
    SymbolSnapshot snap;

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.update_count == 0) {
        snap.is_valid = false;
        return snap;
    }

    auto& state = it->second;

    // Use last known bar, update timestamp
    snap.latest_bar = state.latest_bar;
    snap.latest_bar.timestamp_ms = logical_time;  // Forward-filled timestamp

    snap.last_update_ms = state.last_update_ms;  // Original update time
    snap.forward_fill_count = state.forward_fill_count + 1;

    // Update state forward fill counter
    state.forward_fill_count++;

    // Calculate staleness based on original update time
    snap.update_staleness(logical_time);

    // Mark invalid if too many forward fills
    if (snap.forward_fill_count >= config_.max_forward_fills) {
        snap.is_valid = false;
        utils::log_error("Symbol " + symbol + " exceeded max forward fills (" +
                        std::to_string(config_.max_forward_fills) + ")");
    }

    return snap;
}

// === Helper functions ===

std::string MultiSymbolDataManager::join_symbols() const {
    std::ostringstream oss;
    for (size_t i = 0; i < config_.symbols.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << config_.symbols[i];
    }
    return oss.str();
}

std::string MultiSymbolDataManager::join_vector(const std::vector<std::string>& vec) const {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << vec[i];
    }
    return oss.str();
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 27 of 32**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`
- **Size**: 527 lines
- **Modified**: 2025-10-15 03:54:09
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "features/unified_feature_engine.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

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

UnifiedFeatureEngine::UnifiedFeatureEngine(EngineConfig cfg)
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

void UnifiedFeatureEngine::build_schema_() {
    std::vector<std::string> n;

    // ==========================================================================
    // Time features (cyclical encoding for intraday patterns)
    // ==========================================================================
    if (cfg_.time) {
        n.push_back("time.hour_sin");
        n.push_back("time.hour_cos");
        n.push_back("time.minute_sin");
        n.push_back("time.minute_cos");
        n.push_back("time.dow_sin");
        n.push_back("time.dow_cos");
        n.push_back("time.dom_sin");
        n.push_back("time.dom_cos");
    }

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
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        n.push_back("pattern.doji");           // Body < 10% of range
        n.push_back("pattern.hammer");         // Lower shadow > 2x body
        n.push_back("pattern.shooting_star");  // Upper shadow > 2x body
        n.push_back("pattern.engulfing_bull"); // Bullish engulfing
        n.push_back("pattern.engulfing_bear"); // Bearish engulfing
    }

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

bool UnifiedFeatureEngine::update(const Bar& b) {
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

    // Store previous close BEFORE updating (for 1-bar return calculation)
    prevPrevClose_ = prevClose_;

    // Store current bar values for derived features
    prevTimestamp_ = b.timestamp_ms;
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

void UnifiedFeatureEngine::recompute_vector_() {
    size_t k = 0;

    // ==========================================================================
    // Time features (cyclical encoding from v1.0)
    // ==========================================================================
    if (cfg_.time && prevTimestamp_ > 0) {
        time_t timestamp = prevTimestamp_ / 1000;
        struct tm* time_info = gmtime(&timestamp);

        if (time_info) {
            double hour = time_info->tm_hour;
            double minute = time_info->tm_min;
            double day_of_week = time_info->tm_wday;     // 0-6 (Sunday=0)
            double day_of_month = time_info->tm_mday;    // 1-31

            // Cyclical encoding (sine/cosine to preserve continuity)
            feats_[k++] = std::sin(2.0 * M_PI * hour / 24.0);           // hour_sin
            feats_[k++] = std::cos(2.0 * M_PI * hour / 24.0);           // hour_cos
            feats_[k++] = std::sin(2.0 * M_PI * minute / 60.0);         // minute_sin
            feats_[k++] = std::cos(2.0 * M_PI * minute / 60.0);         // minute_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_week / 7.0);     // dow_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_week / 7.0);     // dow_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_month / 31.0);   // dom_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_month / 31.0);   // dom_cos
        } else {
            // If time parsing fails, fill with NaN
            for (int i = 0; i < 8; ++i) {
                feats_[k++] = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }

    // ==========================================================================
    // Core price/volume
    // ==========================================================================
    feats_[k++] = prevClose_;
    feats_[k++] = prevOpen_;
    feats_[k++] = prevHigh_;
    feats_[k++] = prevLow_;
    feats_[k++] = safe_return(prevClose_, prevPrevClose_);  // 1-bar return
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

        // Debug BB NaN issue - check Welford stats when BB produces NaN
        if (bar_count_ > 100 && std::isnan(bb20_.sd)) {
            static int late_nan_count = 0;
            if (late_nan_count < 10) {
                std::cerr << "[FeatureEngine CRITICAL] BB.sd is NaN!"
                          << " bar_count=" << bar_count_
                          << ", bb20_.win.size=" << bb20_.win.size()
                          << ", bb20_.win.capacity=" << bb20_.win.capacity()
                          << ", bb20_.win.full=" << bb20_.win.full()
                          << ", bb20_.win.welford_n=" << bb20_.win.welford_n()
                          << ", bb20_.win.welford_m2=" << bb20_.win.welford_m2()
                          << ", bb20_.win.variance=" << bb20_.win.variance()
                          << ", bb20_.is_ready=" << bb20_.is_ready()
                          << ", bb20_.mean=" << bb20_.mean
                          << ", bb20_.sd=" << bb20_.sd
                          << ", prevClose=" << prevClose_ << std::endl;
                late_nan_count++;
            }
        }

        size_t bb_start_idx = k;
        feats_[k++] = bb20_.mean;
        feats_[k++] = bb20_.sd;
        feats_[k++] = bb20_.upper;
        feats_[k++] = bb20_.lower;
        feats_[k++] = bb20_.percent_b;
        feats_[k++] = bb20_.bandwidth;

        // Debug: Check if any BB features are NaN after assignment
        if (bar_count_ > 100) {
            static int bb_assign_debug = 0;
            if (bb_assign_debug < 3) {
                std::cerr << "[FeatureEngine] BB features assigned at indices " << bb_start_idx << "-" << (k-1)
                          << ", bb20_.mean=" << bb20_.mean
                          << ", bb20_.sd=" << bb20_.sd
                          << ", feats_[" << bb_start_idx << "]=" << feats_[bb_start_idx]
                          << ", feats_[" << (bb_start_idx+1) << "]=" << feats_[bb_start_idx+1]
                          << std::endl;
                bb_assign_debug++;
            }
        }
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

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        double range = prevHigh_ - prevLow_;
        double body = std::abs(prevClose_ - prevOpen_);
        double upper_shadow = prevHigh_ - std::max(prevOpen_, prevClose_);
        double lower_shadow = std::min(prevOpen_, prevClose_) - prevLow_;

        // Doji: body < 10% of range
        bool is_doji = (range > 0) && (body / range < 0.1);
        feats_[k++] = is_doji ? 1.0 : 0.0;

        // Hammer: lower shadow > 2x body, upper shadow < body
        bool is_hammer = (lower_shadow > 2.0 * body) && (upper_shadow < body);
        feats_[k++] = is_hammer ? 1.0 : 0.0;

        // Shooting star: upper shadow > 2x body, lower shadow < body
        bool is_shooting_star = (upper_shadow > 2.0 * body) && (lower_shadow < body);
        feats_[k++] = is_shooting_star ? 1.0 : 0.0;

        // Engulfing patterns require previous bar - use prevPrevClose_
        bool engulfing_bull = false;
        bool engulfing_bear = false;
        if (!std::isnan(prevPrevClose_)) {
            bool prev_bearish = prevPrevClose_ < prevOpen_;  // Prev bar was bearish
            bool curr_bullish = prevClose_ > prevOpen_;       // Current bar is bullish
            bool engulfs = (prevOpen_ < prevPrevClose_) && (prevClose_ > prevOpen_);
            engulfing_bull = prev_bearish && curr_bullish && engulfs;

            bool prev_bullish = prevPrevClose_ > prevOpen_;
            bool curr_bearish = prevClose_ < prevOpen_;
            engulfs = (prevOpen_ > prevPrevClose_) && (prevClose_ < prevOpen_);
            engulfing_bear = prev_bullish && curr_bearish && engulfs;
        }
        feats_[k++] = engulfing_bull ? 1.0 : 0.0;
        feats_[k++] = engulfing_bear ? 1.0 : 0.0;
    }
}

int UnifiedFeatureEngine::warmup_remaining() const {
    // Conservative: max lookback across all indicators
    int max_period = std::max({
        cfg_.rsi14, cfg_.rsi21, cfg_.atr14, cfg_.bb20,
        cfg_.stoch14, cfg_.will14, cfg_.macd_slow, cfg_.don20,
        cfg_.sma50, cfg_.ema50
    });

    // Need at least max_period + 1 bars for all indicators to be valid
    int required_bars = max_period + 1;
    return std::max(0, required_bars - static_cast<int>(bar_count_));
}

std::vector<std::string> UnifiedFeatureEngine::get_unready_indicators() const {
    std::vector<std::string> unready;

    // Check each indicator's readiness
    if (!bb20_.is_ready()) unready.push_back("BB20");
    if (!rsi14_.is_ready()) unready.push_back("RSI14");
    if (!rsi21_.is_ready()) unready.push_back("RSI21");
    if (!atr14_.is_ready()) unready.push_back("ATR14");
    if (!stoch14_.is_ready()) unready.push_back("Stoch14");
    if (!will14_.is_ready()) unready.push_back("Will14");
    if (!don20_.is_ready()) unready.push_back("Don20");

    // Check moving averages
    if (bar_count_ < static_cast<size_t>(cfg_.sma10)) unready.push_back("SMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.sma20)) unready.push_back("SMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.sma50)) unready.push_back("SMA50");
    if (bar_count_ < static_cast<size_t>(cfg_.ema10)) unready.push_back("EMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.ema20)) unready.push_back("EMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.ema50)) unready.push_back("EMA50");

    return unready;
}

void UnifiedFeatureEngine::reset() {
    *this = UnifiedFeatureEngine(cfg_);
}

std::string UnifiedFeatureEngine::serialize() const {
    std::ostringstream os;
    os << std::setprecision(17);

    os << "prevTimestamp " << prevTimestamp_ << "\n";
    os << "prevClose " << prevClose_ << "\n";
    os << "prevPrevClose " << prevPrevClose_ << "\n";
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

void UnifiedFeatureEngine::restore(const std::string& blob) {
    reset();

    std::istringstream is(blob);
    std::string key;

    while (is >> key) {
        if (key == "prevTimestamp") is >> prevTimestamp_;
        else if (key == "prevClose") is >> prevClose_;
        else if (key == "prevPrevClose") is >> prevPrevClose_;
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

## 📄 **FILE 28 of 32**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`
- **Size**: 466 lines
- **Modified**: 2025-10-15 09:28:14
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>
#include <iostream>
#include <cmath>

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
        // During warmup, provide minimal but non-zero confidence
        result.predicted_return = 0.0;
        result.confidence = 0.01;  // 1% baseline during warmup
        result.volatility_estimate = 0.001;
        return result;
    }

    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());

    // Check for NaN in features
    if (!x.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: NaN detected in features! Returning neutral prediction." << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }

    // Check for NaN in model parameters
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: NaN detected in model parameters (theta or P)! Returning neutral prediction." << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }

    // Linear prediction
    result.predicted_return = theta_.dot(x);

    // FIX #3: Use Empirical Accuracy Instead of Variance
    // The variance-based confidence was giving unrealistically low values (1-1.5%)
    // Now using actual recent prediction errors (RMSE) which reflects real performance

    if (recent_errors_.size() < 20) {
        // During early learning, use conservative confidence
        result.confidence = 0.1 + (static_cast<double>(recent_errors_.size()) / 20.0) * 0.2;
    } else {
        // Calculate confidence from recent RMSE
        double rmse = get_recent_rmse();

        // Convert RMSE to confidence:
        // - RMSE = 0.001 (0.1% error) → confidence ≈ 0.90
        // - RMSE = 0.005 (0.5% error) → confidence ≈ 0.67
        // - RMSE = 0.010 (1.0% error) → confidence ≈ 0.50
        // - RMSE = 0.020 (2.0% error) → confidence ≈ 0.33

        // Use inverse relationship: conf = 1 / (1 + k*RMSE)
        // where k=100 gives good sensitivity for typical RMSE values
        result.confidence = 1.0 / (1.0 + 100.0 * rmse);

        // Also factor in directional accuracy (percentage of correct direction predictions)
        double directional_accuracy = get_directional_accuracy();

        // Blend RMSE-based and direction-based confidence (70% RMSE, 30% direction)
        result.confidence = 0.7 * result.confidence + 0.3 * directional_accuracy;

        // Clamp to reasonable range
        result.confidence = std::clamp(result.confidence, 0.10, 0.95);
    }

    // Apply gradual ramp-up over first 200 samples
    if (samples_seen_ < 200) {
        double ramp_factor = static_cast<double>(samples_seen_) / 200.0;
        result.confidence *= (0.3 + 0.7 * ramp_factor);  // Start at 30%, ramp to 100%
    }

    // Sanity check results
    if (!std::isfinite(result.predicted_return) || !std::isfinite(result.confidence)) {
        std::cerr << "[OnlinePredictor] WARNING: NaN in prediction results! pred_return="
                  << result.predicted_return << ", confidence=" << result.confidence << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
    }

    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();

    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;

    // Check for NaN in input
    if (!std::isfinite(actual_return)) {
        std::cerr << "[OnlinePredictor] WARNING: actual_return is NaN/inf! Skipping update. actual_return=" << actual_return << std::endl;
        return;
    }

    // Use Eigen::Map to avoid copy (zero-copy view of std::vector)
    Eigen::Map<const Eigen::VectorXd> x(features.data(), features.size());

    // Check for NaN in features
    if (!x.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: Features contain NaN/inf! Skipping update." << std::endl;

        // Debug: Show which features are NaN (first occurrence only)
        static bool logged_nan_indices = false;
        if (!logged_nan_indices) {
            std::cerr << "[OnlinePredictor] NaN feature indices: ";
            for (int i = 0; i < x.size(); i++) {
                if (!std::isfinite(x(i))) {
                    std::cerr << i << "(" << x(i) << ") ";
                }
            }
            std::cerr << std::endl;
            std::cerr << "[OnlinePredictor] Total features: " << x.size() << std::endl;
            logged_nan_indices = true;
        }

        return;
    }

    // Check model parameters before update
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] CRITICAL: Model parameters corrupted (theta or P has NaN)! Reinitializing..." << std::endl;
        // Reinitialize model
        theta_ = Eigen::VectorXd::Zero(num_features_);
        P_ = Eigen::MatrixXd::Identity(num_features_, num_features_) / config_.regularization;
        return;
    }

    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }

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
    // NOTE: This is expensive O(n³) but REQUIRED every update for prediction accuracy
    // Skipping this causes numerical instability and prediction divergence
    ensure_positive_definite();

    // FIX #2: Periodic Reconditioning with Explicit Regularization
    // Every 100 samples, add regularization to prevent ill-conditioning
    // This is critical for maintaining confidence over thousands of updates
    if (samples_seen_ % 100 == 0) {
        // Add scaled identity matrix to prevent matrix singularity
        // P = P + regularization * I
        P_ += Eigen::MatrixXd::Identity(num_features_, num_features_) * config_.regularization;
        utils::log_debug("Periodic reconditioning: added regularization term at sample " +
                        std::to_string(samples_seen_));
    }

    // Check for NaN after update
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] CRITICAL: NaN introduced during update! Reinitializing model." << std::endl;
        std::cerr << "  error=" << error << ", denominator=" << denominator << ", lambda=" << current_lambda_ << std::endl;
        // Reinitialize model
        theta_ = Eigen::VectorXd::Zero(num_features_);
        P_ = Eigen::MatrixXd::Identity(num_features_, num_features_) / config_.regularization;
        return;
    }

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
    // Eigenvalue decomposition - CANNOT be optimized without accuracy degradation
    // Attempted optimizations (all failed accuracy tests):
    // 1. Periodic updates (every N samples) - causes divergence
    // 2. Cholesky fast path - causes divergence
    // 3. Matrix symmetrization - causes long-term drift
    // Conclusion: EWRLS is highly sensitive to numerical perturbations
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

## 📄 **FILE 29 of 32**: src/live/alpaca_client.cpp

**File Information**:
- **Path**: `src/live/alpaca_client.cpp`
- **Size**: 477 lines
- **Modified**: 2025-10-09 10:39:50
- **Type**: cpp
- **Permissions**: -rw-r--r--

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

std::vector<AlpacaClient::Order> AlpacaClient::get_open_orders() {
    try {
        std::string response = http_get("/orders?status=open");
        json orders_json = json::parse(response);

        std::vector<Order> orders;
        for (const auto& order_json : orders_json) {
            Order order;
            order.order_id = order_json.value("id", "");
            order.symbol = order_json.value("symbol", "");
            order.quantity = order_json.value("qty", 0.0);
            order.side = order_json.value("side", "");
            order.type = order_json.value("type", "");
            order.time_in_force = order_json.value("time_in_force", "");
            order.status = order_json.value("status", "");
            order.filled_qty = order_json.value("filled_qty", 0.0);
            order.filled_avg_price = order_json.value("filled_avg_price", 0.0);

            if (order_json.contains("limit_price") && !order_json["limit_price"].is_null()) {
                order.limit_price = order_json["limit_price"].get<double>();
            }

            orders.push_back(order);
        }

        return orders;
    } catch (const std::exception& e) {
        std::cerr << "Error getting open orders: " << e.what() << std::endl;
        return {};
    }
}

bool AlpacaClient::cancel_all_orders() {
    try {
        http_delete("/orders");
        std::cout << "[AlpacaClient] All orders cancelled" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error canceling all orders: " << e.what() << std::endl;
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

std::vector<AlpacaClient::BarData> AlpacaClient::get_latest_bars(const std::vector<std::string>& symbols) {
    std::vector<BarData> bars;

    if (symbols.empty()) {
        return bars;
    }

    // Build query string: ?symbols=SPY,SPXL,SH,SDS&feed=iex
    std::string symbols_str;
    for (size_t i = 0; i < symbols.size(); ++i) {
        symbols_str += symbols[i];
        if (i < symbols.size() - 1) {
            symbols_str += ",";
        }
    }

    std::string endpoint = "/stocks/bars/latest?symbols=" + symbols_str + "&feed=iex";

    try {
        std::string response = http_get(endpoint);
        json j = json::parse(response);

        // Response format: {"bars": {"SPY": {...}, "SPXL": {...}}}
        if (j.contains("bars")) {
            for (const auto& symbol : symbols) {
                if (j["bars"].contains(symbol)) {
                    const auto& bar_json = j["bars"][symbol];
                    BarData bar;
                    bar.symbol = symbol;

                    // Parse timestamp (ISO 8601 format)
                    std::string timestamp_str = bar_json["t"].get<std::string>();
                    // Convert RFC3339 to Unix timestamp (simplified - assumes format like "2025-01-09T14:30:00Z")
                    std::tm tm = {};
                    std::istringstream ss(timestamp_str);
                    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                    bar.timestamp_ms = static_cast<uint64_t>(std::mktime(&tm)) * 1000;

                    bar.open = bar_json["o"].get<double>();
                    bar.high = bar_json["h"].get<double>();
                    bar.low = bar_json["l"].get<double>();
                    bar.close = bar_json["c"].get<double>();
                    bar.volume = bar_json["v"].get<uint64_t>();

                    bars.push_back(bar);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error fetching latest bars: " << e.what() << std::endl;
    }

    return bars;
}

std::vector<AlpacaClient::BarData> AlpacaClient::get_bars(const std::string& symbol,
                                                           const std::string& timeframe,
                                                           const std::string& start,
                                                           const std::string& end,
                                                           int limit) {
    std::vector<BarData> bars;

    // Build query string
    std::string endpoint = "/stocks/" + symbol + "/bars?timeframe=" + timeframe + "&feed=iex";
    if (!start.empty()) {
        endpoint += "&start=" + start;
    }
    if (!end.empty()) {
        endpoint += "&end=" + end;
    }
    if (limit > 0) {
        endpoint += "&limit=" + std::to_string(limit);
    }

    try {
        std::string response = http_get(endpoint);
        json j = json::parse(response);

        // Response format: {"bars": [{"t": "...", "o": ..., "h": ..., "l": ..., "c": ..., "v": ...}, ...]}
        if (j.contains("bars") && j["bars"].is_array()) {
            for (const auto& bar_json : j["bars"]) {
                BarData bar;
                bar.symbol = symbol;

                // Parse timestamp
                std::string timestamp_str = bar_json["t"].get<std::string>();
                std::tm tm = {};
                std::istringstream ss(timestamp_str);
                ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                bar.timestamp_ms = static_cast<uint64_t>(std::mktime(&tm)) * 1000;

                bar.open = bar_json["o"].get<double>();
                bar.high = bar_json["h"].get<double>();
                bar.low = bar_json["l"].get<double>();
                bar.close = bar_json["c"].get<double>();
                bar.volume = bar_json["v"].get<uint64_t>();

                bars.push_back(bar);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error fetching bars: " << e.what() << std::endl;
    }

    return bars;
}

} // namespace sentio

```

## 📄 **FILE 30 of 32**: src/live/position_book.cpp

**File Information**:
- **Path**: `src/live/position_book.cpp`
- **Size**: 235 lines
- **Modified**: 2025-10-08 22:51:20
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "live/position_book.h"
#include "common/exceptions.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace sentio {

void PositionBook::on_execution(const ExecutionReport& exec) {
    execution_history_.push_back(exec);

    if (exec.filled_qty == 0) {
        return;  // No fill, nothing to update
    }

    auto& pos = positions_[exec.symbol];

    // Calculate realized P&L if reducing position
    double realized_pnl = calculate_realized_pnl(pos, exec);
    total_realized_pnl_ += realized_pnl;

    // Update position
    update_position_on_fill(exec);

    // Log update
    std::cout << "[PositionBook] " << exec.symbol
              << " qty=" << pos.qty
              << " avg_px=" << pos.avg_entry_price
              << " realized_pnl=" << realized_pnl << std::endl;
}

void PositionBook::update_position_on_fill(const ExecutionReport& exec) {
    auto& pos = positions_[exec.symbol];

    // Convert side to signed qty
    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    int64_t new_qty = pos.qty + fill_qty;

    if (pos.qty == 0) {
        // Opening new position
        pos.avg_entry_price = exec.avg_fill_price;
    } else if ((pos.qty > 0 && fill_qty > 0) || (pos.qty < 0 && fill_qty < 0)) {
        // Adding to position - update weighted average entry price
        double total_cost = pos.qty * pos.avg_entry_price +
                           fill_qty * exec.avg_fill_price;
        pos.avg_entry_price = total_cost / new_qty;
    }
    // If reducing/reversing, keep old avg_entry_price for P&L calculation

    pos.qty = new_qty;
    pos.symbol = exec.symbol;

    // Reset avg price when flat
    if (pos.qty == 0) {
        pos.avg_entry_price = 0.0;
        pos.unrealized_pnl = 0.0;
    }
}

double PositionBook::calculate_realized_pnl(const BrokerPosition& old_pos,
                                            const ExecutionReport& exec) {
    if (old_pos.qty == 0) {
        return 0.0;  // Opening position, no P&L
    }

    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    // Only calculate P&L if reducing position
    if ((old_pos.qty > 0 && fill_qty >= 0) || (old_pos.qty < 0 && fill_qty <= 0)) {
        return 0.0;  // Adding to position
    }

    // Calculate how many shares we're closing
    int64_t closed_qty = std::min(std::abs(fill_qty), std::abs(old_pos.qty));

    // P&L per share = exit price - entry price
    double pnl_per_share = exec.avg_fill_price - old_pos.avg_entry_price;

    // For short positions, invert the P&L
    if (old_pos.qty < 0) {
        pnl_per_share = -pnl_per_share;
    }

    return closed_qty * pnl_per_share;
}

BrokerPosition PositionBook::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return BrokerPosition{.symbol = symbol};
    }
    return it->second;
}

void PositionBook::update_market_price(const std::string& symbol, double price) {
    auto it = positions_.find(symbol);
    if (it == positions_.end() || it->second.qty == 0) {
        return;  // No position, no unrealized P&L
    }

    auto& pos = it->second;
    pos.current_price = price;

    // Calculate unrealized P&L
    double pnl_per_share = price - pos.avg_entry_price;
    if (pos.qty < 0) {
        pnl_per_share = -pnl_per_share;  // Short position
    }
    pos.unrealized_pnl = std::abs(pos.qty) * pnl_per_share;
}

void PositionBook::reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions) {
    std::cout << "[PositionBook] === Position Reconciliation ===" << std::endl;

    // Build broker position map
    std::map<std::string, BrokerPosition> broker_map;
    for (const auto& bp : broker_positions) {
        broker_map[bp.symbol] = bp;
    }

    // Check for discrepancies
    bool has_drift = false;

    // Check local positions against broker
    for (const auto& [symbol, local_pos] : positions_) {
        if (local_pos.qty == 0) continue;  // Skip flat positions

        auto bit = broker_map.find(symbol);

        if (bit == broker_map.end()) {
            std::cerr << "[PositionBook] DRIFT: Local has " << symbol
                     << " (" << local_pos.qty << "), broker has 0" << std::endl;
            has_drift = true;
        } else {
            const auto& broker_pos = bit->second;
            if (local_pos.qty != broker_pos.qty) {
                std::cerr << "[PositionBook] DRIFT: " << symbol
                         << " local=" << local_pos.qty
                         << " broker=" << broker_pos.qty << std::endl;
                has_drift = true;
            }
        }
    }

    // Check for positions broker has but we don't
    for (const auto& [symbol, broker_pos] : broker_map) {
        if (broker_pos.qty == 0) continue;

        auto lit = positions_.find(symbol);
        if (lit == positions_.end() || lit->second.qty == 0) {
            std::cerr << "[PositionBook] DRIFT: Broker has " << symbol
                     << " (" << broker_pos.qty << "), local has 0" << std::endl;
            has_drift = true;
        }
    }

    if (has_drift) {
        std::cerr << "[PositionBook] === POSITION DRIFT DETECTED ===" << std::endl;
        throw PositionReconciliationError("Position drift detected - local != broker");
    } else {
        std::cout << "[PositionBook] Position reconciliation: OK" << std::endl;
    }
}

double PositionBook::get_realized_pnl_since(uint64_t since_ts) const {
    double pnl = 0.0;
    for (const auto& exec : execution_history_) {
        if (exec.timestamp >= since_ts && exec.status == "filled") {
            // Note: This is simplified. In production, track per-exec P&L
            // For now, return total realized P&L
        }
    }
    return total_realized_pnl_;
}

std::map<std::string, BrokerPosition> PositionBook::get_all_positions() const {
    std::map<std::string, BrokerPosition> result;
    for (const auto& [symbol, pos] : positions_) {
        if (pos.qty != 0) {
            result[symbol] = pos;
        }
    }
    return result;
}

void PositionBook::set_position(const std::string& symbol, int64_t qty, double avg_price) {
    BrokerPosition pos;
    pos.symbol = symbol;
    pos.qty = qty;
    pos.avg_entry_price = avg_price;
    pos.current_price = avg_price;  // Will be updated on next price update
    pos.unrealized_pnl = 0.0;
    positions_[symbol] = pos;
}

std::string PositionBook::positions_hash() const {
    if (is_flat()) {
        return "";  // Empty hash for flat book
    }

    // Build sorted position string
    std::stringstream ss;
    bool first = true;

    // positions_ is already sorted (std::map)
    for (const auto& [symbol, pos] : positions_) {
        if (pos.qty == 0) continue;  // Skip flat positions

        if (!first) ss << "|";
        ss << symbol << ":" << pos.qty;
        first = false;
    }

    std::string pos_str = ss.str();

    // Compute hash (using std::hash as placeholder for production SHA1)
    std::hash<std::string> hasher;
    size_t hash_val = hasher(pos_str);

    // Convert to hex string
    std::stringstream hex_ss;
    hex_ss << std::hex << std::setfill('0') << std::setw(16) << hash_val;

    return hex_ss.str();
}

} // namespace sentio

```

## 📄 **FILE 31 of 32**: src/strategy/market_regime_detector.cpp

**File Information**:
- **Path**: `src/strategy/market_regime_detector.cpp`
- **Size**: 171 lines
- **Modified**: 2025-10-08 09:53:39
- **Type**: cpp
- **Permissions**: -rw-r--r--

```text
#include "strategy/market_regime_detector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sentio {

static inline double safe_log(double x) { return std::log(std::max(1e-12, x)); }

MarketRegimeDetector::MarketRegimeDetector() : p_(MarketRegimeDetectorParams{}) {}

MarketRegimeDetector::MarketRegimeDetector(const Params& p) : p_(p) {}

double MarketRegimeDetector::std_log_returns(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 0.0;
    const int n0 = (int)v.size() - win;
    std::vector<double> r; r.reserve(win);
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        r.push_back(safe_log(v[i].close / v[i-1].close));
    }
    double mean = std::accumulate(r.begin(), r.end(), 0.0) / r.size();
    double acc = 0.0;
    for (double x : r) { double d = x - mean; acc += d * d; }
    return std::sqrt(acc / std::max<size_t>(1, r.size() - 1));
}

void MarketRegimeDetector::slope_r2_log_price(const std::vector<Bar>& v, int win, double& slope, double& r2) {
    slope = 0.0; r2 = 0.0;
    if ((int)v.size() < win) return;
    const int n0 = (int)v.size() - win;
    std::vector<double> y; y.reserve(win);
    for (int i = n0; i < (int)v.size(); ++i) y.push_back(safe_log(v[i].close));

    // x = 0..win-1
    const double n = (double)win;
    const double sx = (n - 1.0) * n / 2.0;
    const double sxx = (n - 1.0) * n * (2.0 * n - 1.0) / 6.0;
    double sy = 0.0, sxy = 0.0;
    for (int i = 0; i < win; ++i) { sy += y[i]; sxy += i * y[i]; }

    const double denom = n * sxx - sx * sx;
    if (std::abs(denom) < 1e-12) return;
    slope = (n * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / n;

    // R²
    double ss_tot = 0.0, ss_res = 0.0;
    const double y_bar = sy / n;
    for (int i = 0; i < win; ++i) {
        const double y_hat = intercept + slope * i;
        ss_res += (y[i] - y_hat) * (y[i] - y_hat);
        ss_tot += (y[i] - y_bar) * (y[i] - y_bar);
    }
    r2 = (ss_tot > 0.0) ? (1.0 - ss_res / ss_tot) : 0.0;
}

double MarketRegimeDetector::chop_index(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 50.0;
    const int n0 = (int)v.size() - win;
    double atr_sum = 0.0;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        const auto& c = v[i];
        const auto& p = v[i-1];
        const double tr = std::max({ c.high - c.low,
                                     std::abs(c.high - p.close),
                                     std::abs(c.low  - p.close) });
        atr_sum += tr;
    }
    double hi = v[n0].high, lo = v[n0].low;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        hi = std::max(hi, v[i].high);
        lo = std::min(lo, v[i].low);
    }
    const double range = std::max(1e-12, hi - lo);
    const double x = std::log10(std::max(1e-12, atr_sum / range));
    const double denom = std::log10((double)win);
    return 100.0 * x / std::max(1e-12, denom); // typical CHOP 0..100
}

double MarketRegimeDetector::percentile(std::vector<double>& tmp, double q) {
    if (tmp.empty()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    size_t k = (size_t)std::floor(q * (tmp.size() - 1));
    std::nth_element(tmp.begin(), tmp.begin() + k, tmp.end());
    return tmp[k];
}

void MarketRegimeDetector::update_vol_thresholds(double vol_sample) {
    // Keep rolling window
    vol_cal_.push_back(vol_sample);
    while ((int)vol_cal_.size() > p_.calibr_window) vol_cal_.pop_front();
    if ((int)vol_cal_.size() < std::min(500, p_.calibr_window/2)) return; // not enough

    std::vector<double> tmp(vol_cal_.begin(), vol_cal_.end());
    vol_lo_ = percentile(tmp, 0.30);
    vol_hi_ = percentile(tmp, 0.70);

    // Safety guard: keep some separation
    if (vol_hi_ - vol_lo_ < 5e-5) {
        vol_lo_ = std::max(0.0, vol_lo_ - 1e-4);
        vol_hi_ = vol_hi_ + 1e-4;
    }
}

MarketRegime MarketRegimeDetector::detect(const std::vector<Bar>& bars) {
    // 1) Extract features
    last_feat_.vol = std_log_returns(bars, p_.vol_window);
    slope_r2_log_price(bars, p_.slope_window, last_feat_.slope, last_feat_.r2);
    last_feat_.chop = chop_index(bars, p_.chop_window);

    // 2) Adapt volatility thresholds
    update_vol_thresholds(last_feat_.vol);

    // 3) Compute scores
    auto score_high_vol = (vol_hi_ > 0.0) ? (last_feat_.vol - vol_hi_) / std::max(1e-12, vol_hi_) : -1.0;
    auto score_low_vol  = (vol_lo_ > 0.0) ? (vol_lo_ - last_feat_.vol) / std::max(1e-12, vol_lo_) : -1.0;

    const bool trending = std::abs(last_feat_.slope) >= p_.trend_slope_min && last_feat_.r2 >= p_.trend_r2_min;
    const double trend_mag = (std::abs(last_feat_.slope) / std::max(1e-12, p_.trend_slope_min)) * last_feat_.r2;

    // 4) Precedence: HIGH_VOL -> LOW_VOL -> TREND -> CHOP
    struct Candidate { MarketRegime r; double score; };
    std::vector<Candidate> cands;
    cands.push_back({ MarketRegime::HIGH_VOLATILITY, score_high_vol });
    cands.push_back({ MarketRegime::LOW_VOLATILITY,  score_low_vol  });
    if (trending) {
        cands.push_back({ last_feat_.slope > 0.0 ? MarketRegime::TRENDING_UP : MarketRegime::TRENDING_DOWN, trend_mag });
    } else {
        // Use inverse CHOP as weak score for choppy (higher chop -> more choppy, but we want positive score)
        const double chop_score = std::max(0.0, (last_feat_.chop - 50.0) / 50.0);
        cands.push_back({ MarketRegime::CHOPPY, chop_score });
    }

    // Pick best candidate
    auto best = std::max_element(cands.begin(), cands.end(), [](const auto& a, const auto& b){ return a.score < b.score; });
    MarketRegime proposed = best->r;
    double proposed_score = best->score;

    // 5) Hysteresis + cooldown
    if (cooldown_ > 0) --cooldown_;
    if (last_regime_.has_value()) {
        if (proposed != *last_regime_) {
            if (proposed_score < p_.hysteresis_margin && cooldown_ > 0) {
                return *last_regime_; // hold
            }
            // switch only if the new regime is clearly supported
            if (proposed_score < p_.hysteresis_margin) {
                return *last_regime_;
            }
        }
    }

    if (!last_regime_.has_value() || proposed != *last_regime_) {
        last_regime_ = proposed;
        cooldown_ = p_.cooldown_bars;
    }
    return *last_regime_;
}

std::string MarketRegimeDetector::regime_to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::TRENDING_UP: return "TRENDING_UP";
        case MarketRegime::TRENDING_DOWN: return "TRENDING_DOWN";
        case MarketRegime::CHOPPY: return "CHOPPY";
        case MarketRegime::HIGH_VOLATILITY: return "HIGH_VOLATILITY";
        case MarketRegime::LOW_VOLATILITY: return "LOW_VOLATILITY";
        default: return "UNKNOWN";
    }
}

} // namespace sentio

```

## 📄 **FILE 32 of 32**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`
- **Size**: 825 lines
- **Modified**: 2025-10-15 09:28:59
- **Type**: cpp
- **Permissions**: -rw-r--r--

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
      calibration_count_(0),
      current_regime_(MarketRegime::CHOPPY),
      bars_since_regime_check_(0) {

    static int oes_instance_count = 0;
    std::cout << "[OES::Constructor] Creating OES instance #" << oes_instance_count++ << std::endl;

    // Initialize feature engine V2 (production-grade with O(1) updates)
    features::EngineConfig engine_config;
    engine_config.momentum = true;
    engine_config.volatility = true;
    engine_config.volume = true;
    engine_config.normalize = true;
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>(engine_config);

    // Get feature count from V2 engine schema
    size_t num_features = feature_engine_->names().size();
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with zero warmup
    // EWRLS predictor starts immediately with high uncertainty
    // Strategy-level warmup ensures feature engine is ready
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 0;  // No warmup - start predicting immediately
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

    // Initialize regime detection if enabled
    if (config_.enable_regime_detection) {
        // Use new adaptive detector with default params (vol_window=96, slope_window=120, chop_window=48)
        regime_detector_ = std::make_unique<MarketRegimeDetector>();
        regime_param_manager_ = std::make_unique<RegimeParameterManager>();
        utils::log_info("Regime detection enabled with adaptive thresholds - check interval: " +
                       std::to_string(config_.regime_check_interval) + " bars");
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
        static int not_ready_count = 0;
        if (not_ready_count < 3) {
            std::cout << "[OES::generate_signal] NOT READY: samples_seen=" << samples_seen_
                      << ", warmup_samples=" << config_.warmup_samples << std::endl;
            not_ready_count++;
        }
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check and update regime if enabled
    check_and_update_regime();

    // Extract features
    std::vector<double> features = extract_features(bar);

    if (features.empty()) {
        static int empty_features_count = 0;
        if (empty_features_count < 5) {
            std::cout << "[OES::generate_signal] EMPTY FEATURES (samples_seen=" << samples_seen_
                      << ", bar_history.size=" << bar_history_.size()
                      << ", feature_engine.is_seeded=" << feature_engine_->is_seeded() << ")" << std::endl;
            empty_features_count++;
        }
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        output.metadata["reason"] = "empty_features";
        return output;
    }

    // Check for NaN in critical features before prediction
    bool has_nan = false;
    for (size_t i = 0; i < features.size(); ++i) {
        if (!std::isfinite(features[i])) {
            has_nan = true;
            static int nan_count = 0;
            if (nan_count < 10) {  // Increase limit to see more instances
                std::cout << "[OES::generate_signal] NaN detected in feature " << i
                          << ", samples_seen=" << samples_seen_
                          << ", feature_engine.bar_count=" << feature_engine_->bar_count()
                          << ", feature_engine.warmup_remaining=" << feature_engine_->warmup_remaining()
                          << ", feature_engine.is_seeded=" << feature_engine_->is_seeded()
                          << std::endl;
                // Print BB features (indices 24-29) for diagnosis
                if (features.size() > 29) {
                    std::cout << "  BB features: [24]=" << features[24]
                              << ", [25]=" << features[25]
                              << ", [26]=" << features[26]
                              << ", [27]=" << features[27]
                              << ", [28]=" << features[28]
                              << ", [29]=" << features[29]
                              << std::endl;
                }
                nan_count++;
            }
            break;
        }
    }

    if (has_nan) {
        // Return neutral signal with low confidence during warmup
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        output.metadata["reason"] = "indicators_warming_up";
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
                  << ", is_ready=" << prediction.is_ready << std::endl;
    }

    // Check for NaN in prediction
    if (!std::isfinite(prediction.predicted_return) || !std::isfinite(prediction.confidence)) {
        std::cerr << "[OES] WARNING: NaN in prediction! pred_return=" << prediction.predicted_return
                  << ", confidence=" << prediction.confidence << " - returning neutral" << std::endl;
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        return output;
    }

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    if (signal_count <= 10) {
// DEBUG:         std::cout << "[OES]   → base_prob=" << base_prob << std::endl;
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
    output.confidence = std::max(0.01, prediction.confidence);  // Ensure confidence is properly propagated with minimum 1%
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
    // DEBUG: Track on_bar calls during warmup
    static int on_bar_call_count = 0;
    if (on_bar_call_count < 3) {
        std::cout << "[OES::on_bar] Call #" << on_bar_call_count
                  << " - samples_seen=" << samples_seen_
                  << ", skip_feature_engine=" << skip_feature_engine_update_ << std::endl;
        on_bar_call_count++;
    }

    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine V2 (skip if using external cached features)
    if (!skip_feature_engine_update_) {
        feature_engine_->update(bar);

        // DEBUG: Confirm feature engine update
        static int fe_update_count = 0;
        if (fe_update_count < 3) {
            std::cout << "[OES::on_bar] Feature engine updated (call #" << fe_update_count << ")" << std::endl;
            fe_update_count++;
        }
    }

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
    // Use external features if provided (for feature caching optimization)
    if (external_features_ != nullptr) {
        return *external_features_;
    }

    // DEBUG: Track why features might be empty
    static int extract_count = 0;
    extract_count++;

    if (bar_history_.size() < MIN_FEATURES_BARS) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": bar_history_.size()=" << bar_history_.size()
// DEBUG:                       << " < MIN_FEATURES_BARS=" << MIN_FEATURES_BARS
// DEBUG:                       << " → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_seeded()) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": feature_engine_v2 NOT ready → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};
    }

    // Get features from V2 engine (returns const vector& - no copy)
    const auto& features_view = feature_engine_->features_view();
    std::vector<double> features(features_view.begin(), features_view.end());
    if (extract_count <= 10 || features.empty()) {
// DEBUG:         std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                   << ": got " << features.size() << " features from engine"
// DEBUG:                   << std::endl;
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
    // FIX #4: Ensure process_pending_updates() processes all queued updates
    // Check current bar index (samples_seen_ may have been incremented already in on_bar())
    // So check both samples_seen_ and samples_seen_-1 to handle timing

    int current_bar_index = samples_seen_;

    auto it = pending_updates_.find(current_bar_index);
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

        if (current_bar_index % 100 == 0) {
            utils::log_debug("Processed " + std::to_string(static_cast<int>(update.count)) +
                           " updates at bar " + std::to_string(current_bar_index) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        pending_updates_.erase(it);
    }

    // Clean up very old pending updates (>1000 bars behind)
    // This prevents memory leak if some updates never get processed
    if (samples_seen_ % 1000 == 0) {
        auto old_it = pending_updates_.begin();
        while (old_it != pending_updates_.end()) {
            if (old_it->first < current_bar_index - 1000) {
                utils::log_warning("Cleaning up stale pending update at bar " +
                                  std::to_string(old_it->first) +
                                  " (current bar: " + std::to_string(current_bar_index) + ")");
                old_it = pending_updates_.erase(old_it);
            } else {
                ++old_it;
            }
        }
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

void OnlineEnsembleStrategy::check_and_update_regime() {
    if (!config_.enable_regime_detection || !regime_detector_) {
        return;
    }

    // Check regime periodically
    bars_since_regime_check_++;
    if (bars_since_regime_check_ < config_.regime_check_interval) {
        return;
    }

    bars_since_regime_check_ = 0;

    // Need sufficient history
    if (bar_history_.size() < static_cast<size_t>(config_.regime_lookback_period)) {
        return;
    }

    // Detect current regime
    std::vector<Bar> recent_bars(bar_history_.end() - config_.regime_lookback_period,
                                 bar_history_.end());
    MarketRegime new_regime = regime_detector_->detect_regime(recent_bars);

    // Switch parameters if regime changed
    if (new_regime != current_regime_) {
        MarketRegime old_regime = current_regime_;
        current_regime_ = new_regime;

        RegimeParams params = regime_param_manager_->get_params_for_regime(new_regime);

        // Apply new thresholds
        current_buy_threshold_ = params.buy_threshold;
        current_sell_threshold_ = params.sell_threshold;

        // Log regime transition
        utils::log_info("Regime transition: " +
                       MarketRegimeDetector::regime_to_string(old_regime) + " -> " +
                       MarketRegimeDetector::regime_to_string(new_regime) +
                       " | buy=" + std::to_string(current_buy_threshold_) +
                       " sell=" + std::to_string(current_sell_threshold_) +
                       " lambda=" + std::to_string(params.ewrls_lambda) +
                       " bb=" + std::to_string(params.bb_amplification_factor));

        // Note: For full regime switching, we would also update:
        // - config_.ewrls_lambda (requires rebuilding predictor)
        // - config_.bb_amplification_factor
        // - config_.horizon_weights
        // For now, only threshold switching is implemented (most impactful)
    }
}

} // namespace sentio

```

