/**
 * Example: Using RotationTradingBackend
 *
 * This example demonstrates how to use the complete rotation trading system
 * for both mock (backtest) and live (paper) trading modes.
 *
 * Compile:
 *   g++ -std=c++17 -I../include rotation_backend_usage.cpp -o rotation_example
 *
 * Run:
 *   ./rotation_example
 */

#include "backend/rotation_trading_backend.h"
#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <iostream>

using namespace sentio;

/**
 * Example 1: Mock Trading (Backtest)
 *
 * Tests the rotation strategy on historical data.
 */
void example_mock_trading() {
    utils::log_info("========================================");
    utils::log_info("Example 1: Mock Trading (Backtest)");
    utils::log_info("========================================\n");

    // Configure backend
    RotationTradingBackend::Config config;
    config.symbols = {"TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"};
    config.starting_capital = 100000.0;
    config.enable_trading = true;

    // OES configuration
    config.oes_config.buy_threshold = 0.53;
    config.oes_config.sell_threshold = 0.47;
    config.oes_config.warmup_samples = 100;

    // Signal aggregator configuration
    config.aggregator_config.leverage_boosts = {
        {"TQQQ", 1.5}, {"SQQQ", 1.5}, {"UPRO", 1.5},
        {"SDS", 1.4}, {"UVXY", 1.3}, {"SVIX", 1.3}
    };
    config.aggregator_config.min_strength = 0.40;

    // Rotation configuration
    config.rotation_config.max_positions = 3;
    config.rotation_config.min_strength_to_enter = 0.50;
    config.rotation_config.rotation_strength_delta = 0.10;
    config.rotation_config.profit_target_pct = 0.03;
    config.rotation_config.stop_loss_pct = 0.015;

    // Output paths
    config.signal_log_path = "logs/mock_trading/signals.jsonl";
    config.decision_log_path = "logs/mock_trading/decisions.jsonl";
    config.trade_log_path = "logs/mock_trading/trades.jsonl";
    config.position_log_path = "logs/mock_trading/positions.jsonl";

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = config.symbols;
    auto data_mgr = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create backend
    RotationTradingBackend backend(config, data_mgr, nullptr);

    // Load historical data and warmup
    std::map<std::string, std::vector<Bar>> warmup_data;

    // TODO: Load actual warmup data from CSV files
    // For this example, we'll skip the actual data loading

    // backend.warmup(warmup_data);

    // Create mock feed
    data::MockMultiSymbolFeed::Config feed_config;
    feed_config.symbol_files = {
        {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
        {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"},
        {"UPRO", "data/equities/UPRO_RTH_NH.csv"},
        {"SDS", "data/equities/SDS_RTH_NH.csv"},
        {"UVXY", "data/equities/UVXY_RTH_NH.csv"},
        {"SVIX", "data/equities/SVIX_RTH_NH.csv"}
    };
    feed_config.replay_speed = 0.0;  // Instant replay for testing

    auto feed = std::make_shared<data::MockMultiSymbolFeed>(data_mgr, feed_config);

    // Set callback to trigger backend on each bar
    feed->set_bar_callback([&](const std::string& symbol, const Bar& bar) {
        // Backend processes on every bar
        backend.on_bar();
    });

    // Start trading
    backend.start_trading();

    // Connect and start feed
    feed->connect();
    feed->start();

    // Wait for replay to complete (in real implementation)
    // ...

    // Stop trading
    backend.stop_trading();

    // Print results
    auto stats = backend.get_session_stats();
    std::cout << "\nResults:\n";
    std::cout << "  Total P&L: $" << stats.total_pnl << " (" << (stats.total_pnl_pct * 100.0) << "%)\n";
    std::cout << "  MRD: " << (stats.mrd * 100.0) << "%\n";
    std::cout << "  Win Rate: " << (stats.win_rate * 100.0) << "%\n";
    std::cout << "  Sharpe: " << stats.sharpe_ratio << "\n";
    std::cout << "  Max Drawdown: " << (stats.max_drawdown * 100.0) << "%\n";
    std::cout << "  Trades: " << stats.trades_executed << "\n";
}

/**
 * Example 2: Live Paper Trading
 *
 * Runs the rotation strategy in live paper trading mode with Alpaca.
 */
void example_live_trading() {
    utils::log_info("========================================");
    utils::log_info("Example 2: Live Paper Trading");
    utils::log_info("========================================\n");

    // Configure backend
    RotationTradingBackend::Config config;
    config.symbols = {"TQQQ", "SQQQ", "UPRO"};  // Fewer symbols for live
    config.starting_capital = 100000.0;
    config.enable_trading = true;
    config.paper_trading = true;

    // Alpaca credentials (from environment)
    config.alpaca_api_key = std::getenv("ALPACA_PAPER_API_KEY");
    config.alpaca_secret_key = std::getenv("ALPACA_PAPER_SECRET_KEY");

    // Output paths
    config.signal_log_path = "logs/live_trading/signals.jsonl";
    config.trade_log_path = "logs/live_trading/trades.jsonl";
    config.position_log_path = "logs/live_trading/positions.jsonl";

    // Create data manager
    data::MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = config.symbols;
    auto data_mgr = std::make_shared<data::MultiSymbolDataManager>(dm_config);

    // Create Alpaca client
    // auto broker = std::make_shared<AlpacaClient>(config.alpaca_api_key, config.alpaca_secret_key);

    // Create backend
    RotationTradingBackend backend(config, data_mgr, nullptr);

    // Warmup with recent historical data
    // TODO: Load last 20 days from Alpaca REST API
    // std::map<std::string, std::vector<Bar>> warmup_data = load_recent_history();
    // backend.warmup(warmup_data);

    // Start trading
    backend.start_trading();

    // In live mode, backend.on_bar() would be called:
    // 1. From WebSocket bar callback (real-time)
    // 2. Or from scheduled timer every minute

    // Main loop (simplified)
    /*
    while (market_open && !should_stop) {
        // Wait for next bar
        std::this_thread::sleep_for(std::chrono::seconds(60));

        // Process bar
        backend.on_bar();

        // Check EOD
        if (backend.is_eod(get_minutes_since_open())) {
            break;
        }
    }
    */

    // Stop trading
    backend.stop_trading();
}

/**
 * Example 3: Configuration from JSON
 *
 * Load configuration from rotation_strategy.json
 */
void example_config_from_json() {
    utils::log_info("========================================");
    utils::log_info("Example 3: Configuration from JSON");
    utils::log_info("========================================\n");

    // In production, you'd load this from config/rotation_strategy.json
    // and parse it into RotationTradingBackend::Config

    std::cout << "See config/rotation_strategy.json for full configuration\n";
}

int main() {
    std::cout << "====================================\n";
    std::cout << "Rotation Trading Backend Examples\n";
    std::cout << "====================================\n\n";

    std::cout << "Example 1: Mock Trading\n";
    std::cout << "  Demonstrates backtesting with CSV data\n\n";

    std::cout << "Example 2: Live Trading\n";
    std::cout << "  Demonstrates live paper trading with Alpaca\n\n";

    std::cout << "Example 3: Configuration\n";
    std::cout << "  Shows how to load config from JSON\n\n";

    // Uncomment to run examples
    // example_mock_trading();
    // example_live_trading();
    // example_config_from_json();

    std::cout << "See rotation_backend_usage.cpp for full implementation\n";

    return 0;
}
