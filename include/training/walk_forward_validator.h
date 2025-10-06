#pragma once

#include "cpp_xgboost_trainer.h"
#include <vector>
#include <utility>

namespace sentio {
namespace training {

/**
 * @brief Time-series aware walk-forward validation for trading strategies
 * 
 * Implements walk-forward validation specifically designed for financial
 * time series data with proper temporal splitting and gap handling to
 * avoid lookahead bias.
 * 
 * Features:
 * - Expanding or sliding window validation
 * - Configurable gap between train and test sets
 * - Purging of overlapping samples
 * - Statistical analysis of fold performance
 */
class WalkForwardValidator {
public:
    /**
     * @brief Validation configuration parameters
     */
    struct ValidationConfig {
        int n_folds = 5;                    // Number of validation folds
        int gap_bars = 20;                  // Gap between train and test to avoid lookahead
        bool expanding_window = true;       // If true, use expanding window; else sliding
        double test_ratio = 0.2;            // Proportion of data for testing in each fold
        bool enable_purging = true;         // Remove overlapping samples
        double min_test_samples = 100;      // Minimum samples required for test set
        bool verbose = true;                // Enable detailed logging
    };

    /**
     * @brief Comprehensive validation results
     */
    struct ValidationResults {
        // Per-fold metrics
        std::vector<double> fold_accuracies;
        std::vector<double> fold_aucs;
        std::vector<size_t> fold_train_sizes;
        std::vector<size_t> fold_test_sizes;
        
        // Aggregate statistics
        double mean_accuracy = 0.0;
        double std_accuracy = 0.0;
        double mean_auc = 0.0;
        double std_auc = 0.0;
        
        // Model consistency metrics
        double accuracy_stability = 0.0;    // CV accuracy standard deviation
        double best_fold_accuracy = 0.0;
        double worst_fold_accuracy = 0.0;
        
        // Feature importance aggregation
        std::map<std::string, double> avg_feature_importance;
        std::map<std::string, double> std_feature_importance;
        
        // Validation metadata
        double total_validation_time = 0.0;
        std::string validation_timestamp;
        ValidationConfig config_used;
    };

    WalkForwardValidator();
    ~WalkForwardValidator() = default;

    /**
     * @brief Execute walk-forward validation
     * @param train_config Base training configuration
     * @param val_config Validation-specific configuration
     * @return Comprehensive validation results
     */
    ValidationResults validate(const CppXGBoostTrainer::TrainingConfig& train_config, 
                              const ValidationConfig& val_config);

    /**
     * @brief Generate fold indices for given dataset size
     * @param total_samples Total number of samples in dataset
     * @param config Validation configuration
     * @return Vector of (train_start, train_end, test_start, test_end) tuples
     */
    std::vector<std::tuple<size_t, size_t, size_t, size_t>> 
    generate_fold_indices(size_t total_samples, const ValidationConfig& config);

    /**
     * @brief Validate configuration parameters
     * @param config Configuration to validate
     * @return True if configuration is valid
     */
    static bool validate_config(const ValidationConfig& config);

private:
    /**
     * @brief Individual fold definition
     */
    struct Fold {
        size_t train_start;
        size_t train_end;
        size_t test_start;
        size_t test_end;
        size_t fold_index;
    };

    /**
     * @brief Per-fold evaluation metrics
     */
    struct FoldMetrics {
        double accuracy = 0.0;
        double auc = 0.0;
        double precision = 0.0;
        double recall = 0.0;
        double f1_score = 0.0;
        size_t train_samples = 0;
        size_t test_samples = 0;
        double positive_ratio = 0.0;
        std::map<std::string, double> feature_importance;
    };

    // Core validation methods
    std::vector<Fold> generate_folds(size_t total_samples, const ValidationConfig& config);
    FoldMetrics evaluate_fold(const CppXGBoostTrainer::TrainingConfig& train_config,
                             const Fold& fold,
                             const CppXGBoostTrainer::TrainingDataset& full_dataset);
    
    // Statistical analysis methods
    ValidationResults aggregate_fold_results(const std::vector<FoldMetrics>& fold_metrics,
                                            const ValidationConfig& config);
    void calculate_statistics(const std::vector<double>& values, double& mean, double& std);
    std::map<std::string, double> aggregate_feature_importance(
        const std::vector<std::map<std::string, double>>& fold_importances);

    // Utility methods
    void log_fold_progress(int current_fold, int total_folds, const FoldMetrics& metrics);
    bool validate_fold(const Fold& fold, size_t total_samples);
    void purge_overlapping_samples(Fold& fold, const ValidationConfig& config);
};

} // namespace training
} // namespace sentio
