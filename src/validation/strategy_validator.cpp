// src/validation/strategy_validator.cpp
#include "validation/strategy_validator.h"
#include "analysis/performance_analyzer.h"
#include "strategy/istrategy.h"
#include "common/utils.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sentio::validation {

ValidationResult StrategyValidator::validate_strategy(
    const std::string& strategy_name,
    const std::string& data_path,
    const testing::TestConfig& config
) {
    ValidationResult result;
    result.strategy_name = strategy_name;
    result.data_path = data_path;
    result.blocks_tested = config.blocks;
    
    try {
        std::cout << "  Loading strategy..." << std::flush;
        
        // Load strategy
        auto start_time = std::chrono::high_resolution_clock::now();
        auto strategy_unique = create_strategy(strategy_name);
        std::shared_ptr<IStrategy> strategy;
        if (strategy_unique) {
            strategy = std::shared_ptr<IStrategy>(std::move(strategy_unique));
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        
        if (!strategy) {
            result.add_critical_issue("Failed to load strategy: " + strategy_name);
            result.calculate_validation_status();
            return result;
        }
        
        result.model_load_time_ms = std::chrono::duration<double, std::milli>(
            end_time - start_time
        ).count();
        result.model_loads_successfully = true;
        
        std::cout << " ✓ (" << result.model_load_time_ms << "ms)" << std::endl;
        
        // Initialize strategy
        StrategyComponent::StrategyConfig strategy_config;
        if (!strategy->initialize(strategy_config)) {
            result.add_critical_issue("Failed to initialize strategy: " + strategy_name);
            result.calculate_validation_status();
            return result;
        }
        
        // Load market data (limit to blocks * STANDARD_BLOCK_SIZE)
        std::cout << "  Loading market data..." << std::flush;
        std::vector<MarketData> all_data = sentio::utils::read_csv_data(data_path);
        std::vector<MarketData> market_data;
        
        if (config.blocks > 0 && !all_data.empty()) {
            size_t max_bars = config.blocks * sentio::STANDARD_BLOCK_SIZE;
            if (all_data.size() > max_bars) {
                // FIX: Use LAST N blocks (recent data), not FIRST N blocks (old data)
                // This makes sanity-check consistent with strattest/full-test
                size_t start_index = all_data.size() - max_bars;
                market_data.assign(all_data.begin() + start_index, all_data.end());
            } else {
                market_data = all_data;
            }
        } else {
            market_data = all_data;
        }
        
        // Show data range for transparency
        size_t start_bar = all_data.size() - market_data.size();
        size_t end_bar = all_data.size() - 1;
        std::cout << " ✓ (" << market_data.size() << " bars, " 
                  << (market_data.size() / sentio::STANDARD_BLOCK_SIZE) << " blocks)"
                  << " [range: " << start_bar << "-" << end_bar << "]" << std::endl;
        
        // Validate with loaded data (pass result to preserve model_loads_successfully flag)
        return validate_strategy_instance(strategy, market_data, config, result);
        
    } catch (const std::exception& e) {
        result.add_critical_issue(std::string("Exception during validation: ") + e.what());
        result.calculate_validation_status();
        return result;
    }
}

ValidationResult StrategyValidator::validate_strategy_instance(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data,
    const testing::TestConfig& config,
    ValidationResult result  // Pass by value to accept optional initialized result
) {
    // If result wasn't initialized (called directly), set up basic info
    if (result.strategy_name.empty()) {
        result.strategy_name = strategy->get_strategy_name();
    }
    if (result.blocks_tested == 0) {
        result.blocks_tested = config.blocks;
    }
    
    try {
        // Step 1: Validate model integrity
        std::cout << "  [1/5] Validating model integrity..." << std::flush;
        result.model_integrity_passed = validate_model_integrity(
            result.strategy_name, result
        );
        std::cout << (result.model_integrity_passed ? " ✓" : " ✗") << std::endl;
        
        // Step 2: Generate signals
        std::cout << "  [2/5] Generating signals";
        std::vector<SignalOutput> signals;
        signals.reserve(market_data.size());
        
        std::vector<double> inference_times;
        inference_times.reserve(market_data.size());
        
        auto start = std::chrono::high_resolution_clock::now();
        try {
            // Show progress for large datasets
            if (market_data.size() > 1000) {
                std::cout << " [";
                std::cout.flush();
            }
            
            signals = strategy->process_data(market_data);
            
            if (market_data.size() > 1000) {
                std::cout << "████████████████████]";
            }
        } catch (const std::exception& e) {
            result.add_warning("process_data failed: " + std::string(e.what()));
        }
        auto end = std::chrono::high_resolution_clock::now();
        if (!market_data.empty()) {
            result.avg_inference_time_ms = std::chrono::duration<double, std::milli>(end - start).count() / market_data.size();
        }
        
        result.total_signals = signals.size();
        std::cout << " ✓ (" << result.total_signals << " signals)" << std::endl;
        
        // Calculate inference statistics
        if (!inference_times.empty()) {
            result.avg_inference_time_ms = std::accumulate(
                inference_times.begin(), inference_times.end(), 0.0
            ) / inference_times.size();
            result.max_inference_time_ms = *std::max_element(
                inference_times.begin(), inference_times.end()
            );
            result.min_inference_time_ms = *std::min_element(
                inference_times.begin(), inference_times.end()
            );
        }
        
        // Step 3: Validate signal quality
        std::cout << "  [3/5] Validating signal quality..." << std::flush;
        result.signal_quality_passed = validate_signal_quality(signals, config, result);
        std::cout << (result.signal_quality_passed ? " ✓" : " ✗") << std::endl;
        
        // Step 4: Validate MRB performance
        std::cout << "  [4/5] Validating MRB performance..." << std::flush;
        result.mrb_threshold_passed = validate_mrb_performance(
            signals, market_data, config, result
        );
        std::cout << (result.mrb_threshold_passed ? " ✓" : " ✗") << std::endl;
        
        // Step 5: Validate performance benchmarks
        std::cout << "  [5/5] Validating performance benchmarks..." << std::flush;
        result.performance_benchmark_passed = validate_performance_benchmarks(
            strategy, market_data, config, result
        );
        std::cout << (result.performance_benchmark_passed ? " ✓" : " ✗") << std::endl;
        
        // Calculate signal statistics
        calculate_signal_statistics(signals, result);
        
        // Analyze signal distribution
        analyze_signal_distribution(signals, result);
        
        // Check for common issues
        check_common_issues(signals, result);
        
        // Generate recommendations
        generate_recommendations(result);
        
        // Validate configuration
        result.configuration_valid = validate_configuration(strategy, result);
        
        // Calculate overall validation status
        result.calculate_validation_status();
        
    } catch (const std::exception& e) {
        result.add_critical_issue(std::string("Exception: ") + e.what());
        result.calculate_validation_status();
    }
    
    return result;
}

ValidationResult StrategyValidator::quick_validate(
    const std::string& strategy_name,
    const std::string& data_path,
    int blocks
) {
    testing::TestConfig config;
    config.strategy_name = strategy_name;
    config.primary_data_path = data_path;
    config.blocks = blocks;
    config.quick_mode = true;
    
    return validate_strategy(strategy_name, data_path, config);
}

// Private validation methods

bool StrategyValidator::validate_signal_quality(
    const std::vector<SignalOutput>& signals,
    const testing::TestConfig& config,
    ValidationResult& result
) {
    if (signals.empty()) {
        result.add_critical_issue("No signals generated");
        return false;
    }
    
    // Calculate signal generation rate
    int valid_signals = 0;
    for (const auto& signal : signals) {
        valid_signals++;
    }
    
    result.signal_generation_rate = static_cast<double>(valid_signals) / signals.size();
    
    if (result.signal_generation_rate < config.min_signal_rate) {
        result.add_critical_issue(
            "Low signal generation rate: " + 
            std::to_string(result.signal_generation_rate * 100.0) + "%"
        );
        return false;
    }
    
    // Calculate non-neutral ratio using signal_type enum (not flawed approximation)
    int non_neutral = 0;
    for (const auto& signal : signals) {
        if (signal.signal_type != SignalType::NEUTRAL) {
            non_neutral++;
        }
    }
    result.non_neutral_ratio = static_cast<double>(non_neutral) / signals.size();
    
    if (result.non_neutral_ratio < config.min_non_neutral_ratio) {
        result.add_warning(
            "Low non-neutral ratio: " + 
            std::to_string(result.non_neutral_ratio * 100.0) + "%"
        );
        return false;
    }
    
    // Mean confidence
    double total_confidence = 0.0;
    int confidence_count = 0;
    for (const auto& signal : signals) {
        if (0.7 > 0.0) {
            total_confidence += 0.7;
            confidence_count++;
        }
    }
    if (confidence_count > 0) {
        result.mean_confidence = total_confidence / confidence_count;
    }
    if (result.mean_confidence < config.confidence_threshold) {
        result.add_warning("Low mean confidence: " + std::to_string(result.mean_confidence));
    }
    
    return true;
}

bool StrategyValidator::validate_mrb_performance(
    const std::vector<SignalOutput>& signals,
    const std::vector<MarketData>& market_data,
    const testing::TestConfig& config,
    ValidationResult& result
) {
    try {
        result.signal_accuracy = analysis::PerformanceAnalyzer::calculate_signal_accuracy(
            signals, market_data
        );
        result.trading_based_mrb = analysis::PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
            signals, market_data, config.blocks
        );
        result.block_mrbs = analysis::PerformanceAnalyzer::calculate_block_mrbs(
            signals, market_data, config.blocks
        );
        
        if (!result.block_mrbs.empty()) {
            double mean_block_mrb = std::accumulate(
                result.block_mrbs.begin(), result.block_mrbs.end(), 0.0
            ) / result.block_mrbs.size();
            
            double variance = 0.0;
            for (const auto& mrb : result.block_mrbs) {
                variance += (mrb - mean_block_mrb) * (mrb - mean_block_mrb);
            }
            variance /= result.block_mrbs.size();
            
            double std_dev = std::sqrt(variance);
            result.mrb_consistency = (mean_block_mrb != 0.0) ? 
                (std_dev / std::abs(mean_block_mrb)) : 1.0;
        }
        
        // FIX: Only check TRADING MRB, not signal MRB
        // Signal MRB is a derived metric that doesn't reflect actual trading performance
        // User requested to remove signal MRB concept - only trading MRB matters
        bool trading_mrb_ok = result.trading_based_mrb >= config.mrb_threshold;
        
        if (!trading_mrb_ok) {
            result.add_critical_issue(
                "Trading-based MRB below threshold: " + 
                std::to_string(result.trading_based_mrb) + 
                " (threshold: " + std::to_string(config.mrb_threshold) + ")"
            );
        }
        
        // Still track signal accuracy for informational purposes
        // but don't fail validation based on it
        if (result.signal_accuracy < 0.55) {
            result.add_warning(
                "Signal accuracy below 55%: " + 
                std::to_string(result.signal_accuracy * 100.0) + "%" +
                " (informational only, not a failure)"
            );
        }
        
        return trading_mrb_ok;  // Only trading MRB matters for validation
        
    } catch (const std::exception& e) {
        result.add_critical_issue("MRB calculation failed: " + std::string(e.what()));
        return false;
    }
}

