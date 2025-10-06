// src/validation/walk_forward_validator.cpp
#include "validation/walk_forward_validator.h"
#include "analysis/performance_analyzer.h"
#include "common/utils.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>

namespace sentio::validation {

WalkForwardResult WalkForwardValidator::validate(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data,
    const WalkForwardConfig& config
) {
    WalkForwardResult result;
    result.strategy_name = strategy->get_strategy_name();
    result.config = config;
    
    const size_t BLOCK_SIZE = 480;  // Standard block size
    
    // Calculate total bars needed
    size_t train_bars = config.train_window_blocks * BLOCK_SIZE;
    size_t test_bars = config.test_window_blocks * BLOCK_SIZE;
    size_t step_bars = config.step_size_blocks * BLOCK_SIZE;
    size_t min_window_bars = train_bars + test_bars;
    
    if (market_data.size() < min_window_bars) {
        result.passed = false;
        result.assessment = "FAILED";
        result.issues.push_back("Insufficient data: need " + std::to_string(min_window_bars) + 
                               " bars, have " + std::to_string(market_data.size()));
        return result;
    }
    
    // Generate windows based on mode
    std::vector<std::tuple<size_t, size_t, size_t, size_t>> windows;  // train_start, train_end, test_start, test_end
    
    switch (config.mode) {
        case WindowMode::ROLLING: {
            // Fixed-size sliding window
            for (size_t start = 0; start + min_window_bars <= market_data.size(); start += step_bars) {
                size_t train_start = start;
                size_t train_end = start + train_bars;
                size_t test_start = train_end;
                size_t test_end = std::min(test_start + test_bars, market_data.size());
                
                if (test_end - test_start >= test_bars) {  // Ensure full test window
                    windows.push_back({train_start, train_end, test_start, test_end});
                }
            }
            break;
        }
        
        case WindowMode::ANCHORED: {
            // Growing from fixed start point
            size_t anchor_start = 0;
            for (size_t train_end = train_bars; train_end + test_bars <= market_data.size(); train_end += step_bars) {
                size_t test_start = train_end;
                size_t test_end = std::min(test_start + test_bars, market_data.size());
                
                if (test_end - test_start >= test_bars) {
                    windows.push_back({anchor_start, train_end, test_start, test_end});
                }
            }
            break;
        }
        
        case WindowMode::EXPANDING: {
            // Growing from beginning
            for (size_t test_start = train_bars; test_start + test_bars <= market_data.size(); test_start += step_bars) {
                size_t train_start = 0;
                size_t train_end = test_start;
                size_t test_end = std::min(test_start + test_bars, market_data.size());
                
                if (test_end - test_start >= test_bars && train_end - train_start >= train_bars) {
                    windows.push_back({train_start, train_end, test_start, test_end});
                }
            }
            break;
        }
    }
    
    if (windows.empty()) {
        result.passed = false;
        result.assessment = "FAILED";
        result.issues.push_back("No valid windows generated");
        return result;
    }
    
    std::cout << "\n🔄 Walk-Forward Validation: " << windows.size() << " windows\n";
    std::cout << "   Mode: " << (config.mode == WindowMode::ROLLING ? "ROLLING" : 
                                 config.mode == WindowMode::ANCHORED ? "ANCHORED" : "EXPANDING") << "\n";
    std::cout << "   Train: " << config.train_window_blocks << " blocks, Test: " 
              << config.test_window_blocks << " blocks, Step: " << config.step_size_blocks << " blocks\n\n";
    
    // Process each window
    for (size_t i = 0; i < windows.size(); ++i) {
        auto [train_start, train_end, test_start, test_end] = windows[i];
        
        std::cout << "  Window " << (i + 1) << "/" << windows.size() 
                  << " [Train: " << train_start << "-" << train_end 
                  << ", Test: " << test_start << "-" << test_end << "]... " << std::flush;
        
        WindowResult window_result = process_window(
            strategy, market_data, config,
            train_start, train_end, test_start, test_end, i
        );
        
        result.windows.push_back(window_result);
        
        std::cout << (window_result.passed ? "✓" : "✗") 
                  << " (Test MRB: " << (window_result.test_mrb * 100.0) << "%)" << std::endl;
    }
    
    // Calculate aggregate statistics
    calculate_aggregate_statistics(result);
    calculate_statistical_significance(result);
    calculate_confidence_intervals(result);
    detect_overfitting(result);
    generate_assessment(result);
    
    return result;
}

WalkForwardResult WalkForwardValidator::quick_validate(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data,
    double min_mrb
) {
    WalkForwardConfig config;
    config.mode = WindowMode::ROLLING;
    config.train_window_blocks = 40;
    config.test_window_blocks = 10;
    config.step_size_blocks = 10;
    config.min_mrb_threshold = min_mrb;
    
    return validate(strategy, market_data, config);
}

WindowResult WalkForwardValidator::process_window(
    std::shared_ptr<IStrategy> strategy,
    const std::vector<MarketData>& market_data,
    const WalkForwardConfig& config,
    size_t train_start,
    size_t train_end,
    size_t test_start,
    size_t test_end,
    int window_index
) {
    // IMMEDIATE LOGGING - Print to terminal (THIS SHOULD WORK!)
    if (window_index == 0) {
        std::cout << "\n\n🚨🚨🚨 PROCESS_WINDOW CALLED FOR WINDOW 0 🚨🚨🚨\n";
        std::cout << "train_start=" << train_start << " train_end=" << train_end << "\n";
        std::cout << "test_start=" << test_start << " test_end=" << test_end << "\n";
        std::cout.flush();
    }
    
    WindowResult result;
    result.window_index = window_index;
    result.train_start_bar = train_start;
    result.train_end_bar = train_end;
    result.test_start_bar = test_start;
    result.test_end_bar = test_end;
    
    try {
        // Extract data slices
        std::vector<MarketData> train_data(
            market_data.begin() + train_start,
            market_data.begin() + train_end
        );
        std::vector<MarketData> test_data(
            market_data.begin() + test_start,
            market_data.begin() + test_end
        );
        
        // TODO: If config.enable_optimization, run Optuna on train_data here
        
        // CRITICAL BUG FIX: Initialize strategy before first use
        if (window_index == 0) {
            StrategyComponent::StrategyConfig dummy_config;
            if (!strategy->initialize(dummy_config)) {
                throw std::runtime_error("Strategy initialization failed");
            }
        }
        
        // CRITICAL: Reset strategy state before each window to prevent contamination
        strategy->reset();
        
        // Generate signals for training data (in-sample)
        auto train_signals = strategy->process_data(train_data);
        result.train_signals = train_signals.size();
        for (const auto& sig : train_signals) {
            if (sig.signal_type != SignalType::NEUTRAL) {
                result.train_non_neutral++;
            }
        }
        
        // Calculate training metrics
        result.train_accuracy = analysis::PerformanceAnalyzer::calculate_signal_accuracy(
            train_signals, train_data
        );
        {
            analysis::PSMValidationConfig psm_cfg;
            psm_cfg.temp_directory = ":memory:"; // force in-memory MRB path
            result.train_mrb = analysis::PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
                train_signals, train_data, config.train_window_blocks, psm_cfg
            );
        }
        
        // NOTE: Do NOT reset here - strategy should carry forward indicator warmup from training
        // This mirrors real trading where indicators maintain state across time periods
        // Resetting here causes cold-start bias where test window starts without warmup
        
        // Generate signals for test data (out-of-sample)
        auto test_signals = strategy->process_data(test_data);
        result.test_signals = test_signals.size();
        
        // IMMEDIATE DIAGNOSTIC OUTPUT - Window 0 only
        if (window_index == 0) {
            std::cout << "\n🔍 SIGNALS GENERATED:\n";
            std::cout << "  Test data bars: " << test_data.size() << "\n";
            std::cout << "  Test signals: " << test_signals.size() << "\n";
            std::cout.flush();
        }
        
        for (const auto& sig : test_signals) {
            if (sig.signal_type != SignalType::NEUTRAL) {
                result.test_non_neutral++;
            }
        }
        
        // More diagnostic output
        if (window_index == 0) {
            std::cout << "  Non-neutral: " << result.test_non_neutral << " (" 
                      << (test_signals.size() > 0 ? (result.test_non_neutral * 100.0 / test_signals.size()) : 0.0)
                      << "%)\n";
            std::cout << "  First 5 signals:\n";
            for (size_t i = 0; i < std::min(size_t(5), test_signals.size()); ++i) {
                std::string type = (test_signals[i].signal_type == SignalType::NEUTRAL) ? "NEUT" :
                                  (test_signals[i].signal_type == SignalType::LONG) ? "LONG" : "SHORT";
                std::cout << "    [" << i << "] " << type << " prob=" << test_signals[i].probability << "\n";
            }
            std::cout.flush();
        }
        
        // DIAGNOSTIC: Always log first window to debug zero MRB issue
        if (window_index == 0) {
            std::cerr << "\n🔍 DIAGNOSTIC Window " << window_index << ":" << std::endl;
            std::cerr << "  Train bars: " << train_data.size() << ", Test bars: " << test_data.size() << std::endl;
            std::cerr << "  Train signals: " << result.train_signals << " (" 
                      << result.train_non_neutral << " non-neutral = "
                      << (result.train_signals > 0 ? (result.train_non_neutral * 100.0 / result.train_signals) : 0.0)
                      << "%)" << std::endl;
            std::cerr << "  Test signals: " << result.test_signals << " (" 
                      << result.test_non_neutral << " non-neutral = "
                      << (result.test_signals > 0 ? (result.test_non_neutral * 100.0 / result.test_signals) : 0.0)
                      << "%)" << std::endl;
            
            // Show sample test signals
            std::cerr << "  Sample test signals (first 5):" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), test_signals.size()); ++i) {
                const auto& sig = test_signals[i];
                std::string type_str = (sig.signal_type == SignalType::NEUTRAL) ? "NEUT" :
                                      (sig.signal_type == SignalType::LONG) ? "LONG" : "SHORT";
                std::cerr << "    [" << i << "] " << type_str << " prob=" 
                          << std::fixed << std::setprecision(4) << sig.probability << std::endl;
            }
        }
        
        // Calculate test metrics
        result.test_accuracy = analysis::PerformanceAnalyzer::calculate_signal_accuracy(
            test_signals, test_data
        );
        {
            analysis::PSMValidationConfig psm_cfg;
            psm_cfg.temp_directory = ":memory:";
            result.test_mrb = analysis::PerformanceAnalyzer::calculate_trading_based_mrb_with_psm(
                test_signals, test_data, config.test_window_blocks, psm_cfg
            );
        }
        
        // Calculate degradation
        if (result.train_mrb > 0.0) {
            result.degradation_ratio = (result.train_mrb - result.test_mrb) / result.train_mrb;
        } else {
            result.degradation_ratio = 0.0;
        }
        
        result.is_overfit = (result.degradation_ratio > config.max_degradation_ratio);
        
        // Check pass criteria
        result.passed = (result.test_mrb >= config.min_mrb_threshold) && !result.is_overfit;
        
        if (!result.passed) {
            if (result.test_mrb < config.min_mrb_threshold) {
                result.failure_reason = "Low MRB: " + std::to_string(result.test_mrb * 100.0) + "%";
            } else if (result.is_overfit) {
                result.failure_reason = "Overfitting: " + std::to_string(result.degradation_ratio * 100.0) + "% degradation";
            }
        }
        
    } catch (const std::exception& e) {
        // CRITICAL: Print exception to see what's failing!
        std::cout << "\n❌❌❌ EXCEPTION in process_window: " << e.what() << "\n";
        std::cout.flush();
        result.passed = false;
        result.failure_reason = std::string("Exception: ") + e.what();
    }
    
    return result;
}

