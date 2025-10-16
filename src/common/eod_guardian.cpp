#include "common/eod_guardian.h"
#include "common/exceptions.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace sentio {

EodGuardian::EodGuardian(AlpacaClient& alpaca,
                         EodStateStore& state_store,
                         ETTimeManager& time_mgr,
                         PositionBook& position_book)
    : alpaca_(alpaca)
    , state_store_(state_store)
    , time_mgr_(time_mgr)
    , position_book_(position_book)
    , current_et_date_(time_mgr_.get_current_et_date())
    , current_state_(state_store_.load(current_et_date_)) {
}

void EodGuardian::tick() {
    // Refresh state if day changed
    refresh_state_if_needed();

    // Calculate decision
    EodDecision decision = calc_eod_decision();

    // Log decision (only when in window or status changes)
    if (decision.in_window || decision.should_liquidate) {
        log_decision(decision);
    }

    // Execute if needed
    if (decision.should_liquidate && !liquidation_in_progress_) {
        execute_eod_liquidation();
    }
}

void EodGuardian::force_liquidate() {
    std::cout << "[EodGuardian] FORCE LIQUIDATE requested" << std::endl;
    execute_eod_liquidation();
}

EodState EodGuardian::get_state() const {
    return current_state_;
}

bool EodGuardian::is_eod_complete() const {
    return current_state_.status == EodStatus::DONE && position_book_.is_flat();
}

EodDecision EodGuardian::calc_eod_decision() const {
    EodDecision decision;

    // Check if we're in EOD window (3:55-4:00 PM ET)
    decision.in_window = time_mgr_.is_eod_liquidation_window();
    decision.has_positions = !position_book_.is_flat();

    // Safety-first rule: If in window AND have positions, liquidate
    if (decision.in_window && decision.has_positions) {
        decision.should_liquidate = true;
        decision.reason = "In EOD window with open positions - LIQUIDATE";
        return decision;
    }

    // If in window but flat, check if we need to mark DONE
    if (decision.in_window && !decision.has_positions) {
        if (current_state_.status != EodStatus::DONE) {
            decision.should_liquidate = true;  // Will just mark DONE
            decision.reason = "In EOD window, already flat - mark DONE";
        } else {
            decision.should_liquidate = false;
            decision.reason = "In EOD window, flat, already marked DONE";
        }
        return decision;
    }

    // Not in window
    decision.should_liquidate = false;
    decision.reason = "Not in EOD window";
    return decision;
}

void EodGuardian::execute_eod_liquidation() {
    liquidation_in_progress_ = true;

    try {
        std::cout << "[EodGuardian] === EXECUTING EOD LIQUIDATION ===" << std::endl;

        // Step 1: Mark as IN_PROGRESS
        current_state_.status = EodStatus::IN_PROGRESS;
        current_state_.last_attempt_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_store_.save(current_et_date_, current_state_);
        std::cout << "[EodGuardian] State marked IN_PROGRESS" << std::endl;

        // Step 2: Cancel all open orders
        std::cout << "[EodGuardian] Cancelling all open orders..." << std::endl;
        alpaca_.cancel_all_orders();

        // Step 3: Flatten all positions (if any)
        if (!position_book_.is_flat()) {
            std::cout << "[EodGuardian] Flattening all positions..." << std::endl;
            alpaca_.close_all_positions();

            // Wait for fills (up to 3 seconds)
            for (int i = 0; i < 30; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (position_book_.is_flat()) {
                    break;
                }
            }
        }

        // Step 4: Verify flatness
        verify_flatness();
        std::cout << "[EodGuardian] ✓ Verified flat" << std::endl;

        // Step 5: Calculate position hash (should be empty for flat book)
        std::string hash = position_book_.positions_hash();
        if (!hash.empty()) {
            throw std::runtime_error("Position hash non-empty after liquidation");
        }

        // Step 6: Mark DONE
        current_state_.status = EodStatus::DONE;
        current_state_.positions_hash = hash;
        current_state_.last_attempt_epoch = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_store_.save(current_et_date_, current_state_);

        std::cout << "[EodGuardian] ✓ EOD liquidation complete for " << current_et_date_ << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[EodGuardian] ERROR during liquidation: " << e.what() << std::endl;
        liquidation_in_progress_ = false;
        throw;
    }

    liquidation_in_progress_ = false;
}

void EodGuardian::verify_flatness() const {
    if (!position_book_.is_flat()) {
        auto positions = position_book_.get_all_positions();
        std::cerr << "[EodGuardian] FLATNESS VERIFICATION FAILED:" << std::endl;
        for (const auto& [symbol, pos] : positions) {
            std::cerr << "  " << symbol << ": " << pos.qty << " shares" << std::endl;
        }
        throw std::runtime_error("EOD liquidation failed - positions still open");
    }
}

void EodGuardian::refresh_state_if_needed() {
    std::string today = time_mgr_.get_current_et_date();
    if (today != current_et_date_) {
        std::cout << "[EodGuardian] Day changed: " << current_et_date_
                  << " → " << today << std::endl;
        current_et_date_ = today;
        current_state_ = state_store_.load(current_et_date_);
        liquidation_in_progress_ = false;
    }
}

void EodGuardian::log_decision(const EodDecision& decision) const {
    std::cout << "[EodGuardian] in_window=" << decision.in_window
              << " has_pos=" << decision.has_positions
              << " should_liq=" << decision.should_liquidate
              << " | " << decision.reason << std::endl;
}

} // namespace sentio
