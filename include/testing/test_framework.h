// include/testing/test_framework.h
#pragma once

#include "test_config.h"
#include "test_result.h"
#include "strategy/istrategy.h"
#include "common/types.h"
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace sentio::testing {
using MarketData = sentio::Bar;

/**
 * @brief Core testing framework for strategy validation and analysis
 * 
 * This framework provides comprehensive testing capabilities including:
 * - Sanity checks for deployment readiness
 * - Full behavioral analysis across multiple datasets
 * - Multi-strategy comparison and benchmarking
 */
class TestFramework {
public:
    /**
     * @brief Execute sanity check for deployment readiness
     * @param config Test configuration parameters
     * @return TestResult containing validation metrics and status
     */
    static TestResult run_sanity_check(const TestConfig& config);
    
    /**
     * @brief Execute comprehensive full test suite
     * @param config Test configuration parameters
     * @return TestResult containing detailed analysis metrics
     */
    static TestResult run_full_test(const TestConfig& config);
    
    /**
     * @brief Run tests across all available strategies
     * @param config Base test configuration
     * @return Vector of test results, one per strategy
     */
    static std::vector<TestResult> run_all_strategies(const TestConfig& config);
    
    /**
     * @brief Execute walk-forward analysis
     * @param config Test configuration with walk-forward parameters
     * @return TestResult containing walk-forward analysis metrics
     */
    static TestResult run_walk_forward_analysis(const TestConfig& config);
    
    /**
     * @brief Execute stress testing scenarios
     * @param config Test configuration with stress test scenarios
     * @return TestResult containing stress test results
     */
    static TestResult run_stress_test(const TestConfig& config);
    
    /**
     * @brief Execute cross-validation analysis
     * @param config Test configuration with cross-validation parameters
     * @return TestResult containing cross-validation metrics
     */
    static TestResult run_cross_validation(const TestConfig& config);

private:
    /**
     * @brief Load strategy instance from factory
     */
    static std::shared_ptr<IStrategy> load_strategy(const std::string& strategy_name, const std::string& config_path = "");
    
    /**
     * @brief Load market data from file
     */
    static std::vector<MarketData> load_market_data(const std::string& data_path);
    
    /**
     * @brief Generate signals for given market data
     */
    static std::vector<SignalOutput> generate_signals(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data
    );
    
    /**
     * @brief Validate test configuration
     */
    static bool validate_config(const TestConfig& config, std::string& error_msg);
    
    /**
     * @brief Get list of all available strategies
     */
    static std::vector<std::string> get_available_strategies();
};

} // namespace sentio::testing


