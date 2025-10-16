#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <set>

namespace sentio {
namespace data {

MockMultiSymbolFeed::MockMultiSymbolFeed(
    std::shared_ptr<MultiSymbolDataManager> data_manager,
    const Config& config
)
    : data_manager_(data_manager)
    , config_(config)
    , total_bars_(0) {

    utils::log_info("MockMultiSymbolFeed initialized with " +
                   std::to_string(config_.symbol_files.size()) + " symbols, " +
                   "speed=" + std::to_string(config_.replay_speed) + "x");
}

MockMultiSymbolFeed::~MockMultiSymbolFeed() {
    stop();
    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }
}

// === IBarFeed Interface Implementation ===

bool MockMultiSymbolFeed::connect() {
    if (connected_) {
        utils::log_warning("MockMultiSymbolFeed already connected");
        return true;
    }

    utils::log_info("Loading CSV data for " +
                   std::to_string(config_.symbol_files.size()) + " symbols...");
    std::cout << "[Feed] Loading CSV data for " << config_.symbol_files.size() << " symbols..." << std::endl;

    int total_loaded = 0;

    for (const auto& [symbol, filepath] : config_.symbol_files) {
        std::cout << "[Feed] Loading " << symbol << " from " << filepath << std::endl;
        int loaded = load_csv(symbol, filepath);
        if (loaded == 0) {
            utils::log_error("Failed to load data for " + symbol + " from " + filepath);
            std::cout << "[Feed] ❌ Failed to load " << symbol << " (0 bars loaded)" << std::endl;
            if (error_callback_) {
                error_callback_("Failed to load " + symbol + ": " + filepath);
            }
            return false;
        }

        total_loaded += loaded;
        utils::log_info("  " + symbol + ": " + std::to_string(loaded) + " bars");
        std::cout << "[Feed] ✓ Loaded " << symbol << ": " << loaded << " bars" << std::endl;
    }

    total_bars_ = total_loaded;
    connected_ = true;

    utils::log_info("Total bars loaded: " + std::to_string(total_loaded));

    if (connection_callback_) {
        connection_callback_(true);
    }

    return true;
}

void MockMultiSymbolFeed::disconnect() {
    if (!connected_) {
        return;
    }

    stop();

    symbol_data_.clear();
    bars_replayed_ = 0;
    total_bars_ = 0;
    connected_ = false;

    utils::log_info("MockMultiSymbolFeed disconnected");

    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool MockMultiSymbolFeed::is_connected() const {
    return connected_;
}

bool MockMultiSymbolFeed::start() {
    if (!connected_) {
        utils::log_error("Cannot start - not connected. Call connect() first.");
        return false;
    }

    if (active_) {
        utils::log_warning("MockMultiSymbolFeed already active");
        return true;
    }

    if (symbol_data_.empty()) {
        utils::log_error("No data loaded - call connect() first");
        return false;
    }

    utils::log_info("Starting replay (" +
                   std::to_string(config_.replay_speed) + "x speed)...");

    bars_replayed_ = 0;
    should_stop_ = false;
    active_ = true;

    // Start replay thread
    replay_thread_ = std::thread(&MockMultiSymbolFeed::replay_loop, this);

    return true;
}

void MockMultiSymbolFeed::stop() {
    if (!active_) {
        return;
    }

    utils::log_info("Stopping replay...");
    should_stop_ = true;
    active_ = false;

    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }

    utils::log_info("Replay stopped: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::is_active() const {
    return active_;
}

std::string MockMultiSymbolFeed::get_type() const {
    return "MockMultiSymbolFeed";
}

std::vector<std::string> MockMultiSymbolFeed::get_symbols() const {
    std::vector<std::string> symbols;
    for (const auto& [symbol, _] : config_.symbol_files) {
        symbols.push_back(symbol);
    }
    return symbols;
}

void MockMultiSymbolFeed::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void MockMultiSymbolFeed::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void MockMultiSymbolFeed::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = callback;
}

IBarFeed::FeedStats MockMultiSymbolFeed::get_stats() const {
    FeedStats stats;
    stats.total_bars_received = bars_replayed_.load();
    stats.errors = errors_.load();
    stats.reconnects = 0;  // Mock doesn't reconnect
    stats.avg_latency_ms = 0.0;  // Mock has no latency

    // Per-symbol counts
    int i = 0;
    for (const auto& [symbol, data] : symbol_data_) {
        if (i < 10) {
            stats.bars_per_symbol[i] = static_cast<int>(data.current_index);
            i++;
        }
    }

    return stats;
}

// === Additional Mock-Specific Methods ===

void MockMultiSymbolFeed::replay_loop() {
    utils::log_info("Replay loop started");

    // Reset current_index for all symbols
    for (auto& [symbol, data] : symbol_data_) {
        data.current_index = 0;
    }

    // Replay until all symbols exhausted or stop requested
    while (!should_stop_ && replay_next_bar()) {
        // Sleep for replay speed
        if (config_.replay_speed > 0) {
            sleep_for_replay();
        }
    }

    active_ = false;
    utils::log_info("Replay loop complete: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::replay_next_bar() {
    // Check if any symbol has bars remaining
    bool has_data = false;
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        return false;  // All symbols exhausted
    }

    // If syncing timestamps, find common timestamp
    uint64_t target_timestamp = 0;

    if (config_.sync_timestamps) {
        // Find minimum timestamp across all symbols
        target_timestamp = UINT64_MAX;

        for (const auto& [symbol, data] : symbol_data_) {
            if (data.current_index < data.bars.size()) {
                uint64_t ts = data.bars[data.current_index].timestamp_ms;
                target_timestamp = std::min(target_timestamp, ts);
            }
        }
    }

    // Update all symbols at this timestamp
    std::string last_symbol;
    Bar last_bar;
    bool any_updated = false;

    // FIX 3: Track which symbols are updated for validation
    std::set<std::string> updated_symbols;
    int expected_symbols = symbol_data_.size();

    for (auto& [symbol, data] : symbol_data_) {
        if (data.current_index >= data.bars.size()) {
            continue;  // Symbol exhausted
        }

        const auto& bar = data.bars[data.current_index];

        // If syncing, only update if timestamp matches
        if (config_.sync_timestamps && bar.timestamp_ms != target_timestamp) {
            continue;
        }

        // Feed bar to data manager (direct update)
        if (data_manager_) {
            data_manager_->update_symbol(symbol, bar);
            updated_symbols.insert(symbol);
        }

        data.current_index++;
        bars_replayed_++;

        last_symbol = symbol;
        last_bar = bar;
        any_updated = true;
    }

    // FIX 3: Validate all expected symbols were updated
    static int validation_counter = 0;
    if (validation_counter++ % 100 == 0) {  // Check every 100 bars
        if (updated_symbols.size() != static_cast<size_t>(expected_symbols)) {
            utils::log_warning("[FEED VALIDATION] Only updated " +
                             std::to_string(updated_symbols.size()) +
                             " of " + std::to_string(expected_symbols) + " symbols at bar " +
                             std::to_string(bars_replayed_.load()));

            // Log which symbols are missing
            for (const auto& [symbol, data] : symbol_data_) {
                if (updated_symbols.count(symbol) == 0) {
                    utils::log_warning("  Missing update for: " + symbol +
                                     " (index: " + std::to_string(data.current_index) +
                                     "/" + std::to_string(data.bars.size()) + ")");
                }
            }
        }
    }

    // Call callback ONCE after all symbols updated (not for each symbol!)
    if (bar_callback_ && any_updated) {
        bar_callback_(last_symbol, last_bar);
    }

    return true;
}

std::map<std::string, int> MockMultiSymbolFeed::get_bar_counts() const {
    std::map<std::string, int> counts;
    for (const auto& [symbol, data] : symbol_data_) {
        counts[symbol] = static_cast<int>(data.bars.size());
    }
    return counts;
}

MockMultiSymbolFeed::Progress MockMultiSymbolFeed::get_progress() const {
    Progress prog;
    prog.bars_replayed = bars_replayed_;
    prog.total_bars = total_bars_;
    prog.progress_pct = (total_bars_ > 0) ?
        (static_cast<double>(bars_replayed_) / total_bars_ * 100.0) : 0.0;

    // Find current symbol/timestamp
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            prog.current_symbol = symbol;
            prog.current_timestamp = data.bars[data.current_index].timestamp_ms;
            break;
        }
    }

    return prog;
}

