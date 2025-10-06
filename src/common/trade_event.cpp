#include "common/trade_event.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace sentio {

// =============================================================================
// TradeEvent Implementation
// =============================================================================

std::string TradeEvent::get_formatted_time() const {
    return TradeEventFormatter::format_timestamp(timestamp_ms);
}

// =============================================================================
// TradeEventParser Implementation
// =============================================================================

TradeEvent TradeEventParser::parse_from_json(const std::string& json_line) {
    TradeEvent event;
    
    try {
        auto json_map = sentio::utils::from_json(json_line);
        
        // Parse all fields with safe defaults
        event.timestamp_ms = json_map.count("timestamp_ms") ? std::stoll(json_map["timestamp_ms"]) : 0;
        event.symbol = json_map.count("symbol") ? json_map["symbol"] : "?";
        event.action = json_map.count("action") ? json_map["action"] : "?";
        event.quantity = safe_parse_double(json_map.count("quantity") ? json_map["quantity"] : "0");
        event.price = safe_parse_double(json_map.count("price") ? json_map["price"] : "0");
        event.trade_value = safe_parse_double(json_map.count("trade_value") ? json_map["trade_value"] : "0");
        event.realized_pnl = safe_parse_double(json_map.count("realized_pnl") ? json_map["realized_pnl"] : "0");
        event.equity_after = safe_parse_double(json_map.count("equity_after") ? json_map["equity_after"] : "0");
        event.unrealized_pnl = safe_parse_double(json_map.count("unrealized_pnl") ? json_map["unrealized_pnl"] : "0");
        event.positions_summary = json_map.count("positions_summary") ? json_map["positions_summary"] : "";
        event.cash_after = safe_parse_double(json_map.count("cash_after") ? json_map["cash_after"] : "0");
        event.equity_before = safe_parse_double(json_map.count("equity_before") ? json_map["equity_before"] : "0");
        event.execution_reason = json_map.count("execution_reason") ? json_map["execution_reason"] : "";
        
    } catch (const std::exception& e) {
        // Log error but return partially parsed event
        sentio::utils::log_warning("Failed to parse trade event: " + std::string(e.what()));
    }
    
    return event;
}

std::vector<TradeEvent> TradeEventParser::parse_from_json_lines(const std::vector<std::string>& json_lines) {
    std::vector<TradeEvent> events;
    events.reserve(json_lines.size());
    
    for (const auto& line : json_lines) {
        if (!line.empty()) {
            events.push_back(parse_from_json(line));
        }
    }
    
    return events;
}

std::vector<TradeEvent> TradeEventParser::load_from_file(const std::string& filename) {
    std::vector<TradeEvent> events;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        sentio::utils::log_error("Cannot open trade file: " + filename);
        return events;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            events.push_back(parse_from_json(line));
        }
    }
    
    return events;
}

double TradeEventParser::safe_parse_double(const std::string& value, double default_value) {
    try {
        return std::stod(value);
    } catch (...) {
        return default_value;
    }
}

// =============================================================================
// TradeEventFormatter Implementation
// =============================================================================

std::string TradeEventFormatter::format_for_console(const TradeEvent& event) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << format_timestamp(event.timestamp_ms) << " "
        << std::setw(6) << event.symbol << " "
        << std::setw(4) << event.action << " "
        << std::setw(8) << event.quantity << " @ "
        << std::setw(8) << event.price << " "
        << "PnL: " << std::setw(8) << event.realized_pnl;
    return oss.str();
}

std::string TradeEventFormatter::format_as_table(const std::vector<TradeEvent>& events, size_t max_count) {
    std::ostringstream table;
    
    // Table header
    table << "┌─────────────────┬────────┬────────┬──────────┬──────────┬───────────────┬──────────────┬─────────────────┬───────────────────────────────┬─────────────────┐\n";
    table << "│ Date/Time       │ Symbol │ Action │ Quantity │ Price    │ Trade Value   │ Realized P&L │ Equity After    │ Positions                     │ Execution Reason│\n";
    table << "├─────────────────┼────────┼────────┼──────────┼──────────┼───────────────┼──────────────┼─────────────────┼───────────────────────────────┼─────────────────┤\n";
    
    // Determine which trades to show (most recent)
    size_t start_index = events.size() > max_count ? events.size() - max_count : 0;
    
    for (size_t i = start_index; i < events.size(); ++i) {
        const auto& event = events[i];
        
        table << "│ " << std::left << std::setw(15) << format_timestamp(event.timestamp_ms)
              << " │ " << std::setw(6) << event.symbol
              << " │ " << std::setw(6) << event.action
              << " │ " << std::right << std::setw(8) << std::fixed << std::setprecision(2) << event.quantity
              << " │ " << std::setw(8) << std::fixed << std::setprecision(2) << event.price
              << " │ " << std::setw(13) << std::fixed << std::setprecision(2) << event.trade_value
              << " │ " << std::setw(12) << std::fixed << std::setprecision(2) << event.realized_pnl
              << " │ " << std::setw(15) << std::fixed << std::setprecision(2) << event.equity_after
              << " │ " << std::left << std::setw(29) << event.positions_summary.substr(0, 29)
              << " │ " << std::setw(15) << event.execution_reason.substr(0, 15) << " │\n";
    }
    
    table << "└─────────────────┴────────┴────────┴──────────┴──────────┴───────────────┴──────────────┴─────────────────┴───────────────────────────────┴─────────────────┘\n";
    
    return table.str();
}

std::string TradeEventFormatter::format_timestamp(long long timestamp_ms) {
    std::time_t t = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm* utc_tm = std::gmtime(&t);  // Use UTC
    
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%m/%d %H:%M:%S", utc_tm);
    return std::string(buffer);
}

// =============================================================================
// TradeEventAnalyzer Implementation
// =============================================================================

TradeEventAnalyzer::TradeStatistics TradeEventAnalyzer::analyze(const std::vector<TradeEvent>& events) {
    TradeStatistics stats;
    
    if (events.empty()) {
        return stats;
    }
    
    stats.total_trades = events.size();
    stats.first_trade_time = events.front().timestamp_ms;
    stats.last_trade_time = events.back().timestamp_ms;
    stats.starting_equity = events.front().equity_before > 0 ? events.front().equity_before : events.front().equity_after;
    stats.final_equity = events.back().equity_after;
    
    for (const auto& event : events) {
        // Count trade types
        if (event.is_buy()) {
            stats.buy_trades++;
        } else if (event.is_sell()) {
            stats.sell_trades++;
        } else if (event.is_hold()) {
            stats.hold_trades++;
        }
        
        // Accumulate financial metrics
        stats.total_realized_pnl += event.realized_pnl;
        stats.total_trade_value += std::abs(event.trade_value);
    }
    
    return stats;
}

std::vector<TradeEvent> TradeEventAnalyzer::get_recent_trades(const std::vector<TradeEvent>& events, size_t count) {
    if (events.size() <= count) {
        return events;
    }
    
    // Return the last 'count' trades
    return std::vector<TradeEvent>(events.end() - count, events.end());
}

} // namespace sentio
