#include "backend/rotation_trading_backend.h"
#include "common/utils.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>

using json = nlohmann::json;

namespace sentio {

RotationTradingBackend::RotationTradingBackend(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr,
    std::shared_ptr<AlpacaClient> broker
)
    : config_(config)
    , data_manager_(data_mgr)
    , broker_(broker)
    , current_cash_(config.starting_capital) {

    utils::log_info("========================================");
    utils::log_info("RotationTradingBackend Initializing");
    utils::log_info("========================================");

    // Create data manager if not provided
    if (!data_manager_) {
        data::MultiSymbolDataManager::Config dm_config = config_.data_config;
        dm_config.symbols = config_.symbols;
        data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);
        utils::log_info("Created MultiSymbolDataManager");
    }

    // Create OES manager
    MultiSymbolOESManager::Config oes_config;
    oes_config.symbols = config_.symbols;
    oes_config.base_config = config_.oes_config;
    oes_manager_ = std::make_unique<MultiSymbolOESManager>(oes_config, data_manager_);
    utils::log_info("Created MultiSymbolOESManager");

    // Create signal aggregator
    signal_aggregator_ = std::make_unique<SignalAggregator>(config_.aggregator_config);
    utils::log_info("Created SignalAggregator");

    // Create rotation manager
    rotation_manager_ = std::make_unique<RotationPositionManager>(config_.rotation_config);
    utils::log_info("Created RotationPositionManager");

    utils::log_info("Symbols: " + std::to_string(config_.symbols.size()));
    utils::log_info("Starting capital: $" + std::to_string(config_.starting_capital));
    utils::log_info("Max positions: " + std::to_string(config_.rotation_config.max_positions));
    utils::log_info("Backend initialization complete");
}

RotationTradingBackend::~RotationTradingBackend() {
    if (trading_active_) {
        stop_trading();
    }
}

// === Trading Session Management ===

bool RotationTradingBackend::warmup(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("========================================");
    utils::log_info("Warmup Phase");
    utils::log_info("========================================");
    std::cout << "[Backend] Starting warmup with " << symbol_bars.size() << " symbols" << std::endl;

    // Log warmup data sizes
    for (const auto& [symbol, bars] : symbol_bars) {
        utils::log_info("  " + symbol + ": " + std::to_string(bars.size()) + " warmup bars");
        std::cout << "[Backend]   " << symbol << ": " << bars.size() << " warmup bars" << std::endl;
    }

    bool success = oes_manager_->warmup_all(symbol_bars);

    // Check individual readiness
    auto ready_status = oes_manager_->get_ready_status();
    for (const auto& [symbol, is_ready] : ready_status) {
        utils::log_info("  " + symbol + ": " + (is_ready ? "READY" : "NOT READY"));
        std::cout << "[Backend]   " << symbol << ": " << (is_ready ? "READY" : "NOT READY") << std::endl;
    }

    if (success) {
        utils::log_info("✓ Warmup complete - all OES instances ready");
        std::cout << "[Backend] ✓ Warmup complete - all OES instances ready" << std::endl;
    } else {
        utils::log_error("Warmup failed - some OES instances not ready");
        std::cout << "[Backend] ❌ Warmup failed - some OES instances not ready" << std::endl;
    }

    return success;
}

bool RotationTradingBackend::start_trading() {
    utils::log_info("========================================");
    utils::log_info("Starting Trading Session");
    utils::log_info("========================================");
    std::cout << "[Backend] Checking if ready..." << std::endl;

    // Check if ready
    if (!is_ready()) {
        utils::log_error("Cannot start trading - backend not ready");
        std::cout << "[Backend] ❌ Cannot start trading - backend not ready" << std::endl;

        // Debug: Check which OES instances are not ready
        auto ready_status = oes_manager_->get_ready_status();
        for (const auto& [symbol, is_ready] : ready_status) {
            if (!is_ready) {
                utils::log_error("  " + symbol + " is NOT READY");
                std::cout << "[Backend]   " << symbol << " is NOT READY" << std::endl;
            }
        }

        return false;
    }

    std::cout << "[Backend] ✓ Backend is ready" << std::endl;

    // Open log files
    std::cout << "[Backend] Opening log files..." << std::endl;

    if (config_.log_all_signals) {
        std::cout << "[Backend]   Signal log: " << config_.signal_log_path << std::endl;
        signal_log_.open(config_.signal_log_path, std::ios::out | std::ios::trunc);
        if (!signal_log_.is_open()) {
            utils::log_error("Failed to open signal log: " + config_.signal_log_path);
            std::cout << "[Backend] ❌ Failed to open signal log: " << config_.signal_log_path << std::endl;
            return false;
        }
    }

    if (config_.log_all_decisions) {
        std::cout << "[Backend]   Decision log: " << config_.decision_log_path << std::endl;
        decision_log_.open(config_.decision_log_path, std::ios::out | std::ios::trunc);
        if (!decision_log_.is_open()) {
            utils::log_error("Failed to open decision log: " + config_.decision_log_path);
            std::cout << "[Backend] ❌ Failed to open decision log: " << config_.decision_log_path << std::endl;
            return false;
        }
    }

    std::cout << "[Backend]   Trade log: " << config_.trade_log_path << std::endl;
    trade_log_.open(config_.trade_log_path, std::ios::out | std::ios::trunc);
    if (!trade_log_.is_open()) {
        utils::log_error("Failed to open trade log: " + config_.trade_log_path);
        std::cout << "[Backend] ❌ Failed to open trade log: " << config_.trade_log_path << std::endl;
        return false;
    }

    std::cout << "[Backend]   Position log: " << config_.position_log_path << std::endl;
    position_log_.open(config_.position_log_path, std::ios::out | std::ios::trunc);
    if (!position_log_.is_open()) {
        utils::log_error("Failed to open position log: " + config_.position_log_path);
        std::cout << "[Backend] ❌ Failed to open position log: " << config_.position_log_path << std::endl;
        return false;
    }

    // Initialize session stats
    session_stats_ = SessionStats();
    session_stats_.session_start = std::chrono::system_clock::now();
    session_stats_.current_equity = config_.starting_capital;
    session_stats_.max_equity = config_.starting_capital;
    session_stats_.min_equity = config_.starting_capital;

    trading_active_ = true;
    is_warmup_ = false;  // End warmup mode, start actual trading

    utils::log_info("✓ Trading session started");
    utils::log_info("✓ Warmup mode disabled - trades will now execute");
    utils::log_info("  Signal log: " + config_.signal_log_path);
    utils::log_info("  Decision log: " + config_.decision_log_path);
    utils::log_info("  Trade log: " + config_.trade_log_path);
    utils::log_info("  Position log: " + config_.position_log_path);

    return true;
}

