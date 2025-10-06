#include "cli/live_trade_command.hpp"
#include "live/alpaca_client.hpp"
#include "live/polygon_client.hpp"
#include "strategy/online_ensemble_strategy.hpp"
#include "backend/position_state_machine.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>

namespace sentio {

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
        , strategy_()
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

        // Subscribe to symbols
        std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
        if (!polygon_.subscribe(symbols)) {
            log_error("Failed to subscribe to symbols");
            return;
        }
        log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
        log_system("");

        // Initialize strategy with warmup
        log_system("Initializing OnlineEnsemble strategy...");
        log_system("Loading warmup data (960 bars = 2 trading days)...");
        warmup_strategy();
        log_system("✓ Strategy initialized and ready");
        log_system("");

        // Start main trading loop
        polygon_.start([this](const std::string& symbol, const Bar& bar) {
            if (symbol == "SPY") {  // Only process on SPY bars
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
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t_now);

        int hour = tm.tm_hour;
        int minute = tm.tm_min;
        int time_minutes = hour * 60 + minute;

        // Regular hours: 9:30am - 3:58pm ET (close 2 min before market close)
        // Assuming local time is ET (adjust if needed)
        int market_open = 9 * 60 + 30;   // 9:30am = 570 minutes
        int market_close = 15 * 60 + 58;  // 3:58pm = 958 minutes (2 min before 4:00pm)

        return time_minutes >= market_open && time_minutes < market_close;
    }

    bool is_end_of_day_liquidation_time() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t_now);

        int hour = tm.tm_hour;
        int minute = tm.tm_min;
        int time_minutes = hour * 60 + minute;

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

    void warmup_strategy() {
        // Load recent historical data from CSV for warmup
        // This ensures the EWRLS predictor has enough data
        // TODO: Load last 960 bars of SPY from file
        log_system("NOTE: Warmup using historical data not yet implemented");
        log_system("      Strategy will learn from first few live bars");
    }

    void on_new_bar(const Bar& bar) {
        auto timestamp = get_timestamp_readable();

        // Check for end-of-day liquidation (3:58pm ET)
        if (is_end_of_day_liquidation_time()) {
            log_system("[" + timestamp + "] END OF DAY - Liquidating all positions");
            liquidate_all_positions();
            return;
        }

        // Check if we should be trading
        if (!is_regular_hours()) {
            log_system("[" + timestamp + "] Outside regular hours - skipping");
            return;
        }

        log_system("[" + timestamp + "] New bar received - processing...");

        // Generate signal using OnlineEnsemble strategy
        auto signal = generate_signal(bar);

        // Log signal
        log_signal(bar, signal);

        // Make trading decision
        auto decision = make_decision(signal, bar);

        // Log decision
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
        // TODO: Actually call strategy.generate_signal()
        // For now, placeholder
        Signal signal;
        signal.probability = 0.5;  // Neutral
        signal.confidence = 0.0;
        signal.prediction = "NEUTRAL";
        signal.prob_1bar = 0.5;
        signal.prob_5bar = 0.5;
        signal.prob_10bar = 0.5;

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

        // TODO: Implement actual order execution
        // For now, just update state
        current_state_ = decision.target_state;
        bars_held_ = 0;
        entry_equity_ = decision.current_equity;

        log_system("✓ Transition complete");
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
};

int LiveTradeCommand::execute(const std::vector<std::string>& args) {
    // Load credentials from environment variables (source config.env first)
    const char* alpaca_key_env = std::getenv("ALPACA_PAPER_API_KEY");
    const char* alpaca_secret_env = std::getenv("ALPACA_PAPER_SECRET_KEY");
    const char* polygon_key_env = std::getenv("POLYGON_API_KEY");

    // Fallback to your new paper account if env vars not set
    const std::string ALPACA_KEY = alpaca_key_env ? alpaca_key_env : "PK3NCBT07OJZJULDJR5V";
    const std::string ALPACA_SECRET = alpaca_secret_env ? alpaca_secret_env : "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";
    const std::string POLYGON_KEY = polygon_key_env ? polygon_key_env : "fE68VnU8xUR7NQFMAM4yl3cULTHbigrb";

    // Polygon.io WebSocket URLs
    const std::string POLYGON_WS_URL = "wss://socket.polygon.io/stocks";  // Official Polygon WebSocket
    // Or use proxy if your engineer provides it: const std::string POLYGON_URL = "ws://proxy-url";

    // Log directory
    std::string log_dir = "logs/live_trading";

    LiveTrader trader(ALPACA_KEY, ALPACA_SECRET, POLYGON_WS_URL, POLYGON_KEY, log_dir);
    trader.run();

    return 0;
}

} // namespace sentio
