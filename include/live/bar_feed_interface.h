#ifndef SENTIO_BAR_FEED_INTERFACE_H
#define SENTIO_BAR_FEED_INTERFACE_H

#include "common/types.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace sentio {

/**
 * Bar Feed Interface
 *
 * Polymorphic interface for market data feeds.
 * Allows substitution of PolygonClient with MockBarFeedReplay
 * without modifying LiveTradeCommand logic.
 */
class IBarFeed {
public:
    virtual ~IBarFeed() = default;

    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Connect to data feed
     */
    virtual bool connect() = 0;

    /**
     * Subscribe to symbols
     */
    virtual bool subscribe(const std::vector<std::string>& symbols) = 0;

    /**
     * Start receiving data (runs callback for each bar)
     */
    virtual void start(BarCallback callback) = 0;

    /**
     * Stop receiving data and disconnect
     */
    virtual void stop() = 0;

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    virtual std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const = 0;

    /**
     * Check if connected
     */
    virtual bool is_connected() const = 0;

    /**
     * Check if connection is healthy (received message recently)
     */
    virtual bool is_connection_healthy() const = 0;

    /**
     * Get seconds since last message
     */
    virtual int get_seconds_since_last_message() const = 0;
};

} // namespace sentio

#endif // SENTIO_BAR_FEED_INTERFACE_H
