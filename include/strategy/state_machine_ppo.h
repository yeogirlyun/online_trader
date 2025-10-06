#pragma once

#include "strategy/trading_state.h"
#include "common/types.h"
#include <torch/torch.h>
#include <torch/script.h>
#include <memory>
#include <vector>
#include <filesystem>

namespace sentio {
namespace strategy {

/**
 * @brief PPO Neural Network for Trading State Machine
 * 
 * This PPO implementation learns optimal state transitions for leveraged
 * position management. Unlike traditional signal-based approaches, this
 * agent directly outputs state transition probabilities.
 */

struct StateMachineInput {
    // Market features (technical indicators, momentum, volatility, etc.)
    torch::Tensor market_features;  // [batch_size, market_feature_dim]
    
    // Current state (one-hot encoded)
    torch::Tensor current_state;    // [batch_size, num_states]
    
    // Position information
    torch::Tensor position_info;    // [batch_size, 6] - [cash_ratio, qqq_ratio, tqqq_ratio, sqqq_ratio, pnl, duration]
    
    // Market context
    torch::Tensor market_context;   // [batch_size, 4] - [trend_strength, volatility_regime, time_of_day, session]
};

/**
 * @brief Configuration for State Machine PPO
 */
struct StateMachinePPOConfig {
    // Network architecture
    size_t market_feature_dim = 91;     // Input market features (unified feature engine)
    size_t hidden_dim = 256;            // Hidden layer size
    size_t num_states = 11;             // ✅ CRITICAL FIX: Number of trading states (was 8, must be 11)
    // Inference policy controls
    bool enable_action_masking = true;   // Mask invalid transitions
    double hysteresis_margin = 0.10;     // Required prob margin to switch state
    int hysteresis_cooldown_bars = 3;    // Bars to wait after a switch
    
    // PPO hyperparameters  
    double learning_rate = 3e-4;
    double gamma = 0.99;                // Discount factor
    double gae_lambda = 0.95;           // GAE parameter
    double clip_epsilon = 0.2;          // PPO clipping parameter
    double entropy_coeff = 0.01;        // Entropy regularization
    double value_loss_coeff = 0.5;      // Value function loss weight
    
    // Training parameters
    size_t batch_size = 64;
    size_t epochs_per_update = 4;
    double max_grad_norm = 0.5;         // Gradient clipping
    
    // State machine parameters
    bool enable_invalid_action_masking = true;
    double invalid_action_penalty = -1.0;
    
    // Risk management
    double max_daily_loss = 0.05;       // 5% daily loss limit
    bool enable_emergency_exit = true;   // Allow emergency exit to FLAT
    
    StateMachinePPOConfig() = default;
};

/**
 * @brief PPO Actor Network for State Transitions
 */
class StateMachineActor : public torch::nn::Module {
public:
    StateMachineActor(const StateMachinePPOConfig& config);
    
    // Forward pass: market_features + current_state + position_info -> action_probs
    torch::Tensor forward(const StateMachineInput& input);
    
    // Get action probabilities with masking
    torch::Tensor get_action_probs(const StateMachineInput& input);
    
    // Sample action from policy
    torch::Tensor sample_action(const StateMachineInput& input);
    
    // Get log probability of actions
    torch::Tensor get_log_probs(const StateMachineInput& input, const torch::Tensor& actions);

private:
    torch::nn::Linear input_layer{nullptr};
    torch::nn::Linear hidden_layer{nullptr};
    torch::nn::Linear output_layer{nullptr};
    torch::nn::Dropout dropout{nullptr};
    
    StateMachinePPOConfig config_;
    
    // Apply action masking for invalid transitions
    torch::Tensor apply_action_mask(const torch::Tensor& logits, const torch::Tensor& current_state);
};

/**
 * @brief PPO Critic Network for Value Estimation
 */
class StateMachineCritic : public torch::nn::Module {
public:
    StateMachineCritic(const StateMachinePPOConfig& config);
    
