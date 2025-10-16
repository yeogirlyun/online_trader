#!/usr/bin/env python3
import requests
import json
import os

# Load credentials from environment (set via config.env)
API_KEY = os.environ.get("ALPACA_PAPER_API_KEY")
SECRET_KEY = os.environ.get("ALPACA_PAPER_SECRET_KEY")
BASE_URL = "https://paper-api.alpaca.markets"

if not API_KEY or not SECRET_KEY:
    print("ERROR: ALPACA_PAPER_API_KEY and ALPACA_PAPER_SECRET_KEY must be set")
    print("Run: source config.env")
    exit(1)

headers = {
    "APCA-API-KEY-ID": API_KEY,
    "APCA-API-SECRET-KEY": SECRET_KEY
}

# Get account info
print("=" * 80)
print("ALPACA PAPER TRADING ACCOUNT STATUS")
print("=" * 80)
print()

try:
    account_response = requests.get(f"{BASE_URL}/v2/account", headers=headers)
    account = account_response.json()

    print("ACCOUNT BALANCE:")
    print(f"  Portfolio Value: ${float(account['portfolio_value']):,.2f}")
    print(f"  Cash:            ${float(account['cash']):,.2f}")
    print(f"  Buying Power:    ${float(account['buying_power']):,.2f}")
    print()

    # Get positions
    positions_response = requests.get(f"{BASE_URL}/v2/positions", headers=headers)
    positions = positions_response.json()

    print("CURRENT POSITIONS:")
    if positions:
        for pos in positions:
            symbol = pos['symbol']
            qty = float(pos['qty'])
            entry_price = float(pos['avg_entry_price'])
            current_price = float(pos['current_price'])
            unrealized_pl = float(pos['unrealized_pl'])
            unrealized_plpc = float(pos['unrealized_plpc']) * 100
            market_value = float(pos['market_value'])

            print(f"  {symbol}:")
            print(f"    Quantity:      {qty:,.0f} shares")
            print(f"    Entry Price:   ${entry_price:.2f}")
            print(f"    Current Price: ${current_price:.2f}")
            print(f"    Market Value:  ${market_value:,.2f}")
            print(f"    Unrealized P&L: ${unrealized_pl:+,.2f} ({unrealized_plpc:+.2f}%)")
            print()
    else:
        print("  No open positions")
        print()

    # Get today's P&L
    print("TODAY'S PERFORMANCE:")
    equity = float(account['equity'])
    last_equity = float(account['last_equity'])
    today_pl = equity - last_equity
    today_plpc = (today_pl / last_equity) * 100 if last_equity > 0 else 0

    print(f"  Today's P&L: ${today_pl:+,.2f} ({today_plpc:+.2f}%)")
    print()

except Exception as e:
    print(f"Error querying Alpaca API: {e}")
    print()

print("=" * 80)
