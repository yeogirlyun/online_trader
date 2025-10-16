#include "strategy/market_regime_detector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sentio {

static inline double safe_log(double x) { return std::log(std::max(1e-12, x)); }

MarketRegimeDetector::MarketRegimeDetector() : p_(MarketRegimeDetectorParams{}) {}

MarketRegimeDetector::MarketRegimeDetector(const Params& p) : p_(p) {}

double MarketRegimeDetector::std_log_returns(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 0.0;
    const int n0 = (int)v.size() - win;
    std::vector<double> r; r.reserve(win);
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        r.push_back(safe_log(v[i].close / v[i-1].close));
    }
    double mean = std::accumulate(r.begin(), r.end(), 0.0) / r.size();
    double acc = 0.0;
    for (double x : r) { double d = x - mean; acc += d * d; }
    return std::sqrt(acc / std::max<size_t>(1, r.size() - 1));
}

void MarketRegimeDetector::slope_r2_log_price(const std::vector<Bar>& v, int win, double& slope, double& r2) {
    slope = 0.0; r2 = 0.0;
    if ((int)v.size() < win) return;
    const int n0 = (int)v.size() - win;
    std::vector<double> y; y.reserve(win);
    for (int i = n0; i < (int)v.size(); ++i) y.push_back(safe_log(v[i].close));

    // x = 0..win-1
    const double n = (double)win;
    const double sx = (n - 1.0) * n / 2.0;
    const double sxx = (n - 1.0) * n * (2.0 * n - 1.0) / 6.0;
    double sy = 0.0, sxy = 0.0;
    for (int i = 0; i < win; ++i) { sy += y[i]; sxy += i * y[i]; }

    const double denom = n * sxx - sx * sx;
    if (std::abs(denom) < 1e-12) return;
    slope = (n * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / n;

    // R²
    double ss_tot = 0.0, ss_res = 0.0;
    const double y_bar = sy / n;
    for (int i = 0; i < win; ++i) {
        const double y_hat = intercept + slope * i;
        ss_res += (y[i] - y_hat) * (y[i] - y_hat);
        ss_tot += (y[i] - y_bar) * (y[i] - y_bar);
    }
    r2 = (ss_tot > 0.0) ? (1.0 - ss_res / ss_tot) : 0.0;
}

double MarketRegimeDetector::chop_index(const std::vector<Bar>& v, int win) {
    if ((int)v.size() < win + 1) return 50.0;
    const int n0 = (int)v.size() - win;
    double atr_sum = 0.0;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        const auto& c = v[i];
        const auto& p = v[i-1];
        const double tr = std::max({ c.high - c.low,
                                     std::abs(c.high - p.close),
                                     std::abs(c.low  - p.close) });
        atr_sum += tr;
    }
    double hi = v[n0].high, lo = v[n0].low;
    for (int i = n0 + 1; i < (int)v.size(); ++i) {
        hi = std::max(hi, v[i].high);
        lo = std::min(lo, v[i].low);
    }
    const double range = std::max(1e-12, hi - lo);
    const double x = std::log10(std::max(1e-12, atr_sum / range));
    const double denom = std::log10((double)win);
    return 100.0 * x / std::max(1e-12, denom); // typical CHOP 0..100
}

double MarketRegimeDetector::percentile(std::vector<double>& tmp, double q) {
    if (tmp.empty()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    size_t k = (size_t)std::floor(q * (tmp.size() - 1));
    std::nth_element(tmp.begin(), tmp.begin() + k, tmp.end());
    return tmp[k];
}

void MarketRegimeDetector::update_vol_thresholds(double vol_sample) {
    // Keep rolling window
    vol_cal_.push_back(vol_sample);
    while ((int)vol_cal_.size() > p_.calibr_window) vol_cal_.pop_front();
    if ((int)vol_cal_.size() < std::min(500, p_.calibr_window/2)) return; // not enough

    std::vector<double> tmp(vol_cal_.begin(), vol_cal_.end());
    vol_lo_ = percentile(tmp, 0.30);
    vol_hi_ = percentile(tmp, 0.70);

    // Safety guard: keep some separation
    if (vol_hi_ - vol_lo_ < 5e-5) {
        vol_lo_ = std::max(0.0, vol_lo_ - 1e-4);
        vol_hi_ = vol_hi_ + 1e-4;
    }
}

MarketRegime MarketRegimeDetector::detect(const std::vector<Bar>& bars) {
    // 1) Extract features
    last_feat_.vol = std_log_returns(bars, p_.vol_window);
    slope_r2_log_price(bars, p_.slope_window, last_feat_.slope, last_feat_.r2);
    last_feat_.chop = chop_index(bars, p_.chop_window);

    // 2) Adapt volatility thresholds
    update_vol_thresholds(last_feat_.vol);

    // 3) Compute scores
    auto score_high_vol = (vol_hi_ > 0.0) ? (last_feat_.vol - vol_hi_) / std::max(1e-12, vol_hi_) : -1.0;
    auto score_low_vol  = (vol_lo_ > 0.0) ? (vol_lo_ - last_feat_.vol) / std::max(1e-12, vol_lo_) : -1.0;

    const bool trending = std::abs(last_feat_.slope) >= p_.trend_slope_min && last_feat_.r2 >= p_.trend_r2_min;
    const double trend_mag = (std::abs(last_feat_.slope) / std::max(1e-12, p_.trend_slope_min)) * last_feat_.r2;

    // 4) Precedence: HIGH_VOL -> LOW_VOL -> TREND -> CHOP
    struct Candidate { MarketRegime r; double score; };
    std::vector<Candidate> cands;
    cands.push_back({ MarketRegime::HIGH_VOLATILITY, score_high_vol });
    cands.push_back({ MarketRegime::LOW_VOLATILITY,  score_low_vol  });
    if (trending) {
        cands.push_back({ last_feat_.slope > 0.0 ? MarketRegime::TRENDING_UP : MarketRegime::TRENDING_DOWN, trend_mag });
    } else {
        // Use inverse CHOP as weak score for choppy (higher chop -> more choppy, but we want positive score)
        const double chop_score = std::max(0.0, (last_feat_.chop - 50.0) / 50.0);
        cands.push_back({ MarketRegime::CHOPPY, chop_score });
    }

    // Pick best candidate
    auto best = std::max_element(cands.begin(), cands.end(), [](const auto& a, const auto& b){ return a.score < b.score; });
    MarketRegime proposed = best->r;
    double proposed_score = best->score;

    // 5) Hysteresis + cooldown
    if (cooldown_ > 0) --cooldown_;
    if (last_regime_.has_value()) {
        if (proposed != *last_regime_) {
            if (proposed_score < p_.hysteresis_margin && cooldown_ > 0) {
                return *last_regime_; // hold
            }
            // switch only if the new regime is clearly supported
            if (proposed_score < p_.hysteresis_margin) {
                return *last_regime_;
            }
        }
    }

    if (!last_regime_.has_value() || proposed != *last_regime_) {
        last_regime_ = proposed;
        cooldown_ = p_.cooldown_bars;
    }
    return *last_regime_;
}

std::string MarketRegimeDetector::regime_to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::TRENDING_UP: return "TRENDING_UP";
        case MarketRegime::TRENDING_DOWN: return "TRENDING_DOWN";
        case MarketRegime::CHOPPY: return "CHOPPY";
        case MarketRegime::HIGH_VOLATILITY: return "HIGH_VOLATILITY";
        case MarketRegime::LOW_VOLATILITY: return "LOW_VOLATILITY";
        default: return "UNKNOWN";
    }
}

} // namespace sentio
