#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>

namespace sentio {
namespace features {

// =============================================================================
// Feature Scaler - Normalization with Persistence
// Supports both standard (mean/std) and robust (median/IQR) scaling
// =============================================================================

class Scaler {
public:
    enum class Type {
        STANDARD,  // (x - mean) / std
        ROBUST     // (x - median) / IQR
    };

    explicit Scaler(Type type = Type::STANDARD) : type_(type) {}

    // Fit scaler to training data
    void fit(const std::vector<std::vector<double>>& X) {
        if (X.empty()) return;

        size_t n_samples = X.size();
        size_t n_features = X[0].size();

        mean_.assign(n_features, 0.0);
        stdv_.assign(n_features, 0.0);
        median_.assign(n_features, 0.0);
        iqr_.assign(n_features, 1.0);

        if (type_ == Type::STANDARD) {
            fit_standard(X);
        } else {
            fit_robust(X);
        }

        fitted_ = true;
    }

    // Transform features in-place
    void transform_inplace(std::vector<double>& x) const {
        if (!fitted_) return;

        for (size_t j = 0; j < x.size() && j < mean_.size(); ++j) {
            if (std::isnan(x[j])) continue;

            if (type_ == Type::STANDARD) {
                x[j] = (x[j] - mean_[j]) / stdv_[j];
            } else {
                x[j] = (x[j] - median_[j]) / iqr_[j];
            }
        }
    }

    // Transform and return new vector
    std::vector<double> transform(const std::vector<double>& x) const {
        std::vector<double> result = x;
        transform_inplace(result);
        return result;
    }

    // Inverse transform (denormalize)
    void inverse_transform_inplace(std::vector<double>& x) const {
        if (!fitted_) return;

        for (size_t j = 0; j < x.size() && j < mean_.size(); ++j) {
            if (std::isnan(x[j])) continue;

            if (type_ == Type::STANDARD) {
                x[j] = x[j] * stdv_[j] + mean_[j];
            } else {
                x[j] = x[j] * iqr_[j] + median_[j];
            }
        }
    }

    // Serialize scaler state for persistence
    std::string save() const {
        std::ostringstream oss;
        oss << std::setprecision(17);

        // Save type
        oss << static_cast<int>(type_) << " ";

        // Save dimension
        oss << mean_.size() << " ";

        // Save parameters
        for (size_t j = 0; j < mean_.size(); ++j) {
            oss << mean_[j] << " " << stdv_[j] << " "
                << median_[j] << " " << iqr_[j] << " ";
        }

        return oss.str();
    }

    // Deserialize scaler state
    void load(const std::string& s) {
        std::istringstream iss(s);

        int type_int;
        size_t dim;

        iss >> type_int >> dim;
        type_ = static_cast<Type>(type_int);

        mean_.resize(dim);
        stdv_.resize(dim);
        median_.resize(dim);
        iqr_.resize(dim);

        for (size_t j = 0; j < dim; ++j) {
            iss >> mean_[j] >> stdv_[j] >> median_[j] >> iqr_[j];
        }

        fitted_ = true;
    }

    // Getters
    bool is_fitted() const { return fitted_; }
    const std::vector<double>& get_mean() const { return mean_; }
    const std::vector<double>& get_std() const { return stdv_; }
    const std::vector<double>& get_median() const { return median_; }
    const std::vector<double>& get_iqr() const { return iqr_; }

    void reset() {
        mean_.clear();
        stdv_.clear();
        median_.clear();
        iqr_.clear();
        fitted_ = false;
    }

private:
    Type type_;
    bool fitted_ = false;

    // Standard scaling parameters
    std::vector<double> mean_;
    std::vector<double> stdv_;

    // Robust scaling parameters
    std::vector<double> median_;
    std::vector<double> iqr_;

    void fit_standard(const std::vector<std::vector<double>>& X) {
        size_t n_samples = X.size();
        size_t n_features = X[0].size();

        // Compute mean
        for (const auto& row : X) {
            for (size_t j = 0; j < n_features; ++j) {
                if (!std::isnan(row[j])) {
                    mean_[j] += row[j];
                }
            }
        }
        for (size_t j = 0; j < n_features; ++j) {
            mean_[j] /= n_samples;
        }

        // Compute standard deviation
        for (const auto& row : X) {
            for (size_t j = 0; j < n_features; ++j) {
                if (!std::isnan(row[j])) {
                    double diff = row[j] - mean_[j];
                    stdv_[j] += diff * diff;
                }
            }
        }
        for (size_t j = 0; j < n_features; ++j) {
            stdv_[j] = std::sqrt(stdv_[j] / std::max<size_t>(1, n_samples - 1));
            // Avoid division by zero
            if (stdv_[j] == 0 || std::isnan(stdv_[j])) {
                stdv_[j] = 1.0;
            }
        }
    }

    void fit_robust(const std::vector<std::vector<double>>& X) {
        size_t n_features = X[0].size();

        // Transpose data for easier column access
        std::vector<std::vector<double>> features(n_features);
        for (size_t j = 0; j < n_features; ++j) {
            features[j].reserve(X.size());
            for (const auto& row : X) {
                if (!std::isnan(row[j])) {
                    features[j].push_back(row[j]);
                }
            }
        }

        // Compute median and IQR for each feature
        for (size_t j = 0; j < n_features; ++j) {
            if (features[j].empty()) {
                median_[j] = 0.0;
                iqr_[j] = 1.0;
                continue;
            }

            std::vector<double> sorted = features[j];
            std::sort(sorted.begin(), sorted.end());

            size_t n = sorted.size();

            // Median (50th percentile)
            if (n % 2 == 0) {
                median_[j] = (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
            } else {
                median_[j] = sorted[n/2];
            }

            // Q1 (25th percentile)
            double q1;
            size_t q1_idx = n / 4;
            q1 = sorted[q1_idx];

            // Q3 (75th percentile)
            double q3;
            size_t q3_idx = (3 * n) / 4;
            q3 = sorted[q3_idx];

            // IQR
            iqr_[j] = q3 - q1;
            if (iqr_[j] == 0 || std::isnan(iqr_[j])) {
                iqr_[j] = 1.0;
            }
        }
    }
};

} // namespace features
} // namespace sentio
