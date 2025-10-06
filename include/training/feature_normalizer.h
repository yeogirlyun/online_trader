#pragma once

#include <vector>
#include <string>

namespace sentio {
namespace training {

/**
 * @brief Feature normalization for training-inference consistency
 * 
 * This class ensures that features are normalized using statistics computed
 * only from training data, preventing data leakage and ensuring consistency
 * between training and inference.
 */
class FeatureNormalizer {
public:
    /**
     * @brief Fit the normalizer on training features
     * @param training_features Vector of feature vectors from training data
     */
    void fit(const std::vector<std::vector<double>>& training_features);
    
    /**
     * @brief Transform features using fitted statistics
     * @param features Feature vector to normalize
     * @return Normalized feature vector
     */
    std::vector<double> transform(const std::vector<double>& features) const;
    
    /**
     * @brief Transform features in-place using fitted statistics
     * @param features Feature vector to normalize (modified in-place)
     */
    void transform_inplace(std::vector<double>& features) const;
    
    /**
     * @brief Save normalization statistics to file
     * @param path Path to save the statistics
     * @return True if successful
     */
    bool save(const std::string& path) const;
    
    /**
     * @brief Load normalization statistics from file
     * @param path Path to load the statistics from
     * @return True if successful
     */
    bool load(const std::string& path);
    
    /**
     * @brief Check if the normalizer has been fitted
     * @return True if fitted
     */
    bool is_fitted() const { return !means_.empty() && !stddevs_.empty(); }
    
    /**
     * @brief Get the number of features
     * @return Number of features
     */
    size_t get_num_features() const { return means_.size(); }

private:
    std::vector<double> means_;
    std::vector<double> stddevs_;
    
    static constexpr double EPSILON = 1e-8; // Prevent division by zero
};

} // namespace training
} // namespace sentio

