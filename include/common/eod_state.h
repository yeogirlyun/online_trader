#pragma once

#include <string>
#include <optional>

namespace sentio {

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

private:
    std::string state_file_;
};

} // namespace sentio
