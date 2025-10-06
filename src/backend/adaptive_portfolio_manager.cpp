// =============================================================================
// Module: backend/adaptive_portfolio_manager.cpp
// Purpose: Implementation of the Adaptive Portfolio Manager that completely
//          eliminates phantom sell orders and provides intelligent trading.
//
// CRITICAL BUG FIX: This implementation prevents the phantom sell order bug
// by validating all sell orders before execution.
// =============================================================================

#include "backend/adaptive_portfolio_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace sentio {

// ===================================================================
// POSITION VALIDATOR IMPLEMENTATION
// ===================================================================

PositionValidator::ValidationResult PositionValidator::validate_sell_order(
    const std::string& symbol, 
    double requested_quantity,
    const std::map<std::string, Position>& positions) const {
    
    ValidationResult result;
    
    // CRITICAL CHECK 1: Position existence
    auto it = positions.find(symbol);
    if (it == positions.end()) {
        result.error_message = "Position does not exist for symbol: " + symbol;
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    const Position& position = it->second;
    
    // CRITICAL CHECK 2: Position has actual quantity
    if (std::abs(position.quantity) < 1e-6) {
        result.error_message = "Position exists but has zero quantity for symbol: " + symbol;
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    // CRITICAL CHECK 3: Sufficient quantity available
    if (requested_quantity > position.quantity + 1e-6) {
        result.error_message = "Insufficient quantity. Available: " + 
                             std::to_string(position.quantity) + 
                             ", Requested: " + std::to_string(requested_quantity);
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    // All validations passed - this is a legitimate sell order
    result.is_valid = true;
    result.validated_quantity = std::min(requested_quantity, position.quantity);
    utils::log_debug("SELL ORDER VALIDATED: Can sell " + std::to_string(result.validated_quantity) + 
                    " shares of " + symbol);
    
    return result;
}

bool PositionValidator::validate_buy_order(const std::string& symbol, 
                                         double quantity, 
                                         double price, 
                                         double available_cash,
                                         double fees) const {
    if (quantity <= 0) {
        utils::log_warning("Invalid buy quantity: " + std::to_string(quantity));
        return false;
    }
    
    if (price <= 0) {
        utils::log_warning("Invalid buy price: " + std::to_string(price));
        return false;
    }
    
    double required_cash = quantity * price + fees;
    if (required_cash > available_cash + 1e-6) {
        utils::log_warning("Insufficient cash. Required: " + std::to_string(required_cash) + 
                         ", Available: " + std::to_string(available_cash));
        return false;
    }
    
    return true;
}

// ===================================================================
// CONFLICT RESOLUTION ENGINE IMPLEMENTATION
// ===================================================================

ConflictResolutionEngine::ConflictAnalysis ConflictResolutionEngine::analyze_conflicts(
    const std::string& proposed_symbol,
    const std::map<std::string, Position>& current_positions,
    double current_price) const {
    
    ConflictAnalysis analysis;
    
    // Define conflicting instrument pairs based on leverage trading requirements
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},  // 3x long conflicts with shorts
        {"SQQQ", {"TQQQ", "QQQ"}},  // 3x short conflicts with longs
        {"PSQ",  {"TQQQ", "QQQ"}},  // 1x short conflicts with longs
        {"QQQ",  {"SQQQ", "PSQ"}}   // 1x long conflicts with shorts
    };
    
    auto conflict_it = conflicts.find(proposed_symbol);
    if (conflict_it == conflicts.end()) {
        return analysis; // No known conflicts for this symbol
    }
    
    // Check for existing conflicting positions
    for (const std::string& conflicting_symbol : conflict_it->second) {
        auto pos_it = current_positions.find(conflicting_symbol);
        if (pos_it != current_positions.end() && std::abs(pos_it->second.quantity) > 1e-6) {
            analysis.has_conflicts = true;
            analysis.conflicting_symbols.push_back(conflicting_symbol);
            
            // Create liquidation order
            TradeOrder liquidation_order;
            liquidation_order.action = TradeAction::SELL;
            liquidation_order.symbol = conflicting_symbol;
            liquidation_order.quantity = pos_it->second.quantity;
            liquidation_order.price = current_price;
            liquidation_order.trade_value = liquidation_order.quantity * current_price;
            liquidation_order.execution_reason = "Conflict resolution: Liquidating " + 
                                               conflicting_symbol + " for " + proposed_symbol;
            
            analysis.liquidation_orders.push_back(liquidation_order);
        }
    }
    
    if (analysis.has_conflicts) {
        analysis.resolution_strategy = "AUTO_LIQUIDATE_CONFLICTS";
        utils::log_info("CONFLICT DETECTED: " + proposed_symbol + " conflicts with " + 
                       std::to_string(analysis.conflicting_symbols.size()) + " existing positions");
    }
    
    return analysis;
}

std::vector<TradeOrder> ConflictResolutionEngine::resolve_conflicts_automatically(
    const ConflictAnalysis& analysis) const {
    
    if (!analysis.has_conflicts) {
        return {};
    }
    
    utils::log_info("AUTO-RESOLVING CONFLICTS: Liquidating " + 
                   std::to_string(analysis.liquidation_orders.size()) + " conflicting positions");
    
    return analysis.liquidation_orders;
}

bool ConflictResolutionEngine::would_conflict(const std::string& proposed, const std::string& existing) const {
    return sentio::utils::would_instruments_conflict(proposed, existing);
}

// ===================================================================
// CASH BALANCE PROTECTOR IMPLEMENTATION
// ===================================================================

CashBalanceProtector::CashBalanceProtector(double min_reserve) 
    : minimum_cash_reserve_(min_reserve) {}

CashBalanceProtector::CashValidationResult CashBalanceProtector::validate_transaction(
    const TradeOrder& order, 
    double current_cash,
    double fee_rate) const {
    
    CashValidationResult result;
    
    if (order.action == TradeAction::SELL) {
        // Selling adds cash, always valid from cash perspective
        double fees = order.trade_value * fee_rate;
        result.projected_cash = current_cash + order.trade_value - fees;
        result.is_valid = true;
        return result;
    }
    
    if (order.action == TradeAction::BUY) {
        double fees = order.trade_value * fee_rate;
        double total_cost = order.trade_value + fees;
        
        result.projected_cash = current_cash - total_cost;
        
        // Check if we maintain minimum reserve
        if (result.projected_cash < minimum_cash_reserve_) {
            result.error_message = "Transaction would violate minimum cash reserve. " +
                                 std::string("Projected cash: ") + std::to_string(result.projected_cash) + 
                                 ", Minimum reserve: " + std::to_string(minimum_cash_reserve_);
            
            // Calculate maximum affordable quantity
            double available_for_trading = current_cash - minimum_cash_reserve_;
            if (available_for_trading > 0 && order.price > 0) {
                result.max_affordable_quantity = available_for_trading / (order.price * (1.0 + fee_rate));
            }
            
            utils::log_warning("CASH VALIDATION FAILED: " + result.error_message);
            return result;
        }
        
        result.is_valid = true;
        utils::log_debug("CASH VALIDATION PASSED: Projected cash balance: " + 
                       std::to_string(result.projected_cash));
    }
    
    return result;
}

TradeOrder CashBalanceProtector::adjust_order_for_cash_constraints(
    const TradeOrder& original_order,
    double current_cash,
    double fee_rate) const {
    
    auto validation = validate_transaction(original_order, current_cash, fee_rate);
    
    if (validation.is_valid) {
        return original_order;
    }
    
    if (validation.max_affordable_quantity > 1e-6) {
        TradeOrder adjusted_order = original_order;
        adjusted_order.quantity = validation.max_affordable_quantity;
        adjusted_order.trade_value = adjusted_order.quantity * adjusted_order.price;
        adjusted_order.execution_reason = "Adjusted for cash constraints: " + original_order.execution_reason;
        
        utils::log_info("ORDER ADJUSTED: Reduced quantity from " + 
                       std::to_string(original_order.quantity) + " to " + 
                       std::to_string(adjusted_order.quantity) + " due to cash constraints");
        
        return adjusted_order;
    }
    
    // Cannot afford any quantity, return HOLD order
    TradeOrder hold_order;
    hold_order.action = TradeAction::HOLD;
    hold_order.symbol = original_order.symbol;
    hold_order.execution_reason = "Insufficient cash for trade: " + validation.error_message;
    
    return hold_order;
}

// ===================================================================
// PROFIT MAXIMIZATION ENGINE IMPLEMENTATION
// ===================================================================

std::string ProfitMaximizationEngine::select_optimal_instrument(
    const SignalOutput& signal, 
    const std::map<std::string, Position>& current_positions) const {
    
    std::vector<InstrumentAnalysis> candidates;
    
    // Analyze all available instruments based on signal direction
    if (signal.probability > 0.5) {
        // Bullish signal - consider long instruments
        candidates.push_back(analyze_instrument("QQQ", signal, 1.0));    // 1x long
        candidates.push_back(analyze_instrument("TQQQ", signal, 3.0));   // 3x long
    } else {
        // Bearish signal - consider short instruments
        candidates.push_back(analyze_instrument("PSQ", signal, -1.0));   // 1x short
        candidates.push_back(analyze_instrument("SQQQ", signal, -3.0));  // 3x short
    }
    
    // Filter out instruments that would create conflicts
    candidates = filter_conflicting_instruments(candidates, current_positions);
    
    if (candidates.empty()) {
        utils::log_warning("No suitable instruments available after conflict filtering");
        return "HOLD";
    }
    
    // Select instrument with highest profit potential
    auto best_instrument = std::max_element(candidates.begin(), candidates.end(),
        [](const InstrumentAnalysis& a, const InstrumentAnalysis& b) {
            return a.final_score < b.final_score;
        });
    
    utils::log_info("OPTIMAL INSTRUMENT SELECTED: " + best_instrument->symbol + 
                   " (Score: " + std::to_string(best_instrument->final_score) + ")");
    
    return best_instrument->symbol;
}

ProfitMaximizationEngine::InstrumentAnalysis ProfitMaximizationEngine::analyze_instrument(
    const std::string& symbol, 
    const SignalOutput& signal,
    double leverage_factor) const {
    
    InstrumentAnalysis analysis;
    analysis.symbol = symbol;
    analysis.leverage_factor = leverage_factor;
    
    // Calculate profit potential based on signal strength and leverage
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0; // 0 to 1
    analysis.profit_potential = signal_strength * std::abs(leverage_factor) * signal_strength;
    
    // Calculate risk score (higher leverage = higher risk)
    analysis.risk_score = std::abs(leverage_factor) * 0.1; // Base risk
    analysis.risk_score += (1.0 - signal_strength) * 0.5; // Signal strength risk
    
    // Signal strength adjustment
    analysis.confidence_adjustment = signal_strength;
    
    // Final score balances profit potential against risk
    analysis.final_score = analysis.profit_potential * analysis.confidence_adjustment - 
                          analysis.risk_score * 0.5;
    
    return analysis;
}

std::vector<ProfitMaximizationEngine::InstrumentAnalysis> 
ProfitMaximizationEngine::filter_conflicting_instruments(
    const std::vector<InstrumentAnalysis>& candidates,
    const std::map<std::string, Position>& current_positions) const {
    
    std::vector<InstrumentAnalysis> filtered;
    
    for (const auto& candidate : candidates) {
        bool has_conflict = false;
        
        // Check for conflicts with existing positions
        for (const auto& [symbol, position] : current_positions) {
            if (std::abs(position.quantity) > 1e-6 && would_conflict(candidate.symbol, symbol)) {
                has_conflict = true;
                break;
            }
        }
        
        if (!has_conflict) {
            filtered.push_back(candidate);
        }
    }
    
    return filtered;
}

bool ProfitMaximizationEngine::would_conflict(const std::string& proposed, const std::string& existing) const {
    return sentio::utils::would_instruments_conflict(proposed, existing);
}

// ===================================================================
// RISK MANAGER IMPLEMENTATION
// ===================================================================

RiskManager::RiskManager(double max_pos_size, double max_portfolio_risk, double vol_adj, double kelly_frac)
    : max_position_size_(max_pos_size), max_portfolio_risk_(max_portfolio_risk),
      volatility_adjustment_factor_(vol_adj), kelly_fraction_(kelly_frac) {}

RiskManager::RiskAnalysis RiskManager::calculate_optimal_position_size(
    const std::string& symbol,
    const SignalOutput& signal,
    double available_capital,
    double current_price,
    const std::map<std::string, Position>& positions) const {

    RiskAnalysis analysis;

    // STEP 1: Calculate Kelly Criterion position size
    double win_probability = signal.probability;
    if (win_probability < 0.5) {
        win_probability = 1.0 - win_probability; // For short positions
    }

    // Adjust expected returns based on leverage
    double leverage_factor = get_leverage_factor(symbol);
    double abs_leverage = std::abs(leverage_factor);
    double expected_win = 0.02 * abs_leverage;   // 2% base win, scaled by leverage
    double expected_loss = 0.015 * abs_leverage; // 1.5% base loss, scaled by leverage

    analysis.kelly_position_size = calculate_kelly_size(win_probability, expected_win, expected_loss);

    // STEP 2: Apply fractional Kelly for safety
    double base_size = analysis.kelly_position_size * kelly_fraction_;

    // STEP 3: Adjust for signal strength
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    double strength_adjustment = 0.7 + (signal_strength * 0.3); // 70% to 100% based on strength

    // STEP 4: Adjust for leverage (reduce size for higher leverage)
    double leverage_adjustment = 1.0 / std::sqrt(abs_leverage);

    // STEP 5: Calculate recommended position size
    analysis.recommended_position_size = base_size * strength_adjustment * leverage_adjustment;

    // STEP 6: Ensure we don't exceed maximum position size
    analysis.recommended_position_size = std::min(analysis.recommended_position_size, max_position_size_);
    analysis.recommended_position_size = std::max(analysis.recommended_position_size, 0.05); // Min 5%

    // STEP 7: Calculate maximum safe quantity
    double position_value = available_capital * analysis.recommended_position_size;
    analysis.max_safe_quantity = position_value / current_price;

    // STEP 8: Calculate risk score
    analysis.risk_score = calculate_portfolio_risk_score(symbol, analysis.max_safe_quantity,
                                                       current_price, positions);

    // STEP 9: Determine risk level and apply risk-based adjustments
    if (analysis.risk_score < 0.1) {
        analysis.risk_level = "LOW";
    } else if (analysis.risk_score < 0.25) {
        analysis.risk_level = "MEDIUM";
    } else if (analysis.risk_score < 0.5) {
        analysis.risk_level = "HIGH";
        analysis.risk_warnings.push_back("High portfolio risk detected");
        // Reduce position size by 20%
        analysis.recommended_position_size *= 0.8;
        analysis.max_safe_quantity *= 0.8;
    } else {
        analysis.risk_level = "EXTREME";
        analysis.risk_warnings.push_back("Extreme portfolio risk - position size reduced");
        // Reduce position size by 50%
        analysis.recommended_position_size *= 0.5;
        analysis.max_safe_quantity *= 0.5;
    }

    // STEP 10: Store final position size
    analysis.final_position_size = analysis.recommended_position_size;

    return analysis;
}

double RiskManager::calculate_kelly_size(double win_probability, double avg_win_pct, double avg_loss_pct) const {
    // Kelly Criterion: f* = (p*b - q) / b
    // where: p = win probability, q = 1-p, b = win/loss ratio

    if (win_probability <= 0.5 || avg_loss_pct <= 0.0 || avg_win_pct <= 0.0) {
        return 0.0; // No edge, don't trade
    }

    double p = win_probability;
    double q = 1.0 - p;
    double b = avg_win_pct / avg_loss_pct; // Win/loss ratio

    // Kelly formula
    double kelly = (p * b - q) / b;

    // Clamp to reasonable range
    kelly = std::max(0.0, kelly);
    kelly = std::min(1.0, kelly); // Never risk more than 100%

    return kelly;
}

double RiskManager::get_leverage_factor(const std::string& symbol) const {
    static const std::map<std::string, double> leverage_factors = {
        {"QQQ", 1.0}, {"TQQQ", 3.0}, {"PSQ", -1.0}, {"SQQQ", -3.0}
    };
    
    auto it = leverage_factors.find(symbol);
    return (it != leverage_factors.end()) ? it->second : 1.0;
}

double RiskManager::calculate_portfolio_risk_score(
    const std::string& new_symbol,
    double new_quantity,
    double new_price,
    const std::map<std::string, Position>& positions) const {
    
    // Calculate value-weighted portfolio risk
    double total_portfolio_value = new_quantity * new_price;
    double total_leveraged_exposure = std::abs(get_leverage_factor(new_symbol)) * 
                                    new_quantity * new_price;
    
    for (const auto& [symbol, position] : positions) {
        double position_value = position.quantity * position.current_price;
        total_portfolio_value += position_value;
        total_leveraged_exposure += std::abs(get_leverage_factor(symbol)) * position_value;
    }
    
    if (total_portfolio_value < 1e-6) return 0.0;
    
    return total_leveraged_exposure / total_portfolio_value;
}

// ===================================================================
// ADAPTIVE PORTFOLIO MANAGER IMPLEMENTATION
// ===================================================================

AdaptivePortfolioManager::AdaptivePortfolioManager(double initial_cash) 
    : cash_balance_(initial_cash), total_portfolio_value_(initial_cash) {
    
    // Initialize all components
    position_validator_ = std::make_unique<PositionValidator>();
    conflict_resolver_ = std::make_unique<ConflictResolutionEngine>();
    cash_protector_ = std::make_unique<CashBalanceProtector>(config_.minimum_cash_reserve);
    profit_optimizer_ = std::make_unique<ProfitMaximizationEngine>();
    risk_manager_ = std::make_unique<RiskManager>();
    
    utils::log_info("ADAPTIVE PORTFOLIO MANAGER INITIALIZED with $" + 
                   std::to_string(initial_cash) + " starting capital");
}

std::vector<TradeOrder> AdaptivePortfolioManager::execute_adaptive_trade(
    const SignalOutput& signal, const Bar& bar) {
    
    std::vector<TradeOrder> orders;
    
    double signal_strength_log = std::abs(signal.probability - 0.5) * 2.0;
    utils::log_info("EXECUTING ADAPTIVE TRADE: Signal=" + std::to_string(signal.probability) + 
                   ", Strength=" + std::to_string(signal_strength_log) + 
                   ", Symbol=" + signal.symbol);
    
    // 1. Validate inputs
    if (!validate_inputs(signal, bar)) {
        orders.push_back(create_hold_order("Invalid signal or market data"));
        return orders;
    }
    
    // 2. Determine trade action based on signal
    TradeAction intended_action = determine_trade_action(signal);
    
    if (intended_action == TradeAction::HOLD) {
        orders.push_back(create_hold_order("Signal in neutral zone"));
        return orders;
    }
    
    // 3. Select optimal instrument
    std::string optimal_symbol = profit_optimizer_->select_optimal_instrument(signal, positions_);
    
    if (optimal_symbol == "HOLD") {
        orders.push_back(create_hold_order("No suitable instruments available"));
        return orders;
    }
    
    // 4. Check for and resolve conflicts
    if (config_.enable_auto_conflict_resolution) {
        auto conflict_analysis = conflict_resolver_->analyze_conflicts(optimal_symbol, positions_, bar.close);
        
        if (conflict_analysis.has_conflicts) {
            auto liquidation_orders = conflict_resolver_->resolve_conflicts_automatically(conflict_analysis);
            orders.insert(orders.end(), liquidation_orders.begin(), liquidation_orders.end());
            
            // Update cash balance from liquidations (simplified)
            for (const auto& liquidation_order : liquidation_orders) {
                cash_balance_ += liquidation_order.trade_value * (1.0 - config_.fee_rate);
                positions_.erase(liquidation_order.symbol);
            }
        }
    }
    
    // 5. Create main trade order
    TradeOrder main_order = create_main_order(intended_action, optimal_symbol, signal, bar);
    
    // 6. Apply risk management
    if (config_.enable_risk_management && intended_action == TradeAction::BUY) {
        main_order = apply_risk_management(main_order, signal, bar);
    }
    
    // 7. Validate cash constraints
    main_order = cash_protector_->adjust_order_for_cash_constraints(main_order, cash_balance_, config_.fee_rate);
    
    // 8. CRITICAL: Final validation for sell orders to prevent phantom orders
    if (main_order.action == TradeAction::SELL) {
        auto validation = position_validator_->validate_sell_order(main_order.symbol, 
                                                                 main_order.quantity, positions_);
        if (!validation.is_valid) {
            main_order = create_hold_order("PHANTOM ORDER PREVENTED: " + validation.error_message);
        } else {
            main_order.quantity = validation.validated_quantity;
            main_order.trade_value = main_order.quantity * main_order.price;
        }
    }
    
    orders.push_back(main_order);
    
    utils::log_info("ADAPTIVE TRADE EXECUTION COMPLETE: Generated " + 
                   std::to_string(orders.size()) + " orders");
    
    return orders;
}

bool AdaptivePortfolioManager::execute_order(const TradeOrder& order) {
    if (order.action == TradeAction::HOLD) {
        return true; // Nothing to execute
    }
    
    if (order.action == TradeAction::BUY) {
        return execute_buy_order(order);
    } else if (order.action == TradeAction::SELL) {
        return execute_sell_order(order);
    }
    
    return false;
}

double AdaptivePortfolioManager::get_total_portfolio_value() const {
    return calculate_total_portfolio_value();
}

bool AdaptivePortfolioManager::has_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return (it != positions_.end() && std::abs(it->second.quantity) > 1e-6);
}

