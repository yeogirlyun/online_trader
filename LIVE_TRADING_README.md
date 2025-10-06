# OnlineTrader v1.0 - Live Paper Trading System

## Overview

Complete live trading system for OnlineEnsemble Strategy v1.0 with SPY instruments.

**Performance**: 0.6086% MRB (10.5% monthly, 125% annual) on backtests

## Architecture

```
Polygon Proxy → Live Data → OnlineEnsemble v1.0 → PSM → Alpaca Paper Trading
```

## Instruments

- **SPY** (1x) - Base S&P 500 ETF
- **SPXL** (3x) - Bull leveraged
- **SH** (-1x) - Bear inverse
- **SDS** (-2x) - Bear leveraged

## Trading Rules

### Hours
- **Trading**: 9:30am - 3:58pm ET (regular hours only)
- **No Pre-Market**: No trading before 9:30am
- **No After-Hours**: No trading after 4:00pm
- **EOD Liquidation**: Close all positions at 3:58pm (2 min before close)
- **No Overnight Positions**: 100% cash every night

### Strategy (v1.0)
- **Asymmetric Thresholds**: LONG ≥0.55, SHORT ≤0.45
- **Minimum Hold**: 3 bars (3 minutes)
- **Profit Target**: +2%
- **Stop Loss**: -1.5%
- **Position Sizing**: 100% capital per state

### PSM State Mapping

| Probability | State | SPY Instruments |
|-------------|-------|-----------------|
| ≥0.68 | TQQQ_ONLY | SPXL (3x bull) |
| ≥0.60 | QQQ_TQQQ | SPY + SPXL |
| ≥0.55 | QQQ_ONLY | SPY (1x) |
| 0.49-0.55 | CASH_ONLY | 100% cash |
| ≥0.45 | PSQ_ONLY | SH (-1x) |
| ≥0.35 | PSQ_SQQQ | SH + SDS |
| <0.32 | SQQQ_ONLY | SDS (-2x) |

## Configuration

**Environment Setup**:
```bash
# Load credentials from config.env
source config.env
```

**Credentials in config.env**:
- Polygon API Key: `fE68VnU8xUR7NQFMAM4yl3cULTHbigrb`
- Alpaca Paper Key: `PKYTG7BKKLMUUMKXYA9A`
- Alpaca Paper Secret: `ud2IqjE8aKMSF3DaGign1GkvFltIUdfli1oCs9Rb`

**Fallback Account** (if env not sourced):
- API Key: `PK3NCBT07OJZJULDJR5V`
- Secret Key: `cEZcHNAReKZcjsH5j9cPYgOtI5rvdra1QhVCVBJe`
- Account: PA3FOCO5XA55

**Both accounts have**:
- Starting Capital: $100,000
- Buying Power: $200,000 (2x margin)
- Pattern Day Trader: No
- Status: Active ✓

## Market Data

**Polygon.io Official WebSocket**:
- URL: `wss://socket.polygon.io/stocks`
- API Key: From config.env
- Data: Real-time 1-minute aggregated bars
- Symbols: SPY, SPXL, SH, SDS

**Alternative (if engineer provides proxy)**:
- Can use custom proxy URL
- Auth: "Izene@123"

## Comprehensive Logging

All data logged to `logs/live_trading/` with timestamps:

### 1. System Log (`system_YYYYMMDD_HHMMSS.log`)
Human-readable event log:
- Connection status
- Market hours tracking
- State transitions
- Errors and warnings

### 2. Signals Log (`signals_YYYYMMDD_HHMMSS.jsonl`)
Every signal generated:
```json
{
  "timestamp": "2025-10-07 10:30:00",
  "bar_timestamp_ms": 1728308400000,
  "probability": 0.5823,
  "confidence": 0.1646,
  "prediction": "LONG",
  "prob_1bar": 0.5823,
  "prob_5bar": 0.5891,
  "prob_10bar": 0.5964
}
```

### 3. Decisions Log (`decisions_YYYYMMDD_HHMMSS.jsonl`)
Every trading decision:
```json
{
  "timestamp": "2025-10-07 10:30:00",
  "should_trade": true,
  "current_state": "CASH_ONLY",
  "target_state": "QQQ_ONLY",
  "reason": "STATE_TRANSITION (prob=0.5823)",
  "current_equity": 100523.45,
  "position_pnl_pct": 0.005234,
  "bars_held": 0
}
```

