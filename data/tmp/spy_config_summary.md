# SPY Live Trading Configuration Summary

## Changes Made: QQQ → SPY Migration

### Reason for Switch
Since we have direct Alpaca IEX WebSocket access, SPY is the better choice:
- **Higher liquidity**: SPY avg volume ~80M vs QQQ ~50M daily
- **Tighter spreads**: Lower transaction costs
- **S&P 500 tracking**: Broader market exposure vs NASDAQ-100

### Files Modified

#### 1. `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/live_trade_command.cpp`

**Symbol Map** (lines 70-76):
```cpp
// SPY trading configuration (maps to sentio PSM states)
symbol_map_ = {
    {"SPY", "SPY"},      // Base 1x (S&P 500 ETF)
    {"SPXL", "SPXL"},    // Bull 3x Leveraged
    {"SH", "SH"},        // Bear -1x Inverse
    {"SDS", "SDS"}       // Bear -2x Inverse
};
```

**Log Message** (line 81):
```cpp
log_system("Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)");
```

**Subscription** (lines 105-111):
```cpp
// Subscribe to symbols (SPY instruments)
std::vector<std::string> symbols = {"SPY", "SPXL", "SH", "SDS"};
if (!polygon_.subscribe(symbols)) {
    log_error("Failed to subscribe to symbols");
    return;
}
log_system("✓ Subscribed to SPY, SPXL, SH, SDS");
```

**Bar Callback** (line 123):
```cpp
if (symbol == "SPY") {  // Only process on SPY bars (trigger for multi-instrument PSM)
    on_new_bar(bar);
}
```

**Warmup Data** (line 249):
```cpp
std::string warmup_file = "data/equities/SPY_RTH_NH.csv";
```

#### 2. `/Volumes/ExternalSSD/Dev/C++/online_trader/src/live/polygon_websocket.cpp`

**WebSocket Subscription** (lines 61-70):
```cpp
// Subscribe to minute bars for SPY instruments
// Alpaca format: {"action":"subscribe","bars":["SPY","SPXL","SH","SDS"]}
json sub;
sub["action"] = "subscribe";
sub["bars"] = json::array({"SPY", "SPXL", "SH", "SDS"});
std::string sub_msg = sub.dump();
unsigned char sub_buf[LWS_PRE + 512];
memcpy(&sub_buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
lws_write(wsi, &sub_buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
std::cout << "→ Subscribed to bars: SPY, SPXL, SH, SDS" << std::endl;
```

**Callback Trigger** (lines 128-131):
```cpp
// Callback (only for SPY to trigger strategy)
if (ctx->callback && symbol == "SPY") {
    ctx->callback(symbol, bar);
}
```

## SPY Instrument Details

| Symbol | Type | Leverage | Description |
|--------|------|----------|-------------|
| **SPY** | Base | 1x | SPDR S&P 500 ETF Trust - Most liquid ETF globally |
| **SPXL** | Bull | 3x | Direxion Daily S&P 500 Bull 3X Shares |
| **SH** | Bear | -1x | ProShares Short S&P 500 |
| **SDS** | Bear | -2x | ProShares UltraShort S&P 500 |

## OnlineEnsemble Strategy v1.0 Configuration

**Target Performance**: 0.6086% MRB (10.5% monthly, 125% annual)

### Thresholds (Asymmetric):
- **Buy Threshold**: 0.55
- **Sell Threshold**: 0.45
- **Neutral Zone**: 0.10

### EWRLS Parameters:
- **Lambda**: 0.995
- **Warmup Samples**: 960 bars (2 trading days of 1-min data)

### Features:
- ✅ Bollinger Bands amplification (factor: 0.10)
- ✅ Adaptive learning
- ✅ Threshold calibration

### Risk Management:
- **Profit Target**: +2.0%
- **Stop Loss**: -1.5%
- **Min Hold**: 3 bars (3 minutes)
- **Max Hold**: 100 bars (100 minutes)

## System Architecture

### Data Flow:
1. **Alpaca IEX WebSocket** → Receives minute bars for SPY, SPXL, SH, SDS
2. **Bar Storage** → Stores all 4 symbols' bars in memory
3. **SPY Trigger** → Only SPY bars trigger strategy processing
4. **OnlineEnsemble** → Generates signal (LONG/SHORT/NEUTRAL)
5. **Position State Machine** → Maps signal to instrument (SPY/SPXL/SH/SDS)
6. **Alpaca API** → Executes paper trade via REST API

### Position State Machine States:
```
CASH_ONLY (0x)
  ↓ LONG signal → BUY_1X (SPY)
  ↓ SHORT signal → SELL_1X (SH)

BUY_1X (SPY)
  ↓ Strong LONG → BUY_2X (upgrade)
  ↓ NEUTRAL/SHORT → CASH_ONLY

BUY_2X (SPXL 3x, but limited to 2x exposure)
  ↓ NEUTRAL/SHORT → BUY_1X (downgrade)
  
SELL_1X (SH -1x)
  ↓ Strong SHORT → SELL_2X (upgrade)
  ↓ NEUTRAL/LONG → CASH_ONLY

SELL_2X (SDS -2x)
  ↓ NEUTRAL/LONG → SELL_1X (downgrade)
```

## Warmup Data

**File**: `data/equities/SPY_RTH_NH.csv`  
**Size**: 101 KB  
**Content**: Last 960 1-minute bars (2 trading days)  
**Format**: CSV with timestamp, open, high, low, close, volume

## Alpaca Configuration

**WebSocket URL**: `wss://stream.data.alpaca.markets:443/v2/iex`  
**Data Feed**: IEX (free tier, ~3% market volume)  
**Account Type**: Paper Trading  
**Starting Capital**: $100,000

## Next Steps

1. ✅ SPY configuration completed
2. ⏳ Await market open (9:30 AM ET)
3. ⏳ Monitor real-time SPY bar reception
4. ⏳ Verify signal generation and trade execution
5. ⏳ Track performance metrics and log files

## Log Files Location

All live trading logs will be saved to:
```
logs/live_trading/
├── signals_YYYYMMDD_HHMMSS.jsonl      (strategy signals)
├── decisions_YYYYMMDD_HHMMSS.jsonl    (PSM decisions)
├── trades_YYYYMMDD_HHMMSS.jsonl       (executed trades)
└── positions_YYYYMMDD_HHMMSS.jsonl    (position updates)
```

## Expected Market Data Messages

**Alpaca IEX WebSocket** will send messages in this format:
```json
[
  {
    "T": "b",
    "S": "SPY",
    "o": 560.50,
    "h": 560.75,
    "l": 560.45,
    "c": 560.70,
    "v": 125000,
    "t": "2025-10-07T09:31:00Z"
  }
]
```

Each minute, the system will receive 4 bar messages (SPY, SPXL, SH, SDS), but only SPY will trigger strategy processing.
