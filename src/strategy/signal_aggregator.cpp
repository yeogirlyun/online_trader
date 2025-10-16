#include "strategy/signal_aggregator.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>

namespace sentio {

SignalAggregator::SignalAggregator(const Config& config)
    : config_(config) {

    utils::log_info("SignalAggregator initialized");
    utils::log_info("  Leverage boosts: " + std::to_string(config_.leverage_boosts.size()) + " symbols");
    utils::log_info("  Min probability: " + std::to_string(config_.min_probability));
    utils::log_info("  Min confidence: " + std::to_string(config_.min_confidence));
    utils::log_info("  Min strength: " + std::to_string(config_.min_strength));
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::rank_signals(
    const std::map<std::string, SignalOutput>& signals,
    const std::map<std::string, double>& staleness_weights
) {
    std::vector<RankedSignal> ranked;

    // Track bars for cold-start warmup
    bars_processed_++;

    utils::log_debug("[SignalAggregator BAR " + std::to_string(bars_processed_) +
                   "] Processing " + std::to_string(signals.size()) + " signals");

    stats_.total_signals_processed += signals.size();

    for (const auto& [symbol, signal] : signals) {
        // Apply filters
        if (!passes_filters(signal)) {
            utils::log_debug("[SignalAggregator BAR " + std::to_string(bars_processed_) +
                           "] " + symbol + " FILTERED by passes_filters (type=" +
                           std::to_string(static_cast<int>(signal.signal_type)) +
                           ", prob=" + std::to_string(signal.probability) +
                           ", conf=" + std::to_string(signal.confidence) + ")");
            stats_.signals_filtered++;
            continue;
        }

        // Get leverage boost
        double leverage_boost = get_leverage_boost(symbol);

        // Get staleness weight (default to 1.0 if not provided)
        double staleness_weight = 1.0;
        if (staleness_weights.count(symbol) > 0) {
            staleness_weight = staleness_weights.at(symbol);
        }

        // Calculate strength (with EMA smoothing)
        double strength = calculate_strength(symbol, signal, leverage_boost, staleness_weight);

        // Filter by minimum strength
        if (strength < config_.min_strength) {
            utils::log_debug("[SignalAggregator BAR " + std::to_string(bars_processed_) +
                           "] " + symbol + " FILTERED by min_strength (strength=" +
                           std::to_string(strength) + " < " + std::to_string(config_.min_strength) + ")");
            stats_.signals_filtered++;
            continue;
        }

        utils::log_debug("[SignalAggregator BAR " + std::to_string(bars_processed_) +
                       "] " + symbol + " PASSED filters (strength=" + std::to_string(strength) + ")");

        // Create ranked signal
        RankedSignal ranked_signal;
        ranked_signal.symbol = symbol;
        ranked_signal.signal = signal;
        ranked_signal.leverage_boost = leverage_boost;
        ranked_signal.strength = strength;
        ranked_signal.staleness_weight = staleness_weight;
        ranked_signal.rank = 0;  // Will be set after sorting

        ranked.push_back(ranked_signal);

        // Update stats
        stats_.signals_per_symbol[symbol]++;
    }

    // Sort by strength (descending)
    std::sort(ranked.begin(), ranked.end());

    // Assign ranks
    for (size_t i = 0; i < ranked.size(); i++) {
        ranked[i].rank = static_cast<int>(i + 1);
    }

    // Update stats
    stats_.signals_ranked = static_cast<int>(ranked.size());
    if (!ranked.empty()) {
        double sum_strength = 0.0;
        for (const auto& rs : ranked) {
            sum_strength += rs.strength;
        }
        stats_.avg_strength = sum_strength / ranked.size();
        stats_.max_strength = ranked[0].strength;
    }

    utils::log_debug("[SignalAggregator BAR " + std::to_string(bars_processed_) +
                   "] Ranked " + std::to_string(ranked.size()) + " signals (out of " +
                   std::to_string(signals.size()) + " total)");

    return ranked;
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::get_top_n(
    const std::vector<RankedSignal>& ranked_signals,
    int n
) const {
    std::vector<RankedSignal> top_n;

    int count = std::min(n, static_cast<int>(ranked_signals.size()));
    for (int i = 0; i < count; i++) {
        top_n.push_back(ranked_signals[i]);
    }

    return top_n;
}

std::vector<SignalAggregator::RankedSignal> SignalAggregator::filter_by_direction(
    const std::vector<RankedSignal>& ranked_signals,
    SignalType direction
) const {
    std::vector<RankedSignal> filtered;

    for (const auto& rs : ranked_signals) {
        if (rs.signal.signal_type == direction) {
            filtered.push_back(rs);
        }
    }

    return filtered;
}

// === Private Methods ===

double SignalAggregator::calculate_strength(
    const std::string& symbol,
    const SignalOutput& signal,
    double leverage_boost,
    double staleness_weight
) {
    // Calculate raw strength
    double raw_strength = signal.probability * signal.confidence * leverage_boost * staleness_weight;

    // Apply exponential smoothing to reduce noise
    if (smoothed_strengths_.count(symbol) == 0) {
        smoothed_strengths_[symbol] = raw_strength;  // Initialize
    } else {
        // EMA: new_smooth = alpha * raw + (1-alpha) * old_smooth
        smoothed_strengths_[symbol] = smoothing_alpha_ * raw_strength +
                                      (1.0 - smoothing_alpha_) * smoothed_strengths_[symbol];
    }

    return smoothed_strengths_[symbol];
}

bool SignalAggregator::passes_filters(const SignalOutput& signal) const {
    // Filter NEUTRAL signals
    if (signal.signal_type == SignalType::NEUTRAL) {
        return false;
    }

    // NEW: Require minimum confidence to prevent zero-strength signals
    if (signal.confidence < 0.005) {  // Require at least 0.5% confidence
        static int low_conf_count = 0;
        if (low_conf_count < 10) {
            utils::log_warning("Signal filtered - confidence too low: " +
                             std::to_string(signal.confidence));
            low_conf_count++;
        }
        return false;
    }

    // During first 200 bars, use relaxed thresholds for cold-start
    double effective_min_confidence = config_.min_confidence;
    double effective_min_probability = config_.min_probability;

    if (bars_processed_ < 200) {
        // Use much lower thresholds during warmup to allow predictor to learn
        effective_min_confidence = 0.0000001;
        effective_min_probability = 0.45;
    }

    // Filter by minimum probability
    if (signal.probability < effective_min_probability) {
        return false;
    }

    // Filter by minimum confidence
    if (signal.confidence < effective_min_confidence) {
        return false;
    }

    // Check for NaN or invalid values
    if (std::isnan(signal.probability) || std::isnan(signal.confidence)) {
        utils::log_warning("Invalid signal: NaN probability or confidence");
        return false;
    }

    if (signal.probability < 0.0 || signal.probability > 1.0) {
        utils::log_warning("Invalid signal: probability out of range [0,1]");
        return false;
    }

    if (signal.confidence < 0.0 || signal.confidence > 1.0) {
        utils::log_warning("Invalid signal: confidence out of range [0,1]");
        return false;
    }

    return true;
}

double SignalAggregator::get_leverage_boost(const std::string& symbol) const {
    auto it = config_.leverage_boosts.find(symbol);
    if (it != config_.leverage_boosts.end()) {
        return it->second;
    }

    // Default: no boost (1.0)
    return 1.0;
}

} // namespace sentio
