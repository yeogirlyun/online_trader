# Mock Alpaca Server for Live Trading Testing

## Overview

We've created a complete mock server that simulates Alpaca Paper Trading API and Polygon WebSocket to test live trading locally without connecting to real Alpaca.

## Why This Is Critical

Yesterday's live trading session produced **zero trades** because `live-trade` command had a placeholder `generate_signal()` function returning hardcoded neutral signals (0.5).

**The bug has been fixed**, but we need to verify the fix works correctly before connecting to real Alpaca again.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Live Trading Test Setup                    │
└─────────────────────────────────────────────────────────────┘

┌──────────────────┐           ┌──────────────────────────┐
│  sentio_cli      │    HTTP   │  Mock Alpaca Server      │
│  live-trade      ├──────────►│  (Python)                │
│                  │           │                          │
│  - Connects to   │           │  REST API:               │
│    mock server   │           │  - GET /v2/account       │
│  - Same code as  │           │  - GET /v2/positions     │
│    real Alpaca   │           │  - POST /v2/orders       │
│  - Uses actual   │           │  - DELETE /v2/orders/:id │
│    strategy      │           │                          │
└────────┬─────────┘           └──────────┬───────────────┘
         │                                 │
         │ WebSocket                       │
         └─────────────────────────────────┘
                      │
                      ▼
         ┌────────────────────────────┐
         │  Replays Historical Data   │
         │  - Loads MarS CSV data     │
         │  - Broadcasts 1-min bars   │
         │  - Simulates real-time     │
         │  - Adjustable speed        │
         └────────────────────────────┘
```

## Components

### 1. Mock Server (`tools/mock_alpaca_server.py`)

**Features:**
- **HTTP REST API** mimicking Alpaca:
  - `/v2/account` - Returns account info (cash, portfolio value, buying power)
  - `/v2/positions` - Returns all open positions
  - `/v2/orders` - Submit market orders, get order status
- **WebSocket Server** mimicking Polygon:
  - Broadcasts 1-minute bars for SPY, SPXL, SH, SDS
  - Replays historical data from MarS CSV files
  - Configurable replay speed (e.g., 600x = 1 hour in 6 seconds)
- **Portfolio Simulation**:
  - Tracks positions, cash, P&L in real-time
  - Updates market values as bars arrive
  - Validates order requests (buying power checks)
  - Logs all trades and account state

**Usage:**
```bash
python3 tools/mock_alpaca_server.py \
    --data data/equities/SPY_4blocks.csv \
    --port 8000 \
    --ws-port 8765 \
    --symbols SPY,SPXL,SH,SDS \
    --capital 100000 \
    --speed 600