    // Forward pass: same input as actor -> state value
    torch::Tensor forward(const StateMachineInput& input);

private:
    torch::nn::Linear input_layer{nullptr};
    torch::nn::Linear hidden_layer{nullptr};
    torch::nn::Linear output_layer{nullptr};
    torch::nn::Dropout dropout{nullptr};
    
    StateMachinePPOConfig config_;
};

/**
 * @brief Complete PPO Agent for State Machine Trading
 */
class StateMachinePPO {
public:
    StateMachinePPO(const StateMachinePPOConfig& config = StateMachinePPOConfig{});
    ~StateMachinePPO() = default;
    
    // Initialize networks and optimizers
    bool initialize();
    
    // Get next action (state transition) from current state and market data
    TradingStateType get_next_state(
        const StateMachineInput& input,
        TradingStateType current_state,
        bool deterministic = false
    );
    
    // Training interface
    struct Experience {
        StateMachineInput input;
        TradingStateType current_state;
        TradingStateType next_state;
        double reward;
        bool done;
        double value;
        double log_prob;
        
        Experience() = default;
    };
    
    // Store experience for training
    void store_experience(const Experience& exp);
    
    // Update policy using stored experiences
    void update_policy();
    
    // Clear experience buffer
    void clear_experiences();
    
    // Save/load model
    bool save_model(const std::string& path);
    bool load_model(const std::string& path);
    
    // TorchScript model loading (for exported models)
    bool load_torchscript_model(const std::string& path);
    
    // Get training statistics
    struct TrainingStats {
        double policy_loss = 0.0;
        double value_loss = 0.0;
        double entropy_loss = 0.0;
        double total_loss = 0.0;
        size_t num_updates = 0;
        
        TrainingStats() = default;
    };
    
    TrainingStats get_training_stats() const { return training_stats_; }

private:
    StateMachinePPOConfig config_;
    TradingState state_machine_;
    int cooldown_bars_remaining_ = 0;
    
    // Neural networks
    std::shared_ptr<StateMachineActor> actor_;
    std::shared_ptr<StateMachineCritic> critic_;
    
    // Optimizers
    std::shared_ptr<torch::optim::Adam> actor_optimizer_;
    std::shared_ptr<torch::optim::Adam> critic_optimizer_;
    
    // Experience buffer
    std::vector<Experience> experiences_;
    
    // Training statistics
    TrainingStats training_stats_;
    
    // TorchScript model support
    std::shared_ptr<torch::jit::script::Module> torchscript_model_;
    std::vector<int64_t> input_shape_ = {1, 16};  // Default input shape
    bool model_loaded_ = false;
    
    // Helper methods
    StateMachineInput prepare_input(
        const torch::Tensor& market_features,
        TradingStateType current_state,
        const torch::Tensor& position_info,
        const torch::Tensor& market_context = torch::Tensor{}
    );
    
    torch::Tensor state_to_tensor(TradingStateType state);
    TradingStateType tensor_to_state(const torch::Tensor& state_tensor);
    
    // PPO training implementation
    torch::Tensor calculate_gae(const std::vector<Experience>& experiences);
    void update_actor(const std::vector<Experience>& experiences, const torch::Tensor& advantages);
    void update_critic(const std::vector<Experience>& experiences, const torch::Tensor& returns);
};

/**
 * @brief Integration helper for converting market data to PPO input
 */
class MarketDataToPPOInput {
public:
    static StateMachineInput convert(
        const Bar& current_bar,
        const std::vector<Bar>& history,
        TradingStateType current_state,
        double cash_ratio,
        double qqq_ratio, 
        double tqqq_ratio,
        double sqqq_ratio,
        double current_pnl,
        int position_duration
    );
    
private:
    static torch::Tensor extract_market_features(const Bar& current_bar, const std::vector<Bar>& history);
    static torch::Tensor extract_market_context(const Bar& current_bar);
};

} // namespace strategy
} // namespace sentio
