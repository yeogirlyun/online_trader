#include "cli/parameter_validator.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <limits>
#include <cmath>

namespace sentio::cli {

// ================================================================================================
// PARAMETER VALIDATOR IMPLEMENTATION
// ================================================================================================

ParameterValidator::ValidationResult ParameterValidator::validate_parameters(
    const std::string& command_name,
    const std::vector<std::string>& args,
    const RuleMap& rules) {
    
    ValidationResult result;
    
    // Parse arguments into key-value pairs
    auto parsed = parse_arguments(args);
    
    // Check required parameters
    for (const auto& [param, rule] : rules) {
        if (rule.required && parsed.find(param) == parsed.end()) {
            result.errors.push_back("Required parameter missing: " + param);
            result.suggestions[param] = format_suggestion(param, rule);
            continue;
        }
    }
    
    // Validate each provided parameter
    for (const auto& [param, value] : parsed) {
        auto rule_it = rules.find(param);
        if (rule_it == rules.end()) {
            result.warnings.push_back("Unknown parameter: " + param);
            
            // Suggest similar parameter names
            std::vector<std::string> param_names;
            for (const auto& [name, _] : rules) {
                param_names.push_back(name);
            }
            std::string suggestion = suggest_similar_value(param, param_names);
            if (!suggestion.empty()) {
                result.suggestions[param] = "Did you mean: " + suggestion + "?";
            }
            continue;
        }
        
        const auto& rule = rule_it->second;
        
        // Type validation
        if (!validate_type(value, rule)) {
            result.errors.push_back(format_error_message(param, value, rule, "type"));
            continue;
        }
        
        // Enum validation
        if (!rule.allowed_values.empty() && !validate_enum(value, rule.allowed_values)) {
            result.errors.push_back(format_error_message(param, value, rule, "enum"));
            
            std::string suggestion = suggest_similar_value(value, rule.allowed_values);
            if (!suggestion.empty()) {
                result.suggestions[param] = "Did you mean: " + suggestion + "?";
            }
            continue;
        }
        
        // Pattern validation
        if (!rule.pattern.empty() && !validate_pattern(value, rule.pattern)) {
            result.errors.push_back(format_error_message(param, value, rule, "pattern"));
            continue;
        }
        
        // Range validation for numeric types
        if ((rule.type == "int" || rule.type == "float") && 
            !validate_range(std::stod(value), rule.min_value, rule.max_value)) {
            result.errors.push_back(format_error_message(param, value, rule, "range"));
            continue;
        }
        
        // Custom validation
        if (rule.custom_validator && !rule.custom_validator(value)) {
            result.errors.push_back(format_error_message(param, value, rule, "custom"));
            continue;
        }
        
        // If we get here, the parameter is valid
        result.validated_params[param] = value;
    }
    
    // Apply defaults for missing optional parameters
    for (const auto& [param, rule] : rules) {
        if (!rule.required && !rule.default_value.empty() && 
            result.validated_params.find(param) == result.validated_params.end()) {
            result.validated_params[param] = rule.default_value;
        }
    }
    
    result.success = result.errors.empty();
    return result;
}

// ================================================================================================
// PREDEFINED VALIDATION RULES
// ================================================================================================

ParameterValidator::ValidationRule ParameterValidator::create_strategy_rule() {
    ValidationRule rule;
    rule.required = true;
    rule.type = "enum";
    rule.allowed_values = {"sgo", "xgb", "ppo", "leveraged_ppo", "momentum"};
    rule.description = "Trading strategy to use";
    rule.example = "sgo";
    return rule;
}

ParameterValidator::ValidationRule ParameterValidator::create_data_path_rule() {
    ValidationRule rule;
    rule.required = true;
    rule.type = "path";
    rule.description = "Market data file path";
    rule.example = "data/equities/QQQ_RTH_NH.csv";
    rule.custom_validator = [](const std::string& path) {
        return is_valid_file_path(path);
    };
    return rule;
}

ParameterValidator::ValidationRule ParameterValidator::create_output_path_rule() {
    ValidationRule rule;
    rule.required = false;
    rule.type = "string";
    rule.description = "Output file path (auto-generated if not specified)";
    rule.example = "my_output.jsonl";
    return rule;
}

ParameterValidator::ValidationRule ParameterValidator::create_blocks_rule() {
    ValidationRule rule;
    rule.required = false;
    rule.type = "int";
    rule.min_value = 0;
    rule.max_value = 1000;
    rule.default_value = "0";
    rule.description = "Number of blocks to process (0 = all)";
    rule.example = "20";
    return rule;
}

ParameterValidator::ValidationRule ParameterValidator::create_capital_rule() {
    ValidationRule rule;
    rule.required = false;
    rule.type = "float";
    rule.min_value = 1000.0;
    rule.max_value = 10000000.0;
    rule.default_value = "100000";
    rule.description = "Starting capital amount";
    rule.example = "100000";
    return rule;
}

ParameterValidator::ValidationRule ParameterValidator::create_threshold_rule() {
    ValidationRule rule;
    rule.required = false;
    rule.type = "float";
    rule.min_value = 0.0;
    rule.max_value = 1.0;
    rule.description = "Probability threshold (0.0 to 1.0)";
    rule.example = "0.6";
    return rule;
}

// ================================================================================================
// COMMAND-SPECIFIC RULE SETS
// ================================================================================================

ParameterValidator::RuleMap ParameterValidator::get_generate_rules() {
    RuleMap rules;
    rules["--strategy"] = create_strategy_rule();
    rules["--data"] = create_data_path_rule();
    rules["--output"] = create_output_path_rule();
    rules["--blocks"] = create_blocks_rule();
    
    ValidationRule config_rule;
    config_rule.required = false;
    config_rule.type = "path";
    config_rule.description = "Strategy configuration file";
    config_rule.example = "config/strategy.json";
    rules["--config"] = config_rule;
    
    ValidationRule format_rule;
    format_rule.required = false;
    format_rule.type = "enum";
    format_rule.allowed_values = {"jsonl"};
    format_rule.default_value = "jsonl";
    format_rule.description = "Output format";
    format_rule.example = "jsonl";
    rules["--format"] = format_rule;
    
    return rules;
}

ParameterValidator::RuleMap ParameterValidator::get_analyze_rules() {
    RuleMap rules;
    
    ValidationRule signals_rule;
    signals_rule.required = false;  // Either signals or trades required
    signals_rule.type = "path";
    signals_rule.description = "Signal file path";
    signals_rule.example = "data/signals/sgo-timestamp.jsonl";
    rules["--signals"] = signals_rule;
    
    ValidationRule trades_rule;
    trades_rule.required = false;   // Either signals or trades required
    trades_rule.type = "path";
    trades_rule.description = "Trade file path";
    trades_rule.example = "data/trades/run_id_trades.jsonl";
    rules["--trades"] = trades_rule;
    
    ValidationRule max_rule;
    max_rule.required = false;
    max_rule.type = "int";
    max_rule.min_value = 1;
    max_rule.max_value = 1000;
    max_rule.default_value = "20";
    max_rule.description = "Maximum trades to display";
    max_rule.example = "50";
    rules["--max"] = max_rule;
    
    rules["--output"] = create_output_path_rule();
    
    ValidationRule format_rule;
    format_rule.required = false;
    format_rule.type = "enum";
    format_rule.allowed_values = {"text", "json", "html"};
    format_rule.default_value = "text";
    format_rule.description = "Report format";
    format_rule.example = "text";
    rules["--format"] = format_rule;
    
    return rules;
}

ParameterValidator::RuleMap ParameterValidator::get_execute_rules() {
    RuleMap rules;
    
    ValidationRule signals_rule;
    signals_rule.required = true;
    signals_rule.type = "path";
    signals_rule.description = "Signal file path";
    signals_rule.example = "data/signals/sgo-timestamp.jsonl";
    signals_rule.custom_validator = [](const std::string& path) {
        return is_valid_file_path(path);
    };
    rules["--signals"] = signals_rule;
    
    rules["--capital"] = create_capital_rule();
    rules["--output"] = create_output_path_rule();
    
    ValidationRule config_rule;
    config_rule.required = false;
    config_rule.type = "path";
    config_rule.description = "Trading configuration file";
    config_rule.example = "config/trading.json";
    rules["--config"] = config_rule;
    
    return rules;
}

ParameterValidator::RuleMap ParameterValidator::get_pipeline_rules() {
    RuleMap rules;
    
    ValidationRule strategy_rule;
    strategy_rule.required = false;  // Required for backtest workflow
    strategy_rule.type = "enum";
    strategy_rule.allowed_values = {"sgo", "xgb", "ppo", "leveraged_ppo", "momentum"};
    strategy_rule.description = "Strategy for backtest workflow";
    strategy_rule.example = "sgo";
    rules["--strategy"] = strategy_rule;
    
    ValidationRule strategies_rule;
    strategies_rule.required = false;  // Required for compare workflow
    strategies_rule.type = "string";
    strategies_rule.pattern = "^[a-z_,\\s]+$";
    strategies_rule.description = "Comma-separated strategies for compare workflow";
    strategies_rule.example = "sgo,xgb,ppo";
    rules["--strategies"] = strategies_rule;
    
    ValidationRule data_rule = create_data_path_rule();
    data_rule.required = false;
    data_rule.default_value = "data/equities/QQQ_RTH_NH.csv";
    rules["--data"] = data_rule;
    
    rules["--blocks"] = create_blocks_rule();
    
    ValidationRule output_dir_rule;
    output_dir_rule.required = false;
    output_dir_rule.type = "string";
    output_dir_rule.default_value = ".";
    output_dir_rule.description = "Output directory";
    output_dir_rule.example = "results/";
    rules["--output"] = output_dir_rule;
    
    return rules;
}

// ================================================================================================
// UTILITY FUNCTIONS
// ================================================================================================

bool ParameterValidator::is_valid_strategy(const std::string& strategy) {
    static const std::vector<std::string> valid_strategies = {
        "sgo", "xgb", "ppo", "leveraged_ppo", "momentum"
    };
    return std::find(valid_strategies.begin(), valid_strategies.end(), strategy) != valid_strategies.end();
}

bool ParameterValidator::is_valid_file_path(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool ParameterValidator::is_valid_directory_path(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

bool ParameterValidator::is_valid_number(const std::string& value, double min_val, double max_val) {
    try {
        double num = std::stod(value);
        return num >= min_val && num <= max_val && !std::isnan(num) && !std::isinf(num);
    } catch (const std::exception&) {
        return false;
    }
}

bool ParameterValidator::is_valid_integer(const std::string& value, int min_val, int max_val) {
    try {
        int num = std::stoi(value);
        return num >= min_val && num <= max_val;
    } catch (const std::exception&) {
        return false;
    }
}

std::string ParameterValidator::generate_help_message(const std::string& command_name, const RuleMap& rules) {
    std::ostringstream ss;
    
    ss << "Parameter validation for " << command_name << ":\n\n";
    
    // Required parameters
    ss << "Required:\n";
    bool has_required = false;
    for (const auto& [param, rule] : rules) {
        if (rule.required) {
            has_required = true;
            ss << "  " << param << " <" << rule.type << ">";
            if (!rule.description.empty()) {
                ss << " - " << rule.description;
            }
            if (!rule.example.empty()) {
                ss << " (e.g., " << rule.example << ")";
            }
            ss << "\n";
        }
    }
    if (!has_required) {
        ss << "  (none)\n";
    }
    
    // Optional parameters
    ss << "\nOptional:\n";
    bool has_optional = false;
    for (const auto& [param, rule] : rules) {
        if (!rule.required) {
            has_optional = true;
            ss << "  " << param << " <" << rule.type << ">";
            if (!rule.default_value.empty()) {
                ss << " (default: " << rule.default_value << ")";
            }
            if (!rule.description.empty()) {
                ss << " - " << rule.description;
            }
            ss << "\n";
        }
    }
    if (!has_optional) {
        ss << "  (none)\n";
    }
    
    return ss.str();
}

std::string ParameterValidator::suggest_similar_value(const std::string& input, const std::vector<std::string>& valid_values) {
    if (valid_values.empty()) return "";
    
    std::string best_match;
    int min_distance = std::numeric_limits<int>::max();
    
    for (const auto& valid : valid_values) {
        int distance = levenshtein_distance(input, valid);
        if (distance < min_distance && distance <= static_cast<int>(valid.length() / 2)) {
            min_distance = distance;
            best_match = valid;
        }
    }
    
    return best_match;
}

// ================================================================================================
// PRIVATE HELPER FUNCTIONS
// ================================================================================================

std::map<std::string, std::string> ParameterValidator::parse_arguments(const std::vector<std::string>& args) {
    std::map<std::string, std::string> parsed;
    
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg.substr(0, 2) == "--") {
            if (i + 1 < args.size() && args[i + 1].substr(0, 1) != "-") {
                parsed[arg] = args[i + 1];
                ++i;  // Skip the value
            } else {
                parsed[arg] = "true";  // Flag without value
            }
        } else if (arg.substr(0, 1) == "-" && arg.length() == 2) {
            if (i + 1 < args.size() && args[i + 1].substr(0, 1) != "-") {
                parsed[arg] = args[i + 1];
                ++i;  // Skip the value
            } else {
                parsed[arg] = "true";  // Flag without value
            }
        }
        // Skip positional arguments for now
    }
    
    return parsed;
}

bool ParameterValidator::validate_type(const std::string& value, const ValidationRule& rule) {
    if (rule.type == "string") {
        return true;
    } else if (rule.type == "int") {
        return is_valid_integer(value);
    } else if (rule.type == "float") {
        return is_valid_number(value);
    } else if (rule.type == "path") {
        return !value.empty();  // Basic path validation
    } else if (rule.type == "enum") {
        return !rule.allowed_values.empty();  // Will be validated separately
    }
    
    return true;
}

bool ParameterValidator::validate_enum(const std::string& value, const std::vector<std::string>& allowed_values) {
    return std::find(allowed_values.begin(), allowed_values.end(), value) != allowed_values.end();
}

bool ParameterValidator::validate_pattern(const std::string& value, const std::string& pattern) {
    try {
        std::regex regex_pattern(pattern);
        return std::regex_match(value, regex_pattern);
    } catch (const std::exception&) {
        return false;
    }
}

bool ParameterValidator::validate_range(double value, double min_val, double max_val) {
    return value >= min_val && value <= max_val;
}

std::string ParameterValidator::format_error_message(const std::string& param, const std::string& value, 
                                                    const ValidationRule& rule, const std::string& error_type) {
    std::ostringstream ss;
    
    if (error_type == "type") {
        ss << "Invalid " << rule.type << " value for " << param << ": '" << value << "'";
    } else if (error_type == "enum") {
        ss << "Invalid value for " << param << ": '" << value << "'. ";
        ss << "Allowed values: ";
        for (size_t i = 0; i < rule.allowed_values.size(); ++i) {
            ss << rule.allowed_values[i];
            if (i < rule.allowed_values.size() - 1) ss << ", ";
        }
    } else if (error_type == "range") {
        ss << "Value for " << param << " out of range: " << value;
        if (rule.min_value != std::numeric_limits<double>::lowest()) {
            ss << " (min: " << rule.min_value;
        }
        if (rule.max_value != std::numeric_limits<double>::max()) {
            ss << ", max: " << rule.max_value;
        }
        ss << ")";
    } else if (error_type == "pattern") {
        ss << "Value for " << param << " doesn't match required pattern: '" << value << "'";
    } else if (error_type == "custom") {
        ss << "Custom validation failed for " << param << ": '" << value << "'";
    }
    
    return ss.str();
}

std::string ParameterValidator::format_suggestion(const std::string& param, const ValidationRule& rule) {
    std::ostringstream ss;
    
    ss << param << " <" << rule.type << ">";
    if (!rule.description.empty()) {
        ss << " - " << rule.description;
    }
    if (!rule.example.empty()) {
        ss << " (e.g., " << rule.example << ")";
    }
    
    return ss.str();
}

int ParameterValidator::levenshtein_distance(const std::string& s1, const std::string& s2) {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = static_cast<int>(j);
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    
    return dp[len1][len2];
}

// ================================================================================================
// VALIDATED COMMAND IMPLEMENTATION
// ================================================================================================

ParameterValidator::ValidationResult ValidatedCommand::validate_args(
    const std::vector<std::string>& args,
    const ParameterValidator::RuleMap& rules) {
    
    return ParameterValidator::validate_parameters("command", args, rules);
}

void ValidatedCommand::show_validation_errors(const ParameterValidator::ValidationResult& result) {
    if (!result.errors.empty()) {
        std::cerr << "❌ Validation Errors:\n";
        for (const auto& error : result.errors) {
            std::cerr << "  " << error << "\n";
        }
    }
    
    if (!result.warnings.empty()) {
        std::cerr << "⚠️  Warnings:\n";
        for (const auto& warning : result.warnings) {
            std::cerr << "  " << warning << "\n";
        }
    }
    
    if (!result.suggestions.empty()) {
        std::cerr << "💡 Suggestions:\n";
        for (const auto& [param, suggestion] : result.suggestions) {
            std::cerr << "  " << param << ": " << suggestion << "\n";
        }
    }
}

bool ValidatedCommand::is_help_requested(const std::vector<std::string>& args) {
    return std::find(args.begin(), args.end(), "--help") != args.end() ||
           std::find(args.begin(), args.end(), "-h") != args.end();
}

} // namespace sentio::cli
