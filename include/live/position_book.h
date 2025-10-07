#pragma once

#include "common/types.h"
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace sentio {

struct BrokerPosition {
    std::string symbol;
    int64_t qty{0};
    double avg_entry_price{0.0};
    double unrealized_pnl{0.0};
    double current_price{0.0};

    bool is_flat() const { return qty == 0; }
};

struct ExecutionReport {
    std::string order_id;
    std::string client_order_id;
    std::string symbol;
    std::string side;  // "buy" or "sell"
    int64_t filled_qty{0};
    double avg_fill_price{0.0};
    std::string status;  // "filled", "partial_fill", "pending", etc.
    uint64_t timestamp{0};
};

struct ReconcileResult {
    double realized_pnl{0.0};
    int64_t filled_qty{0};
    bool flat{false};
    std::string status;
};

/**
 * @brief Position book that tracks positions and reconciles with broker
 *
 * This class maintains local position state and provides reconciliation
 * against broker truth to detect position drift.
 */
class PositionBook {
public:
    PositionBook() = default;

    /**
     * @brief Update position from execution report
     * @param exec Execution report from broker
     */
    void on_execution(const ExecutionReport& exec);

    /**
     * @brief Get current position for symbol
     * @param symbol Symbol to query
     * @return BrokerPosition (returns flat position if symbol not found)
     */
    BrokerPosition get_position(const std::string& symbol) const;

    /**
     * @brief Reconcile local positions against broker truth
     * @param broker_positions Positions from broker API
     * @throws PositionReconciliationError if drift detected
     */
    void reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions);

    /**
     * @brief Get all non-flat positions
     * @return Map of symbol -> position
     */
    std::map<std::string, BrokerPosition> get_all_positions() const;

    /**
     * @brief Get total realized P&L since timestamp
     * @param since_ts Unix timestamp in microseconds
     * @return Realized P&L in dollars
     */
    double get_realized_pnl_since(uint64_t since_ts) const;

    /**
     * @brief Get total realized P&L today
     * @return Realized P&L in dollars
     */
    double get_total_realized_pnl() const { return total_realized_pnl_; }

    /**
     * @brief Reset daily P&L (call at market open)
     */
    void reset_daily_pnl() { total_realized_pnl_ = 0.0; }

    /**
     * @brief Update current market prices for unrealized P&L calculation
     * @param symbol Symbol
     * @param price Current market price
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * @brief Set position directly (for startup reconciliation)
     * @param symbol Symbol
     * @param qty Quantity
     * @param avg_price Average entry price
     */
    void set_position(const std::string& symbol, int64_t qty, double avg_price);

private:
    std::map<std::string, BrokerPosition> positions_;
    std::vector<ExecutionReport> execution_history_;
    double total_realized_pnl_{0.0};

    void update_position_on_fill(const ExecutionReport& exec);
    double calculate_realized_pnl(const BrokerPosition& old_pos, const ExecutionReport& exec);
};

} // namespace sentio