void RotationTradingBackend::stop_trading() {
    if (!trading_active_) {
        return;
    }

    utils::log_info("========================================");
    utils::log_info("Stopping Trading Session");
    utils::log_info("========================================");

    // DIAGNOSTIC: Pre-liquidation state
    utils::log_info("========================================");
    utils::log_info("Pre-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Cash: $" + std::to_string(current_cash_));
    utils::log_info("Allocated Capital: $" + std::to_string(allocated_capital_));

    auto positions = rotation_manager_->get_positions();
    double unrealized_total = 0.0;

    for (const auto& [symbol, pos] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_[symbol];
            double entry_cost = position_entry_costs_[symbol];
            double current_value = shares * pos.current_price;
            double unrealized = current_value - entry_cost;
            unrealized_total += unrealized;

            utils::log_info("Position " + symbol + ": " +
                          std::to_string(shares) + " shares, " +
                          "entry_cost=$" + std::to_string(entry_cost) +
                          ", current_value=$" + std::to_string(current_value) +
                          ", unrealized=$" + std::to_string(unrealized) +
                          " (" + std::to_string(unrealized / entry_cost * 100.0) + "%)");
        }
    }

    utils::log_info("Total Unrealized P&L: $" + std::to_string(unrealized_total));
    double pre_liquidation_equity = current_cash_ + allocated_capital_ + unrealized_total;
    utils::log_info("Pre-liquidation Equity: $" + std::to_string(pre_liquidation_equity) +
                   " (" + std::to_string(pre_liquidation_equity / config_.starting_capital * 100.0) + "%)");

    // Liquidate all positions
    if (rotation_manager_->get_position_count() > 0) {
        utils::log_info("========================================");
        utils::log_info("Liquidating " + std::to_string(positions.size()) + " positions...");
        liquidate_all_positions("Session End");
    }

    // Update session stats after liquidation
    update_session_stats();

    // DIAGNOSTIC: Post-liquidation state
    utils::log_info("========================================");
    utils::log_info("Post-Liquidation State");
    utils::log_info("========================================");
    utils::log_info("Final Cash: $" + std::to_string(current_cash_));
    utils::log_info("Final Allocated: $" + std::to_string(allocated_capital_) +
                   " (should be ~$0)");
    utils::log_info("Final Equity (from stats): $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");

    // Close log files
    if (signal_log_.is_open()) signal_log_.close();
    if (decision_log_.is_open()) decision_log_.close();
    if (trade_log_.is_open()) trade_log_.close();
    if (position_log_.is_open()) position_log_.close();

    // Finalize session stats
    session_stats_.session_end = std::chrono::system_clock::now();

    trading_active_ = false;

    // Print summary
    utils::log_info("========================================");
    utils::log_info("Session Summary");
    utils::log_info("========================================");
    utils::log_info("Bars processed: " + std::to_string(session_stats_.bars_processed));
    utils::log_info("Signals generated: " + std::to_string(session_stats_.signals_generated));
    utils::log_info("Trades executed: " + std::to_string(session_stats_.trades_executed));
    utils::log_info("Positions opened: " + std::to_string(session_stats_.positions_opened));
    utils::log_info("Positions closed: " + std::to_string(session_stats_.positions_closed));
    utils::log_info("Rotations: " + std::to_string(session_stats_.rotations));
    utils::log_info("");
    utils::log_info("Total P&L: $" + std::to_string(session_stats_.total_pnl) +
                   " (" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%)");
    utils::log_info("Final equity: $" + std::to_string(session_stats_.current_equity));
    utils::log_info("Max drawdown: " + std::to_string(session_stats_.max_drawdown * 100.0) + "%");
    utils::log_info("Win rate: " + std::to_string(session_stats_.win_rate * 100.0) + "%");
    utils::log_info("Sharpe ratio: " + std::to_string(session_stats_.sharpe_ratio));
    utils::log_info("MRD: " + std::to_string(session_stats_.mrd * 100.0) + "%");
    utils::log_info("========================================");
}

