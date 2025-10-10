# OnlineTrader Design Principles

**Date**: 2025-10-10
**Status**: ENFORCED

---

## 🎯 Core Principle #1: Self-Sufficient Launch Script

### The Golden Rule
**ALWAYS run `./scripts/launch_trading.sh [mock|live]` - NEVER run manual commands**

### What This Means
1. **For Mock Trading**:
   ```bash
   ./scripts/launch_trading.sh mock --date 2025-10-09
   ```
   **Must do everything**:
   - Run morning optimization (if needed)
   - Save baseline parameters for micro-adaptation
   - Extract session data
   - Generate leveraged ETF prices
   - Run mock trading session
   - Generate dashboard
   - Send email (if configured)
   - Report MRD and performance metrics

2. **For Live Trading**:
   ```bash
   ./scripts/launch_trading.sh live
   ```
   **Must do everything**:
   - Check if within trading hours
   - Run morning optimization (6-10 AM ET)
   - Save baseline parameters for micro-adaptation
   - Prepare warmup data
   - Start Alpaca bridge
   - Run live trading
   - Handle EOD liquidation
   - Generate dashboard
   - Send email
   - Auto-shutdown at market close

### What NOT To Do
❌ **NEVER** run manual commands like:
```bash
# WRONG - Don't do this!
python3 scripts/run_2phase_optuna.py --data ...
build/sentio_cli live-trade
python3 scripts/professional_trading_dashboard.py ...
```

✅ **ALWAYS** use the launch script:
```bash
# CORRECT - Always do this!
./scripts/launch_trading.sh mock
./scripts/launch_trading.sh live
```

### How To Fix Issues
If something is missing from the workflow:
1. **Improve `launch_trading.sh`** to handle it
2. **Or improve `sentio_cli`** to handle it internally
3. **Or create a helper script** and integrate it into launch_trading.sh
4. **Document the change** in this file

**DO NOT** add manual steps to documentation!

---

## Core Principle #2: Idempotency

Every operation should be idempotent:
- Running optimization twice = same result
- Running mock session twice = same result (deterministic)
- Restarting live trading = seamless continuation

---

## Core Principle #3: CRASH FAST, NO FALLBACK ⚠️

**CRITICAL SAFETY PRINCIPLE: If anything is not perfect, NEVER trade**

- If optimization fails → **EXIT IMMEDIATELY** (no fallback to old params)
- If data fetch fails → **EXIT IMMEDIATELY** (don't continue with stale data)
- If credentials missing → **EXIT with clear error** (don't attempt workarounds)
- If outside trading hours → Wait or exit gracefully (safe exception)

### Why NO FALLBACK?
Trading with unoptimized or stale parameters is **UNSAFE**:
- Market conditions change constantly
- Old parameters may cause significant losses
- Better to miss a trading day than lose money with bad parameters

### Examples of CRASH FAST:
```bash
# ✅ CORRECT - Crash immediately on failure
if ! run_optimization; then
    log_error "❌ FATAL: Optimization failed"
    exit 1
fi

# ❌ WRONG - DO NOT continue with fallback
if ! run_optimization; then
    log_info "⚠️  Using old parameters..."  # NEVER DO THIS!
fi
```

---

## Core Principle #4: Comprehensive Logging

Everything logged to:
- `logs/mock_trading/` (mock mode)
- `logs/live_trading/` (live mode)

Logs include:
- System events
- Signals (JSONL)
- Trades (JSONL)
- Decisions (JSONL)
- Positions (JSONL)
- Performance metrics

---

## Core Principle #5: Single Source of Truth

- **Parameters**: `config/best_params.json` (production)
- **Baseline**: `data/tmp/morning_baseline_params.json` (micro-adaptation)
- **Credentials**: `config.env` (not committed)
- **Data**: `data/equities/SPY_RTH_NH.csv` (canonical)

---

## Implementation Checklist

Before claiming a feature is "complete":
- [ ] Integrated into `launch_trading.sh`
- [ ] Works with `./scripts/launch_trading.sh mock`
- [ ] Works with `./scripts/launch_trading.sh live`
- [ ] No manual steps required
- [ ] Documented in `launch_trading.sh --help`
- [ ] Tested end-to-end

---

## Examples of Good Design

### ✅ Good: Morning Optimization
```bash
# Automatically runs during launch_trading.sh
if [ "$MODE" = "live" ] && [ $current_hour -ge 6 ] && [ $current_hour -lt 10 ]; then
    run_morning_optimization
fi
```

### ✅ Good: Data Preparation
```bash
# Automatically downloads missing data
if [ ! -f "data/equities/SPY_RTH_NH.csv" ]; then
    python3 tools/data_downloader.py SPY --outdir data/equities
fi
```

### ❌ Bad: Manual Steps
```markdown
# BAD DOCUMENTATION - Don't write this!
## Setup
1. First run: python3 scripts/run_2phase_optuna.py ...
2. Then run: ./scripts/launch_trading.sh mock
3. Finally analyze: python3 scripts/analyze_results.py ...
```

---

## Maintenance Guidelines

### When Adding New Features
Ask yourself:
1. Can this be integrated into `launch_trading.sh`?
2. If not, can `sentio_cli` handle it internally?
3. If not, create a helper script and call it from launch_trading.sh

### When Debugging
1. Run the full launch script: `./scripts/launch_trading.sh mock`
2. If it fails, fix the script (don't work around it)
3. Test the fix by running the script again
4. Commit the improved script

### When Documenting
1. Always show the single command: `./scripts/launch_trading.sh [mode]`
2. Explain what it does internally (optional)
3. Never require users to run multiple manual commands

---

## Version History

- **v2.6** (2025-10-10): Added micro-adaptation self-sufficiency
  - Morning optimization integrated
  - Baseline parameter saving automated
  - No manual steps for mock or live trading

- **v2.5** (2025-10-09): Added mock infrastructure
  - Full session replay with optimization
  - Dashboard generation integrated

- **v2.0** (2025-10-08): Production hardening
  - EOD safety mechanisms
  - Seamless warmup and position reconciliation

---

**Remember**: If you find yourself running manual commands, you're doing it wrong. Fix the script instead! 🛠️
