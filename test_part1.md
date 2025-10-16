# Test

**Part 1 of 2**

**Generated**: 2025-10-15 02:50:58
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: Review document: MULTI_SYMBOL_ROTATION_DETAILED_DESIGN.md (46 valid files)
**Description**: Test

**Part 1 Files**: See file count below

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

