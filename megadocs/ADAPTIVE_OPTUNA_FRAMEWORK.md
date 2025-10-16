# Adaptive Optuna Experimentation Framework

**Version:** 1.0
**Date:** 2025-10-08
**Status:** 🔧 Implementation Complete, Testing in Progress

---

## Overview

This framework implements three adaptive strategies for parameter optimization of the OnlineEnsemble strategy:

**Strategy A:** Per-Block Adaptive
- Retune every block (~6.5 hours/390 bars)
- Tests on next 5 blocks
- Maximum adaptivity, catches intraday regime changes

**Strategy B:** Twice-Daily Adaptive
- Retune at 9:30 AM and 12:45 PM
- Tests on next 5 blocks
- Balances adaptivity with stability

**Strategy C:** Static Baseline
- Tune once at start
- Fixed parameters for entire experiment
- Baseline to measure adaptation value

---

## Architecture

### 1. Core Framework: `tools/adaptive_optuna.py`

**Main Class:** `AdaptiveOptunaFramework`

**Key Methods:**

```python
def tune_on_window(block_start, block_end, n_trials=100):
    """
    Tune parameters using Optuna on specified block window.
    Returns: (best_params, best_mrb, tuning_time)
    """

def test_on_window(params, block_start, block_end):
    """
    Test parameters on specified block window.
    Returns: Dict with test results including MRB
    """

def strategy_a_per_block():
    """Per-block adaptive: retune every block."""

def strategy_b_4hour():
    """Twice-daily adaptive: retune every 2 blocks."""

def strategy_c_static():
    """Static baseline: tune once, fixed params."""
```

**Parameters Optimized:**
- `buy_threshold`: [0.50, 0.65] step 0.01
- `sell_threshold`: [0.35, 0.50] step 0.01
- `ewrls_lambda`: [0.990, 0.999] step 0.001
- `bb_amplification_factor`: [0.05, 0.15] step 0.01

**Block Structure:**
- 1 block = 390 bars = 1 regular trading session (6.5 hours)
- 9:30 AM - 4:00 PM (390 minutes)

---

### 2. Orchestrator: `tools/ab_test_runner.sh`

Bash script that orchestrates execution of all three strategies in sequence.

**Usage:**
```bash
./tools/ab_test_runner.sh \
  --data data/equities/SPY_30blocks.csv \
  --strategies A,B,C
```

**Options:**
- `--data` - Path to data CSV file
- `--strategies` - Comma-separated list of strategies to run
- `--help` - Show help message

**Output:**
- `data/tmp/ab_test_results/strategy_a_results.json`
- `data/tmp/ab_test_results/strategy_b_results.json`
- `data/tmp/ab_test_results/strategy_c_results.json`
- `data/tmp/ab_test_results/comparison_report.md`

---

### 3. Analysis Tool: `tools/compare_strategies.py`

Generates comprehensive comparison report with:
- Performance metrics (mean, std, min, max MRB)
- Parameter drift analysis
- Top/bottom 5 tests per strategy
- Comparative analysis (A vs. C, B vs. C)
- Winner determination
- Recommendations for live trading

**Usage:**
```bash
python3 tools/compare_strategies.py \
  --strategies A,B,C \
  --results-dir data/tmp/ab_test_results \
  --output data/tmp/ab_test_results/comparison_report.md
```

---

## Workflow

### Walk-Forward Validation

**Strategy A (Per-Block):**
```
For block B in [10, N-5]:
  1. Tune on blocks [B-10, B-1] (last 10 blocks)
  2. Validate on blocks [B-5, B-1] (last 5 blocks)
  3. Test on blocks [B, B+4] (next 5 blocks)
  4. Record MRB and parameters
```

**Strategy B (Twice-Daily):**
```
For block B in [20, N-5] step 2:
  1. Tune on blocks [B-20, B-1] (last 20 blocks)
  2. Test on blocks [B, B+4] (next 5 blocks)
  3. Record MRB and parameters
```

**Strategy C (Static):**
```
1. Tune on blocks [0, 19] (first 20 blocks)
2. For block B in [20, N-5] step 5:
   - Test on blocks [B, B+4] with fixed params
   - Record MRB
```

---

## Expected Results

### Hypotheses

**H1: Strategy A Performance**
- Highest variance (chases noise)
- Highest peaks (catches regime shifts)
- Lowest valleys (false signals)
- **Expected MRB:** 0.3-0.5% (similar or slightly better than C)

**H2: Strategy B Performance**
- Best risk-adjusted returns (Sharpe ratio)
- Most consistent MRB (lower variance)
- **Expected MRB:** 0.4-0.6% (10-20% better than C)

**H3: Strategy C Performance**
- Most stable (no parameter drift)
- **Expected MRB:** 0.3-0.4% (baseline from v2.0)
- Fails during regime changes

**H4: Parameter Drift**
- `buy_threshold` and `sell_threshold` drift most
- `ewrls_lambda` relatively stable
- Strategy A shows 3-5x more variance than B

---

## Implementation Status

### ✅ Completed

1. **Core Framework** (`adaptive_optuna.py`)
   - Strategy A/B/C implementations
   - Block data extraction
   - Walk-forward validation
   - Optuna integration (TPE sampler)

2. **Orchestrator** (`ab_test_runner.sh`)
   - Sequential strategy execution
   - Logging and progress tracking
   - Results aggregation

3. **Analysis Tool** (`compare_strategies.py`)
   - Performance metrics computation
   - Parameter drift analysis
   - Markdown report generation
   - Comparative analysis

