#pragma once

// =============================================================================
// Module: backend/adaptive_portfolio_manager.h
// Purpose: Comprehensive adaptive portfolio management system that eliminates
//          phantom sell orders and provides intelligent trading capabilities.
//
// Core Components:
// - Position Validator: Prevents phantom sell orders
// - Conflict Resolution Engine: Handles position conflicts automatically
// - Cash Balance Protector: Maintains positive cash balances
// - Profit Maximization Engine: Optimizes instrument selection
// - Risk Manager: Dynamic position sizing and risk controls
// =============================================================================

#include "common/types.h"
#include "strategy/signal_output.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sentio {

// Forward declarations
struct Position;
class BackendComponent;

// We need to use BackendComponent::TradeOrder but can't include the full header due to circular dependency
// So we'll define our own TradeOrder that's compatible
struct TradeOrder {
    TradeAction action = TradeAction::HOLD;
    std::string symbol;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    std::string execution_reason;
    double confidence = 0.0;
    
    TradeOrder() = default;
    TradeOrder(TradeAction act, const std::string& sym, double qty, double prc)
        : action(act), symbol(sym), quantity(qty), price(prc) {
        trade_value = quantity * price;
    }
};

// TradeAction is already defined in common/types.h

// ===================================================================
// POSITION VALIDATOR - Prevents phantom sell orders
// ===================================================================

class PositionValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        double validated_quantity = 0.0;
    };

    /// Validates a sell order to prevent phantom orders
    /// This is the CRITICAL function that fixes the phantom sell order bug
    ValidationResult validate_sell_order(const std::string& symbol, 
                                       double requested_quantity,
                                       const std::map<std::string, Position>& positions) const;
    
    /// Validates a buy order for cash availability
    bool validate_buy_order(const std::string& symbol, 
                          double quantity, 
                          double price, 
                          double available_cash,
                          double fees = 0.0) const;
};

// ===================================================================
// CONFLICT RESOLUTION ENGINE - Handles position conflicts
// ===================================================================

class ConflictResolutionEngine {
public:
    struct ConflictAnalysis {
        bool has_conflicts = false;
        std::vector<std::string> conflicting_symbols;
        std::vector<TradeOrder> liquidation_orders;
        std::string resolution_strategy;
    };

    /// Analyzes potential conflicts with proposed symbol
    ConflictAnalysis analyze_conflicts(const std::string& proposed_symbol,
                                     const std::map<std::string, Position>& current_positions,
                                     double current_price) const;
    
    /// Automatically resolves conflicts by generating liquidation orders
    std::vector<TradeOrder> resolve_conflicts_automatically(const ConflictAnalysis& analysis) const;

private:
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// CASH BALANCE PROTECTOR - Prevents negative cash balances
// ===================================================================

class CashBalanceProtector {
private:
    double minimum_cash_reserve_;
    
public:
    explicit CashBalanceProtector(double min_reserve = 1000.0);
    
    struct CashValidationResult {
        bool is_valid = false;
        std::string error_message;
        double projected_cash = 0.0;
        double max_affordable_quantity = 0.0;
    };
    
    /// Validates transaction won't cause negative cash balance
    CashValidationResult validate_transaction(const TradeOrder& order, 
                                            double current_cash,
                                            double fee_rate = 0.001) const;
    
    /// Adjusts order size to fit cash constraints
    TradeOrder adjust_order_for_cash_constraints(const TradeOrder& original_order,
                                                double current_cash,
                                                double fee_rate = 0.001) const;
};

// ===================================================================
// PROFIT MAXIMIZATION ENGINE - Intelligent instrument selection
// ===================================================================

class ProfitMaximizationEngine {
public:
    struct InstrumentAnalysis {
        std::string symbol;
        double profit_potential = 0.0;
        double risk_score = 0.0;
        double leverage_factor = 1.0;
        double confidence_adjustment = 1.0;
        double final_score = 0.0;
    };
    
    /// Selects optimal instrument based on signal and portfolio state
    std::string select_optimal_instrument(const SignalOutput& signal, 
                                        const std::map<std::string, Position>& current_positions) const;

private:
    InstrumentAnalysis analyze_instrument(const std::string& symbol, 
                                        const SignalOutput& signal,
                                        double leverage_factor) const;
    
