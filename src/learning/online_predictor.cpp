#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>

namespace sentio {
namespace learning {

OnlinePredictor::OnlinePredictor(size_t num_features, const Config& config)
    : config_(config), num_features_(num_features), samples_seen_(0),
      current_lambda_(config.lambda) {
    
    // Initialize parameters to zero
    theta_ = Eigen::VectorXd::Zero(num_features);
    
    // Initialize covariance with high uncertainty
    P_ = Eigen::MatrixXd::Identity(num_features, num_features) * config.initial_variance;
    
    utils::log_info("OnlinePredictor initialized with " + std::to_string(num_features) + 
                   " features, lambda=" + std::to_string(config.lambda));
}

OnlinePredictor::PredictionResult OnlinePredictor::predict(const std::vector<double>& features) {
    PredictionResult result;
    result.is_ready = is_ready();
    
    if (!result.is_ready) {
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }
    
    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Linear prediction
    result.predicted_return = theta_.dot(x);
    
    // Confidence from prediction variance
    double prediction_variance = x.transpose() * P_ * x;
    result.confidence = 1.0 / (1.0 + std::sqrt(prediction_variance));
    
    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();
    
    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;
    
    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }
    
    // Convert to Eigen
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());
    
    // Current prediction
    double predicted = theta_.dot(x);
    double error = actual_return - predicted;
    
    // Store error for diagnostics
    recent_errors_.push_back(error);
    if (recent_errors_.size() > HISTORY_SIZE) {
        recent_errors_.pop_front();
    }
    
    // Store direction accuracy
    bool correct_direction = (predicted > 0 && actual_return > 0) || 
                           (predicted < 0 && actual_return < 0);
    recent_directions_.push_back(correct_direction);
    if (recent_directions_.size() > HISTORY_SIZE) {
        recent_directions_.pop_front();
    }
    
    // EWRLS update with regularization
    double lambda_reg = current_lambda_ + config_.regularization;
    
    // Kalman gain
    Eigen::VectorXd Px = P_ * x;
    double denominator = lambda_reg + x.dot(Px);
    
    if (std::abs(denominator) < EPSILON) {
        utils::log_warning("Near-zero denominator in EWRLS update, skipping");
        return;
    }
    
    Eigen::VectorXd k = Px / denominator;
    
    // Update parameters
    theta_ += k * error;
    
    // Update covariance
    P_ = (P_ - k * x.transpose() * P_) / current_lambda_;
    
    // Ensure numerical stability
    ensure_positive_definite();
    
    // Adapt learning rate if enabled
    if (config_.adaptive_learning && samples_seen_ % 10 == 0) {
        adapt_learning_rate(estimate_volatility());
    }
}

OnlinePredictor::PredictionResult OnlinePredictor::predict_and_update(
    const std::vector<double>& features, double actual_return) {
    
    auto result = predict(features);
    update(features, actual_return);
    return result;
}

void OnlinePredictor::adapt_learning_rate(double market_volatility) {
    // Higher volatility -> faster adaptation (lower lambda)
    // Lower volatility -> slower adaptation (higher lambda)
    
    double baseline_vol = 0.001;  // 0.1% baseline volatility
    double vol_ratio = market_volatility / baseline_vol;
    
    // Map volatility ratio to lambda
    // High vol (ratio=2) -> lambda=0.990
    // Low vol (ratio=0.5) -> lambda=0.999
    double target_lambda = config_.lambda - 0.005 * std::log(vol_ratio);
    target_lambda = std::clamp(target_lambda, config_.min_lambda, config_.max_lambda);
    
    // Smooth transition
    current_lambda_ = 0.9 * current_lambda_ + 0.1 * target_lambda;
    
    utils::log_debug("Adapted lambda: " + std::to_string(current_lambda_) + 
                    " (volatility=" + std::to_string(market_volatility) + ")");
}

