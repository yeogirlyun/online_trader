# Production Trading Workflow

**Version:** 2.0 (Clean Architecture)
**Date:** October 15, 2025
**System:** Rotation Trading Strategy with 6 Instruments

---

## Overview

The Sentio trading system is designed for **production trading** with a clear workflow:

1. **Mock Mode** - Test strategy on historical dates, measure MRD
2. **Live Paper Mode** - Verify strategy works in real-time with paper account
3. **Live Real Mode** - Deploy to production with real capital

**Key Principle:** MRD (Mean Return per Day) is the ultimate performance metric.

---

## System Architecture

### Components

```
sentio_cli (CLI entry point)
    │
    ├─→ mock     (Historical date replay)
    └─→ live     (Real-time trading)
            │
            ├─→ --paper    (Paper trading on Alpaca)
            └─→ --real     (Real trading on Alpaca)
```

### Strategy Components

- **RotationPositionManager** - Manages top 3 positions with rotation logic
- **SignalAggregator** - Ranks signals by strength (prob × conf × leverage × staleness)
- **OnlineEnsemble** - Multi-horizon EWRLS predictor (1, 5, 10 bars)
- **UnifiedFeatureEngine** - 126 features (momentum, volatility, volume)

---

## Trading Instruments

The system trades **6 leveraged ETFs** with rotation:

| Symbol | Description | Leverage | Boost Factor |
|--------|-------------|----------|--------------|
| **TQQQ** | 3x NASDAQ Bull | +3x | 1.5 |
| **SQQQ** | -3x NASDAQ Bear | -3x | 1.5 |
| **SPXL** | 3x S&P 500 Bull | +3x | 1.5 |
| **SDS** | -2x S&P 500 Bear | -2x | 1.4 |
| **UVXY** | 1.5x VIX Short-Term | ~+1.5x | 1.6 |
| **SVXY** | -0.5x VIX Short-Term | ~-0.5x | 1.3 |

**Position Limits:**
- Max positions: 3 (hold top 3 ranked signals)
- Position sizing: 33% capital per position
- EOD liquidation: All positions closed at 3:58 PM ET

---

## Workflow

### Step 1: Test with Mock Mode

**Purpose:** Measure MRD on historical dates to validate strategy

**Usage:**
```bash
# Test a single trading day
sentio_cli mock --date 2025-10-14

# Test a date range
sentio_cli mock --start-date 2025-10-01 --end-date 2025-10-14

# Test with custom config
sentio_cli mock --date 2025-10-14 --config config/rotation_strategy.json
```

**Output:**
```
logs/mock_trading/
├── session_2025-10-14.log        # Full session log
├── signals_2025-10-14.jsonl      # All signals generated
├── decisions_2025-10-14.jsonl    # Position decisions
├── trades_2025-10-14.jsonl       # Executed trades
├── positions_2025-10-14.jsonl    # Position snapshots
└── analysis_report.txt           # Performance summary
```

**Key Metrics:**
- **MRD** (Mean Return per Day) - Primary metric
- **Total Return %** - Day's performance
- **Trades** - Number of trades executed
- **Win Rate %** - Percentage of winning trades
- **Max Drawdown** - Worst intraday drawdown

**Success Criteria:**
```
✅ MRD ≥ 0.5%    (Target: 0.5-1.0% daily)
✅ Win Rate ≥ 50%
✅ Max Drawdown ≤ 5%
✅ Trade Count: 50-200 per day (not churning)
```

---

### Step 2: Verify MRD Performance

**Check Analysis Report:**
```bash
cat logs/mock_trading/analysis_report.txt
```

**Example Good Report:**
```
==============================================
 TRADING SESSION ANALYSIS REPORT
==============================================
Date: 2025-10-14
Strategy: Rotation Strategy v2.0
Instruments: TQQQ, SQQQ, SPXL, SDS, UVXY, SVXY

Performance Metrics:
  MRD:                +0.73%          ✅ Above target (0.5%)
  Total Return:       +0.73%
  Win Rate:           52.3%           ✅ Above 50%
  Trades Executed:    127
  Max Drawdown:       -2.1%           ✅ Below 5%

Top Performing Instrument:
  TQQQ: +1.2% (45 trades, 55% win rate)

Confidence Evolution:
  Start: 0.15 (15%)
  End:   0.62 (62%)                  ✅ Good learning

Signal Strength:
  Average: 0.42
  Max:     0.78                       ✅ Strong signals

VERDICT: ✅ READY FOR LIVE PAPER TRADING
```

