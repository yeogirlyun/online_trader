#pragma once

#include "cpp_xgboost_trainer.h"
#include "walk_forward_validator.h"
#include <string>
#include <utility>
#include <map>
#include <random>
#include <chrono>

namespace sentio {
namespace training {

/**
 * @brief Hyperparameter optimization for XGBoost models
 * 
 * Implements automated hyperparameter search using random search
 * with validation-based objective functions. Can be extended to
 * support more sophisticated optimization algorithms.
 * 
 * Features:
 * - Random search optimization
 * - Cross-validation based objective function
 * - Configurable parameter spaces
 * - Early stopping based on time or trials
 * - Best parameter tracking and export
 */
class HyperparameterOptimizer {
public:
    /**
     * @brief Optimization configuration
     */
    struct OptimizationConfig {
        int n_trials = 100;                     // Maximum number of trials to run
        int cv_folds = 3;                       // Cross-validation folds for evaluation
        double timeout_seconds = 3600.0;        // Maximum optimization time
        std::string optimization_metric = "accuracy";  // "accuracy" or "auc"
        bool enable_early_stopping = true;      // Stop if no improvement
        int early_stopping_rounds = 10;         // Rounds without improvement to stop
        double min_improvement = 0.001;         // Minimum improvement to continue
        bool verbose = true;                     // Enable detailed logging
        int random_seed = 42;                   // Random seed for reproducibility
    };

    /**
     * @brief Parameter space definition with ranges
     */
    struct ParameterSpace {
        // Tree structure parameters
        std::pair<int, int> max_depth = {3, 10};
        std::pair<double, double> learning_rate = {0.01, 0.3};
        std::pair<int, int> n_estimators = {50, 500};
        
        // Sampling parameters
        std::pair<double, double> subsample = {0.5, 1.0};
        std::pair<double, double> colsample_bytree = {0.5, 1.0};
        
        // Regularization parameters
        std::pair<double, double> reg_alpha = {0.0, 10.0};
        std::pair<double, double> reg_lambda = {0.0, 10.0};
        
        // Advanced parameters
        std::pair<int, int> min_child_weight = {1, 10};
        std::pair<double, double> gamma = {0.0, 5.0};
        
        // Validation parameters
        bool validate_ranges() const;
    };

    /**
     * @brief Single trial result
     */
    struct TrialResult {
        CppXGBoostTrainer::TrainingConfig config;
        double score = 0.0;                     // Objective function value
        double accuracy = 0.0;
        double auc = 0.0;
        double training_time = 0.0;
        int trial_number = 0;
        std::string trial_timestamp;
        std::map<std::string, double> parameters;  // For easy access
    };

    /**
     * @brief Complete optimization results
     */
    struct OptimizationResults {
        CppXGBoostTrainer::TrainingConfig best_config;
        std::vector<TrialResult> all_trials;
        double best_score = 0.0;
        double optimization_time = 0.0;
        int total_trials = 0;
        bool converged = false;                 // Whether early stopping occurred
        std::string optimization_timestamp;
        OptimizationConfig config_used;
        
        // Analysis
        std::map<std::string, double> parameter_importance;  // Parameter sensitivity
    };

    HyperparameterOptimizer();
    ~HyperparameterOptimizer() = default;

    /**
     * @brief Execute hyperparameter optimization
     * @param base_config Base configuration to optimize from
     * @param opt_config Optimization settings
     * @param space Parameter space to search
     * @return Optimization results with best configuration
     */
    OptimizationResults optimize(const CppXGBoostTrainer::TrainingConfig& base_config,
                                const OptimizationConfig& opt_config,
                                const ParameterSpace& space);

    /**
     * @brief Set custom objective function (advanced usage)
     * @param objective Custom objective function
     */
    void set_custom_objective(std::function<double(const CppXGBoostTrainer::TrainingConfig&)> objective);

    /**
     * @brief Export optimization results for analysis
     * @param results Optimization results to export
     * @param path Output file path
     * @return True if export successful
     */
    static bool export_results(const OptimizationResults& results, const std::string& path);

    /**
     * @brief Load previous optimization results
     * @param path Input file path
     * @return Loaded optimization results
     */
    static OptimizationResults load_results(const std::string& path);

private:
    // Core optimization methods
    double objective_function(const CppXGBoostTrainer::TrainingConfig& config);
    CppXGBoostTrainer::TrainingConfig sample_parameters(
        const CppXGBoostTrainer::TrainingConfig& base_config,
        const ParameterSpace& space);
    
    // Sampling methods for different parameter types
    int sample_int_uniform(int min_val, int max_val);
    double sample_double_uniform(double min_val, double max_val);
    double sample_double_log_uniform(double min_val, double max_val);  // For learning rate, etc.
    
    // Analysis methods
    bool check_early_stopping(const std::vector<TrialResult>& trials,
                             const OptimizationConfig& config);
    std::map<std::string, double> analyze_parameter_importance(
        const std::vector<TrialResult>& trials);
    double calculate_correlation(const std::vector<double>& x, const std::vector<double>& y);
    
    // Utility methods
    void log_trial_progress(const TrialResult& trial, const OptimizationConfig& config);
    std::string current_timestamp();
    void validate_optimization_config(const OptimizationConfig& config);

    // Member variables
    std::mt19937 rng_;  // Random number generator
    std::function<double(const CppXGBoostTrainer::TrainingConfig&)> custom_objective_;
    OptimizationConfig current_opt_config_;
    
    // Statistics
    size_t total_evaluations_ = 0;
    double total_optimization_time_ = 0.0;
};

} // namespace training
} // namespace sentio