Position AdaptivePortfolioManager::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return (it != positions_.end()) ? it->second : Position{};
}

// Private helper methods implementation continues...
bool AdaptivePortfolioManager::validate_inputs(const SignalOutput& signal, const Bar& bar) const {
    if (signal.probability < 0.0 || signal.probability > 1.0) {
        utils::log_error("Invalid signal probability: " + std::to_string(signal.probability));
        return false;
    }
    
    if (bar.close <= 0.0) {
        utils::log_error("Invalid bar close price: " + std::to_string(bar.close));
        return false;
    }
    
    return true;
}

TradeAction AdaptivePortfolioManager::determine_trade_action(const SignalOutput& signal) const {
    if (signal.probability > config_.buy_threshold) {
        return TradeAction::BUY;
    } else if (signal.probability < config_.sell_threshold) {
        return TradeAction::SELL;
    } else {
        return TradeAction::HOLD;
    }
}

TradeOrder AdaptivePortfolioManager::create_main_order(
    TradeAction action, const std::string& symbol, 
    const SignalOutput& signal, const Bar& bar) const {
    
    TradeOrder order;
    order.action = action;
    order.symbol = symbol;
    order.price = bar.close;
    // order.confidence removed - no longer using confidence for trade decisions
    
    if (action == TradeAction::BUY) {
        // Use Kelly Criterion sizing (will be refined by risk management later)
        double available_cash = cash_balance_ - config_.minimum_cash_reserve;

        // Preliminary position size calculation - will be optimized by risk manager
        // Use signal-based sizing as baseline
        double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
        double base_allocation = 0.15 + (signal_strength * 0.35); // 15% to 50% based on strength

        order.quantity = (available_cash * base_allocation) / bar.close;
        order.trade_value = order.quantity * order.price;
        order.execution_reason = "Buy signal above threshold (Prob: " +
                               std::to_string(signal.probability) +
                               ", Strength: " + std::to_string(signal_strength) + ")";
    } else if (action == TradeAction::SELL) {
        // For sell orders, quantity will be validated later
        auto pos_it = positions_.find(symbol);
        if (pos_it != positions_.end()) {
            order.quantity = pos_it->second.quantity;
            order.trade_value = order.quantity * order.price;
            order.execution_reason = "Sell signal below threshold (Prob: " + 
                                   std::to_string(signal.probability) + ")";
        }
    }
    
    return order;
}

