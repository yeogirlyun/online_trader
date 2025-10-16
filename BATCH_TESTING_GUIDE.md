# Batch Mock Testing Guide

## Quick Start

Run mock tests across multiple trading days with automatic dashboard generation:

```bash
# Test full October 2025 (10 trading days)
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --generate-dashboards

# Test specific week
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-07 \
  --end-date 2025-10-11 \
  --generate-dashboards \
  --log-dir logs/week_oct7

# Fast test without dashboards
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --log-dir logs/october_fast
```

## Features

- **Flexible Date Ranges**: Test any period from your historical data
- **Automatic Trading Day Detection**: Extracts actual trading days from SPY data
- **Per-Day Dashboards**: Individual HTML reports for each trading day
- **Summary Report**: Markdown summary with links to all dashboards and data files
- **Clean Organization**: Structured output with date-based directories

## Output Structure

```
logs/october_tests/
├── 2025-10-01/
│   ├── trades.jsonl          # Trade execution data
│   ├── signals.jsonl         # Strategy signals
│   ├── decisions.jsonl       # Decision log
│   ├── positions.jsonl       # Position tracking
│   └── dashboard.html        # Interactive dashboard
├── 2025-10-02/
│   └── ...
├── 2025-10-03/
│   └── ...
└── dashboards/
    └── SUMMARY.md            # Summary with links to all days
```

## Command Options

| Option | Description | Example |
|--------|-------------|---------|
| `--start-date` | Start date (YYYY-MM-DD) | `--start-date 2025-10-01` |
| `--end-date` | End date (YYYY-MM-DD) | `--end-date 2025-10-14` |
| `--generate-dashboards` | Generate HTML dashboards | `--generate-dashboards` |
| `--dashboard-dir` | Dashboard output directory | `--dashboard-dir logs/my_dashboards` |
| `--log-dir` | Base log directory | `--log-dir logs/my_tests` |
| `--data-dir` | Historical data directory | `--data-dir data/equities` |
| `--config` | Strategy configuration | `--config config/my_params.json` |
| `--capital` | Starting capital | `--capital 100000` |

## Examples

### Example 1: October 2025 Full Month Test

```bash
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --generate-dashboards \
  --log-dir logs/october_full
```

**Output**: 10 trading days tested, individual dashboards + summary

### Example 2: First Week of October

```bash
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-03 \
  --generate-dashboards \
  --log-dir logs/october_week1
```

**Output**: 3 trading days tested with dashboards

### Example 3: Quick Performance Test (No Dashboards)

```bash
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --log-dir logs/october_quick
```

**Output**: Trade/signal files only, no dashboards (faster execution)

### Example 4: Custom Configuration

```bash
./build/sentio_cli mock \
  --mode mock \
  --start-date 2025-10-07 \
  --end-date 2025-10-11 \
  --generate-dashboards \
  --config config/aggressive_params.json \
  --capital 50000 \
  --log-dir logs/test_aggressive
```

**Output**: Tests with custom parameters and $50K starting capital

## Viewing Results

### Individual Day Dashboard

Open any day's dashboard in your browser:
```bash
open logs/october_tests/2025-10-14/dashboard.html
```

### Summary Report

View the summary markdown:
```bash
cat logs/october_tests/dashboards/SUMMARY.md
```

### Analyze Trade Data

```bash
# Count total trades
wc -l logs/october_tests/*/trades.jsonl

# View specific day's trades
head -20 logs/october_tests/2025-10-14/trades.jsonl

# Extract P&L for all days
for dir in logs/october_tests/2025-*/; do
  echo "$dir: $(grep -c "EXIT" "$dir/trades.jsonl") exits"
done
```

## Performance Tips