bool RotationTradingBackend::on_bar() {
    if (!trading_active_) {
        utils::log_error("Cannot process bar - trading not active");
        return false;
    }

    session_stats_.bars_processed++;

    // Step 1: Update OES on_bar (updates feature engines)
    oes_manager_->on_bar();

    // Step 1.5: Data quality validation
    // Get current snapshot and validate bars
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, Bar> current_bars;
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_bars[symbol] = snap.latest_bar;
    }
    if (!data_validator_.validate_snapshot(current_bars)) {
        std::string error = data_validator_.get_last_error();
        utils::log_error("[DataValidator] Bar validation failed: " + error);
        // In strict mode, we could skip this bar, but for now just warn
    }

    // Step 2: Generate signals
    auto signals = generate_signals();
    session_stats_.signals_generated += signals.size();

    // Log signals
    if (config_.log_all_signals) {
        for (const auto& [symbol, signal] : signals) {
            log_signal(symbol, signal);
        }
    }

    // Step 3: Rank signals
    auto ranked_signals = rank_signals(signals);

    // CRITICAL FIX: Circuit breaker - check for large losses or minimum capital
    // IMPORTANT: Calculate total unrealized P&L using current position values
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();
    for (const auto& [symbol, position] : positions) {
        if (position_entry_costs_.count(symbol) > 0 && position_shares_.count(symbol) > 0) {
            double entry_cost = position_entry_costs_.at(symbol);
            int shares = position_shares_.at(symbol);
            double current_value = shares * position.current_price;
            double pnl = current_value - entry_cost;
            unrealized_pnl += pnl;
        }
    }
    double current_equity = current_cash_ + allocated_capital_ + unrealized_pnl;
    double equity_pct = current_equity / config_.starting_capital;
    const double MIN_TRADING_CAPITAL = 10000.0;  // $10k minimum to continue trading

    // Update trading monitor with equity
    trading_monitor_.update_equity(current_equity, config_.starting_capital);

    // Use stderr to ensure this shows up
    std::cerr << "[EQUITY] cash=$" << current_cash_
              << ", allocated=$" << allocated_capital_
              << ", unrealized=$" << unrealized_pnl
              << ", equity=$" << current_equity
              << " (" << (equity_pct * 100.0) << "%)" << std::endl;

    if (!circuit_breaker_triggered_) {
        if (equity_pct < 0.60) {  // 40% loss threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - LARGE LOSS DETECTED          ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (" + std::to_string(equity_pct * 100.0) + "% of start)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        } else if (current_equity < MIN_TRADING_CAPITAL) {  // Minimum capital threshold
            circuit_breaker_triggered_ = true;
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
            utils::log_error("║ CIRCUIT BREAKER TRIGGERED - MINIMUM CAPITAL BREACH       ║");
            utils::log_error("║ Current equity: $" + std::to_string(current_equity) +
                            " (below $" + std::to_string(MIN_TRADING_CAPITAL) + " minimum)      ║");
            utils::log_error("║ Stopping all new entries - will only exit positions      ║");
            utils::log_error("╔════════════════════════════════════════════════════════════╗");
        }
    }

    // Step 4: Check for EOD
    int current_time_minutes = get_current_time_minutes();

    if (is_eod(current_time_minutes)) {
        utils::log_info("EOD reached - liquidating all positions");
        liquidate_all_positions("EOD");
        return true;
    }

    // Step 5: Make position decisions
    auto decisions = make_decisions(ranked_signals);

    // DIAGNOSTIC: Log received decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ BACKEND RECEIVED " + std::to_string(decisions.size()) + " DECISIONS FROM make_decisions()     ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");

    // Log decisions
    if (config_.log_all_decisions) {
        for (const auto& decision : decisions) {
            log_decision(decision);
        }
    }

    // Step 6: Execute decisions
    utils::log_info("╔════════════════════════════════════════════════════════════╗");
    utils::log_info("║ EXECUTING DECISIONS (skipping HOLDs)                      ║");
    utils::log_info("╚════════════════════════════════════════════════════════════╝");
    int executed_count = 0;
    for (const auto& decision : decisions) {
        if (decision.decision != RotationPositionManager::Decision::HOLD) {
            utils::log_info(">>> EXECUTING decision #" + std::to_string(executed_count + 1) +
                          ": " + decision.symbol);
            execute_decision(decision);
            executed_count++;
        }
    }
    utils::log_info(">>> EXECUTED " + std::to_string(executed_count) + " decisions (skipped " +
                   std::to_string(decisions.size() - executed_count) + " HOLDs)");

    // Step 7: Update learning
    update_learning();

    // Step 8: Log positions
    log_positions();

    // Step 9: Update statistics
    update_session_stats();

    return true;
}

bool RotationTradingBackend::is_eod(int current_time_minutes) const {
    return current_time_minutes >= config_.rotation_config.eod_exit_time_minutes;
}

