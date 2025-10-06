// File: include/analysis/enhanced_performance_analyzer.h
#ifndef ENHANCED_PERFORMANCE_ANALYZER_H
#define ENHANCED_PERFORMANCE_ANALYZER_H

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "analysis/performance_analyzer.h"
#include "backend/enhanced_backend_component.h"
#include "backend/dynamic_hysteresis_manager.h"
#include "backend/dynamic_allocation_manager.h"
#include "strategy/signal_output.h"
#include "common/types.h"

namespace sentio::analysis {

// Enhanced performance configuration that integrates with Enhanced PSM
struct EnhancedPerformanceConfig {
    // Enhanced PSM settings (DEFAULT: all enabled)
    bool use_enhanced_psm = true;           // Use Enhanced PSM by default
    bool enable_hysteresis = true;          // Enable dynamic hysteresis
    bool enable_dynamic_allocation = true;  // Enable intelligent allocation
    
    // Hysteresis configuration
    backend::DynamicHysteresisManager::HysteresisConfig hysteresis_config;
    
    // Allocation configuration  
    backend::DynamicAllocationManager::AllocationConfig allocation_config;
    
    // Enhanced PSM configuration
    sentio::EnhancedPositionStateMachine::EnhancedConfig psm_config;
    
    // Risk management
    double max_position_value = 1000000.0;
    double max_portfolio_leverage = 3.0;
    double daily_loss_limit = 0.10;
    
    // Performance calculation settings
    double initial_capital = 100000.0;
    double transaction_cost = 0.0;     // Alpaca zero commission model
    bool include_slippage = false;     // No slippage for pure signal quality measurement
    double slippage_factor = 0.0;      // No slippage
    
    // Legacy fallback
    bool allow_legacy_simulation = true;    // For backward compatibility
    
    // Logging and debugging
    bool verbose_logging = false;
    bool track_state_transitions = true;
    bool save_trade_history = true;
};

// Enhanced performance analyzer that integrates with Enhanced PSM
class EnhancedPerformanceAnalyzer : public sentio::analysis::PerformanceAnalyzer {
public:
    // Constructor
    explicit EnhancedPerformanceAnalyzer(
        const EnhancedPerformanceConfig& config = EnhancedPerformanceConfig()
    );
    
    // Enhanced performance calculation methods
    struct EnhancedPerformanceMetrics {
        // Core metrics
        double total_return = 0.0;
        double trading_based_mrb = 0.0;
        double signal_based_mrb = 0.0;     // For comparison
        double sharpe_ratio = 0.0;
        double sortino_ratio = 0.0;
        double max_drawdown = 0.0;
        double calmar_ratio = 0.0;
        
        // Trading statistics
        int total_trades = 0;
        int winning_trades = 0;
        int losing_trades = 0;
        double win_rate = 0.0;
        double avg_win = 0.0;
        double avg_loss = 0.0;
        double profit_factor = 0.0;
        
        // Position statistics
        int total_positions = 0;
        int dual_positions = 0;
        int leveraged_positions = 0;
        double avg_position_duration = 0.0;
        
        // Risk metrics
        double value_at_risk = 0.0;
        double conditional_var = 0.0;
        double effective_leverage = 0.0;
        
        // Block-level metrics
        std::vector<double> block_returns;
        std::vector<double> block_mrbs;
        std::vector<int> block_trades;
        
        // State transition tracking
        std::map<std::string, int> state_transitions;
        std::map<std::string, double> state_pnl;
        
        // Hysteresis effectiveness
        int whipsaw_prevented = 0;
        double hysteresis_benefit = 0.0;
    };
    
    // Main calculation method - uses Enhanced PSM by default
    std::vector<double> calculate_block_mrbs(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks,
        bool use_enhanced_psm = true  // DEFAULT: true
    );
    
    // Calculate comprehensive metrics with Enhanced PSM
    EnhancedPerformanceMetrics calculate_enhanced_metrics(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks
    );
    
    // Calculate trading-based MRB using Enhanced PSM
    double calculate_trading_based_mrb(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks
    );
    
    // Batch process signals through Enhanced PSM
    std::vector<sentio::BackendComponent::TradeOrder> process_signals_with_enhanced_psm(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        size_t start_index = 0,
        size_t end_index = SIZE_MAX
    );
    
    // Configuration management
    void update_config(const EnhancedPerformanceConfig& config);
    const EnhancedPerformanceConfig& get_config() const { return config_; }
    
    // Validation and comparison methods
    bool validate_enhanced_results(const EnhancedPerformanceMetrics& metrics) const;
    double compare_with_legacy(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks
    );
    
    // State inspection for debugging
    struct SystemState {
        sentio::PositionStateMachine::State psm_state;
        double current_equity;
        std::map<std::string, double> positions;
        backend::DynamicHysteresisManager::DynamicThresholds thresholds;
        std::string market_regime;
    };
    
    SystemState get_current_state() const;
    
protected:
    // Enhanced PSM-based calculation
    std::vector<double> calculate_block_mrbs_with_enhanced_psm(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks
    );
    
    // Legacy calculation for backward compatibility
    std::vector<double> calculate_block_mrbs_legacy(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        int blocks
    );
    
    // Helper methods
    double calculate_return_from_trades(
        const std::vector<sentio::BackendComponent::TradeOrder>& trades,
        double initial_capital
    ) const;
    
    void calculate_risk_metrics(
        const std::vector<double>& returns,
        EnhancedPerformanceMetrics& metrics
    ) const;
    
    void track_state_transition(
        sentio::PositionStateMachine::State from,
        sentio::PositionStateMachine::State to,
        double pnl
    );
    
private:
    EnhancedPerformanceConfig config_;
    std::unique_ptr<sentio::EnhancedBackendComponent> enhanced_backend_;
    
    // State tracking
    SystemState current_state_;
    std::vector<sentio::BackendComponent::TradeOrder> trade_history_;
    std::map<std::string, int> state_transition_counts_;
    std::map<std::string, double> state_pnl_map_;
    
    // Performance tracking
    double current_equity_;
    double high_water_mark_;
    std::vector<double> equity_curve_;
    
    // Initialize Enhanced Backend
    void initialize_enhanced_backend();
    
    // Process a single block
    double process_block(
        const std::vector<SignalOutput>& signals,
        const std::vector<sentio::Bar>& market_data,
        size_t start_idx,
        size_t end_idx
    );
    
    // Calculate recent volatility
    double calculate_recent_volatility(
        const std::vector<sentio::Bar>& market_data,
        size_t current_index,
        size_t lookback_period = 20
    ) const;
};

} // namespace sentio::analysis

#endif // ENHANCED_PERFORMANCE_ANALYZER_H
