// src/testing/test_result.cpp
#include "testing/test_result.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace sentio::testing {

void TestResult::add_check(const CheckResult& check) {
    checks.push_back(check);
    check_status[check.name] = check.passed;
}

void TestResult::add_metric(const std::string& name, double value) {
    metrics[name] = value;
}

void TestResult::add_recommendation(const std::string& recommendation) {
    recommendations.push_back(recommendation);
}

void TestResult::add_warning(const std::string& warning) {
    warnings.push_back(warning);
}

void TestResult::add_error(const std::string& error) {
    errors.push_back(error);
}

void TestResult::calculate_overall_score() {
    if (checks.empty()) {
        overall_score = 0.0;
        return;
    }
    
    // Calculate score based on passed checks (50%) and metrics (50%)
    double check_score = 0.0;
    int critical_checks = 0;
    int passed_checks = 0;
    
    for (const auto& check : checks) {
        if (check.severity == "critical") {
            critical_checks++;
            if (check.passed) passed_checks++;
        }
    }
    
    if (critical_checks > 0) {
        check_score = (static_cast<double>(passed_checks) / critical_checks) * 50.0;
    }
    
    // Metrics score based on MRB and other performance indicators
    double metrics_score = 0.0;
    
    // MRB score (20 points)
    if (trading_based_mrb >= 0.020) metrics_score += 20.0;
    else if (trading_based_mrb >= 0.015) metrics_score += 15.0;
    else if (trading_based_mrb >= 0.010) metrics_score += 10.0;
    else if (trading_based_mrb >= 0.005) metrics_score += 5.0;
    
    // Signal quality score (15 points)
    if (signal_generation_rate >= 0.95 && non_neutral_ratio >= 0.20) {
        metrics_score += 15.0;
    } else if (signal_generation_rate >= 0.90 && non_neutral_ratio >= 0.15) {
        metrics_score += 10.0;
    } else if (signal_generation_rate >= 0.85) {
        metrics_score += 5.0;
    }
    
    // Performance score (15 points)
    if (sharpe_ratio >= 0.8) metrics_score += 15.0;
    else if (sharpe_ratio >= 0.6) metrics_score += 10.0;
    else if (sharpe_ratio >= 0.4) metrics_score += 5.0;
    
    overall_score = check_score + metrics_score;
    overall_score = std::min(100.0, std::max(0.0, overall_score));
}

void TestResult::determine_status() {
    if (!errors.empty()) {
        status = TestStatus::ERROR;
        status_message = "Test execution error";
        return;
    }
    
    // Check critical failures
    bool critical_failure = false;
    for (const auto& check : checks) {
        if (check.severity == "critical" && !check.passed) {
            critical_failure = true;
            break;
        }
    }
    
    if (critical_failure) {
        status = TestStatus::FAILED;
        status_message = "Critical checks failed";
        return;
    }
    
    // Check warnings
    bool has_warnings = !warnings.empty();
    for (const auto& check : checks) {
        if (check.severity == "warning" && !check.passed) {
            has_warnings = true;
            break;
        }
    }
    
    if (has_warnings) {
        status = TestStatus::CONDITIONAL;
        status_message = "Passed with warnings";
    } else {
        status = TestStatus::PASSED;
        status_message = "All checks passed";
    }
}