bool RotationTradingBackend::liquidate_all_positions(const std::string& reason) {
    auto positions = rotation_manager_->get_positions();

    utils::log_info("[EOD] Liquidating " + std::to_string(positions.size()) +
                   " positions. Reason: " + reason);
    utils::log_info("[EOD] Cash before: $" + std::to_string(current_cash_) +
                   ", Allocated: $" + std::to_string(allocated_capital_));

    for (const auto& [symbol, position] : positions) {
        // Get tracking info for logging
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double exit_value = shares * position.current_price;
            double realized_pnl = exit_value - entry_cost;

            utils::log_info("[EOD] Liquidating " + symbol + ": " +
                          std::to_string(shares) + " shares @ $" +
                          std::to_string(position.current_price) +
                          ", proceeds=$" + std::to_string(exit_value) +
                          ", P&L=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl / entry_cost * 100.0) + "%)");
        }

        // Create EOD exit decision
        RotationPositionManager::PositionDecision decision;
        decision.symbol = symbol;
        decision.decision = RotationPositionManager::Decision::EOD_EXIT;
        decision.position = position;
        decision.reason = reason;

        // Execute (this handles all accounting via execute_decision)
        execute_decision(decision);
    }

    utils::log_info("[EOD] Liquidation complete. Final cash: $" +
                   std::to_string(current_cash_) +
                   ", Final allocated: $" + std::to_string(allocated_capital_));

    // Verify accounting - allocated should be 0 or near-0 after liquidation
    if (std::abs(allocated_capital_) > 0.01) {
        utils::log_error("[EOD] WARNING: Allocated capital should be ~0 but is $" +
                        std::to_string(allocated_capital_) +
                        " after liquidation!");
    }

    return true;
}

// === State Access ===

bool RotationTradingBackend::is_ready() const {
    return oes_manager_->all_ready();
}

double RotationTradingBackend::get_current_equity() const {
    // CRITICAL FIX: Calculate proper unrealized P&L using tracked positions
    double unrealized_pnl = 0.0;
    auto positions = rotation_manager_->get_positions();

    for (const auto& [symbol, position] : positions) {
        if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
            int shares = position_shares_.at(symbol);
            double entry_cost = position_entry_costs_.at(symbol);
            double current_value = shares * position.current_price;
            unrealized_pnl += (current_value - entry_cost);
        }
    }

    // CRITICAL FIX: Include allocated_capital_ which represents entry costs of positions
    return current_cash_ + allocated_capital_ + unrealized_pnl;
}

void RotationTradingBackend::update_config(const Config& new_config) {
    config_ = new_config;

    // Update component configs
    oes_manager_->update_config(new_config.oes_config);
    signal_aggregator_->update_config(new_config.aggregator_config);
    rotation_manager_->update_config(new_config.rotation_config);
}

// === Private Methods ===

std::map<std::string, SignalOutput> RotationTradingBackend::generate_signals() {
    return oes_manager_->generate_all_signals();
}

std::vector<SignalAggregator::RankedSignal> RotationTradingBackend::rank_signals(
    const std::map<std::string, SignalOutput>& signals
) {
    // Get staleness weights from data manager
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> staleness_weights;

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        staleness_weights[symbol] = snap.staleness_weight;
    }

    return signal_aggregator_->rank_signals(signals, staleness_weights);
}

std::vector<RotationPositionManager::PositionDecision>
RotationTradingBackend::make_decisions(
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals
) {
    // Get current prices
    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> current_prices;

    // FIX 1: Diagnostic logging to identify data synchronization issues
    static int call_count = 0;
    if (call_count++ % 100 == 0) {  // Log every 100 calls to avoid spam
        utils::log_info("[DEBUG] make_decisions() call #" + std::to_string(call_count) +
                       ": Snapshot has " + std::to_string(snapshot.snapshots.size()) + " symbols");
    }

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_prices[symbol] = snap.latest_bar.close;

        if (call_count % 100 == 0) {
            utils::log_info("[DEBUG]   " + symbol + " price: " +
                           std::to_string(snap.latest_bar.close) +
                           " (bar_id: " + std::to_string(snap.latest_bar.bar_id) + ")");
        }
    }

    if (current_prices.empty()) {
        utils::log_error("[CRITICAL] No current prices available for position decisions!");
        utils::log_error("  Snapshot size: " + std::to_string(snapshot.snapshots.size()));
        utils::log_error("  Data manager appears to have no data");
    }

    int current_time_minutes = get_current_time_minutes();

    return rotation_manager_->make_decisions(
        ranked_signals,
        current_prices,
        current_time_minutes
    );
}

