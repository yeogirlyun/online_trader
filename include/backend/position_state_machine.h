#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

struct MarketState;

class PositionStateMachine {
public:
    enum class State {
        CASH_ONLY,      
        QQQ_ONLY,       
        TQQQ_ONLY,      
        PSQ_ONLY,       
        SQQQ_ONLY,      
        QQQ_TQQQ,       
        PSQ_SQQQ,       
        INVALID         
    };

    enum class SignalType {
        STRONG_BUY,     
        WEAK_BUY,       
        WEAK_SELL,      
        STRONG_SELL,    
        NEUTRAL         
    };

    struct StateTransition {
        State current_state;
        SignalType signal_type;
        State target_state;
        std::string optimal_action;
        std::string theoretical_basis;
        double expected_return = 0.0;      
        double risk_score = 0.0;           
        double confidence = 0.0;           
        
        // NEW: Multi-bar prediction support
        int prediction_horizon = 1;           // How many bars ahead this predicts
        uint64_t position_open_bar_id = 0;    // When position was opened
        uint64_t earliest_exit_bar_id = 0;    // When position can be closed
        bool is_hold_enforced = false;        // Currently in minimum hold period
        int bars_held = 0;                    // How many bars position has been held
        int bars_remaining = 0;               // Bars until hold period ends
    };
    
    struct PositionTracking {
        uint64_t open_bar_id;
        int horizon;
        double entry_price;
        std::string symbol;
    };

    PositionStateMachine();

    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    );

    std::pair<double, double> get_state_aware_thresholds(
        double base_buy_threshold,
        double base_sell_threshold,
        State current_state
    ) const;

    bool validate_transition(
        const StateTransition& transition,
        const PortfolioState& current_portfolio,
        double available_capital
    ) const;
    
    // NEW: Multi-bar support methods
    bool can_close_position(uint64_t current_bar_id, const std::string& symbol) const;
    void record_position_entry(const std::string& symbol, uint64_t bar_id, 
                              int horizon, double entry_price);
    void record_position_exit(const std::string& symbol);
    void clear_position_tracking();
    int get_bars_held(const std::string& symbol, uint64_t current_bar_id) const;
    int get_bars_remaining(const std::string& symbol, uint64_t current_bar_id) const;
    bool is_in_hold_period(const PortfolioState& portfolio, uint64_t current_bar_id) const;

    static std::string state_to_string(State s);
    static std::string signal_type_to_string(SignalType st);
    
    State determine_current_state(const PortfolioState& portfolio) const;

protected:
    StateTransition get_base_transition(State current, SignalType signal) const;

private:
    SignalType classify_signal(
        const SignalOutput& signal,
        double buy_threshold,
        double sell_threshold,
        double confidence_threshold = CONFIDENCE_THRESHOLD
    ) const;
    
    void initialize_transition_matrix();
    double apply_state_risk_adjustment(State state, double base_value) const;
    double calculate_kelly_position_size(
        double signal_probability,
        double expected_return,
        double risk_estimate,
        double available_capital
    ) const;
    
    void update_position_tracking(const SignalOutput& signal, 
                                 const StateTransition& transition);

    std::map<std::pair<State, SignalType>, StateTransition> transition_matrix_;
    
    // NEW: Multi-bar position tracking
    std::map<std::string, PositionTracking> position_tracking_;

    static constexpr double DEFAULT_BUY_THRESHOLD = 0.55;
    static constexpr double DEFAULT_SELL_THRESHOLD = 0.45;
    static constexpr double CONFIDENCE_THRESHOLD = 0.7;
    static constexpr double STRONG_MARGIN = 0.15;
    static constexpr double MAX_LEVERAGE_EXPOSURE = 0.8;
    static constexpr double MAX_POSITION_SIZE = 0.6;
    static constexpr double MIN_CASH_BUFFER = 0.1;
};

using PSM = PositionStateMachine;

} // namespace sentio