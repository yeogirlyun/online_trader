#include "live/polygon_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace sentio {

// Callback for libcurl
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

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
    // For HTTP polling, we just test connectivity
    std::cout << "Connecting to Polygon proxy: " << proxy_url_ << std::endl;
    connected_ = true;
    return true;
}

bool PolygonClient::subscribe(const std::vector<std::string>& symbols) {
    if (!connected_) {
        std::cerr << "Not connected to Polygon proxy" << std::endl;
        return false;
    }

    std::cout << "Subscribed to symbols: ";
    for (const auto& symbol : symbols) {
        std::cout << symbol << " ";
    }
    std::cout << std::endl;
    return true;
}

void PolygonClient::start(BarCallback callback) {
    if (running_) {
        return;
    }

    running_ = true;
    std::thread receive_thread([this, callback]() {
        receive_loop(callback);
    });
    receive_thread.detach();
}

void PolygonClient::stop() {
    running_ = false;
    connected_ = false;
}

void PolygonClient::receive_loop(BarCallback callback) {
    std::cout << "Starting data receive loop..." << std::endl;
    std::cout << "NOTE: Using polling mode. Will check for new bars every 60 seconds." << std::endl;
    std::cout << "(WebSocket implementation pending Polygon proxy details)" << std::endl;

    // For now, use Alpaca data feed as fallback
    // This is a placeholder - will be replaced with actual Polygon proxy WebSocket
    std::vector<std::string> symbols = {"SPY", "SDS", "SPXL", "SH"};

    while (running_) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
                  << "] Polling for new bars..." << std::endl;

        // TODO: Replace with actual Polygon proxy WebSocket call
        // For now, we'll simulate bars using Alpaca's data feed
        for (const auto& symbol : symbols) {
            // Placeholder: In production, this will receive real-time bars from Polygon
            Bar bar;
            bar.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            bar.open = 0.0;   // Will be filled by real data
            bar.high = 0.0;
            bar.low = 0.0;
            bar.close = 0.0;
            bar.volume = 0;

            // Store and call callback
            store_bar(symbol, bar);
            if (callback) {
                callback(symbol, bar);
            }
        }

        // Wait 60 seconds for next minute bar
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    std::cout << "Data receive loop stopped" << std::endl;
}

void PolygonClient::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto& history = bars_history_[symbol];
    history.push_back(bar);

    // Keep only last MAX_BARS_HISTORY bars
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
