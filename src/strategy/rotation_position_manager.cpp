#include "strategy/rotation_position_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>

namespace sentio {

RotationPositionManager::RotationPositionManager(const Config& config)
    : config_(config) {

    utils::log_info("RotationPositionManager initialized");
    utils::log_info("  Max positions: " + std::to_string(config_.max_positions));
    utils::log_info("  Min strength to enter: " + std::to_string(config_.min_strength_to_enter));
    utils::log_info("  Min strength to hold: " + std::to_string(config_.min_strength_to_hold));
    utils::log_info("  Rotation delta: " + std::to_string(config_.rotation_strength_delta));
}

std::vector<RotationPositionManager::PositionDecision>
RotationPositionManager::make_decisions(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
    const std::map<std::string, double>& current_prices,
    int current_time_minutes
)  {
    // DIAGNOSTIC: Log every call to make_decisions
    int current_positions = get_position_count();
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    utils::log_info("make_decisions() CALLED - Bar " + std::to_string(current_bar_ + 1) +
                   ", Time: " + std::to_string(current_time_minutes) + "min" +
                   ", Current positions: " + std::to_string(current_positions) +
                   ", Max positions: " + std::to_string(config_.max_positions));
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    std::vector<PositionDecision> decisions;

    current_bar_++;
    stats_.total_decisions++;

    // Update exit cooldowns (decrement all)
    for (auto& [symbol, cooldown] : exit_cooldown_) {
        if (cooldown > 0) cooldown--;
    }

    // Step 1: Check existing positions for exit conditions
    std::set<std::string> symbols_to_exit;

    for (auto& [symbol, position] : positions_) {
        position.bars_held++;

        // Update current price
        if (current_prices.count(symbol) > 0) {
            position.current_price = current_prices.at(symbol);

            // Calculate P&L
            if (position.direction == SignalType::LONG) {
                position.pnl = position.current_price - position.entry_price;
                position.pnl_pct = position.pnl / position.entry_price;
            } else {  // SHORT
                position.pnl = position.entry_price - position.current_price;
                position.pnl_pct = position.pnl / position.entry_price;
            }
        }

        // Update current rank and strength
        const auto* signal = find_signal(symbol, ranked_signals);
        if (signal) {
            position.current_rank = signal->rank;
            position.current_strength = signal->strength;
            utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                           " signal found: rank=" + std::to_string(signal->rank) +
                           ", strength=" + std::to_string(signal->strength));
        } else {
            utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                           " signal NOT found in ranked list (" +
                           std::to_string(ranked_signals.size()) + " signals available)");

            // Don't immediately mark for exit - keep previous rank/strength
            // During cold-start (first 200 bars), don't decay - allow predictor to stabilize
            if (current_bar_ > 200) {
                utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                               " applying decay (post-warmup)");
                // Only decay strength gradually to allow time for signal to return
                position.current_strength *= 0.95;  // 5% decay per bar

                // Only mark for exit if strength decays below hold threshold
                if (position.current_strength < config_.min_strength_to_hold) {
                    position.current_rank = 9999;
                    utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                                   " strength fell below hold threshold -> marking for exit");
                }
            } else {
                utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                               " in warmup period - keeping previous rank/strength unchanged");
            }
            // Otherwise keep previous rank and strength unchanged during warmup
        }

        // Check exit conditions
        Decision decision = check_exit_conditions(position, ranked_signals, current_time_minutes);

        if (decision != Decision::HOLD) {
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = decision;
            pd.position = position;

            switch (decision) {
                case Decision::EXIT:
                    pd.reason = "Rank fell below threshold (" + std::to_string(position.current_rank) + ")";
                    stats_.exits++;
                    break;
                case Decision::PROFIT_TARGET:
                    pd.reason = "Profit target hit (" + std::to_string(position.pnl_pct * 100.0) + "%)";
                    stats_.profit_targets++;
                    break;
                case Decision::STOP_LOSS:
                    pd.reason = "Stop loss hit (" + std::to_string(position.pnl_pct * 100.0) + "%)";
                    stats_.stop_losses++;
                    break;
                case Decision::EOD_EXIT:
                    pd.reason = "End of day liquidation";
                    stats_.eod_exits++;
                    break;
                default:
                    break;
            }

            decisions.push_back(pd);
            symbols_to_exit.insert(symbol);
        } else {
            // HOLD decision
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = Decision::HOLD;
            pd.position = position;
            pd.reason = "Holding (rank=" + std::to_string(position.current_rank) +
                       ", strength=" + std::to_string(position.current_strength) + ")";
            decisions.push_back(pd);
            stats_.holds++;
        }
    }

    // CRITICAL FIX: Don't erase positions here!
    // execute_decision() will erase them after successful execution.
    // If we erase here, execute_decision() will fail because position doesn't exist!

    // Set exit cooldown for exited symbols
    for (const auto& symbol : symbols_to_exit) {
        exit_cooldown_[symbol] = 10;  // 10-bar cooldown after exit (anti-churning)
        utils::log_info("[EXIT DECISION] " + symbol + " marked for exit, cooldown set");
    }

    // Step 2: Consider new entries
    // Re-check position count before entries (may have changed due to exits)
    current_positions = get_position_count();

    // FIX 3: CRITICAL - Enforce max_positions hard limit
    if (current_positions >= config_.max_positions) {
        utils::log_info("╔══════════════════════════════════════════════════════════╗");
        utils::log_info("║ MAX POSITIONS REACHED - BLOCKING NEW ENTRIES            ║");
        utils::log_info("║ Current: " + std::to_string(current_positions) +
                       " / Max: " + std::to_string(config_.max_positions) + "                                      ║");
        utils::log_info("║ Returning " + std::to_string(decisions.size()) + " decisions (exits/holds only)              ║");
        utils::log_info("╚══════════════════════════════════════════════════════════╝");
        return decisions;  // Skip entire entry section
    }

    int available_slots = config_.max_positions - current_positions;

    // CRITICAL FIX: Prevent new entries near EOD to avoid immediate liquidation
    int bars_until_eod = config_.eod_exit_time_minutes - current_time_minutes;
    if (bars_until_eod <= 30 && available_slots > 0) {
        utils::log_info("╔══════════════════════════════════════════════════════════╗");
        utils::log_info("║ NEAR EOD - BLOCKING NEW ENTRIES                         ║");
        utils::log_info("║ Bars until EOD: " + std::to_string(bars_until_eod) +
                       " (< 30 bar minimum hold)                     ║");
        utils::log_info("║ Returning " + std::to_string(decisions.size()) + " decisions (exits/holds only)              ║");
        utils::log_info("╚══════════════════════════════════════════════════════════╝");
        available_slots = 0;  // Block all new entries
    }

    if (available_slots > 0) {
        // Find top signals not currently held
        for (const auto& ranked_signal : ranked_signals) {
            if (available_slots <= 0) {
                break;
            }

            const auto& symbol = ranked_signal.symbol;

            // Skip if already have position
            if (has_position(symbol)) {
                continue;
            }

            // Skip if in rotation cooldown
            if (rotation_cooldown_.count(symbol) > 0 && rotation_cooldown_[symbol] > 0) {
                rotation_cooldown_[symbol]--;
                continue;
            }

            // Skip if in exit cooldown (anti-churning)
            if (exit_cooldown_.count(symbol) > 0 && exit_cooldown_[symbol] > 0) {
                continue;  // Don't re-enter immediately after exit
            }

            // Check minimum strength
            if (ranked_signal.strength < config_.min_strength_to_enter) {
                break;  // Signals are sorted, so no point checking further
            }

            // Check minimum rank
            if (ranked_signal.rank > config_.min_rank_to_hold) {
                break;
            }

            // FIX 4: Enhanced position entry validation
            if (current_prices.count(symbol) == 0) {
                utils::log_error("[ENTRY VALIDATION] No price for " + symbol + " - cannot enter position");
                utils::log_error("  Available prices for " + std::to_string(current_prices.size()) + " symbols:");

                // List available symbols for debugging
                int count = 0;
                for (const auto& [sym, price] : current_prices) {
                    if (count++ < 10) {  // Show first 10
                        utils::log_error("    " + sym + " @ $" + std::to_string(price));
                    }
                }
                continue;
            }

            // Validate price is reasonable
            double price = current_prices.at(symbol);
            if (price <= 0.0 || price > 1000000.0) {  // Sanity check
                utils::log_error("[ENTRY VALIDATION] Invalid price for " + symbol + ": $" +
                               std::to_string(price) + " - skipping");
                continue;
            }

            // Enter position
            PositionDecision pd;
            pd.symbol = symbol;
            pd.decision = (ranked_signal.signal.signal_type == SignalType::LONG) ?
                         Decision::ENTER_LONG : Decision::ENTER_SHORT;
            pd.signal = ranked_signal;
            pd.reason = "Entering (rank=" + std::to_string(ranked_signal.rank) +
                       ", strength=" + std::to_string(ranked_signal.strength) + ")";

            utils::log_info("[ENTRY] " + symbol + " @ $" + std::to_string(price) +
                          " (rank=" + std::to_string(ranked_signal.rank) +
                          ", strength=" + std::to_string(ranked_signal.strength) + ")");

            utils::log_info(">>> ADDING ENTRY DECISION: " + symbol +
                          " (decision #" + std::to_string(decisions.size() + 1) + ")" +
                          ", available_slots=" + std::to_string(available_slots) +
                          " -> " + std::to_string(available_slots - 1));

            decisions.push_back(pd);
            stats_.entries++;

            available_slots--;
        }
    }

    // Step 3: Check if rotation needed (better signal available)
    if (available_slots == 0 && should_rotate(ranked_signals)) {
        // Find weakest current position
        std::string weakest = find_weakest_position();

        if (!weakest.empty()) {
            // Find strongest non-held signal
            for (const auto& ranked_signal : ranked_signals) {
                if (has_position(ranked_signal.symbol)) {
                    continue;
                }

                // Check if significantly stronger
                auto& weakest_pos = positions_.at(weakest);
                double strength_delta = ranked_signal.strength - weakest_pos.current_strength;

                if (strength_delta >= config_.rotation_strength_delta) {
                    // Rotate out weakest
                    PositionDecision exit_pd;
                    exit_pd.symbol = weakest;
                    exit_pd.decision = Decision::ROTATE_OUT;
                    exit_pd.position = weakest_pos;
                    exit_pd.reason = "Rotating out for stronger signal (" +
                                    ranked_signal.symbol + ", delta=" +
                                    std::to_string(strength_delta) + ")";
                    decisions.push_back(exit_pd);
                    stats_.rotations++;

                    // CRITICAL FIX: Don't erase here! Let execute_decision() handle it.
                    // positions_.erase(weakest);  // ← REMOVED
                    utils::log_info("[ROTATION] " + weakest + " marked for rotation out");

                    // Enter new position
                    PositionDecision enter_pd;
                    enter_pd.symbol = ranked_signal.symbol;
                    enter_pd.decision = (ranked_signal.signal.signal_type == SignalType::LONG) ?
                                       Decision::ENTER_LONG : Decision::ENTER_SHORT;
                    enter_pd.signal = ranked_signal;
                    enter_pd.reason = "Entering via rotation (rank=" +
                                     std::to_string(ranked_signal.rank) +
                                     ", strength=" + std::to_string(ranked_signal.strength) + ")";
                    decisions.push_back(enter_pd);
                    stats_.entries++;

                    // Set cooldown for rotated symbol
                    rotation_cooldown_[weakest] = config_.rotation_cooldown_bars;

                    break;  // Only rotate one per bar
                }
            }
        }
    }

    // DIAGNOSTIC: Log all decisions being returned
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    utils::log_info("make_decisions() RETURNING " + std::to_string(decisions.size()) + " decisions:");
    for (size_t i = 0; i < decisions.size(); i++) {
        const auto& d = decisions[i];
        std::string decision_type;
        switch (d.decision) {
            case Decision::ENTER_LONG: decision_type = "ENTER_LONG"; break;
            case Decision::ENTER_SHORT: decision_type = "ENTER_SHORT"; break;
            case Decision::EXIT: decision_type = "EXIT"; break;
            case Decision::HOLD: decision_type = "HOLD"; break;
            case Decision::ROTATE_OUT: decision_type = "ROTATE_OUT"; break;
            case Decision::PROFIT_TARGET: decision_type = "PROFIT_TARGET"; break;
            case Decision::STOP_LOSS: decision_type = "STOP_LOSS"; break;
            case Decision::EOD_EXIT: decision_type = "EOD_EXIT"; break;
            default: decision_type = "UNKNOWN"; break;
        }
        utils::log_info("  [" + std::to_string(i+1) + "] " + d.symbol + ": " + decision_type);
    }
    utils::log_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    return decisions;
}

