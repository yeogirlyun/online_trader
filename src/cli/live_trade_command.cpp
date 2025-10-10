#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "live/broker_client_interface.h"
#include "live/bar_feed_interface.h"
#include "live/mock_broker.h"
#include "live/mock_bar_feed_replay.h"
#include "live/alpaca_client_adapter.h"
#include "live/polygon_client_adapter.h"
#include "live/alpaca_rest_bar_feed.h"
#include "live/mock_config.h"
#include "live/state_persistence.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include "common/time_utils.h"
#include "common/bar_validator.h"
#include "common/exceptions.h"
#include "common/eod_state.h"
#include "common/nyse_calendar.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <optional>
#include <memory>
#include <csignal>
#include <atomic>

namespace sentio {
namespace cli {

// Global pointer for signal handler (necessary for C-style signal handlers)
static std::atomic<bool> g_shutdown_requested{false};

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 *
 * Now loads optimized parameters from midday_selected_params.json if available
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config() {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // Default v1.0 parameters
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 7800;  // 20 blocks @ 390 bars/block
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;
    config.bb_period = 20;
    config.bb_std_dev = 2.0;
    config.bb_proximity_threshold = 0.30;
    config.regularization = 0.01;
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;
    config.enable_regime_detection = false;
    config.regime_check_interval = 60;

    // Try to load optimized parameters from JSON file
    std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
    std::ifstream file(json_file);

    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            file.close();

            // Load phase 1 parameters
            config.buy_threshold = j.value("buy_threshold", config.buy_threshold);
            config.sell_threshold = j.value("sell_threshold", config.sell_threshold);
            config.bb_amplification_factor = j.value("bb_amplification_factor", config.bb_amplification_factor);
            config.ewrls_lambda = j.value("ewrls_lambda", config.ewrls_lambda);

            // Load phase 2 parameters
            double h1 = j.value("h1_weight", 0.3);
            double h5 = j.value("h5_weight", 0.5);
            double h10 = j.value("h10_weight", 0.2);
            config.horizon_weights = {h1, h5, h10};
            config.bb_period = j.value("bb_period", config.bb_period);
            config.bb_std_dev = j.value("bb_std_dev", config.bb_std_dev);
            config.bb_proximity_threshold = j.value("bb_proximity", config.bb_proximity_threshold);
            config.regularization = j.value("regularization", config.regularization);

            std::cout << "✅ Loaded optimized parameters from: " << json_file << std::endl;
            std::cout << "   Source: " << j.value("source", "unknown") << std::endl;
            std::cout << "   MRB target: " << j.value("expected_mrb", 0.0) << "%" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Failed to load optimized parameters: " << e.what() << std::endl;
            std::cerr << "   Using default configuration" << std::endl;
        }
    }

    return config;
}

/**
 * Load leveraged ETF prices from CSV files for mock mode
 * Returns: map[timestamp_sec][symbol] -> close_price
 */
static std::unordered_map<uint64_t, std::unordered_map<std::string, double>>
load_leveraged_prices(const std::string& base_path) {
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> prices;

    std::vector<std::string> symbols = {"SH", "SDS", "SPXL"};

    for (const auto& symbol : symbols) {
        std::string filepath = base_path + "/" + symbol + "_yesterday.csv";
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "⚠️  Warning: Could not load " << filepath << std::endl;
            continue;
        }

        std::string line;
        int line_count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string date_str, ts_str, o, h, l, close_str, v;

            if (std::getline(iss, date_str, ',') &&
                std::getline(iss, ts_str, ',') &&
                std::getline(iss, o, ',') &&
                std::getline(iss, h, ',') &&
                std::getline(iss, l, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, v)) {

                uint64_t timestamp_sec = std::stoull(ts_str);
                double close_price = std::stod(close_str);

                prices[timestamp_sec][symbol] = close_price;
                line_count++;
            }
        }

        if (line_count > 0) {
            std::cout << "✅ Loaded " << line_count << " bars for " << symbol << std::endl;
        }
    }

    return prices;
}

/**
 * Live Trading Runner for OnlineEnsemble Strategy v1.0
 *
 * - Trades SPY/SDS/SPXL/SH during regular hours (9:30am - 4:00pm ET)
 * - Uses OnlineEnsemble EWRLS with asymmetric thresholds
 * - Comprehensive logging of all decisions and trades
 */
class LiveTrader {
public:
    LiveTrader(std::unique_ptr<IBrokerClient> broker,
               std::unique_ptr<IBarFeed> bar_feed,
               const std::string& log_dir,
               bool is_mock_mode = false,
               const std::string& data_file = "")
        : broker_(std::move(broker))
        , bar_feed_(std::move(bar_feed))
        , log_dir_(log_dir)
        , is_mock_mode_(is_mock_mode)
        , data_file_(data_file)
        , strategy_(create_v1_config())
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
        , previous_portfolio_value_(100000.0)  // Initialize to starting equity
        , et_time_()  // Initialize ET time manager
        , eod_state_(log_dir + "/eod_state.txt")  // Persistent EOD tracking
        , nyse_calendar_()  // NYSE holiday calendar
        , state_persistence_(std::make_unique<StatePersistence>(log_dir + "/state"))  // State persistence
    {
        // Initialize log files
        init_logs();

        // SPY trading configuration (maps to sentio PSM states)
        symbol_map_ = {
            {"SPY", "SPY"},      // Base 1x
            {"SPXL", "SPXL"},    // Bull 3x
            {"SH", "SH"},        // Bear -1x
            {"SDS", "SDS"}       // Bear -2x
        };
    }

    ~LiveTrader() {
        // Generate dashboard on exit
        generate_dashboard();
    }

    void run() {
        if (is_mock_mode_) {
            log_system("=== OnlineTrader v1.0 Mock Trading Started ===");
            log_system("Mode: MOCK REPLAY (39x speed)");
        } else {
            log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
            log_system("Mode: LIVE TRADING");
        }
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to broker (Alpaca or Mock)
        log_system(is_mock_mode_ ? "Initializing Mock Broker..." : "Connecting to Alpaca Paper Trading...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account");
            return;
        }
        log_system("✓ Account ready - ID: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to bar feed (Polygon or Mock)
        log_system(is_mock_mode_ ? "Loading mock bar feed..." : "Connecting to Polygon proxy...");
        if (!bar_feed_->connect()) {
            log_error("Failed to connect to bar feed");
            return;
        }
        log_system(is_mock_mode_ ? "✓ Mock bars loaded" : "✓ Connected to Polygon");

        // In mock mode, load leveraged ETF prices
        if (is_mock_mode_) {
            log_system("Loading leveraged ETF prices for mock mode...");
            leveraged_prices_ = load_leveraged_prices("/tmp");
            if (!leveraged_prices_.empty()) {
                log_system("✓ Leveraged ETF prices loaded (SH, SDS, SPXL)");
            } else {
                log_system("⚠️  Warning: No leveraged ETF prices loaded - using fallback prices");
            }
            log_system("");
        }

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!bar_feed_->subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Reconcile existing positions on startup (seamless continuation)
        reconcile_startup_positions();

        // Check for missed EOD and startup catch-up liquidation
        check_startup_eod_catch_up();

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Start main trading loop
        bar_feed_->start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Install signal handlers for graceful shutdown
        std::signal(SIGINT, [](int) { g_shutdown_requested = true; });
        std::signal(SIGTERM, [](int) { g_shutdown_requested = true; });

        // Keep running until shutdown requested
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Auto-shutdown at market close (4:00 PM ET) after EOD liquidation completes
            std::string today_et = et_time_.get_current_et_date();
            if (et_time_.is_market_close_time() && eod_state_.is_eod_complete(today_et)) {
                log_system("⏰ Market closed and EOD complete - initiating automatic shutdown");
                g_shutdown_requested = true;
            }
        }

        log_system("=== Shutdown requested - cleaning up ===");
    }

