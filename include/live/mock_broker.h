#ifndef SENTIO_MOCK_BROKER_H
#define SENTIO_MOCK_BROKER_H

#include "live/broker_client_interface.h"
#include "live/position_book.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <random>
#include <memory>
#include <chrono>

namespace sentio {

/**
 * Market Impact Model
 *
 * Simulates realistic slippage and price impact based on:
 * - Order size relative to average volume
 * - Temporary vs permanent impact
 * - Bid-ask spread
 */
struct MarketImpactModel {
    double temporary_impact_bps = 5.0;  // 5 bps temporary impact
    double permanent_impact_bps = 2.0;  // 2 bps permanent impact
    double bid_ask_spread_bps = 2.0;    // 2 bps spread

    /**
     * Calculate realistic fill price with market impact
     *
     * @param base_price Current market price
     * @param quantity Order quantity (positive = buy, negative = sell)
     * @param avg_volume Average daily volume
     * @return Adjusted fill price including impact
     */
    double calculate_fill_price(double base_price, double quantity, double avg_volume) const {
        double abs_qty = std::abs(quantity);
        double participation_rate = abs_qty / avg_volume;

        // Square-root impact model (standard in literature)
        double impact_bps = temporary_impact_bps * std::sqrt(participation_rate);

        // Add bid-ask spread (pay offer when buying, hit bid when selling)
        double spread_cost = bid_ask_spread_bps / 2.0;

        double total_impact_bps = impact_bps + spread_cost;

        // Apply impact (positive for buys, negative for sells)
        double impact_multiplier = 1.0 + (quantity > 0 ? 1 : -1) * total_impact_bps / 10000.0;

        return base_price * impact_multiplier;
    }
};

/**
 * Mock Broker Client
 *
 * Simulates realistic broker behavior for testing:
 * - Order fills with configurable delays
 * - Market impact and slippage
 * - Partial fills
 * - Portfolio tracking
 * - Commission simulation
 */
class MockBroker : public IBrokerClient {
public:
    /**
     * Constructor
     *
     * @param initial_cash Starting capital
     * @param commission_per_share Commission rate (default: $0)
     */
    explicit MockBroker(double initial_cash = 100000.0, double commission_per_share = 0.0);

    ~MockBroker() override = default;

    // IBrokerClient interface implementation
    void set_execution_callback(ExecutionCallback cb) override;
    void set_fill_behavior(FillBehavior behavior) override;
    std::optional<AccountInfo> get_account() override;
    std::vector<BrokerPosition> get_positions() override;
    std::optional<BrokerPosition> get_position(const std::string& symbol) override;
    std::optional<Order> place_market_order(const std::string& symbol,
                                           double quantity,
                                           const std::string& time_in_force = "gtc") override;
    bool close_position(const std::string& symbol) override;
    bool close_all_positions() override;
    std::optional<Order> get_order(const std::string& order_id) override;
    bool cancel_order(const std::string& order_id) override;
    std::vector<Order> get_open_orders() override;
    bool cancel_all_orders() override;
    bool is_market_open() override;

    // Mock-specific methods

    /**
     * Update market prices for symbols (needed for position valuation)
     */
    void update_market_price(const std::string& symbol, double price);

    /**
     * Set average volume for symbol (for market impact calculation)
     */
    void set_avg_volume(const std::string& symbol, double avg_volume);

    /**
     * Process pending orders (called by mock session)
     */
    void process_pending_orders();

    /**
     * Get total portfolio value
     */
    double get_portfolio_value() const;

    /**
     * Get performance metrics
     */
    struct PerformanceMetrics {
        double total_commission_paid = 0.0;
        double total_slippage = 0.0;
        int total_orders = 0;
        int filled_orders = 0;
        int partial_fills = 0;
    };

    PerformanceMetrics get_performance_metrics() const;

private:
    // Account state
    double cash_;
    double initial_cash_;
    std::string account_number_;

    // Positions: symbol -> quantity
    std::map<std::string, double> positions_;
    std::map<std::string, double> avg_entry_prices_;

    // Market data
    std::map<std::string, double> market_prices_;
    std::map<std::string, double> avg_volumes_;

    // Orders
    std::map<std::string, Order> orders_;
    std::vector<std::string> pending_orders_;
    int next_order_id_;

    // Configuration
    double commission_per_share_;
    FillBehavior fill_behavior_;
    MarketImpactModel impact_model_;
    ExecutionCallback execution_callback_;

    // Performance tracking
    PerformanceMetrics metrics_;

    // Random number generation for realistic fills
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;

    // Helper methods
    std::string generate_order_id();
    void execute_order(Order& order);
    void update_position(const std::string& symbol, double quantity, double price);
    double calculate_position_value(const std::string& symbol) const;
    double calculate_unrealized_pnl(const std::string& symbol) const;
};

} // namespace sentio

#endif // SENTIO_MOCK_BROKER_H
