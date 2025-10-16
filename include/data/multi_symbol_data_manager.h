#pragma once

#include "common/types.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace sentio {
namespace data {

/**
 * @brief Snapshot of a symbol's latest data with staleness tracking
 *
 * Each symbol maintains its own update timeline. Staleness weighting
 * reduces influence of old data in signal ranking.
 */
struct SymbolSnapshot {
    Bar latest_bar;                     // Most recent bar
    uint64_t last_update_ms;            // Timestamp of last update
    double staleness_seconds;           // Age of data (seconds)
    double staleness_weight;            // Exponential decay weight: exp(-age/60)
    int forward_fill_count;             // How many times forward-filled
    bool is_valid;                      // Data is usable

    SymbolSnapshot()
        : last_update_ms(0)
        , staleness_seconds(0.0)
        , staleness_weight(1.0)
        , forward_fill_count(0)
        , is_valid(false) {}

    // Calculate staleness based on current time
    void update_staleness(uint64_t current_time_ms) {
        staleness_seconds = (current_time_ms - last_update_ms) / 1000.0;

        // Exponential decay: fresh = 1.0, 60s = 0.37, 120s = 0.14
        staleness_weight = std::exp(-staleness_seconds / 60.0);

        is_valid = (staleness_seconds < 300.0);  // Invalid after 5 minutes
    }
};

/**
 * @brief Synchronized snapshot of all symbols at a logical timestamp
 *
 * Not all symbols may be present (async data arrival). Missing symbols
 * are forward-filled from last known data.
 */
struct MultiSymbolSnapshot {
    uint64_t logical_timestamp_ms;                     // Logical time (aligned)
    std::map<std::string, SymbolSnapshot> snapshots;   // Symbol → data
    std::vector<std::string> missing_symbols;          // Symbols not updated
    int total_forward_fills;                           // Total symbols forward-filled
    bool is_complete;                                  // All symbols present
    double avg_staleness_seconds;                      // Average age across symbols

    MultiSymbolSnapshot()
        : logical_timestamp_ms(0)
        , total_forward_fills(0)
        , is_complete(false)
        , avg_staleness_seconds(0.0) {}

    // Check if snapshot is usable
    bool is_usable() const {
        // Must have at least 50% of symbols with fresh data
        int valid_count = 0;
        for (const auto& [symbol, snap] : snapshots) {
            if (snap.is_valid && snap.staleness_seconds < 120.0) {
                valid_count++;
            }
        }
        return valid_count >= (snapshots.size() / 2);
    }
};

/**
 * @brief Asynchronous multi-symbol data manager
 *
 * Core design principles:
 * 1. NON-BLOCKING: Never wait for all symbols, use latest available
 * 2. STALENESS WEIGHTING: Reduce influence of old data
 * 3. FORWARD FILL: Use last known price for missing data (max 5 fills)
 * 4. THREAD-SAFE: WebSocket updates from background thread
 *
 * Usage:
 *   auto snapshot = data_mgr->get_latest_snapshot();
 *   for (const auto& [symbol, data] : snapshot.snapshots) {
 *       double adjusted_strength = base_strength * data.staleness_weight;
 *   }
 */
class MultiSymbolDataManager {
public:
    struct Config {
        std::vector<std::string> symbols;          // Active symbols to track
        int max_forward_fills = 5;                 // Max consecutive forward fills
        double max_staleness_seconds = 300.0;      // Max age before invalid (5 min)
        int history_size = 500;                    // Bars to keep per symbol
        bool log_data_quality = true;              // Log missing/stale data
        bool backtest_mode = false;                // Disable timestamp validation for backtesting
    };

    explicit MultiSymbolDataManager(const Config& config);
    virtual ~MultiSymbolDataManager() = default;

    // === Main Interface ===

    /**
     * @brief Get latest snapshot for all symbols (non-blocking)
     *
     * Returns immediately with whatever data is available. Applies
     * staleness weighting and forward-fill for missing symbols.
     *
     * @return MultiSymbolSnapshot with latest data
     */
    MultiSymbolSnapshot get_latest_snapshot();

    /**
     * @brief Update a symbol's data (called from WebSocket/feed thread)
     *
     * Thread-safe update of symbol data. Validates and stores bar.
     *
     * @param symbol Symbol ticker
     * @param bar New bar data
     * @return true if update successful, false if validation failed
     */
    bool update_symbol(const std::string& symbol, const Bar& bar);

    /**
     * @brief Bulk update multiple symbols (for mock replay)
     *
     * @param bars Map of symbol → bar
     * @return Number of successful updates
     */
    int update_all(const std::map<std::string, Bar>& bars);

    // === History Access ===

    /**
     * @brief Get recent bars for a symbol (for volatility calculation)
     *
     * @param symbol Symbol ticker
     * @param count Number of bars to retrieve
     * @return Vector of recent bars (newest first)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, int count) const;

    /**
     * @brief Get all history for a symbol
     *
     * @param symbol Symbol ticker
     * @return Deque of all bars (oldest first)
     */
    std::deque<Bar> get_all_bars(const std::string& symbol) const;

    // === Statistics & Monitoring ===

    /**
     * @brief Get data quality stats
     */
    struct DataQualityStats {
        std::map<std::string, int> update_counts;          // Symbol → updates
        std::map<std::string, double> avg_staleness;       // Symbol → avg age
        std::map<std::string, int> forward_fill_counts;    // Symbol → fills
        int total_updates;
        int total_forward_fills;
        int total_rejections;
        double overall_avg_staleness;
    };

    DataQualityStats get_quality_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get configured symbols
     */
    const std::vector<std::string>& get_symbols() const { return config_.symbols; }

protected:
    /**
     * @brief Validate incoming bar data
     *
     * @param symbol Symbol ticker
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    bool validate_bar(const std::string& symbol, const Bar& bar);

    /**
     * @brief Forward-fill missing symbol from last known bar
     *
     * @param symbol Symbol ticker
     * @param logical_time Timestamp to use for forward-filled bar
     * @return SymbolSnapshot with forward-filled data
     */
    SymbolSnapshot forward_fill_symbol(const std::string& symbol,
                                       uint64_t logical_time);

private:
    struct SymbolState {
        std::deque<Bar> history;           // Bar history
        Bar latest_bar;                    // Most recent bar
        uint64_t last_update_ms;           // Last update timestamp
        int update_count;                  // Total updates received
        int forward_fill_count;            // Consecutive forward fills
        int rejection_count;               // Rejected updates
        double cumulative_staleness;       // For avg calculation

        SymbolState()
            : last_update_ms(0)
            , update_count(0)
            , forward_fill_count(0)
            , rejection_count(0)
            , cumulative_staleness(0.0) {}
    };

    Config config_;
    std::map<std::string, SymbolState> symbol_states_;
    mutable std::mutex data_mutex_;

    // Statistics
    std::atomic<int> total_updates_{0};
    std::atomic<int> total_forward_fills_{0};
    std::atomic<int> total_rejections_{0};

    // Current time (for testing - can be injected)
    std::function<uint64_t()> time_provider_;

    uint64_t get_current_time_ms() const {
        if (time_provider_) {
            return time_provider_();
        }
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

public:
    // For testing: inject time provider
    void set_time_provider(std::function<uint64_t()> provider) {
        time_provider_ = provider;
    }

private:
    // Helper functions
    std::string join_symbols() const;
    std::string join_vector(const std::vector<std::string>& vec) const;
};

} // namespace data
} // namespace sentio