1. **Skip Dashboards for Speed**: Omit `--generate-dashboards` when testing many days
2. **Generate Dashboards Later**: Run batch test first, then generate dashboards manually:
   ```bash
   # Run tests (fast)
   ./build/sentio_cli mock --mode mock --start-date 2025-10-01 --end-date 2025-10-14

   # Generate dashboards afterward
   for dir in logs/rotation_trading/2025-*/; do
     python3 scripts/rotation_trading_dashboard.py \
       --trades "$dir/trades.jsonl" \
       --output "$dir/dashboard.html"
   done
   ```

3. **Use SSD for Data**: Place `data/equities/` on SSD for faster loading

## Troubleshooting

### No Trading Days Found

```
❌ No trading days found in date range
```

**Solution**: Check that SPY data exists for your date range:
```bash
awk -F',' 'NR>1 {split($1,a,"T"); print a[1]}' data/equities/SPY_RTH_NH.csv | sort -u
```

### Dashboard Generation Failed

```
❌ Dashboard generation failed
```

**Solution**: Ensure Python dashboard script is available:
```bash
python3 scripts/rotation_trading_dashboard.py --help
```

### Missing Data Files

```
❌ Could not open data/equities/SPY_RTH_NH.csv
```

**Solution**: Verify data directory:
```bash
ls -lh data/equities/*_RTH_NH.csv
```

## Integration with Existing Tools

### Compare with Single-Day Mode

```bash
# Batch mode
./build/sentio_cli mock --mode mock --start-date 2025-10-14 --end-date 2025-10-14

# Single-day mode (equivalent)
./build/sentio_cli mock --mode mock --date 2025-10-14
```

Both produce identical trading results.

### Use with Optuna Optimization

```bash
# 1. Run batch tests with different parameters
./build/sentio_cli mock --mode mock \
  --start-date 2025-10-01 --end-date 2025-10-14 \
  --config config/test_params_v1.json \
  --log-dir logs/param_test_v1

./build/sentio_cli mock --mode mock \
  --start-date 2025-10-01 --end-date 2025-10-14 \
  --config config/test_params_v2.json \
  --log-dir logs/param_test_v2

# 2. Compare results across all days
# (Aggregate trade data from both runs)
```

## Summary Report Format

The generated `dashboards/SUMMARY.md` includes:

- Test period (start/end dates)
- Number of trading days
- Table with links to:
  - Individual dashboards
  - Trade data files
  - Signal files
  - Decision logs
- Generation timestamp

Example:
```markdown
# Rotation Trading Batch Test Summary

## Test Period
- **Start Date**: 2025-10-01
- **End Date**: 2025-10-14
- **Trading Days**: 10

## Daily Results

| Date | Dashboard | Trades | Signals | Decisions |
|------|-----------|--------|---------|----------|
| 2025-10-01 | [View](logs/.../dashboard.html) | [trades.jsonl](...) | ... | ... |
| 2025-10-02 | [View](...) | ... | ... | ... |
...
```

---

## Advanced Usage

### Scripted Testing

Create a bash script for repeated testing:

```bash
#!/bin/bash
# test_multiple_periods.sh

./build/sentio_cli mock --mode mock \
  --start-date 2025-10-01 --end-date 2025-10-07 \
  --generate-dashboards \
  --log-dir logs/oct_week1

./build/sentio_cli mock --mode mock \
  --start-date 2025-10-08 --end-date 2025-10-14 \
  --generate-dashboards \
  --log-dir logs/oct_week2

echo "Testing complete. Results in logs/oct_week{1,2}/"
```

### Continuous Testing

Monitor strategy performance over time:

```bash
#!/bin/bash
# Add to crontab for weekly testing

DATE=$(date +%Y-%m-%d)
WEEK_AGO=$(date -d '7 days ago' +%Y-%m-%d)

./build/sentio_cli mock --mode mock \
  --start-date "$WEEK_AGO" \
  --end-date "$DATE" \
  --generate-dashboards \
  --log-dir "logs/weekly_$(date +%Y%W)"
```

---

**For more details, see**: `megadocs/BATCH_MOCK_TESTING_IMPLEMENTATION.md`
