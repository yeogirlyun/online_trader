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

        // Symbols to trade (loaded from config file - no hardcoding)
        std::vector<std::string> symbols;  // Will be loaded from rotation_strategy.json

        // Capital
        double starting_capital = 100000.0;

        // Warmup configuration
        int warmup_blocks = 20;  // For live mode
        int warmup_days = 4;      // Days of historical data before test_date for warmup
        std::string warmup_dir = "data/equities";

        // Date filtering (for single-day tests)
        std::string test_date;    // YYYY-MM-DD format (empty = run all data)

        // Test period (for multi-day batch testing)
        std::string start_date;   // YYYY-MM-DD format (empty = single day mode)
        std::string end_date;     // YYYY-MM-DD format (empty = single day mode)
        bool generate_dashboards = false;  // Generate HTML dashboards for each day

        // Output paths
        std::string log_dir = "logs/rotation_trading";
        std::string dashboard_output_dir;  // For batch test dashboards (default: log_dir + "/dashboards")

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
     * @brief Execute batch mock trading across multiple days
     *
     * Runs mock trading for each trading day in the specified range,
     * generates dashboards, and creates a summary report.
     *
     * @return 0 on success, non-zero on error
     */
    int execute_batch_mock_trading();

    /**
     * @brief Extract trading days from data within date range
     *
     * @param start_date Start date (YYYY-MM-DD)
     * @param end_date End date (YYYY-MM-DD)
     * @return Vector of trading dates
     */
    std::vector<std::string> extract_trading_days(
        const std::string& start_date,
        const std::string& end_date
    );

    /**
     * @brief Generate dashboard for a specific day's results
     *
     * @param date Trading date (YYYY-MM-DD)
     * @param output_dir Output directory for the day
     * @return 0 on success, non-zero on error
     */
    int generate_daily_dashboard(
        const std::string& date,
        const std::string& output_dir
    );

    /**
     * @brief Generate summary dashboard across all test days
     *
     * @param daily_results Results from each day
     * @param output_dir Output directory for summary
     * @return 0 on success, non-zero on error
     */
    int generate_summary_dashboard(
        const std::vector<std::map<std::string, std::string>>& daily_results,
        const std::string& output_dir
    );

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
