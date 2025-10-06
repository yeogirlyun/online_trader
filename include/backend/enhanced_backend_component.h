// File: include/backend/enhanced_backend_component.h
#ifndef ENHANCED_BACKEND_COMPONENT_H
#define ENHANCED_BACKEND_COMPONENT_H

#include "backend/backend_component.h"
#include "backend/enhanced_position_state_machine.h"
#include "backend/dynamic_hysteresis_manager.h"
#include "backend/dynamic_allocation_manager.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <memory>
#include <vector>

namespace sentio {

// Enhanced backend component with dynamic PSM integration
class EnhancedBackendComponent : public BackendComponent {
public:
    struct EnhancedBackendConfig : public BackendConfig {
        // Dynamic PSM settings
        bool enable_dynamic_psm = true;
        bool enable_hysteresis = true;
        bool enable_dynamic_allocation = true;
        
        // Hysteresis configuration
        backend::DynamicHysteresisManager::HysteresisConfig hysteresis_config;
        
        // Allocation configuration
        backend::DynamicAllocationManager::AllocationConfig allocation_config;
        
        // Enhanced PSM configuration
        EnhancedPositionStateMachine::EnhancedConfig psm_config;
        
        // Risk management
        double max_position_value = 1000000.0;  // Maximum $ per position
        double max_portfolio_leverage = 3.0;    // Maximum effective leverage
        double daily_loss_limit = 0.10;         // 10% daily loss limit
        
        // Execution settings
        bool enable_partial_fills = false;
        bool enable_smart_routing = false;
        double slippage_factor = 0.0001;  // 1 basis point slippage
        
        // Monitoring and logging
        bool log_all_transitions = true;
        bool log_allocation_details = true;
        bool track_performance_metrics = true;
        int performance_report_frequency = 100;  // Report every N bars
        
        // NEW: Multi-bar prediction settings
        int default_prediction_horizon = 5;            // Default 5-bar predictions
        std::string signal_generation_mode = "ADAPTIVE"; // "EVERY_BAR" or "ADAPTIVE"
        int signal_generation_interval = 3;            // Generate signal every N bars
        bool enforce_minimum_hold = true;              // Enforce hold periods
        double early_exit_penalty = 0.02;              // 2% penalty for early exits
        
        // Position horizon tracking
        bool track_position_horizons = true;
        bool log_hold_enforcement = true;
        
        // Performance tracking for multi-bar
        bool track_horizon_performance = true;
        std::map<int, double> horizon_success_rates;   // horizon -> success rate
    };
    
    explicit EnhancedBackendComponent(const EnhancedBackendConfig& config);
    
    // Process signals with enhanced PSM
    std::vector<TradeOrder> process_signals_enhanced(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        size_t start_index = 0,
        size_t end_index = SIZE_MAX
    );

    // In-memory processing API for validation (no file I/O)
    struct InMemoryResult {
        double final_equity = 0.0;
        size_t trade_count = 0;
    };
    InMemoryResult process_in_memory(
        const std::vector<SignalOutput>& signals,
        const std::vector<Bar>& market_data,
        size_t start_index = 0,
        size_t end_index = SIZE_MAX
    );
    
    // File-based processing API (inherits from BackendComponent)
    using BackendComponent::process_to_jsonl;
    
    // Enhanced execution methods
    std::vector<TradeOrder> execute_dual_position_transition(
        const EnhancedPositionStateMachine::EnhancedTransition& transition,
        const SignalOutput& signal,
        const Bar& bar
    );
    
    std::vector<TradeOrder> execute_single_position_transition(
        const EnhancedPositionStateMachine::EnhancedTransition& transition,
        const SignalOutput& signal,
        const Bar& bar
    );
    
    // Risk management
    bool validate_risk_limits(const std::vector<TradeOrder>& orders);
    void apply_position_limits(std::vector<TradeOrder>& orders);
    void check_daily_loss_limit();
    
    // Performance tracking
    void record_transition_performance(
        const EnhancedPositionStateMachine::EnhancedTransition& transition,
        const std::vector<TradeOrder>& orders
    );
    
    void generate_performance_report();
    
    // NEW: Multi-bar horizon tracking
    void track_horizon_transition(
        const EnhancedPositionStateMachine::EnhancedTransition& transition,
        int prediction_horizon
    );
    
    // Getters for components
    std::shared_ptr<EnhancedPositionStateMachine> get_enhanced_psm() { 
        return enhanced_psm_; 
    }
    std::shared_ptr<backend::DynamicHysteresisManager> get_hysteresis_manager() {
        return hysteresis_manager_;
    }
    std::shared_ptr<backend::DynamicAllocationManager> get_allocation_manager() {
        return allocation_manager_;
    }
    
protected:
    // Helper methods
    void liquidate_positions_for_transition(
        const EnhancedPositionStateMachine::EnhancedTransition& transition,
        std::vector<TradeOrder>& orders,
        const Bar& bar
    );
    
    void execute_allocation_orders(
        const backend::DynamicAllocationManager::AllocationResult& allocation,
        std::vector<TradeOrder>& orders,
        const SignalOutput& signal,
        const Bar& bar
    );
    
    double calculate_position_pnl(
        const Position& position,
        double current_price
    ) const;
    
    void update_daily_pnl(double pnl);
    
    // Market data helpers
    double get_current_price(const std::string& symbol, const Bar& bar) const;
    double estimate_execution_price(
        const std::string& symbol,
        TradeAction action,
        double base_price
    ) const;
    
private:
    EnhancedBackendConfig enhanced_config_;
    std::shared_ptr<EnhancedPositionStateMachine> enhanced_psm_;
    std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_manager_;
    std::shared_ptr<backend::DynamicAllocationManager> allocation_manager_;
    
    // Performance tracking
    struct TransitionMetrics {
        int total_transitions = 0;
        int dual_state_transitions = 0;
        int single_state_transitions = 0;
        int hold_decisions = 0;
        double total_pnl = 0.0;
        double max_drawdown = 0.0;
        double sharpe_ratio = 0.0;
        std::vector<double> daily_returns;
    };
    
    TransitionMetrics metrics_;
    double daily_pnl_;
    double session_high_water_mark_;
    int bars_since_last_report_;
    
    // State tracking
    PositionStateMachine::State last_state_;
    int64_t last_transition_timestamp_;
    
    // Helper to format order details for logging
    std::string format_order_details(const TradeOrder& order) const;
    std::string format_allocation_summary(
        const backend::DynamicAllocationManager::AllocationResult& allocation
    ) const;
};

} // namespace sentio

#endif // ENHANCED_BACKEND_COMPONENT_H