TradeOrder AdaptivePortfolioManager::apply_risk_management(
    const TradeOrder& order, const SignalOutput& signal, const Bar& bar) const {
    
    auto risk_analysis = risk_manager_->calculate_optimal_position_size(
        order.symbol, signal, cash_balance_, bar.close, positions_);
    
    TradeOrder risk_adjusted_order = order;
    
    if (risk_analysis.max_safe_quantity < order.quantity) {
        risk_adjusted_order.quantity = risk_analysis.max_safe_quantity;
        risk_adjusted_order.trade_value = risk_adjusted_order.quantity * risk_adjusted_order.price;
        risk_adjusted_order.execution_reason = "Risk-adjusted: " + order.execution_reason + 
                                             " (Risk level: " + risk_analysis.risk_level + ")";
        
        utils::log_info("RISK MANAGEMENT: Reduced position size from " + 
                       std::to_string(order.quantity) + " to " + 
                       std::to_string(risk_adjusted_order.quantity) + " due to " + risk_analysis.risk_level + " risk");
    }
    
    return risk_adjusted_order;
}

TradeOrder AdaptivePortfolioManager::create_hold_order(const std::string& reason) const {
    TradeOrder hold_order;
    hold_order.action = TradeAction::HOLD;
    hold_order.execution_reason = reason;
    return hold_order;
}

