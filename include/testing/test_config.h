// include/testing/test_config.h
#pragma once

#include <string>
#include <vector>

namespace sentio::testing {

/**
 * @brief Configuration structure for testing framework
 */
struct TestConfig {
    // Strategy configuration
    std::string strategy_name;
    bool all_strategies = false;
    std::string strategy_config_path;  // Optional custom config path
    
    // Data configuration
    std::vector<std::string> datasets;
    std::string primary_data_path;
    int blocks = 20;
    
    // Threshold configuration
    double mrb_threshold = 0.01;
    double confidence_threshold = 0.2;
    double min_signal_rate = 0.95;
    double min_non_neutral_ratio = 0.20;
    
    // Backend configuration
    bool use_enhanced_psm = true;           // NEW: default to Enhanced PSM
    bool enable_hysteresis = true;          // NEW: Enable hysteresis
    bool enable_dynamic_allocation = true;  // NEW: Enable dynamic allocation
    
    // Test mode configuration
    bool quick_mode = false;
    bool deployment_check = false;
    bool verbose = false;
    bool quiet = false;  // Suppress progress output (for JSON mode)
    
    // Advanced testing configuration
    bool walk_forward = false;
    int window_size = 252;      // Trading days in window
    int step_size = 21;         // Trading days to step forward
    
    bool stress_test = false;
    std::vector<std::string> stress_scenarios;
    
    bool cross_validate = false;
    int cv_folds = 5;
    
    bool compare_strategies = false;
    
    // Output configuration
    std::string output_path;
    bool json_output = false;
    bool save_signals = false;
    
    // Performance limits
    double max_model_load_time_ms = 10000.0;
    double max_inference_time_ms = 100.0;
    double max_memory_usage_mb = 2048.0;
};

/**
 * @brief Predefined test configurations for common scenarios
 */
class TestConfigPresets {
public:
    static TestConfig quick_sanity_check();
    static TestConfig deployment_validation();
    static TestConfig comprehensive_analysis();
    static TestConfig stress_test_suite();
    static TestConfig walk_forward_validation();
    static TestConfig cross_validation_suite();
};

} // namespace sentio::testing


