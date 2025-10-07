#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/position_state_machine.h"
#include "common/time_utils.h"
#include "common/bar_validator.h"
#include "common/exceptions.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <optional>

namespace sentio {
namespace cli {

/**
 * Create OnlineEnsemble v1.0 configuration with asymmetric thresholds
 * Target: 0.6086% MRB (10.5% monthly, 125% annual)
 */
static OnlineEnsembleStrategy::OnlineEnsembleConfig create_v1_config() {
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;

    // v1.0 asymmetric thresholds (from optimization)
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;

    // EWRLS parameters
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 960;  // 2 days of 1-min bars

    // Enable BB amplification
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;

    // Adaptive learning
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    return config;
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
    LiveTrader(const std::string& alpaca_key,
               const std::string& alpaca_secret,
               const std::string& polygon_url,
               const std::string& polygon_key,
               const std::string& log_dir)
        : alpaca_(alpaca_key, alpaca_secret, true /* paper */)
        , polygon_(polygon_url, polygon_key)
        , log_dir_(log_dir)
        , strategy_(create_v1_config())
        , psm_()
        , current_state_(PositionStateMachine::State::CASH_ONLY)
        , bars_held_(0)
        , entry_equity_(100000.0)
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

    void run() {
        log_system("=== OnlineTrader v1.0 Live Paper Trading Started ===");
        log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
        log_system("Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)");
        log_system("Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds");
        log_system("");

        // Connect to Alpaca
        log_system("Connecting to Alpaca Paper Trading...");
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to connect to Alpaca");
            return;
        }
        log_system("✓ Connected - Account: " + account->account_number);
        log_system("  Starting Capital: $" + std::to_string(account->portfolio_value));
        entry_equity_ = account->portfolio_value;

        // Connect to Polygon
        log_system("Connecting to Polygon proxy...");
        if (!polygon_.connect()) {
            log_error("Failed to connect to Polygon");
            return;
        }
        log_system("✓ Connected to Polygon");

        // Subscribe to symbols (SPY instruments)
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!polygon_.subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Reconcile existing positions on startup (seamless continuation)
        reconcile_startup_positions();

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Start main trading loop
        polygon_.start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
                on_new_bar(bar);
            }
        });

        log_system("=== Live trading active - Press Ctrl+C to stop ===");
        log_system("");

        // Keep running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

