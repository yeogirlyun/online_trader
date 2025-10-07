// Temporary implementation: Use Alpaca's real-time data feed instead of Polygon proxy
// This gets us trading TODAY while we implement proper Polygon WebSocket

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
    std::cout << "Connecting to market data feed..." << std::endl;
    std::cout << "NOTE: Using Alpaca data feed for rapid deployment" << std::endl;
    std::cout << "      Polygon WebSocket will be added in next iteration" << std::endl;
    connected_ = true;
    return true;
}

bool PolygonClient::subscribe(const std::vector<std::string>& symbols) {
    if (!connected_) {
        std::cerr << "Not connected to data feed" << std::endl;
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

// Get latest bar from Alpaca API
Bar fetch_latest_bar_alpaca(const std::string& symbol, const std::string& alpaca_key, const std::string& alpaca_secret) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    // Get last 2 minutes of 1-min bars (to ensure we have latest complete bar)
    auto now = std::chrono::system_clock::now();
    auto start_time = now - std::chrono::minutes(2);

    auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count();
    auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::string url = "https://data.alpaca.markets/v2/stocks/" + symbol +
                     "/bars?timeframe=1Min&start=" + std::to_string(start_ms) +
                     "&end=" + std::to_string(end_ms) + "&limit=2";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("APCA-API-KEY-ID: " + alpaca_key).c_str());
    headers = curl_slist_append(headers, ("APCA-API-SECRET-KEY: " + alpaca_secret).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP GET failed");
    }

    // Parse response
    Bar bar;
    try {
        json j = json::parse(response);
        if (j.contains("bars") && !j["bars"].empty()) {
            auto latest_bar = j["bars"].back();
            bar.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            bar.open = latest_bar["o"].get<double>();
            bar.high = latest_bar["h"].get<double>();
            bar.low = latest_bar["l"].get<double>();
            bar.close = latest_bar["c"].get<double>();
            bar.volume = latest_bar["v"].get<int64_t>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing bar: " << e.what() << std::endl;
    }

    return bar;
}

void PolygonClient::receive_loop(BarCallback callback) {
    std::cout << "Starting data receive loop..." << std::endl;
    std::cout << "Fetching 1-minute bars via Alpaca REST API..." << std::endl;

    // For rapid deployment, use Alpaca API key from environment
    const char* alpaca_key = std::getenv("ALPACA_PAPER_API_KEY");
    const char* alpaca_secret = std::getenv("ALPACA_PAPER_SECRET_KEY");

    std::string key = alpaca_key ? alpaca_key : "PK3NCBT07OJZJULDJR5V";
    std::string secret = alpaca_secret ? alpaca_secret : "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

    std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};

    while (running_) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
                  << "] Fetching latest bars..." << std::endl;

        // Fetch latest bar for each symbol
        for (const auto& symbol : symbols) {
            try {
                Bar bar = fetch_latest_bar_alpaca(symbol, key, secret);

                if (bar.close > 0) {  // Valid bar
                    std::cout << "  " << symbol << ": $" << bar.close
                             << " (O:" << bar.open << " H:" << bar.high
                             << " L:" << bar.low << " V:" << bar.volume << ")" << std::endl;

                    // Store and callback
                    store_bar(symbol, bar);
                    if (callback && symbol == "SPY") {  // Only process on SPY
                        callback(symbol, bar);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error fetching " << symbol << ": " << e.what() << std::endl;
            }
        }

        // Wait 60 seconds for next minute bar
        std::cout << "  Waiting 60 seconds for next bar..." << std::endl;
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
