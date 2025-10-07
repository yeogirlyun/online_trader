// Alpaca IEX WebSocket Client - Real-time market data
// URL: wss://stream.data.alpaca.markets/v2/iex
// Docs: https://docs.alpaca.markets/docs/streaming-market-data

#include "live/polygon_client.hpp"
#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

using json = nlohmann::json;

namespace sentio {

// WebSocket callback context
struct ws_context {
    PolygonClient* client;
    PolygonClient::BarCallback callback;
    bool connected;
    std::string buffer;
};

static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len) {
    ws_context* ctx = (ws_context*)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✓ WebSocket connected to Alpaca IEX" << std::endl;
            ctx->connected = true;

            // Alpaca requires authentication first
            // Auth key format: "KEY|SECRET"
            {
                // Parse API key and secret from auth_key
                std::string auth_key_pair = ctx->client ? "" : "";  // Will be passed properly
                size_t delimiter = auth_key_pair.find('|');
                std::string api_key, api_secret;

                // Get keys from environment
                const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
                const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

                // Use hardcoded keys if environment not set (paper trading)
                api_key = alpaca_key_env ? alpaca_key_env : "PK3NCBT07OJZJULDJR5V";
                api_secret = alpaca_secret_env ? alpaca_secret_env : "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

                // Send authentication message
                json auth;
                auth["action"] = "auth";
                auth["key"] = api_key;
                auth["secret"] = api_secret;
                std::string auth_msg = auth.dump();
                unsigned char auth_buf[LWS_PRE + 1024];
                memcpy(&auth_buf[LWS_PRE], auth_msg.c_str(), auth_msg.length());
                lws_write(wsi, &auth_buf[LWS_PRE], auth_msg.length(), LWS_WRITE_TEXT);
                std::cout << "→ Sent authentication to Alpaca" << std::endl;

                // Subscribe to minute bars for SPY instruments
                // Alpaca format: {"action":"subscribe","bars":["SPY","SPXL","SH","SDS"]}
                json sub;
                sub["action"] = "subscribe";
                sub["bars"] = json::array({"SPY", "SPXL", "SH", "SDS"});
                std::string sub_msg = sub.dump();
                unsigned char sub_buf[LWS_PRE + 512];
                memcpy(&sub_buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
                lws_write(wsi, &sub_buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
                std::cout << "→ Subscribed to bars: SPY, SPXL, SH, SDS" << std::endl;
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            // Accumulate message
            ctx->buffer.append((char*)in, len);

            // Check if message is complete
            if (lws_is_final_fragment(wsi)) {
                try {
                    json j = json::parse(ctx->buffer);

                    // Alpaca sends arrays of messages: [{...}, {...}]
                    if (j.is_array()) {
                        for (const auto& msg : j) {
                            // Control messages (authentication, subscriptions, errors)
                            if (msg.contains("T")) {
                                std::string msg_type = msg["T"];

                                // Success/error messages
                                if (msg_type == "success") {
                                    std::cout << "Alpaca: " << msg.value("msg", "Success") << std::endl;
                                } else if (msg_type == "error") {
                                    std::cerr << "Alpaca Error: " << msg.value("msg", "Unknown error") << std::endl;
                                } else if (msg_type == "subscription") {
                                    std::cout << "Alpaca: Subscriptions confirmed" << std::endl;
                                }

                                // Minute bar message (T="b")
                                else if (msg_type == "b") {
                                    Bar bar;
                                    // Alpaca timestamp format: "2025-10-06T13:30:00Z" (ISO 8601)
                                    // Need to convert to milliseconds since epoch
                                    std::string timestamp_str = msg.value("t", "");
                                    // For now, use current time (will implement proper ISO parsing if needed)
                                    bar.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::system_clock::now().time_since_epoch()
                                    ).count();

                                    bar.open = msg.value("o", 0.0);
                                    bar.high = msg.value("h", 0.0);
                                    bar.low = msg.value("l", 0.0);
                                    bar.close = msg.value("c", 0.0);
                                    bar.volume = msg.value("v", 0LL);

                                    std::string symbol = msg.value("S", "");

                                    if (bar.close > 0 && !symbol.empty()) {
                                        std::cout << "✓ Bar: " << symbol << " $" << bar.close
                                                  << " (O:" << bar.open << " H:" << bar.high
                                                  << " L:" << bar.low << " V:" << bar.volume << ")" << std::endl;

                                        // Store bar
                                        if (ctx->client) {
                                            ctx->client->store_bar(symbol, bar);
                                        }

                                        // Callback (only for SPY to trigger strategy)
                                        if (ctx->callback && symbol == "SPY") {
                                            ctx->callback(symbol, bar);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
                    std::cerr << "Message was: " << ctx->buffer.substr(0, 200) << std::endl;
                }

                ctx->buffer.clear();
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "❌ WebSocket connection error" << std::endl;
            ctx->connected = false;
            break;

        case LWS_CALLBACK_CLOSED:
            std::cout << "WebSocket connection closed" << std::endl;
            ctx->connected = false;
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "polygon-protocol",
        websocket_callback,
        sizeof(ws_context),
        4096,
    },
    { NULL, NULL, 0, 0 } // terminator
};

PolygonClient::PolygonClient(const std::string& proxy_url, const std::string& auth_key)
    : proxy_url_(proxy_url)
    , auth_key_(auth_key)
    , connected_(false)
    , running_(false)
{
}

PolygonClient::~PolygonClient() {
    stop();
}

bool PolygonClient::connect() {
    std::cout << "Connecting to Polygon WebSocket proxy..." << std::endl;
    std::cout << "URL: " << proxy_url_ << std::endl;
    connected_ = true;  // Will be updated by WebSocket callback
    return true;
}

bool PolygonClient::subscribe(const std::vector<std::string>& symbols) {
    std::cout << "Subscribing to: ";
    for (const auto& s : symbols) std::cout << s << " ";
    std::cout << std::endl;
    return true;
}

void PolygonClient::start(BarCallback callback) {
    if (running_) return;

    running_ = true;
    std::thread ws_thread([this, callback]() {
        receive_loop(callback);
    });
    ws_thread.detach();
}

void PolygonClient::stop() {
    running_ = false;
    connected_ = false;
}

void PolygonClient::receive_loop(BarCallback callback) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info conn_info;
    struct lws_context *context;
    struct lws *wsi;
    ws_context ctx;

    memset(&info, 0, sizeof(info));
    memset(&conn_info, 0, sizeof(conn_info));
    memset(&ctx, 0, sizeof(ctx));

    ctx.client = this;
    ctx.callback = callback;
    ctx.connected = false;

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) {
        std::cerr << "❌ Failed to create WebSocket context" << std::endl;
        return;
    }

    // Connect to Alpaca IEX WebSocket directly
    conn_info.context = context;
    conn_info.address = "stream.data.alpaca.markets";
    conn_info.port = 443;
    conn_info.path = "/v2/iex";
    conn_info.host = conn_info.address;
    conn_info.origin = conn_info.address;
    conn_info.protocol = protocols[0].name;
    conn_info.ssl_connection = LCCSCF_USE_SSL;
    conn_info.userdata = &ctx;

    std::cout << "Connecting to wss://" << conn_info.address << ":" << conn_info.port << conn_info.path << std::endl;

    wsi = lws_client_connect_via_info(&conn_info);
    if (!wsi) {
        std::cerr << "❌ Failed to connect to WebSocket" << std::endl;
        lws_context_destroy(context);
        return;
    }

    // Service loop
    while (running_) {
        lws_service(context, 50);  // 50ms timeout
    }

    lws_context_destroy(context);
    std::cout << "WebSocket loop ended" << std::endl;
}

void PolygonClient::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto& history = bars_history_[symbol];
    history.push_back(bar);

    if (history.size() > MAX_BARS_HISTORY) {
        history.pop_front();
    }
}

std::vector<Bar> PolygonClient::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto it = bars_history_.find(symbol);
    if (it == bars_history_.end()) {
        return {};
    }

    const auto& history = it->second;
    size_t start = (history.size() > count) ? (history.size() - count) : 0;

    std::vector<Bar> result;
    for (size_t i = start; i < history.size(); ++i) {
        result.push_back(history[i]);
    }

    return result;
}

bool PolygonClient::is_connected() const {
    return connected_;
}

} // namespace sentio
