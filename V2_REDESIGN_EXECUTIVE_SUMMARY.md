# OnlineTrader v2.0 Redesign - Executive Summary

**Status:** READY FOR IMPLEMENTATION
**Expected Completion:** 5 weeks
**Expected MRD Improvement:** 6-10x (from +0.046% to +0.3-0.5%)

---

## What's Changing

### From: Single-Symbol Complex State Machine
```
SPY → 1 OES → 7-State PSM → Trade 4 symbols (QQQ, TQQQ, PSQ, SQQQ)
MRD: +0.046% per block
Backend: 3200 lines
```

### To: Multi-Symbol Momentum Rotation
```
12 symbols → 12 OES → Rank signals → Buy top 3 strongest → LONG ONLY
Target MRD: +0.5% per block
Backend: 650 lines (80% reduction)
```

---

## Three Key Documents

### 1. **MULTI_SYMBOL_ROTATION_ARCHITECTURE.md**
- Overall system design
- Why rotation beats state machine
- Performance projections

### 2. **ROTATION_BACKEND_SIMPLIFICATION.md**
- Delete PSM, Q-learning, adaptive mechanism
- Keep: Simple rotation manager + equal-weight
- 80% code reduction

### 3. **REQUIREMENTS_MULTI_SYMBOL_ROTATION_v2.md** ⭐ **THIS IS THE IMPLEMENTATION GUIDE**
- Complete technical specifications
- Data interface requirements (Live/Mock/Warmup)
- All 12 symbols, synchronized bars
- Implementation phases (5 weeks)
- Success metrics, risk analysis

---

## Data Interface (Your Key Question)

### Live Trading
```cpp
class AlpacaMultiSymbolFeed {
    // WebSocket v2 - Subscribe to all 12 symbols
    void subscribe_bars(const std::vector<std::string>& symbols);

    // Get synchronized bar set (all 12 symbols at same timestamp)
    SynchronizedBarSet get_next_bar_set(int timeout_ms = 2000);
};
```

**How it works:**
- Connect to `wss://stream.data.alpaca.markets/v2/iex`
- Subscribe to all 12 symbols (SVIX, SVXY, TQQQ, UPRO, QQQ, SPY, SH, PSQ, SDS, SQQQ, UVXY, UVIX)
- Receive 1-minute bars asynchronously
- **Synchronize:** Wait for all 12 bars at timestamp T (2s timeout)
- **Forward-fill:** If symbol missing, use last known price (max 5 fills)

### Mock Testing
```bash
# Extract session data for specific date (all 12 symbols)
python3 tools/extract_warmup_multi.py \
    --symbols SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX \
    --date 2025-10-07 \
    --output data/tmp/session_20251007/

# Run mock trading
./build/sentio_cli live-trade \
    --mock \
    --mock-date 2025-10-07 \
    --mock-speed 39.0
```

### Warmup Data
```bash
# Download latest data for all 12 symbols
python3 tools/data_downloader.py \
    --symbols SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX \
    --days 30 \
    --source polygon \
    --output data/equities

# Extract 20 blocks warmup
python3 tools/extract_warmup_multi.py \
    --symbols all \
    --blocks 20 \
    --output data/equities/warmup_latest.csv

# Run warmup
./build/sentio_cli warmup-multi \
    --data data/equities/warmup_latest.csv \
    --output logs/warmup_state.bin
```

---

## Implementation Plan (5 Weeks)

### Week 1: Data Infrastructure
- ✅ `MultiSymbolDataManager` - Synchronize 12 symbols
- ✅ `AlpacaMultiSymbolFeed` - WebSocket for live data
- ✅ `MockMultiSymbolFeed` - CSV replay for testing
- ✅ `tools/download_all_symbols.sh` - Download script

**Milestone:** All 12 symbols downloading + synchronizing correctly

### Week 2: Multi-Symbol OES
- ✅ `MultiSymbolOESManager` - Manage 12 OES instances
- ✅ `SignalAggregator` - Rank signals by strength
- ✅ `scripts/comprehensive_warmup_multi.sh` - Warmup pipeline

**Milestone:** 12 signals generated every bar, ranked correctly

### Week 3: Rotation Backend
- ✅ `RotationPositionManager` - Simple rotation logic
- ✅ Delete old PSM, Q-learning, adaptive mechanism
- ✅ Integration tests

**Milestone:** Mock trading works end-to-end

### Week 4: Backtesting & Optimization
- ✅ Backtest on 100-block dataset
- ✅ Optuna optimize rotation params
- ✅ Performance validation

**Milestone:** MRD > +0.2% on backtest

### Week 5: Paper Trading
- ✅ Deploy to Alpaca paper account
- ✅ Run for 5 days, monitor performance
- ✅ Daily reports