bool RotationTradingBackend::execute_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!config_.enable_trading) {
        // Dry run mode - just log
        utils::log_info("[DRY RUN] " + decision.symbol + ": " +
                       std::to_string(static_cast<int>(decision.decision)));
        return true;
    }

    // WARMUP FIX: Skip trade execution during warmup phase
    if (is_warmup_) {
        utils::log_info("[WARMUP] Skipping trade execution for " + decision.symbol +
                       " (warmup mode active)");
        return true;  // Return success but don't execute
    }

    // CRITICAL FIX: Circuit breaker - block new entries if triggered
    bool is_entry = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                     decision.decision == RotationPositionManager::Decision::ENTER_SHORT);

    if (circuit_breaker_triggered_ && is_entry) {
        utils::log_warning("[CIRCUIT BREAKER] Blocking new entry for " + decision.symbol +
                          " - circuit breaker active due to large losses");
        return false;  // Block entry
    }

    // Get execution price
    std::string side = (decision.decision == RotationPositionManager::Decision::ENTER_LONG) ?
                       "BUY" : "SELL";
    double execution_price = get_execution_price(decision.symbol, side);

    // Calculate position size
    int shares = 0;
    double position_cost = 0.0;

    if (is_entry) {

        shares = calculate_position_size(decision);

        if (shares == 0) {
            utils::log_warning("Position size is 0 for " + decision.symbol + " - skipping");
            return false;
        }

        // CRITICAL FIX: Validate we have sufficient cash BEFORE proceeding
        position_cost = shares * execution_price;

        if (position_cost > current_cash_) {
            utils::log_error("INSUFFICIENT FUNDS: Need $" + std::to_string(position_cost) +
                           " but only have $" + std::to_string(current_cash_) +
                           " for " + decision.symbol);
            return false;
        }

        // PRE-DEDUCT cash to prevent over-allocation race condition
        current_cash_ -= position_cost;
        utils::log_info("Pre-deducted $" + std::to_string(position_cost) +
                       " for " + decision.symbol +
                       " (remaining cash: $" + std::to_string(current_cash_) + ")");

    }

    // Execute with rotation manager
    bool success = rotation_manager_->execute_decision(decision, execution_price);

    // Variables for tracking realized P&L (for EXIT trades)
    double realized_pnl = std::numeric_limits<double>::quiet_NaN();
    double realized_pnl_pct = std::numeric_limits<double>::quiet_NaN();

    if (success) {
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            // Cash already deducted above, track allocated capital
            allocated_capital_ += position_cost;

            // CRITICAL FIX: Track entry cost and shares for this position
            position_entry_costs_[decision.symbol] = position_cost;
            position_shares_[decision.symbol] = shares;

            session_stats_.positions_opened++;
            session_stats_.trades_executed++;

            utils::log_info("Entry: allocated $" + std::to_string(position_cost) +
                          " for " + decision.symbol + " (" + std::to_string(shares) + " shares)");

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }
        } else {
            // Exit - return cash and release allocated capital
            // CRITICAL FIX: Use tracked entry cost and shares
            if (position_entry_costs_.count(decision.symbol) == 0) {
                utils::log_error("CRITICAL: No entry cost tracked for " + decision.symbol);
                return false;
            }

            double entry_cost = position_entry_costs_[decision.symbol];
            int exit_shares = position_shares_[decision.symbol];
            double exit_value = exit_shares * execution_price;

            current_cash_ += exit_value;
            allocated_capital_ -= entry_cost;  // Remove the original allocation

            // Remove from tracking maps
            position_entry_costs_.erase(decision.symbol);
            position_shares_.erase(decision.symbol);

            session_stats_.positions_closed++;
            session_stats_.trades_executed++;

            // Calculate realized P&L for this exit
            realized_pnl = exit_value - entry_cost;
            realized_pnl_pct = realized_pnl / entry_cost;

            // Update trading monitor with trade result
            bool is_win = (realized_pnl > 0.0);
            trading_monitor_.update_trade_result(is_win, realized_pnl);

            utils::log_info("Exit: " + decision.symbol +
                          " - entry_cost=$" + std::to_string(entry_cost) +
                          ", exit_value=$" + std::to_string(exit_value) +
                          ", realized_pnl=$" + std::to_string(realized_pnl) +
                          " (" + std::to_string(realized_pnl_pct * 100.0) + "%)");

            // Track realized P&L for learning
            realized_pnls_[decision.symbol] = realized_pnl;

            // Track trade history for adaptive volatility adjustment (last 2 trades)
            TradeHistory trade_record;
            trade_record.pnl_pct = realized_pnl_pct;
            trade_record.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            auto& history = symbol_trade_history_[decision.symbol];
            history.push_back(trade_record);

            // Keep only last 2 trades
            if (history.size() > 2) {
                history.pop_front();
            }

            // Validate total capital
            double total_capital = current_cash_ + allocated_capital_;
            if (std::abs(total_capital - config_.starting_capital) > 1.0) {
                utils::log_warning("Capital tracking error after exit: cash=$" +
                                 std::to_string(current_cash_) +
                                 ", allocated=$" + std::to_string(allocated_capital_) +
                                 ", total=$" + std::to_string(total_capital) +
                                 ", expected=$" + std::to_string(config_.starting_capital));
            }

            // Update shares for logging
            shares = exit_shares;
        }

        // Track rotations
        if (decision.decision == RotationPositionManager::Decision::ROTATE_OUT) {
            session_stats_.rotations++;
        }

        // Log trade (with actual realized P&L for exits)
        log_trade(decision, execution_price, shares, realized_pnl, realized_pnl_pct);
    } else {
        // ROLLBACK on failure for entry positions
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            current_cash_ += position_cost;  // Restore cash
            utils::log_error("Failed to execute " + decision.symbol +
                           " - rolled back $" + std::to_string(position_cost) +
                           " (cash now: $" + std::to_string(current_cash_) + ")");
        }
    }

    return success;
}

double RotationTradingBackend::get_execution_price(
    const std::string& symbol,
    const std::string& side
) {
    auto snapshot = data_manager_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        utils::log_error("CRITICAL: No data for " + symbol + " - cannot get price");
        // Return last known price if available, or throw
        if (rotation_manager_->has_position(symbol)) {
            auto& positions = rotation_manager_->get_positions();
            return positions.at(symbol).current_price;
        }
        throw std::runtime_error("No price available for " + symbol);
    }

    double price = snapshot.snapshots.at(symbol).latest_bar.close;
    if (price <= 0.0) {
        throw std::runtime_error("Invalid price for " + symbol + ": " + std::to_string(price));
    }

    return price;
}

