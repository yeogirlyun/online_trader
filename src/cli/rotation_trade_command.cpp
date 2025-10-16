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
#include <filesystem>
#include <set>
#include <cstdlib>

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
        } else if (arg == "--warmup-days" && i + 1 < args.size()) {
            options_.warmup_days = std::stoi(args[++i]);
        } else if (arg == "--date" && i + 1 < args.size()) {
            options_.test_date = args[++i];
        } else if (arg == "--start-date" && i + 1 < args.size()) {
            options_.start_date = args[++i];
        } else if (arg == "--end-date" && i + 1 < args.size()) {
            options_.end_date = args[++i];
        } else if (arg == "--generate-dashboards") {
            options_.generate_dashboards = true;
        } else if (arg == "--dashboard-dir" && i + 1 < args.size()) {
            options_.dashboard_output_dir = args[++i];
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
    std::cout << "  --date <YYYY-MM-DD>   Test specific date only (mock mode)\n";
    std::cout << "                        Warmup data loaded from prior days\n";
    std::cout << "  --start-date <date>   Start date for batch testing (YYYY-MM-DD)\n";
    std::cout << "  --end-date <date>     End date for batch testing (YYYY-MM-DD)\n";
    std::cout << "                        When specified, runs mock tests for each trading day\n";
    std::cout << "  --generate-dashboards Generate HTML dashboards for batch tests\n";
    std::cout << "  --dashboard-dir <dir> Dashboard output directory (default: <log-dir>/dashboards)\n";
    std::cout << "  --warmup-days <N>     Days of historical data for warmup (default: 4)\n";
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
        // Check if batch mode (date range specified)
        if (!options_.start_date.empty() && !options_.end_date.empty()) {
            return execute_batch_mock_trading();
        } else {
            return execute_mock_trading();
        }
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
                config.aggregator_config.min_probability = agg.value("min_probability", 0.51);
                config.aggregator_config.min_confidence = agg.value("min_confidence", 0.55);
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
                config.rotation_config.eod_liquidation = rot.value("eod_liquidation", true);
                config.rotation_config.eod_exit_time_minutes = rot.value("eod_exit_time_minutes", 388);
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

        if (options_.is_mock_mode && !options_.test_date.empty()) {
            // Date-specific test: Load warmup_days before test_date
            // Parse test_date (YYYY-MM-DD)
            int test_year, test_month, test_day;
            if (std::sscanf(options_.test_date.c_str(), "%d-%d-%d", &test_year, &test_month, &test_day) == 3) {
                // Calculate warmup start date (test_date - warmup_days)
                std::tm test_tm = {};
                test_tm.tm_year = test_year - 1900;
                test_tm.tm_mon = test_month - 1;
                test_tm.tm_mday = test_day - options_.warmup_days;  // Go back warmup_days
                std::mktime(&test_tm);  // Normalize the date

                std::tm end_tm = {};
                end_tm.tm_year = test_year - 1900;
                end_tm.tm_mon = test_month - 1;
                end_tm.tm_mday = test_day - 1;  // Day before test_date
                end_tm.tm_hour = 23;
                end_tm.tm_min = 59;
                std::mktime(&end_tm);

                // Filter bars between warmup_start and test_date-1
                std::vector<Bar> warmup_bars;
                for (const auto& bar : bars) {
                    std::time_t bar_time = bar.timestamp_ms / 1000;
                    std::tm* bar_tm = std::localtime(&bar_time);

                    if (bar_tm &&
                        std::mktime(bar_tm) >= std::mktime(&test_tm) &&
                        std::mktime(bar_tm) <= std::mktime(&end_tm)) {
                        warmup_bars.push_back(bar);
                    }
                }

                warmup_data[symbol] = warmup_bars;
                log_system("  " + symbol + ": " + std::to_string(warmup_bars.size()) +
                          " bars (" + std::to_string(options_.warmup_days) + " days warmup before " +
                          options_.test_date + ")");
            } else {
                log_system("⚠️  Invalid date format: " + options_.test_date);
                warmup_data[symbol] = bars;
            }
        } else if (options_.is_mock_mode) {
            // For mock mode (no specific date), use last 1560 bars (4 blocks) for warmup
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
    feed_config.filter_date = options_.test_date;  // Apply date filter if specified

    if (!options_.test_date.empty()) {
        log_system("Starting mock trading session...");
        log_system("Test date: " + options_.test_date + " (single-day mode)");
        log_system("");
    } else {
        log_system("Starting mock trading session...");
        log_system("");
    }

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

int RotationTradeCommand::execute_batch_mock_trading() {
    log_system("========================================");
    log_system("Batch Mock Trading Mode");
    log_system("========================================");
    log_system("Start Date: " + options_.start_date);
    log_system("End Date: " + options_.end_date);
    log_system("");

    // Set dashboard output directory if not specified
    if (options_.dashboard_output_dir.empty()) {
        options_.dashboard_output_dir = options_.log_dir + "/dashboards";
    }

    // Extract trading days from data
    auto trading_days = extract_trading_days(options_.start_date, options_.end_date);

    if (trading_days.empty()) {
        log_system("❌ No trading days found in date range");
        return 1;
    }

    log_system("Found " + std::to_string(trading_days.size()) + " trading days");
    for (const auto& day : trading_days) {
        log_system("  - " + day);
    }
    log_system("");

    // Results tracking
    std::vector<std::map<std::string, std::string>> daily_results;
    int success_count = 0;

    // Run mock trading for each day
    for (size_t i = 0; i < trading_days.size(); ++i) {
        const auto& date = trading_days[i];

        log_system("");
        log_system("========================================");
        log_system("[" + std::to_string(i+1) + "/" + std::to_string(trading_days.size()) + "] " + date);
        log_system("========================================");
        log_system("");

        // Set test date for this iteration
        options_.test_date = date;

        // Create day-specific output directory
        std::string day_output = options_.log_dir + "/" + date;
        std::filesystem::create_directories(day_output);

        // Temporarily redirect log_dir for this day
        std::string original_log_dir = options_.log_dir;
        options_.log_dir = day_output;

        // Execute single day mock trading
        int result = execute_mock_trading();

        // Restore log_dir
        options_.log_dir = original_log_dir;

        if (result == 0) {
            success_count++;

            // Generate dashboard if requested
            if (options_.generate_dashboards) {
                generate_daily_dashboard(date, day_output);
            }

            // Store results for summary
            std::map<std::string, std::string> day_result;
            day_result["date"] = date;
            day_result["output_dir"] = day_output;
            daily_results.push_back(day_result);
        }
    }

    log_system("");
    log_system("========================================");
    log_system("BATCH TEST COMPLETE");
    log_system("========================================");
    log_system("Successful days: " + std::to_string(success_count) + "/" + std::to_string(trading_days.size()));
    log_system("");

    // Generate summary dashboard
    if (!daily_results.empty() && options_.generate_dashboards) {
        generate_summary_dashboard(daily_results, options_.dashboard_output_dir);
    }

    return (success_count > 0) ? 0 : 1;
}

std::vector<std::string> RotationTradeCommand::extract_trading_days(
    const std::string& start_date,
    const std::string& end_date
) {
    std::vector<std::string> trading_days;
    std::set<std::string> unique_dates;

    // Read SPY data file to extract trading days
    std::string spy_file = options_.data_dir + "/SPY_RTH_NH.csv";
    std::ifstream file(spy_file);

    if (!file.is_open()) {
        log_system("❌ Could not open " + spy_file);
        return trading_days;
    }

    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Extract date from timestamp (format: YYYY-MM-DDTHH:MM:SS)
        size_t t_pos = line.find('T');
        if (t_pos != std::string::npos) {
            std::string date_str = line.substr(0, t_pos);

            // Check if within range
            if (date_str >= start_date && date_str <= end_date) {
                unique_dates.insert(date_str);
            }
        }
    }

    file.close();

    // Convert set to vector
    trading_days.assign(unique_dates.begin(), unique_dates.end());
    std::sort(trading_days.begin(), trading_days.end());

    return trading_days;
}

int RotationTradeCommand::generate_daily_dashboard(
    const std::string& date,
    const std::string& output_dir
) {
    log_system("📈 Generating dashboard for " + date + "...");

    // Build command
    std::string cmd = "python3 scripts/rotation_trading_dashboard.py";
    cmd += " --trades " + output_dir + "/trades.jsonl";
    cmd += " --output " + output_dir + "/dashboard.html";

    // Add optional files if they exist
    std::string signals_file = output_dir + "/signals.jsonl";
    std::string positions_file = output_dir + "/positions.jsonl";
    std::string decisions_file = output_dir + "/decisions.jsonl";

    if (std::filesystem::exists(signals_file)) {
        cmd += " --signals " + signals_file;
    }
    if (std::filesystem::exists(positions_file)) {
        cmd += " --positions " + positions_file;
    }
    if (std::filesystem::exists(decisions_file)) {
        cmd += " --decisions " + decisions_file;
    }

    // Execute
    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✓ Dashboard generated: " + output_dir + "/dashboard.html");
    } else {
        log_system("❌ Dashboard generation failed");
    }

    return result;
}

