// src/validation/validation_result.cpp
#include "validation/validation_result.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace sentio::validation {

void ValidationResult::add_critical_issue(const std::string& issue) {
    critical_issues.push_back(issue);
}

void ValidationResult::add_warning(const std::string& warning) {
    warnings.push_back(warning);
}

void ValidationResult::add_recommendation(const std::string& recommendation) {
    recommendations.push_back(recommendation);
}

void ValidationResult::add_info(const std::string& info) {
    info_messages.push_back(info);
}

void ValidationResult::calculate_validation_status() {
    // Determine if passed based on critical checks
    if (!critical_issues.empty()) {
        passed = false;
        deployment_ready = false;
        status_message = "Critical validation failures";
        return;
    }
    
    // Check all required validations
    bool all_checks_passed = 
        signal_quality_passed &&
        mrb_threshold_passed &&
        model_integrity_passed &&
        performance_benchmark_passed &&
        configuration_valid;
    
    passed = all_checks_passed;
    
    // Deployment readiness requires passing + no warnings
    deployment_ready = passed && warnings.empty();
    
    if (deployment_ready) {
        status_message = "Ready for deployment";
    } else if (passed) {
        status_message = "Validation passed with warnings";
    } else {
        status_message = "Validation failed";
    }
    
    // Set timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    timestamp = ss.str();
}

