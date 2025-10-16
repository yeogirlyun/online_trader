#pragma once
#include "common/types.h"
#include <deque>
#include <vector>
#include <optional>
#include <string>

namespace sentio {

enum class MarketRegime {
    TRENDING_UP,
    TRENDING_DOWN,
    CHOPPY,
    HIGH_VOLATILITY,
    LOW_VOLATILITY
};

struct RegimeFeatures {
    double vol = 0.0;   // std of log-returns
    double slope = 0.0; // slope of log-price
    double r2 = 0.0;    // R² of the regression
    double chop = 50.0; // CHOP index
};

struct MarketRegimeDetectorParams {
    int vol_window = 96;     // for std(log-returns)
    int slope_window = 120;  // for slope & R²
    int chop_window = 48;    // for CHOP
    int calibr_window = 8 * 390; // ~8 trading days of 1-min bars
    double trend_slope_min = 1.2e-4; // slope threshold (log-price / bar)
    double trend_r2_min = 0.60;      // require some linearity
    double hysteresis_margin = 0.15; // score margin to switch regimes
    int cooldown_bars = 60;          // bars before allowing another switch
};

class MarketRegimeDetector {
public:
    using Params = MarketRegimeDetectorParams;

    MarketRegimeDetector();
    explicit MarketRegimeDetector(const Params& p);

    MarketRegime detect(const std::vector<Bar>& bars);

    // Legacy API compatibility
    MarketRegime detect_regime(const std::vector<Bar>& recent_bars) {
        return detect(recent_bars);
    }

    // For testing/telemetry
    RegimeFeatures last_features() const { return last_feat_; }
    std::pair<double,double> vol_thresholds() const { return {vol_lo_, vol_hi_}; }
    MarketRegime last_regime() const { return last_regime_.value_or(MarketRegime::CHOPPY); }

    // Get regime name as string
    static std::string regime_to_string(MarketRegime regime);

private:
    Params p_;
    std::deque<double> vol_cal_; // rolling volatility samples for adaptive thresholds
    double vol_lo_ = 0.0, vol_hi_ = 0.0; // adaptive thresholds (p30/p70)
    std::optional<MarketRegime> last_regime_;
    int cooldown_ = 0;
    RegimeFeatures last_feat_{};

    // feature helpers
    static double std_log_returns(const std::vector<Bar>& v, int win);
    static void slope_r2_log_price(const std::vector<Bar>& v, int win, double& slope, double& r2);
    static double chop_index(const std::vector<Bar>& v, int win);

    // thresholds
    void update_vol_thresholds(double vol_sample);
    static double percentile(std::vector<double>& tmp, double q);
};

} // namespace sentio