void WalkForwardValidator::calculate_aggregate_statistics(WalkForwardResult& result) {
    if (result.windows.empty()) return;
    
    // Collect test MRBs
    std::vector<double> test_mrbs;
    std::vector<double> train_mrbs;
    std::vector<double> degradations;
    
    for (const auto& window : result.windows) {
        test_mrbs.push_back(window.test_mrb);
        train_mrbs.push_back(window.train_mrb);
        degradations.push_back(window.degradation_ratio);
        
        if (window.passed) {
            result.passing_windows++;
        }
        if (window.is_overfit) {
            result.overfit_windows++;
        }
    }
    
    result.total_windows = result.windows.size();
    result.win_rate = static_cast<double>(result.passing_windows) / result.total_windows;
    result.overfit_percentage = static_cast<double>(result.overfit_windows) / result.total_windows;
    
    // Calculate mean and std
    result.mean_test_mrb = std::accumulate(test_mrbs.begin(), test_mrbs.end(), 0.0) / test_mrbs.size();
    result.mean_train_mrb = std::accumulate(train_mrbs.begin(), train_mrbs.end(), 0.0) / train_mrbs.size();
    result.mean_degradation = std::accumulate(degradations.begin(), degradations.end(), 0.0) / degradations.size();
    
    double variance = 0.0;
    for (const auto& mrb : test_mrbs) {
        variance += (mrb - result.mean_test_mrb) * (mrb - result.mean_test_mrb);
    }
    variance /= test_mrbs.size();
    result.std_test_mrb = std::sqrt(variance);
    
    // Consistency score (1.0 = perfect consistency, 0.0 = very inconsistent)
    if (std::abs(result.mean_test_mrb) > 0.0001) {
        result.consistency_score = std::max(0.0, 1.0 - (result.std_test_mrb / std::abs(result.mean_test_mrb)));
    } else {
        result.consistency_score = 0.0;
    }
}

