#ifndef SENTIO_MOCK_BAR_FEED_REPLAY_H
#define SENTIO_MOCK_BAR_FEED_REPLAY_H

#include "live/bar_feed_interface.h"
#include <deque>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace sentio {

/**
 * Mock Bar Feed with Replay Capability
 *
 * Replays historical bar data with precise time synchronization:
 * - Drift-free timing using absolute time anchors
 * - Configurable speed multiplier (1x = real-time, 39x = accelerated)
 * - Multi-symbol support
 * - Thread-safe bar delivery
 */
class MockBarFeedReplay : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param csv_file Path to CSV file with historical bars
     * @param speed_multiplier Replay speed (1.0 = real-time, 39.0 = 39x speed)
     */
    explicit MockBarFeedReplay(const std::string& csv_file, double speed_multiplier = 1.0);

    ~MockBarFeedReplay() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

    // Mock-specific methods

    /**
     * Load bars from CSV file
     * Format: timestamp,open,high,low,close,volume
     */
    bool load_csv(const std::string& csv_file);

    /**
     * Add bar programmatically (for testing)
     */
    void add_bar(const std::string& symbol, const Bar& bar);

    /**
     * Set speed multiplier (can be changed during replay)
     */
    void set_speed_multiplier(double multiplier);

    /**
     * Get current replay progress
     */
    struct ReplayProgress {
        size_t total_bars;
        size_t current_index;
        double progress_pct;
        uint64_t current_bar_timestamp_ms;
        std::string current_bar_time_str;
    };

    ReplayProgress get_progress() const;

    /**
     * Check if replay is complete
     */
    bool is_replay_complete() const;

    /**
     * Data validation
     */
    bool validate_data_integrity() const;

private:
    using Clock = std::chrono::steady_clock;

    // Bar data (symbol -> bars)
    std::map<std::string, std::vector<Bar>> bars_by_symbol_;
    std::vector<std::string> subscribed_symbols_;

    // Replay state
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<size_t> current_index_;
    double speed_multiplier_;

    // Time synchronization (drift-free)
    Clock::time_point replay_start_real_;
    uint64_t replay_start_market_ms_;

    // Thread management
    std::unique_ptr<std::thread> replay_thread_;
    BarCallback callback_;

    // Recent bars cache (for get_recent_bars)
    mutable std::mutex bars_mutex_;
    std::map<std::string, std::deque<Bar>> bars_history_;
    static constexpr size_t MAX_BARS_HISTORY = 1000;

    // Health monitoring
    std::atomic<Clock::time_point> last_message_time_;

    // Replay loop
    void replay_loop();

    // Helper methods
    void store_bar(const std::string& symbol, const Bar& bar);
    std::optional<Bar> get_next_bar(std::string& out_symbol);
    void wait_until_bar_time(const Bar& bar);
};

} // namespace sentio

#endif // SENTIO_MOCK_BAR_FEED_REPLAY_H
