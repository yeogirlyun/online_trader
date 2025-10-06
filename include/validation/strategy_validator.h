// include/validation/strategy_validator.h
#pragma once

#include "validation_result.h"
#include "testing/test_config.h"
#include "strategy/istrategy.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <memory>
#include <string>
#include <vector>

namespace sentio::validation {
using MarketData = sentio::Bar;

/**
 * @brief Core strategy validation engine
 * 
 * Validates strategy implementations against deployment criteria including:
 * - Signal quality and generation rate
 * - MRB threshold compliance
 * - Model integrity and performance
 * - Configuration correctness
 */
class StrategyValidator {
public:
    /**
     * @brief Validate strategy for deployment readiness
     * @param strategy_name Name of strategy to validate
     * @param data_path Path to market data file
     * @param config Test configuration parameters
     * @return ValidationResult with detailed validation metrics
     */
    static ValidationResult validate_strategy(
        const std::string& strategy_name,
        const std::string& data_path,
        const testing::TestConfig& config
    );
    
    /**
     * @brief Validate strategy instance with pre-loaded data
     * @param strategy Strategy instance to validate
     * @param market_data Pre-loaded market data
     * @param config Test configuration parameters
     * @return ValidationResult with detailed validation metrics
     */
    static ValidationResult validate_strategy_instance(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data,
        const testing::TestConfig& config,
        ValidationResult result = ValidationResult()  // Optional pre-initialized result
    );
    
    /**
     * @brief Quick validation for CI/CD pipelines
     * @param strategy_name Name of strategy to validate
     * @param data_path Path to market data file
     * @param blocks Number of blocks for quick test
     * @return ValidationResult with basic validation metrics
     */
    static ValidationResult quick_validate(
        const std::string& strategy_name,
        const std::string& data_path,
        int blocks = 5
    );

private:
    /**
     * @brief Validate signal quality metrics
     * - Signal generation rate
     * - Non-neutral ratio
     * - Confidence distribution
     * - Signal consistency
     */
    static bool validate_signal_quality(
        const std::vector<SignalOutput>& signals,
        const testing::TestConfig& config,
        ValidationResult& result
    );
    
    /**
     * @brief Validate MRB performance against thresholds
     * - Signal-based MRB
     * - Trading-based MRB
     * - MRB consistency across blocks
     */
    static bool validate_mrb_performance(
        const std::vector<SignalOutput>& signals,
        const std::vector<MarketData>& market_data,
        const testing::TestConfig& config,
        ValidationResult& result
    );
    
    /**
     * @brief Validate model integrity
     * - Model loading success
     * - Model inference functionality
     * - Model file integrity
     * - Version compatibility
     */
    static bool validate_model_integrity(
        const std::string& strategy_name,
        ValidationResult& result
    );
    
    /**
     * @brief Validate performance benchmarks
     * - Execution time
     * - Memory usage
     * - Inference speed
     * - Resource efficiency
     */
    static bool validate_performance_benchmarks(
        std::shared_ptr<IStrategy> strategy,
        const std::vector<MarketData>& market_data,
        const testing::TestConfig& config,
        ValidationResult& result
    );
    
    /**
     * @brief Validate configuration correctness
     * - Required parameters present
     * - Parameter value ranges
     * - Configuration consistency
     */
    static bool validate_configuration(
        std::shared_ptr<IStrategy> strategy,
        ValidationResult& result
    );
    
    /**
     * @brief Calculate signal statistics
     */
    static void calculate_signal_statistics(
        const std::vector<SignalOutput>& signals,
        ValidationResult& result
    );
    
    /**
     * @brief Analyze signal distribution
     */
    static void analyze_signal_distribution(
        const std::vector<SignalOutput>& signals,
        ValidationResult& result
    );
    
    /**
     * @brief Check for common issues
     */
    static void check_common_issues(
        const std::vector<SignalOutput>& signals,
        ValidationResult& result
    );
    
    /**
     * @brief Generate recommendations based on validation
     */
    static void generate_recommendations(
        ValidationResult& result
    );
};

/**
 * @brief Batch validator for multiple strategies
 */
class BatchValidator {
public:
    /**
     * @brief Validate multiple strategies
     */
    static std::map<std::string, ValidationResult> validate_multiple_strategies(
        const std::vector<std::string>& strategy_names,
        const std::string& data_path,
        const testing::TestConfig& config
    );
    
    /**
     * @brief Compare validation results across strategies
     */
    static std::string compare_validation_results(
        const std::map<std::string, ValidationResult>& results
    );
};

} // namespace sentio::validation


