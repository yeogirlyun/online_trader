#ifndef SENTIO_ALPACA_REST_BAR_FEED_H
#define SENTIO_ALPACA_REST_BAR_FEED_H

#include "live/bar_feed_interface.h"
#include "live/alpaca_client.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>

namespace sentio {

/**
 * Alpaca REST Bar Feed
 *
 * Polls Alpaca REST API for latest bars instead of using WebSocket.
 * Simpler and more reliable than FIFO-based WebSocket bridge.
 *
 * Usage:
 *   auto feed = std::make_unique<AlpacaRestBarFeed>(api_key, secret_key);
 *   feed->connect();
 *   feed->subscribe({"SPY"});
 *   feed->start([](const std::string& symbol, const Bar& bar) {
 *       // Process bar
 *   });
 */
class AlpacaRestBarFeed : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param api_key Alpaca API key
     * @param secret_key Alpaca secret key
     * @param paper_trading Use paper trading endpoint (default: true)
     * @param poll_interval_ms Poll interval in milliseconds (default: 60000 = 1 minute)
     */
    AlpacaRestBarFeed(const std::string& api_key,
                      const std::string& secret_key,
                      bool paper_trading = true,
                      int poll_interval_ms = 60000);

    ~AlpacaRestBarFeed() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

private:
    std::unique_ptr<AlpacaClient> client_;
    std::vector<std::string> subscribed_symbols_;
    int poll_interval_ms_;

    // Threading
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::thread poll_thread_;
    BarCallback callback_;

    // Recent bars cache (for get_recent_bars)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::vector<Bar>> recent_bars_;
    static constexpr size_t MAX_CACHED_BARS = 1000;

    // Health tracking
    std::atomic<std::chrono::steady_clock::time_point> last_message_time_;
    std::atomic<int64_t> last_bar_timestamp_ms_;

    // Polling loop
    void poll_loop();

    // Convert AlpacaClient::BarData to Bar
    Bar convert_bar(const AlpacaClient::BarData& alpaca_bar);

    // Add bar to cache
    void cache_bar(const std::string& symbol, const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_ALPACA_REST_BAR_FEED_H