int RotationTradeCommand::generate_summary_dashboard(
    const std::vector<std::map<std::string, std::string>>& daily_results,
    const std::string& output_dir
) {
    log_system("");
    log_system("========================================");
    log_system("Generating Summary Dashboard");
    log_system("========================================");

    std::filesystem::create_directories(output_dir);

    // Create summary markdown
    std::string summary_file = output_dir + "/SUMMARY.md";
    std::ofstream out(summary_file);

    out << "# Rotation Trading Batch Test Summary\n\n";
    out << "## Test Period\n";
    out << "- **Start Date**: " << options_.start_date << "\n";
    out << "- **End Date**: " << options_.end_date << "\n";
    out << "- **Trading Days**: " << daily_results.size() << "\n\n";

    out << "## Daily Results\n\n";
    out << "| Date | Dashboard | Trades | Signals | Decisions |\n";
    out << "|------|-----------|--------|---------|----------|\n";

    for (const auto& result : daily_results) {
        std::string date = result.at("date");
        std::string dir = result.at("output_dir");

        out << "| " << date << " ";
        out << "| [View](" << dir << "/dashboard.html) ";
        out << "| [trades.jsonl](" << dir << "/trades.jsonl) ";
        out << "| [signals.jsonl](" << dir << "/signals.jsonl) ";
        out << "| [decisions.jsonl](" << dir << "/decisions.jsonl) ";
        out << "|\n";
    }

    out << "\n---\n\n";
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << "Generated on: " << std::ctime(&time);

    out.close();

    log_system("✅ Summary saved: " + summary_file);

    return 0;
}

} // namespace cli
} // namespace sentio
