# Daily Optimization Workflow for Live Trading

**Date**: 2025-10-08
**Purpose**: Daily parameter optimization for next-day Alpaca live trading with 58-feature set

---

## Overview

The daily optimization workflow focuses on **short-term (1-day) performance** rather than long-term metrics. This approach allows us to:

1. **Adapt to current market conditions** (optimize parameters every day)
2. **Reduce overfitting risk** (only 2 blocks training, 2 blocks testing)
3. **Fast turnaround** (~30-45 minutes vs hours for long-term optimization)
4. **Deploy fresh parameters** for each trading session

---

## Philosophy: Short-Term vs Long-Term Optimization

### Why Short-Term Focus?

**Traditional approach** (what we were doing):
- Train on 20 blocks (20 days)
- Test on 5-10 blocks (5-10 days)
- Goal: Find parameters that work across diverse market regimes

**Problems with this**:
1. **Overfits to historical regimes** that may not match tomorrow's market
2. **Slow adaptation** to regime changes (bull → bear, low vol → high vol)
3. **Long optimization time** (1.5-2 hours for 200 trials)

**Daily approach** (new):
- Train on 2 blocks (2 days of recent data)
- Test on 2 blocks (2 days of recent data)
- Goal: Find parameters that work for **tomorrow's session**

**Advantages**:
1. **Adapts to current regime** (only uses last 4 days of data)
2. **Fast optimization** (30-45 min for 100 trials)
3. **Fresh parameters daily** (like professional quant firms)
4. **Better for live trading** (parameters optimized for "now", not "general")

---

## Daily Workflow

### Step 1: Download Latest Data (Every Morning)

**Before market open** (e.g., 8:00 AM ET):

```bash
# Download last 4 days of SPY data
cd /Volumes/ExternalSSD/Dev/C++/online_trader

python3 tools/data_downloader.py SPY --days 4 \
    --outdir data/equities
```

**Output**: `data/equities/SPY_4blocks.csv` (1,560 bars = 4 days)

---

### Step 2: Run Daily Optimization

**Before market open** (e.g., 8:30 AM ET):

```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader

./tools/run_daily_optuna.sh
```

**What it does**:
1. Validates `SPY_4blocks.csv` exists
2. Runs Strategy C with **2 train blocks + 2 test blocks**
3. Tests **100 trials** (reduced from 200 for speed)
4. Saves optimal parameters to `data/tmp/daily_optuna/daily_params_YYYYMMDD_HHMMSS.json`

**Duration**: 30-45 minutes (vs 1.5-2 hours for long-term optimization)

**Output**:
```json
{
  "strategy": "C",
  "train_blocks": 2,
  "test_blocks": 2,
  "best_params": {
    "buy_threshold": 0.5734,
    "sell_threshold": 0.4221,
    "ewrls_lambda": 0.997342,
    "bb_amplification_factor": 0.0856,
    "enable_threshold_calibration": false
  },
  "best_value": 0.3421,  // Training MRB (2 blocks)
  "test_mrb": 0.2987,    // Test MRB (2 blocks)
  "timestamp": "20251008_083045"
}
```

---

### Step 3: Validate Parameters (Pre-Market)

**Before deploying to live** (e.g., 9:15 AM ET):

```bash
cd build

# Test on the 4-block dataset
./sentio_cli backtest --blocks 2 --warmup-blocks 2 \
    --params "$(cat ../data/tmp/daily_optuna/daily_params_*.json | jq -c .best_params)" \
    --data ../data/equities/SPY_4blocks.csv
```

**Review**:
- Check MRB is positive (>0.1%)
- Check trade count is reasonable (50-200 trades/block)
- Check win rate is >55%

If results look good → proceed to Step 4
If results look poor → use previous day's parameters or default

---

### Step 4: Deploy to Live Trading

**At market open** (9:30 AM ET):

```bash
cd build

# Start live trading with optimized parameters
./sentio_cli live-trade \
    --params "$(cat ../data/tmp/daily_optuna/daily_params_*.json | jq -c .best_params)"
```

**Monitoring**:
- Check `/tmp/live_trading_*.log` for trade executions
- Monitor equity curve in real-time
- Stop trading if losses exceed -2% for the day

---

## Configuration Details

### run_daily_optuna.sh Parameters

```bash
TRAIN_BLOCKS=2      # Train on 2 blocks (2 days)
TEST_BLOCKS=2       # Test on 2 blocks (2 days)
N_TRIALS=100        # 100 trials (vs 200 for long-term)
TIMEOUT_MINUTES=60  # 1 hour max (vs 6 hours for long-term)
```

### Why 2 Blocks for Training?

**Statistical reasoning**:
- 2 blocks = 780 bars (2 trading days)
- With ~150 trades/block → ~300 trades for optimization
- Enough data to find patterns, not enough to overfit to old regimes

**Market reasoning**:
- Last 2 days capture **current volatility regime**
- Last 2 days capture **current trend direction**
- Last 2 days capture **current market microstructure**

