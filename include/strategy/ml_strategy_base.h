#pragma once

#ifdef TORCH_AVAILABLE
#include <torch/torch.h>
#include <torch/script.h>
#endif

#include "strategy_component.h"
#include "common/json_utils.h"
#include "common/utils.h"
#include <deque>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Base class for ML strategies to consolidate common functionality
 * 
 * This class handles all common ML strategy operations:
 * - Model loading and validation
 * - Metadata parsing and configuration
 * - Feature buffer management
 * - Common PyTorch operations
 * - Error handling and logging
 */
class MLStrategyBase : public StrategyComponent {
protected:
#ifdef TORCH_AVAILABLE
    torch::jit::script::Module model_;
#endif
    std::deque<std::vector<double>> feature_buffer_;
    bool model_loaded_ = false;
    
    // Common ML configuration that all strategies need
    struct MLBaseConfig {
        std::string model_path;
        std::string metadata_path;
        int sequence_length = 64;
        int feature_count = 53;
        std::vector<double> feature_means;
        std::vector<double> feature_scales;
        std::vector<std::string> feature_names;
    };

public:
    explicit MLStrategyBase(const StrategyConfig& config) : StrategyComponent(config) {}
    virtual ~MLStrategyBase() = default;

    /**
     * @brief Get strategy version
     */
    std::string get_version() const { return "1.0.0"; }

    /**
     * @brief Check if strategy is ready for inference
     */
    bool is_ready() const { return model_loaded_ && feature_buffer_.size() >= get_sequence_length(); }

    /**
     * @brief Get current buffer size
     */
    size_t get_buffer_size() const { return feature_buffer_.size(); }

    /**
     * @brief Convert model logit output to probability
     */
    double logit_to_probability(double logit) const {
        return 1.0 / (1.0 + std::exp(-logit));
    }

protected:
    /**
     * @brief Initialize ML strategy (calls load_metadata and load_model)
     */
    bool initialize_ml_base(const std::string& strategy_name);

    /**
     * @brief Load TorchScript model from file
     */
    bool load_model_base(const std::string& model_path, const std::string& strategy_name);

    /**
     * @brief Load metadata and parse common configuration
     */
    bool load_metadata_base(const std::string& metadata_path, MLBaseConfig& config);

    /**
     * @brief Apply metadata configuration to derived strategy config
     */
    virtual void apply_metadata_config(const json_utils::JsonValue& metadata) = 0;

    /**
     * @brief Get model path from derived strategy
     */
    virtual std::string get_model_path() const = 0;

    /**
     * @brief Get metadata path from derived strategy
     */
    virtual std::string get_metadata_path() const = 0;

    /**
     * @brief Get sequence length (pure virtual - must be implemented by derived classes)
     */
    virtual size_t get_sequence_length() const = 0;
    
    /**
     * @brief Get feature count (pure virtual - must be implemented by derived classes)
     */
    virtual int get_feature_count() const = 0;
    
    /**
     * @brief Initialize strategy-specific components after base initialization
     */
    virtual bool initialize_strategy_specific() { return true; }

    /**
     * @brief Clear history buffers (common operation)
     */
    void clear_history_buffers();
};

} // namespace sentio
