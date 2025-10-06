#pragma once

#include "abstract_state_machine.h"
#include "common/types.h"
#include <torch/torch.h>
#include <vector>
#include <memory>
#include <map>
#include <optional>
#include <random>

namespace sentio::training {

/**
 * Training mode enumeration for different PPO variants
 */
enum class TrainingMode {
    SIMPLE_PPO,     // 5-state SimplePPO (conservative)
    LEVERAGED_PPO,  // 11-state LeveragedPPO (aggressive)
    LEGACY          // 3-action legacy compatibility
};

/**
 * Configuration for the state-aligned training environment
 */
struct StateAlignedTrainingConfig {
    TrainingMode mode = TrainingMode::SIMPLE_PPO;
    
    // Episode parameters
    int max_episode_length = 2000;
    
    // Reward function weights
    double reward_scale = 1.0;
    double transition_cost_weight = 0.1;
    double risk_penalty_weight = 0.05;
    double drawdown_penalty_weight = 0.2;
    double position_reward_weight = 1.0;
    
    // Environment features
    bool enable_action_masking = true;
    bool normalize_rewards = true;
    
    /**
     * Apply mode-specific default parameters
     */
    void apply_mode_defaults();
};

/**
 * Unified training environment that aligns with inference state machines
 * 
 * This environment uses AbstractStateMachine to ensure perfect consistency
 * between training and inference. It supports multiple PPO variants through
 * a configurable state machine architecture.
 */
class StateAlignedTrainingEnvironment {
public:
    /**
     * Environment state returned by reset() and step()
     */
    struct EnvironmentState {
        torch::Tensor observation;    // Current observation tensor
        double reward;               // Reward for this step
        bool done;                  // Episode termination flag
        torch::Tensor action_mask;   // Valid actions mask (if enabled)
        std::map<std::string, double> info;  // Additional information
    };
    
    /**
     * Reward components for analysis and debugging
     */
    struct RewardComponents {
        double transition_cost = 0.0;
        double risk_penalty = 0.0;
        double drawdown_penalty = 0.0;
        double position_reward = 0.0;
        double total_reward = 0.0;
    };
    
    /**
     * Constructor
     * @param market_data Historical market data for training
     * @param config Environment configuration
     */
    explicit StateAlignedTrainingEnvironment(
        const std::vector<Bar>& market_data,
        const StateAlignedTrainingConfig& config
    );
    
    /**
     * Reset environment to start of new episode
     * @return Initial environment state
     */
    EnvironmentState reset();
    
    /**
     * Take action and advance environment
     * @param action_id Action to take (state to transition to)
     * @return New environment state
     */
    EnvironmentState step(int action_id);
    
    // Environment information
    int get_action_dim() const { return state_machine_->get_num_states(); }
    int get_observation_dim() const { return 16; }  // Feature dimension
    int get_current_state_id() const { return current_state_id_; }
    
    // Validation and debugging
    std::vector<int> get_valid_actions() const;
    const RewardComponents& get_last_reward_components() const { return reward_components_; }
    
private:
    // Configuration and data
    const std::vector<Bar>& market_data_;
    StateAlignedTrainingConfig config_;
    TrainingMode mode_;
    
    // State machine
    std::unique_ptr<AbstractStateMachine> state_machine_;
    
    // Episode state
    int current_step_;
    int current_state_id_;
    int episode_start_idx_;
    
    // Performance tracking
    double episode_return_;
    double max_drawdown_;
    double peak_equity_;
    double current_equity_;
    
    // Reward analysis
    RewardComponents reward_components_;
    
    // Feature extraction (simplified for now)
    
    // Random number generation
    std::mt19937 rng_;
    
    // Internal methods
    void initialize_state_machine();
    void initialize_reward_components();
    
    torch::Tensor get_observation() const;
    torch::Tensor get_action_mask() const;
    
    double calculate_reward(int from_state, int to_state);
    void update_equity();
    
    bool is_episode_done() const;
    std::map<std::string, double> get_info() const;
};

/**
 * Factory for creating training environments
 */
class TrainingEnvironmentFactory {
public:
    /**
     * Create training environment with specified mode
     * @param market_data Historical market data
     * @param mode Training mode
     * @param config Optional configuration (uses defaults if not provided)
     * @return Unique pointer to training environment
     */
    static std::unique_ptr<StateAlignedTrainingEnvironment> create(
        const std::vector<Bar>& market_data,
        TrainingMode mode,
        const std::optional<StateAlignedTrainingConfig>& config = std::nullopt
    );
    
    /**
     * Get human-readable name for training mode
     */
    static std::string get_mode_name(TrainingMode mode);
    
    /**
     * Get default configuration for training mode
     */
    static StateAlignedTrainingConfig get_default_config(TrainingMode mode);
};

} // namespace sentio::training
