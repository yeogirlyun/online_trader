# OnlineEnsemble CLI Guide

Complete guide for using the OnlineEnsemble trading strategy workflow CLI.

---

## Quick Start

```bash
# Complete workflow (one command)
./scripts/run_ensemble_workflow.sh

# Or step-by-step
sentio_cli generate-signals --data data/QQQ.csv --output signals.jsonl
sentio_cli execute-trades --signals signals.jsonl --data data/QQQ.csv --output trades.jsonl
sentio_cli analyze-trades --trades trades.jsonl
```

---

## Command Reference

### 1. Generate Signals

Generate trading signals from market data using OnlineEnsemble strategy.

#### Usage
```bash
sentio_cli generate-signals --data <path> [OPTIONS]
```

#### Required Arguments
- `--data <path>` - Path to market data file (CSV or binary)

#### Optional Arguments
| Argument | Default | Description |
|----------|---------|-------------|
| `--output <path>` | signals.jsonl | Output signal file path |
| `--warmup <bars>` | 100 | Warmup period before trading |
| `--start <bar>` | 0 | Start bar index |
| `--end <bar>` | all | End bar index (-1 = all) |
| `--csv` | false | Output in CSV format |
| `--verbose, -v` | false | Show progress updates |

#### Examples

**Basic usage:**
```bash
sentio_cli generate-signals --data data/SPY_1min.csv
```

**Custom warmup and range:**
```bash
sentio_cli generate-signals \
    --data data/QQQ.bin \
    --warmup 200 \
    --start 1000 \
    --end 5000 \
    --output my_signals.jsonl
```

**CSV output with verbose progress:**
```bash
sentio_cli generate-signals \
    --data data/futures.bin \
    --csv \
    --verbose \
    --output signals.csv
```

#### Output Format (JSONL)

Each line contains:
```json
{
    "bar_id": 12345,
    "timestamp_ms": 1609459200000,
    "bar_index": 123,
    "symbol": "QQQ",
    "probability": 0.6234,
    "confidence": 0.82,
    "signal_type": "1",
    "prediction_horizon": 5,
    "ensemble_agreement": 0.75
}
```

**Signal Types:**
- `0` = NEUTRAL
- `1` = LONG
- `2` = SHORT

---

### 2. Execute Trades

Execute trades from signal file and generate portfolio history.

#### Usage
```bash
sentio_cli execute-trades --signals <path> --data <path> [OPTIONS]
```

#### Required Arguments
- `--signals <path>` - Path to signal file (JSONL or CSV)
- `--data <path>` - Path to market data file

#### Optional Arguments
| Argument | Default | Description |
|----------|---------|-------------|
| `--output <path>` | trades.jsonl | Output trade file path |
| `--capital <amount>` | 100000 | Starting capital ($) |
| `--buy-threshold <val>` | 0.53 | Buy signal threshold |
| `--sell-threshold <val>` | 0.47 | Sell signal threshold |
| `--no-kelly` | false | Disable Kelly criterion sizing |
| `--csv` | false | Output in CSV format |
| `--verbose, -v` | false | Show each trade |

#### Examples

**Basic execution:**
```bash
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/SPY.csv
```

**Custom capital and thresholds:**
```bash
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/QQQ.bin \
    --capital 50000 \
    --buy-threshold 0.55 \
    --sell-threshold 0.45
```

**Aggressive trading (lower thresholds):**
```bash
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/futures.bin \
    --buy-threshold 0.51 \
    --sell-threshold 0.49 \
    --verbose
```

**Disable Kelly sizing (fixed position sizes):**
```bash
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/QQQ.csv \
    --no-kelly
```

#### Output Files

1. **Trade history** (`trades.jsonl` or `trades.csv`)
   - Every trade executed
   - Portfolio state after each trade
   - P&L details

2. **Equity curve** (`trades_equity.csv`)
   - Bar-by-bar portfolio value
   - Drawdown curve
   - Useful for plotting

#### Trade Record Format (JSONL)

```json
{
    "bar_id": 12345,
    "timestamp_ms": 1609459200000,
    "bar_index": 123,
    "symbol": "QQQ",
    "action": "BUY",
    "quantity": 125.5,
    "price": 350.25,
    "trade_value": 43956.38,
    "fees": 43.96,
    "cash_balance": 56000.00,
    "portfolio_value": 100000.00,
    "position_quantity": 125.5,
    "position_avg_price": 350.25,
    "reason": "Buy signal above threshold (Prob: 0.6234)"
}
```

---

### 3. Analyze Trades

Analyze trade history and generate comprehensive performance reports.

#### Usage
```bash
sentio_cli analyze-trades --trades <path> [OPTIONS]
```

#### Required Arguments
- `--trades <path>` - Path to trade history file (JSONL or CSV)

#### Optional Arguments
| Argument | Default | Description |
|----------|---------|-------------|
| `--output <path>` | analysis_report.json | Output report file |
| `--summary-only` | false | Show only summary metrics |
| `--show-trades` | false | Display individual trades |
| `--csv` | false | Export report as CSV |
| `--no-json` | false | Don't save JSON report |

#### Examples

