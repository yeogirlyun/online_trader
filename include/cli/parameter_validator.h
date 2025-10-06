#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>

namespace sentio::cli {

/**
 * @brief Comprehensive parameter validation system for CLI commands
 * 
 * Provides consistent validation across all commands with detailed error
 * messages and suggestions for correct usage.
 */
class ParameterValidator {
public:
    struct ValidationRule {
        bool required = false;
        std::string type = "string";        // string, int, float, path, enum, email, url
        std::vector<std::string> allowed_values;
        std::string default_value;
        double min_value = std::numeric_limits<double>::lowest();
        double max_value = std::numeric_limits<double>::max();
        std::string pattern;                // Regex pattern for string validation
        std::function<bool(const std::string&)> custom_validator;
        std::string description;            // Help text for the parameter
        std::string example;                // Example value
    };
    
    using RuleMap = std::map<std::string, ValidationRule>;
    
    struct ValidationResult {
        bool success = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::map<std::string, std::string> suggestions;
        std::map<std::string, std::string> validated_params;
    };
    
    /**
     * @brief Validate command parameters against rules
     */
    static ValidationResult validate_parameters(
        const std::string& command_name,
        const std::vector<std::string>& args,
        const RuleMap& rules
    );
    
    /**
     * @brief Get predefined validation rules for common parameter types
     */
    static ValidationRule create_strategy_rule();
    static ValidationRule create_data_path_rule();
    static ValidationRule create_output_path_rule();
    static ValidationRule create_blocks_rule();
    static ValidationRule create_capital_rule();
    static ValidationRule create_threshold_rule();
    
    /**
     * @brief Get validation rules for specific commands
     */
    static RuleMap get_generate_rules();
    static RuleMap get_analyze_rules();
    static RuleMap get_execute_rules();
    static RuleMap get_pipeline_rules();
    
    /**
     * @brief Utility functions for common validations
     */
    static bool is_valid_strategy(const std::string& strategy);
    static bool is_valid_file_path(const std::string& path);
    static bool is_valid_directory_path(const std::string& path);
    static bool is_valid_number(const std::string& value, double min_val = std::numeric_limits<double>::lowest(), 
                               double max_val = std::numeric_limits<double>::max());
    static bool is_valid_integer(const std::string& value, int min_val = std::numeric_limits<int>::min(),
                                int max_val = std::numeric_limits<int>::max());
    
    /**
     * @brief Generate helpful error messages and suggestions
     */
    static std::string generate_help_message(const std::string& command_name, const RuleMap& rules);
    static std::string suggest_similar_value(const std::string& input, const std::vector<std::string>& valid_values);
    
private:
    static std::map<std::string, std::string> parse_arguments(const std::vector<std::string>& args);
    static bool validate_type(const std::string& value, const ValidationRule& rule);
    static bool validate_enum(const std::string& value, const std::vector<std::string>& allowed_values);
    static bool validate_pattern(const std::string& value, const std::string& pattern);
    static bool validate_range(double value, double min_val, double max_val);
    
    static std::string format_error_message(const std::string& param, const std::string& value, 
                                          const ValidationRule& rule, const std::string& error_type);
    static std::string format_suggestion(const std::string& param, const ValidationRule& rule);
    
    // Levenshtein distance for string similarity
    static int levenshtein_distance(const std::string& s1, const std::string& s2);
};

/**
 * @brief Enhanced command interface with built-in validation
 * 
 * Base class that provides automatic parameter validation for commands.
 */
class ValidatedCommand {
public:
    virtual ~ValidatedCommand() = default;
    
    struct ParsedArgs {
        std::map<std::string, std::string> params;
        std::vector<std::string> positional;
        bool help_requested = false;
    };
    
protected:
    /**
     * @brief Parse and validate arguments using predefined rules
     */
    ParameterValidator::ValidationResult validate_args(
        const std::vector<std::string>& args,
        const ParameterValidator::RuleMap& rules
    );
    
    /**
     * @brief Show validation errors with helpful suggestions
     */
    void show_validation_errors(const ParameterValidator::ValidationResult& result);
    
    /**
     * @brief Get parameter value with type conversion
     */
    template<typename T>
    T get_validated_param(const std::map<std::string, std::string>& params, 
                         const std::string& name, const T& default_value = T{});
    
    /**
     * @brief Check if help was requested
     */
    bool is_help_requested(const std::vector<std::string>& args);
};

// Template implementation
template<typename T>
T ValidatedCommand::get_validated_param(const std::map<std::string, std::string>& params, 
                                       const std::string& name, const T& default_value) {
    auto it = params.find(name);
    if (it == params.end()) {
        return default_value;
    }
    
    const std::string& value = it->second;
    
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_same_v<T, int>) {
        return std::stoi(value);
    } else if constexpr (std::is_same_v<T, double>) {
        return std::stod(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value == "true" || value == "1" || value == "yes";
    } else {
        static_assert(std::is_same_v<T, std::string>, "Unsupported type for parameter conversion");
        return default_value;
    }
}

} // namespace sentio::cli

