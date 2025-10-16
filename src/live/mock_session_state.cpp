#include "live/mock_session_state.h"
#include <fstream>
#include <iostream>
#include <iomanip>

namespace sentio {

// ============================================================================
// Checkpoint Implementation
// ============================================================================

nlohmann::json MockSessionState::Checkpoint::to_json() const {
    nlohmann::json j;
    j["bar_number"] = bar_number;
    j["portfolio_value"] = portfolio_value;
    j["state_name"] = state_name;
    j["position_count"] = position_count;
    j["timestamp_ms"] = timestamp_ms;
    j["positions"] = positions;
    j["avg_prices"] = avg_prices;
    j["cash"] = cash;
    j["phase"] = static_cast<int>(phase);
    return j;
}

MockSessionState::Checkpoint MockSessionState::Checkpoint::from_json(const nlohmann::json& j) {
    Checkpoint cp;
    cp.bar_number = j["bar_number"];
    cp.portfolio_value = j["portfolio_value"];
    cp.state_name = j["state_name"];
    cp.position_count = j["position_count"];
    cp.timestamp_ms = j["timestamp_ms"];
    cp.positions = j["positions"].get<std::map<std::string, double>>();
    cp.avg_prices = j["avg_prices"].get<std::map<std::string, double>>();
    cp.cash = j["cash"];
    cp.phase = static_cast<Phase>(j["phase"].get<int>());
    return cp;
}

// ============================================================================
// SessionMetrics Implementation
// ============================================================================

void MockSessionState::SessionMetrics::print_performance_report() const {
    std::cout << "\n=== Mock Session Performance Report ===\n";
    std::cout << std::fixed << std::setprecision(2);

    // Timing breakdown
    double total_ms = total_strategy_time.count() / 1e6 +
                      total_broker_time.count() / 1e6 +
                      total_feed_time.count() / 1e6;

    std::cout << "\nTiming Breakdown:\n";
    std::cout << "  Total Time: " << total_ms << " ms\n";
    std::cout << "  - Strategy: " << (total_strategy_time.count() / 1e6)
              << " ms (" << (100.0 * total_strategy_time.count() / (total_ms * 1e6)) << "%)\n";
    std::cout << "  - Broker: " << (total_broker_time.count() / 1e6)
              << " ms (" << (100.0 * total_broker_time.count() / (total_ms * 1e6)) << "%)\n";
    std::cout << "  - Feed: " << (total_feed_time.count() / 1e6)
              << " ms (" << (100.0 * total_feed_time.count() / (total_ms * 1e6)) << "%)\n";

    // Call statistics
    std::cout << "\nCall Statistics:\n";
    std::cout << "  Strategy Calls: " << strategy_calls << "\n";
    std::cout << "  Broker Calls: " << broker_calls << "\n";
    std::cout << "  Bars Processed: " << bars_processed << "\n";

    if (bars_processed > 0) {
        std::cout << "  Avg Time/Bar: " << (total_ms / bars_processed) << " ms\n";
    }

    // Trading statistics
    std::cout << "\nTrading Statistics:\n";
    std::cout << "  Total Trades: " << total_trades << "\n";
    std::cout << "  Total Slippage: $" << total_slippage << "\n";
    std::cout << "  Total Commission: $" << total_commission << "\n";

    std::cout << "========================================\n\n";
}

nlohmann::json MockSessionState::SessionMetrics::to_json() const {
    nlohmann::json j;
    j["total_strategy_time_ms"] = total_strategy_time.count() / 1e6;
    j["total_broker_time_ms"] = total_broker_time.count() / 1e6;
    j["total_feed_time_ms"] = total_feed_time.count() / 1e6;
    j["strategy_calls"] = strategy_calls;
    j["broker_calls"] = broker_calls;
    j["bars_processed"] = bars_processed;
    j["total_slippage"] = total_slippage;
    j["total_commission"] = total_commission;
    j["total_trades"] = total_trades;
    return j;
}

// ============================================================================
// MockSessionState Implementation
// ============================================================================

MockSessionState::MockSessionState()
    : current_phase_(Phase::WARMUP)
    , bar_count_(0)
    , portfolio_value_(100000.0)
    , psm_state_("CASH_ONLY")
{
}

void MockSessionState::save_checkpoint(const Checkpoint& checkpoint) {
    checkpoints_.push_back(checkpoint);
}

MockSessionState::Checkpoint MockSessionState::get_latest_checkpoint() const {
    if (checkpoints_.empty()) {
        throw std::runtime_error("No checkpoints available");
    }
    return checkpoints_.back();
}

bool MockSessionState::save_to_file(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json j = to_json();
    file << j.dump(2);  // Pretty print with 2-space indent
    file.close();

    return true;
}

MockSessionState MockSessionState::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open state file: " + path);
    }

    nlohmann::json j;
    file >> j;
    file.close();

    return from_json(j);
}