void WalkForwardValidator::calculate_statistical_significance(WalkForwardResult& result) {
    if (result.windows.size() < 2) {
        result.statistically_significant = false;
        return;
    }
    
    // One-sample t-test: H0: mean_test_mrb = 0
    double n = static_cast<double>(result.windows.size());
    double std_error = result.std_test_mrb / std::sqrt(n);
    
    if (std_error > 0.0) {
        result.t_statistic = result.mean_test_mrb / std_error;
        
        // Approximate p-value using normal distribution (valid for n > 30)
        // For small n, this is conservative
        double z = std::abs(result.t_statistic);
        
        // Two-tailed p-value approximation
        // For z > 2.58 (99%), p < 0.01; z > 1.96 (95%), p < 0.05
        if (z >= 1.96) {
            result.p_value = 0.05;  // Approximation
            result.statistically_significant = true;
        } else {
            result.p_value = 0.1;  // Approximation (not significant)
            result.statistically_significant = false;
        }
    } else {
        result.statistically_significant = false;
    }
}

void WalkForwardValidator::calculate_confidence_intervals(WalkForwardResult& result) {
    if (result.windows.size() < 2) {
        result.ci_lower_95 = result.mean_test_mrb;
        result.ci_upper_95 = result.mean_test_mrb;
        return;
    }
    
    // 95% confidence interval using t-distribution
    // For simplicity, use z = 1.96 (valid for large n)
    double n = static_cast<double>(result.windows.size());
    double std_error = result.std_test_mrb / std::sqrt(n);
    double margin = 1.96 * std_error;  // 95% CI
    
    result.ci_lower_95 = result.mean_test_mrb - margin;
    result.ci_upper_95 = result.mean_test_mrb + margin;
}

