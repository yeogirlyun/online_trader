#pragma once

#include "strategy/strategy_component.h"
#include "strategy/trading_state.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Configuration for SimplePPOStrategy
 */
struct SimplePPOConfig {
    // Model configuration
    std::string model_path = "artifacts/PPO/simple_ppo_model.pt";
    double initial_capital = 100000.0;
    
    // Position management (conservative)
    double max_daily_loss_pct = 0.01;          // 1% max daily loss (conservative)
    double max_position_size = 0.9;            // Maximum 90% position size
    bool enable_emergency_exits = true;        // Enable emergency risk management
    
    // Trading parameters
    int warmup_bars = 64;                      // Minimum bars before trading
    int max_history_size = 300;               // Smaller history for simple strategy
    bool enable_training_mode = false;        // Enable online learning
    
    // PPO-specific settings (conservative)
    double confidence_threshold = 0.4;         // Lower threshold for more trades
    bool enable_action_masking = true;         // Use state transition constraints
    bool enable_debug_logging = false;         // Detailed logging for debugging
    
    // Risk management (no leverage)
    bool enable_leverage = false;              // NO LEVERAGE - QQQ/PSQ only
    double volatility_adjustment = true;       // Adjust position sizes based on volatility
    
    // Performance tracking
    size_t performance_history_size = 500;    // Smaller tracking for simple strategy
    bool enable_detailed_logging = false;     // Log detailed state transitions
};

/**
 * @brief Simple PPO Strategy - Conservative 5-State Implementation
 * 
 * This class provides a simplified version of the PPO strategy for comparison
 * and experimentation. It uses only 5 states and 2 instruments (no leverage).
 * 
 * Key Differences from LeveragedPPOStrategy:
 * - Only 5 trading states (vs 11)
 * - Only QQQ and PSQ instruments (no TQQQ/SQQQ leverage)
 * - More conservative risk management
 * - Simpler state transitions
 * - Lower confidence thresholds for more frequent trading
 */
class SimplePPOStrategy : public StrategyComponent {
public:
    // ✅ FIXED: Add target_symbol parameter (following SGO/XGBoost pattern)
    SimplePPOStrategy(const StrategyConfig& base_config, 
                     const SimplePPOConfig& ppo_config,
                     const std::string& target_symbol);
    ~SimplePPOStrategy() = default;

    // StrategyComponent interface implementation
    bool initialize();
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void reset();
    std::string get_strategy_name() const { return "simple_ppo"; }

    // SimplePPO specific methods
    
    /**
     * @brief Enable or disable training mode for online learning
     */
    void enable_training_mode(bool enable);
    
    /**
     * @brief Store trading outcome for reinforcement learning
     */
    void store_outcome(double reward, const Bar& result_bar, bool episode_done = false);
    
    /**
     * @brief Update the PPO policy with accumulated experiences
     */
    void update_policy();
    
    /**
     * @brief Get current position allocation from simple state machine
     */
    strategy::PositionAllocation get_current_allocation() const;
    
    /**
     * @brief Get current effective exposure (no leverage, so max 1.0)
     */
    double get_current_exposure() const;
    
    /**
     * @brief Get simple performance metrics
     */
    struct SimplePerformanceMetrics {
        double total_return = 0.0;
        size_t total_trades = 0;
        size_t successful_trades = 0;
        double win_rate = 0.0;
        double avg_return_per_trade = 0.0;
        double max_drawdown = 0.0;
        std::string current_state = "FLAT";
    };
    
    SimplePerformanceMetrics get_performance_metrics() const;
    
    /**
     * @brief Get current state probabilities for analysis
     */
    std::vector<double> get_state_probabilities(const Bar& current_bar) const;
    
    /**
     * @brief Log current position state for debugging
     */
    void log_current_state() const;
    
    /**
     * @brief Save the current PPO model to disk
     */
    bool save_model(const std::string& path = "") const;
    
    /**
     * @brief Load a PPO model from disk
     */
    bool load_model(const std::string& path = "");
    
    /**
     * @brief Reset daily P&L tracking
     */
    void reset_daily_pnl();
    
    /**
     * @brief Allow adapter to set dataset path for metadata
     */
    void set_dataset_path(const std::string& path);

private:
    SimplePPOConfig config_;
    std::string symbol_;              // ✅ ADDED: Store configured symbol (following SGO/XGBoost pattern)
    std::string dataset_path_;        // ✅ ADDED: Store dataset path for metadata
    
    // Simple state machine (5 states only)
    enum class SimpleStateType {
        FLAT = 0,           // All cash
        LIGHT_LONG,         // 30% QQQ
        MODERATE_LONG,      // 60% QQQ  
        LIGHT_SHORT,        // 30% PSQ
        MODERATE_SHORT,     // 60% PSQ
        COUNT               // Total: 5 states
    };
    
    // Market data history for PPO input
    std::deque<Bar> market_history_;
    
    // Signal conversion state
    SimpleStateType current_state_;
    SimpleStateType last_state_;
    double last_confidence_;
    std::string last_reasoning_;
    
    // Performance tracking
    size_t total_signals_generated_;
    size_t successful_state_transitions_;
    double cumulative_reward_;
    double current_pnl_;
    double max_drawdown_;
    
    /**
     * @brief Convert simple state to position allocation
     */
    strategy::PositionAllocation state_to_allocation(SimpleStateType state) const;
    
    /**
     * @brief Get valid transitions for simple state machine
     */
    std::vector<SimpleStateType> get_valid_transitions(SimpleStateType from_state) const;
    
    /**
     * @brief Simple PPO decision making (mock implementation for now)
     */
    SimpleStateType get_next_state(const Bar& current_bar, SimpleStateType current_state) const;
    
    /**
     * @brief Convert simple state decision to SignalOutput
     */
    SignalOutput convert_state_to_signal(
        SimpleStateType from_state,
        SimpleStateType to_state,
        double confidence,
        const Bar& bar, 
        int bar_index
    );
    
    /**
     * @brief Calculate reward for reinforcement learning
     */
    double calculate_reward(const Bar& previous_bar, const Bar& current_bar) const;
    
    /**
     * @brief Check if sufficient market history is available
     */
    bool has_sufficient_history() const;
    
    /**
     * @brief Update internal tracking metrics
     */
    void update_performance_tracking(SimpleStateType from_state, SimpleStateType to_state);
    
    /**
     * @brief Get state name for logging
     */
    std::string get_state_name(SimpleStateType state) const;
};

/**
 * @brief Factory methods for creating SimplePPOStrategy instances
 */
class SimplePPOStrategyFactory {
public:
    /**
     * @brief Create a conservative strategy instance for comparison
     */
    static std::unique_ptr<SimplePPOStrategy> create_for_comparison(
        double initial_capital = 100000.0
    );
    
    /**
     * @brief Create a training-enabled strategy instance
     */
    static std::unique_ptr<SimplePPOStrategy> create_for_training(
        const std::string& training_data_path,
        double initial_capital = 100000.0
    );
};

} // namespace sentio
