#pragma once

#include "metrics/signal_accuracy_metrics.h"
#include "common/types.h"
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace training {

/**
 * Enhanced training evaluator for multi-bar regression models
 * Properly handles overlapping predictions and magnitude accuracy
 */
class TrainingEvaluator {
public:
    struct EvaluationConfig {
        int prediction_horizon = 5;
        int signal_interval = 3;
        bool evaluate_overlapping = true;  // Track overlapping prediction accuracy
        bool weight_by_magnitude = true;   // Weight accuracy by return magnitude
        double significance_threshold = 0.001; // Min return to be considered significant
    };
    
    struct DetailedMetrics {
        // Standard metrics
        double rmse = 0.0;
        double mae = 0.0;
        double directional_accuracy = 0.0;
        
        // Enhanced metrics for regression
        double magnitude_weighted_accuracy = 0.0;  // Accuracy weighted by return size
        double correlation = 0.0;                  // Pearson correlation
        double r_squared = 0.0;                   // Coefficient of determination
        
        // Practical trading metrics
        double profitable_direction_rate = 0.0;    // % of correct directions with |return| > threshold
        double large_move_accuracy = 0.0;          // Accuracy on moves > 1%
        double calm_period_accuracy = 0.0;         // Accuracy when |return| < 0.2%
        
        // Consistency metrics
        double prediction_consistency = 0.0;       // How consistent are overlapping predictions
        double temporal_stability = 0.0;           // Stability of predictions over time
        
        // Risk metrics
        double max_error = 0.0;                   // Worst prediction error
        double error_skewness = 0.0;              // Skewness of error distribution
        double downside_accuracy = 0.0;           // Accuracy on negative returns
        double upside_accuracy = 0.0;             // Accuracy on positive returns
        
        // By magnitude buckets
        std::map<std::string, double> accuracy_by_magnitude;
        
        // Statistical tests
        double diebold_mariano_statistic = 0.0;   // DM test for forecast superiority
        double pesaran_timmermann_statistic = 0.0; // PT test for directional accuracy
    };
    
    struct OverlappingAnalysis {
        // For signals every 3 bars predicting 5 bars ahead, we have overlaps
        std::map<int, double> consistency_by_overlap;  // overlap_count -> consistency
        double average_consistency = 0.0;
        double prediction_revision_rate = 0.0;  // How often predictions change significantly
    };

    explicit TrainingEvaluator(const EvaluationConfig& config);
    
    // Evaluate during training
    DetailedMetrics evaluate_predictions(
        const std::vector<float>& predicted_returns,
        const std::vector<float>& actual_returns,
        const std::vector<size_t>& bar_indices
    );
    
    // Evaluate with overlapping predictions
    OverlappingAnalysis analyze_overlapping_predictions(
        const std::vector<float>& all_predictions,
        const std::vector<Bar>& market_data,
        int start_bar,
        int end_bar
    );
    
    // Walk-forward validation
    std::vector<DetailedMetrics> walk_forward_evaluation(
        const std::vector<std::vector<float>>& predictions_by_window,
        const std::vector<std::vector<float>>& actuals_by_window
    );
    
    // Generate comprehensive report
    std::string generate_evaluation_report(const DetailedMetrics& metrics) const;
    
private:
    EvaluationConfig config_;
    
    double calculate_correlation(const std::vector<float>& x,
                                 const std::vector<float>& y) const;
    
    double calculate_r_squared(const std::vector<float>& predicted,
                               const std::vector<float>& actual) const;
    
    double calculate_consistency(const std::vector<float>& predictions_t1,
                                const std::vector<float>& predictions_t2) const;
    
    double diebold_mariano_test(const std::vector<float>& errors1,
                                const std::vector<float>& errors2) const;
    
    double pesaran_timmermann_test(const std::vector<float>& predicted,
                                   const std::vector<float>& actual) const;
};

} // namespace training
} // namespace sentio
