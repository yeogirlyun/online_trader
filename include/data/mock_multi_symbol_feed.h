#pragma once

#include "data/bar_feed_interface.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <thread>
#include <atomic>

namespace sentio {
namespace data {

/**
 * @brief Mock data feed for testing - replays historical CSV data
 *
 * Reads CSV files for multiple symbols and replays them synchronously.
 * Useful for backtesting and integration testing.
 *
 * CSV Format (per symbol):
 *   timestamp,open,high,low,close,volume
 *   1696464600000,432.15,432.89,431.98,432.56,12345678
 *
 * Usage:
 *   MockMultiSymbolFeed feed(data_manager, {
 *       {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
 *       {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"}
 *   });
 *   feed.connect();  // Load CSV files
 *   feed.start();    // Start replay in background thread
 */
class MockMultiSymbolFeed : public IBarFeed {
public:
    struct Config {
        std::map<std::string, std::string> symbol_files;  // Symbol → CSV path
        double replay_speed = 39.0;                       // Speed multiplier (0=instant)
        bool loop = false;                                // Loop replay?
        bool sync_timestamps = true;                      // Synchronize timestamps?
        std::string filter_date;                          // Filter to specific date (YYYY-MM-DD, empty = all)
    };

    /**
     * @brief Construct mock feed
     *
     * @param data_manager Data manager to feed
     * @param config Configuration
     */
    MockMultiSymbolFeed(std::shared_ptr<MultiSymbolDataManager> data_manager,
                       const Config& config);

    ~MockMultiSymbolFeed() override;

    // === IBarFeed Interface ===

    /**
     * @brief Connect to data source (loads CSV files)
     * @return true if all CSV files loaded successfully
     */
    bool connect() override;

    /**
     * @brief Disconnect from data source
     */
    void disconnect() override;

    /**
     * @brief Check if connected (data loaded)
     */
    bool is_connected() const override;

    /**
     * @brief Start feeding data (begins replay in background thread)
     * @return true if started successfully
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

    // === Additional Mock-Specific Methods ===

    /**
     * @brief Get total bars loaded per symbol
     */
    std::map<std::string, int> get_bar_counts() const;

    /**
     * @brief Get replay progress
     */
    struct Progress {
        int bars_replayed;
        int total_bars;
        double progress_pct;
        std::string current_symbol;
        uint64_t current_timestamp;
    };

    Progress get_progress() const;

private:
    /**
     * @brief Load CSV file for a symbol
     *
     * @param symbol Symbol ticker
     * @param filepath Path to CSV file
     * @return Number of bars loaded
     */
    int load_csv(const std::string& symbol, const std::string& filepath);

    /**
     * @brief Parse CSV line into Bar
     *
     * @param line CSV line
     * @param bar Output bar
     * @return true if parsed successfully
     */
    bool parse_csv_line(const std::string& line, Bar& bar);

    /**
     * @brief Sleep for replay speed
     *
     * @param bars Number of bars to sleep for (1 = 1 minute real-time)
     */
    void sleep_for_replay(int bars = 1);

    /**
     * @brief Replay loop (runs in background thread)
     */
    void replay_loop();

    /**
     * @brief Replay single bar for all symbols
     * @return true if bars available, false if EOF
     */
    bool replay_next_bar();

    std::shared_ptr<MultiSymbolDataManager> data_manager_;
    Config config_;

    // Data storage
    struct SymbolData {
        std::deque<Bar> bars;
        size_t current_index;

        SymbolData() : current_index(0) {}
    };

    std::map<std::string, SymbolData> symbol_data_;

    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> should_stop_{false};

    // Background thread
    std::thread replay_thread_;

    // Callbacks
    BarCallback bar_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;

    // Replay state
    std::atomic<int> bars_replayed_{0};
    int total_bars_{0};
    std::atomic<int> errors_{0};
};

} // namespace data
} // namespace sentio
