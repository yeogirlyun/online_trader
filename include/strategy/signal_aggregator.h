#pragma once

#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <map>

namespace sentio {

/**
 * @brief Aggregates and ranks signals from multiple symbols
 *
 * Takes raw signals from 6 OES instances and:
 * 1. Applies leverage boost (1.5x for leveraged ETFs)
 * 2. Calculates signal strength: probability × confidence × leverage_boost
 * 3. Ranks signals by strength
 * 4. Filters by minimum strength threshold
 *
 * This is the CORE of the rotation strategy - the best signals win.
 *
 * Design Principle:
 * "Let the signals compete - highest strength gets capital"
 *
 * Usage:
 *   SignalAggregator aggregator(config);
 *   auto ranked = aggregator.rank_signals(all_signals);
 *   // Top N signals will be held by RotationPositionManager
 */
class SignalAggregator {
public:
    struct Config {
        // Leverage boost factors
        std::map<std::string, double> leverage_boosts = {
            {"TQQQ", 1.5},
            {"SQQQ", 1.5},
            {"UPRO", 1.5},
            {"SDS", 1.4},   // -2x, slightly less boost
            {"UVXY", 1.3},  // Volatility, more unpredictable
            {"SVIX", 1.3}
        };

        // Signal filtering
        double min_probability = 0.51;     // Minimum probability for consideration
        double min_confidence = 0.55;      // Minimum confidence for consideration
        double min_strength = 0.40;        // Minimum combined strength

        // Correlation filtering (future enhancement)
        bool enable_correlation_filter = false;
        double max_correlation = 0.85;     // Reject if correlation > 0.85

        // Signal quality thresholds
        bool filter_stale_signals = true;  // Filter signals from stale data
        double max_staleness_seconds = 120.0;  // Max 2 minutes old
    };

    /**
     * @brief Ranked signal with calculated strength
     */
    struct RankedSignal {
        std::string symbol;
        SignalOutput signal;
        double leverage_boost;      // Applied leverage boost factor
        double strength;            // probability × confidence × leverage_boost
        double staleness_weight;    // Staleness factor (1.0 = fresh, 0.0 = very old)
        int rank;                   // 1 = strongest, 2 = second, etc.

        // For sorting
        bool operator<(const RankedSignal& other) const {
            return strength > other.strength;  // Descending order
        }
    };

    explicit SignalAggregator(const Config& config);
    ~SignalAggregator() = default;

    /**
     * @brief Rank all signals by strength
     *
     * Applies leverage boost, calculates strength, filters weak signals,
     * and returns ranked list (strongest first).
     *
     * @param signals Map of symbol → signal
     * @param staleness_weights Optional staleness weights (from DataManager)
     * @return Vector of ranked signals (sorted by strength, descending)
     */
    std::vector<RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals,
        const std::map<std::string, double>& staleness_weights = {}
    );

    /**
     * @brief Get top N signals
     *
     * @param ranked_signals Ranked signals (from rank_signals)
     * @param n Number of top signals to return
     * @return Top N signals
     */
    std::vector<RankedSignal> get_top_n(
        const std::vector<RankedSignal>& ranked_signals,
        int n
    ) const;

    /**
     * @brief Filter signals by direction (LONG or SHORT only)
     *
     * @param ranked_signals Ranked signals
     * @param direction Direction to filter (LONG or SHORT)
     * @return Filtered signals
     */
    std::vector<RankedSignal> filter_by_direction(
        const std::vector<RankedSignal>& ranked_signals,
        SignalType direction
    ) const;

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config) { config_ = new_config; }

    /**
     * @brief Get configuration
     *
     * @return Current configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Get statistics
     */
    struct Stats {
        int total_signals_processed;
        int signals_filtered;
        int signals_ranked;
        std::map<std::string, int> signals_per_symbol;
        double avg_strength;
        double max_strength;
    };

    Stats get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats(); }

private:
    /**
     * @brief Calculate signal strength with EMA smoothing
     *
     * @param symbol Symbol ticker (for EMA tracking)
     * @param signal Signal output
     * @param leverage_boost Leverage boost factor
     * @param staleness_weight Staleness weight (1.0 = fresh)
     * @return Combined strength score (smoothed)
     */
    double calculate_strength(
        const std::string& symbol,
        const SignalOutput& signal,
        double leverage_boost,
        double staleness_weight
    );

    /**
     * @brief Check if signal passes filters
     *
     * @param signal Signal output
     * @return true if signal passes all filters
     */
    bool passes_filters(const SignalOutput& signal) const;

    /**
     * @brief Get leverage boost for symbol
     *
     * @param symbol Symbol ticker
     * @return Leverage boost factor (1.0 if not found)
     */
    double get_leverage_boost(const std::string& symbol) const;

    Config config_;
    Stats stats_;
    int bars_processed_ = 0;  // Track bars for cold-start warmup
    std::map<std::string, double> smoothed_strengths_;  // EMA of strengths (anti-churning)
    double smoothing_alpha_ = 0.3;  // EMA factor (0.3 = 30% new, 70% old)
};

} // namespace sentio