**Example Bad Report:**
```
Performance Metrics:
  MRD:                -1.2%           ❌ Negative
  Total Return:       -1.2%
  Win Rate:           45.8%           ❌ Below 50%
  Trades Executed:    523             ❌ Too many (churning)
  Max Drawdown:       -8.5%           ❌ Above 5%

VERDICT: ❌ DO NOT GO LIVE - Tune parameters
```

---

### Step 3: Go Live (Paper Trading)

**Purpose:** Verify strategy works in real-time market conditions

**Usage:**
```bash
# Standard paper trading
sentio_cli live --paper

# Dry-run mode (no orders, logging only)
sentio_cli live --paper --dry-run

# With custom config
sentio_cli live --paper --config config/rotation_strategy.json
```

**Requirements:**
- Alpaca paper account API keys in `config.env`:
  ```
  ALPACA_PAPER_API_KEY=PK...
  ALPACA_PAPER_SECRET_KEY=...
  POLYGON_API_KEY=...
  ```

**What Happens:**
1. Connects to Alpaca paper account
2. Connects to Polygon WebSocket for real-time quotes
3. Generates signals every minute (9:30 AM - 4:00 PM ET)
4. Executes trades on paper account
5. Liquidates all positions at 3:58 PM ET
6. Logs everything to `logs/live_trading/`

**Monitoring:**
```bash
# Watch live trading in real-time
tail -f logs/live_trading/session_$(date +%Y-%m-%d).log

# Check current positions
tail -f logs/live_trading/positions_*.jsonl | jq .

# Monitor MRD evolution
grep "MRD" logs/live_trading/session_*.log
```

**Paper Trading Duration:**
- **Minimum:** 5 trading days
- **Recommended:** 10-20 trading days
- **Goal:** Verify MRD ≥ 0.5% consistently

---

### Step 4: Production (Real Trading)

**⚠️ WARNING:** Only proceed if paper trading MRD ≥ 0.5% for 10+ days

**Usage:**
```bash
# Real trading (use with caution!)
sentio_cli live --real --config config/rotation_strategy.json
```

**Requirements:**
- Alpaca live account API keys in `config.env`:
  ```
  ALPACA_API_KEY=AK...
  ALPACA_SECRET_KEY=...
  ```
- Sufficient capital (recommend $10,000+ for 6 instruments)
- Risk management plan
- Stop-loss procedures

**Pre-Flight Checklist:**
```
□ Paper trading MRD ≥ 0.5% for 10+ days
□ Win rate ≥ 50% consistently
□ Max drawdown ≤ 5% consistently
□ No churning (trade count: 50-200/day)
□ Confidence reaches 50-70% within first hour
□ Signal strength 0.30-0.70 range
□ All 6 instruments have historical data
□ Alpaca account funded
□ Config.env has real API keys
□ Stop-loss strategy defined
□ Monitoring alerts configured
```

**Production Monitoring:**
```bash
# Real-time monitoring
watch -n 60 'tail -20 logs/live_trading/session_$(date +%Y-%m-%d).log'

# MRD tracking
sentio_cli analyze-mrd --date $(date +%Y-%m-%d)

# Position check
sentio_cli status --positions
```

---

## Configuration

### Strategy Parameters

File: `config/rotation_strategy.json`

**Key Parameters:**

```json
{
  "oes_config": {
    "ewrls_lambda": 0.98,           // Faster learning (5x vs 0.995)
    "initial_variance": 10.0,       // Lower initial uncertainty
    "regularization": 0.1,          // Stronger regularization
    "warmup_samples": 100           // 100 bars warmup
  },

  "signal_aggregator_config": {
    "min_confidence": 0.01,         // 1% minimum confidence
    "min_strength": 0.005           // 0.5% minimum strength
  },

  "rotation_manager_config": {
    "max_positions": 3,             // Hold top 3 signals
    "min_strength_to_enter": 0.008, // 0.8% to enter (hysteresis)
    "min_strength_to_exit": 0.004,  // 0.4% to exit
    "minimum_hold_bars": 5,         // 5-bar minimum hold (anti-churning)
    "rotation_cooldown_bars": 10,   // 10-bar cooldown after exit
    "profit_target_pct": 0.03,      // 3% profit target
    "stop_loss_pct": 0.015,         // 1.5% stop loss
    "eod_liquidation": true,        // Close all at 3:58 PM
    "eod_exit_time_minutes": 358    // 3:58 PM = minute 358 from 9:30 AM
  }
}
```

