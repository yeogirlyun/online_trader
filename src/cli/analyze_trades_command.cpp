#include "cli/ensemble_workflow_command.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace sentio {
namespace cli {

// Per-instrument performance tracking
struct InstrumentMetrics {
    std::string symbol;
    int num_trades = 0;
    int buy_count = 0;
    int sell_count = 0;
    double total_buy_value = 0.0;
    double total_sell_value = 0.0;
    double realized_pnl = 0.0;
    double avg_allocation_pct = 0.0;  // Average % of portfolio allocated
    double win_rate = 0.0;
    int winning_trades = 0;
    int losing_trades = 0;
};

int AnalyzeTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string trades_path = get_arg(args, "--trades", "");
    std::string output_path = get_arg(args, "--output", "analysis_report.json");
    int num_blocks = std::stoi(get_arg(args, "--blocks", "0"));  // Number of blocks for MRB calculation
    bool show_detailed = !has_flag(args, "--summary-only");
    bool show_trades = has_flag(args, "--show-trades");
    bool export_csv = has_flag(args, "--csv");
    bool export_json = !has_flag(args, "--no-json");
    bool json_stdout = has_flag(args, "--json");  // Output JSON metrics to stdout for Optuna

    if (trades_path.empty()) {
        std::cerr << "Error: --trades is required\n";
        show_help();
        return 1;
    }

    if (!json_stdout) {
        std::cout << "=== OnlineEnsemble Trade Analysis ===\n";
        std::cout << "Trade file: " << trades_path << "\n\n";
    }

    // Load trades from JSONL
    if (!json_stdout) {
        std::cout << "Loading trade history...\n";
    }
    std::vector<ExecuteTradesCommand::TradeRecord> trades;

