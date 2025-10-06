#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace sentio {
namespace strategy {

/**
 * @brief Trading State Machine for Leveraged Position Management
 * 
 * Defines discrete states for portfolio allocation with predefined
 * transitions to enforce risk constraints and position sizing rules.
 * 
 * This state machine approach allows PPO to learn optimal transitions
 * between valid states rather than arbitrary position sizes.
 */

struct PositionAllocation {
    double cash;    // Percentage in cash (0.0 - 1.0)
    double qqq;     // Percentage in QQQ (1x long)
    double psq;     // Percentage in PSQ (1x short)
    double tqqq;    // Percentage in TQQQ (3x leveraged long)
    double sqqq;    // Percentage in SQQQ (3x leveraged short)
    
    PositionAllocation(double c = 1.0, double q = 0.0, double p = 0.0, double t = 0.0, double s = 0.0)
        : cash(c), qqq(q), psq(p), tqqq(t), sqqq(s) {}
    
    // Calculate effective leverage (considering 3x instruments and short positions)
    double effective_leverage() const {
        return qqq + (tqqq * 3.0) - psq - (sqqq * 3.0);
    }
    
    // Check if allocation is valid (sums to 1.0, no conflicting positions)
    bool is_valid() const {
        double total = cash + qqq + psq + tqqq + sqqq;
        return (std::abs(total - 1.0) < 1e-6) && 
               (cash >= 0) && (qqq >= 0) && (psq >= 0) && (tqqq >= 0) && (sqqq >= 0) &&
               (tqqq == 0 || sqqq == 0) &&  // Can't hold both 3x long and 3x short
               (qqq == 0 || psq == 0);      // Can't hold both 1x long and 1x short
    }
    
    // Calculate transition cost (for reward function)
    double transition_cost_to(const PositionAllocation& target) const {
        return std::abs(qqq - target.qqq) + 
               std::abs(psq - target.psq) +
               std::abs(tqqq - target.tqqq) + 
               std::abs(sqqq - target.sqqq);
    }
};

/**
 * @brief Enumeration of all valid trading states
 * 
 * 11 balanced states: 5 long, 1 neutral, 5 short
 * 4 instruments: QQQ (1x long), PSQ (1x short), TQQQ (3x long), SQQQ (3x short)
 */
enum class TradingStateType {
    // Neutral state
    FLAT = 0,           // All cash, no positions
    
    // Long states (5 states)
    LIGHT_LONG,         // Conservative long position (QQQ)
    MODERATE_LONG,      // Standard long position (QQQ)  
    HEAVY_LONG,         // Aggressive long position (QQQ)
    LEVERAGED_LONG,     // Mixed QQQ + TQQQ position
    MAX_LONG,           // Maximum leverage with TQQQ
    
    // Short states (5 states - balanced with long)
    LIGHT_SHORT,        // Conservative short position (PSQ)
    MODERATE_SHORT,     // Standard short position (PSQ)
    HEAVY_SHORT,        // Aggressive short position (PSQ)
    LEVERAGED_SHORT,    // Mixed PSQ + SQQQ position
    MAX_SHORT,          // Maximum leverage with SQQQ
    
    COUNT               // Total number of states (11)
};

/**
 * @brief Trading State Machine Implementation
 */
class TradingState {
public:
    TradingState();
    ~TradingState() = default;

    // State information
    static constexpr size_t NUM_STATES = static_cast<size_t>(TradingStateType::COUNT);
    
    // Get state allocation
    const PositionAllocation& get_allocation(TradingStateType state) const {
        return allocations_.at(state);
    }
    
    // Get state name
    const std::string& get_name(TradingStateType state) const {
        return state_names_.at(state);
    }
    
    // Get valid transitions from a state
    const std::vector<TradingStateType>& get_valid_transitions(TradingStateType from_state) const {
        return valid_transitions_.at(from_state);
    }
    
    // Check if transition is valid
    bool is_valid_transition(TradingStateType from_state, TradingStateType to_state) const {
        const auto& valid = valid_transitions_.at(from_state);
        return std::find(valid.begin(), valid.end(), to_state) != valid.end();
    }
    
    // Get state from index
    TradingStateType get_state_from_index(size_t index) const {
        return static_cast<TradingStateType>(index);
    }
    
    // Get index from state
    size_t get_index_from_state(TradingStateType state) const {
        return static_cast<size_t>(state);
    }
    
    // Get state description for logging
    std::string get_state_description(TradingStateType state) const;
    
    // Calculate transition reward (for PPO training)
    double calculate_transition_reward(
        TradingStateType from_state,
        TradingStateType to_state,
        double resulting_pnl,
        double market_momentum,
        double volatility,
        double recent_loss
    ) const;

private:
    void initialize_states();
    void initialize_transitions();
    
    // State definitions
    std::unordered_map<TradingStateType, PositionAllocation> allocations_;
    std::unordered_map<TradingStateType, std::string> state_names_;
    
    // Transition constraints  
    std::unordered_map<TradingStateType, std::vector<TradingStateType>> valid_transitions_;
};

/**
 * @brief Order generation from state transitions
 */
struct StateTransitionOrder {
    enum class Type { BUY, SELL };
    
    Type action;
    std::string symbol;
    double percentage;  // Percentage of portfolio
    std::string reason;
    
    StateTransitionOrder(Type a, const std::string& s, double p, const std::string& r)
        : action(a), symbol(s), percentage(p), reason(r) {}
};

/**
 * @brief Generate orders to execute state transition
 */
class StateTransitionExecutor {
public:
    static std::vector<StateTransitionOrder> generate_transition_orders(
        TradingStateType from_state,
        TradingStateType to_state,
        const TradingState& state_machine
    );
    
private:
    static std::string transition_reason(TradingStateType from_state, TradingStateType to_state);
};

} // namespace strategy
} // namespace sentio
