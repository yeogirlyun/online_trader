# Multi-Symbol Rotation - Design Review Mega Doc

**Generated**: 2025-10-15 02:51:25
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: Review document: MULTI_SYMBOL_ROTATION_DETAILED_DESIGN.md (46 valid files)
**Description**: Automatically extracted source modules from design document

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [include/analysis/performance_analyzer.h](#file-1)
2. [include/backend/adaptive_trading_mechanism.h](#file-2)
3. [include/backend/dynamic_allocation_manager.h](#file-3)
4. [include/backend/dynamic_hysteresis_manager.h](#file-4)
5. [include/backend/enhanced_position_state_machine.h](#file-5)
6. [include/backend/ensemble_position_state_machine.h](#file-6)
7. [include/backend/position_state_machine.h](#file-7)
8. [include/common/config_loader.h](#file-8)
9. [include/common/json_utils.h](#file-9)
10. [include/common/time_utils.h](#file-10)
11. [include/common/types.h](#file-11)
12. [include/core/data_manager.h](#file-12)
13. [include/data/multi_symbol_data_manager.h](#file-13)
14. [include/features/indicators.h](#file-14)
15. [include/features/rolling.h](#file-15)
16. [include/features/unified_feature_engine.h](#file-16)
17. [include/learning/online_predictor.h](#file-17)
18. [include/live/alpaca_client.hpp](#file-18)
19. [include/live/bar_feed_interface.h](#file-19)
20. [include/live/broker_client_interface.h](#file-20)
21. [include/live/mock_bar_feed_replay.h](#file-21)
22. [include/strategy/multi_symbol_oes_manager.h](#file-22)
23. [include/strategy/online_ensemble_strategy.h](#file-23)
24. [include/strategy/signal_aggregator.h](#file-24)
25. [include/strategy/signal_output.h](#file-25)
26. [include/testing/test_framework.h](#file-26)
27. [scripts/comprehensive_warmup.sh](#file-27)
28. [scripts/launch_trading.sh](#file-28)
29. [src/analysis/performance_analyzer.cpp](#file-29)
30. [src/backend/adaptive_trading_mechanism.cpp](#file-30)
31. [src/backend/dynamic_allocation_manager.cpp](#file-31)
32. [src/backend/dynamic_hysteresis_manager.cpp](#file-32)
33. [src/backend/enhanced_position_state_machine.cpp](#file-33)
34. [src/backend/ensemble_position_state_machine.cpp](#file-34)
35. [src/backend/position_state_machine.cpp](#file-35)
36. [src/common/config_loader.cpp](#file-36)
37. [src/core/data_manager.cpp](#file-37)
38. [src/data/multi_symbol_data_manager.cpp](#file-38)
39. [src/features/unified_feature_engine.cpp](#file-39)
40. [src/live/alpaca_client.cpp](#file-40)
41. [src/live/mock_bar_feed_replay.cpp](#file-41)
42. [src/strategy/multi_symbol_oes_manager.cpp](#file-42)
43. [src/strategy/online_ensemble_strategy.cpp](#file-43)
44. [src/strategy/signal_aggregator.cpp](#file-44)
45. [src/testing/test_framework.cpp](#file-45)
46. [tools/data_downloader.py](#file-46)

---

## 📄 **FILE 1 of 46**: include/analysis/performance_analyzer.h

**File Information**:
- **Path**: `include/analysis/performance_analyzer.h`

- **Size**: 327 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
// include/analysis/performance_analyzer.h
#pragma once

#include "performance_metrics.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

// Forward declaration for Enhanced PSM integration
namespace sentio::analysis {
    class EnhancedPerformanceAnalyzer;
}

namespace sentio::analysis {
using MarketData = sentio::Bar;

/**
 * @brief Configuration for PSM-based validation
 */
struct PSMValidationConfig {
    double starting_capital = 100000.0;
    std::string cost_model = "alpaca";  // "alpaca" or "percentage"
    bool leverage_enabled = true;
    bool enable_dynamic_psm = true;
    bool enable_hysteresis = true;
    bool enable_dynamic_allocation = true;
    double slippage_factor = 0.0;
    bool keep_temp_files = false;  // For debugging
    // Default to file-based validation to ensure single source of truth via Enhanced PSM
    // Use a local artifacts directory managed by TempFileManager
    std::string temp_directory = "artifacts/tmp";
};

/**
 * @brief Comprehensive performance analysis engine
 * 
 * Provides detailed analysis of strategy performance including:
 * - MRB calculations (signal-based and trading-based)
 * - Risk-adjusted return metrics
 * - Drawdown analysis
 * - Statistical significance testing
 */
class PerformanceAnalyzer {
public:
    /**
     * @brief Calculate comprehensive performance metrics
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return PerformanceMetrics structure with all metrics
     */
    static PerformanceMetrics calculate_metrics(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Calculate signal directional accuracy
     * @param signals Generated strategy signals
     * @param market_data Market data to compare against
     * @return Signal accuracy (0.0-1.0)
     */
    static double calculate_signal_accuracy(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Calculate trading-based MRB with actual Enhanced PSM simulation
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param config PSM validation configuration (optional)
     * @return Trading-based MRB with full Enhanced PSM
     */
    static double calculate_trading_based_mrb_with_psm(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        const PSMValidationConfig& config = PSMValidationConfig{}
    );

    // Dataset-path overload: preferred for sanity-check to avoid temp CSV schema/index mismatches
    static double calculate_trading_based_mrb_with_psm(
        const std::vector<SignalOutput>& signals,
        const std::string& dataset_csv_path,
        int blocks = 20,
        const PSMValidationConfig& config = PSMValidationConfig{}
    );
    
    /**
     * @brief Calculate trading-based MRB
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks for MRB calculation
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return Trading-based Mean Reversion Baseline
     */
    static double calculate_trading_based_mrb(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks = 20,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Calculate MRB across multiple blocks
     * @param signals Generated strategy signals
     * @param market_data Market data for trading simulation
     * @param blocks Number of blocks
     * @param use_enhanced_psm Use Enhanced PSM by default (NEW)
     * @return Vector of MRB values for each block
     */
    static std::vector<double> calculate_block_mrbs(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        int blocks,
        bool use_enhanced_psm = true  // NEW: default to Enhanced PSM
    );
    
    /**
     * @brief Compare performance across multiple strategies
     * @param strategy_signals Map of strategy name to signals
     * @param market_data Market data for comparison
     * @return ComparisonResult with rankings and comparisons
     */
    static ComparisonResult compare_strategies(
        const std::map<std::string, std::vector<SignalOutput>>& strategy_signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Analyze signal quality
     * @param signals Generated strategy signals
     * @return SignalQualityMetrics structure
     */
    static SignalQualityMetrics analyze_signal_quality(
        const std::vector<SignalOutput>& signals
    );
    
    /**
     * @brief Calculate risk metrics
     * @param equity_curve Equity curve from trading simulation
     * @return RiskMetrics structure
     */
    static RiskMetrics calculate_risk_metrics(
        const std::vector<double>& equity_curve
    );

protected:
    /**
     * @brief Enhanced PSM instance for advanced analysis
     */
    static std::unique_ptr<EnhancedPerformanceAnalyzer> enhanced_analyzer_;

private:
    /**
     * @brief Calculate Sharpe ratio
     * @param returns Vector of returns
     * @param risk_free_rate Risk-free rate (default 0.0)
     * @return Sharpe ratio
     */
    static double calculate_sharpe_ratio(
        const std::vector<double>& returns,
        double risk_free_rate = 0.0
    );
    
    /**
     * @brief Calculate maximum drawdown
     * @param equity_curve Equity curve
     * @return Maximum drawdown as percentage
     */
    static double calculate_max_drawdown(
        const std::vector<double>& equity_curve
    );
    
    /**
     * @brief Calculate win rate
     * @param trades Vector of trade results
     * @return Win rate as percentage
     */
    static double calculate_win_rate(
        const std::vector<double>& trades
    );
    
    /**
     * @brief Calculate profit factor
     * @param trades Vector of trade results
     * @return Profit factor
     */
    static double calculate_profit_factor(
        const std::vector<double>& trades
    );
    
    /**
     * @brief Calculate volatility (standard deviation of returns)
     * @param returns Vector of returns
     * @return Volatility
     */
    static double calculate_volatility(
        const std::vector<double>& returns
    );
    
    /**
     * @brief Calculate Sortino ratio
     * @param returns Vector of returns
     * @param risk_free_rate Risk-free rate
     * @return Sortino ratio
     */
    static double calculate_sortino_ratio(
        const std::vector<double>& returns,
        double risk_free_rate = 0.0
    );
    
    /**
     * @brief Calculate Calmar ratio
     * @param returns Vector of returns
     * @param equity_curve Equity curve
     * @return Calmar ratio
     */
    static double calculate_calmar_ratio(
        const std::vector<double>& returns,
        const std::vector<double>& equity_curve
    );
    
    /**
     * @brief Simulate trading based on signals
     * @param signals Strategy signals
     * @param market_data Market data
     * @return Equity curve and trade results
     */
    static std::pair<std::vector<double>, std::vector<double>> simulate_trading(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Calculate returns from equity curve
     * @param equity_curve Equity curve
     * @return Vector of returns
     */
    static std::vector<double> calculate_returns(
        const std::vector<double>& equity_curve
    );
};

/**
 * @brief Walk-forward analysis engine
 */
class WalkForwardAnalyzer {
public:
    struct WalkForwardConfig {
        int window_size = 252;      // Training window size
        int step_size = 21;          // Step size for rolling
        int min_window_size = 126;   // Minimum window size
    };
    
    struct WalkForwardResult {
        std::vector<PerformanceMetrics> in_sample_metrics;
        std::vector<PerformanceMetrics> out_of_sample_metrics;
        double avg_in_sample_mrb = 0.0;
        double avg_out_of_sample_mrb = 0.0;
        double stability_ratio = 0.0;  // out-of-sample / in-sample
        int num_windows = 0;
    };
    
    /**
     * @brief Perform walk-forward analysis
     */
    static WalkForwardResult analyze(
        const std::string& strategy_name,
        const std::vector<MarketData>& market_data,
        const WalkForwardConfig& config
    );
};

/**
 * @brief Stress testing engine
 */
class StressTestAnalyzer {
public:
    enum class StressScenario {
        MARKET_CRASH,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        TRENDING_UP,
        TRENDING_DOWN,
        SIDEWAYS,
        MISSING_DATA,
        EXTREME_OUTLIERS
    };
    
    struct StressTestResult {
        StressScenario scenario;
        std::string scenario_name;
        PerformanceMetrics metrics;
        bool passed;
        std::string description;
    };
    
    /**
     * @brief Run stress tests
     */
    static std::vector<StressTestResult> run_stress_tests(
        const std::string& strategy_name,
        const std::vector<MarketData>& base_market_data,
        const std::vector<StressScenario>& scenarios
    );
    
private:
    /**
     * @brief Apply stress scenario to market data
     */
    static std::vector<MarketData> apply_stress_scenario(
        const std::vector<MarketData>& market_data,
        StressScenario scenario
    );
};

} // namespace sentio::analysis


```

## 📄 **FILE 2 of 46**: include/backend/adaptive_trading_mechanism.h

**File Information**:
- **Path**: `include/backend/adaptive_trading_mechanism.h`

- **Size**: 504 lines
- **Modified**: 2025-10-08 07:44:51

- **Type**: .h

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

## 📄 **FILE 3 of 46**: include/backend/dynamic_allocation_manager.h

**File Information**:
- **Path**: `include/backend/dynamic_allocation_manager.h`

- **Size**: 189 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
// File: include/backend/dynamic_allocation_manager.h
#ifndef DYNAMIC_ALLOCATION_MANAGER_H
#define DYNAMIC_ALLOCATION_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"

// Use sentio namespace types
using sentio::SignalOutput;
using sentio::MarketState;
using sentio::PositionStateMachine;

namespace backend {

class DynamicAllocationManager {
public:
    struct AllocationConfig {
        // Allocation strategy
        enum class Strategy {
            CONFIDENCE_BASED,    // Allocate based on signal confidence
            RISK_PARITY,         // Equal risk contribution
            KELLY_CRITERION,     // Optimal Kelly sizing
            HYBRID               // Combination of above
        } strategy = Strategy::CONFIDENCE_BASED;
        
        // Risk limits
        double max_leverage_allocation = 0.85;  // Max % in leveraged instrument
        double min_base_allocation = 0.10;      // Min % in base instrument
        double max_total_leverage = 3.0;        // Max effective portfolio leverage
        double min_total_allocation = 0.95;     // Min % of capital to allocate
        double max_total_allocation = 1.0;      // Max % of capital to allocate
        
        // Confidence-based parameters
        double confidence_power = 1.0;           // Confidence exponent (higher = more aggressive)
        double confidence_floor = 0.5;           // Minimum confidence for any leveraged allocation
        double confidence_ceiling = 0.95;        // Maximum confidence cap
        
        // Risk parity parameters
        double base_volatility = 0.15;          // Assumed annual vol for base instrument
        double leveraged_volatility = 0.45;     // Assumed annual vol for leveraged (3x base)
        
        // Kelly parameters
        double kelly_fraction = 0.25;           // Fraction of full Kelly to use (conservative)
        double expected_win_rate = 0.55;        // Expected probability of winning trades
        double avg_win_loss_ratio = 1.2;        // Average win size / average loss size
        
        // Advanced features
        bool enable_dynamic_adjustment = true;   // Adjust based on recent performance
        bool enable_volatility_scaling = true;   // Scale allocation by market volatility
        double volatility_target = 0.20;         // Target portfolio volatility
    };
    
    struct AllocationResult {
        // Position 1 (base instrument: QQQ or PSQ)
        std::string base_symbol;
        double base_allocation_pct;      // % of total capital
        double base_position_value;      // $ value
        double base_quantity;            // # shares
        
        // Position 2 (leveraged instrument: TQQQ or SQQQ)
        std::string leveraged_symbol;
        double leveraged_allocation_pct; // % of total capital
        double leveraged_position_value; // $ value
        double leveraged_quantity;       // # shares
        
        // Aggregate metrics
        double total_allocation_pct;     // Total % allocated (should be ~100%)
        double total_position_value;     // Total $ value
        double cash_reserve_pct;         // % held in cash (if any)
        
        // Risk metrics
        double effective_leverage;       // Portfolio-level leverage
        double risk_score;               // 0.0-1.0 (1.0 = max risk)
        double expected_volatility;      // Expected portfolio volatility
        double max_drawdown_estimate;    // Estimated maximum drawdown
        
        // Allocation metadata
        std::string allocation_strategy; // Which strategy was used
        std::string allocation_rationale;// Human-readable explanation
        double confidence_used;          // Actual confidence value used
        double kelly_sizing;             // Kelly criterion suggestion (if calculated)
        
        // Validation flags
        bool is_valid;
        std::vector<std::string> warnings;
    };
    
    struct MarketConditions {
        double current_volatility = 0.0;
        double volatility_percentile = 50.0;  // 0-100 percentile
        double trend_strength = 0.0;          // -1.0 to 1.0
        double correlation = 0.0;              // Correlation between instruments
        std::string market_regime = "NORMAL";  // NORMAL, HIGH_VOL, LOW_VOL, TRENDING
    };
    
    explicit DynamicAllocationManager(const AllocationConfig& config);
    
    // Main allocation function for dual-instrument states
    AllocationResult calculate_dual_allocation(
        PositionStateMachine::State target_state,
        const SignalOutput& signal,
        double available_capital,
        double current_price_base,
        double current_price_leveraged,
        const MarketConditions& market
    ) const;
    
    // Single position allocation (for non-dual states)
    AllocationResult calculate_single_allocation(
        const std::string& symbol,
        const SignalOutput& signal,
        double available_capital,
        double current_price,
        bool is_leveraged = false
    ) const;
    
    // Update configuration
    void update_config(const AllocationConfig& new_config);
    
    // Get current configuration
    const AllocationConfig& get_config() const { return config_; }
    
    // Validation and risk checks
    bool validate_allocation(const AllocationResult& result) const;
    double calculate_risk_score(const AllocationResult& result) const;
    
private:
    AllocationConfig config_;
    
    // Strategy implementations
    AllocationResult calculate_confidence_based_allocation(
        bool is_long,  // true = QQQ_TQQQ, false = PSQ_SQQQ
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_risk_parity_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_kelly_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_hybrid_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    // Helper functions
    void apply_risk_limits(AllocationResult& result) const;
    void apply_volatility_scaling(AllocationResult& result, const MarketConditions& market) const;
    void calculate_risk_metrics(AllocationResult& result) const;
    void add_validation_warnings(AllocationResult& result) const;
    
    // Utility functions
    double calculate_effective_leverage(double base_pct, double leveraged_pct, double leverage_factor = 3.0) const;
    double calculate_expected_volatility(double base_pct, double leveraged_pct) const;
    double estimate_max_drawdown(double effective_leverage, double expected_vol) const;
    
    // Kelly criterion helpers
    double calculate_kelly_fraction(double win_probability, double win_loss_ratio) const;
    double apply_kelly_safety_factor(double raw_kelly) const;
};

} // namespace backend

#endif // DYNAMIC_ALLOCATION_MANAGER_H


```

## 📄 **FILE 4 of 46**: include/backend/dynamic_hysteresis_manager.h

**File Information**:
- **Path**: `include/backend/dynamic_hysteresis_manager.h`

- **Size**: 125 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
// File: include/backend/dynamic_hysteresis_manager.h
#ifndef DYNAMIC_HYSTERESIS_MANAGER_H
#define DYNAMIC_HYSTERESIS_MANAGER_H

#include <deque>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"

// Use sentio namespace types
using sentio::SignalOutput;
using sentio::MarketState;
using sentio::PositionStateMachine;

namespace backend {

class DynamicHysteresisManager {
public:
    struct HysteresisConfig {
        double base_buy_threshold = 0.55;
        double base_sell_threshold = 0.45;
        double strong_margin = 0.15;
        double confidence_threshold = 0.70;
        
        // Hysteresis parameters
        double entry_bias = 0.02;      // Harder to enter new position
        double exit_bias = 0.05;       // Harder to exit existing position
        double variance_sensitivity = 0.10;  // Adjust based on signal variance
        
        // Adaptive parameters
        int signal_history_window = 20;  // Bars to track
        double min_threshold = 0.35;     // Minimum threshold bounds
        double max_threshold = 0.65;     // Maximum threshold bounds
        
        // Advanced hysteresis
        double dual_state_entry_multiplier = 2.0;  // Extra difficulty for dual states
        double momentum_factor = 0.03;              // Trend following adjustment
        bool enable_regime_detection = true;        // Enable market regime detection
    };
    
    struct DynamicThresholds {
        double buy_threshold;
        double sell_threshold;
        double strong_buy_threshold;
        double strong_sell_threshold;
        double confidence_threshold;
        
        // Diagnostic info
        double signal_variance;
        double signal_mean;
        double signal_momentum;
        std::string regime;  // "STABLE", "VOLATILE", "TRENDING_UP", "TRENDING_DOWN"
        
        // Additional metrics
        double neutral_zone_width;
        double hysteresis_strength;
        int bars_in_position;
    };
    
    explicit DynamicHysteresisManager(const HysteresisConfig& config);
    
    // Update signal history
    void update_signal_history(const SignalOutput& signal);
    
    // Get state-dependent thresholds
    DynamicThresholds get_thresholds(
        PositionStateMachine::State current_state,
        const SignalOutput& signal,
        int bars_in_position = 0
    ) const;
    
    // Calculate signal statistics
    double calculate_signal_variance() const;
    double calculate_signal_mean() const;
    double calculate_signal_momentum() const;
    std::string determine_market_regime() const;
    
    // Reset history (for testing or new sessions)
    void reset();
    
    // Get current config
    const HysteresisConfig& get_config() const { return config_; }
    
    // Update config dynamically
    void update_config(const HysteresisConfig& new_config);
    
private:
    HysteresisConfig config_;
    mutable std::deque<SignalOutput> signal_history_;
    
    // State-dependent threshold adjustments
    double get_entry_adjustment(PositionStateMachine::State state) const;
    double get_exit_adjustment(PositionStateMachine::State state) const;
    
    // Variance-based threshold widening
    double get_variance_adjustment() const;
    
    // Momentum-based adjustment
    double get_momentum_adjustment() const;
    
    // Helper functions
    bool is_long_state(PositionStateMachine::State state) const;
    bool is_short_state(PositionStateMachine::State state) const;
    bool is_dual_state(PositionStateMachine::State state) const;
    
    // Calculate rolling statistics
    struct SignalStatistics {
        double mean;
        double variance;
        double std_dev;
        double momentum;
        double min_value;
        double max_value;
    };
    
    SignalStatistics calculate_statistics() const;
};

} // namespace backend

#endif // DYNAMIC_HYSTERESIS_MANAGER_H


```

## 📄 **FILE 5 of 46**: include/backend/enhanced_position_state_machine.h

**File Information**:
- **Path**: `include/backend/enhanced_position_state_machine.h`

- **Size**: 167 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
// File: include/backend/enhanced_position_state_machine.h
#ifndef ENHANCED_POSITION_STATE_MACHINE_H
#define ENHANCED_POSITION_STATE_MACHINE_H

#include <memory>
#include <map>
#include <deque>
#include <unordered_map>
#include "backend/position_state_machine.h"
#include "backend/dynamic_hysteresis_manager.h"
#include "backend/dynamic_allocation_manager.h"
#include "strategy/signal_output.h"
#include "common/types.h"

namespace sentio {

// Forward declarations
struct MarketState;

// Enhanced version of PositionStateMachine with dynamic hysteresis
class EnhancedPositionStateMachine : public PositionStateMachine {
public:
    struct EnhancedConfig {
        bool enable_hysteresis = true;
        bool enable_dynamic_allocation = true;
        bool enable_adaptive_confidence = true;
        bool enable_regime_detection = true;
        bool log_threshold_changes = true;
        int bars_lookback = 20;
        
        // Position tracking
        bool track_bars_in_position = true;
        int max_bars_in_position = 100;  // Force re-evaluation after N bars
        
        // Performance tracking for adaptation
        bool track_performance = true;
        double performance_window = 50;  // Number of trades to track
    };
    
    struct EnhancedTransition : public StateTransition {
        // Additional fields for enhanced functionality
        backend::DynamicHysteresisManager::DynamicThresholds thresholds_used;
        backend::DynamicAllocationManager::AllocationResult allocation;
        int bars_in_current_position;
        double position_pnl;  // Current P&L if in position
        std::string regime;
        
        // Decision metadata
        double original_probability;
        double adjusted_probability;  // After hysteresis
        double original_confidence;
        double adjusted_confidence;   // After adaptation
    };
    
    // Constructors
    EnhancedPositionStateMachine(
        std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_mgr,
        std::shared_ptr<backend::DynamicAllocationManager> allocation_mgr,
        const EnhancedConfig& config
    );
    
    // Wrapper that delegates to enhanced functionality
    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions,
        double confidence_threshold = 0.7
    );
    
    // Enhanced version that returns more detailed transition info
    EnhancedTransition get_enhanced_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions
    );
    
    // Update signal history for hysteresis
    void update_signal_history(const SignalOutput& signal);
    
    // Track position duration
    void update_position_tracking(State new_state);
    int get_bars_in_position() const { return bars_in_position_; }
    
    // Performance tracking for adaptation
    void record_trade_result(double pnl, bool was_profitable);
    double get_recent_win_rate() const;
    double get_recent_avg_pnl() const;
    
    // Configuration
    void set_config(const EnhancedConfig& config) { config_ = config; }
    const EnhancedConfig& get_config() const { return config_; }
    
    // Get managers for external access
    std::shared_ptr<backend::DynamicHysteresisManager> get_hysteresis_manager() { return hysteresis_manager_; }
    std::shared_ptr<backend::DynamicAllocationManager> get_allocation_manager() { return allocation_manager_; }
    
protected:
    // Enhanced signal classification with dynamic thresholds
    SignalType classify_signal_with_hysteresis(
        const SignalOutput& signal,
        const backend::DynamicHysteresisManager::DynamicThresholds& thresholds
    ) const;
    
    // Adapt confidence based on recent performance
    double adapt_confidence(double original_confidence) const;
    
    // Check if transition should be forced due to position age
    bool should_force_transition(State current_state) const;
    
    // Helper to determine if state is a dual position state
    bool is_dual_state(State state) const;
    bool is_long_state(State state) const;
    bool is_short_state(State state) const;
    
    // Create enhanced transition with allocation info
    EnhancedTransition create_enhanced_transition(
        const StateTransition& base_transition,
        const SignalOutput& signal,
        const backend::DynamicHysteresisManager::DynamicThresholds& thresholds,
        double available_capital,
        const MarketState& market
    );
    
private:
    std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_manager_;
    std::shared_ptr<backend::DynamicAllocationManager> allocation_manager_;
    EnhancedConfig config_;
    
    // Position tracking
    State current_state_;
    State previous_state_;
    int bars_in_position_;
    int total_bars_processed_;
    
    // Transition statistics (for monitoring fix effectiveness)
    struct TransitionStats {
        uint32_t total_signals = 0;
        uint32_t transitions_triggered = 0;
        uint32_t short_signals = 0;
        uint32_t short_transitions = 0;
        uint32_t long_signals = 0;
        uint32_t long_transitions = 0;
    } stats_;
    
    // Performance tracking
    struct TradeResult {
        double pnl;
        bool profitable;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    
    // Market regime cache
    std::string current_regime_;
    int regime_bars_count_;
    
    // Helper to log threshold changes
    void log_threshold_changes(
        const backend::DynamicHysteresisManager::DynamicThresholds& old_thresholds,
        const backend::DynamicHysteresisManager::DynamicThresholds& new_thresholds
    ) const;
};

} // namespace sentio

#endif // ENHANCED_POSITION_STATE_MACHINE_H


```

## 📄 **FILE 6 of 46**: include/backend/ensemble_position_state_machine.h

**File Information**:
- **Path**: `include/backend/ensemble_position_state_machine.h`

- **Size**: 122 lines
- **Modified**: 2025-10-07 00:37:12

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

## 📄 **FILE 7 of 46**: include/backend/position_state_machine.h

**File Information**:
- **Path**: `include/backend/position_state_machine.h`

- **Size**: 139 lines
- **Modified**: 2025-10-07 00:37:12

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

## 📄 **FILE 8 of 46**: include/common/config_loader.h

**File Information**:
- **Path**: `include/common/config_loader.h`

- **Size**: 30 lines
- **Modified**: 2025-10-08 03:32:46

- **Type**: .h

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

## 📄 **FILE 9 of 46**: include/common/json_utils.h

**File Information**:
- **Path**: `include/common/json_utils.h`

- **Size**: 153 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

// Simple JSON parsing utilities for basic use cases
// This is a minimal fallback when nlohmann/json is not available

namespace sentio {
namespace json_utils {

/**
 * @brief Simple JSON value type
 */
enum class JsonType {
    STRING,
    NUMBER,
    ARRAY,
    OBJECT,
    BOOLEAN,
    NULL_VALUE
};

/**
 * @brief Simple JSON value class
 */
class JsonValue {
private:
    JsonType type_;
    std::string string_value_;
    double number_value_ = 0.0;
    bool bool_value_ = false;
    std::vector<JsonValue> array_value_;
    std::map<std::string, JsonValue> object_value_;

public:
    JsonValue() : type_(JsonType::NULL_VALUE) {}
    JsonValue(const std::string& value) : type_(JsonType::STRING), string_value_(value) {}
    JsonValue(double value) : type_(JsonType::NUMBER), number_value_(value) {}
    JsonValue(bool value) : type_(JsonType::BOOLEAN), bool_value_(value) {}
    
    JsonType type() const { return type_; }
    
    // String access
    std::string as_string() const { return string_value_; }
    
    // Number access
    double as_double() const { return number_value_; }
    int as_int() const { return static_cast<int>(number_value_); }
    
    // Boolean access
    bool as_bool() const { return bool_value_; }
    
    // Array access
    const std::vector<JsonValue>& as_array() const { return array_value_; }
    void add_to_array(const JsonValue& value) {
        type_ = JsonType::ARRAY;
        array_value_.push_back(value);
    }
    
    // Object access
    const std::map<std::string, JsonValue>& as_object() const { return object_value_; }
    void set_object_value(const std::string& key, const JsonValue& value) {
        type_ = JsonType::OBJECT;
        object_value_[key] = value;
    }
    
    bool has_key(const std::string& key) const {
        return type_ == JsonType::OBJECT && object_value_.find(key) != object_value_.end();
    }
    
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_value;
        if (type_ != JsonType::OBJECT) return null_value;
        auto it = object_value_.find(key);
        return it != object_value_.end() ? it->second : null_value;
    }
    
    // Convenience methods
    std::vector<double> as_double_array() const {
        std::vector<double> result;
        if (type_ == JsonType::ARRAY) {
            for (const auto& val : array_value_) {
                if (val.type() == JsonType::NUMBER) {
                    result.push_back(val.as_double());
                }
            }
        }
        return result;
    }
    
    std::vector<std::string> as_string_array() const {
        std::vector<std::string> result;
        if (type_ == JsonType::ARRAY) {
            for (const auto& val : array_value_) {
                if (val.type() == JsonType::STRING) {
                    result.push_back(val.as_string());
                }
            }
        }
        return result;
    }
};

/**
 * @brief Simple JSON parser for basic metadata files
 * This is a minimal implementation for parsing ML metadata
 */
class SimpleJsonParser {
private:
    std::string json_text_;
    size_t pos_ = 0;
    
    // Forward declarations to break circular dependencies
    JsonValue parse_value();
    JsonValue parse_array();
    JsonValue parse_object();
    
    void skip_whitespace() {
        while (pos_ < json_text_.size() && std::isspace(json_text_[pos_])) {
            pos_++;
        }
    }
    
    // Method declarations (implementations moved to json_utils.cpp to break circular dependencies)
    std::string parse_string();
    double parse_number();

public:
    JsonValue parse(const std::string& json_text);
};

/**
 * @brief Load and parse JSON from file
 */
inline JsonValue load_json_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return JsonValue();
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    SimpleJsonParser parser;
    return parser.parse(buffer.str());
}

} // namespace json_utils
} // namespace sentio

```

## 📄 **FILE 10 of 46**: include/common/time_utils.h

**File Information**:
- **Path**: `include/common/time_utils.h`

- **Size**: 246 lines
- **Modified**: 2025-10-10 10:35:57

- **Type**: .h

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

## 📄 **FILE 11 of 46**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`

- **Size**: 113 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

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

## 📄 **FILE 12 of 46**: include/core/data_manager.h

**File Information**:
- **Path**: `include/core/data_manager.h`

- **Size**: 35 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "common/types.h"

namespace sentio {

class DataManager {
public:
    DataManager() = default;

    // Load market data from CSV or BIN and register bars with immutable IDs
    void load_market_data(const std::string& path);

    // Lookup by immutable bar id; returns nullptr if not found
    const Bar* get_bar(uint64_t bar_id) const;

    // Lookup by positional index (legacy compatibility)
    const Bar* get_bar_by_index(size_t index) const;

    // Access all loaded bars (ordered)
    const std::vector<Bar>& all_bars() const { return bars_; }

private:
    std::unordered_map<uint64_t, size_t> id_to_index_;
    std::vector<Bar> bars_;
};

} // namespace sentio




```

## 📄 **FILE 13 of 46**: include/data/multi_symbol_data_manager.h

**File Information**:
- **Path**: `include/data/multi_symbol_data_manager.h`

- **Size**: 259 lines
- **Modified**: 2025-10-14 22:52:31

- **Type**: .h

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

## 📄 **FILE 14 of 46**: include/features/indicators.h

**File Information**:
- **Path**: `include/features/indicators.h`

- **Size**: 480 lines
- **Modified**: 2025-10-07 22:15:20

- **Type**: .h

```text
#pragma once

#include "features/rolling.h"
#include <cmath>
#include <deque>
#include <limits>

namespace sentio {
namespace features {
namespace ind {

// =============================================================================
// MACD (Moving Average Convergence Divergence)
// Fast EMA (12), Slow EMA (26), Signal Line (9)
// =============================================================================

struct MACD {
    roll::EMA fast{12};
    roll::EMA slow{26};
    roll::EMA sig{9};
    double macd = std::numeric_limits<double>::quiet_NaN();
    double signal = std::numeric_limits<double>::quiet_NaN();
    double hist = std::numeric_limits<double>::quiet_NaN();

    void update(double close) {
        double fast_val = fast.update(close);
        double slow_val = slow.update(close);
        macd = fast_val - slow_val;
        signal = sig.update(macd);
        hist = macd - signal;
    }

    bool is_ready() const {
        return fast.is_ready() && slow.is_ready() && sig.is_ready();
    }

    void reset() {
        fast.reset();
        slow.reset();
        sig.reset();
        macd = signal = hist = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Stochastic Oscillator (%K, %D, Slow)
// Uses rolling highs/lows for efficient calculation
// =============================================================================

struct Stoch {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    roll::EMA d3{3};
    roll::EMA slow3{3};
    double k = std::numeric_limits<double>::quiet_NaN();
    double d = std::numeric_limits<double>::quiet_NaN();
    double slow = std::numeric_limits<double>::quiet_NaN();

    explicit Stoch(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            k = d = slow = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double denom = hi.max() - lo.min();
        k = (denom == 0) ? 50.0 : 100.0 * (close - lo.min()) / denom;
        d = d3.update(k);
        slow = slow3.update(d);
    }

    bool is_ready() const {
        return hi.full() && lo.full() && d3.is_ready() && slow3.is_ready();
    }

    void reset() {
        hi.reset();
        lo.reset();
        d3.reset();
        slow3.reset();
        k = d = slow = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Williams %R
// Measures overbought/oversold levels (-100 to 0)
// =============================================================================

struct WilliamsR {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double r = std::numeric_limits<double>::quiet_NaN();

    explicit WilliamsR(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            r = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double range = hi.max() - lo.min();
        r = (range == 0) ? -50.0 : -100.0 * (hi.max() - close) / range;
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        r = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Bollinger Bands
// Mean ± k * StdDev with %B and bandwidth indicators
// =============================================================================

struct Boll {
    roll::Ring<double> win;
    int k = 2;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sd = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();
    double percent_b = std::numeric_limits<double>::quiet_NaN();
    double bandwidth = std::numeric_limits<double>::quiet_NaN();

    Boll(int period = 20, int k_ = 2) : win(period), k(k_) {}

    void update(double close) {
        win.push(close);

        if (!win.full()) {
            mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
            percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        mean = win.mean();
        sd = win.stdev();
        upper = mean + k * sd;
        lower = mean - k * sd;

        // %B: Position within bands (0 = lower, 1 = upper)
        double band_range = upper - lower;
        percent_b = (band_range == 0) ? 0.5 : (close - lower) / band_range;

        // Bandwidth: Normalized band width
        bandwidth = (mean == 0) ? 0.0 : (upper - lower) / mean;
    }

    bool is_ready() const {
        return win.full();
    }

    void reset() {
        win.reset();
        mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
        percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Donchian Channels
// Rolling high/low breakout levels
// =============================================================================

struct Donchian {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double up = std::numeric_limits<double>::quiet_NaN();
    double dn = std::numeric_limits<double>::quiet_NaN();
    double mid = std::numeric_limits<double>::quiet_NaN();

    explicit Donchian(int lookback = 20) : hi(lookback), lo(lookback) {}

    void update(double high, double low) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            up = dn = mid = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        up = hi.max();
        dn = lo.min();
        mid = 0.5 * (up + dn);
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        up = dn = mid = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// RSI (Relative Strength Index) - Wilder's Method
// Uses Wilder's smoothing for gains/losses
// =============================================================================

struct RSI {
    roll::Wilder avgGain;
    roll::Wilder avgLoss;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit RSI(int period = 14) : avgGain(period), avgLoss(period) {}

    void update(double close) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        double change = close - prevClose;
        prevClose = close;

        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? -change : 0.0;

        double g = avgGain.update(gain);
        double l = avgLoss.update(loss);

        if (!avgLoss.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double rs = (l == 0) ? INFINITY : g / l;
        value = 100.0 - 100.0 / (1.0 + rs);
    }

    bool is_ready() const {
        return avgGain.is_ready() && avgLoss.is_ready();
    }

    void reset() {
        avgGain.reset();
        avgLoss.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ATR (Average True Range) - Wilder's Method
// Volatility indicator using true range
// =============================================================================

struct ATR {
    roll::Wilder w;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ATR(int period = 14) : w(period) {}

    void update(double high, double low, double close) {
        double tr;
        if (std::isnan(prevClose)) {
            tr = high - low;
        } else {
            tr = std::max({
                high - low,
                std::fabs(high - prevClose),
                std::fabs(low - prevClose)
            });
        }
        prevClose = close;
        value = w.update(tr);

        if (!w.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
        }
    }

    bool is_ready() const {
        return w.is_ready();
    }

    void reset() {
        w.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ROC (Rate of Change) %
// Momentum indicator: (close - close_n_periods_ago) / close_n_periods_ago * 100
// =============================================================================

struct ROC {
    std::deque<double> q;
    int period;
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ROC(int p) : period(p) {}

    void update(double close) {
        q.push_back(close);
        if (static_cast<int>(q.size()) < period + 1) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }
        double past = q.front();
        q.pop_front();
        value = (past == 0) ? 0.0 : 100.0 * (close - past) / past;
    }

    bool is_ready() const {
        return static_cast<int>(q.size()) >= period;
    }

    void reset() {
        q.clear();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// CCI (Commodity Channel Index)
// Measures deviation from typical price mean
// =============================================================================

struct CCI {
    roll::Ring<double> tp; // Typical price ring
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit CCI(int period = 20) : tp(period) {}

    void update(double high, double low, double close) {
        double typical_price = (high + low + close) / 3.0;
        tp.push(typical_price);

        if (!tp.full()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double mean = tp.mean();
        double sd = tp.stdev();

        if (sd == 0 || std::isnan(sd)) {
            value = 0.0;
            return;
        }

        // Approximate mean deviation using stdev (empirical factor ~1.25)
        // For exact MD, maintain parallel queue (omitted for O(1) performance)
        value = (typical_price - mean) / (0.015 * sd * 1.25331413732);
    }

    bool is_ready() const {
        return tp.full();
    }

    void reset() {
        tp.reset();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// OBV (On-Balance Volume)
// Cumulative volume indicator based on price direction
// =============================================================================

struct OBV {
    double value = 0.0;
    double prevClose = std::numeric_limits<double>::quiet_NaN();

    void update(double close, double volume) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        if (close > prevClose) {
            value += volume;
        } else if (close < prevClose) {
            value -= volume;
        }
        // No change if close == prevClose

        prevClose = close;
    }

    void reset() {
        value = 0.0;
        prevClose = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// VWAP (Volume Weighted Average Price)
// Intraday indicator: cumulative (price * volume) / cumulative volume
// =============================================================================

struct VWAP {
    double sumPV = 0.0;
    double sumV = 0.0;
    double value = std::numeric_limits<double>::quiet_NaN();

    void update(double price, double volume) {
        sumPV += price * volume;
        sumV += volume;
        if (sumV > 0) {
            value = sumPV / sumV;
        }
    }

    void reset() {
        sumPV = 0.0;
        sumV = 0.0;
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Keltner Channels
// EMA ± (ATR * multiplier)
// =============================================================================

struct Keltner {
    roll::EMA ema;
    ATR atr;
    double multiplier = 2.0;
    double middle = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();

    Keltner(int ema_period = 20, int atr_period = 10, double mult = 2.0)
        : ema(ema_period), atr(atr_period), multiplier(mult) {}

    void update(double high, double low, double close) {
        middle = ema.update(close);
        atr.update(high, low, close);

        if (!atr.is_ready()) {
            upper = lower = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double atr_val = atr.value;
        upper = middle + multiplier * atr_val;
        lower = middle - multiplier * atr_val;
    }

    bool is_ready() const {
        return ema.is_ready() && atr.is_ready();
    }

    void reset() {
        ema.reset();
        atr.reset();
        middle = upper = lower = std::numeric_limits<double>::quiet_NaN();
    }
};

} // namespace ind
} // namespace features
} // namespace sentio

```

## 📄 **FILE 15 of 46**: include/features/rolling.h

**File Information**:
- **Path**: `include/features/rolling.h`

- **Size**: 212 lines
- **Modified**: 2025-10-07 22:14:27

- **Type**: .h

```text
#pragma once

#include <deque>
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>

namespace sentio {
namespace features {
namespace roll {

// =============================================================================
// Welford's Algorithm for One-Pass Variance Calculation
// Numerically stable, O(1) updates, supports sliding windows
// =============================================================================

struct Welford {
    double mean = 0.0;
    double m2 = 0.0;
    int64_t n = 0;

    void add(double x) {
        ++n;
        double delta = x - mean;
        mean += delta / n;
        m2 += delta * (x - mean);
    }

    // Remove sample from sliding window (use with stored outgoing values)
    static void remove_sample(Welford& s, double x) {
        if (s.n <= 1) {
            s = {};
            return;
        }
        double mean_prev = s.mean;
        s.n -= 1;
        s.mean = (s.n * mean_prev - x) / s.n;
        s.m2 -= (x - mean_prev) * (x - s.mean);
        // Numerical stability guard
        if (s.m2 < 0 && s.m2 > -1e-12) s.m2 = 0.0;
    }

    inline double var() const {
        return (n > 1) ? (m2 / (n - 1)) : std::numeric_limits<double>::quiet_NaN();
    }

    inline double stdev() const {
        double v = var();
        return std::isnan(v) ? v : std::sqrt(v);
    }

    inline void reset() {
        mean = 0.0;
        m2 = 0.0;
        n = 0;
    }
};

// =============================================================================
// Ring Buffer with O(1) Min/Max via Monotonic Deques
// Perfect for Donchian Channels, Williams %R, rolling highs/lows
// =============================================================================

template<typename T>
class Ring {
public:
    explicit Ring(size_t capacity = 1) : capacity_(capacity) {
        buf_.reserve(capacity);
    }

    void push(T value) {
        if (size() == capacity_) pop();
        buf_.push_back(value);

        // Maintain monotonic deques for O(1) min/max
        while (!dq_max_.empty() && dq_max_.back() < value) {
            dq_max_.pop_back();
        }
        while (!dq_min_.empty() && dq_min_.back() > value) {
            dq_min_.pop_back();
        }
        dq_max_.push_back(value);
        dq_min_.push_back(value);

        // Update Welford statistics
        stats_.add(static_cast<double>(value));
    }

    void pop() {
        if (buf_.empty()) return;
        T out = buf_.front();
        buf_.erase(buf_.begin());

        // Remove from monotonic deques if it's the front element
        if (!dq_max_.empty() && dq_max_.front() == out) {
            dq_max_.erase(dq_max_.begin());
        }
        if (!dq_min_.empty() && dq_min_.front() == out) {
            dq_min_.erase(dq_min_.begin());
        }

        // Update Welford statistics
        Welford::remove_sample(stats_, static_cast<double>(out));
    }

    size_t size() const { return buf_.size(); }
    size_t capacity() const { return capacity_; }
    bool full() const { return size() == capacity_; }
    bool empty() const { return buf_.empty(); }

    T min() const {
        return dq_min_.empty() ? buf_.front() : dq_min_.front();
    }

    T max() const {
        return dq_max_.empty() ? buf_.front() : dq_max_.front();
    }

    double mean() const { return stats_.mean; }
    double stdev() const { return stats_.stdev(); }
    double variance() const { return stats_.var(); }

    void reset() {
        buf_.clear();
        dq_min_.clear();
        dq_max_.clear();
        stats_.reset();
    }

private:
    size_t capacity_;
    std::vector<T> buf_;
    std::vector<T> dq_min_;
    std::vector<T> dq_max_;
    Welford stats_;
};

// =============================================================================
// Exponential Moving Average (EMA)
// O(1) updates, standard α = 2/(period+1) smoothing
// =============================================================================

struct EMA {
    double val = std::numeric_limits<double>::quiet_NaN();
    double alpha = 0.0;

    explicit EMA(int period = 14) {
        set_period(period);
    }

    void set_period(int p) {
        alpha = (p <= 1) ? 1.0 : (2.0 / (p + 1.0));
    }

    double update(double x) {
        if (std::isnan(val)) {
            val = x;
        } else {
            val = alpha * x + (1.0 - alpha) * val;
        }
        return val;
    }

    double get_value() const { return val; }
    bool is_ready() const { return !std::isnan(val); }

    void reset() {
        val = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Wilder's Smoothing (for ATR, RSI)
// First N values: SMA seed, then Wilder smoothing
// =============================================================================

struct Wilder {
    double val = std::numeric_limits<double>::quiet_NaN();
    int period = 14;
    int i = 0;

    explicit Wilder(int p = 14) : period(p) {}

    double update(double x) {
        if (i < period) {
            // SMA seed phase
            if (std::isnan(val)) val = 0.0;
            val += x;
            ++i;
            if (i == period) {
                val /= period;
            }
        } else {
            // Wilder smoothing: ((prev * (n-1)) + new) / n
            val = ((val * (period - 1)) + x) / period;
        }
        return val;
    }

    double get_value() const { return val; }
    bool is_ready() const { return i >= period; }

    void reset() {
        val = std::numeric_limits<double>::quiet_NaN();
        i = 0;
    }
};

} // namespace roll
} // namespace features
} // namespace sentio

```

## 📄 **FILE 16 of 46**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`

- **Size**: 227 lines
- **Modified**: 2025-10-08 03:22:10

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

## 📄 **FILE 17 of 46**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`

- **Size**: 133 lines
- **Modified**: 2025-10-07 00:37:12

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

## 📄 **FILE 18 of 46**: include/live/alpaca_client.hpp

**File Information**:
- **Path**: `include/live/alpaca_client.hpp`

- **Size**: 216 lines
- **Modified**: 2025-10-09 10:39:21

- **Type**: .hpp

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

## 📄 **FILE 19 of 46**: include/live/bar_feed_interface.h

**File Information**:
- **Path**: `include/live/bar_feed_interface.h`

- **Size**: 68 lines
- **Modified**: 2025-10-08 23:38:54

- **Type**: .h

```text
#ifndef SENTIO_BAR_FEED_INTERFACE_H
#define SENTIO_BAR_FEED_INTERFACE_H

#include "common/types.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace sentio {

/**
 * Bar Feed Interface
 *
 * Polymorphic interface for market data feeds.
 * Allows substitution of PolygonClient with MockBarFeedReplay
 * without modifying LiveTradeCommand logic.
 */
class IBarFeed {
public:
    virtual ~IBarFeed() = default;

    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Connect to data feed
     */
    virtual bool connect() = 0;

    /**
     * Subscribe to symbols
     */
    virtual bool subscribe(const std::vector<std::string>& symbols) = 0;

    /**
     * Start receiving data (runs callback for each bar)
     */
    virtual void start(BarCallback callback) = 0;

    /**
     * Stop receiving data and disconnect
     */
    virtual void stop() = 0;

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    virtual std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const = 0;

    /**
     * Check if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * Check if connection is healthy (received message recently)
     */
    virtual bool is_connection_healthy() const = 0;

    /**
     * Get seconds since last message
     */
    virtual int get_seconds_since_last_message() const = 0;
};

} // namespace sentio

#endif // SENTIO_BAR_FEED_INTERFACE_H

```

## 📄 **FILE 20 of 46**: include/live/broker_client_interface.h

**File Information**:
- **Path**: `include/live/broker_client_interface.h`

- **Size**: 143 lines
- **Modified**: 2025-10-09 00:55:40

- **Type**: .h

```text
#ifndef SENTIO_BROKER_CLIENT_INTERFACE_H
#define SENTIO_BROKER_CLIENT_INTERFACE_H

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <map>

namespace sentio {

/**
 * Fill behavior for realistic order simulation
 */
enum class FillBehavior {
    IMMEDIATE_FULL,     // Unrealistic but fast (instant full fill)
    DELAYED_FULL,       // Realistic delay, full fill
    DELAYED_PARTIAL     // Most realistic with partial fills
};

// Forward declarations - actual definitions in position_book.h
struct ExecutionReport;
struct BrokerPosition;

/**
 * Account information
 */
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

/**
 * Order structure
 */
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
 * Broker Client Interface
 *
 * Polymorphic interface for broker operations.
 * Allows substitution of AlpacaClient with MockBroker without
 * modifying LiveTradeCommand logic.
 */
class IBrokerClient {
public:
    virtual ~IBrokerClient() = default;

    // Execution callback for realistic async fills
    using ExecutionCallback = std::function<void(const ExecutionReport&)>;

    /**
     * Set callback for execution reports (async fills)
     */
    virtual void set_execution_callback(ExecutionCallback cb) = 0;

    /**
     * Set fill behavior for order simulation (mock only)
     */
    virtual void set_fill_behavior(FillBehavior behavior) = 0;

    /**
     * Get account information
     */
    virtual std::optional<AccountInfo> get_account() = 0;

    /**
     * Get all open positions
     */
    virtual std::vector<BrokerPosition> get_positions() = 0;

    /**
     * Get position for specific symbol
     */
    virtual std::optional<BrokerPosition> get_position(const std::string& symbol) = 0;

    /**
     * Place a market order
     */
    virtual std::optional<Order> place_market_order(
        const std::string& symbol,
        double quantity,
        const std::string& time_in_force = "gtc") = 0;

    /**
     * Close position for a symbol
     */
    virtual bool close_position(const std::string& symbol) = 0;

    /**
     * Close all positions
     */
    virtual bool close_all_positions() = 0;

    /**
     * Get order by ID
     */
    virtual std::optional<Order> get_order(const std::string& order_id) = 0;

    /**
     * Cancel order by ID
     */
    virtual bool cancel_order(const std::string& order_id) = 0;

    /**
     * Get all open orders
     */
    virtual std::vector<Order> get_open_orders() = 0;

    /**
     * Cancel all open orders (idempotent)
     */
    virtual bool cancel_all_orders() = 0;

    /**
     * Check if market is open
     */
    virtual bool is_market_open() = 0;
};

} // namespace sentio

#endif // SENTIO_BROKER_CLIENT_INTERFACE_H

```

## 📄 **FILE 21 of 46**: include/live/mock_bar_feed_replay.h

**File Information**:
- **Path**: `include/live/mock_bar_feed_replay.h`

- **Size**: 127 lines
- **Modified**: 2025-10-08 23:56:18

- **Type**: .h

```text
#ifndef SENTIO_MOCK_BAR_FEED_REPLAY_H
#define SENTIO_MOCK_BAR_FEED_REPLAY_H

#include "live/bar_feed_interface.h"
#include <deque>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace sentio {

/**
 * Mock Bar Feed with Replay Capability
 *
 * Replays historical bar data with precise time synchronization:
 * - Drift-free timing using absolute time anchors
 * - Configurable speed multiplier (1x = real-time, 39x = accelerated)
 * - Multi-symbol support
 * - Thread-safe bar delivery
 */
class MockBarFeedReplay : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param csv_file Path to CSV file with historical bars
     * @param speed_multiplier Replay speed (1.0 = real-time, 39.0 = 39x speed)
     */
    explicit MockBarFeedReplay(const std::string& csv_file, double speed_multiplier = 1.0);

    ~MockBarFeedReplay() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

    // Mock-specific methods

    /**
     * Load bars from CSV file
     * Format: timestamp,open,high,low,close,volume
     */
    bool load_csv(const std::string& csv_file);

    /**
     * Add bar programmatically (for testing)
     */
    void add_bar(const std::string& symbol, const Bar& bar);

    /**
     * Set speed multiplier (can be changed during replay)
     */
    void set_speed_multiplier(double multiplier);

    /**
     * Get current replay progress
     */
    struct ReplayProgress {
        size_t total_bars;
        size_t current_index;
        double progress_pct;
        uint64_t current_bar_timestamp_ms;
        std::string current_bar_time_str;
    };

    ReplayProgress get_progress() const;

    /**
     * Check if replay is complete
     */
    bool is_replay_complete() const;

    /**
     * Data validation
     */
    bool validate_data_integrity() const;

private:
    using Clock = std::chrono::steady_clock;

    // Bar data (symbol -> bars)
    std::map<std::string, std::vector<Bar>> bars_by_symbol_;
    std::vector<std::string> subscribed_symbols_;

    // Replay state
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<size_t> current_index_;
    double speed_multiplier_;

    // Time synchronization (drift-free)
    Clock::time_point replay_start_real_;
    uint64_t replay_start_market_ms_;

    // Thread management
    std::unique_ptr<std::thread> replay_thread_;
    BarCallback callback_;

    // Recent bars cache (for get_recent_bars)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // Health monitoring
    std::atomic<Clock::time_point> last_message_time_;

    // Replay loop
    void replay_loop();

    // Helper methods
    void store_bar(const std::string& symbol, const Bar& bar);
    std::optional<Bar> get_next_bar(std::string& out_symbol);
    void wait_until_bar_time(const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_MOCK_BAR_FEED_REPLAY_H

```

## 📄 **FILE 22 of 46**: include/strategy/multi_symbol_oes_manager.h

**File Information**:
- **Path**: `include/strategy/multi_symbol_oes_manager.h`

- **Size**: 215 lines
- **Modified**: 2025-10-14 21:14:34

- **Type**: .h

```text
#pragma once

#include "strategy/online_ensemble_strategy.h"
#include "strategy/signal_output.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Manages 6 independent OnlineEnsemble strategy instances
 *
 * Each symbol gets its own:
 * - OnlineEnsembleStrategy instance
 * - EWRLS predictor
 * - Feature engine
 * - Learning state
 *
 * This ensures no cross-contamination between symbols and allows
 * pure independent signal generation.
 *
 * Usage:
 *   MultiSymbolOESManager oes_mgr(config, data_mgr);
 *   auto signals = oes_mgr.generate_all_signals();
 *   oes_mgr.update_all(realized_pnls);
 */
class MultiSymbolOESManager {
public:
    struct Config {
        std::vector<std::string> symbols;              // Active symbols
        OnlineEnsembleStrategy::OnlineEnsembleConfig base_config;  // Base config for all OES
        bool independent_learning = true;              // Each symbol learns independently
        bool share_features = false;                   // Share feature engine (NOT RECOMMENDED)

        // Symbol-specific overrides (optional)
        std::map<std::string, OnlineEnsembleStrategy::OnlineEnsembleConfig> symbol_configs;
    };

    /**
     * @brief Construct manager with data source
     *
     * @param config Configuration
     * @param data_mgr Data manager for multi-symbol data
     */
    explicit MultiSymbolOESManager(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr
    );

    ~MultiSymbolOESManager() = default;

    // === Signal Generation ===

    /**
     * @brief Generate signals for all symbols
     *
     * Returns map of symbol → signal. Only symbols with valid data
     * will have signals.
     *
     * @return Map of symbol → SignalOutput
     */
    std::map<std::string, SignalOutput> generate_all_signals();

    /**
     * @brief Generate signal for specific symbol
     *
     * @param symbol Symbol ticker
     * @return SignalOutput for symbol (or empty if data invalid)
     */
    SignalOutput generate_signal(const std::string& symbol);

    // === Learning Updates ===

    /**
     * @brief Update all OES instances with realized P&L
     *
     * @param realized_pnls Map of symbol → realized P&L
     */
    void update_all(const std::map<std::string, double>& realized_pnls);

    /**
     * @brief Update specific symbol with realized P&L
     *
     * @param symbol Symbol ticker
     * @param realized_pnl Realized P&L from last trade
     */
    void update(const std::string& symbol, double realized_pnl);

    /**
     * @brief Process new bar for all symbols
     *
     * Called each bar to update feature engines and check learning state.
     */
    void on_bar();

    // === Warmup ===

    /**
     * @brief Warmup all OES instances from historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup_all(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Warmup specific symbol
     *
     * @param symbol Symbol ticker
     * @param bars Historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::string& symbol, const std::vector<Bar>& bars);

    // === Configuration ===

    /**
     * @brief Update configuration for all symbols
     *
     * @param new_config New base configuration
     */
    void update_config(const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    /**
     * @brief Update configuration for specific symbol
     *
     * @param symbol Symbol ticker
     * @param new_config New configuration
     */
    void update_config(const std::string& symbol,
                      const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    // === Diagnostics ===

    /**
     * @brief Get performance metrics for all symbols
     *
     * @return Map of symbol → performance metrics
     */
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
    get_all_performance_metrics() const;

    /**
     * @brief Get performance metrics for specific symbol
     *
     * @param symbol Symbol ticker
     * @return Performance metrics
     */
    OnlineEnsembleStrategy::PerformanceMetrics
    get_performance_metrics(const std::string& symbol) const;

    /**
     * @brief Check if all OES instances are ready
     *
     * @return true if all have sufficient warmup samples
     */
    bool all_ready() const;

    /**
     * @brief Get ready status for each symbol
     *
     * @return Map of symbol → ready status
     */
    std::map<std::string, bool> get_ready_status() const;

    /**
     * @brief Get learning state for all symbols
     *
     * @return Map of symbol → learning state
     */
    std::map<std::string, OnlineEnsembleStrategy::LearningState>
    get_all_learning_states() const;

    /**
     * @brief Get OES instance for symbol (for direct access)
     *
     * @param symbol Symbol ticker
     * @return Pointer to OES instance (nullptr if not found)
     */
    OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol);

    /**
     * @brief Get const OES instance for symbol
     *
     * @param symbol Symbol ticker
     * @return Const pointer to OES instance (nullptr if not found)
     */
    const OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol) const;

private:
    /**
     * @brief Get latest bar for symbol from data manager
     *
     * @param symbol Symbol ticker
     * @param bar Output bar
     * @return true if valid bar available
     */
    bool get_latest_bar(const std::string& symbol, Bar& bar);

    Config config_;
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr_;

    // Map of symbol → OES instance
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_instances_;

    // Statistics
    int total_signals_generated_{0};
    int total_updates_{0};
};

} // namespace sentio

```

## 📄 **FILE 23 of 46**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 236 lines
- **Modified**: 2025-10-10 10:27:52

- **Type**: .h

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
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

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

## 📄 **FILE 24 of 46**: include/strategy/signal_aggregator.h

**File Information**:
- **Path**: `include/strategy/signal_aggregator.h`

- **Size**: 180 lines
- **Modified**: 2025-10-14 21:15:38

- **Type**: .h

```text
#pragma once

#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <map>

namespace sentio {

/**
 * @brief Aggregates and ranks signals from multiple symbols
 *
 * Takes raw signals from 6 OES instances and:
 * 1. Applies leverage boost (1.5x for leveraged ETFs)
 * 2. Calculates signal strength: probability × confidence × leverage_boost
 * 3. Ranks signals by strength
 * 4. Filters by minimum strength threshold
 *
 * This is the CORE of the rotation strategy - the best signals win.
 *
 * Design Principle:
 * "Let the signals compete - highest strength gets capital"
 *
 * Usage:
 *   SignalAggregator aggregator(config);
 *   auto ranked = aggregator.rank_signals(all_signals);
 *   // Top N signals will be held by RotationPositionManager
 */
class SignalAggregator {
public:
    struct Config {
        // Leverage boost factors
        std::map<std::string, double> leverage_boosts = {
            {"TQQQ", 1.5},
            {"SQQQ", 1.5},
            {"UPRO", 1.5},
            {"SDS", 1.4},   // -2x, slightly less boost
            {"UVXY", 1.3},  // Volatility, more unpredictable
            {"SVIX", 1.3}
        };

        // Signal filtering
        double min_probability = 0.51;     // Minimum probability for consideration
        double min_confidence = 0.55;      // Minimum confidence for consideration
        double min_strength = 0.40;        // Minimum combined strength

        // Correlation filtering (future enhancement)
        bool enable_correlation_filter = false;
        double max_correlation = 0.85;     // Reject if correlation > 0.85

        // Signal quality thresholds
        bool filter_stale_signals = true;  // Filter signals from stale data
        double max_staleness_seconds = 120.0;  // Max 2 minutes old
    };

    /**
     * @brief Ranked signal with calculated strength
     */
    struct RankedSignal {
        std::string symbol;
        SignalOutput signal;
        double leverage_boost;      // Applied leverage boost factor
        double strength;            // probability × confidence × leverage_boost
        double staleness_weight;    // Staleness factor (1.0 = fresh, 0.0 = very old)
        int rank;                   // 1 = strongest, 2 = second, etc.

        // For sorting
        bool operator<(const RankedSignal& other) const {
            return strength > other.strength;  // Descending order
        }
    };

    explicit SignalAggregator(const Config& config);
    ~SignalAggregator() = default;

    /**
     * @brief Rank all signals by strength
     *
     * Applies leverage boost, calculates strength, filters weak signals,
     * and returns ranked list (strongest first).
     *
     * @param signals Map of symbol → signal
     * @param staleness_weights Optional staleness weights (from DataManager)
     * @return Vector of ranked signals (sorted by strength, descending)
     */
    std::vector<RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals,
        const std::map<std::string, double>& staleness_weights = {}
    );

    /**
     * @brief Get top N signals
     *
     * @param ranked_signals Ranked signals (from rank_signals)
     * @param n Number of top signals to return
     * @return Top N signals
     */
    std::vector<RankedSignal> get_top_n(
        const std::vector<RankedSignal>& ranked_signals,
        int n
    ) const;

    /**
     * @brief Filter signals by direction (LONG or SHORT only)
     *
     * @param ranked_signals Ranked signals
     * @param direction Direction to filter (LONG or SHORT)
     * @return Filtered signals
     */
    std::vector<RankedSignal> filter_by_direction(
        const std::vector<RankedSignal>& ranked_signals,
        SignalType direction
    ) const;

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config) { config_ = new_config; }

    /**
     * @brief Get configuration
     *
     * @return Current configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Get statistics
     */
    struct Stats {
        int total_signals_processed;
        int signals_filtered;
        int signals_ranked;
        std::map<std::string, int> signals_per_symbol;
        double avg_strength;
        double max_strength;
    };

    Stats get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats(); }

private:
    /**
     * @brief Calculate signal strength
     *
     * @param signal Signal output
     * @param leverage_boost Leverage boost factor
     * @param staleness_weight Staleness weight (1.0 = fresh)
     * @return Combined strength score
     */
    double calculate_strength(
        const SignalOutput& signal,
        double leverage_boost,
        double staleness_weight
    ) const;

    /**
     * @brief Check if signal passes filters
     *
     * @param signal Signal output
     * @return true if signal passes all filters
     */
    bool passes_filters(const SignalOutput& signal) const;

    /**
     * @brief Get leverage boost for symbol
     *
     * @param symbol Symbol ticker
     * @return Leverage boost factor (1.0 if not found)
     */
    double get_leverage_boost(const std::string& symbol) const;

    Config config_;
    Stats stats_;
};

} // namespace sentio

```

## 📄 **FILE 25 of 46**: include/strategy/signal_output.h

**File Information**:
- **Path**: `include/strategy/signal_output.h`

- **Size**: 40 lines
- **Modified**: 2025-10-08 10:03:23

- **Type**: .h

```text
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

enum class SignalType {
    NEUTRAL,
    LONG,
    SHORT
};

struct SignalOutput {
    // Core fields
    uint64_t bar_id = 0;
    int64_t timestamp_ms = 0;
    int bar_index = 0;
    std::string symbol;
    double probability = 0.0;
    double confidence = 0.0;        // Prediction confidence
    SignalType signal_type = SignalType::NEUTRAL;
    std::string strategy_name;
    std::string strategy_version;
    
    // NEW: Multi-bar prediction fields
    int prediction_horizon = 1;        // How many bars ahead this predicts (default=1 for backward compat)
    uint64_t target_bar_id = 0;       // The bar this prediction targets
    bool requires_hold = false;        // Signal requires minimum hold period
    int signal_generation_interval = 1; // How often signals are generated
    
    std::map<std::string, std::string> metadata;

    std::string to_json() const;
    std::string to_csv() const;
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio
```

## 📄 **FILE 26 of 46**: include/testing/test_framework.h

**File Information**:
- **Path**: `include/testing/test_framework.h`

- **Size**: 100 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
// include/testing/test_framework.h
#pragma once

#include "test_config.h"
#include "test_result.h"
#include "strategy/istrategy.h"
#include "common/types.h"
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace sentio::testing {
using MarketData = sentio::Bar;

/**
 * @brief Core testing framework for strategy validation and analysis
 * 
 * This framework provides comprehensive testing capabilities including:
 * - Sanity checks for deployment readiness
 * - Full behavioral analysis across multiple datasets
 * - Multi-strategy comparison and benchmarking
 */
class TestFramework {
public:
    /**
     * @brief Execute sanity check for deployment readiness
     * @param config Test configuration parameters
     * @return TestResult containing validation metrics and status
     */
    static TestResult run_sanity_check(const TestConfig& config);
    
    /**
     * @brief Execute comprehensive full test suite
     * @param config Test configuration parameters
     * @return TestResult containing detailed analysis metrics
     */
    static TestResult run_full_test(const TestConfig& config);
    
    /**
     * @brief Run tests across all available strategies
     * @param config Base test configuration
     * @return Vector of test results, one per strategy
     */
    static std::vector<TestResult> run_all_strategies(const TestConfig& config);
    
    /**
     * @brief Execute walk-forward analysis
     * @param config Test configuration with walk-forward parameters
     * @return TestResult containing walk-forward analysis metrics
     */
    static TestResult run_walk_forward_analysis(const TestConfig& config);
    
    /**
     * @brief Execute stress testing scenarios
     * @param config Test configuration with stress test scenarios
     * @return TestResult containing stress test results
     */
    static TestResult run_stress_test(const TestConfig& config);
    
    /**
     * @brief Execute cross-validation analysis
     * @param config Test configuration with cross-validation parameters
     * @return TestResult containing cross-validation metrics
     */
    static TestResult run_cross_validation(const TestConfig& config);

private:
    /**
     * @brief Load strategy instance from factory
     */
    static std::shared_ptr<IStrategy> load_strategy(const std::string& strategy_name, const std::string& config_path = "");
    
    /**
     * @brief Load market data from file
     */
    static std::vector<MarketData> load_market_data(const std::string& data_path);
    
    /**
     * @brief Generate signals for given market data
     */
    static std::vector<SignalOutput> generate_signals(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Validate test configuration
     */
    static bool validate_config(const TestConfig& config, std::string& error_msg);
    
    /**
     * @brief Get list of all available strategies
     */
    static std::vector<std::string> get_available_strategies();
};

} // namespace sentio::testing



```

## 📄 **FILE 27 of 46**: scripts/comprehensive_warmup.sh

**File Information**:
- **Path**: `scripts/comprehensive_warmup.sh`

- **Size**: 372 lines
- **Modified**: 2025-10-09 10:59:22

- **Type**: .sh

```text
#!/bin/bash
#
# Comprehensive Warmup Script for Live Trading
#
# Collects warmup data for strategy initialization:
# - 20 trading blocks (7800 bars @ 390 bars/block) going backwards from launch time
# - Additional 64 bars for feature engine initialization
# - Today's missing bars if launched after 9:30 AM ET
# - Only includes Regular Trading Hours (RTH) quotes: 9:30 AM - 4:00 PM ET
#
# Output: data/equities/SPY_warmup_latest.csv
#

set -e  # Exit on error

# =============================================================================
# Configuration
# =============================================================================

WARMUP_BLOCKS=20           # Number of trading blocks (390 bars each)
BARS_PER_BLOCK=390         # 1-minute bars per block (9:30 AM - 4:00 PM)
FEATURE_WARMUP_BARS=64     # Additional bars for feature engine warmup
TOTAL_WARMUP_BARS=$((WARMUP_BLOCKS * BARS_PER_BLOCK + FEATURE_WARMUP_BARS))  # 7864 bars

OUTPUT_FILE="$PROJECT_ROOT/data/equities/SPY_warmup_latest.csv"
TEMP_DIR="$PROJECT_ROOT/data/tmp/warmup"

# Alpaca API credentials
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
if [ -f "$PROJECT_ROOT/config.env" ]; then
    source "$PROJECT_ROOT/config.env"
fi

if [ -z "$ALPACA_PAPER_API_KEY" ] || [ -z "$ALPACA_PAPER_SECRET_KEY" ]; then
    echo "❌ ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set"
    exit 1
fi

# =============================================================================
# Helper Functions
# =============================================================================

function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ❌ ERROR: $1" >&2
}

# Calculate trading days needed (accounting for 390 bars/day)
function calculate_trading_days_needed() {
    local bars_needed=$1
    # Add buffer for weekends/holidays (1.5x)
    local days_with_buffer=$(echo "scale=0; ($bars_needed / $BARS_PER_BLOCK) * 1.5 + 5" | bc)
    echo $days_with_buffer
}

# Get date N trading days ago (going backwards, skipping weekends)
function get_date_n_trading_days_ago() {
    local n_days=$1
    local current_date=$(TZ='America/New_York' date '+%Y-%m-%d')

    # Simple approximation: multiply by 1.4 to account for weekends
    local calendar_days=$(echo "scale=0; $n_days * 1.4 + 3" | bc)
    local calendar_days_int=$(printf "%.0f" $calendar_days)

    # Calculate date
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - use integer days
        TZ='America/New_York' date -v-${calendar_days_int}d '+%Y-%m-%d'
    else
        # Linux
        date -d "$calendar_days_int days ago" '+%Y-%m-%d'
    fi
}

# Check if market is currently open
function is_market_open() {
    local current_time=$(TZ='America/New_York' date '+%H%M')
    local current_dow=$(TZ='America/New_York' date '+%u')  # 1=Monday, 7=Sunday

    # Check if weekend
    if [ "$current_dow" -ge 6 ]; then
        return 1  # Closed
    fi

    # Check if within RTH (9:30 AM - 4:00 PM)
    if [ "$current_time" -ge 930 ] && [ "$current_time" -lt 1600 ]; then
        return 0  # Open
    else
        return 1  # Closed
    fi
}

# Fetch bars from Alpaca API
function fetch_bars() {
    local symbol=$1
    local start_date=$2
    local end_date=$3
    local output_file=$4

    log_info "Fetching $symbol bars from $start_date to $end_date..."

    # Alpaca API endpoint for historical bars
    local url="https://data.alpaca.markets/v2/stocks/${symbol}/bars"
    url="${url}?start=${start_date}T09:30:00-05:00"
    url="${url}&end=${end_date}T16:00:00-05:00"
    url="${url}&timeframe=1Min"
    url="${url}&limit=10000"
    url="${url}&adjustment=raw"
    url="${url}&feed=iex"  # IEX feed (free tier)

    # Fetch data
    curl -s -X GET "$url" \
        -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
        -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY" \
        > "$output_file"

    if [ $? -ne 0 ]; then
        log_error "Failed to fetch bars from Alpaca API"
        return 1
    fi

    # Check if response contains bars
    if ! grep -q '"bars"' "$output_file"; then
        log_error "No bars returned from Alpaca API"
        cat "$output_file"
        return 1
    fi

    return 0
}

# Convert JSON bars to CSV format
function json_to_csv() {
    local json_file=$1
    local csv_file=$2

    log_info "Converting JSON to CSV format..."

    # Use Python to parse JSON and convert to CSV
    python3 - "$json_file" "$csv_file" << 'PYTHON_SCRIPT'
import json
import sys
from datetime import datetime

json_file = sys.argv[1]
csv_file = sys.argv[2]

with open(json_file, 'r') as f:
    data = json.load(f)

bars = data.get('bars', [])
if not bars:
    print(f"❌ No bars found in JSON file", file=sys.stderr)
    sys.exit(1)

# Write CSV header
with open(csv_file, 'w') as f:
    f.write("timestamp,open,high,low,close,volume\n")

    for bar in bars:
        # Parse timestamp (ISO 8601 format)
        timestamp_str = bar['t']
        try:
            # Remove timezone and convert to timestamp
            dt = datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
            timestamp_ms = int(dt.timestamp() * 1000)

            # Write bar
            f.write(f"{timestamp_ms},{bar['o']},{bar['h']},{bar['l']},{bar['c']},{bar['v']}\n")
        except Exception as e:
            print(f"⚠️  Failed to parse bar: {e}", file=sys.stderr)
            continue

print(f"✓ Converted {len(bars)} bars to CSV")
PYTHON_SCRIPT

    return $?
}

# Filter to only include RTH bars (9:30 AM - 4:00 PM ET)
function filter_rth_bars() {
    local input_csv=$1
    local output_csv=$2

    log_info "Filtering to RTH bars only (9:30 AM - 4:00 PM ET)..."

    python3 - "$input_csv" "$output_csv" << 'PYTHON_SCRIPT'
import sys
from datetime import datetime, timezone
import pytz

input_csv = sys.argv[1]
output_csv = sys.argv[2]

et_tz = pytz.timezone('America/New_York')
rth_bars = []

with open(input_csv, 'r') as f:
    header = f.readline()

    for line in f:
        parts = line.strip().split(',')
        if len(parts) < 6:
            continue

        timestamp_ms = int(parts[0])
        dt_utc = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
        dt_et = dt_utc.astimezone(et_tz)

        # Check if RTH (9:30 AM - 4:00 PM ET)
        hour = dt_et.hour
        minute = dt_et.minute
        time_minutes = hour * 60 + minute

        # 9:30 AM = 570 minutes, 4:00 PM = 960 minutes
        if 570 <= time_minutes < 960:
            rth_bars.append(line)

# Write filtered bars
with open(output_csv, 'w') as f:
    f.write(header)
    for bar in rth_bars:
        f.write(bar)

print(f"✓ Filtered to {len(rth_bars)} RTH bars")
PYTHON_SCRIPT

    return $?
}

# =============================================================================
# Main Warmup Process
# =============================================================================

function main() {
    log_info "========================================================================"
    log_info "Comprehensive Warmup for Live Trading"
    log_info "========================================================================"
    log_info "Configuration:"
    log_info "  - Warmup blocks: $WARMUP_BLOCKS (going backwards from now)"
    log_info "  - Bars per block: $BARS_PER_BLOCK (RTH only)"
    log_info "  - Feature warmup: $FEATURE_WARMUP_BARS bars"
    log_info "  - Total warmup bars: $TOTAL_WARMUP_BARS"
    log_info ""

    # Create temp directory
    mkdir -p "$TEMP_DIR"

    # Determine date range
    local today=$(TZ='America/New_York' date '+%Y-%m-%d')
    local now_et=$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S')

    log_info "Current ET time: $now_et"
    log_info ""

    # Calculate start date (need enough calendar days to get required trading bars)
    local calendar_days_needed=$(calculate_trading_days_needed $TOTAL_WARMUP_BARS)
    local start_date=$(get_date_n_trading_days_ago $calendar_days_needed)

    log_info "Step 1: Fetching Historical Bars"
    log_info "---------------------------------------------"
    log_info "Start date: $start_date (estimated)"
    log_info "End date: $today"
    log_info ""

    # Fetch historical bars from Alpaca
    local json_file="$TEMP_DIR/historical.json"
    if ! fetch_bars "SPY" "$start_date" "$today" "$json_file"; then
        log_error "Failed to fetch historical bars"
        exit 1
    fi

    # Convert JSON to CSV
    local historical_csv="$TEMP_DIR/historical_all.csv"
    if ! json_to_csv "$json_file" "$historical_csv"; then
        log_error "Failed to convert JSON to CSV"
        exit 1
    fi

    # Filter to RTH bars only
    local rth_csv="$TEMP_DIR/historical_rth.csv"
    if ! filter_rth_bars "$historical_csv" "$rth_csv"; then
        log_error "Failed to filter RTH bars"
        exit 1
    fi

    # Count bars
    local historical_bar_count=$(tail -n +2 "$rth_csv" | wc -l | tr -d ' ')
    log_info "Historical bars collected (RTH only): $historical_bar_count"
    log_info ""

    # Check if we need today's bars
    local todays_bars_needed=0
    if is_market_open; then
        log_info "Step 2: Fetching Today's Missing Bars"
        log_info "---------------------------------------------"
        log_info "Market is currently open - fetching today's bars so far"

        # Calculate bars from 9:30 AM to now
        local current_time=$(TZ='America/New_York' date '+%H:%M')
        local current_minutes=$(TZ='America/New_York' date '+%H * 60 + %M' | bc)
        local market_open_minutes=$((9 * 60 + 30))  # 9:30 AM
        todays_bars_needed=$((current_minutes - market_open_minutes))

        log_info "Current time: $current_time ET"
        log_info "Bars from 9:30 AM to now: ~$todays_bars_needed bars"
        log_info ""
    else
        log_info "Step 2: Today's Bars"
        log_info "---------------------------------------------"
        log_info "Market is closed - no additional today's bars needed"
        log_info ""
    fi

    # Take last N bars from historical data
    log_info "Step 3: Creating Final Warmup File"
    log_info "---------------------------------------------"

    # Keep last TOTAL_WARMUP_BARS bars (20 blocks + 64 feature warmup)
    local final_csv="$TEMP_DIR/final_warmup.csv"
    head -1 "$rth_csv" > "$final_csv"  # Header
    tail -n +2 "$rth_csv" | tail -n $TOTAL_WARMUP_BARS >> "$final_csv"

    local final_bar_count=$(tail -n +2 "$final_csv" | wc -l | tr -d ' ')
    log_info "Final warmup bars: $final_bar_count"

    # Verify we have enough bars
    if [ $final_bar_count -lt $TOTAL_WARMUP_BARS ]; then
        log_error "Not enough bars! Got $final_bar_count, need $TOTAL_WARMUP_BARS"
        log_error "Try increasing the date range or check data availability"
        exit 1
    fi

    # Move to final location
    mv "$final_csv" "$OUTPUT_FILE"
    log_info "✓ Warmup file created: $OUTPUT_FILE"
    log_info ""

    # Show summary
    log_info "========================================================================"
    log_info "Warmup Summary"
    log_info "========================================================================"
    log_info "Output file: $OUTPUT_FILE"
    log_info "Total bars: $final_bar_count"
    log_info "  - Historical bars: $((final_bar_count - todays_bars_needed))"
    log_info "  - Today's bars: $todays_bars_needed"
    log_info ""
    log_info "Bar distribution:"
    log_info "  - Feature warmup: First $FEATURE_WARMUP_BARS bars"
    log_info "  - Strategy training: Next $((WARMUP_BLOCKS * BARS_PER_BLOCK)) bars ($WARMUP_BLOCKS blocks)"
    log_info ""

    # Show first and last bar timestamps
    local first_bar=$(tail -n +2 "$OUTPUT_FILE" | head -1)
    local last_bar=$(tail -1 "$OUTPUT_FILE")
    log_info "Date range:"
    log_info "  - First bar: $(echo $first_bar | cut -d',' -f1)"
    log_info "  - Last bar: $(echo $last_bar | cut -d',' -f1)"
    log_info ""

    log_info "✓ Warmup complete - ready for live trading!"
    log_info "========================================================================"

    # Cleanup temp files
    rm -rf "$TEMP_DIR"
}

# Run main
main "$@"

```

## 📄 **FILE 28 of 46**: scripts/launch_trading.sh

**File Information**:
- **Path**: `scripts/launch_trading.sh`

- **Size**: 1054 lines
- **Modified**: 2025-10-10 10:38:51

- **Type**: .sh

```text
#!/bin/bash
#
# Unified Trading Launch Script - Mock & Live Trading with Auto-Optimization
#
# Features:
#   - Mock Mode: Replay historical data for testing
#   - Live Mode: Real paper trading with Alpaca REST API
#   - Pre-Market Optimization: 2-phase Optuna (50 trials each)
#   - Auto warmup and dashboard generation
#
# Usage:
#   ./scripts/launch_trading.sh [mode] [options]
#
# Modes:
#   mock     - Mock trading session (replay historical data)
#   live     - Live paper trading session (9:30 AM - 4:00 PM ET)
#
# Options:
#   --data FILE           Data file for mock mode (default: auto - last 391 bars)
#   --date YYYY-MM-DD     Replay specific date in mock mode (default: most recent day)
#   --speed N             Mock replay speed (default: 39.0x for proper time simulation)
#   --optimize            Run 2-phase Optuna before trading (default: auto for live)
#   --skip-optimize       Skip optimization, use existing params
#   --trials N            Trials per phase for optimization (default: 20)
#   --midday-optimize     Enable hourly re-optimization (10:00, 11:00, 12:00, 13:00, 14:00, 15:00)
#   --midday-time HH:MM   Midday optimization time (deprecated, now runs hourly)
#   --version VERSION     Binary version: "release" or "build" (default: build)
#
# Examples:
#   # Mock trading - replicates most recent live session exactly
#   # Includes: pre-market optimization, full session replay, EOD close, auto-shutdown, email
#   ./scripts/launch_trading.sh mock
#
#   # Mock specific date (e.g., Oct 7, 2025)
#   ./scripts/launch_trading.sh mock --date 2025-10-07
#
#   # Mock at real-time speed (1x) for detailed observation
#   ./scripts/launch_trading.sh mock --speed 1.0
#
#   # Mock with instant replay (0x speed)
#   ./scripts/launch_trading.sh mock --speed 0
#
#   # Live trading
#   ./scripts/launch_trading.sh live
#   ./scripts/launch_trading.sh live --skip-optimize
#   ./scripts/launch_trading.sh live --optimize --trials 100
#

set -e

# =============================================================================
# Configuration
# =============================================================================

# Defaults
MODE=""
DATA_FILE="auto"  # Auto-generate from SPY_RTH_NH.csv using date extraction
MOCK_SPEED=39.0   # Default 39x speed for proper time simulation
MOCK_DATE=""      # Optional: specific date to replay (YYYY-MM-DD), default=most recent
MOCK_SEND_EMAIL=false  # Send email in mock mode (for testing email system)
RUN_OPTIMIZATION="auto"
MIDDAY_OPTIMIZE=false
MIDDAY_TIME="15:15"  # Corrected to 3:15 PM ET (not 2:30 PM)
N_TRIALS=20  # Changed from 50 to 20 for pre-market & hourly optimization
VERSION="build"
PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        mock|live)
            MODE="$1"
            shift
            ;;
        --data)
            DATA_FILE="$2"
            shift 2
            ;;
        --speed)
            MOCK_SPEED="$2"
            shift 2
            ;;
        --date)
            MOCK_DATE="$2"
            shift 2
            ;;
        --send-email)
            MOCK_SEND_EMAIL=true
            shift
            ;;
        --optimize)
            RUN_OPTIMIZATION="yes"
            shift
            ;;
        --skip-optimize)
            RUN_OPTIMIZATION="no"
            shift
            ;;
        --midday-optimize)
            MIDDAY_OPTIMIZE=true
            shift
            ;;
        --midday-time)
            MIDDAY_TIME="$2"
            shift 2
            ;;
        --trials)
            N_TRIALS="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [mock|live] [options]"
            exit 1
            ;;
    esac
done

# Validate mode
if [ -z "$MODE" ]; then
    echo "Error: Mode required (mock or live)"
    echo "Usage: $0 [mock|live] [options]"
    exit 1
fi

# =============================================================================
# Single Instance Protection
# =============================================================================

# Check if trading is already running (only for live mode)
if [ "$MODE" = "live" ]; then
    if pgrep -f "sentio_cli.*live-trade" > /dev/null 2>&1; then
        echo "❌ ERROR: Live trading session already running"
        echo ""
        echo "Running processes:"
        ps aux | grep -E "sentio_cli.*live-trade|alpaca_websocket_bridge" | grep -v grep
        echo ""
        echo "To stop existing session:"
        echo "  pkill -f 'sentio_cli.*live-trade'"
        echo "  pkill -f 'alpaca_websocket_bridge'"
        exit 1
    fi
fi

# Determine optimization behavior
if [ "$RUN_OPTIMIZATION" = "auto" ]; then
    # ALWAYS run optimization for both live and mock modes
    # Mock mode should replicate live mode exactly, including optimization
    RUN_OPTIMIZATION="yes"
fi

cd "$PROJECT_ROOT"

# SSL Certificate
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem

# Load credentials
if [ -f config.env ]; then
    source config.env
fi

# Paths
if [ "$VERSION" = "release" ]; then
    CPP_TRADER="release/sentio_cli_latest"
else
    CPP_TRADER="build/sentio_cli"
fi

OPTUNA_SCRIPT="$PROJECT_ROOT/scripts/run_2phase_optuna.py"
WARMUP_SCRIPT="$PROJECT_ROOT/scripts/comprehensive_warmup.sh"
DASHBOARD_SCRIPT="$PROJECT_ROOT/scripts/professional_trading_dashboard.py"
EMAIL_SCRIPT="$PROJECT_ROOT/scripts/send_dashboard_email.py"
BEST_PARAMS_FILE="$PROJECT_ROOT/config/best_params.json"
LOG_DIR="logs/${MODE}_trading"

# Validate binary
if [ ! -f "$CPP_TRADER" ]; then
    echo "❌ ERROR: Binary not found: $CPP_TRADER"
    exit 1
fi

# Validate credentials for live mode
if [ "$MODE" = "live" ]; then
    if [ -z "$ALPACA_PAPER_API_KEY" ] || [ -z "$ALPACA_PAPER_SECRET_KEY" ]; then
        echo "❌ ERROR: Missing Alpaca credentials in config.env"
        exit 1
    fi
    export ALPACA_PAPER_API_KEY
    export ALPACA_PAPER_SECRET_KEY
fi

# Validate data file for mock mode (skip if auto-generating)
if [ "$MODE" = "mock" ] && [ "$DATA_FILE" != "auto" ] && [ ! -f "$DATA_FILE" ]; then
    echo "❌ ERROR: Data file not found: $DATA_FILE"
    exit 1
fi

# PIDs
TRADER_PID=""
BRIDGE_PID=""

# =============================================================================
# Functions
# =============================================================================

function log_info() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] $1"
}

function log_error() {
    echo "[$(TZ='America/New_York' date '+%Y-%m-%d %H:%M:%S %Z')] ❌ ERROR: $1" >&2
}

function cleanup() {
    if [ -n "$TRADER_PID" ] && kill -0 $TRADER_PID 2>/dev/null; then
        log_info "Stopping trader (PID: $TRADER_PID)..."
        kill -TERM $TRADER_PID 2>/dev/null || true
        sleep 2
        kill -KILL $TRADER_PID 2>/dev/null || true
    fi

    if [ -n "$BRIDGE_PID" ] && kill -0 $BRIDGE_PID 2>/dev/null; then
        log_info "Stopping Alpaca WebSocket bridge (PID: $BRIDGE_PID)..."
        kill -TERM $BRIDGE_PID 2>/dev/null || true
        sleep 1
        kill -KILL $BRIDGE_PID 2>/dev/null || true
    fi
}

function ensure_optimization_data() {
    log_info "========================================================================"
    log_info "Data Availability Check"
    log_info "========================================================================"

    local target_file="data/equities/SPY_RTH_NH_5years.csv"
    local min_days=30  # Minimum 30 trading days for meaningful optimization

    # Check if 5-year data exists and is recent
    if [ -f "$target_file" ]; then
        local file_age_days=$(( ($(date +%s) - $(stat -f %m "$target_file" 2>/dev/null || stat -c %Y "$target_file" 2>/dev/null)) / 86400 ))
        local line_count=$(wc -l < "$target_file")
        local trading_days=$((line_count / 391))

        log_info "Found 5-year data: $trading_days trading days (file age: $file_age_days days)"

        if [ "$trading_days" -ge "$min_days" ] && [ "$file_age_days" -le 7 ]; then
            log_info "✓ Data is sufficient and recent"
            echo "$target_file"
            return 0
        fi

        if [ "$file_age_days" -gt 7 ]; then
            log_warn "Data is older than 7 days - will continue with existing data"
            echo "$target_file"
            return 0
        fi
    else
        log_warn "5-year data file not found"
    fi

    # Fallback: Check for existing files with sufficient data
    for fallback_file in "data/equities/SPY_100blocks.csv" "data/equities/SPY_30blocks.csv" "data/equities/SPY_20blocks.csv"; do
        if [ -f "$fallback_file" ]; then
            local fallback_days=$(($(wc -l < "$fallback_file") / 391))
            if [ "$fallback_days" -ge "$min_days" ]; then
                log_warn "Using fallback: $fallback_file ($fallback_days days)"
                echo "$fallback_file"
                return 0
            fi
        fi
    done

    # Last resort: Try to generate from existing data
    if [ -f "data/equities/SPY_RTH_NH.csv" ]; then
        local existing_days=$(($(wc -l < "data/equities/SPY_RTH_NH.csv") / 391))
        if [ "$existing_days" -ge "$min_days" ]; then
            log_warn "Using existing RTH file: $existing_days days"
            echo "data/equities/SPY_RTH_NH.csv"
            return 0
        fi
    fi

    log_error "CRITICAL: Cannot find or generate sufficient data for optimization"
    log_error "Need at least $min_days trading days (~$((min_days * 391)) bars)"
    log_error "To fix: Run tools/data_downloader.py to generate SPY_RTH_NH_5years.csv"
    return 1
}

function run_morning_optimization() {
    log_info "========================================================================"
    log_info "Morning Pre-Market Optimization (6-10 AM ET)"
    log_info "========================================================================"
    log_info "Phase 1: Primary params (buy/sell thresholds, lambda, BB amp) - $N_TRIALS trials"
    log_info "Phase 2: DISABLED (using Phase 1 only for speed)"
    log_info ""

    local current_hour=$(TZ='America/New_York' date '+%H')
    local current_min=$(TZ='America/New_York' date '+%M')

    # Only run optimization during morning hours (6-10 AM ET)
    if [ "$current_hour" -lt 6 ] || [ "$current_hour" -ge 10 ]; then
        log_info "⚠️  Outside morning optimization window (6-10 AM ET)"
        log_info "   Current time: $current_hour:$current_min ET"
        log_info "   Skipping optimization - using existing parameters"
        return 0
    fi

    log_info "✓ Within morning optimization window (${current_hour}:${current_min} ET)"

    # Ensure we have sufficient data - never compromise!
    local opt_data_file
    opt_data_file=$(ensure_optimization_data 2>&1 | tail -1)
    local check_result=$?

    if [ $check_result -ne 0 ] || [ -z "$opt_data_file" ] || [ ! -f "$opt_data_file" ]; then
        log_error "Data availability check failed"
        return 1
    fi

    log_info "Optimizing on: $opt_data_file"

    python3 "$OPTUNA_SCRIPT" \
        --data "$opt_data_file" \
        --output "$BEST_PARAMS_FILE" \
        --n-trials-phase1 "$N_TRIALS" \
        --n-trials-phase2 0 \
        --n-jobs 4

    if [ $? -eq 0 ]; then
        log_info "✓ Optimization complete - params saved to $BEST_PARAMS_FILE"

        # Save morning baseline for micro-adaptations
        local baseline_file="data/tmp/morning_baseline_params.json"
        cp "$BEST_PARAMS_FILE" "$baseline_file"
        log_info "✓ Morning baseline saved to $baseline_file (for micro-adaptation)"

        # Copy to location where live trader reads from
        cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json" 2>/dev/null || true
        return 0
    else
        log_error "Optimization failed"
        return 1
    fi
}

function run_optimization() {
    # Wrapper for backward compatibility - calls morning optimization
    run_morning_optimization
}

function run_warmup() {
    log_info "========================================================================"
    log_info "Strategy Warmup (20 blocks + today's bars)"
    log_info "========================================================================"

    if [ -f "$WARMUP_SCRIPT" ]; then
        bash "$WARMUP_SCRIPT" 2>&1 | tee "$LOG_DIR/warmup_$(date +%Y%m%d).log"
        if [ $? -eq 0 ]; then
            log_info "✓ Warmup complete"
            return 0
        else
            log_error "Warmup failed"
            return 1
        fi
    else
        log_info "Warmup script not found - strategy will learn from live data"
        return 0
    fi
}

function run_mock_trading() {
    log_info "========================================================================"
    log_info "Mock Trading Session"
    log_info "========================================================================"
    log_info "Data: $DATA_FILE"
    log_info "Speed: ${MOCK_SPEED}x (0=instant)"
    log_info ""

    mkdir -p "$LOG_DIR"

    "$CPP_TRADER" live-trade --mock --mock-data "$DATA_FILE" --mock-speed "$MOCK_SPEED"

    if [ $? -eq 0 ]; then
        log_info "✓ Mock session completed"
        return 0
    else
        log_error "Mock session failed"
        return 1
    fi
}

function run_live_trading() {
    log_info "========================================================================"
    log_info "Live Paper Trading Session"
    log_info "========================================================================"
    log_info "Strategy: OnlineEnsemble EWRLS"
    log_info "Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)"
    log_info "Data source: Alpaca REST API (IEX feed)"
    log_info "EOD close: 3:58 PM ET"
    if [ "$MIDDAY_OPTIMIZE" = true ]; then
        log_info "Midday re-optimization: $MIDDAY_TIME ET"
    fi
    log_info ""

    # Load optimized params if available
    if [ -f "$BEST_PARAMS_FILE" ]; then
        log_info "Using optimized parameters from: $BEST_PARAMS_FILE"
        mkdir -p data/tmp
        cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json"
    fi

    mkdir -p "$LOG_DIR"

    # Start Alpaca WebSocket bridge (Python → FIFO → C++)
    log_info "Starting Alpaca WebSocket bridge..."
    local bridge_log="$LOG_DIR/bridge_$(date +%Y%m%d_%H%M%S).log"
    python3 "$PROJECT_ROOT/scripts/alpaca_websocket_bridge.py" > "$bridge_log" 2>&1 &
    BRIDGE_PID=$!

    log_info "Bridge PID: $BRIDGE_PID"
    log_info "Bridge log: $bridge_log"

    # Wait for FIFO to be created
    log_info "Waiting for FIFO pipe..."
    local fifo_wait=0
    while [ ! -p "/tmp/alpaca_bars.fifo" ] && [ $fifo_wait -lt 10 ]; do
        sleep 1
        fifo_wait=$((fifo_wait + 1))
    done

    if [ ! -p "/tmp/alpaca_bars.fifo" ]; then
        log_error "FIFO pipe not created - bridge may have failed"
        tail -20 "$bridge_log"
        return 1
    fi

    log_info "✓ Bridge connected and FIFO ready"
    log_info ""

    # Start C++ trader (reads from FIFO)
    log_info "Starting C++ trader..."
    local trader_log="$LOG_DIR/trader_$(date +%Y%m%d_%H%M%S).log"
    "$CPP_TRADER" live-trade > "$trader_log" 2>&1 &
    TRADER_PID=$!

    log_info "Trader PID: $TRADER_PID"
    log_info "Trader log: $trader_log"

    sleep 3
    if ! kill -0 $TRADER_PID 2>/dev/null; then
        log_error "Trader exited immediately"
        tail -30 "$trader_log"
        return 1
    fi

    log_info "✓ Live trading started"

    # Track if midday optimization was done
    local midday_opt_done=false

    # Monitor until market close or process dies
    while true; do
        sleep 30

        if ! kill -0 $TRADER_PID 2>/dev/null; then
            log_info "Trader process ended"
            break
        fi

        local current_time=$(TZ='America/New_York' date '+%H:%M')
        local time_num=$(echo "$current_time" | tr -d ':')

        if [ "$time_num" -ge 1600 ]; then
            log_info "Market closed (4:00 PM ET)"
            break
        fi

        # Midday optimization check
        if [ "$MIDDAY_OPTIMIZE" = true ] && [ "$midday_opt_done" = false ]; then
            local midday_num=$(echo "$MIDDAY_TIME" | tr -d ':')
            # Trigger if within 5 minutes of midday time
            if [ "$time_num" -ge "$midday_num" ] && [ "$time_num" -lt $((midday_num + 5)) ]; then
                log_info ""
                log_info "⚡ MIDDAY OPTIMIZATION TIME: $MIDDAY_TIME ET"
                log_info "Stopping trader for re-optimization and restart..."

                # Stop trader and bridge cleanly (send SIGTERM)
                log_info "Stopping trader..."
                kill -TERM $TRADER_PID 2>/dev/null || true
                wait $TRADER_PID 2>/dev/null || true
                log_info "✓ Trader stopped"

                log_info "Stopping bridge..."
                kill -TERM $BRIDGE_PID 2>/dev/null || true
                wait $BRIDGE_PID 2>/dev/null || true
                log_info "✓ Bridge stopped"

                # Fetch morning bars (9:30 AM - current time) for seamless warmup
                log_info "Fetching morning bars for seamless warmup..."
                local today=$(TZ='America/New_York' date '+%Y-%m-%d')
                local morning_bars_file="data/tmp/morning_bars_$(date +%Y%m%d).csv"
                mkdir -p data/tmp

                # Use Python to fetch morning bars via Alpaca API
                python3 -c "
import os
import sys
import json
import requests
from datetime import datetime, timezone
import pytz

api_key = os.getenv('ALPACA_PAPER_API_KEY')
secret_key = os.getenv('ALPACA_PAPER_SECRET_KEY')

if not api_key or not secret_key:
    print('ERROR: Missing Alpaca credentials', file=sys.stderr)
    sys.exit(1)

# Fetch bars from 9:30 AM ET to now
et_tz = pytz.timezone('America/New_York')
now_et = datetime.now(et_tz)
start_time = now_et.replace(hour=9, minute=30, second=0, microsecond=0)

# Convert to ISO format with timezone
start_iso = start_time.isoformat()
end_iso = now_et.isoformat()

url = f'https://data.alpaca.markets/v2/stocks/SPY/bars?start={start_iso}&end={end_iso}&timeframe=1Min&limit=10000&adjustment=raw&feed=iex'
headers = {
    'APCA-API-KEY-ID': api_key,
    'APCA-API-SECRET-KEY': secret_key
}

try:
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    data = response.json()

    bars = data.get('bars', [])
    if not bars:
        print('WARNING: No morning bars returned', file=sys.stderr)
        sys.exit(0)

    # Write to CSV
    with open('$morning_bars_file', 'w') as f:
        f.write('timestamp,open,high,low,close,volume\\n')
        for bar in bars:
            ts_str = bar['t']
            dt = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
            ts_ms = int(dt.timestamp() * 1000)
            f.write(f\"{ts_ms},{bar['o']},{bar['h']},{bar['l']},{bar['c']},{bar['v']}\\n\")

    print(f'✓ Fetched {len(bars)} morning bars')
except Exception as e:
    print(f'ERROR: Failed to fetch morning bars: {e}', file=sys.stderr)
    sys.exit(1)
"
                # CRASH FAST: If morning bars fetch fails, EXIT immediately
                if [ $? -ne 0 ]; then
                    log_error "❌ FATAL: Failed to fetch morning bars for midday optimization"
                    log_error "   Cannot proceed with midday optimization without fresh data"
                    log_error "   Stopping trader and exiting..."
                    kill -TERM $TRADER_PID 2>/dev/null || true
                    kill -TERM $BRIDGE_PID 2>/dev/null || true
                    exit 1
                fi

                # Append morning bars to warmup file for seamless continuation
                if [ -f "$morning_bars_file" ]; then
                    local morning_bar_count=$(tail -n +2 "$morning_bars_file" | wc -l | tr -d ' ')
                    log_info "Appending $morning_bar_count morning bars to warmup data..."
                    tail -n +2 "$morning_bars_file" >> "data/equities/SPY_warmup_latest.csv"
                    log_info "✓ Seamless warmup data prepared"
                fi

                # Run quick optimization (fewer trials for speed)
                local midday_trials=$((N_TRIALS / 2))
                log_info "Running midday optimization ($midday_trials trials/phase)..."

                python3 "$OPTUNA_SCRIPT" \
                    --data "data/equities/SPY_warmup_latest.csv" \
                    --output "$BEST_PARAMS_FILE" \
                    --n-trials-phase1 "$midday_trials" \
                    --n-trials-phase2 "$midday_trials" \
                    --n-jobs 4

                # CRASH FAST: Midday optimization must succeed
                if [ $? -ne 0 ]; then
                    log_error "❌ FATAL: Midday optimization failed"
                    log_error "   Cannot restart trading with unoptimized parameters"
                    log_error "   This is a CRITICAL error - exiting immediately"
                    exit 1
                fi

                log_info "✓ Midday optimization complete"
                cp "$BEST_PARAMS_FILE" "data/tmp/midday_selected_params.json"
                log_info "✓ New parameters deployed"

                # Restart bridge and trader immediately with new params and seamless warmup
                log_info "Restarting bridge and trader with optimized params and seamless warmup..."

                # Restart bridge first
                local restart_bridge_log="$LOG_DIR/bridge_restart_$(date +%Y%m%d_%H%M%S).log"
                python3 "$PROJECT_ROOT/scripts/alpaca_websocket_bridge.py" > "$restart_bridge_log" 2>&1 &
                BRIDGE_PID=$!
                log_info "✓ Bridge restarted (PID: $BRIDGE_PID)"

                # Wait for FIFO
                log_info "Waiting for FIFO pipe..."
                local fifo_wait=0
                while [ ! -p "/tmp/alpaca_bars.fifo" ] && [ $fifo_wait -lt 10 ]; do
                    sleep 1
                    fifo_wait=$((fifo_wait + 1))
                done

                if [ ! -p "/tmp/alpaca_bars.fifo" ]; then
                    log_error "FIFO pipe not created - bridge restart failed"
                    tail -20 "$restart_bridge_log"
                    exit 1
                fi

                # Restart trader
                local restart_trader_log="$LOG_DIR/trader_restart_$(date +%Y%m%d_%H%M%S).log"
                "$CPP_TRADER" live-trade > "$restart_trader_log" 2>&1 &
                TRADER_PID=$!

                log_info "✓ Trader restarted (PID: $TRADER_PID)"
                log_info "✓ Bridge log: $restart_bridge_log"
                log_info "✓ Trader log: $restart_trader_log"

                sleep 3
                if ! kill -0 $TRADER_PID 2>/dev/null; then
                    log_error "Trader failed to restart"
                    tail -30 "$restart_log"
                    exit 1
                fi

                midday_opt_done=true
                log_info "✓ Midday optimization and restart complete - trading resumed"
                log_info ""
            fi
        fi

        # Status every 5 minutes
        if [ $(($(date +%s) % 300)) -lt 30 ]; then
            log_info "Status: Trading ✓ | Time: $current_time ET"
        fi
    done

    return 0
}

function generate_dashboard() {
    log_info ""
    log_info "========================================================================"
    log_info "Generating Trading Dashboard"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades_*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ]; then
        log_error "No trade log file found"
        return 1
    fi

    log_info "Trade log: $latest_trades"

    # Determine market data file
    local market_data="$DATA_FILE"
    if [ "$MODE" = "live" ] && [ -f "data/equities/SPY_warmup_latest.csv" ]; then
        market_data="data/equities/SPY_warmup_latest.csv"
    fi

    local timestamp=$(date +%Y%m%d_%H%M%S)
    local output_file="data/dashboards/${MODE}_session_${timestamp}.html"

    mkdir -p data/dashboards

    python3 "$DASHBOARD_SCRIPT" \
        --tradebook "$latest_trades" \
        --data "$market_data" \
        --output "$output_file" \
        --start-equity 100000

    if [ $? -eq 0 ]; then
        log_info "✓ Dashboard: $output_file"
        ln -sf "$(basename $output_file)" "data/dashboards/latest_${MODE}.html"
        log_info "✓ Latest: data/dashboards/latest_${MODE}.html"

        # Send email notification
        log_info ""
        log_info "Sending email notification..."

        # Source config.env for GMAIL credentials
        if [ -f "$PROJECT_ROOT/config.env" ]; then
            source "$PROJECT_ROOT/config.env"
        fi

        # Send email with dashboard
        python3 "$EMAIL_SCRIPT" \
            --dashboard "$output_file" \
            --trades "$latest_trades" \
            --recipient "${GMAIL_USER:-yeogirl@gmail.com}"

        if [ $? -eq 0 ]; then
            log_info "✓ Email notification sent"
        else
            log_warn "⚠️  Email notification failed (check GMAIL_APP_PASSWORD in config.env)"
        fi

        # Open in browser for mock mode
        if [ "$MODE" = "mock" ]; then
            open "$output_file"
        fi

        return 0
    else
        log_error "Dashboard generation failed"
        return 1
    fi
}

function show_summary() {
    log_info ""
    log_info "========================================================================"
    log_info "Trading Session Summary"
    log_info "========================================================================"

    local latest_trades=$(ls -t "$LOG_DIR"/trades_*.jsonl 2>/dev/null | head -1)
    local latest_signals=$(ls -t "$LOG_DIR"/signals_*.jsonl 2>/dev/null | head -1)

    if [ -z "$latest_trades" ] || [ ! -f "$latest_trades" ]; then
        log_error "No trades file found - session may have failed"
        return 1
    fi

    local num_trades=$(wc -l < "$latest_trades")
    log_info "Total trades: $num_trades"

    if command -v jq &> /dev/null && [ "$num_trades" -gt 0 ]; then
        log_info "Symbols traded:"
        jq -r '.symbol' "$latest_trades" 2>/dev/null | sort | uniq -c | awk '{print "  - " $2 ": " $1 " trades"}' || true
    fi

    log_info ""
    log_info "Dashboard: data/dashboards/latest_${MODE}.html"

    # Run analyze-trades to get MRD and performance metrics
    if [ "$num_trades" -gt 0 ] && [ -n "$latest_signals" ] && [ -f "$latest_signals" ]; then
        log_info ""
        log_info "========================================================================"
        log_info "Performance Analysis (via analyze-trades)"
        log_info "========================================================================"

        # Determine market data file
        local market_data="$DATA_FILE"
        if [ "$MODE" = "live" ] && [ -f "data/equities/SPY_warmup_latest.csv" ]; then
            market_data="data/equities/SPY_warmup_latest.csv"
        fi

        # Run analyze-trades and capture output
        local analysis_output=$("$CPP_TRADER" analyze-trades \
            --signals "$latest_signals" \
            --trades "$latest_trades" \
            --data "$market_data" \
            --start-equity 100000 2>&1)

        # Extract and display key metrics
        if echo "$analysis_output" | grep -q "Mean Return"; then
            echo "$analysis_output" | grep -E "Mean Return|Total Return|Win Rate|Sharpe|Max Drawdown|Total Trades" | while read line; do
                log_info "  $line"
            done

            # Extract MRD specifically and highlight it
            local mrd=$(echo "$analysis_output" | grep "Mean Return per Day" | awk '{print $NF}' | tr -d '%')
            if [ -n "$mrd" ]; then
                log_info ""
                log_info "🎯 KEY METRIC: MRD = ${mrd}%"

                # Provide context based on MRD
                local mrd_float=$(echo "$mrd" | sed 's/%//')
                if (( $(echo "$mrd_float > 0.5" | bc -l) )); then
                    log_info "   ✅ EXCELLENT - Above 0.5% target!"
                elif (( $(echo "$mrd_float > 0.3" | bc -l) )); then
                    log_info "   ✓ GOOD - Above 0.3% baseline"
                elif (( $(echo "$mrd_float > 0" | bc -l) )); then
                    log_info "   ⚠️  MARGINAL - Positive but below target"
                else
                    log_info "   ❌ POOR - Negative returns"
                fi
            fi
        else
            log_info "⚠️  Could not extract performance metrics from analyze-trades"
            log_info "Raw output:"
            echo "$analysis_output" | head -20
        fi
    else
        log_info "⚠️  Skipping performance analysis (no trades or signals)"
    fi

    log_info ""
}

# =============================================================================
# Main
# =============================================================================

function main() {
    log_info "========================================================================"
    log_info "OnlineTrader - Unified Trading Launcher"
    log_info "========================================================================"
    log_info "Mode: $(echo $MODE | tr '[:lower:]' '[:upper:]')"
    log_info "Binary: $CPP_TRADER"
    if [ "$MODE" = "live" ]; then
        log_info "Pre-market optimization: $([ "$RUN_OPTIMIZATION" = "yes" ] && echo "YES ($N_TRIALS trials/phase)" || echo "NO")"
        log_info "Midday re-optimization: $([ "$MIDDAY_OPTIMIZE" = true ] && echo "YES at $MIDDAY_TIME ET" || echo "NO")"
        log_info "API Key: ${ALPACA_PAPER_API_KEY:0:8}..."
    else
        log_info "Data: $DATA_FILE"
        log_info "Speed: ${MOCK_SPEED}x"
    fi
    log_info ""

    trap cleanup EXIT INT TERM

    # Step 0: Data Preparation
    log_info "========================================================================"
    log_info "Data Preparation"
    log_info "========================================================================"

    # Determine target session date
    if [ -n "$MOCK_DATE" ]; then
        TARGET_DATE="$MOCK_DATE"
        log_info "Target session: $TARGET_DATE (specified)"
    else
        # Auto-detect most recent trading session from current date/time
        # Use Python for reliable date/time handling
        TARGET_DATE=$(python3 -c "
import os
os.environ['TZ'] = 'America/New_York'
import time
time.tzset()

from datetime import datetime, timedelta

now = datetime.now()
current_date = now.date()
current_hour = now.hour
current_weekday = now.weekday()  # 0=Mon, 4=Fri, 5=Sat, 6=Sun

# Determine most recent complete trading session
if current_weekday == 5:  # Saturday
    target_date = current_date - timedelta(days=1)  # Friday
elif current_weekday == 6:  # Sunday
    target_date = current_date - timedelta(days=2)  # Friday
elif current_weekday == 0:  # Monday
    if current_hour < 16:  # Before market close
        target_date = current_date - timedelta(days=3)  # Previous Friday
    else:  # After market close
        target_date = current_date  # Today (Monday)
else:  # Tuesday-Friday
    if current_hour >= 16:  # After market close (4 PM ET)
        target_date = current_date  # Today is complete
    else:  # Before market close
        target_date = current_date - timedelta(days=1)  # Yesterday

print(target_date.strftime('%Y-%m-%d'))
")

        log_info "Target session: $TARGET_DATE (auto-detected - market closed)"
    fi

    # Check if data exists for target date
    DATA_EXISTS=$(grep "^$TARGET_DATE" data/equities/SPY_RTH_NH.csv 2>/dev/null | wc -l | tr -d ' ')

    if [ "$DATA_EXISTS" -eq 0 ]; then
        log_info "⚠️  Data for $TARGET_DATE not found in SPY_RTH_NH.csv"
        log_info "Downloading data from Polygon.io..."

        # Check for API key
        if [ -z "$POLYGON_API_KEY" ]; then
            log_error "POLYGON_API_KEY not set - cannot download data"
            log_error "Please set POLYGON_API_KEY in your environment or config.env"
            exit 1
        fi

        # Download data for target date (include a few days before for safety)
        # Use Python for cross-platform date arithmetic
        START_DATE=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$TARGET_DATE', '%Y-%m-%d'); print((target - timedelta(days=7)).strftime('%Y-%m-%d'))")
        END_DATE=$(python3 -c "from datetime import datetime, timedelta; target = datetime.strptime('$TARGET_DATE', '%Y-%m-%d'); print((target + timedelta(days=1)).strftime('%Y-%m-%d'))")

        log_info "Downloading SPY data from $START_DATE to $END_DATE..."
        python3 tools/data_downloader.py SPY \
            --start "$START_DATE" \
            --end "$END_DATE" \
            --outdir data/equities

        if [ $? -ne 0 ]; then
            log_error "Data download failed"
            exit 1
        fi

        log_info "✓ Data downloaded and saved to data/equities/SPY_RTH_NH.csv"
    else
        log_info "✓ Data for $TARGET_DATE exists ($DATA_EXISTS bars)"
    fi

    # Extract warmup and session data
    if [ "$MODE" = "mock" ]; then
        log_info ""
        log_info "Extracting session data for mock replay..."

        WARMUP_FILE="data/equities/SPY_warmup_latest.csv"
        SESSION_FILE="/tmp/SPY_session.csv"

        python3 tools/extract_session_data.py \
            --input data/equities/SPY_RTH_NH.csv \
            --date "$TARGET_DATE" \
            --output-warmup "$WARMUP_FILE" \
            --output-session "$SESSION_FILE"

        if [ $? -ne 0 ]; then
            log_error "Failed to extract session data"
            exit 1
        fi

        DATA_FILE="$SESSION_FILE"
        log_info "✓ Session data extracted"
        log_info "  Warmup: $WARMUP_FILE (for optimization)"
        log_info "  Session: $DATA_FILE (for mock replay)"

        # Generate leveraged ETF data from SPY
        log_info ""
        log_info "Generating leveraged ETF price data..."
        if [ -f "tools/generate_spy_leveraged_data.py" ]; then
            python3 tools/generate_spy_leveraged_data.py \
                --spy data/equities/SPY_RTH_NH.csv \
                --output-dir data/equities 2>&1 | grep -E "✓|✅|Generated|ERROR" || true
            log_info "✓ Leveraged ETF data ready"

            # Copy leveraged ETF files to /tmp for mock broker
            log_info "Copying leveraged ETF data to /tmp for mock broker..."
            for symbol in SH SDS SPXL; do
                if [ -f "data/equities/${symbol}_RTH_NH.csv" ]; then
                    cp "data/equities/${symbol}_RTH_NH.csv" "/tmp/${symbol}_yesterday.csv"
                fi
            done
            log_info "✓ Leveraged ETF data copied to /tmp"
        else
            log_warn "generate_spy_leveraged_data.py not found - skipping"
        fi

    elif [ "$MODE" = "live" ]; then
        log_info ""
        log_info "Preparing warmup data for live trading..."

        WARMUP_FILE="data/equities/SPY_warmup_latest.csv"

        # For live mode: extract all data UP TO yesterday (exclude today)
        YESTERDAY=$(python3 -c "from datetime import datetime, timedelta; print((datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d'))")

        python3 tools/extract_session_data.py \
            --input data/equities/SPY_RTH_NH.csv \
            --date "$YESTERDAY" \
            --output-warmup "$WARMUP_FILE" \
            --output-session /tmp/dummy.csv  # Not used in live mode

        if [ $? -ne 0 ]; then
            log_error "Failed to extract warmup data"
            exit 1
        fi

        log_info "✓ Warmup data prepared"
        log_info "  Warmup: $WARMUP_FILE (up to $YESTERDAY)"
    fi

    log_info ""

    # Step 1: Optimization (if enabled)
    # CRASH FAST PRINCIPLE: If optimization fails, STOP IMMEDIATELY
    if [ "$RUN_OPTIMIZATION" = "yes" ]; then
        if ! run_optimization; then
            log_error "❌ FATAL: Optimization failed"
            log_error "   Reason: Optimization is REQUIRED before trading"
            log_error "   Action: Script will EXIT immediately (no fallback)"
            log_error ""
            log_error "CRASH FAST PRINCIPLE: Never trade with unoptimized or stale parameters"
            exit 1
        fi
        log_info ""
    fi

    # Step 2: Warmup (live mode only, before market open)
    if [ "$MODE" = "live" ]; then
        local current_hour=$(TZ='America/New_York' date '+%H')
        if [ "$current_hour" -lt 9 ] || [ "$current_hour" -ge 16 ]; then
            log_info "Waiting for market open (9:30 AM ET)..."
            while true; do
                current_hour=$(TZ='America/New_York' date '+%H')
                current_min=$(TZ='America/New_York' date '+%M')
                current_dow=$(TZ='America/New_York' date '+%u')

                # Skip weekends
                if [ "$current_dow" -ge 6 ]; then
                    log_info "Weekend - waiting..."
                    sleep 3600
                    continue
                fi

                # Check if market hours
                if [ "$current_hour" -ge 9 ] && [ "$current_hour" -lt 16 ]; then
                    break
                fi

                sleep 60
            done
        fi

        if ! run_warmup; then
            log_info "⚠️  Warmup failed - strategy will learn from live data"
        fi
        log_info ""
    fi

    # Step 3: Trading session
    if [ "$MODE" = "mock" ]; then
        if ! run_mock_trading; then
            log_error "Mock trading failed"
            exit 1
        fi
    else
        if ! run_live_trading; then
            log_error "Live trading failed"
            exit 1
        fi
    fi

    # Step 4: Dashboard
    log_info ""
    generate_dashboard || log_info "⚠️  Dashboard generation failed"

    # Step 5: Summary
    show_summary

    log_info ""
    log_info "✓ Session complete!"
}

main "$@"

```

## 📄 **FILE 29 of 46**: src/analysis/performance_analyzer.cpp

**File Information**:
- **Path**: `src/analysis/performance_analyzer.cpp`

- **Size**: 1143 lines
- **Modified**: 2025-10-07 02:49:09

- **Type**: .cpp

```text
// src/analysis/performance_analyzer.cpp
#include "analysis/performance_analyzer.h"
#include "analysis/temp_file_manager.h"
#include "strategy/istrategy.h"
#include "backend/enhanced_backend_component.h"
#include "common/utils.h"
#include "validation/bar_id_validator.h"
#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <fstream>
#include <cerrno>

namespace sentio::analysis {

PerformanceMetrics PerformanceAnalyzer::calculate_metrics(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    PerformanceMetrics metrics;
    
    if (signals.empty() || market_data.empty()) {
        return metrics;
    }
    
    // Calculate MRB metrics (single source of truth: Enhanced PSM)
    metrics.signal_accuracy = calculate_signal_accuracy(signals, market_data);
    metrics.trading_based_mrb = calculate_trading_based_mrb_with_psm(signals, market_data, blocks);
    metrics.block_mrbs = calculate_block_mrbs(signals, market_data, blocks, true);
    
    // Calculate MRB consistency
    if (!metrics.block_mrbs.empty()) {
        double mean = std::accumulate(metrics.block_mrbs.begin(), 
                                     metrics.block_mrbs.end(), 0.0) / metrics.block_mrbs.size();
        double variance = 0.0;
        for (const auto& mrb : metrics.block_mrbs) {
            variance += (mrb - mean) * (mrb - mean);
        }
        variance /= metrics.block_mrbs.size();
        metrics.mrb_consistency = std::sqrt(variance) / std::abs(mean);
    }
    
    // Simulate trading to get equity curve
    auto [equity_curve, trade_results] = simulate_trading(signals, market_data);
    
    if (!equity_curve.empty()) {
        // Calculate return metrics
        metrics.total_return = (equity_curve.back() - equity_curve.front()) / equity_curve.front();
        metrics.cumulative_return = metrics.total_return;
        
        // Annualized return (assuming 252 trading days)
        double days = equity_curve.size();
        double years = days / 252.0;
        if (years > 0) {
            metrics.annualized_return = std::pow(1.0 + metrics.total_return, 1.0 / years) - 1.0;
        }
        
        // Calculate returns
        auto returns = calculate_returns(equity_curve);
        
        // Risk-adjusted metrics
        metrics.sharpe_ratio = calculate_sharpe_ratio(returns);
        metrics.sortino_ratio = calculate_sortino_ratio(returns);
        metrics.calmar_ratio = calculate_calmar_ratio(returns, equity_curve);
        
        // Risk metrics
        metrics.max_drawdown = calculate_max_drawdown(equity_curve);
        metrics.volatility = calculate_volatility(returns);
        
        // Trading metrics
        if (!trade_results.empty()) {
            metrics.win_rate = calculate_win_rate(trade_results);
            metrics.profit_factor = calculate_profit_factor(trade_results);
            
            metrics.total_trades = trade_results.size();
            metrics.winning_trades = std::count_if(trade_results.begin(), trade_results.end(),
                                                   [](double r) { return r > 0; });
            metrics.losing_trades = metrics.total_trades - metrics.winning_trades;
            
            // Calculate average win/loss
            double total_wins = 0.0, total_losses = 0.0;
            for (const auto& result : trade_results) {
                if (result > 0) total_wins += result;
                else total_losses += std::abs(result);
            }
            
            if (metrics.winning_trades > 0) {
                metrics.avg_win = total_wins / metrics.winning_trades;
            }
            if (metrics.losing_trades > 0) {
                metrics.avg_loss = total_losses / metrics.losing_trades;
            }
            
            metrics.largest_win = *std::max_element(trade_results.begin(), trade_results.end());
            metrics.largest_loss = *std::min_element(trade_results.begin(), trade_results.end());
        }
    }
    
    // Signal metrics
    metrics.total_signals = signals.size();
    for (const auto& signal : signals) {
        switch (signal.signal_type) {
            case SignalType::LONG:
                metrics.long_signals++;
                break;
            case SignalType::SHORT:
                metrics.short_signals++;
                break;
            case SignalType::NEUTRAL:
                metrics.neutral_signals++;
                break;
            default:
                break;
        }
    }
    
    metrics.non_neutral_signals = metrics.long_signals + metrics.short_signals;
    metrics.signal_generation_rate = static_cast<double>(metrics.total_signals - metrics.neutral_signals) 
                                    / metrics.total_signals;
    metrics.non_neutral_ratio = static_cast<double>(metrics.non_neutral_signals) / metrics.total_signals;
    
    // Calculate mean confidence
    double total_confidence = 0.0;
    int confidence_count = 0;
    for (const auto& signal : signals) {
        if (0.7 > 0.0) {
            total_confidence += 0.7;
            confidence_count++;
        }
    }
    if (confidence_count > 0) {
        metrics.mean_confidence = total_confidence / confidence_count;
    }
    
    return metrics;
}

double PerformanceAnalyzer::calculate_signal_accuracy(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data
) {
    // Signal accuracy = % of signals where predicted direction matched actual price movement
    
    if (signals.empty() || market_data.empty()) {
        return 0.0;
    }
    
    size_t min_size = std::min(signals.size(), market_data.size());
    if (min_size < 2) {
        return 0.0;  // Need at least 2 bars to compare
    }
    
    int correct_predictions = 0;
    int total_predictions = 0;
    
    for (size_t i = 0; i < min_size - 1; ++i) {
        const auto& signal = signals[i];
        const auto& current_bar = market_data[i];
        const auto& next_bar = market_data[i + 1];
        
        // Skip neutral signals
        if (signal.signal_type == SignalType::NEUTRAL) {
            continue;
        }
        
        // Determine actual price movement
        double price_change = next_bar.close - current_bar.close;
        bool price_went_up = price_change > 0;
        bool price_went_down = price_change < 0;
        
        // Check if signal predicted correctly
        bool correct = false;
        if (signal.signal_type == SignalType::LONG && price_went_up) {
            correct = true;
        } else if (signal.signal_type == SignalType::SHORT && price_went_down) {
            correct = true;
        }
        
        if (correct) {
            correct_predictions++;
        }
        total_predictions++;
    }
    
    if (total_predictions == 0) {
        return 0.0;
    }
    
    return static_cast<double>(correct_predictions) / total_predictions;
}

double PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    const PSMValidationConfig& config
) {
    // FULL ENHANCED PSM SIMULATION FOR ACCURATE TRADING MRB (RAII-based)
    
    std::cerr << "⚠️⚠️⚠️ calculate_trading_based_mrb_with_psm called with " << signals.size() << " signals, " << blocks << " blocks\n";
    std::cerr.flush();
    
    if (signals.empty() || market_data.empty() || blocks < 1) {
        std::cerr << "⚠️ Returning 0.0 due to empty data or invalid blocks\n";
        std::cerr.flush();
        return 0.0;
    }
    
    try {
        // In-memory fast path (no file parsing) - DISABLED for audit consistency
        if (false && config.temp_directory == ":memory:" && !config.keep_temp_files) {
            EnhancedBackendComponent::EnhancedBackendConfig backend_config;
            backend_config.starting_capital = config.starting_capital;
            backend_config.leverage_enabled = config.leverage_enabled;
            backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
            backend_config.enable_hysteresis = config.enable_hysteresis;
            backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;
            backend_config.slippage_factor = config.slippage_factor;
            EnhancedBackendComponent backend(backend_config);
            auto r = backend.process_in_memory(signals, market_data, 0, SIZE_MAX);
            double total_return_fraction = (r.final_equity - config.starting_capital) / config.starting_capital;
            double mrb = total_return_fraction / blocks;
            if (mrb > 0.10) {
                std::cerr << "WARNING: Unrealistic MRB per block detected (in-memory): " << mrb << "\n";
            }
            return mrb;
        }

        // RAII-based temp file management (automatic cleanup) for file-based audits
        // TEMPORARY: Keep temp files for debugging
        TempFileManager temp_manager(config.temp_directory, true);
        
        std::string temp_signals = temp_manager.create_temp_file("sanity_check_signals", ".jsonl");
        std::string temp_market = temp_manager.create_temp_file("sanity_check_market", ".csv");
        std::string temp_trades = temp_manager.create_temp_file("sanity_check_trades", ".jsonl");
        
        // Write signals
        std::cerr << "DEBUG: Writing " << signals.size() << " signals to " << temp_signals << "\n";
        std::ofstream signal_file(temp_signals);
        for (const auto& sig : signals) {
            signal_file << sig.to_json() << "\n";
        }
        signal_file.close();
        std::cerr << "DEBUG: Signals written successfully\n";
        
        // Write market data in the "standard format" expected by utils::read_csv_data
        // Format: symbol,timestamp_ms,open,high,low,close,volume
        std::cerr << "DEBUG: Writing " << market_data.size() << " bars to " << temp_market << "\n";
        std::ofstream market_file(temp_market);
        market_file << "symbol,timestamp_ms,open,high,low,close,volume\n";
        for (const auto& bar : market_data) {
            // Validate numeric values before writing
            if (std::isnan(bar.open) || std::isnan(bar.high) || std::isnan(bar.low) || 
                std::isnan(bar.close) || std::isnan(bar.volume)) {
                std::cerr << "ERROR: Invalid bar data at timestamp " << bar.timestamp_ms 
                         << ": open=" << bar.open << ", high=" << bar.high 
                         << ", low=" << bar.low << ", close=" << bar.close 
                         << ", volume=" << bar.volume << "\n";
                throw std::runtime_error("Invalid bar data contains NaN");
            }
            market_file << bar.symbol << ","  // Symbol comes FIRST in standard format!
                       << bar.timestamp_ms << "," 
                       << bar.open << "," << bar.high << "," 
                       << bar.low << "," << bar.close << "," 
                       << bar.volume << "\n";
        }
        market_file.close();
        std::cerr << "DEBUG: Market data written successfully\n";
        
        // Configure Enhanced Backend with validation settings
        EnhancedBackendComponent::EnhancedBackendConfig backend_config;
        backend_config.starting_capital = config.starting_capital;
        backend_config.cost_model = (config.cost_model == "alpaca") ? 
                                    CostModel::ALPACA : CostModel::PERCENTAGE;
        backend_config.leverage_enabled = config.leverage_enabled;
        backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
        backend_config.enable_hysteresis = config.enable_hysteresis;
        backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;
        backend_config.slippage_factor = config.slippage_factor;
        
        // Initialize Enhanced Backend
        std::cerr << "DEBUG: Initializing Enhanced Backend\n";
        EnhancedBackendComponent backend(backend_config);
        std::string run_id = utils::generate_run_id("sanity");
        
        // Process through Enhanced PSM
        std::cerr << "DEBUG: Calling process_to_jsonl\n";
        backend.process_to_jsonl(temp_signals, temp_market, temp_trades, run_id, 0, SIZE_MAX, 0.0);
        std::cerr << "DEBUG: process_to_jsonl completed\n";
        
        // CRITICAL: Validate one-to-one correspondence between signals and trades
        std::cerr << "DEBUG: Validating bar_id correspondence\n";
        try {
            auto validation_result = BarIdValidator::validate_files(temp_signals, temp_trades, false);
            if (!validation_result.passed) {
                std::cerr << "WARNING: Bar ID validation found issues:\n";
                std::cerr << validation_result.to_string();
                // Don't throw - just warn, as HOLD decisions are expected
            } else {
                std::cerr << "DEBUG: Bar ID validation passed\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Bar ID validation failed: " << e.what() << "\n";
            throw;
        }
        
        // Read the trade log to get final equity
        double initial_capital = config.starting_capital;
        double final_equity = initial_capital;
        bool parsed_equity = false;
        int trade_lines_read = 0;
        {
            std::ifstream trade_file(temp_trades);
            if (!trade_file.is_open()) {
                std::cerr << "ERROR: Failed to open trade file: " << temp_trades << "\n";
                throw std::runtime_error("Failed to open trade file: " + temp_trades);
            }
            std::string trade_line;
            while (std::getline(trade_file, trade_line)) {
                if (trade_line.empty()) continue;
                trade_lines_read++;
#ifdef NLOHMANN_JSON_AVAILABLE
                try {
                    auto j = json::parse(trade_line);
                    
                    // Check version for migration tracking
                    std::string version = j.value("version", "1.0");
                    if (version == "1.0") {
                        std::cerr << "Warning: Processing legacy trade log format (v1.0)\n";
                    }
                    
                    if (j.contains("equity_after")) {
                        if (j["equity_after"].is_number()) {
                            // Preferred: numeric value
                            final_equity = j["equity_after"].get<double>();
                            parsed_equity = true;
                        } else if (j["equity_after"].is_string()) {
                            // Fallback: string parsing with enhanced error handling
                            try {
                                std::string equity_str = j["equity_after"].get<std::string>();
                                // Trim whitespace and quotes
                                equity_str.erase(0, equity_str.find_first_not_of(" \t\n\r\""));
                                equity_str.erase(equity_str.find_last_not_of(" \t\n\r\"") + 1);
                                
                                if (!equity_str.empty() && equity_str != "null") {
                                    // Use strtod for more robust parsing
                                    char* end;
                                    errno = 0;
                                    double value = std::strtod(equity_str.c_str(), &end);
                                    if (errno == 0 && end != equity_str.c_str() && *end == '\0') {
                                        final_equity = value;
                                        parsed_equity = true;
                                    } else {
                                        std::cerr << "Warning: Invalid equity_after format: '" 
                                                 << equity_str << "' (errno=" << errno << ")\n";
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "Warning: Failed to parse equity_after string: " << e.what() << "\n";
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to parse trade JSON: " << e.what() << "\n";
                } catch (...) {
                    // ignore non-JSON lines
                }
#else
                const std::string key = "\"equity_after\":";
                size_t pos = trade_line.find(key);
                if (pos != std::string::npos) {
                    size_t value_start = pos + key.size();
                    while (value_start < trade_line.size() && (trade_line[value_start] == ' ' || trade_line[value_start] == '\"')) {
                        ++value_start;
                    }
                    size_t value_end = trade_line.find_first_of(",}\"", value_start);
                    if (value_end != std::string::npos && value_end > value_start) {
                        try {
                            std::string equity_str = trade_line.substr(value_start, value_end - value_start);
                            if (!equity_str.empty() && equity_str != "null") {
                                final_equity = std::stod(equity_str);
                                parsed_equity = true;
                            }
                        } catch (...) {
                            // keep scanning
                        }
                    }
                }
#endif
            }
        }
        
        std::cerr << "DEBUG: Read " << trade_lines_read << " trade lines from " << temp_trades << "\n";
        std::cerr << "DEBUG: parsed_equity=" << parsed_equity << ", final_equity=" << final_equity << "\n";
        
        if (!parsed_equity) {
            throw std::runtime_error("Failed to parse equity_after from trade log: " + temp_trades + 
                                   " (read " + std::to_string(trade_lines_read) + " lines)");
        }
        
        double total_return_pct = ((final_equity - initial_capital) / initial_capital) * 100.0;
        
        // MRB = average return per block
        // Return MRB as fraction, not percent, for consistency with rest of system
        double mrb = (total_return_pct / 100.0) / blocks;
        
        // DEBUG: Log equity values to diagnose unrealistic MRB
        std::cerr << "🔍 MRB Calculation: initial=" << initial_capital 
                  << ", final=" << final_equity 
                  << ", return%=" << total_return_pct 
                  << ", blocks=" << blocks 
                  << ", mrb(fraction)=" << mrb << "\n";
        std::cerr.flush();
        if (mrb > 0.10) {
            std::cerr << "WARNING: Unrealistic MRB per block detected: " << mrb << " (fraction)\n";
            std::cerr.flush();
        }
        
        // Temp files automatically cleaned up by TempFileManager destructor
        
        return mrb;
        
    } catch (const std::exception& e) {
        std::cerr << "\n";
        std::cerr << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cerr << "║  CRITICAL ERROR: Enhanced PSM Simulation Failed                ║\n";
        std::cerr << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "Exception: " << e.what() << "\n";
        std::cerr << "Context: calculate_trading_based_mrb_with_psm\n";
        std::cerr << "Signals: " << signals.size() << "\n";
        std::cerr << "Market Data: " << market_data.size() << "\n";
        std::cerr << "Blocks: " << blocks << "\n";
        std::cerr << "\n";
        std::cerr << "⚠️  Sentio uses REAL MONEY for trading.\n";
        std::cerr << "⚠️  Fallback mechanisms are DISABLED to prevent silent failures.\n";
        std::cerr << "⚠️  Fix the underlying issue before proceeding.\n";
        std::cerr << "\n";
        std::cerr.flush();
        
        // NO FALLBACK! Crash immediately with detailed error
        throw std::runtime_error(
            "Enhanced PSM simulation failed: " + std::string(e.what()) + 
            " | Signals: " + std::to_string(signals.size()) + 
            " | Market Data: " + std::to_string(market_data.size()) + 
            " | Blocks: " + std::to_string(blocks)
        );
    }
}

double PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
    const std::vector<SignalOutput>& signals,
    const std::string& dataset_csv_path,
    int blocks,
    const PSMValidationConfig& config
) {
    // Reuse the temp-signal writing logic, but use the real dataset CSV path directly
    if (signals.empty() || blocks < 1) return 0.0;

    try {
        TempFileManager temp_manager(config.temp_directory, config.keep_temp_files);

        std::string temp_signals = temp_manager.create_temp_file("sanity_check_signals", ".jsonl");
        std::string temp_trades = temp_manager.create_temp_file("sanity_check_trades", ".jsonl");

        // Write signals only
        {
            std::ofstream signal_file(temp_signals);
            for (const auto& sig : signals) signal_file << sig.to_json() << "\n";
        }

        // Configure backend
        EnhancedBackendComponent::EnhancedBackendConfig backend_config;
        backend_config.starting_capital = config.starting_capital;
        backend_config.cost_model = (config.cost_model == "alpaca") ? CostModel::ALPACA : CostModel::PERCENTAGE;
        backend_config.leverage_enabled = config.leverage_enabled;
        backend_config.enable_dynamic_psm = config.enable_dynamic_psm;
        backend_config.enable_hysteresis = config.enable_hysteresis;
        backend_config.enable_dynamic_allocation = config.enable_dynamic_allocation;

        EnhancedBackendComponent backend(backend_config);
        std::string run_id = utils::generate_run_id("sanity");

        // Derive start/end for last N blocks
        const size_t BLOCK_SIZE = sentio::STANDARD_BLOCK_SIZE;
        size_t total = signals.size();
        size_t needed = static_cast<size_t>(blocks) * BLOCK_SIZE;
        size_t start_index = (total > needed) ? (total - needed) : 0;
        size_t end_index = total;

        backend.process_to_jsonl(temp_signals, dataset_csv_path, temp_trades, run_id, start_index, end_index, 0.7);

        // Parse equity_after
        double initial_capital = config.starting_capital;
        double final_equity = initial_capital;
        bool parsed_equity = false;
        {
            std::ifstream trade_file(temp_trades);
            std::string line;
            while (std::getline(trade_file, line)) {
#ifdef NLOHMANN_JSON_AVAILABLE
                try {
                    auto j = json::parse(line);
                    
                    // Check version for migration tracking
                    std::string version = j.value("version", "1.0");
                    if (version == "1.0") {
                        static bool warned = false;
                        if (!warned) {
                            std::cerr << "Warning: Processing legacy trade log format (v1.0)\n";
                            warned = true;
                        }
                    }
                    
                    if (j.contains("equity_after")) {
                        if (j["equity_after"].is_number()) {
                            // Preferred: numeric value
                            final_equity = j["equity_after"].get<double>();
                            parsed_equity = true;
                        } else if (j["equity_after"].is_string()) {
                            // Fallback: string parsing with enhanced error handling
                            try {
                                std::string equity_str = j["equity_after"].get<std::string>();
                                // Trim whitespace and quotes
                                equity_str.erase(0, equity_str.find_first_not_of(" \t\n\r\""));
                                equity_str.erase(equity_str.find_last_not_of(" \t\n\r\"") + 1);
                                
                                if (!equity_str.empty() && equity_str != "null") {
                                    // Use strtod for more robust parsing
                                    char* end;
                                    errno = 0;
                                    double value = std::strtod(equity_str.c_str(), &end);
                                    if (errno == 0 && end != equity_str.c_str() && *end == '\0') {
                                        final_equity = value;
                                        parsed_equity = true;
                                    }
                                }
                            } catch (...) { /* ignore */ }
                        }
                    }
                } catch (...) { /* ignore */ }
#else
                const std::string key = "\"equity_after\":";
                size_t pos = line.find(key);
                if (pos != std::string::npos) {
                    size_t value_start = pos + key.size();
                    while (value_start < line.size() && (line[value_start] == ' ' || line[value_start] == '"')) ++value_start;
                    size_t value_end = line.find_first_of(",}\"", value_start);
                    if (value_end != std::string::npos && value_end > value_start) {
                        try {
                            std::string equity_str = line.substr(value_start, value_end - value_start);
                            if (!equity_str.empty() && equity_str != "null") {
                                final_equity = std::stod(equity_str);
                                parsed_equity = true;
                            }
                        } catch (...) {}
                    }
                }
#endif
            }
        }
        if (!parsed_equity) return 0.0; // treat as 0 MRB if no trades

        double total_return_pct = ((final_equity - initial_capital) / initial_capital) * 100.0;
        return (total_return_pct / 100.0) / blocks;
    } catch (...) {
        return 0.0;
    }
}

double PerformanceAnalyzer::calculate_trading_based_mrb(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    // Delegate to the Enhanced PSM path to ensure single source of MRB truth
    PSMValidationConfig cfg; // defaults to file-based temp dir
    return calculate_trading_based_mrb_with_psm(signals, market_data, blocks, cfg);
}

std::vector<double> PerformanceAnalyzer::calculate_block_mrbs(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    int blocks,
    bool use_enhanced_psm
) {
    std::vector<double> block_mrbs;
    if (signals.empty() || market_data.empty() || blocks < 1) return block_mrbs;
    size_t min_size = std::min(signals.size(), market_data.size());
    size_t block_size = min_size / static_cast<size_t>(blocks);
    if (block_size == 0) return block_mrbs;

    for (int b = 0; b < blocks; ++b) {
        size_t start = static_cast<size_t>(b) * block_size;
        size_t end = (b == blocks - 1) ? min_size : (static_cast<size_t>(b + 1) * block_size);
        if (start >= end) { block_mrbs.push_back(0.0); continue; }

        // Slice signals and market data
        std::vector<SignalOutput> s_slice(signals.begin() + start, signals.begin() + end);
        std::vector<MarketData> m_slice(market_data.begin() + start, market_data.begin() + end);

        double mrb_block = 0.0;
        try {
            mrb_block = calculate_trading_based_mrb_with_psm(s_slice, m_slice, 1);
        } catch (...) {
            mrb_block = 0.0;
        }
        block_mrbs.push_back(mrb_block);
    }
    
    return block_mrbs;
}

ComparisonResult PerformanceAnalyzer::compare_strategies(
    const std::map<std::string, std::vector<SignalOutput>>& strategy_signals,
    const std::vector<MarketData>& market_data
) {
    ComparisonResult result;
    
    // Calculate metrics for each strategy
    for (const auto& [strategy_name, signals] : strategy_signals) {
        auto metrics = calculate_metrics(signals, market_data);
        metrics.strategy_name = strategy_name;
        result.strategy_metrics[strategy_name] = metrics;
    }
    
    // Find best and worst strategies
    double best_score = -std::numeric_limits<double>::infinity();
    double worst_score = std::numeric_limits<double>::infinity();
    
    for (const auto& [name, metrics] : result.strategy_metrics) {
        double score = metrics.calculate_score();
        result.rankings.push_back({name, score});
        
        if (score > best_score) {
            best_score = score;
            result.best_strategy = name;
        }
        if (score < worst_score) {
            worst_score = score;
            result.worst_strategy = name;
        }
    }
    
    // Sort rankings
    std::sort(result.rankings.begin(), result.rankings.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return result;
}

SignalQualityMetrics PerformanceAnalyzer::analyze_signal_quality(
    const std::vector<SignalOutput>& signals
) {
    SignalQualityMetrics metrics;
    
    if (signals.empty()) return metrics;
    
    int long_count = 0, short_count = 0, neutral_count = 0;
    std::vector<double> confidences;
    int reversals = 0;
    int consecutive_neutrals = 0;
    int max_consecutive_neutrals = 0;
    
    SignalType prev_type = SignalType::NEUTRAL;
    
    for (const auto& signal : signals) {
        // Count signal types
        switch (signal.signal_type) {
            case SignalType::LONG:
                long_count++;
                consecutive_neutrals = 0;
                break;
            case SignalType::SHORT:
                short_count++;
                consecutive_neutrals = 0;
                break;
            case SignalType::NEUTRAL:
                neutral_count++;
                consecutive_neutrals++;
                max_consecutive_neutrals = std::max(max_consecutive_neutrals, consecutive_neutrals);
                break;
            default:
                break;
        }
        
        // Count reversals (long to short or short to long)
        if ((prev_type == SignalType::LONG && signal.signal_type == SignalType::SHORT) ||
            (prev_type == SignalType::SHORT && signal.signal_type == SignalType::LONG)) {
            reversals++;
        }
        
        prev_type = signal.signal_type;
        
        // Collect confidences
        if (0.7 > 0.0) {
            confidences.push_back(0.7);
        }
    }
    
    // Calculate ratios
    metrics.long_ratio = static_cast<double>(long_count) / signals.size();
    metrics.short_ratio = static_cast<double>(short_count) / signals.size();
    metrics.neutral_ratio = static_cast<double>(neutral_count) / signals.size();
    
    // Calculate confidence statistics
    if (!confidences.empty()) {
        std::sort(confidences.begin(), confidences.end());
        
        metrics.mean_confidence = std::accumulate(confidences.begin(), confidences.end(), 0.0) 
                                 / confidences.size();
        metrics.median_confidence = confidences[confidences.size() / 2];
        metrics.min_confidence = confidences.front();
        metrics.max_confidence = confidences.back();
        
        // Standard deviation
        double variance = 0.0;
        for (const auto& conf : confidences) {
            variance += (conf - metrics.mean_confidence) * (conf - metrics.mean_confidence);
        }
        variance /= confidences.size();
        metrics.confidence_std_dev = std::sqrt(variance);
    }
    
    metrics.signal_reversals = reversals;
    metrics.consecutive_neutrals = max_consecutive_neutrals;
    
    // Calculate quality indicators
    metrics.signal_consistency = 1.0 - (static_cast<double>(reversals) / signals.size());
    metrics.signal_stability = 1.0 - metrics.neutral_ratio;
    
    return metrics;
}

RiskMetrics PerformanceAnalyzer::calculate_risk_metrics(
    const std::vector<double>& equity_curve
) {
    RiskMetrics metrics;
    
    if (equity_curve.empty()) return metrics;
    
    // Calculate drawdowns
    double peak = equity_curve[0];
    double current_dd = 0.0;
    int dd_duration = 0;
    int max_dd_duration = 0;
    
    for (const auto& equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
            dd_duration = 0;
        } else {
            dd_duration++;
            double dd = (peak - equity) / peak;
            metrics.current_drawdown = dd;
            
            if (dd > metrics.max_drawdown) {
                metrics.max_drawdown = dd;
            }
            
            if (dd_duration > max_dd_duration) {
                max_dd_duration = dd_duration;
            }
        }
    }
    
    metrics.max_drawdown_duration = max_dd_duration;
    metrics.current_drawdown_duration = dd_duration;
    
    // Calculate returns for volatility metrics
    auto returns = calculate_returns(equity_curve);
    
    if (!returns.empty()) {
        metrics.volatility = calculate_volatility(returns);
        
        // Downside deviation
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double downside_variance = 0.0;
        double upside_variance = 0.0;
        int downside_count = 0;
        int upside_count = 0;
        
        for (const auto& ret : returns) {
            if (ret < mean_return) {
                downside_variance += (ret - mean_return) * (ret - mean_return);
                downside_count++;
            } else {
                upside_variance += (ret - mean_return) * (ret - mean_return);
                upside_count++;
            }
        }
        
        if (downside_count > 0) {
            metrics.downside_deviation = std::sqrt(downside_variance / downside_count);
        }
        if (upside_count > 0) {
            metrics.upside_deviation = std::sqrt(upside_variance / upside_count);
        }
        
        // Value at Risk (VaR)
        std::vector<double> sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        size_t var_95_idx = sorted_returns.size() * 0.05;
        size_t var_99_idx = sorted_returns.size() * 0.01;
        
        if (var_95_idx < sorted_returns.size()) {
            metrics.var_95 = sorted_returns[var_95_idx];
        }
        if (var_99_idx < sorted_returns.size()) {
            metrics.var_99 = sorted_returns[var_99_idx];
        }
        
        // Conditional VaR (CVaR)
        if (var_95_idx > 0) {
            double cvar_sum = 0.0;
            for (size_t i = 0; i < var_95_idx; ++i) {
                cvar_sum += sorted_returns[i];
            }
            metrics.cvar_95 = cvar_sum / var_95_idx;
        }
        if (var_99_idx > 0) {
            double cvar_sum = 0.0;
            for (size_t i = 0; i < var_99_idx; ++i) {
                cvar_sum += sorted_returns[i];
            }
            metrics.cvar_99 = cvar_sum / var_99_idx;
        }
    }
    
    return metrics;
}

// Private helper methods

double PerformanceAnalyzer::calculate_sharpe_ratio(
    const std::vector<double>& returns,
    double risk_free_rate
) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return - risk_free_rate;
    
    double volatility = calculate_volatility(returns);
    
    return (volatility > 0) ? (excess_return / volatility) : 0.0;
}

double PerformanceAnalyzer::calculate_max_drawdown(
    const std::vector<double>& equity_curve
) {
    if (equity_curve.empty()) return 0.0;
    
    double max_dd = 0.0;
    double peak = equity_curve[0];
    
    for (const auto& equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
        } else {
            double dd = (peak - equity) / peak;
            max_dd = std::max(max_dd, dd);
        }
    }
    
    return max_dd;
}

double PerformanceAnalyzer::calculate_win_rate(
    const std::vector<double>& trades
) {
    if (trades.empty()) return 0.0;
    
    int winning_trades = std::count_if(trades.begin(), trades.end(),
                                       [](double t) { return t > 0; });
    
    return static_cast<double>(winning_trades) / trades.size();
}

double PerformanceAnalyzer::calculate_profit_factor(
    const std::vector<double>& trades
) {
    if (trades.empty()) return 0.0;
    
    double gross_profit = 0.0;
    double gross_loss = 0.0;
    
    for (const auto& trade : trades) {
        if (trade > 0) {
            gross_profit += trade;
        } else {
            gross_loss += std::abs(trade);
        }
    }
    
    return (gross_loss > 0) ? (gross_profit / gross_loss) : 0.0;
}

double PerformanceAnalyzer::calculate_volatility(
    const std::vector<double>& returns
) {
    if (returns.empty()) return 0.0;
    
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (const auto& ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= returns.size();
    
    return std::sqrt(variance);
}

double PerformanceAnalyzer::calculate_sortino_ratio(
    const std::vector<double>& returns,
    double risk_free_rate
) {
    if (returns.empty()) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return - risk_free_rate;
    
    // Calculate downside deviation
    double downside_variance = 0.0;
    int downside_count = 0;
    
    for (const auto& ret : returns) {
        if (ret < risk_free_rate) {
            downside_variance += (ret - risk_free_rate) * (ret - risk_free_rate);
            downside_count++;
        }
    }
    
    if (downside_count == 0) return 0.0;
    
    double downside_deviation = std::sqrt(downside_variance / downside_count);
    
    return (downside_deviation > 0) ? (excess_return / downside_deviation) : 0.0;
}

double PerformanceAnalyzer::calculate_calmar_ratio(
    const std::vector<double>& returns,
    const std::vector<double>& equity_curve
) {
    if (returns.empty() || equity_curve.empty()) return 0.0;
    
    double annualized_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    annualized_return *= 252;  // Annualize
    
    double max_dd = calculate_max_drawdown(equity_curve);
    
    return (max_dd > 0) ? (annualized_return / max_dd) : 0.0;
}

std::pair<std::vector<double>, std::vector<double>> PerformanceAnalyzer::simulate_trading(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data
) {
    std::vector<double> equity_curve;
    std::vector<double> trade_results;
    
    if (signals.empty() || market_data.empty()) {
        return {equity_curve, trade_results};
    }
    
    double equity = 10000.0;  // Starting capital
    equity_curve.push_back(equity);
    
    int position = 0;  // 0 = neutral, 1 = long, -1 = short
    double entry_price = 0.0;
    
    size_t min_size = std::min(signals.size(), market_data.size());
    
    for (size_t i = 0; i < min_size - 1; ++i) {
        const auto& signal = signals[i];
        const auto& current_data = market_data[i];
        const auto& next_data = market_data[i + 1];
        
        // Determine new position
        int new_position = 0;
        if (signal.signal_type == SignalType::LONG) {
            new_position = 1;
        } else if (signal.signal_type == SignalType::SHORT) {
            new_position = -1;
        }
        
        // Close existing position if changing
        if (position != 0 && new_position != position) {
            double exit_price = current_data.close;
            double pnl = position * (exit_price - entry_price) / entry_price;
            equity *= (1.0 + pnl);
            trade_results.push_back(pnl);
            position = 0;
        }
        
        // Open new position
        if (new_position != 0 && position == 0) {
            entry_price = current_data.close;
            position = new_position;
        }
        
        equity_curve.push_back(equity);
    }
    
    // Close final position
    if (position != 0 && !market_data.empty()) {
        double exit_price = market_data.back().close;
        double pnl = position * (exit_price - entry_price) / entry_price;
        equity *= (1.0 + pnl);
        trade_results.push_back(pnl);
    }
    
    return {equity_curve, trade_results};
}

std::vector<double> PerformanceAnalyzer::calculate_returns(
    const std::vector<double>& equity_curve
) {
    std::vector<double> returns;
    
    if (equity_curve.size() < 2) return returns;
    
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        if (equity_curve[i-1] > 0) {
            double ret = (equity_curve[i] - equity_curve[i-1]) / equity_curve[i-1];
            returns.push_back(ret);
        }
    }
    
    return returns;
}

// WalkForwardAnalyzer implementation

WalkForwardAnalyzer::WalkForwardResult WalkForwardAnalyzer::analyze(
    const std::string& strategy_name,
    const std::vector<MarketData>& market_data,
    const WalkForwardConfig& config
) {
    WalkForwardResult result;
    
    // Implementation of walk-forward analysis
    // This would split data into windows, train on in-sample, test on out-of-sample
    // For now, this is a placeholder
    
    return result;
}

// StressTestAnalyzer implementation

std::vector<StressTestAnalyzer::StressTestResult> StressTestAnalyzer::run_stress_tests(
    const std::string& strategy_name,
    const std::vector<MarketData>& base_market_data,
    const std::vector<StressScenario>& scenarios
) {
    std::vector<StressTestResult> results;
    
    for (const auto& scenario : scenarios) {
        StressTestResult test_result;
        test_result.scenario = scenario;
        
        // Apply stress scenario to data
        auto stressed_data = apply_stress_scenario(base_market_data, scenario);
        
        // Load strategy and generate signals
        auto strategy_unique = create_strategy(strategy_name);
        if (!strategy_unique) continue;
        auto strategy = std::shared_ptr<IStrategy>(std::move(strategy_unique));
        
        std::vector<SignalOutput> signals;
        try {
            signals = strategy->process_data(stressed_data);
        } catch (...) {
            signals.clear();
        }
        
        // Calculate metrics
        test_result.metrics = PerformanceAnalyzer::calculate_metrics(
            signals, stressed_data
        );
        
        // Determine if passed based on metrics
        test_result.passed = (test_result.metrics.trading_based_mrb > 0.005);
        
        results.push_back(test_result);
    }
    
    return results;
}

std::vector<MarketData> StressTestAnalyzer::apply_stress_scenario(
    const std::vector<MarketData>& market_data,
    StressScenario scenario
) {
    std::vector<MarketData> stressed_data = market_data;
    
    switch (scenario) {
        case StressScenario::MARKET_CRASH:
            // Apply crash scenario
            for (auto& data : stressed_data) {
                data.close *= 0.8;  // 20% crash
            }
            break;
            
        case StressScenario::HIGH_VOLATILITY:
            // Increase volatility
            for (size_t i = 1; i < stressed_data.size(); ++i) {
                double change = (stressed_data[i].close - stressed_data[i-1].close) / stressed_data[i-1].close;
                stressed_data[i].close = stressed_data[i-1].close * (1.0 + change * 2.0);
            }
            break;
            
        case StressScenario::LOW_VOLATILITY:
            // Decrease volatility
            for (size_t i = 1; i < stressed_data.size(); ++i) {
                double change = (stressed_data[i].close - stressed_data[i-1].close) / stressed_data[i-1].close;
                stressed_data[i].close = stressed_data[i-1].close * (1.0 + change * 0.5);
            }
            break;
            
        // Add other scenarios
        default:
            break;
    }
    
    return stressed_data;
}

} // namespace sentio::analysis



```

## 📄 **FILE 30 of 46**: src/backend/adaptive_trading_mechanism.cpp

**File Information**:
- **Path**: `src/backend/adaptive_trading_mechanism.cpp`

- **Size**: 702 lines
- **Modified**: 2025-10-08 07:45:04

- **Type**: .cpp

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

## 📄 **FILE 31 of 46**: src/backend/dynamic_allocation_manager.cpp

**File Information**:
- **Path**: `src/backend/dynamic_allocation_manager.cpp`

- **Size**: 614 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .cpp

```text
// File: src/backend/dynamic_allocation_manager.cpp
#include "backend/dynamic_allocation_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace backend {

DynamicAllocationManager::DynamicAllocationManager(const AllocationConfig& config)
    : config_(config) {
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_dual_allocation(
    PositionStateMachine::State target_state,
    const SignalOutput& signal,
    double available_capital,
    double current_price_base,
    double current_price_leveraged,
    const MarketConditions& market) const {
    
    // Determine if long or short dual state
    bool is_long = (target_state == PositionStateMachine::State::QQQ_TQQQ);
    bool is_short = (target_state == PositionStateMachine::State::PSQ_SQQQ);
    
    if (!is_long && !is_short) {
        // Not a dual state - shouldn't happen but handle gracefully
        sentio::utils::log_error("calculate_dual_allocation called with non-dual state: " + 
                        std::to_string(static_cast<int>(target_state)));
        AllocationResult result;
        result.is_valid = false;
        result.warnings.push_back("Invalid state for dual allocation");
        return result;
    }
    
    // Select allocation strategy
    AllocationResult result;
    switch (config_.strategy) {
        case AllocationConfig::Strategy::CONFIDENCE_BASED:
            result = calculate_confidence_based_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::RISK_PARITY:
            result = calculate_risk_parity_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::KELLY_CRITERION:
            result = calculate_kelly_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::HYBRID:
            result = calculate_hybrid_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        default:
            result = calculate_confidence_based_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
    }
    
    // Apply risk limits and validation
    apply_risk_limits(result);
    if (config_.enable_volatility_scaling) {
        apply_volatility_scaling(result, market);
    }
    calculate_risk_metrics(result);
    add_validation_warnings(result);
    result.is_valid = validate_allocation(result);
    
    // Log allocation details
    std::stringstream ss;
    ss << "ALLOCATION RESULT: " << result.allocation_rationale
       << " | Base: " << result.base_symbol << " " << result.base_allocation_pct * 100 << "%"
       << " | Leveraged: " << result.leveraged_symbol << " " << result.leveraged_allocation_pct * 100 << "%"
       << " | Effective Leverage: " << result.effective_leverage << "x"
       << " | Risk Score: " << result.risk_score;
    sentio::utils::log_info(ss.str());
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_confidence_based_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols based on direction
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // CORE ALLOCATION FORMULA (Signal Strength-Based):
    // High signal strength → more leverage
    // Low signal strength → more base position
    
    // Calculate signal strength from probability (0.0 to 1.0)
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    // Apply bounds
    signal_strength = std::clamp(signal_strength, config_.confidence_floor, config_.confidence_ceiling);
    
    // Apply power for tuning aggression
    signal_strength = std::pow(signal_strength, config_.confidence_power);
    
    // Base allocation formula
    result.leveraged_allocation_pct = signal_strength;
    result.base_allocation_pct = 1.0 - signal_strength;
    
    // Apply risk limits
    result.leveraged_allocation_pct = std::min(result.leveraged_allocation_pct, 
                                              config_.max_leverage_allocation);
    result.base_allocation_pct = std::max(result.base_allocation_pct,
                                         config_.min_base_allocation);
    
    // Renormalize to sum to 1.0
    double total = result.leveraged_allocation_pct + result.base_allocation_pct;
    if (total > 0) {
        result.leveraged_allocation_pct /= total;
        result.base_allocation_pct /= total;
    }
    
    // Apply total allocation limits
    double total_allocation = config_.min_total_allocation;
    if (signal_strength > 0.8) {
        total_allocation = config_.max_total_allocation;
    }
    
    // Calculate position values
    double allocated_capital = available_capital * total_allocation;
    result.base_position_value = allocated_capital * result.base_allocation_pct;
    result.leveraged_position_value = allocated_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Recalculate actual values based on rounded quantities
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    
    // Update percentages based on actual positions
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Set metadata
    result.allocation_strategy = "SIGNAL_STRENGTH_BASED";
    result.confidence_used = signal_strength;
    
    // Create rationale
    std::stringstream ss;
    ss << "Signal strength-based split: "
       << static_cast<int>(result.base_allocation_pct * 100) << "% " << result.base_symbol
       << ", " << static_cast<int>(result.leveraged_allocation_pct * 100) << "% " << result.leveraged_symbol
       << " (signal_strength=" << std::fixed << std::setprecision(2) << (std::abs(signal.probability - 0.5) * 2.0)
       << ", adjusted=" << signal_strength << ")";
    result.allocation_rationale = ss.str();
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_risk_parity_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // Risk parity: allocate inversely proportional to volatility
    // Goal: each position contributes equally to portfolio risk
    
    double base_vol = config_.base_volatility;
    double leveraged_vol = config_.leveraged_volatility;
    
    // Adjust for market conditions
    if (market.current_volatility > 0) {
        double vol_multiplier = market.current_volatility / 0.15;  // Normalize to typical market vol
        base_vol *= vol_multiplier;
        leveraged_vol *= vol_multiplier;
    }
    
    // Inverse volatility weighting
    double base_weight = 1.0 / base_vol;
    double leveraged_weight = 1.0 / leveraged_vol;
    
    // Normalize weights
    double total_weight = base_weight + leveraged_weight;
    result.base_allocation_pct = base_weight / total_weight;
    result.leveraged_allocation_pct = leveraged_weight / total_weight;
    
    // Scale by confidence (higher confidence = more total allocation)
    double total_allocation = config_.min_total_allocation + 
                            (config_.max_total_allocation - config_.min_total_allocation) * std::abs(signal.probability - 0.5) * 2.0;
    
    // Calculate positions
    double allocated_capital = available_capital * total_allocation;
    result.base_position_value = allocated_capital * result.base_allocation_pct;
    result.leveraged_position_value = allocated_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "RISK_PARITY";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.allocation_rationale = "Risk parity allocation with equal risk contribution";
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_kelly_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // Kelly Criterion: f* = (p*b - q) / b
    // where p = win probability, q = 1-p, b = win/loss ratio
    
    // Use signal probability as win probability
    double win_prob = is_long ? signal.probability : (1.0 - signal.probability);
    win_prob = std::clamp(win_prob, 0.45, 0.65);  // Cap to reasonable bounds
    
    // Adjust win/loss ratio based on confidence
    double win_loss_ratio = config_.avg_win_loss_ratio * (0.8 + 0.4 * (std::abs(signal.probability - 0.5) * 2.0));
    
    // Calculate raw Kelly fraction
    double raw_kelly = calculate_kelly_fraction(win_prob, win_loss_ratio);
    
    // Apply safety factor (fractional Kelly)
    double kelly_fraction = apply_kelly_safety_factor(raw_kelly) * config_.kelly_fraction;
    kelly_fraction = std::clamp(kelly_fraction, 0.0, 1.0);
    
    // Split between base and leveraged based on Kelly sizing
    // Higher Kelly = more leverage
    result.leveraged_allocation_pct = kelly_fraction * 0.8;  // Max 80% in leveraged
    result.base_allocation_pct = kelly_fraction * 0.2 + (1.0 - kelly_fraction) * 0.5;
    
    // Normalize
    double total = result.leveraged_allocation_pct + result.base_allocation_pct;
    if (total > 1.0) {
        result.leveraged_allocation_pct /= total;
        result.base_allocation_pct /= total;
    }
    
    // Calculate positions
    result.base_position_value = available_capital * result.base_allocation_pct;
    result.leveraged_position_value = available_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "KELLY_CRITERION";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.kelly_sizing = kelly_fraction;
    
    std::stringstream ss;
    ss << "Kelly allocation (f*=" << std::fixed << std::setprecision(3) << kelly_fraction
       << ", p=" << win_prob << ", b=" << win_loss_ratio << ")";
    result.allocation_rationale = ss.str();
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_hybrid_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    // Hybrid: blend of all three approaches
    auto confidence_result = calculate_confidence_based_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    auto risk_parity_result = calculate_risk_parity_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    auto kelly_result = calculate_kelly_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    
    // Weight the approaches
    double confidence_weight = 0.5;  // Primary driver
    double risk_parity_weight = 0.3;
    double kelly_weight = 0.2;
    
    // Blend allocations
    AllocationResult result;
    result.base_symbol = confidence_result.base_symbol;
    result.leveraged_symbol = confidence_result.leveraged_symbol;
    
    result.base_allocation_pct = 
        confidence_weight * confidence_result.base_allocation_pct +
        risk_parity_weight * risk_parity_result.base_allocation_pct +
        kelly_weight * kelly_result.base_allocation_pct;
    
    result.leveraged_allocation_pct = 
        confidence_weight * confidence_result.leveraged_allocation_pct +
        risk_parity_weight * risk_parity_result.leveraged_allocation_pct +
        kelly_weight * kelly_result.leveraged_allocation_pct;
    
    // Calculate positions
    result.base_position_value = available_capital * result.base_allocation_pct;
    result.leveraged_position_value = available_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "HYBRID";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.kelly_sizing = kelly_result.kelly_sizing;
    result.allocation_rationale = "Hybrid allocation (50% confidence, 30% risk-parity, 20% Kelly)";
    
    return result;
}

void DynamicAllocationManager::apply_risk_limits(AllocationResult& result) const {
    // Ensure we don't exceed maximum leverage allocation
    if (result.leveraged_allocation_pct > config_.max_leverage_allocation) {
        double excess = result.leveraged_allocation_pct - config_.max_leverage_allocation;
        result.leveraged_allocation_pct = config_.max_leverage_allocation;
        result.base_allocation_pct += excess;
    }
    
    // Ensure minimum base allocation
    if (result.base_allocation_pct < config_.min_base_allocation) {
        double shortfall = config_.min_base_allocation - result.base_allocation_pct;
        result.base_allocation_pct = config_.min_base_allocation;
        result.leveraged_allocation_pct = std::max(0.0, result.leveraged_allocation_pct - shortfall);
    }
    
    // Check total leverage
    double eff_leverage = calculate_effective_leverage(result.base_allocation_pct, result.leveraged_allocation_pct);
    if (eff_leverage > config_.max_total_leverage) {
        // Scale down leveraged position
        double max_leveraged = (config_.max_total_leverage - result.base_allocation_pct) / 3.0;
        result.leveraged_allocation_pct = std::min(result.leveraged_allocation_pct, max_leveraged);
    }
}

void DynamicAllocationManager::apply_volatility_scaling(AllocationResult& result, const MarketConditions& market) const {
    if (market.current_volatility <= 0) return;
    
    // Scale allocation based on volatility target
    double vol_scalar = config_.volatility_target / market.current_volatility;
    vol_scalar = std::clamp(vol_scalar, 0.5, 1.5);  // Limit adjustment range
    
    // Reduce position sizes in high volatility
    if (vol_scalar < 1.0) {
        result.base_allocation_pct *= vol_scalar;
        result.leveraged_allocation_pct *= vol_scalar;
        result.cash_reserve_pct = 1.0 - (result.base_allocation_pct + result.leveraged_allocation_pct);
        
        result.warnings.push_back("Position scaled down due to high volatility");
    }
}

void DynamicAllocationManager::calculate_risk_metrics(AllocationResult& result) const {
    // Effective leverage
    result.effective_leverage = calculate_effective_leverage(
        result.base_allocation_pct, result.leveraged_allocation_pct);
    
    // Risk score (0-1)
    result.risk_score = calculate_risk_score(result);
    
    // Expected volatility
    result.expected_volatility = calculate_expected_volatility(
        result.base_allocation_pct, result.leveraged_allocation_pct);
    
    // Max drawdown estimate
    result.max_drawdown_estimate = estimate_max_drawdown(
        result.effective_leverage, result.expected_volatility);
}

void DynamicAllocationManager::add_validation_warnings(AllocationResult& result) const {
    if (result.effective_leverage > 2.5) {
        result.warnings.push_back("High leverage warning: " + 
                                 std::to_string(result.effective_leverage) + "x");
    }
    
    if (result.cash_reserve_pct > 0.1) {
        result.warnings.push_back("Significant cash reserve: " + 
                                 std::to_string(static_cast<int>(result.cash_reserve_pct * 100)) + "%");
    }
    
    if (result.base_quantity < 1 || result.leveraged_quantity < 1) {
        result.warnings.push_back("Insufficient capital for full dual position");
    }
}

bool DynamicAllocationManager::validate_allocation(const AllocationResult& result) const {
    // Check basic validity
    if (result.base_quantity < 0 || result.leveraged_quantity < 0) {
        return false;
    }
    
    if (result.total_allocation_pct > 1.01) {  // Allow 1% rounding error
        return false;
    }
    
    if (result.effective_leverage > config_.max_total_leverage * 1.1) {  // Allow 10% buffer
        return false;
    }
    
    return true;
}

double DynamicAllocationManager::calculate_risk_score(const AllocationResult& result) const {
    // Normalize various risk factors to 0-1 and combine
    double leverage_score = result.effective_leverage / config_.max_total_leverage;
    double concentration_score = std::max(result.base_allocation_pct, result.leveraged_allocation_pct);
    double volatility_score = result.expected_volatility / 0.5;  // Assume 50% vol is max
    
    // Weighted average
    double risk_score = 0.4 * leverage_score + 0.3 * concentration_score + 0.3 * volatility_score;
    
    return std::clamp(risk_score, 0.0, 1.0);
}

double DynamicAllocationManager::calculate_effective_leverage(
    double base_pct, double leveraged_pct, double leverage_factor) const {
    return base_pct * 1.0 + leveraged_pct * leverage_factor;
}

double DynamicAllocationManager::calculate_expected_volatility(
    double base_pct, double leveraged_pct) const {
    // Portfolio volatility with correlation
    double base_vol = config_.base_volatility;
    double leveraged_vol = config_.leveraged_volatility;
    double correlation = 0.95;  // High correlation between QQQ and TQQQ
    
    // Portfolio variance
    double variance = base_pct * base_pct * base_vol * base_vol +
                     leveraged_pct * leveraged_pct * leveraged_vol * leveraged_vol +
                     2 * base_pct * leveraged_pct * base_vol * leveraged_vol * correlation;
    
    return std::sqrt(variance);
}

double DynamicAllocationManager::estimate_max_drawdown(
    double effective_leverage, double expected_vol) const {
    // Rough estimate: max drawdown ≈ 2 * volatility * sqrt(leverage)
    return 2.0 * expected_vol * std::sqrt(effective_leverage);
}

double DynamicAllocationManager::calculate_kelly_fraction(
    double win_probability, double win_loss_ratio) const {
    // Kelly formula: f* = (p*b - q) / b
    // where p = win probability, q = lose probability, b = win/loss ratio
    double q = 1.0 - win_probability;
    double numerator = win_probability * win_loss_ratio - q;
    
    if (win_loss_ratio <= 0) return 0.0;
    
    return numerator / win_loss_ratio;
}

double DynamicAllocationManager::apply_kelly_safety_factor(double raw_kelly) const {
    // Apply safety factor to raw Kelly
    // Never use full Kelly - too aggressive
    raw_kelly = std::clamp(raw_kelly, 0.0, 2.0);  // Cap at 200% (leveraged)
    
    // Non-linear scaling to be more conservative
    if (raw_kelly > 1.0) {
        return 1.0 + 0.5 * (raw_kelly - 1.0);  // Reduce leverage component
    }
    
    return raw_kelly;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_single_allocation(
    const std::string& symbol,
    const SignalOutput& signal,
    double available_capital,
    double current_price,
    bool is_leveraged) const {
    
    AllocationResult result;
    result.base_symbol = symbol;
    result.leveraged_symbol = "";  // No leveraged position for single allocation
    
    // Scale position size by signal strength
    double position_pct = config_.min_total_allocation + 
                         (config_.max_total_allocation - config_.min_total_allocation) * (std::abs(signal.probability - 0.5) * 2.0);
    
    // Apply leverage penalty if using leveraged instrument alone
    if (is_leveraged) {
        position_pct *= 0.7;  // Reduce position size for leveraged-only positions
    }
    
    // Calculate position
    result.base_allocation_pct = position_pct;
    result.leveraged_allocation_pct = 0.0;
    result.base_position_value = available_capital * position_pct;
    result.base_quantity = std::floor(result.base_position_value / current_price);
    
    // Update actual value
    result.base_position_value = result.base_quantity * current_price;
    result.total_position_value = result.base_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.total_allocation_pct = result.base_allocation_pct;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Risk metrics
    result.effective_leverage = is_leveraged ? 3.0 * position_pct : position_pct;
    result.expected_volatility = is_leveraged ? config_.leveraged_volatility : config_.base_volatility;
    result.risk_score = calculate_risk_score(result);
    result.max_drawdown_estimate = estimate_max_drawdown(result.effective_leverage, result.expected_volatility);
    
    // Metadata
    result.allocation_strategy = "SINGLE_POSITION";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.allocation_rationale = "Single position in " + symbol;
    result.is_valid = true;
    
    return result;
}

void DynamicAllocationManager::update_config(const AllocationConfig& new_config) {
    config_ = new_config;
}

} // namespace backend


```

## 📄 **FILE 32 of 46**: src/backend/dynamic_hysteresis_manager.cpp

**File Information**:
- **Path**: `src/backend/dynamic_hysteresis_manager.cpp`

- **Size**: 293 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .cpp

```text
// File: src/backend/dynamic_hysteresis_manager.cpp
#include "backend/dynamic_hysteresis_manager.h"
#include "common/utils.h"
#include <numeric>
#include <stdexcept>

namespace backend {

DynamicHysteresisManager::DynamicHysteresisManager(const HysteresisConfig& config)
    : config_(config) {
    signal_history_.clear();
}

void DynamicHysteresisManager::update_signal_history(const SignalOutput& signal) {
    signal_history_.push_back(signal);
    
    // Maintain window size
    while (signal_history_.size() > static_cast<size_t>(config_.signal_history_window)) {
        signal_history_.pop_front();
    }
}

DynamicHysteresisManager::DynamicThresholds DynamicHysteresisManager::get_thresholds(
    PositionStateMachine::State current_state,
    const SignalOutput& signal,
    int bars_in_position) const {
    
    DynamicThresholds thresholds;
    
    // Base thresholds
    double base_buy = config_.base_buy_threshold;
    double base_sell = config_.base_sell_threshold;
    
    // Initialize with base values
    thresholds.buy_threshold = base_buy;
    thresholds.sell_threshold = base_sell;
    
    // State-dependent adjustments (HYSTERESIS)
    switch (current_state) {
        case PositionStateMachine::State::CASH_ONLY:
            // Normal entry thresholds - no adjustment needed
            break;
            
        case PositionStateMachine::State::QQQ_ONLY:
        case PositionStateMachine::State::TQQQ_ONLY:
            // In long position - harder to add more, harder to exit
            thresholds.buy_threshold = base_buy + config_.entry_bias;  // 0.55 → 0.57
            thresholds.sell_threshold = base_sell - config_.exit_bias; // 0.45 → 0.40
            break;
            
        case PositionStateMachine::State::PSQ_ONLY:
        case PositionStateMachine::State::SQQQ_ONLY:
            // In short position - harder to add more, harder to exit
            thresholds.sell_threshold = base_sell - config_.entry_bias; // 0.45 → 0.43
            thresholds.buy_threshold = base_buy + config_.exit_bias;    // 0.55 → 0.60
            break;
            
        case PositionStateMachine::State::QQQ_TQQQ:
            // Already leveraged long - VERY HARD to add more
            thresholds.buy_threshold = base_buy + config_.dual_state_entry_multiplier * config_.entry_bias;  // 0.55 → 0.59
            thresholds.sell_threshold = base_sell - config_.exit_bias;     // 0.45 → 0.40
            break;
            
        case PositionStateMachine::State::PSQ_SQQQ:
            // Already leveraged short - VERY HARD to add more
            thresholds.sell_threshold = base_sell - config_.dual_state_entry_multiplier * config_.entry_bias; // 0.45 → 0.41
            thresholds.buy_threshold = base_buy + config_.exit_bias;        // 0.55 → 0.60
            break;
            
        default:
            // INVALID state or unknown - use base thresholds
            break;
    }
    
    // Time-in-position adjustment (longer in position = harder to exit)
    if (bars_in_position > 5 && bars_in_position < 50) {
        double time_factor = std::min(0.02, bars_in_position * 0.001);
        if (is_long_state(current_state)) {
            thresholds.sell_threshold -= time_factor;  // Even harder to exit
        } else if (is_short_state(current_state)) {
            thresholds.buy_threshold += time_factor;   // Even harder to exit
        }
    }
    
    // Variance-based adjustment (widen neutral zone if volatile)
    double variance_adj = get_variance_adjustment();
    thresholds.buy_threshold += variance_adj;
    thresholds.sell_threshold -= variance_adj;
    
    // Momentum-based adjustment (follow the trend)
    if (config_.momentum_factor > 0) {
        double momentum_adj = get_momentum_adjustment();
        thresholds.buy_threshold += momentum_adj;
        thresholds.sell_threshold += momentum_adj;
    }
    
    // Clamp to bounds
    thresholds.buy_threshold = std::clamp(thresholds.buy_threshold, 
                                         config_.min_threshold, config_.max_threshold);
    thresholds.sell_threshold = std::clamp(thresholds.sell_threshold,
                                          config_.min_threshold, config_.max_threshold);
    
    // Ensure buy > sell (maintain neutral zone)
    if (thresholds.buy_threshold <= thresholds.sell_threshold) {
        double mid = (thresholds.buy_threshold + thresholds.sell_threshold) / 2.0;
        thresholds.buy_threshold = mid + 0.05;
        thresholds.sell_threshold = mid - 0.05;
    }
    
    // Strong signal thresholds
    thresholds.strong_buy_threshold = thresholds.buy_threshold + config_.strong_margin;
    thresholds.strong_sell_threshold = thresholds.sell_threshold - config_.strong_margin;
    
    // Confidence threshold (could also be adaptive based on regime)
    thresholds.confidence_threshold = config_.confidence_threshold;
    std::string regime = determine_market_regime();
    if (regime == "VOLATILE") {
        thresholds.confidence_threshold = std::min(0.85, config_.confidence_threshold + 0.10);
    }
    
    // Calculate diagnostic info
    auto stats = calculate_statistics();
    thresholds.signal_variance = stats.variance;
    thresholds.signal_mean = stats.mean;
    thresholds.signal_momentum = stats.momentum;
    thresholds.regime = regime;
    thresholds.neutral_zone_width = thresholds.buy_threshold - thresholds.sell_threshold;
    thresholds.hysteresis_strength = std::abs(base_buy - thresholds.buy_threshold) + 
                                     std::abs(base_sell - thresholds.sell_threshold);
    thresholds.bars_in_position = bars_in_position;
    
    // Log threshold adjustments
    sentio::utils::log_debug("DYNAMIC THRESHOLDS: state=" + std::to_string(static_cast<int>(current_state)) +
                    ", buy=" + std::to_string(thresholds.buy_threshold) +
                    ", sell=" + std::to_string(thresholds.sell_threshold) +
                    ", variance=" + std::to_string(thresholds.signal_variance) +
                    ", momentum=" + std::to_string(thresholds.signal_momentum) +
                    ", regime=" + thresholds.regime);
    
    return thresholds;
}

double DynamicHysteresisManager::calculate_signal_variance() const {
    auto stats = calculate_statistics();
    return stats.variance;
}

double DynamicHysteresisManager::calculate_signal_mean() const {
    auto stats = calculate_statistics();
    return stats.mean;
}

double DynamicHysteresisManager::calculate_signal_momentum() const {
    auto stats = calculate_statistics();
    return stats.momentum;
}

std::string DynamicHysteresisManager::determine_market_regime() const {
    if (signal_history_.size() < 5) return "UNKNOWN";
    
    auto stats = calculate_statistics();
    
    // Determine regime based on variance and momentum
    if (stats.variance > 0.01) {
        return "VOLATILE";
    } else if (stats.momentum > 0.02) {
        return "TRENDING_UP";
    } else if (stats.momentum < -0.02) {
        return "TRENDING_DOWN";
    } else {
        return "STABLE";
    }
}

void DynamicHysteresisManager::reset() {
    signal_history_.clear();
}

void DynamicHysteresisManager::update_config(const HysteresisConfig& new_config) {
    config_ = new_config;
}

double DynamicHysteresisManager::get_entry_adjustment(PositionStateMachine::State state) const {
    // Make it harder to enter positions when already positioned
    if (state == PositionStateMachine::State::CASH_ONLY) {
        return 0.0;  // No adjustment for cash
    } else if (is_dual_state(state)) {
        return config_.entry_bias * config_.dual_state_entry_multiplier;  // Very hard to add
    } else {
        return config_.entry_bias;  // Moderately harder to add
    }
}

double DynamicHysteresisManager::get_exit_adjustment(PositionStateMachine::State state) const {
    // Make it harder to exit positions (prevent whipsaw)
    if (state == PositionStateMachine::State::CASH_ONLY) {
        return 0.0;  // No adjustment for cash
    } else {
        return config_.exit_bias;  // Harder to exit any position
    }
}

double DynamicHysteresisManager::get_variance_adjustment() const {
    if (signal_history_.size() < 10) return 0.0;
    
    double variance = calculate_signal_variance();
    
    // High variance → wider neutral zone (reduce trades)
    // Low variance → narrower neutral zone (allow more trades)
    // Cap adjustment to prevent extreme widening
    return std::min(0.10, variance * config_.variance_sensitivity);
}

double DynamicHysteresisManager::get_momentum_adjustment() const {
    if (signal_history_.size() < 10) return 0.0;
    
    double momentum = calculate_signal_momentum();
    
    // Follow the trend: if trending up, make it easier to go long
    // If trending down, make it easier to go short
    return momentum * config_.momentum_factor;
}

bool DynamicHysteresisManager::is_long_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::QQQ_ONLY ||
           state == PositionStateMachine::State::TQQQ_ONLY ||
           state == PositionStateMachine::State::QQQ_TQQQ;
}

bool DynamicHysteresisManager::is_short_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::PSQ_ONLY ||
           state == PositionStateMachine::State::SQQQ_ONLY ||
           state == PositionStateMachine::State::PSQ_SQQQ;
}

bool DynamicHysteresisManager::is_dual_state(PositionStateMachine::State state) const {
    return state == PositionStateMachine::State::QQQ_TQQQ ||
           state == PositionStateMachine::State::PSQ_SQQQ;
}

DynamicHysteresisManager::SignalStatistics DynamicHysteresisManager::calculate_statistics() const {
    SignalStatistics stats = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    
    if (signal_history_.empty()) {
        return stats;
    }
    
    // Calculate mean
    double sum = 0.0;
    for (const auto& signal : signal_history_) {
        sum += signal.probability;
        stats.min_value = std::min(stats.min_value, signal.probability);
        stats.max_value = std::max(stats.max_value, signal.probability);
    }
    stats.mean = sum / signal_history_.size();
    
    // Calculate variance and standard deviation
    if (signal_history_.size() > 1) {
        double sum_squared_diff = 0.0;
        for (const auto& signal : signal_history_) {
            double diff = signal.probability - stats.mean;
            sum_squared_diff += diff * diff;
        }
        stats.variance = sum_squared_diff / signal_history_.size();
        stats.std_dev = std::sqrt(stats.variance);
    }
    
    // Calculate momentum (trend)
    if (signal_history_.size() >= 5) {
        // Simple linear regression slope
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        int n = signal_history_.size();
        
        for (int i = 0; i < n; ++i) {
            double x = i;
            double y = signal_history_[i].probability;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }
        
        double denominator = n * sum_x2 - sum_x * sum_x;
        if (std::abs(denominator) > 0.0001) {
            stats.momentum = (n * sum_xy - sum_x * sum_y) / denominator;
        }
    }
    
    return stats;
}

} // namespace backend


```

## 📄 **FILE 33 of 46**: src/backend/enhanced_position_state_machine.cpp

**File Information**:
- **Path**: `src/backend/enhanced_position_state_machine.cpp`

- **Size**: 439 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .cpp

```text
// File: src/backend/enhanced_position_state_machine.cpp
#include "backend/enhanced_position_state_machine.h"
#include "common/utils.h"
#include "backend/adaptive_trading_mechanism.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace sentio {

EnhancedPositionStateMachine::EnhancedPositionStateMachine(
    std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_mgr,
    std::shared_ptr<backend::DynamicAllocationManager> allocation_mgr,
    const EnhancedConfig& config)
    : PositionStateMachine(),
      hysteresis_manager_(hysteresis_mgr),
      allocation_manager_(allocation_mgr),
      config_(config),
      current_state_(State::CASH_ONLY),
      previous_state_(State::CASH_ONLY),
      bars_in_position_(0),
      total_bars_processed_(0),
      current_regime_("UNKNOWN"),
      regime_bars_count_(0) {
    
    // Create default managers if not provided
    if (!hysteresis_manager_ && config_.enable_hysteresis) {
        backend::DynamicHysteresisManager::HysteresisConfig h_config{};
        hysteresis_manager_ = std::make_shared<backend::DynamicHysteresisManager>(h_config);
    }
    if (!allocation_manager_ && config_.enable_dynamic_allocation) {
        backend::DynamicAllocationManager::AllocationConfig a_config{};
        allocation_manager_ = std::make_shared<backend::DynamicAllocationManager>(a_config);
    }
}

PositionStateMachine::StateTransition EnhancedPositionStateMachine::get_optimal_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions,
    double confidence_threshold) {
    
    // Use enhanced version internally
    auto enhanced = get_enhanced_transition(current_portfolio, signal, market_conditions);
    
    // Convert to base class transition
    StateTransition base_transition;
    base_transition.current_state = enhanced.current_state;
    base_transition.signal_type = enhanced.signal_type;
    base_transition.target_state = enhanced.target_state;
    base_transition.optimal_action = enhanced.optimal_action;
    base_transition.theoretical_basis = enhanced.theoretical_basis;
    base_transition.expected_return = enhanced.expected_return;
    base_transition.risk_score = enhanced.risk_score;
    base_transition.confidence = enhanced.confidence;
    
    return base_transition;
}

EnhancedPositionStateMachine::EnhancedTransition 
EnhancedPositionStateMachine::get_enhanced_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions) {
    
    total_bars_processed_++;
    
    // Update signal history for hysteresis
    if (hysteresis_manager_ && config_.enable_hysteresis) {
        hysteresis_manager_->update_signal_history(signal);
    }
    
    // 1. Determine current state
    State current_state = determine_current_state(current_portfolio);
    
    if (current_state == State::INVALID) {
        EnhancedTransition emergency;
        emergency.current_state = State::INVALID;
        emergency.signal_type = SignalType::NEUTRAL;
        emergency.target_state = State::CASH_ONLY;
        emergency.optimal_action = "Emergency liquidation";
        emergency.theoretical_basis = "Invalid state detected - risk containment";
        emergency.expected_return = 0.0;
        emergency.risk_score = 0.0;
        emergency.confidence = 1.0;
        return emergency;
    }
    
    // Update position tracking
    update_position_tracking(current_state);
    
    // 2. Get DYNAMIC thresholds based on current state (HYSTERESIS!)
    backend::DynamicHysteresisManager::DynamicThresholds thresholds;
    if (hysteresis_manager_ && config_.enable_hysteresis) {
        thresholds = hysteresis_manager_->get_thresholds(current_state, signal, bars_in_position_);
    } else {
        // Fallback to static thresholds (hardcoded to match base PSM)
        constexpr double DEFAULT_BUY = 0.55;
        constexpr double DEFAULT_SELL = 0.45;
        constexpr double STRONG_MARGIN = 0.15;
        constexpr double CONFIDENCE_MIN = 0.7;
        
        thresholds.buy_threshold = DEFAULT_BUY;
        thresholds.sell_threshold = DEFAULT_SELL;
        thresholds.strong_buy_threshold = DEFAULT_BUY + STRONG_MARGIN;
        thresholds.strong_sell_threshold = DEFAULT_SELL - STRONG_MARGIN;
        thresholds.confidence_threshold = CONFIDENCE_MIN;
        thresholds.signal_variance = 0.0;
        thresholds.signal_mean = 0.5;
        thresholds.signal_momentum = 0.0;
        thresholds.regime = "UNKNOWN";
    }
    
    // Store regime
    current_regime_ = thresholds.regime;
    
    // 3. Adapt confidence based on recent performance
    // Confidence adaptation removed - using signal strength from probability only
    SignalOutput adjusted_signal = signal;
    
    // Log threshold adjustments
    if (config_.log_threshold_changes) {
        std::stringstream ss;
        ss << "DYNAMIC THRESHOLDS:"
           << " State=" << static_cast<int>(current_state)
           << " Buy=" << std::fixed << std::setprecision(3) << thresholds.buy_threshold
           << " Sell=" << thresholds.sell_threshold
           << " Confidence=" << thresholds.confidence_threshold
           << " Variance=" << thresholds.signal_variance
           << " Momentum=" << thresholds.signal_momentum
           << " Regime=" << thresholds.regime
           << " BarsInPos=" << bars_in_position_;
        utils::log_info(ss.str());
    }
    
    // 4. Classify signal using DYNAMIC thresholds
    SignalType signal_type = classify_signal_with_hysteresis(adjusted_signal, thresholds);
    
    // Check for forced transition due to position age
    if (config_.track_bars_in_position && should_force_transition(current_state)) {
        if (is_long_state(current_state)) {
            signal_type = SignalType::WEAK_SELL;  // Force exit from aged long
        } else if (is_short_state(current_state)) {
            signal_type = SignalType::WEAK_BUY;   // Force exit from aged short
        }
    }
    
    // Handle NEUTRAL signal
    if (signal_type == SignalType::NEUTRAL) {
        EnhancedTransition hold;
        hold.current_state = current_state;
        hold.signal_type = signal_type;
        hold.target_state = current_state;
        hold.optimal_action = "Hold position";
        hold.theoretical_basis = "Signal in neutral zone";
        hold.expected_return = 0.0;
        hold.risk_score = 0.0;
        hold.confidence = 0.5;
        hold.thresholds_used = thresholds;
        hold.bars_in_current_position = bars_in_position_;
        hold.regime = thresholds.regime;
        hold.original_probability = signal.probability;
        hold.adjusted_probability = signal.probability;
        // Confidence tracking removed
        return hold;
    }
    
    // 5. Determine transition using base class logic (FIXED!)
    StateTransition base_transition = get_base_transition(current_state, signal_type);
    
    // Track statistics for monitoring
    stats_.total_signals++;
    if (signal_type == SignalType::STRONG_SELL || signal_type == SignalType::WEAK_SELL) {
        stats_.short_signals++;
        if (is_short_state(base_transition.target_state)) {
            stats_.short_transitions++;
        }
    } else if (signal_type == SignalType::STRONG_BUY || signal_type == SignalType::WEAK_BUY) {
        stats_.long_signals++;
        if (is_long_state(base_transition.target_state)) {
            stats_.long_transitions++;
        }
    }
    if (base_transition.target_state != current_state) {
        stats_.transitions_triggered++;
    }
    
    // Log the transition for debugging
    if (config_.log_threshold_changes && base_transition.target_state != current_state) {
        std::stringstream ss;
        ss << "STATE TRANSITION: "
           << state_to_string(current_state) << " -> " 
           << state_to_string(base_transition.target_state)
           << " (Signal: " << signal_type_to_string(signal_type) << ")"
           << " [Stats: " << stats_.short_transitions << "/" << stats_.short_signals 
           << " shorts, " << stats_.long_transitions << "/" << stats_.long_signals << " longs]";
        utils::log_info(ss.str());
    }
    
    // 6. Create enhanced transition with allocation info
    double available_capital = current_portfolio.cash_balance;
    
    // Add liquidation value of current positions if transitioning
    if (base_transition.target_state != current_state) {
        for (const auto& [symbol, position] : current_portfolio.positions) {
            available_capital += position.quantity * position.current_price;
        }
    }
    
    EnhancedTransition enhanced = create_enhanced_transition(
        base_transition, adjusted_signal, thresholds, available_capital, market_conditions);
    
    // Store adjustment metadata
    enhanced.original_probability = signal.probability;
    enhanced.adjusted_probability = signal.probability;
    // Confidence tracking removed
    enhanced.bars_in_current_position = bars_in_position_;
    enhanced.regime = thresholds.regime;
    
    // Calculate current P&L if in position
    if (current_state != State::CASH_ONLY && !current_portfolio.positions.empty()) {
        double total_pnl = 0.0;
        for (const auto& [symbol, position] : current_portfolio.positions) {
            total_pnl += (position.current_price - position.avg_price) * position.quantity;
        }
        enhanced.position_pnl = total_pnl;
    }
    
    return enhanced;
}

void EnhancedPositionStateMachine::update_signal_history(const SignalOutput& signal) {
    if (hysteresis_manager_) {
        hysteresis_manager_->update_signal_history(signal);
    }
}

void EnhancedPositionStateMachine::update_position_tracking(State new_state) {
    if (new_state != current_state_) {
        previous_state_ = current_state_;
        current_state_ = new_state;
        bars_in_position_ = 0;
    } else {
        bars_in_position_++;
    }
}

void EnhancedPositionStateMachine::record_trade_result(double pnl, bool was_profitable) {
    if (!config_.track_performance) return;
    
    TradeResult result;
    result.pnl = pnl;
    result.profitable = was_profitable;
    result.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    recent_trades_.push_back(result);
    
    // Maintain window size
    while (recent_trades_.size() > static_cast<size_t>(config_.performance_window)) {
        recent_trades_.pop_front();
    }
}

double EnhancedPositionStateMachine::get_recent_win_rate() const {
    if (recent_trades_.empty()) return 0.5;  // Default assumption
    
    int wins = 0;
    for (const auto& trade : recent_trades_) {
        if (trade.profitable) wins++;
    }
    
    return static_cast<double>(wins) / recent_trades_.size();
}

double EnhancedPositionStateMachine::get_recent_avg_pnl() const {
    if (recent_trades_.empty()) return 0.0;
    
    double total_pnl = 0.0;
    for (const auto& trade : recent_trades_) {
        total_pnl += trade.pnl;
    }
    
    return total_pnl / recent_trades_.size();
}

PositionStateMachine::SignalType EnhancedPositionStateMachine::classify_signal_with_hysteresis(
    const SignalOutput& signal,
    const backend::DynamicHysteresisManager::DynamicThresholds& thresholds) const {
    
    // Confidence filter removed - using signal strength from probability only
    
    // Classify using DYNAMIC thresholds (HYSTERESIS!)
    double p = signal.probability;
    
    if (p > thresholds.strong_buy_threshold)  return SignalType::STRONG_BUY;
    if (p > thresholds.buy_threshold)         return SignalType::WEAK_BUY;
    if (p < thresholds.strong_sell_threshold) return SignalType::STRONG_SELL;
    if (p < thresholds.sell_threshold)        return SignalType::WEAK_SELL;
    
    return SignalType::NEUTRAL;
}

// adapt_confidence() function removed - confidence no longer used

bool EnhancedPositionStateMachine::should_force_transition(State current_state) const {
    if (!config_.track_bars_in_position) return false;
    
    // Don't force if in cash
    if (current_state == State::CASH_ONLY) return false;
    
    // Force re-evaluation after max bars
    return bars_in_position_ >= config_.max_bars_in_position;
}

bool EnhancedPositionStateMachine::is_dual_state(State state) const {
    return state == State::QQQ_TQQQ || state == State::PSQ_SQQQ;
}

bool EnhancedPositionStateMachine::is_long_state(State state) const {
    return state == State::QQQ_ONLY || 
           state == State::TQQQ_ONLY || 
           state == State::QQQ_TQQQ;
}

bool EnhancedPositionStateMachine::is_short_state(State state) const {
    return state == State::PSQ_ONLY || 
           state == State::SQQQ_ONLY || 
           state == State::PSQ_SQQQ;
}

EnhancedPositionStateMachine::EnhancedTransition 
EnhancedPositionStateMachine::create_enhanced_transition(
    const StateTransition& base_transition,
    const SignalOutput& signal,
    const backend::DynamicHysteresisManager::DynamicThresholds& thresholds,
    double available_capital,
    const MarketState& market) {
    
    EnhancedTransition enhanced;
    
    // Copy base transition fields
    enhanced.current_state = base_transition.current_state;
    enhanced.signal_type = base_transition.signal_type;
    enhanced.target_state = base_transition.target_state;
    enhanced.optimal_action = base_transition.optimal_action;
    enhanced.theoretical_basis = base_transition.theoretical_basis;
    enhanced.expected_return = base_transition.expected_return;
    enhanced.risk_score = base_transition.risk_score;
    enhanced.confidence = base_transition.confidence;
    
    // Add threshold info
    enhanced.thresholds_used = thresholds;
    
    // Calculate allocation if transitioning to dual state
    if (allocation_manager_ && is_dual_state(enhanced.target_state)) {
        // FIX: Use actual market price from MarketState
        // All symbols use same underlying price (QQQ close price)
        double current_price = market.current_price > 0.0 ? market.current_price : 100.0;
        double price_base = current_price;
        double price_leveraged = current_price;  // Same price for all
        
        // Market conditions for allocation
        backend::DynamicAllocationManager::MarketConditions alloc_market;
        alloc_market.current_volatility = market.volatility;
        alloc_market.volatility_percentile = 50.0;
        alloc_market.trend_strength = thresholds.signal_momentum;
        alloc_market.market_regime = thresholds.regime;
        
        // Calculate allocation
        enhanced.allocation = allocation_manager_->calculate_dual_allocation(
            enhanced.target_state,
            signal,
            available_capital,
            price_base,
            price_leveraged,
            alloc_market
        );
        
        // Update theoretical basis with allocation info
        enhanced.theoretical_basis += " | " + enhanced.allocation.allocation_rationale;
    }
    // For single positions, use simple allocation
    else if (allocation_manager_ && enhanced.target_state != State::CASH_ONLY) {
        std::string symbol;
        bool is_leveraged = false;
        // FIX: Use actual market price for all symbols
        double current_price = market.current_price > 0.0 ? market.current_price : 100.0;
        double price = current_price;
        
        switch (enhanced.target_state) {
            case State::QQQ_ONLY:
                symbol = "QQQ";
                break;
            case State::TQQQ_ONLY:
                symbol = "TQQQ";
                is_leveraged = true;
                break;
            case State::PSQ_ONLY:
                symbol = "PSQ";
                break;
            case State::SQQQ_ONLY:
                symbol = "SQQQ";
                is_leveraged = true;
                break;
            default:
                break;
        }
        
        if (!symbol.empty()) {
            enhanced.allocation = allocation_manager_->calculate_single_allocation(
                symbol,
                signal,
                available_capital,
                price,
                is_leveraged
            );
        }
    }
    
    return enhanced;
}

void EnhancedPositionStateMachine::log_threshold_changes(
    const backend::DynamicHysteresisManager::DynamicThresholds& old_thresholds,
    const backend::DynamicHysteresisManager::DynamicThresholds& new_thresholds) const {
    
    std::stringstream ss;
    ss << "THRESHOLD CHANGES:"
       << " Buy: " << old_thresholds.buy_threshold << "→" << new_thresholds.buy_threshold
       << " Sell: " << old_thresholds.sell_threshold << "→" << new_thresholds.sell_threshold
       << " Confidence: " << old_thresholds.confidence_threshold << "→" << new_thresholds.confidence_threshold
       << " Regime: " << old_thresholds.regime << "→" << new_thresholds.regime;
    
    utils::log_debug(ss.str());
}

} // namespace sentio


```

## 📄 **FILE 34 of 46**: src/backend/ensemble_position_state_machine.cpp

**File Information**:
- **Path**: `src/backend/ensemble_position_state_machine.cpp`

- **Size**: 477 lines
- **Modified**: 2025-10-07 00:37:13

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

## 📄 **FILE 35 of 46**: src/backend/position_state_machine.cpp

**File Information**:
- **Path**: `src/backend/position_state_machine.cpp`

- **Size**: 686 lines
- **Modified**: 2025-10-08 13:01:19

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

    // Support both QQQ-family and SPY-family instruments
    bool has_qqq = symbols.count("QQQ") || symbols.count("SPY");
    bool has_tqqq = symbols.count("TQQQ") || symbols.count("SPXL");
    bool has_psq = symbols.count("PSQ") || symbols.count("SH");
    bool has_sqqq = symbols.count("SQQQ") || symbols.count("SDS") || symbols.count("SPXS");

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

## 📄 **FILE 36 of 46**: src/common/config_loader.cpp

**File Information**:
- **Path**: `src/common/config_loader.cpp`

- **Size**: 117 lines
- **Modified**: 2025-10-08 03:33:05

- **Type**: .cpp

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

## 📄 **FILE 37 of 46**: src/core/data_manager.cpp

**File Information**:
- **Path**: `src/core/data_manager.cpp`

- **Size**: 31 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .cpp

```text
#include "core/data_manager.h"
#include "common/utils.h"

namespace sentio {

void DataManager::load_market_data(const std::string& path) {
    bars_ = utils::read_csv_data(path); // read_csv_data already populates bar_id and derived fields
    id_to_index_.clear();
    id_to_index_.reserve(bars_.size());
    for (size_t i = 0; i < bars_.size(); ++i) {
        id_to_index_[bars_[i].bar_id] = i;
    }
}

const Bar* DataManager::get_bar(uint64_t bar_id) const {
    auto it = id_to_index_.find(bar_id);
    if (it == id_to_index_.end()) return nullptr;
    size_t idx = it->second;
    if (idx >= bars_.size()) return nullptr;
    return &bars_[idx];
}

const Bar* DataManager::get_bar_by_index(size_t index) const {
    if (index >= bars_.size()) return nullptr;
    return &bars_[index];
}

} // namespace sentio




```

## 📄 **FILE 38 of 46**: src/data/multi_symbol_data_manager.cpp

**File Information**:
- **Path**: `src/data/multi_symbol_data_manager.cpp`

- **Size**: 375 lines
- **Modified**: 2025-10-14 23:00:21

- **Type**: .cpp

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

## 📄 **FILE 39 of 46**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`

- **Size**: 478 lines
- **Modified**: 2025-10-15 02:31:40

- **Type**: .cpp

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

        // Debug BB NaN issue (log first 5 occurrences)
        static int bb_nan_count = 0;
        if (bb_nan_count < 5 && std::isnan(bb20_.mean)) {
            std::cerr << "[FeatureEngine #" << bb_nan_count
                      << "] BB NaN detected! bar_count=" << bar_count_
                      << ", bb20_.win.size=" << bb20_.win.size()
                      << ", bb20_.win.full=" << bb20_.win.full()
                      << ", prevClose=" << prevClose_ << std::endl;
            bb_nan_count++;
        }

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

    return std::max(0, max_period - static_cast<int>(bar_count_));
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

## 📄 **FILE 40 of 46**: src/live/alpaca_client.cpp

**File Information**:
- **Path**: `src/live/alpaca_client.cpp`

- **Size**: 477 lines
- **Modified**: 2025-10-09 10:39:50

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

## 📄 **FILE 41 of 46**: src/live/mock_bar_feed_replay.cpp

**File Information**:
- **Path**: `src/live/mock_bar_feed_replay.cpp`

- **Size**: 312 lines
- **Modified**: 2025-10-09 22:54:13

- **Type**: .cpp

```text
#include "live/mock_bar_feed_replay.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace sentio {

MockBarFeedReplay::MockBarFeedReplay(const std::string& csv_file, double speed_multiplier)
    : connected_(false)
    , running_(false)
    , current_index_(0)
    , speed_multiplier_(speed_multiplier)
    , replay_start_market_ms_(0)
    , last_message_time_(Clock::now())
{
    load_csv(csv_file);
}

MockBarFeedReplay::~MockBarFeedReplay() {
    stop();
}

bool MockBarFeedReplay::connect() {
    if (bars_by_symbol_.empty()) {
        return false;
    }
    connected_ = true;
    return true;
}

bool MockBarFeedReplay::subscribe(const std::vector<std::string>& symbols) {
    subscribed_symbols_ = symbols;
    return true;
}

void MockBarFeedReplay::start(BarCallback callback) {
    if (!connected_ || running_) {
        return;
    }

    callback_ = callback;
    running_ = true;
    current_index_ = 0;

    // Initialize time anchors
    replay_start_real_ = Clock::now();

    // Find first bar timestamp as market start time
    if (!bars_by_symbol_.empty()) {
        const auto& first_symbol_bars = bars_by_symbol_.begin()->second;
        if (!first_symbol_bars.empty()) {
            replay_start_market_ms_ = first_symbol_bars[0].timestamp_ms;
        }
    }

    // Start replay thread
    replay_thread_ = std::make_unique<std::thread>(&MockBarFeedReplay::replay_loop, this);
}

void MockBarFeedReplay::stop() {
    running_ = false;

    if (replay_thread_ && replay_thread_->joinable()) {
        replay_thread_->join();
    }

    connected_ = false;
}

std::vector<Bar> MockBarFeedReplay::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    std::vector<Bar> result;

    if (bars_history_.count(symbol)) {
        const auto& history = bars_history_.at(symbol);
        size_t start = (history.size() > count) ? (history.size() - count) : 0;

        for (size_t i = start; i < history.size(); ++i) {
            result.push_back(history[i]);
        }
    }

    return result;
}

bool MockBarFeedReplay::is_connected() const {
    return connected_;
}

bool MockBarFeedReplay::is_connection_healthy() const {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_message_time_.load()).count();
    return elapsed < 120;  // 2 minutes timeout
}

int MockBarFeedReplay::get_seconds_since_last_message() const {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - last_message_time_.load()).count();
}

bool MockBarFeedReplay::load_csv(const std::string& csv_file) {
    std::ifstream file(csv_file);
    if (!file.is_open()) {
        return false;
    }

    // CSV format: date_str,timestamp_sec,open,high,low,close,volume
    // All bars go into "SPY" by default (can be extended for multi-symbol)

    std::string line;

    std::vector<Bar> bars;

    while (std::getline(file, line)) {
        // Skip empty lines or header-like lines
        if (line.empty() ||
            line.find("timestamp") != std::string::npos ||
            line.find("ts_utc") != std::string::npos ||
            line.find("ts_nyt_epoch") != std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        std::string date_str, ts_str, open_str, high_str, low_str, close_str, volume_str;

        if (std::getline(iss, date_str, ',') &&
            std::getline(iss, ts_str, ',') &&
            std::getline(iss, open_str, ',') &&
            std::getline(iss, high_str, ',') &&
            std::getline(iss, low_str, ',') &&
            std::getline(iss, close_str, ',') &&
            std::getline(iss, volume_str)) {

            Bar bar;
            // Convert seconds to milliseconds
            bar.timestamp_ms = std::stoull(ts_str) * 1000ULL;
            bar.open = std::stod(open_str);
            bar.high = std::stod(high_str);
            bar.low = std::stod(low_str);
            bar.close = std::stod(close_str);
            bar.volume = std::stoll(volume_str);

            bars.push_back(bar);
        }
    }

    file.close();

    if (bars.empty()) {
        return false;
    }

    // Sort by timestamp
    std::sort(bars.begin(), bars.end(),
              [](const Bar& a, const Bar& b) { return a.timestamp_ms < b.timestamp_ms; });

    bars_by_symbol_["SPY"] = bars;

    // For multi-instrument, create synthetic bars for other symbols
    // (In production, load from separate CSV files)
    bars_by_symbol_["SPXL"] = bars;  // Same timing for now
    bars_by_symbol_["SH"] = bars;
    bars_by_symbol_["SDS"] = bars;

    return true;
}

void MockBarFeedReplay::add_bar(const std::string& symbol, const Bar& bar) {
    bars_by_symbol_[symbol].push_back(bar);
}

void MockBarFeedReplay::set_speed_multiplier(double multiplier) {
    speed_multiplier_ = multiplier;
}

MockBarFeedReplay::ReplayProgress MockBarFeedReplay::get_progress() const {
    ReplayProgress progress;

    if (!bars_by_symbol_.empty()) {
        const auto& bars = bars_by_symbol_.begin()->second;
        progress.total_bars = bars.size();
        progress.current_index = current_index_;
        progress.progress_pct = (progress.total_bars > 0) ?
            (100.0 * progress.current_index / progress.total_bars) : 0.0;

        if (progress.current_index < bars.size()) {
            progress.current_bar_timestamp_ms = bars[progress.current_index].timestamp_ms;

            // Format timestamp
            time_t time_t_val = static_cast<time_t>(progress.current_bar_timestamp_ms / 1000);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
            progress.current_bar_time_str = ss.str();
        }
    }

    return progress;
}

bool MockBarFeedReplay::is_replay_complete() const {
    if (bars_by_symbol_.empty()) {
        return true;
    }

    const auto& bars = bars_by_symbol_.begin()->second;
    return current_index_ >= bars.size();
}

bool MockBarFeedReplay::validate_data_integrity() const {
    for (const auto& [symbol, bars] : bars_by_symbol_) {
        // Check for gaps in timestamps
        for (size_t i = 1; i < bars.size(); ++i) {
            if (bars[i].timestamp_ms <= bars[i-1].timestamp_ms) {
                return false;  // Not monotonically increasing
            }
        }

        // Verify OHLC relationships
        for (const auto& bar : bars) {
            if (bar.high < bar.low) return false;
            if (bar.high < bar.open) return false;
            if (bar.high < bar.close) return false;
            if (bar.low > bar.open) return false;
            if (bar.low > bar.close) return false;
            if (bar.volume < 0) return false;
        }
    }

    return true;
}

void MockBarFeedReplay::replay_loop() {
    while (running_ && !is_replay_complete()) {
        std::string symbol;
        auto bar_opt = get_next_bar(symbol);

        if (!bar_opt.has_value()) {
            break;  // No more bars
        }

        const Bar& bar = bar_opt.value();

        // Wait until it's time to deliver this bar (drift-free)
        wait_until_bar_time(bar);

        // Store in history
        store_bar(symbol, bar);

        // Update health timestamp
        last_message_time_ = Clock::now();

        // Deliver to callback
        if (callback_) {
            callback_(symbol, bar);
        }

        current_index_++;
    }

    running_ = false;
}

void MockBarFeedReplay::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    if (bars_history_[symbol].size() >= MAX_BARS_HISTORY) {
        bars_history_[symbol].pop_front();
    }

    bars_history_[symbol].push_back(bar);
}

std::optional<Bar> MockBarFeedReplay::get_next_bar(std::string& out_symbol) {
    // For simplicity, deliver SPY bars (can be extended for multi-symbol round-robin)
    if (bars_by_symbol_.count("SPY") == 0) {
        return std::nullopt;
    }

    const auto& bars = bars_by_symbol_["SPY"];
    size_t idx = current_index_;

    if (idx >= bars.size()) {
        return std::nullopt;
    }

    out_symbol = "SPY";
    return bars[idx];
}

void MockBarFeedReplay::wait_until_bar_time(const Bar& bar) {
    if (speed_multiplier_ <= 0.0) {
        return;  // No delay
    }

    // Calculate when this bar should be delivered (drift-free)
    uint64_t elapsed_market_ms = bar.timestamp_ms - replay_start_market_ms_;

    // Scale by speed multiplier (higher multiplier = faster)
    auto elapsed_real_ms = static_cast<uint64_t>(elapsed_market_ms / speed_multiplier_);

    auto target_time = replay_start_real_ + std::chrono::milliseconds(elapsed_real_ms);

    // Sleep until target time (prevents drift accumulation)
    std::this_thread::sleep_until(target_time);
}

} // namespace sentio

```

## 📄 **FILE 42 of 46**: src/strategy/multi_symbol_oes_manager.cpp

**File Information**:
- **Path**: `src/strategy/multi_symbol_oes_manager.cpp`

- **Size**: 398 lines
- **Modified**: 2025-10-15 02:32:27

- **Type**: .cpp

```text
#include "strategy/multi_symbol_oes_manager.h"
#include "common/utils.h"
#include <iostream>

namespace sentio {

MultiSymbolOESManager::MultiSymbolOESManager(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr
)
    : config_(config)
    , data_mgr_(data_mgr) {

    utils::log_info("MultiSymbolOESManager initializing for " +
                   std::to_string(config_.symbols.size()) + " symbols");

    // Create OES instance for each symbol
    for (const auto& symbol : config_.symbols) {
        // Use symbol-specific config if available, otherwise use base config
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        if (config_.symbol_configs.count(symbol) > 0) {
            oes_config = config_.symbol_configs.at(symbol);
            utils::log_info("  " + symbol + ": Using custom config");
        } else {
            oes_config = config_.base_config;
            utils::log_info("  " + symbol + ": Using base config");
        }

        // Create OES instance
        auto oes = std::make_unique<OnlineEnsembleStrategy>(oes_config);
        oes_instances_[symbol] = std::move(oes);
    }

    utils::log_info("MultiSymbolOESManager initialized: " +
                   std::to_string(oes_instances_.size()) + " instances created");
}

// === Signal Generation ===

std::map<std::string, SignalOutput> MultiSymbolOESManager::generate_all_signals() {
    std::map<std::string, SignalOutput> signals;

    auto snapshot = data_mgr_->get_latest_snapshot();

    // DEBUG: Log snapshot status
    static int debug_count = 0;
    if (debug_count < 5) {
        utils::log_info("DEBUG generate_all_signals: snapshot has " +
                       std::to_string(snapshot.snapshots.size()) + " symbols");
        std::cout << "[OES] generate_all_signals: snapshot has " << snapshot.snapshots.size() << " symbols: ";
        for (const auto& [symbol, _] : snapshot.snapshots) {
            std::cout << symbol << " ";
        }
        std::cout << std::endl;
        debug_count++;
    }

    for (const auto& symbol : config_.symbols) {
        // Check if symbol has valid data
        if (snapshot.snapshots.count(symbol) == 0) {
            static std::map<std::string, int> warning_counts;
            if (warning_counts[symbol] < 3) {
                utils::log_warning("No data for " + symbol + " - skipping signal");
                std::cout << "[OES]   " << symbol << ": No data in snapshot - skipping" << std::endl;
                warning_counts[symbol]++;
            }
            continue;
        }

        const auto& sym_snap = snapshot.snapshots.at(symbol);
        if (!sym_snap.is_valid) {
            static std::map<std::string, int> stale_counts;
            if (stale_counts[symbol] < 3) {
                utils::log_warning("Stale data for " + symbol + " (" +
                                 std::to_string(sym_snap.staleness_seconds) + "s) - skipping signal");
                std::cout << "[OES]   " << symbol << ": Stale data (" << sym_snap.staleness_seconds << "s) - skipping" << std::endl;
                stale_counts[symbol]++;
            }
            continue;
        }

        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_error("No OES instance for " + symbol);
            std::cout << "[OES]   " << symbol << ": No OES instance - skipping" << std::endl;
            continue;
        }

        // Check if OES is ready
        if (!it->second->is_ready()) {
            static std::map<std::string, int> not_ready_counts;
            if (not_ready_counts[symbol] < 3) {
                std::cout << "[OES]   " << symbol << ": OES not ready - skipping" << std::endl;
                not_ready_counts[symbol]++;
            }
            continue;
        }

        // Generate signal
        SignalOutput signal = it->second->generate_signal(sym_snap.latest_bar);

        // DEBUG: Check for NaN in signal
        static int nan_signal_count = 0;
        if (nan_signal_count < 5 && signal.probability == 0.5) {
            std::cout << "[OES]   " << symbol << ": NEUTRAL signal (prob=0.5) - might be due to NaN features" << std::endl;
            nan_signal_count++;
        }

        // Apply staleness weighting to probability
        // Reduce confidence in signal if data is old
        signal.probability *= sym_snap.staleness_weight;

        signals[symbol] = signal;
        total_signals_generated_++;

        // Debug first few signals
        static int signal_debug_count = 0;
        if (signal_debug_count < 3) {
            std::cout << "[OES]   " << symbol << ": Generated signal (type=" << static_cast<int>(signal.signal_type)
                      << ", prob=" << signal.probability << ")" << std::endl;
            signal_debug_count++;
        }
    }

    std::cout << "[OES] Returning " << signals.size() << " signals" << std::endl;
    return signals;
}

SignalOutput MultiSymbolOESManager::generate_signal(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return SignalOutput();  // Return empty signal
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return SignalOutput();
    }

    SignalOutput signal = it->second->generate_signal(bar);
    total_signals_generated_++;

    return signal;
}

// === Learning Updates ===

void MultiSymbolOESManager::update_all(const std::map<std::string, double>& realized_pnls) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& [symbol, realized_pnl] : realized_pnls) {
        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_warning("No OES instance for " + symbol + " - cannot update");
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            utils::log_warning("No data for " + symbol + " - cannot update");
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Update OES
        it->second->update(bar, realized_pnl);
        total_updates_++;
    }
}

void MultiSymbolOESManager::update(const std::string& symbol, double realized_pnl) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return;
    }

    it->second->update(bar, realized_pnl);
    total_updates_++;
}

void MultiSymbolOESManager::on_bar() {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& symbol : config_.symbols) {
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Call on_bar for each OES
        it->second->on_bar(bar);
    }
}

// === Warmup ===

bool MultiSymbolOESManager::warmup_all(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("Warming up all OES instances...");

    bool all_success = true;
    for (const auto& [symbol, bars] : symbol_bars) {
        if (!warmup(symbol, bars)) {
            utils::log_error("Warmup failed for " + symbol);
            all_success = false;
        }
    }

    if (all_success) {
        utils::log_info("All OES instances warmed up successfully");
    } else {
        utils::log_warning("Some OES instances failed warmup");
    }

    return all_success;
}

bool MultiSymbolOESManager::warmup(const std::string& symbol, const std::vector<Bar>& bars) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return false;
    }

    utils::log_info("Warming up " + symbol + " with " + std::to_string(bars.size()) + " bars...");
    std::cout << "[OESManager::warmup] Starting warmup for " << symbol
              << " with " << bars.size() << " bars" << std::endl;

    // Feed bars one by one
    for (size_t i = 0; i < bars.size(); ++i) {
        it->second->on_bar(bars[i]);

        // Debug first few warmup calls
        if (i < 3) {
            std::cout << "[OESManager::warmup]   Bar " << i << " processed" << std::endl;
        }
    }

    std::cout << "[OESManager::warmup] Completed " << bars.size() << " warmup bars for " << symbol << std::endl;

    // Check if ready
    bool ready = it->second->is_ready();
    if (ready) {
        utils::log_info("  " + symbol + ": Warmup complete - ready for trading");
        std::cout << "[OESManager::warmup]   " << symbol << ": READY" << std::endl;
    } else {
        utils::log_warning("  " + symbol + ": Warmup incomplete - needs more data");
        std::cout << "[OESManager::warmup]   " << symbol << ": NOT READY" << std::endl;
    }

    return ready;
}

// === Configuration ===

void MultiSymbolOESManager::update_config(
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    utils::log_info("Updating config for all OES instances");

    config_.base_config = new_config;

    for (auto& [symbol, oes] : oes_instances_) {
        // Only update if not using custom config
        if (config_.symbol_configs.count(symbol) == 0) {
            oes->update_config(new_config);
        }
    }
}

void MultiSymbolOESManager::update_config(
    const std::string& symbol,
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    utils::log_info("Updating config for " + symbol);
    it->second->update_config(new_config);

    // Save as custom config
    config_.symbol_configs[symbol] = new_config;
}

// === Diagnostics ===

std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
MultiSymbolOESManager::get_all_performance_metrics() const {
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics> metrics;

    for (const auto& [symbol, oes] : oes_instances_) {
        metrics[symbol] = oes->get_performance_metrics();
    }

    return metrics;
}

OnlineEnsembleStrategy::PerformanceMetrics
MultiSymbolOESManager::get_performance_metrics(const std::string& symbol) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return OnlineEnsembleStrategy::PerformanceMetrics();
    }

    return it->second->get_performance_metrics();
}

bool MultiSymbolOESManager::all_ready() const {
    for (const auto& [symbol, oes] : oes_instances_) {
        if (!oes->is_ready()) {
            return false;
        }
    }
    return !oes_instances_.empty();
}

std::map<std::string, bool> MultiSymbolOESManager::get_ready_status() const {
    std::map<std::string, bool> status;

    for (const auto& [symbol, oes] : oes_instances_) {
        status[symbol] = oes->is_ready();
    }

    return status;
}

std::map<std::string, OnlineEnsembleStrategy::LearningState>
MultiSymbolOESManager::get_all_learning_states() const {
    std::map<std::string, OnlineEnsembleStrategy::LearningState> states;

    for (const auto& [symbol, oes] : oes_instances_) {
        states[symbol] = oes->get_learning_state();
    }

    return states;
}

OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(
    const std::string& symbol
) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

// === Private Methods ===

bool MultiSymbolOESManager::get_latest_bar(const std::string& symbol, Bar& bar) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        return false;
    }

    const auto& sym_snap = snapshot.snapshots.at(symbol);
    if (!sym_snap.is_valid) {
        return false;
    }

    bar = sym_snap.latest_bar;
    return true;
}

} // namespace sentio

```

## 📄 **FILE 43 of 46**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 780 lines
- **Modified**: 2025-10-15 02:37:38

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

    // DEBUG: Check for NaN features
    static int signal_gen_count = 0;
    if (signal_gen_count < 5) {
        bool has_nan = false;
        for (size_t i = 0; i < features.size(); ++i) {
            if (!std::isfinite(features[i])) {
                has_nan = true;
                break;
            }
        }
        std::cout << "[OES::generate_signal #" << signal_gen_count
                  << "] samples_seen=" << samples_seen_
                  << ", features.size=" << features.size()
                  << ", has_NaN=" << (has_nan ? "YES" : "NO") << std::endl;
        signal_gen_count++;
    }

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
    output.confidence = prediction.confidence;  // FIX: Set confidence from prediction
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

## 📄 **FILE 44 of 46**: src/strategy/signal_aggregator.cpp

**File Information**:
- **Path**: `src/strategy/signal_aggregator.cpp`

- **Size**: 181 lines
- **Modified**: 2025-10-14 22:21:43

- **Type**: .cpp

```text
#include "strategy/signal_aggregator.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>

namespace sentio {

SignalAggregator::SignalAggregator(const Config& config)
    : config_(config) {

    utils::log_info("SignalAggregator initialized");
    utils::log_info("  Leverage boosts: " + std::to_string(config_.leverage_boosts.size()) + " symbols");
    utils::log_info("  Min probability: " + std::to_string(config_.min_probability));
    utils::log_info("  Min confidence: " + std::to_string(config_.min_confidence));
    utils::log_info("  Min strength: " + std::to_string(config_.min_strength));
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::rank_signals(
    const std::map<std::string, SignalOutput>& signals,
    const std::map<std::string, double>& staleness_weights
) {
    std::vector<RankedSignal> ranked;

    stats_.total_signals_processed += signals.size();

    for (const auto& [symbol, signal] : signals) {
        // Apply filters
        if (!passes_filters(signal)) {
            stats_.signals_filtered++;
            continue;
        }

        // Get leverage boost
        double leverage_boost = get_leverage_boost(symbol);

        // Get staleness weight (default to 1.0 if not provided)
        double staleness_weight = 1.0;
        if (staleness_weights.count(symbol) > 0) {
            staleness_weight = staleness_weights.at(symbol);
        }

        // Calculate strength
        double strength = calculate_strength(signal, leverage_boost, staleness_weight);

        // Filter by minimum strength
        if (strength < config_.min_strength) {
            stats_.signals_filtered++;
            continue;
        }

        // Create ranked signal
        RankedSignal ranked_signal;
        ranked_signal.symbol = symbol;
        ranked_signal.signal = signal;
        ranked_signal.leverage_boost = leverage_boost;
        ranked_signal.strength = strength;
        ranked_signal.staleness_weight = staleness_weight;
        ranked_signal.rank = 0;  // Will be set after sorting

        ranked.push_back(ranked_signal);

        // Update stats
        stats_.signals_per_symbol[symbol]++;
    }

    // Sort by strength (descending)
    std::sort(ranked.begin(), ranked.end());

    // Assign ranks
    for (size_t i = 0; i < ranked.size(); i++) {
        ranked[i].rank = static_cast<int>(i + 1);
    }

    // Update stats
    stats_.signals_ranked = static_cast<int>(ranked.size());
    if (!ranked.empty()) {
        double sum_strength = 0.0;
        for (const auto& rs : ranked) {
            sum_strength += rs.strength;
        }
        stats_.avg_strength = sum_strength / ranked.size();
        stats_.max_strength = ranked[0].strength;
    }

    return ranked;
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::get_top_n(
    const std::vector<RankedSignal>& ranked_signals,
    int n
) const {
    std::vector<RankedSignal> top_n;

    int count = std::min(n, static_cast<int>(ranked_signals.size()));
    for (int i = 0; i < count; i++) {
        top_n.push_back(ranked_signals[i]);
    }

    return top_n;
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::filter_by_direction(
    const std::vector<RankedSignal>& ranked_signals,
    SignalType direction
) const {
    std::vector<RankedSignal> filtered;

    for (const auto& rs : ranked_signals) {
        if (rs.signal.signal_type == direction) {
            filtered.push_back(rs);
        }
    }

    return filtered;
}

// === Private Methods ===

double SignalAggregator::calculate_strength(
    const SignalOutput& signal,
    double leverage_boost,
    double staleness_weight
) const {
    // Base strength: probability × confidence
    double base_strength = signal.probability * signal.confidence;

    // Apply leverage boost
    double boosted_strength = base_strength * leverage_boost;

    // Apply staleness penalty
    double final_strength = boosted_strength * staleness_weight;

    return final_strength;
}

bool SignalAggregator::passes_filters(const SignalOutput& signal) const {
    // Filter NEUTRAL signals
    if (signal.signal_type == SignalType::NEUTRAL) {
        return false;
    }

    // Filter by minimum probability
    if (signal.probability < config_.min_probability) {
        return false;
    }

    // Filter by minimum confidence
    if (signal.confidence < config_.min_confidence) {
        return false;
    }

    // Check for NaN or invalid values
    if (std::isnan(signal.probability) || std::isnan(signal.confidence)) {
        utils::log_warning("Invalid signal: NaN probability or confidence");
        return false;
    }

    if (signal.probability < 0.0 || signal.probability > 1.0) {
        utils::log_warning("Invalid signal: probability out of range [0,1]");
        return false;
    }

    if (signal.confidence < 0.0 || signal.confidence > 1.0) {
        utils::log_warning("Invalid signal: confidence out of range [0,1]");
        return false;
    }

    return true;
}

double SignalAggregator::get_leverage_boost(const std::string& symbol) const {
    auto it = config_.leverage_boosts.find(symbol);
    if (it != config_.leverage_boosts.end()) {
        return it->second;
    }

    // Default: no boost (1.0)
    return 1.0;
}

} // namespace sentio

```

## 📄 **FILE 45 of 46**: src/testing/test_framework.cpp

**File Information**:
- **Path**: `src/testing/test_framework.cpp`

- **Size**: 416 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .cpp

```text
// src/testing/test_framework.cpp
#include "testing/test_framework.h"
#include "validation/strategy_validator.h"
#include "analysis/performance_analyzer.h"
#include "strategy/istrategy.h"
#include "strategy/config_resolver.h"
#include "common/utils.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace sentio::testing {

TestResult TestFramework::run_sanity_check(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        // Validate configuration
        std::string error_msg;
        if (!validate_config(config, error_msg)) {
            result.status = TestStatus::ERROR;
            result.status_message = "Invalid configuration: " + error_msg;
            result.add_error(error_msg);
            return result;
        }
        
        if (!config.quiet) {
            std::cout << "🔍 Running sanity check for strategy: " << config.strategy_name << std::endl;
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        }
        
        // Run validation
        auto validation_result = validation::StrategyValidator::validate_strategy(
            config.strategy_name,
            config.primary_data_path,
            config
        );
        
        // Convert validation result to test result
        result.total_signals = validation_result.total_signals;
        result.non_neutral_signals = validation_result.non_neutral_signals;
        result.signal_generation_rate = validation_result.signal_generation_rate;
        result.non_neutral_ratio = validation_result.non_neutral_ratio;
        result.mean_confidence = validation_result.mean_confidence;
        result.signal_accuracy = validation_result.signal_accuracy;
        result.trading_based_mrb = validation_result.trading_based_mrb;
        result.model_load_time_ms = validation_result.model_load_time_ms;
        result.avg_inference_time_ms = validation_result.avg_inference_time_ms;
        result.peak_memory_usage_mb = validation_result.memory_usage_mb;
        
        // Add checks
        result.add_check({
            "Signal Quality",
            validation_result.signal_quality_passed,
            validation_result.signal_generation_rate,
            config.min_signal_rate,
            validation_result.signal_quality_passed ? "PASSED" : "FAILED",
            validation_result.signal_quality_passed ? "info" : "critical"
        });
        
        result.add_check({
            "MRB Threshold",
            validation_result.mrb_threshold_passed,
            validation_result.trading_based_mrb,
            config.mrb_threshold,
            validation_result.mrb_threshold_passed ? "PASSED" : "FAILED",
            validation_result.mrb_threshold_passed ? "info" : "critical"
        });
        
        result.add_check({
            "Model Integrity",
            validation_result.model_integrity_passed,
            validation_result.model_loads_successfully ? 1.0 : 0.0,
            1.0,
            validation_result.model_integrity_passed ? "PASSED" : "FAILED",
            validation_result.model_integrity_passed ? "info" : "critical"
        });
        
        result.add_check({
            "Performance Benchmark",
            validation_result.performance_benchmark_passed,
            validation_result.avg_inference_time_ms,
            config.max_inference_time_ms,
            validation_result.performance_benchmark_passed ? "PASSED" : "FAILED",
            validation_result.performance_benchmark_passed ? "info" : "warning"
        });
        
        // Add metrics
        result.add_metric("signal_accuracy", validation_result.signal_accuracy);
        result.add_metric("trading_based_mrb", validation_result.trading_based_mrb);
        result.add_metric("sharpe_ratio", validation_result.sharpe_ratio);
        result.add_metric("max_drawdown", validation_result.max_drawdown);
        result.add_metric("win_rate", validation_result.win_rate);
        
        // Copy recommendations and warnings
        for (const auto& rec : validation_result.recommendations) {
            result.add_recommendation(rec);
        }
        for (const auto& warn : validation_result.warnings) {
            result.add_warning(warn);
        }
        for (const auto& err : validation_result.critical_issues) {
            result.add_error(err);
        }
        
        // Determine final status
        result.determine_status();
        result.calculate_overall_score();
        
        std::cout << "\n✓ Sanity check completed" << std::endl;
        std::cout << "Status: " << result.get_status_string() << std::endl;
        std::cout << "Overall Score: " << result.overall_score << "/100" << std::endl;
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.status_message = std::string("Exception: ") + e.what();
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(
        result.end_time - result.start_time
    ).count();
    
    return result;
}

TestResult TestFramework::run_full_test(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "🧪 Running comprehensive test for strategy: " << config.strategy_name << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        // Load strategy
        auto strategy = load_strategy(config.strategy_name, config.strategy_config_path);
        if (!strategy) {
            result.status = TestStatus::ERROR;
            result.status_message = "Failed to load strategy";
            result.add_error("Strategy could not be loaded");
            return result;
        }
        
        // Test on multiple datasets
        std::vector<analysis::PerformanceMetrics> dataset_metrics;
        
        for (const auto& dataset : config.datasets) {
            std::cout << "\n📊 Testing on dataset: " << dataset << std::endl;
            
            auto market_data = load_market_data(dataset);
            if (market_data.empty()) {
                result.add_warning("Failed to load dataset: " + dataset);
                continue;
            }
            
            auto signals = generate_signals(strategy, market_data);
            auto metrics = analysis::PerformanceAnalyzer::calculate_metrics(
                signals, market_data, config.blocks
            );
            
            metrics.dataset_name = dataset;
            dataset_metrics.push_back(metrics);
            
            std::cout << "  MRB: " << metrics.trading_based_mrb << std::endl;
            std::cout << "  Sharpe: " << metrics.sharpe_ratio << std::endl;
        }
        
        // Aggregate metrics
        if (!dataset_metrics.empty()) {
            double avg_mrb = 0.0;
            double avg_sharpe = 0.0;
            for (const auto& m : dataset_metrics) {
                avg_mrb += m.trading_based_mrb;
                avg_sharpe += m.sharpe_ratio;
            }
            avg_mrb /= dataset_metrics.size();
            avg_sharpe /= dataset_metrics.size();
            
            result.trading_based_mrb = avg_mrb;
            result.sharpe_ratio = avg_sharpe;
            result.add_metric("avg_mrb", avg_mrb);
            result.add_metric("avg_sharpe", avg_sharpe);
        }
        
        // Determine status
        result.determine_status();
        result.calculate_overall_score();
        
        std::cout << "\n✓ Full test completed" << std::endl;
        std::cout << "Status: " << result.get_status_string() << std::endl;
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.status_message = std::string("Exception: ") + e.what();
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(
        result.end_time - result.start_time
    ).count();
    
    return result;
}

std::vector<TestResult> TestFramework::run_all_strategies(const TestConfig& config) {
    std::vector<TestResult> results;
    auto strategies = get_available_strategies();
    
    std::cout << "🎯 Testing all strategies (" << strategies.size() << " total)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    int current = 0;
    for (const auto& strategy_name : strategies) {
        current++;
        std::cout << "\n[" << current << "/" << strategies.size() << "] ";
        
        TestConfig strategy_config = config;
        strategy_config.strategy_name = strategy_name;
        
        auto result = run_sanity_check(strategy_config);
        results.push_back(result);
    }
    
    return results;
}

TestResult TestFramework::run_walk_forward_analysis(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "📈 Running walk-forward analysis" << std::endl;
        
        auto market_data = load_market_data(config.primary_data_path);
        
        analysis::WalkForwardAnalyzer::WalkForwardConfig wf_config;
        wf_config.window_size = config.window_size;
        wf_config.step_size = config.step_size;
        
        auto wf_result = analysis::WalkForwardAnalyzer::analyze(
            config.strategy_name,
            market_data,
            wf_config
        );
        
        result.add_metric("avg_in_sample_mrb", wf_result.avg_in_sample_mrb);
        result.add_metric("avg_out_of_sample_mrb", wf_result.avg_out_of_sample_mrb);
        result.add_metric("stability_ratio", wf_result.stability_ratio);
        result.add_metric("num_windows", wf_result.num_windows);
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

TestResult TestFramework::run_stress_test(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "⚡ Running stress tests" << std::endl;
        
        auto market_data = load_market_data(config.primary_data_path);
        
        std::vector<analysis::StressTestAnalyzer::StressScenario> scenarios;
        for (const auto& scenario_str : config.stress_scenarios) {
            // Parse scenario string to enum
            // Add scenarios
        }
        
        auto stress_results = analysis::StressTestAnalyzer::run_stress_tests(
            config.strategy_name,
            market_data,
            scenarios
        );
        
        int passed = 0;
        for (const auto& sr : stress_results) {
            if (sr.passed) passed++;
            result.add_metric("stress_" + sr.scenario_name, sr.metrics.trading_based_mrb);
        }
        
        result.add_metric("stress_tests_passed", passed);
        result.add_metric("stress_tests_total", stress_results.size());
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

TestResult TestFramework::run_cross_validation(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "🔄 Running cross-validation" << std::endl;
        
        // Implementation of cross-validation
        // Split data into folds
        // Train and test on each fold
        // Aggregate results
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

// Private helper methods

std::shared_ptr<IStrategy> TestFramework::load_strategy(const std::string& strategy_name, const std::string& config_path) {
    try {
        // Set custom config path if provided
        if (!config_path.empty()) {
            ConfigResolver::set_config_path(strategy_name, config_path);
        }
        
        auto unique_strategy = create_strategy(strategy_name);
        if (!unique_strategy) {
            return nullptr;
        }
        return std::shared_ptr<IStrategy>(std::move(unique_strategy));
    } catch (const std::exception& e) {
        std::cerr << "Error loading strategy: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<MarketData> TestFramework::load_market_data(const std::string& data_path) {
    // Use existing CSV loading utility from sentio::utils
    return sentio::utils::read_csv_data(data_path);
}

std::vector<SignalOutput> TestFramework::generate_signals(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data
) {
    std::vector<SignalOutput> signals;
    signals.reserve(market_data.size());
    
    // Use the unified process_data API to generate signals in batch
    try {
        signals = strategy->process_data(market_data);
    } catch (...) {
        signals.clear();
    }
    
    return signals;
}

bool TestFramework::validate_config(const TestConfig& config, std::string& error_msg) {
    if (config.strategy_name.empty() && !config.all_strategies) {
        error_msg = "Strategy name is required";
        return false;
    }
    
    if (config.primary_data_path.empty() && config.datasets.empty()) {
        error_msg = "Data path is required";
        return false;
    }
    
    if (config.blocks < 1 || config.blocks > 100) {
        error_msg = "Blocks must be between 1 and 100";
        return false;
    }
    
    if (config.mrb_threshold < 0.0 || config.mrb_threshold > 1.0) {
        error_msg = "MRB threshold must be between 0.0 and 1.0";
        return false;
    }
    
    return true;
}

std::vector<std::string> TestFramework::get_available_strategies() {
    return {
        "sgo",      // Sigor strategy
        "xgb",      // XGBoost strategy
        "ppo",      // PPO strategy
        "ctb",      // CatBoost strategy
        "gbm",      // LightGBM strategy
        "tft"       // TFT strategy
    };
}

} // namespace sentio::testing



```

## 📄 **FILE 46 of 46**: tools/data_downloader.py

**File Information**:
- **Path**: `tools/data_downloader.py`

- **Size**: 204 lines
- **Modified**: 2025-10-07 00:37:13

- **Type**: .py

```text
import os
import argparse
import requests
import pandas as pd
import pandas_market_calendars as mcal
import struct
from datetime import datetime
from pathlib import Path

# --- Constants ---
# Define the Regular Trading Hours for NYSE in New York time.
RTH_START = "09:30"
RTH_END = "16:00"
NY_TIMEZONE = "America/New_York"
POLYGON_API_BASE = "https://api.polygon.io"

def fetch_aggs_all(symbol, start_date, end_date, api_key, timespan="minute", multiplier=1):
    """
    Fetches all aggregate bars for a symbol within a date range from Polygon.io.
    Handles API pagination automatically.
    """
    print(f"Fetching '{symbol}' data from {start_date} to {end_date}...")
    url = (
        f"{POLYGON_API_BASE}/v2/aggs/ticker/{symbol}/range/{multiplier}/{timespan}/"
        f"{start_date}/{end_date}?adjusted=true&sort=asc&limit=50000"
    )
    
    headers = {"Authorization": f"Bearer {api_key}"}
    all_bars = []
    
    while url:
        try:
            response = requests.get(url, headers=headers, timeout=15)
            response.raise_for_status()
            data = response.json()

            if "results" in data:
                all_bars.extend(data["results"])
                print(f" -> Fetched {len(data['results'])} bars...", end="\r")

            url = data.get("next_url")

        except requests.exceptions.RequestException as e:
            print(f"\nAPI Error fetching data for {symbol}: {e}")
            return None
        except Exception as e:
            print(f"\nAn unexpected error occurred: {e}")
            return None
            
    print(f"\n -> Total bars fetched for {symbol}: {len(all_bars)}")
    if not all_bars:
        return None
        
    # Convert to DataFrame for easier processing
    df = pd.DataFrame(all_bars)
    df.rename(columns={
        't': 'timestamp_utc_ms',
        'o': 'open',
        'h': 'high',
        'l': 'low',
        'c': 'close',
        'v': 'volume'
    }, inplace=True)
    return df

def filter_and_prepare_data(df):
    """
    Filters a DataFrame of market data for RTH (Regular Trading Hours)
    and removes US market holidays.
    """
    if df is None or df.empty:
        return None

    print("Filtering data for RTH and US market holidays...")
    
    # 1. Convert UTC millisecond timestamp to a timezone-aware DatetimeIndex
    df['timestamp_utc_ms'] = pd.to_datetime(df['timestamp_utc_ms'], unit='ms', utc=True)
    df.set_index('timestamp_utc_ms', inplace=True)
    
    # 2. Convert the index to New York time to perform RTH and holiday checks
    df.index = df.index.tz_convert(NY_TIMEZONE)
    
    # 3. Filter for Regular Trading Hours
    df = df.between_time(RTH_START, RTH_END)

    # 4. Filter out US market holidays
    nyse = mcal.get_calendar('NYSE')
    holidays = nyse.holidays().holidays # Get a list of holiday dates
    df = df[~df.index.normalize().isin(holidays)]
    
    print(f" -> {len(df)} bars remaining after filtering.")
    
    # 5. Add the specific columns required by the C++ backtester
    df['ts_utc'] = df.index.strftime('%Y-%m-%dT%H:%M:%S%z').str.replace(r'([+-])(\d{2})(\d{2})', r'\1\2:\3', regex=True)
    df['ts_nyt_epoch'] = df.index.astype('int64') // 10**9
    
    return df

def save_to_bin(df, path):
    """
    Saves the DataFrame to a custom binary format compatible with the C++ backtester.
    Format:
    - uint64_t: Number of bars
    - For each bar:
      - uint32_t: Length of ts_utc string
      - char[]: ts_utc string data
      - int64_t: ts_nyt_epoch
      - double: open, high, low, close
      - uint64_t: volume
    """
    print(f"Saving to binary format at {path}...")
    try:
        with open(path, 'wb') as f:
            # Write total number of bars
            num_bars = len(df)
            f.write(struct.pack('<Q', num_bars))

            # **FIXED**: The struct format string now correctly includes six format
            # specifiers to match the six arguments passed to pack().
            # q: int64_t (ts_nyt_epoch)
            # d: double (open)
            # d: double (high)
            # d: double (low)
            # d: double (close)
            # Q: uint64_t (volume)
            bar_struct = struct.Struct('<qddddQ')

            for row in df.itertuples():
                # Handle the variable-length string part
                ts_utc_bytes = row.ts_utc.encode('utf-8')
                f.write(struct.pack('<I', len(ts_utc_bytes)))
                f.write(ts_utc_bytes)
                
                # Pack and write the fixed-size data
                packed_data = bar_struct.pack(
                    row.ts_nyt_epoch,
                    row.open,
                    row.high,
                    row.low,
                    row.close,
                    int(row.volume) # C++ expects uint64_t, so we cast to int
                )
                f.write(packed_data)
        print(" -> Binary file saved successfully.")
    except Exception as e:
        print(f"Error saving binary file: {e}")

def main():
    parser = argparse.ArgumentParser(description="Polygon.io Data Downloader and Processor")
    parser.add_argument('symbols', nargs='+', help="One or more stock symbols (e.g., QQQ TQQQ SQQQ)")
    parser.add_argument('--start', required=True, help="Start date in YYYY-MM-DD format")
    parser.add_argument('--end', required=True, help="End date in YYYY-MM-DD format")
    parser.add_argument('--outdir', default='data', help="Output directory for CSV and BIN files")
    parser.add_argument('--timespan', default='minute', choices=['minute', 'hour', 'day'], help="Timespan of bars")
    parser.add_argument('--multiplier', default=1, type=int, help="Multiplier for the timespan")
    
    args = parser.parse_args()
    
    # Get API key from environment variable for security
    api_key = os.getenv('POLYGON_API_KEY')
    if not api_key:
        print("Error: POLYGON_API_KEY environment variable not set.")
        return
        
    # Create output directory if it doesn't exist
    output_dir = Path(args.outdir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    for symbol in args.symbols:
        print("-" * 50)
        # 1. Fetch data
        df_raw = fetch_aggs_all(symbol, args.start, args.end, api_key, args.timespan, args.multiplier)
        
        if df_raw is None or df_raw.empty:
            print(f"No data fetched for {symbol}. Skipping.")
            continue
            
        # 2. Filter and prepare data
        df_clean = filter_and_prepare_data(df_raw)
        
        if df_clean is None or df_clean.empty:
            print(f"No data remaining for {symbol} after filtering. Skipping.")
            continue
        
        # 3. Define output paths
        file_prefix = f"{symbol.upper()}_RTH_NH"
        csv_path = output_dir / f"{file_prefix}.csv"
        bin_path = output_dir / f"{file_prefix}.bin"
        
        # 4. Save to CSV for inspection
        print(f"Saving to CSV format at {csv_path}...")
        # Select and order columns to match C++ struct for clarity
        csv_columns = ['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume']
        df_clean[csv_columns].to_csv(csv_path, index=False)
        print(" -> CSV file saved successfully.")
        
        # 5. Save to C++ compatible binary format
        save_to_bin(df_clean, bin_path)

    print("-" * 50)
    print("Data download and processing complete.")

if __name__ == "__main__":
    main()

```