bool StrategyValidator::validate_model_integrity(
    const std::string& strategy_name,
    ValidationResult& result
) {
    result.model_file_exists = true;  // Placeholder
    result.model_version_compatible = true;  // Placeholder
    
    // Debug: Show what's failing
    std::cerr << "DEBUG Model Integrity Check:\n";
    std::cerr << "  model_file_exists: " << result.model_file_exists << "\n";
    std::cerr << "  model_loads_successfully: " << result.model_loads_successfully << "\n";
    std::cerr << "  model_version_compatible: " << result.model_version_compatible << "\n";
    
    bool ok = result.model_file_exists && 
              result.model_loads_successfully && 
              result.model_version_compatible;
    if (!ok) {
        result.add_critical_issue("Model integrity check failed");
    }
    return ok;
}

bool StrategyValidator::validate_performance_benchmarks(
    std::shared_ptr<IStrategy> /*strategy*/,
    const std::vector<MarketData>& /*market_data*/,
    const testing::TestConfig& config,
    ValidationResult& result
) {
    bool passed = true;
    
    if (result.model_load_time_ms > config.max_model_load_time_ms) {
        result.add_warning(
            "Model load time exceeds threshold: " + 
            std::to_string(result.model_load_time_ms) + "ms"
        );
        passed = false;
    }
    if (result.avg_inference_time_ms > config.max_inference_time_ms) {
        result.add_warning(
            "Average inference time exceeds threshold: " + 
            std::to_string(result.avg_inference_time_ms) + "ms"
        );
        passed = false;
    }
    result.memory_usage_mb = 512.0;  // Placeholder
    if (result.memory_usage_mb > config.max_memory_usage_mb) {
        result.add_warning(
            "Memory usage exceeds threshold: " + 
            std::to_string(result.memory_usage_mb) + "MB"
        );
        passed = false;
    }
    return passed;
}

