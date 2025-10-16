#pragma once

#include "cli/command_registry.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * Command to extract features from OHLCV data and save to CSV.
 *
 * This pre-computes the feature matrix once, which can be reused
 * for multiple Optuna trials, eliminating redundant feature calculations.
 *
 * Output format: CSV with timestamp + 58 features
 * Example: timestamp,time.hour_sin,time.hour_cos,...,obv.value
 */
class ExtractFeaturesCommand : public Command {
public:
    std::string get_name() const override { return "extract-features"; }

    std::string get_description() const override {
        return "Extract features from OHLCV data and save to CSV (for Optuna caching)";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;
};

} // namespace cli
} // namespace sentio