### 🔧 In Progress

1. **CLI Parameter Support**
   - Need to add command-line parameter support to `sentio_cli`
   - Currently parameters are hardcoded in strategy
   - Blocking Optuna optimization

2. **Testing & Validation**
   - Pilot test started with Strategy C
   - Encountering signal parsing issues
   - Need to debug trade execution

### 📋 TODO

1. **Add CLI Parameters to sentio_cli**
   ```cpp
   // In generate_signals_command.cpp:
   parser.add_argument("--buy-threshold")
        .default_value(0.55)
        .scan<'g', double>();

   parser.add_argument("--sell-threshold")
        .default_value(0.45)
        .scan<'g', double>();

   parser.add_argument("--lambda")
        .default_value(0.995)
        .scan<'g', double>();

   parser.add_argument("--bb-amp")
        .default_value(0.10)
        .scan<'g', double>();
   ```

2. **Run Full A/B/C Test**
   - Execute on SPY_30blocks.csv (36 blocks)
   - Generate comparison report
   - Analyze parameter drift
   - Determine winner

3. **Deploy Winning Strategy**
   - Integrate with live trading system
   - Schedule retuning (9:30 AM, 12:45 PM for Strategy B)
   - Monitor performance vs. static baseline

4. **Visualization Dashboard**
   - MRB over time line plot
   - Parameter evolution heatmap
   - Strategy comparison bar chart
   - Drawdown analysis

---

## Files Created

### Framework Files
- `tools/adaptive_optuna.py` (500 lines) - Core framework
- `tools/ab_test_runner.sh` (200 lines) - Orchestrator script
- `tools/compare_strategies.py` (400 lines) - Analysis tool

### Documentation
- `megadocs/ADAPTIVE_OPTUNA_FRAMEWORK.md` (this file)

### Output Directories
- `data/tmp/ab_test_results/` - Test results and reports

---

## Integration with v2.0 Live Trading

The adaptive framework integrates seamlessly with v2.0's warmup and position reconciliation:

**Live Trading Workflow with Strategy B:**

1. **9:30 AM (Market Open)**
   ```bash
   # Run warmup script
   bash tools/warmup_live_trading.sh

   # Run Optuna tuning (100 trials, ~10 min)
   python3 tools/adaptive_optuna.py \
     --strategy B \
     --data data/equities/SPY_warmup_latest.csv \
     --output data/tmp/morning_params.json

   # Extract best params and update config
   # ... (write params to config file)

   # Start live trading with new params
   cd build && ./sentio_cli live-trade
   ```

2. **12:45 PM (Mid-Day Retune)**
   ```bash
   # Download latest data including morning session
   bash tools/warmup_live_trading.sh

   # Retune on recent data
   python3 tools/adaptive_optuna.py \
     --strategy B \
     --data data/equities/SPY_warmup_latest.csv \
     --output data/tmp/midday_params.json

   # Stop live trading
   pkill -f "sentio_cli live-trade"

   # Update params and restart
   # ... (seamless takeover with position reconciliation)
   cd build && ./sentio_cli live-trade
   ```

3. **4:00 PM (Market Close)**
   - Live trading automatically stops
   - Logs saved to `logs/live_trading/`
   - Performance analysis generated

---

## Next Steps

### Immediate (Week 1)

1. Add CLI parameter support to `generate-signals` command
2. Test framework on SPY_20blocks.csv (smaller dataset)
3. Debug and fix any issues with backtest execution
4. Run pilot test with Strategy C (static baseline)

### Short-Term (Week 2)

1. Run full A/B/C comparison on SPY_30blocks.csv
2. Generate comparison report
3. Analyze results and validate hypotheses
4. Select winning strategy for deployment

### Medium-Term (Week 3-4)

1. Deploy Strategy B to paper trading with scheduled retuning
2. Monitor live performance vs. static baseline
3. Create visualization dashboard
4. Document lessons learned and parameter insights

---

## Performance Expectations

### Target Metrics

**Strategy C (Baseline):**
- Mean MRB: 0.345% (from v1.5 calibration)
- Sharpe Ratio: ~1.2
- Max Drawdown: -3%

**Strategy B (Goal):**
- Mean MRB: 0.40-0.50% (+10-45% vs. baseline)
- Sharpe Ratio: ~1.5 (+25% vs. baseline)
- Max Drawdown: -2.5% (better risk control)

**Strategy A (Research):**
- Mean MRB: 0.35-0.55% (high variance)
- Peak MRB: 0.70%+ (occasional)
- Sharpe Ratio: ~1.0 (lower due to variance)

---

## Risk Management

### Safeguards

1. **Max Drawdown Threshold**
   - Stop experiment if drawdown exceeds -5%
   - Revert to static baseline parameters

2. **Parameter Bounds**
   - All parameters constrained to safe ranges
   - Asymmetric threshold enforcement (buy > sell)

3. **Tuning Timeout**
   - 5-min timeout per Optuna trial
   - 30-min total timeout for tuning session

4. **Audit Trail**
   - All parameter changes logged to JSON
   - Complete backtest results saved
   - Can replay any historical configuration

---

## Conclusion

The Adaptive Optuna Framework provides a systematic way to test whether adaptive parameter optimization provides value over static parameters. The walk-forward validation ensures out-of-sample testing, and the three-strategy comparison (A/B/C) allows us to measure the tradeoff between adaptivity and stability.

**Expected Outcome:** Strategy B (twice-daily adaptive) will outperform the static baseline by 10-20%, providing justification for automated parameter tuning in live trading.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
