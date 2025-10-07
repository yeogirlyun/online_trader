#ifndef SENTIO_POLYGON_CLIENT_HPP
#define SENTIO_POLYGON_CLIENT_HPP

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <deque>
#include <mutex>

namespace sentio {

/**
 * Polygon.io WebSocket Client for Real-Time Market Data
 *
 * Connects to Polygon proxy server and receives 1-minute aggregated bars
 * for SPY, SDS, SPXL, and SH in real-time.
 */
class PolygonClient {
public:
    using BarCallback = std::function<void(const std::string& symbol, const Bar& bar)>;

    /**
     * Constructor
     * @param proxy_url WebSocket URL for Polygon proxy (e.g., "ws://proxy.example.com:8080")
     * @param auth_key Authentication key for proxy
     */
    PolygonClient(const std::string& proxy_url, const std::string& auth_key);
    ~PolygonClient();

    /**
     * Connect to Polygon proxy and authenticate
     */
    bool connect();

    /**
     * Subscribe to symbols for 1-minute aggregates
     */
    bool subscribe(const std::vector<std::string>& symbols);

    /**
     * Start receiving data (runs in separate thread)
     * @param callback Function called when new bar arrives
     */
    void start(BarCallback callback);

    /**
     * Stop receiving data and disconnect
     */
    void stop();

    /**
     * Get recent bars for a symbol (last N bars in memory)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const;

    /**
     * Check if connected
     */
    bool is_connected() const;

    /**
     * Store a bar in history (public for WebSocket callback access)
     */
    void store_bar(const std::string& symbol, const Bar& bar);

private:
    std::string proxy_url_;
    std::string auth_key_;
    bool connected_;
    bool running_;

    // Thread-safe storage of recent bars (per symbol)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // WebSocket implementation
    void receive_loop(BarCallback callback);
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_HPP
