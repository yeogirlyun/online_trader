// src/testing/test_framework.cpp
#include "testing/test_framework.h"
#include "validation/strategy_validator.h"
#include "analysis/performance_analyzer.h"
#include "strategy/istrategy.h"
#include "strategy/config_resolver.h"
#include "common/utils.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace sentio::testing {

TestResult TestFramework::run_sanity_check(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        // Validate configuration
        std::string error_msg;
        if (!validate_config(config, error_msg)) {
            result.status = TestStatus::ERROR;
            result.status_message = "Invalid configuration: " + error_msg;
            result.add_error(error_msg);
            return result;
        }
        
        if (!config.quiet) {
            std::cout << "🔍 Running sanity check for strategy: " << config.strategy_name << std::endl;
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        }
        
        // Run validation
        auto validation_result = validation::StrategyValidator::validate_strategy(
            config.strategy_name,
            config.primary_data_path,
            config
        );
        
        // Convert validation result to test result
        result.total_signals = validation_result.total_signals;
        result.non_neutral_signals = validation_result.non_neutral_signals;
        result.signal_generation_rate = validation_result.signal_generation_rate;
        result.non_neutral_ratio = validation_result.non_neutral_ratio;
        result.mean_confidence = validation_result.mean_confidence;
        result.signal_accuracy = validation_result.signal_accuracy;
        result.trading_based_mrb = validation_result.trading_based_mrb;
        result.model_load_time_ms = validation_result.model_load_time_ms;
        result.avg_inference_time_ms = validation_result.avg_inference_time_ms;
        result.peak_memory_usage_mb = validation_result.memory_usage_mb;
        
        // Add checks
        result.add_check({
            "Signal Quality",
            validation_result.signal_quality_passed,
            validation_result.signal_generation_rate,
            config.min_signal_rate,
            validation_result.signal_quality_passed ? "PASSED" : "FAILED",
            validation_result.signal_quality_passed ? "info" : "critical"
        });
        
        result.add_check({
            "MRB Threshold",
            validation_result.mrb_threshold_passed,
            validation_result.trading_based_mrb,
            config.mrb_threshold,
            validation_result.mrb_threshold_passed ? "PASSED" : "FAILED",
            validation_result.mrb_threshold_passed ? "info" : "critical"
        });
        
        result.add_check({
            "Model Integrity",
            validation_result.model_integrity_passed,
            validation_result.model_loads_successfully ? 1.0 : 0.0,
            1.0,
            validation_result.model_integrity_passed ? "PASSED" : "FAILED",
            validation_result.model_integrity_passed ? "info" : "critical"
        });
        
        result.add_check({
            "Performance Benchmark",
            validation_result.performance_benchmark_passed,
            validation_result.avg_inference_time_ms,
            config.max_inference_time_ms,
            validation_result.performance_benchmark_passed ? "PASSED" : "FAILED",
            validation_result.performance_benchmark_passed ? "info" : "warning"
        });
        
        // Add metrics
        result.add_metric("signal_accuracy", validation_result.signal_accuracy);
        result.add_metric("trading_based_mrb", validation_result.trading_based_mrb);
        result.add_metric("sharpe_ratio", validation_result.sharpe_ratio);
        result.add_metric("max_drawdown", validation_result.max_drawdown);
        result.add_metric("win_rate", validation_result.win_rate);
        
        // Copy recommendations and warnings
        for (const auto& rec : validation_result.recommendations) {
            result.add_recommendation(rec);
        }
        for (const auto& warn : validation_result.warnings) {
            result.add_warning(warn);
        }
        for (const auto& err : validation_result.critical_issues) {
            result.add_error(err);
        }
        
        // Determine final status
        result.determine_status();
        result.calculate_overall_score();
        
        std::cout << "\n✓ Sanity check completed" << std::endl;
        std::cout << "Status: " << result.get_status_string() << std::endl;
        std::cout << "Overall Score: " << result.overall_score << "/100" << std::endl;
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.status_message = std::string("Exception: ") + e.what();
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(
        result.end_time - result.start_time
    ).count();
    
    return result;
}

TestResult TestFramework::run_full_test(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "🧪 Running comprehensive test for strategy: " << config.strategy_name << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        // Load strategy
        auto strategy = load_strategy(config.strategy_name, config.strategy_config_path);
        if (!strategy) {
            result.status = TestStatus::ERROR;
            result.status_message = "Failed to load strategy";
            result.add_error("Strategy could not be loaded");
            return result;
        }
        
        // Test on multiple datasets
        std::vector<analysis::PerformanceMetrics> dataset_metrics;
        
        for (const auto& dataset : config.datasets) {
            std::cout << "\n📊 Testing on dataset: " << dataset << std::endl;
            
            auto market_data = load_market_data(dataset);
            if (market_data.empty()) {
                result.add_warning("Failed to load dataset: " + dataset);
                continue;
            }
            
            auto signals = generate_signals(strategy, market_data);
            auto metrics = analysis::PerformanceAnalyzer::calculate_metrics(
                signals, market_data, config.blocks
            );
            
            metrics.dataset_name = dataset;
            dataset_metrics.push_back(metrics);
            
            std::cout << "  MRB: " << metrics.trading_based_mrb << std::endl;
            std::cout << "  Sharpe: " << metrics.sharpe_ratio << std::endl;
        }
        
        // Aggregate metrics
        if (!dataset_metrics.empty()) {
            double avg_mrb = 0.0;
            double avg_sharpe = 0.0;
            for (const auto& m : dataset_metrics) {
                avg_mrb += m.trading_based_mrb;
                avg_sharpe += m.sharpe_ratio;
            }
            avg_mrb /= dataset_metrics.size();
            avg_sharpe /= dataset_metrics.size();
            
            result.trading_based_mrb = avg_mrb;
            result.sharpe_ratio = avg_sharpe;
            result.add_metric("avg_mrb", avg_mrb);
            result.add_metric("avg_sharpe", avg_sharpe);
        }
        
        // Determine status
        result.determine_status();
        result.calculate_overall_score();
        
        std::cout << "\n✓ Full test completed" << std::endl;
        std::cout << "Status: " << result.get_status_string() << std::endl;
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.status_message = std::string("Exception: ") + e.what();
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(
        result.end_time - result.start_time
    ).count();
    
    return result;
}

