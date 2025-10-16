#include "live/alpaca_rest_bar_feed.h"
#include <iostream>
#include <sstream>

namespace sentio {

AlpacaRestBarFeed::AlpacaRestBarFeed(const std::string& api_key,
                                      const std::string& secret_key,
                                      bool paper_trading,
                                      int poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms),
      running_(false),
      connected_(false),
      last_bar_timestamp_ms_(0) {

    client_ = std::make_unique<AlpacaClient>(api_key, secret_key, paper_trading);
    last_message_time_.store(std::chrono::steady_clock::now());
}

AlpacaRestBarFeed::~AlpacaRestBarFeed() {
    stop();
}

bool AlpacaRestBarFeed::connect() {
    if (connected_.load()) {
        return true;
    }

    // Test connection by calling get_account
    try {
        auto account = client_->get_account();
        if (account) {
            connected_.store(true);
            std::cout << "[REST_FEED] ✓ Connected to Alpaca REST API\n" << std::flush;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[REST_FEED] ❌ Connection failed: " << e.what() << "\n" << std::flush;
    }

    return false;
}

bool AlpacaRestBarFeed::subscribe(const std::vector<std::string>& symbols) {
    subscribed_symbols_ = symbols;

    std::cout << "[REST_FEED] ✓ Subscribed to: ";
    for (const auto& sym : symbols) {
        std::cout << sym << " ";
    }
    std::cout << "\n" << std::flush;

    return true;
}

void AlpacaRestBarFeed::start(BarCallback callback) {
    if (running_.load()) {
        std::cerr << "[REST_FEED] ⚠️  Already running\n" << std::flush;
        return;
    }

    callback_ = callback;
    running_.store(true);

    std::cout << "[REST_FEED] ✓ Starting REST polling loop (interval: "
              << poll_interval_ms_ << "ms)\n" << std::flush;

    // Start polling thread
    poll_thread_ = std::thread(&AlpacaRestBarFeed::poll_loop, this);
}

void AlpacaRestBarFeed::stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "[REST_FEED] Stopping polling loop...\n" << std::flush;

    running_.store(false);

    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }

    connected_.store(false);
    std::cout << "[REST_FEED] ✓ Stopped\n" << std::flush;
}

std::vector<Bar> AlpacaRestBarFeed::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto it = recent_bars_.find(symbol);
    if (it == recent_bars_.end() || it->second.empty()) {
        return {};
    }

    const auto& bars = it->second;
    size_t start_idx = (bars.size() > count) ? (bars.size() - count) : 0;

    return std::vector<Bar>(bars.begin() + start_idx, bars.end());
}

bool AlpacaRestBarFeed::is_connected() const {
    return connected_.load();
}

bool AlpacaRestBarFeed::is_connection_healthy() const {
    // Consider healthy if received message in last 5 minutes
    return get_seconds_since_last_message() < 300;
}

int AlpacaRestBarFeed::get_seconds_since_last_message() const {
    auto now = std::chrono::steady_clock::now();
    auto last_time = last_message_time_.load();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
    return static_cast<int>(duration.count());
}

void AlpacaRestBarFeed::poll_loop() {
    std::cout << "[REST_FEED] Polling loop started\n" << std::flush;

    while (running_.load()) {
        try {
            // Poll latest bars for all subscribed symbols
            auto bars_data = client_->get_latest_bars(subscribed_symbols_);

            if (!bars_data.empty()) {
                for (const auto& bar_data : bars_data) {
                    // Convert to Bar
                    Bar bar = convert_bar(bar_data);

                    // Only process if this is a new bar (avoid duplicates)
                    if (bar.timestamp_ms > last_bar_timestamp_ms_.load()) {
                        // Cache bar
                        cache_bar(bar_data.symbol, bar);

                        // Call callback
                        if (callback_) {
                            callback_(bar_data.symbol, bar);
                        }

                        // Update last bar timestamp
                        last_bar_timestamp_ms_.store(bar.timestamp_ms);
                        last_message_time_.store(std::chrono::steady_clock::now());

                        std::cout << "[REST_FEED] ✓ " << bar_data.symbol
                                  << " @ " << bar.timestamp_ms
                                  << " | C:" << bar.close
                                  << " V:" << bar.volume << "\n" << std::flush;
                    }
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "[REST_FEED] ❌ Poll error: " << e.what() << "\n" << std::flush;
        }

        // Sleep for poll interval (check running flag every 100ms for quick shutdown)
        for (int i = 0; i < poll_interval_ms_ / 100 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[REST_FEED] Polling loop ended\n" << std::flush;
}

Bar AlpacaRestBarFeed::convert_bar(const AlpacaClient::BarData& alpaca_bar) {
    Bar bar;
    bar.timestamp_ms = alpaca_bar.timestamp_ms;
    bar.open = alpaca_bar.open;
    bar.high = alpaca_bar.high;
    bar.low = alpaca_bar.low;
    bar.close = alpaca_bar.close;
    bar.volume = alpaca_bar.volume;
    return bar;
}

void AlpacaRestBarFeed::cache_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    auto& bars = recent_bars_[symbol];
    bars.push_back(bar);

    // Keep only last MAX_CACHED_BARS
    if (bars.size() > MAX_CACHED_BARS) {
        bars.erase(bars.begin(), bars.begin() + (bars.size() - MAX_CACHED_BARS));
    }
}

} // namespace sentio