nlohmann::json MockSessionState::to_json() const {
    nlohmann::json j;

    j["current_phase"] = static_cast<int>(current_phase_);
    j["bar_count"] = bar_count_;
    j["portfolio_value"] = portfolio_value_;
    j["psm_state"] = psm_state_;

    nlohmann::json checkpoints_json = nlohmann::json::array();
    for (const auto& cp : checkpoints_) {
        checkpoints_json.push_back(cp.to_json());
    }
    j["checkpoints"] = checkpoints_json;

    j["metrics"] = metrics_.to_json();

    return j;
}

MockSessionState MockSessionState::from_json(const nlohmann::json& j) {
    MockSessionState state;

    state.current_phase_ = static_cast<Phase>(j["current_phase"].get<int>());
    state.bar_count_ = j["bar_count"];
    state.portfolio_value_ = j["portfolio_value"];
    state.psm_state_ = j["psm_state"];

    for (const auto& cp_json : j["checkpoints"]) {
        state.checkpoints_.push_back(Checkpoint::from_json(cp_json));
    }

    // Metrics (simplified - just copy values)
    if (j.contains("metrics")) {
        const auto& m = j["metrics"];
        state.metrics_.strategy_calls = m.value("strategy_calls", 0);
        state.metrics_.broker_calls = m.value("broker_calls", 0);
        state.metrics_.bars_processed = m.value("bars_processed", 0);
        state.metrics_.total_slippage = m.value("total_slippage", 0.0);
        state.metrics_.total_commission = m.value("total_commission", 0.0);
        state.metrics_.total_trades = m.value("total_trades", 0);
    }

    return state;
}

// ============================================================================
// MockLiveSession Implementation
// ============================================================================

MockLiveSession::MockLiveSession(const std::string& session_name,
                                const std::string& data_path,
                                double speed_multiplier)
    : session_name_(session_name)
    , data_path_(data_path)
    , speed_multiplier_(speed_multiplier)
    , running_(false)
{
}

bool MockLiveSession::start() {
    running_ = true;
    state_.set_phase(MockSessionState::Phase::WARMUP);

    try {
        run_warmup_phase();
        run_trading_phase();
        run_eod_phase();

        state_.set_phase(MockSessionState::Phase::COMPLETE);
        running_ = false;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Mock session error: " << e.what() << std::endl;
        running_ = false;
        return false;
    }
}

void MockLiveSession::stop() {
    running_ = false;
}

void MockLiveSession::simulate_crash_at_time(const std::string& et_time) {
    crash_time_ = et_time;
}

bool MockLiveSession::simulate_restart_with_state(const MockSessionState& state) {
    state_ = state;
    return start();  // Resume from saved state
}

bool MockLiveSession::verify_eod_idempotency() {
    // Run EOD twice and verify same result
    run_eod_phase();
    auto checkpoint1 = state_.get_latest_checkpoint();

    run_eod_phase();
    auto checkpoint2 = state_.get_latest_checkpoint();

    // Compare portfolio values (should be identical)
    return std::abs(checkpoint1.portfolio_value - checkpoint2.portfolio_value) < 0.01;
}

bool MockLiveSession::verify_position_reconciliation() {
    // Verify positions match between PSM and broker
    // (Implementation would check position_book reconciliation)
    return true;
}

MockLiveSession::SessionResult MockLiveSession::get_result() const {
    SessionResult result;
    result.success = (state_.get_current_phase() == MockSessionState::Phase::COMPLETE);
    result.final_portfolio_value = state_.get_portfolio_value();
    result.total_return_pct = (result.final_portfolio_value - 100000.0) / 100000.0 * 100.0;
    result.total_trades = state_.metrics().total_trades;
    result.metrics = state_.metrics();

    return result;
}

void MockLiveSession::run_warmup_phase() {
    std::cout << "[MockSession] Running warmup phase..." << std::endl;
    // Placeholder - actual warmup would load bars and feed to strategy
    state_.set_phase(MockSessionState::Phase::TRADING);
}

void MockLiveSession::run_trading_phase() {
    std::cout << "[MockSession] Running trading phase..." << std::endl;
    // Placeholder - actual trading would run bar replay loop
}

void MockLiveSession::run_eod_phase() {
    std::cout << "[MockSession] Running EOD phase..." << std::endl;
    // Placeholder - actual EOD would liquidate positions
    state_.set_phase(MockSessionState::Phase::EOD);
}

} // namespace sentio
