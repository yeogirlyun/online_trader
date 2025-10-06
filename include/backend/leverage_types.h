#pragma once

// =============================================================================
// Module: backend/leverage_types.h
// Purpose: Centralized types and specifications for leverage trading
//
// Core Architecture & Design:
// This module provides the fundamental types and specifications for leverage
// trading instruments. It centralizes all leverage-related definitions to
// ensure consistency across the system and prevent logical inconsistencies.
//
// Key Design Principles:
// - Clear classification of instruments by market direction and leverage
// - Centralized specifications to prevent scattered definitions
// - Type-safe enums to prevent invalid instrument combinations
// - Extensible design for future leverage instruments
//
// Recent Improvements:
// - Corrected conflict logic based on directional analysis
// - Clear separation between long and inverse instruments
// - Robust type system to prevent logical errors
// =============================================================================

#include <string>
#include <map>

namespace sentio {

/// Enum to classify instruments by their market direction and leverage
/// 
/// This classification is crucial for conflict detection. The system prevents
/// holding both LONG and INVERSE instruments simultaneously, as this creates
/// directional conflicts that drain capital through fees.
enum class InstrumentType {
    NEUTRAL,        // Unknown or non-leverage instruments
    LONG_1X,        // QQQ - 1x long QQQ exposure
    LONG_3X,        // TQQQ - 3x long QQQ exposure  
    INVERSE_1X,     // PSQ - 1x short QQQ exposure
    INVERSE_3X      // SQQQ - 3x short QQQ exposure
};

/// Holds the specifications for a single leverage instrument
/// 
/// This structure contains all the essential information needed to:
/// 1. Classify the instrument for conflict detection
/// 2. Generate theoretical pricing data
/// 3. Calculate position sizing and risk metrics
struct LeverageSpec {
    std::string symbol;           // Instrument symbol (e.g., "TQQQ")
    std::string base_symbol;      // Base asset symbol (e.g., "QQQ")
    double leverage_factor;       // Leverage multiplier (1.0, 3.0)
    bool is_inverse;              // true for short/inverse instruments
    InstrumentType type;          // Classification for conflict detection
    
    /// Default constructor for unknown instruments
    LeverageSpec() : symbol(""), base_symbol(""), leverage_factor(1.0), 
                     is_inverse(false), type(InstrumentType::NEUTRAL) {}
    
    /// Constructor for known leverage instruments
    LeverageSpec(const std::string& sym, const std::string& base, 
                 double factor, bool inverse, InstrumentType inst_type)
        : symbol(sym), base_symbol(base), leverage_factor(factor),
          is_inverse(inverse), type(inst_type) {}
};

/// Registry of all supported leverage instruments
/// 
/// This registry provides centralized access to leverage specifications
/// and ensures consistent behavior across the system.
class LeverageRegistry {
public:
    /// Get the singleton instance
    static LeverageRegistry& instance() {
        static LeverageRegistry registry;
        return registry;
    }
    
    /// Get specification for a symbol
    /// @param symbol The instrument symbol to look up
    /// @return LeverageSpec for the symbol, or default spec if not found
    LeverageSpec get_spec(const std::string& symbol) const {
        auto it = specs_.find(symbol);
        if (it != specs_.end()) {
            return it->second;
        }
        return LeverageSpec(); // Return default (NEUTRAL) spec
    }
    
    /// Check if a symbol is a leverage instrument
    /// @param symbol The instrument symbol to check
    /// @return true if the symbol is a known leverage instrument
    bool is_leverage_instrument(const std::string& symbol) const {
        return specs_.find(symbol) != specs_.end();
    }
    
    /// Get all supported symbols
    /// @return Vector of all supported leverage instrument symbols
    std::vector<std::string> get_all_symbols() const {
        std::vector<std::string> symbols;
        for (const auto& [symbol, spec] : specs_) {
            symbols.push_back(symbol);
        }
        return symbols;
    }

private:
    std::map<std::string, LeverageSpec> specs_;
    
    /// Private constructor - initialize with default specifications
    LeverageRegistry() {
        // QQQ Family - Long Instruments
        specs_["QQQ"] = LeverageSpec("QQQ", "QQQ", 1.0, false, InstrumentType::LONG_1X);
        specs_["TQQQ"] = LeverageSpec("TQQQ", "QQQ", 3.0, false, InstrumentType::LONG_3X);
        
        // QQQ Family - Inverse Instruments  
        specs_["PSQ"] = LeverageSpec("PSQ", "QQQ", 1.0, true, InstrumentType::INVERSE_1X);
        specs_["SQQQ"] = LeverageSpec("SQQQ", "QQQ", 3.0, true, InstrumentType::INVERSE_3X);
        
        // Future: SPY Family
        // specs_["SPY"] = LeverageSpec("SPY", "SPY", 1.0, false, InstrumentType::LONG_1X);
        // specs_["UPRO"] = LeverageSpec("UPRO", "SPY", 3.0, false, InstrumentType::LONG_3X);
        // specs_["SPXU"] = LeverageSpec("SPXU", "SPY", 3.0, true, InstrumentType::INVERSE_3X);
    }
};

/// Convenience functions for instrument classification
namespace leverage_utils {

/// Check if an instrument type represents long exposure
/// @param type The instrument type to check
/// @return true if the type represents long market exposure
inline bool is_long_exposure(InstrumentType type) {
    return type == InstrumentType::LONG_1X || type == InstrumentType::LONG_3X;
}

/// Check if an instrument type represents short/inverse exposure
/// @param type The instrument type to check
/// @return true if the type represents short market exposure
inline bool is_short_exposure(InstrumentType type) {
    return type == InstrumentType::INVERSE_1X || type == InstrumentType::INVERSE_3X;
}

/// Check if two instrument types would create a directional conflict
/// @param type1 First instrument type
/// @param type2 Second instrument type
/// @return true if holding both types would create a conflict
inline bool would_create_conflict(InstrumentType type1, InstrumentType type2) {
    // A conflict exists if one is long and the other is short
    return (is_long_exposure(type1) && is_short_exposure(type2)) ||
           (is_short_exposure(type1) && is_long_exposure(type2));
}

/// Get a human-readable description of an instrument type
/// @param type The instrument type
/// @return String description of the type
inline std::string get_type_description(InstrumentType type) {
    switch (type) {
        case InstrumentType::NEUTRAL: return "Unknown/Non-leverage";
        case InstrumentType::LONG_1X: return "1x Long";
        case InstrumentType::LONG_3X: return "3x Long";
        case InstrumentType::INVERSE_1X: return "1x Short/Inverse";
        case InstrumentType::INVERSE_3X: return "3x Short/Inverse";
        default: return "Invalid Type";
    }
}

} // namespace leverage_utils

} // namespace sentio