bool RotationPositionManager::execute_decision(
    const PositionDecision& decision,
    double execution_price
) {
    switch (decision.decision) {
        case Decision::ENTER_LONG:
        case Decision::ENTER_SHORT:
            {
                Position pos;
                pos.symbol = decision.symbol;
                pos.direction = decision.signal.signal.signal_type;
                pos.entry_price = execution_price;
                pos.current_price = execution_price;
                pos.pnl = 0.0;
                pos.pnl_pct = 0.0;
                pos.bars_held = 0;
                pos.entry_rank = decision.signal.rank;
                pos.current_rank = decision.signal.rank;
                pos.entry_strength = decision.signal.strength;
                pos.current_strength = decision.signal.strength;
                pos.entry_timestamp_ms = decision.signal.signal.timestamp_ms;

                positions_[decision.symbol] = pos;

                utils::log_info("Entered " + decision.symbol + " " +
                              (pos.direction == SignalType::LONG ? "LONG" : "SHORT") +
                              " @ " + std::to_string(execution_price));
                return true;
            }

        case Decision::EXIT:
        case Decision::ROTATE_OUT:
        case Decision::PROFIT_TARGET:
        case Decision::STOP_LOSS:
        case Decision::EOD_EXIT:
            {
                if (positions_.count(decision.symbol) > 0) {
                    auto& pos = positions_.at(decision.symbol);

                    // Calculate final P&L
                    double final_pnl_pct = 0.0;
                    if (pos.direction == SignalType::LONG) {
                        final_pnl_pct = (execution_price - pos.entry_price) / pos.entry_price;
                    } else {
                        final_pnl_pct = (pos.entry_price - execution_price) / pos.entry_price;
                    }

                    utils::log_info("Exited " + decision.symbol + " " +
                                  (pos.direction == SignalType::LONG ? "LONG" : "SHORT") +
                                  " @ " + std::to_string(execution_price) +
                                  " (P&L: " + std::to_string(final_pnl_pct * 100.0) + "%, " +
                                  "bars: " + std::to_string(pos.bars_held) + ")");

                    // Update stats
                    stats_.avg_bars_held = (stats_.avg_bars_held * stats_.exits + pos.bars_held) /
                                          (stats_.exits + 1);
                    stats_.avg_pnl_pct = (stats_.avg_pnl_pct * stats_.exits + final_pnl_pct) /
                                        (stats_.exits + 1);

                    // CRITICAL FIX: Always erase after successful exit execution
                    // (Old code had special case for ROTATE_OUT, but that was part of the bug)
                    positions_.erase(decision.symbol);
                    utils::log_info("[EXECUTED EXIT] " + decision.symbol + " removed from positions");

                    return true;
                }
                return false;
            }

        case Decision::HOLD:
            // Nothing to do
            return true;

        default:
            return false;
    }
}

