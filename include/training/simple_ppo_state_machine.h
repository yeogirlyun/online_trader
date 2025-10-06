#pragma once

#include "abstract_state_machine.h"
#include <unordered_map>

namespace sentio::training {

/**
 * SimplePPO State Machine - 5 states with conservative positioning
 * Maps directly to SimplePPOStrategy::SimpleStateType
 */
class SimplePPOStateMachine : public AbstractStateMachine {
public:
    // State enumeration matching SimplePPOStrategy
    enum class SimpleStateType {
        FLAT = 0,           // 100% cash
        LIGHT_LONG = 1,     // 30% QQQ, 70% cash
        MODERATE_LONG = 2,  // 60% QQQ, 40% cash  
        LIGHT_SHORT = 3,    // 30% PSQ, 70% cash
        MODERATE_SHORT = 4  // 60% PSQ, 40% cash
    };
    
    SimplePPOStateMachine();
    ~SimplePPOStateMachine() override = default;
    
    // Core state information
    int get_num_states() const override { return 5; }
    int get_initial_state() const override { return static_cast<int>(SimpleStateType::FLAT); }
    
    // State allocation and naming
    PositionAllocation get_allocation(int state_id) const override;
    std::string get_state_name(int state_id) const override;
    std::string get_state_description(int state_id) const override;
    
    // State transitions
    std::vector<int> get_valid_transitions(int from_state) const override;
    bool is_valid_transition(int from_state, int to_state) const override;
    double get_transition_cost(int from_state, int to_state) const override;
    
    // Risk management
    double get_state_risk_level(int state_id) const override;
    bool is_high_risk_state(int state_id) const override;
    
    // Utility methods
    std::vector<std::string> get_required_instruments() const override {
        return {"QQQ", "PSQ"};
    }
    
    bool requires_leverage() const override { return false; }
    
    // State categorization
    StateCategory get_state_category(int state_id) const override;
    
private:
    // Transition rules
    void initialize_transition_rules();
    
    // Internal state mappings
    std::unordered_map<int, std::vector<int>> valid_transitions_;
    std::unordered_map<int, PositionAllocation> state_allocations_;
    std::unordered_map<int, std::string> state_names_;
    std::unordered_map<int, std::string> state_descriptions_;
    
    // Helper to convert state ID to enum
    SimpleStateType id_to_state(int state_id) const {
        return static_cast<SimpleStateType>(state_id);
    }
};

} // namespace sentio::training