    std::ifstream file(trades_path);
    if (!file) {
        std::cerr << "Error: Could not open trade file\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json j = json::parse(line);
            ExecuteTradesCommand::TradeRecord trade;

            trade.bar_id = j["bar_id"];
            trade.timestamp_ms = j["timestamp_ms"];
            trade.bar_index = j["bar_index"];
            trade.symbol = j["symbol"];

            std::string action_str = j["action"];
            trade.action = (action_str == "BUY") ? TradeAction::BUY : TradeAction::SELL;

            trade.quantity = j["quantity"];
            trade.price = j["price"];
            trade.trade_value = j["trade_value"];
            trade.fees = j["fees"];
            trade.reason = j["reason"];

            trade.cash_balance = j["cash_balance"];
            trade.portfolio_value = j["portfolio_value"];
            trade.position_quantity = j["position_quantity"];
            trade.position_avg_price = j["position_avg_price"];

            trades.push_back(trade);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse line: " << e.what() << "\n";
        }
    }

    if (!json_stdout) {
        std::cout << "Loaded " << trades.size() << " trades\n\n";
    }

    if (trades.empty()) {
        std::cerr << "Error: No trades loaded\n";
        return 1;
    }

    // Calculate per-instrument metrics
    if (!json_stdout) {
        std::cout << "Calculating per-instrument metrics...\n";
    }
    std::map<std::string, InstrumentMetrics> instrument_metrics;
    std::map<std::string, std::vector<std::pair<double, double>>> position_tracking;  // symbol -> [(buy_price, quantity)]

    double starting_capital = 100000.0;  // Assume standard starting capital
    double total_allocation_samples = 0;

    for (const auto& trade : trades) {
        auto& metrics = instrument_metrics[trade.symbol];
        metrics.symbol = trade.symbol;
        metrics.num_trades++;

        if (trade.action == TradeAction::BUY) {
            metrics.buy_count++;
            metrics.total_buy_value += trade.trade_value;

            // Track position for P/L calculation
            position_tracking[trade.symbol].push_back({trade.price, trade.quantity});

            // Track allocation
            double allocation_pct = (trade.trade_value / trade.portfolio_value) * 100.0;
            metrics.avg_allocation_pct += allocation_pct;
            total_allocation_samples++;

        } else {  // SELL
            metrics.sell_count++;
            metrics.total_sell_value += trade.trade_value;

            // Calculate realized P/L using FIFO
            auto& positions = position_tracking[trade.symbol];
            double remaining_qty = trade.quantity;
            double trade_pnl = 0.0;

            while (remaining_qty > 0 && !positions.empty()) {
                auto& pos = positions.front();
                double qty_to_close = std::min(remaining_qty, pos.second);

                // P/L = (sell_price - buy_price) * quantity
                trade_pnl += (trade.price - pos.first) * qty_to_close;

                pos.second -= qty_to_close;
                remaining_qty -= qty_to_close;

                if (pos.second <= 0) {
                    positions.erase(positions.begin());
                }
            }

            metrics.realized_pnl += trade_pnl;

            // Track win/loss
            if (trade_pnl > 0) {
                metrics.winning_trades++;
            } else if (trade_pnl < 0) {
                metrics.losing_trades++;
            }
        }
    }

    // Calculate averages and win rates
    for (auto& [symbol, metrics] : instrument_metrics) {
        if (metrics.buy_count > 0) {
            metrics.avg_allocation_pct /= metrics.buy_count;
        }
        int completed_trades = metrics.winning_trades + metrics.losing_trades;
        if (completed_trades > 0) {
            metrics.win_rate = (double)metrics.winning_trades / completed_trades * 100.0;
        }
    }

    // Calculate overall metrics
    if (!json_stdout) {
        std::cout << "Calculating overall performance metrics...\n";
    }
    PerformanceReport report = calculate_metrics(trades);

    // Print instrument analysis
    if (!json_stdout) {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║         PER-INSTRUMENT PERFORMANCE ANALYSIS                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    // Sort instruments by realized P/L (descending)
    std::vector<std::pair<std::string, InstrumentMetrics>> sorted_instruments;
    for (const auto& [symbol, metrics] : instrument_metrics) {
        sorted_instruments.push_back({symbol, metrics});
    }
    std::sort(sorted_instruments.begin(), sorted_instruments.end(),
              [](const auto& a, const auto& b) { return a.second.realized_pnl > b.second.realized_pnl; });

    if (!json_stdout) {
        std::cout << std::fixed << std::setprecision(2);

        for (const auto& [symbol, m] : sorted_instruments) {
            std::string pnl_indicator = (m.realized_pnl > 0) ? "✅" : (m.realized_pnl < 0) ? "❌" : "  ";

            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << symbol << " " << pnl_indicator << "\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            std::cout << "  Trades:           " << m.num_trades << " (" << m.buy_count << " BUY, " << m.sell_count << " SELL)\n";
            std::cout << "  Total Buy Value:  $" << std::setw(12) << m.total_buy_value << "\n";
            std::cout << "  Total Sell Value: $" << std::setw(12) << m.total_sell_value << "\n";
            std::cout << "  Realized P/L:     $" << std::setw(12) << m.realized_pnl
                      << "  (" << std::showpos << (m.realized_pnl / starting_capital * 100.0)
                      << std::noshowpos << "% of capital)\n";
            std::cout << "  Avg Allocation:   " << std::setw(12) << m.avg_allocation_pct << "%\n";
            std::cout << "  Win Rate:         " << std::setw(12) << m.win_rate << "%  ("
                      << m.winning_trades << "W / " << m.losing_trades << "L)\n";
            std::cout << "\n";
        }

        // Summary table
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║              INSTRUMENT SUMMARY TABLE                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        std::cout << std::left << std::setw(8) << "Symbol"
                  << std::right << std::setw(10) << "Trades"
                  << std::setw(12) << "Alloc %"
                  << std::setw(15) << "P/L ($)"
                  << std::setw(12) << "P/L (%)"
                  << std::setw(12) << "Win Rate"
                  << "\n";
        std::cout << "────────────────────────────────────────────────────────────────\n";

        for (const auto& [symbol, m] : sorted_instruments) {
            double pnl_pct = (m.realized_pnl / starting_capital) * 100.0;
            std::cout << std::left << std::setw(8) << symbol
                      << std::right << std::setw(10) << m.num_trades
                      << std::setw(12) << m.avg_allocation_pct
                      << std::setw(15) << m.realized_pnl
                      << std::setw(12) << std::showpos << pnl_pct << std::noshowpos
                      << std::setw(12) << m.win_rate
                      << "\n";
        }
    }

    // Calculate total realized P/L from instruments
    double total_realized_pnl = 0.0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_realized_pnl += m.realized_pnl;
    }

    if (!json_stdout) {
        std::cout << "────────────────────────────────────────────────────────────────\n";
        std::cout << std::left << std::setw(8) << "TOTAL"
                  << std::right << std::setw(10) << trades.size()
                  << std::setw(12) << ""
                  << std::setw(15) << total_realized_pnl
                  << std::setw(12) << std::showpos << (total_realized_pnl / starting_capital * 100.0) << std::noshowpos
                  << std::setw(12) << ""
                  << "\n\n";
    }

    // Calculate MRB (Mean Return per Block) - for strategies with overnight carry
    double total_return_pct = (total_realized_pnl / starting_capital) * 100.0;
    double mrb = 0.0;
    if (num_blocks > 0) {
        mrb = total_return_pct / num_blocks;
    }

    // Calculate MRD (Mean Return per Day) - for daily reset strategies
    // This is the more accurate metric for strategies with EOD liquidation
    double mrd = 0.0;
    int num_trading_days = 0;
    std::vector<double> daily_returns;

    if (!trades.empty()) {
        // Group trades by trading day
        std::map<std::string, std::vector<ExecuteTradesCommand::TradeRecord>> trades_by_day;

        for (const auto& trade : trades) {
            // Extract date from timestamp (YYYY-MM-DD)
            std::time_t trade_time = static_cast<std::time_t>(trade.timestamp_ms / 1000);
            std::tm tm_utc{};
            #ifdef _WIN32
                gmtime_s(&tm_utc, &trade_time);
            #else
                gmtime_r(&trade_time, &tm_utc);
            #endif

            // Convert to ET (subtract 4 hours for EDT)
            int et_hour = tm_utc.tm_hour - 4;
            if (et_hour < 0) et_hour += 24;

            // Format as YYYY-MM-DD
            char date_str[32];
            std::snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday);

            trades_by_day[date_str].push_back(trade);
        }

        // Calculate daily returns
        double prev_day_end_value = starting_capital;

        for (const auto& [date, day_trades] : trades_by_day) {
            if (day_trades.empty()) continue;

            // Get final portfolio value of the day
            double day_end_value = day_trades.back().portfolio_value;

            // Calculate daily return
            double daily_return_pct = ((day_end_value - prev_day_end_value) / prev_day_end_value) * 100.0;
            daily_returns.push_back(daily_return_pct);

            // Update for next day
            prev_day_end_value = day_end_value;
        }

        num_trading_days = static_cast<int>(daily_returns.size());

        // MRD = mean of daily returns
        if (!daily_returns.empty()) {
            double sum = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0);
            mrd = sum / daily_returns.size();
        }
    }

    // Print metrics
    if (!json_stdout) {
        if (num_blocks > 0) {
            std::cout << "Mean Return per Block (MRB): " << std::showpos << std::fixed << std::setprecision(4)
                      << mrb << std::noshowpos << "% (" << num_blocks << " blocks of 391 bars)\n";
        }

        if (num_trading_days > 0) {
            std::cout << "Mean Return per Day (MRD):   " << std::showpos << std::fixed << std::setprecision(4)
                      << mrd << std::noshowpos << "% (" << num_trading_days << " trading days)\n";

            // Show annualized projection
            double annualized_mrd = mrd * 252.0;  // 252 trading days per year
            std::cout << "  Annualized (252 days):     " << std::showpos << std::fixed << std::setprecision(2)
                      << annualized_mrd << std::noshowpos << "%\n";
        }

        std::cout << "\n";
    }

    // Calculate overall win rate and trades per block
    int total_winning = 0, total_losing = 0;
    for (const auto& [symbol, m] : instrument_metrics) {
        total_winning += m.winning_trades;
        total_losing += m.losing_trades;
    }
    double overall_win_rate = (total_winning + total_losing > 0)
        ? (double)total_winning / (total_winning + total_losing) * 100.0 : 0.0;
    double trades_per_block = (num_blocks > 0) ? (double)trades.size() / num_blocks : 0.0;

    // If --json flag, output metrics as JSON to stdout and exit
    if (json_stdout) {
        json result;
        result["mrb"] = mrb;
        result["mrd"] = mrd;  // New: Mean Return per Day (primary metric for daily strategies)
        result["total_return_pct"] = total_return_pct;
        result["win_rate"] = overall_win_rate;
        result["total_trades"] = trades.size();
        result["trades_per_block"] = trades_per_block;
        result["num_blocks"] = num_blocks;
        result["num_trading_days"] = num_trading_days;

        // Output compact JSON (single line) for Optuna parsing
        std::cout << result.dump() << std::endl;
        return 0;
    }

    // Print overall report
    print_report(report);

    // Save report
    if (export_json) {
        std::cout << "\nSaving report to " << output_path << "...\n";
        save_report_json(report, output_path);
    }

    std::cout << "\n✅ Analysis complete!\n";
    return 0;
}

