#pragma once

#include "validation_result.h"
#include "strategy/istrategy.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <memory>

namespace sentio::validation {
using MarketData = sentio::Bar;

/**
 * @brief Window mode for walk-forward testing
 */
enum class WindowMode {
    ROLLING,    // Fixed-size sliding window
    ANCHORED,   // Growing window from fixed start
    EXPANDING   // Growing window from beginning
};

/**
 * @brief Configuration for walk-forward validation
 */
struct WalkForwardConfig {
    WindowMode mode = WindowMode::ROLLING;
    size_t train_window_blocks = 40;      // Training window size in blocks
    size_t test_window_blocks = 10;       // Test window size in blocks
    size_t step_size_blocks = 10;         // Step size between windows
    double min_mrb_threshold = 0.0035;    // 0.35% minimum MRB
    double max_degradation_ratio = 0.5;   // Max 50% train->test degradation
    bool enable_optimization = false;     // Run Optuna in train windows
    int optuna_trials = 30;               // Trials per train window
};

/**
 * @brief Result for a single walk-forward window
 */
struct WindowResult {
    int window_index = 0;
    
    // Data ranges
    size_t train_start_bar = 0;
    size_t train_end_bar = 0;
    size_t test_start_bar = 0;
    size_t test_end_bar = 0;
    
    // Training metrics
    double train_mrb = 0.0;
    double train_accuracy = 0.0;
    int train_signals = 0;
    int train_non_neutral = 0;
    
    // Testing metrics (out-of-sample)
    double test_mrb = 0.0;
    double test_accuracy = 0.0;
    int test_signals = 0;
    int test_non_neutral = 0;
    
    // Overfitting detection
    double degradation_ratio = 0.0;  // (train_mrb - test_mrb) / train_mrb
    bool is_overfit = false;
    
    // Pass/fail
    bool passed = false;
    std::string failure_reason;
};

/**
 * @brief Aggregate result for walk-forward validation
 */
struct WalkForwardResult {
    std::string strategy_name;
    WalkForwardConfig config;
    
    // Per-window results
    std::vector<WindowResult> windows;
    
    // Aggregate statistics
    double mean_test_mrb = 0.0;
    double std_test_mrb = 0.0;
    double ci_lower_95 = 0.0;           // 95% confidence interval
    double ci_upper_95 = 0.0;
    
    double mean_train_mrb = 0.0;
    double mean_degradation = 0.0;
    
    // Win rate
    int passing_windows = 0;
    int total_windows = 0;
    double win_rate = 0.0;
    
    // Consistency
    double consistency_score = 0.0;     // 1.0 - (std / |mean|)
    
    // Statistical significance
    double t_statistic = 0.0;
    double p_value = 0.0;
    bool statistically_significant = false;
    
    // Overfitting
    int overfit_windows = 0;
    double overfit_percentage = 0.0;
    
    // Overall assessment
    bool passed = false;
    std::string assessment;  // "EXCELLENT", "GOOD", "FAIR", "POOR", "FAILED"
    std::vector<std::string> issues;
    std::vector<std::string> recommendations;
};

/**
 * @brief Walk-forward validator for robust strategy testing
 */
class WalkForwardValidator {
public:
    /**
     * @brief Run walk-forward validation
     */
    static WalkForwardResult validate(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data,
        const WalkForwardConfig& config
    );
    
    /**
     * @brief Quick walk-forward validation (3 windows, rolling)
     */
    static WalkForwardResult quick_validate(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data,
        double min_mrb = 0.0035
    );
    
private:
    /**
     * @brief Process a single walk-forward window
     */
    static WindowResult process_window(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data,
        const WalkForwardConfig& config,
        size_t train_start,
        size_t train_end,
        size_t test_start,
        size_t test_end,
        int window_index
    );
    
    /**
     * @brief Calculate aggregate statistics
     */
    static void calculate_aggregate_statistics(WalkForwardResult& result);
    
    /**
     * @brief Calculate statistical significance (t-test)
     */
    static void calculate_statistical_significance(WalkForwardResult& result);
    
    /**
     * @brief Calculate bootstrap confidence intervals
     */
    static void calculate_confidence_intervals(WalkForwardResult& result);
    
    /**
     * @brief Detect overfitting
     */
    static void detect_overfitting(WalkForwardResult& result);
    
    /**
     * @brief Generate overall assessment
     */
    static void generate_assessment(WalkForwardResult& result);
};

} // namespace sentio::validation

