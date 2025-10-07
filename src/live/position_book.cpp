#include "live/position_book.h"
#include "common/exceptions.h"
#include <cmath>
#include <sstream>
#include <iostream>

namespace sentio {

void PositionBook::on_execution(const ExecutionReport& exec) {
    execution_history_.push_back(exec);

    if (exec.filled_qty == 0) {
        return;  // No fill, nothing to update
    }

    auto& pos = positions_[exec.symbol];

    // Calculate realized P&L if reducing position
    double realized_pnl = calculate_realized_pnl(pos, exec);
    total_realized_pnl_ += realized_pnl;

    // Update position
    update_position_on_fill(exec);

    // Log update
    std::cout << "[PositionBook] " << exec.symbol
              << " qty=" << pos.qty
              << " avg_px=" << pos.avg_entry_price
              << " realized_pnl=" << realized_pnl << std::endl;
}

void PositionBook::update_position_on_fill(const ExecutionReport& exec) {
    auto& pos = positions_[exec.symbol];

    // Convert side to signed qty
    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    int64_t new_qty = pos.qty + fill_qty;

    if (pos.qty == 0) {
        // Opening new position
        pos.avg_entry_price = exec.avg_fill_price;
    } else if ((pos.qty > 0 && fill_qty > 0) || (pos.qty < 0 && fill_qty < 0)) {
        // Adding to position - update weighted average entry price
        double total_cost = pos.qty * pos.avg_entry_price +
                           fill_qty * exec.avg_fill_price;
        pos.avg_entry_price = total_cost / new_qty;
    }
    // If reducing/reversing, keep old avg_entry_price for P&L calculation

    pos.qty = new_qty;
    pos.symbol = exec.symbol;

    // Reset avg price when flat
    if (pos.qty == 0) {
        pos.avg_entry_price = 0.0;
        pos.unrealized_pnl = 0.0;
    }
}

double PositionBook::calculate_realized_pnl(const BrokerPosition& old_pos,
                                            const ExecutionReport& exec) {
    if (old_pos.qty == 0) {
        return 0.0;  // Opening position, no P&L
    }

    int64_t fill_qty = exec.filled_qty;
    if (exec.side == "sell") {
        fill_qty = -fill_qty;
    }

    // Only calculate P&L if reducing position
    if ((old_pos.qty > 0 && fill_qty >= 0) || (old_pos.qty < 0 && fill_qty <= 0)) {
        return 0.0;  // Adding to position
    }

    // Calculate how many shares we're closing
    int64_t closed_qty = std::min(std::abs(fill_qty), std::abs(old_pos.qty));

    // P&L per share = exit price - entry price
    double pnl_per_share = exec.avg_fill_price - old_pos.avg_entry_price;

    // For short positions, invert the P&L
    if (old_pos.qty < 0) {
        pnl_per_share = -pnl_per_share;
    }

    return closed_qty * pnl_per_share;
}

BrokerPosition PositionBook::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return BrokerPosition{.symbol = symbol};
    }
    return it->second;
}

void PositionBook::update_market_price(const std::string& symbol, double price) {
    auto it = positions_.find(symbol);
    if (it == positions_.end() || it->second.qty == 0) {
        return;  // No position, no unrealized P&L
    }

    auto& pos = it->second;
    pos.current_price = price;

    // Calculate unrealized P&L
    double pnl_per_share = price - pos.avg_entry_price;
    if (pos.qty < 0) {
        pnl_per_share = -pnl_per_share;  // Short position
    }
    pos.unrealized_pnl = std::abs(pos.qty) * pnl_per_share;
}

void PositionBook::reconcile_with_broker(const std::vector<BrokerPosition>& broker_positions) {
    std::cout << "[PositionBook] === Position Reconciliation ===" << std::endl;

    // Build broker position map
    std::map<std::string, BrokerPosition> broker_map;
    for (const auto& bp : broker_positions) {
        broker_map[bp.symbol] = bp;
    }

    // Check for discrepancies
    bool has_drift = false;

    // Check local positions against broker
    for (const auto& [symbol, local_pos] : positions_) {
        if (local_pos.qty == 0) continue;  // Skip flat positions

        auto bit = broker_map.find(symbol);

        if (bit == broker_map.end()) {
            std::cerr << "[PositionBook] DRIFT: Local has " << symbol
                     << " (" << local_pos.qty << "), broker has 0" << std::endl;
            has_drift = true;
        } else {
            const auto& broker_pos = bit->second;
            if (local_pos.qty != broker_pos.qty) {
                std::cerr << "[PositionBook] DRIFT: " << symbol
                         << " local=" << local_pos.qty
                         << " broker=" << broker_pos.qty << std::endl;
                has_drift = true;
            }
        }
    }

    // Check for positions broker has but we don't
    for (const auto& [symbol, broker_pos] : broker_map) {
        if (broker_pos.qty == 0) continue;

        auto lit = positions_.find(symbol);
        if (lit == positions_.end() || lit->second.qty == 0) {
            std::cerr << "[PositionBook] DRIFT: Broker has " << symbol
                     << " (" << broker_pos.qty << "), local has 0" << std::endl;
            has_drift = true;
        }
    }

    if (has_drift) {
        std::cerr << "[PositionBook] === POSITION DRIFT DETECTED ===" << std::endl;
        throw PositionReconciliationError("Position drift detected - local != broker");
    } else {
        std::cout << "[PositionBook] Position reconciliation: OK" << std::endl;
    }
}

double PositionBook::get_realized_pnl_since(uint64_t since_ts) const {
    double pnl = 0.0;
    for (const auto& exec : execution_history_) {
        if (exec.timestamp >= since_ts && exec.status == "filled") {
            // Note: This is simplified. In production, track per-exec P&L
            // For now, return total realized P&L
        }
    }
    return total_realized_pnl_;
}

std::map<std::string, BrokerPosition> PositionBook::get_all_positions() const {
    std::map<std::string, BrokerPosition> result;
    for (const auto& [symbol, pos] : positions_) {
        if (pos.qty != 0) {
            result[symbol] = pos;
        }
    }
    return result;
}

void PositionBook::set_position(const std::string& symbol, int64_t qty, double avg_price) {
    BrokerPosition pos;
    pos.symbol = symbol;
    pos.qty = qty;
    pos.avg_entry_price = avg_price;
    pos.current_price = avg_price;  // Will be updated on next price update
    pos.unrealized_pnl = 0.0;
    positions_[symbol] = pos;
}

} // namespace sentio