int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    // CRITICAL FIX: Use current equity (not starting capital) to prevent over-allocation
    // This adapts position sizing to account for current P&L
    double current_equity = current_cash_ + allocated_capital_;
    int max_positions = config_.rotation_config.max_positions;
    double base_allocation = (current_equity * 0.95) / max_positions;

    // ADAPTIVE Volatility-adjusted position sizing
    // Get realized volatility from the feature engine for this symbol
    auto* oes_instance = oes_manager_->get_oes_instance(decision.symbol);
    double volatility = 0.0;
    if (oes_instance && oes_instance->get_feature_engine()) {
        volatility = oes_instance->get_feature_engine()->get_realized_volatility(20);
    }

    // Check past 2 trades performance to determine if volatility is helping or hurting
    double volatility_weight = 1.0;
    std::string adjustment_reason = "no_history";

    if (symbol_trade_history_.count(decision.symbol) > 0) {
        const auto& history = symbol_trade_history_.at(decision.symbol);

        if (history.size() >= 2) {
            // Have 2 trades - check if both winning, both losing, or mixed
            bool trade1_win = (history[0].pnl_pct > 0.0);
            bool trade2_win = (history[1].pnl_pct > 0.0);

            if (trade1_win && trade2_win) {
                // Both trades won - volatility is helping us!
                // INCREASE position aggressively when winning
                double avg_pnl = (history[0].pnl_pct + history[1].pnl_pct) / 2.0;
                if (avg_pnl > 0.03) {  // Average > 3% gain - strong winners
                    volatility_weight = 1.5;  // AGGRESSIVE increase
                    adjustment_reason = "both_wins_strong";
                } else if (avg_pnl > 0.01) {  // Average > 1% gain
                    volatility_weight = 1.3;  // Moderate increase
                    adjustment_reason = "both_wins_moderate";
                } else {
                    volatility_weight = 1.15;  // Slight increase even for small wins
                    adjustment_reason = "both_wins";
                }
            } else if (!trade1_win && !trade2_win) {
                // Both trades lost - volatility is hurting us!
                // Apply VERY aggressive inverse volatility reduction
                if (volatility > 0.0) {
                    const double baseline_vol = 0.01;  // VERY low baseline for extreme reduction
                    volatility_weight = baseline_vol / (volatility + baseline_vol);
                    volatility_weight = std::max(0.3, std::min(0.9, volatility_weight));  // Clamp [0.3, 0.9]
                    adjustment_reason = "both_losses";
                } else {
                    volatility_weight = 0.7;  // Reduce even with no volatility data
                    adjustment_reason = "both_losses_no_vol";
                }
            } else {
                // Mixed results (1 win, 1 loss) - stay neutral or slight reduction
                volatility_weight = 0.95;  // Very slight reduction
                adjustment_reason = "mixed";
            }
        } else if (history.size() == 1) {
            // Only 1 trade - use it as a signal and react quickly
            bool trade_win = (history[0].pnl_pct > 0.0);
            if (trade_win) {
                // React faster to wins - increase position after just 1 win
                if (history[0].pnl_pct > 0.03) {
                    volatility_weight = 1.4;  // Strong win -> aggressive increase
                    adjustment_reason = "one_win_strong";
                } else if (history[0].pnl_pct > 0.015) {
                    volatility_weight = 1.25;  // Good win -> moderate increase
                    adjustment_reason = "one_win_good";
                } else {
                    volatility_weight = 1.15;  // Small win -> slight increase
                    adjustment_reason = "one_win";
                }
            } else {
                // React to losses with reduction
                if (volatility > 0.0) {
                    const double baseline_vol = 0.015;
                    volatility_weight = baseline_vol / (volatility + baseline_vol);
                    volatility_weight = std::max(0.6, std::min(1.0, volatility_weight));  // Clamp [0.6, 1.0]
                    adjustment_reason = "one_loss";
                } else {
                    volatility_weight = 0.85;  // Reduce even without volatility data
                    adjustment_reason = "one_loss_no_vol";
                }
            }
        }
    } else if (volatility > 0.0) {
        // No trade history - use standard inverse volatility
        const double baseline_vol = 0.02;
        volatility_weight = baseline_vol / (volatility + baseline_vol);
        volatility_weight = std::max(0.7, std::min(1.3, volatility_weight));  // Conservative clamp
        adjustment_reason = "no_history";
    }

    // Apply volatility weight to allocation
    double fixed_allocation = base_allocation * volatility_weight;

    // Log volatility adjustment with reasoning (helps understand position sizing decisions)
    std::cerr << "[ADAPTIVE VOL] " << decision.symbol
              << ": vol=" << (volatility * 100.0) << "%"
              << ", weight=" << volatility_weight
              << ", reason=" << adjustment_reason
              << ", base=$" << base_allocation
              << " → adj=$" << fixed_allocation << std::endl;

    // But still check against available cash
    double available_cash = current_cash_;
    double allocation = std::min(fixed_allocation, available_cash * 0.95);

    if (allocation <= 100.0) {
        utils::log_warning("Insufficient cash for position: $" +
                          std::to_string(available_cash) +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) + ")");
        return 0;  // Don't trade with less than $100
    }

    // Get execution price
    double price = get_execution_price(decision.symbol, "BUY");
    if (price <= 0) {
        utils::log_error("Invalid price for position sizing: " +
                        std::to_string(price));
        return 0;
    }

    int shares = static_cast<int>(allocation / price);

    // Final validation - ensure position doesn't exceed available cash
    double position_value = shares * price;
    if (position_value > available_cash) {
        shares = static_cast<int>(available_cash / price);
    }

    // Validate we got non-zero shares
    if (shares == 0) {
        utils::log_warning("[POSITION SIZE] Calculated 0 shares for " + decision.symbol +
                          " (fixed_alloc=$" + std::to_string(fixed_allocation) +
                          ", available=$" + std::to_string(available_cash) +
                          ", allocation=$" + std::to_string(allocation) +
                          ", price=$" + std::to_string(price) + ")");

        // Force minimum 1 share if we have enough capital
        if (allocation >= price) {
            utils::log_info("[POSITION SIZE] Forcing minimum 1 share");
            shares = 1;
        } else {
            utils::log_error("[POSITION SIZE] Insufficient capital even for 1 share - skipping");
            return 0;
        }
    }

    utils::log_info("Position sizing for " + decision.symbol +
                   ": fixed_alloc=$" + std::to_string(fixed_allocation) +
                   ", available=$" + std::to_string(available_cash) +
                   ", allocation=$" + std::to_string(allocation) +
                   ", price=$" + std::to_string(price) +
                   ", shares=" + std::to_string(shares) +
                   ", value=$" + std::to_string(shares * price));

    return shares;
}

