# Multi-Symbol Rotation System - Detailed Design Document v2.0

**Date:** 2025-10-15  
**Status:** Ready for Implementation  
**Target MRD:** +0.5% per block (10.8x improvement from current +0.046%)

---

## 1. Executive Summary

This document consolidates the complete design for transitioning from a **single-symbol (SPY) Position State Machine** to a **12-symbol momentum rotation strategy**.

### Core Changes

| Aspect | Current v1.x | Target v2.0 |
|--------|-------------|-------------|
| **Symbols** | 1 (SPY) | 12 (SVIX, SVXY, TQQQ, UPRO, QQQ, SPY, SH, PSQ, SDS, SQQQ, UVXY, UVIX) |
| **Strategy** | 7-state PSM | Top-N rotation (simple ranking) |
| **Backend LOC** | ~3200 lines | ~650 lines (80% reduction) |
| **Position Logic** | Complex state transitions | Hold top 3 strongest signals |
| **Allocation** | Kelly + dynamic | Equal-weight (start simple) |
| **Learning** | Q-learning (runtime) | Optuna (offline optimization) |

**Philosophy:** *"Buy the strongest momentum, rotate when it weakens."*

---

## 2. System Architecture

```
┌──────────────────────────────────────────────────┐
│  DATA LAYER                                      │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│  │ Live       │  │ Mock       │  │ Warmup     │ │
│  │ (Alpaca WS)│  │ (CSV)      │  │ (Download) │ │
│  └──────┬─────┘  └──────┬─────┘  └──────┬─────┘ │
│         └────────────────┼────────────────┘       │
│                          v                        │
│              MultiSymbolDataManager               │
│              (Synchronize 12 symbols)             │
└──────────────────────────┬───────────────────────┘
                           │
┌──────────────────────────┼───────────────────────┐
│  STRATEGY LAYER          v                       │
│         MultiSymbolOESManager                    │
│         (12 OES instances)                       │
│                          │                        │
│                          v                        │
│         SignalAggregator                         │
│         (Rank by strength)                       │
└──────────────────────────┬───────────────────────┘
                           │
┌──────────────────────────┼───────────────────────┐
│  EXECUTION LAYER         v                       │
│         RotationPositionManager                  │
│         (Hold top 3, rotate when rank changes)   │
│                          │                        │
│                          v                        │
│         OrderGenerator → BrokerInterface         │
└──────────────────────────────────────────────────┘
```

---

## 3. Data Interfaces

### 3.1 Live Trading (Alpaca WebSocket)

```cpp
class AlpacaMultiSymbolFeed {
    void subscribe_bars(const std::vector<std::string>& symbols);
    SynchronizedBarSet get_next_bar_set(int timeout_ms = 2000);
};

struct SynchronizedBarSet {
    int64_t timestamp_ms;
    std::map<std::string, Bar> bars;           // All 12 symbols
    std::set<std::string> missing_symbols;     // Forward-fill if needed
    bool is_complete;                          // All symbols present?
};
```

**Connection:** `wss://stream.data.alpaca.markets/v2/iex`  
**Synchronization:** Wait max 2s for all 12 symbols, forward-fill missing (max 5 consecutive)

### 3.2 Mock Testing (Historical Replay)

```bash
# Extract session data for specific date
python3 tools/extract_warmup_multi.py \
    --symbols SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX \
    --date 2025-10-07 \
    --output data/tmp/session_20251007/

# Run mock trading (accelerated)
./build/sentio_cli live-trade \
    --mock \
    --mock-date 2025-10-07 \
    --mock-speed 39.0  # 39x real-time (10s per 390-bar day)
```

### 3.3 Warmup Data Pipeline

```bash
# 1. Download latest data (30 days, all 12 symbols)
python3 tools/data_downloader.py \
    --symbols SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX \
    --days 30 --source polygon --output data/equities

# 2. Extract 20 blocks warmup (7800 bars)
python3 tools/extract_warmup_multi.py \
    --blocks 20 --output data/equities/warmup_latest.csv

# 3. Run warmup (train all 12 OES instances)
./build/sentio_cli warmup-multi \
    --data data/equities/warmup_latest.csv \
    --output logs/warmup_state.bin
```

**Warmup Requirements:** 20 blocks × 390 bars/block = 7800 bars ≈ 13 trading days

---

## 4. Core Components

### 4.1 MultiSymbolOESManager

