# Live Trading Quick Start Guide

## Python WebSocket Bridge Setup (One-Time)

The live trading system uses a Python bridge to connect to Alpaca's WebSocket API, bypassing libwebsockets connection issues.

### Prerequisites

```bash
# Install Python Alpaca SDK
pip install alpaca-py

# Verify SSL certificate location
ls -l /opt/homebrew/etc/ca-certificates/cert.pem
```

### Credentials Setup

Create `config.env` in the project root:

```bash
# Alpaca Paper Trading (recommended for testing)
export ALPACA_PAPER_API_KEY=your_paper_key_here
export ALPACA_PAPER_SECRET_KEY=your_paper_secret_here

# Polygon API Key (for additional data, optional)
export POLYGON_API_KEY=your_polygon_key_here

# SSL Certificate for Python WebSocket
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem
```

## Quick Launch

### Option 1: Automated Launch Script (Recommended)

```bash
# Use default times (2:30 PM midday optimization, 3:58 PM EOD close)
./tools/launch_live_trading.sh

# Or specify custom times (24-hour format)
./tools/launch_live_trading.sh 14:30 15:58
```

**Features:**
- ✅ Waits for market open (9:30 AM ET)
- ✅ **Comprehensive Auto-warmup** - fetches 20 blocks (7800 bars) + 64 feature bars + today's bars
- ✅ Auto-starts Python bridge and C++ trader
- ✅ Monitors health (restarts on crash)
- ✅ Midday optimization at configurable time
- ✅ EOD position closing at configurable time
- ✅ Auto-shutdown at market close (4:00 PM ET)
- ✅ Saves all logs to `data/logs/`

### Option 2: Manual Launch (2 terminals)

**Terminal 1: Python WebSocket Bridge**
```bash
source config.env
python3 tools/alpaca_websocket_bridge.py
```

**Terminal 2: C++ Live Trader**
```bash
source config.env
./build/sentio_cli live-trade
```

## Architecture

```
Alpaca IEX WebSocket
        ↓
Python Bridge (tools/alpaca_websocket_bridge.py)
        ↓
FIFO Pipe (/tmp/alpaca_bars.fifo)
        ↓
C++ Trader (build/sentio_cli live-trade)
        ↓
OnlineEnsemble Strategy
        ↓
Alpaca Paper Trading API (Orders)
```

## Trading Schedule

| Time (ET) | Event |
|-----------|-------|
| 9:30 AM | Market open - Trading starts |
| 2:30 PM | Midday optimization (configurable) |
| 3:58 PM | EOD position closing (configurable) |
| 4:00 PM | Market close - Trading stops |

## Strategy Configuration

**Instruments:**
- SPY (1x baseline)
- SPXL (3x leveraged long)
- SH (-1x inverse)
- SDS (-2x leveraged inverse)

**Strategy:** OnlineEnsemble EWRLS with Position State Machine (PSM)
- Multi-horizon predictions (1, 5, 10 bars)
- 126-feature unified feature engine
- Adaptive threshold calibration
- Dynamic leverage management (0x, ±1x, ±2x, ±3x)

## Monitoring

### Real-Time Logs

```bash
# Watch Python bridge
tail -f data/logs/python_bridge_$(date +%Y%m%d).log

# Watch C++ trader
tail -f data/logs/cpp_trader_$(date +%Y%m%d).log
```

### Health Checks

```bash
# Check if processes are running
ps aux | grep -E "alpaca_websocket_bridge|sentio_cli"

# Check FIFO pipe exists
ls -l /tmp/alpaca_bars.fifo

# Check recent bars received
tail -20 data/logs/python_bridge_$(date +%Y%m%d).log | grep "✓.*Bar:"
```

### Performance Tracking

The trader logs include:
- Real-time P&L updates
- Position changes (PSM state transitions)
- Trade execution confirmations
- Strategy signals and predictions
- Risk management actions

## Troubleshooting

### Python Bridge Won't Start

**Issue:** SSL certificate verification fails

```bash
# Solution: Set SSL certificate path
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem

# Verify path exists
ls -l $SSL_CERT_FILE

# Update certifi
pip install --upgrade certifi
```