    std::vector<InstrumentAnalysis> filter_conflicting_instruments(
        const std::vector<InstrumentAnalysis>& candidates,
        const std::map<std::string, Position>& current_positions) const;
    
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// RISK MANAGER - Dynamic position sizing and risk controls
// ===================================================================

class RiskManager {
private:
    double max_position_size_;
    double max_portfolio_risk_;
    double volatility_adjustment_factor_;
    double kelly_fraction_;  // Fraction of full Kelly to use (0.25 = 25% Kelly)

public:
    RiskManager(double max_pos_size = 0.25, double max_portfolio_risk = 0.15,
                double vol_adj = 0.1, double kelly_frac = 0.25);

    struct RiskAnalysis {
        double recommended_position_size = 0.0;
        double max_safe_quantity = 0.0;
        double risk_score = 0.0;
        std::string risk_level; // "LOW", "MEDIUM", "HIGH", "EXTREME"
        std::vector<std::string> risk_warnings;
        double kelly_position_size = 0.0;  // Kelly Criterion recommended size
        double final_position_size = 0.0;  // After all constraints
    };

    /// Calculates optimal position size with Kelly Criterion and risk controls
    RiskAnalysis calculate_optimal_position_size(const std::string& symbol,
                                               const SignalOutput& signal,
                                               double available_capital,
                                               double current_price,
                                               const std::map<std::string, Position>& positions) const;

    /// Calculate Kelly Criterion position size
    /// @param win_probability Expected win probability (from signal)
    /// @param avg_win_pct Average win percentage (estimated)
    /// @param avg_loss_pct Average loss percentage (estimated)
    /// @return Optimal position size as fraction of capital (0.0-1.0)
    double calculate_kelly_size(double win_probability,
                               double avg_win_pct = 0.02,
                               double avg_loss_pct = 0.015) const;

private:
    double get_leverage_factor(const std::string& symbol) const;
    double calculate_portfolio_risk_score(const std::string& new_symbol,
                                        double new_quantity,
                                        double new_price,
                                        const std::map<std::string, Position>& positions) const;
};

// ===================================================================
// ADAPTIVE PORTFOLIO MANAGER - Main orchestrator class
// ===================================================================

class AdaptivePortfolioManager {
private:
    std::unique_ptr<PositionValidator> position_validator_;
    std::unique_ptr<ConflictResolutionEngine> conflict_resolver_;
    std::unique_ptr<CashBalanceProtector> cash_protector_;
    std::unique_ptr<ProfitMaximizationEngine> profit_optimizer_;
    std::unique_ptr<RiskManager> risk_manager_;
    
    // Portfolio state
    std::map<std::string, Position> positions_;
    double cash_balance_;
    double total_portfolio_value_;
    
    // Configuration
    struct Config {
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        double fee_rate = 0.001;
        bool enable_auto_conflict_resolution = true;
        bool enable_risk_management = true;
        double minimum_cash_reserve = 1000.0;
    } config_;
    
public:
    explicit AdaptivePortfolioManager(double initial_cash = 100000.0);
    
    /// Main method that replaces the buggy evaluate_signal logic
    /// This method GUARANTEES no phantom sell orders
    std::vector<TradeOrder> execute_adaptive_trade(const SignalOutput& signal, const Bar& bar);
    
    /// Execute individual orders with validation
    bool execute_order(const TradeOrder& order);
    
    // Portfolio state access
    const std::map<std::string, Position>& get_positions() const { return positions_; }
    double get_cash_balance() const { return cash_balance_; }
    double get_total_portfolio_value() const;
    bool has_position(const std::string& symbol) const;
    Position get_position(const std::string& symbol) const;
    
private:
    bool validate_inputs(const SignalOutput& signal, const Bar& bar) const;
    TradeAction determine_trade_action(const SignalOutput& signal) const;
    TradeOrder create_main_order(TradeAction action, const std::string& symbol, 
                               const SignalOutput& signal, const Bar& bar) const;
    TradeOrder apply_risk_management(const TradeOrder& order, const SignalOutput& signal, const Bar& bar) const;
    TradeOrder create_hold_order(const std::string& reason) const;
    bool execute_buy_order(const TradeOrder& order);
    bool execute_sell_order(const TradeOrder& order);
    double calculate_total_portfolio_value() const;
};

} // namespace sentio