```cpp
class MultiSymbolOESManager {
    // Manage 12 independent OES instances
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_map_;
    
    void update_all(const SynchronizedBarSet& bar_set);
    std::map<std::string, SignalOutput> generate_signals(const SynchronizedBarSet& bar_set);
};
```

**Key Properties:**
- One OES per symbol (each learns symbol-specific patterns)
- Shared config (lambda, thresholds) across all symbols
- Independent learning state (no cross-symbol coupling)

### 4.2 SignalAggregator

```cpp
struct RankedSignal {
    std::string symbol;
    SignalOutput signal;
    double strength;           // probability × confidence
    int rank;                  // 1 = strongest, 12 = weakest
};

class SignalAggregator {
    std::vector<RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals,
        double buy_threshold = 0.55
    );
};
```

### 4.3 RotationPositionManager

```cpp
class RotationPositionManager {
    struct Config {
        int max_positions = 3;           // Hold top 3
        int min_hold_bars = 2;           // Min hold period
        double buy_threshold = 0.55;     // Min probability to enter
        SizingMethod sizing = EQUAL_WEIGHT;
    };
    
    std::vector<TradeOrder> update(
        const std::vector<RankedSignal>& ranked_signals,
        uint64_t current_bar_id,
        double available_capital
    );
    
private:
    bool should_rotate(const ManagedPosition& pos, 
                      const std::vector<RankedSignal>& signals,
                      uint64_t bar_id) {
        // Rule 1: Enforce min hold
        if (bar_id - pos.entry_bar_id < min_hold_bars) return false;
        
        // Rule 2: Check if still in top N
        for (int i = 0; i < max_positions; ++i) {
            if (signals[i].symbol == pos.symbol) return false;
        }
        
        // Rule 3: Not in top N → rotate
        return true;
    }
};
```

**Rotation Logic:** Simple, transparent ranking. No state machine needed.

---

## 5. Backend Simplification

### 5.1 Components to DELETE

| Component | Files | Lines | Reason |
|-----------|-------|-------|--------|
| Position State Machine | `position_state_machine.{h,cpp}` | ~800 | Replaced by RotationPositionManager |
| Enhanced PSM | `enhanced_position_state_machine.{h,cpp}` | ~400 | Not needed |
| Adaptive Thresholds | `adaptive_trading_mechanism.{h,cpp}` | ~1200 | Use Optuna offline |
| Dynamic Allocation | `dynamic_allocation_manager.{h,cpp}` | ~600 | Equal-weight sufficient |
| Dynamic Hysteresis | `dynamic_hysteresis_manager.{h,cpp}` | ~400 | Not needed |

**Total Deletion:** ~3400 lines

### 5.2 New Backend Structure

```
backend/
├── rotation_position_manager.{h,cpp}      NEW (~300 lines)
├── simple_risk_manager.{h,cpp}            NEW (~150 lines)
└── order_generator.{h,cpp}                REFACTORED (~200 lines)

Total: ~650 lines (vs 3200 in v1.x)
```

---

## 6. Configuration

### 6.1 Rotation Parameters (`config/rotation_params.json`)

```json
{
  "version": "2.0",
  "parameters": {
    "max_positions": 3,
    "min_hold_bars": 2,
    "buy_threshold": 0.557,
    "rotation_threshold": 1.23,
    "sizing_method": "EQUAL_WEIGHT"
  },
  "oes_config": {
    "ewrls_lambda": 0.995,
    "buy_threshold": 0.557,
    "bb_amplification": true
  }
}
```

**Optimization:** Use `tools/optuna_rotation_optimizer.py` to find optimal parameters on 100-block dataset.

### 6.2 Symbol Configuration (`config/symbols.json`)

```json
{
  "active_symbols": [
    "SVIX", "SVXY", "TQQQ", "UPRO", "QQQ", "SPY",
    "SH", "PSQ", "SDS", "SQQQ", "UVXY", "UVIX"
  ],
  "symbol_metadata": {
    "TQQQ": { "leverage": 3.0, "direction": "long", "underlying": "QQQ" },
    "SQQQ": { "leverage": -3.0, "direction": "short", "underlying": "QQQ" }
  }
}
```

---

## 7. Implementation Phases (5 Weeks)

### Week 1: Data Infrastructure
- [ ] `MultiSymbolDataManager` (synchronization)
- [ ] `AlpacaMultiSymbolFeed` (WebSocket integration)
- [ ] `MockMultiSymbolFeed` (CSV replay)
- [ ] `tools/download_all_symbols.sh`

