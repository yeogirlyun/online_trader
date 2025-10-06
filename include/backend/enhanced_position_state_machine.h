// File: include/backend/enhanced_position_state_machine.h
#ifndef ENHANCED_POSITION_STATE_MACHINE_H
#define ENHANCED_POSITION_STATE_MACHINE_H

#include <memory>
#include <map>
#include <deque>
#include <unordered_map>
#include "backend/position_state_machine.h"
#include "backend/dynamic_hysteresis_manager.h"
#include "backend/dynamic_allocation_manager.h"
#include "strategy/signal_output.h"
#include "common/types.h"

namespace sentio {

// Forward declarations
struct MarketState;

// Enhanced version of PositionStateMachine with dynamic hysteresis
class EnhancedPositionStateMachine : public PositionStateMachine {
public:
    struct EnhancedConfig {
        bool enable_hysteresis = true;
        bool enable_dynamic_allocation = true;
        bool enable_adaptive_confidence = true;
        bool enable_regime_detection = true;
        bool log_threshold_changes = true;
        int bars_lookback = 20;
        
        // Position tracking
        bool track_bars_in_position = true;
        int max_bars_in_position = 100;  // Force re-evaluation after N bars
        
        // Performance tracking for adaptation
        bool track_performance = true;
        double performance_window = 50;  // Number of trades to track
    };
    
    struct EnhancedTransition : public StateTransition {
        // Additional fields for enhanced functionality
        backend::DynamicHysteresisManager::DynamicThresholds thresholds_used;
        backend::DynamicAllocationManager::AllocationResult allocation;
        int bars_in_current_position;
        double position_pnl;  // Current P&L if in position
        std::string regime;
        
        // Decision metadata
        double original_probability;
        double adjusted_probability;  // After hysteresis
        double original_confidence;
        double adjusted_confidence;   // After adaptation
    };
    
    // Constructors
    EnhancedPositionStateMachine(
        std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_mgr,
        std::shared_ptr<backend::DynamicAllocationManager> allocation_mgr,
        const EnhancedConfig& config
    );
    
    // Wrapper that delegates to enhanced functionality
    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions,
        double confidence_threshold = 0.7
    );
    
    // Enhanced version that returns more detailed transition info
    EnhancedTransition get_enhanced_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions
    );
    
    // Update signal history for hysteresis
    void update_signal_history(const SignalOutput& signal);
    
    // Track position duration
    void update_position_tracking(State new_state);
    int get_bars_in_position() const { return bars_in_position_; }
    
    // Performance tracking for adaptation
    void record_trade_result(double pnl, bool was_profitable);
    double get_recent_win_rate() const;
    double get_recent_avg_pnl() const;
    
    // Configuration
    void set_config(const EnhancedConfig& config) { config_ = config; }
    const EnhancedConfig& get_config() const { return config_; }
    
    // Get managers for external access
    std::shared_ptr<backend::DynamicHysteresisManager> get_hysteresis_manager() { return hysteresis_manager_; }
    std::shared_ptr<backend::DynamicAllocationManager> get_allocation_manager() { return allocation_manager_; }
    
protected:
    // Enhanced signal classification with dynamic thresholds
    SignalType classify_signal_with_hysteresis(
        const SignalOutput& signal,
        const backend::DynamicHysteresisManager::DynamicThresholds& thresholds
    ) const;
    
    // Adapt confidence based on recent performance
    double adapt_confidence(double original_confidence) const;
    
    // Check if transition should be forced due to position age
    bool should_force_transition(State current_state) const;
    
    // Helper to determine if state is a dual position state
    bool is_dual_state(State state) const;
    bool is_long_state(State state) const;
    bool is_short_state(State state) const;
    
    // Create enhanced transition with allocation info
    EnhancedTransition create_enhanced_transition(
        const StateTransition& base_transition,
        const SignalOutput& signal,
        const backend::DynamicHysteresisManager::DynamicThresholds& thresholds,
        double available_capital,
        const MarketState& market
    );
    
private:
    std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_manager_;
    std::shared_ptr<backend::DynamicAllocationManager> allocation_manager_;
    EnhancedConfig config_;
    
    // Position tracking
    State current_state_;
    State previous_state_;
    int bars_in_position_;
    int total_bars_processed_;
    
    // Transition statistics (for monitoring fix effectiveness)
    struct TransitionStats {
        uint32_t total_signals = 0;
        uint32_t transitions_triggered = 0;
        uint32_t short_signals = 0;
        uint32_t short_transitions = 0;
        uint32_t long_signals = 0;
        uint32_t long_transitions = 0;
    } stats_;
    
    // Performance tracking
    struct TradeResult {
        double pnl;
        bool profitable;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    
    // Market regime cache
    std::string current_regime_;
    int regime_bars_count_;
    
    // Helper to log threshold changes
    void log_threshold_changes(
        const backend::DynamicHysteresisManager::DynamicThresholds& old_thresholds,
        const backend::DynamicHysteresisManager::DynamicThresholds& new_thresholds
    ) const;
};

} // namespace sentio

#endif // ENHANCED_POSITION_STATE_MACHINE_H

