#pragma once

#include "strategy/strategy_component.h"
#include "strategy/leveraged_position_manager.h"
#include "strategy/trading_state.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Configuration for LeveragedPPOStrategy
 */
struct LeveragedPPOConfig {
    // Model configuration
    std::string model_path = "artifacts/PPO/leveraged_trained/final_model_fixed.pt";  // ✅ FIXED: Use working model
    double initial_capital = 100000.0;
    
    // Position management
    double max_daily_loss_pct = 0.02;          // 2% max daily loss
    double max_position_size = 1.0;            // Maximum position size multiplier
    bool enable_emergency_exits = true;        // Enable emergency risk management
    
    // Trading parameters
    int warmup_bars = 64;                      // Minimum bars before trading
    int max_history_size = 500;               // Maximum market history to maintain
    bool enable_training_mode = false;        // Enable online learning
    
    // PPO-specific settings
    double confidence_threshold = 0.5;         // Minimum confidence for trading
    bool enable_action_masking = true;         // Use state transition constraints
    bool enable_debug_logging = false;         // Detailed logging for debugging
    
    // Risk management
    double leverage_limit = 3.5;              // Maximum allowed leverage
    double volatility_adjustment = true;       // Adjust position sizes based on volatility
    
    // Performance tracking
    size_t performance_history_size = 1000;   // Number of decisions to track
    bool enable_detailed_logging = false;     // Log detailed state transitions
};

/**
 * @brief Leveraged PPO Strategy - Sentio Integration Wrapper
 * 
 * This class provides a standard StrategyComponent interface for the
 * LeveragedPositionManager, allowing seamless integration with the
 * existing Sentio trading architecture.
 * 
 * The wrapper maintains compatibility with the Sentio signal-based
 * workflow while internally using the advanced PPO state machine
 * for position management decisions.
 */
class LeveragedPPOStrategy : public StrategyComponent {
public:
    explicit LeveragedPPOStrategy(const StrategyConfig& base_config, const LeveragedPPOConfig& ppo_config);
    ~LeveragedPPOStrategy() = default;

    // StrategyComponent interface implementation
    bool initialize();
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void reset();
    std::string get_strategy_name() const { return "leveraged_ppo"; }

    // LeveragedPPO specific methods
    
    /**
     * @brief Enable or disable training mode for online learning
     */
    void enable_training_mode(bool enable);
    
    /**
     * @brief Store trading outcome for reinforcement learning
     * Called after each trading decision to provide feedback to the PPO agent
     */
    void store_outcome(double reward, const Bar& result_bar, bool episode_done = false);
    
    /**
     * @brief Update the PPO policy with accumulated experiences
     * Should be called periodically (e.g., end of trading day)
     */
    void update_policy();
    
    /**
     * @brief Get current position allocation from state machine
     */
    strategy::PositionAllocation get_current_allocation() const;
    
    /**
     * @brief Get current effective leverage
     */
    double get_current_leverage() const;
    
    /**
     * @brief Get comprehensive performance metrics
     */
    strategy::LeveragedPositionManager::PerformanceMetrics get_performance_metrics() const;
    
    /**
     * @brief Get current state probabilities for analysis
     */
    std::vector<double> get_state_probabilities(const Bar& current_bar) const;
    
    /**
     * @brief Get detailed state transition log
     */
    std::string get_state_transition_log() const;
    
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
     * @brief Reset daily P&L tracking (call at start of new trading session)
     */
    void reset_daily_pnl();
    
    /**
     * @brief Update position state from external portfolio manager
     * Call this when positions are executed to keep the state machine in sync
     */
    void update_position_state(
        double cash_ratio,
        double qqq_ratio, 
        double tqqq_ratio,
        double sqqq_ratio,
        double unrealized_pnl
    );

private:
    LeveragedPPOConfig config_;
    std::unique_ptr<strategy::LeveragedPositionManager> position_manager_;
    
    // Market data history for PPO input
    std::deque<Bar> market_history_;
    
    // Signal conversion state
    strategy::TradingStateType last_state_;
    double last_confidence_;
    std::string last_reasoning_;
    
    // Performance tracking
    size_t total_signals_generated_;
    size_t successful_state_transitions_;
    double cumulative_reward_;
    
    /**
     * @brief Convert LeveragedPositionManager trading decision to SignalOutput
     */
    SignalOutput convert_decision_to_signal(
        const strategy::LeveragedPositionManager::TradingDecision& decision, 
        const Bar& bar, 
        int bar_index
    );
    
    /**
     * @brief Calculate reward for reinforcement learning based on price movement
     */
    double calculate_reward(const Bar& previous_bar, const Bar& current_bar) const;
    
    /**
     * @brief Check if sufficient market history is available
     */
    bool has_sufficient_history() const;
    
    /**
     * @brief Update internal tracking metrics
     */
    void update_performance_tracking(const strategy::LeveragedPositionManager::TradingDecision& decision);
    
    /**
     * @brief Log strategy performance summary
     */
    void log_performance_summary() const;
};

/**
 * @brief Factory methods for creating LeveragedPPOStrategy instances
 */
class LeveragedPPOStrategyFactory {
public:
    /**
     * @brief Create a production-ready strategy instance
     */
    static std::unique_ptr<LeveragedPPOStrategy> create_for_production(
        const std::string& model_path,
        double initial_capital = 100000.0
    );
    
    /**
     * @brief Create a training-enabled strategy instance
     */
    static std::unique_ptr<LeveragedPPOStrategy> create_for_training(
        const std::string& training_data_path,
        double initial_capital = 100000.0
    );
    
    /**
     * @brief Create a backtesting strategy instance with detailed logging
     */
    static std::unique_ptr<LeveragedPPOStrategy> create_for_backtesting(
        const std::string& model_path,
        double initial_capital = 100000.0
    );
};

} // namespace sentio