**Milestone:** All 12 symbols downloading + synchronizing

### Week 2: Multi-Symbol OES
- [ ] `MultiSymbolOESManager` (12 OES instances)
- [ ] `SignalAggregator` (ranking logic)
- [ ] `scripts/comprehensive_warmup_multi.sh`

**Milestone:** 12 signals generated per bar, ranked correctly

### Week 3: Rotation Backend
- [ ] `RotationPositionManager` (rotation logic)
- [ ] Archive old backend (PSM, Q-learning, etc.)
- [ ] Integration tests

**Milestone:** Mock trading works end-to-end

### Week 4: Backtesting
- [ ] Backtest on 100-block dataset
- [ ] Optuna optimization (200 trials)
- [ ] Walk-forward validation

**Milestone:** MRD > +0.2%

### Week 5: Paper Trading
- [ ] Deploy to Alpaca paper account
- [ ] Monitor for 5 days
- [ ] Daily performance reports

**Milestone:** MRD > +0.3% → Production deployment

---

## 8. Testing Strategy

### 8.1 Unit Tests (>80% coverage)

| Component | Test File | Key Scenarios |
|-----------|-----------|---------------|
| MultiSymbolDataManager | `test_multi_symbol_data.cpp` | Sync, forward-fill, late arrivals |
| MultiSymbolOESManager | `test_multi_oes.cpp` | 12 OES init, signal generation |
| SignalAggregator | `test_signal_aggregator.cpp` | Ranking, filtering |
| RotationPositionManager | `test_rotation_manager.cpp` | Min hold, rotation logic |

### 8.2 Integration Tests

```bash
# End-to-end mock trading
./build/sentio_cli live-trade --mock --mock-date 2025-10-07 --mock-speed 0

# Warmup pipeline
./scripts/comprehensive_warmup_multi.sh

# Paper trading (live)
./build/sentio_cli live-trade --config config/rotation_params.json
```

### 8.3 Performance Benchmarks

| Metric | Target |
|--------|--------|
| Signal generation latency | < 50ms (12 signals) |
| Memory usage | < 500 MB (12 OES) |
| Warmup time | < 60s (7800 bars) |

---

## 9. Success Metrics

| Metric | Backtest Target | Production Target | Current v1.x |
|--------|-----------------|-------------------|--------------|
| **MRD** | +0.3% | +0.5% | +0.046% |
| **Sharpe** | 1.5 | 2.0 | 0.8 |
| **Max Drawdown** | < 5% | < 3% | 8% |
| **Win Rate** | 55% | 60% | 48% |

---

## 10. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **Data sync failures** | Forward-fill (max 5), skip bar if >5 missing |
| **WebSocket disconnects** | Auto-reconnect (5 retries), save state |
| **Overfitting** | Walk-forward validation, Optuna regularization |
| **Overtrading** | Min hold = 2 bars, optimize rotation threshold |
| **Flash crash** | Circuit breaker at 5% drawdown |

---

## 11. Reference: Source Modules

### 11.1 Modules to REUSE (Existing Codebase)

**Strategy Layer:**
- `include/strategy/online_ensemble_strategy.h` - Core OES implementation (reuse as-is)
- `src/strategy/online_ensemble_strategy.cpp`
- `include/strategy/signal_output.h` - Signal structure
- `include/learning/online_predictor.h` - EWRLS predictor

**Feature Engineering:**
- `include/features/unified_feature_engine.h` - Feature extraction (45+ features)
- `src/features/unified_feature_engine.cpp`
- `include/features/indicators.h` - Technical indicators
- `include/features/rolling.h` - Rolling window calculations

**Data Management:**
- `include/core/data_manager.h` - Bar management utilities
- `src/core/data_manager.cpp`
- `include/common/types.h` - Bar, TradeOrder, Portfolio types
- `include/common/time_utils.h` - Timestamp utilities

**Live Trading:**
- `include/live/alpaca_client.hpp` - Alpaca API interface (extend for WebSocket)
- `src/live/alpaca_client.cpp`
- `include/live/bar_feed_interface.h` - Bar feed abstraction
- `include/live/mock_bar_feed_replay.h` - Mock replay (extend for multi-symbol)
- `src/live/mock_bar_feed_replay.cpp`
- `include/live/broker_client_interface.h` - Broker abstraction

**Configuration:**
- `include/common/config_loader.h` - JSON config loading
- `src/common/config_loader.cpp`
- `include/common/json_utils.h` - JSON utilities

