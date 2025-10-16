#include "live/alpaca_client_adapter.h"

namespace sentio {

AlpacaClientAdapter::AlpacaClientAdapter(const std::string& api_key,
                                       const std::string& secret_key,
                                       bool paper_trading)
    : client_(std::make_unique<AlpacaClient>(api_key, secret_key, paper_trading))
    , execution_callback_(nullptr)
{
}

void AlpacaClientAdapter::set_execution_callback(ExecutionCallback cb) {
    execution_callback_ = cb;
    // Note: Alpaca client doesn't support async callbacks in current implementation
}

void AlpacaClientAdapter::set_fill_behavior(FillBehavior behavior) {
    // Not applicable for real broker - Alpaca determines fill behavior
}

std::optional<AccountInfo> AlpacaClientAdapter::get_account() {
    auto alpaca_acc = client_->get_account();
    if (!alpaca_acc.has_value()) {
        return std::nullopt;
    }
    return convert_account(alpaca_acc.value());
}

std::vector<BrokerPosition> AlpacaClientAdapter::get_positions() {
    auto alpaca_positions = client_->get_positions();
    std::vector<BrokerPosition> result;

    for (const auto& alpaca_pos : alpaca_positions) {
        result.push_back(convert_position(alpaca_pos));
    }

    return result;
}

std::optional<BrokerPosition> AlpacaClientAdapter::get_position(const std::string& symbol) {
    auto alpaca_pos = client_->get_position(symbol);
    if (!alpaca_pos.has_value()) {
        return std::nullopt;
    }
    return convert_position(alpaca_pos.value());
}

std::optional<Order> AlpacaClientAdapter::place_market_order(
    const std::string& symbol,
    double quantity,
    const std::string& time_in_force) {

    auto alpaca_order = client_->place_market_order(symbol, quantity, time_in_force);
    if (!alpaca_order.has_value()) {
        return std::nullopt;
    }

    Order order = convert_order(alpaca_order.value());

    // If callback is set, invoke it (simulate execution report)
    if (execution_callback_) {
        ExecutionReport report;
        report.order_id = order.order_id;
        report.symbol = order.symbol;
        report.side = order.side;
        report.quantity = order.quantity;
        report.filled_qty = order.filled_qty;
        report.filled_avg_price = order.filled_avg_price;
        report.status = order.status;
        report.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        report.fill_type = (order.filled_qty == order.quantity) ? "full" : "partial";

        execution_callback_(report);
    }

    return order;
}

bool AlpacaClientAdapter::close_position(const std::string& symbol) {
    return client_->close_position(symbol);
}

bool AlpacaClientAdapter::close_all_positions() {
    return client_->close_all_positions();
}

std::optional<Order> AlpacaClientAdapter::get_order(const std::string& order_id) {
    auto alpaca_order = client_->get_order(order_id);
    if (!alpaca_order.has_value()) {
        return std::nullopt;
    }
    return convert_order(alpaca_order.value());
}

bool AlpacaClientAdapter::cancel_order(const std::string& order_id) {
    return client_->cancel_order(order_id);
}

std::vector<Order> AlpacaClientAdapter::get_open_orders() {
    auto alpaca_orders = client_->get_open_orders();
    std::vector<Order> result;

    for (const auto& alpaca_order : alpaca_orders) {
        result.push_back(convert_order(alpaca_order));
    }

    return result;
}

bool AlpacaClientAdapter::cancel_all_orders() {
    return client_->cancel_all_orders();
}

bool AlpacaClientAdapter::is_market_open() {
    return client_->is_market_open();
}

// Helper conversion methods

BrokerPosition AlpacaClientAdapter::convert_position(const AlpacaClient::Position& alpaca_pos) {
    BrokerPosition pos;
    pos.symbol = alpaca_pos.symbol;
    pos.quantity = alpaca_pos.quantity;
    pos.avg_entry_price = alpaca_pos.avg_entry_price;
    pos.current_price = alpaca_pos.current_price;
    pos.market_value = alpaca_pos.market_value;
    pos.unrealized_pl = alpaca_pos.unrealized_pl;
    pos.unrealized_pl_pct = alpaca_pos.unrealized_pl_pct;
    return pos;
}

AccountInfo AlpacaClientAdapter::convert_account(const AlpacaClient::AccountInfo& alpaca_acc) {
    AccountInfo info;
    info.account_number = alpaca_acc.account_number;
    info.buying_power = alpaca_acc.buying_power;
    info.cash = alpaca_acc.cash;
    info.portfolio_value = alpaca_acc.portfolio_value;
    info.equity = alpaca_acc.equity;
    info.last_equity = alpaca_acc.last_equity;
    info.pattern_day_trader = alpaca_acc.pattern_day_trader;
    info.trading_blocked = alpaca_acc.trading_blocked;
    info.account_blocked = alpaca_acc.account_blocked;
    return info;
}

Order AlpacaClientAdapter::convert_order(const AlpacaClient::Order& alpaca_order) {
    Order order;
    order.symbol = alpaca_order.symbol;
    order.quantity = alpaca_order.quantity;
    order.side = alpaca_order.side;
    order.type = alpaca_order.type;
    order.time_in_force = alpaca_order.time_in_force;
    order.limit_price = alpaca_order.limit_price;
    order.order_id = alpaca_order.order_id;
    order.status = alpaca_order.status;
    order.filled_qty = alpaca_order.filled_qty;
    order.filled_avg_price = alpaca_order.filled_avg_price;
    return order;
}

} // namespace sentio