private:
    std::unique_ptr<IBrokerClient> broker_;
    std::unique_ptr<IBarFeed> bar_feed_;
    std::string log_dir_;
    bool is_mock_mode_;
    std::string data_file_;  // Path to market data CSV file for dashboard generation
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // NEW: Production safety infrastructure
    PositionBook position_book_;
    ETTimeManager et_time_;  // Centralized ET time management
    EodStateStore eod_state_;  // Idempotent EOD tracking
    NyseCalendar nyse_calendar_;  // Holiday and half-day calendar
    std::unique_ptr<StatePersistence> state_persistence_;  // Atomic state persistence
    std::optional<Bar> previous_bar_;  // For bar-to-bar learning
    uint64_t bar_count_{0};

    // Mid-day optimization (15:15 PM ET / 3:15pm)
    std::vector<Bar> todays_bars_;  // Collect ALL bars from 9:30 onwards
    bool midday_optimization_done_{false};  // Flag to track if optimization ran today
    std::string midday_optimization_date_;  // Date of last optimization (YYYY-MM-DD)

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;
    double previous_portfolio_value_;  // Track portfolio value before trade for P&L calculation

    // Mock mode: Leveraged ETF prices loaded from CSV
    std::unordered_map<uint64_t, std::unordered_map<std::string, double>> leveraged_prices_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;
    std::string session_timestamp_;  // Store timestamp for dashboard generation

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        session_timestamp_ = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + session_timestamp_ + ".log");
        log_signals_.open(log_dir_ + "/signals_" + session_timestamp_ + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + session_timestamp_ + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + session_timestamp_ + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + session_timestamp_ + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        return et_time_.get_current_et_string();
    }

    bool is_regular_hours() const {
        return et_time_.is_regular_hours();
    }

    bool is_end_of_day_liquidation_time() const {
        return et_time_.is_eod_liquidation_window();
    }

    void log_system(const std::string& message) {
        auto timestamp = get_timestamp_readable();
        std::cout << "[" << timestamp << "] " << message << std::endl;
        log_system_ << "[" << timestamp << "] " << message << std::endl;
        log_system_.flush();
    }

    void log_error(const std::string& message) {
        log_system("ERROR: " + message);
    }

    void generate_dashboard() {
        // Close log files to ensure all data is flushed
        log_system_.close();
        log_signals_.close();
        log_trades_.close();
        log_positions_.close();
        log_decisions_.close();

        std::cout << "\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "📊 Generating Trading Dashboard...\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

        // Construct file paths
        std::string trades_file = log_dir_ + "/trades_" + session_timestamp_ + ".jsonl";
        std::string signals_file = log_dir_ + "/signals_" + session_timestamp_ + ".jsonl";
        std::string dashboard_dir = "data/dashboards";
        std::string dashboard_file = dashboard_dir + "/session_" + session_timestamp_ + ".html";

        // Create dashboard directory
        system(("mkdir -p " + dashboard_dir).c_str());

        // Build Python command
        std::string python_cmd = "python3 tools/professional_trading_dashboard.py "
                                "--tradebook " + trades_file + " "
                                "--signals " + signals_file + " "
                                "--output " + dashboard_file + " "
                                "--start-equity 100000 ";

        // Add data file if available (for candlestick charts and trade markers)
        if (!data_file_.empty()) {
            python_cmd += "--data " + data_file_ + " ";
        }

        python_cmd += "> /dev/null 2>&1";

        std::cout << "  Tradebook: " << trades_file << "\n";
        std::cout << "  Signals: " << signals_file << "\n";
        if (!data_file_.empty()) {
            std::cout << "  Data: " + data_file_ + "\n";
        }
        std::cout << "  Output: " << dashboard_file << "\n";
        std::cout << "\n";

        // Execute Python dashboard generator
        int result = system(python_cmd.c_str());

        if (result == 0) {
            std::cout << "✅ Dashboard generated successfully!\n";
            std::cout << "   📂 Open: " << dashboard_file << "\n";
            std::cout << "\n";

            // Send email notification (works in both live and mock modes)
            std::cout << "📧 Sending email notification...\n";

            std::string email_cmd = "python3 tools/send_dashboard_email.py "
                                   "--dashboard " + dashboard_file + " "
                                   "--trades " + trades_file + " "
                                   "--recipient yeogirl@gmail.com "
                                   "> /dev/null 2>&1";

            int email_result = system(email_cmd.c_str());

            if (email_result == 0) {
                std::cout << "✅ Email sent to yeogirl@gmail.com\n";
            } else {
                std::cout << "⚠️  Email sending failed (check GMAIL_APP_PASSWORD)\n";
            }
        } else {
            std::cout << "⚠️  Dashboard generation failed (exit code: " << result << ")\n";
            std::cout << "   You can manually generate it with:\n";
            std::cout << "   python3 tools/professional_trading_dashboard.py \\\n";
            std::cout << "     --tradebook " << trades_file << " \\\n";
            std::cout << "     --signals " << signals_file << " \\\n";
            std::cout << "     --output " << dashboard_file << "\n";
        }

        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "\n";
    }

    void reconcile_startup_positions() {
        log_system("=== Startup Position Reconciliation ===");

        // Get current broker state
        auto account = broker_->get_account();
        if (!account) {
            log_error("Failed to get account info for startup reconciliation");
            return;
        }

        auto broker_positions = get_broker_positions();

        log_system("  Cash: $" + std::to_string(account->cash));
        log_system("  Portfolio Value: $" + std::to_string(account->portfolio_value));

        // ===================================================================
        // STEP 1: Try to load persisted state from previous session
        // ===================================================================
        if (auto persisted = state_persistence_->load_state()) {
            log_system("[STATE_PERSIST] ✓ Found persisted state from previous session");
            log_system("  Session ID: " + persisted->session_id);
            log_system("  Last save: " + persisted->last_bar_time_str);
            log_system("  PSM State: " + psm_.state_to_string(persisted->psm_state));
            log_system("  Bars held: " + std::to_string(persisted->bars_held));

            // Validate positions match broker
            bool positions_match = validate_positions_match(persisted->positions, broker_positions);

            if (positions_match) {
                log_system("[STATE_PERSIST] ✓ Positions match broker - restoring exact state");

                // Restore exact state
                current_state_ = persisted->psm_state;
                bars_held_ = persisted->bars_held;
                entry_equity_ = persisted->entry_equity;

                // Calculate bars elapsed since last save
                if (previous_bar_.has_value()) {
                    uint64_t bars_elapsed = calculate_bars_since(
                        persisted->last_bar_timestamp,
                        previous_bar_->timestamp_ms
                    );
                    bars_held_ += bars_elapsed;
                    log_system("  Adjusted bars held: " + std::to_string(bars_held_) +
                              " (+" + std::to_string(bars_elapsed) + " bars since save)");
                }

                // Initialize position book
                for (const auto& pos : broker_positions) {
                    position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
                }

                log_system("✓ State fully recovered from persistence");
                log_system("");
                return;
            } else {
                log_system("[STATE_PERSIST] ⚠️  Position mismatch - falling back to broker reconciliation");
            }
        } else {
            log_system("[STATE_PERSIST] No persisted state found - using broker reconciliation");
        }

        // ===================================================================
        // STEP 2: Fall back to broker-based reconciliation
        // ===================================================================
        if (broker_positions.empty()) {
            log_system("  Current Positions: NONE (starting flat)");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;
            log_system("  Initial State: CASH_ONLY");
            log_system("  Bars Held: 0 (no positions)");
        } else {
            log_system("  Current Positions:");
            for (const auto& pos : broker_positions) {
                log_system("    " + pos.symbol + ": " +
                          std::to_string(pos.qty) + " shares @ $" +
                          std::to_string(pos.avg_entry_price) +
                          " (P&L: $" + std::to_string(pos.unrealized_pnl) + ")");

                // Initialize position book with existing positions
                position_book_.set_position(pos.symbol, pos.qty, pos.avg_entry_price);
            }

            // Infer current PSM state from positions
            current_state_ = infer_state_from_positions(broker_positions);

            // CRITICAL FIX: Set bars_held to MIN_HOLD_BARS to allow immediate exits
            // since we don't know how long the positions have been held
            bars_held_ = MIN_HOLD_BARS;

            log_system("  Inferred PSM State: " + psm_.state_to_string(current_state_));
            log_system("  Bars Held: " + std::to_string(bars_held_) +
                      " (set to MIN_HOLD to allow immediate exits on startup)");
            log_system("  NOTE: Positions were reconciled from broker - assuming min hold satisfied");
        }

        log_system("✓ Startup reconciliation complete - resuming trading seamlessly");
        log_system("");
    }

    void check_startup_eod_catch_up() {
        log_system("=== Startup EOD Catch-Up Check ===");

        auto et_tm = et_time_.get_current_et_tm();
        std::string today_et = format_et_date(et_tm);
        std::string prev_trading_day = get_previous_trading_day(et_tm);

        log_system("  Current ET Time: " + et_time_.get_current_et_string());
        log_system("  Today (ET): " + today_et);
        log_system("  Previous Trading Day: " + prev_trading_day);

        // Check 1: Did we miss previous trading day's EOD?
        if (!eod_state_.is_eod_complete(prev_trading_day)) {
            log_system("  ⚠️  WARNING: Previous trading day's EOD not completed");

            auto broker_positions = get_broker_positions();
            if (!broker_positions.empty()) {
                log_system("  ⚠️  Open positions detected - executing catch-up liquidation");
                liquidate_all_positions();
                eod_state_.mark_eod_complete(prev_trading_day);
                log_system("  ✓ Catch-up liquidation complete for " + prev_trading_day);
            } else {
                log_system("  ✓ No open positions - marking previous EOD as complete");
                eod_state_.mark_eod_complete(prev_trading_day);
            }
        } else {
            log_system("  ✓ Previous trading day EOD already complete");
        }

        // Check 2: Started outside trading hours with positions?
        if (et_time_.should_liquidate_on_startup(has_open_positions())) {
            log_system("  ⚠️  Started outside trading hours with open positions");
            log_system("  ⚠️  Executing immediate liquidation");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("  ✓ Startup liquidation complete");
        }

        log_system("✓ Startup EOD check complete");
        log_system("");
    }

    std::string format_et_date(const std::tm& tm) const {
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        return std::string(buffer);
    }

    std::string get_previous_trading_day(const std::tm& current_tm) const {
        // Walk back day-by-day until we find a trading day
        std::tm tm = current_tm;
        for (int i = 1; i <= 10; ++i) {
            // Subtract i days (approximate - good enough for recent history)
            std::time_t t = std::mktime(&tm) - (i * 86400);
            std::tm* prev_tm = std::localtime(&t);
            std::string prev_date = format_et_date(*prev_tm);

            // Check if weekday and not holiday
            if (prev_tm->tm_wday >= 1 && prev_tm->tm_wday <= 5) {
                if (nyse_calendar_.is_trading_day(prev_date)) {
                    return prev_date;
                }
            }
        }
        // Fallback: return today if can't find previous trading day
        return format_et_date(current_tm);
    }

    bool has_open_positions() {
        auto broker_positions = get_broker_positions();
        return !broker_positions.empty();
    }

    PositionStateMachine::State infer_state_from_positions(
        const std::vector<BrokerPosition>& positions) {

        // Map SPY instruments to equivalent QQQ PSM states
        // SPY/SPXL/SH/SDS → QQQ/TQQQ/PSQ/SQQQ
        bool has_base = false;   // SPY
        bool has_bull3x = false; // SPXL
        bool has_bear1x = false; // SH
        bool has_bear_nx = false; // SDS

        for (const auto& pos : positions) {
            if (pos.qty > 0) {
                if (pos.symbol == "SPXL") has_bull3x = true;
                if (pos.symbol == "SPY") has_base = true;
                if (pos.symbol == "SH") has_bear1x = true;
                if (pos.symbol == "SDS") has_bear_nx = true;
            }
        }

        // Check for dual-instrument states first
        if (has_base && has_bull3x) return PositionStateMachine::State::QQQ_TQQQ;    // BASE_BULL_3X
        if (has_bear1x && has_bear_nx) return PositionStateMachine::State::PSQ_SQQQ; // BEAR_1X_NX

        // Single instrument states
        if (has_bull3x) return PositionStateMachine::State::TQQQ_ONLY;  // BULL_3X_ONLY
        if (has_base) return PositionStateMachine::State::QQQ_ONLY;     // BASE_ONLY
        if (has_bear1x) return PositionStateMachine::State::PSQ_ONLY;   // BEAR_1X_ONLY
        if (has_bear_nx) return PositionStateMachine::State::SQQQ_ONLY; // BEAR_NX_ONLY

        return PositionStateMachine::State::CASH_ONLY;
    }

    // =====================================================================
    // State Persistence Helper Methods
    // =====================================================================

    /**
     * Calculate number of 1-minute bars elapsed between two timestamps
     */
    uint64_t calculate_bars_since(uint64_t from_ts_ms, uint64_t to_ts_ms) const {
        if (to_ts_ms <= from_ts_ms) return 0;
        uint64_t elapsed_ms = to_ts_ms - from_ts_ms;
        uint64_t elapsed_minutes = elapsed_ms / (60 * 1000);
        return elapsed_minutes;
    }

    /**
     * Validate that persisted positions match broker positions
     */
    bool validate_positions_match(
        const std::vector<StatePersistence::PositionDetail>& persisted,
        const std::vector<BrokerPosition>& broker) {

        // Quick check: same number of positions
        if (persisted.size() != broker.size()) {
            log_system("  Position count mismatch: persisted=" +
                      std::to_string(persisted.size()) +
                      " broker=" + std::to_string(broker.size()));
            return false;
        }

        // Build maps for easier comparison
        std::map<std::string, double> persisted_map;
        for (const auto& p : persisted) {
            persisted_map[p.symbol] = p.quantity;
        }

        std::map<std::string, double> broker_map;
        for (const auto& p : broker) {
            broker_map[p.symbol] = p.qty;
        }

        // Check each symbol
        for (const auto& [symbol, qty] : persisted_map) {
            if (broker_map.find(symbol) == broker_map.end()) {
                log_system("  Symbol mismatch: " + symbol + " in persisted but not in broker");
                return false;
            }
            if (std::abs(broker_map[symbol] - qty) > 0.01) {  // Allow tiny floating point difference
                log_system("  Quantity mismatch for " + symbol + ": persisted=" +
                          std::to_string(qty) + " broker=" + std::to_string(broker_map[symbol]));
                return false;
            }
        }

        return true;
    }

    /**
     * Persist current trading state to disk
     */
    void persist_current_state() {
        try {
            StatePersistence::TradingState state;
            state.psm_state = current_state_;
            state.bars_held = bars_held_;
            state.entry_equity = entry_equity_;

            if (previous_bar_.has_value()) {
                state.last_bar_timestamp = previous_bar_->timestamp_ms;
                state.last_bar_time_str = format_bar_time(*previous_bar_);
            }

            // Add current positions
            auto broker_positions = get_broker_positions();
            for (const auto& pos : broker_positions) {
                StatePersistence::PositionDetail detail;
                detail.symbol = pos.symbol;
                detail.quantity = pos.qty;
                detail.avg_entry_price = pos.avg_entry_price;
                detail.entry_timestamp = previous_bar_ ? previous_bar_->timestamp_ms : 0;
                state.positions.push_back(detail);
            }

            state.session_id = session_timestamp_;

            if (!state_persistence_->save_state(state)) {
                log_system("⚠️  State persistence failed (non-fatal - continuing)");
            }

        } catch (const std::exception& e) {
            log_system("⚠️  State persistence error: " + std::string(e.what()));
        }
    }

    void warmup_strategy() {
        // Load warmup data created by comprehensive_warmup.sh script
        // This file contains: 7864 warmup bars (20 blocks @ 390 bars/block + 64 feature bars) + all of today's bars up to now
        std::string warmup_file = "data/equities/SPY_warmup_latest.csv";

        // Try relative path first, then from parent directory
        std::ifstream file(warmup_file);
        if (!file.is_open()) {
            warmup_file = "../data/equities/SPY_warmup_latest.csv";
            file.open(warmup_file);
        }

        if (!file.is_open()) {
            log_system("WARNING: Could not open warmup file: " + warmup_file);
            log_system("         Run tools/warmup_live_trading.sh first!");
            log_system("         Strategy will learn from first few live bars");
            return;
        }

        // Read all bars from warmup file
        std::vector<Bar> all_bars;
        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string ts_str, open_str, high_str, low_str, close_str, volume_str;

            // CSV format: timestamp,open,high,low,close,volume
            if (std::getline(iss, ts_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_str);  // Already in milliseconds
                bar.open = std::stod(open_str);
                bar.high = std::stod(high_str);
                bar.low = std::stod(low_str);
                bar.close = std::stod(close_str);
                bar.volume = std::stoll(volume_str);
                all_bars.push_back(bar);
            }
        }
        file.close();

        if (all_bars.empty()) {
            log_system("WARNING: No bars loaded from warmup file");
            return;
        }

        log_system("Loaded " + std::to_string(all_bars.size()) + " bars from warmup file");
        log_system("");

        // Feed ALL bars (3900 warmup + today's bars)
        // This ensures we're caught up to the current time
        log_system("=== Starting Warmup Process ===");
        log_system("  Target: 3900 bars (10 blocks @ 390 bars/block)");
        log_system("  Available: " + std::to_string(all_bars.size()) + " bars");
        log_system("");

        int predictor_training_count = 0;
        int feature_engine_ready_bar = 0;
        int strategy_ready_bar = 0;

        for (size_t i = 0; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]);

            // Report feature engine ready
            if (i == 64 && feature_engine_ready_bar == 0) {
                feature_engine_ready_bar = i;
                log_system("✓ Feature Engine Warmup Complete (64 bars)");
                log_system("  - All rolling windows initialized");
                log_system("  - Technical indicators ready");
                log_system("  - Starting predictor training...");
                log_system("");
            }

            // Train predictor on bar-to-bar returns (wait for strategy to be fully ready)
            if (strategy_.is_ready() && i + 1 < all_bars.size()) {
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }

            // Report strategy ready
            if (strategy_.is_ready() && strategy_ready_bar == 0) {
                strategy_ready_bar = i;
                log_system("✓ Strategy Warmup Complete (" + std::to_string(i) + " bars)");
                log_system("  - EWRLS predictor fully trained");
                log_system("  - Multi-horizon predictions ready");
                log_system("  - Strategy ready for live trading");
                log_system("");
            }

            // Progress indicator every 1000 bars
            if ((i + 1) % 1000 == 0) {
                log_system("  Progress: " + std::to_string(i + 1) + "/" + std::to_string(all_bars.size()) +
                          " bars (" + std::to_string(predictor_training_count) + " training samples)");
            }

            // Update bar_count_ and previous_bar_ for seamless transition to live
            bar_count_++;
            previous_bar_ = all_bars[i];
        }

        log_system("");
        log_system("=== Warmup Summary ===");
        log_system("✓ Total bars processed: " + std::to_string(all_bars.size()));
        log_system("✓ Feature engine ready: Bar " + std::to_string(feature_engine_ready_bar));
        log_system("✓ Strategy ready: Bar " + std::to_string(strategy_ready_bar));
        log_system("✓ Predictor trained: " + std::to_string(predictor_training_count) + " samples");
        log_system("✓ Last warmup bar: " + format_bar_time(all_bars.back()));
        log_system("✓ Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("");
    }

    std::string format_bar_time(const Bar& bar) const {
        time_t time_t_val = static_cast<time_t>(bar.timestamp_ms / 1000);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void on_new_bar(const Bar& bar) {
        bar_count_++;

        // In mock mode, sync time manager to bar timestamp and update market prices
        if (is_mock_mode_) {
            et_time_.set_mock_time(bar.timestamp_ms);

            // Update MockBroker with current market prices
            auto* mock_broker = dynamic_cast<MockBroker*>(broker_.get());
            if (mock_broker) {
                // Update SPY price from bar
                mock_broker->update_market_price("SPY", bar.close);

                // Update leveraged ETF prices from loaded CSV data
                uint64_t bar_ts_sec = bar.timestamp_ms / 1000;

                // CRITICAL: Crash fast if no price data found (no silent fallbacks!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " (bar time: " +
                        get_timestamp_readable() + ")");
                }

                const auto& prices_at_ts = leveraged_prices_[bar_ts_sec];

                // Validate all required symbols have prices
                std::vector<std::string> required_symbols = {"SPXL", "SH", "SDS"};
                for (const auto& symbol : required_symbols) {
                    if (!prices_at_ts.count(symbol)) {
                        throw std::runtime_error(
                            "CRITICAL: Missing price for " + symbol +
                            " at timestamp " + std::to_string(bar_ts_sec));
                    }
                    mock_broker->update_market_price(symbol, prices_at_ts.at(symbol));
                }
            }
        }

        auto timestamp = get_timestamp_readable();

        // Log bar received
        log_system("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        log_system("📊 BAR #" + std::to_string(bar_count_) + " Received from Polygon");
        log_system("  Time: " + timestamp);
        log_system("  OHLC: O=$" + std::to_string(bar.open) + " H=$" + std::to_string(bar.high) +
                  " L=$" + std::to_string(bar.low) + " C=$" + std::to_string(bar.close));
        log_system("  Volume: " + std::to_string(bar.volume));

        // =====================================================================
        // STEP 1: Bar Validation (NEW - P4)
        // =====================================================================
        if (!is_valid_bar(bar)) {
            log_error("❌ Invalid bar dropped: " + BarValidator::get_error_message(bar));
            return;
        }
        log_system("✓ Bar validation passed");

        // =====================================================================
        // STEP 2: Feed to Strategy (ALWAYS - for continuous learning)
        // =====================================================================
        log_system("⚙️  Feeding bar to strategy (updating indicators)...");
        strategy_.on_bar(bar);

        // =====================================================================
        // STEP 3: Continuous Bar-to-Bar Learning (NEW - P1-1 fix)
        // =====================================================================
        if (previous_bar_.has_value()) {
            auto features = strategy_.extract_features(*previous_bar_);
            if (!features.empty()) {
                double return_1bar = (bar.close - previous_bar_->close) /
                                    previous_bar_->close;
                strategy_.train_predictor(features, return_1bar);
                log_system("✓ Predictor updated (learning from previous bar return: " +
                          std::to_string(return_1bar * 100) + "%)");
            }
        }
        previous_bar_ = bar;

        // =====================================================================
        // STEP 3.5: Increment bars_held counter (CRITICAL for min hold period)
        // =====================================================================
        if (current_state_ != PositionStateMachine::State::CASH_ONLY) {
            bars_held_++;
            log_system("📊 Position holding duration: " + std::to_string(bars_held_) + " bars");
        }

        // =====================================================================
        // STEP 4: Periodic Position Reconciliation (NEW - P0-3)
        // Skip in mock mode - no external broker to drift from
        // =====================================================================
        if (!is_mock_mode_ && bar_count_ % 60 == 0) {  // Every 60 bars (60 minutes)
            try {
                auto broker_positions = get_broker_positions();
                position_book_.reconcile_with_broker(broker_positions);
            } catch (const PositionReconciliationError& e) {
                log_error("[" + timestamp + "] RECONCILIATION FAILED: " +
                         std::string(e.what()));
                log_error("[" + timestamp + "] Initiating emergency flatten");
                liquidate_all_positions();
                throw;  // Exit for supervisor restart
            }
        }

        // =====================================================================
        // STEP 4.5: Persist State (Every 10 bars for low overhead)
        // =====================================================================
        if (bar_count_ % 10 == 0) {
            persist_current_state();
        }

        // =====================================================================
        // STEP 5: Check End-of-Day Liquidation (IDEMPOTENT)
        // =====================================================================
        std::string today_et = timestamp.substr(0, 10);  // Extract YYYY-MM-DD from timestamp

        // Check if today is a trading day
        if (!nyse_calendar_.is_trading_day(today_et)) {
            log_system("⏸️  Holiday/Weekend - no trading (learning continues)");
            return;
        }

        // Idempotent EOD check: only liquidate once per trading day
        if (is_end_of_day_liquidation_time() && !eod_state_.is_eod_complete(today_et)) {
            log_system("🔔 END OF DAY - Liquidation window active");
            liquidate_all_positions();
            eod_state_.mark_eod_complete(today_et);
            log_system("✓ EOD liquidation complete for " + today_et);
            return;
        }

        // =====================================================================
        // STEP 5.5: Mid-Day Optimization at 16:05 PM ET (NEW)
        // =====================================================================
        // Reset optimization flag for new trading day
        if (midday_optimization_date_ != today_et) {
            midday_optimization_done_ = false;
            midday_optimization_date_ = today_et;
            todays_bars_.clear();  // Clear today's bars for new day
        }

        // Collect ALL bars during regular hours (9:30-16:00) for optimization
        if (is_regular_hours()) {
            todays_bars_.push_back(bar);

            // Check if it's 15:15 PM ET and optimization hasn't been done yet
            if (et_time_.is_midday_optimization_time() && !midday_optimization_done_) {
                log_system("🔔 MID-DAY OPTIMIZATION TIME (15:15 PM ET / 3:15pm)");

                // Liquidate all positions before optimization
                log_system("Liquidating all positions before optimization...");
                liquidate_all_positions();
                log_system("✓ Positions liquidated - going 100% cash");

                // Run optimization
                run_midday_optimization();

                // Mark as done
                midday_optimization_done_ = true;

                // Skip trading for this bar (optimization takes time)
                return;
            }
        }

        // =====================================================================
        // STEP 6: Trading Hours Gate (NEW - only trade during RTH, before EOD)
        // =====================================================================
        if (!is_regular_hours()) {
            log_system("⏰ After-hours - learning only, no trading");
            return;  // Learning continues, but no trading
        }

        // CRITICAL: Block trading after EOD liquidation (3:58 PM - 4:00 PM)
        if (et_time_.is_eod_liquidation_window()) {
            log_system("🔴 EOD window active - learning only, no new trades");
            return;  // Learning continues, but no new positions
        }

        log_system("🕐 Regular Trading Hours - processing for signals and trades");

        // =====================================================================
        // STEP 7: Generate Signal and Trade (RTH only)
        // =====================================================================
        log_system("🧠 Generating signal from strategy...");
        auto signal = generate_signal(bar);

        // Log signal with detailed info
        log_system("📈 SIGNAL GENERATED:");
        log_system("  Prediction: " + signal.prediction);
        log_system("  Probability: " + std::to_string(signal.probability));
        log_system("  Confidence: " + std::to_string(signal.confidence));
        log_system("  Strategy Ready: " + std::string(strategy_.is_ready() ? "YES" : "NO"));

        log_signal(bar, signal);

        // Make trading decision
        log_system("🎯 Evaluating trading decision...");
        auto decision = make_decision(signal, bar);

        // Enhanced decision logging with detailed explanation
        log_enhanced_decision(signal, decision);
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
        } else {
            log_system("⏸️  NO TRADE: " + decision.reason);
        }

        // Log current portfolio state
        log_portfolio_state();
    }

    struct Signal {
        double probability;
        double confidence;
        std::string prediction;  // "LONG", "SHORT", "NEUTRAL"
        double prob_1bar;
        double prob_5bar;
        double prob_10bar;
    };

    Signal generate_signal(const Bar& bar) {
        // Call OnlineEnsemble strategy to generate real signal
        auto strategy_signal = strategy_.generate_signal(bar);

        // DEBUG: Check why we're getting 0.5
        if (strategy_signal.probability == 0.5) {
            std::string reason = "unknown";
            if (strategy_signal.metadata.count("skip_reason")) {
                reason = strategy_signal.metadata.at("skip_reason");
            }
            std::cout << "  [DBG: p=0.5 reason=" << reason << "]" << std::endl;
        }

        Signal signal;
        signal.probability = strategy_signal.probability;
        signal.confidence = strategy_signal.confidence;  // Use confidence from strategy

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Use same probability for all horizons (OnlineEnsemble provides single probability)
        signal.prob_1bar = strategy_signal.probability;
        signal.prob_5bar = strategy_signal.probability;
        signal.prob_10bar = strategy_signal.probability;

        return signal;
    }

    struct Decision {
        bool should_trade;
        PositionStateMachine::State target_state;
        std::string reason;
        double current_equity;
        double position_pnl_pct;
        bool profit_target_hit;
        bool stop_loss_hit;
        bool min_hold_violated;
    };

    Decision make_decision(const Signal& signal, const Bar& bar) {
        Decision decision;
        decision.should_trade = false;

        // Get current portfolio state
        auto account = broker_->get_account();
        if (!account) {
            decision.reason = "Failed to get account info";
            return decision;
        }

        decision.current_equity = account->portfolio_value;
        decision.position_pnl_pct = (decision.current_equity - entry_equity_) / entry_equity_;

        // Check profit target / stop loss
        decision.profit_target_hit = (decision.position_pnl_pct >= PROFIT_TARGET &&
                                      current_state_ != PositionStateMachine::State::CASH_ONLY);
        decision.stop_loss_hit = (decision.position_pnl_pct <= STOP_LOSS &&
                                  current_state_ != PositionStateMachine::State::CASH_ONLY);

        // Check minimum hold period
        decision.min_hold_violated = (bars_held_ < MIN_HOLD_BARS);

        // Force exit to cash if profit/stop hit
        if (decision.profit_target_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "PROFIT_TARGET (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        if (decision.stop_loss_hit) {
            decision.should_trade = true;
            decision.target_state = PositionStateMachine::State::CASH_ONLY;
            decision.reason = "STOP_LOSS (" + std::to_string(decision.position_pnl_pct * 100) + "%)";
            return decision;
        }

        // Map signal probability to PSM state (v1.0 asymmetric thresholds)
        PositionStateMachine::State target_state;

        if (signal.probability >= 0.68) {
            target_state = PositionStateMachine::State::TQQQ_ONLY;  // Maps to SPXL
        } else if (signal.probability >= 0.60) {
            target_state = PositionStateMachine::State::QQQ_TQQQ;   // Mixed
        } else if (signal.probability >= 0.55) {
            target_state = PositionStateMachine::State::QQQ_ONLY;   // Maps to SPY
        } else if (signal.probability >= 0.49) {
            target_state = PositionStateMachine::State::CASH_ONLY;
        } else if (signal.probability >= 0.45) {
            target_state = PositionStateMachine::State::PSQ_ONLY;   // Maps to SH
        } else if (signal.probability >= 0.35) {
            target_state = PositionStateMachine::State::PSQ_SQQQ;   // Mixed
        } else if (signal.probability < 0.32) {
            target_state = PositionStateMachine::State::SQQQ_ONLY;  // Maps to SDS
        } else {
            target_state = PositionStateMachine::State::CASH_ONLY;
        }

        decision.target_state = target_state;

        // Check if state transition needed
        if (target_state != current_state_) {
            // Check minimum hold period
            if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
                decision.should_trade = false;
                decision.reason = "MIN_HOLD_PERIOD (held " + std::to_string(bars_held_) + " bars)";
            } else {
                decision.should_trade = true;
                decision.reason = "STATE_TRANSITION (prob=" + std::to_string(signal.probability) + ")";
            }
        } else {
            decision.should_trade = false;
            decision.reason = "NO_CHANGE";
        }

        return decision;
    }

    void liquidate_all_positions() {
        log_system("Closing all positions for end of day...");

        if (broker_->close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = broker_->get_account();
            if (account) {
                log_system("Final portfolio value: $" + std::to_string(account->portfolio_value));
                entry_equity_ = account->portfolio_value;
            }
        } else {
            log_error("Failed to close all positions");
        }

        log_portfolio_state();
    }

    void execute_transition(const Decision& decision) {
        log_system("");
        log_system("🚀 *** EXECUTING TRADE ***");
        log_system("  Current State: " + psm_.state_to_string(current_state_));
        log_system("  Target State: " + psm_.state_to_string(decision.target_state));
        log_system("  Reason: " + decision.reason);
        log_system("");

        // Step 1: Close all current positions
        log_system("📤 Step 1: Closing current positions...");

        // Get current positions before closing (for logging)
        auto positions_to_close = broker_->get_positions();

        if (!broker_->close_all_positions()) {
            log_error("❌ Failed to close positions - aborting transition");
            return;
        }

        // Get account info before closing for accurate P&L calculation
        auto account_before = broker_->get_account();
        double portfolio_before = account_before ? account_before->portfolio_value : previous_portfolio_value_;

        // Log the close orders
        if (!positions_to_close.empty()) {
            for (const auto& pos : positions_to_close) {
                if (std::abs(pos.quantity) >= 0.001) {
                    // Create a synthetic Order object for logging
                    Order close_order;
                    close_order.symbol = pos.symbol;
                    close_order.quantity = -pos.quantity;  // Negative to close
                    close_order.side = (pos.quantity > 0) ? "sell" : "buy";
                    close_order.type = "market";
                    close_order.time_in_force = "gtc";
                    close_order.order_id = "CLOSE-" + pos.symbol;
                    close_order.status = "filled";
                    close_order.filled_qty = std::abs(pos.quantity);
                    close_order.filled_avg_price = pos.current_price;

                    // Calculate realized P&L for this close
                    double trade_pnl = (pos.quantity > 0) ?
                        pos.quantity * (pos.current_price - pos.avg_entry_price) :  // Long close
                        pos.quantity * (pos.avg_entry_price - pos.current_price);   // Short close

                    // Get updated account info
                    auto account_after = broker_->get_account();
                    double cash = account_after ? account_after->cash : 0.0;
                    double portfolio = account_after ? account_after->portfolio_value : portfolio_before;

                    log_trade(close_order, bar_count_, cash, portfolio, trade_pnl, "Close position");
                    log_system("  🔴 CLOSE " + std::to_string(std::abs(pos.quantity)) + " " + pos.symbol +
                              " (P&L: $" + std::to_string(trade_pnl) + ")");

                    previous_portfolio_value_ = portfolio;
                }
            }
        }

        log_system("✓ All positions closed");

        // Wait a moment for orders to settle (only in live mode)
        // In mock mode, skip sleep to avoid deadlock with replay thread
        if (!is_mock_mode_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        // Step 2: Get current account info
        log_system("💰 Step 2: Fetching account balance from Alpaca...");
        auto account = broker_->get_account();
        if (!account) {
            log_error("❌ Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        double portfolio_value = account->portfolio_value;
        log_system("✓ Account Status:");
        log_system("  Cash: $" + std::to_string(available_capital));
        log_system("  Portfolio Value: $" + std::to_string(portfolio_value));
        log_system("  Buying Power: $" + std::to_string(account->buying_power));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // CRITICAL: If target is not CASH_ONLY but we got empty positions, something is wrong
        bool position_entry_failed = false;
        if (target_positions.empty() && decision.target_state != PositionStateMachine::State::CASH_ONLY) {
            log_error("❌ CRITICAL: Target state is " + psm_.state_to_string(decision.target_state) +
                     " but failed to calculate positions (likely price fetch failure)");
            log_error("   Staying in CASH_ONLY for safety");
            position_entry_failed = true;
        }

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("");
            log_system("📥 Step 3: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  🔵 Sending BUY order to Alpaca:");
                    log_system("     Symbol: " + symbol);
                    log_system("     Quantity: " + std::to_string(quantity) + " shares");

                    auto order = broker_->place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order Confirmed:");
                        log_system("     Order ID: " + order->order_id);
                        log_system("     Status: " + order->status);

                        // Get updated account info for accurate logging
                        auto account_after = broker_->get_account();
                        double cash = account_after ? account_after->cash : 0.0;
                        double portfolio = account_after ? account_after->portfolio_value : previous_portfolio_value_;
                        double trade_pnl = portfolio - previous_portfolio_value_;  // Portfolio change from this trade

                        // Build reason string from decision
                        std::string reason = "Enter " + psm_.state_to_string(decision.target_state);
                        if (decision.profit_target_hit) reason += " (profit target)";
                        else if (decision.stop_loss_hit) reason += " (stop loss)";

                        log_trade(*order, bar_count_, cash, portfolio, trade_pnl, reason);
                        previous_portfolio_value_ = portfolio;
                    } else {
                        log_error("  ❌ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("💵 Target state is CASH_ONLY - no positions to open");
        }

        // Update state - CRITICAL FIX: Only update to target state if we successfully entered positions
        // or if target was CASH_ONLY
        if (position_entry_failed) {
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            log_system("⚠️  State forced to CASH_ONLY due to position entry failure");
        } else {
            current_state_ = decision.target_state;
        }
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        // Final account status
        log_system("");
        log_system("✓ Transition Complete!");
        log_system("  New State: " + psm_.state_to_string(current_state_));
        log_system("  Entry Equity: $" + std::to_string(entry_equity_));
        log_system("");

        // Persist state immediately after transition
        persist_current_state();
    }

    // Calculate position allocations based on PSM state
    std::map<std::string, double> calculate_target_allocations(
        PositionStateMachine::State state, double capital) {

        std::map<std::string, double> allocations;

        // Map PSM states to SPY instrument allocations
        switch (state) {
            case PositionStateMachine::State::TQQQ_ONLY:
                // 3x bull → SPXL only
                allocations["SPXL"] = capital;
                break;

            case PositionStateMachine::State::QQQ_TQQQ:
                // Blended long → SPY (50%) + SPXL (50%)
                allocations["SPY"] = capital * 0.5;
                allocations["SPXL"] = capital * 0.5;
                break;

            case PositionStateMachine::State::QQQ_ONLY:
                // 1x base → SPY only
                allocations["SPY"] = capital;
                break;

            case PositionStateMachine::State::CASH_ONLY:
                // No positions
                break;

            case PositionStateMachine::State::PSQ_ONLY:
                // -1x bear → SH only
                allocations["SH"] = capital;
                break;

            case PositionStateMachine::State::PSQ_SQQQ:
                // Blended short → SH (50%) + SDS (50%)
                allocations["SH"] = capital * 0.5;
                allocations["SDS"] = capital * 0.5;
                break;

            case PositionStateMachine::State::SQQQ_ONLY:
                // -2x bear → SDS only
                allocations["SDS"] = capital;
                break;

            default:
                break;
        }

        // Convert dollar allocations to share quantities
        std::map<std::string, double> quantities;
        for (const auto& [symbol, dollar_amount] : allocations) {
            double price = 0.0;

            // In mock mode, use leveraged_prices_ for SH, SDS, SPXL
            if (is_mock_mode_ && (symbol == "SH" || symbol == "SDS" || symbol == "SPXL")) {
                // Get current bar timestamp
                auto spy_bars = bar_feed_->get_recent_bars("SPY", 1);
                if (spy_bars.empty()) {
                    throw std::runtime_error("CRITICAL: No SPY bars available for timestamp lookup");
                }

                uint64_t bar_ts_sec = spy_bars[0].timestamp_ms / 1000;

                // Crash fast if no price data (no silent failures!)
                if (!leveraged_prices_.count(bar_ts_sec)) {
                    throw std::runtime_error(
                        "CRITICAL: No leveraged ETF price data for timestamp " +
                        std::to_string(bar_ts_sec) + " when calculating " + symbol + " position");
                }

                if (!leveraged_prices_[bar_ts_sec].count(symbol)) {
                    throw std::runtime_error(
                        "CRITICAL: No price for " + symbol + " at timestamp " +
                        std::to_string(bar_ts_sec));
                }

                price = leveraged_prices_[bar_ts_sec].at(symbol);
            } else {
                // Get price from bar feed (SPY or live mode)
                auto bars = bar_feed_->get_recent_bars(symbol, 1);
                if (bars.empty() || bars[0].close <= 0) {
                    throw std::runtime_error(
                        "CRITICAL: No valid price for " + symbol + " from bar feed");
                }
                price = bars[0].close;
            }

            // Calculate shares
            if (price <= 0) {
                throw std::runtime_error(
                    "CRITICAL: Invalid price " + std::to_string(price) + " for " + symbol);
            }

            double shares = std::floor(dollar_amount / price);
            if (shares > 0) {
                quantities[symbol] = shares;
            }
        }

        return quantities;
    }

    void log_trade(const Order& order, uint64_t bar_index = 0, double cash_balance = 0.0,
                   double portfolio_value = 0.0, double trade_pnl = 0.0, const std::string& reason = "") {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_index"] = bar_index;
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;
        j["cash_balance"] = cash_balance;
        j["portfolio_value"] = portfolio_value;
        j["trade_pnl"] = trade_pnl;
        if (!reason.empty()) {
            j["reason"] = reason;
        }

        log_trades_ << j.dump() << std::endl;
        log_trades_.flush();
    }

    void log_signal(const Bar& bar, const Signal& signal) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["bar_timestamp_ms"] = bar.timestamp_ms;
        j["probability"] = signal.probability;
        j["confidence"] = signal.confidence;
        j["prediction"] = signal.prediction;
        j["prob_1bar"] = signal.prob_1bar;
        j["prob_5bar"] = signal.prob_5bar;
        j["prob_10bar"] = signal.prob_10bar;

        log_signals_ << j.dump() << std::endl;
        log_signals_.flush();
    }

    void log_enhanced_decision(const Signal& signal, const Decision& decision) {
        log_system("");
        log_system("╔═══════════════════════════════════════════════════════════════");
        log_system("║ 📋 DECISION ANALYSIS");
        log_system("╠═══════════════════════════════════════════════════════════════");

        // Current state
        log_system("║ Current State: " + psm_.state_to_string(current_state_));
        log_system("║   - Bars Held: " + std::to_string(bars_held_) + " bars");
        log_system("║   - Min Hold: " + std::to_string(MIN_HOLD_BARS) + " bars required");
        log_system("║   - Position P&L: " + std::to_string(decision.position_pnl_pct * 100) + "%");
        log_system("║   - Current Equity: $" + std::to_string(decision.current_equity));
        log_system("║");

        // Signal analysis
        log_system("║ Signal Input:");
        log_system("║   - Probability: " + std::to_string(signal.probability));
        log_system("║   - Prediction: " + signal.prediction);
        log_system("║   - Confidence: " + std::to_string(signal.confidence));
        log_system("║");

        // Target state mapping
        log_system("║ PSM Threshold Mapping:");
        if (signal.probability >= 0.68) {
            log_system("║   ✓ prob >= 0.68 → BULL_3X_ONLY (SPXL)");
        } else if (signal.probability >= 0.60) {
            log_system("║   ✓ 0.60 <= prob < 0.68 → BASE_BULL_3X (SPY+SPXL)");
        } else if (signal.probability >= 0.55) {
            log_system("║   ✓ 0.55 <= prob < 0.60 → BASE_ONLY (SPY)");
        } else if (signal.probability >= 0.49) {
            log_system("║   ✓ 0.49 <= prob < 0.55 → CASH_ONLY");
        } else if (signal.probability >= 0.45) {
            log_system("║   ✓ 0.45 <= prob < 0.49 → BEAR_1X_ONLY (SH)");
        } else if (signal.probability >= 0.35) {
            log_system("║   ✓ 0.35 <= prob < 0.45 → BEAR_1X_NX (SH+SDS)");
        } else {
            log_system("║   ✓ prob < 0.35 → BEAR_NX_ONLY (SDS)");
        }
        log_system("║   → Target State: " + psm_.state_to_string(decision.target_state));
        log_system("║");

        // Decision logic
        log_system("║ Decision Logic:");
        if (decision.profit_target_hit) {
            log_system("║   🎯 PROFIT TARGET HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.stop_loss_hit) {
            log_system("║   🛑 STOP LOSS HIT (" + std::to_string(decision.position_pnl_pct * 100) + "%)");
            log_system("║   → Force exit to CASH");
        } else if (decision.target_state == current_state_) {
            log_system("║   ✓ Target matches current state");
            log_system("║   → NO CHANGE (hold position)");
        } else if (decision.min_hold_violated && current_state_ != PositionStateMachine::State::CASH_ONLY) {
            log_system("║   ⏳ MIN HOLD PERIOD VIOLATED");
            log_system("║      - Currently held: " + std::to_string(bars_held_) + " bars");
            log_system("║      - Required: " + std::to_string(MIN_HOLD_BARS) + " bars");
            log_system("║      - Remaining: " + std::to_string(MIN_HOLD_BARS - bars_held_) + " bars");
            log_system("║   → BLOCKED (must wait)");
        } else {
            log_system("║   ✓ State transition approved");
            log_system("║      - Target differs from current");
            log_system("║      - Min hold satisfied or in CASH");
            log_system("║   → EXECUTE TRANSITION");
        }
        log_system("║");

        // Final decision
        if (decision.should_trade) {
            log_system("║ ✅ FINAL DECISION: TRADE");
            log_system("║    Transition: " + psm_.state_to_string(current_state_) +
                      " → " + psm_.state_to_string(decision.target_state));
        } else {
            log_system("║ ⏸️  FINAL DECISION: NO TRADE");
        }
        log_system("║    Reason: " + decision.reason);
        log_system("╚═══════════════════════════════════════════════════════════════");
        log_system("");
    }

    void log_decision(const Decision& decision) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["should_trade"] = decision.should_trade;
        j["current_state"] = psm_.state_to_string(current_state_);
        j["target_state"] = psm_.state_to_string(decision.target_state);
        j["reason"] = decision.reason;
        j["current_equity"] = decision.current_equity;
        j["position_pnl_pct"] = decision.position_pnl_pct;
        j["bars_held"] = bars_held_;

        log_decisions_ << j.dump() << std::endl;
        log_decisions_.flush();
    }

    void log_portfolio_state() {
        auto account = broker_->get_account();
        if (!account) return;

        auto positions = broker_->get_positions();

        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["cash"] = account->cash;
        j["buying_power"] = account->buying_power;
        j["portfolio_value"] = account->portfolio_value;
        j["equity"] = account->equity;
        j["total_return"] = account->portfolio_value - 100000.0;
        j["total_return_pct"] = (account->portfolio_value - 100000.0) / 100000.0;

        nlohmann::json positions_json = nlohmann::json::array();
        for (const auto& pos : positions) {
            nlohmann::json p;
            p["symbol"] = pos.symbol;
            p["quantity"] = pos.quantity;
            p["avg_entry_price"] = pos.avg_entry_price;
            p["current_price"] = pos.current_price;
            p["market_value"] = pos.market_value;
            p["unrealized_pl"] = pos.unrealized_pl;
            p["unrealized_pl_pct"] = pos.unrealized_pl_pct;
            positions_json.push_back(p);
        }
        j["positions"] = positions_json;

        log_positions_ << j.dump() << std::endl;
        log_positions_.flush();
    }

    // NEW: Convert Alpaca positions to BrokerPosition format for reconciliation
    std::vector<BrokerPosition> get_broker_positions() {
        auto alpaca_positions = broker_->get_positions();
        std::vector<BrokerPosition> broker_positions;

        for (const auto& pos : alpaca_positions) {
            BrokerPosition bp;
            bp.symbol = pos.symbol;
            bp.qty = static_cast<int64_t>(pos.quantity);
            bp.avg_entry_price = pos.avg_entry_price;
            bp.current_price = pos.current_price;
            bp.unrealized_pnl = pos.unrealized_pl;
            broker_positions.push_back(bp);
        }

        return broker_positions;
    }

    /**
     * Save comprehensive warmup data: historical bars + all of today's bars
     * This ensures optimization uses ALL available data up to current moment
     */
    std::string save_comprehensive_warmup_to_csv() {
        auto et_tm = et_time_.get_current_et_tm();
        std::string today = format_et_date(et_tm);

        std::string filename = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/comprehensive_warmup_" +
                               today + ".csv";

        std::ofstream csv(filename);
        if (!csv.is_open()) {
            log_error("Failed to open file for writing: " + filename);
            return "";
        }

        // Write CSV header
        csv << "timestamp,open,high,low,close,volume\n";

        log_system("Building comprehensive warmup data...");

        // Step 1: Load historical warmup bars (20 blocks = 7800 bars + 64 feature bars)
        std::string warmup_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_warmup_latest.csv";
        std::ifstream warmup_csv(warmup_file);

        if (!warmup_csv.is_open()) {
            log_error("Failed to open historical warmup file: " + warmup_file);
            log_error("Falling back to today's bars only");
        } else {
            std::string line;
            std::getline(warmup_csv, line);  // Skip header

            int historical_count = 0;
            while (std::getline(warmup_csv, line)) {
                // Filter: only include bars BEFORE today (to avoid duplicates)
                if (line.find(today) == std::string::npos) {
                    csv << line << "\n";
                    historical_count++;
                }
            }
            warmup_csv.close();

            log_system("  ✓ Historical bars: " + std::to_string(historical_count));
        }

        // Step 2: Append all of today's bars collected so far
        for (const auto& bar : todays_bars_) {
            csv << bar.timestamp_ms << ","
                << bar.open << ","
                << bar.high << ","
                << bar.low << ","
                << bar.close << ","
                << bar.volume << "\n";
        }

        csv.close();

        log_system("  ✓ Today's bars: " + std::to_string(todays_bars_.size()));
        log_system("✓ Comprehensive warmup saved: " + filename);

        return filename;
    }

    /**
     * Load optimized parameters from midday_selected_params.json
     */
    struct OptimizedParams {
        bool success{false};
        std::string source;
        // Phase 1 parameters
        double buy_threshold{0.55};
        double sell_threshold{0.45};
        double bb_amplification_factor{0.10};
        double ewrls_lambda{0.995};
        // Phase 2 parameters
        double h1_weight{0.3};
        double h5_weight{0.5};
        double h10_weight{0.2};
        int bb_period{20};
        double bb_std_dev{2.0};
        double bb_proximity{0.30};
        double regularization{0.01};
        double expected_mrb{0.0};
    };

    OptimizedParams load_optimized_parameters() {
        OptimizedParams params;

        std::string json_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/midday_selected_params.json";
        std::ifstream file(json_file);

        if (!file.is_open()) {
            log_error("Failed to open optimization results: " + json_file);
            return params;
        }

        try {
            nlohmann::json j;
            file >> j;
            file.close();

            params.success = true;
            params.source = j.value("source", "baseline");
            // Phase 1 parameters
            params.buy_threshold = j.value("buy_threshold", 0.55);
            params.sell_threshold = j.value("sell_threshold", 0.45);
            params.bb_amplification_factor = j.value("bb_amplification_factor", 0.10);
            params.ewrls_lambda = j.value("ewrls_lambda", 0.995);
            // Phase 2 parameters
            params.h1_weight = j.value("h1_weight", 0.3);
            params.h5_weight = j.value("h5_weight", 0.5);
            params.h10_weight = j.value("h10_weight", 0.2);
            params.bb_period = j.value("bb_period", 20);
            params.bb_std_dev = j.value("bb_std_dev", 2.0);
            params.bb_proximity = j.value("bb_proximity", 0.30);
            params.regularization = j.value("regularization", 0.01);
            params.expected_mrb = j.value("expected_mrb", 0.0);

            log_system("✓ Loaded optimized parameters from: " + json_file);
            log_system("  Source: " + params.source);
            log_system("  Phase 1 Parameters:");
            log_system("    buy_threshold: " + std::to_string(params.buy_threshold));
            log_system("    sell_threshold: " + std::to_string(params.sell_threshold));
            log_system("    bb_amplification_factor: " + std::to_string(params.bb_amplification_factor));
            log_system("    ewrls_lambda: " + std::to_string(params.ewrls_lambda));
            log_system("  Phase 2 Parameters:");
            log_system("    h1_weight: " + std::to_string(params.h1_weight));
            log_system("    h5_weight: " + std::to_string(params.h5_weight));
            log_system("    h10_weight: " + std::to_string(params.h10_weight));
            log_system("    bb_period: " + std::to_string(params.bb_period));
            log_system("    bb_std_dev: " + std::to_string(params.bb_std_dev));
            log_system("    bb_proximity: " + std::to_string(params.bb_proximity));
            log_system("    regularization: " + std::to_string(params.regularization));
            log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");

        } catch (const std::exception& e) {
            log_error("Failed to parse optimization results: " + std::string(e.what()));
            params.success = false;
        }

        return params;
    }

    /**
     * Update strategy configuration with new parameters
     */
    void update_strategy_parameters(const OptimizedParams& params) {
        log_system("📊 Updating strategy parameters...");

        // Create new config with optimized parameters
        auto config = create_v1_config();
        // Phase 1 parameters
        config.buy_threshold = params.buy_threshold;
        config.sell_threshold = params.sell_threshold;
        config.bb_amplification_factor = params.bb_amplification_factor;
        config.ewrls_lambda = params.ewrls_lambda;
        // Phase 2 parameters
        config.horizon_weights = {params.h1_weight, params.h5_weight, params.h10_weight};
        config.bb_period = params.bb_period;
        config.bb_std_dev = params.bb_std_dev;
        config.bb_proximity_threshold = params.bb_proximity;
        config.regularization = params.regularization;

        // Update strategy
        strategy_.update_config(config);

        log_system("✓ Strategy parameters updated with phase 1 + phase 2 optimizations");
    }

    /**
     * Run mid-day optimization at 15:15 PM ET (3:15pm)
     */
    void run_midday_optimization() {
        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("🔄 MID-DAY OPTIMIZATION TRIGGERED (15:15 PM ET / 3:15pm)");
        log_system("════════════════════════════════════════════════");
        log_system("");

        // Step 1: Save comprehensive warmup data (historical + today's bars)
        log_system("Step 1: Saving comprehensive warmup data to CSV...");
        std::string warmup_data_file = save_comprehensive_warmup_to_csv();
        if (warmup_data_file.empty()) {
            log_error("Failed to save warmup data - continuing with baseline parameters");
            return;
        }

        // Step 2: Call optimization script
        log_system("Step 2: Running Optuna optimization script...");
        log_system("  (This will take ~5 minutes for 50 trials)");

        std::string cmd = "/Volumes/ExternalSSD/Dev/C++/online_trader/tools/midday_optuna_relaunch.sh \"" +
                          warmup_data_file + "\" 2>&1 | tail -30";

        int exit_code = system(cmd.c_str());

        if (exit_code != 0) {
            log_error("Optimization script failed (exit code: " + std::to_string(exit_code) + ")");
            log_error("Continuing with baseline parameters");
            return;
        }

        log_system("✓ Optimization script completed");

        // Step 3: Load optimized parameters
        log_system("Step 3: Loading optimized parameters...");
        auto params = load_optimized_parameters();

        if (!params.success) {
            log_error("Failed to load optimized parameters - continuing with baseline");
            return;
        }

        // Step 4: Update strategy configuration
        log_system("Step 4: Updating strategy configuration...");
        update_strategy_parameters(params);

        log_system("");
        log_system("════════════════════════════════════════════════");
        log_system("✅ MID-DAY OPTIMIZATION COMPLETE");
        log_system("════════════════════════════════════════════════");
        log_system("  Parameters: " + params.source);
        log_system("  Expected MRB: " + std::to_string(params.expected_mrb) + "%");
        log_system("  Resuming trading at 14:46 PM ET");
        log_system("════════════════════════════════════════════════");
        log_system("");
    }
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Parse command-line flags
    bool is_mock = has_flag(args, "--mock");
    std::string mock_data_file = get_arg(args, "--mock-data", "");
    double mock_speed = std::stod(get_arg(args, "--mock-speed", "39.0"));

    // Log directory
    std::string log_dir = is_mock ? "logs/mock_trading" : "logs/live_trading";

    // Create broker and bar feed based on mode
    std::unique_ptr<IBrokerClient> broker;
    std::unique_ptr<IBarFeed> bar_feed;

    if (is_mock) {
        // ================================================================
        // MOCK MODE - Replay historical data
        // ================================================================
        if (mock_data_file.empty()) {
            std::cerr << "ERROR: --mock-data <file> is required in mock mode\n";
            std::cerr << "Example: sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n";
            return 1;
        }

        std::cout << "🎭 MOCK MODE ENABLED\n";
        std::cout << "  Data file: " << mock_data_file << "\n";
        std::cout << "  Speed: " << mock_speed << "x real-time\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create mock broker
        auto mock_broker = std::make_unique<MockBroker>(
            100000.0,  // initial_capital
            0.0        // commission_per_share (zero for testing)
        );
        mock_broker->set_fill_behavior(FillBehavior::IMMEDIATE_FULL);
        broker = std::move(mock_broker);

        // Create mock bar feed
        bar_feed = std::make_unique<MockBarFeedReplay>(
            mock_data_file,
            mock_speed
        );

    } else {
        // ================================================================
        // LIVE MODE - Real trading with Alpaca + Polygon
        // ================================================================

        // Read Alpaca credentials from environment
        const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
        const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");

        if (!alpaca_key_env || !alpaca_secret_env) {
            std::cerr << "ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set\n";
            std::cerr << "Run: source config.env\n";
            return 1;
        }

        const std::string ALPACA_KEY = alpaca_key_env;
        const std::string ALPACA_SECRET = alpaca_secret_env;

        // Polygon API key
        const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
        const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";
        const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

        std::cout << "📈 LIVE MODE ENABLED\n";
        std::cout << "  Account: " << ALPACA_KEY.substr(0, 8) << "...\n";
        std::cout << "  Data source: Alpaca WebSocket (via Python bridge)\n";
        std::cout << "  Logs: " << log_dir << "/\n";
        std::cout << "\n";

        // Create live broker adapter
        broker = std::make_unique<AlpacaClientAdapter>(ALPACA_KEY, ALPACA_SECRET, true /* paper */);

        // Create live bar feed adapter (WebSocket via FIFO)
        bar_feed = std::make_unique<PolygonClientAdapter>(ALPACA_MARKET_DATA_URL, POLYGON_KEY);
    }

    // Create and run trader (same code path for both modes!)
    LiveTrader trader(std::move(broker), std::move(bar_feed), log_dir, is_mock, mock_data_file);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade [options]\n\n";
    std::cout << "Run OnlineTrader v1.0 in live or mock mode\n\n";
    std::cout << "Options:\n";
    std::cout << "  --mock              Enable mock trading mode (replay historical data)\n";
    std::cout << "  --mock-data <file>  CSV file to replay (required with --mock)\n";
    std::cout << "  --mock-speed <x>    Replay speed multiplier (default: 39.0)\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Warmup: 7,864 bars (20 blocks + 64 feature bars)\n\n";
    std::cout << "Logs:\n";
    std::cout << "  Live:  logs/live_trading/\n";
    std::cout << "  Mock:  logs/mock_trading/\n";
    std::cout << "  Files: system_*.log, signals_*.jsonl, trades_*.jsonl, decisions_*.jsonl\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Live trading\n";
    std::cout << "  sentio_cli live-trade\n\n";
    std::cout << "  # Mock trading (replay yesterday)\n";
    std::cout << "  tail -391 data/equities/SPY_RTH_NH.csv > /tmp/SPY_yesterday.csv\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data /tmp/SPY_yesterday.csv\n\n";
    std::cout << "  # Mock trading at different speed\n";
    std::cout << "  sentio_cli live-trade --mock --mock-data yesterday.csv --mock-speed 100.0\n";
}

} // namespace cli
} // namespace sentio
