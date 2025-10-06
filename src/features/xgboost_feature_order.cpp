#include "features/xgboost_feature_order.h"
#include <unordered_set>

namespace sentio {
namespace features {

/**
 * CRITICAL: This is the EXACT feature order used by IntradayFeatureExtractor (25 features)
 * DO NOT MODIFY without retraining the model and updating the checksum.
 */
const std::vector<std::string> XGBoostFeatureOrder::FEATURE_ORDER = {
    // Microstructure (6)
    "tick_direction",
    "spread_ratio",
    "wicks_ratio",
    "volume_imbalance",
    "trade_intensity",
    "price_acceleration",
    // Short-term momentum (5)
    "micro_momentum_1",
    "micro_momentum_3",
    "rsi_3",
    "ema_cross_fast",
    "velocity",
    // Multi-timeframe (5)
    "mtf_trend_5m",
    "mtf_trend_15m",
    "mtf_volume_5m",
    "mtf_volatility_ratio",
    "session_range_position",
    // Session patterns (4)
    "vwap_distance",
    "opening_range_breakout",
    "time_of_day_sin",
    "time_of_day_cos",
    // Volume profile (5)
    "volume_percentile",
    "delta_cumulative",
    "vwap_stdev_bands",
    "relative_volume",
    "volume_price_trend"
};

// Runtime guard (compile-time is not possible due to non-constexpr std::vector::size())
namespace {
struct FeatureSchemaGuard {
    FeatureSchemaGuard() {
        const auto& order = XGBoostFeatureOrder::FEATURE_ORDER;
        if (order.size() != XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT) {
            throw std::runtime_error(
                "CRITICAL: XGBoost FEATURE_ORDER must contain exactly " +
                std::to_string(XGBoostFeatureOrder::EXPECTED_FEATURE_COUNT) +
                " features, got " +
                std::to_string(order.size()));
        }
        // Validate no empty names and no duplicates
        std::unordered_set<std::string> seen;
        seen.reserve(order.size());
        for (const auto& name : order) {
            if (name.empty()) {
                throw std::runtime_error("CRITICAL: Empty feature name in FEATURE_ORDER");
            }
            if (!seen.insert(name).second) {
                throw std::runtime_error("CRITICAL: Duplicate feature name in FEATURE_ORDER: " + name);
            }
            // Character whitelist: lowercase letters, digits, underscore
            for (unsigned char c : name) {
                if (!(std::islower(c) || std::isdigit(c) || c == '_')) {
                    throw std::runtime_error("CRITICAL: Invalid character in feature name: " + name);
                }
            }
            // Ensure no leading/trailing underscores and no double underscores
            if (name.front() == '_' || name.back() == '_' || name.find("__") != std::string::npos) {
                throw std::runtime_error("CRITICAL: Malformed feature name: " + name);
            }
        }
        // Note: runtime checksum comparison is enforced during model initialize(),
        // using the training-side checksum file next to the model. This static
        // validator ensures local integrity (count/dupes), while runtime ensures
        // training-inference consistency.
    }
};
static FeatureSchemaGuard g_feature_schema_guard;
}

} // namespace features
} // namespace sentio
