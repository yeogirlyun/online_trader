#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace sentio {
namespace features {

/**
 * @brief XGBoost Feature Order Definition - CRITICAL for Training-Inference Consistency
 * 
 * This class defines the EXACT feature order used during Python training.
 * Any deviation from this order will cause catastrophic performance degradation.
 * 
 * Source: artifacts/XGBoost/professional_v3/model_config.json
 * Training Performance: 85.35% accuracy, 92.91% AUC
 */
class XGBoostFeatureOrder {
public:
    /**
     * @brief The EXACT feature order from training (25 intraday features)
     * 
     * CRITICAL: Do NOT change this order without retraining the model!
     * Each feature must be computed exactly as in training.
     */
    static const std::vector<std::string> FEATURE_ORDER;
    
    /**
     * @brief Get feature name to index mapping for O(1) lookups
     */
    static std::unordered_map<std::string, int> get_feature_index_map() {
        std::unordered_map<std::string, int> map;
        for (size_t i = 0; i < FEATURE_ORDER.size(); ++i) {
            map[FEATURE_ORDER[i]] = static_cast<int>(i);
        }
        return map;
    }
    
    /**
     * @brief Get expected feature count
     */
    static constexpr size_t EXPECTED_FEATURE_COUNT = 25;
    
    /**
     * @brief Validate feature vector size
     */
    static bool validate_feature_count(size_t actual_count) {
        return actual_count == EXPECTED_FEATURE_COUNT;
    }
    
    /**
     * @brief Get feature name by index (bounds checked)
     */
    static const std::string& get_feature_name(size_t index) {
        static const std::string INVALID_FEATURE = "INVALID_INDEX";
        if (index >= FEATURE_ORDER.size()) {
            return INVALID_FEATURE;
        }
        return FEATURE_ORDER[index];
    }
};

} // namespace features
} // namespace sentio
