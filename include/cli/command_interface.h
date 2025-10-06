#pragma once

#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Abstract base class for all CLI commands
 * 
 * This interface defines the contract for all command implementations,
 * enabling clean separation of concerns and testability.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /**
     * @brief Execute the command with given arguments
     * @param args Command-line arguments (excluding program name and command)
     * @return Exit code (0 for success, non-zero for error)
     */
    virtual int execute(const std::vector<std::string>& args) = 0;
    
    /**
     * @brief Get command name for registration
     * @return Command name as used in CLI
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get command description for help text
     * @return Brief description of what the command does
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Show detailed help for this command
     */
    virtual void show_help() const = 0;

protected:
    /**
     * @brief Helper to extract argument value by name
     * @param args Argument vector
     * @param name Argument name (e.g., "--dataset")
     * @param default_value Default value if not found
     * @return Argument value or default
     */
    std::string get_arg(const std::vector<std::string>& args, 
                       const std::string& name, 
                       const std::string& default_value = "") const;
    
    /**
     * @brief Check if flag is present in arguments
     * @param args Argument vector
     * @param flag Flag name (e.g., "--verbose")
     * @return True if flag is present
     */
    bool has_flag(const std::vector<std::string>& args, 
                  const std::string& flag) const;
};

/**
 * @brief Command dispatcher that manages and executes commands
 */
class CommandDispatcher {
public:
    /**
     * @brief Register a command with the dispatcher
     * @param command Unique pointer to command implementation
     */
    void register_command(std::unique_ptr<Command> command);
    
    /**
     * @brief Execute command based on arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit code
     */
    int execute(int argc, char** argv);
    
    /**
     * @brief Show general help with all available commands
     */
    void show_help() const;

private:
    std::vector<std::unique_ptr<Command>> commands_;
    
    /**
     * @brief Find command by name
     * @param name Command name
     * @return Pointer to command or nullptr if not found
     */
    Command* find_command(const std::string& name) const;
};

} // namespace cli
} // namespace sentio
