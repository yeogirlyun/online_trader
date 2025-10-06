#include "strategy/trading_state.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace sentio {
namespace strategy {

TradingState::TradingState() {
    initialize_states();
    initialize_transitions();
}

void TradingState::initialize_states() {
    // Define all valid trading states with their allocations
    // Format: PositionAllocation(cash, qqq, psq, tqqq, sqqq)
    
    // Neutral state
    allocations_[TradingStateType::FLAT] = 
        PositionAllocation(1.0, 0.0, 0.0, 0.0, 0.0);  // 100% cash
    state_names_[TradingStateType::FLAT] = "FLAT";
    
    // Long states (5 states)
    allocations_[TradingStateType::LIGHT_LONG] = 
        PositionAllocation(0.7, 0.3, 0.0, 0.0, 0.0);  // 30% QQQ, 70% cash
    state_names_[TradingStateType::LIGHT_LONG] = "LIGHT_LONG";
    
    allocations_[TradingStateType::MODERATE_LONG] = 
        PositionAllocation(0.4, 0.6, 0.0, 0.0, 0.0);  // 60% QQQ, 40% cash
    state_names_[TradingStateType::MODERATE_LONG] = "MODERATE_LONG";
    
    allocations_[TradingStateType::HEAVY_LONG] = 
        PositionAllocation(0.1, 0.9, 0.0, 0.0, 0.0);  // 90% QQQ, 10% cash
    state_names_[TradingStateType::HEAVY_LONG] = "HEAVY_LONG";
    
    allocations_[TradingStateType::LEVERAGED_LONG] = 
        PositionAllocation(0.1, 0.4, 0.0, 0.5, 0.0);  // 40% QQQ, 50% TQQQ, 10% cash
    state_names_[TradingStateType::LEVERAGED_LONG] = "LEVERAGED_LONG";
    
    allocations_[TradingStateType::MAX_LONG] = 
        PositionAllocation(0.0, 0.0, 0.0, 1.0, 0.0);  // 100% TQQQ (3x leverage)
    state_names_[TradingStateType::MAX_LONG] = "MAX_LONG";
    
    // Short states (5 states - balanced with long)
    allocations_[TradingStateType::LIGHT_SHORT] = 
        PositionAllocation(0.7, 0.0, 0.3, 0.0, 0.0);  // 30% PSQ, 70% cash
    state_names_[TradingStateType::LIGHT_SHORT] = "LIGHT_SHORT";
    
    allocations_[TradingStateType::MODERATE_SHORT] = 
        PositionAllocation(0.4, 0.0, 0.6, 0.0, 0.0);  // 60% PSQ, 40% cash
    state_names_[TradingStateType::MODERATE_SHORT] = "MODERATE_SHORT";
    
    allocations_[TradingStateType::HEAVY_SHORT] = 
        PositionAllocation(0.1, 0.0, 0.9, 0.0, 0.0);  // 90% PSQ, 10% cash
    state_names_[TradingStateType::HEAVY_SHORT] = "HEAVY_SHORT";
    
    allocations_[TradingStateType::LEVERAGED_SHORT] = 
        PositionAllocation(0.1, 0.0, 0.4, 0.0, 0.5);  // 40% PSQ, 50% SQQQ, 10% cash
    state_names_[TradingStateType::LEVERAGED_SHORT] = "LEVERAGED_SHORT";
    
    allocations_[TradingStateType::MAX_SHORT] = 
        PositionAllocation(0.0, 0.0, 0.0, 0.0, 1.0);  // 100% SQQQ (3x short leverage)
    state_names_[TradingStateType::MAX_SHORT] = "MAX_SHORT";
    
    // Validate all allocations
    for (const auto& [state, allocation] : allocations_) {
        if (!allocation.is_valid()) {
            utils::log_error("Invalid allocation for state: " + state_names_[state]);
        }
    }
}

void TradingState::initialize_transitions() {
    // Define valid state transitions (risk management constraints)
    
    // From FLAT: Can go to light positions or stay flat
    valid_transitions_[TradingStateType::FLAT] = {
        TradingStateType::FLAT,
        TradingStateType::LIGHT_LONG,
        TradingStateType::LIGHT_SHORT
    };
    
    // From LIGHT_LONG: Can reduce to flat, stay, or increase to moderate
    valid_transitions_[TradingStateType::LIGHT_LONG] = {
        TradingStateType::FLAT,
        TradingStateType::LIGHT_LONG,
        TradingStateType::MODERATE_LONG
    };
    
    // From MODERATE_LONG: Can reduce, stay, or increase to heavy
    valid_transitions_[TradingStateType::MODERATE_LONG] = {
        TradingStateType::LIGHT_LONG,
        TradingStateType::MODERATE_LONG,
        TradingStateType::HEAVY_LONG
    };
    
    // From HEAVY_LONG: Can reduce, stay, or add leverage
    valid_transitions_[TradingStateType::HEAVY_LONG] = {
        TradingStateType::MODERATE_LONG,
        TradingStateType::HEAVY_LONG,
        TradingStateType::LEVERAGED_LONG
    };
    
    // From LEVERAGED_LONG: Can reduce or go to max leverage
    valid_transitions_[TradingStateType::LEVERAGED_LONG] = {
        TradingStateType::HEAVY_LONG,
        TradingStateType::LEVERAGED_LONG,
        TradingStateType::MAX_LONG
    };
    
    // From MAX_LONG: Can only reduce leverage (risk management)
    valid_transitions_[TradingStateType::MAX_LONG] = {
        TradingStateType::LEVERAGED_LONG,
        TradingStateType::MAX_LONG
    };
    
    // From LIGHT_SHORT: Can go flat, stay, or increase to moderate
    valid_transitions_[TradingStateType::LIGHT_SHORT] = {
        TradingStateType::FLAT,
        TradingStateType::LIGHT_SHORT,
        TradingStateType::MODERATE_SHORT
    };
    
    // From MODERATE_SHORT: Can reduce, stay, or increase to heavy
    valid_transitions_[TradingStateType::MODERATE_SHORT] = {
        TradingStateType::LIGHT_SHORT,
        TradingStateType::MODERATE_SHORT,
        TradingStateType::HEAVY_SHORT
    };
    
    // From HEAVY_SHORT: Can reduce, stay, or add leverage
    valid_transitions_[TradingStateType::HEAVY_SHORT] = {
        TradingStateType::MODERATE_SHORT,
        TradingStateType::HEAVY_SHORT,
        TradingStateType::LEVERAGED_SHORT
    };
    
    // From LEVERAGED_SHORT: Can reduce or go to max leverage
    valid_transitions_[TradingStateType::LEVERAGED_SHORT] = {
        TradingStateType::HEAVY_SHORT,
        TradingStateType::LEVERAGED_SHORT,
        TradingStateType::MAX_SHORT
    };
    
    // From MAX_SHORT: Can only reduce leverage (risk management)
    valid_transitions_[TradingStateType::MAX_SHORT] = {
        TradingStateType::LEVERAGED_SHORT,
        TradingStateType::MAX_SHORT
    };
    
    // Add emergency exit to FLAT from any state (panic selling)
    for (auto& [state, transitions] : valid_transitions_) {
        if (state != TradingStateType::FLAT) {
            // Add FLAT if not already present (emergency exit)
            if (std::find(transitions.begin(), transitions.end(), TradingStateType::FLAT) == transitions.end()) {
                transitions.push_back(TradingStateType::FLAT);
            }
        }
    }
}

std::string TradingState::get_state_description(TradingStateType state) const {
    const auto& allocation = get_allocation(state);
    std::ostringstream oss;
    
    oss << get_name(state) << " [";
    if (allocation.cash > 0) oss << "Cash:" << (allocation.cash * 100) << "% ";
    if (allocation.qqq > 0) oss << "QQQ:" << (allocation.qqq * 100) << "% ";
    if (allocation.tqqq > 0) oss << "TQQQ:" << (allocation.tqqq * 100) << "% ";
    if (allocation.sqqq > 0) oss << "SQQQ:" << (allocation.sqqq * 100) << "% ";
    oss << "Leverage:" << std::fixed << std::setprecision(1) << allocation.effective_leverage() << "x]";
    
    return oss.str();
}

double TradingState::calculate_transition_reward(
    TradingStateType from_state,
    TradingStateType to_state,
    double resulting_pnl,
    double market_momentum,
    double volatility,
    double recent_loss
) const {
    double reward = 0.0;
    
    // Base reward: P&L from the transition
    reward += resulting_pnl;
    
    // Transaction costs (penalize frequent transitions)
    const auto& from_alloc = get_allocation(from_state);
    const auto& to_alloc = get_allocation(to_state);
    double transaction_cost = from_alloc.transition_cost_to(to_alloc);
    reward -= transaction_cost * 0.001; // Small transaction cost penalty
    
    // Market regime rewards
    double to_leverage = to_alloc.effective_leverage();
    double from_leverage = from_alloc.effective_leverage();
    
    // Reward aligning leverage with market momentum
    if (market_momentum > 0.02) { // Strong uptrend
        if (to_leverage > from_leverage) reward += 0.005; // Reward increasing long leverage
    } else if (market_momentum < -0.02) { // Strong downtrend  
        if (to_leverage < from_leverage) reward += 0.005; // Reward reducing long leverage or going short
    }
    
    // Penalize high leverage in high volatility
    if (volatility > 0.03 && std::abs(to_leverage) > 2.0) {
        reward -= 0.01; // Penalty for high leverage in volatile conditions
    }
    
    // Risk management rewards
    if (recent_loss > 0.02) { // If recent losses > 2%
        if (std::abs(to_leverage) < std::abs(from_leverage)) {
            reward += 0.008; // Reward for reducing risk after losses
        }
    }
    
    // Reward staying in profitable positions (momentum)
    if (resulting_pnl > 0 && to_state == from_state) {
        reward += 0.002; // Small reward for letting profits run
    }
    
    // Penalize whipsawing (rapid state changes)
    if (transaction_cost > 0.5) { // Large position change
        reward -= 0.005;
    }
    
    return reward;
}

// StateTransitionExecutor implementation

std::vector<StateTransitionOrder> StateTransitionExecutor::generate_transition_orders(
    TradingStateType from_state,
    TradingStateType to_state,
    const TradingState& state_machine
) {
    std::vector<StateTransitionOrder> orders;
    
    const auto& from_alloc = state_machine.get_allocation(from_state);
    const auto& to_alloc = state_machine.get_allocation(to_state);
    std::string reason = transition_reason(from_state, to_state);
    
    // Calculate position changes
    double qqq_change = to_alloc.qqq - from_alloc.qqq;
    double tqqq_change = to_alloc.tqqq - from_alloc.tqqq;
    double sqqq_change = to_alloc.sqqq - from_alloc.sqqq;
    
    // Generate orders (sell before buy to free up cash)
    const double MIN_TRADE_SIZE = 0.01; // 1% minimum
    
    // SELL orders first
    if (qqq_change < -MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::SELL, "QQQ", 
                           std::abs(qqq_change), reason);
    }
    if (tqqq_change < -MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::SELL, "TQQQ", 
                           std::abs(tqqq_change), reason);
    }
    if (sqqq_change < -MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::SELL, "SQQQ", 
                           std::abs(sqqq_change), reason);
    }
    
    // BUY orders after selling
    if (qqq_change > MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::BUY, "QQQ", 
                           qqq_change, reason);
    }
    if (tqqq_change > MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::BUY, "TQQQ", 
                           tqqq_change, reason);
    }
    if (sqqq_change > MIN_TRADE_SIZE) {
        orders.emplace_back(StateTransitionOrder::Type::BUY, "SQQQ", 
                           sqqq_change, reason);
    }
    
    return orders;
}

std::string StateTransitionExecutor::transition_reason(TradingStateType from_state, TradingStateType to_state) {
    if (from_state == to_state) {
        return "Hold position - PPO maintains current state";
    }
    
    // Determine direction and magnitude of change
    double from_lev = 0, to_lev = 0; // Simplified leverage comparison
    
    // This is a simplified version - in practice you'd use the actual allocations
    std::string direction = (static_cast<int>(to_state) > static_cast<int>(from_state)) 
                           ? "increase" : "reduce";
    
    return "PPO state transition: " + direction + " exposure";
}

} // namespace strategy  
} // namespace sentio