std::string TestResult::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"strategy_name\": \"" << strategy_name << "\",\n";
    ss << "  \"status\": \"" << get_status_string() << "\",\n";
    ss << "  \"status_message\": \"" << status_message << "\",\n";
    ss << "  \"overall_score\": " << overall_score << ",\n";
    ss << "  \"execution_time_ms\": " << execution_time_ms << ",\n";
    
    // Metrics
    ss << "  \"metrics\": {\n";
    ss << "    \"signal_accuracy\": " << signal_accuracy << ",\n";
    ss << "    \"trading_based_mrb\": " << trading_based_mrb << ",\n";
    ss << "    \"sharpe_ratio\": " << sharpe_ratio << ",\n";
    ss << "    \"max_drawdown\": " << max_drawdown << ",\n";
    ss << "    \"win_rate\": " << win_rate << ",\n";
    ss << "    \"total_signals\": " << total_signals << ",\n";
    ss << "    \"non_neutral_signals\": " << non_neutral_signals << ",\n";
    ss << "    \"signal_generation_rate\": " << signal_generation_rate << ",\n";
    ss << "    \"mean_confidence\": " << mean_confidence << "\n";
    ss << "  },\n";
    
    // Checks
    ss << "  \"checks\": [\n";
    for (size_t i = 0; i < checks.size(); ++i) {
        const auto& check = checks[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << check.name << "\",\n";
        ss << "      \"passed\": " << (check.passed ? "true" : "false") << ",\n";
        ss << "      \"value\": " << check.value << ",\n";
        ss << "      \"threshold\": " << check.threshold << ",\n";
        ss << "      \"severity\": \"" << check.severity << "\"\n";
        ss << "    }";
        if (i < checks.size() - 1) ss << ",";
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
    ss << "  ],\n";
    
    // Warnings
    ss << "  \"warnings\": [\n";
    for (size_t i = 0; i < warnings.size(); ++i) {
        ss << "    \"" << warnings[i] << "\"";
        if (i < warnings.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    // Errors
    ss << "  \"errors\": [\n";
    for (size_t i = 0; i < errors.size(); ++i) {
        ss << "    \"" << errors[i] << "\"";
        if (i < errors.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string TestResult::to_report() const {
    std::stringstream ss;
    
    ss << "\n";
    ss << "═══════════════════════════════════════════════════════════\n";
    ss << "  SENTIO STRATEGY TEST REPORT\n";
    ss << "═══════════════════════════════════════════════════════════\n";
    ss << "\n";
    
    ss << "Strategy: " << strategy_name << "\n";
    ss << "Status: " << get_status_string() << "\n";
    ss << "Overall Score: " << std::fixed << std::setprecision(1) << overall_score << "/100.0\n";
    ss << "Execution Time: " << std::fixed << std::setprecision(2) << execution_time_ms << "ms\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────\n";
    ss << "  PERFORMANCE METRICS\n";
    ss << "───────────────────────────────────────────────────────────\n";
    ss << "\n";
    ss << std::left << std::setw(30) << "Signal Accuracy:" 
       << std::fixed << std::setprecision(2) << (signal_accuracy * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Trading-based MRB:" 
       << std::fixed << std::setprecision(4) << trading_based_mrb << "\n";
    ss << std::left << std::setw(30) << "Sharpe Ratio:" 
       << std::fixed << std::setprecision(3) << sharpe_ratio << "\n";
    ss << std::left << std::setw(30) << "Max Drawdown:" 
       << std::fixed << std::setprecision(2) << (max_drawdown * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Win Rate:" 
       << std::fixed << std::setprecision(2) << (win_rate * 100.0) << "%\n";
    ss << "\n";
    
    ss << "───────────────────────────────────────────────────────────\n";
    ss << "  SIGNAL QUALITY\n";
    ss << "───────────────────────────────────────────────────────────\n";
    ss << "\n";
    ss << std::left << std::setw(30) << "Total Signals:" << total_signals << "\n";
    ss << std::left << std::setw(30) << "Non-Neutral Signals:" << non_neutral_signals << "\n";
    ss << std::left << std::setw(30) << "Signal Generation Rate:" 
       << std::fixed << std::setprecision(2) << (signal_generation_rate * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Non-Neutral Ratio:" 
       << std::fixed << std::setprecision(2) << (non_neutral_ratio * 100.0) << "%\n";
    ss << std::left << std::setw(30) << "Mean Confidence:" 
       << std::fixed << std::setprecision(3) << mean_confidence << "\n";
    ss << "\n";
    
    if (!checks.empty()) {
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "  VALIDATION CHECKS\n";
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "\n";
        
        for (const auto& check : checks) {
            std::string icon = check.passed ? "✓" : "✗";
            ss << icon << " " << check.name << ": " 
               << std::fixed << std::setprecision(4) << check.value;
            
            if (!check.passed) {
                ss << " (threshold: " << check.threshold << ")";
                if (check.severity == "critical") {
                    ss << " [CRITICAL]";
                } else if (check.severity == "warning") {
                    ss << " [WARNING]";
                }
            }
            ss << "\n";
        }
        ss << "\n";
    }
    
    if (!recommendations.empty()) {
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "  RECOMMENDATIONS\n";
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "\n";
        
        for (size_t i = 0; i < recommendations.size(); ++i) {
            ss << "  " << (i + 1) << ". " << recommendations[i] << "\n";
        }
        ss << "\n";
    }
    
    if (!warnings.empty()) {
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "  WARNINGS\n";
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "\n";
        
        for (const auto& warning : warnings) {
            ss << "  ⚠️  " << warning << "\n";
        }
        ss << "\n";
    }
    
    if (!errors.empty()) {
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "  ERRORS\n";
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "\n";
        
        for (const auto& error : errors) {
            ss << "  ❌ " << error << "\n";
        }
        ss << "\n";
    }
    
    ss << "═══════════════════════════════════════════════════════════\n";
    ss << "  END OF REPORT\n";
    ss << "═══════════════════════════════════════════════════════════\n";
    
    return ss.str();
}

std::string TestResult::get_status_string() const {
    switch (status) {
        case TestStatus::PASSED:
            return "PASSED";
        case TestStatus::CONDITIONAL:
            return "CONDITIONAL";
        case TestStatus::FAILED:
            return "FAILED";
        case TestStatus::ERROR:
            return "ERROR";
        case TestStatus::NOT_RUN:
            return "NOT_RUN";
        default:
            return "UNKNOWN";
    }
}

bool TestResult::passed() const {
    return status == TestStatus::PASSED || status == TestStatus::CONDITIONAL;
}

// ComparisonResult implementation

std::string ComparisonResult::to_json() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "{\n";
    ss << "  \"best_strategy\": \"" << best_strategy << "\",\n";
    ss << "  \"worst_strategy\": \"" << worst_strategy << "\",\n";
    ss << "  \"rankings\": [\n";
    
    for (size_t i = 0; i < rankings.size(); ++i) {
        ss << "    {\"strategy\": \"" << rankings[i].first << "\", ";
        ss << "\"score\": " << rankings[i].second << "}";
        if (i < rankings.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string ComparisonResult::to_report() const {
    std::stringstream ss;
    
    ss << "\n";
    ss << "═══════════════════════════════════════════════════════════\n";
    ss << "  STRATEGY COMPARISON REPORT\n";
    ss << "═══════════════════════════════════════════════════════════\n";
    ss << "\n";
    
    ss << "Best Strategy: " << best_strategy << "\n";
    ss << "Worst Strategy: " << worst_strategy << "\n";
    ss << "\n";
    
    ss << "Rankings:\n";
    for (size_t i = 0; i < rankings.size(); ++i) {
        ss << "  " << (i + 1) << ". " << rankings[i].first 
           << " (Score: " << std::fixed << std::setprecision(1) 
           << rankings[i].second << ")\n";
    }
    
    return ss.str();
}

} // namespace sentio::testing


