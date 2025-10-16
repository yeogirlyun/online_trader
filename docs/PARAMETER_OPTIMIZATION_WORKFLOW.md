# Parameter Optimization Workflow

## Overview

This document explains how to optimize trading parameters using Optuna and deploy them to production.

## Architecture

### Parameter Flow

```
Optuna Optimization → best_params.json → Live Trading
```

1. **Optuna finds best parameters** via parallel trials
2. **Results saved** to `config/best_params.json`
3. **Live trading loads** parameters on startup

### Thread Safety

✅ **Parallel Optuna trials are SAFE** because:
- Each trial runs in isolated process
- Parameters passed via CLI arguments (not shared config files)
- Unique temporary files per trial

## Workflow

### Step 1: Run Optuna Optimization

```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
./scripts/run_optuna_4blocks.sh
```

**What happens:**
- 50 trials with 4 parallel jobs (4x speedup)
- ~3-5 minutes runtime
- Results saved to `data/tmp/optuna_4blocks_parallel/optuna_results.json`
- **Automatically updates** `config/best_params.json`

### Step 2: Verify Best Parameters

Check the optimized parameters:

```bash
cat config/best_params.json
```

Example output:
```json
{
  "last_updated": "2025-10-08T16:30:00Z",
  "optimization_source": "optuna_strategy_c",
  "best_mrb": 0.00234,
  "parameters": {
    "buy_threshold": 0.537,
    "sell_threshold": 0.463,
    "ewrls_lambda": 0.996,
    "bb_amplification_factor": 0.15
  }
}
```

### Step 3: Deploy to Live Trading

**Option A: Automatic (Recommended)**

Live trading automatically loads `config/best_params.json` on startup using:

```cpp
#include "common/config_loader.h"

auto config = sentio::config::get_production_config();
```

**Option B: Manual Override**

Manually update parameters if needed:

```bash
python3 tools/update_best_params.py \
    --optuna-results data/tmp/optuna_results.json
```

### Step 4: Restart Live Trading

```bash
# Stop current live trading (if running)
pkill -f "sentio_cli live-trade"

# Start with new parameters
./build/sentio_cli live-trade
```

The system will log:
```
✅ Using optimized parameters from config/best_params.json
  buy_threshold: 0.537
  sell_threshold: 0.463
  ewrls_lambda: 0.996
  bb_amplification_factor: 0.15
```

## Files

### Configuration Files

- **`config/best_params.json`** - Production parameters (updated by Optuna)
- **`config.env`** - API credentials (NEVER commit to git)

### Scripts

- **`scripts/run_optuna_4blocks.sh`** - Run 4-block optimization
- **`tools/adaptive_optuna.py`** - Optuna framework
- **`tools/update_best_params.py`** - Update best_params.json manually

### Code

- **`include/common/config_loader.h`** - Parameter loading interface
- **`src/common/config_loader.cpp`** - Parameter loading implementation
- **`src/cli/live_trade_command.cpp`** - Live trading (uses config_loader)

## Advanced Usage

### Custom Optimization

Run with custom settings:

```bash
python3 tools/adaptive_optuna.py \
    --strategy C \
    --data data/equities/SPY_20blocks.csv \
    --build-dir build \
    --output data/tmp/optuna_custom/results.json \
    --n-trials 100 \
    --n-jobs 8
```

### A/B Testing

Compare different parameter sets:

```bash
# Save current best
cp config/best_params.json config/best_params_backup.json

# Run optimization on different dataset
./scripts/run_optuna_4blocks.sh

# Compare results
diff config/best_params.json config/best_params_backup.json
```

### Scheduled Optimization

Add to crontab for daily re-optimization:

```bash
# Run every day at 4 PM (after market close)
0 16 * * 1-5 cd /path/to/online_trader && ./scripts/run_optuna_4blocks.sh
```

## Safety Features

### 1. Fallback to Defaults

If `config/best_params.json` is missing or corrupt:
- System logs warning: `⚠️  Using hardcoded default parameters`
- Falls back to safe defaults
- Trading continues without interruption

### 2. Parameter Validation

All parameters are validated on load:
- `buy_threshold` > `sell_threshold`
- `ewrls_lambda` ∈ [0.990, 0.999]
- `bb_amplification_factor` ≥ 0

### 3. Audit Trail

Every optimization update logs:
- Timestamp
- Source optimization run
- Previous MRB vs new MRB
- All parameter changes

## Troubleshooting

### Optuna fails with "No trials completed"

**Cause**: All trials failed (data issues, build errors)

**Solution**:
```bash
# Check build
cd build && cmake --build . -j8

# Verify data file exists
ls -lh data/equities/SPY_4blocks.csv

# Run single trial manually
./build/sentio_cli generate-signals \
    --data data/equities/SPY_4blocks.csv \
    --output /tmp/test_signals.jsonl \
    --warmup 960 \
    --buy-threshold 0.55 \
    --sell-threshold 0.45
```

### Live trading not using new parameters

**Cause**: `config/best_params.json` not found or live trading not restarted

**Solution**:
```bash
# Verify file exists
cat config/best_params.json

# Restart live trading
pkill -f "sentio_cli live-trade"
./build/sentio_cli live-trade  # Should log "Using optimized parameters"
```

### Parameters seem worse than defaults

**Cause**: Overfitting to small dataset

**Solution**:
- Use larger dataset (20+ blocks recommended)
- Increase trials (100-200 for production)
- Use Strategy B (4-hour adaptive) for time-varying markets

## Best Practices

1. **Optimize on recent data** (last 30 days)
2. **Use 20+ blocks** for stable results
3. **Run 100+ trials** for production parameters
4. **Validate on out-of-sample data** before deploying
5. **Monitor live performance** and re-optimize if MRB degrades
6. **Keep backups** of working parameter sets

## Performance

- **4-block optimization**: ~3-5 minutes (50 trials, 4 parallel)
- **20-block optimization**: ~15-20 minutes (100 trials, 4 parallel)
- **Speedup from parallelization**: 4x (with n_jobs=4)

## Example Session

```bash
# 1. Run optimization
./scripts/run_optuna_4blocks.sh

# 2. Check results
cat config/best_params.json

# 3. Restart live trading
pkill -f "sentio_cli live-trade"
./build/sentio_cli live-trade

# 4. Monitor logs
tail -f logs/live_trading/signals_*.jsonl
```

## See Also

- [Optuna Documentation](https://optuna.org/)
- [Feature Caching (Deprecated)](megadocs/FEATURE_CACHING_DEPRECATED.md)
- [Live Trading Guide](CLI_GUIDE.md#live-trading)