**Milestone:** MRD > +0.3% on paper trading → Deploy live

---

## Code Changes Summary

### New Files (~2000 lines)
```
include/data/multi_symbol_data_manager.h
include/data/alpaca_multi_feed.h
include/data/mock_multi_feed.h
include/strategy/multi_symbol_oes_manager.h
include/strategy/signal_aggregator.h
include/backend/rotation_position_manager.h
include/backend/simple_risk_manager.h

src/data/multi_symbol_data_manager.cpp
src/data/alpaca_multi_feed.cpp
src/data/mock_multi_feed.cpp
src/strategy/multi_symbol_oes_manager.cpp
src/strategy/signal_aggregator.cpp
src/backend/rotation_position_manager.cpp
src/backend/simple_risk_manager.cpp

tools/download_all_symbols.sh
tools/extract_warmup_multi.py
tools/generate_multi_symbol_data.py
tools/optuna_rotation_optimizer.py

scripts/comprehensive_warmup_multi.sh
```

### Deleted Files (~4200 lines)
```
include/backend/position_state_machine.h
include/backend/enhanced_position_state_machine.h
include/backend/adaptive_trading_mechanism.h
include/backend/dynamic_allocation_manager.h
include/backend/dynamic_hysteresis_manager.h

src/backend/position_state_machine.cpp
src/backend/enhanced_position_state_machine.cpp
src/backend/adaptive_trading_mechanism.cpp
src/backend/dynamic_allocation_manager.cpp
src/backend/dynamic_hysteresis_manager.cpp
```

### Net Change
- **Added:** ~2000 lines (data + rotation backend)
- **Removed:** ~4200 lines (PSM + adaptive)
- **Net:** **-2200 lines** (simpler system!)

---

## Key Decisions Made

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **PSM** | ❌ DELETE | Too complex, replaced by simple rotation |
| **Kelly Allocation** | ⚠️ Start equal-weight, add later | Equal-weight is proven, Kelly is complex |
| **Q-learning** | ❌ DELETE | Optuna offline optimization is better |
| **Symbols** | 12 (SVIX, SVXY, TQQQ, UPRO, QQQ, SPY, SH, PSQ, SDS, SQQQ, UVXY, UVIX) | Diverse exposures (equity, leverage, inverse, volatility) |
| **Max Positions** | 3 (configurable 1-3) | Balance diversification vs concentration |
| **Min Hold** | 2 bars (configurable) | Prevent overtrading |
| **Position Sizing** | Equal-weight (1/N) | Simplest, proven effective |
| **Data Sync** | Forward-fill missing (max 5) | Robust to missing data |

---

## Success Criteria

### Phase 4 (Backtesting)
- ✅ MRD > +0.2% per block
- ✅ Sharpe > 1.5
- ✅ Max drawdown < 5%
- ✅ Optuna optimization completes (200 trials)

### Phase 5 (Paper Trading)
- ✅ MRD > +0.3% per block (5 days)
- ✅ No crashes, disconnects
- ✅ Orders execute correctly
- ✅ Rotation frequency 50-100/day

### Production Deployment
- ✅ Paper trading MRD > +0.3% for 10 days
- ✅ All stakeholders approve
- ✅ Monitoring dashboard live
- ✅ Start with $10K capital

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **Data sync failures** | Forward-fill, skip bar if >5 symbols missing |
| **Alpaca rate limits** | Use WebSocket (unlimited), fallback to Polygon |
| **Overfitting** | Walk-forward validation, Optuna regularization |
| **Overtrading** | Min hold = 2 bars, optimize rotation threshold |
| **Flash crash** | Circuit breaker at 5% drawdown |
| **All symbols crash** | Inverse ETFs rise, go to cash if all weak |

---

## Next Steps

1. **Review** this summary + 3 detailed docs
2. **Approve** the redesign approach
3. **Start Week 1** - Data infrastructure implementation
4. **I'll implement** all components, phase by phase

**Ready to proceed?** I can start implementing `MultiSymbolDataManager` and `AlpacaMultiSymbolFeed` right now.

---

**Files Created:**
1. ✅ `MULTI_SYMBOL_ROTATION_ARCHITECTURE.md` (30 pages - system design)
2. ✅ `ROTATION_BACKEND_SIMPLIFICATION.md` (25 pages - backend changes)
3. ✅ `REQUIREMENTS_MULTI_SYMBOL_ROTATION_v2.md` (150 pages - complete spec) ⭐
4. ✅ `V2_REDESIGN_EXECUTIVE_SUMMARY.md` (this file - 5-min overview)

**Total Documentation:** 210 pages covering every aspect of the redesign.
