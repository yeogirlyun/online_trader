#pragma once

#include "state_aligned_training_environment.h"
#include "abstract_state_machine.h"
#include "../common/types.h"
#include <torch/torch.h>
#include <memory>
#include <vector>
#include <string>

namespace sentio::training {

/**
 * Enhanced PPO configuration with mode support
 */
struct EnhancedPpoTrainerConfig {
    // Training mode
    TrainingMode mode = TrainingMode::SIMPLE_PPO;
    
    // Model architecture
    int hidden_size = 256;
    int num_layers = 3;
    double dropout_rate = 0.1;
    bool use_lstm = false;
    int lstm_hidden_size = 128;
    
    // PPO hyperparameters
    double learning_rate = 3e-4;
    double gamma = 0.99;
    double gae_lambda = 0.95;
    double clip_epsilon = 0.2;
    double value_loss_coef = 0.5;
    double entropy_coef = 0.01;
    double max_grad_norm = 0.5;
    
    // Training parameters
    int num_epochs = 100;
    int batch_size = 512;
    int num_mini_batches = 4;
    int num_parallel_envs = 8;
    int steps_per_update = 2048;
    
    // Environment configuration
    StateAlignedTrainingConfig env_config;
    
    // Data paths
    std::string train_data_path = "data/equities/QQQ_RTH_NH.csv";
    std::string validation_data_path = "";  // If empty, split from train data
    
    // Output configuration
    std::string output_dir = "artifacts/PPO/";
    std::string model_name = "state_aligned_ppo";
    bool save_checkpoints = true;
    int checkpoint_interval = 10;
    
    // Export options
    bool export_torchscript = true;
    bool export_onnx = false;
    
    // Logging
    bool verbose = true;
    int log_interval = 10;
    bool use_tensorboard = false;
    std::string tensorboard_dir = "runs/";
};

/**
 * PPO Actor-Critic network with configurable action space
 */
class PPOActorCritic : public torch::nn::Module {
public:
    PPOActorCritic(int observation_dim, int action_dim, 
                   const EnhancedPpoTrainerConfig& config);
    
    struct Output {
        torch::Tensor action_logits;  // [batch_size, action_dim]
        torch::Tensor value;          // [batch_size, 1]
        torch::Tensor hidden_state;   // LSTM hidden state if applicable
    };
    
    Output forward(torch::Tensor observation, 
                   torch::Tensor state_features = torch::Tensor(),
                   torch::Tensor hidden_state = torch::Tensor());
    
    torch::Tensor get_action_probs(torch::Tensor observation,
                                   torch::Tensor action_mask = torch::Tensor());
    
    std::pair<torch::Tensor, torch::Tensor> evaluate_actions(
        torch::Tensor observations,
        torch::Tensor actions,
        torch::Tensor action_masks = torch::Tensor()
    );
    
private:
    // Shared feature extractor
    torch::nn::Sequential shared_layers_;
    
    // Actor head (policy)
    torch::nn::Sequential actor_head_;
    
    // Critic head (value)
    torch::nn::Sequential critic_head_;
    
    // Optional LSTM
    torch::nn::LSTM lstm_{nullptr};
    
    // Configuration
    int observation_dim_;
    int action_dim_;
    bool use_lstm_;
    int lstm_hidden_size_;
};

/**
 * PPO training buffer for experience replay
 */
class PPOBuffer {
public:
    PPOBuffer(int capacity, int observation_dim, int action_dim);
    
    void add(
        torch::Tensor observation,
        torch::Tensor action,
        torch::Tensor reward,
        torch::Tensor value,
        torch::Tensor log_prob,
        torch::Tensor done,
        torch::Tensor action_mask = torch::Tensor()
    );
    
    struct Batch {
        torch::Tensor observations;
        torch::Tensor actions;
        torch::Tensor rewards;
        torch::Tensor values;
        torch::Tensor log_probs;
        torch::Tensor dones;
        torch::Tensor advantages;
        torch::Tensor returns;
        torch::Tensor action_masks;
    };
    
