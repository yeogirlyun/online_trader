# NAN_BUG_FIX_IMPLEMENTATION_MEGA - Complete Analysis

**Generated**: 2025-10-15 03:30:58
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: Review document: /Volumes/ExternalSSD/Dev/C++/online_trader/megadocs/NAN_BUG_FIX_IMPLEMENTATION_MEGA.md (19 valid files)

**Total Files**: 19

---

## 📋 **TABLE OF CONTENTS**

1. [config/rotation_strategy.json](#file-1)
2. [include/backend/rotation_trading_backend.h](#file-2)
3. [include/cli/rotation_trade_command.h](#file-3)
4. [include/data/mock_multi_symbol_feed.h](#file-4)
5. [include/data/multi_symbol_data_manager.h](#file-5)
6. [include/features/indicators.h](#file-6)
7. [include/features/unified_feature_engine.h](#file-7)
8. [include/learning/online_predictor.h](#file-8)
9. [include/strategy/multi_symbol_oes_manager.h](#file-9)
10. [include/strategy/online_ensemble_strategy.h](#file-10)
11. [megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md](#file-11)
12. [src/backend/rotation_trading_backend.cpp](#file-12)
13. [src/cli/rotation_trade_command.cpp](#file-13)
14. [src/data/mock_multi_symbol_feed.cpp](#file-14)
15. [src/data/multi_symbol_data_manager.cpp](#file-15)
16. [src/features/unified_feature_engine.cpp](#file-16)
17. [src/learning/online_predictor.cpp](#file-17)
18. [src/strategy/multi_symbol_oes_manager.cpp](#file-18)
19. [src/strategy/online_ensemble_strategy.cpp](#file-19)

---

## 📄 **FILE 1 of 19**: config/rotation_strategy.json

**File Information**:
- **Path**: `config/rotation_strategy.json`

- **Size**: 101 lines
- **Modified**: 2025-10-15 02:13:26

- **Type**: .json

```text
{
  "name": "Multi-Symbol Rotation Strategy v2.0",
  "description": "6-symbol leveraged ETF rotation with OnlineEnsemble learning + VIX exposure",
  "version": "2.0.1",

  "symbols": {
    "active": ["TQQQ", "SQQQ", "SPXL", "SDS", "UVXY", "SVXY"],
    "leverage_boosts": {
      "TQQQ": 1.5,
      "SQQQ": 1.5,
      "SPXL": 1.5,
      "SDS": 1.4,
      "UVXY": 1.6,
      "SVXY": 1.3
    }
  },

  "oes_config": {
    "ewrls_lambda": 0.995,
    "initial_variance": 100.0,
    "regularization": 0.01,
    "warmup_samples": 100,

    "prediction_horizons": [1, 5, 10],
    "horizon_weights": [0.3, 0.5, 0.2],

    "buy_threshold": 0.53,
    "sell_threshold": 0.47,
    "neutral_zone": 0.06,

    "enable_bb_amplification": true,
    "bb_period": 20,
    "bb_std_dev": 2.0,
    "bb_proximity_threshold": 0.30,
    "bb_amplification_factor": 0.10,

    "enable_threshold_calibration": true,
    "calibration_window": 200,
    "target_win_rate": 0.60,
    "threshold_step": 0.005,

    "enable_kelly_sizing": false,
    "kelly_fraction": 0.25,
    "max_position_size": 0.50
  },

  "signal_aggregator_config": {
    "min_probability": 0.51,
    "min_confidence": 0.55,
    "min_strength": 0.40,

    "enable_correlation_filter": false,
    "max_correlation": 0.85,

    "filter_stale_signals": true,
    "max_staleness_seconds": 120.0
  },

  "rotation_manager_config": {
    "max_positions": 3,
    "min_rank_to_hold": 5,
    "min_strength_to_enter": 0.50,
    "min_strength_to_hold": 0.45,

    "rotation_strength_delta": 0.10,
    "rotation_cooldown_bars": 5,

    "equal_weight": true,
    "volatility_weight": false,
    "capital_per_position": 0.33,

    "enable_profit_target": true,
    "profit_target_pct": 0.03,
    "enable_stop_loss": true,
    "stop_loss_pct": 0.015,

    "eod_liquidation": true,
    "eod_exit_time_minutes": 358
  },

  "data_manager_config": {
    "max_forward_fills": 5,
    "max_staleness_seconds": 300.0,
    "history_size": 500,
    "log_data_quality": true
  },

  "performance_targets": {
    "target_mrd": 0.005,
    "target_win_rate": 0.60,
    "target_sharpe": 2.0,
    "max_drawdown": 0.05
  },

  "notes": {
    "eod_liquidation": "All positions closed at 3:58 PM ET - eliminates overnight decay risk",
    "leverage_boost": "Prioritizes leveraged ETFs due to higher profit potential with EOD exit",
    "rotation_logic": "Capital flows to strongest signals - simpler than PSM",
    "independence": "Each symbol learns independently - no cross-contamination"
  }
}

```

## 📄 **FILE 2 of 19**: include/backend/rotation_trading_backend.h

**File Information**:
- **Path**: `include/backend/rotation_trading_backend.h`

- **Size**: 355 lines
- **Modified**: 2025-10-14 21:32:03

- **Type**: .h

```text
#pragma once

#include "strategy/multi_symbol_oes_manager.h"
#include "strategy/signal_aggregator.h"
#include "strategy/rotation_position_manager.h"
#include "data/multi_symbol_data_manager.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace sentio {

/**
 * @brief Complete trading backend for multi-symbol rotation strategy
 *
 * This backend integrates all Phase 1 and Phase 2 components into a
 * unified trading system:
 *
 * Phase 1 (Data):
 * - MultiSymbolDataManager (async data handling)
 * - IBarFeed (live or mock data source)
 *
 * Phase 2 (Strategy):
 * - MultiSymbolOESManager (6 independent learners)
 * - SignalAggregator (rank by strength)
 * - RotationPositionManager (hold top N, rotate)
 *
 * Integration:
 * - Broker interface (Alpaca for live, mock for testing)
 * - Performance tracking (MRD, Sharpe, win rate)
 * - Trade logging (signals, decisions, executions)
 * - EOD management (liquidate at 3:58 PM ET)
 *
 * Usage:
 *   RotationTradingBackend backend(config);
 *   backend.warmup(historical_data);
 *   backend.start_trading();
 *
 *   // Each bar:
 *   backend.on_bar();
 *
 *   backend.stop_trading();
 */
class RotationTradingBackend {
public:
    struct Config {
        // Symbols to trade
        std::vector<std::string> symbols = {
            "TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"
        };

        // Component configurations
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        SignalAggregator::Config aggregator_config;
        RotationPositionManager::Config rotation_config;
        data::MultiSymbolDataManager::Config data_config;

        // Trading parameters
        double starting_capital = 100000.0;
        bool enable_trading = true;        // If false, only log signals
        bool log_all_signals = true;       // Log all signals (not just trades)
        bool log_all_decisions = true;     // Log all position decisions

        // Output paths
        std::string signal_log_path = "logs/live_trading/signals.jsonl";
        std::string decision_log_path = "logs/live_trading/decisions.jsonl";
        std::string trade_log_path = "logs/live_trading/trades.jsonl";
        std::string position_log_path = "logs/live_trading/positions.jsonl";

        // Performance tracking
        bool enable_performance_tracking = true;
        int performance_window = 200;      // Bars for rolling metrics

        // Broker (for live trading)
        std::string alpaca_api_key = "";
        std::string alpaca_secret_key = "";
        bool paper_trading = true;
    };

    /**
     * @brief Trading session statistics
     */
    struct SessionStats {
        int bars_processed = 0;
        int signals_generated = 0;
        int trades_executed = 0;
        int positions_opened = 0;
        int positions_closed = 0;
        int rotations = 0;

        double total_pnl = 0.0;
        double total_pnl_pct = 0.0;
        double current_equity = 0.0;
        double max_equity = 0.0;
        double min_equity = 0.0;
        double max_drawdown = 0.0;

        double win_rate = 0.0;
        double avg_win_pct = 0.0;
        double avg_loss_pct = 0.0;
        double sharpe_ratio = 0.0;
        double mrd = 0.0;  // Mean Return per Day

        std::chrono::system_clock::time_point session_start;
        std::chrono::system_clock::time_point session_end;
    };

    /**
     * @brief Construct backend
     *
     * @param config Configuration
     * @param data_mgr Data manager (optional, will create if not provided)
     * @param broker Broker client (optional, for live trading)
     */
    explicit RotationTradingBackend(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr = nullptr,
        std::shared_ptr<AlpacaClient> broker = nullptr
    );

    ~RotationTradingBackend();

    // === Trading Session Management ===

    /**
     * @brief Warmup strategy with historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Start trading session
     *
     * Opens log files, initializes session stats.
     *
     * @return true if started successfully
     */
    bool start_trading();

    /**
     * @brief Stop trading session
     *
     * Closes all positions, finalizes logs, prints summary.
     */
    void stop_trading();

    /**
     * @brief Process new bar (main trading loop)
     *
     * This is the core method called each minute:
     * 1. Update data manager
     * 2. Generate signals (6 symbols)
     * 3. Rank signals by strength
     * 4. Make position decisions
     * 5. Execute trades
     * 6. Update learning
     * 7. Log everything
     *
     * @return true if processing successful
     */
    bool on_bar();

    /**
     * @brief Check if EOD time reached
     *
     * @param current_time_minutes Minutes since market open (9:30 AM ET)
     * @return true if at or past EOD exit time
     */
    bool is_eod(int current_time_minutes) const;

    /**
     * @brief Liquidate all positions (EOD or emergency)
     *
     * @param reason Reason for liquidation
     * @return true if liquidation successful
     */
    bool liquidate_all_positions(const std::string& reason = "EOD");

    // === State Access ===

    /**
     * @brief Check if backend is ready to trade
     *
     * @return true if all components warmed up
     */
    bool is_ready() const;

    /**
     * @brief Get current session statistics
     *
     * @return Session stats
     */
    const SessionStats& get_session_stats() const { return session_stats_; }

    /**
     * @brief Get current equity
     *
     * @return Current equity (cash + unrealized P&L)
     */
    double get_current_equity() const;

    /**
     * @brief Get current positions
     *
     * @return Map of symbol → position
     */
    const std::map<std::string, RotationPositionManager::Position>& get_positions() const {
        return rotation_manager_->get_positions();
    }

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration
     */
    void update_config(const Config& new_config);

private:
    /**
     * @brief Generate all signals
     *
     * @return Map of symbol → signal
     */
    std::map<std::string, SignalOutput> generate_signals();

    /**
     * @brief Rank signals by strength
     *
     * @param signals Map of symbol → signal
     * @return Ranked signals (strongest first)
     */
    std::vector<SignalAggregator::RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals
    );

    /**
     * @brief Make position decisions
     *
     * @param ranked_signals Ranked signals
     * @return Position decisions
     */
    std::vector<RotationPositionManager::PositionDecision> make_decisions(
        const std::vector<SignalAggregator::RankedSignal>& ranked_signals
    );

    /**
     * @brief Execute position decision
     *
     * @param decision Position decision
     * @return true if execution successful
     */
    bool execute_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Get execution price for symbol
     *
     * @param symbol Symbol ticker
     * @param side BUY or SELL
     * @return Execution price
     */
    double get_execution_price(const std::string& symbol, const std::string& side);

    /**
     * @brief Calculate position size
     *
     * @param decision Position decision
     * @return Number of shares
     */
    int calculate_position_size(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Update learning with realized P&L
     */
    void update_learning();

    /**
     * @brief Log signal
     *
     * @param symbol Symbol
     * @param signal Signal output
     */
    void log_signal(const std::string& symbol, const SignalOutput& signal);

    /**
     * @brief Log position decision
     *
     * @param decision Position decision
     */
    void log_decision(const RotationPositionManager::PositionDecision& decision);

    /**
     * @brief Log trade execution
     *
     * @param decision Position decision
     * @param execution_price Price
     * @param shares Shares traded
     */
    void log_trade(
        const RotationPositionManager::PositionDecision& decision,
        double execution_price,
        int shares
    );

    /**
     * @brief Log current positions
     */
    void log_positions();

    /**
     * @brief Update session statistics
     */
    void update_session_stats();

    /**
     * @brief Calculate current time in minutes since market open
     *
     * @return Minutes since 9:30 AM ET
     */
    int get_current_time_minutes() const;

    Config config_;

    // Core components
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::unique_ptr<MultiSymbolOESManager> oes_manager_;
    std::unique_ptr<SignalAggregator> signal_aggregator_;
    std::unique_ptr<RotationPositionManager> rotation_manager_;

    // Broker (for live trading)
    std::shared_ptr<AlpacaClient> broker_;

    // State
    bool trading_active_{false};
    double current_cash_;
    std::map<std::string, double> realized_pnls_;  // For learning updates

    // Logging
    std::ofstream signal_log_;
    std::ofstream decision_log_;
    std::ofstream trade_log_;
    std::ofstream position_log_;

    // Statistics
    SessionStats session_stats_;
    std::vector<double> equity_curve_;
    std::vector<double> returns_;
};

} // namespace sentio

```

## 📄 **FILE 3 of 19**: include/cli/rotation_trade_command.h

**File Information**:
- **Path**: `include/cli/rotation_trade_command.h`

- **Size**: 145 lines
- **Modified**: 2025-10-15 02:53:20

- **Type**: .h

```text
#pragma once

#include "cli/command_interface.h"
#include "backend/rotation_trading_backend.h"
#include "data/multi_symbol_data_manager.h"
#include "data/mock_multi_symbol_feed.h"
#include "live/alpaca_client.hpp"
#include "common/types.h"
#include <string>
#include <memory>
#include <atomic>

namespace sentio {
namespace cli {

/**
 * @brief CLI command for multi-symbol rotation trading
 *
 * Supports two modes:
 * 1. Live trading: Real-time trading with Alpaca paper/live account
 * 2. Mock trading: Backtest replay from CSV files
 *
 * Usage:
 *   ./sentio_cli rotation-trade --mode live
 *   ./sentio_cli rotation-trade --mode mock --data-dir data/equities
 */
class RotationTradeCommand : public Command {
public:
    struct Options {
        // Mode selection
        bool is_mock_mode = false;
        std::string data_dir = "data/equities";

        // Symbols to trade
        std::vector<std::string> symbols = {
            "TQQQ", "SQQQ", "SPXL", "SDS", "UVXY", "SVXY"
        };

        // Capital
        double starting_capital = 100000.0;

        // Warmup configuration
        int warmup_blocks = 20;  // For live mode
        std::string warmup_dir = "data/equities";

        // Output paths
        std::string log_dir = "logs/rotation_trading";

        // Alpaca credentials (for live mode)
        std::string alpaca_api_key;
        std::string alpaca_secret_key;
        bool paper_trading = true;

        // Configuration file
        std::string config_file = "config/rotation_strategy.json";
    };

    RotationTradeCommand();
    explicit RotationTradeCommand(const Options& options);
    ~RotationTradeCommand() override;

    // Command interface
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "rotation-trade"; }
    std::string get_description() const override {
        return "Multi-symbol rotation trading (live or mock mode)";
    }
    void show_help() const override;

    /**
     * @brief Execute with pre-configured options
     *
     * @return 0 on success, non-zero on error
     */
    int execute_with_options();

private:
    /**
     * @brief Load configuration from JSON file
     *
     * @return Backend configuration
     */
    RotationTradingBackend::Config load_config();

    /**
     * @brief Load warmup data for all symbols
     *
     * For live mode: Loads recent historical data (warmup_blocks)
     * For mock mode: Loads from CSV files
     *
     * @return Map of symbol → bars
     */
    std::map<std::string, std::vector<Bar>> load_warmup_data();

    /**
     * @brief Execute mock trading (backtest)
     *
     * @return 0 on success, non-zero on error
     */
    int execute_mock_trading();

    /**
     * @brief Execute live trading
     *
     * @return 0 on success, non-zero on error
     */
    int execute_live_trading();

    /**
     * @brief Setup signal handlers for graceful shutdown
     */
    void setup_signal_handlers();

    /**
     * @brief Check if EOD time reached
     *
     * @return true if at or past 3:58 PM ET
     */
    bool is_eod() const;

    /**
     * @brief Get minutes since market open (9:30 AM ET)
     *
     * @return Minutes since open
     */
    int get_minutes_since_open() const;

    /**
     * @brief Print session summary
     */
    void print_summary(const RotationTradingBackend::SessionStats& stats);

    /**
     * @brief Log system message
     */
    void log_system(const std::string& msg);

    Options options_;
    std::unique_ptr<RotationTradingBackend> backend_;
    std::shared_ptr<data::MultiSymbolDataManager> data_manager_;
    std::shared_ptr<AlpacaClient> broker_;
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 4 of 19**: include/data/mock_multi_symbol_feed.h

**File Information**:
- **Path**: `include/data/mock_multi_symbol_feed.h`

- **Size**: 209 lines
- **Modified**: 2025-10-14 21:06:28

- **Type**: .h

```text
#pragma once

#include "data/bar_feed_interface.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <thread>
#include <atomic>

namespace sentio {
namespace data {

/**
 * @brief Mock data feed for testing - replays historical CSV data
 *
 * Reads CSV files for multiple symbols and replays them synchronously.
 * Useful for backtesting and integration testing.
 *
 * CSV Format (per symbol):
 *   timestamp,open,high,low,close,volume
 *   1696464600000,432.15,432.89,431.98,432.56,12345678
 *
 * Usage:
 *   MockMultiSymbolFeed feed(data_manager, {
 *       {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
 *       {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"}
 *   });
 *   feed.connect();  // Load CSV files
 *   feed.start();    // Start replay in background thread
 */
class MockMultiSymbolFeed : public IBarFeed {
public:
    struct Config {
        std::map<std::string, std::string> symbol_files;  // Symbol → CSV path
        double replay_speed = 39.0;                       // Speed multiplier (0=instant)
        bool loop = false;                                // Loop replay?
        bool sync_timestamps = true;                      // Synchronize timestamps?
    };

    /**
     * @brief Construct mock feed
     *
     * @param data_manager Data manager to feed
     * @param config Configuration
     */
    MockMultiSymbolFeed(std::shared_ptr<MultiSymbolDataManager> data_manager,
                       const Config& config);

    ~MockMultiSymbolFeed() override;

    // === IBarFeed Interface ===

    /**
     * @brief Connect to data source (loads CSV files)
     * @return true if all CSV files loaded successfully
     */
    bool connect() override;

    /**
     * @brief Disconnect from data source
     */
    void disconnect() override;

    /**
     * @brief Check if connected (data loaded)
     */
    bool is_connected() const override;

    /**
     * @brief Start feeding data (begins replay in background thread)
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop feeding data
     */
    void stop() override;

    /**
     * @brief Check if feed is active
     */
    bool is_active() const override;

    /**
     * @brief Get feed type identifier
     */
    std::string get_type() const override;

    /**
     * @brief Get symbols being fed
     */
    std::vector<std::string> get_symbols() const override;

    /**
     * @brief Set callback for bar updates
     */
    void set_bar_callback(BarCallback callback) override;

    /**
     * @brief Set callback for errors
     */
    void set_error_callback(ErrorCallback callback) override;

    /**
     * @brief Set callback for connection state changes
     */
    void set_connection_callback(ConnectionCallback callback) override;

    /**
     * @brief Get feed statistics
     */
    FeedStats get_stats() const override;

    // === Additional Mock-Specific Methods ===

    /**
     * @brief Get total bars loaded per symbol
     */
    std::map<std::string, int> get_bar_counts() const;

    /**
     * @brief Get replay progress
     */
    struct Progress {
        int bars_replayed;
        int total_bars;
        double progress_pct;
        std::string current_symbol;
        uint64_t current_timestamp;
    };

    Progress get_progress() const;

private:
    /**
     * @brief Load CSV file for a symbol
     *
     * @param symbol Symbol ticker
     * @param filepath Path to CSV file
     * @return Number of bars loaded
     */
    int load_csv(const std::string& symbol, const std::string& filepath);

    /**
     * @brief Parse CSV line into Bar
     *
     * @param line CSV line
     * @param bar Output bar
     * @return true if parsed successfully
     */
    bool parse_csv_line(const std::string& line, Bar& bar);

    /**
     * @brief Sleep for replay speed
     *
     * @param bars Number of bars to sleep for (1 = 1 minute real-time)
     */
    void sleep_for_replay(int bars = 1);

    /**
     * @brief Replay loop (runs in background thread)
     */
    void replay_loop();

    /**
     * @brief Replay single bar for all symbols
     * @return true if bars available, false if EOF
     */
    bool replay_next_bar();

    std::shared_ptr<MultiSymbolDataManager> data_manager_;
    Config config_;

    // Data storage
    struct SymbolData {
        std::deque<Bar> bars;
        size_t current_index;

        SymbolData() : current_index(0) {}
    };

    std::map<std::string, SymbolData> symbol_data_;

    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> active_{false};
    std::atomic<bool> should_stop_{false};

    // Background thread
    std::thread replay_thread_;

    // Callbacks
    BarCallback bar_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;

    // Replay state
    std::atomic<int> bars_replayed_{0};
    int total_bars_{0};
    std::atomic<int> errors_{0};
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 5 of 19**: include/data/multi_symbol_data_manager.h

**File Information**:
- **Path**: `include/data/multi_symbol_data_manager.h`

- **Size**: 259 lines
- **Modified**: 2025-10-14 22:52:31

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include <map>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace sentio {
namespace data {

/**
 * @brief Snapshot of a symbol's latest data with staleness tracking
 *
 * Each symbol maintains its own update timeline. Staleness weighting
 * reduces influence of old data in signal ranking.
 */
struct SymbolSnapshot {
    Bar latest_bar;                     // Most recent bar
    uint64_t last_update_ms;            // Timestamp of last update
    double staleness_seconds;           // Age of data (seconds)
    double staleness_weight;            // Exponential decay weight: exp(-age/60)
    int forward_fill_count;             // How many times forward-filled
    bool is_valid;                      // Data is usable

    SymbolSnapshot()
        : last_update_ms(0)
        , staleness_seconds(0.0)
        , staleness_weight(1.0)
        , forward_fill_count(0)
        , is_valid(false) {}

    // Calculate staleness based on current time
    void update_staleness(uint64_t current_time_ms) {
        staleness_seconds = (current_time_ms - last_update_ms) / 1000.0;

        // Exponential decay: fresh = 1.0, 60s = 0.37, 120s = 0.14
        staleness_weight = std::exp(-staleness_seconds / 60.0);

        is_valid = (staleness_seconds < 300.0);  // Invalid after 5 minutes
    }
};

/**
 * @brief Synchronized snapshot of all symbols at a logical timestamp
 *
 * Not all symbols may be present (async data arrival). Missing symbols
 * are forward-filled from last known data.
 */
struct MultiSymbolSnapshot {
    uint64_t logical_timestamp_ms;                     // Logical time (aligned)
    std::map<std::string, SymbolSnapshot> snapshots;   // Symbol → data
    std::vector<std::string> missing_symbols;          // Symbols not updated
    int total_forward_fills;                           // Total symbols forward-filled
    bool is_complete;                                  // All symbols present
    double avg_staleness_seconds;                      // Average age across symbols

    MultiSymbolSnapshot()
        : logical_timestamp_ms(0)
        , total_forward_fills(0)
        , is_complete(false)
        , avg_staleness_seconds(0.0) {}

    // Check if snapshot is usable
    bool is_usable() const {
        // Must have at least 50% of symbols with fresh data
        int valid_count = 0;
        for (const auto& [symbol, snap] : snapshots) {
            if (snap.is_valid && snap.staleness_seconds < 120.0) {
                valid_count++;
            }
        }
        return valid_count >= (snapshots.size() / 2);
    }
};

/**
 * @brief Asynchronous multi-symbol data manager
 *
 * Core design principles:
 * 1. NON-BLOCKING: Never wait for all symbols, use latest available
 * 2. STALENESS WEIGHTING: Reduce influence of old data
 * 3. FORWARD FILL: Use last known price for missing data (max 5 fills)
 * 4. THREAD-SAFE: WebSocket updates from background thread
 *
 * Usage:
 *   auto snapshot = data_mgr->get_latest_snapshot();
 *   for (const auto& [symbol, data] : snapshot.snapshots) {
 *       double adjusted_strength = base_strength * data.staleness_weight;
 *   }
 */
class MultiSymbolDataManager {
public:
    struct Config {
        std::vector<std::string> symbols;          // Active symbols to track
        int max_forward_fills = 5;                 // Max consecutive forward fills
        double max_staleness_seconds = 300.0;      // Max age before invalid (5 min)
        int history_size = 500;                    // Bars to keep per symbol
        bool log_data_quality = true;              // Log missing/stale data
        bool backtest_mode = false;                // Disable timestamp validation for backtesting
    };

    explicit MultiSymbolDataManager(const Config& config);
    virtual ~MultiSymbolDataManager() = default;

    // === Main Interface ===

    /**
     * @brief Get latest snapshot for all symbols (non-blocking)
     *
     * Returns immediately with whatever data is available. Applies
     * staleness weighting and forward-fill for missing symbols.
     *
     * @return MultiSymbolSnapshot with latest data
     */
    MultiSymbolSnapshot get_latest_snapshot();

    /**
     * @brief Update a symbol's data (called from WebSocket/feed thread)
     *
     * Thread-safe update of symbol data. Validates and stores bar.
     *
     * @param symbol Symbol ticker
     * @param bar New bar data
     * @return true if update successful, false if validation failed
     */
    bool update_symbol(const std::string& symbol, const Bar& bar);

    /**
     * @brief Bulk update multiple symbols (for mock replay)
     *
     * @param bars Map of symbol → bar
     * @return Number of successful updates
     */
    int update_all(const std::map<std::string, Bar>& bars);

    // === History Access ===

    /**
     * @brief Get recent bars for a symbol (for volatility calculation)
     *
     * @param symbol Symbol ticker
     * @param count Number of bars to retrieve
     * @return Vector of recent bars (newest first)
     */
    std::vector<Bar> get_recent_bars(const std::string& symbol, int count) const;

    /**
     * @brief Get all history for a symbol
     *
     * @param symbol Symbol ticker
     * @return Deque of all bars (oldest first)
     */
    std::deque<Bar> get_all_bars(const std::string& symbol) const;

    // === Statistics & Monitoring ===

    /**
     * @brief Get data quality stats
     */
    struct DataQualityStats {
        std::map<std::string, int> update_counts;          // Symbol → updates
        std::map<std::string, double> avg_staleness;       // Symbol → avg age
        std::map<std::string, int> forward_fill_counts;    // Symbol → fills
        int total_updates;
        int total_forward_fills;
        int total_rejections;
        double overall_avg_staleness;
    };

    DataQualityStats get_quality_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get configured symbols
     */
    const std::vector<std::string>& get_symbols() const { return config_.symbols; }

protected:
    /**
     * @brief Validate incoming bar data
     *
     * @param symbol Symbol ticker
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    bool validate_bar(const std::string& symbol, const Bar& bar);

    /**
     * @brief Forward-fill missing symbol from last known bar
     *
     * @param symbol Symbol ticker
     * @param logical_time Timestamp to use for forward-filled bar
     * @return SymbolSnapshot with forward-filled data
     */
    SymbolSnapshot forward_fill_symbol(const std::string& symbol,
                                       uint64_t logical_time);

private:
    struct SymbolState {
        std::deque<Bar> history;           // Bar history
        Bar latest_bar;                    // Most recent bar
        uint64_t last_update_ms;           // Last update timestamp
        int update_count;                  // Total updates received
        int forward_fill_count;            // Consecutive forward fills
        int rejection_count;               // Rejected updates
        double cumulative_staleness;       // For avg calculation

        SymbolState()
            : last_update_ms(0)
            , update_count(0)
            , forward_fill_count(0)
            , rejection_count(0)
            , cumulative_staleness(0.0) {}
    };

    Config config_;
    std::map<std::string, SymbolState> symbol_states_;
    mutable std::mutex data_mutex_;

    // Statistics
    std::atomic<int> total_updates_{0};
    std::atomic<int> total_forward_fills_{0};
    std::atomic<int> total_rejections_{0};

    // Current time (for testing - can be injected)
    std::function<uint64_t()> time_provider_;

    uint64_t get_current_time_ms() const {
        if (time_provider_) {
            return time_provider_();
        }
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

public:
    // For testing: inject time provider
    void set_time_provider(std::function<uint64_t()> provider) {
        time_provider_ = provider;
    }

private:
    // Helper functions
    std::string join_symbols() const;
    std::string join_vector(const std::vector<std::string>& vec) const;
};

} // namespace data
} // namespace sentio

```

## 📄 **FILE 6 of 19**: include/features/indicators.h

**File Information**:
- **Path**: `include/features/indicators.h`

- **Size**: 480 lines
- **Modified**: 2025-10-07 22:15:20

- **Type**: .h

```text
#pragma once

#include "features/rolling.h"
#include <cmath>
#include <deque>
#include <limits>

namespace sentio {
namespace features {
namespace ind {

// =============================================================================
// MACD (Moving Average Convergence Divergence)
// Fast EMA (12), Slow EMA (26), Signal Line (9)
// =============================================================================

struct MACD {
    roll::EMA fast{12};
    roll::EMA slow{26};
    roll::EMA sig{9};
    double macd = std::numeric_limits<double>::quiet_NaN();
    double signal = std::numeric_limits<double>::quiet_NaN();
    double hist = std::numeric_limits<double>::quiet_NaN();

    void update(double close) {
        double fast_val = fast.update(close);
        double slow_val = slow.update(close);
        macd = fast_val - slow_val;
        signal = sig.update(macd);
        hist = macd - signal;
    }

    bool is_ready() const {
        return fast.is_ready() && slow.is_ready() && sig.is_ready();
    }

    void reset() {
        fast.reset();
        slow.reset();
        sig.reset();
        macd = signal = hist = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Stochastic Oscillator (%K, %D, Slow)
// Uses rolling highs/lows for efficient calculation
// =============================================================================

struct Stoch {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    roll::EMA d3{3};
    roll::EMA slow3{3};
    double k = std::numeric_limits<double>::quiet_NaN();
    double d = std::numeric_limits<double>::quiet_NaN();
    double slow = std::numeric_limits<double>::quiet_NaN();

    explicit Stoch(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            k = d = slow = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double denom = hi.max() - lo.min();
        k = (denom == 0) ? 50.0 : 100.0 * (close - lo.min()) / denom;
        d = d3.update(k);
        slow = slow3.update(d);
    }

    bool is_ready() const {
        return hi.full() && lo.full() && d3.is_ready() && slow3.is_ready();
    }

    void reset() {
        hi.reset();
        lo.reset();
        d3.reset();
        slow3.reset();
        k = d = slow = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Williams %R
// Measures overbought/oversold levels (-100 to 0)
// =============================================================================

struct WilliamsR {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double r = std::numeric_limits<double>::quiet_NaN();

    explicit WilliamsR(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            r = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double range = hi.max() - lo.min();
        r = (range == 0) ? -50.0 : -100.0 * (hi.max() - close) / range;
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        r = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Bollinger Bands
// Mean ± k * StdDev with %B and bandwidth indicators
// =============================================================================

struct Boll {
    roll::Ring<double> win;
    int k = 2;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sd = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();
    double percent_b = std::numeric_limits<double>::quiet_NaN();
    double bandwidth = std::numeric_limits<double>::quiet_NaN();

    Boll(int period = 20, int k_ = 2) : win(period), k(k_) {}

    void update(double close) {
        win.push(close);

        if (!win.full()) {
            mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
            percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        mean = win.mean();
        sd = win.stdev();
        upper = mean + k * sd;
        lower = mean - k * sd;

        // %B: Position within bands (0 = lower, 1 = upper)
        double band_range = upper - lower;
        percent_b = (band_range == 0) ? 0.5 : (close - lower) / band_range;

        // Bandwidth: Normalized band width
        bandwidth = (mean == 0) ? 0.0 : (upper - lower) / mean;
    }

    bool is_ready() const {
        return win.full();
    }

    void reset() {
        win.reset();
        mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
        percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Donchian Channels
// Rolling high/low breakout levels
// =============================================================================

struct Donchian {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double up = std::numeric_limits<double>::quiet_NaN();
    double dn = std::numeric_limits<double>::quiet_NaN();
    double mid = std::numeric_limits<double>::quiet_NaN();

    explicit Donchian(int lookback = 20) : hi(lookback), lo(lookback) {}

    void update(double high, double low) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            up = dn = mid = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        up = hi.max();
        dn = lo.min();
        mid = 0.5 * (up + dn);
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        up = dn = mid = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// RSI (Relative Strength Index) - Wilder's Method
// Uses Wilder's smoothing for gains/losses
// =============================================================================

struct RSI {
    roll::Wilder avgGain;
    roll::Wilder avgLoss;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit RSI(int period = 14) : avgGain(period), avgLoss(period) {}

    void update(double close) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        double change = close - prevClose;
        prevClose = close;

        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? -change : 0.0;

        double g = avgGain.update(gain);
        double l = avgLoss.update(loss);

        if (!avgLoss.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double rs = (l == 0) ? INFINITY : g / l;
        value = 100.0 - 100.0 / (1.0 + rs);
    }

    bool is_ready() const {
        return avgGain.is_ready() && avgLoss.is_ready();
    }

    void reset() {
        avgGain.reset();
        avgLoss.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ATR (Average True Range) - Wilder's Method
// Volatility indicator using true range
// =============================================================================

struct ATR {
    roll::Wilder w;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ATR(int period = 14) : w(period) {}

    void update(double high, double low, double close) {
        double tr;
        if (std::isnan(prevClose)) {
            tr = high - low;
        } else {
            tr = std::max({
                high - low,
                std::fabs(high - prevClose),
                std::fabs(low - prevClose)
            });
        }
        prevClose = close;
        value = w.update(tr);

        if (!w.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
        }
    }

    bool is_ready() const {
        return w.is_ready();
    }

    void reset() {
        w.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ROC (Rate of Change) %
// Momentum indicator: (close - close_n_periods_ago) / close_n_periods_ago * 100
// =============================================================================

struct ROC {
    std::deque<double> q;
    int period;
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ROC(int p) : period(p) {}

    void update(double close) {
        q.push_back(close);
        if (static_cast<int>(q.size()) < period + 1) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }
        double past = q.front();
        q.pop_front();
        value = (past == 0) ? 0.0 : 100.0 * (close - past) / past;
    }

    bool is_ready() const {
        return static_cast<int>(q.size()) >= period;
    }

    void reset() {
        q.clear();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// CCI (Commodity Channel Index)
// Measures deviation from typical price mean
// =============================================================================

struct CCI {
    roll::Ring<double> tp; // Typical price ring
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit CCI(int period = 20) : tp(period) {}

    void update(double high, double low, double close) {
        double typical_price = (high + low + close) / 3.0;
        tp.push(typical_price);

        if (!tp.full()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double mean = tp.mean();
        double sd = tp.stdev();

        if (sd == 0 || std::isnan(sd)) {
            value = 0.0;
            return;
        }

        // Approximate mean deviation using stdev (empirical factor ~1.25)
        // For exact MD, maintain parallel queue (omitted for O(1) performance)
        value = (typical_price - mean) / (0.015 * sd * 1.25331413732);
    }

    bool is_ready() const {
        return tp.full();
    }

    void reset() {
        tp.reset();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// OBV (On-Balance Volume)
// Cumulative volume indicator based on price direction
// =============================================================================

struct OBV {
    double value = 0.0;
    double prevClose = std::numeric_limits<double>::quiet_NaN();

    void update(double close, double volume) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        if (close > prevClose) {
            value += volume;
        } else if (close < prevClose) {
            value -= volume;
        }
        // No change if close == prevClose

        prevClose = close;
    }

    void reset() {
        value = 0.0;
        prevClose = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// VWAP (Volume Weighted Average Price)
// Intraday indicator: cumulative (price * volume) / cumulative volume
// =============================================================================

struct VWAP {
    double sumPV = 0.0;
    double sumV = 0.0;
    double value = std::numeric_limits<double>::quiet_NaN();

    void update(double price, double volume) {
        sumPV += price * volume;
        sumV += volume;
        if (sumV > 0) {
            value = sumPV / sumV;
        }
    }

    void reset() {
        sumPV = 0.0;
        sumV = 0.0;
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Keltner Channels
// EMA ± (ATR * multiplier)
// =============================================================================

struct Keltner {
    roll::EMA ema;
    ATR atr;
    double multiplier = 2.0;
    double middle = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();

    Keltner(int ema_period = 20, int atr_period = 10, double mult = 2.0)
        : ema(ema_period), atr(atr_period), multiplier(mult) {}

    void update(double high, double low, double close) {
        middle = ema.update(close);
        atr.update(high, low, close);

        if (!atr.is_ready()) {
            upper = lower = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double atr_val = atr.value;
        upper = middle + multiplier * atr_val;
        lower = middle - multiplier * atr_val;
    }

    bool is_ready() const {
        return ema.is_ready() && atr.is_ready();
    }

    void reset() {
        ema.reset();
        atr.reset();
        middle = upper = lower = std::numeric_limits<double>::quiet_NaN();
    }
};

} // namespace ind
} // namespace features
} // namespace sentio

```

## 📄 **FILE 7 of 19**: include/features/unified_feature_engine.h

**File Information**:
- **Path**: `include/features/unified_feature_engine.h`

- **Size**: 232 lines
- **Modified**: 2025-10-15 03:28:55

- **Type**: .h

```text
#pragma once

#include "common/types.h"
#include "features/indicators.h"
#include "features/scaler.h"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace features {

// =============================================================================
// Configuration for Production-Grade Unified Feature Engine
// =============================================================================

struct EngineConfig {
    // Feature toggles
    bool time = true;         // Time-of-day features (8 features)
    bool patterns = true;     // Candlestick patterns (5 features)
    bool momentum = true;
    bool volatility = true;
    bool volume = true;
    bool statistics = true;

    // Indicator periods
    int rsi14 = 14;
    int rsi21 = 21;
    int atr14 = 14;
    int bb20 = 20;
    int bb_k = 2;
    int stoch14 = 14;
    int will14 = 14;
    int macd_fast = 12;
    int macd_slow = 26;
    int macd_sig = 9;
    int roc5 = 5;
    int roc10 = 10;
    int roc20 = 20;
    int cci20 = 20;
    int don20 = 20;
    int keltner_ema = 20;
    int keltner_atr = 10;
    double keltner_mult = 2.0;

    // Moving averages
    int sma10 = 10;
    int sma20 = 20;
    int sma50 = 50;
    int ema10 = 10;
    int ema20 = 20;
    int ema50 = 50;

    // Normalization
    bool normalize = true;
    bool robust = false;
};

// =============================================================================
// Feature Schema with Hash for Model Compatibility
// =============================================================================

struct Schema {
    std::vector<std::string> names;
    std::string sha1_hash;  // Hash of (names + config) for version control
};

// =============================================================================
// Production-Grade Unified Feature Engine
//
// Key Features:
// - Stable, deterministic feature ordering (std::map, not unordered_map)
// - O(1) incremental updates using Welford's algorithm and ring buffers
// - Schema hash for model compatibility checks
// - Complete public API: update(), features_view(), names(), schema()
// - Serialization/restoration for online learning
// - Zero duplicate calculations (shared statistics cache)
// =============================================================================

class UnifiedFeatureEngine {
public:
    explicit UnifiedFeatureEngine(EngineConfig cfg = {});

    // ==========================================================================
    // Core API
    // ==========================================================================

    /**
     * Idempotent update with new bar. Returns true if state advanced.
     */
    bool update(const Bar& b);

    /**
     * Get contiguous feature vector in stable order (ready for model input).
     * Values may contain NaN until warmup complete for each feature.
     */
    const std::vector<double>& features_view() const { return feats_; }

    /**
     * Get canonical feature names in fixed, deterministic order.
     */
    const std::vector<std::string>& names() const { return schema_.names; }

    /**
     * Get schema with hash for model compatibility checks.
     */
    const Schema& schema() const { return schema_; }

    /**
     * Count of bars remaining before all features are non-NaN.
     */
    int warmup_remaining() const;

    /**
     * Get list of indicator names that are not yet ready (for debugging).
     */
    std::vector<std::string> get_unready_indicators() const;

    /**
     * Reset engine to initial state.
     */
    void reset();

    /**
     * Serialize engine state for persistence (online learning resume).
     */
    std::string serialize() const;

    /**
     * Restore engine state from serialized blob.
     */
    void restore(const std::string& blob);

    /**
     * Check if engine has processed at least one bar.
     */
    bool is_seeded() const { return seeded_; }

    /**
     * Get number of bars processed.
     */
    size_t bar_count() const { return bar_count_; }

    /**
     * Get normalization scaler (for external persistence).
     */
    const Scaler& get_scaler() const { return scaler_; }

    /**
     * Set scaler from external source (for trained models).
     */
    void set_scaler(const Scaler& s) { scaler_ = s; }

private:
    void build_schema_();
    void recompute_vector_();
    std::string compute_schema_hash_(const std::string& concatenated_names);

    EngineConfig cfg_;
    Schema schema_;

    // ==========================================================================
    // Indicators (all O(1) incremental)
    // ==========================================================================

    ind::RSI rsi14_;
    ind::RSI rsi21_;
    ind::ATR atr14_;
    ind::Boll bb20_;
    ind::Stoch stoch14_;
    ind::WilliamsR will14_;
    ind::MACD macd_;
    ind::ROC roc5_, roc10_, roc20_;
    ind::CCI cci20_;
    ind::Donchian don20_;
    ind::Keltner keltner_;
    ind::OBV obv_;
    ind::VWAP vwap_;

    // Moving averages
    roll::EMA ema10_, ema20_, ema50_;
    roll::Ring<double> sma10_ring_, sma20_ring_, sma50_ring_;

    // ==========================================================================
    // State
    // ==========================================================================

    bool seeded_ = false;
    size_t bar_count_ = 0;
    uint64_t prevTimestamp_ = 0;  // For time features
    double prevClose_ = std::numeric_limits<double>::quiet_NaN();
    double prevOpen_ = std::numeric_limits<double>::quiet_NaN();
    double prevHigh_ = std::numeric_limits<double>::quiet_NaN();
    double prevLow_ = std::numeric_limits<double>::quiet_NaN();
    double prevVolume_ = std::numeric_limits<double>::quiet_NaN();

    // For computing 1-bar return (current close vs previous close)
    double prevPrevClose_ = std::numeric_limits<double>::quiet_NaN();

    // Feature vector (stable order, contiguous for model input)
    std::vector<double> feats_;

    // Normalization
    Scaler scaler_;
    std::vector<std::vector<double>> normalization_buffer_;  // For fit()
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Compute SHA1 hash of string (for schema versioning).
 */
std::string sha1_hex(const std::string& s);

/**
 * Safe return calculation (handles NaN and division by zero).
 */
inline double safe_return(double current, double previous) {
    if (std::isnan(previous) || previous == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (current / previous) - 1.0;
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 8 of 19**: include/learning/online_predictor.h

**File Information**:
- **Path**: `include/learning/online_predictor.h`

- **Size**: 133 lines
- **Modified**: 2025-10-07 00:37:12

- **Type**: .h

```text
#pragma once

#include <Eigen/Dense>
#include <vector>
#include <string>
#include <fstream>
#include <deque>
#include <memory>
#include <cmath>

namespace sentio {
namespace learning {

/**
 * Online learning predictor that eliminates train/inference parity issues
 * Uses Exponentially Weighted Recursive Least Squares (EWRLS)
 */
class OnlinePredictor {
public:
    struct Config {
        double lambda;
        double initial_variance;
        double regularization;
        int warmup_samples;
        bool adaptive_learning;
        double min_lambda;
        double max_lambda;
        
        Config()
            : lambda(0.995),
              initial_variance(100.0),
              regularization(0.01),
              warmup_samples(100),
              adaptive_learning(true),
              min_lambda(0.990),
              max_lambda(0.999) {}
    };
    
    struct PredictionResult {
        double predicted_return;
        double confidence;
        double volatility_estimate;
        bool is_ready;
        
        PredictionResult()
            : predicted_return(0.0),
              confidence(0.0),
              volatility_estimate(0.0),
              is_ready(false) {}
    };
    
    explicit OnlinePredictor(size_t num_features, const Config& config = Config());
    
    // Main interface - predict and optionally update
    PredictionResult predict(const std::vector<double>& features);
    void update(const std::vector<double>& features, double actual_return);
    
    // Combined predict-then-update for efficiency
    PredictionResult predict_and_update(const std::vector<double>& features, 
                                        double actual_return);
    
    // Adaptive learning rate based on recent volatility
    void adapt_learning_rate(double market_volatility);
    
    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);
    
    // Diagnostics
    double get_recent_rmse() const;
    double get_directional_accuracy() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }
    
private:
    Config config_;
    size_t num_features_;
    int samples_seen_;
    
    // EWRLS parameters
    Eigen::VectorXd theta_;      // Model parameters
    Eigen::MatrixXd P_;          // Covariance matrix
    double current_lambda_;      // Adaptive forgetting factor
    
    // Performance tracking
    std::deque<double> recent_errors_;
    std::deque<bool> recent_directions_;
    static constexpr size_t HISTORY_SIZE = 100;
    
    // Volatility estimation for adaptive learning
    std::deque<double> recent_returns_;
    double estimate_volatility() const;
    
    // Numerical stability
    void ensure_positive_definite();
    static constexpr double EPSILON = 1e-8;
};

/**
 * Ensemble of online predictors for different time horizons
 */
class MultiHorizonPredictor {
public:
    struct HorizonConfig {
        int horizon_bars;
        double weight;
        OnlinePredictor::Config predictor_config;
        
        HorizonConfig()
            : horizon_bars(1),
              weight(1.0),
              predictor_config() {}
    };
    
    explicit MultiHorizonPredictor(size_t num_features);
    
    // Add predictors for different horizons
    void add_horizon(int bars, double weight = 1.0);
    
    // Ensemble prediction
    OnlinePredictor::PredictionResult predict(const std::vector<double>& features);
    
    // Update all predictors
    void update(int bars_ago, const std::vector<double>& features, double actual_return);
    
private:
    size_t num_features_;
    std::vector<std::unique_ptr<OnlinePredictor>> predictors_;
    std::vector<HorizonConfig> configs_;
};

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 9 of 19**: include/strategy/multi_symbol_oes_manager.h

**File Information**:
- **Path**: `include/strategy/multi_symbol_oes_manager.h`

- **Size**: 215 lines
- **Modified**: 2025-10-14 21:14:34

- **Type**: .h

```text
#pragma once

#include "strategy/online_ensemble_strategy.h"
#include "strategy/signal_output.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Manages 6 independent OnlineEnsemble strategy instances
 *
 * Each symbol gets its own:
 * - OnlineEnsembleStrategy instance
 * - EWRLS predictor
 * - Feature engine
 * - Learning state
 *
 * This ensures no cross-contamination between symbols and allows
 * pure independent signal generation.
 *
 * Usage:
 *   MultiSymbolOESManager oes_mgr(config, data_mgr);
 *   auto signals = oes_mgr.generate_all_signals();
 *   oes_mgr.update_all(realized_pnls);
 */
class MultiSymbolOESManager {
public:
    struct Config {
        std::vector<std::string> symbols;              // Active symbols
        OnlineEnsembleStrategy::OnlineEnsembleConfig base_config;  // Base config for all OES
        bool independent_learning = true;              // Each symbol learns independently
        bool share_features = false;                   // Share feature engine (NOT RECOMMENDED)

        // Symbol-specific overrides (optional)
        std::map<std::string, OnlineEnsembleStrategy::OnlineEnsembleConfig> symbol_configs;
    };

    /**
     * @brief Construct manager with data source
     *
     * @param config Configuration
     * @param data_mgr Data manager for multi-symbol data
     */
    explicit MultiSymbolOESManager(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr
    );

    ~MultiSymbolOESManager() = default;

    // === Signal Generation ===

    /**
     * @brief Generate signals for all symbols
     *
     * Returns map of symbol → signal. Only symbols with valid data
     * will have signals.
     *
     * @return Map of symbol → SignalOutput
     */
    std::map<std::string, SignalOutput> generate_all_signals();

    /**
     * @brief Generate signal for specific symbol
     *
     * @param symbol Symbol ticker
     * @return SignalOutput for symbol (or empty if data invalid)
     */
    SignalOutput generate_signal(const std::string& symbol);

    // === Learning Updates ===

    /**
     * @brief Update all OES instances with realized P&L
     *
     * @param realized_pnls Map of symbol → realized P&L
     */
    void update_all(const std::map<std::string, double>& realized_pnls);

    /**
     * @brief Update specific symbol with realized P&L
     *
     * @param symbol Symbol ticker
     * @param realized_pnl Realized P&L from last trade
     */
    void update(const std::string& symbol, double realized_pnl);

    /**
     * @brief Process new bar for all symbols
     *
     * Called each bar to update feature engines and check learning state.
     */
    void on_bar();

    // === Warmup ===

    /**
     * @brief Warmup all OES instances from historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup_all(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Warmup specific symbol
     *
     * @param symbol Symbol ticker
     * @param bars Historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::string& symbol, const std::vector<Bar>& bars);

    // === Configuration ===

    /**
     * @brief Update configuration for all symbols
     *
     * @param new_config New base configuration
     */
    void update_config(const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    /**
     * @brief Update configuration for specific symbol
     *
     * @param symbol Symbol ticker
     * @param new_config New configuration
     */
    void update_config(const std::string& symbol,
                      const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    // === Diagnostics ===

    /**
     * @brief Get performance metrics for all symbols
     *
     * @return Map of symbol → performance metrics
     */
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
    get_all_performance_metrics() const;

    /**
     * @brief Get performance metrics for specific symbol
     *
     * @param symbol Symbol ticker
     * @return Performance metrics
     */
    OnlineEnsembleStrategy::PerformanceMetrics
    get_performance_metrics(const std::string& symbol) const;

    /**
     * @brief Check if all OES instances are ready
     *
     * @return true if all have sufficient warmup samples
     */
    bool all_ready() const;

    /**
     * @brief Get ready status for each symbol
     *
     * @return Map of symbol → ready status
     */
    std::map<std::string, bool> get_ready_status() const;

    /**
     * @brief Get learning state for all symbols
     *
     * @return Map of symbol → learning state
     */
    std::map<std::string, OnlineEnsembleStrategy::LearningState>
    get_all_learning_states() const;

    /**
     * @brief Get OES instance for symbol (for direct access)
     *
     * @param symbol Symbol ticker
     * @return Pointer to OES instance (nullptr if not found)
     */
    OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol);

    /**
     * @brief Get const OES instance for symbol
     *
     * @param symbol Symbol ticker
     * @return Const pointer to OES instance (nullptr if not found)
     */
    const OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol) const;

private:
    /**
     * @brief Get latest bar for symbol from data manager
     *
     * @param symbol Symbol ticker
     * @param bar Output bar
     * @return true if valid bar available
     */
    bool get_latest_bar(const std::string& symbol, Bar& bar);

    Config config_;
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr_;

    // Map of symbol → OES instance
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_instances_;

    // Statistics
    int total_signals_generated_{0};
    int total_updates_{0};
};

} // namespace sentio

```

## 📄 **FILE 10 of 19**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 240 lines
- **Modified**: 2025-10-15 03:27:23

- **Type**: .h

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "strategy/market_regime_detector.h"
#include "strategy/regime_parameter_manager.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Regime detection parameters
        bool enable_regime_detection = false;  // Enable regime-aware parameter switching
        int regime_check_interval = 100;       // Check regime every N bars
        int regime_lookback_period = 100;      // Bars to analyze for regime detection

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Predictor training (for warmup)
    void train_predictor(const std::vector<double>& features, double realized_return);
    std::vector<double> extract_features(const Bar& current_bar);

    // Feature caching support (for Optuna optimization speedup)
    void set_external_features(const std::vector<double>* features) {
        external_features_ = features;
        skip_feature_engine_update_ = (features != nullptr);
    }

    // Runtime configuration update (for mid-day optimization)
    void update_config(const OnlineEnsembleConfig& new_config) {
        config_ = new_config;
        // CRITICAL: Update member variables used by determine_signal()
        current_buy_threshold_ = new_config.buy_threshold;
        current_sell_threshold_ = new_config.sell_threshold;
    }

    // Get current thresholds (for PSM decision logic)
    double get_current_buy_threshold() const { return current_buy_threshold_; }
    double get_current_sell_threshold() const { return current_sell_threshold_; }

    // Learning state management
    struct LearningState {
        int64_t last_trained_bar_id = -1;      // Global bar ID of last training
        int last_trained_bar_index = -1;       // Index of last trained bar
        int64_t last_trained_timestamp_ms = 0; // Timestamp of last training
        bool is_warmed_up = false;              // Feature engine ready
        bool is_learning_current = true;        // Learning is up-to-date
        int bars_behind = 0;                    // How many bars behind
    };

    LearningState get_learning_state() const { return learning_state_; }
    bool ensure_learning_current(const Bar& bar);  // Catch up if needed
    bool is_learning_current() const { return learning_state_.is_learning_current; }

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const {
        // Check both predictor warmup AND feature engine warmup
        return samples_seen_ >= config_.warmup_samples &&
               feature_engine_->warmup_remaining() == 0;
    }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering (production-grade with O(1) updates, 45 features)
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::shared_ptr<const std::vector<double>> features;  // Shared, immutable
        double entry_price;
        bool is_long;
    };

    struct PendingUpdate {
        std::array<HorizonPrediction, 3> horizons;  // Fixed size for 3 horizons
        uint8_t count = 0;  // Track actual count (1-3)
    };

    std::map<int, PendingUpdate> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Learning state tracking
    LearningState learning_state_;
    std::deque<Bar> missed_bars_;  // Queue of bars that need training

    // External feature support for caching
    const std::vector<double>* external_features_ = nullptr;
    bool skip_feature_engine_update_ = false;

    // Regime detection (optional)
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<RegimeParameterManager> regime_param_manager_;
    MarketRegime current_regime_;
    int bars_since_regime_check_;

    // Private methods
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);
    void check_and_update_regime();  // Regime detection method

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

## 📄 **FILE 11 of 19**: megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md

**File Information**:
- **Path**: `megadocs/ROTATION_TRADING_NAN_BUG_REPORT.md`

- **Size**: 419 lines
- **Modified**: 2025-10-15 03:18:19

- **Type**: .md

```text
# Rotation Trading NaN Features Bug Report

**Date:** October 15, 2025
**Severity:** CRITICAL
**Status:** PARTIALLY RESOLVED
**Impact:** System generates 48,425 signals but 0 trades due to predictor receiving NaN features

---

## Executive Summary

Multi-symbol rotation trading system generates signals for all 6 leveraged ETFs but executes zero trades. Root cause: OnlinePredictor receives NaN feature values (Bollinger Bands features 25-29), preventing learning and causing all predictions to return neutral signals (prob=0.5).

**Two Critical Bugs Identified:**

1. **FIXED:** Predictor warmup requirement preventing predictions (predictor.is_ready=false)
2. **ONGOING:** Bollinger Bands features remain NaN despite 780-bar warmup

---

## Bug #1: Predictor Warmup Requirement (FIXED)

### Symptoms
- Predictor returns `predicted_return=0, confidence=0, is_ready=0`
- After 781 bars processed (780 warmup + 1 trading)
- Features extracted successfully (58 features, non-empty)

### Root Cause
**Warmup only updates feature engine, never trains the predictor.**

Warmup flow:
```
rotation_trade_command.cpp::execute_mock_trading()
  └─> backend_->warmup(warmup_data)
      └─> oes_manager_->warmup_all(symbol_bars)
          └─> oes->on_bar(bar)  [called 780 times per symbol]
              └─> feature_engine_->update(bar)  ✅ Feature engine updated
              └─> predictor->update(features, return)  ❌ NEVER CALLED
```

**Result:** Feature engine has 780 bars, but predictor has 0 training samples.

Since `predictor.samples_seen_ = 0 < warmup_samples (50)`, `is_ready()` returns false.

### Fix Applied
**File:** `src/strategy/online_ensemble_strategy.cpp:36`

**Before:**
```cpp
predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
```

**After:**
```cpp
predictor_config.warmup_samples = 0;  // No warmup - start predicting immediately
```

**Rationale:** Predictor starts with high uncertainty (`initial_variance=100.0`) and learns online as trades execute. Strategy-level warmup ensures feature engine is ready before first signal.

---

## Bug #2: Bollinger Bands NaN Features (ONGOING)

### Symptoms
```
[OnlinePredictor] WARNING: Features contain NaN/inf! Skipping update.
[OnlinePredictor] NaN feature indices: 25(nan) 26(nan) 27(nan) 28(nan) 29(nan)
Total features: 58
```

### Feature Mapping
| Index | Feature | Source Indicator |
|-------|---------|-----------------|
| 25 | bb20_.mean | Bollinger Bands 20-period mean |
| 26 | bb20_.sd | Bollinger Bands standard deviation |
| 27 | bb20_.upper | Upper band (mean + 2*sd) |
| 28 | bb20_.lower | Lower band (mean - 2*sd) |
| 29 | bb20_.percent_b | Price position within bands |

### Debug Evidence
```
[FeatureEngine #0] BB NaN detected! bar_count=0, bb20_.win.size=1, bb20_.win.full=0
[FeatureEngine #1] BB NaN detected! bar_count=1, bb20_.win.size=2, bb20_.win.full=0
[FeatureEngine #2] BB NaN detected! bar_count=2, bb20_.win.size=3, bb20_.win.full=0
[FeatureEngine #3] BB NaN detected! bar_count=3, bb20_.win.size=4, bb20_.win.full=0
[FeatureEngine #4] BB NaN detected! bar_count=4, bb20_.win.size=5, bb20_.win.full=0
```

**First 5 warnings during warmup (expected - BB needs 20 bars to be full).**

But during trading:
```
[OES::generate_signal #1] samples_seen=781, features.size=58, has_NaN=YES
[OES::generate_signal #4] samples_seen=782, features.size=58, has_NaN=YES
```

**Features still contain NaN after 781+ bars!**

### Expected vs Actual
- **Expected:** After 780 warmup bars, BB window has 20 bars → `bb20_.win.full()` returns true → BB values computed
- **Actual:** Features still contain NaN during trading → predictor skips all updates → no learning → neutral signals only

### Hypotheses Under Investigation
1. ✅ **Feature engine reset after warmup:** Debug shows `on_bar()` calls during warmup increment `bar_count`
2. ✅ **Multiple feature engines:** Each symbol has its own OES instance with separate feature engine (expected)
3. ⚠️ **New OES instances created after warmup:** Need to verify with constructor debug logging
4. ⚠️ **BB window not accumulating correctly:** Need to verify `bb20_.win.push()` is called during warmup

---

## Affected Modules and Source Files

### 1. Strategy Layer
**File:** `src/strategy/online_ensemble_strategy.cpp`
- `on_bar()` method - Updates feature engine but not predictor during warmup
- `generate_signal()` - Returns neutral when predictor not ready or features have NaN
- Constructor - Sets predictor warmup configuration
- **Lines Modified:** 36 (predictor warmup_samples), 88-90 (debug)

**File:** `include/strategy/online_ensemble_strategy.h`
- `is_ready()` inline - Checks `samples_seen_ >= warmup_samples`
- **Lines:** 147

### 2. Multi-Symbol OES Manager
**File:** `src/strategy/multi_symbol_oes_manager.cpp`
- `warmup()` method - Only calls `on_bar()`, never `update()`
- `on_bar()` method - Updates OES instances when data available in snapshot
- `generate_all_signals()` - Generates signals for all symbols with valid data
- **Lines:** 231-265 (warmup), 186-205 (on_bar), 40-127 (signal generation)

**File:** `include/strategy/multi_symbol_oes_manager.h`
- OES instance management (one instance per symbol)
- **Lines:** 45-50 (instance map)

### 3. Feature Engine
**File:** `src/features/unified_feature_engine.cpp`
- `update()` method - Calls `bb20_.update()` and `recompute_vector_()`
- `recompute_vector_()` - Extracts BB features (indices 25-29)
- **Lines:** 188-247 (update), 248-333 (recompute_vector_), 315-323 (BB NaN debug)

**File:** `include/features/unified_feature_engine.h`
- Feature schema definition (58 total features)
- BB indicator state (`bb20_` member)
- **Lines:** 67-80 (feature schema), 110 (bb20_ member)

### 4. Bollinger Bands Indicator
**File:** `include/features/indicators.h`
- `Boll` struct - Rolling window indicator
- `update()` method - Returns NaN if window not full (`!win.full()`)
- `is_ready()` method - Returns `win.full()`
- **Lines:** 85-116 (Boll definition)

**Code:**
```cpp
struct Boll {
    roll::Ring<double> win;
    int k = 2;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sd = std::numeric_limits<double>::quiet_NaN();
    // ...

    void update(double close) {
        win.push(close);

        if (!win.full()) {
            // ❌ All values remain NaN until window is full (20 bars)
            mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
            percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        mean = win.mean();
        sd = win.stdev();
        // ...
    }

    bool is_ready() const {
        return win.full();
    }
};
```

### 5. Online Predictor (EWRLS)
**File:** `src/learning/online_predictor.cpp`
- `predict()` method - Returns zeros if `!is_ready()`
- `update()` method - Skips update if features contain NaN
- `is_ready()` method - Checks `samples_seen_ >= config_.warmup_samples`
- **Lines:** 25-76 (predict), 78-185 (update), 90-109 (NaN feature detection)

**File:** `include/learning/online_predictor.h`
- Config struct with `warmup_samples` parameter
- **Lines:** 18-29 (Config definition)

### 6. Multi-Horizon Predictor
**File:** `src/learning/online_predictor.cpp`
- `MultiHorizonPredictor::predict()` - Aggregates predictions from multiple horizons
- `add_horizon()` - Creates OnlinePredictor instances with adjusted lambda
- **Lines:** 354-407 (MultiHorizonPredictor implementation)

### 7. Rotation Trading Backend
**File:** `src/backend/rotation_trading_backend.cpp`
- `warmup()` method - Calls `oes_manager_->warmup_all()`
- `on_bar()` method - Called during trading, updates OES and generates signals
- **Lines:** 63-90 (warmup), 226-246 (on_bar)

**File:** `include/backend/rotation_trading_backend.h`
- Backend configuration
- **Lines:** 28-75 (Config struct)

### 8. Rotation Trade Command (CLI)
**File:** `src/cli/rotation_trade_command.cpp`
- `load_warmup_data()` - Loads 780 bars per symbol from CSV
- `execute_mock_trading()` - Calls backend warmup before starting trading
- **Lines:** 203-290 (warmup data loading), 292-380 (mock trading execution)

**File:** `include/cli/rotation_trade_command.h`
- Command options and interface
- **Lines:** 29-56 (Options struct)

### 9. Data Management
**File:** `src/data/multi_symbol_data_manager.cpp`
- `get_latest_snapshot()` - Returns snapshot with latest bars for each symbol
- `update_symbol()` - Updates symbol state with new bar
- **Lines:** 25-113 (get_latest_snapshot), 115-156 (update_symbol)

**File:** `include/data/multi_symbol_data_manager.h`
- Snapshot structure
- Symbol state tracking
- **Lines:** 24-48 (MultiSymbolSnapshot), 50-66 (SymbolSnapshot)

### 10. Mock Multi-Symbol Feed
**File:** `src/data/mock_multi_symbol_feed.cpp`
- `replay_next_bar()` - Replays bars chronologically across all symbols
- Bar callback triggering (triggers backend `on_bar()`)
- **Lines:** 237-273 (replay logic)

**File:** `include/data/mock_multi_symbol_feed.h`
- Feed interface and configuration
- **Lines:** 18-51 (class definition)

### 11. Configuration
**File:** `config/rotation_strategy.json`
- `oes_config.warmup_samples: 100` - Strategy-level warmup
- `oes_config.bb_period: 20` - Bollinger Bands window size
- **Lines:** 22 (warmup_samples), 32 (bb_period)

---

## Impact Analysis

### Current State
```
Bars processed: 8211
Signals generated: 48,425
Trades executed: 0        ❌ ZERO TRADES
Positions opened: 0
Positions closed: 0
Total P&L: $0.00
MRD: 0.000%
```

### Expected State (After Full Fix)
```
Bars processed: 8211
Signals generated: 48,425
Trades executed: 1000+    ✅ Active trading
Positions opened: 500+
Positions closed: 500+
Total P&L: Variable
MRD: Target > 0.3%
```

---

## Next Steps

### Immediate Actions
1. ✅ **DONE:** Set `predictor_config.warmup_samples = 0`
2. 🔄 **IN PROGRESS:** Add OES constructor debug to detect multiple instances
3. 🔄 **IN PROGRESS:** Verify BB window accumulation during warmup
4. ⏳ **PENDING:** Add per-symbol BB readiness check before signal generation

### Alternative Solutions Under Consideration

#### Option A: Forward-fill BB features during warmup
Replace NaN with last valid value or 0.0 during first 20 bars.

**Pros:**
- Predictor can start learning immediately
- No modifications to indicator logic

**Cons:**
- Forward-filled features may mislead predictor
- Not addressing root cause

#### Option B: Start trading after BB warmup
Skip signal generation until all feature engines report `bb20_.is_ready()`.

**Pros:**
- Guarantees no NaN features
- Clean separation of concerns

**Cons:**
- Delays trading start by 20 bars per symbol
- Reduces training data for predictor

#### Option C: Feature-level warmup tracking
Track readiness per feature, skip unready features in prediction.

**Pros:**
- Gradual feature availability
- Predictor can start with partial features

**Cons:**
- Complex feature selection logic
- Predictor expects fixed-size feature vector

---

## Testing Plan

### Test 1: Verify Predictor Readiness
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup \
  --warmup-dir data/tmp/rotation_warmup 2>&1 | \
  grep "is_ready="
```

**Expected:** `is_ready=1` after warmup
**Actual:** `is_ready=0` (before fix), `is_ready=1` (after fix) ✅

### Test 2: Count NaN Features During Trading
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup 2>&1 | \
  grep "NaN feature indices" | wc -l
```

**Expected:** 0 (no NaN features after warmup)
**Actual:** 100+ (NaN features persist) ❌

### Test 3: Verify Trade Execution
```bash
./build/sentio_cli rotation-trade --mode mock \
  --data-dir data/tmp/rotation_warmup 2>&1 | \
  grep "Trades executed"
```

**Expected:** `Trades executed: 1000+`
**Actual:** `Trades executed: 0` ❌

---

## Related Issues

- **Data Availability:** Some symbols missing from snapshot during trading
  - SPXL and SDS frequently show "No data in snapshot"
  - Caused by different bar counts across symbols (7125-8211 bars)

- **Callback Timing:** Previously fixed - callback was per-symbol instead of per-timestamp
  - Fixed in `src/data/mock_multi_symbol_feed.cpp:267`

---

## References

### Debug Outputs
```
[FeatureEngine] WARNING: bb20_ not ready! bar_count=0, bb20_.win.size=1, bb20_.win.full=0
[OnlinePredictor] NaN feature indices: 25(nan) 26(nan) 27(nan) 28(nan) 29(nan)
[OES] generate_signal #1: predicted_return=0, confidence=0, is_ready=0
[OES::generate_signal #1] samples_seen=781, features.size=58, has_NaN=YES
```

### Key Code Locations
- Predictor warmup config: `src/strategy/online_ensemble_strategy.cpp:36`
- BB NaN check: `src/features/unified_feature_engine.cpp:316`
- Predictor NaN skip: `src/learning/online_predictor.cpp:91`
- Warmup execution: `src/strategy/multi_symbol_oes_manager.cpp:243`
- BB indicator logic: `include/features/indicators.h:100`

---

## Source Module Reference

### Complete List of Related Files

**Source Files (Implementation):**
1. `src/strategy/online_ensemble_strategy.cpp` - Main OES implementation with predictor integration
2. `src/strategy/multi_symbol_oes_manager.cpp` - Multi-symbol OES coordination and warmup
3. `src/features/unified_feature_engine.cpp` - Feature computation including Bollinger Bands
4. `src/learning/online_predictor.cpp` - EWRLS predictor with NaN detection
5. `src/backend/rotation_trading_backend.cpp` - Trading session management
6. `src/cli/rotation_trade_command.cpp` - CLI entry point and warmup data loading
7. `src/data/multi_symbol_data_manager.cpp` - Multi-symbol data synchronization
8. `src/data/mock_multi_symbol_feed.cpp` - CSV-based bar replay for backtesting

**Header Files (Interface):**
1. `include/strategy/online_ensemble_strategy.h` - OES interface and configuration
2. `include/strategy/multi_symbol_oes_manager.h` - Multi-symbol manager interface
3. `include/features/unified_feature_engine.h` - Feature engine interface and schema
4. `include/features/indicators.h` - Bollinger Bands indicator definition
5. `include/learning/online_predictor.h` - Predictor configuration and interface
6. `include/backend/rotation_trading_backend.h` - Backend configuration
7. `include/cli/rotation_trade_command.h` - CLI command interface
8. `include/data/multi_symbol_data_manager.h` - Data manager configuration
9. `include/data/mock_multi_symbol_feed.h` - Mock feed interface

**Configuration Files:**
1. `config/rotation_strategy.json` - Strategy parameters and symbol configuration

**Total Files:** 18 (8 source, 9 headers, 1 config)

---

**Bug Report Generated:** October 15, 2025
**Last Updated:** October 15, 2025
**Tracking ID:** ROTATION-NAN-001

```

## 📄 **FILE 12 of 19**: src/backend/rotation_trading_backend.cpp

**File Information**:
- **Path**: `src/backend/rotation_trading_backend.cpp`

- **Size**: 643 lines
- **Modified**: 2025-10-14 22:47:44

- **Type**: .cpp

```text
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

    utils::log_info("✓ Trading session started");
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

    // Liquidate all positions
    if (rotation_manager_->get_position_count() > 0) {
        utils::log_info("Liquidating all positions...");
        liquidate_all_positions("Session End");
    }

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

    // Step 4: Check for EOD
    int current_time_minutes = get_current_time_minutes();
    if (is_eod(current_time_minutes)) {
        utils::log_info("EOD reached - liquidating all positions");
        liquidate_all_positions("EOD");
        return true;
    }

    // Step 5: Make position decisions
    auto decisions = make_decisions(ranked_signals);

    // Log decisions
    if (config_.log_all_decisions) {
        for (const auto& decision : decisions) {
            log_decision(decision);
        }
    }

    // Step 6: Execute decisions
    for (const auto& decision : decisions) {
        if (decision.decision != RotationPositionManager::Decision::HOLD) {
            execute_decision(decision);
        }
    }

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

    for (const auto& [symbol, position] : positions) {
        RotationPositionManager::PositionDecision decision;
        decision.symbol = symbol;
        decision.decision = RotationPositionManager::Decision::EOD_EXIT;
        decision.position = position;
        decision.reason = reason;

        execute_decision(decision);
    }

    return true;
}

// === State Access ===

bool RotationTradingBackend::is_ready() const {
    return oes_manager_->all_ready();
}

double RotationTradingBackend::get_current_equity() const {
    double unrealized_pnl = rotation_manager_->get_total_unrealized_pnl();
    return current_cash_ + unrealized_pnl;
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

    for (const auto& [symbol, snap] : snapshot.snapshots) {
        current_prices[symbol] = snap.latest_bar.close;
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

    // Get execution price
    std::string side = (decision.decision == RotationPositionManager::Decision::ENTER_LONG) ?
                       "BUY" : "SELL";
    double execution_price = get_execution_price(decision.symbol, side);

    // Calculate position size
    int shares = 0;
    if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
        decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
        shares = calculate_position_size(decision);
    } else {
        // Exit - get current position size
        auto positions = rotation_manager_->get_positions();
        if (positions.count(decision.symbol) > 0) {
            // For now, assume 1 share (will be enhanced with actual position tracking)
            shares = static_cast<int>(current_cash_ * config_.rotation_config.capital_per_position /
                                     execution_price);
        }
    }

    // Execute with rotation manager
    bool success = rotation_manager_->execute_decision(decision, execution_price);

    if (success) {
        // Update cash
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            current_cash_ -= shares * execution_price;
            session_stats_.positions_opened++;
            session_stats_.trades_executed++;
        } else {
            current_cash_ += shares * execution_price;
            session_stats_.positions_closed++;
            session_stats_.trades_executed++;

            // Track realized P&L for learning
            realized_pnls_[decision.symbol] = decision.position.pnl;
        }

        // Track rotations
        if (decision.decision == RotationPositionManager::Decision::ROTATE_OUT) {
            session_stats_.rotations++;
        }

        // Log trade
        log_trade(decision, execution_price, shares);
    }

    return success;
}

double RotationTradingBackend::get_execution_price(
    const std::string& symbol,
    const std::string& side
) {
    auto snapshot = data_manager_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        utils::log_warning("No data for " + symbol + " - using 0.0");
        return 0.0;
    }

    // Use close price (in live trading, would use bid/ask)
    return snapshot.snapshots.at(symbol).latest_bar.close;
}

int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    double capital_allocated = current_cash_ * config_.rotation_config.capital_per_position;
    double price = get_execution_price(decision.symbol, "BUY");

    if (price <= 0.0) {
        return 0;
    }

    return static_cast<int>(capital_allocated / price);
}

void RotationTradingBackend::update_learning() {
    if (!realized_pnls_.empty()) {
        oes_manager_->update_all(realized_pnls_);
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
    int shares
) {
    if (!trade_log_.is_open()) {
        return;
    }

    json j;
    j["symbol"] = decision.symbol;
    j["action"] = (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
                   decision.decision == RotationPositionManager::Decision::ENTER_SHORT) ?
                  "ENTRY" : "EXIT";
    j["direction"] = (decision.signal.signal.signal_type == SignalType::LONG) ? "LONG" : "SHORT";
    j["price"] = execution_price;
    j["shares"] = shares;
    j["value"] = execution_price * shares;

    if (decision.decision != RotationPositionManager::Decision::ENTER_LONG &&
        decision.decision != RotationPositionManager::Decision::ENTER_SHORT) {
        j["pnl"] = decision.position.pnl;
        j["pnl_pct"] = decision.position.pnl_pct;
        j["bars_held"] = decision.position.bars_held;
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

    // Calculate total P&L
    session_stats_.total_pnl = session_stats_.current_equity - config_.starting_capital;
    session_stats_.total_pnl_pct = session_stats_.total_pnl / config_.starting_capital;

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
    // For mock mode, calculate from bar count
    // For live mode, would use actual time

    // Assume market opens at 9:30 AM, 390 bars per day (6.5 hours)
    // bar_id % 390 = minutes since open

    auto snapshot = data_manager_->get_latest_snapshot();
    if (snapshot.snapshots.empty()) {
        return 0;
    }

    // Get first symbol's bar_id
    auto first_snap = snapshot.snapshots.begin()->second;
    uint64_t bar_id = first_snap.latest_bar.bar_id;

    return static_cast<int>(bar_id % 390);
}

} // namespace sentio

```

## 📄 **FILE 13 of 19**: src/cli/rotation_trade_command.cpp

**File Information**:
- **Path**: `src/cli/rotation_trade_command.cpp`

- **Size**: 512 lines
- **Modified**: 2025-10-15 03:28:38

- **Type**: .cpp

```text
#include "cli/rotation_trade_command.h"
#include "common/utils.h"
#include "common/time_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <csignal>
#include <thread>
#include <chrono>

namespace sentio {
namespace cli {

// Static member for signal handling
static std::atomic<bool> g_shutdown_requested{false};

// Signal handler
static void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << " - initiating graceful shutdown...\n";
    g_shutdown_requested = true;
}

RotationTradeCommand::RotationTradeCommand()
    : options_() {
}

RotationTradeCommand::RotationTradeCommand(const Options& options)
    : options_(options) {
}

RotationTradeCommand::~RotationTradeCommand() {
}

int RotationTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--mode" && i + 1 < args.size()) {
            std::string mode = args[++i];
            options_.is_mock_mode = (mode == "mock" || mode == "backtest");
        } else if (arg == "--data-dir" && i + 1 < args.size()) {
            options_.data_dir = args[++i];
        } else if (arg == "--warmup-dir" && i + 1 < args.size()) {
            options_.warmup_dir = args[++i];
        } else if (arg == "--log-dir" && i + 1 < args.size()) {
            options_.log_dir = args[++i];
        } else if (arg == "--config" && i + 1 < args.size()) {
            options_.config_file = args[++i];
        } else if (arg == "--capital" && i + 1 < args.size()) {
            options_.starting_capital = std::stod(args[++i]);
        } else if (arg == "--help" || arg == "-h") {
            show_help();
            return 0;
        }
    }

    // Get Alpaca credentials from environment if in live mode
    if (!options_.is_mock_mode) {
        const char* api_key = std::getenv("ALPACA_PAPER_API_KEY");
        const char* secret_key = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (api_key) options_.alpaca_api_key = api_key;
        if (secret_key) options_.alpaca_secret_key = secret_key;
    }

    return execute_with_options();
}

void RotationTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli rotation-trade [OPTIONS]\n\n";
    std::cout << "Multi-symbol rotation trading system\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mode <mode>         Trading mode: 'live' or 'mock' (default: live)\n";
    std::cout << "  --data-dir <dir>      Data directory for CSV files (default: data/equities)\n";
    std::cout << "  --warmup-dir <dir>    Warmup data directory (default: data/equities)\n";
    std::cout << "  --log-dir <dir>       Log output directory (default: logs/rotation_trading)\n";
    std::cout << "  --config <file>       Configuration file (default: config/rotation_strategy.json)\n";
    std::cout << "  --capital <amount>    Starting capital (default: 100000.0)\n";
    std::cout << "  --help, -h            Show this help message\n\n";
    std::cout << "Environment Variables (for live mode):\n";
    std::cout << "  ALPACA_PAPER_API_KEY      Alpaca API key\n";
    std::cout << "  ALPACA_PAPER_SECRET_KEY   Alpaca secret key\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Mock trading (backtest)\n";
    std::cout << "  sentio_cli rotation-trade --mode mock --data-dir data/equities\n\n";
    std::cout << "  # Live paper trading\n";
    std::cout << "  export ALPACA_PAPER_API_KEY=your_key\n";
    std::cout << "  export ALPACA_PAPER_SECRET_KEY=your_secret\n";
    std::cout << "  sentio_cli rotation-trade --mode live\n";
}

int RotationTradeCommand::execute_with_options() {
    log_system("========================================");
    log_system("Multi-Symbol Rotation Trading System");
    log_system("========================================");
    log_system("");

    log_system("Mode: " + std::string(options_.is_mock_mode ? "MOCK (Backtest)" : "LIVE (Paper)"));
    log_system("Symbols: " + std::to_string(options_.symbols.size()) + " instruments");
    for (const auto& symbol : options_.symbols) {
        log_system("  - " + symbol);
    }
    log_system("");

    // Setup signal handlers
    setup_signal_handlers();

    // Execute based on mode
    if (options_.is_mock_mode) {
        return execute_mock_trading();
    } else {
        return execute_live_trading();
    }
}

RotationTradingBackend::Config RotationTradeCommand::load_config() {
    RotationTradingBackend::Config config;

    // Load from JSON if available
    std::ifstream file(options_.config_file);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load OES config
            if (j.contains("oes_config")) {
                auto oes = j["oes_config"];
                config.oes_config.buy_threshold = oes.value("buy_threshold", 0.53);
                config.oes_config.sell_threshold = oes.value("sell_threshold", 0.47);
                config.oes_config.neutral_zone = oes.value("neutral_zone", 0.06);
                config.oes_config.ewrls_lambda = oes.value("ewrls_lambda", 0.995);
                config.oes_config.warmup_samples = oes.value("warmup_samples", 100);
                config.oes_config.enable_bb_amplification = oes.value("enable_bb_amplification", true);
                config.oes_config.bb_amplification_factor = oes.value("bb_amplification_factor", 0.10);
                config.oes_config.bb_period = oes.value("bb_period", 20);
                config.oes_config.bb_std_dev = oes.value("bb_std_dev", 2.0);
                config.oes_config.bb_proximity_threshold = oes.value("bb_proximity_threshold", 0.30);
                config.oes_config.regularization = oes.value("regularization", 0.01);

                if (oes.contains("horizon_weights")) {
                    config.oes_config.horizon_weights = oes["horizon_weights"].get<std::vector<double>>();
                }
            }

            // Load signal aggregator config
            if (j.contains("signal_aggregator_config")) {
                auto agg = j["signal_aggregator_config"];
                config.aggregator_config.min_strength = agg.value("min_strength", 0.40);
            }

            // Load leverage boosts
            if (j.contains("symbols") && j["symbols"].contains("leverage_boosts")) {
                auto boosts = j["symbols"]["leverage_boosts"];
                for (const auto& symbol : options_.symbols) {
                    if (boosts.contains(symbol)) {
                        config.aggregator_config.leverage_boosts[symbol] = boosts[symbol];
                    }
                }
            }

            // Load rotation config
            if (j.contains("rotation_manager_config")) {
                auto rot = j["rotation_manager_config"];
                config.rotation_config.max_positions = rot.value("max_positions", 3);
                config.rotation_config.min_strength_to_enter = rot.value("min_strength_to_enter", 0.50);
                config.rotation_config.rotation_strength_delta = rot.value("rotation_strength_delta", 0.10);
                config.rotation_config.profit_target_pct = rot.value("profit_target_pct", 0.03);
                config.rotation_config.stop_loss_pct = rot.value("stop_loss_pct", 0.015);
            }

            log_system("✓ Loaded configuration from: " + options_.config_file);
        } catch (const std::exception& e) {
            log_system("⚠️  Failed to load config: " + std::string(e.what()));
            log_system("   Using default configuration");
        }
    } else {
        log_system("⚠️  Config file not found: " + options_.config_file);
        log_system("   Using default configuration");
    }

    // Set symbols and capital
    config.symbols = options_.symbols;
    config.starting_capital = options_.starting_capital;

    // Set output paths
    config.signal_log_path = options_.log_dir + "/signals.jsonl";
    config.decision_log_path = options_.log_dir + "/decisions.jsonl";
    config.trade_log_path = options_.log_dir + "/trades.jsonl";
    config.position_log_path = options_.log_dir + "/positions.jsonl";

    // Set broker credentials
    config.alpaca_api_key = options_.alpaca_api_key;
    config.alpaca_secret_key = options_.alpaca_secret_key;
    config.paper_trading = options_.paper_trading;

    return config;
}

std::map<std::string, std::vector<Bar>> RotationTradeCommand::load_warmup_data() {
    std::map<std::string, std::vector<Bar>> warmup_data;

    log_system("Loading warmup data...");

    for (const auto& symbol : options_.symbols) {
        std::string filename = options_.warmup_dir + "/" + symbol + "_RTH_NH.csv";

        std::ifstream file(filename);
        if (!file.is_open()) {
            log_system("⚠️  Could not open warmup file for " + symbol + ": " + filename);
            continue;
        }

        std::vector<Bar> bars;
        std::string line;

        // Skip header
        std::getline(file, line);

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::vector<std::string> tokens;
            std::string token;

            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }

            try {
                Bar bar;

                // Support both 6-column and 7-column formats
                if (tokens.size() == 7) {
                    // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert epoch seconds to ms
                    bar.open = std::stod(tokens[2]);
                    bar.high = std::stod(tokens[3]);
                    bar.low = std::stod(tokens[4]);
                    bar.close = std::stod(tokens[5]);
                    bar.volume = std::stoll(tokens[6]);
                } else if (tokens.size() >= 6) {
                    // Format: timestamp,open,high,low,close,volume
                    bar.timestamp_ms = std::stoull(tokens[0]);
                    bar.open = std::stod(tokens[1]);
                    bar.high = std::stod(tokens[2]);
                    bar.low = std::stod(tokens[3]);
                    bar.close = std::stod(tokens[4]);
                    bar.volume = std::stoll(tokens[5]);
                } else {
                    continue;  // Skip malformed lines
                }

                bars.push_back(bar);
            } catch (const std::exception& e) {
                // Skip malformed lines
                continue;
            }
        }

        if (options_.is_mock_mode) {
            // For mock mode, use last 1560 bars (4 blocks) for warmup
            // This ensures 50+ bars for indicator warmup (max_period=50) plus 100+ for predictor training
            if (bars.size() > 1560) {
                std::vector<Bar> warmup_bars(bars.end() - 1560, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (4 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        } else {
            // For live mode, use last 7800 bars (20 blocks) for warmup
            if (bars.size() > 7800) {
                std::vector<Bar> warmup_bars(bars.end() - 7800, bars.end());
                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) + " bars (20 blocks)");
            } else {
                warmup_data[symbol] = bars;
                log_system("  " + symbol + ": " + std::to_string(bars.size()) + " bars (all available)");
            }
        }
    }

    log_system("");
    return warmup_data;
}

int RotationTradeCommand::execute_mock_trading() {
    log_system("========================================");
    log_system("Mock Trading Mode (Backtest)");
    log_system("========================================");
    log_system("");

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    dm_config.backtest_mode = true;  // Disable timestamp validation for mock trading
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create backend (no broker for mock mode)
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, nullptr);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // Create mock feed
    data::MockMultiSymbolFeed::Config feed_config;
    for (const auto& symbol : options_.symbols) {
        feed_config.symbol_files[symbol] = options_.data_dir + "/" + symbol + "_RTH_NH.csv";
    }
    feed_config.replay_speed = 0.0;  // Instant replay for testing

    auto feed = std::make_shared<data::MockMultiSymbolFeed>(data_manager_, feed_config);

    // Set callback to trigger backend on each bar
    feed->set_bar_callback([this](const std::string& symbol, const Bar& bar) {
        if (g_shutdown_requested) {
            return;
        }
        backend_->on_bar();
    });

    // Create log directory if it doesn't exist
    std::string mkdir_cmd = "mkdir -p " + options_.log_dir;
    int result = system(mkdir_cmd.c_str());
    if (result != 0) {
        log_system("⚠️  Failed to create log directory: " + options_.log_dir);
    }

    // Start trading
    log_system("Starting mock trading session...");
    log_system("");

    if (!backend_->start_trading()) {
        log_system("❌ Failed to start trading!");
        return 1;
    }

    // Connect and start feed
    if (!feed->connect()) {
        log_system("❌ Failed to connect feed!");
        return 1;
    }

    if (!feed->start()) {
        log_system("❌ Failed to start feed!");
        return 1;
    }

    // Wait for replay to complete
    log_system("Waiting for backtest to complete...");
    while (feed->is_active() && !g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop trading
    backend_->stop_trading();
    log_system("");
    log_system("Mock trading session complete");
    log_system("");

    // Print summary
    print_summary(backend_->get_session_stats());

    return 0;
}

int RotationTradeCommand::execute_live_trading() {
    log_system("========================================");
    log_system("Live Trading Mode (Paper)");
    log_system("========================================");
    log_system("");

    // Check credentials
    if (options_.alpaca_api_key.empty() || options_.alpaca_secret_key.empty()) {
        log_system("❌ Missing Alpaca credentials!");
        log_system("   Set ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY");
        return 1;
    }

    // Load configuration
    auto config = load_config();

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = options_.symbols;
    data_manager_ = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create Alpaca client
    broker_ = std::make_shared<AlpacaClient>(
        options_.alpaca_api_key,
        options_.alpaca_secret_key,
        options_.paper_trading
    );

    // Test connection
    log_system("Testing Alpaca connection...");
    auto account = broker_->get_account();
    if (!account || account->cash < 0) {
        log_system("❌ Failed to connect to Alpaca!");
        return 1;
    }
    log_system("✓ Connected to Alpaca");
    log_system("  Cash: $" + std::to_string(account->cash));
    log_system("");

    // Create backend
    backend_ = std::make_unique<RotationTradingBackend>(config, data_manager_, broker_);

    // Load and warmup
    auto warmup_data = load_warmup_data();
    if (!backend_->warmup(warmup_data)) {
        log_system("❌ Warmup failed!");
        return 1;
    }
    log_system("✓ Warmup complete");
    log_system("");

    // TODO: Create Alpaca WebSocket feed for live bars
    // For now, this is a placeholder - the actual WebSocket integration
    // would go here using AlpacaMultiSymbolFeed

    log_system("⚠️  Live WebSocket feed not yet implemented");
    log_system("   Use mock mode for now");

    return 0;
}

void RotationTradeCommand::setup_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

bool RotationTradeCommand::is_eod() const {
    int minutes = get_minutes_since_open();
    return minutes >= 358;  // 3:58 PM ET
}

int RotationTradeCommand::get_minutes_since_open() const {
    // Get current ET time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Convert to ET (this is simplified - production should use proper timezone handling)
    std::tm* now_tm = std::localtime(&now_time);

    // Calculate minutes since 9:30 AM
    int minutes = (now_tm->tm_hour - 9) * 60 + (now_tm->tm_min - 30);

    return minutes;
}

void RotationTradeCommand::print_summary(const RotationTradingBackend::SessionStats& stats) {
    log_system("========================================");
    log_system("Session Summary");
    log_system("========================================");

    log_system("Bars processed: " + std::to_string(stats.bars_processed));
    log_system("Signals generated: " + std::to_string(stats.signals_generated));
    log_system("Trades executed: " + std::to_string(stats.trades_executed));
    log_system("Positions opened: " + std::to_string(stats.positions_opened));
    log_system("Positions closed: " + std::to_string(stats.positions_closed));
    log_system("Rotations: " + std::to_string(stats.rotations));
    log_system("");

    log_system("Total P&L: $" + std::to_string(stats.total_pnl) +
               " (" + std::to_string(stats.total_pnl_pct * 100.0) + "%)");
    log_system("Final equity: $" + std::to_string(stats.current_equity));
    log_system("Max drawdown: " + std::to_string(stats.max_drawdown * 100.0) + "%");
    log_system("");

    log_system("Win rate: " + std::to_string(stats.win_rate * 100.0) + "%");
    log_system("Avg win: " + std::to_string(stats.avg_win_pct * 100.0) + "%");
    log_system("Avg loss: " + std::to_string(stats.avg_loss_pct * 100.0) + "%");
    log_system("Sharpe ratio: " + std::to_string(stats.sharpe_ratio));
    log_system("MRD: " + std::to_string(stats.mrd * 100.0) + "%");

    log_system("========================================");

    // Highlight MRD performance
    if (stats.mrd >= 0.005) {
        log_system("");
        log_system("🎯 TARGET ACHIEVED! MRD >= 0.5%");
    } else if (stats.mrd >= 0.0) {
        log_system("");
        log_system("✓ Positive MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    } else {
        log_system("");
        log_system("⚠️  Negative MRD: " + std::to_string(stats.mrd * 100.0) + "%");
    }
}

void RotationTradeCommand::log_system(const std::string& msg) {
    std::cout << msg << std::endl;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 14 of 19**: src/data/mock_multi_symbol_feed.cpp

**File Information**:
- **Path**: `src/data/mock_multi_symbol_feed.cpp`

- **Size**: 430 lines
- **Modified**: 2025-10-14 22:58:49

- **Type**: .cpp

```text
#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace sentio {
namespace data {

MockMultiSymbolFeed::MockMultiSymbolFeed(
    std::shared_ptr<MultiSymbolDataManager> data_manager,
    const Config& config
)
    : data_manager_(data_manager)
    , config_(config)
    , total_bars_(0) {

    utils::log_info("MockMultiSymbolFeed initialized with " +
                   std::to_string(config_.symbol_files.size()) + " symbols, " +
                   "speed=" + std::to_string(config_.replay_speed) + "x");
}

MockMultiSymbolFeed::~MockMultiSymbolFeed() {
    stop();
    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }
}

// === IBarFeed Interface Implementation ===

bool MockMultiSymbolFeed::connect() {
    if (connected_) {
        utils::log_warning("MockMultiSymbolFeed already connected");
        return true;
    }

    utils::log_info("Loading CSV data for " +
                   std::to_string(config_.symbol_files.size()) + " symbols...");
    std::cout << "[Feed] Loading CSV data for " << config_.symbol_files.size() << " symbols..." << std::endl;

    int total_loaded = 0;

    for (const auto& [symbol, filepath] : config_.symbol_files) {
        std::cout << "[Feed] Loading " << symbol << " from " << filepath << std::endl;
        int loaded = load_csv(symbol, filepath);
        if (loaded == 0) {
            utils::log_error("Failed to load data for " + symbol + " from " + filepath);
            std::cout << "[Feed] ❌ Failed to load " << symbol << " (0 bars loaded)" << std::endl;
            if (error_callback_) {
                error_callback_("Failed to load " + symbol + ": " + filepath);
            }
            return false;
        }

        total_loaded += loaded;
        utils::log_info("  " + symbol + ": " + std::to_string(loaded) + " bars");
        std::cout << "[Feed] ✓ Loaded " << symbol << ": " << loaded << " bars" << std::endl;
    }

    total_bars_ = total_loaded;
    connected_ = true;

    utils::log_info("Total bars loaded: " + std::to_string(total_loaded));

    if (connection_callback_) {
        connection_callback_(true);
    }

    return true;
}

void MockMultiSymbolFeed::disconnect() {
    if (!connected_) {
        return;
    }

    stop();

    symbol_data_.clear();
    bars_replayed_ = 0;
    total_bars_ = 0;
    connected_ = false;

    utils::log_info("MockMultiSymbolFeed disconnected");

    if (connection_callback_) {
        connection_callback_(false);
    }
}

bool MockMultiSymbolFeed::is_connected() const {
    return connected_;
}

bool MockMultiSymbolFeed::start() {
    if (!connected_) {
        utils::log_error("Cannot start - not connected. Call connect() first.");
        return false;
    }

    if (active_) {
        utils::log_warning("MockMultiSymbolFeed already active");
        return true;
    }

    if (symbol_data_.empty()) {
        utils::log_error("No data loaded - call connect() first");
        return false;
    }

    utils::log_info("Starting replay (" +
                   std::to_string(config_.replay_speed) + "x speed)...");

    bars_replayed_ = 0;
    should_stop_ = false;
    active_ = true;

    // Start replay thread
    replay_thread_ = std::thread(&MockMultiSymbolFeed::replay_loop, this);

    return true;
}

void MockMultiSymbolFeed::stop() {
    if (!active_) {
        return;
    }

    utils::log_info("Stopping replay...");
    should_stop_ = true;
    active_ = false;

    if (replay_thread_.joinable()) {
        replay_thread_.join();
    }

    utils::log_info("Replay stopped: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::is_active() const {
    return active_;
}

std::string MockMultiSymbolFeed::get_type() const {
    return "MockMultiSymbolFeed";
}

std::vector<std::string> MockMultiSymbolFeed::get_symbols() const {
    std::vector<std::string> symbols;
    for (const auto& [symbol, _] : config_.symbol_files) {
        symbols.push_back(symbol);
    }
    return symbols;
}

void MockMultiSymbolFeed::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void MockMultiSymbolFeed::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void MockMultiSymbolFeed::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = callback;
}

IBarFeed::FeedStats MockMultiSymbolFeed::get_stats() const {
    FeedStats stats;
    stats.total_bars_received = bars_replayed_.load();
    stats.errors = errors_.load();
    stats.reconnects = 0;  // Mock doesn't reconnect
    stats.avg_latency_ms = 0.0;  // Mock has no latency

    // Per-symbol counts
    int i = 0;
    for (const auto& [symbol, data] : symbol_data_) {
        if (i < 10) {
            stats.bars_per_symbol[i] = static_cast<int>(data.current_index);
            i++;
        }
    }

    return stats;
}

// === Additional Mock-Specific Methods ===

void MockMultiSymbolFeed::replay_loop() {
    utils::log_info("Replay loop started");

    // Reset current_index for all symbols
    for (auto& [symbol, data] : symbol_data_) {
        data.current_index = 0;
    }

    // Replay until all symbols exhausted or stop requested
    while (!should_stop_ && replay_next_bar()) {
        // Sleep for replay speed
        if (config_.replay_speed > 0) {
            sleep_for_replay();
        }
    }

    active_ = false;
    utils::log_info("Replay loop complete: " + std::to_string(bars_replayed_.load()) + " bars");
}

bool MockMultiSymbolFeed::replay_next_bar() {
    // Check if any symbol has bars remaining
    bool has_data = false;
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        return false;  // All symbols exhausted
    }

    // If syncing timestamps, find common timestamp
    uint64_t target_timestamp = 0;

    if (config_.sync_timestamps) {
        // Find minimum timestamp across all symbols
        target_timestamp = UINT64_MAX;

        for (const auto& [symbol, data] : symbol_data_) {
            if (data.current_index < data.bars.size()) {
                uint64_t ts = data.bars[data.current_index].timestamp_ms;
                target_timestamp = std::min(target_timestamp, ts);
            }
        }
    }

    // Update all symbols at this timestamp
    std::string last_symbol;
    Bar last_bar;
    bool any_updated = false;

    for (auto& [symbol, data] : symbol_data_) {
        if (data.current_index >= data.bars.size()) {
            continue;  // Symbol exhausted
        }

        const auto& bar = data.bars[data.current_index];

        // If syncing, only update if timestamp matches
        if (config_.sync_timestamps && bar.timestamp_ms != target_timestamp) {
            continue;
        }

        // Feed bar to data manager (direct update)
        if (data_manager_) {
            data_manager_->update_symbol(symbol, bar);
        }

        data.current_index++;
        bars_replayed_++;

        last_symbol = symbol;
        last_bar = bar;
        any_updated = true;
    }

    // Call callback ONCE after all symbols updated (not for each symbol!)
    if (bar_callback_ && any_updated) {
        bar_callback_(last_symbol, last_bar);
    }

    return true;
}

std::map<std::string, int> MockMultiSymbolFeed::get_bar_counts() const {
    std::map<std::string, int> counts;
    for (const auto& [symbol, data] : symbol_data_) {
        counts[symbol] = static_cast<int>(data.bars.size());
    }
    return counts;
}

MockMultiSymbolFeed::Progress MockMultiSymbolFeed::get_progress() const {
    Progress prog;
    prog.bars_replayed = bars_replayed_;
    prog.total_bars = total_bars_;
    prog.progress_pct = (total_bars_ > 0) ?
        (static_cast<double>(bars_replayed_) / total_bars_ * 100.0) : 0.0;

    // Find current symbol/timestamp
    for (const auto& [symbol, data] : symbol_data_) {
        if (data.current_index < data.bars.size()) {
            prog.current_symbol = symbol;
            prog.current_timestamp = data.bars[data.current_index].timestamp_ms;
            break;
        }
    }

    return prog;
}

// === Private methods ===

int MockMultiSymbolFeed::load_csv(const std::string& symbol, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        utils::log_error("Cannot open file: " + filepath);
        return 0;
    }

    SymbolData data;
    std::string line;
    int line_num = 0;
    int loaded = 0;
    int parse_failures = 0;

    // Skip header
    if (std::getline(file, line)) {
        line_num++;
        utils::log_info("  Header: " + line.substr(0, std::min(100, (int)line.size())));
    }

    // Read data
    while (std::getline(file, line)) {
        line_num++;

        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty/comment lines
        }

        Bar bar;
        if (parse_csv_line(line, bar)) {
            data.bars.push_back(bar);
            loaded++;

            // Log first bar as sample
            if (loaded == 1) {
                utils::log_info("  First bar: timestamp=" + std::to_string(bar.timestamp_ms) +
                              " close=" + std::to_string(bar.close));
            }
        } else {
            parse_failures++;
            if (parse_failures <= 3) {
                utils::log_warning("Failed to parse line " + std::to_string(line_num) +
                                  " in " + filepath + ": " + line.substr(0, 80));
            }
        }
    }

    file.close();

    if (parse_failures > 3) {
        utils::log_warning("  Total parse failures: " + std::to_string(parse_failures));
    }

    if (loaded > 0) {
        symbol_data_[symbol] = std::move(data);
        utils::log_info("  Successfully loaded " + std::to_string(loaded) + " bars for " + symbol);
    } else {
        utils::log_error("  No bars loaded for " + symbol);
    }

    return loaded;
}

bool MockMultiSymbolFeed::parse_csv_line(const std::string& line, Bar& bar) {
    std::istringstream ss(line);
    std::string token;

    try {
        // Format: timestamp,open,high,low,close,volume
        // OR: ts_utc,ts_nyt_epoch,open,high,low,close,volume (7 columns)
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        // Support both 6-column and 7-column formats
        if (tokens.size() == 7) {
            // Format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[1]) * 1000;  // Convert seconds to ms
            bar.open = std::stod(tokens[2]);
            bar.high = std::stod(tokens[3]);
            bar.low = std::stod(tokens[4]);
            bar.close = std::stod(tokens[5]);
            bar.volume = std::stoll(tokens[6]);
        } else if (tokens.size() >= 6) {
            // Format: timestamp,open,high,low,close,volume
            bar.timestamp_ms = std::stoull(tokens[0]);
            bar.open = std::stod(tokens[1]);
            bar.high = std::stod(tokens[2]);
            bar.low = std::stod(tokens[3]);
            bar.close = std::stod(tokens[4]);
            bar.volume = std::stoll(tokens[5]);
        } else {
            return false;
        }

        // Set bar_id (not in CSV, use timestamp)
        bar.bar_id = bar.timestamp_ms / 60000;  // Minutes since epoch

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

void MockMultiSymbolFeed::sleep_for_replay(int bars) {
    if (config_.replay_speed <= 0.0) {
        return;  // Instant replay
    }

    // 1 minute real-time = 60000 ms
    // At 39x speed: 60000 / 39 = 1538 ms per bar
    double ms_per_bar = 60000.0 / config_.replay_speed;
    int sleep_ms = static_cast<int>(ms_per_bar * bars);

    if (sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 15 of 19**: src/data/multi_symbol_data_manager.cpp

**File Information**:
- **Path**: `src/data/multi_symbol_data_manager.cpp`

- **Size**: 375 lines
- **Modified**: 2025-10-14 23:00:21

- **Type**: .cpp

```text
#include "data/multi_symbol_data_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iostream>

namespace sentio {
namespace data {

MultiSymbolDataManager::MultiSymbolDataManager(const Config& config)
    : config_(config)
    , time_provider_(nullptr) {

    // Initialize state for each symbol
    for (const auto& symbol : config_.symbols) {
        symbol_states_[symbol] = SymbolState();
    }

    utils::log_info("MultiSymbolDataManager initialized with " +
                   std::to_string(config_.symbols.size()) + " symbols: " +
                   join_symbols());
}

MultiSymbolSnapshot MultiSymbolDataManager::get_latest_snapshot() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    MultiSymbolSnapshot snapshot;

    // In backtest mode, use the latest bar timestamp instead of wall-clock time
    // This prevents historical data from being marked as "stale"
    if (config_.backtest_mode) {
        // Find the latest timestamp across all symbols
        uint64_t max_ts = 0;
        for (const auto& [symbol, state] : symbol_states_) {
            if (state.update_count > 0 && state.last_update_ms > max_ts) {
                max_ts = state.last_update_ms;
            }
        }
        snapshot.logical_timestamp_ms = (max_ts > 0) ? max_ts : get_current_time_ms();
    } else {
        snapshot.logical_timestamp_ms = get_current_time_ms();
    }

    double total_staleness = 0.0;
    int stale_count = 0;

    // Build snapshot for each symbol
    for (const auto& symbol : config_.symbols) {
        auto it = symbol_states_.find(symbol);
        if (it == symbol_states_.end()) {
            continue;  // Symbol not tracked
        }

        const auto& state = it->second;

        if (state.update_count == 0) {
            // Never received data - skip this symbol
            snapshot.missing_symbols.push_back(symbol);
            continue;
        }

        SymbolSnapshot sym_snap;
        sym_snap.latest_bar = state.latest_bar;
        sym_snap.last_update_ms = state.last_update_ms;
        sym_snap.forward_fill_count = state.forward_fill_count;

        // Calculate staleness
        sym_snap.update_staleness(snapshot.logical_timestamp_ms);

        // Check if we need to forward-fill
        if (sym_snap.staleness_seconds > 60.0 &&
            state.forward_fill_count < config_.max_forward_fills) {

            // Forward-fill (use last known bar, update timestamp)
            sym_snap = forward_fill_symbol(symbol, snapshot.logical_timestamp_ms);
            snapshot.total_forward_fills++;
            total_forward_fills_++;

            if (config_.log_data_quality) {
                utils::log_warning("Forward-filling " + symbol +
                                 " (stale: " + std::to_string(sym_snap.staleness_seconds) +
                                 "s, fill #" + std::to_string(sym_snap.forward_fill_count) + ")");
            }
        }

        snapshot.snapshots[symbol] = sym_snap;

        total_staleness += sym_snap.staleness_seconds;
        stale_count++;

        // Track missing if too stale
        if (!sym_snap.is_valid) {
            snapshot.missing_symbols.push_back(symbol);
        }
    }

    // Calculate aggregate stats
    snapshot.avg_staleness_seconds = (stale_count > 0) ?
        (total_staleness / stale_count) : 0.0;

    snapshot.is_complete = snapshot.missing_symbols.empty();

    // Log quality issues
    if (config_.log_data_quality && !snapshot.is_complete) {
        utils::log_warning("Snapshot incomplete: " +
                          std::to_string(snapshot.missing_symbols.size()) +
                          "/" + std::to_string(config_.symbols.size()) +
                          " missing: " + join_vector(snapshot.missing_symbols));
    }

    return snapshot;
}

bool MultiSymbolDataManager::update_symbol(const std::string& symbol, const Bar& bar) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    static int update_count = 0;
    if (update_count < 3) {
        std::cout << "[DataMgr] update_symbol called for " << symbol << " (bar timestamp: " << bar.timestamp_ms << ")" << std::endl;
        update_count++;
    }

    // Check if symbol is tracked
    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        utils::log_warning("Ignoring update for untracked symbol: " + symbol);
        std::cout << "[DataMgr] ❌ Ignoring update for untracked symbol: " << symbol << std::endl;
        return false;
    }

    // Validate bar
    if (!validate_bar(symbol, bar)) {
        it->second.rejection_count++;
        total_rejections_++;
        return false;
    }

    auto& state = it->second;

    // Add to history
    state.history.push_back(bar);
    if (state.history.size() > static_cast<size_t>(config_.history_size)) {
        state.history.pop_front();
    }

    // Update latest
    state.latest_bar = bar;
    state.last_update_ms = bar.timestamp_ms;
    state.update_count++;
    state.forward_fill_count = 0;  // Reset forward fill counter

    total_updates_++;

    return true;
}

int MultiSymbolDataManager::update_all(const std::map<std::string, Bar>& bars) {
    int success_count = 0;
    for (const auto& [symbol, bar] : bars) {
        if (update_symbol(symbol, bar)) {
            success_count++;
        }
    }
    return success_count;
}

std::vector<Bar> MultiSymbolDataManager::get_recent_bars(
    const std::string& symbol,
    int count
) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.history.empty()) {
        return {};
    }

    const auto& history = it->second.history;
    int available = static_cast<int>(history.size());
    int to_return = std::min(count, available);

    std::vector<Bar> result;
    result.reserve(to_return);

    // Return newest bars first
    auto start_it = history.end() - to_return;
    for (auto it = start_it; it != history.end(); ++it) {
        result.push_back(*it);
    }

    std::reverse(result.begin(), result.end());  // Newest first

    return result;
}

std::deque<Bar> MultiSymbolDataManager::get_all_bars(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end()) {
        return {};
    }

    return it->second.history;
}

MultiSymbolDataManager::DataQualityStats
MultiSymbolDataManager::get_quality_stats() const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    DataQualityStats stats;
    stats.total_updates = total_updates_.load();
    stats.total_forward_fills = total_forward_fills_.load();
    stats.total_rejections = total_rejections_.load();

    double total_avg_staleness = 0.0;
    int count = 0;

    for (const auto& [symbol, state] : symbol_states_) {
        stats.update_counts[symbol] = state.update_count;
        stats.forward_fill_counts[symbol] = state.forward_fill_count;

        if (state.update_count > 0) {
            double avg = state.cumulative_staleness / state.update_count;
            stats.avg_staleness[symbol] = avg;
            total_avg_staleness += avg;
            count++;
        }
    }

    stats.overall_avg_staleness = (count > 0) ?
        (total_avg_staleness / count) : 0.0;

    return stats;
}

void MultiSymbolDataManager::reset_stats() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    total_updates_ = 0;
    total_forward_fills_ = 0;
    total_rejections_ = 0;

    for (auto& [symbol, state] : symbol_states_) {
        state.update_count = 0;
        state.forward_fill_count = 0;
        state.rejection_count = 0;
        state.cumulative_staleness = 0.0;
    }
}

bool MultiSymbolDataManager::validate_bar(const std::string& symbol, const Bar& bar) {
    // Check 1: Timestamp is reasonable (not in future, not too old)
    // SKIP timestamp validation in backtest mode (historical data is expected to be old)
    if (!config_.backtest_mode) {
        uint64_t now = get_current_time_ms();
        if (bar.timestamp_ms > now + 60000) {  // Future by > 1 minute
            utils::log_error("Rejected " + symbol + " bar: timestamp in future (" +
                            std::to_string(bar.timestamp_ms) + " vs " +
                            std::to_string(now) + ")");
            return false;
        }

        if (bar.timestamp_ms < now - 86400000) {  // Older than 24 hours
            utils::log_warning("Rejected " + symbol + " bar: timestamp too old (" +
                              std::to_string((now - bar.timestamp_ms) / 1000) + "s)");
            return false;
        }
    }

    // Check 2: Price sanity (0.01 < price < 10000)
    if (bar.close <= 0.01 || bar.close > 10000.0) {
        utils::log_error("Rejected " + symbol + " bar: invalid price (" +
                        std::to_string(bar.close) + ")");
        return false;
    }

    // Check 3: OHLC consistency
    if (bar.low > bar.close || bar.high < bar.close ||
        bar.low > bar.open || bar.high < bar.open) {
        utils::log_warning("Rejected " + symbol + " bar: OHLC inconsistent (O=" +
                          std::to_string(bar.open) + " H=" +
                          std::to_string(bar.high) + " L=" +
                          std::to_string(bar.low) + " C=" +
                          std::to_string(bar.close) + ")");
        return false;
    }

    // Check 4: Volume non-negative
    if (bar.volume < 0) {
        utils::log_warning("Rejected " + symbol + " bar: negative volume (" +
                          std::to_string(bar.volume) + ")");
        return false;
    }

    // Check 5: Duplicate detection (same timestamp as last bar)
    auto it = symbol_states_.find(symbol);
    if (it != symbol_states_.end() && it->second.update_count > 0) {
        if (bar.timestamp_ms == it->second.last_update_ms) {
            // Duplicate - not necessarily an error, just skip
            return false;
        }

        // Check timestamp ordering (must be after last update)
        if (bar.timestamp_ms < it->second.last_update_ms) {
            utils::log_warning("Rejected " + symbol + " bar: out-of-order timestamp (" +
                              std::to_string(bar.timestamp_ms) + " < " +
                              std::to_string(it->second.last_update_ms) + ")");
            return false;
        }
    }

    return true;
}

SymbolSnapshot MultiSymbolDataManager::forward_fill_symbol(
    const std::string& symbol,
    uint64_t logical_time
) {
    SymbolSnapshot snap;

    auto it = symbol_states_.find(symbol);
    if (it == symbol_states_.end() || it->second.update_count == 0) {
        snap.is_valid = false;
        return snap;
    }

    auto& state = it->second;

    // Use last known bar, update timestamp
    snap.latest_bar = state.latest_bar;
    snap.latest_bar.timestamp_ms = logical_time;  // Forward-filled timestamp

    snap.last_update_ms = state.last_update_ms;  // Original update time
    snap.forward_fill_count = state.forward_fill_count + 1;

    // Update state forward fill counter
    state.forward_fill_count++;

    // Calculate staleness based on original update time
    snap.update_staleness(logical_time);

    // Mark invalid if too many forward fills
    if (snap.forward_fill_count >= config_.max_forward_fills) {
        snap.is_valid = false;
        utils::log_error("Symbol " + symbol + " exceeded max forward fills (" +
                        std::to_string(config_.max_forward_fills) + ")");
    }

    return snap;
}

// === Helper functions ===

std::string MultiSymbolDataManager::join_symbols() const {
    std::ostringstream oss;
    for (size_t i = 0; i < config_.symbols.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << config_.symbols[i];
    }
    return oss.str();
}

std::string MultiSymbolDataManager::join_vector(const std::vector<std::string>& vec) const {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << vec[i];
    }
    return oss.str();
}

} // namespace data
} // namespace sentio

```

## 📄 **FILE 16 of 19**: src/features/unified_feature_engine.cpp

**File Information**:
- **Path**: `src/features/unified_feature_engine.cpp`

- **Size**: 503 lines
- **Modified**: 2025-10-15 03:28:55

- **Type**: .cpp

```text
#include "features/unified_feature_engine.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

// OpenSSL for SHA1 hashing (already in dependencies)
#include <openssl/sha.h>

namespace sentio {
namespace features {

// =============================================================================
// SHA1 Hash Utility
// =============================================================================

std::string sha1_hex(const std::string& s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);

    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        os << std::setw(2) << static_cast<int>(c);
    }
    return os.str();
}

// =============================================================================
// UnifiedFeatureEngineV2 Implementation
// =============================================================================

UnifiedFeatureEngine::UnifiedFeatureEngine(EngineConfig cfg)
    : cfg_(cfg),
      rsi14_(cfg.rsi14),
      rsi21_(cfg.rsi21),
      atr14_(cfg.atr14),
      bb20_(cfg.bb20, cfg.bb_k),
      stoch14_(cfg.stoch14),
      will14_(cfg.will14),
      macd_(),  // Uses default periods 12/26/9
      roc5_(cfg.roc5),
      roc10_(cfg.roc10),
      roc20_(cfg.roc20),
      cci20_(cfg.cci20),
      don20_(cfg.don20),
      keltner_(cfg.keltner_ema, cfg.keltner_atr, cfg.keltner_mult),
      obv_(),
      vwap_(),
      ema10_(cfg.ema10),
      ema20_(cfg.ema20),
      ema50_(cfg.ema50),
      sma10_ring_(cfg.sma10),
      sma20_ring_(cfg.sma20),
      sma50_ring_(cfg.sma50),
      scaler_(cfg.robust ? Scaler::Type::ROBUST : Scaler::Type::STANDARD)
{
    build_schema_();
    feats_.assign(schema_.names.size(), std::numeric_limits<double>::quiet_NaN());
}

void UnifiedFeatureEngine::build_schema_() {
    std::vector<std::string> n;

    // ==========================================================================
    // Time features (cyclical encoding for intraday patterns)
    // ==========================================================================
    if (cfg_.time) {
        n.push_back("time.hour_sin");
        n.push_back("time.hour_cos");
        n.push_back("time.minute_sin");
        n.push_back("time.minute_cos");
        n.push_back("time.dow_sin");
        n.push_back("time.dow_cos");
        n.push_back("time.dom_sin");
        n.push_back("time.dom_cos");
    }

    // ==========================================================================
    // Core price/volume features (always included)
    // ==========================================================================
    n.push_back("price.close");
    n.push_back("price.open");
    n.push_back("price.high");
    n.push_back("price.low");
    n.push_back("price.return_1");
    n.push_back("volume.raw");

    // ==========================================================================
    // Moving Averages (always included for baseline)
    // ==========================================================================
    n.push_back("sma10");
    n.push_back("sma20");
    n.push_back("sma50");
    n.push_back("ema10");
    n.push_back("ema20");
    n.push_back("ema50");
    n.push_back("price_vs_sma20");  // (close - sma20) / sma20
    n.push_back("price_vs_ema20");  // (close - ema20) / ema20

    // ==========================================================================
    // Volatility Features
    // ==========================================================================
    if (cfg_.volatility) {
        n.push_back("atr14");
        n.push_back("atr14_pct");  // ATR / close
        n.push_back("bb20.mean");
        n.push_back("bb20.sd");
        n.push_back("bb20.upper");
        n.push_back("bb20.lower");
        n.push_back("bb20.percent_b");
        n.push_back("bb20.bandwidth");
        n.push_back("keltner.middle");
        n.push_back("keltner.upper");
        n.push_back("keltner.lower");
    }

    // ==========================================================================
    // Momentum Features
    // ==========================================================================
    if (cfg_.momentum) {
        n.push_back("rsi14");
        n.push_back("rsi21");
        n.push_back("stoch14.k");
        n.push_back("stoch14.d");
        n.push_back("stoch14.slow");
        n.push_back("will14");
        n.push_back("macd.line");
        n.push_back("macd.signal");
        n.push_back("macd.hist");
        n.push_back("roc5");
        n.push_back("roc10");
        n.push_back("roc20");
        n.push_back("cci20");
    }

    // ==========================================================================
    // Volume Features
    // ==========================================================================
    if (cfg_.volume) {
        n.push_back("obv");
        n.push_back("vwap");
        n.push_back("vwap_dist");  // (close - vwap) / vwap
    }

    // ==========================================================================
    // Donchian Channels (pattern/breakout detection)
    // ==========================================================================
    n.push_back("don20.up");
    n.push_back("don20.mid");
    n.push_back("don20.dn");
    n.push_back("don20.position");  // (close - dn) / (up - dn)

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        n.push_back("pattern.doji");           // Body < 10% of range
        n.push_back("pattern.hammer");         // Lower shadow > 2x body
        n.push_back("pattern.shooting_star");  // Upper shadow > 2x body
        n.push_back("pattern.engulfing_bull"); // Bullish engulfing
        n.push_back("pattern.engulfing_bear"); // Bearish engulfing
    }

    // ==========================================================================
    // Finalize schema and compute hash
    // ==========================================================================
    schema_.names = std::move(n);

    // Concatenate names and critical config for hash
    std::ostringstream cat;
    for (const auto& name : schema_.names) {
        cat << name << "\n";
    }
    cat << "cfg:"
        << cfg_.rsi14 << ","
        << cfg_.bb20 << ","
        << cfg_.bb_k << ","
        << cfg_.macd_fast << ","
        << cfg_.macd_slow << ","
        << cfg_.macd_sig;

    schema_.sha1_hash = sha1_hex(cat.str());
}

bool UnifiedFeatureEngine::update(const Bar& b) {
    // ==========================================================================
    // Update all indicators (O(1) incremental)
    // ==========================================================================

    // Volatility
    atr14_.update(b.high, b.low, b.close);
    bb20_.update(b.close);
    keltner_.update(b.high, b.low, b.close);

    // Momentum
    rsi14_.update(b.close);
    rsi21_.update(b.close);
    stoch14_.update(b.high, b.low, b.close);
    will14_.update(b.high, b.low, b.close);
    macd_.update(b.close);
    roc5_.update(b.close);
    roc10_.update(b.close);
    roc20_.update(b.close);
    cci20_.update(b.high, b.low, b.close);

    // Channels
    don20_.update(b.high, b.low);

    // Volume
    obv_.update(b.close, b.volume);
    vwap_.update(b.close, b.volume);

    // Moving averages
    ema10_.update(b.close);
    ema20_.update(b.close);
    ema50_.update(b.close);
    sma10_ring_.push(b.close);
    sma20_ring_.push(b.close);
    sma50_ring_.push(b.close);

    // Store previous close BEFORE updating (for 1-bar return calculation)
    prevPrevClose_ = prevClose_;

    // Store current bar values for derived features
    prevTimestamp_ = b.timestamp_ms;
    prevClose_ = b.close;
    prevOpen_ = b.open;
    prevHigh_ = b.high;
    prevLow_ = b.low;
    prevVolume_ = b.volume;

    // Recompute feature vector
    recompute_vector_();

    seeded_ = true;
    ++bar_count_;
    return true;
}

void UnifiedFeatureEngine::recompute_vector_() {
    size_t k = 0;

    // ==========================================================================
    // Time features (cyclical encoding from v1.0)
    // ==========================================================================
    if (cfg_.time && prevTimestamp_ > 0) {
        time_t timestamp = prevTimestamp_ / 1000;
        struct tm* time_info = gmtime(&timestamp);

        if (time_info) {
            double hour = time_info->tm_hour;
            double minute = time_info->tm_min;
            double day_of_week = time_info->tm_wday;     // 0-6 (Sunday=0)
            double day_of_month = time_info->tm_mday;    // 1-31

            // Cyclical encoding (sine/cosine to preserve continuity)
            feats_[k++] = std::sin(2.0 * M_PI * hour / 24.0);           // hour_sin
            feats_[k++] = std::cos(2.0 * M_PI * hour / 24.0);           // hour_cos
            feats_[k++] = std::sin(2.0 * M_PI * minute / 60.0);         // minute_sin
            feats_[k++] = std::cos(2.0 * M_PI * minute / 60.0);         // minute_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_week / 7.0);     // dow_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_week / 7.0);     // dow_cos
            feats_[k++] = std::sin(2.0 * M_PI * day_of_month / 31.0);   // dom_sin
            feats_[k++] = std::cos(2.0 * M_PI * day_of_month / 31.0);   // dom_cos
        } else {
            // If time parsing fails, fill with NaN
            for (int i = 0; i < 8; ++i) {
                feats_[k++] = std::numeric_limits<double>::quiet_NaN();
            }
        }
    }

    // ==========================================================================
    // Core price/volume
    // ==========================================================================
    feats_[k++] = prevClose_;
    feats_[k++] = prevOpen_;
    feats_[k++] = prevHigh_;
    feats_[k++] = prevLow_;
    feats_[k++] = safe_return(prevClose_, prevPrevClose_);  // 1-bar return
    feats_[k++] = prevVolume_;

    // ==========================================================================
    // Moving Averages
    // ==========================================================================
    double sma10 = sma10_ring_.full() ? sma10_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma20 = sma20_ring_.full() ? sma20_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double sma50 = sma50_ring_.full() ? sma50_ring_.mean() : std::numeric_limits<double>::quiet_NaN();
    double ema10 = ema10_.get_value();
    double ema20 = ema20_.get_value();
    double ema50 = ema50_.get_value();

    feats_[k++] = sma10;
    feats_[k++] = sma20;
    feats_[k++] = sma50;
    feats_[k++] = ema10;
    feats_[k++] = ema20;
    feats_[k++] = ema50;

    // Price vs MA ratios
    feats_[k++] = (!std::isnan(sma20) && sma20 != 0) ? (prevClose_ - sma20) / sma20 : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = (!std::isnan(ema20) && ema20 != 0) ? (prevClose_ - ema20) / ema20 : std::numeric_limits<double>::quiet_NaN();

    // ==========================================================================
    // Volatility
    // ==========================================================================
    if (cfg_.volatility) {
        feats_[k++] = atr14_.value;
        feats_[k++] = (prevClose_ != 0 && !std::isnan(atr14_.value)) ? atr14_.value / prevClose_ : std::numeric_limits<double>::quiet_NaN();

        // Debug BB NaN issue (log first 5 occurrences)
        static int bb_nan_count = 0;
        if (bb_nan_count < 5 && std::isnan(bb20_.mean)) {
            std::cerr << "[FeatureEngine #" << bb_nan_count
                      << "] BB NaN detected! bar_count=" << bar_count_
                      << ", bb20_.win.size=" << bb20_.win.size()
                      << ", bb20_.win.full=" << bb20_.win.full()
                      << ", prevClose=" << prevClose_ << std::endl;
            bb_nan_count++;
        }

        feats_[k++] = bb20_.mean;
        feats_[k++] = bb20_.sd;
        feats_[k++] = bb20_.upper;
        feats_[k++] = bb20_.lower;
        feats_[k++] = bb20_.percent_b;
        feats_[k++] = bb20_.bandwidth;
        feats_[k++] = keltner_.middle;
        feats_[k++] = keltner_.upper;
        feats_[k++] = keltner_.lower;
    }

    // ==========================================================================
    // Momentum
    // ==========================================================================
    if (cfg_.momentum) {
        feats_[k++] = rsi14_.value;
        feats_[k++] = rsi21_.value;
        feats_[k++] = stoch14_.k;
        feats_[k++] = stoch14_.d;
        feats_[k++] = stoch14_.slow;
        feats_[k++] = will14_.r;
        feats_[k++] = macd_.macd;
        feats_[k++] = macd_.signal;
        feats_[k++] = macd_.hist;
        feats_[k++] = roc5_.value;
        feats_[k++] = roc10_.value;
        feats_[k++] = roc20_.value;
        feats_[k++] = cci20_.value;
    }

    // ==========================================================================
    // Volume
    // ==========================================================================
    if (cfg_.volume) {
        feats_[k++] = obv_.value;
        feats_[k++] = vwap_.value;
        double vwap_dist = (!std::isnan(vwap_.value) && vwap_.value != 0)
                           ? (prevClose_ - vwap_.value) / vwap_.value
                           : std::numeric_limits<double>::quiet_NaN();
        feats_[k++] = vwap_dist;
    }

    // ==========================================================================
    // Donchian
    // ==========================================================================
    feats_[k++] = don20_.up;
    feats_[k++] = don20_.mid;
    feats_[k++] = don20_.dn;

    // Donchian position: (close - dn) / (up - dn)
    double don_range = don20_.up - don20_.dn;
    double don_pos = (don_range != 0 && !std::isnan(don20_.up) && !std::isnan(don20_.dn))
                     ? (prevClose_ - don20_.dn) / don_range
                     : std::numeric_limits<double>::quiet_NaN();
    feats_[k++] = don_pos;

    // ==========================================================================
    // Candlestick Pattern Features (from v1.0)
    // ==========================================================================
    if (cfg_.patterns) {
        double range = prevHigh_ - prevLow_;
        double body = std::abs(prevClose_ - prevOpen_);
        double upper_shadow = prevHigh_ - std::max(prevOpen_, prevClose_);
        double lower_shadow = std::min(prevOpen_, prevClose_) - prevLow_;

        // Doji: body < 10% of range
        bool is_doji = (range > 0) && (body / range < 0.1);
        feats_[k++] = is_doji ? 1.0 : 0.0;

        // Hammer: lower shadow > 2x body, upper shadow < body
        bool is_hammer = (lower_shadow > 2.0 * body) && (upper_shadow < body);
        feats_[k++] = is_hammer ? 1.0 : 0.0;

        // Shooting star: upper shadow > 2x body, lower shadow < body
        bool is_shooting_star = (upper_shadow > 2.0 * body) && (lower_shadow < body);
        feats_[k++] = is_shooting_star ? 1.0 : 0.0;

        // Engulfing patterns require previous bar - use prevPrevClose_
        bool engulfing_bull = false;
        bool engulfing_bear = false;
        if (!std::isnan(prevPrevClose_)) {
            bool prev_bearish = prevPrevClose_ < prevOpen_;  // Prev bar was bearish
            bool curr_bullish = prevClose_ > prevOpen_;       // Current bar is bullish
            bool engulfs = (prevOpen_ < prevPrevClose_) && (prevClose_ > prevOpen_);
            engulfing_bull = prev_bearish && curr_bullish && engulfs;

            bool prev_bullish = prevPrevClose_ > prevOpen_;
            bool curr_bearish = prevClose_ < prevOpen_;
            engulfs = (prevOpen_ > prevPrevClose_) && (prevClose_ < prevOpen_);
            engulfing_bear = prev_bullish && curr_bearish && engulfs;
        }
        feats_[k++] = engulfing_bull ? 1.0 : 0.0;
        feats_[k++] = engulfing_bear ? 1.0 : 0.0;
    }
}

int UnifiedFeatureEngine::warmup_remaining() const {
    // Conservative: max lookback across all indicators
    int max_period = std::max({
        cfg_.rsi14, cfg_.rsi21, cfg_.atr14, cfg_.bb20,
        cfg_.stoch14, cfg_.will14, cfg_.macd_slow, cfg_.don20,
        cfg_.sma50, cfg_.ema50
    });

    // Need at least max_period + 1 bars for all indicators to be valid
    int required_bars = max_period + 1;
    return std::max(0, required_bars - static_cast<int>(bar_count_));
}

std::vector<std::string> UnifiedFeatureEngine::get_unready_indicators() const {
    std::vector<std::string> unready;

    // Check each indicator's readiness
    if (!bb20_.is_ready()) unready.push_back("BB20");
    if (!rsi14_.is_ready()) unready.push_back("RSI14");
    if (!rsi21_.is_ready()) unready.push_back("RSI21");
    if (!atr14_.is_ready()) unready.push_back("ATR14");
    if (!stoch14_.is_ready()) unready.push_back("Stoch14");
    if (!will14_.is_ready()) unready.push_back("Will14");
    if (!don20_.is_ready()) unready.push_back("Don20");

    // Check moving averages
    if (bar_count_ < static_cast<size_t>(cfg_.sma10)) unready.push_back("SMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.sma20)) unready.push_back("SMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.sma50)) unready.push_back("SMA50");
    if (bar_count_ < static_cast<size_t>(cfg_.ema10)) unready.push_back("EMA10");
    if (bar_count_ < static_cast<size_t>(cfg_.ema20)) unready.push_back("EMA20");
    if (bar_count_ < static_cast<size_t>(cfg_.ema50)) unready.push_back("EMA50");

    return unready;
}

void UnifiedFeatureEngine::reset() {
    *this = UnifiedFeatureEngine(cfg_);
}

std::string UnifiedFeatureEngine::serialize() const {
    std::ostringstream os;
    os << std::setprecision(17);

    os << "prevTimestamp " << prevTimestamp_ << "\n";
    os << "prevClose " << prevClose_ << "\n";
    os << "prevPrevClose " << prevPrevClose_ << "\n";
    os << "prevOpen " << prevOpen_ << "\n";
    os << "prevHigh " << prevHigh_ << "\n";
    os << "prevLow " << prevLow_ << "\n";
    os << "prevVolume " << prevVolume_ << "\n";
    os << "bar_count " << bar_count_ << "\n";
    os << "obv " << obv_.value << "\n";
    os << "vwap " << vwap_.sumPV << " " << vwap_.sumV << "\n";

    // Add EMA/indicator states if exact resume needed
    // (Omitted for brevity; can be extended)

    return os.str();
}

void UnifiedFeatureEngine::restore(const std::string& blob) {
    reset();

    std::istringstream is(blob);
    std::string key;

    while (is >> key) {
        if (key == "prevTimestamp") is >> prevTimestamp_;
        else if (key == "prevClose") is >> prevClose_;
        else if (key == "prevPrevClose") is >> prevPrevClose_;
        else if (key == "prevOpen") is >> prevOpen_;
        else if (key == "prevHigh") is >> prevHigh_;
        else if (key == "prevLow") is >> prevLow_;
        else if (key == "prevVolume") is >> prevVolume_;
        else if (key == "bar_count") is >> bar_count_;
        else if (key == "obv") is >> obv_.value;
        else if (key == "vwap") is >> vwap_.sumPV >> vwap_.sumV;
    }
}

} // namespace features
} // namespace sentio

```

## 📄 **FILE 17 of 19**: src/learning/online_predictor.cpp

**File Information**:
- **Path**: `src/learning/online_predictor.cpp`

- **Size**: 421 lines
- **Modified**: 2025-10-15 02:19:44

- **Type**: .cpp

```text
#include "learning/online_predictor.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace sentio {
namespace learning {

OnlinePredictor::OnlinePredictor(size_t num_features, const Config& config)
    : config_(config), num_features_(num_features), samples_seen_(0),
      current_lambda_(config.lambda) {
    
    // Initialize parameters to zero
    theta_ = Eigen::VectorXd::Zero(num_features);
    
    // Initialize covariance with high uncertainty
    P_ = Eigen::MatrixXd::Identity(num_features, num_features) * config.initial_variance;
    
    utils::log_info("OnlinePredictor initialized with " + std::to_string(num_features) + 
                   " features, lambda=" + std::to_string(config.lambda));
}

OnlinePredictor::PredictionResult OnlinePredictor::predict(const std::vector<double>& features) {
    PredictionResult result;
    result.is_ready = is_ready();
    
    if (!result.is_ready) {
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }
    
    // Convert to Eigen vector
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(features.data(), features.size());

    // Check for NaN in features
    if (!x.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: NaN detected in features! Returning neutral prediction." << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }

    // Check for NaN in model parameters
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: NaN detected in model parameters (theta or P)! Returning neutral prediction." << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
        result.volatility_estimate = 0.0;
        return result;
    }

    // Linear prediction
    result.predicted_return = theta_.dot(x);

    // Confidence from prediction variance
    double prediction_variance = x.transpose() * P_ * x;
    result.confidence = 1.0 / (1.0 + std::sqrt(prediction_variance));

    // Sanity check results
    if (!std::isfinite(result.predicted_return) || !std::isfinite(result.confidence)) {
        std::cerr << "[OnlinePredictor] WARNING: NaN in prediction results! pred_return="
                  << result.predicted_return << ", confidence=" << result.confidence << std::endl;
        result.predicted_return = 0.0;
        result.confidence = 0.0;
    }

    // Current volatility estimate
    result.volatility_estimate = estimate_volatility();

    return result;
}

void OnlinePredictor::update(const std::vector<double>& features, double actual_return) {
    samples_seen_++;

    // Check for NaN in input
    if (!std::isfinite(actual_return)) {
        std::cerr << "[OnlinePredictor] WARNING: actual_return is NaN/inf! Skipping update. actual_return=" << actual_return << std::endl;
        return;
    }

    // Use Eigen::Map to avoid copy (zero-copy view of std::vector)
    Eigen::Map<const Eigen::VectorXd> x(features.data(), features.size());

    // Check for NaN in features
    if (!x.allFinite()) {
        std::cerr << "[OnlinePredictor] WARNING: Features contain NaN/inf! Skipping update." << std::endl;

        // Debug: Show which features are NaN (first occurrence only)
        static bool logged_nan_indices = false;
        if (!logged_nan_indices) {
            std::cerr << "[OnlinePredictor] NaN feature indices: ";
            for (int i = 0; i < x.size(); i++) {
                if (!std::isfinite(x(i))) {
                    std::cerr << i << "(" << x(i) << ") ";
                }
            }
            std::cerr << std::endl;
            std::cerr << "[OnlinePredictor] Total features: " << x.size() << std::endl;
            logged_nan_indices = true;
        }

        return;
    }

    // Check model parameters before update
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] CRITICAL: Model parameters corrupted (theta or P has NaN)! Reinitializing..." << std::endl;
        // Reinitialize model
        theta_ = Eigen::VectorXd::Zero(num_features_);
        P_ = Eigen::MatrixXd::Identity(num_features_, num_features_) / config_.regularization;
        return;
    }

    // Store return for volatility estimation
    recent_returns_.push_back(actual_return);
    if (recent_returns_.size() > HISTORY_SIZE) {
        recent_returns_.pop_front();
    }

    // Current prediction
    double predicted = theta_.dot(x);
    double error = actual_return - predicted;
    
    // Store error for diagnostics
    recent_errors_.push_back(error);
    if (recent_errors_.size() > HISTORY_SIZE) {
        recent_errors_.pop_front();
    }
    
    // Store direction accuracy
    bool correct_direction = (predicted > 0 && actual_return > 0) || 
                           (predicted < 0 && actual_return < 0);
    recent_directions_.push_back(correct_direction);
    if (recent_directions_.size() > HISTORY_SIZE) {
        recent_directions_.pop_front();
    }
    
    // EWRLS update with regularization
    double lambda_reg = current_lambda_ + config_.regularization;
    
    // Kalman gain
    Eigen::VectorXd Px = P_ * x;
    double denominator = lambda_reg + x.dot(Px);
    
    if (std::abs(denominator) < EPSILON) {
        utils::log_warning("Near-zero denominator in EWRLS update, skipping");
        return;
    }
    
    Eigen::VectorXd k = Px / denominator;

    // Update parameters
    theta_.noalias() += k * error;

    // Update covariance (optimized: reuse Px, avoid k * x.transpose() * P_)
    // P = (P - k * x' * P) / lambda = (P - k * Px') / lambda
    P_.noalias() -= k * Px.transpose();
    P_ /= current_lambda_;

    // Ensure numerical stability
    // NOTE: This is expensive O(n³) but REQUIRED every update for prediction accuracy
    // Skipping this causes numerical instability and prediction divergence
    ensure_positive_definite();

    // Check for NaN after update
    if (!theta_.allFinite() || !P_.allFinite()) {
        std::cerr << "[OnlinePredictor] CRITICAL: NaN introduced during update! Reinitializing model." << std::endl;
        std::cerr << "  error=" << error << ", denominator=" << denominator << ", lambda=" << current_lambda_ << std::endl;
        // Reinitialize model
        theta_ = Eigen::VectorXd::Zero(num_features_);
        P_ = Eigen::MatrixXd::Identity(num_features_, num_features_) / config_.regularization;
        return;
    }

    // Adapt learning rate if enabled
    if (config_.adaptive_learning && samples_seen_ % 10 == 0) {
        adapt_learning_rate(estimate_volatility());
    }
}

OnlinePredictor::PredictionResult OnlinePredictor::predict_and_update(
    const std::vector<double>& features, double actual_return) {
    
    auto result = predict(features);
    update(features, actual_return);
    return result;
}

void OnlinePredictor::adapt_learning_rate(double market_volatility) {
    // Higher volatility -> faster adaptation (lower lambda)
    // Lower volatility -> slower adaptation (higher lambda)
    
    double baseline_vol = 0.001;  // 0.1% baseline volatility
    double vol_ratio = market_volatility / baseline_vol;
    
    // Map volatility ratio to lambda
    // High vol (ratio=2) -> lambda=0.990
    // Low vol (ratio=0.5) -> lambda=0.999
    double target_lambda = config_.lambda - 0.005 * std::log(vol_ratio);
    target_lambda = std::clamp(target_lambda, config_.min_lambda, config_.max_lambda);
    
    // Smooth transition
    current_lambda_ = 0.9 * current_lambda_ + 0.1 * target_lambda;
    
    utils::log_debug("Adapted lambda: " + std::to_string(current_lambda_) + 
                    " (volatility=" + std::to_string(market_volatility) + ")");
}

bool OnlinePredictor::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Save config
        file.write(reinterpret_cast<const char*>(&config_), sizeof(Config));
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_lambda_), sizeof(double));
        
        // Save theta
        file.write(reinterpret_cast<const char*>(theta_.data()), 
                  sizeof(double) * theta_.size());
        
        // Save P (covariance)
        file.write(reinterpret_cast<const char*>(P_.data()), 
                  sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Saved predictor state to: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlinePredictor::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Load config
        file.read(reinterpret_cast<char*>(&config_), sizeof(Config));
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_lambda_), sizeof(double));
        
        // Load theta
        theta_.resize(num_features_);
        file.read(reinterpret_cast<char*>(theta_.data()), 
                 sizeof(double) * theta_.size());
        
        // Load P
        P_.resize(num_features_, num_features_);
        file.read(reinterpret_cast<char*>(P_.data()), 
                 sizeof(double) * P_.size());
        
        file.close();
        utils::log_info("Loaded predictor state from: " + path);
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

double OnlinePredictor::get_recent_rmse() const {
    if (recent_errors_.empty()) return 0.0;
    
    double sum_sq = 0.0;
    for (double error : recent_errors_) {
        sum_sq += error * error;
    }
    return std::sqrt(sum_sq / recent_errors_.size());
}

double OnlinePredictor::get_directional_accuracy() const {
    if (recent_directions_.empty()) return 0.5;
    
    int correct = std::count(recent_directions_.begin(), recent_directions_.end(), true);
    return static_cast<double>(correct) / recent_directions_.size();
}

std::vector<double> OnlinePredictor::get_feature_importance() const {
    // Feature importance based on parameter magnitude * covariance
    std::vector<double> importance(num_features_);
    
    for (size_t i = 0; i < num_features_; ++i) {
        // Combine parameter magnitude with certainty (inverse variance)
        double param_importance = std::abs(theta_[i]);
        double certainty = 1.0 / (1.0 + std::sqrt(P_(i, i)));
        importance[i] = param_importance * certainty;
    }
    
    // Normalize
    double max_imp = *std::max_element(importance.begin(), importance.end());
    if (max_imp > 0) {
        for (double& imp : importance) {
            imp /= max_imp;
        }
    }
    
    return importance;
}

double OnlinePredictor::estimate_volatility() const {
    if (recent_returns_.size() < 20) return 0.001;  // Default 0.1%
    
    double mean = std::accumulate(recent_returns_.begin(), recent_returns_.end(), 0.0) 
                 / recent_returns_.size();
    
    double sum_sq = 0.0;
    for (double ret : recent_returns_) {
        sum_sq += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sum_sq / recent_returns_.size());
}

void OnlinePredictor::ensure_positive_definite() {
    // Eigenvalue decomposition - CANNOT be optimized without accuracy degradation
    // Attempted optimizations (all failed accuracy tests):
    // 1. Periodic updates (every N samples) - causes divergence
    // 2. Cholesky fast path - causes divergence
    // 3. Matrix symmetrization - causes long-term drift
    // Conclusion: EWRLS is highly sensitive to numerical perturbations
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(P_);
    Eigen::VectorXd eigenvalues = solver.eigenvalues();

    // Ensure all eigenvalues are positive
    bool needs_correction = false;
    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (eigenvalues[i] < EPSILON) {
            eigenvalues[i] = EPSILON;
            needs_correction = true;
        }
    }

    if (needs_correction) {
        // Reconstruct with corrected eigenvalues
        P_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        utils::log_debug("Corrected covariance matrix for positive definiteness");
    }
}

// MultiHorizonPredictor Implementation

MultiHorizonPredictor::MultiHorizonPredictor(size_t num_features) 
    : num_features_(num_features) {
}

void MultiHorizonPredictor::add_horizon(int bars, double weight) {
    HorizonConfig config;
    config.horizon_bars = bars;
    config.weight = weight;

    // Adjust learning rate based on horizon
    config.predictor_config.lambda = 0.995 + 0.001 * std::log(bars);
    config.predictor_config.lambda = std::clamp(config.predictor_config.lambda, 0.990, 0.999);

    // Reduce warmup for multi-horizon learning
    // Updates arrive delayed by horizon length, so effective warmup is longer
    config.predictor_config.warmup_samples = 20;

    predictors_.emplace_back(std::make_unique<OnlinePredictor>(num_features_, config.predictor_config));
    configs_.push_back(config);

    utils::log_info("Added predictor for " + std::to_string(bars) + "-bar horizon");
}

OnlinePredictor::PredictionResult MultiHorizonPredictor::predict(const std::vector<double>& features) {
    OnlinePredictor::PredictionResult ensemble_result;
    ensemble_result.predicted_return = 0.0;
    ensemble_result.confidence = 0.0;
    ensemble_result.volatility_estimate = 0.0;
    
    double total_weight = 0.0;
    int ready_count = 0;
    
    for (size_t i = 0; i < predictors_.size(); ++i) {
        auto result = predictors_[i]->predict(features);
        
        if (result.is_ready) {
            double weight = configs_[i].weight * result.confidence;
            ensemble_result.predicted_return += result.predicted_return * weight;
            ensemble_result.confidence += result.confidence * configs_[i].weight;
            ensemble_result.volatility_estimate += result.volatility_estimate * configs_[i].weight;
            total_weight += weight;
            ready_count++;
        }
    }
    
    if (total_weight > 0) {
        ensemble_result.predicted_return /= total_weight;
        ensemble_result.confidence /= configs_.size();
        ensemble_result.volatility_estimate /= configs_.size();
        ensemble_result.is_ready = true;
    }
    
    return ensemble_result;
}

void MultiHorizonPredictor::update(int bars_ago, const std::vector<double>& features, 
                                   double actual_return) {
    // Update the appropriate predictor
    for (size_t i = 0; i < predictors_.size(); ++i) {
        if (configs_[i].horizon_bars == bars_ago) {
            predictors_[i]->update(features, actual_return);
            break;
        }
    }
}

} // namespace learning
} // namespace sentio

```

## 📄 **FILE 18 of 19**: src/strategy/multi_symbol_oes_manager.cpp

**File Information**:
- **Path**: `src/strategy/multi_symbol_oes_manager.cpp`

- **Size**: 404 lines
- **Modified**: 2025-10-15 03:29:22

- **Type**: .cpp

```text
#include "strategy/multi_symbol_oes_manager.h"
#include "common/utils.h"
#include <iostream>

namespace sentio {

MultiSymbolOESManager::MultiSymbolOESManager(
    const Config& config,
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr
)
    : config_(config)
    , data_mgr_(data_mgr) {

    utils::log_info("MultiSymbolOESManager initializing for " +
                   std::to_string(config_.symbols.size()) + " symbols");

    // Create OES instance for each symbol
    for (const auto& symbol : config_.symbols) {
        // Use symbol-specific config if available, otherwise use base config
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
        if (config_.symbol_configs.count(symbol) > 0) {
            oes_config = config_.symbol_configs.at(symbol);
            utils::log_info("  " + symbol + ": Using custom config");
        } else {
            oes_config = config_.base_config;
            utils::log_info("  " + symbol + ": Using base config");
        }

        // Create OES instance
        auto oes = std::make_unique<OnlineEnsembleStrategy>(oes_config);
        oes_instances_[symbol] = std::move(oes);
    }

    utils::log_info("MultiSymbolOESManager initialized: " +
                   std::to_string(oes_instances_.size()) + " instances created");
}

// === Signal Generation ===

std::map<std::string, SignalOutput> MultiSymbolOESManager::generate_all_signals() {
    std::map<std::string, SignalOutput> signals;

    auto snapshot = data_mgr_->get_latest_snapshot();

    // DEBUG: Log snapshot status
    static int debug_count = 0;
    if (debug_count < 5) {
        utils::log_info("DEBUG generate_all_signals: snapshot has " +
                       std::to_string(snapshot.snapshots.size()) + " symbols");
        std::cout << "[OES] generate_all_signals: snapshot has " << snapshot.snapshots.size() << " symbols: ";
        for (const auto& [symbol, _] : snapshot.snapshots) {
            std::cout << symbol << " ";
        }
        std::cout << std::endl;
        debug_count++;
    }

    for (const auto& symbol : config_.symbols) {
        // Check if symbol has valid data
        if (snapshot.snapshots.count(symbol) == 0) {
            static std::map<std::string, int> warning_counts;
            if (warning_counts[symbol] < 3) {
                utils::log_warning("No data for " + symbol + " - skipping signal");
                std::cout << "[OES]   " << symbol << ": No data in snapshot - skipping" << std::endl;
                warning_counts[symbol]++;
            }
            continue;
        }

        const auto& sym_snap = snapshot.snapshots.at(symbol);
        if (!sym_snap.is_valid) {
            static std::map<std::string, int> stale_counts;
            if (stale_counts[symbol] < 3) {
                utils::log_warning("Stale data for " + symbol + " (" +
                                 std::to_string(sym_snap.staleness_seconds) + "s) - skipping signal");
                std::cout << "[OES]   " << symbol << ": Stale data (" << sym_snap.staleness_seconds << "s) - skipping" << std::endl;
                stale_counts[symbol]++;
            }
            continue;
        }

        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_error("No OES instance for " + symbol);
            std::cout << "[OES]   " << symbol << ": No OES instance - skipping" << std::endl;
            continue;
        }

        // Check if OES is ready
        if (!it->second->is_ready()) {
            static std::map<std::string, int> not_ready_counts;
            if (not_ready_counts[symbol] < 3) {
                std::cout << "[OES]   " << symbol << ": OES not ready - skipping" << std::endl;
                not_ready_counts[symbol]++;
            }
            continue;
        }

        // Generate signal
        SignalOutput signal = it->second->generate_signal(sym_snap.latest_bar);

        // DEBUG: Check for NaN in signal
        static int nan_signal_count = 0;
        if (nan_signal_count < 5 && signal.probability == 0.5) {
            std::cout << "[OES]   " << symbol << ": NEUTRAL signal (prob=0.5) - might be due to NaN features" << std::endl;
            nan_signal_count++;
        }

        // Apply staleness weighting to probability
        // Reduce confidence in signal if data is old
        signal.probability *= sym_snap.staleness_weight;

        signals[symbol] = signal;
        total_signals_generated_++;

        // Debug first few signals
        static int signal_debug_count = 0;
        if (signal_debug_count < 3) {
            std::cout << "[OES]   " << symbol << ": Generated signal (type=" << static_cast<int>(signal.signal_type)
                      << ", prob=" << signal.probability << ")" << std::endl;
            signal_debug_count++;
        }
    }

    std::cout << "[OES] Returning " << signals.size() << " signals" << std::endl;
    return signals;
}

SignalOutput MultiSymbolOESManager::generate_signal(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return SignalOutput();  // Return empty signal
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return SignalOutput();
    }

    SignalOutput signal = it->second->generate_signal(bar);
    total_signals_generated_++;

    return signal;
}

// === Learning Updates ===

void MultiSymbolOESManager::update_all(const std::map<std::string, double>& realized_pnls) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& [symbol, realized_pnl] : realized_pnls) {
        // Get OES instance
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            utils::log_warning("No OES instance for " + symbol + " - cannot update");
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            utils::log_warning("No data for " + symbol + " - cannot update");
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Update OES
        it->second->update(bar, realized_pnl);
        total_updates_++;
    }
}

void MultiSymbolOESManager::update(const std::string& symbol, double realized_pnl) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    Bar bar;
    if (!get_latest_bar(symbol, bar)) {
        utils::log_warning("No valid bar for " + symbol);
        return;
    }

    it->second->update(bar, realized_pnl);
    total_updates_++;
}

void MultiSymbolOESManager::on_bar() {
    auto snapshot = data_mgr_->get_latest_snapshot();

    for (const auto& symbol : config_.symbols) {
        auto it = oes_instances_.find(symbol);
        if (it == oes_instances_.end()) {
            continue;
        }

        // Get latest bar
        if (snapshot.snapshots.count(symbol) == 0) {
            continue;
        }

        const auto& bar = snapshot.snapshots.at(symbol).latest_bar;

        // Call on_bar for each OES
        it->second->on_bar(bar);
    }
}

// === Warmup ===

bool MultiSymbolOESManager::warmup_all(
    const std::map<std::string, std::vector<Bar>>& symbol_bars
) {
    utils::log_info("Warming up all OES instances...");

    bool all_success = true;
    for (const auto& [symbol, bars] : symbol_bars) {
        if (!warmup(symbol, bars)) {
            utils::log_error("Warmup failed for " + symbol);
            all_success = false;
        }
    }

    if (all_success) {
        utils::log_info("All OES instances warmed up successfully");
    } else {
        utils::log_warning("Some OES instances failed warmup");
    }

    return all_success;
}

bool MultiSymbolOESManager::warmup(const std::string& symbol, const std::vector<Bar>& bars) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return false;
    }

    utils::log_info("Warming up " + symbol + " with " + std::to_string(bars.size()) + " bars...");
    std::cout << "[OESManager::warmup] Starting warmup for " << symbol
              << " with " << bars.size() << " bars" << std::endl;

    // Feed bars one by one
    for (size_t i = 0; i < bars.size(); ++i) {
        it->second->on_bar(bars[i]);

        // Debug first few warmup calls
        if (i < 3) {
            std::cout << "[OESManager::warmup]   Bar " << i << " processed" << std::endl;
        }
    }

    std::cout << "[OESManager::warmup] Completed " << bars.size() << " warmup bars for " << symbol << std::endl;

    // Check if ready
    bool ready = it->second->is_ready();
    if (ready) {
        utils::log_info("  " + symbol + ": Warmup complete - ready for trading");
        std::cout << "[OESManager::warmup]   " << symbol << ": READY" << std::endl;
    } else {
        utils::log_warning("  " + symbol + ": Warmup incomplete - needs more data");
        std::cout << "[OESManager::warmup]   " << symbol << ": NOT READY" << std::endl;
    }

    return ready;
}

// === Configuration ===

void MultiSymbolOESManager::update_config(
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    utils::log_info("Updating config for all OES instances");

    config_.base_config = new_config;

    for (auto& [symbol, oes] : oes_instances_) {
        // Only update if not using custom config
        if (config_.symbol_configs.count(symbol) == 0) {
            oes->update_config(new_config);
        }
    }
}

void MultiSymbolOESManager::update_config(
    const std::string& symbol,
    const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config
) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return;
    }

    utils::log_info("Updating config for " + symbol);
    it->second->update_config(new_config);

    // Save as custom config
    config_.symbol_configs[symbol] = new_config;
}

// === Diagnostics ===

std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
MultiSymbolOESManager::get_all_performance_metrics() const {
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics> metrics;

    for (const auto& [symbol, oes] : oes_instances_) {
        metrics[symbol] = oes->get_performance_metrics();
    }

    return metrics;
}

OnlineEnsembleStrategy::PerformanceMetrics
MultiSymbolOESManager::get_performance_metrics(const std::string& symbol) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        utils::log_error("No OES instance for " + symbol);
        return OnlineEnsembleStrategy::PerformanceMetrics();
    }

    return it->second->get_performance_metrics();
}

bool MultiSymbolOESManager::all_ready() const {
    for (const auto& [symbol, oes] : oes_instances_) {
        if (!oes->is_ready()) {
            // Log which symbol isn't ready and why (debug only, limit output)
            static std::map<std::string, int> log_count;
            if (log_count[symbol] < 3) {
                std::cout << "[MultiSymbolOES] " << symbol << " not ready" << std::endl;
                log_count[symbol]++;
            }
            return false;
        }
    }
    return !oes_instances_.empty();
}

std::map<std::string, bool> MultiSymbolOESManager::get_ready_status() const {
    std::map<std::string, bool> status;

    for (const auto& [symbol, oes] : oes_instances_) {
        status[symbol] = oes->is_ready();
    }

    return status;
}

std::map<std::string, OnlineEnsembleStrategy::LearningState>
MultiSymbolOESManager::get_all_learning_states() const {
    std::map<std::string, OnlineEnsembleStrategy::LearningState> states;

    for (const auto& [symbol, oes] : oes_instances_) {
        states[symbol] = oes->get_learning_state();
    }

    return states;
}

OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(const std::string& symbol) {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const OnlineEnsembleStrategy* MultiSymbolOESManager::get_oes_instance(
    const std::string& symbol
) const {
    auto it = oes_instances_.find(symbol);
    if (it == oes_instances_.end()) {
        return nullptr;
    }
    return it->second.get();
}

// === Private Methods ===

bool MultiSymbolOESManager::get_latest_bar(const std::string& symbol, Bar& bar) {
    auto snapshot = data_mgr_->get_latest_snapshot();

    if (snapshot.snapshots.count(symbol) == 0) {
        return false;
    }

    const auto& sym_snap = snapshot.snapshots.at(symbol);
    if (!sym_snap.is_valid) {
        return false;
    }

    bar = sym_snap.latest_bar;
    return true;
}

} // namespace sentio

```

## 📄 **FILE 19 of 19**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 791 lines
- **Modified**: 2025-10-15 03:27:49

- **Type**: .cpp

```text
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0),
      current_regime_(MarketRegime::CHOPPY),
      bars_since_regime_check_(0) {

    static int oes_instance_count = 0;
    std::cout << "[OES::Constructor] Creating OES instance #" << oes_instance_count++ << std::endl;

    // Initialize feature engine V2 (production-grade with O(1) updates)
    features::EngineConfig engine_config;
    engine_config.momentum = true;
    engine_config.volatility = true;
    engine_config.volume = true;
    engine_config.normalize = true;
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>(engine_config);

    // Get feature count from V2 engine schema
    size_t num_features = feature_engine_->names().size();
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with zero warmup
    // EWRLS predictor starts immediately with high uncertainty
    // Strategy-level warmup ensures feature engine is ready
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 0;  // No warmup - start predicting immediately
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    // Initialize regime detection if enabled
    if (config_.enable_regime_detection) {
        // Use new adaptive detector with default params (vol_window=96, slope_window=120, chop_window=48)
        regime_detector_ = std::make_unique<MarketRegimeDetector>();
        regime_param_manager_ = std::make_unique<RegimeParameterManager>();
        utils::log_info("Regime detection enabled with adaptive thresholds - check interval: " +
                       std::to_string(config_.regime_check_interval) + " bars");
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    // CRITICAL: Ensure learning is current before generating signal
    if (!ensure_learning_current(bar)) {
        throw std::runtime_error(
            "[OnlineEnsemble] FATAL: Cannot generate signal - learning state is not current. "
            "Bar ID: " + std::to_string(bar.bar_id) +
            ", Last trained: " + std::to_string(learning_state_.last_trained_bar_id) +
            ", Bars behind: " + std::to_string(learning_state_.bars_behind));
    }

    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        static int not_ready_count = 0;
        if (not_ready_count < 3) {
            std::cout << "[OES::generate_signal] NOT READY: samples_seen=" << samples_seen_
                      << ", warmup_samples=" << config_.warmup_samples << std::endl;
            not_ready_count++;
        }
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check and update regime if enabled
    check_and_update_regime();

    // Extract features
    std::vector<double> features = extract_features(bar);

    if (features.empty()) {
        static int empty_features_count = 0;
        if (empty_features_count < 5) {
            std::cout << "[OES::generate_signal] EMPTY FEATURES (samples_seen=" << samples_seen_
                      << ", bar_history.size=" << bar_history_.size()
                      << ", feature_engine.is_seeded=" << feature_engine_->is_seeded() << ")" << std::endl;
            empty_features_count++;
        }
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        output.metadata["reason"] = "empty_features";
        return output;
    }

    // Check for NaN in critical features before prediction
    bool has_nan = false;
    for (size_t i = 0; i < features.size(); ++i) {
        if (!std::isfinite(features[i])) {
            has_nan = true;
            static int nan_count = 0;
            if (nan_count < 3) {
                std::cout << "[OES::generate_signal] NaN detected in feature " << i
                          << ", samples_seen=" << samples_seen_
                          << ", feature_engine.warmup_remaining=" << feature_engine_->warmup_remaining()
                          << std::endl;
                nan_count++;
            }
            break;
        }
    }

    if (has_nan) {
        // Return neutral signal with low confidence during warmup
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        output.metadata["reason"] = "indicators_warming_up";
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // DEBUG: Log prediction
    static int signal_count = 0;
    signal_count++;
    if (signal_count <= 10) {
        std::cout << "[OES] generate_signal #" << signal_count
                  << ": predicted_return=" << prediction.predicted_return
                  << ", confidence=" << prediction.confidence
                  << ", is_ready=" << prediction.is_ready << std::endl;
    }

    // Check for NaN in prediction
    if (!std::isfinite(prediction.predicted_return) || !std::isfinite(prediction.confidence)) {
        std::cerr << "[OES] WARNING: NaN in prediction! pred_return=" << prediction.predicted_return
                  << ", confidence=" << prediction.confidence << " - returning neutral" << std::endl;
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.confidence = 0.0;
        return output;
    }

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    if (signal_count <= 10) {
// DEBUG:         std::cout << "[OES]   → base_prob=" << base_prob << std::endl;
    }

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.confidence = prediction.confidence;  // FIX: Set confidence from prediction
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // DEBUG: Track on_bar calls during warmup
    static int on_bar_call_count = 0;
    if (on_bar_call_count < 3) {
        std::cout << "[OES::on_bar] Call #" << on_bar_call_count
                  << " - samples_seen=" << samples_seen_
                  << ", skip_feature_engine=" << skip_feature_engine_update_ << std::endl;
        on_bar_call_count++;
    }

    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine V2 (skip if using external cached features)
    if (!skip_feature_engine_update_) {
        feature_engine_->update(bar);

        // DEBUG: Confirm feature engine update
        static int fe_update_count = 0;
        if (fe_update_count < 3) {
            std::cout << "[OES::on_bar] Feature engine updated (call #" << fe_update_count << ")" << std::endl;
            fe_update_count++;
        }
    }

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);

    // Update learning state after processing this bar
    learning_state_.last_trained_bar_id = bar.bar_id;
    learning_state_.last_trained_bar_index = samples_seen_ - 1;  // 0-indexed
    learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
    learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    // Use external features if provided (for feature caching optimization)
    if (external_features_ != nullptr) {
        return *external_features_;
    }

    // DEBUG: Track why features might be empty
    static int extract_count = 0;
    extract_count++;

    if (bar_history_.size() < MIN_FEATURES_BARS) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": bar_history_.size()=" << bar_history_.size()
// DEBUG:                       << " < MIN_FEATURES_BARS=" << MIN_FEATURES_BARS
// DEBUG:                       << " → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_seeded()) {
        if (extract_count <= 10) {
// DEBUG:             std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                       << ": feature_engine_v2 NOT ready → returning empty"
// DEBUG:                       << std::endl;
        }
        return {};
    }

    // Get features from V2 engine (returns const vector& - no copy)
    const auto& features_view = feature_engine_->features_view();
    std::vector<double> features(features_view.begin(), features_view.end());
    if (extract_count <= 10 || features.empty()) {
// DEBUG:         std::cout << "[OES] extract_features #" << extract_count
// DEBUG:                   << ": got " << features.size() << " features from engine"
// DEBUG:                   << std::endl;
    }

    return features;
}

void OnlineEnsembleStrategy::train_predictor(const std::vector<double>& features, double realized_return) {
    if (features.empty()) {
        return;  // Nothing to train on
    }

    // Train all horizon predictors with the same realized return
    // (In practice, each horizon would use its own future return, but for warmup we use next-bar return)
    for (int horizon : config_.prediction_horizons) {
        ensemble_predictor_->update(horizon, features, realized_return);
    }
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    // Create shared_ptr only once per bar (reuse for all horizons)
    static std::shared_ptr<const std::vector<double>> shared_features;
    static int last_bar_index = -1;

    if (bar_index != last_bar_index) {
        // New bar - create new shared features
        shared_features = std::make_shared<const std::vector<double>>(features);
        last_bar_index = bar_index;
    }

    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = shared_features;  // Share, don't copy
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    // Use fixed array instead of vector
    auto& update = pending_updates_[pred.target_bar_index];
    if (update.count < 3) {
        update.horizons[update.count++] = std::move(pred);  // Move, don't copy
    }
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& update = it->second;

        // Process only the valid predictions (0 to count-1)
        for (uint8_t i = 0; i < update.count; ++i) {
            const auto& pred = update.horizons[i];

            double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
            if (!pred.is_long) {
                actual_return = -actual_return;
            }

            // Dereference shared_ptr only when needed
            ensemble_predictor_->update(pred.horizon, *pred.features, actual_return);
        }

        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed " + std::to_string(static_cast<int>(update.count)) +
                           " updates at bar " + std::to_string(samples_seen_) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

// ============================================================================
// Learning State Management - Ensures model is always current before signals
// ============================================================================

bool OnlineEnsembleStrategy::ensure_learning_current(const Bar& bar) {
    // Check if this is the first bar (initial state)
    if (learning_state_.last_trained_bar_id == -1) {
        // First bar - just update state, don't train yet
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_warmed_up = (samples_seen_ >= config_.warmup_samples);
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // Check if we're already current with this bar
    if (learning_state_.last_trained_bar_id == bar.bar_id) {
        return true;  // Already trained on this bar
    }

    // Calculate how many bars behind we are
    int64_t bars_behind = bar.bar_id - learning_state_.last_trained_bar_id;

    if (bars_behind < 0) {
        // Going backwards in time - this should only happen during replay/testing
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Bar ID went backwards! "
                  << "Current: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << " (replaying historical data)" << std::endl;

        // Reset learning state for replay
        learning_state_.last_trained_bar_id = bar.bar_id;
        learning_state_.last_trained_bar_index = samples_seen_;
        learning_state_.last_trained_timestamp_ms = bar.timestamp_ms;
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    if (bars_behind == 0) {
        return true;  // Current bar
    }

    if (bars_behind == 1) {
        // Normal case: exactly 1 bar behind (typical sequential processing)
        learning_state_.is_learning_current = true;
        learning_state_.bars_behind = 0;
        return true;
    }

    // We're more than 1 bar behind - need to catch up
    learning_state_.bars_behind = static_cast<int>(bars_behind);
    learning_state_.is_learning_current = false;

    // Only warn if feature engine is warmed up
    // (during warmup, it's normal to skip bars)
    if (learning_state_.is_warmed_up) {
        std::cerr << "⚠️  [OnlineEnsemble] WARNING: Learning engine is " << bars_behind << " bars behind!"
                  << std::endl;
        std::cerr << "    Current bar ID: " << bar.bar_id
                  << ", Last trained: " << learning_state_.last_trained_bar_id
                  << std::endl;
        std::cerr << "    This should only happen during warmup. Once warmed up, "
                  << "the system must stay fully updated." << std::endl;

        // In production live trading, this is FATAL
        // Cannot generate signals without being current
        return false;
    }

    // During warmup, it's OK to be behind
    // Mark as current and continue
    learning_state_.is_learning_current = true;
    learning_state_.bars_behind = 0;
    return true;
}

void OnlineEnsembleStrategy::check_and_update_regime() {
    if (!config_.enable_regime_detection || !regime_detector_) {
        return;
    }

    // Check regime periodically
    bars_since_regime_check_++;
    if (bars_since_regime_check_ < config_.regime_check_interval) {
        return;
    }

    bars_since_regime_check_ = 0;

    // Need sufficient history
    if (bar_history_.size() < static_cast<size_t>(config_.regime_lookback_period)) {
        return;
    }

    // Detect current regime
    std::vector<Bar> recent_bars(bar_history_.end() - config_.regime_lookback_period,
                                 bar_history_.end());
    MarketRegime new_regime = regime_detector_->detect_regime(recent_bars);

    // Switch parameters if regime changed
    if (new_regime != current_regime_) {
        MarketRegime old_regime = current_regime_;
        current_regime_ = new_regime;

        RegimeParams params = regime_param_manager_->get_params_for_regime(new_regime);

        // Apply new thresholds
        current_buy_threshold_ = params.buy_threshold;
        current_sell_threshold_ = params.sell_threshold;

        // Log regime transition
        utils::log_info("Regime transition: " +
                       MarketRegimeDetector::regime_to_string(old_regime) + " -> " +
                       MarketRegimeDetector::regime_to_string(new_regime) +
                       " | buy=" + std::to_string(current_buy_threshold_) +
                       " sell=" + std::to_string(current_sell_threshold_) +
                       " lambda=" + std::to_string(params.ewrls_lambda) +
                       " bb=" + std::to_string(params.bb_amplification_factor));

        // Note: For full regime switching, we would also update:
        // - config_.ewrls_lambda (requires rebuilding predictor)
        // - config_.bb_amplification_factor
        // - config_.horizon_weights
        // For now, only threshold switching is implemented (most impactful)
    }
}

} // namespace sentio

```

