// File: include/testing/enhanced_test_framework.h
#ifndef ENHANCED_TEST_FRAMEWORK_H
#define ENHANCED_TEST_FRAMEWORK_H

#include "testing/test_framework.h"
#include "testing/test_config.h"
#include "testing/test_result.h"
#include "analysis/enhanced_performance_analyzer.h"
#include "backend/enhanced_backend_component.h"
#include <memory>
#include <string>
#include <vector>

namespace sentio {
namespace testing {

// Enhanced test configuration with PSM integration
struct EnhancedTestConfig : public sentio::testing::TestConfig {
    // Enhanced PSM Configuration (DEFAULT: all enabled)
    bool use_enhanced_psm = true;           // Use Enhanced PSM by default
    bool enable_hysteresis = true;          // Enable hysteresis by default
    bool enable_dynamic_allocation = true;  // Enable allocation by default
    
    // Performance calculation method
    enum class PerformanceMethod {
        ENHANCED_PSM,       // Use Enhanced PSM (DEFAULT)
        LEGACY_SIMULATION,  // Use simple simulation
        HYBRID             // Use both and compare
    };
    PerformanceMethod performance_method = PerformanceMethod::ENHANCED_PSM;
    
    // Enhanced PSM specific settings
    sentio::analysis::EnhancedPerformanceConfig performance_config;
    
    // Comparison settings for HYBRID mode
    bool compare_with_legacy = false;
    double acceptable_difference = 0.10;  // 10% difference acceptable
    
    // Additional validation
    bool validate_state_transitions = true;
    bool track_hysteresis_effectiveness = true;
    bool measure_whipsaw_reduction = true;
    
    // Reporting options
    bool generate_detailed_report = true;
    bool save_trade_history = true;
    bool plot_equity_curve = false;
};

// Enhanced test result with detailed metrics
struct EnhancedTestResult : public sentio::testing::TestResult {
    // Enhanced PSM metrics
    sentio::analysis::EnhancedPerformanceAnalyzer::EnhancedPerformanceMetrics enhanced_metrics;
    
    // Comparison with legacy
    double legacy_mrb = 0.0;
    double enhanced_mrb = 0.0;
    double improvement_percentage = 0.0;
    
    // State transition analysis
    std::map<std::string, int> state_transitions;
    int total_transitions = 0;
    int whipsaw_prevented = 0;
    
    // Trading efficiency
    double trade_reduction_percentage = 0.0;
    double win_rate_improvement = 0.0;
    double drawdown_reduction = 0.0;
    
    // Hysteresis effectiveness
    double hysteresis_benefit = 0.0;
    int signals_filtered = 0;
    
    // Validation results
    bool enhanced_psm_valid = true;
    std::vector<std::string> validation_warnings;
    
    // Performance comparison
    struct PerformanceComparison {
        std::string metric_name;
        double legacy_value;
        double enhanced_value;
        double improvement;
        std::string status;  // "IMPROVED", "DEGRADED", "NEUTRAL"
    };
    std::vector<PerformanceComparison> comparisons;
};

// Enhanced test framework with PSM integration
class EnhancedTestFramework : public sentio::testing::TestFramework {
public:
    explicit EnhancedTestFramework(const std::string& base_path = ".");
    
    // Override base methods to use Enhanced PSM (non-virtual in base, so no override)
    sentio::testing::TestResult run_sanity_check(const sentio::testing::TestConfig& config);
    sentio::testing::TestResult run_full_test(const sentio::testing::TestConfig& config);
    
    // Enhanced test methods
    EnhancedTestResult run_enhanced_sanity_check(const EnhancedTestConfig& config);
    EnhancedTestResult run_enhanced_full_test(const EnhancedTestConfig& config);
    
    // Comparison and validation methods
    EnhancedTestResult run_comparison_test(
        const EnhancedTestConfig& config,
        const std::string& strategy_type = "all"
    );
    
    bool validate_enhanced_results(
        const EnhancedTestResult& result,
        const EnhancedTestConfig& config
    );
    
    // Analysis methods
    void analyze_hysteresis_effectiveness(
        const std::vector<sentio::SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        EnhancedTestResult& result
    );
    
    void analyze_state_transitions(
        const sentio::analysis::EnhancedPerformanceAnalyzer::EnhancedPerformanceMetrics& metrics,
        EnhancedTestResult& result
    );
    
    void compare_trading_efficiency(
        const EnhancedTestResult& enhanced,
        const sentio::testing::TestResult& legacy,
        EnhancedTestResult& result
    );
    
    // Reporting methods
    void generate_enhanced_report(
        const EnhancedTestResult& result,
        const std::string& output_file = ""
    );
    
    void print_comparison_summary(const EnhancedTestResult& result);
    
    // Configuration helpers
    static EnhancedTestConfig create_default_config();
    static EnhancedTestConfig create_aggressive_config();
    static EnhancedTestConfig create_conservative_config();
    
protected:
    // Enhanced calculation methods
    EnhancedTestResult calculate_enhanced_metrics(
        const std::string& strategy_type,
        const std::vector<sentio::SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        const EnhancedTestConfig& config
    );
    
    sentio::testing::TestResult convert_to_base_result(const EnhancedTestResult& enhanced);
    
    // Helper methods
    void run_with_enhanced_psm(
        const std::vector<sentio::SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        const EnhancedTestConfig& config,
        EnhancedTestResult& result
    );
    
    void run_with_legacy_simulation(
        const std::vector<sentio::SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        const EnhancedTestConfig& config,
        EnhancedTestResult& result
    );
    
    void run_hybrid_comparison(
        const std::vector<sentio::SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        const EnhancedTestConfig& config,
        EnhancedTestResult& result
    );
    
private:
    std::unique_ptr<sentio::analysis::EnhancedPerformanceAnalyzer> enhanced_analyzer_;
    std::unique_ptr<sentio::EnhancedBackendComponent> enhanced_backend_;
    
    // Configuration
    EnhancedTestConfig default_config_;
    
    // State tracking
    std::vector<sentio::BackendComponent::TradeOrder> trade_history_;
    std::map<std::string, int> state_transition_counts_;
    
    // Initialize components
    void initialize_enhanced_components(const EnhancedTestConfig& config);
    
    // Validation helpers
    bool validate_mrb_improvement(double enhanced_mrb, double legacy_mrb, double threshold);
    bool validate_trade_reduction(int enhanced_trades, int legacy_trades, double threshold);
    bool validate_risk_metrics(const EnhancedTestResult& result);
};

} // namespace testing
} // namespace sentio

#endif // ENHANCED_TEST_FRAMEWORK_H
