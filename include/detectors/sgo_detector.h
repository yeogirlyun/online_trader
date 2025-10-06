#pragma once
#include "core/detector_interface.h"
#include <deque>
#include <vector>

namespace sentio {

class SGODetector : public IDetector {
public:
    SGODetector(const std::string& name, const nlohmann::json& config);

    double process(const ::sentio::Bar& bar) override;
    void reset() override;
    std::string get_name() const override { return name_; }
    bool should_abstain() const override;

private:
    std::string name_;
    nlohmann::json config_;

    // SGO-specific parameters
    int lookback_period_;
    double threshold_;
    double momentum_weight_;

    // State
    std::deque<double> price_history_;
    double last_signal_strength_;
    bool abstain_;

    // Internal calculations
    double calculate_sgo_probability(const Bar& bar);
    double calculate_momentum();
    double calculate_volatility();
};

} // namespace sentio





