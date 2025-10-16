#ifndef SENTIO_STATE_PERSISTENCE_H
#define SENTIO_STATE_PERSISTENCE_H

#include <string>
#include <optional>
#include <mutex>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "backend/position_state_machine.h"

namespace sentio {

/**
 * StatePersistence - Atomic state persistence for exact position recovery
 *
 * Provides crash-safe state persistence with:
 * - Atomic writes with backup rotation
 * - SHA256 checksum validation
 * - Multi-level recovery (primary → backup → timestamped)
 * - Exact bars_held tracking across restarts
 *
 * Usage:
 *   auto persistence = std::make_unique<StatePersistence>(log_dir + "/state");
 *
 *   // Save after every N bars and after state transitions
 *   persistence->save_state(current_state);
 *
 *   // Load on startup
 *   if (auto state = persistence->load_state()) {
 *       // Restore exact state
 *   }
 */
class StatePersistence {
public:
    struct PositionDetail {
        std::string symbol;
        double quantity;
        double avg_entry_price;
        uint64_t entry_timestamp;
    };

    struct TradingState {
        // Core PSM state
        PositionStateMachine::State psm_state;
        int bars_held;
        double entry_equity;
        uint64_t last_bar_timestamp;
        std::string last_bar_time_str;

        // Position details (for validation against broker)
        std::vector<PositionDetail> positions;

        // Metadata
        std::string session_id;
        uint64_t save_timestamp;
        int save_count;
        std::string checksum;

        // Serialization
        nlohmann::json to_json() const;
        static TradingState from_json(const nlohmann::json& j);

        // Integrity
        std::string calculate_checksum() const;
        bool validate_checksum() const;
    };

    explicit StatePersistence(const std::string& state_dir);

    // Save state atomically with backup
    bool save_state(const TradingState& state);

    // Load state with validation and fallback
    std::optional<TradingState> load_state();

    // Emergency recovery from corrupted state
    std::optional<TradingState> recover_from_backup();

    // Clean old backup files (keep last N)
    void cleanup_old_backups(int keep_count = 5);

private:
    std::string state_dir_;
    std::string primary_file_;
    std::string backup_file_;
    std::string temp_file_;
    std::string lock_file_;
    mutable std::mutex mutex_;
    mutable int lock_fd_;

    bool write_atomic(const std::string& filepath, const nlohmann::json& data);
    std::optional<TradingState> load_from_file(const std::string& filepath);
    std::string generate_backup_filename() const;

    // File locking for cross-process safety
    bool acquire_file_lock(int timeout_ms = 1000);
    void release_file_lock();
};

} // namespace sentio

#endif // SENTIO_STATE_PERSISTENCE_H
