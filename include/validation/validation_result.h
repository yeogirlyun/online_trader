// include/validation/validation_result.h
#pragma once

#include <string>
#include <vector>
#include <map>

namespace sentio::validation {

/**
 * @brief Detailed validation result for strategy assessment
 */
struct ValidationResult {
    // Overall validation status
    bool passed = false;
    bool deployment_ready = false;
    std::string status_message;
    
    // Individual check results
    bool signal_quality_passed = false;
    bool mrb_threshold_passed = false;
    bool model_integrity_passed = false;
    bool performance_benchmark_passed = false;
    bool configuration_valid = false;
    
    // Signal quality metrics
    double signal_generation_rate = 0.0;
    double non_neutral_ratio = 0.0;
    double mean_confidence = 0.0;
    double confidence_std_dev = 0.0;
    int total_signals = 0;
    int non_neutral_signals = 0;
    int long_signals = 0;
    int short_signals = 0;
    int neutral_signals = 0;
    
    // Performance metrics
    double signal_accuracy = 0.0;  // Directional accuracy of signals (0.0-1.0)
    double trading_based_mrb = 0.0;  // Actual trading MRB with Enhanced PSM simulation
    double mrb_consistency = 0.0;  // Variance across blocks
    std::vector<double> block_mrbs;
    
    // Performance metrics
    double model_load_time_ms = 0.0;
    double avg_inference_time_ms = 0.0;
    double max_inference_time_ms = 0.0;
    double min_inference_time_ms = 0.0;
    double memory_usage_mb = 0.0;
    double peak_memory_mb = 0.0;
    
    // Trading performance metrics
    double sharpe_ratio = 0.0;
    double max_drawdown = 0.0;
    double win_rate = 0.0;
    double profit_factor = 0.0;
    double total_return = 0.0;
    double volatility = 0.0;
    
    // Model integrity checks
    bool model_file_exists = false;
    bool model_loads_successfully = false;
    bool model_version_compatible = false;
    std::string model_path;
    std::string model_version;
    
    // Configuration checks
    bool has_required_parameters = false;
    bool parameters_in_valid_range = false;
    std::map<std::string, std::string> config_parameters;
    std::vector<std::string> missing_parameters;
    std::vector<std::string> invalid_parameters;
    
    // Issues and recommendations
    std::vector<std::string> critical_issues;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    std::vector<std::string> info_messages;
    
    // Signal distribution analysis
    std::map<std::string, int> signal_type_distribution;
    std::map<std::string, double> confidence_distribution;
    
    // Additional metadata
    std::string strategy_name;
    std::string data_path;
    int blocks_tested = 0;
    std::string timestamp;
    
    /**
     * @brief Add a critical issue
     */
    void add_critical_issue(const std::string& issue);
    
    /**
     * @brief Add a warning
     */
    void add_warning(const std::string& warning);
    
    /**
     * @brief Add a recommendation
     */
    void add_recommendation(const std::string& recommendation);
    
    /**
     * @brief Add an info message
     */
    void add_info(const std::string& info);
    
    /**
     * @brief Calculate overall validation status
     */
    void calculate_validation_status();
    
    /**
     * @brief Convert to JSON string
     */
    std::string to_json() const;
    
    /**
     * @brief Convert to human-readable report
     */
    std::string to_report() const;
    
    /**
     * @brief Get summary string
     */
    std::string get_summary() const;
    
    /**
     * @brief Get deployment readiness assessment
     */
    std::string get_deployment_assessment() const;
};

} // namespace sentio::validation


