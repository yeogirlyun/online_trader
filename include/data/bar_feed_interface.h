#pragma once

#include "common/types.h"
#include <map>
#include <string>
#include <functional>

namespace sentio {
namespace data {

/**
 * @brief Abstract interface for multi-symbol bar feeds
 *
 * This interface allows live and mock trading to use the same code path.
 * Only the data source changes - everything else (strategy, rotation, risk) is identical.
 *
 * Implementations:
 * - AlpacaMultiSymbolFeed: Real-time WebSocket data
 * - MockMultiSymbolFeed: Historical CSV replay
 * - PolygonMultiSymbolFeed: REST API polling (fallback)
 */
class IBarFeed {
public:
    virtual ~IBarFeed() = default;

    /**
     * @brief Connect to data source
     *
     * @return true if connection successful
     */
    virtual bool connect() = 0;

    /**
     * @brief Disconnect from data source
     */
    virtual void disconnect() = 0;

    /**
     * @brief Check if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * @brief Start feeding data
     *
     * For live feeds: Start WebSocket subscription
     * For mock feeds: Start CSV replay
     *
     * @return true if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop feeding data
     */
    virtual void stop() = 0;

    /**
     * @brief Check if feed is active
     */
    virtual bool is_active() const = 0;

    /**
     * @brief Get feed type identifier
     */
    virtual std::string get_type() const = 0;

    /**
     * @brief Get symbols being fed
     */
    virtual std::vector<std::string> get_symbols() const = 0;

    /**
     * @brief Set callback for bar updates
     *
     * Called whenever new bars arrive (any symbol)
     *
     * @param callback Function(symbol, bar)
     */
    using BarCallback = std::function<void(const std::string&, const Bar&)>;
    virtual void set_bar_callback(BarCallback callback) = 0;

    /**
     * @brief Set callback for errors
     *
     * @param callback Function(error_message)
     */
    using ErrorCallback = std::function<void(const std::string&)>;
    virtual void set_error_callback(ErrorCallback callback) = 0;

    /**
     * @brief Set callback for connection state changes
     *
     * @param callback Function(connected)
     */
    using ConnectionCallback = std::function<void(bool)>;
    virtual void set_connection_callback(ConnectionCallback callback) = 0;

    /**
     * @brief Get feed statistics
     */
    struct FeedStats {
        int total_bars_received;
        int bars_per_symbol[10];  // Per-symbol counts
        int errors;
        int reconnects;
        double avg_latency_ms;
    };

    virtual FeedStats get_stats() const = 0;
};

} // namespace data
} // namespace sentio
