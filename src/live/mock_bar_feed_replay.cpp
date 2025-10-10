#include "live/mock_bar_feed_replay.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace sentio {

MockBarFeedReplay::MockBarFeedReplay(const std::string& csv_file, double speed_multiplier)
    : connected_(false)
    , running_(false)
    , current_index_(0)
    , speed_multiplier_(speed_multiplier)
    , replay_start_market_ms_(0)
    , last_message_time_(Clock::now())
{
    load_csv(csv_file);
}

MockBarFeedReplay::~MockBarFeedReplay() {
    stop();
}

bool MockBarFeedReplay::connect() {
    if (bars_by_symbol_.empty()) {
        return false;
    }
    connected_ = true;
    return true;
}

bool MockBarFeedReplay::subscribe(const std::vector<std::string>& symbols) {
    subscribed_symbols_ = symbols;
    return true;
}

void MockBarFeedReplay::start(BarCallback callback) {
    if (!connected_ || running_) {
        return;
    }

    callback_ = callback;
    running_ = true;
    current_index_ = 0;

    // Initialize time anchors
    replay_start_real_ = Clock::now();

    // Find first bar timestamp as market start time
    if (!bars_by_symbol_.empty()) {
        const auto& first_symbol_bars = bars_by_symbol_.begin()->second;
        if (!first_symbol_bars.empty()) {
            replay_start_market_ms_ = first_symbol_bars[0].timestamp_ms;
        }
    }

    // Start replay thread
    replay_thread_ = std::make_unique<std::thread>(&MockBarFeedReplay::replay_loop, this);
}

void MockBarFeedReplay::stop() {
    running_ = false;

    if (replay_thread_ && replay_thread_->joinable()) {
        replay_thread_->join();
    }

    connected_ = false;
}

std::vector<Bar> MockBarFeedReplay::get_recent_bars(const std::string& symbol, size_t count) const {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    std::vector<Bar> result;

    if (bars_history_.count(symbol)) {
        const auto& history = bars_history_.at(symbol);
        size_t start = (history.size() > count) ? (history.size() - count) : 0;

        for (size_t i = start; i < history.size(); ++i) {
            result.push_back(history[i]);
        }
    }

    return result;
}

bool MockBarFeedReplay::is_connected() const {
    return connected_;
}

bool MockBarFeedReplay::is_connection_healthy() const {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_message_time_.load()).count();
    return elapsed < 120;  // 2 minutes timeout
}

int MockBarFeedReplay::get_seconds_since_last_message() const {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now - last_message_time_.load()).count();
}