// === Private methods ===

int MockMultiSymbolFeed::load_csv(const std::string& symbol, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        utils::log_error("Cannot open file: " + filepath);
        return 0;
    }

    SymbolData data;
    std::string line;
    int line_num = 0;
    int loaded = 0;
    int parse_failures = 0;

    // Skip header
    if (std::getline(file, line)) {
        line_num++;
        utils::log_info("  Header: " + line.substr(0, std::min(100, (int)line.size())));
    }

    // Read data
    while (std::getline(file, line)) {
        line_num++;

        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty/comment lines
        }

        Bar bar;
        if (parse_csv_line(line, bar)) {
            data.bars.push_back(bar);
            loaded++;

            // Log first bar as sample
            if (loaded == 1) {
                utils::log_info("  First bar: timestamp=" + std::to_string(bar.timestamp_ms) +
                              " close=" + std::to_string(bar.close));
            }
        } else {
            parse_failures++;
            if (parse_failures <= 3) {
                utils::log_warning("Failed to parse line " + std::to_string(line_num) +
                                  " in " + filepath + ": " + line.substr(0, 80));
            }
        }
    }

    file.close();

    if (parse_failures > 3) {
        utils::log_warning("  Total parse failures: " + std::to_string(parse_failures));
    }

    // Apply date filter if specified
    if (!config_.filter_date.empty() && loaded > 0) {
        // Parse filter_date (YYYY-MM-DD)
        int year, month, day;
        if (std::sscanf(config_.filter_date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
            // Filter bars to only include this specific date
            std::deque<Bar> filtered_bars;
            for (const auto& bar : data.bars) {
                std::time_t bar_time = bar.timestamp_ms / 1000;
                std::tm* bar_tm = std::localtime(&bar_time);

                if (bar_tm &&
                    bar_tm->tm_year + 1900 == year &&
                    bar_tm->tm_mon + 1 == month &&
                    bar_tm->tm_mday == day) {
                    filtered_bars.push_back(bar);
                }
            }

            data.bars = std::move(filtered_bars);
            int filtered_count = static_cast<int>(data.bars.size());
            utils::log_info("  Date-filtered to " + std::to_string(filtered_count) +
                          " bars for " + symbol + " on " + config_.filter_date);
            loaded = filtered_count;
        } else {
            utils::log_warning("  Invalid filter_date format: " + config_.filter_date);
        }
    }

    if (loaded > 0) {
        symbol_data_[symbol] = std::move(data);
        utils::log_info("  Successfully loaded " + std::to_string(loaded) + " bars for " + symbol);
    } else {
        utils::log_error("  No bars loaded for " + symbol);
    }

    return loaded;
}

