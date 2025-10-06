#pragma once

#include <deque>
#include <vector>
#include <torch/torch.h>
#include <stdexcept>
#include <iostream>

namespace sentio {

/**
 * @brief Manages proper temporal feature sequences for transformer models
 * 
 * This class fixes the critical bug where features were repeated across
 * all timesteps instead of maintaining proper historical sequences.
 * It provides a clean interface for both training and inference to
 * maintain consistent temporal patterns that transformers can learn from.
 */
class FeatureSequenceManager {
private:
    std::deque<std::vector<double>> history_;
    size_t sequence_length_;
    size_t feature_dim_;
    
public:
    /**
     * @brief Constructor for sequence manager
     * @param seq_len Number of timesteps in sequence (e.g., 64)
     * @param feat_dim Number of features per timestep (e.g., 126)
     */
    FeatureSequenceManager(size_t seq_len, size_t feat_dim) 
        : sequence_length_(seq_len), feature_dim_(feat_dim) {
        if (seq_len == 0 || feat_dim == 0) {
            throw std::invalid_argument("Sequence length and feature dimension must be > 0");
        }
    }
    
    /**
     * @brief Add new features to the sequence history
     * @param features Feature vector for current timestep
     * 
     * Maintains a sliding window of the most recent features.
     * Older features are automatically removed when sequence_length is exceeded.
     */
    void add_features(const std::vector<double>& features) {
        if (features.size() != feature_dim_) {
            std::cerr << "Warning: Feature dimension mismatch. Expected " << feature_dim_ 
                      << ", got " << features.size() << std::endl;
        }
        
        history_.push_back(features);
        
        // Maintain sliding window
        if (history_.size() > sequence_length_) {
            history_.pop_front();
        }
    }
    
    /**
     * @brief Check if sequence is ready for inference
     * @return true if we have enough historical features
     */
    bool is_ready() const {
        return history_.size() == sequence_length_;
    }
    
    /**
     * @brief Get current history size for debugging
     * @return Number of feature vectors currently in history
     */
    size_t get_history_size() const {
        return history_.size();
    }
    
    /**
     * @brief Get current sequence size
     * @return Number of feature vectors currently stored
     */
    size_t current_size() const {
        return history_.size();
    }
    
    /**
     * @brief Generate tensor from feature sequence
     * @return Tensor of shape [sequence_length, feature_dim] with proper temporal ordering
     * 
     * Creates a proper temporal sequence where:
     * - tensor[0] = oldest features
     * - tensor[sequence_length-1] = newest features
     */
    torch::Tensor get_sequence_tensor() const {
        if (!is_ready()) {
            throw std::runtime_error("Sequence not ready. Need " + std::to_string(sequence_length_) + 
                                   " features, have " + std::to_string(history_.size()));
        }
        
        torch::Tensor sequence = torch::zeros({static_cast<long>(sequence_length_), 
                                               static_cast<long>(feature_dim_)});
        
        // Fill tensor with proper temporal ordering
        for (size_t seq_idx = 0; seq_idx < sequence_length_; ++seq_idx) {
            const auto& hist_features = history_[seq_idx];
            size_t max_features = std::min(feature_dim_, hist_features.size());
            
            for (size_t feat_idx = 0; feat_idx < max_features; ++feat_idx) {
                sequence[seq_idx][feat_idx] = hist_features[feat_idx];
            }
            
            // Zero-pad if features are shorter than expected
            // (remaining elements are already zero from initialization)
        }
        
        return sequence;
    }
    
    /**
     * @brief Get batched tensor for inference
     * @return Tensor of shape [1, sequence_length, feature_dim] ready for model input
     */
    torch::Tensor get_batched_tensor() const {
        auto sequence = get_sequence_tensor();
        return sequence.unsqueeze(0); // Add batch dimension
    }
    
    /**
     * @brief Get flattened feature vector (for legacy compatibility)
     * @return Flat vector suitable for torch::from_blob
     */
    std::vector<float> get_flat_features() const {
        auto sequence = get_sequence_tensor();
        std::vector<float> flat_features;
        flat_features.reserve(sequence_length_ * feature_dim_);
        
        // Flatten in row-major order
        for (int seq_idx = 0; seq_idx < sequence.size(0); ++seq_idx) {
            for (int feat_idx = 0; feat_idx < sequence.size(1); ++feat_idx) {
                flat_features.push_back(sequence[seq_idx][feat_idx].item<float>());
            }
        }
        
        return flat_features;
    }
    
    /**
     * @brief Reset sequence history
     * 
     * Clears all stored features. Useful for starting fresh
     * or when switching to different market conditions.
     */
    void reset() {
        history_.clear();
    }
    
    /**
     * @brief Get debug information about sequence state
     * @return String with current state information
     */
    std::string get_debug_info() const {
        return "FeatureSequenceManager [" + std::to_string(current_size()) + "/" + 
               std::to_string(sequence_length_) + " features, " + 
               std::to_string(feature_dim_) + " dims each]";
    }
    
    /**
     * @brief Validate sequence has proper temporal progression
     * @return true if sequence appears to have temporal variation
     */
    bool validate_temporal_diversity() const {
        if (!is_ready() || history_.size() < 2) {
            return false;
        }
        
        // Check if consecutive features are different
        // (prevents the old bug where all timesteps were identical)
        const auto& first = history_[0];
        const auto& last = history_[history_.size() - 1];
        
        double diff = 0.0;
        size_t min_size = std::min(first.size(), last.size());
        
        for (size_t i = 0; i < min_size; ++i) {
            diff += std::abs(first[i] - last[i]);
        }
        
        // If features are too similar across time, warn about potential issues
        return diff > 1e-6; // Threshold for detecting identical sequences
    }
};

} // namespace sentio
