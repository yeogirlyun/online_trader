#pragma once

#include "strategy/xgb_feature_set.h"
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace sentio {

class XGBFeatureSet60SA : public XGBFeatureSet {
public:
    static constexpr size_t FEATURE_COUNT = 45;
    static constexpr size_t MIN_WARMUP_BARS = 50;
    static constexpr size_t MAX_LOOKBACK = 50;

    XGBFeatureSet60SA();
    ~XGBFeatureSet60SA() override = default;

    const std::string& name() const override { return name_; }
    size_t featureCount() const override { return FEATURE_COUNT; }
    const std::vector<std::string>& featureNames() const override { return feature_names_; }

    void reset() override;
    void update(const ::sentio::Bar& bar) override;
    bool isReady() const override { return bars_seen_ >= MIN_WARMUP_BARS; }
    void extract(std::vector<float>& outFeatures) const override;

private:
    std::string name_ = "60SA";
    std::vector<std::string> feature_names_;
    
    // Historical data storage
    std::deque<::sentio::Bar> history_;
    size_t bars_seen_ = 0;
    
    // Cached calculations for efficiency
    mutable std::vector<double> returns_cache_;
    mutable std::vector<double> log_returns_cache_;
    mutable bool cache_valid_ = false;
    
    // Helper methods for feature calculation
    void updateHistory(const ::sentio::Bar& bar);
    void calculateReturns() const;
    
    // Feature calculation methods
    double calculateReturn(size_t lag) const;
    double calculateLogReturn(size_t lag) const;
    double calculateRealizedVolatility(size_t window) const;
    double calculateRSI(size_t period) const;
    double calculateBollingerPosition(size_t period, double num_std) const;
    double calculateMACD() const;
    double calculateATR(size_t period) const;
    double calculateSMA(size_t period) const;
    double calculateEMA(size_t period) const;
    double calculateVWAP(size_t period) const;
    double calculateOBV() const;
    double calculateStochastic(size_t period) const;
    double calculateWilliamsR(size_t period) const;
    double calculateADX(size_t period) const;
    double calculateCCI(size_t period) const;
    double calculateROC(size_t period) const;
    double calculateMFI(size_t period) const;
    double calculateVolumeRatio(size_t period) const;
    double calculatePriceEfficiency(size_t period) const;
    double calculateKurtosis(size_t period) const;
    double calculateSkewness(size_t period) const;
    double calculateAutoCorrelation(size_t lag) const;
    double calculateHurstExponent() const;
    double calculateMicrostructureNoise() const;
    double calculateInformationRatio(size_t period) const;
    
    // Utility methods
    double safeDivide(double numerator, double denominator, double default_value = 0.0) const;
    double normalize(double value, double min_val, double max_val) const;
    double robustZScore(double value, const std::vector<double>& data) const;
};

} // namespace sentio