bool OnlinePredictor::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Save config
        file.write(reinterpret_cast<const char*>(&config_), sizeof(Config));
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_lambda_), sizeof(double));
        
        // Save theta
        file.write(reinterpret_cast<const char*>(theta_.data()), 
                  sizeof(double) * theta_.size());
        
        // Save P (covariance)
        file.write(reinterpret_cast<const char*>(P_.data()), 
                  sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Saved predictor state to: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlinePredictor::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Load config
        file.read(reinterpret_cast<char*>(&config_), sizeof(Config));
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_lambda_), sizeof(double));
        
        // Load theta
        theta_.resize(num_features_);
        file.read(reinterpret_cast<char*>(theta_.data()), 
                 sizeof(double) * theta_.size());
        
        // Load P
        P_.resize(num_features_, num_features_);
        file.read(reinterpret_cast<char*>(P_.data()), 
                 sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Loaded predictor state from: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

double OnlinePredictor::get_recent_rmse() const {
    if (recent_errors_.empty()) return 0.0;
    
    double sum_sq = 0.0;
    for (double error : recent_errors_) {
        sum_sq += error * error;
    }
    return std::sqrt(sum_sq / recent_errors_.size());
}

double OnlinePredictor::get_directional_accuracy() const {
    if (recent_directions_.empty()) return 0.5;
    
    int correct = std::count(recent_directions_.begin(), recent_directions_.end(), true);
    return static_cast<double>(correct) / recent_directions_.size();
}

std::vector<double> OnlinePredictor::get_feature_importance() const {
    // Feature importance based on parameter magnitude * covariance
    std::vector<double> importance(num_features_);
    
    for (size_t i = 0; i < num_features_; ++i) {
        // Combine parameter magnitude with certainty (inverse variance)
        double param_importance = std::abs(theta_[i]);
        double certainty = 1.0 / (1.0 + std::sqrt(P_(i, i)));
        importance[i] = param_importance * certainty;
    }
    
    // Normalize
    double max_imp = *std::max_element(importance.begin(), importance.end());
    if (max_imp > 0) {
        for (double& imp : importance) {
            imp /= max_imp;
        }
    }
    
    return importance;
}

double OnlinePredictor::estimate_volatility() const {
    if (recent_returns_.size() < 20) return 0.001;  // Default 0.1%
    
    double mean = std::accumulate(recent_returns_.begin(), recent_returns_.end(), 0.0) 
                 / recent_returns_.size();
    
    double sum_sq = 0.0;
    for (double ret : recent_returns_) {
        sum_sq += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sum_sq / recent_returns_.size());
}

void OnlinePredictor::ensure_positive_definite() {
    // Eigenvalue decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);
    Eigen::VectorXd eigenvalues = solver.eigenvalues();
    
    // Ensure all eigenvalues are positive
    bool needs_correction = false;
    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (eigenvalues[i] < EPSILON) {
            eigenvalues[i] = EPSILON;
            needs_correction = true;
        }
    }
    
    if (needs_correction) {
        // Reconstruct with corrected eigenvalues
        P_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        utils::log_debug("Corrected covariance matrix for positive definiteness");
    }
}

// MultiHorizonPredictor Implementation

MultiHorizonPredictor::MultiHorizonPredictor(size_t num_features) 
    : num_features_(num_features) {
}

void MultiHorizonPredictor::add_horizon(int bars, double weight) {
    HorizonConfig config;
    config.horizon_bars = bars;
    config.weight = weight;

    // Adjust learning rate based on horizon
    config.predictor_config.lambda = 0.995 + 0.001 * std::log(bars);
    config.predictor_config.lambda = std::clamp(config.predictor_config.lambda, 0.990, 0.999);

    // Reduce warmup for multi-horizon learning
    // Updates arrive delayed by horizon length, so effective warmup is longer
    config.predictor_config.warmup_samples = 20;

    predictors_.emplace_back(std::make_unique<OnlinePredictor>(num_features_, config.predictor_config));
    configs_.push_back(config);

    utils::log_info("Added predictor for " + std::to_string(bars) + "-bar horizon");
}

OnlinePredictor::PredictionResult MultiHorizonPredictor::predict(const std::vector<double>& features) {
    OnlinePredictor::PredictionResult ensemble_result;
    ensemble_result.predicted_return = 0.0;
    ensemble_result.confidence = 0.0;
    ensemble_result.volatility_estimate = 0.0;
    
    double total_weight = 0.0;
    int ready_count = 0;
    
    for (size_t i = 0; i < predictors_.size(); ++i) {
        auto result = predictors_[i]->predict(features);
        
        if (result.is_ready) {
            double weight = configs_[i].weight * result.confidence;
            ensemble_result.predicted_return += result.predicted_return * weight;
            ensemble_result.confidence += result.confidence * configs_[i].weight;
            ensemble_result.volatility_estimate += result.volatility_estimate * configs_[i].weight;
            total_weight += weight;
            ready_count++;
        }
    }
    
    if (total_weight > 0) {
        ensemble_result.predicted_return /= total_weight;
        ensemble_result.confidence /= configs_.size();
        ensemble_result.volatility_estimate /= configs_.size();
        ensemble_result.is_ready = true;
    }
    
    return ensemble_result;
}

void MultiHorizonPredictor::update(int bars_ago, const std::vector<double>& features, 
                                   double actual_return) {
    // Update the appropriate predictor
    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (configs_[i].horizon_bars == bars_ago) {
            predictors_[i]->update(features, actual_return);
            break;
        }
    }
}

} // namespace learning
} // namespace sentio
