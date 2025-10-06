#include "backend/portfolio_manager.h"
#include "common/utils.h"

#include <numeric>
#include <algorithm>

namespace sentio {

PortfolioManager::PortfolioManager(double starting_capital)
    : cash_balance_(starting_capital), realized_pnl_(0.0) {}

bool PortfolioManager::can_buy(const std::string& /*symbol*/, double quantity, double price, double fees) {
    double required_capital = (quantity * price) + fees;
    return cash_balance_ >= required_capital;
}

bool PortfolioManager::can_sell(const std::string& symbol, double quantity) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return false;
    }
    return it->second.quantity >= quantity;
}

void PortfolioManager::execute_buy(const std::string& symbol, double quantity,
                                   double price, double fees) {
    double total_cost = (quantity * price) + fees;
    cash_balance_ -= total_cost;
    if (cash_balance_ < -1e-9) {
        utils::log_error("Negative cash after BUY: symbol=" + symbol +
                         ", qty=" + std::to_string(quantity) +
                         ", price=" + std::to_string(price) +
                         ", fees=" + std::to_string(fees) +
                         ", cash_balance=" + std::to_string(cash_balance_));
        std::abort();
    }
    update_position(symbol, quantity, price);
}

void PortfolioManager::execute_sell(const std::string& symbol, double quantity,
                                    double price, double fees) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return;
    }

    double proceeds = (quantity * price) - fees;
    cash_balance_ += proceeds;
    if (cash_balance_ < -1e-9) {
        utils::log_error("Negative cash after SELL: symbol=" + symbol +
                         ", qty=" + std::to_string(quantity) +
                         ", price=" + std::to_string(price) +
                         ", fees=" + std::to_string(fees) +
                         ", cash_balance=" + std::to_string(cash_balance_));
        std::abort();
    }

    // Realized P&L
    double gross_pnl = (price - it->second.avg_price) * quantity;
    realized_pnl_ += (gross_pnl - fees);

    // Update or remove position
    if (it->second.quantity > quantity) {
        it->second.quantity -= quantity;
    } else {
        positions_.erase(it);
    }
}

PortfolioState PortfolioManager::get_state() const {
    PortfolioState state;
    state.cash_balance = cash_balance_;
    state.total_equity = get_total_equity();
    state.unrealized_pnl = get_unrealized_pnl();
    state.realized_pnl = realized_pnl_;
    state.positions = positions_;
    state.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return state;
}

void PortfolioManager::update_market_prices(const std::map<std::string, double>& prices) {
    for (auto& [symbol, position] : positions_) {
        auto price_it = prices.find(symbol);
        if (price_it != prices.end()) {
            position.current_price = price_it->second;
            position.unrealized_pnl = (position.current_price - position.avg_price) * position.quantity;
        }
    }
}

double PortfolioManager::get_total_equity() const {
    double positions_value = 0.0;
    for (const auto& [symbol, position] : positions_) {
        positions_value += position.quantity * position.current_price;
    }
    return cash_balance_ + positions_value;
}

double PortfolioManager::get_unrealized_pnl() const {
    double total_unrealized = 0.0;
    for (const auto& [symbol, position] : positions_) {
        (void)symbol; // unused
        total_unrealized += position.unrealized_pnl;
    }
    return total_unrealized;
}

bool PortfolioManager::has_position(const std::string& symbol) const {
    return positions_.find(symbol) != positions_.end();
}

Position PortfolioManager::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        return it->second;
    }
    return Position{};
}

void PortfolioManager::update_position(const std::string& symbol,
                                       double quantity, double price) {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        // Average in
        double total_quantity = it->second.quantity + quantity;
        double total_value = (it->second.quantity * it->second.avg_price) + (quantity * price);
        it->second.avg_price = total_value / total_quantity;
        it->second.quantity = total_quantity;
        it->second.current_price = price;
    } else {
        Position pos;
        pos.symbol = symbol;
        pos.quantity = quantity;
        pos.avg_price = price;
        pos.current_price = price;
        pos.unrealized_pnl = 0.0;
        pos.realized_pnl = 0.0;
        positions_[symbol] = pos;
    }
}

// ------------------------- StaticPositionManager -----------------------------
StaticPositionManager::StaticPositionManager()
    : current_direction_(Direction::NEUTRAL), position_count_(0) {}

bool StaticPositionManager::would_cause_conflict(const std::string& symbol,
                                                 TradeAction action) {
    if (action == TradeAction::BUY) {
        bool is_inverse = is_inverse_etf(symbol);
        if (current_direction_ == Direction::SHORT && !is_inverse) {
            return true;
        }
        if (current_direction_ == Direction::LONG && is_inverse) {
            return true;
        }
    }
    return false;
}

void StaticPositionManager::update_direction(Direction new_direction) {
    if (position_count_ == 0) {
        current_direction_ = Direction::NEUTRAL;
    } else {
        current_direction_ = new_direction;
    }
}

void StaticPositionManager::add_position(const std::string& symbol) {
    symbol_positions_[symbol]++;
    position_count_++;
}

void StaticPositionManager::remove_position(const std::string& symbol) {
    auto it = symbol_positions_.find(symbol);
    if (it != symbol_positions_.end()) {
        it->second--;
        if (it->second <= 0) {
            symbol_positions_.erase(it);
        }
        position_count_--;
    }
}

bool StaticPositionManager::is_inverse_etf(const std::string& symbol) const {
    static const std::vector<std::string> inverse_etfs = {
        "PSQ", "SH", "SDS", "SPXS", "SQQQ", "QID", "DXD", "SDOW", "DOG"
    };
    return std::find(inverse_etfs.begin(), inverse_etfs.end(), symbol) != inverse_etfs.end();
}

} // namespace sentio