bool AdaptivePortfolioManager::execute_buy_order(const TradeOrder& order) {
    double total_cost = order.trade_value + (order.trade_value * config_.fee_rate);
    
    if (total_cost > cash_balance_) {
        utils::log_error("Insufficient cash for buy order");
        return false;
    }
    
    // Update cash balance
    cash_balance_ -= total_cost;
    
    // Update or create position
    auto it = positions_.find(order.symbol);
    if (it != positions_.end()) {
        // Update existing position (average cost basis)
        Position& pos = it->second;
        double total_quantity = pos.quantity + order.quantity;
        double total_cost_basis = (pos.quantity * pos.avg_price) + order.trade_value;
        pos.avg_price = total_cost_basis / total_quantity;
        pos.quantity = total_quantity;
        pos.current_price = order.price;
    } else {
        // Create new position
        Position pos;
        pos.symbol = order.symbol;
        pos.quantity = order.quantity;
        pos.avg_price = order.price;
        pos.current_price = order.price;
        pos.unrealized_pnl = 0.0;
        pos.realized_pnl = 0.0;
        positions_[order.symbol] = pos;
    }
    
    utils::log_info("BUY ORDER EXECUTED: " + std::to_string(order.quantity) + 
                   " shares of " + order.symbol + " at $" + std::to_string(order.price));
    
    return true;
}

