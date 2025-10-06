#pragma once
#include "core/detector_interface.h"
#include "common/types.h"
#include <vector>
#include <deque>

namespace sentio {

class AWRDetector : public IDetector {
public:
    AWRDetector(const nlohmann::json& config);

    double process(const ::sentio::Bar& bar) override;
    void reset() override;
    std::string get_name() const override { return "awr"; }
    bool should_abstain() const override { return abstain_; }

private:
    // AWR parameters
    int window_size_;
    double risk_threshold_;
    double adaptive_factor_;

    // State
    std::deque<::sentio::Bar> bar_history_;
    double current_risk_level_;
    bool abstain_;

    // AWR calculations
    double calculate_awr_probability(const ::sentio::Bar& bar);
    double assess_risk_level();
    double calculate_adaptive_weight();
};

} // namespace sentio