    Batch get_batch();
    std::vector<Batch> get_mini_batches(int num_mini_batches);
    
    void compute_returns_and_advantages(double gamma, double gae_lambda);
    void clear();
    
    bool is_full() const { return size_ >= capacity_; }
    int size() const { return size_; }
    
private:
    int capacity_;
    int observation_dim_;
    int action_dim_;
    int size_;
    int position_;
    
    // Storage tensors
    torch::Tensor observations_;
    torch::Tensor actions_;
    torch::Tensor rewards_;
    torch::Tensor values_;
    torch::Tensor log_probs_;
    torch::Tensor dones_;
    torch::Tensor advantages_;
    torch::Tensor returns_;
    torch::Tensor action_masks_;
};

/**
 * Enhanced PPO Trainer with state-aligned training
 */
class EnhancedCppPpoTrainer {
public:
    explicit EnhancedCppPpoTrainer(const EnhancedPpoTrainerConfig& config);
    ~EnhancedCppPpoTrainer() = default;
    
    // Training interface
    bool initialize();
    bool train();
    bool validate();
    bool export_model();
    
    // Mode-specific training
    bool train_simple_ppo();
    bool train_leveraged_ppo();
    bool train_legacy_ppo();
    
    // Model management
    bool save_checkpoint(const std::string& path);
    bool load_checkpoint(const std::string& path);
    
    // Performance metrics
    struct TrainingMetrics {
        double average_reward;
        double average_return;
        double average_episode_length;
        double policy_loss;
        double value_loss;
        double entropy;
        double learning_rate;
        double win_rate;
        double sharpe_ratio;
    };
    
    TrainingMetrics get_current_metrics() const { return current_metrics_; }
    std::vector<TrainingMetrics> get_training_history() const { return training_history_; }
    
private:
    // Configuration
    EnhancedPpoTrainerConfig config_;
    TrainingMode current_mode_;
    
    // Environments
    std::vector<std::unique_ptr<StateAlignedTrainingEnvironment>> training_envs_;
    std::unique_ptr<StateAlignedTrainingEnvironment> validation_env_;
    
    // Model
    std::shared_ptr<PPOActorCritic> model_;
    std::unique_ptr<torch::optim::Adam> optimizer_;
    
    // Training buffer
    std::unique_ptr<PPOBuffer> buffer_;
    
    // Training state
    int current_epoch_;
    int total_steps_;
    TrainingMetrics current_metrics_;
    std::vector<TrainingMetrics> training_history_;
    
    // Data
    std::vector<Bar> train_data_;
    std::vector<Bar> validation_data_;
    
    // Helper methods
    bool load_data();
    bool setup_environments();
    bool setup_model();
    
    // Training loop components
    void collect_rollouts();
    void update_policy();
    double compute_policy_loss(const PPOBuffer::Batch& batch);
    double compute_value_loss(const PPOBuffer::Batch& batch);
    double compute_entropy(torch::Tensor logits);
    
    // Validation and metrics
    void update_metrics();
    void log_progress();
    
    // Export utilities
    bool export_torchscript(const std::string& path);
    bool export_onnx(const std::string& path);
};

/**
 * Model exporter for deployment
 */
class StateAlignedModelExporter {
public:
    struct ExportConfig {
        TrainingMode mode;
        std::string model_path;
        std::string output_path;
        bool optimize_for_inference = true;
        bool validate_export = true;
        bool include_metadata = true;
    };
    
    static bool export_model(
        std::shared_ptr<PPOActorCritic> model,
        const ExportConfig& config
    );
    
    static bool validate_exported_model(
        const std::string& model_path,
        TrainingMode mode
    );
    
    static torch::jit::script::Module prepare_for_export(
        std::shared_ptr<PPOActorCritic> model,
        TrainingMode mode
    );
    
private:
    static void add_metadata(
        torch::jit::script::Module& module,
        const ExportConfig& config
    );
    
    static bool run_validation_tests(
        const torch::jit::script::Module& module,
        TrainingMode mode
    );
};

} // namespace sentio::training

