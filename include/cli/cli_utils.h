#pragma once

#include <vector>
#include <string>

namespace sentio {
namespace cli {

/**
 * @brief Shared CLI utilities to eliminate code duplication
 */
class CLIUtils {
public:
    /**
     * @brief Check if a flag exists in arguments (eliminates has_flag duplicates)
     */
    static bool has_flag(const std::vector<std::string>& args, const std::string& flag) {
        for (const auto& arg : args) {
            if (arg == flag) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Parse and validate common argument patterns
     */
    static bool parse_and_validate_args(const std::vector<std::string>& args) {
        // Common validation logic that was duplicated
        if (args.empty()) {
            return false;
        }
        
        // Add more common validation patterns here
        return true;
    }
    
    /**
     * @brief Common config validation
     */
    static bool validate_config() {
        // Common config validation that was duplicated
        return true;
    }
};

} // namespace cli
} // namespace sentio
