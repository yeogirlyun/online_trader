#ifndef SENTIO_MOCK_SESSION_STATE_H
#define SENTIO_MOCK_SESSION_STATE_H

#include "common/types.h"
#include "backend/position_state_machine.h"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace sentio {

/**
 * Mock Session State
 *
 * Comprehensive state tracking for mock trading sessions:
 * - Phase tracking (warmup, trading, EOD)
 * - Checkpoint/restore for crash simulation
 * - Performance metrics
 * - Debugging support
 */
class MockSessionState {
public:
    enum class Phase {
        WARMUP,
        TRADING,
        EOD,
        COMPLETE
    };

    struct Checkpoint {
        uint64_t bar_number;
        double portfolio_value;
        std::string state_name;
        int position_count;
        uint64_t timestamp_ms;
        std::map<std::string, double> positions;  // symbol -> quantity
        std::map<std::string, double> avg_prices;  // symbol -> avg entry price
        double cash;
        Phase phase;

        // Serialize to JSON
        nlohmann::json to_json() const;

        // Deserialize from JSON
        static Checkpoint from_json(const nlohmann::json& j);
    };

    MockSessionState();

    // Phase management
    Phase get_current_phase() const { return current_phase_; }
    void set_phase(Phase phase) { current_phase_ = phase; }

    // Checkpoint management
    void save_checkpoint(const Checkpoint& checkpoint);
    std::vector<Checkpoint> get_checkpoints() const { return checkpoints_; }
    Checkpoint get_latest_checkpoint() const;
    bool has_checkpoints() const { return !checkpoints_.empty(); }

    // Persistence
    bool save_to_file(const std::string& path) const;
    static MockSessionState load_from_file(const std::string& path);

    // Metrics
    struct SessionMetrics {
        std::chrono::nanoseconds total_strategy_time{0};
        std::chrono::nanoseconds total_broker_time{0};
        std::chrono::nanoseconds total_feed_time{0};
        size_t strategy_calls{0};
        size_t broker_calls{0};
        size_t bars_processed{0};
        double total_slippage{0.0};
        double total_commission{0.0};
        int total_trades{0};

        void print_performance_report() const;
        nlohmann::json to_json() const;
    };

    SessionMetrics& metrics() { return metrics_; }
    const SessionMetrics& metrics() const { return metrics_; }

    // State tracking
    void set_bar_count(uint64_t count) { bar_count_ = count; }
    uint64_t get_bar_count() const { return bar_count_; }

    void set_portfolio_value(double value) { portfolio_value_ = value; }
    double get_portfolio_value() const { return portfolio_value_; }

    void set_psm_state(const std::string& state) { psm_state_ = state; }
    std::string get_psm_state() const { return psm_state_; }

    // JSON serialization
    nlohmann::json to_json() const;
    static MockSessionState from_json(const nlohmann::json& j);

private:
    Phase current_phase_;
    std::vector<Checkpoint> checkpoints_;
    SessionMetrics metrics_;

    // Current state
    uint64_t bar_count_;
    double portfolio_value_;
    std::string psm_state_;
};

/**
 * Mock Live Session
 *
 * High-level orchestration for mock trading sessions with:
 * - Crash/restart simulation
 * - EOD idempotency verification
 * - Position reconciliation testing
 */
class MockLiveSession {
public:
    MockLiveSession(const std::string& session_name,
                   const std::string& data_path,
                   double speed_multiplier = 1.0);

    // Session control
    bool start();
    void stop();
    bool is_running() const { return running_; }

    // Testing scenarios
    void simulate_crash_at_time(const std::string& et_time);
    bool simulate_restart_with_state(const MockSessionState& state);

    // Verification
    bool verify_eod_idempotency();
    bool verify_position_reconciliation();

    // State access
    MockSessionState& state() { return state_; }
    const MockSessionState& state() const { return state_; }

    // Results
    struct SessionResult {
        bool success;
        std::string error_message;
        double final_portfolio_value;
        double total_return_pct;
        int total_trades;
        MockSessionState::SessionMetrics metrics;
    };

    SessionResult get_result() const;

private:
    std::string session_name_;
    std::string data_path_;
    double speed_multiplier_;
    bool running_;

    MockSessionState state_;
    std::string crash_time_;  // ET time to simulate crash (empty = no crash)

    // Helper methods
    void run_warmup_phase();
    void run_trading_phase();
    void run_eod_phase();
};

} // namespace sentio

#endif // SENTIO_MOCK_SESSION_STATE_H
