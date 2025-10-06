#pragma once

#include "cli/command_interface.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>

namespace sentio::cli {

/**
 * @brief Centralized command registry with aliases and deprecation support
 * 
 * Manages all CLI commands with support for:
 * - Command registration and discovery
 * - Alias management for backward compatibility
 * - Deprecation warnings and migration guidance
 * - Dynamic command loading
 * - Help generation
 */
class CommandRegistry {
public:
    struct CommandInfo {
        std::shared_ptr<Command> command;
        std::string description;
        std::string category;
        std::string version;
        bool deprecated;
        std::string deprecation_message;
        std::string replacement_command;
        std::vector<std::string> aliases;
        std::vector<std::string> tags;
        
        CommandInfo() : category("General"), version("1.0"), deprecated(false) {}
    };
    
    struct AliasInfo {
        std::string target_command;
        bool deprecated;
        std::string deprecation_message;
        std::string migration_guide;
        
        AliasInfo() : deprecated(false) {}
    };
    
    /**
     * @brief Get singleton instance
     */
    static CommandRegistry& instance();
    
    /**
     * @brief Register a command with metadata
     */
    void register_command(const std::string& name, 
                         std::shared_ptr<Command> command,
                         const CommandInfo& info = CommandInfo{});
    
    /**
     * @brief Register command alias
     */
    void register_alias(const std::string& alias, 
                       const std::string& target_command,
                       const AliasInfo& info = AliasInfo{});
    
    /**
     * @brief Mark command as deprecated
     */
    void deprecate_command(const std::string& name, 
                          const std::string& replacement = "",
                          const std::string& message = "");
    
    /**
     * @brief Get command by name (handles aliases and deprecation)
     */
    std::shared_ptr<Command> get_command(const std::string& name);
    
    /**
     * @brief Check if command exists
     */
    bool has_command(const std::string& name) const;
    
    /**
     * @brief Get all available commands
     */
    std::vector<std::string> get_available_commands() const;
    
    /**
     * @brief Get commands by category
     */
    std::vector<std::string> get_commands_by_category(const std::string& category) const;
    
    /**
     * @brief Get command information
     */
    const CommandInfo* get_command_info(const std::string& name) const;
    
    /**
     * @brief Show comprehensive help
     */
    void show_help() const;
    
    /**
     * @brief Show help for specific category
     */
    void show_category_help(const std::string& category) const;
    
    /**
     * @brief Show migration guide for deprecated commands
     */
    void show_migration_guide() const;
    
    /**
     * @brief Execute command with enhanced error handling
     */
    int execute_command(const std::string& name, const std::vector<std::string>& args);
    
    /**
     * @brief Suggest similar commands for typos
     */
    std::vector<std::string> suggest_commands(const std::string& input) const;
    
    /**
     * @brief Initialize with default commands
     */
    void initialize_default_commands();
    
    /**
     * @brief Setup canonical command aliases
     */
    void setup_canonical_aliases();
    
private:
    CommandRegistry() = default;
    
    std::map<std::string, CommandInfo> commands_;
    std::map<std::string, AliasInfo> aliases_;
    
    void show_deprecation_warning(const std::string& command_name, const CommandInfo& info);
    void show_alias_warning(const std::string& alias, const AliasInfo& info);
    std::string format_command_list(const std::vector<std::string>& commands) const;
    int levenshtein_distance(const std::string& s1, const std::string& s2) const;
};

/**
 * @brief Enhanced command dispatcher with registry integration
 * 
 * Replaces the basic CommandDispatcher with advanced features.
 */
class EnhancedCommandDispatcher {
public:
    /**
     * @brief Execute command with full registry support
     */
    static int execute(int argc, char** argv);
    
    /**
     * @brief Show enhanced help with categories and examples
     */
    static void show_help();
    
    /**
     * @brief Show version information
     */
    static void show_version();
    
    /**
     * @brief Show usage examples
     */
    static void show_usage_examples();
    
    /**
     * @brief Handle special global flags
     */
    static bool handle_global_flags(const std::vector<std::string>& args);
    
private:
    static void show_command_not_found_help(const std::string& command_name);
    static std::string get_version_string();
};

/**
 * @brief Command factory for dynamic command creation
 * 
 * Enables lazy loading and plugin-style command registration.
 */
class CommandFactory {
public:
    using CommandCreator = std::function<std::shared_ptr<Command>()>;
    
    /**
     * @brief Register command factory function
     */
    static void register_factory(const std::string& name, CommandCreator creator);
    
    /**
     * @brief Create command instance
     */
    static std::shared_ptr<Command> create_command(const std::string& name);
    
    /**
     * @brief Register all built-in commands
     */
    static void register_builtin_commands();
    
private:
    static std::map<std::string, CommandCreator> factories_;
};

} // namespace sentio::cli