**Analysis:**
- `include/analysis/performance_analyzer.h` - Performance metrics
- `src/analysis/performance_analyzer.cpp`

**Testing:**
- `include/testing/test_framework.h` - Test infrastructure
- `src/testing/test_framework.cpp`

### 11.2 Modules to CREATE (New Components)

**Data Layer:**
- `include/data/multi_symbol_data_manager.h` - NEW
- `src/data/multi_symbol_data_manager.cpp` - NEW
- `include/data/alpaca_multi_feed.h` - NEW (WebSocket for 12 symbols)
- `src/data/alpaca_multi_feed.cpp` - NEW
- `include/data/mock_multi_feed.h` - NEW (multi-symbol replay)
- `src/data/mock_multi_feed.cpp` - NEW

**Strategy Layer:**
- `include/strategy/multi_symbol_oes_manager.h` - NEW (manage 12 OES)
- `src/strategy/multi_symbol_oes_manager.cpp` - NEW
- `include/strategy/signal_aggregator.h` - NEW (ranking logic)
- `src/strategy/signal_aggregator.cpp` - NEW

**Backend Layer:**
- `include/backend/rotation_position_manager.h` - NEW (rotation logic)
- `src/backend/rotation_position_manager.cpp` - NEW
- `include/backend/simple_risk_manager.h` - NEW (basic risk checks)
- `src/backend/simple_risk_manager.cpp` - NEW

**CLI:**
- `src/cli/warmup_multi_command.cpp` - NEW (multi-symbol warmup)

### 11.3 Modules to DELETE (Archive First)

**Backend (Complex PSM):**
- `include/backend/position_state_machine.h` - DELETE (archive to `archive/v1_backend/`)
- `src/backend/position_state_machine.cpp` - DELETE
- `include/backend/enhanced_position_state_machine.h` - DELETE
- `src/backend/enhanced_position_state_machine.cpp` - DELETE
- `include/backend/ensemble_position_state_machine.h` - DELETE
- `src/backend/ensemble_position_state_machine.cpp` - DELETE

**Backend (Adaptive Learning):**
- `include/backend/adaptive_trading_mechanism.h` - DELETE (use Optuna)
- `src/backend/adaptive_trading_mechanism.cpp` - DELETE
- `include/backend/dynamic_allocation_manager.h` - DELETE
- `src/backend/dynamic_allocation_manager.cpp` - DELETE
- `include/backend/dynamic_hysteresis_manager.h` - DELETE
- `src/backend/dynamic_hysteresis_manager.cpp` - DELETE

**Archive Command:**
```bash
mkdir -p archive/v1_backend
mv include/backend/{position_state_machine,enhanced_position_state_machine,adaptive_trading_mechanism,dynamic_*}.* archive/v1_backend/
mv src/backend/{position_state_machine,enhanced_position_state_machine,adaptive_trading_mechanism,dynamic_*}.* archive/v1_backend/
```

### 11.4 Tools and Scripts

**Existing Tools (to Extend):**
- `tools/data_downloader.py` - Extend for 12 symbols
- `scripts/launch_trading.sh` - Modify for rotation mode
- `scripts/comprehensive_warmup.sh` - Extend to `comprehensive_warmup_multi.sh`

**New Tools:**
- `tools/extract_warmup_multi.py` - NEW (extract multi-symbol warmup)
- `tools/generate_multi_symbol_data.py` - NEW (synthetic leveraged ETFs)
- `tools/optuna_rotation_optimizer.py` - NEW (optimize rotation params)
- `tools/archive_old_backend.sh` - NEW (archive v1.x backend)
- `scripts/download_all_symbols.sh` - NEW (download 12 symbols)

### 11.5 Configuration Files

**New Configs:**
- `config/symbols.json` - NEW (12 symbol definitions)
- `config/rotation_params.json` - NEW (Optuna-optimized params)

**Existing (Reuse):**
- `config.env` - API keys (reuse)

---

## 12. Next Steps

1. **Review & Approve** this design document
2. **Week 1 Start:** Implement `MultiSymbolDataManager` and data pipeline
3. **Archive old backend** before deletion (safety)
4. **Incremental deployment:** Build → Test → Validate each phase

**Estimated Total Effort:** 5 weeks (1 developer, full-time)  
**Risk Level:** Moderate (simpler system, but significant refactor)  
**Expected ROI:** 6-10x MRD improvement

---

**Document Status:** ✅ Ready for Implementation  
**Last Updated:** 2025-10-15  
**Version:** 2.0

