#pragma once

#include "common/eod_state.h"
#include "common/time_utils.h"
#include "live/position_book.h"
#include "live/alpaca_client.hpp"
#include <memory>
#include <string>

namespace sentio {

/**
 * @brief EOD liquidation decision
 */
struct EodDecision {
    bool in_window{false};          // Are we in EOD liquidation window?
    bool has_positions{false};       // Does PositionBook have open positions?
    bool should_liquidate{false};    // Final decision: execute liquidation?
    std::string reason;              // Human-readable reason for decision
};

/**
 * @brief Production-grade EOD Guardian subsystem
 *
 * Safety-first design principles:
 * 1. Idempotency is anchored to FACTS (flatness), not file flags
 * 2. Always liquidate if (in_window AND has_positions)
 * 3. Position hash verification prevents stale state bugs
 * 4. Atomic state updates with status tracking
 * 5. Fail-safe: If uncertain, liquidate
 *
 * State Machine:
 *   PENDING → IN_PROGRESS → DONE
 *   ↑__________↓ (new positions opened after DONE)
 *
 * Usage:
 *   EodGuardian guardian(broker, calendar, state_store, time_mgr, position_book);
 *   // In main trading loop:
 *   guardian.tick();  // Call every heartbeat
 */
class EodGuardian {
public:
    /**
     * @brief Construct EOD Guardian
     * @param alpaca Alpaca broker client for order execution
     * @param state_store Persistent EOD state storage
     * @param time_mgr ET time manager
     * @param position_book Position tracking
     */
    EodGuardian(AlpacaClient& alpaca,
                EodStateStore& state_store,
                ETTimeManager& time_mgr,
                PositionBook& position_book);

    /**
     * @brief Main entry point - call every heartbeat
     *
     * This method:
     * 1. Checks if we're in EOD window (3:55-4:00 PM ET)
     * 2. Checks if positions are open
     * 3. Decides whether to liquidate
     * 4. Executes liquidation if needed
     * 5. Updates state atomically
     */
    void tick();

    /**
     * @brief Force liquidation (for testing/manual override)
     */
    void force_liquidate();

    /**
     * @brief Get current EOD state
     */
    EodState get_state() const;

    /**
     * @brief Check if EOD is complete for today
     * @return true if status == DONE and positions are flat
     */
    bool is_eod_complete() const;

private:
    AlpacaClient& alpaca_;
    EodStateStore& state_store_;
    ETTimeManager& time_mgr_;
    PositionBook& position_book_;

    std::string current_et_date_;
    EodState current_state_;
    bool liquidation_in_progress_{false};

    /**
     * @brief Calculate EOD decision based on current state
     * @return EodDecision with liquidation decision and reason
     */
    EodDecision calc_eod_decision() const;

    /**
     * @brief Execute EOD liquidation
     *
     * Steps:
     * 1. Mark state as IN_PROGRESS
     * 2. Cancel all open orders
     * 3. Flatten all positions
     * 4. Verify flatness
     * 5. Calculate position hash
     * 6. Mark state as DONE with hash
     */
    void execute_eod_liquidation();

    /**
     * @brief Verify positions are flat
     * @throws std::runtime_error if not flat
     */
    void verify_flatness() const;

    /**
     * @brief Update current date and reload state if day changed
     */
    void refresh_state_if_needed();

    /**
     * @brief Log EOD decision (for debugging)
     */
    void log_decision(const EodDecision& decision) const;
};

} // namespace sentio