std::vector<TestResult> TestFramework::run_all_strategies(const TestConfig& config) {
    std::vector<TestResult> results;
    auto strategies = get_available_strategies();
    
    std::cout << "🎯 Testing all strategies (" << strategies.size() << " total)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    int current = 0;
    for (const auto& strategy_name : strategies) {
        current++;
        std::cout << "\n[" << current << "/" << strategies.size() << "] ";
        
        TestConfig strategy_config = config;
        strategy_config.strategy_name = strategy_name;
        
        auto result = run_sanity_check(strategy_config);
        results.push_back(result);
    }
    
    return results;
}

TestResult TestFramework::run_walk_forward_analysis(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "📈 Running walk-forward analysis" << std::endl;
        
        auto market_data = load_market_data(config.primary_data_path);
        
        analysis::WalkForwardAnalyzer::WalkForwardConfig wf_config;
        wf_config.window_size = config.window_size;
        wf_config.step_size = config.step_size;
        
        auto wf_result = analysis::WalkForwardAnalyzer::analyze(
            config.strategy_name,
            market_data,
            wf_config
        );
        
        result.add_metric("avg_in_sample_mrb", wf_result.avg_in_sample_mrb);
        result.add_metric("avg_out_of_sample_mrb", wf_result.avg_out_of_sample_mrb);
        result.add_metric("stability_ratio", wf_result.stability_ratio);
        result.add_metric("num_windows", wf_result.num_windows);
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

TestResult TestFramework::run_stress_test(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "⚡ Running stress tests" << std::endl;
        
        auto market_data = load_market_data(config.primary_data_path);
        
        std::vector<analysis::StressTestAnalyzer::StressScenario> scenarios;
        for (const auto& scenario_str : config.stress_scenarios) {
            // Parse scenario string to enum
            // Add scenarios
        }
        
        auto stress_results = analysis::StressTestAnalyzer::run_stress_tests(
            config.strategy_name,
            market_data,
            scenarios
        );
        
        int passed = 0;
        for (const auto& sr : stress_results) {
            if (sr.passed) passed++;
            result.add_metric("stress_" + sr.scenario_name, sr.metrics.trading_based_mrb);
        }
        
        result.add_metric("stress_tests_passed", passed);
        result.add_metric("stress_tests_total", stress_results.size());
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

TestResult TestFramework::run_cross_validation(const TestConfig& config) {
    TestResult result;
    result.strategy_name = config.strategy_name;
    result.start_time = std::chrono::system_clock::now();
    
    try {
        std::cout << "🔄 Running cross-validation" << std::endl;
        
        // Implementation of cross-validation
        // Split data into folds
        // Train and test on each fold
        // Aggregate results
        
        result.determine_status();
        result.calculate_overall_score();
        
    } catch (const std::exception& e) {
        result.status = TestStatus::ERROR;
        result.add_error(e.what());
    }
    
    result.end_time = std::chrono::system_clock::now();
    return result;
}

// Private helper methods

std::shared_ptr<IStrategy> TestFramework::load_strategy(const std::string& strategy_name, const std::string& config_path) {
    try {
        // Set custom config path if provided
        if (!config_path.empty()) {
            ConfigResolver::set_config_path(strategy_name, config_path);
        }
        
        auto unique_strategy = create_strategy(strategy_name);
        if (!unique_strategy) {
            return nullptr;
        }
        return std::shared_ptr<IStrategy>(std::move(unique_strategy));
    } catch (const std::exception& e) {
        std::cerr << "Error loading strategy: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<MarketData> TestFramework::load_market_data(const std::string& data_path) {
    // Use existing CSV loading utility from sentio::utils
    return sentio::utils::read_csv_data(data_path);
}

std::vector<SignalOutput> TestFramework::generate_signals(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data
) {
    std::vector<SignalOutput> signals;
    signals.reserve(market_data.size());
    
    // Use the unified process_data API to generate signals in batch
    try {
        signals = strategy->process_data(market_data);
    } catch (...) {
        signals.clear();
    }
    
    return signals;
}

bool TestFramework::validate_config(const TestConfig& config, std::string& error_msg) {
    if (config.strategy_name.empty() && !config.all_strategies) {
        error_msg = "Strategy name is required";
        return false;
    }
    
    if (config.primary_data_path.empty() && config.datasets.empty()) {
        error_msg = "Data path is required";
        return false;
    }
    
    if (config.blocks < 1 || config.blocks > 100) {
        error_msg = "Blocks must be between 1 and 100";
        return false;
    }
    
    if (config.mrb_threshold < 0.0 || config.mrb_threshold > 1.0) {
        error_msg = "MRB threshold must be between 0.0 and 1.0";
        return false;
    }
    
    return true;
}

std::vector<std::string> TestFramework::get_available_strategies() {
    return {
        "sgo",      // Sigor strategy
        "xgb",      // XGBoost strategy
        "ppo",      // PPO strategy
        "ctb",      // CatBoost strategy
        "gbm",      // LightGBM strategy
        "tft"       // TFT strategy
    };
}

} // namespace sentio::testing


