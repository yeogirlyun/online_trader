#pragma once

#include <string>
#include <optional>

namespace sentio {

/**
 * @brief EOD liquidation status
 */
enum class EodStatus {
    PENDING,      // No EOD action taken yet today
    IN_PROGRESS,  // EOD liquidation in progress
    DONE          // EOD liquidation completed
};

/**
 * @brief EOD state for a trading day
 */
struct EodState {
    EodStatus status{EodStatus::PENDING};
    std::string positions_hash;  // Hash of positions when DONE (for verification)
    int64_t last_attempt_epoch{0};  // Unix timestamp (seconds) of last liquidation attempt
};

/**
 * @brief Persistent state tracking for End-of-Day (EOD) liquidation
 *
 * Ensures idempotent EOD execution - prevents double liquidation on process restart
 * and enables detection of missed EOD events.
 *
 * State is persisted to a simple text file containing the last ET date (YYYY-MM-DD)
 * for which EOD liquidation was completed.
 */
class EodStateStore {
public:
    /**
     * @brief Construct state store with file path
     * @param state_file_path Full path to state file (e.g., "/tmp/sentio_eod_state.txt")
     */
    explicit EodStateStore(std::string state_file_path);

    /**
     * @brief Get the ET date (YYYY-MM-DD) of the last completed EOD
     * @return ET date string if available, nullopt if no EOD recorded
     */
    std::optional<std::string> last_eod_date() const;

    /**
     * @brief Mark EOD liquidation as complete for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     *
     * Atomically writes the date to the state file, overwriting previous value.
     * This ensures exactly-once semantics for EOD execution per trading day.
     */
    void mark_eod_complete(const std::string& et_date);

    /**
     * @brief Check if EOD already completed for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return true if EOD already done for this date
     */
    bool is_eod_complete(const std::string& et_date) const;

    /**
     * @brief Load EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @return EodState (PENDING if no state exists)
     */
    EodState load(const std::string& et_date) const;

    /**
     * @brief Save EOD state for given ET date
     * @param et_date ET date in YYYY-MM-DD format
     * @param state EOD state to save
     */
    void save(const std::string& et_date, const EodState& state);

private:
    std::string state_file_;
};

} // namespace sentio
