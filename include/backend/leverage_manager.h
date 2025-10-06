#pragma once

// =============================================================================
// Module: backend/leverage_manager.h
// Purpose: Implements leverage trading rules, including position conflict
//          prevention and intelligent instrument selection.
//
// Core Architecture & Design:
// This module encapsulates the strict position conflict prevention rules from
// the Leverage Trading Requirements document. It provides a whitelist validator
// for allowed position combinations and implements the corrected directional
// conflict logic.
//
// Key Design Principles:
// - Strict whitelist approach: Only explicitly allowed combinations permitted
// - Directional conflict prevention: Prevents long + short combinations
// - Flexible allocation: Allows multiple instruments on same side of market
// - Clear error messages: Provides specific reasons for conflicts
//
// Recent Improvements:
// - Corrected conflict logic based on directional analysis
// - PSQ + SQQQ now allowed (both provide short QQQ exposure)
// - Clear separation between allocation decisions and directional conflicts
// =============================================================================

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace sentio {

/// Leverage position validator implementing strict conflict prevention rules
/// 
/// This class implements the corrected leverage trading logic that prevents
/// directional conflicts while allowing flexible allocation decisions.
/// 
/// CORE RULE: A conflict exists if, and only if, the portfolio holds both
/// a LONG instrument (QQQ/TQQQ) and a SHORT/INVERSE instrument (PSQ/SQQQ)
/// at the same time.
class LeveragePositionValidator {
public:
    /// Constructor - initializes with leverage specifications and allowed combinations
    LeveragePositionValidator();
    
    /// Checks if adding a new symbol would violate conflict rules
    /// 
    /// This method implements the strict whitelist approach from the requirements.
    /// It checks the proposed portfolio state against the allowed combinations
    /// and provides specific conflict reasons.
    /// 
    /// @param new_symbol The symbol of the proposed trade
    /// @param current_positions A map of current positions held
    /// @return A reason string if it conflicts, otherwise an empty string
    std::string would_cause_conflict(const std::string& new_symbol,
                                   const std::map<std::string, Position>& current_positions) const;
    
    /// Get all allowed position combinations
    /// @return Set of allowed position combinations
    const std::set<std::set<std::string>>& get_allowed_combinations() const {
        return allowed_combinations_;
    }
    
    /// Check if a symbol is a leverage instrument
    /// @param symbol The symbol to check
    /// @return true if the symbol is a known leverage instrument
    bool is_leverage_instrument(const std::string& symbol) const;

private:
    /// Returns true if a symbol has long QQQ exposure (QQQ, TQQQ)
    bool is_long_qqq(const std::string& symbol) const;
    
    /// Returns true if a symbol has short QQQ exposure (SQQQ, PSQ)
    bool is_short_qqq(const std::string& symbol) const;
    
    /// Get current active symbols with non-zero quantity
    std::set<std::string> get_active_symbols(const std::map<std::string, Position>& positions) const;
    
    /// Check if a combination is in the allowed whitelist
    bool is_allowed_combination(const std::set<std::string>& symbols) const;
    
    /// Generate specific conflict reason based on symbol types
    std::string generate_conflict_reason(const std::string& new_symbol, 
                                       const std::set<std::string>& active_symbols) const;
    
    // Leverage instrument specifications
    std::set<std::string> long_symbols_;      // QQQ, TQQQ
    std::set<std::string> short_symbols_;     // SQQQ, PSQ
    
    // Allowed position combinations (whitelist approach)
    std::set<std::set<std::string>> allowed_combinations_;
};

/// Leverage instrument selector for intelligent instrument selection
/// 
/// This class implements the logic for selecting the optimal leverage
/// instrument based on signal strength and market conditions.
class LeverageSelector {
public:
    /// Select the optimal leverage instrument based on signal characteristics
    /// 
    /// This method implements the instrument selection logic from the requirements:
    /// - High confidence bullish signals -> TQQQ (maximum leverage)
    /// - Moderate bullish signals -> QQQ (moderate exposure)
    /// - High confidence bearish signals -> SQQQ (maximum inverse leverage)
    /// - Moderate bearish signals -> PSQ (moderate inverse exposure)
    /// - Neutral zone or low confidence -> HOLD
    /// 
    /// @param probability Signal probability (0.0 to 1.0)
    /// @param confidence Signal confidence (0.0 to 1.0)
    /// @return Recommended symbol for the trade, or "HOLD" if no recommendation
    std::string select_optimal_instrument(double probability, double confidence) const;
    
    /// Calculate risk-adjusted position size based on leverage factor
    /// 
    /// This method implements risk parity by reducing position size for higher
    /// leverage instruments to maintain consistent risk exposure.
    /// 
    /// @param symbol The leverage instrument symbol
    /// @param base_position_size Base position size for 1x leverage
    /// @return Risk-adjusted position size
    double calculate_risk_adjusted_size(const std::string& symbol, double base_position_size) const;
    
    /// Get leverage factor for a symbol
    /// @param symbol The instrument symbol
    /// @return Leverage factor (1.0 for QQQ/PSQ, 3.0 for TQQQ/SQQQ)
    double get_leverage_factor(const std::string& symbol) const;

private:
    /// Validate signal parameters
    void validate_signal_params(double probability, double confidence) const;
    
    /// Check if signal is in bullish zone
    bool is_bullish_signal(double probability, double confidence) const;
    
    /// Check if signal is in bearish zone
    bool is_bearish_signal(double probability, double confidence) const;
    
    /// Check if signal is in neutral zone
    bool is_neutral_signal(double probability, double confidence) const;
};

} // namespace sentio
