#pragma once

// =============================================================================
// Module: backend/portfolio_manager.h
// Purpose: Portfolio accounting, position tracking, and basic P&L metrics.
//
// Core idea:
// - Encapsulate portfolio state transitions (buy/sell) and derived metrics.
// - No external I/O; callers provide prices and receive updated snapshots.
// =============================================================================

#include <map>
#include <memory>
#include <string>
#include "common/types.h"

namespace sentio {

class PortfolioManager {
public:
    explicit PortfolioManager(double starting_capital);

    // Portfolio operations
    bool can_buy(const std::string& symbol, double quantity, double price, double fees);
    bool can_sell(const std::string& symbol, double quantity);

    void execute_buy(const std::string& symbol, double quantity, double price, double fees);
    void execute_sell(const std::string& symbol, double quantity, double price, double fees);

    // State management
    PortfolioState get_state() const;
    void update_market_prices(const std::map<std::string, double>& prices);

    // Metrics
    double get_cash_balance() const { return cash_balance_; }
    double get_total_equity() const;
    double get_unrealized_pnl() const;
    double get_realized_pnl() const { return realized_pnl_; }

    // Position queries
    bool has_position(const std::string& symbol) const;
    Position get_position(const std::string& symbol) const;
    std::map<std::string, Position> get_all_positions() const { return positions_; }

private:
    double cash_balance_ = 0.0;
    double realized_pnl_ = 0.0;
    std::map<std::string, Position> positions_;

    void update_position(const std::string& symbol, double quantity, double price);
    void close_position(const std::string& symbol, double price);
};

// Static Position Manager for conflict prevention
class StaticPositionManager {
public:
    enum Direction {
        NEUTRAL = 0,
        LONG = 1,
        SHORT = -1
    };

    StaticPositionManager();

    // Conflict checking
    bool would_cause_conflict(const std::string& symbol, TradeAction action);
    void update_direction(Direction new_direction);

    // Position tracking
    void add_position(const std::string& symbol);
    void remove_position(const std::string& symbol);

    Direction get_direction() const { return current_direction_; }
    int get_position_count() const { return position_count_; }

private:
    Direction current_direction_;
    int position_count_;
    std::map<std::string, int> symbol_positions_;

    bool is_inverse_etf(const std::string& symbol) const;
};

} // namespace sentio