void RotationTradingBackend::update_learning() {
    // FIX #1: Continuous Learning Feedback
    // Predictor now receives bar-to-bar returns EVERY bar, not just on exits
    // This is critical for learning - predictor needs frequent feedback

    auto snapshot = data_manager_->get_latest_snapshot();
    std::map<std::string, double> bar_returns;

    // Calculate bar-to-bar return for each symbol
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        auto history = data_manager_->get_recent_bars(symbol, 2);
        if (history.size() >= 2) {
            // Return = (current_close - previous_close) / previous_close
            double bar_return = (history[0].close - history[1].close) / history[1].close;
            bar_returns[symbol] = bar_return;
        }
    }

    // Update all predictors with bar-to-bar returns (weight = 1.0)
    if (!bar_returns.empty()) {
        oes_manager_->update_all(bar_returns);
    }

    // ALSO update with realized P&L when positions exit (weight = 10.0)
    // Realized P&L is more important than bar-to-bar noise
    if (!realized_pnls_.empty()) {
        // Scale realized P&L by 10x to give it more weight in learning
        std::map<std::string, double> weighted_pnls;
        for (const auto& [symbol, pnl] : realized_pnls_) {
            // Convert P&L to return percentage
            double return_pct = pnl / config_.starting_capital;
            weighted_pnls[symbol] = return_pct * 10.0;  // 10x weight
        }
        oes_manager_->update_all(weighted_pnls);
        realized_pnls_.clear();
    }
}