void RotationPositionManager::update_prices(
    const std::map<std::string, double>& current_prices
) {
    for (auto& [symbol, position] : positions_) {
        if (current_prices.count(symbol) > 0) {
            position.current_price = current_prices.at(symbol);

            // Update P&L
            if (position.direction == SignalType::LONG) {
                position.pnl = position.current_price - position.entry_price;
                position.pnl_pct = position.pnl / position.entry_price;
            } else {
                position.pnl = position.entry_price - position.current_price;
                position.pnl_pct = position.pnl / position.entry_price;
            }
        }
    }
}

double RotationPositionManager::get_total_unrealized_pnl() const {
    double total = 0.0;
    for (const auto& [symbol, position] : positions_) {
        total += position.pnl;
    }
    return total;
}

// === Private Methods ===

RotationPositionManager::Decision RotationPositionManager::check_exit_conditions(
    const Position& position,
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
    int current_time_minutes
) {
    // CRITICAL: Enforce minimum holding period to prevent churning
    if (position.bars_held < position.minimum_hold_bars) {
        // Only allow exit for critical conditions during minimum hold
        if (config_.enable_stop_loss && position.pnl_pct <= -config_.stop_loss_pct) {
            return Decision::STOP_LOSS;  // Allow stop loss
        }
        if (config_.eod_liquidation && current_time_minutes >= config_.eod_exit_time_minutes) {
            return Decision::EOD_EXIT;  // Allow EOD exit
        }
        return Decision::HOLD;  // Force hold otherwise
    }

    // Check EOD exit
    if (config_.eod_liquidation && current_time_minutes >= config_.eod_exit_time_minutes) {
        return Decision::EOD_EXIT;
    }

    // Check profit target
    if (config_.enable_profit_target && position.pnl_pct >= config_.profit_target_pct) {
        return Decision::PROFIT_TARGET;
    }

    // Check stop loss
    if (config_.enable_stop_loss && position.pnl_pct <= -config_.stop_loss_pct) {
        return Decision::STOP_LOSS;
    }

    // Check if rank fell below threshold
    if (position.current_rank > config_.min_rank_to_hold) {
        return Decision::EXIT;
    }

    // HYSTERESIS: Use different threshold for exit vs hold
    // This creates a "dead zone" to prevent oscillation
    double exit_threshold = config_.min_strength_to_exit;  // Lower than entry threshold
    if (position.current_strength < exit_threshold) {
        return Decision::EXIT;
    }

    return Decision::HOLD;
}

