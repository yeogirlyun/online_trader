// File: include/backend/dynamic_allocation_manager.h
#ifndef DYNAMIC_ALLOCATION_MANAGER_H
#define DYNAMIC_ALLOCATION_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"

// Use sentio namespace types
using sentio::SignalOutput;
using sentio::MarketState;
using sentio::PositionStateMachine;

namespace backend {

class DynamicAllocationManager {
public:
    struct AllocationConfig {
        // Allocation strategy
        enum class Strategy {
            CONFIDENCE_BASED,    // Allocate based on signal confidence
            RISK_PARITY,         // Equal risk contribution
            KELLY_CRITERION,     // Optimal Kelly sizing
            HYBRID               // Combination of above
        } strategy = Strategy::CONFIDENCE_BASED;
        
        // Risk limits
        double max_leverage_allocation = 0.85;  // Max % in leveraged instrument
        double min_base_allocation = 0.10;      // Min % in base instrument
        double max_total_leverage = 3.0;        // Max effective portfolio leverage
        double min_total_allocation = 0.95;     // Min % of capital to allocate
        double max_total_allocation = 1.0;      // Max % of capital to allocate
        
        // Confidence-based parameters
        double confidence_power = 1.0;           // Confidence exponent (higher = more aggressive)
        double confidence_floor = 0.5;           // Minimum confidence for any leveraged allocation
        double confidence_ceiling = 0.95;        // Maximum confidence cap
        
        // Risk parity parameters
        double base_volatility = 0.15;          // Assumed annual vol for base instrument
        double leveraged_volatility = 0.45;     // Assumed annual vol for leveraged (3x base)
        
        // Kelly parameters
        double kelly_fraction = 0.25;           // Fraction of full Kelly to use (conservative)
        double expected_win_rate = 0.55;        // Expected probability of winning trades
        double avg_win_loss_ratio = 1.2;        // Average win size / average loss size
        
        // Advanced features
        bool enable_dynamic_adjustment = true;   // Adjust based on recent performance
        bool enable_volatility_scaling = true;   // Scale allocation by market volatility
        double volatility_target = 0.20;         // Target portfolio volatility
    };
    
    struct AllocationResult {
        // Position 1 (base instrument: QQQ or PSQ)
        std::string base_symbol;
        double base_allocation_pct;      // % of total capital
        double base_position_value;      // $ value
        double base_quantity;            // # shares
        
        // Position 2 (leveraged instrument: TQQQ or SQQQ)
        std::string leveraged_symbol;
        double leveraged_allocation_pct; // % of total capital
        double leveraged_position_value; // $ value
        double leveraged_quantity;       // # shares
        
        // Aggregate metrics
        double total_allocation_pct;     // Total % allocated (should be ~100%)
        double total_position_value;     // Total $ value
        double cash_reserve_pct;         // % held in cash (if any)
        
        // Risk metrics
        double effective_leverage;       // Portfolio-level leverage
        double risk_score;               // 0.0-1.0 (1.0 = max risk)
        double expected_volatility;      // Expected portfolio volatility
        double max_drawdown_estimate;    // Estimated maximum drawdown
        
        // Allocation metadata
        std::string allocation_strategy; // Which strategy was used
        std::string allocation_rationale;// Human-readable explanation
        double confidence_used;          // Actual confidence value used
        double kelly_sizing;             // Kelly criterion suggestion (if calculated)
        
        // Validation flags
        bool is_valid;
        std::vector<std::string> warnings;
    };
    
    struct MarketConditions {
        double current_volatility = 0.0;
        double volatility_percentile = 50.0;  // 0-100 percentile
        double trend_strength = 0.0;          // -1.0 to 1.0
        double correlation = 0.0;              // Correlation between instruments
        std::string market_regime = "NORMAL";  // NORMAL, HIGH_VOL, LOW_VOL, TRENDING
    };
    
    explicit DynamicAllocationManager(const AllocationConfig& config);
    
    // Main allocation function for dual-instrument states
    AllocationResult calculate_dual_allocation(
        PositionStateMachine::State target_state,
        const SignalOutput& signal,
        double available_capital,
        double current_price_base,
        double current_price_leveraged,
        const MarketConditions& market
    ) const;
    
    // Single position allocation (for non-dual states)
    AllocationResult calculate_single_allocation(
        const std::string& symbol,
        const SignalOutput& signal,
        double available_capital,
        double current_price,
        bool is_leveraged = false
    ) const;
    
    // Update configuration
    void update_config(const AllocationConfig& new_config);
    
    // Get current configuration
    const AllocationConfig& get_config() const { return config_; }
    
    // Validation and risk checks
    bool validate_allocation(const AllocationResult& result) const;
    double calculate_risk_score(const AllocationResult& result) const;
    
private:
    AllocationConfig config_;
    
    // Strategy implementations
    AllocationResult calculate_confidence_based_allocation(
        bool is_long,  // true = QQQ_TQQQ, false = PSQ_SQQQ
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_risk_parity_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_kelly_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    AllocationResult calculate_hybrid_allocation(
        bool is_long,
        const SignalOutput& signal,
        double available_capital,
        double price_base,
        double price_leveraged,
        const MarketConditions& market
    ) const;
    
    // Helper functions
    void apply_risk_limits(AllocationResult& result) const;
    void apply_volatility_scaling(AllocationResult& result, const MarketConditions& market) const;
    void calculate_risk_metrics(AllocationResult& result) const;
    void add_validation_warnings(AllocationResult& result) const;
    
    // Utility functions
    double calculate_effective_leverage(double base_pct, double leveraged_pct, double leverage_factor = 3.0) const;
    double calculate_expected_volatility(double base_pct, double leveraged_pct) const;
    double estimate_max_drawdown(double effective_leverage, double expected_vol) const;
    
    // Kelly criterion helpers
    double calculate_kelly_fraction(double win_probability, double win_loss_ratio) const;
    double apply_kelly_safety_factor(double raw_kelly) const;
};

} // namespace backend

#endif // DYNAMIC_ALLOCATION_MANAGER_H