bool MockMultiSymbolFeed::parse_csv_line(const std::string& line, Bar& bar) {
    std::istringstream ss(line);
    std::string token;

    try {
        // Format: timestamp,open,high,low,close,volume
        // OR: ts_utc,ts_nyt_epoch,open,high,low,close,volume (7 columns)
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        // Support both 6-column and 7-column formats
        if (tokens.size() == 7) {
            // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert seconds to ms
            bar.open = std::stod(tokens[2]);
            bar.high = std::stod(tokens[3]);
            bar.low = std::stod(tokens[4]);
            bar.close = std::stod(tokens[5]);
            bar.volume = std::stoll(tokens[6]);
        } else if (tokens.size() >= 6) {
            // Format: timestamp,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[0]);
            bar.open = std::stod(tokens[1]);
            bar.high = std::stod(tokens[2]);
            bar.low = std::stod(tokens[3]);
            bar.close = std::stod(tokens[4]);
            bar.volume = std::stoll(tokens[5]);
        } else {
            return false;
        }

        // Set bar_id (not in CSV, use timestamp)
        bar.bar_id = bar.timestamp_ms / 60000;  // Minutes since epoch

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

void MockMultiSymbolFeed::sleep_for_replay(int bars) {
    if (config_.replay_speed <= 0.0) {
        return;  // Instant replay
    }

    // 1 minute real-time = 60000 ms
    // At 39x speed: 60000 / 39 = 1538 ms per bar
    double ms_per_bar = 60000.0 / config_.replay_speed;
    int sleep_ms = static_cast<int>(ms_per_bar * bars);

    if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

} // namespace data
} // namespace sentio
