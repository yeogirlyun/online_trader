#include "cli/ensemble_workflow_command.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace sentio {
namespace cli {

int AnalyzeTradesCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string trades_path = get_arg(args, "--trades", "");
    std::string output_path = get_arg(args, "--output", "analysis_report.json");
    bool show_detailed = !has_flag(args, "--summary-only");
    bool show_trades = has_flag(args, "--show-trades");
    bool export_csv = has_flag(args, "--csv");
    bool export_json = !has_flag(args, "--no-json");

    if (trades_path.empty()) {
        std::cerr << "Error: --trades is required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Trade Analysis ===\n";
    std::cout << "Trade file: " << trades_path << "\n\n";

    // Load trades
    std::cout << "Loading trade history...\n";
    std::vector<ExecuteTradesCommand::TradeRecord> trades;

    std::ifstream file(trades_path);
    if (!file) {
        std::cerr << "Error: Could not open trade file\n";
        return 1;
    }

    std::string line;
    // Skip header if CSV
    if (trades_path.find(".csv") != std::string::npos) {
        std::getline(file, line);
    }

    while (std::getline(file, line)) {
        // Parse trade record (simplified - would need proper JSON parsing)
        ExecuteTradesCommand::TradeRecord trade;
        // TODO: Parse from JSONL or CSV
        trades.push_back(trade);
    }

    std::cout << "Loaded " << trades.size() << " trades\n\n";

    // Calculate metrics
    std::cout << "Calculating performance metrics...\n";
    PerformanceReport report = calculate_metrics(trades);

    // Print report
    print_report(report);

    // Save report
    if (export_json) {
        std::cout << "\nSaving report to " << output_path << "...\n";
        save_report_json(report, output_path);
    }

    // Show individual trades if requested
    if (show_trades) {
        std::cout << "\n=== Trade Details ===\n";
        for (size_t i = 0; i < std::min(size_t(50), trades.size()); ++i) {
            const auto& t = trades[i];
            std::cout << "[" << i << "] " << t.symbol << " "
                     << (t.action == TradeAction::BUY ? "BUY" : "SELL") << " "
                     << t.quantity << " @ $" << t.price << "\n";
        }
        if (trades.size() > 50) {
            std::cout << "... and " << (trades.size() - 50) << " more trades\n";
        }
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
    std::vector<double> returns;

    double starting_capital = 100000.0;
    if (!trades.empty()) {
        starting_capital = trades[0].portfolio_value;
    }

    for (const auto& trade : trades) {
        equity.push_back(trade.portfolio_value);

        // Calculate return for this trade
        if (trade.action == TradeAction::SELL && trade.position_avg_price > 0) {
            double pnl = (trade.price - trade.position_avg_price) * trade.quantity;
            double ret = pnl / (trade.position_avg_price * trade.quantity);
            returns.push_back(ret);

            if (ret > 0) {
                report.winning_trades++;
                report.avg_win += ret;
                report.largest_win = std::max(report.largest_win, ret);
            } else {
                report.losing_trades++;
                report.avg_loss += std::abs(ret);
                report.largest_loss = std::max(report.largest_loss, std::abs(ret));
            }

            // Count long/short
            if (trade.quantity > 0) {
                report.total_long_trades++;
            } else {
                report.total_short_trades++;
            }
        }
    }

    // Win rate
    int total_closed = report.winning_trades + report.losing_trades;
    if (total_closed > 0) {
        report.win_rate = static_cast<double>(report.winning_trades) / total_closed;
    }

    // Average win/loss
    if (report.winning_trades > 0) {
        report.avg_win /= report.winning_trades;
    }
    if (report.losing_trades > 0) {
        report.avg_loss /= report.losing_trades;
    }

    // Profit factor
    if (report.avg_loss > 0) {
        report.profit_factor = (report.avg_win * report.winning_trades) /
                              (report.avg_loss * report.losing_trades);
    }

    // Returns
    double final_capital = equity.back();
    report.total_return_pct = ((final_capital / starting_capital) - 1.0) * 100;

    // Estimate annualized return (assuming 252 trading days)
    int trading_days = report.total_trades;  // Rough estimate
    report.trading_days = trading_days;
    report.bars_traded = report.total_trades;

    if (trading_days > 0) {
        double years = trading_days / 252.0;
        report.annualized_return = (std::pow(final_capital / starting_capital, 1.0 / years) - 1.0) * 100;
        report.monthly_return = report.annualized_return / 12.0;
        report.daily_return = report.annualized_return / 252.0;
    }

    // Calculate drawdowns
    double peak = starting_capital;
    report.max_drawdown = 0.0;
    double total_drawdown = 0.0;
    int drawdown_count = 0;

    for (double value : equity) {
        if (value > peak) {
            peak = value;
        }
        double dd = (peak - value) / peak;
        report.max_drawdown = std::max(report.max_drawdown, dd);
        if (dd > 0) {
            total_drawdown += dd;
            drawdown_count++;
        }
    }

    if (drawdown_count > 0) {
        report.avg_drawdown = total_drawdown / drawdown_count;
    }

    // Volatility and Sharpe ratio
    if (returns.size() > 1) {
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        report.avg_trade = mean_return;

        // Variance
        double variance = 0.0;
        for (double r : returns) {
            variance += (r - mean_return) * (r - mean_return);
        }
        variance /= (returns.size() - 1);
        report.volatility = std::sqrt(variance);

        // Sharpe ratio (assuming 0% risk-free rate)
        if (report.volatility > 0) {
            report.sharpe_ratio = (mean_return / report.volatility) * std::sqrt(252);
        }

        // Downside deviation (only negative returns)
        double downside_var = 0.0;
        int negative_count = 0;
        for (double r : returns) {
            if (r < 0) {
                downside_var += r * r;
                negative_count++;
            }
        }
        if (negative_count > 0) {
            report.downside_deviation = std::sqrt(downside_var / negative_count);

            // Sortino ratio
            if (report.downside_deviation > 0) {
                report.sortino_ratio = (mean_return / report.downside_deviation) * std::sqrt(252);
            }
        }
    }

    // Calmar ratio
    if (report.max_drawdown > 0) {
        report.calmar_ratio = report.annualized_return / (report.max_drawdown * 100);
    }

    // Kelly criterion (estimated from win rate and avg win/loss)
    if (report.win_rate > 0.5 && report.avg_loss > 0) {
        double p = report.win_rate;
        double b = report.avg_win / report.avg_loss;
        report.kelly_criterion = (p * b - (1 - p)) / b;
    }

    return report;
}

void AnalyzeTradesCommand::print_report(const PerformanceReport& report) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Returns
    std::cout << "=== RETURNS ===\n";
    std::cout << "  Total Return:        " << std::fixed << std::setprecision(2)
             << report.total_return_pct << "%\n";
    std::cout << "  Annualized Return:   " << report.annualized_return << "%\n";
    std::cout << "  Monthly Return:      " << report.monthly_return << "% ";
    if (report.monthly_return >= 10.0) {
        std::cout << "✅ TARGET MET!\n";
    } else {
        std::cout << "⚠️  (target: 10%)\n";
    }
    std::cout << "  Daily Return:        " << report.daily_return << "%\n\n";

    // Risk Metrics
    std::cout << "=== RISK METRICS ===\n";
    std::cout << "  Max Drawdown:        " << std::setprecision(2) << (report.max_drawdown * 100) << "%\n";
    std::cout << "  Avg Drawdown:        " << (report.avg_drawdown * 100) << "%\n";
    std::cout << "  Volatility:          " << (report.volatility * 100) << "%\n";
    std::cout << "  Downside Deviation:  " << (report.downside_deviation * 100) << "%\n";
    std::cout << "  Sharpe Ratio:        " << std::setprecision(3) << report.sharpe_ratio << "\n";
    std::cout << "  Sortino Ratio:       " << report.sortino_ratio << "\n";
    std::cout << "  Calmar Ratio:        " << report.calmar_ratio << "\n\n";

    // Trading Metrics
    std::cout << "=== TRADING METRICS ===\n";
    std::cout << "  Total Trades:        " << report.total_trades << "\n";
    std::cout << "  Winning Trades:      " << report.winning_trades << "\n";
    std::cout << "  Losing Trades:       " << report.losing_trades << "\n";
    std::cout << "  Win Rate:            " << std::setprecision(1) << (report.win_rate * 100) << "% ";
    if (report.win_rate >= 0.60) {
        std::cout << "✅ TARGET MET!\n";
    } else {
        std::cout << "⚠️  (target: 60%)\n";
    }
    std::cout << "  Profit Factor:       " << std::setprecision(2) << report.profit_factor << "\n";
    std::cout << "  Avg Win:             " << (report.avg_win * 100) << "%\n";
    std::cout << "  Avg Loss:            " << (report.avg_loss * 100) << "%\n";
    std::cout << "  Avg Trade:           " << (report.avg_trade * 100) << "%\n";
    std::cout << "  Largest Win:         " << (report.largest_win * 100) << "%\n";
    std::cout << "  Largest Loss:        " << (report.largest_loss * 100) << "%\n\n";

    // Position Metrics
    std::cout << "=== POSITION METRICS ===\n";
    std::cout << "  Long Trades:         " << report.total_long_trades << "\n";
    std::cout << "  Short Trades:        " << report.total_short_trades << "\n";
    std::cout << "  Kelly Criterion:     " << std::setprecision(3) << report.kelly_criterion << "\n";
    std::cout << "  Avg Position Size:   " << (report.avg_position_size * 100) << "%\n";
    std::cout << "  Max Position Size:   " << (report.max_position_size * 100) << "%\n\n";

    // Time Analysis
    std::cout << "=== TIME ANALYSIS ===\n";
    std::cout << "  Trading Days:        " << report.trading_days << "\n";
    std::cout << "  Bars Traded:         " << report.bars_traded << "\n";
    std::cout << "  Start Date:          " << report.start_date << "\n";
    std::cout << "  End Date:            " << report.end_date << "\n\n";

    // Target Check
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    TARGET CHECK                            ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ ✓ Monthly Return ≥ 10%:  ";
    if (report.monthly_return >= 10.0) {
        std::cout << "PASS ✅                          ║\n";
    } else {
        std::cout << "FAIL ❌ (" << std::setprecision(1) << report.monthly_return << "%)              ║\n";
    }
    std::cout << "║ ✓ Win Rate ≥ 60%:        ";
    if (report.win_rate >= 0.60) {
        std::cout << "PASS ✅                          ║\n";
    } else {
        std::cout << "FAIL ❌ (" << std::setprecision(1) << (report.win_rate * 100) << "%)              ║\n";
    }
    std::cout << "║ ✓ Max Drawdown < 15%:    ";
    if (report.max_drawdown < 0.15) {
        std::cout << "PASS ✅                          ║\n";
    } else {
        std::cout << "FAIL ❌ (" << std::setprecision(1) << (report.max_drawdown * 100) << "%)             ║\n";
    }
    std::cout << "║ ✓ Sharpe Ratio > 1.5:    ";
    if (report.sharpe_ratio > 1.5) {
        std::cout << "PASS ✅                          ║\n";
    } else {
        std::cout << "FAIL ❌ (" << std::setprecision(2) << report.sharpe_ratio << ")                    ║\n";
    }
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
}

void AnalyzeTradesCommand::save_report_json(const PerformanceReport& report, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    out << "{\n";
    out << "  \"returns\": {\n";
    out << "    \"total_return_pct\": " << report.total_return_pct << ",\n";
    out << "    \"annualized_return\": " << report.annualized_return << ",\n";
    out << "    \"monthly_return\": " << report.monthly_return << ",\n";
    out << "    \"daily_return\": " << report.daily_return << "\n";
    out << "  },\n";
    out << "  \"risk\": {\n";
    out << "    \"max_drawdown\": " << report.max_drawdown << ",\n";
    out << "    \"sharpe_ratio\": " << report.sharpe_ratio << ",\n";
    out << "    \"sortino_ratio\": " << report.sortino_ratio << ",\n";
    out << "    \"volatility\": " << report.volatility << "\n";
    out << "  },\n";
    out << "  \"trading\": {\n";
    out << "    \"total_trades\": " << report.total_trades << ",\n";
    out << "    \"win_rate\": " << report.win_rate << ",\n";
    out << "    \"profit_factor\": " << report.profit_factor << ",\n";
    out << "    \"avg_trade\": " << report.avg_trade << "\n";
    out << "  },\n";
    out << "  \"targets_met\": {\n";
    out << "    \"monthly_return_10pct\": " << (report.monthly_return >= 10.0 ? "true" : "false") << ",\n";
    out << "    \"win_rate_60pct\": " << (report.win_rate >= 0.60 ? "true" : "false") << ",\n";
    out << "    \"max_drawdown_15pct\": " << (report.max_drawdown < 0.15 ? "true" : "false") << ",\n";
    out << "    \"sharpe_1_5\": " << (report.sharpe_ratio > 1.5 ? "true" : "false") << "\n";
    out << "  }\n";
    out << "}\n";
}

void AnalyzeTradesCommand::show_help() const {
    std::cout << R"(
Analyze OnlineEnsemble Trades
==============================

Analyze trade history and generate performance reports.

USAGE:
    sentio_cli analyze-trades --trades <path> [OPTIONS]

REQUIRED:
    --trades <path>            Path to trade history file (JSONL or CSV)

OPTIONS:
    --output <path>            Output report file (default: analysis_report.json)
    --summary-only             Show only summary (skip detailed metrics)
    --show-trades              Display individual trades
    --csv                      Export report as CSV
    --no-json                  Don't save JSON report

EXAMPLES:
    # Analyze trades and save report
    sentio_cli analyze-trades --trades trades.jsonl

    # Show individual trades
    sentio_cli analyze-trades --trades trades.jsonl --show-trades

    # Export as CSV
    sentio_cli analyze-trades --trades trades.csv --csv --output report.csv

METRICS:
    Returns:      Total, annualized, monthly, daily
    Risk:         Max DD, volatility, Sharpe, Sortino, Calmar
    Trading:      Win rate, profit factor, avg win/loss
    Targets:      10% monthly, 60% win rate, <15% DD, >1.5 Sharpe

)" << std::endl;
}

} // namespace cli
} // namespace sentio