**Basic analysis:**
```bash
sentio_cli analyze-trades --trades trades.jsonl
```

**Show individual trades:**
```bash
sentio_cli analyze-trades \
    --trades trades.jsonl \
    --show-trades
```

**Export as CSV:**
```bash
sentio_cli analyze-trades \
    --trades trades.csv \
    --csv \
    --output report.csv
```

#### Metrics Calculated

**Returns:**
- Total return (%)
- Annualized return (%)
- Monthly return (%) - **TARGET: ≥10%**
- Daily return (%)

**Risk Metrics:**
- Max drawdown (%) - **TARGET: <15%**
- Average drawdown (%)
- Volatility (%)
- Downside deviation (%)
- Sharpe ratio - **TARGET: >1.5**
- Sortino ratio
- Calmar ratio

**Trading Metrics:**
- Total trades
- Win rate (%) - **TARGET: ≥60%**
- Profit factor
- Average win/loss (%)
- Largest win/loss (%)

**Position Metrics:**
- Long vs short trades
- Kelly criterion
- Avg/max position size

#### Sample Output

```
╔════════════════════════════════════════════════════════════╗
║         ONLINE ENSEMBLE PERFORMANCE REPORT                 ║
╚════════════════════════════════════════════════════════════╝

=== RETURNS ===
  Total Return:        45.23%
  Annualized Return:   18.92%
  Monthly Return:      11.58% ✅ TARGET MET!
  Daily Return:        0.55%

=== RISK METRICS ===
  Max Drawdown:        12.34%
  Sharpe Ratio:        1.82
  Sortino Ratio:       2.15
  Calmar Ratio:        1.53

=== TRADING METRICS ===
  Total Trades:        127
  Win Rate:            62.3% ✅ TARGET MET!
  Profit Factor:       1.85
  Avg Win:             1.82%
  Avg Loss:            1.15%

╔════════════════════════════════════════════════════════════╗
║                    TARGET CHECK                            ║
╠════════════════════════════════════════════════════════════╣
║ ✓ Monthly Return ≥ 10%:  PASS ✅                          ║
║ ✓ Win Rate ≥ 60%:        PASS ✅                          ║
║ ✓ Max Drawdown < 15%:    PASS ✅                          ║
║ ✓ Sharpe Ratio > 1.5:    PASS ✅                          ║
╚════════════════════════════════════════════════════════════╝
```

---

## Complete Workflow

### Option 1: Automated Script

Use the provided shell script for complete automation:

```bash
# Default settings
./scripts/run_ensemble_workflow.sh

# Custom settings via environment variables
export DATA_PATH="data/custom_data.csv"
export OUTPUT_DIR="results/experiment_001"
export WARMUP_BARS=200
export STARTING_CAPITAL=50000
export BUY_THRESHOLD=0.51
export VERBOSE=true
./scripts/run_ensemble_workflow.sh
```

**Environment Variables:**

| Variable | Default | Description |
|----------|---------|-------------|
| `DATA_PATH` | data/QQQ_1min.csv | Market data file |
| `OUTPUT_DIR` | results/experiment_TIMESTAMP | Output directory |
| `CLI_PATH` | build/sentio_cli | CLI executable path |
| `WARMUP_BARS` | 100 | Warmup period |
| `START_BAR` | 0 | Start bar index |
| `END_BAR` | -1 | End bar (-1 = all) |
| `STARTING_CAPITAL` | 100000 | Starting capital |
| `BUY_THRESHOLD` | 0.53 | Buy threshold |
| `SELL_THRESHOLD` | 0.47 | Sell threshold |
| `VERBOSE` | false | Verbose output |
| `CSV_OUTPUT` | false | CSV format |

### Option 2: Manual Step-by-Step

```bash
# Step 1: Generate signals
sentio_cli generate-signals \
    --data data/QQQ.csv \
    --output signals.jsonl \
    --warmup 100 \
    --verbose

# Step 2: Execute trades
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/QQQ.csv \
    --output trades.jsonl \
    --capital 100000 \
    --verbose

# Step 3: Analyze results
sentio_cli analyze-trades \
    --trades trades.jsonl \
    --output report.json
```

---

## Parameter Optimization

### Tuning for Better Performance

If not meeting targets, try these adjustments:

#### 1. Trade More (Increase Signal Capture)

```bash
# Lower thresholds
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/QQQ.csv \
    --buy-threshold 0.51 \
    --sell-threshold 0.49
```

#### 2. Bigger Positions (Higher Returns)

```bash
# Enable Kelly sizing (default, but verify)
sentio_cli execute-trades \
    --signals signals.jsonl \
    --data data/QQQ.csv
    # Kelly fraction is 0.25 by default in strategy
```

#### 3. Longer Warmup (Better Model)

```bash
# More warmup bars
sentio_cli generate-signals \
    --data data/QQQ.csv \
    --warmup 200
```

#### 4. Different Horizons

Edit `src/strategy/online_ensemble_strategy.cpp` to adjust horizons:

```cpp
// Default: {1, 5, 10}
prediction_horizons = {1, 3, 5, 10, 20};
horizon_weights = {0.15, 0.20, 0.30, 0.25, 0.10};
```