void RotationTradingBackend::log_signal(
    const std::string& symbol,
    const SignalOutput& signal
) {
    if (!signal_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = signal.timestamp_ms;
    j["bar_id"] = signal.bar_id;
    j["symbol"] = symbol;
    j["signal"] = static_cast<int>(signal.signal_type);
    j["probability"] = signal.probability;
    j["confidence"] = signal.confidence;

    signal_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!decision_log_.is_open()) {
        return;
    }

    json j;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["reason"] = decision.reason;

    if (decision.decision != RotationPositionManager::Decision::HOLD) {
        j["rank"] = decision.signal.rank;
        j["strength"] = decision.signal.strength;
    }

    decision_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_trade(
    const RotationPositionManager::PositionDecision& decision,
    double execution_price,
    int shares,
    double realized_pnl,
    double realized_pnl_pct
) {
    if (!trade_log_.is_open()) {
        return;
    }

    json j;
    j["timestamp_ms"] = data_manager_->get_latest_snapshot().logical_timestamp_ms;
    j["symbol"] = decision.symbol;
    j["decision"] = static_cast<int>(decision.decision);
    j["exec_price"] = execution_price;
    j["shares"] = shares;
    j["action"] = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                   decision.decision == RotationPositionManager::Decision::ENTER_SHORT) ?
                  "ENTRY" : "EXIT";
    j["direction"] = (decision.signal.signal.signal_type == SignalType::LONG) ?
                     "LONG" : "SHORT";
    j["price"] = execution_price;
    j["value"] = execution_price * shares;
    j["reason"] = decision.reason;  // Add reason for entry/exit

    // Add P&L for exits
    if (decision.decision != RotationPositionManager::Decision::ENTER_LONG &&
        decision.decision != RotationPositionManager::Decision::ENTER_SHORT) {
        // CRITICAL FIX: Use actual realized P&L passed from execute_decision (exit_value - entry_cost)
        if (!std::isnan(realized_pnl) && !std::isnan(realized_pnl_pct)) {
            j["pnl"] = realized_pnl;
            j["pnl_pct"] = realized_pnl_pct;
        } else {
            // Fallback to position P&L (should not happen for EXIT trades)
            j["pnl"] = decision.position.pnl * shares;
            j["pnl_pct"] = decision.position.pnl_pct;
        }
        j["bars_held"] = decision.position.bars_held;
    } else {
        // For ENTRY trades, add signal metadata
        j["signal_probability"] = decision.signal.signal.probability;
        j["signal_confidence"] = decision.signal.signal.confidence;
        j["signal_rank"] = decision.signal.rank;
    }

    trade_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::log_positions() {
    if (!position_log_.is_open()) {
        return;
    }

    json j;
    j["bar"] = session_stats_.bars_processed;
    j["positions"] = json::array();

    for (const auto& [symbol, position] : rotation_manager_->get_positions()) {
        json pos_j;
        pos_j["symbol"] = symbol;
        pos_j["direction"] = (position.direction == SignalType::LONG) ? "LONG" : "SHORT";
        pos_j["entry_price"] = position.entry_price;
        pos_j["current_price"] = position.current_price;
        pos_j["pnl"] = position.pnl;
        pos_j["pnl_pct"] = position.pnl_pct;
        pos_j["bars_held"] = position.bars_held;
        pos_j["current_rank"] = position.current_rank;
        pos_j["current_strength"] = position.current_strength;

        j["positions"].push_back(pos_j);
    }

    j["total_unrealized_pnl"] = rotation_manager_->get_total_unrealized_pnl();
    j["current_equity"] = get_current_equity();

    position_log_ << j.dump() << std::endl;
}

void RotationTradingBackend::update_session_stats() {
    // Calculate current equity using CORRECT formula (cash + allocated + unrealized)
    session_stats_.current_equity = get_current_equity();

    // Track equity curve
    equity_curve_.push_back(session_stats_.current_equity);

    // Update max/min equity
    if (session_stats_.current_equity > session_stats_.max_equity) {
        session_stats_.max_equity = session_stats_.current_equity;
    }
    if (session_stats_.current_equity < session_stats_.min_equity) {
        session_stats_.min_equity = session_stats_.current_equity;
    }

    // Calculate drawdown
    double drawdown = (session_stats_.max_equity - session_stats_.current_equity) /
                     session_stats_.max_equity;
    if (drawdown > session_stats_.max_drawdown) {
        session_stats_.max_drawdown = drawdown;
    }

    // Calculate total P&L from FULL equity (not just cash!)
    session_stats_.total_pnl = session_stats_.current_equity - config_.starting_capital;
    session_stats_.total_pnl_pct = session_stats_.total_pnl / config_.starting_capital;

    // Diagnostic logging every 100 bars
    if (session_stats_.bars_processed % 100 == 0) {
        // Calculate unrealized P&L for logging
        double unrealized_pnl = 0.0;
        auto positions = rotation_manager_->get_positions();
        for (const auto& [symbol, position] : positions) {
            if (position_shares_.count(symbol) > 0 && position_entry_costs_.count(symbol) > 0) {
                int shares = position_shares_.at(symbol);
                double entry_cost = position_entry_costs_.at(symbol);
                double current_value = shares * position.current_price;
                unrealized_pnl += (current_value - entry_cost);
            }
        }

        utils::log_info("[STATS] Bar " + std::to_string(session_stats_.bars_processed) +
                       ": Cash=$" + std::to_string(current_cash_) +
                       ", Allocated=$" + std::to_string(allocated_capital_) +
                       ", Unrealized=$" + std::to_string(unrealized_pnl) +
                       ", Equity=$" + std::to_string(session_stats_.current_equity) +
                       ", P&L=" + std::to_string(session_stats_.total_pnl_pct * 100.0) + "%");
    }

    // Calculate returns for Sharpe
    if (equity_curve_.size() > 1) {
        double ret = (equity_curve_.back() - equity_curve_[equity_curve_.size() - 2]) /
                     equity_curve_[equity_curve_.size() - 2];
        returns_.push_back(ret);
    }

    // Calculate Sharpe ratio (if enough data)
    if (returns_.size() >= 20) {
        double mean_return = 0.0;
        for (double r : returns_) {
            mean_return += r;
        }
        mean_return /= returns_.size();

        double variance = 0.0;
        for (double r : returns_) {
            variance += (r - mean_return) * (r - mean_return);
        }
        variance /= returns_.size();

        double std_dev = std::sqrt(variance);
        if (std_dev > 0.0) {
            // Annualize: 390 bars per day, ~252 trading days
            session_stats_.sharpe_ratio = (mean_return / std_dev) * std::sqrt(390.0 * 252.0);
        }
    }

    // Calculate MRD (Mean Return per Day)
    // Assume 390 bars per day
    if (session_stats_.bars_processed >= 390) {
        int trading_days = session_stats_.bars_processed / 390;
        session_stats_.mrd = session_stats_.total_pnl_pct / trading_days;
    }
}

int RotationTradingBackend::get_current_time_minutes() const {
    // Calculate minutes since market open (9:30 AM ET)
    // Works for both mock and live modes

    auto snapshot = data_manager_->get_latest_snapshot();
    if (snapshot.snapshots.empty()) {
        return 0;
    }

    // Get first symbol's timestamp
    auto first_snap = snapshot.snapshots.begin()->second;
    int64_t timestamp_ms = first_snap.latest_bar.timestamp_ms;

    // Convert to time-of-day (assuming ET timezone)
    int64_t timestamp_sec = timestamp_ms / 1000;
    std::time_t t = timestamp_sec;
    std::tm* tm_info = std::localtime(&t);

    if (!tm_info) {
        utils::log_error("Failed to convert timestamp to local time");
        return 0;
    }

    // Calculate minutes since market open (9:30 AM)
    int hour = tm_info->tm_hour;
    int minute = tm_info->tm_min;
    int minutes_since_midnight = hour * 60 + minute;
    constexpr int market_open_minutes = 9 * 60 + 30;  // 9:30 AM = 570 minutes
    int minutes_since_open = minutes_since_midnight - market_open_minutes;

    return minutes_since_open;
}

} // namespace sentio
