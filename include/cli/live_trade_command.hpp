#ifndef SENTIO_LIVE_TRADE_COMMAND_HPP
#define SENTIO_LIVE_TRADE_COMMAND_HPP

#include "cli/command_interface.h"
#include <vector>
#include <string>

namespace sentio {
namespace cli {

/**
 * Live Trading Command - Run OnlineTrader v1.0 with paper account
 *
 * Connects to Alpaca Paper Trading and Polygon for live trading.
 * Trades SPY/SDS/SPXL/SH during regular hours with comprehensive logging.
 */
class LiveTradeCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "live-trade"; }
    std::string get_description() const override {
        return "Run OnlineTrader v1.0 with paper account (SPY/SPXL/SH/SDS)";
    }
    void show_help() const override;
};

} // namespace cli
} // namespace sentio

#endif // SENTIO_LIVE_TRADE_COMMAND_HPP
