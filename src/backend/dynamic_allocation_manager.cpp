// File: src/backend/dynamic_allocation_manager.cpp
#include "backend/dynamic_allocation_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace backend {

DynamicAllocationManager::DynamicAllocationManager(const AllocationConfig& config)
    : config_(config) {
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_dual_allocation(
    PositionStateMachine::State target_state,
    const SignalOutput& signal,
    double available_capital,
    double current_price_base,
    double current_price_leveraged,
    const MarketConditions& market) const {
    
    // Determine if long or short dual state
    bool is_long = (target_state == PositionStateMachine::State::QQQ_TQQQ);
    bool is_short = (target_state == PositionStateMachine::State::PSQ_SQQQ);
    
    if (!is_long && !is_short) {
        // Not a dual state - shouldn't happen but handle gracefully
        sentio::utils::log_error("calculate_dual_allocation called with non-dual state: " + 
                        std::to_string(static_cast<int>(target_state)));
        AllocationResult result;
        result.is_valid = false;
        result.warnings.push_back("Invalid state for dual allocation");
        return result;
    }
    
    // Select allocation strategy
    AllocationResult result;
    switch (config_.strategy) {
        case AllocationConfig::Strategy::CONFIDENCE_BASED:
            result = calculate_confidence_based_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::RISK_PARITY:
            result = calculate_risk_parity_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::KELLY_CRITERION:
            result = calculate_kelly_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        case AllocationConfig::Strategy::HYBRID:
            result = calculate_hybrid_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
            
        default:
            result = calculate_confidence_based_allocation(
                is_long, signal, available_capital, current_price_base, current_price_leveraged, market);
            break;
    }
    
    // Apply risk limits and validation
    apply_risk_limits(result);
    if (config_.enable_volatility_scaling) {
        apply_volatility_scaling(result, market);
    }
    calculate_risk_metrics(result);
    add_validation_warnings(result);
    result.is_valid = validate_allocation(result);
    
    // Log allocation details
    std::stringstream ss;
    ss << "ALLOCATION RESULT: " << result.allocation_rationale
       << " | Base: " << result.base_symbol << " " << result.base_allocation_pct * 100 << "%"
       << " | Leveraged: " << result.leveraged_symbol << " " << result.leveraged_allocation_pct * 100 << "%"
       << " | Effective Leverage: " << result.effective_leverage << "x"
       << " | Risk Score: " << result.risk_score;
    sentio::utils::log_info(ss.str());
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_confidence_based_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols based on direction
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // CORE ALLOCATION FORMULA (Signal Strength-Based):
    // High signal strength → more leverage
    // Low signal strength → more base position
    
    // Calculate signal strength from probability (0.0 to 1.0)
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    // Apply bounds
    signal_strength = std::clamp(signal_strength, config_.confidence_floor, config_.confidence_ceiling);
    
    // Apply power for tuning aggression
    signal_strength = std::pow(signal_strength, config_.confidence_power);
    
    // Base allocation formula
    result.leveraged_allocation_pct = signal_strength;
    result.base_allocation_pct = 1.0 - signal_strength;
    
    // Apply risk limits
    result.leveraged_allocation_pct = std::min(result.leveraged_allocation_pct, 
                                              config_.max_leverage_allocation);
    result.base_allocation_pct = std::max(result.base_allocation_pct,
                                         config_.min_base_allocation);
    
    // Renormalize to sum to 1.0
    double total = result.leveraged_allocation_pct + result.base_allocation_pct;
    if (total > 0) {
        result.leveraged_allocation_pct /= total;
        result.base_allocation_pct /= total;
    }
    
    // Apply total allocation limits
    double total_allocation = config_.min_total_allocation;
    if (signal_strength > 0.8) {
        total_allocation = config_.max_total_allocation;
    }
    
    // Calculate position values
    double allocated_capital = available_capital * total_allocation;
    result.base_position_value = allocated_capital * result.base_allocation_pct;
    result.leveraged_position_value = allocated_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Recalculate actual values based on rounded quantities
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    
    // Update percentages based on actual positions
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Set metadata
    result.allocation_strategy = "SIGNAL_STRENGTH_BASED";
    result.confidence_used = signal_strength;
    
    // Create rationale
    std::stringstream ss;
    ss << "Signal strength-based split: "
       << static_cast<int>(result.base_allocation_pct * 100) << "% " << result.base_symbol
       << ", " << static_cast<int>(result.leveraged_allocation_pct * 100) << "% " << result.leveraged_symbol
       << " (signal_strength=" << std::fixed << std::setprecision(2) << (std::abs(signal.probability - 0.5) * 2.0)
       << ", adjusted=" << signal_strength << ")";
    result.allocation_rationale = ss.str();
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_risk_parity_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // Risk parity: allocate inversely proportional to volatility
    // Goal: each position contributes equally to portfolio risk
    
    double base_vol = config_.base_volatility;
    double leveraged_vol = config_.leveraged_volatility;
    
    // Adjust for market conditions
    if (market.current_volatility > 0) {
        double vol_multiplier = market.current_volatility / 0.15;  // Normalize to typical market vol
        base_vol *= vol_multiplier;
        leveraged_vol *= vol_multiplier;
    }
    
    // Inverse volatility weighting
    double base_weight = 1.0 / base_vol;
    double leveraged_weight = 1.0 / leveraged_vol;
    
    // Normalize weights
    double total_weight = base_weight + leveraged_weight;
    result.base_allocation_pct = base_weight / total_weight;
    result.leveraged_allocation_pct = leveraged_weight / total_weight;
    
    // Scale by confidence (higher confidence = more total allocation)
    double total_allocation = config_.min_total_allocation + 
                            (config_.max_total_allocation - config_.min_total_allocation) * std::abs(signal.probability - 0.5) * 2.0;
    
    // Calculate positions
    double allocated_capital = available_capital * total_allocation;
    result.base_position_value = allocated_capital * result.base_allocation_pct;
    result.leveraged_position_value = allocated_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "RISK_PARITY";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.allocation_rationale = "Risk parity allocation with equal risk contribution";
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_kelly_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    AllocationResult result;
    
    // Set symbols
    if (is_long) {
        result.base_symbol = "QQQ";
        result.leveraged_symbol = "TQQQ";
    } else {
        result.base_symbol = "PSQ";
        result.leveraged_symbol = "SQQQ";
    }
    
    // Kelly Criterion: f* = (p*b - q) / b
    // where p = win probability, q = 1-p, b = win/loss ratio
    
    // Use signal probability as win probability
    double win_prob = is_long ? signal.probability : (1.0 - signal.probability);
    win_prob = std::clamp(win_prob, 0.45, 0.65);  // Cap to reasonable bounds
    
    // Adjust win/loss ratio based on confidence
    double win_loss_ratio = config_.avg_win_loss_ratio * (0.8 + 0.4 * (std::abs(signal.probability - 0.5) * 2.0));
    
    // Calculate raw Kelly fraction
    double raw_kelly = calculate_kelly_fraction(win_prob, win_loss_ratio);
    
    // Apply safety factor (fractional Kelly)
    double kelly_fraction = apply_kelly_safety_factor(raw_kelly) * config_.kelly_fraction;
    kelly_fraction = std::clamp(kelly_fraction, 0.0, 1.0);
    
    // Split between base and leveraged based on Kelly sizing
    // Higher Kelly = more leverage
    result.leveraged_allocation_pct = kelly_fraction * 0.8;  // Max 80% in leveraged
    result.base_allocation_pct = kelly_fraction * 0.2 + (1.0 - kelly_fraction) * 0.5;
    
    // Normalize
    double total = result.leveraged_allocation_pct + result.base_allocation_pct;
    if (total > 1.0) {
        result.leveraged_allocation_pct /= total;
        result.base_allocation_pct /= total;
    }
    
    // Calculate positions
    result.base_position_value = available_capital * result.base_allocation_pct;
    result.leveraged_position_value = available_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "KELLY_CRITERION";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.kelly_sizing = kelly_fraction;
    
    std::stringstream ss;
    ss << "Kelly allocation (f*=" << std::fixed << std::setprecision(3) << kelly_fraction
       << ", p=" << win_prob << ", b=" << win_loss_ratio << ")";
    result.allocation_rationale = ss.str();
    
    return result;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_hybrid_allocation(
    bool is_long,
    const SignalOutput& signal,
    double available_capital,
    double price_base,
    double price_leveraged,
    const MarketConditions& market) const {
    
    // Hybrid: blend of all three approaches
    auto confidence_result = calculate_confidence_based_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    auto risk_parity_result = calculate_risk_parity_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    auto kelly_result = calculate_kelly_allocation(
        is_long, signal, available_capital, price_base, price_leveraged, market);
    
    // Weight the approaches
    double confidence_weight = 0.5;  // Primary driver
    double risk_parity_weight = 0.3;
    double kelly_weight = 0.2;
    
    // Blend allocations
    AllocationResult result;
    result.base_symbol = confidence_result.base_symbol;
    result.leveraged_symbol = confidence_result.leveraged_symbol;
    
    result.base_allocation_pct = 
        confidence_weight * confidence_result.base_allocation_pct +
        risk_parity_weight * risk_parity_result.base_allocation_pct +
        kelly_weight * kelly_result.base_allocation_pct;
    
    result.leveraged_allocation_pct = 
        confidence_weight * confidence_result.leveraged_allocation_pct +
        risk_parity_weight * risk_parity_result.leveraged_allocation_pct +
        kelly_weight * kelly_result.leveraged_allocation_pct;
    
    // Calculate positions
    result.base_position_value = available_capital * result.base_allocation_pct;
    result.leveraged_position_value = available_capital * result.leveraged_allocation_pct;
    
    // Calculate quantities
    result.base_quantity = std::floor(result.base_position_value / price_base);
    result.leveraged_quantity = std::floor(result.leveraged_position_value / price_leveraged);
    
    // Update values
    result.base_position_value = result.base_quantity * price_base;
    result.leveraged_position_value = result.leveraged_quantity * price_leveraged;
    result.total_position_value = result.base_position_value + result.leveraged_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.leveraged_allocation_pct = result.leveraged_position_value / available_capital;
        result.total_allocation_pct = result.total_position_value / available_capital;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Metadata
    result.allocation_strategy = "HYBRID";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.kelly_sizing = kelly_result.kelly_sizing;
    result.allocation_rationale = "Hybrid allocation (50% confidence, 30% risk-parity, 20% Kelly)";
    
    return result;
}

void DynamicAllocationManager::apply_risk_limits(AllocationResult& result) const {
    // Ensure we don't exceed maximum leverage allocation
    if (result.leveraged_allocation_pct > config_.max_leverage_allocation) {
        double excess = result.leveraged_allocation_pct - config_.max_leverage_allocation;
        result.leveraged_allocation_pct = config_.max_leverage_allocation;
        result.base_allocation_pct += excess;
    }
    
    // Ensure minimum base allocation
    if (result.base_allocation_pct < config_.min_base_allocation) {
        double shortfall = config_.min_base_allocation - result.base_allocation_pct;
        result.base_allocation_pct = config_.min_base_allocation;
        result.leveraged_allocation_pct = std::max(0.0, result.leveraged_allocation_pct - shortfall);
    }
    
    // Check total leverage
    double eff_leverage = calculate_effective_leverage(result.base_allocation_pct, result.leveraged_allocation_pct);
    if (eff_leverage > config_.max_total_leverage) {
        // Scale down leveraged position
        double max_leveraged = (config_.max_total_leverage - result.base_allocation_pct) / 3.0;
        result.leveraged_allocation_pct = std::min(result.leveraged_allocation_pct, max_leveraged);
    }
}

void DynamicAllocationManager::apply_volatility_scaling(AllocationResult& result, const MarketConditions& market) const {
    if (market.current_volatility <= 0) return;
    
    // Scale allocation based on volatility target
    double vol_scalar = config_.volatility_target / market.current_volatility;
    vol_scalar = std::clamp(vol_scalar, 0.5, 1.5);  // Limit adjustment range
    
    // Reduce position sizes in high volatility
    if (vol_scalar < 1.0) {
        result.base_allocation_pct *= vol_scalar;
        result.leveraged_allocation_pct *= vol_scalar;
        result.cash_reserve_pct = 1.0 - (result.base_allocation_pct + result.leveraged_allocation_pct);
        
        result.warnings.push_back("Position scaled down due to high volatility");
    }
}

void DynamicAllocationManager::calculate_risk_metrics(AllocationResult& result) const {
    // Effective leverage
    result.effective_leverage = calculate_effective_leverage(
        result.base_allocation_pct, result.leveraged_allocation_pct);
    
    // Risk score (0-1)
    result.risk_score = calculate_risk_score(result);
    
    // Expected volatility
    result.expected_volatility = calculate_expected_volatility(
        result.base_allocation_pct, result.leveraged_allocation_pct);
    
    // Max drawdown estimate
    result.max_drawdown_estimate = estimate_max_drawdown(
        result.effective_leverage, result.expected_volatility);
}

void DynamicAllocationManager::add_validation_warnings(AllocationResult& result) const {
    if (result.effective_leverage > 2.5) {
        result.warnings.push_back("High leverage warning: " + 
                                 std::to_string(result.effective_leverage) + "x");
    }
    
    if (result.cash_reserve_pct > 0.1) {
        result.warnings.push_back("Significant cash reserve: " + 
                                 std::to_string(static_cast<int>(result.cash_reserve_pct * 100)) + "%");
    }
    
    if (result.base_quantity < 1 || result.leveraged_quantity < 1) {
        result.warnings.push_back("Insufficient capital for full dual position");
    }
}

bool DynamicAllocationManager::validate_allocation(const AllocationResult& result) const {
    // Check basic validity
    if (result.base_quantity < 0 || result.leveraged_quantity < 0) {
        return false;
    }
    
    if (result.total_allocation_pct > 1.01) {  // Allow 1% rounding error
        return false;
    }
    
    if (result.effective_leverage > config_.max_total_leverage * 1.1) {  // Allow 10% buffer
        return false;
    }
    
    return true;
}

double DynamicAllocationManager::calculate_risk_score(const AllocationResult& result) const {
    // Normalize various risk factors to 0-1 and combine
    double leverage_score = result.effective_leverage / config_.max_total_leverage;
    double concentration_score = std::max(result.base_allocation_pct, result.leveraged_allocation_pct);
    double volatility_score = result.expected_volatility / 0.5;  // Assume 50% vol is max
    
    // Weighted average
    double risk_score = 0.4 * leverage_score + 0.3 * concentration_score + 0.3 * volatility_score;
    
    return std::clamp(risk_score, 0.0, 1.0);
}

double DynamicAllocationManager::calculate_effective_leverage(
    double base_pct, double leveraged_pct, double leverage_factor) const {
    return base_pct * 1.0 + leveraged_pct * leverage_factor;
}

double DynamicAllocationManager::calculate_expected_volatility(
    double base_pct, double leveraged_pct) const {
    // Portfolio volatility with correlation
    double base_vol = config_.base_volatility;
    double leveraged_vol = config_.leveraged_volatility;
    double correlation = 0.95;  // High correlation between QQQ and TQQQ
    
    // Portfolio variance
    double variance = base_pct * base_pct * base_vol * base_vol +
                     leveraged_pct * leveraged_pct * leveraged_vol * leveraged_vol +
                     2 * base_pct * leveraged_pct * base_vol * leveraged_vol * correlation;
    
    return std::sqrt(variance);
}

double DynamicAllocationManager::estimate_max_drawdown(
    double effective_leverage, double expected_vol) const {
    // Rough estimate: max drawdown ≈ 2 * volatility * sqrt(leverage)
    return 2.0 * expected_vol * std::sqrt(effective_leverage);
}

double DynamicAllocationManager::calculate_kelly_fraction(
    double win_probability, double win_loss_ratio) const {
    // Kelly formula: f* = (p*b - q) / b
    // where p = win probability, q = lose probability, b = win/loss ratio
    double q = 1.0 - win_probability;
    double numerator = win_probability * win_loss_ratio - q;
    
    if (win_loss_ratio <= 0) return 0.0;
    
    return numerator / win_loss_ratio;
}

double DynamicAllocationManager::apply_kelly_safety_factor(double raw_kelly) const {
    // Apply safety factor to raw Kelly
    // Never use full Kelly - too aggressive
    raw_kelly = std::clamp(raw_kelly, 0.0, 2.0);  // Cap at 200% (leveraged)
    
    // Non-linear scaling to be more conservative
    if (raw_kelly > 1.0) {
        return 1.0 + 0.5 * (raw_kelly - 1.0);  // Reduce leverage component
    }
    
    return raw_kelly;
}

DynamicAllocationManager::AllocationResult DynamicAllocationManager::calculate_single_allocation(
    const std::string& symbol,
    const SignalOutput& signal,
    double available_capital,
    double current_price,
    bool is_leveraged) const {
    
    AllocationResult result;
    result.base_symbol = symbol;
    result.leveraged_symbol = "";  // No leveraged position for single allocation
    
    // Scale position size by signal strength
    double position_pct = config_.min_total_allocation + 
                         (config_.max_total_allocation - config_.min_total_allocation) * (std::abs(signal.probability - 0.5) * 2.0);
    
    // Apply leverage penalty if using leveraged instrument alone
    if (is_leveraged) {
        position_pct *= 0.7;  // Reduce position size for leveraged-only positions
    }
    
    // Calculate position
    result.base_allocation_pct = position_pct;
    result.leveraged_allocation_pct = 0.0;
    result.base_position_value = available_capital * position_pct;
    result.base_quantity = std::floor(result.base_position_value / current_price);
    
    // Update actual value
    result.base_position_value = result.base_quantity * current_price;
    result.total_position_value = result.base_position_value;
    
    // Update percentages
    if (available_capital > 0) {
        result.base_allocation_pct = result.base_position_value / available_capital;
        result.total_allocation_pct = result.base_allocation_pct;
        result.cash_reserve_pct = 1.0 - result.total_allocation_pct;
    }
    
    // Risk metrics
    result.effective_leverage = is_leveraged ? 3.0 * position_pct : position_pct;
    result.expected_volatility = is_leveraged ? config_.leveraged_volatility : config_.base_volatility;
    result.risk_score = calculate_risk_score(result);
    result.max_drawdown_estimate = estimate_max_drawdown(result.effective_leverage, result.expected_volatility);
    
    // Metadata
    result.allocation_strategy = "SINGLE_POSITION";
    result.confidence_used = std::abs(signal.probability - 0.5) * 2.0;  // signal strength
    result.allocation_rationale = "Single position in " + symbol;
    result.is_valid = true;
    
    return result;
}

void DynamicAllocationManager::update_config(const AllocationConfig& new_config) {
    config_ = new_config;
}

} // namespace backend

