// Alpaca IEX WebSocket Client via Python Bridge - Real-time market data
// Reads JSON bars from FIFO pipe written by Python bridge
// Python bridge: tools/alpaca_websocket_bridge.py

#include "live/polygon_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>

using json = nlohmann::json;

namespace sentio {

// FIFO pipe path (must match Python bridge)
constexpr const char* FIFO_PATH = "/tmp/alpaca_bars.fifo";

PolygonClient::PolygonClient(const std::string& proxy_url, const std::string& auth_key)
    : proxy_url_(proxy_url)
    , auth_key_(auth_key)
    , connected_(false)
    , running_(false)
    , last_message_time_(std::chrono::steady_clock::now())
{
}

PolygonClient::~PolygonClient() {
    stop();
}

bool PolygonClient::connect() {
    std::cout << "Connecting to Python WebSocket bridge..." << std::endl;
    std::cout << "Reading from FIFO: " << FIFO_PATH << std::endl;
    connected_ = true;  // Will be verified when first bar arrives
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
    std::thread fifo_thread([this, callback]() {
        receive_loop(callback);
    });
    fifo_thread.detach();
}

void PolygonClient::stop() {
    running_ = false;
    connected_ = false;
}

void PolygonClient::receive_loop(BarCallback callback) {
    std::cout << "→ FIFO read loop started" << std::endl;

    while (running_) {
        try {
            // Open FIFO for reading
            // NOTE: This blocks until Python bridge opens it for writing
            std::ifstream fifo(FIFO_PATH);
            if (!fifo.is_open()) {
                std::cerr << "❌ Failed to open FIFO: " << FIFO_PATH << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            connected_ = true;
            std::cout << "✓ FIFO opened - reading bars from Python bridge" << std::endl;

            // Read JSON bars line by line
            std::string line;
            while (running_ && std::getline(fifo, line)) {
                if (line.empty()) continue;

                try {
                    json j = json::parse(line);

                    // Extract bar data from Python bridge format
                    Bar bar;
                    bar.timestamp_ms = j["timestamp_ms"];
                    bar.open = j["open"];
                    bar.high = j["high"];
                    bar.low = j["low"];
                    bar.close = j["close"];
                    bar.volume = j["volume"];

                    std::string symbol = j["symbol"];

                    // Update health timestamp
                    update_last_message_time();

                    std::cout << "✓ Bar: " << symbol << " $" << bar.close
                              << " (O:" << bar.open << " H:" << bar.high
                              << " L:" << bar.low << " V:" << bar.volume << ")" << std::endl;

                    // Store bar
                    store_bar(symbol, bar);

                    // Callback (only for SPY to trigger strategy)
                    if (callback && symbol == "SPY") {
                        callback(symbol, bar);
                    }

                } catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON from FIFO: " << e.what() << std::endl;
                    std::cerr << "Line was: " << line.substr(0, 200) << std::endl;
                }
            }

            fifo.close();

            if (!running_) {
                std::cout << "FIFO read loop ended (stop requested)" << std::endl;
                return;
            }

            // FIFO closed (Python bridge restarted?) - reconnect
            std::cerr << "❌ FIFO closed - attempting to reopen in 3s..." << std::endl;
            connected_ = false;
            std::this_thread::sleep_for(std::chrono::seconds(3));

        } catch (const std::exception& e) {
            std::cerr << "❌ FIFO read error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    std::cout << "FIFO read loop terminated" << std::endl;
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

void PolygonClient::update_last_message_time() {
    last_message_time_.store(std::chrono::steady_clock::now());
}

bool PolygonClient::is_connection_healthy() const {
    auto now = std::chrono::steady_clock::now();
    auto last_msg = last_message_time_.load();
    auto silence_duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_msg
    ).count();

    return silence_duration < HEALTH_CHECK_TIMEOUT_SECONDS;
}

int PolygonClient::get_seconds_since_last_message() const {
    auto now = std::chrono::steady_clock::now();
    auto last_msg = last_message_time_.load();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - last_msg
    ).count();
}

} // namespace sentio
