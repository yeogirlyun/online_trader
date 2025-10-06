#include "backend/leverage_manager.h"
#include "common/utils.h"
#include <numeric>
#include <algorithm>
#include <stdexcept>

namespace sentio {

// =============================================================================
// LeveragePositionValidator Implementation
// =============================================================================

LeveragePositionValidator::LeveragePositionValidator() {
    // Define symbols as per requirement 1.1
    long_symbols_ = {"QQQ", "TQQQ"};
    short_symbols_ = {"SQQQ", "PSQ"};
    
    // Define allowed combinations as per corrected requirement 2.2
    // CORRECTED: PSQ + SQQQ is now allowed (both provide short QQQ exposure)
    allowed_combinations_ = {
        {"QQQ"},           // Single long position
        {"TQQQ"},          // Single 3x long position
        {"SQQQ"},          // Single 3x short position
        {"PSQ"},           // Single 1x short position
        {"QQQ", "TQQQ"},   // Dual long positions (amplified long exposure)
        {"PSQ", "SQQQ"}    // Dual short positions (amplified short exposure) - NOW ALLOWED
    };
}

bool LeveragePositionValidator::is_long_qqq(const std::string& symbol) const {
    return long_symbols_.count(symbol) > 0;
}

bool LeveragePositionValidator::is_short_qqq(const std::string& symbol) const {
    return short_symbols_.count(symbol) > 0;
}

bool LeveragePositionValidator::is_leverage_instrument(const std::string& symbol) const {
    return is_long_qqq(symbol) || is_short_qqq(symbol);
}

std::set<std::string> LeveragePositionValidator::get_active_symbols(
    const std::map<std::string, Position>& positions) const {
    
    std::set<std::string> active_symbols;
    for (const auto& pair : positions) {
        if (std::abs(pair.second.quantity) > 1e-6) {
            active_symbols.insert(pair.first);
        }
    }
    return active_symbols;
}

bool LeveragePositionValidator::is_allowed_combination(const std::set<std::string>& symbols) const {
    return allowed_combinations_.count(symbols) > 0;
}

std::string LeveragePositionValidator::generate_conflict_reason(
    const std::string& new_symbol, 
    const std::set<std::string>& active_symbols) const {
    
    // Check for directional conflicts (long + short)
    if (is_long_qqq(new_symbol)) {
        for (const auto& symbol : active_symbols) {
            if (is_short_qqq(symbol)) {
                return "Conflict: Cannot add long exposure (" + new_symbol + 
                       ") to a portfolio with short exposure (" + symbol + ")";
            }
        }
    }
    
    if (is_short_qqq(new_symbol)) {
        for (const auto& symbol : active_symbols) {
            if (is_long_qqq(symbol)) {
                return "Conflict: Cannot add short exposure (" + new_symbol + 
                       ") to a portfolio with long exposure (" + symbol + ")";
            }
        }
    }
    
    // Check for invalid combinations not in whitelist
    return "Conflict: Proposed position combination is not on the allowed whitelist";
}

std::string LeveragePositionValidator::would_cause_conflict(
    const std::string& new_symbol,
    const std::map<std::string, Position>& current_positions) const {
    
    // If the new symbol is not a leverage instrument, no conflict possible
    if (!is_leverage_instrument(new_symbol)) {
        return "";
    }
    
    // Get current active symbols with non-zero quantity
    std::set<std::string> active_symbols = get_active_symbols(current_positions);
    
    // Determine the proposed portfolio state
    std::set<std::string> proposed_symbols = active_symbols;
    proposed_symbols.insert(new_symbol);
    
    // Check against the whitelist (requirement 2.4)
    if (is_allowed_combination(proposed_symbols)) {
        return ""; // No conflict - combination is allowed
    }
    
    // Generate specific conflict reason
    return generate_conflict_reason(new_symbol, active_symbols);
}

// =============================================================================
// LeverageSelector Implementation
// =============================================================================

void LeverageSelector::validate_signal_params(double probability, double confidence) const {
    if (probability < 0.0 || probability > 1.0) {
        throw std::invalid_argument("Signal probability must be between 0.0 and 1.0");
    }
    if (confidence < 0.0 || confidence > 1.0) {
        throw std::invalid_argument("Signal confidence must be between 0.0 and 1.0");
    }
}

bool LeverageSelector::is_bullish_signal(double probability, double confidence) const {
    return probability > 0.6 && confidence > 0.6;
}

bool LeverageSelector::is_bearish_signal(double probability, double confidence) const {
    return probability < 0.4 && confidence > 0.6;
}

bool LeverageSelector::is_neutral_signal(double probability, double confidence) const {
    return !is_bullish_signal(probability, confidence) && 
           !is_bearish_signal(probability, confidence);
}

std::string LeverageSelector::select_optimal_instrument(double probability, double confidence) const {
    // Validate inputs
    validate_signal_params(probability, confidence);
    
    // High confidence bullish signals -> TQQQ (maximum leverage)
    if (probability > 0.7 && confidence > 0.8) {
        return "TQQQ";
    }
    
    // Moderate bullish signals -> QQQ (moderate exposure)
    if (is_bullish_signal(probability, confidence)) {
        return "QQQ";
    }
    
    // High confidence bearish signals -> SQQQ (maximum inverse leverage)
    if (probability < 0.3 && confidence > 0.8) {
        return "SQQQ";
    }
    
    // Moderate bearish signals -> PSQ (moderate inverse exposure)
    if (is_bearish_signal(probability, confidence)) {
        return "PSQ";
    }
    
    // Neutral zone or low confidence -> no recommendation
    return "HOLD";
}

double LeverageSelector::get_leverage_factor(const std::string& symbol) const {
    if (symbol == "TQQQ" || symbol == "SQQQ") {
        return 3.0;
    }
    // QQQ and PSQ have leverage factor of 1.0
    return 1.0;
}

double LeverageSelector::calculate_risk_adjusted_size(const std::string& symbol, 
                                                    double base_position_size) const {
    double leverage_factor = get_leverage_factor(symbol);
    
    // Reduce position size for higher leverage to maintain risk parity
    // This prevents over-leveraging and maintains consistent risk exposure
    return base_position_size / leverage_factor;
}

} // namespace sentio
