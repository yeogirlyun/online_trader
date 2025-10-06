#pragma once

#include <string>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Represents a single trade execution event
 * 
 * This class follows the Single Responsibility Principle by focusing
 * solely on data representation of trade events.
 */
class TradeEvent {
public:
    // Core trade data
    long long timestamp_ms = 0;
    std::string symbol;
    std::string action;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double realized_pnl = 0.0;
    double equity_after = 0.0;
    double unrealized_pnl = 0.0;
    std::string positions_summary;
    double cash_after = 0.0;
    double equity_before = 0.0;
    std::string execution_reason;
    
    // Default constructor
    TradeEvent() = default;
    
    // Convenience methods
    bool is_buy() const { return action == "BUY"; }
    bool is_sell() const { return action == "SELL"; }
    bool is_hold() const { return action == "HOLD"; }
    
    /**
     * @brief Get formatted timestamp string
     * @return Formatted time string in MM/DD HH:MM:SS format (UTC)
     */
    std::string get_formatted_time() const;
    
    /**
     * @brief Check if this is a profitable trade
     * @return True if realized P&L is positive
     */
    bool is_profitable() const { return realized_pnl > 0.0; }
};

/**
 * @brief Parses trade events from JSON data
 * 
 * This class follows SRP by focusing solely on parsing/conversion logic.
 * Separated from TradeEvent to avoid mixing data representation with parsing.
 */
class TradeEventParser {
public:
    /**
     * @brief Parse a single trade event from JSON string
     * @param json_line JSON line from trade book
     * @return Parsed TradeEvent
     */
    static TradeEvent parse_from_json(const std::string& json_line);
    
    /**
     * @brief Parse multiple trade events from JSON lines
     * @param json_lines Vector of JSON strings
     * @return Vector of parsed TradeEvents
     */
    static std::vector<TradeEvent> parse_from_json_lines(const std::vector<std::string>& json_lines);
    
    /**
     * @brief Load and parse trade events from file
     * @param filename Path to trade book file
     * @return Vector of parsed TradeEvents
     */
    static std::vector<TradeEvent> load_from_file(const std::string& filename);

private:
    /**
     * @brief Safe double parsing with default value
     * @param value String value to parse
     * @param default_value Default value if parsing fails
     * @return Parsed double or default
     */
    static double safe_parse_double(const std::string& value, double default_value = 0.0);
};

/**
 * @brief Formats trade events for display
 * 
 * This class follows SRP by focusing solely on formatting/display logic.
 * Separated from TradeEvent to avoid mixing data with presentation.
 */
class TradeEventFormatter {
public:
    /**
     * @brief Format trade event for console display
     * @param event Trade event to format
     * @return Formatted string
     */
    static std::string format_for_console(const TradeEvent& event);
    
    /**
     * @brief Format multiple trade events as a table
     * @param events Vector of trade events
     * @param max_count Maximum number of events to display
     * @return Formatted table string
     */
    static std::string format_as_table(const std::vector<TradeEvent>& events, size_t max_count = 20);
    
    /**
     * @brief Format time from milliseconds timestamp
     * @param timestamp_ms Timestamp in milliseconds
     * @return Formatted time string (MM/DD HH:MM:SS UTC)
     */
    static std::string format_timestamp(long long timestamp_ms);
};

/**
 * @brief Analyzes collections of trade events
 * 
 * This class follows SRP by focusing solely on analysis/aggregation logic.
 */
class TradeEventAnalyzer {
public:
    struct TradeStatistics {
        size_t total_trades = 0;
        size_t buy_trades = 0;
        size_t sell_trades = 0;
        size_t hold_trades = 0;
        double total_realized_pnl = 0.0;
        double total_trade_value = 0.0;
        double final_equity = 0.0;
        double starting_equity = 0.0;
        long long first_trade_time = 0;
        long long last_trade_time = 0;
    };
    
    /**
     * @brief Analyze a collection of trade events
     * @param events Vector of trade events to analyze
     * @return Trade statistics
     */
    static TradeStatistics analyze(const std::vector<TradeEvent>& events);
    
    /**
     * @brief Get the most recent N trades
     * @param events Vector of trade events (assumed chronologically ordered)
     * @param count Number of recent trades to return
     * @return Vector of most recent trades
     */
    static std::vector<TradeEvent> get_recent_trades(const std::vector<TradeEvent>& events, size_t count);
};

} // namespace sentio
