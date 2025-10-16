#pragma once

#include "cli/command_interface.h"
#include <string>
#include <vector>

namespace sentio {
namespace cli {

/**
 * @brief Backtest command - Run end-to-end backtest on last N blocks
 *
 * Usage:
 *   sentio_cli backtest --blocks 20
 *   sentio_cli backtest --blocks 100 --data custom.csv
 *   sentio_cli backtest --blocks 50 --warmup 200
 *
 * Features:
 * - Extracts last N blocks from binary or CSV data
 * - Integrated pipeline: generate-signals → execute-trades → analyze-trades
 * - Defaults to SPY_5years.bin for fast loading
 * - Auto-detects CSV vs BIN format
 * - No temp files created
 */
class BacktestCommand : public Command {
public:
    BacktestCommand() = default;
    virtual ~BacktestCommand() = default;

    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "backtest"; }
    std::string get_description() const override {
        return "Run end-to-end backtest on last N blocks of data";
    }
    void show_help() const override;

private:
    static constexpr int BARS_PER_BLOCK = 480;
    static constexpr const char* DEFAULT_DATA_PATH = "data/equities/SPY_5years.bin";
};

} // namespace cli
} // namespace sentio
