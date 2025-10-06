#pragma once

#include <torch/torch.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace sentio::training {

/**
 * Training Data Quality Validator
 * 
 * Ensures training data has sufficient signal-to-noise ratio and 
 * proper label distribution to prevent model collapse
 */
class TrainingDataValidator {
public:
    struct DataQualityMetrics {
        double label_balance_score;      // 0-1, higher is better
        double temporal_consistency;     // 0-1, higher is better  
        double feature_informativeness;  // Higher is better
        double predictability_score;     // 0-1, higher is better
        double signal_to_noise_ratio;    // Higher is better
        bool is_acceptable;
        
        // Detailed breakdown
        double buy_percentage;
        double sell_percentage;
        double hold_percentage;
        double feature_mean_variance;
        std::string quality_summary;
    };
    
    /**
     * Comprehensive validation of training data quality
     */
    DataQualityMetrics validate_training_data(
        const std::vector<torch::Tensor>& features,
        const std::vector<float>& targets
    ) {
        DataQualityMetrics metrics = {};
        
        // 1. Label Distribution Analysis
        analyze_label_distribution(targets, metrics);
        
        // 2. Feature Quality Assessment
        analyze_feature_quality(features, metrics);
        
        // 3. Temporal Consistency Check
        analyze_temporal_consistency(targets, metrics);
        
        // 4. Signal-to-Noise Ratio
        calculate_signal_to_noise_ratio(features, targets, metrics);
        
        // 5. Overall Assessment
        assess_overall_quality(metrics);
        
        return metrics;
    }
    
