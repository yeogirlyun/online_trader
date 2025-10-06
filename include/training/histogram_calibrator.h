#pragma once

#include <vector>
#include <string>

class HistogramCalibrator {
private:
    static constexpr int NUM_BINS = 20;
    std::vector<float> bin_edges_;
    std::vector<float> bin_probabilities_;
    
public:
    void fit(const std::vector<float>& predictions, const std::vector<float>& labels);
    float transform(float raw_score) const;
    void save(const std::string& path) const;
    bool load(const std::string& path);
    
    // Getters for validation
    int get_num_bins() const { return NUM_BINS; }
    const std::vector<float>& get_bin_edges() const { return bin_edges_; }
    const std::vector<float>& get_bin_probabilities() const { return bin_probabilities_; }
};

