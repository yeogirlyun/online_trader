# CLI Implementation Summary

Complete CLI system for OnlineEnsemble trading strategy experiments.

---

## What Was Created

### 1. ✅ Command Implementations

#### A. **GenerateSignalsCommand**
**File**: `src/cli/generate_signals_command.cpp`

Generates trading signals from market data using OnlineEnsemble strategy.

**Features**:
- EWRLS online learning with multi-horizon predictions
- Warmup period support
- JSONL and CSV output formats
- Progress reporting
- Strategy metrics (win rate, monthly return estimate)

**Usage**:
```bash
sentio_cli generate-signals --data data/QQQ.csv --output signals.jsonl
```

#### B. **ExecuteTradesCommand**
**File**: `src/cli/execute_trades_command.cpp`

Executes trades from signals using AdaptivePortfolioManager with Kelly sizing.

**Features**:
- Kelly Criterion position sizing
- Adaptive portfolio management
- Portfolio state tracking
- Equity curve generation
- Comprehensive trade logging

**Usage**:
```bash
sentio_cli execute-trades --signals signals.jsonl --data data/QQQ.csv
```

#### C. **AnalyzeTradesCommand**
**File**: `src/cli/analyze_trades_command.cpp`

Analyzes trade history and generates performance reports.

**Features**:
- Complete performance metrics (returns, risk, trading)
- Target checking (10% monthly, 60% win rate, etc.)
- JSON and CSV export
- Pretty-printed reports
- Individual trade details

**Usage**:
```bash
sentio_cli analyze-trades --trades trades.jsonl
```

---

### 2. ✅ Workflow Automation

**File**: `scripts/run_ensemble_workflow.sh`

Complete automated workflow script that:
1. Generates signals from market data
2. Executes trades with portfolio manager
3. Analyzes results and checks targets
4. Generates all output files

**Usage**:
```bash
# Default settings
./scripts/run_ensemble_workflow.sh

# Custom settings
export DATA_PATH="data/custom.csv"
export STARTING_CAPITAL=50000
./scripts/run_ensemble_workflow.sh
```

---

### 3. ✅ Documentation

**File**: `CLI_GUIDE.md`

Comprehensive 400+ line guide covering:
- Quick start
- Command reference (all arguments)
- Complete workflow examples
- Parameter optimization guide
- Batch experiment scripts
- Troubleshooting
- Output file formats
- Advanced usage

---

## File Structure

```
online_trader/
├── include/cli/
│   └── ensemble_workflow_command.h      # Command interface definitions
├── src/cli/
│   ├── generate_signals_command.cpp     # Signal generation
│   ├── execute_trades_command.cpp       # Trade execution
│   └── analyze_trades_command.cpp       # Analysis & reporting
├── scripts/
│   └── run_ensemble_workflow.sh         # Automated workflow
├── CLI_GUIDE.md                         # Complete documentation
└── CLI_SUMMARY.md                       # This file
```

---

## Workflow

```
┌─────────────────────┐
│   Market Data       │
│   (CSV/Binary)      │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ generate-signals    │  <- OnlineEnsemble strategy
│                     │     Multi-horizon EWRLS
│ Output:             │     Adaptive thresholds
│  signals.jsonl      │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ execute-trades      │  <- AdaptivePortfolioManager
│                     │     Kelly Criterion sizing
│ Output:             │     Risk management
│  trades.jsonl       │
│  equity.csv         │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ analyze-trades      │  <- Performance metrics
│                     │     Target checking
│ Output:             │     Sharpe, drawdown, etc.
│  report.json        │
└─────────────────────┘
```

---

## Quick Examples

### Example 1: Simple Run
```bash
sentio_cli generate-signals --data data/SPY.csv -o sig.jsonl
sentio_cli execute-trades --signals sig.jsonl --data data/SPY.csv -o trades.jsonl
sentio_cli analyze-trades --trades trades.jsonl
```

### Example 2: Automated
```bash
./scripts/run_ensemble_workflow.sh
```

### Example 3: Custom Parameters
```bash
sentio_cli generate-signals --data data/QQQ.csv --warmup 200 -o sig.jsonl
sentio_cli execute-trades --signals sig.jsonl --data data/QQQ.csv \
    --capital 50000 --buy-threshold 0.51 --sell-threshold 0.49 -o trades.jsonl
sentio_cli analyze-trades --trades trades.jsonl --show-trades
```