const SignalAggregator::RankedSignal* RotationPositionManager::find_signal(
    const std::string& symbol,
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) const {
    for (const auto& rs : ranked_signals) {
        if (rs.symbol == symbol) {
            return &rs;
        }
    }
    return nullptr;
}

bool RotationPositionManager::should_rotate(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) {
    // Only rotate if at max capacity
    if (get_position_count() < config_.max_positions) {
        return false;
    }

    // Find strongest non-held signal
    for (const auto& ranked_signal : ranked_signals) {
        if (!has_position(ranked_signal.symbol)) {
            // Check if significantly stronger than weakest position
            std::string weakest = find_weakest_position();
            if (!weakest.empty()) {
                auto& weakest_pos = positions_.at(weakest);
                double strength_delta = ranked_signal.strength - weakest_pos.current_strength;

                return (strength_delta >= config_.rotation_strength_delta);
            }
        }
    }

    return false;
}

std::string RotationPositionManager::find_weakest_position() const {
    if (positions_.empty()) {
        return "";
    }

    std::string weakest;
    double min_strength = std::numeric_limits<double>::max();

    for (const auto& [symbol, position] : positions_) {
        if (position.current_strength < min_strength) {
            min_strength = position.current_strength;
            weakest = symbol;
        }
    }

    return weakest;
}

} // namespace sentio
