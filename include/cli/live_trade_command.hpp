#ifndef SENTIO_LIVE_TRADE_COMMAND_HPP
#define SENTIO_LIVE_TRADE_COMMAND_HPP

#include <vector>
#include <string>

namespace sentio {

/**
 * Live Trading Command - Run OnlineTrader v1.0 with paper account
 *
 * Connects to Alpaca Paper Trading and Polygon for live trading.
 * Trades SPY/SDS/SPXL/SH during regular hours with comprehensive logging.
 */
class LiveTradeCommand {
public:
    int execute(const std::vector<std::string>& args);
};

} // namespace sentio

#endif // SENTIO_LIVE_TRADE_COMMAND_HPP
