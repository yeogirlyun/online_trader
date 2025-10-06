#pragma once

#include "strategy/xgb_feature_set.h"
#include "core/detector_interface.h"
#include <deque>

namespace sentio {

class XGBFeatureSet8Detector : public XGBFeatureSet {
public:
    XGBFeatureSet8Detector();
    ~XGBFeatureSet8Detector() override = default;

    const std::string& name() const override { return name_; }
    size_t featureCount() const override { return 8; }
    const std::vector<std::string>& featureNames() const override { return names_; }

    void reset() override;
    void update(const ::sentio::Bar& bar) override;
    bool isReady() const override { return ready_; }
    void extract(std::vector<float>& outFeatures) const override;

private:
    std::string name_ = "8Detector";
    std::vector<std::string> names_ = {"sgo1","sgo2","sgo3","sgo4","sgo5","sgo6","sgo7","awr"};
    std::vector<std::unique_ptr<IDetector>> detectors_;
    bool ready_ = false;
    std::array<float,8> last_probs_{};
};

} // namespace sentio