**Comparison to v1.0**:
- v1.0 used 20 blocks for training (20 days)
- v1.0's 0.55/0.45 thresholds were optimized for **general market conditions**
- Daily approach optimizes for **tomorrow's specific conditions**

---

## Example: Daily vs Long-Term Results

### Long-Term Optimization (20 blocks train)

**Parameters found**:
```json
{
  "buy_threshold": 0.55,
  "sell_threshold": 0.45,
  "ewrls_lambda": 0.995
}
```

**Training MRB**: 0.60% (20 blocks)
**Test MRB**: 0.42% (10 blocks)

**Interpretation**: Works well **on average** across diverse regimes

---

### Daily Optimization (2 blocks train)

**Day 1 (volatile market)**:
```json
{
  "buy_threshold": 0.62,  // Higher threshold (more selective)
  "sell_threshold": 0.38,  // Lower threshold (more selective)
  "ewrls_lambda": 0.998    // Slower learning (trust history more)
}
```

**Day 2 (trending market)**:
```json
{
  "buy_threshold": 0.52,  // Lower threshold (trade more)
  "sell_threshold": 0.48,  // Higher threshold (trade more)
  "ewrls_lambda": 0.992    // Faster learning (adapt quickly)
}
```

**Interpretation**: Parameters **adapt to current regime**

---

## Risk Management

### Daily Optimization Risks

1. **Overfitting to noise**
   - **Mitigation**: Use 2-block test set for validation
   - **Mitigation**: Require test MRB within 20% of train MRB

2. **Parameter instability** (wild swings day-to-day)
   - **Mitigation**: Clamp parameters to reasonable bounds
   - **Mitigation**: Fall back to previous day if test MRB < 0

3. **Data quality issues**
   - **Mitigation**: Validate 4 blocks exist before optimization
   - **Mitigation**: Check for data gaps or anomalies

### Live Trading Safeguards

1. **Kill switch**: Stop trading if loss > -2% intraday
2. **Parameter validation**: Only deploy if test MRB > 0.1%
3. **Fallback**: Use previous day's parameters if optimization fails

---

## Comparison: Daily vs Long-Term Scripts

| Aspect | `run_daily_optuna.sh` | `run_optuna_58features.sh` |
|--------|----------------------|---------------------------|
| **Purpose** | Daily live trading prep | Research / long-term baseline |
| **Data file** | SPY_4blocks.csv | SPY_30blocks.csv |
| **Train blocks** | 2 (2 days) | 20 (20 days) |
| **Test blocks** | 2 (2 days) | 10 (10 days) |
| **Trials** | 100 | 200 |
| **Duration** | 30-45 min | 1.5-2 hours |
| **Focus** | Short-term performance | Long-term robustness |
| **Output** | `daily_params_YYYYMMDD.json` | `results_YYYYMMDD.json` |
| **When to use** | Every morning before market | Weekly/monthly research |

---

## When to Use Which Script?

### Use `run_daily_optuna.sh` when:
- ✅ Preparing for next day's live trading
- ✅ Market regime has changed (new volatility, new trend)
- ✅ You want parameters optimized for "now"
- ✅ You need fast turnaround (<1 hour)

### Use `run_optuna_58features.sh` when:
- ✅ Researching long-term performance
- ✅ Finding baseline parameters for new feature set
- ✅ Testing strategy robustness across regimes
- ✅ Publishing performance metrics for investors

---

## Daily Checklist

**Every Morning (Pre-Market)**:

- [ ] Download latest 4 days of SPY data (`data_downloader.py`)
- [ ] Run daily optimization (`run_daily_optuna.sh`)
- [ ] Review optimized parameters (buy/sell thresholds, lambda, etc.)
- [ ] Validate with backtest on 4-block data
- [ ] Check test MRB > 0.1% (if not, use previous day's params)
- [ ] Deploy to live trading at 9:30 AM ET

**During Market Hours**:

- [ ] Monitor `/tmp/live_trading_*.log` for trades
- [ ] Check equity curve every hour
- [ ] Stop if intraday loss > -2%

**After Market Close**:

- [ ] Review day's performance (analyze-trades)
- [ ] Save successful parameter sets to `data/params/YYYYMMDD_params.json`
- [ ] Note any regime changes or unusual market behavior

---

## Future Enhancements

1. **Auto-download data** (cron job at 8:00 AM ET)
2. **Auto-run optimization** (cron job at 8:30 AM ET)
3. **Auto-deploy to live** (if test MRB > threshold)
4. **Slack/email alerts** for optimization results
5. **Parameter history tracking** (see how thresholds evolve over time)

---

## Key Takeaways

✅ **Short-term focus**: Optimize for tomorrow, not forever
✅ **Fast turnaround**: 30-45 min vs 2 hours
✅ **Regime adaptation**: Parameters match current market conditions
✅ **Daily workflow**: Download → Optimize → Validate → Deploy
✅ **Risk management**: Test MRB validation, kill switches, fallback params

**Bottom line**: Professional quant firms re-optimize daily. Now we can too.

---

**Status**: Daily optimization workflow complete and ready for production use.