### 4. Trades Log (`trades_YYYYMMDD_HHMMSS.jsonl`)
Every order executed:
```json
{
  "timestamp": "2025-10-07 10:30:00",
  "order_id": "abc123",
  "symbol": "SPY",
  "side": "buy",
  "quantity": 195.42,
  "price": 512.34,
  "filled_qty": 195.42,
  "filled_avg_price": 512.35,
  "status": "filled",
  "reason": "STATE_TRANSITION"
}
```

### 5. Positions Log (`positions_YYYYMMDD_HHMMSS.jsonl`)
Portfolio snapshot after each bar:
```json
{
  "timestamp": "2025-10-07 10:30:00",
  "cash": 523.45,
  "buying_power": 200523.45,
  "portfolio_value": 100523.45,
  "equity": 100523.45,
  "total_return": 523.45,
  "total_return_pct": 0.005234,
  "positions": [
    {
      "symbol": "SPY",
      "quantity": 195.42,
      "avg_entry_price": 512.34,
      "current_price": 512.35,
      "market_value": 100000.00,
      "unrealized_pl": 19.54,
      "unrealized_pl_pct": 0.000195
    }
  ]
}
```

## Running Live Trading

### Setup:
```bash
# Load API credentials
source config.env

# Create log directory
mkdir -p logs/live_trading
```

### Foreground (for testing):
```bash
source config.env
./build/sentio_cli live-trade
```

### Background (production):
```bash
source config.env
nohup ./build/sentio_cli live-trade > logs/live_trading/runner.log 2>&1 &
echo $! > logs/live_trading/runner.pid
```

### Stop Background Job:
```bash
kill $(cat logs/live_trading/runner.pid)
```

### Monitor Logs:
```bash
# System log
tail -f logs/live_trading/system_*.log

# Signals
tail -f logs/live_trading/signals_*.jsonl | jq

# Decisions
tail -f logs/live_trading/decisions_*.jsonl | jq

# Positions
tail -f logs/live_trading/positions_*.jsonl | jq
```

## Research & Analysis

### Daily Analysis Script
```python
import json
import pandas as pd

# Load logs
signals = pd.read_json('logs/live_trading/signals_20251007_093000.jsonl', lines=True)
decisions = pd.read_json('logs/live_trading/decisions_20251007_093000.jsonl', lines=True)
positions = pd.read_json('logs/live_trading/positions_20251007_093000.jsonl', lines=True)

# Analyze performance
final_value = positions.iloc[-1]['portfolio_value']
total_return = (final_value - 100000) / 100000
print(f"Total Return: {total_return*100:.2f}%")

# Analyze signals
print(f"Total Signals: {len(signals)}")
print(f"Avg Probability: {signals['probability'].mean():.4f}")
print(f"LONG signals: {(signals['probability'] >= 0.55).sum()}")
print(f"SHORT signals: {(signals['probability'] <= 0.45).sum()}")

# Analyze trades
trades_executed = decisions[decisions['should_trade'] == True]
print(f"Trades Executed: {len(trades_executed)}")
print(f"Avg Hold Time: {decisions['bars_held'].mean():.1f} bars")
```

## Safety Features

1. **Paper Trading Only**: No real money at risk
2. **Regular Hours Only**: 9:30am - 3:58pm ET
3. **EOD Liquidation**: Forced close at 3:58pm
4. **No Overnight Risk**: 100% cash every night
5. **Profit Target**: Auto-exit at +2%
6. **Stop Loss**: Auto-exit at -1.5%
7. **Minimum Hold**: Prevents over-trading
8. **Comprehensive Logging**: Full audit trail

## Next Steps

1. ✅ Alpaca client built and tested
2. ⏳ Polygon WebSocket integration (after engineer meeting)
3. ⏳ Historical warmup data loading
4. ⏳ Actual order execution implementation
5. ⏳ Deploy as background job
6. ⏳ Monitor first trading day
7. ⏳ Analyze logs and improve

## Performance Monitoring

### Key Metrics to Track
- Daily return vs backtest (expected: +0.6086% per 480 bars)
- Signal accuracy (LONG win rate, SHORT win rate)
- Trade frequency (expected: ~3,500 trades per day)
- Slippage vs backtest
- Position holding times
- Profit target / stop loss hit rates

### Expected Performance
- **Per Day**: ~+0.6% (based on 480-bar blocks)
- **Per Week**: ~+3.0%
- **Per Month**: ~+10.5%
- **Per Year**: ~+125%

## Contact

Questions about Polygon proxy setup → Engineer meeting
Questions about strategy → See backtest results in /tmp/spy_trades_phase2.jsonl
