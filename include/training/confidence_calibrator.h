#pragma once

#include <vector>
#include <deque>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace sentio {
namespace training {

/**
 * @brief Production-ready confidence calibration system
 * 
 * This class monitors the relationship between predicted confidence and actual
 * prediction accuracy, providing calibrated confidence scores for trading decisions.
 * 
 * Key features:
 * - Real-time confidence calibration tracking
 * - Rolling window statistics for adaptive calibration
 * - Calibration report generation for performance monitoring
 * - Blended confidence scoring for improved reliability
 */
class ConfidenceCalibrator {
private:
    struct CalibrationBin {
        double conf_min, conf_max;
        std::deque<bool> outcomes;  // Rolling window of prediction outcomes
        static constexpr size_t MAX_SIZE = 1000;  // Rolling window size
        
        /**
         * @brief Get actual accuracy for this confidence bin
         * @return Accuracy as fraction [0,1]
         */
        double get_actual_accuracy() const {
            if (outcomes.empty()) return 0.5;  // Default to neutral if no data
            return std::accumulate(outcomes.begin(), outcomes.end(), 0.0) / outcomes.size();
        }
        
        /**
         * @brief Add new prediction outcome to rolling window
         * @param correct Whether the prediction was correct
         */
        void add_outcome(bool correct) {
            outcomes.push_back(correct);
            if (outcomes.size() > MAX_SIZE) {
                outcomes.pop_front();
            }
        }
        
        /**
         * @brief Get number of samples in this bin
         */
        size_t size() const {
            return outcomes.size();
        }
        
        /**
         * @brief Check if bin has sufficient data for reliable calibration
         */
        bool has_sufficient_data() const {
            return outcomes.size() >= 10;  // Minimum samples for reliability
        }
    };
    
    std::vector<CalibrationBin> bins_;
    size_t total_samples_;
    double calibration_blend_factor_;  // How much to blend with actual accuracy
    
public:
    /**
     * @brief Constructor with configurable parameters
     * @param num_bins Number of confidence bins (default: 10)
     * @param blend_factor Blending factor for calibration (default: 0.3)
     */
    explicit ConfidenceCalibrator(size_t num_bins = 10, double blend_factor = 0.3)
        : total_samples_(0), calibration_blend_factor_(blend_factor) {
        
        // Initialize confidence bins with equal ranges
        bins_.reserve(num_bins);
        for (size_t i = 0; i < num_bins; ++i) {
            double min_conf = static_cast<double>(i) / num_bins;
            double max_conf = static_cast<double>(i + 1) / num_bins;
            bins_.push_back({min_conf, max_conf});
        }
    }
    
    /**
     * @brief Update calibrator with new prediction outcome
     * @param confidence Raw confidence from model [0,1]
     * @param prediction_correct Whether the prediction was correct
     */
    void update(double confidence, bool prediction_correct) {
        // Clamp confidence to valid range
        confidence = std::max(0.0, std::min(1.0, confidence));
        
        // Find appropriate bin
        int bin_idx = std::min(static_cast<int>(bins_.size() - 1), 
                              static_cast<int>(confidence * bins_.size()));
        
        bins_[bin_idx].add_outcome(prediction_correct);
        total_samples_++;
    }
    
    /**
     * @brief Get calibrated confidence score
     * @param raw_confidence Raw confidence from model
     * @return Calibrated confidence blending raw and observed accuracy
     */
    double get_calibrated_confidence(double raw_confidence) const {
        // Clamp to valid range
        raw_confidence = std::max(0.0, std::min(1.0, raw_confidence));
        
        // Find appropriate bin
        int bin_idx = std::min(static_cast<int>(bins_.size() - 1), 
                              static_cast<int>(raw_confidence * bins_.size()));
        
        const auto& bin = bins_[bin_idx];
        
        // If insufficient data, return raw confidence
        if (!bin.has_sufficient_data()) {
            return raw_confidence;
        }
        
        // Blend raw confidence with observed accuracy
        double actual_accuracy = bin.get_actual_accuracy();
        double calibrated = (1.0 - calibration_blend_factor_) * raw_confidence + 
                           calibration_blend_factor_ * actual_accuracy;
        
        return std::max(0.0, std::min(1.0, calibrated));
    }
    
    /**
     * @brief Print comprehensive calibration report
     */
    void print_calibration_report() const {
        std::cout << "\n📊 CONFIDENCE CALIBRATION REPORT\n";
        std::cout << "================================\n";
        std::cout << "Total Samples: " << total_samples_ << "\n";
        std::cout << "Blend Factor: " << calibration_blend_factor_ << "\n\n";
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Conf Range    | Samples | Expected | Actual  | Calibration | Error\n";
        std::cout << "------------- | ------- | -------- | ------- | ----------- | -----\n";
        
        double total_calibration_error = 0.0;
        size_t bins_with_data = 0;
        
        for (const auto& bin : bins_) {
            if (bin.size() > 0) {
                double expected = (bin.conf_min + bin.conf_max) / 2.0;
                double actual = bin.get_actual_accuracy();
                double calibrated = get_calibrated_confidence(expected);
                double error = std::abs(expected - actual);
                
                std::cout << "[" << bin.conf_min << "-" << bin.conf_max << "] | "
                         << std::setw(7) << bin.size() << " | "
                         << std::setw(8) << expected << " | "
                         << std::setw(7) << actual << " | "
                         << std::setw(11) << calibrated << " | "
                         << std::setw(5) << error << "\n";
                
                total_calibration_error += error;
                bins_with_data++;
            }
        }
        
        if (bins_with_data > 0) {
            double avg_calibration_error = total_calibration_error / bins_with_data;
            std::cout << "\nAverage Calibration Error: " << avg_calibration_error << "\n";
            
            // Provide calibration quality assessment
            if (avg_calibration_error < 0.05) {
                std::cout << "✅ Excellent calibration quality!\n";
            } else if (avg_calibration_error < 0.10) {
                std::cout << "✅ Good calibration quality.\n";
            } else if (avg_calibration_error < 0.20) {
                std::cout << "⚠️  Fair calibration quality - consider retraining.\n";
            } else {
                std::cout << "❌ Poor calibration quality - recalibration needed!\n";
            }
        }
        
        std::cout << std::endl;
    }
    
    /**
     * @brief Get calibration statistics for monitoring
     */
    struct CalibrationStats {
        double avg_calibration_error;
        size_t total_samples;
        size_t bins_with_data;
        double coverage;  // Fraction of confidence range with data
    };
    
    CalibrationStats get_stats() const {
        CalibrationStats stats = {0.0, total_samples_, 0, 0.0};
        
        double total_error = 0.0;
        for (const auto& bin : bins_) {
            if (bin.has_sufficient_data()) {
                double expected = (bin.conf_min + bin.conf_max) / 2.0;
                double actual = bin.get_actual_accuracy();
                total_error += std::abs(expected - actual);
                stats.bins_with_data++;
            }
        }
        
        if (stats.bins_with_data > 0) {
            stats.avg_calibration_error = total_error / stats.bins_with_data;
            stats.coverage = static_cast<double>(stats.bins_with_data) / bins_.size();
        }
        
        return stats;
    }
    
    /**
     * @brief Reset calibration data (for retraining scenarios)
     */
    void reset() {
        for (auto& bin : bins_) {
            bin.outcomes.clear();
        }
        total_samples_ = 0;
    }
    
    /**
     * @brief Check if calibrator has sufficient data for reliable calibration
     */
    bool is_calibrated() const {
        return total_samples_ >= 100 && get_stats().bins_with_data >= 3;
    }
};

} // namespace training
} // namespace sentio
