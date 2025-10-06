#pragma once

#include "strategy/xgb_feature_set.h"
#include "strategy/intraday_ml_strategies.h"

namespace sentio {

class XGBFeatureSet25Intraday : public XGBFeatureSet {
public:
    XGBFeatureSet25Intraday();
    ~XGBFeatureSet25Intraday() override = default;

    const std::string& name() const override { return name_; }
    size_t featureCount() const override { return 25; }
    const std::vector<std::string>& featureNames() const override { return names_; }

    void reset() override;
    void update(const ::sentio::Bar& bar) override;
    bool isReady() const override;
    void extract(std::vector<float>& outFeatures) const override;

private:
    std::string name_ = "25Intraday";
    std::vector<std::string> names_;
    std::unique_ptr<intraday::IntradayFeatureExtractor> extractor_;
};

} // namespace sentio