bool MockBarFeedReplay::load_csv(const std::string& csv_file) {
    std::ifstream file(csv_file);
    if (!file.is_open()) {
        return false;
    }

    // CSV format: date_str,timestamp_sec,open,high,low,close,volume
    // All bars go into "SPY" by default (can be extended for multi-symbol)

    std::string line;

    std::vector<Bar> bars;

    while (std::getline(file, line)) {
        // Skip empty lines or header-like lines
        if (line.empty() ||
            line.find("timestamp") != std::string::npos ||
            line.find("ts_utc") != std::string::npos ||
            line.find("ts_nyt_epoch") != std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        std::string date_str, ts_str, open_str, high_str, low_str, close_str, volume_str;

        if (std::getline(iss, date_str, ',') &&
            std::getline(iss, ts_str, ',') &&
            std::getline(iss, open_str, ',') &&
            std::getline(iss, high_str, ',') &&
            std::getline(iss, low_str, ',') &&
            std::getline(iss, close_str, ',') &&
            std::getline(iss, volume_str)) {

            Bar bar;
            // Convert seconds to milliseconds
            bar.timestamp_ms = std::stoull(ts_str) * 1000ULL;
            bar.open = std::stod(open_str);
            bar.high = std::stod(high_str);
            bar.low = std::stod(low_str);
            bar.close = std::stod(close_str);
            bar.volume = std::stoll(volume_str);

            bars.push_back(bar);
        }
    }

    file.close();

    if (bars.empty()) {
        return false;
    }

    // Sort by timestamp
    std::sort(bars.begin(), bars.end(),
              [](const Bar& a, const Bar& b) { return a.timestamp_ms < b.timestamp_ms; });

    bars_by_symbol_["SPY"] = bars;

    // For multi-instrument, create synthetic bars for other symbols
    // (In production, load from separate CSV files)
    bars_by_symbol_["SPXL"] = bars;  // Same timing for now
    bars_by_symbol_["SH"] = bars;
    bars_by_symbol_["SDS"] = bars;

    return true;
}

void MockBarFeedReplay::add_bar(const std::string& symbol, const Bar& bar) {
    bars_by_symbol_[symbol].push_back(bar);
}

void MockBarFeedReplay::set_speed_multiplier(double multiplier) {
    speed_multiplier_ = multiplier;
}

MockBarFeedReplay::ReplayProgress MockBarFeedReplay::get_progress() const {
    ReplayProgress progress;

    if (!bars_by_symbol_.empty()) {
        const auto& bars = bars_by_symbol_.begin()->second;
        progress.total_bars = bars.size();
        progress.current_index = current_index_;
        progress.progress_pct = (progress.total_bars > 0) ?
            (100.0 * progress.current_index / progress.total_bars) : 0.0;

        if (progress.current_index < bars.size()) {
            progress.current_bar_timestamp_ms = bars[progress.current_index].timestamp_ms;

            // Format timestamp
            time_t time_t_val = static_cast<time_t>(progress.current_bar_timestamp_ms / 1000);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
            progress.current_bar_time_str = ss.str();
        }
    }

    return progress;
}

bool MockBarFeedReplay::is_replay_complete() const {
    if (bars_by_symbol_.empty()) {
        return true;
    }

    const auto& bars = bars_by_symbol_.begin()->second;
    return current_index_ >= bars.size();
}

bool MockBarFeedReplay::validate_data_integrity() const {
    for (const auto& [symbol, bars] : bars_by_symbol_) {
        // Check for gaps in timestamps
        for (size_t i = 1; i < bars.size(); ++i) {
            if (bars[i].timestamp_ms <= bars[i-1].timestamp_ms) {
                return false;  // Not monotonically increasing
            }
        }

        // Verify OHLC relationships
        for (const auto& bar : bars) {
            if (bar.high < bar.low) return false;
            if (bar.high < bar.open) return false;
            if (bar.high < bar.close) return false;
            if (bar.low > bar.open) return false;
            if (bar.low > bar.close) return false;
            if (bar.volume < 0) return false;
        }
    }

    return true;
}

void MockBarFeedReplay::replay_loop() {
    while (running_ && !is_replay_complete()) {
        std::string symbol;
        auto bar_opt = get_next_bar(symbol);

        if (!bar_opt.has_value()) {
            break;  // No more bars
        }

        const Bar& bar = bar_opt.value();

        // Wait until it's time to deliver this bar (drift-free)
        wait_until_bar_time(bar);

        // Store in history
        store_bar(symbol, bar);

        // Update health timestamp
        last_message_time_ = Clock::now();

        // Deliver to callback
        if (callback_) {
            callback_(symbol, bar);
        }

        current_index_++;
    }

    running_ = false;
}

void MockBarFeedReplay::store_bar(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(bars_mutex_);

    if (bars_history_[symbol].size() >= MAX_BARS_HISTORY) {
        bars_history_[symbol].pop_front();
    }

    bars_history_[symbol].push_back(bar);
}

std::optional<Bar> MockBarFeedReplay::get_next_bar(std::string& out_symbol) {
    // For simplicity, deliver SPY bars (can be extended for multi-symbol round-robin)
    if (bars_by_symbol_.count("SPY") == 0) {
        return std::nullopt;
    }

    const auto& bars = bars_by_symbol_["SPY"];
    size_t idx = current_index_;

    if (idx >= bars.size()) {
        return std::nullopt;
    }

    out_symbol = "SPY";
    return bars[idx];
}

void MockBarFeedReplay::wait_until_bar_time(const Bar& bar) {
    if (speed_multiplier_ <= 0.0) {
        return;  // No delay
    }

    // Calculate when this bar should be delivered (drift-free)
    uint64_t elapsed_market_ms = bar.timestamp_ms - replay_start_market_ms_;

    // Scale by speed multiplier (higher multiplier = faster)
    auto elapsed_real_ms = static_cast<uint64_t>(elapsed_market_ms / speed_multiplier_);

    auto target_time = replay_start_real_ + std::chrono::milliseconds(elapsed_real_ms);

    // Sleep until target time (prevents drift accumulation)
    std::this_thread::sleep_until(target_time);
}

} // namespace sentio
