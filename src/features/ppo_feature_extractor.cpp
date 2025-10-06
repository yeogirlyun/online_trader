#include "features/ppo_feature_extractor.h"
#include <stdexcept>
#include <cmath>

namespace sentio {
namespace features {

PPOFeatureExtractor::PPOFeatureExtractor() : config_{} {}
PPOFeatureExtractor::PPOFeatureExtractor(const FeatureConfig& config) : config_(config) {}

torch::Tensor PPOFeatureExtractor::extract_features(const ::sentio::Bar& current_bar,
                                                    const std::deque<::sentio::Bar>& history) {
    if (history.size() < HISTORY_WINDOW) {
        throw std::runtime_error("PPOFeatureExtractor: insufficient history");
    }

    std::vector<float> f(FEATURE_DIM, 0.0f);

    const auto lastN = [&](size_t n) {
        std::deque<::sentio::Bar> out;
        out.insert(out.end(), history.begin(), history.begin() + static_cast<long>(std::min(n, history.size())));
        return out;
    };

    // Basic stats
    const double sma5 = sma(history, config_.sma_fast);
    const double sma20 = sma(history, config_.sma_slow);
    const double stdev5 = stddev(history, 5);
    const double stdev20 = stddev(history, 20);
    const double atr14 = atr(history, config_.atr_period);

    // Populate deterministic 16 features (lightweight, aligned with prior inline set)
    f[0]  = static_cast<float>(current_bar.close);
    f[1]  = static_cast<float>(current_bar.volume);
    f[2]  = static_cast<float>(current_bar.high - current_bar.low);
    f[3]  = static_cast<float>(current_bar.close - current_bar.open);
    f[4]  = static_cast<float>(sma5 == 0.0 ? 0.0 : current_bar.close / sma5);
    f[5]  = static_cast<float>(sma20 == 0.0 ? 0.0 : current_bar.close / sma20);
    f[6]  = static_cast<float>(history.size() >= 5 ? current_bar.close - history[4].close : 0.0);
    f[7]  = static_cast<float>(history.size() >= 20 ? current_bar.close - history[19].close : 0.0);
    f[8]  = static_cast<float>(5 == 0 ? 0.0 : (sma(history, 0) == 0.0 ? 0.0 : history.front().volume / std::max(1.0, sma(history, 5))));
    f[9]  = static_cast<float>(stdev5);
    f[10] = static_cast<float>(stdev20);
    f[11] = static_cast<float>(current_bar.high - current_bar.close);
    f[12] = static_cast<float>(current_bar.close - current_bar.low);
    const double day_ms = 24.0 * 60.0 * 60.0 * 1000.0;
    f[13] = static_cast<float>(std::fmod(static_cast<double>(current_bar.timestamp_ms), day_ms) / day_ms);
    // Approx range measures
    {
        double s = 0.0; size_t n = std::min<size_t>(5, history.size());
        for (size_t i = 0; i < n; ++i) s += (history[i].high - history[i].low);
        f[14] = static_cast<float>((n ? s / n : 0.0));
    }
    {
        double s = 0.0; size_t n = std::min<size_t>(20, history.size());
        for (size_t i = 0; i < n; ++i) s += (history[i].high - history[i].low);
        f[15] = static_cast<float>((n ? s / n : 0.0));
    }

    auto t = torch::from_blob(f.data(), {1, static_cast<long>(FEATURE_DIM)}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
    if (config_.normalize_features) {
        auto mean = t.mean();
        auto sd = t.std();
        if (sd.item<float>() == 0.0f) throw std::runtime_error("PPOFeatureExtractor: zero std after feature build");
        t = (t - mean) / sd;
    }
    if (torch::any(torch::isnan(t)).item<bool>() || torch::any(torch::isinf(t)).item<bool>()) {
        throw std::runtime_error("PPOFeatureExtractor: NaN/Inf detected");
    }
    return t;
}

double PPOFeatureExtractor::sma(const std::deque<::sentio::Bar>& h, size_t n) {
    if (h.empty() || n == 0) return 0.0;
    n = std::min(n, h.size());
    double s = 0.0; for (size_t i = 0; i < n; ++i) s += h[i].close;
    return s / n;
}

double PPOFeatureExtractor::stddev(const std::deque<::sentio::Bar>& h, size_t n) {
    if (h.empty() || n == 0) return 0.0;
    n = std::min(n, h.size());
    double m = sma(h, n); double acc = 0.0; for (size_t i = 0; i < n; ++i) { double d = h[i].close - m; acc += d * d; }
    return std::sqrt(acc / n);
}

double PPOFeatureExtractor::atr(const std::deque<::sentio::Bar>& h, size_t n) {
    if (h.size() < n + 1) return 0.0;
    double s = 0.0; for (size_t i = 0; i < n; ++i) {
        const auto& b = h[i]; const auto& prev = h[i + 1];
        const double tr = std::max({b.high - b.low, std::abs(b.high - prev.close), std::abs(b.low - prev.close)});
        s += tr;
    }
    return s / n;
}

} // namespace features
} // namespace sentio


