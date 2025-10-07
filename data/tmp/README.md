# data/tmp/ - Temporary Files and Logs

This directory contains temporary files, logs, and reports for the online trading system.

## Directory Structure

```
data/tmp/
├── README.md                                    # This file
├── start_live_trading.sh                        # Script to start live trading
├── monitor_trading.sh                           # Script to monitor trading session
├── live_trade.log                               # Live trading session log
├── spy_config_summary.md                        # SPY configuration documentation
├── live_trading_analysis_report.md              # QQQ session analysis (Oct 6)
├── trading_summary_YYYYMMDD_HHMMSS.txt          # Market close summary reports
├── qqq_full_signals.jsonl                       # Generated signals for QQQ
└── signal_gen.log                               # Signal generation log

```

## Scripts

### start_live_trading.sh
Starts live paper trading with SPY instruments via Alpaca IEX WebSocket.

**Usage:**
```bash
./data/tmp/start_live_trading.sh
```

**What it does:**
1. Kills any existing live trading process
2. Starts `sentio_cli live-trade` in background
3. Logs output to `data/tmp/live_trade.log`
4. Prints monitoring commands

### monitor_trading.sh
Monitors live trading session and generates summary report at market close (4:00 PM ET).

**Usage:**
```bash
nohup ./data/tmp/monitor_trading.sh > data/tmp/monitor.log 2>&1 &
```

**Output:**
- Real-time monitoring every 5 minutes
- Summary report saved to `data/tmp/trading_summary_YYYYMMDD_HHMMSS.txt`
- Includes bar counts, signals, and trades

## Log Files

### live_trade.log
Main log file for live trading sessions. Contains:
- Connection status (Alpaca API, WebSocket)
- Market data bars received (SPY, SPXL, SH, SDS)
- Strategy initialization and warmup
- Signal generation and trade execution
- Error messages

**Monitor in real-time:**
```bash
tail -f data/tmp/live_trade.log
```

### signal_gen.log
Log file for offline signal generation runs.

## Reports

### spy_config_summary.md
Complete documentation of SPY live trading configuration:
- Instrument details (SPY, SPXL, SH, SDS)
- OnlineEnsemble v1.0 strategy parameters
- Position State Machine states
- Alpaca WebSocket configuration
- Expected data formats

### live_trading_analysis_report.md
Analysis of October 6, 2025 QQQ live trading session:
- Market data reception summary (152 bars)
- Root cause analysis of zero trades (symbol mismatch bug)
- WebSocket connection journey (failed proxy → successful Alpaca IEX)
- Fixes applied

### trading_summary_YYYYMMDD_HHMMSS.txt
Auto-generated summary reports at market close:
- Bar counts by symbol
- Signals generated
- Trades executed
- Timestamp of market close

## Data Files

### qqq_full_signals.jsonl (55 MB)
Generated signals for full QQQ dataset. Each line is a JSON object:
```json
{
  "timestamp": 1728000000000,
  "signal": "LONG",
  "prediction": 0.65,
  "bb_amp": 0.05
}
```

## Usage Examples

### Start Live Trading
```bash
./data/tmp/start_live_trading.sh
```

### Monitor Live Trading
```bash
# Real-time log monitoring
tail -f data/tmp/live_trade.log

# Grep for specific events
grep "✓ Bar:" data/tmp/live_trade.log | tail -20
grep "Signal:" data/tmp/live_trade.log
grep "Trade:" data/tmp/live_trade.log
```

### Check Trading Summary
```bash
# View latest summary
cat data/tmp/trading_summary_*.txt | tail -1

# Count bars received
grep "✓ Bar:" data/tmp/live_trade.log | awk '{print $3}' | sort | uniq -c
```

### Stop Live Trading
```bash
pkill -f 'sentio_cli live-trade'
```

## File Retention

- **Log files**: Keep for analysis, archive periodically
- **Summary reports**: Keep for performance tracking
- **Signal files**: Archive large files (>100 MB) after analysis
- **Temporary scripts**: Version control in git

## Related Directories

- `logs/live_trading/` - Structured JSONL logs (signals, decisions, trades, positions)
- `data/equities/` - Historical market data (CSV files)
- `data/dashboards/` - Trading performance visualizations (HTML)

## Notes

- All times are in Eastern Time (ET)
- Market hours: 9:30 AM - 4:00 PM ET
- Alpaca IEX provides minute bars with ~3% market volume coverage
- Paper trading account starts with $100,000 virtual capital