private:
    AlpacaClient alpaca_;
    PolygonClient polygon_;
    std::string log_dir_;
    OnlineEnsembleStrategy strategy_;
    PositionStateMachine psm_;
    std::map<std::string, std::string> symbol_map_;

    // NEW: Production safety infrastructure
    PositionBook position_book_;
    TradingSession session_{"America/New_York"};
    std::optional<Bar> previous_bar_;  // For bar-to-bar learning
    uint64_t bar_count_{0};

    // State tracking
    PositionStateMachine::State current_state_;
    int bars_held_;
    double entry_equity_;

    // Log file streams
    std::ofstream log_system_;
    std::ofstream log_signals_;
    std::ofstream log_trades_;
    std::ofstream log_positions_;
    std::ofstream log_decisions_;

    // Risk management (v1.0 parameters)
    const double PROFIT_TARGET = 0.02;   // 2%
    const double STOP_LOSS = -0.015;     // -1.5%
    const int MIN_HOLD_BARS = 3;
    const int MAX_HOLD_BARS = 100;

    void init_logs() {
        // Create log directory if needed
        system(("mkdir -p " + log_dir_).c_str());

        auto timestamp = get_timestamp();

        log_system_.open(log_dir_ + "/system_" + timestamp + ".log");
        log_signals_.open(log_dir_ + "/signals_" + timestamp + ".jsonl");
        log_trades_.open(log_dir_ + "/trades_" + timestamp + ".jsonl");
        log_positions_.open(log_dir_ + "/positions_" + timestamp + ".jsonl");
        log_decisions_.open(log_dir_ + "/decisions_" + timestamp + ".jsonl");
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string get_timestamp_readable() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    bool is_regular_hours() const {
        // NEW: Use DST-aware TradingSession
        auto now = std::chrono::system_clock::now();
        return session_.is_trading_day(now) && session_.is_regular_hours(now);
    }

    bool is_end_of_day_liquidation_time() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        // Use GMT time and convert to ET (EDT = GMT-4)
        auto gmt_tm = *std::gmtime(&time_t_now);
        int gmt_hour = gmt_tm.tm_hour;
        int gmt_minute = gmt_tm.tm_min;

        // Convert GMT to ET (EDT = GMT-4)
        int et_hour = gmt_hour - 4;
        if (et_hour < 0) et_hour += 24;

        int time_minutes = et_hour * 60 + gmt_minute;

        // Liquidate at 3:58pm ET (2 minutes before close)
        int liquidation_time = 15 * 60 + 58;  // 3:58pm = 958 minutes

        return time_minutes == liquidation_time;
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

    void reconcile_startup_positions() {
        // Get current broker positions and cash
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to get account info for startup reconciliation");
            return;
        }

        auto broker_positions = get_broker_positions();

        log_system("=== Startup Position Reconciliation ===");
        log_system("  Cash: $" + std::to_string(account->cash));
        log_system("  Portfolio Value: $" + std::to_string(account->portfolio_value));

        if (broker_positions.empty()) {
            log_system("  Current Positions: NONE (starting flat)");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            log_system("  Initial State: CASH_ONLY");
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
            log_system("  Inferred PSM State: " + psm_.state_to_string(current_state_));
        }

        log_system("✓ Startup reconciliation complete - resuming trading seamlessly");
        log_system("");
    }

    PositionStateMachine::State infer_state_from_positions(
        const std::vector<BrokerPosition>& positions) {

        // Map SPY instruments to equivalent QQQ PSM states
        // SPY/SPXL/SH/SDS → QQQ/TQQQ/PSQ/SQQQ
        for (const auto& pos : positions) {
            if (pos.qty > 0) {
                if (pos.symbol == "SPXL") return PositionStateMachine::State::TQQQ_ONLY;  // 3x bull
                if (pos.symbol == "SPY") return PositionStateMachine::State::QQQ_ONLY;    // 1x base
                if (pos.symbol == "SH") return PositionStateMachine::State::PSQ_ONLY;     // -1x bear
                if (pos.symbol == "SDS") return PositionStateMachine::State::SQQQ_ONLY;   // -2x bear
            }
        }

        return PositionStateMachine::State::CASH_ONLY;
    }

    void warmup_strategy() {
        // Load warmup data created by warmup_live_trading.sh script
        // This file contains: 960 warmup bars + all of today's bars up to now
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
            std::string ts_utc, ts_epoch_str, open_str, high_str, low_str, close_str, volume_str;

            // CSV format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            if (std::getline(iss, ts_utc, ',') &&
                std::getline(iss, ts_epoch_str, ',') &&
                std::getline(iss, open_str, ',') &&
                std::getline(iss, high_str, ',') &&
                std::getline(iss, low_str, ',') &&
                std::getline(iss, close_str, ',') &&
                std::getline(iss, volume_str)) {

                Bar bar;
                bar.timestamp_ms = std::stoll(ts_epoch_str) * 1000LL;  // Convert sec to ms
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

        // Feed ALL bars (960 warmup + today's bars)
        // This ensures we're caught up to the current time
        log_system("Feeding all warmup bars to strategy (warmup + today's bars)...");

        int predictor_training_count = 0;
        for (size_t i = 0; i < all_bars.size(); ++i) {
            strategy_.on_bar(all_bars[i]);

            // Train predictor on bar-to-bar returns (after feature engine is ready at bar 64)
            if (i > 64 && i + 1 < all_bars.size()) {
                auto features = strategy_.extract_features(all_bars[i]);
                if (!features.empty()) {
                    double current_close = all_bars[i].close;
                    double next_close = all_bars[i + 1].close;
                    double realized_return = (next_close - current_close) / current_close;

                    strategy_.train_predictor(features, realized_return);
                    predictor_training_count++;
                }
            }

            // Update bar_count_ and previous_bar_ for seamless transition to live
            bar_count_++;
            previous_bar_ = all_bars[i];
        }

        log_system("✓ Warmup complete - processed " + std::to_string(all_bars.size()) + " bars");
        log_system("  Strategy is_ready() = " + std::string(strategy_.is_ready() ? "YES" : "NO"));
        log_system("  Predictor trained on " + std::to_string(predictor_training_count) + " historical returns");
        log_system("  Last warmup bar: " + format_bar_time(all_bars.back()));
    }

    std::string format_bar_time(const Bar& bar) const {
        time_t time_t_val = static_cast<time_t>(bar.timestamp_ms / 1000);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void on_new_bar(const Bar& bar) {
        auto timestamp = get_timestamp_readable();
        bar_count_++;

        // =====================================================================
        // STEP 1: Bar Validation (NEW - P4)
        // =====================================================================
        if (!is_valid_bar(bar)) {
            log_error("[" + timestamp + "] Invalid bar dropped: " +
                     BarValidator::get_error_message(bar));
            return;
        }

        // =====================================================================
        // STEP 2: Feed to Strategy (ALWAYS - for continuous learning)
        // =====================================================================
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
            }
        }
        previous_bar_ = bar;

        // =====================================================================
        // STEP 4: Periodic Position Reconciliation (NEW - P0-3)
        // =====================================================================
        if (bar_count_ % 60 == 0) {  // Every 60 bars (60 minutes)
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
        // STEP 5: Check End-of-Day Liquidation
        // =====================================================================
        if (is_end_of_day_liquidation_time()) {
            log_system("[" + timestamp + "] END OF DAY - Liquidating all positions");
            liquidate_all_positions();
            return;
        }

        // =====================================================================
        // STEP 6: Trading Hours Gate (NEW - only trade during RTH)
        // =====================================================================
        if (!is_regular_hours()) {
            // Log less frequently to avoid spam
            if (bar_count_ % 60 == 1) {
                log_system("[" + timestamp + "] After-hours - learning only, no trading");
            }
            return;  // Learning continues, but no trading
        }

        log_system("[" + timestamp + "] RTH bar - processing for trading...");

        // =====================================================================
        // STEP 7: Generate Signal and Trade (RTH only)
        // =====================================================================
        auto signal = generate_signal(bar);

        // Log signal to console
        std::cout << "[" << timestamp << "] Signal: "
                  << signal.prediction << " "
                  << "(p=" << std::fixed << std::setprecision(4) << signal.probability
                  << ", conf=" << std::setprecision(2) << signal.confidence << ")"
                  << " [DBG: ready=" << (strategy_.is_ready() ? "YES" : "NO") << "]"
                  << std::endl;

        log_signal(bar, signal);

        // Make trading decision
        auto decision = make_decision(signal, bar);
        log_decision(decision);

        // Execute if needed
        if (decision.should_trade) {
            execute_transition(decision);
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

        // Map signal type to prediction string
        if (strategy_signal.signal_type == SignalType::LONG) {
            signal.prediction = "LONG";
        } else if (strategy_signal.signal_type == SignalType::SHORT) {
            signal.prediction = "SHORT";
        } else {
            signal.prediction = "NEUTRAL";
        }

        // Set confidence based on distance from neutral (0.5)
        signal.confidence = std::abs(strategy_signal.probability - 0.5) * 2.0;

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
        auto account = alpaca_.get_account();
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

        if (alpaca_.close_all_positions()) {
            log_system("✓ All positions closed");
            current_state_ = PositionStateMachine::State::CASH_ONLY;
            bars_held_ = 0;

            auto account = alpaca_.get_account();
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
        log_system("Executing transition: " + psm_.state_to_string(current_state_) +
                   " → " + psm_.state_to_string(decision.target_state));
        log_system("Reason: " + decision.reason);

        // Step 1: Close all current positions
        log_system("Step 1: Closing current positions...");
        if (!alpaca_.close_all_positions()) {
            log_error("Failed to close positions - aborting transition");
            return;
        }
        log_system("✓ All positions closed");

        // Wait a moment for orders to settle
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Step 2: Get current account info
        auto account = alpaca_.get_account();
        if (!account) {
            log_error("Failed to get account info - aborting transition");
            return;
        }

        double available_capital = account->cash;
        log_system("Available capital: $" + std::to_string(available_capital));

        // Step 3: Calculate target positions based on PSM state
        auto target_positions = calculate_target_allocations(decision.target_state, available_capital);

        // Step 4: Execute buy orders for target positions
        if (!target_positions.empty()) {
            log_system("Step 2: Opening new positions...");
            for (const auto& [symbol, quantity] : target_positions) {
                if (quantity > 0) {
                    log_system("  Buying " + std::to_string(quantity) + " shares of " + symbol);

                    auto order = alpaca_.place_market_order(symbol, quantity, "gtc");
                    if (order) {
                        log_system("  ✓ Order placed: " + order->order_id + " (" + order->status + ")");
                        log_trade(*order);
                    } else {
                        log_error("  ✗ Failed to place order for " + symbol);
                    }

                    // Small delay between orders
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }
        } else {
            log_system("Target state is CASH_ONLY - no positions to open");
        }

        // Update state
        current_state_ = decision.target_state;
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        log_system("✓ Transition complete");
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
            // Get current price from recent bars
            auto bars = polygon_.get_recent_bars(symbol, 1);
            if (!bars.empty() && bars[0].close > 0) {
                double shares = std::floor(dollar_amount / bars[0].close);
                if (shares > 0) {
                    quantities[symbol] = shares;
                }
            }
        }

        return quantities;
    }

    void log_trade(const AlpacaClient::Order& order) {
        nlohmann::json j;
        j["timestamp"] = get_timestamp_readable();
        j["order_id"] = order.order_id;
        j["symbol"] = order.symbol;
        j["side"] = order.side;
        j["quantity"] = order.quantity;
        j["type"] = order.type;
        j["time_in_force"] = order.time_in_force;
        j["status"] = order.status;
        j["filled_qty"] = order.filled_qty;
        j["filled_avg_price"] = order.filled_avg_price;

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
        auto account = alpaca_.get_account();
        if (!account) return;

        auto positions = alpaca_.get_positions();

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
        auto alpaca_positions = alpaca_.get_positions();
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
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // PAPER TRADING CREDENTIALS (your new paper account)
    // Use these for paper trading validation before switching to live
    const std::string ALPACA_KEY = "PK3NCBT07OJZJULDJR5V";
    const std::string ALPACA_SECRET = "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

    // Polygon API key from config.env
    const char* polygon_key_env = std::getenv("POLYGON_API_KEY");
    // Use Alpaca's IEX WebSocket for real-time market data (FREE tier)
    // IEX provides ~3% of market volume, sufficient for paper trading
    // Upgrade to SIP ($49/mo) for 100% market coverage in production
    const std::string ALPACA_MARKET_DATA_URL = "wss://stream.data.alpaca.markets/v2/iex";

    // Polygon key (kept for future use, currently using Alpaca IEX)
    const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "";

    // NOTE: Only switch to LIVE credentials after paper trading success is confirmed!
    // LIVE credentials are in config.env (ALPACA_LIVE_API_KEY / ALPACA_LIVE_SECRET_KEY)

    // Log directory
    std::string log_dir = "logs/live_trading";

    LiveTrader trader(ALPACA_KEY, ALPACA_SECRET, ALPACA_MARKET_DATA_URL, POLYGON_KEY, log_dir);
    trader.run();

    return 0;
}

void LiveTradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli live-trade\n\n";
    std::cout << "Run OnlineTrader v1.0 with paper account\n\n";
    std::cout << "Trading Configuration:\n";
    std::cout << "  Instruments: SPY, SPXL (3x), SH (-1x), SDS (-2x)\n";
    std::cout << "  Hours: 9:30am - 3:58pm ET (regular hours only)\n";
    std::cout << "  Strategy: OnlineEnsemble v1.0 with asymmetric thresholds\n";
    std::cout << "  Data: Real-time via Alpaca (upgradeable to Polygon WebSocket)\n";
    std::cout << "  Account: Paper trading (PK3NCBT07OJZJULDJR5V)\n\n";
    std::cout << "Logs: logs/live_trading/\n";
    std::cout << "  - system_*.log: Human-readable events\n";
    std::cout << "  - signals_*.jsonl: Every prediction\n";
    std::cout << "  - decisions_*.jsonl: Trading decisions\n";
    std::cout << "  - trades_*.jsonl: Order executions\n";
    std::cout << "  - positions_*.jsonl: Portfolio snapshots\n\n";
    std::cout << "Example:\n";
    std::cout << "  sentio_cli live-trade\n";
}

} // namespace cli
} // namespace sentio
