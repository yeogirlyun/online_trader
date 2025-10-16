#include "data/alpaca_multi_symbol_feed.h"
#include "common/utils.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>

using json = nlohmann::json;

namespace sentio {
namespace data {

// libwebsockets protocols definition
struct lws_protocols AlpacaMultiSymbolFeed::protocols_[] = {
    {
        "alpaca-data-stream",
        AlpacaMultiSymbolFeed::websocket_callback,
        0,
        4096,
    },
    { nullptr, nullptr, 0, 0 }  // Terminator
};

AlpacaMultiSymbolFeed::AlpacaMultiSymbolFeed(
    std::shared_ptr<MultiSymbolDataManager> data_manager,
    const Config& config
)
    : data_manager_(data_manager)
    , config_(config) {

    utils::log_info("AlpacaMultiSymbolFeed initialized for " +
                   std::to_string(config_.symbols.size()) + " symbols");
}

AlpacaMultiSymbolFeed::~AlpacaMultiSymbolFeed() {
    disconnect();
}

// === IBarFeed Interface Implementation ===

bool AlpacaMultiSymbolFeed::connect() {
    if (connected_) {
        utils::log_warning("AlpacaMultiSymbolFeed already connected");
        return true;
    }

    utils::log_info("Connecting to Alpaca WebSocket: " + config_.websocket_url);

    // Create libwebsockets context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols_;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = this;  // Pass this pointer to callback

    ws_context_ = lws_create_context(&info);
    if (!ws_context_) {
        utils::log_error("Failed to create libwebsockets context");
        if (error_callback_) {
            error_callback_("Failed to create WebSocket context");
        }
        return false;
    }

    // Parse WebSocket URL
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));

    std::string url = config_.websocket_url;
    bool use_ssl = (url.find("wss://") == 0);
    std::string host = url.substr(use_ssl ? 6 : 5);  // Remove wss:// or ws://

    size_t slash_pos = host.find('/');
    std::string path = "/";
    if (slash_pos != std::string::npos) {
        path = host.substr(slash_pos);
        host = host.substr(0, slash_pos);
    }

    ccinfo.context = ws_context_;
    ccinfo.address = host.c_str();
    ccinfo.port = use_ssl ? 443 : 80;
    ccinfo.path = path.c_str();
    ccinfo.host = host.c_str();
    ccinfo.origin = host.c_str();
    ccinfo.protocol = protocols_[0].name;
    ccinfo.userdata = this;

    if (use_ssl) {
        ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED |
                                LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    }

    wsi_ = lws_client_connect_via_info(&ccinfo);
    if (!wsi_) {
        utils::log_error("Failed to create WebSocket connection");
        lws_context_destroy(ws_context_);
        ws_context_ = nullptr;
        if (error_callback_) {
            error_callback_("Failed to connect to WebSocket");
        }
        return false;
    }

    connected_ = true;

    // Start service thread
    should_stop_ = false;
    service_thread_ = std::thread(&AlpacaMultiSymbolFeed::service_loop, this);

    utils::log_info("WebSocket connection established");

    if (connection_callback_) {
        connection_callback_(true);
    }

    return true;
}

void AlpacaMultiSymbolFeed::disconnect() {
    if (!connected_) {
        return;
    }

    utils::log_info("Disconnecting from Alpaca WebSocket...");

    stop();
    should_stop_ = true;

    if (service_thread_.joinable()) {
        service_thread_.join();
    }

    if (ws_context_) {
        lws_context_destroy(ws_context_);
        ws_context_ = nullptr;
        wsi_ = nullptr;
    }

    connected_ = false;
    authenticated_ = false;

    utils::log_info("Disconnected from Alpaca WebSocket");

    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool AlpacaMultiSymbolFeed::is_connected() const {
    return connected_;
}

bool AlpacaMultiSymbolFeed::start() {
    if (!connected_) {
        utils::log_error("Cannot start - not connected. Call connect() first.");
        return false;
    }

    if (active_) {
        utils::log_warning("AlpacaMultiSymbolFeed already active");
        return true;
    }

    // Wait for authentication (up to 10 seconds)
    int wait_count = 0;
    while (!authenticated_ && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (!authenticated_) {
        utils::log_error("Authentication failed or timed out");
        return false;
    }

    utils::log_info("Starting data feed for " + std::to_string(config_.symbols.size()) + " symbols...");

    if (!subscribe_symbols()) {
        utils::log_error("Failed to subscribe to symbols");
        return false;
    }

    active_ = true;
    utils::log_info("Data feed started");

    return true;
}

void AlpacaMultiSymbolFeed::stop() {
    if (!active_) {
        return;
    }

    utils::log_info("Stopping data feed...");
    active_ = false;
}

bool AlpacaMultiSymbolFeed::is_active() const {
    return active_;
}

std::string AlpacaMultiSymbolFeed::get_type() const {
    return "AlpacaMultiSymbolFeed";
}

std::vector<std::string> AlpacaMultiSymbolFeed::get_symbols() const {
    return config_.symbols;
}

void AlpacaMultiSymbolFeed::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void AlpacaMultiSymbolFeed::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void AlpacaMultiSymbolFeed::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = callback;
}

IBarFeed::FeedStats AlpacaMultiSymbolFeed::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    FeedStats stats;
    stats.total_bars_received = total_bars_.load();
    stats.errors = errors_.load();
    stats.reconnects = reconnects_.load();
    stats.avg_latency_ms = 0.0;  // TODO: Implement latency tracking

    // Per-symbol counts
    int i = 0;
    for (const auto& [symbol, count] : bars_per_symbol_) {
        if (i < 10) {
            stats.bars_per_symbol[i] = count;
            i++;
        }
    }

    return stats;
}

