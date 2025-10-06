#pragma once

#include "abstract_state_machine.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace sentio::training {

/**
 * LeveragedPPO State Machine - 11 states with full leverage capability
 * Maps directly to TradingStateType from trading_state.h
 */
class LeveragedPPOStateMachine : public AbstractStateMachine {
public:
    // State enumeration matching TradingStateType
    enum class TradingStateType {
        FLAT = 0,           // All cash, no positions
        
        // Long states (5 states)
        LIGHT_LONG = 1,     // 30% QQQ, 70% cash
        MODERATE_LONG = 2,  // 60% QQQ, 40% cash  
        HEAVY_LONG = 3,     // 90% QQQ, 10% cash
        LEVERAGED_LONG = 4, // 40% QQQ, 50% TQQQ, 10% cash
        MAX_LONG = 5,       // 100% TQQQ (3x leverage)
        
        // Short states (5 states)
        LIGHT_SHORT = 6,    // 30% PSQ, 70% cash
        MODERATE_SHORT = 7, // 60% PSQ, 40% cash
        HEAVY_SHORT = 8,    // 90% PSQ, 10% cash
        LEVERAGED_SHORT = 9,// 40% PSQ, 50% SQQQ, 10% cash
        MAX_SHORT = 10      // 100% SQQQ (3x short leverage)
    };
    
    LeveragedPPOStateMachine();
    ~LeveragedPPOStateMachine() override = default;
    
    // Core state information
    int get_num_states() const override { return 11; }
    int get_initial_state() const override { return static_cast<int>(TradingStateType::FLAT); }
    
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
        return {"QQQ", "PSQ", "TQQQ", "SQQQ"};
    }
    
    bool requires_leverage() const override { return true; }
    
    // State categorization
    StateCategory get_state_category(int state_id) const override;
    
private:
    // Transition rules
    void initialize_transition_rules();
    void initialize_state_allocations();
    void initialize_state_metadata();
    
    // Internal state mappings
    std::unordered_map<int, std::vector<int>> valid_transitions_;
    std::unordered_map<int, PositionAllocation> state_allocations_;
    std::unordered_map<int, std::string> state_names_;
    std::unordered_map<int, std::string> state_descriptions_;
    std::unordered_map<int, double> state_risk_levels_;
    
    // Helper to convert state ID to enum
    TradingStateType id_to_state(int state_id) const {
        return static_cast<TradingStateType>(state_id);
    }
    
    // Calculate transition distance for cost
    int calculate_state_distance(int from_state, int to_state) const;
};

} // namespace sentio::training