void WalkForwardValidator::detect_overfitting(WalkForwardResult& result) {
    // Already detected per-window in process_window
    // This method can add aggregate overfitting metrics if needed
    
    // Check if training performance is significantly better than test
    if (result.mean_train_mrb > 0.0 && result.mean_test_mrb > 0.0) {
        double aggregate_degradation = (result.mean_train_mrb - result.mean_test_mrb) / result.mean_train_mrb;
        
        if (aggregate_degradation > result.config.max_degradation_ratio) {
            result.issues.push_back(
                "Overall overfitting detected: " + 
                std::to_string(aggregate_degradation * 100.0) + "% degradation (max: " +
                std::to_string(result.config.max_degradation_ratio * 100.0) + "%)"
            );
        }
    }
}

void WalkForwardValidator::generate_assessment(WalkForwardResult& result) {
    // Check pass criteria
    bool mrb_sufficient = (result.mean_test_mrb >= result.config.min_mrb_threshold);
    bool ci_positive = (result.ci_lower_95 > 0.0);
    bool high_win_rate = (result.win_rate >= 0.6);
    bool consistent = (result.consistency_score >= 0.6);
    bool significant = result.statistically_significant;
    bool low_overfitting = (result.overfit_percentage < 0.3);
    
    int passed_criteria = 0;
    if (mrb_sufficient) passed_criteria++;
    if (ci_positive) passed_criteria++;
    if (high_win_rate) passed_criteria++;
    if (consistent) passed_criteria++;
    if (significant) passed_criteria++;
    if (low_overfitting) passed_criteria++;
    
    // Generate assessment
    if (passed_criteria >= 5) {
        result.assessment = "EXCELLENT";
        result.passed = true;
    } else if (passed_criteria >= 4) {
        result.assessment = "GOOD";
        result.passed = true;
    } else if (passed_criteria >= 3) {
        result.assessment = "FAIR";
        result.passed = false;
        result.recommendations.push_back("Strategy shows potential but needs improvement");
    } else if (passed_criteria >= 2) {
        result.assessment = "POOR";
        result.passed = false;
        result.recommendations.push_back("Significant improvements needed");
    } else {
        result.assessment = "FAILED";
        result.passed = false;
        result.recommendations.push_back("Strategy not ready for production");
    }
    
    // Add specific issues
    if (!mrb_sufficient) {
        result.issues.push_back("Mean test MRB (" + std::to_string(result.mean_test_mrb * 100.0) + 
                               "%) below threshold (" + std::to_string(result.config.min_mrb_threshold * 100.0) + "%)");
    }
    if (!ci_positive) {
        result.issues.push_back("95% CI lower bound is negative (" + std::to_string(result.ci_lower_95 * 100.0) + "%)");
    }
    if (!high_win_rate) {
        result.issues.push_back("Low win rate (" + std::to_string(result.win_rate * 100.0) + "%, target: 60%+)");
    }
    if (!consistent) {
        result.issues.push_back("Inconsistent performance (consistency: " + std::to_string(result.consistency_score * 100.0) + "%, target: 60%+)");
    }
    if (!significant) {
        result.issues.push_back("Results not statistically significant (p-value: " + std::to_string(result.p_value) + ")");
    }
    if (!low_overfitting) {
        result.issues.push_back("High overfitting rate (" + std::to_string(result.overfit_percentage * 100.0) + "%)");
    }
    
    // Add recommendations
    if (result.mean_test_mrb < result.config.min_mrb_threshold) {
        result.recommendations.push_back("Improve signal quality or adjust strategy parameters");
    }
    if (result.overfit_percentage > 0.3) {
        result.recommendations.push_back("Reduce model complexity or increase regularization");
    }
    if (result.consistency_score < 0.6) {
        result.recommendations.push_back("Investigate regime-dependent performance");
    }
}

} // namespace sentio::validation

