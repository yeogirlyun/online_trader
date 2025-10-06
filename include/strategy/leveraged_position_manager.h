#pragma once

#include "strategy/state_machine_ppo.h"
#include "strategy/trading_state.h"
#include "strategy/strategy_component.h"
#include "strategy/model_loading_error.h"
#include "common/types.h"
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <optional>
#include <torch/script.h>

namespace sentio {
namespace strategy {

/**
 * @brief Leveraged Position Manager using PPO State Machine
 * 
 * This class integrates the PPO state machine into Sentio's architecture,
 * providing both direct order generation and compatibility with existing
 * signal-based strategies.
 * 
 * Key Features:
 * - Direct state machine trading (bypasses signal generation)
 * - Risk-aware position transitions
 * - Leveraged instrument management (QQQ, TQQQ, SQQQ)
 * - Emergency risk controls
 * - Training data collection for PPO updates
 */

struct LeveragedPositionManagerConfig {
    // PPO Configuration
    StateMachinePPOConfig ppo_config;
    
    // Model paths
    std::string model_path = "artifacts/PPO/state_machine_model.pt";
    std::string training_data_path = "artifacts/PPO/training_data/";
    
    // Trading parameters
    double initial_capital = 100000.0;
    double min_trade_value = 100.0;        // Minimum trade size
    bool enable_training_mode = false;     // Collect training data
    
    // Risk management
    double max_daily_loss = 0.05;          // 5% daily loss limit
    double max_position_size = 1.0;        // 100% of capital
    bool enable_emergency_exits = true;    // Allow panic selling
    
    // Performance tracking
    size_t performance_history_size = 1000;
    bool enable_detailed_logging = true;
    
    LeveragedPositionManagerConfig() = default;
};

/**
 * @brief Complete position management solution with PPO
 */
class LeveragedPositionManager {
public:
    LeveragedPositionManager(const LeveragedPositionManagerConfig& config);
    ~LeveragedPositionManager() = default;
    
    // Initialization
    bool initialize();
    bool load_model(const std::string& model_path = "");
    bool save_model(const std::string& model_path = "") const;
    
    // ======================================================================== 
    // PRIMARY INTERFACE: Direct State Machine Trading
    // ========================================================================
    
    struct TradingDecision {
        TradingStateType from_state;
        TradingStateType to_state;
        std::vector<StateTransitionOrder> orders;
        double confidence;          // PPO confidence in decision
        std::string reasoning;      // Human-readable explanation
        bool emergency_exit;        // Was this an emergency decision?
        
        TradingDecision() : from_state(TradingStateType::FLAT), to_state(TradingStateType::FLAT),
                           confidence(0.0), emergency_exit(false) {}
    };
    
    /**
     * @brief Get trading decision for current market conditions
     * 
     * This is the main interface - given current market data and position state,
     * returns PPO's decision on what state transition to make.
     * 
     * @return std::optional<TradingDecision> - nullopt if model inference fails
     * @throws ModelInferenceError for critical inference failures
     */
    std::optional<TradingDecision> get_trading_decision(
        const Bar& current_bar,
        const std::vector<Bar>& market_history,
        bool deterministic = false
    );

    /**
     * @brief Validate model loading and perform test inference
     * 
     * This method should be called during initialization to ensure the model
     * is properly loaded and can perform inference. It performs a test forward
     * pass with dummy data to catch model loading issues early.
     * 
     * @throws ModelLoadingError if model validation fails
     */
    void validate_model();

    /**
     * @brief Check if model is loaded and ready for inference
     * 
     * @return true if model is loaded and validated, false otherwise
     */
    bool is_model_ready() const;
    
    // ========================================================================
    // COMPATIBILITY INTERFACE: Signal-based Integration  
    // ========================================================================
    
    /**
     * @brief Convert state machine decision to traditional signal format
     * 
     * This allows integration with existing Sentio signal processing pipeline
     * while still using the advanced PPO state machine internally.
     */
    SignalOutput convert_to_signal(const TradingDecision& decision, const Bar& bar, int bar_index);
    
    // ========================================================================
    // POSITION STATE MANAGEMENT
    // ========================================================================
    
    // Current position state
    TradingStateType get_current_state() const { return current_state_; }
    PositionAllocation get_current_allocation() const;
    double get_current_leverage() const;
    
    // Position updates (called by trading system after order execution)
    void update_position_state(
        double cash_ratio,
        double qqq_ratio, 
        double tqqq_ratio,
        double sqqq_ratio,
        double unrealized_pnl
    );
    
    // Position history and performance
    struct PositionSnapshot {
        TradingStateType state;
        PositionAllocation allocation;
        double total_value;
        double unrealized_pnl;
        double daily_pnl;
        int64_t timestamp_ms;
        
        PositionSnapshot() : state(TradingStateType::FLAT), total_value(0), 
                           unrealized_pnl(0), daily_pnl(0), timestamp_ms(0) {}
    };
    
    const std::deque<PositionSnapshot>& get_position_history() const { return position_history_; }
    
    // ========================================================================
    // TRAINING AND LEARNING
    // ========================================================================
    