### No Bars Received

**Issue:** Python bridge connected but no bars

**Cause:** Market is closed or after hours

**Check:** Market hours are 9:30 AM - 4:00 PM ET, Monday-Friday

```bash
# Check current ET time
TZ='America/New_York' date
```

### C++ Trader Can't Read FIFO

**Issue:** "Failed to open FIFO"

```bash
# Check if FIFO exists
ls -l /tmp/alpaca_bars.fifo

# Check if Python bridge is running
ps aux | grep alpaca_websocket_bridge

# Restart both processes
pkill -f alpaca_websocket_bridge
pkill sentio_cli
./tools/launch_live_trading.sh
```

### Trader Exited Unexpectedly

```bash
# Check last 50 lines of trader log
tail -50 data/logs/cpp_trader_$(date +%Y%m%d).log

# Common causes:
# - Invalid credentials
# - Insufficient buying power
# - Position reconciliation failed
# - Strategy initialization error
```

## Safety Features

### Position Limits
- Maximum leverage: 3x (SPXL/SDS)
- Dynamic position sizing based on capital
- Profit-taking thresholds
- Stop-loss protection

### Risk Management
- Real-time P&L monitoring
- Position State Machine (PSM) constraints
- EOD flat position enforcement
- Connection health monitoring

### Automatic Recovery
- WebSocket auto-reconnection (built into alpaca-py SDK)
- FIFO pipe recreation on disconnect
- Startup position reconciliation
- Previous day EOD catch-up check

## Testing Before Live Trading

### 1. Verify Credentials

```bash
# Test Alpaca REST API
curl -X GET "https://paper-api.alpaca.markets/v2/account" \
  -H "APCA-API-KEY-ID: $ALPACA_PAPER_API_KEY" \
  -H "APCA-API-SECRET-KEY: $ALPACA_PAPER_SECRET_KEY"
```

### 2. Test Python Bridge

```bash
# Should connect and show subscriptions
source config.env
python3 tools/alpaca_websocket_bridge.py

# Look for:
# - "✓ Bridge active"
# - "Created FIFO pipe"
# - "Subscribed to bars: SPY, SPXL, SH, SDS"
```

### 3. Dry Run (After Market Hours)

```bash
# Start both processes - won't trade but will connect
./tools/launch_live_trading.sh

# Verify:
# - Python bridge connects to Alpaca
# - C++ trader loads strategy
# - FIFO communication works
# - Position reconciliation succeeds
```

## Production Checklist

Before running live (paper or real):

- [ ] Credentials configured in `config.env`
- [ ] Python bridge tested successfully
- [ ] C++ trader builds without errors
- [ ] Strategy warmup data available (10+ blocks)
- [ ] Sufficient account buying power
- [ ] Market hours confirmed (9:30 AM - 4:00 PM ET)
- [ ] Monitoring logs set up
- [ ] Backup laptop/system for redundancy
- [ ] Emergency stop procedure documented

## Emergency Stop

```bash
# Graceful shutdown (closes positions)
pkill -TERM sentio_cli
sleep 5
pkill -TERM -f alpaca_websocket_bridge

# Force stop (does NOT close positions)
pkill -KILL sentio_cli
pkill -KILL -f alpaca_websocket_bridge

# Manual position close via Alpaca dashboard
# → https://app.alpaca.markets/paper/dashboard/overview
```

## Files Reference

| File | Purpose |
|------|---------|
| `tools/launch_live_trading.sh` | Main launch script |
| `tools/alpaca_websocket_bridge.py` | Python WebSocket client |
| `build/sentio_cli live-trade` | C++ trader executable |
| `src/live/polygon_websocket_fifo.cpp` | FIFO reader implementation |
| `config.env` | Credentials and environment |
| `data/logs/` | Trading session logs |

## Support

For issues or questions:
1. Check logs in `data/logs/`
2. Review `megadocs/LIBWEBSOCKETS_CONNECTION_FAILURE.md` for connection debugging
3. Test with paper trading first
4. Verify market hours and credentials

**Remember:** Always test thoroughly with paper trading before using real money!