// === Private Methods ===

void AlpacaMultiSymbolFeed::service_loop() {
    utils::log_info("WebSocket service loop started");

    while (!should_stop_ && ws_context_) {
        lws_service(ws_context_, 50);  // 50ms timeout
    }

    utils::log_info("WebSocket service loop stopped");
}

bool AlpacaMultiSymbolFeed::send_auth() {
    utils::log_info("Sending authentication...");

    json auth_msg = {
        {"action", "auth"},
        {"key", config_.api_key},
        {"secret", config_.secret_key}
    };

    std::string msg = auth_msg.dump();

    unsigned char buf[LWS_PRE + 4096];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.size());

    int written = lws_write(wsi_, &buf[LWS_PRE], msg.size(), LWS_WRITE_TEXT);
    if (written < 0) {
        utils::log_error("Failed to send authentication message");
        return false;
    }

    return true;
}

bool AlpacaMultiSymbolFeed::subscribe_symbols() {
    utils::log_info("Subscribing to " + std::to_string(config_.symbols.size()) + " symbols...");

    json subscribe_msg = {
        {"action", "subscribe"},
        {"bars", config_.symbols}
    };

    std::string msg = subscribe_msg.dump();

    unsigned char buf[LWS_PRE + 4096];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.size());

    int written = lws_write(wsi_, &buf[LWS_PRE], msg.size(), LWS_WRITE_TEXT);
    if (written < 0) {
        utils::log_error("Failed to send subscription message");
        return false;
    }

    utils::log_info("Subscription request sent");
    return true;
}

void AlpacaMultiSymbolFeed::parse_message(const std::string& message) {
    try {
        json msg = json::parse(message);

        // Check message type
        if (msg.contains("T")) {
            std::string msg_type = msg["T"];

            if (msg_type == "success" && msg.contains("msg")) {
                std::string success_msg = msg["msg"];
                utils::log_info("Alpaca: " + success_msg);

                if (success_msg.find("authenticated") != std::string::npos) {
                    authenticated_ = true;
                    utils::log_info("Authentication successful");
                }
            }
            else if (msg_type == "error") {
                std::string error_msg = msg.contains("msg") ? msg["msg"].get<std::string>() : "Unknown error";
                utils::log_error("Alpaca error: " + error_msg);
                errors_++;
                if (error_callback_) {
                    error_callback_(error_msg);
                }
            }
            else if (msg_type == "b") {
                // Bar data
                std::string symbol;
                Bar bar;
                if (parse_bar(message, symbol, bar)) {
                    // Update data manager
                    if (data_manager_) {
                        data_manager_->update_symbol(symbol, bar);
                    }

                    // Call callback
                    if (bar_callback_) {
                        bar_callback_(symbol, bar);
                    }

                    // Update stats
                    total_bars_++;
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        bars_per_symbol_[symbol]++;
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        utils::log_error("Failed to parse WebSocket message: " + std::string(e.what()));
        errors_++;
    }
}

bool AlpacaMultiSymbolFeed::parse_bar(const std::string& json_str, std::string& symbol, Bar& bar) {
    try {
        json msg = json::parse(json_str);

        if (!msg.contains("S") || !msg.contains("o") || !msg.contains("h") ||
            !msg.contains("l") || !msg.contains("c") || !msg.contains("v") ||
            !msg.contains("t")) {
            return false;
        }

        symbol = msg["S"];
        bar.open = msg["o"];
        bar.high = msg["h"];
        bar.low = msg["l"];
        bar.close = msg["c"];
        bar.volume = msg["v"];

        // Parse timestamp (Alpaca sends RFC3339 format)
        std::string timestamp_str = msg["t"];
        // Convert to milliseconds since epoch
        // TODO: Proper RFC3339 parsing
        bar.timestamp_ms = 0;  // Placeholder

        // Set bar_id
        bar.bar_id = bar.timestamp_ms / 60000;

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

void AlpacaMultiSymbolFeed::reconnect() {
    if (reconnect_count_ >= config_.max_reconnects && config_.max_reconnects != -1) {
        utils::log_error("Max reconnect attempts reached");
        return;
    }

    utils::log_info("Reconnecting in " + std::to_string(config_.reconnect_delay_ms) + "ms...");

    std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));

    disconnect();

    if (connect()) {
        reconnect_count_++;
        reconnects_++;
        utils::log_info("Reconnected successfully (attempt " + std::to_string(reconnect_count_) + ")");
    } else {
        utils::log_error("Reconnection failed");
    }
}

int AlpacaMultiSymbolFeed::websocket_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
) {
    AlpacaMultiSymbolFeed* feed = static_cast<AlpacaMultiSymbolFeed*>(
        lws_context_user(lws_get_context(wsi))
    );

    if (!feed) {
        return 0;
    }

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            utils::log_info("WebSocket connection established");
            feed->send_auth();
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            {
                std::string message((char*)in, len);
                feed->parse_message(message);
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            utils::log_warning("WebSocket connection closed");
            if (feed->config_.auto_reconnect && !feed->should_stop_) {
                feed->reconnect();
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            utils::log_error("WebSocket connection error");
            feed->errors_++;
            if (feed->config_.auto_reconnect && !feed->should_stop_) {
                feed->reconnect();
            }
            break;

        default:
            break;
    }

    return 0;
}

} // namespace data
} // namespace sentio
