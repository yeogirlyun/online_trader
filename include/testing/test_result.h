// include/testing/test_result.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace sentio::testing {

/**
 * @brief Test execution status
 */
enum class TestStatus {
    PASSED,           // All checks passed
    CONDITIONAL,      // Passed with warnings
    FAILED,           // Critical checks failed
    ERROR,            // Execution error
    NOT_RUN           // Test not executed
};

/**
 * @brief Individual test check result
 */
struct CheckResult {
    std::string name;
    bool passed;
    double value;
    double threshold;
    std::string message;
    std::string severity;  // "critical", "warning", "info"
};

/**
 * @brief Comprehensive test result structure
 */
struct TestResult {
    // Basic information
    std::string strategy_name;
    TestStatus status = TestStatus::NOT_RUN;
    std::string status_message;
    double overall_score = 0.0;  // 0.0 to 100.0
    
    // Execution metadata
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    double execution_time_ms = 0.0;
    
    // Check results
    std::vector<CheckResult> checks;
    std::map<std::string, bool> check_status;
    
    // Performance metrics
    std::map<std::string, double> metrics;
    
    // Signal statistics
    int total_signals = 0;
    int non_neutral_signals = 0;
    double signal_generation_rate = 0.0;
    double non_neutral_ratio = 0.0;
    double mean_confidence = 0.0;
    
    // MRB metrics
    double signal_accuracy = 0.0;  // Directional accuracy (0.0-1.0)
    double trading_based_mrb = 0.0;
    
    // Performance metrics
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate = 0.0;
    double volatility = 0.0;
    double total_return = 0.0;
    
    // Resource metrics
    double model_load_time_ms = 0.0;
    double avg_inference_time_ms = 0.0;
    double peak_memory_usage_mb = 0.0;
    
    // Recommendations and issues
    std::vector<std::string> recommendations;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    // Additional analysis data
    std::map<std::string, std::vector<double>> time_series_data;
    std::map<std::string, std::string> metadata;
    
    /**
     * @brief Add a check result
     */
    void add_check(const CheckResult& check);
    
    /**
     * @brief Add a metric
     */
    void add_metric(const std::string& name, double value);
    
    /**
     * @brief Add a recommendation
     */
    void add_recommendation(const std::string& recommendation);
    
    /**
     * @brief Add a warning
     */
    void add_warning(const std::string& warning);
    
    /**
     * @brief Add an error
     */
    void add_error(const std::string& error);
    
    /**
     * @brief Calculate overall score based on checks and metrics
     */
    void calculate_overall_score();
    
    /**
     * @brief Determine final status based on checks
     */
    void determine_status();
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
    
    /**
     * @brief Convert to human-readable report
     */
    std::string to_report() const;
    
    /**
     * @brief Get status string
     */
    std::string get_status_string() const;
    
    /**
     * @brief Check if test passed
     */
    bool passed() const;
};

/**
 * @brief Comparison result for multiple strategies
 */
struct ComparisonResult {
    std::map<std::string, TestResult> strategy_results;
    std::string best_strategy;
    std::string worst_strategy;
    std::vector<std::pair<std::string, double>> rankings;  // strategy name, score
    std::map<std::string, std::string> comparisons;
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
    
    /**
     * @brief Convert to comparison report
     */
    std::string to_report() const;
};

} // namespace sentio::testing


