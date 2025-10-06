#pragma once

#include "cli/command_interface.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * CLI command to run online learning strategies
 * Supports: online-60sa, online-8detector, online-25intraday
 */
class OnlineCommand : public Command {
public:
    std::string get_name() const override;
    std::string get_description() const override;
    std::vector<std::string> get_usage() const;
    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    int run_online_strategy(
        const std::string& strategy_name,
        const std::string& data_path,
        const std::string& output_path,
        int max_bars = 0
    );
    
    void print_usage() const;
};

} // namespace cli
} // namespace sentio