    // Training mode (collect experiences for PPO updates)
    void enable_training_mode(bool enable = true);
    bool is_training_mode() const { return config_.enable_training_mode; }
    
    // Store outcome of previous decision (for PPO learning)
    void store_outcome(
        double reward,
        const Bar& result_bar,
        bool episode_done = false
    );
    
    // Update PPO policy (call periodically during training)
    void update_policy();
    
    // Training statistics
    struct TrainingProgress {
        size_t total_decisions = 0;
        size_t successful_transitions = 0;
        double average_reward = 0.0;
        double total_return = 0.0;
        size_t policy_updates = 0;
        
        TrainingProgress() = default;
    };
    
    TrainingProgress get_training_progress() const { return training_progress_; }
    
    // ========================================================================
    // RISK MANAGEMENT
    // ========================================================================
    
    // Risk checks (called before executing decisions)
    bool check_risk_limits(const TradingDecision& decision) const;
    bool is_emergency_exit_needed() const;
    TradingDecision create_emergency_exit();
    
    // Daily P&L tracking
    void reset_daily_pnl();
    double get_daily_pnl() const { return daily_pnl_; }
    double get_max_daily_loss() const { return config_.max_daily_loss; }
    
    // ========================================================================
    // PERFORMANCE ANALYTICS
    // ========================================================================
    
    struct PerformanceMetrics {
        // Returns
        double total_return = 0.0;
        double annualized_return = 0.0;
        double sharpe_ratio = 0.0;
        double max_drawdown = 0.0;
        
        // State machine specific
        double avg_position_duration = 0.0;
        size_t total_state_transitions = 0;
        std::vector<std::pair<TradingStateType, size_t>> state_frequency;
        
        // Risk metrics
        double avg_leverage = 0.0;
        double max_leverage = 0.0;
        size_t emergency_exits = 0;
        
        PerformanceMetrics() = default;
    };
    
    PerformanceMetrics calculate_performance_metrics() const;
    
    // ========================================================================
    // DEBUGGING AND MONITORING
    // ========================================================================
    
    // State machine visualization
    std::string get_state_transition_log() const;
    void log_current_state() const;
    
    // Model inspection
    std::vector<double> get_current_state_probabilities(const Bar& current_bar, const std::vector<Bar>& history);

private:
    LeveragedPositionManagerConfig config_;
    TradingState state_machine_;
    std::unique_ptr<StateMachinePPO> ppo_agent_;
    
    // Model validation state
    bool model_validated_ = false;
    std::string model_validation_error_;
    
    // Position state
    TradingStateType current_state_;
    double cash_ratio_;
    double qqq_ratio_;
    double tqqq_ratio_; 
    double sqqq_ratio_;
    double current_portfolio_value_;
    double unrealized_pnl_;
    double daily_pnl_;
    int64_t session_start_time_;
    int position_duration_; // Bars since last state change
    
    // Position tracking
    std::deque<PositionSnapshot> position_history_;
    
    // Training data
    struct PendingExperience {
        StateMachineInput input;
        TradingStateType from_state;
        TradingStateType to_state;
        double predicted_value;
        double log_prob;
        int64_t timestamp_ms;
        bool outcome_stored;
        
        PendingExperience() : from_state(TradingStateType::FLAT), 
                             to_state(TradingStateType::FLAT),
                             predicted_value(0), log_prob(0), 
                             timestamp_ms(0), outcome_stored(false) {}
    };
    
    std::vector<PendingExperience> pending_experiences_;
    TrainingProgress training_progress_;
    
    // State transition tracking
    std::vector<std::pair<TradingStateType, TradingStateType>> recent_transitions_;
    
    // Helper methods
    StateMachineInput prepare_market_input(const Bar& current_bar, const std::vector<Bar>& history);
    void update_position_history(const PositionSnapshot& snapshot);
    bool validate_state_consistency();
    
    // Risk management helpers
    bool is_within_risk_limits(const TradingDecision& decision) const;
    double calculate_transition_risk(TradingStateType from_state, TradingStateType to_state) const;
    
    // Performance tracking helpers
    void update_training_progress(const TradingDecision& decision, double reward);
    PositionSnapshot create_position_snapshot() const;
    
    // Logging helpers
    void log_state_transition(const TradingDecision& decision);
    void log_risk_event(const std::string& event, double severity);
};

/**
 * @brief Factory for creating LeveragedPositionManager with different configurations
 */
class LeveragedPositionManagerFactory {
public:
    // Create for production trading (deterministic decisions)
    static std::unique_ptr<LeveragedPositionManager> create_for_production(
        const std::string& model_path,
        double initial_capital = 100000.0
    );
    
    // Create for training (exploration enabled)
    static std::unique_ptr<LeveragedPositionManager> create_for_training(
        const std::string& training_data_path,
        double initial_capital = 100000.0
    );
    
    // Create for backtesting (detailed logging)
    static std::unique_ptr<LeveragedPositionManager> create_for_backtesting(
        const std::string& model_path,
        double initial_capital = 100000.0
    );
};

} // namespace strategy
} // namespace sentio