```

### 2. Integration Test Script (`tools/test_live_trading_mock.sh`)

**What it does:**
1. Starts mock Alpaca server
2. Verifies REST API connectivity
3. Documents how to run `live-trade` command against mock server
4. Monitors logs and output

**Current limitation:**
The `live-trade` command has **hardcoded URLs** for Alpaca/Polygon. We need to add CLI parameters to make them configurable.

## Required Changes to Live Trading

To complete the testing framework, we need to modify `src/cli/live_trade_command.cpp`:

### Add CLI Parameters:

```cpp
--alpaca-url <url>     Custom Alpaca API URL (default: https://paper-api.alpaca.markets)
--polygon-url <url>    Custom Polygon WebSocket URL (default: production)
--duration <seconds>   Run for specified duration then exit (for testing)
```

### Benefits:

1. **Local Testing**: Test against mock server without any network calls
2. **CI/CD Integration**: Automated testing in build pipeline
3. **Debugging**: Run with controlled data to reproduce issues
4. **Speed**: Test hours of trading in seconds (600x speed)
5. **Safety**: Never risk real money during development

## Testing Workflow

### Step 1: Start Mock Server

```bash
# Terminal 1
cd /Volumes/ExternalSSD/Dev/C++/online_trader
python3 tools/mock_alpaca_server.py \
    --data data/equities/SPY_4blocks.csv \
    --port 8000 \
    --ws-port 8765 \
    --speed 600
```

### Step 2: Run Live Trading (Once URLs are configurable)

```bash
# Terminal 2
./build/sentio_cli live-trade \
    --alpaca-url http://localhost:8000 \
    --polygon-url ws://localhost:8765 \
    --duration 120 \
    --log-dir data/tmp/mock_test_logs
```

### Step 3: Verify Results

Check logs in `data/tmp/mock_test_logs/`:
- `system_*.log` - Connection status, strategy initialization
- `signals_*.log` - Generated signals (should NOT all be 0.5!)
- `trades_*.log` - Executed trades
- `decisions_*.log` - Trading decisions with reasons
- `positions_*.log` - Position changes

### Success Criteria:

✅ **Signal Generation Works**
- Signals are NOT all 0.5 (neutral)
- Probability varies based on market conditions
- Uses actual OnlineEnsemble strategy

✅ **Trading Execution Works**
- Orders submitted to mock server
- Positions updated correctly
- Cash balance changes appropriately
- P&L tracked accurately

✅ **Decision Logic Works**
- Takes positions based on signal thresholds
- Respects profit targets (2%)
- Respects stop losses (-1.5%)
- Enforces hold periods (3 bars min)

✅ **No Crashes or Errors**
- Runs for full duration
- Handles WebSocket reconnections
- Properly logs all events

## What We Fixed Today

**Critical Bug in `src/cli/live_trade_command.cpp:353-365`:**

**Before (Broken):**
```cpp
Signal generate_signal(const Bar& bar) {
    // TODO: Actually call strategy.generate_signal()
    // For now, placeholder
    Signal signal;
    signal.probability = 0.5;  // Neutral - ALWAYS!
    signal.confidence = 0.0;
    signal.prediction = "NEUTRAL";
    // ... all hardcoded to neutral
    return signal;
}
```

**After (Fixed):**
```cpp
Signal generate_signal(const Bar& bar) {
    // Call OnlineEnsemble strategy to generate real signal
    auto strategy_signal = strategy_.generate_signal(bar);

    Signal signal;
    signal.probability = strategy_signal.probability;  // REAL probability!

    // Map signal type to prediction string
    if (strategy_signal.signal_type == SignalType::LONG) {
        signal.prediction = "LONG";
    } else if (strategy_signal.signal_type == SignalType::SHORT) {
        signal.prediction = "SHORT";
    } else {
        signal.prediction = "NEUTRAL";
    }

    signal.confidence = std::abs(strategy_signal.probability - 0.5) * 2.0;
    // ... use actual strategy output
    return signal;
}
```

**Impact:**
- Before: Always returned 0.5 (neutral) → no trades
- After: Returns actual strategy predictions → will generate trades

## Next Steps

1. **Add CLI parameters** to `live_trade_command.cpp`:
   - `--alpaca-url`
   - `--polygon-url`
   - `--duration`

2. **Run integration test:**
   ```bash
   tools/test_live_trading_mock.sh
   ```

3. **Verify signal generation:**
   - Check that signals vary (not all 0.5)
   - Verify LONG/SHORT predictions occur
   - Confirm trades are executed

4. **Run extended test:**
   - Use 20 blocks of data
   - Run for 10+ minutes
   - Verify consistent behavior

5. **Deploy to Alpaca paper trading:**
   - Once mock tests pass
   - Monitor first 30 minutes closely
   - Verify trades match expectations

## Why Mock Testing Matters

**Yesterday's Failure:**
- Connected to real Alpaca
- Ran for hours with no trades
- Wasted time and opportunity
- Didn't catch the bug early

**With Mock Testing:**
- Catch bugs in seconds, not hours
- Test without network dependencies
- Reproduce issues reliably
- Verify fixes immediately
- Safe to experiment and iterate

## Files Created

1. `tools/mock_alpaca_server.py` - Mock Alpaca + Polygon server
2. `tools/test_live_trading_mock.sh` - Integration test script
3. `MOCK_SERVER_TESTING.md` - This documentation

**Git commit:** `6e565c9` - "CRITICAL FIX: Live trading now calls actual strategy"

---

**Status:** ✅ Bug fixed, mock server ready, waiting for URL parameters to be added to `live-trade` command.
