#ifndef SENTIO_BROKER_CLIENT_INTERFACE_H
#define SENTIO_BROKER_CLIENT_INTERFACE_H

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <map>

namespace sentio {

/**
 * Fill behavior for realistic order simulation
 */
enum class FillBehavior {
    IMMEDIATE_FULL,     // Unrealistic but fast (instant full fill)
    DELAYED_FULL,       // Realistic delay, full fill
    DELAYED_PARTIAL     // Most realistic with partial fills
};

// Forward declarations - actual definitions in position_book.h
struct ExecutionReport;
struct BrokerPosition;

/**
 * Account information
 */
struct AccountInfo {
    std::string account_number;
    double buying_power;
    double cash;
    double portfolio_value;
    double equity;
    double last_equity;
    bool pattern_day_trader;
    bool trading_blocked;
    bool account_blocked;
};

/**
 * Order structure
 */
struct Order {
    std::string symbol;
    double quantity;
    std::string side;          // "buy" or "sell"
    std::string type;          // "market", "limit", etc.
    std::string time_in_force; // "day", "gtc", "ioc", "fok"
    std::optional<double> limit_price;

    // Response fields
    std::string order_id;
    std::string status;        // "new", "filled", "canceled", etc.
    double filled_qty;
    double filled_avg_price;
};

/**
 * Broker Client Interface
 *
 * Polymorphic interface for broker operations.
 * Allows substitution of AlpacaClient with MockBroker without
 * modifying LiveTradeCommand logic.
 */
class IBrokerClient {
public:
    virtual ~IBrokerClient() = default;

    // Execution callback for realistic async fills
    using ExecutionCallback = std::function<void(const ExecutionReport&)>;

    /**
     * Set callback for execution reports (async fills)
     */
    virtual void set_execution_callback(ExecutionCallback cb) = 0;

    /**
     * Set fill behavior for order simulation (mock only)
     */
    virtual void set_fill_behavior(FillBehavior behavior) = 0;

    /**
     * Get account information
     */
    virtual std::optional<AccountInfo> get_account() = 0;

    /**
     * Get all open positions
     */
    virtual std::vector<BrokerPosition> get_positions() = 0;

    /**
     * Get position for specific symbol
     */
    virtual std::optional<BrokerPosition> get_position(const std::string& symbol) = 0;

    /**
     * Place a market order
     */
    virtual std::optional<Order> place_market_order(
        const std::string& symbol,
        double quantity,
        const std::string& time_in_force = "gtc") = 0;

    /**
     * Close position for a symbol
     */
    virtual bool close_position(const std::string& symbol) = 0;

    /**
     * Close all positions
     */
    virtual bool close_all_positions() = 0;

    /**
     * Get order by ID
     */
    virtual std::optional<Order> get_order(const std::string& order_id) = 0;

    /**
     * Cancel order by ID
     */
    virtual bool cancel_order(const std::string& order_id) = 0;

    /**
     * Get all open orders
     */
    virtual std::vector<Order> get_open_orders() = 0;

    /**
     * Cancel all open orders (idempotent)
     */
    virtual bool cancel_all_orders() = 0;

    /**
     * Check if market is open
     */
    virtual bool is_market_open() = 0;
};

} // namespace sentio

#endif // SENTIO_BROKER_CLIENT_INTERFACE_H
