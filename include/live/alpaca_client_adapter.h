#ifndef SENTIO_ALPACA_CLIENT_ADAPTER_H
#define SENTIO_ALPACA_CLIENT_ADAPTER_H

#include "live/broker_client_interface.h"
#include "live/alpaca_client.hpp"
#include "live/position_book.h"
#include <memory>

namespace sentio {

/**
 * Alpaca Client Adapter
 *
 * Adapts existing AlpacaClient to IBrokerClient interface.
 * Provides minimal wrapper to enable polymorphic substitution.
 */
class AlpacaClientAdapter : public IBrokerClient {
public:
    /**
     * Constructor
     *
     * @param api_key Alpaca API key
     * @param secret_key Alpaca secret key
     * @param paper_trading Use paper trading endpoint
     */
    AlpacaClientAdapter(const std::string& api_key,
                       const std::string& secret_key,
                       bool paper_trading = true);

    ~AlpacaClientAdapter() override = default;

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

private:
    std::unique_ptr<AlpacaClient> client_;
    ExecutionCallback execution_callback_;

    // Helper to convert AlpacaClient types to interface types
    BrokerPosition convert_position(const AlpacaClient::Position& alpaca_pos);
    AccountInfo convert_account(const AlpacaClient::AccountInfo& alpaca_acc);
    Order convert_order(const AlpacaClient::Order& alpaca_order);
};

} // namespace sentio

#endif // SENTIO_ALPACA_CLIENT_ADAPTER_H