    /**
     * Quick validation for streaming data
     */
    bool quick_validate_batch(
        const torch::Tensor& features,
        const torch::Tensor& targets
    ) {
        // Check for NaN or infinite values
        if (torch::any(torch::isnan(features)).item<bool>() || 
            torch::any(torch::isnan(targets)).item<bool>()) {
            std::cerr << "❌ NaN values detected in batch!" << std::endl;
            return false;
        }
        
        // Check feature variance (must have signal)
        torch::Tensor feature_stds = torch::std(features, /*dim=*/0);
        double min_std = torch::min(feature_stds).item<double>();
        
        if (min_std < 1e-6) {
            std::cerr << "❌ Zero variance features detected!" << std::endl;
            return false;
        }
        
        // Check target diversity
        torch::Tensor target_std = torch::std(targets);
        if (target_std.item<double>() < 0.01) {
            std::cerr << "❌ No target diversity in batch!" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void print_quality_report(const DataQualityMetrics& metrics) {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "📊 TRAINING DATA QUALITY REPORT\n";
        std::cout << std::string(60, '=') << "\n";
        
        std::cout << std::fixed << std::setprecision(2);
        
        // Label distribution
        std::cout << "📈 Label Distribution:\n";
        std::cout << "   BUY:  " << std::setw(6) << metrics.buy_percentage << "%";
        std::cout << (metrics.buy_percentage > 20 ? " ✅" : " ⚠️") << "\n";
        
        std::cout << "   SELL: " << std::setw(6) << metrics.sell_percentage << "%";
        std::cout << (metrics.sell_percentage > 20 ? " ✅" : " ⚠️") << "\n";
        
        std::cout << "   HOLD: " << std::setw(6) << metrics.hold_percentage << "%";
        std::cout << (metrics.hold_percentage < 60 ? " ✅" : " ⚠️") << "\n";
        
        // Quality scores
        std::cout << "\n🔍 Quality Metrics:\n";
        std::cout << "   Balance Score:      " << std::setw(6) << (metrics.label_balance_score * 100) << "%";
        std::cout << (metrics.label_balance_score > 0.7 ? " ✅" : " ❌") << "\n";
        
        std::cout << "   Feature Info:       " << std::setw(8) << std::setprecision(4) << metrics.feature_informativeness;
        std::cout << (metrics.feature_informativeness > 0.01 ? " ✅" : " ❌") << "\n";
        
        std::cout << "   Signal/Noise:       " << std::setw(8) << std::setprecision(2) << metrics.signal_to_noise_ratio;
        std::cout << (metrics.signal_to_noise_ratio > 1.5 ? " ✅" : " ❌") << "\n";
        
        std::cout << "   Temporal Consist:   " << std::setw(6) << (metrics.temporal_consistency * 100) << "%";
        std::cout << (metrics.temporal_consistency > 0.6 ? " ✅" : " ⚠️") << "\n";
        
        // Overall assessment
        std::cout << "\n🎯 Overall Assessment: ";
        if (metrics.is_acceptable) {
            std::cout << "✅ ACCEPTABLE FOR TRAINING\n";
        } else {
            std::cout << "❌ REQUIRES DATA IMPROVEMENT\n";
        }
        
        std::cout << "   Summary: " << metrics.quality_summary << "\n";
        std::cout << std::string(60, '=') << std::endl;
    }
    
private:
    void analyze_label_distribution(
        const std::vector<float>& targets,
        DataQualityMetrics& metrics
    ) {
        int buy_count = 0, sell_count = 0, hold_count = 0;
        
        for (float target : targets) {
            if (target > 0.0001) buy_count++;
            else if (target < -0.0001) sell_count++;
            else hold_count++;
        }
        
        double total = static_cast<double>(targets.size());
        metrics.buy_percentage = (buy_count / total) * 100.0;
        metrics.sell_percentage = (sell_count / total) * 100.0;
        metrics.hold_percentage = (hold_count / total) * 100.0;
        
        // Calculate balance score (penalize extreme imbalance)
        double buy_pct = buy_count / total;
        double sell_pct = sell_count / total;
        double hold_pct = hold_count / total;
        
        // Ideal distribution: ~35% BUY, ~35% SELL, ~30% HOLD
        double buy_ideal = 0.35, sell_ideal = 0.35, hold_ideal = 0.30;
        double deviation = std::abs(buy_pct - buy_ideal) + 
                          std::abs(sell_pct - sell_ideal) + 
                          std::abs(hold_pct - hold_ideal);
        
        metrics.label_balance_score = std::max(0.0, 1.0 - deviation);
        
        // Warn about insufficient directional signals
        if (buy_pct < 0.2 || sell_pct < 0.2) {
            std::cerr << "⚠️ WARNING: Insufficient directional labels! ";
            std::cerr << "Buy: " << (buy_pct*100) << "%, Sell: " << (sell_pct*100) << "%\n";
        }
    }
    
    void analyze_feature_quality(
        const std::vector<torch::Tensor>& features,
        DataQualityMetrics& metrics
    ) {
        if (features.empty()) {
            metrics.feature_informativeness = 0.0; // No features available
            return;
        }
        
        // Stack all features to analyze variance
        torch::Tensor all_features = torch::stack(features);
        
        // Calculate standard deviation for each feature dimension
        torch::Tensor feature_stds = torch::std(all_features, /*dim=*/0);
        metrics.feature_mean_variance = torch::mean(feature_stds).item<double>();
        metrics.feature_informativeness = metrics.feature_mean_variance;
        
        // Check for dead features (zero variance)
        int dead_features = (feature_stds < 1e-6).sum().item<int>();
        if (dead_features > 0) {
            std::cerr << "⚠️ WARNING: " << dead_features << " dead features detected!\n";
        }
    }
    
    void analyze_temporal_consistency(
        const std::vector<float>& targets,
        DataQualityMetrics& metrics
    ) {
        if (targets.size() < 10) {
            metrics.temporal_consistency = 0.0;
            return;
        }
        
        // Calculate autocorrelation at lag 1
        double mean = std::accumulate(targets.begin(), targets.end(), 0.0) / targets.size();
        
        double numerator = 0.0, denominator = 0.0;
        for (size_t i = 1; i < targets.size(); ++i) {
            numerator += (targets[i-1] - mean) * (targets[i] - mean);
        }
        
        for (float target : targets) {
            denominator += (target - mean) * (target - mean);
        }
        
        if (denominator > 1e-10) {
            double autocorr = numerator / denominator;
            metrics.temporal_consistency = std::max(0.0, std::min(1.0, autocorr + 0.5));
        } else {
            metrics.temporal_consistency = 0.0;
        }
    }
    
    void calculate_signal_to_noise_ratio(
        const std::vector<torch::Tensor>& features,
        const std::vector<float>& targets,
        DataQualityMetrics& metrics
    ) {
        if (features.empty() || targets.empty()) {
            metrics.signal_to_noise_ratio = 0.0;
            return;
        }
        
        // Simple signal-to-noise estimate using feature variance vs target variance
        torch::Tensor all_features = torch::stack(features);
        torch::Tensor feature_var = torch::var(all_features);
        
        torch::Tensor target_tensor = torch::tensor(targets);
        torch::Tensor target_var = torch::var(target_tensor);
        
        double feature_variance = feature_var.item<double>();
        double target_variance = target_var.item<double>();
        
        if (target_variance > 1e-10) {
            metrics.signal_to_noise_ratio = std::sqrt(feature_variance / target_variance);
        } else {
            metrics.signal_to_noise_ratio = 0.0;
        }
    }
    
    void assess_overall_quality(DataQualityMetrics& metrics) {
        std::vector<std::string> issues;
        
        // Check each quality metric
        if (metrics.label_balance_score < 0.7) {
            issues.push_back("Poor label balance");
        }
        
        if (metrics.feature_informativeness < 0.01) {
            issues.push_back("Low feature informativeness");
        }
        
        if (metrics.signal_to_noise_ratio < 1.5) {
            issues.push_back("Low signal-to-noise ratio");
        }
        
        if (metrics.temporal_consistency < 0.3) {
            issues.push_back("Poor temporal consistency");
        }
        
        if (metrics.buy_percentage < 20 || metrics.sell_percentage < 20) {
            issues.push_back("Insufficient directional signals");
        }
        
        // Overall assessment
        metrics.is_acceptable = issues.empty();
        
        if (issues.empty()) {
            // No validation issues found
            metrics.quality_summary = "High quality training data";
        } else {
            metrics.quality_summary = "Issues: " + std::to_string(issues.size());
            for (size_t i = 0; i < issues.size() && i < 2; ++i) {
                metrics.quality_summary += (i == 0 ? " - " : ", ") + issues[i];
            }
        }
    }
};

} // namespace sentio::training
