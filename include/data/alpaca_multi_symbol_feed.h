#pragma once

#include "data/bar_feed_interface.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <libwebsockets.h>

namespace sentio {
namespace data {

/**
 * @brief Live multi-symbol data feed using Alpaca WebSocket API
 *
 * Connects to Alpaca's real-time data stream via WebSocket and receives
 * 1-minute aggregated bars for multiple symbols simultaneously.
 *
 * WebSocket URL: wss://stream.data.alpaca.markets/v2/iex
 * Authentication: API key/secret in initial message
 * Message format: JSON with bar data
 *
 * Usage:
 *   AlpacaMultiSymbolFeed feed(data_manager, {
 *       .symbols = {"TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"},
 *       .api_key = "PK...",
 *       .secret_key = "..."
 *   });
 *   feed.connect();
 *   feed.start();
 */
class AlpacaMultiSymbolFeed : public IBarFeed {
public:
    struct Config {
        std::vector<std::string> symbols;   // Symbols to subscribe
        std::string api_key;                // Alpaca API key
        std::string secret_key;             // Alpaca secret key
        std::string websocket_url = "wss://stream.data.alpaca.markets/v2/iex";
        bool auto_reconnect = true;         // Auto-reconnect on disconnect
        int reconnect_delay_ms = 5000;      // Delay before reconnect
        int max_reconnects = 10;            // Max reconnect attempts (-1 = infinite)
    };

    /**
     * @brief Construct Alpaca feed
     *
     * @param data_manager Data manager to feed
     * @param config Configuration
     */
    AlpacaMultiSymbolFeed(std::shared_ptr<MultiSymbolDataManager> data_manager,
                         const Config& config);

    ~AlpacaMultiSymbolFeed() override;

    // === IBarFeed Interface ===

    /**
     * @brief Connect to Alpaca WebSocket
     * @return true if connection successful
     */
    bool connect() override;

    /**
     * @brief Disconnect from WebSocket
     */
    void disconnect() override;

    /**
     * @brief Check if connected
     */
    bool is_connected() const override;

    /**
     * @brief Start feeding data (subscribe to symbols)
     * @return true if subscription successful
     */
    bool start() override;

    /**
     * @brief Stop feeding data
     */
    void stop() override;

    /**
     * @brief Check if feed is active
     */
    bool is_active() const override;

    /**
     * @brief Get feed type identifier
     */
    std::string get_type() const override;

    /**
     * @brief Get symbols being fed
     */
    std::vector<std::string> get_symbols() const override;

    /**
     * @brief Set callback for bar updates
     */
    void set_bar_callback(BarCallback callback) override;

    /**
     * @brief Set callback for errors
     */
    void set_error_callback(ErrorCallback callback) override;

    /**
     * @brief Set callback for connection state changes
     */
    void set_connection_callback(ConnectionCallback callback) override;

    /**
     * @brief Get feed statistics
     */
    FeedStats get_stats() const override;

private:
    /**
     * @brief WebSocket service thread
     */
    void service_loop();

    /**
     * @brief Send authentication message
     */
    bool send_auth();

    /**
     * @brief Subscribe to symbols
     */
    bool subscribe_symbols();

    /**
     * @brief Parse incoming WebSocket message
     */
    void parse_message(const std::string& message);

    /**
     * @brief Parse bar data from JSON
     */
    bool parse_bar(const std::string& json, std::string& symbol, Bar& bar);

    /**
     * @brief Reconnect to WebSocket
     */
    void reconnect();

    /**
     * @brief libwebsockets callback
     */
    static int websocket_callback(
        struct lws *wsi,
        enum lws_callback_reasons reason,
        void *user,
        void *in,
        size_t len
    );

    std::shared_ptr<MultiSymbolDataManager> data_manager_;
    Config config_;

    // WebSocket state
    struct lws_context *ws_context_{nullptr};
    struct lws *wsi_{nullptr};
    std::atomic<bool> connected_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> authenticated_{false};

    // Service thread
    std::thread service_thread_;

    // Callbacks
    BarCallback bar_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;

    // Statistics
    std::atomic<int> total_bars_{0};
    std::map<std::string, int> bars_per_symbol_;
    std::atomic<int> errors_{0};
    std::atomic<int> reconnects_{0};
    mutable std::mutex stats_mutex_;

    // Reconnection state
    int reconnect_count_{0};
    uint64_t last_reconnect_time_ms_{0};

    // Message buffer
    std::string message_buffer_;
    std::mutex message_mutex_;

    // libwebsockets protocols
    static struct lws_protocols protocols_[];
};

} // namespace data
} // namespace sentio
