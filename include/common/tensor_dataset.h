#pragma once

#include <torch/torch.h>

namespace sentio {

/**
 * TensorDataset for efficient PyTorch data loading
 * 
 * This class provides a PyTorch-compatible dataset interface for loading
 * pre-processed tensor data from binary files. It serves as a bridge between
 * our binary data format and PyTorch's data loader system.
 */
class TensorDataset : public torch::data::datasets::Dataset<TensorDataset> {
public:
    /**
     * Constructor
     * @param features Input features tensor [N x sequence_length x feature_dim]  
     * @param targets Target values tensor [N x 1]
     */
    TensorDataset(torch::Tensor features, torch::Tensor targets)
        : features_(std::move(features)), targets_(std::move(targets)) {}

    /**
     * Get a single data example by index
     * @param index Sample index
     * @return PyTorch data example containing features and target
     */
    torch::data::Example<> get(size_t index) override {
        return {features_[index], targets_[index]};
    }

    /**
     * Get the total number of samples in the dataset
     * @return Number of samples
     */
    torch::optional<size_t> size() const override {
        return features_.size(0);
    }

    /**
     * Get the complete features tensor (for inspection/debugging)
     * @return Features tensor
     */
    torch::Tensor get_features() const { return features_; }

    /**
     * Get the complete targets tensor (for inspection/debugging)
     * @return Targets tensor
     */
    torch::Tensor get_targets() const { return targets_; }

private:
    torch::Tensor features_;  ///< Input features [N x sequence_length x feature_dim]
    torch::Tensor targets_;   ///< Target values [N x 1]
};

} // namespace sentio
