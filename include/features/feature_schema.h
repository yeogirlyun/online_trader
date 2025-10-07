#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace sentio {

/**
 * @brief Feature schema for reproducibility and validation
 *
 * Tracks feature names, version, and hash for model compatibility checking.
 */
struct FeatureSchema {
    std::vector<std::string> feature_names;
    int version{1};
    std::string hash;  // Hex digest of names + params

    /**
     * @brief Compute hash from feature names and version
     * @return Hex string hash (16 chars)
     */
    std::string compute_hash() const {
        std::stringstream ss;
        for (const auto& name : feature_names) {
            ss << name << "|";
        }
        ss << "v" << version;

        // Use std::hash as placeholder (use proper SHA256 in production)
        std::string s = ss.str();
        std::hash<std::string> hasher;
        size_t h = hasher(s);

        std::stringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << h;
        return hex.str();
    }

    /**
     * @brief Finalize schema by computing hash
     */
    void finalize() {
        hash = compute_hash();
    }

    /**
     * @brief Check if schema matches another
     * @param other Other schema to compare
     * @return true if compatible (same hash)
     */
    bool is_compatible(const FeatureSchema& other) const {
        return hash == other.hash && version == other.version;
    }
};

/**
 * @brief Feature snapshot with timestamp and schema
 */
struct FeatureSnapshot {
    uint64_t timestamp{0};
    uint64_t bar_id{0};
    std::vector<double> features;
    FeatureSchema schema;

    /**
     * @brief Check if snapshot is valid (size matches schema)
     * @return true if valid
     */
    bool is_valid() const {
        return features.size() == schema.feature_names.size();
    }
};

/**
 * @brief Replace NaN/Inf values with 0.0
 * @param features Feature vector to clean
 */
inline void nan_guard(std::vector<double>& features) {
    for (auto& f : features) {
        if (!std::isfinite(f)) {
            f = 0.0;
        }
    }
}

/**
 * @brief Clamp extreme feature values
 * @param features Feature vector to clamp
 * @param min_val Minimum allowed value
 * @param max_val Maximum allowed value
 */
inline void clamp_features(std::vector<double>& features,
                          double min_val = -1e6,
                          double max_val = 1e6) {
    for (auto& f : features) {
        f = std::clamp(f, min_val, max_val);
    }
}

/**
 * @brief Sanitize features: NaN guard + clamp
 * @param features Feature vector to sanitize
 */
inline void sanitize_features(std::vector<double>& features) {
    nan_guard(features);
    clamp_features(features);
}

/**
 * @brief Check if feature vector contains any invalid values
 * @param features Feature vector to check
 * @return true if all values are finite
 */
inline bool is_feature_vector_valid(const std::vector<double>& features) {
    return std::all_of(features.begin(), features.end(),
                      [](double f) { return std::isfinite(f); });
}

} // namespace sentio