std::string ValidationResult::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"strategy_name\": \"" << strategy_name << "\",\n";
    ss << "  \"data_path\": \"" << data_path << "\",\n";
    ss << "  \"timestamp\": \"" << timestamp << "\",\n";
    ss << "  \"passed\": " << (passed ? "true" : "false") << ",\n";
    ss << "  \"deployment_ready\": " << (deployment_ready ? "true" : "false") << ",\n";
    ss << "  \"status_message\": \"" << status_message << "\",\n";
    
    // Validation checks
    ss << "  \"validation_checks\": {\n";
    ss << "    \"signal_quality_passed\": " << (signal_quality_passed ? "true" : "false") << ",\n";
    ss << "    \"mrb_threshold_passed\": " << (mrb_threshold_passed ? "true" : "false") << ",\n";
    ss << "    \"model_integrity_passed\": " << (model_integrity_passed ? "true" : "false") << ",\n";
    ss << "    \"performance_benchmark_passed\": " << (performance_benchmark_passed ? "true" : "false") << ",\n";
    ss << "    \"configuration_valid\": " << (configuration_valid ? "true" : "false") << "\n";
    ss << "  },\n";
    
    // Signal quality metrics
    ss << "  \"signal_quality_metrics\": {\n";
    ss << "    \"signal_generation_rate\": " << signal_generation_rate << ",\n";
    ss << "    \"non_neutral_ratio\": " << non_neutral_ratio << ",\n";
    ss << "    \"mean_confidence\": " << mean_confidence << ",\n";
    ss << "    \"confidence_std_dev\": " << confidence_std_dev << ",\n";
    ss << "    \"total_signals\": " << total_signals << ",\n";
    ss << "    \"non_neutral_signals\": " << non_neutral_signals << ",\n";
    ss << "    \"long_signals\": " << long_signals << ",\n";
    ss << "    \"short_signals\": " << short_signals << ",\n";
    ss << "    \"neutral_signals\": " << neutral_signals << "\n";
    ss << "  },\n";
    
    // MRB metrics
    ss << "  \"mrb_metrics\": {\n";
    ss << "    \"signal_accuracy\": " << signal_accuracy << ",\n";
    ss << "    \"trading_based_mrb\": " << trading_based_mrb << ",\n";
    ss << "    \"mrb_consistency\": " << mrb_consistency << "\n";
    ss << "  },\n";
    
    // Performance metrics
    ss << "  \"performance_metrics\": {\n";
    ss << "    \"model_load_time_ms\": " << model_load_time_ms << ",\n";
    ss << "    \"avg_inference_time_ms\": " << avg_inference_time_ms << ",\n";
    ss << "    \"max_inference_time_ms\": " << max_inference_time_ms << ",\n";
    ss << "    \"min_inference_time_ms\": " << min_inference_time_ms << ",\n";
    ss << "    \"memory_usage_mb\": " << memory_usage_mb << ",\n";
    ss << "    \"peak_memory_mb\": " << peak_memory_mb << "\n";
    ss << "  },\n";
    
    // Trading performance metrics
    ss << "  \"trading_performance\": {\n";
    ss << "    \"sharpe_ratio\": " << sharpe_ratio << ",\n";
    ss << "    \"max_drawdown\": " << max_drawdown << ",\n";
    ss << "    \"win_rate\": " << win_rate << ",\n";
    ss << "    \"profit_factor\": " << profit_factor << ",\n";
    ss << "    \"total_return\": " << total_return << ",\n";
    ss << "    \"volatility\": " << volatility << "\n";
    ss << "  },\n";
    
    // Critical issues
    ss << "  \"critical_issues\": [\n";
    for (size_t i = 0; i < critical_issues.size(); ++i) {
        ss << "    \"" << critical_issues[i] << "\"";
        if (i < critical_issues.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Warnings
    ss << "  \"warnings\": [\n";
    for (size_t i = 0; i < warnings.size(); ++i) {
        ss << "    \"" << warnings[i] << "\"";
        if (i < warnings.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Recommendations
    ss << "  \"recommendations\": [\n";
    for (size_t i = 0; i < recommendations.size(); ++i) {
        ss << "    \"" << recommendations[i] << "\"";
        if (i < recommendations.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string ValidationResult::to_report() const {
    std::stringstream ss;
    
    ss << "\n";
    ss << "═══════════════════════════════════════════════════════════════\n";
    ss << "  STRATEGY VALIDATION REPORT\n";
    ss << "═══════════════════════════════════════════════════════════════\n";
    ss << "\n";
    
    ss << "Strategy: " << strategy_name << "\n";
    ss << "Data: " << data_path << "\n";
    ss << "Timestamp: " << timestamp << "\n";
    ss << "Status: " << (passed ? "✓ PASSED" : "✗ FAILED") << "\n";
    ss << "Deployment Ready: " << (deployment_ready ? "✓ YES" : "✗ NO") << "\n";
    ss << "Blocks Tested: " << blocks_tested << "\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "  VALIDATION CHECKS\n";
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "\n";
    
    ss << (signal_quality_passed ? "✓" : "✗") << " Signal Quality\n";
    ss << (mrb_threshold_passed ? "✓" : "✗") << " MRB Threshold\n";
    ss << (model_integrity_passed ? "✓" : "✗") << " Model Integrity\n";
    ss << (performance_benchmark_passed ? "✓" : "✗") << " Performance Benchmark\n";
    ss << (configuration_valid ? "✓" : "✗") << " Configuration Valid\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "  SIGNAL QUALITY METRICS\n";
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "\n";
    ss << std::left << std::setw(30) << "Signal Generation Rate:" 
       << std::fixed << std::setprecision(2) << (signal_generation_rate * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Non-Neutral Ratio:" 
       << std::fixed << std::setprecision(2) << (non_neutral_ratio * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Mean Confidence:" 
       << std::fixed << std::setprecision(3) << mean_confidence << "\n";
    ss << std::left << std::setw(30) << "Total Signals:" << total_signals << "\n";
    ss << std::left << std::setw(30) << "  Long Signals:" << long_signals << "\n";
    ss << std::left << std::setw(30) << "  Short Signals:" << short_signals << "\n";
    ss << std::left << std::setw(30) << "  Neutral Signals:" << neutral_signals << "\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "  PERFORMANCE METRICS\n";
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "\n";
    ss << std::left << std::setw(30) << "Signal Accuracy:" 
       << std::fixed << std::setprecision(2) << (signal_accuracy * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Trading MRB:" 
       << std::fixed << std::setprecision(4) << trading_based_mrb << "\n";
    ss << std::left << std::setw(30) << "MRB Consistency:" 
       << std::fixed << std::setprecision(4) << mrb_consistency << "\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "  PERFORMANCE METRICS\n";
    ss << "───────────────────────────────────────────────────────────────\n";
    ss << "\n";
    ss << std::left << std::setw(30) << "Model Load Time:" 
       << std::fixed << std::setprecision(2) << model_load_time_ms << "ms\n";
    ss << std::left << std::setw(30) << "Avg Inference Time:" 
       << std::fixed << std::setprecision(2) << avg_inference_time_ms << "ms\n";
    ss << std::left << std::setw(30) << "Memory Usage:" 
       << std::fixed << std::setprecision(1) << memory_usage_mb << "MB\n";
    ss << std::left << std::setw(30) << "Sharpe Ratio:" 
       << std::fixed << std::setprecision(3) << sharpe_ratio << "\n";
    ss << std::left << std::setw(30) << "Max Drawdown:" 
       << std::fixed << std::setprecision(2) << (max_drawdown * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Win Rate:" 
       << std::fixed << std::setprecision(2) << (win_rate * 100.0) << "%\n";
    ss << "\n";
    
    if (!critical_issues.empty()) {
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "  CRITICAL ISSUES\n";
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "\n";
        for (const auto& issue : critical_issues) {
            ss << "  ❌ " << issue << "\n";
        }
        ss << "\n";
    }
    
    if (!warnings.empty()) {
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "  WARNINGS\n";
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "\n";
        for (const auto& warning : warnings) {
            ss << "  ⚠️  " << warning << "\n";
        }
        ss << "\n";
    }
    
    if (!recommendations.empty()) {
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "  RECOMMENDATIONS\n";
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "\n";
        for (size_t i = 0; i < recommendations.size(); ++i) {
            ss << "  " << (i + 1) << ". " << recommendations[i] << "\n";
        }
        ss << "\n";
    }
    
    if (!info_messages.empty()) {
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "  INFORMATION\n";
        ss << "───────────────────────────────────────────────────────────────\n";
        ss << "\n";
        for (const auto& info : info_messages) {
            ss << "  ℹ️  " << info << "\n";
        }
        ss << "\n";
    }
    
    ss << "═══════════════════════════════════════════════════════════════\n";
    ss << "  END OF VALIDATION REPORT\n";
    ss << "═══════════════════════════════════════════════════════════════\n";
    
    return ss.str();
}

std::string ValidationResult::get_summary() const {
    std::stringstream ss;
    
    ss << strategy_name << " - ";
    ss << (passed ? "PASSED" : "FAILED");
    
    if (deployment_ready) {
        ss << " (Deployment Ready)";
    } else if (passed) {
        ss << " (With Warnings)";
    }
    
    ss << " | MRB: " << std::fixed << std::setprecision(4) << trading_based_mrb;
    ss << " | Signals: " << total_signals;
    
    return ss.str();
}

std::string ValidationResult::get_deployment_assessment() const {
    std::stringstream ss;
    
    ss << "\n";
    ss << "╔══════════════════════════════════════════════════════╗\n";
    ss << "║         DEPLOYMENT READINESS ASSESSMENT             ║\n";
    ss << "╚══════════════════════════════════════════════════════╝\n";
    ss << "\n";
    
    if (deployment_ready) {
        ss << "✅ READY FOR DEPLOYMENT\n\n";
        ss << "The strategy has passed all validation checks and meets\n";
        ss << "the required thresholds for production deployment.\n";
    } else if (passed) {
        ss << "⚠️  CONDITIONAL DEPLOYMENT\n\n";
        ss << "The strategy passed validation but has warnings that should\n";
        ss << "be reviewed before deployment:\n\n";
        for (const auto& warning : warnings) {
            ss << "  • " << warning << "\n";
        }
    } else {
        ss << "❌ NOT READY FOR DEPLOYMENT\n\n";
        ss << "The strategy failed critical validation checks:\n\n";
        for (const auto& issue : critical_issues) {
            ss << "  • " << issue << "\n";
        }
    }
    
    ss << "\n";
    ss << "Key Metrics:\n";
    ss << "  Trading MRB: " << std::fixed << std::setprecision(4) << trading_based_mrb << "\n";
    ss << "  Signal Quality: " << std::fixed << std::setprecision(1) 
       << (signal_generation_rate * 100.0) << "%\n";
    ss << "  Sharpe Ratio: " << std::fixed << std::setprecision(3) << sharpe_ratio << "\n";
    
    if (!recommendations.empty()) {
        ss << "\nRecommended Actions:\n";
        for (size_t i = 0; i < std::min(size_t(3), recommendations.size()); ++i) {
            ss << "  " << (i + 1) << ". " << recommendations[i] << "\n";
        }
    }
    
    ss << "\n";
    
    return ss.str();
}

} // namespace sentio::validation