**Tuning Guidelines:**

| Parameter | Increase If | Decrease If |
|-----------|-------------|-------------|
| `ewrls_lambda` | Too reactive to noise | Too slow to adapt |
| `min_strength_to_enter` | Too many trades | Too few trades |
| `minimum_hold_bars` | Still churning | Trades too infrequent |
| `profit_target_pct` | Taking profits too late | Taking profits too early |
| `stop_loss_pct` | Losses too large | Stopped out too often |

---

## Performance Metrics

### MRD (Mean Return per Day)

**Definition:** Average return per trading day

**Calculation:**
```
MRD = (Ending_Capital - Starting_Capital) / Starting_Capital / Trading_Days
```

**Targets:**
- **Break-even:** MRD = 0.0%
- **Minimum viable:** MRD ≥ 0.5%
- **Good:** MRD ≥ 1.0%
- **Excellent:** MRD ≥ 2.0%

**Annualized Return:**
```
Annualized Return = (1 + MRD)^252 - 1

MRD = 0.5%  → 252%  annualized
MRD = 1.0%  → 1,155% annualized
MRD = 2.0%  → ~15,000% annualized
```

**Why MRD is Primary Metric:**
- Simple, clear, actionable
- Directly measures daily performance
- Easy to track and improve
- Industry-standard for intraday strategies

---

## Predictor Learning Fixes (v2.0)

The system includes **5 critical predictor learning fixes** (implemented Oct 15, 2025):

### Fix #1: Continuous Bar-to-Bar Learning
- **Before:** Feedback only on position exits (10-20 updates/day)
- **After:** Feedback every bar (390 updates/day)
- **Impact:** 39x more learning signals

### Fix #2: Periodic Covariance Reconditioning
- **Before:** Matrix ill-conditioning after thousands of updates
- **After:** Regularization every 100 samples prevents singularity
- **Impact:** Stable predictions even after 10,000+ updates

### Fix #3: Empirical Accuracy-Based Confidence
- **Before:** Variance-based confidence stuck at 1-1.5%
- **After:** RMSE + directional accuracy → 30-70% confidence
- **Impact:** Realistic confidence reflecting actual performance

### Fix #4: Reliable Multi-Horizon Updates
- **Before:** Pending updates might not process
- **After:** Cleanup of stale updates + reliable processing
- **Impact:** All 1, 5, 10-bar predictions get feedback

### Fix #5: Faster Learning (Lambda 0.98)
- **Before:** Lambda 0.995 = 200-bar memory (5+ hours)
- **After:** Lambda 0.98 = 50-bar memory (1.3 hours)
- **Impact:** 5x faster adaptation to regime changes

**Expected Improvements:**
- Confidence: 1-1.5% → 30-70%
- Signal strength: 0.008-0.015 → 0.30-0.60
- MRD: -4.5% → 0.5-1.0% (target)

---

## Troubleshooting

### Mock Mode Issues

**Problem:** No trades executed
- Check: Signal strength in `signals_*.jsonl`
- Verify: `min_strength_to_enter` not too high
- Solution: Lower to 0.005 for testing

**Problem:** Too many trades (churning)
- Check: Trade count > 300/day
- Verify: `minimum_hold_bars` = 5
- Solution: Increase to 10 bars

**Problem:** Negative MRD
- Check: Win rate < 50%
- Verify: Confidence reaches 50%+
- Solution: Run longer test (10+ days) for predictor to learn

### Live Trading Issues

**Problem:** Connection failed
- Check: API keys in `config.env`
- Verify: Alpaca account status
- Solution: Test with `--dry-run` first

**Problem:** Orders rejected
- Check: Account buying power
- Verify: Position sizing (33% per position)
- Solution: Reduce `capital_per_position` in config

**Problem:** Predictor not learning
- Check: Confidence evolution in logs
- Verify: Bar-to-bar updates happening
- Solution: Check `update_learning()` logs

---

## Data Requirements

### Historical Data for Mock Mode

Required for each instrument:
```
data/equities/
├── TQQQ_RTH_NH.csv
├── SQQQ_RTH_NH.csv
├── SPXL_RTH_NH.csv
├── SDS_RTH_NH.csv
├── UVXY_RTH_NH.csv
└── SVXY_RTH_NH.csv
```