bool AdaptivePortfolioManager::execute_sell_order(const TradeOrder& order) {
    auto it = positions_.find(order.symbol);
    if (it == positions_.end()) {
        utils::log_error("PHANTOM ORDER PREVENTED: Cannot sell non-existent position: " + order.symbol);
        return false;
    }
    
    Position& pos = it->second;
    if (pos.quantity < order.quantity) {
        utils::log_error("PHANTOM ORDER PREVENTED: Insufficient quantity to sell");
        return false;
    }
    
    // Update cash balance (subtract fees)
    double proceeds = order.trade_value - (order.trade_value * config_.fee_rate);
    cash_balance_ += proceeds;
    
    // Update position
    pos.quantity -= order.quantity;
    pos.current_price = order.price;
    
    // Remove position if fully liquidated
    if (pos.quantity < 1e-6) {
        positions_.erase(it);
    }
    
    utils::log_info("SELL ORDER EXECUTED: " + std::to_string(order.quantity) + 
                   " shares of " + order.symbol + " at $" + std::to_string(order.price));
    
    return true;
}

double AdaptivePortfolioManager::calculate_total_portfolio_value() const {
    double total_value = cash_balance_;
    
    for (const auto& [symbol, position] : positions_) {
        total_value += position.quantity * position.current_price;
    }
    
    return total_value;
}

} // namespace sentio
