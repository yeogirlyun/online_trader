#pragma once

#include <vector>
#include <algorithm>
#include <fstream>
#include <string>

class IsotonicCalibrator {
public:
    struct CalibrationPoint {
        float score;
        float calibrated_prob;
        
        CalibrationPoint(float s = 0.0f, float p = 0.0f) : score(s), calibrated_prob(p) {}
    };
    
private:
    std::vector<CalibrationPoint> calibration_map_;
    float min_score_ = 0.0f;
    float max_score_ = 1.0f;
    
public:
    void fit(const std::vector<float>& predictions, const std::vector<float>& labels);
    float transform(float raw_score) const;
    void save(const std::string& path) const;
    bool load(const std::string& path);
    
    // Getters for validation
    size_t get_calibration_points_count() const { return calibration_map_.size(); }
    float get_min_score() const { return min_score_; }
    float get_max_score() const { return max_score_; }
    
private:
    void pool_adjacent_violators(std::vector<float>& y, const std::vector<float>& weights);
};