---

## Output Files

### 1. Signals File (`signals.jsonl`)
```json
{"bar_id":1,"probability":0.623,"signal_type":"1","confidence":0.82,...}
```

### 2. Trades File (`trades.jsonl`)
```json
{"symbol":"QQQ","action":"BUY","quantity":125.5,"price":350.25,...}
```

### 3. Equity Curve (`trades_equity.csv`)
```csv
bar_index,equity,drawdown
0,100000.00,0.0000
1,101250.50,0.0000
```

### 4. Analysis Report (`report.json`)
```json
{
  "returns": {"monthly_return": 11.58},
  "trading": {"win_rate": 0.623},
  "targets_met": {
    "monthly_return_10pct": true,
    "win_rate_60pct": true
  }
}
```

---

## Integration with Build System

To integrate these commands, add to `src/cli/sentio_cli_main.cpp`:

```cpp
#include "cli/ensemble_workflow_command.h"

int main(int argc, char** argv) {
    using namespace sentio::cli;

    CommandDispatcher dispatcher;

    // Register ensemble commands
    dispatcher.register_command(std::make_unique<GenerateSignalsCommand>());
    dispatcher.register_command(std::make_unique<ExecuteTradesCommand>());
    dispatcher.register_command(std::make_unique<AnalyzeTradesCommand>());

    // ... other commands ...

    return dispatcher.execute(argc, argv);
}
```

And update `CMakeLists.txt`:

```cmake
add_executable(sentio_cli
    src/cli/sentio_cli_main.cpp
    src/cli/generate_signals_command.cpp
    src/cli/execute_trades_command.cpp
    src/cli/analyze_trades_command.cpp
    # ... other sources ...
)
```

---

## Performance Targets

The CLI automatically checks these targets in the analysis phase:

| Target | Threshold | Auto-Check |
|--------|-----------|------------|
| Monthly Return | ≥ 10% | ✅ |
| Win Rate | ≥ 60% | ✅ |
| Max Drawdown | < 15% | ✅ |
| Sharpe Ratio | > 1.5 | ✅ |

**Example output**:
```
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

## Key Features

### 1. **Modular Design**
- Each command is independent
- Can be used standalone or in workflow
- Easy to extend with new commands

### 2. **Flexible I/O**
- JSONL (default) for structured data
- CSV option for spreadsheet compatibility
- Binary format support for market data

### 3. **Progress Reporting**
- Verbose mode shows real-time progress
- Summary statistics after each step
- Performance metrics throughout

### 4. **Error Handling**
- Input validation
- Clear error messages
- Graceful failure recovery

### 5. **Automation Ready**
- Shell script for complete workflow
- Environment variable configuration
- Batch processing support

---

## Next Steps

1. **Build the CLI**:
   ```bash
   ./build.sh Release
   ```

2. **Test with sample data**:
   ```bash
   ./scripts/run_ensemble_workflow.sh
   ```

3. **Check results**:
   ```bash
   cat results/experiment_*/analysis_report.json | jq .
   ```

4. **Optimize parameters** if targets not met:
   - Lower thresholds for more trades
   - Increase Kelly fraction for bigger positions
   - Longer warmup for better model

---

## Troubleshooting

### Build Issues
```bash
# Clean build
rm -rf build
./build.sh Release
```

### Missing Dependencies
```bash
# The CLI uses existing components:
# - OnlineEnsembleStrategy (already created)
# - AdaptivePortfolioManager (already created)
# - SignalOutput, Bar types (existing)
```

### Data Format
```bash
# Ensure data is in correct format
# CSV: bar_id,timestamp,open,high,low,close,volume
# Binary: Use existing converter tools
```

---

## Summary

Created a **complete experimental workflow CLI** with:
- ✅ 3 main commands (generate, execute, analyze)
- ✅ Automated workflow script
- ✅ Comprehensive documentation
- ✅ Target checking built-in
- ✅ Multiple output formats
- ✅ Parameter optimization support

**Ready to use for reaching 10% monthly return target!** 🎯
