#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <optional>

namespace sentio::training {

/**
 * Position allocation for a given state
 * Represents the target portfolio composition
 */
struct PositionAllocation {
    struct Position {
        std::string instrument;  // e.g., "QQQ", "TQQQ", "PSQ", "SQQQ"
        double weight;          // Portfolio weight (0.0 to 1.0)
        double leverage;        // Leverage factor (1.0 for no leverage, 3.0 for TQQQ)
    };
    
    std::vector<Position> positions;
    double cash_weight;  // Remaining cash position (0.0 to 1.0)
    
    // Utility methods
    double get_total_weight() const {
        double total = cash_weight;
        for (const auto& pos : positions) {
            total += pos.weight;
        }
        return total;
    }
    
    bool is_valid() const {
        return std::abs(get_total_weight() - 1.0) < 1e-6;
    }
    
    double get_effective_leverage() const {
        double leverage = 0.0;
        for (const auto& pos : positions) {
            leverage += pos.weight * pos.leverage;
        }
        return leverage;
    }
};

/**
 * Abstract interface for state machines used in training
 * Ensures consistency between training and inference architectures
 */
class AbstractStateMachine {
public:
    virtual ~AbstractStateMachine() = default;
    
    // Core state information
    virtual int get_num_states() const = 0;
    virtual int get_initial_state() const { return 0; }  // Default to FLAT/HOLD
    
    // State allocation and naming
    virtual PositionAllocation get_allocation(int state_id) const = 0;
    virtual std::string get_state_name(int state_id) const = 0;
    virtual std::string get_state_description(int state_id) const = 0;
    
    // State transitions
    virtual std::vector<int> get_valid_transitions(int from_state) const = 0;
    virtual bool is_valid_transition(int from_state, int to_state) const = 0;
    virtual double get_transition_cost(int from_state, int to_state) const = 0;
    
    // Risk management
    virtual double get_state_risk_level(int state_id) const = 0;
    virtual bool is_high_risk_state(int state_id) const = 0;
    
    // Utility methods
    virtual std::vector<std::string> get_required_instruments() const = 0;
    virtual bool requires_leverage() const = 0;
    
    // State categorization
    enum class StateCategory {
        NEUTRAL,
        LONG,
        SHORT
    };
    virtual StateCategory get_state_category(int state_id) const = 0;
    
    // Validation
    virtual bool validate_state_machine() const {
        int num_states = get_num_states();
        if (num_states <= 0) return false;
        
        // Check all states have valid allocations
        for (int i = 0; i < num_states; ++i) {
            auto allocation = get_allocation(i);
            if (!allocation.is_valid()) return false;
            
            // Check valid transitions exist
            auto transitions = get_valid_transitions(i);
            if (transitions.empty() && i != get_initial_state()) return false;
        }
        
        return true;
    }
    
    // Helper to get transition matrix
    virtual std::vector<std::vector<bool>> get_transition_matrix() const {
        int n = get_num_states();
        std::vector<std::vector<bool>> matrix(n, std::vector<bool>(n, false));
        
        for (int i = 0; i < n; ++i) {
            auto valid_transitions = get_valid_transitions(i);
            for (int j : valid_transitions) {
                matrix[i][j] = true;
            }
        }
        
        return matrix;
    }
};

/**
 * Factory for creating state machines based on training mode
 */
class StateMachineFactory {
public:
    enum class StateMachineType {
        SIMPLE_PPO,      // 5 states: SimplePPOStrategy compatible
        LEVERAGED_PPO,   // 11 states: LeveragedPPOStrategy compatible
        LEGACY           // 3 actions: Backward compatibility
    };
    
    static std::unique_ptr<AbstractStateMachine> create(StateMachineType type);
    static std::string get_type_name(StateMachineType type);
    static int get_expected_states(StateMachineType type);
};

} // namespace sentio::training

