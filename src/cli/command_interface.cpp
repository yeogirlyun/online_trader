#include "cli/command_interface.h"
#include <iostream>
#include <algorithm>

namespace sentio {
namespace cli {

std::string Command::get_arg(const std::vector<std::string>& args, 
                            const std::string& name, 
                            const std::string& default_value) const {
    auto it = std::find(args.begin(), args.end(), name);
    if (it != args.end() && (it + 1) != args.end()) {
        return *(it + 1);
    }
    return default_value;
}

bool Command::has_flag(const std::vector<std::string>& args, 
                      const std::string& flag) const {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

void CommandDispatcher::register_command(std::unique_ptr<Command> command) {
    commands_.push_back(std::move(command));
}

int CommandDispatcher::execute(int argc, char** argv) {
    // Validate minimum arguments
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    std::string command_name = argv[1];
    Command* command = find_command(command_name);
    
    if (!command) {
        std::cerr << "Error: Unknown command '" << command_name << "'\n\n";
        show_help();
        return 1;
    }
    
    // Convert remaining arguments to vector
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    try {
        return command->execute(args);
    } catch (const std::exception& e) {
        std::cerr << "Error executing command '" << command_name << "': " << e.what() << std::endl;
        return 1;
    }
}

void CommandDispatcher::show_help() const {
    std::cout << "Usage: sentio_cli <command> [options]\n\n";
    std::cout << "Available commands:\n";
    
    for (const auto& command : commands_) {
        std::cout << "  " << command->get_name() 
                  << " - " << command->get_description() << "\n";
    }
    
    std::cout << "\nUse 'sentio_cli <command> --help' for detailed command help.\n";
}

Command* CommandDispatcher::find_command(const std::string& name) const {
    auto it = std::find_if(commands_.begin(), commands_.end(),
        [&name](const std::unique_ptr<Command>& cmd) {
            return cmd->get_name() == name;
        });
    
    return (it != commands_.end()) ? it->get() : nullptr;
}

} // namespace cli
} // namespace sentio