---

## Batch Experiments

### Testing Multiple Configurations

```bash
#!/bin/bash
# batch_test.sh

THRESHOLDS="0.51 0.52 0.53 0.54 0.55"
DATA="data/QQQ.csv"

for threshold in $THRESHOLDS; do
    echo "Testing threshold: $threshold"

    # Generate signals
    sentio_cli generate-signals \
        --data $DATA \
        --output signals_${threshold}.jsonl

    # Execute trades
    sentio_cli execute-trades \
        --signals signals_${threshold}.jsonl \
        --data $DATA \
        --buy-threshold $threshold \
        --sell-threshold $(echo "1.0 - $threshold" | bc) \
        --output trades_${threshold}.jsonl

    # Analyze
    sentio_cli analyze-trades \
        --trades trades_${threshold}.jsonl \
        --output report_${threshold}.json
done

# Compare results
echo "Results summary:"
for threshold in $THRESHOLDS; do
    monthly_return=$(jq .returns.monthly_return report_${threshold}.json)
    win_rate=$(jq .trading.win_rate report_${threshold}.json)
    echo "Threshold $threshold: Monthly=$monthly_return%, WinRate=$win_rate%"
done
```

---

## Troubleshooting

### Common Issues

#### 1. "CLI not found"
```bash
# Build the project first
./build.sh Release
```

#### 2. "Data file not found"
```bash
# Check path exists
ls -la data/

# Use absolute path
sentio_cli generate-signals --data /full/path/to/data.csv
```

#### 3. "No trades generated"
```bash
# Check signal quality
head signals.jsonl

# Try lower thresholds
sentio_cli execute-trades ... --buy-threshold 0.50 --sell-threshold 0.50
```

#### 4. "Performance below target"
```bash
# Tune parameters (see Parameter Optimization section)
# Increase warmup
# Lower thresholds
# Enable Kelly sizing
```

---

## Output File Formats

### Signals (JSONL)
```json
{"bar_id":1,"timestamp_ms":1609459200000,"probability":0.623,"signal_type":"1",...}
{"bar_id":2,"timestamp_ms":1609459260000,"probability":0.512,"signal_type":"0",...}
```

### Trades (JSONL)
```json
{"bar_id":5,"symbol":"QQQ","action":"BUY","quantity":125.5,"price":350.25,...}
{"bar_id":15,"symbol":"QQQ","action":"SELL","quantity":125.5,"price":355.80,...}
```

### Equity Curve (CSV)
```csv
bar_index,equity,drawdown
0,100000.00,0.0000
1,100125.50,0.0000
2,99850.25,0.0027
```

### Analysis Report (JSON)
```json
{
  "returns": {
    "total_return_pct": 45.23,
    "monthly_return": 11.58
  },
  "risk": {
    "max_drawdown": 0.1234,
    "sharpe_ratio": 1.82
  },
  "trading": {
    "win_rate": 0.623,
    "profit_factor": 1.85
  },
  "targets_met": {
    "monthly_return_10pct": true,
    "win_rate_60pct": true
  }
}
```

---

## Advanced Usage

### Custom Strategy Config

Modify `online_ensemble_strategy.h` before building:

```cpp
struct OnlineEnsembleConfig {
    std::vector<int> prediction_horizons = {1, 3, 5, 10, 20};
    std::vector<double> horizon_weights = {0.15, 0.20, 0.30, 0.25, 0.10};

    double lambda = 0.995;
    double buy_threshold = 0.51;
    double sell_threshold = 0.49;
    double kelly_fraction = 0.30;
};
```

Then rebuild:
```bash
./build.sh Release
```

---

## Examples

### Example 1: Quick Test

```bash
sentio_cli generate-signals --data data/sample.csv --output sig.jsonl
sentio_cli execute-trades --signals sig.jsonl --data data/sample.csv --output trades.jsonl
sentio_cli analyze-trades --trades trades.jsonl
```

### Example 2: Production Run

```bash
./scripts/run_ensemble_workflow.sh \
    DATA_PATH=data/QQQ_full.csv \
    OUTPUT_DIR=results/production_run \
    WARMUP_BARS=200 \
    STARTING_CAPITAL=100000 \
    VERBOSE=true
```

### Example 3: Parameter Sweep

See **Batch Experiments** section above.

---

## Next Steps

1. **Build the project**: `./build.sh Release`
2. **Prepare data**: Convert to CSV or binary format
3. **Run workflow**: `./scripts/run_ensemble_workflow.sh`
4. **Analyze results**: Check if targets met
5. **Optimize**: Tune parameters if needed
6. **Iterate**: Repeat with different settings

---

## Target Performance

| Metric | Target | How to Check |
|--------|--------|--------------|
| Monthly Return | ≥ 10% | `jq .returns.monthly_return report.json` |
| Win Rate | ≥ 60% | `jq .trading.win_rate report.json` |
| Max Drawdown | < 15% | `jq .risk.max_drawdown report.json` |
| Sharpe Ratio | > 1.5 | `jq .risk.sharpe_ratio report.json` |

**All targets must be met for successful strategy!** ✅