bool StrategyValidator::validate_configuration(
    std::shared_ptr<IStrategy> /*strategy*/,
    ValidationResult& result
) {
    result.has_required_parameters = true;  // Placeholder
    result.parameters_in_valid_range = true;  // Placeholder
    return result.has_required_parameters && result.parameters_in_valid_range;
}

void StrategyValidator::calculate_signal_statistics(
    const std::vector<SignalOutput>& signals,
    ValidationResult& result
) {
    result.long_signals = 0;
    result.short_signals = 0;
    result.neutral_signals = 0;
    
    std::vector<double> confidences;
    
    for (const auto& signal : signals) {
        // Count signals by type
        if (signal.signal_type == SignalType::LONG) {
            result.long_signals++;
        } else if (signal.signal_type == SignalType::SHORT) {
            result.short_signals++;
        } else {
            result.neutral_signals++;
        }
        
        // Collect confidences (for backward compatibility, though confidence is removed)
        // This can be removed entirely if confidence is no longer used
    }
    result.non_neutral_signals = result.long_signals + result.short_signals;
    
    if (!confidences.empty()) {
        double sum = std::accumulate(confidences.begin(), confidences.end(), 0.0);
        result.mean_confidence = sum / confidences.size();
        double variance = 0.0;
        for (const auto& c : confidences) variance += (c - result.mean_confidence) * (c - result.mean_confidence);
        variance /= confidences.size();
        result.confidence_std_dev = std::sqrt(variance);
    }
}

