#pragma once

#include "common/types.h"
#include "common/exceptions.h"
#include <cmath>
#include <string>
#include <sstream>

namespace sentio {

/**
 * @brief Validate bar data for correctness
 *
 * Checks OHLC relationships, finite values, and reasonable ranges.
 */
class BarValidator {
public:
    /**
     * @brief Check if bar is valid
     * @param bar Bar to validate
     * @return true if valid, false otherwise
     */
    static bool is_valid(const Bar& bar) {
        // Check for finite values
        if (!std::isfinite(bar.open) || !std::isfinite(bar.high) ||
            !std::isfinite(bar.low) || !std::isfinite(bar.close)) {
            return false;
        }

        if (!std::isfinite(bar.volume) || bar.volume < 0) {
            return false;
        }

        // Check OHLC relationships
        if (!(bar.high >= bar.low)) return false;
        if (!(bar.high >= bar.open && bar.high >= bar.close)) return false;
        if (!(bar.low <= bar.open && bar.low <= bar.close)) return false;

        // Check for positive prices
        if (bar.high <= 0 || bar.low <= 0 || bar.open <= 0 || bar.close <= 0) {
            return false;
        }

        // Check for reasonable intrabar moves (>50% move is suspicious)
        if (bar.high / bar.low > 1.5) {
            return false;
        }

        return true;
    }

    /**
     * @brief Validate bar and throw if invalid
     * @param bar Bar to validate
     * @throws InvalidBarError if bar is invalid
     */
    static void validate_or_throw(const Bar& bar) {
        if (!is_valid(bar)) {
            std::stringstream ss;
            ss << "Invalid bar: "
               << "O=" << bar.open
               << " H=" << bar.high
               << " L=" << bar.low
               << " C=" << bar.close
               << " V=" << bar.volume;
            throw InvalidBarError(ss.str());
        }
    }

    /**
     * @brief Get validation error message for invalid bar
     * @param bar Bar to check
     * @return Error message (empty if valid)
     */
    static std::string get_error_message(const Bar& bar) {
        if (!std::isfinite(bar.open) || !std::isfinite(bar.high) ||
            !std::isfinite(bar.low) || !std::isfinite(bar.close)) {
            return "Non-finite OHLC values";
        }

        if (!std::isfinite(bar.volume) || bar.volume < 0) {
            return "Invalid volume";
        }

        if (!(bar.high >= bar.low)) {
            return "High < Low";
        }

        if (!(bar.high >= bar.open && bar.high >= bar.close)) {
            return "High not highest";
        }

        if (!(bar.low <= bar.open && bar.low <= bar.close)) {
            return "Low not lowest";
        }

        if (bar.high <= 0 || bar.low <= 0) {
            return "Non-positive prices";
        }

        if (bar.high / bar.low > 1.5) {
            return "Excessive intrabar move (>50%)";
        }

        return "";
    }
};

/**
 * @brief Convenience function for bar validation
 * @param bar Bar to validate
 * @return true if valid
 */
inline bool is_valid_bar(const Bar& bar) {
    return BarValidator::is_valid(bar);
}

} // namespace sentio
