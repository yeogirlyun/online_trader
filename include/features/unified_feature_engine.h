#pragma once

#include "common/types.h"
#include "features/indicators.h"
#include "features/scaler.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <optional>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace features {

// =============================================================================
// Configuration for Production-Grade Unified Feature Engine
// =============================================================================

struct EngineConfig {
    // Feature toggles
    bool time = true;         // Time-of-day features (8 features)
    bool patterns = true;     // Candlestick patterns (5 features)
    bool momentum = true;
    bool volatility = true;
    bool volume = true;
    bool statistics = true;

    // Indicator periods
    int rsi14 = 14;
    int rsi21 = 21;
    int atr14 = 14;
    int bb20 = 20;
    int bb_k = 2;
    int stoch14 = 14;
    int will14 = 14;
    int macd_fast = 12;
    int macd_slow = 26;
    int macd_sig = 9;
    int roc5 = 5;
    int roc10 = 10;
    int roc20 = 20;
    int cci20 = 20;
    int don20 = 20;
    int keltner_ema = 20;
    int keltner_atr = 10;
    double keltner_mult = 2.0;

    // Moving averages
    int sma10 = 10;
    int sma20 = 20;
    int sma50 = 50;
    int ema10 = 10;
    int ema20 = 20;
    int ema50 = 50;

    // Normalization
    bool normalize = true;
    bool robust = false;
};

// =============================================================================
// Feature Schema with Hash for Model Compatibility
// =============================================================================

struct Schema {
    std::vector<std::string> names;
    std::string sha1_hash;  // Hash of (names + config) for version control
};

// =============================================================================
// Production-Grade Unified Feature Engine
//
// Key Features:
// - Stable, deterministic feature ordering (std::map, not unordered_map)
// - O(1) incremental updates using Welford's algorithm and ring buffers
// - Schema hash for model compatibility checks
// - Complete public API: update(), features_view(), names(), schema()
// - Serialization/restoration for online learning
// - Zero duplicate calculations (shared statistics cache)
// =============================================================================

class UnifiedFeatureEngine {
public:
    explicit UnifiedFeatureEngine(EngineConfig cfg = {});

    // ==========================================================================
    // Core API
    // ==========================================================================

    /**
     * Idempotent update with new bar. Returns true if state advanced.
     */
    bool update(const Bar& b);

    /**
     * Get contiguous feature vector in stable order (ready for model input).
     * Values may contain NaN until warmup complete for each feature.
     */
    const std::vector<double>& features_view() const { return feats_; }

    /**
     * Get canonical feature names in fixed, deterministic order.
     */
    const std::vector<std::string>& names() const { return schema_.names; }

    /**
     * Get schema with hash for model compatibility checks.
     */
    const Schema& schema() const { return schema_; }

    /**
     * Count of bars remaining before all features are non-NaN.
     */
    int warmup_remaining() const;

    /**
     * Get list of indicator names that are not yet ready (for debugging).
     */
    std::vector<std::string> get_unready_indicators() const;

    /**
     * Reset engine to initial state.
     */
    void reset();

    /**
     * Serialize engine state for persistence (online learning resume).
     */
    std::string serialize() const;

    /**
     * Restore engine state from serialized blob.
     */
    void restore(const std::string& blob);

    /**
     * Check if engine has processed at least one bar.
     */
    bool is_seeded() const { return seeded_; }

    /**
     * Get number of bars processed.
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Get normalization scaler (for external persistence).
     */
    const Scaler& get_scaler() const { return scaler_; }

    /**
     * Set scaler from external source (for trained models).
     */
    void set_scaler(const Scaler& s) { scaler_ = s; }

    /**
     * Get realized volatility (standard deviation of returns).
     * @param lookback Number of bars to calculate over (default 20)
     * @return Realized volatility, or 0.0 if insufficient data
     */
    double get_realized_volatility(int lookback = 20) const;

    /**
     * Get annualized volatility (realized vol * sqrt(252 * 390 minutes/day)).
     * @return Annualized volatility percentage
     */
    double get_annualized_volatility() const;

private:
    void build_schema_();
    void recompute_vector_();
    std::string compute_schema_hash_(const std::string& concatenated_names);

    EngineConfig cfg_;
    Schema schema_;

    // ==========================================================================
    // Indicators (all O(1) incremental)
    // ==========================================================================

    ind::RSI rsi14_;
    ind::RSI rsi21_;
    ind::ATR atr14_;
    ind::Boll bb20_;
    ind::Stoch stoch14_;
    ind::WilliamsR will14_;
    ind::MACD macd_;
    ind::ROC roc5_, roc10_, roc20_;
    ind::CCI cci20_;
    ind::Donchian don20_;
    ind::Keltner keltner_;
    ind::OBV obv_;
    ind::VWAP vwap_;

    // Moving averages
    roll::EMA ema10_, ema20_, ema50_;
    roll::Ring<double> sma10_ring_, sma20_ring_, sma50_ring_;

    // ==========================================================================
    // State
    // ==========================================================================

    bool seeded_ = false;
    size_t bar_count_ = 0;
    uint64_t prevTimestamp_ = 0;  // For time features
    double prevClose_ = std::numeric_limits<double>::quiet_NaN();
    double prevOpen_ = std::numeric_limits<double>::quiet_NaN();
    double prevHigh_ = std::numeric_limits<double>::quiet_NaN();
    double prevLow_ = std::numeric_limits<double>::quiet_NaN();
    double prevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // For computing 1-bar return (current close vs previous close)
    double prevPrevClose_ = std::numeric_limits<double>::quiet_NaN();

    // Rolling returns buffer for volatility calculation (stores last 50 returns)
    std::deque<double> recent_returns_;
    static constexpr size_t MAX_RETURNS_HISTORY = 50;

    // Feature vector (stable order, contiguous for model input)
    std::vector<double> feats_;

    // Normalization
    Scaler scaler_;
    std::vector<std::vector<double>> normalization_buffer_;  // For fit()
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Compute SHA1 hash of string (for schema versioning).
 */
std::string sha1_hex(const std::string& s);

/**
 * Safe return calculation (handles NaN and division by zero).
 */
inline double safe_return(double current, double previous) {
    if (std::isnan(previous) || previous == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (current / previous) - 1.0;
}

} // namespace features
} // namespace sentio