AnalyzeTradesCommand::PerformanceReport
AnalyzeTradesCommand::calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades) {
    PerformanceReport report;

    if (trades.empty()) {
        return report;
    }

    // Basic counts
    report.total_trades = static_cast<int>(trades.size());

    // Extract equity curve from trades
    std::vector<double> equity;
    for (const auto& trade : trades) {
        equity.push_back(trade.portfolio_value);
    }

    // Calculate returns (stub - would need full implementation)
    report.total_return_pct = 0.0;
    report.annualized_return = 0.0;

    return report;
}

void AnalyzeTradesCommand::print_report(const PerformanceReport& report) {
    // Stub - basic implementation
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Total Trades: " << report.total_trades << "\n";
}

void AnalyzeTradesCommand::save_report_json(const PerformanceReport& report, const std::string& path) {
    // Stub
}

void AnalyzeTradesCommand::show_help() const {
    std::cout << "Usage: sentio_cli analyze-trades --trades <file> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --trades <file>     Trade history file (JSONL format)\n";
    std::cout << "  --output <file>     Output report file (default: analysis_report.json)\n";
    std::cout << "  --blocks <N>        Number of blocks traded (for MRB calculation)\n";
    std::cout << "  --json              Output metrics as JSON to stdout (for Optuna)\n";
    std::cout << "  --summary-only      Show only summary metrics\n";
    std::cout << "  --show-trades       Show individual trade details\n";
    std::cout << "  --csv               Export to CSV format\n";
    std::cout << "  --no-json           Disable JSON export\n";
}

} // namespace cli
} // namespace sentio
