#include "live/mock_broker.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace sentio {

MockBroker::MockBroker(double initial_cash, double commission_per_share)
    : cash_(initial_cash)
    , initial_cash_(initial_cash)
    , account_number_("MOCK-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()))
    , next_order_id_(1000)
    , commission_per_share_(commission_per_share)
    , fill_behavior_(FillBehavior::IMMEDIATE_FULL)
    , execution_callback_(nullptr)
    , rng_(std::random_device{}())
    , dist_(0.0, 1.0)
{
}

void MockBroker::set_execution_callback(ExecutionCallback cb) {
    execution_callback_ = cb;
}

void MockBroker::set_fill_behavior(FillBehavior behavior) {
    fill_behavior_ = behavior;
}

std::optional<AccountInfo> MockBroker::get_account() {
    AccountInfo info;
    info.account_number = account_number_;
    info.cash = cash_;
    info.equity = get_portfolio_value();
    info.portfolio_value = info.equity;
    info.buying_power = cash_ * 2.0;  // Simulate 2x margin
    info.last_equity = info.equity;
    info.pattern_day_trader = false;
    info.trading_blocked = false;
    info.account_blocked = false;

    return info;
}

std::vector<BrokerPosition> MockBroker::get_positions() {
    std::vector<BrokerPosition> result;

    for (const auto& [symbol, qty] : positions_) {
        if (std::abs(qty) < 0.001) continue;  // Skip zero positions

        BrokerPosition pos;
        pos.symbol = symbol;
        pos.quantity = qty;
        pos.avg_entry_price = avg_entry_prices_[symbol];
        pos.current_price = market_prices_.count(symbol) ? market_prices_[symbol] : 0.0;
        pos.market_value = qty * pos.current_price;
        pos.unrealized_pl = calculate_unrealized_pnl(symbol);
        pos.unrealized_pl_pct = (pos.avg_entry_price > 0) ?
            pos.unrealized_pl / (std::abs(qty) * pos.avg_entry_price) : 0.0;

        result.push_back(pos);
    }

    return result;
}

std::optional<BrokerPosition> MockBroker::get_position(const std::string& symbol) {
    if (positions_.count(symbol) == 0 || std::abs(positions_[symbol]) < 0.001) {
        return std::nullopt;
    }

    auto positions = get_positions();
    for (const auto& pos : positions) {
        if (pos.symbol == symbol) {
            return pos;
        }
    }

    return std::nullopt;
}

std::optional<Order> MockBroker::place_market_order(
    const std::string& symbol,
    double quantity,
    const std::string& time_in_force) {

    Order order;
    order.symbol = symbol;
    order.quantity = quantity;
    order.side = quantity > 0 ? "buy" : "sell";
    order.type = "market";
    order.time_in_force = time_in_force;
    order.order_id = generate_order_id();
    order.status = "new";
    order.filled_qty = 0.0;
    order.filled_avg_price = 0.0;

    orders_[order.order_id] = order;
    metrics_.total_orders++;

    // Execute based on fill behavior
    if (fill_behavior_ == FillBehavior::IMMEDIATE_FULL) {
        execute_order(orders_[order.order_id]);
    } else {
        pending_orders_.push_back(order.order_id);
    }

    return orders_[order.order_id];
}

bool MockBroker::close_position(const std::string& symbol) {
    if (positions_.count(symbol) == 0 || std::abs(positions_[symbol]) < 0.001) {
        return true;  // Already flat
    }

    double qty = positions_[symbol];
    place_market_order(symbol, -qty, "gtc");

    return true;
}

bool MockBroker::close_all_positions() {
    for (const auto& [symbol, qty] : positions_) {
        if (std::abs(qty) >= 0.001) {
            close_position(symbol);
        }
    }
    return true;
}

std::optional<Order> MockBroker::get_order(const std::string& order_id) {
    if (orders_.count(order_id) == 0) {
        return std::nullopt;
    }
    return orders_[order_id];
}

bool MockBroker::cancel_order(const std::string& order_id) {
    if (orders_.count(order_id) == 0) {
        return false;
    }

    Order& order = orders_[order_id];
    if (order.status == "filled") {
        return false;  // Can't cancel filled order
    }

    order.status = "canceled";

    // Remove from pending
    pending_orders_.erase(
        std::remove(pending_orders_.begin(), pending_orders_.end(), order_id),
        pending_orders_.end());

    return true;
}

std::vector<Order> MockBroker::get_open_orders() {
    std::vector<Order> result;
    for (const auto& [id, order] : orders_) {
        if (order.status == "new" || order.status == "partially_filled") {
            result.push_back(order);
        }
    }
    return result;
}

bool MockBroker::cancel_all_orders() {
    auto open_orders = get_open_orders();
    for (const auto& order : open_orders) {
        cancel_order(order.order_id);
    }
    return true;
}

bool MockBroker::is_market_open() {
    return true;  // Mock always returns true
}

void MockBroker::update_market_price(const std::string& symbol, double price) {
    market_prices_[symbol] = price;
}

void MockBroker::set_avg_volume(const std::string& symbol, double avg_volume) {
    avg_volumes_[symbol] = avg_volume;
}

void MockBroker::process_pending_orders() {
    std::vector<std::string> to_remove;

    for (const auto& order_id : pending_orders_) {
        if (orders_.count(order_id) == 0) continue;

        Order& order = orders_[order_id];

        // Simulate fill delay based on behavior
        if (fill_behavior_ == FillBehavior::DELAYED_FULL) {
            // 50% chance to fill each call
            if (dist_(rng_) < 0.5) {
                execute_order(order);
                to_remove.push_back(order_id);
            }
        } else if (fill_behavior_ == FillBehavior::DELAYED_PARTIAL) {
            // Fill 30-70% of remaining quantity
            double fill_pct = 0.3 + dist_(rng_) * 0.4;
            double remaining_qty = order.quantity - order.filled_qty;
            double fill_qty = remaining_qty * fill_pct;

            if (std::abs(remaining_qty - fill_qty) < 0.001) {
                // Final fill
                execute_order(order);
                to_remove.push_back(order_id);
            } else {
                // Partial fill
                order.filled_qty += fill_qty;
                order.status = "partially_filled";
                metrics_.partial_fills++;

                // Execute partial
                double price = market_prices_[order.symbol];
                double avg_volume = avg_volumes_.count(order.symbol) ?
                    avg_volumes_[order.symbol] : 1000000.0;
                double fill_price = impact_model_.calculate_fill_price(
                    price, fill_qty, avg_volume);

                update_position(order.symbol, fill_qty, fill_price);

                // Commission
                double commission = commission_per_share_ * std::abs(fill_qty);
                cash_ -= commission;
                metrics_.total_commission_paid += commission;

                // Callback
                if (execution_callback_) {
                    ExecutionReport report;
                    report.order_id = order.order_id;
                    report.symbol = order.symbol;
                    report.side = order.side;
                    report.quantity = order.quantity;
                    report.filled_qty = fill_qty;
                    report.filled_avg_price = fill_price;
                    report.status = "partially_filled";
                    report.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    report.fill_type = "partial";

                    execution_callback_(report);
                }
            }
        }
    }

    // Remove filled orders
    for (const auto& order_id : to_remove) {
        pending_orders_.erase(
            std::remove(pending_orders_.begin(), pending_orders_.end(), order_id),
            pending_orders_.end());
    }
}

double MockBroker::get_portfolio_value() const {
    double total_value = cash_;

    for (const auto& [symbol, qty] : positions_) {
        if (market_prices_.count(symbol)) {
            total_value += qty * market_prices_.at(symbol);
        }
    }

    return total_value;
}

MockBroker::PerformanceMetrics MockBroker::get_performance_metrics() const {
    return metrics_;
}

std::string MockBroker::generate_order_id() {
    std::ostringstream oss;
    oss << "MOCK-" << std::setfill('0') << std::setw(8) << next_order_id_++;
    return oss.str();
}

void MockBroker::execute_order(Order& order) {
    if (market_prices_.count(order.symbol) == 0) {
        order.status = "rejected";
        return;
    }

    double price = market_prices_[order.symbol];
    double avg_volume = avg_volumes_.count(order.symbol) ?
        avg_volumes_[order.symbol] : 1000000.0;  // Default 1M volume

    // Calculate fill price with market impact
    double fill_price = impact_model_.calculate_fill_price(
        price, order.quantity, avg_volume);

    // Track slippage
    double slippage = (fill_price - price) * order.quantity;
    metrics_.total_slippage += slippage;

    // Calculate commission
    double commission = commission_per_share_ * std::abs(order.quantity);

    // Check if we have enough cash (for buys)
    if (order.quantity > 0) {
        double required_cash = order.quantity * fill_price + commission;
        if (required_cash > cash_) {
            order.status = "rejected";
            return;
        }
    }

    // Update position
    update_position(order.symbol, order.quantity, fill_price);

    // Update cash
    cash_ -= (order.quantity * fill_price + commission);
    metrics_.total_commission_paid += commission;

    // Update order
    order.filled_qty = order.quantity;
    order.filled_avg_price = fill_price;
    order.status = "filled";
    metrics_.filled_orders++;

    // Execution callback
    if (execution_callback_) {
        ExecutionReport report;
        report.order_id = order.order_id;
        report.symbol = order.symbol;
        report.side = order.side;
        report.quantity = order.quantity;
        report.filled_qty = order.quantity;
        report.filled_avg_price = fill_price;
        report.status = "filled";
        report.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        report.fill_type = "full";

        execution_callback_(report);
    }
}

void MockBroker::update_position(const std::string& symbol, double quantity, double price) {
    if (positions_.count(symbol) == 0) {
        positions_[symbol] = 0.0;
        avg_entry_prices_[symbol] = 0.0;
    }

    double old_qty = positions_[symbol];
    double old_avg = avg_entry_prices_[symbol];
    double new_qty = old_qty + quantity;

    if (std::abs(new_qty) < 0.001) {
        // Position closed
        positions_[symbol] = 0.0;
        avg_entry_prices_[symbol] = 0.0;
    } else if ((old_qty > 0 && quantity > 0) || (old_qty < 0 && quantity < 0)) {
        // Adding to position - update average price
        double total_cost = old_qty * old_avg + quantity * price;
        avg_entry_prices_[symbol] = total_cost / new_qty;
        positions_[symbol] = new_qty;
    } else {
        // Reducing or reversing position
        positions_[symbol] = new_qty;
        if (old_qty * new_qty < 0) {
            // Position reversed - new average is current price
            avg_entry_prices_[symbol] = price;
        }
        // If just reducing, keep old average
    }
}

double MockBroker::calculate_position_value(const std::string& symbol) const {
    if (positions_.count(symbol) == 0 || market_prices_.count(symbol) == 0) {
        return 0.0;
    }

    return positions_.at(symbol) * market_prices_.at(symbol);
}

double MockBroker::calculate_unrealized_pnl(const std::string& symbol) const {
    if (positions_.count(symbol) == 0 || market_prices_.count(symbol) == 0) {
        return 0.0;
    }

    double qty = positions_.at(symbol);
    double avg_entry = avg_entry_prices_.at(symbol);
    double current_price = market_prices_.at(symbol);

    return qty * (current_price - avg_entry);
}

} // namespace sentio