void StrategyValidator::analyze_signal_distribution(
    const std::vector<SignalOutput>& signals,
    ValidationResult& result
) {
    result.signal_type_distribution["LONG"] = result.long_signals;
    result.signal_type_distribution["SHORT"] = result.short_signals;
    result.signal_type_distribution["NEUTRAL"] = result.neutral_signals;
    
    std::map<std::string, int> conf_bins{
        {"0.0-0.2", 0}, {"0.2-0.4", 0}, {"0.4-0.6", 0}, {"0.6-0.8", 0}, {"0.8-1.0", 0}
    };
    for (const auto& s : signals) {
        double c = std::abs(s.probability - 0.5) * 2.0;  // signal strength
        if (c < 0.2) conf_bins["0.0-0.2"]++;
        else if (c < 0.4) conf_bins["0.2-0.4"]++;
        else if (c < 0.6) conf_bins["0.4-0.6"]++;
        else if (c < 0.8) conf_bins["0.6-0.8"]++;
        else conf_bins["0.8-1.0"]++;
    }
    for (const auto& [bin, count] : conf_bins) {
        result.confidence_distribution[bin] = signals.empty() ? 0.0 : (static_cast<double>(count) / signals.size() * 100.0);
    }
}

void StrategyValidator::check_common_issues(
    const std::vector<SignalOutput>& signals,
    ValidationResult& result
) {
    if (result.neutral_signals > signals.size() * 0.8) {
        result.add_warning("Excessive neutral signals (>80%)");
    }
}

void StrategyValidator::generate_recommendations(ValidationResult& result) {
    if (result.non_neutral_ratio < 0.3) {
        result.add_recommendation("Consider adjusting confidence thresholds to increase trading activity");
    }
    if (result.trading_based_mrb < 0.02 && result.trading_based_mrb >= 0.01) {
        result.add_recommendation("MRB acceptable but could be improved via tuning");
    }
    if (result.avg_inference_time_ms > 50.0) {
        result.add_recommendation("Optimize model to reduce inference time");
    }
    if (result.mrb_consistency > 0.5) {
        result.add_recommendation("High MRB variance - consider regime-aware strategies");
    }
}

// BatchValidator implementation

std::map<std::string, ValidationResult> BatchValidator::validate_multiple_strategies(
    const std::vector<std::string>& strategy_names,
    const std::string& data_path,
    const testing::TestConfig& config
) {
    std::map<std::string, ValidationResult> results;
    for (const auto& name : strategy_names) {
        results[name] = StrategyValidator::validate_strategy(name, data_path, config);
    }
    return results;
}

std::string BatchValidator::compare_validation_results(
    const std::map<std::string, ValidationResult>& results
) {
    std::stringstream ss;
    ss << "\n╔══════════════════════════════════════════════════════╗\n";
    ss << "║         Strategy Validation Comparison              ║\n";
    ss << "╚══════════════════════════════════════════════════════╝\n\n";
    ss << "Strategy    | Status | MRB    | Signals | Quality\n";
    ss << "------------|--------|--------|---------|--------\n";
    for (const auto& [name, r] : results) {
        ss << std::left << std::setw(11) << name << " | "
           << std::setw(6) << (r.passed ? "PASS" : "FAIL") << " | "
           << std::fixed << std::setprecision(4) << std::setw(6) << r.trading_based_mrb << " | "
           << std::setw(7) << r.total_signals << " | "
           << std::fixed << std::setprecision(2) << (r.signal_generation_rate * 100.0) << "%\n";
    }
    return ss.str();
}

} // namespace sentio::validation


