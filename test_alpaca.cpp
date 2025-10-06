#include "live/alpaca_client.hpp"
#include <iostream>
#include <iomanip>

using namespace sentio;

int main() {
    // Your Alpaca Paper Account credentials
    const std::string API_KEY = "PK3NCBT07OJZJULDJR5V";
    const std::string SECRET_KEY = "cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe";

    std::cout << "=== Alpaca Paper Trading Client Test ===" << std::endl;
    std::cout << std::endl;

    AlpacaClient client(API_KEY, SECRET_KEY, true /* paper trading */);

    // Test 1: Get account information
    std::cout << "Test 1: Getting account information..." << std::endl;
    auto account = client.get_account();
    if (account) {
        std::cout << "✓ Account Number: " << account->account_number << std::endl;
        std::cout << "  Buying Power:   $" << std::fixed << std::setprecision(2) << account->buying_power << std::endl;
        std::cout << "  Cash:           $" << account->cash << std::endl;
        std::cout << "  Portfolio Value: $" << account->portfolio_value << std::endl;
        std::cout << "  Equity:         $" << account->equity << std::endl;
        std::cout << "  PDT:            " << (account->pattern_day_trader ? "Yes" : "No") << std::endl;
        std::cout << "  Trading Blocked: " << (account->trading_blocked ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "✗ Failed to get account info" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // Test 2: Check if market is open
    std::cout << "Test 2: Checking market status..." << std::endl;
    bool is_open = client.is_market_open();
    std::cout << "  Market is: " << (is_open ? "OPEN ✓" : "CLOSED") << std::endl;
    std::cout << std::endl;

    // Test 3: Get current positions
    std::cout << "Test 3: Getting current positions..." << std::endl;
    auto positions = client.get_positions();
    if (positions.empty()) {
        std::cout << "  No open positions" << std::endl;
    } else {
        std::cout << "  " << positions.size() << " open position(s):" << std::endl;
        for (const auto& pos : positions) {
            std::cout << "    " << pos.symbol << ": "
                     << pos.quantity << " shares @ $" << pos.avg_entry_price
                     << " (Current: $" << pos.current_price << ", "
                     << "P/L: $" << pos.unrealized_pl << " / "
                     << std::setprecision(2) << pos.unrealized_pl_pct * 100 << "%)" << std::endl;
        }
    }
    std::cout << std::endl;

    // Test 4: Try to get position for QQQ
    std::cout << "Test 4: Checking for QQQ position..." << std::endl;
    auto qqq_pos = client.get_position("QQQ");
    if (qqq_pos) {
        std::cout << "  ✓ QQQ position exists: " << qqq_pos->quantity << " shares" << std::endl;
    } else {
        std::cout << "  No QQQ position" << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== All Tests Complete ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Your Alpaca Paper Trading account is ready!" << std::endl;
    std::cout << "Account value: $" << std::fixed << std::setprecision(2) << account->portfolio_value << std::endl;
    std::cout << std::endl;

    return 0;
}