**Format:** 1-minute OHLCV bars, RTH only (9:30 AM - 4:00 PM ET)

**Columns:**
```
timestamp,open,high,low,close,volume,bar_id
```

**Download Script:**
```bash
python tools/download_historical_data.py --symbols TQQQ,SQQQ,SPXL,SDS,UVXY,SVXY \
  --start-date 2024-01-01 --end-date 2025-10-15 \
  --output data/equities/
```

### Live Data Requirements

- **Polygon.io** WebSocket for real-time quotes
- **Alpaca** for trade execution
- Both require active subscriptions

---

## System Maintenance

### Daily

```bash
# Check system health
sentio_cli status --health

# Verify data is current
sentio_cli verify-data --all

# Review yesterday's performance
sentio_cli analyze-mrd --date $(date -d yesterday +%Y-%m-%d)
```

### Weekly

```bash
# Clean old logs (keep last 30 days)
find logs/ -type f -mtime +30 -delete

# Backup trading logs
tar -czf backups/logs_$(date +%Y-%m-%d).tar.gz logs/

# Review weekly MRD
sentio_cli analyze-mrd --week
```

### Monthly

```bash
# Comprehensive performance review
sentio_cli report --month $(date +%Y-%m)

# Parameter optimization
python tools/optuna_monthly_tune.py

# Update configuration if needed
cp config/rotation_strategy.json config/rotation_strategy_backup.json
cp config/optimized_params.json config/rotation_strategy.json
```

---

## Safety Features

### Built-In Protections

1. **EOD Liquidation** - All positions closed at 3:58 PM (no overnight risk)
2. **Stop Loss** - 1.5% max loss per position
3. **Profit Target** - 3% take profit per position
4. **Minimum Hold** - 5-bar minimum prevents churning
5. **Exit Cooldown** - 10-bar cooldown after exit
6. **Position Limits** - Max 3 positions (diversification)

### Manual Circuit Breakers

**Emergency Stop:**
```bash
# Kill trading process immediately
pkill -9 sentio_cli

# Close all positions via Alpaca dashboard
# https://app.alpaca.markets/paper/dashboard/portfolio
```

**Pause Trading:**
```bash
# Set maintenance mode (prevents new orders)
touch .maintenance_mode

# Resume trading
rm .maintenance_mode
```

---

## Support and Resources

### Documentation

- `megadocs/` - Detailed implementation docs
  - `ANTI_CHURNING_FIXES_IMPLEMENTATION_SUMMARY.md`
  - `PREDICTOR_LEARNING_FIXES_IMPLEMENTATION_SUMMARY.md`
- `config/rotation_strategy.json` - Strategy configuration
- `PRODUCTION_WORKFLOW.md` - This document

### Logging

- `logs/mock_trading/` - Mock trading sessions
- `logs/live_trading/` - Live trading sessions
- All logs use JSON Lines format for easy parsing

### Monitoring

```bash
# Real-time MRD tracking
watch -n 60 "grep MRD logs/live_trading/session_$(date +%Y-%m-%d).log | tail -1"

# Position monitoring
watch -n 10 "tail -1 logs/live_trading/positions_$(date +%Y-%m-%d).jsonl | jq ."

# Trade alerts
tail -f logs/live_trading/trades_$(date +%Y-%m-%d).jsonl | \
  jq 'select(.pnl_pct < -0.01) | {symbol, pnl_pct, reason}'
```

---

## Version History

**v2.0 (Oct 15, 2025)**
- Clean architecture: mock + live modes only
- 6-instrument rotation (TQQQ/SQQQ/SPXL/SDS/UVXY/SVXY)
- Predictor learning fixes (continuous feedback, empirical confidence)
- Anti-churning fixes (minimum hold, hysteresis, cooldowns)
- Removed old backtest system and PSM code

**v1.0 (Sep 2025)**
- Initial rotation strategy
- 4-instrument trading (SPY/SPXL/SH/SDS)
- Basic Position State Machine

---

**Conclusion**

The production workflow is simple:
1. **Test** with mock mode → measure MRD
2. **Verify** MRD ≥ 0.5% consistently
3. **Deploy** to live paper trading
4. **Confirm** performance in real-time
5. **Go live** with real capital (if confident)

**Remember:** MRD is the ultimate metric. Everything else is noise.
