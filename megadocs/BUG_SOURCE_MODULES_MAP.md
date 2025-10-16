# Bug Source Modules Map
## Catastrophic Capital Depletion (-99.9% Daily Loss)

This document maps all source code modules involved in creating the catastrophic capital depletion bug.

---

## Module Dependency Tree

```
┌─────────────────────────────────────────────────────────────┐
│                    Trading Session Entry                     │
│              src/cli/rotation_trade_command.cpp              │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  Rotation Trading Backend                    │
│           src/backend/rotation_trading_backend.cpp           │
│         include/backend/rotation_trading_backend.h           │
│                                                               │
│  ├─ on_bar() [Line 220-295]        ← Main trading loop     │
│  ├─ execute_decision() [Line 391-490]  ← Trade execution   │
│  └─ calculate_position_size() [Line 516-578] ← 🔴 BUG #1   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│               Rotation Position Manager                      │
│       src/strategy/rotation_position_manager.cpp             │
│      include/strategy/rotation_position_manager.h            │
│                                                               │
│  ├─ make_decisions() [Line 18-300]  ← Decision logic       │
│  ├─ check_exit_conditions() [Line 537-601]  ← 🔴 BUG #2    │
│  └─ Position state update [Line 37-86]  ← Strength decay   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  Configuration System                        │
│              config/rotation_strategy.json                   │
│                                                               │
│  └─ Parameters [All]  ← 🔴 BUG #3 (Wrong params)           │
└─────────────────────────────────────────────────────────────┘
```

---

## Critical Source Modules

### Module 1: Position Sizing (Primary Bug)
**File**: `src/backend/rotation_trading_backend.cpp`
**Function**: `calculate_position_size()`
**Lines**: 516-578
**Bug Type**: Over-allocation

#### Critical Code Section
```cpp
int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    // 🔴 BUG: Uses starting_capital instead of current_equity
    int max_positions = config_.rotation_config.max_positions;
    double fixed_allocation = (config_.starting_capital * 0.95) / max_positions;
    //                         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //                         SHOULD BE: current_equity * risk_per_trade

    // 🔴 BUG: Takes 95% of available cash AGAIN (double allocation)
    double available_cash = current_cash_;
    double allocation = std::min(fixed_allocation, available_cash * 0.95);
    //                                              ^^^^^^^^^^^^^^^^^^^^^
    //                                              Compounds the problem

    if (allocation <= 100.0) {
        return 0;
    }

    double price = get_execution_price(decision.symbol, "BUY");
    int shares = static_cast<int>(allocation / price);

    // ❌ MISSING: No check for minimum viable position size
    // ❌ MISSING: No check for current drawdown
    // ❌ MISSING: No check for minimum trading capital

    return shares;
}
```

#### Impact Flow
```
Starting: $100,000
  ↓
fixed_allocation = $100k × 0.95 / 3 = $31,667 per position
  ↓
Position 1: $31,667 → Cash: $68,333
Position 2: $31,667 → Cash: $36,666
Position 3: $31,667 → Cash: $5,000 (95% deployed)
  ↓
After exits (5 minutes later): $11,917 (88% loss)
  ↓
Position 4: min($31,667, $11,917 × 0.95) = $11,321 → Cash: $596
Position 5: min($31,667, $596 × 0.95) = $566 → Cash: $30
  ↓
Final: $39 (99.96% loss)
```

**Fix Required**: Adaptive position sizing based on current equity

---

### Module 2: Exit Conditions (Secondary Bug)
**File**: `src/strategy/rotation_position_manager.cpp`
**Function**: `check_exit_conditions()`
**Lines**: 537-601
**Bug Type**: Premature exit, no minimum hold time

#### Critical Code Section
```cpp
RotationPositionManager::Decision RotationPositionManager::check_exit_conditions(
    const Position& position,
    const std::vector<SignalAggregator::RankedSignal>& ranked_signals,
    int current_time_minutes
) const {
    // EOD exit (15:58 PM ET = 388 minutes from open)
    if (current_time_minutes >= 388) {
        return Decision::EOD_EXIT;
    }

    // Profit target
    if (position.pnl_pct >= config_.profit_target_pct) {
        return Decision::PROFIT_TARGET;
    }

    // Stop loss
    if (position.pnl_pct <= -config_.stop_loss_pct) {
        return Decision::STOP_LOSS;
    }

    // 🔴 BUG: Exit if rank falls - no minimum hold time
    if (position.current_rank > config_.min_rank_to_hold) {
        return Decision::EXIT;  // ← Triggers after 5 minutes
    }

    // 🔴 BUG: Exit if strength falls - too aggressive
    if (position.current_strength < config_.min_strength_to_hold) {
        return Decision::EXIT;
    }

    // ❌ MISSING: No minimum hold time check
    // ❌ MISSING: No profitability consideration before exit
    // ❌ MISSING: No volatility adjustment

    return Decision::HOLD;
}
```

#### Position Update with Strength Decay
**Lines**: 54-86

```cpp
// Update current rank and strength
const auto* signal = find_signal(symbol, ranked_signals);
if (signal) {
    position.current_rank = signal->rank;
    position.current_strength = signal->strength;
} else {
    // 🔴 BUG: Decay happens even without signal
    if (current_bar_ > 200) {
        // 5% decay per bar is too aggressive
        position.current_strength *= 0.95;

        if (position.current_strength < config_.min_strength_to_hold) {
            position.current_rank = 9999;  // ← Guarantees exit
        }
    }
}
```

#### Impact Flow
```
Bar 21: Entry TQQQ (rank=1, strength=0.024)
  ↓
Bar 22: TQQQ rank → 2 (still holding)
  ↓
Bar 23: TQQQ rank → 3 (still holding)
  ↓
Bar 24: TQQQ rank → 4 (still holding, but threshold is 3)
  ↓
Bar 25: TQQQ rank → 4 > min_rank_to_hold (3)
  ↓
Bar 26: EXIT triggered → Crystallizes loss
```

**Fix Required**: Minimum hold time (30+ bars) and profit-aware exit logic

---

### Module 3: Trade Execution
**File**: `src/backend/rotation_trading_backend.cpp`
**Function**: `execute_decision()`
**Lines**: 391-490
**Bug Type**: No safeguards against depleted capital

#### Critical Code Section
```cpp
bool RotationTradingBackend::execute_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!config_.enable_trading) {
        return false;
    }

    // ... validation ...

    if (decision.decision == ENTER_LONG || decision.decision == ENTER_SHORT) {
        shares = calculate_position_size(decision);

        if (shares == 0) {
            return false;  // ← Only checks for 0, not minimum viable size
        }

        position_cost = shares * execution_price;

        if (position_cost > current_cash_) {
            return false;
        }

        // ❌ MISSING: No check for minimum trading capital
        // ❌ MISSING: No circuit breaker for drawdown
        // ❌ MISSING: No emergency stop

        current_cash_ -= position_cost;
    }

    // ... execution ...
}
```

**Fix Required**: Add circuit breakers and minimum capital checks

---

### Module 4: Backend Main Loop
**File**: `src/backend/rotation_trading_backend.cpp`
**Function**: `on_bar()`
**Lines**: 220-295
**Bug Type**: No risk checks in main loop

#### Critical Code Section
```cpp
bool RotationTradingBackend::on_bar() {
    if (!is_ready()) {
        return false;
    }

    session_stats_.bars_processed++;

    // ... signal generation ...

    // Step 5: Make position decisions
    auto decisions = make_decisions(ranked_signals);

    // Step 6: Execute decisions
    for (const auto& decision : decisions) {
        if (decision.decision != RotationPositionManager::Decision::HOLD) {
            execute_decision(decision);  // ← No risk checks before execution
        }
    }

    // ❌ MISSING: No check for current drawdown
    // ❌ MISSING: No check for remaining capital
    // ❌ MISSING: No emergency liquidation trigger

    // ... learning update ...

    return true;
}
```

**Fix Required**: Add risk management checks in main loop

---

### Module 5: Configuration
**File**: `config/rotation_strategy.json` (implied)
**Bug Type**: Wrong parameters for leveraged instruments

#### Current Parameters (Problematic)
```json
{
  "max_positions": 3,
  "min_strength_to_enter": 0.02,      // Too low for leveraged ETFs
  "min_strength_to_hold": 0.02,       // Too strict - causes early exit
  "min_rank_to_hold": 3,              // Too strict - rank changes fast
  "profit_target_pct": 0.05,          // 5% - rarely hit in 5 minutes
  "stop_loss_pct": 0.03,              // 3% - easily triggered
  "rotation_strength_delta": 0.05     // OK
}
```

#### Missing Parameters
```json
{
  // ❌ MISSING:
  "min_hold_bars": 30,                // Minimum 30 minutes hold
  "max_drawdown_pct": 0.20,           // Stop at 20% loss
  "min_trading_capital": 50000.0,     // Don't trade below $50k
  "min_position_size": 1000.0,        // Minimum $1k per position
  "max_position_size_pct": 0.25,      // Max 25% per position
  "circuit_breaker_enabled": true
}
```

**Fix Required**: Update all parameters for leveraged instruments

---

### Module 6: Backend Configuration
**File**: `src/backend/rotation_trading_backend.cpp`
**Struct**: `RotationTradingBackend::Config`
**Lines**: 50-82
**Bug Type**: Missing risk management parameters

#### Current Structure
```cpp
struct Config {
    std::vector<std::string> symbols = {
        "TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"
    };

    OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
    SignalAggregator::Config aggregator_config;
    RotationPositionManager::Config rotation_config;
    data::MultiSymbolDataManager::Config data_config;

    double starting_capital = 100000.0;
    bool enable_trading = true;
    bool log_all_signals = true;
    bool log_all_decisions = true;

    // ❌ MISSING FIELDS:
    // double max_drawdown_pct = 0.20;
    // double min_trading_capital = 50000.0;
    // double min_position_size = 1000.0;
    // double max_position_size_pct = 0.25;
    // bool circuit_breaker_enabled = true;
};
```

**Fix Required**: Add risk management configuration fields

---

### Module 7: Position Manager Configuration
**File**: `include/strategy/rotation_position_manager.h`
**Struct**: `RotationPositionManager::Config`
**Lines**: 50-82 (estimated)
**Bug Type**: Missing hold time parameters

#### Current Structure (Partial)
```cpp
struct Config {
    int max_positions = 3;
    double min_strength_to_enter = 0.02;
    double min_strength_to_hold = 0.02;
    int min_rank_to_hold = 3;
    double profit_target_pct = 0.05;
    double stop_loss_pct = 0.03;
    double rotation_strength_delta = 0.05;
    int rotation_cooldown_bars = 10;

    // ❌ MISSING:
    // int min_hold_bars = 30;
    // bool exit_if_profitable = false;  // Don't exit profitable positions
    // double strength_decay_rate = 0.99;  // Slower decay
};
```

**Fix Required**: Add minimum hold time and profit protection

---

## Bug Interaction Flow

### Timeline: Bar-by-Bar Execution

```
┌──────────────────────────────────────────────────────────────┐
│ Bar 1-20: Warmup - No trading                                │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bar 21: First Position Entry                                 │
│                                                               │
│  on_bar()                                                     │
│    ↓                                                          │
│  make_decisions()  → finds TQQQ signal                       │
│    ↓                                                          │
│  calculate_position_size()                                   │
│    → fixed_alloc = $100k × 0.95 / 3 = $31,667   🔴 BUG #1   │
│    → allocation = min($31,667, $100k × 0.95) = $31,667       │
│    → shares = 311                                            │
│    ↓                                                          │
│  execute_decision(ENTRY)                                     │
│    → Pre-deduct: $100k → $68,393                            │
│    → Execute: TQQQ entry                                     │
│    ↓                                                          │
│  Result: Position 1 open, $68k cash remaining               │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bars 22-23: Fill Remaining Positions                         │
│                                                               │
│  Same flow for UVXY ($31,657) and SPXL ($31,616)            │
│    ↓                                                          │
│  Result: 3 positions open, $5,119 cash remaining (95% used) │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bars 24-25: Position Monitoring                              │
│                                                               │
│  on_bar()                                                     │
│    ↓                                                          │
│  make_decisions()                                            │
│    → Update position prices                                  │
│    → TQQQ: rank changes 1 → 2 → 3 → 4                       │
│    → check_exit_conditions(TQQQ)                             │
│       → rank (4) > min_rank_to_hold (3)  🔴 BUG #2          │
│       → Returns: EXIT                                        │
│    ↓                                                          │
│  Result: EXIT decision for TQQQ after 5 bars                │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bar 26: Mass Exit Event                                      │
│                                                               │
│  execute_decision(EXIT TQQQ)                                 │
│    → Close 311 shares @ $101.91                              │
│    → P&L: -0.27% (-$87.09)                                   │
│    → Return cash: $1,630.56                                  │
│    ↓                                                          │
│  execute_decision(EXIT UVXY)                                 │
│    → Close 2,995 shares @ $10.55                             │
│    → P&L: -0.19% (-$59.90)                                   │
│    → Return cash: $2,226.05                                  │
│    ↓                                                          │
│  execute_decision(EXIT SPXL)                                 │
│    → Close 151 shares @ $210.06                              │
│    → P&L: +0.68% (+$214.68)                                  │
│    → Return cash: $2,940.84                                  │
│    ↓                                                          │
│  Result: All positions closed                                │
│          Cash: $5,119 + $6,797 = $11,916                     │
│          Loss: $88,084 (88%)  ← CATASTROPHIC                │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bars 27-31: Continued Trading with Depleted Capital         │
│                                                               │
│  on_bar() - No circuit breaker! Continues trading  🔴 BUG #3│
│    ↓                                                          │
│  make_decisions() → finds SVXY signal                        │
│    ↓                                                          │
│  calculate_position_size()                                   │
│    → fixed_alloc = $31,667 (unchanged!)                      │
│    → allocation = min($31,667, $11,916 × 0.95) = $11,320    │
│    → Opens SVXY for $11,310                                  │
│    ↓                                                          │
│  Result: $606 remaining (99.4% deployed again)              │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bar 32: Microscopic Position                                 │
│                                                               │
│  calculate_position_size()                                   │
│    → allocation = min($31,667, $606 × 0.95) = $575          │
│    → Opens SDS for $567                                      │
│    ↓                                                          │
│  Result: $39 remaining (99.96% total deployment)            │
└──────────────────────────────────────────────────────────────┘
                           ↓
┌──────────────────────────────────────────────────────────────┐
│ Bars 36-37: Final Exits                                      │
│                                                               │
│  Exit SVXY and SDS after 5 bars                              │
│    ↓                                                          │
│  Result: Final equity = $39.06                               │
│          Total loss: $99,960.94 (99.96%)  ← TOTAL FAILURE   │
└──────────────────────────────────────────────────────────────┘
```

---

## Module Interaction Matrix

| Module | Calls | Called By | Bug Type | Severity |
|--------|-------|-----------|----------|----------|
| `rotation_trade_command.cpp` | Backend | CLI | N/A | - |
| `rotation_trading_backend.cpp::on_bar()` | make_decisions, execute_decision | Command | Missing risk checks | P0 |
| `rotation_trading_backend.cpp::calculate_position_size()` | get_execution_price | execute_decision | Over-allocation | P0 |
| `rotation_trading_backend.cpp::execute_decision()` | calculate_position_size, rotation_manager | on_bar | No safeguards | P0 |
| `rotation_position_manager.cpp::make_decisions()` | check_exit_conditions | Backend | Premature exit | P0 |
| `rotation_position_manager.cpp::check_exit_conditions()` | - | make_decisions | No min hold time | P0 |
| `rotation_strategy.json` | - | Backend | Wrong params | P1 |

---

## Fix Priority Matrix

| Priority | Module | Function | Lines | Fix Type | Estimated Effort |
|----------|--------|----------|-------|----------|------------------|
| P0 | `rotation_trading_backend.cpp` | `calculate_position_size()` | 519-520 | Change allocation formula | 2 hours |
| P0 | `rotation_trading_backend.cpp` | `execute_decision()` | 391-490 | Add circuit breakers | 4 hours |
| P0 | `rotation_position_manager.cpp` | `check_exit_conditions()` | 537-601 | Add min hold time | 2 hours |
| P1 | `rotation_trading_backend.cpp` | `on_bar()` | 220-295 | Add risk checks | 3 hours |
| P1 | `rotation_strategy.json` | All | All | Update parameters | 1 hour |
| P2 | `rotation_position_manager.h` | `Config` | 50-82 | Add fields | 1 hour |
| P2 | `rotation_trading_backend.h` | `Config` | 50-82 | Add fields | 1 hour |

**Total Estimated Effort**: ~14 hours

---

## Verification Checklist

After implementing fixes, verify:

- [ ] Daily loss < 20% across all test days
- [ ] MRD > 0.1% on at least 5/10 days
- [ ] Positions held minimum 30 minutes
- [ ] Circuit breaker triggers at 20% drawdown
- [ ] No positions opened when equity < $50k
- [ ] Minimum position size = $1,000
- [ ] Maximum position size = 25% of equity
- [ ] Exit logic respects profitability

---

## References

- Main bug report: `megadocs/BUG_CATASTROPHIC_CAPITAL_DEPLETION.md`
- Test results: `OCTOBER_MOCK_RESULTS_SUMMARY.md`
- Original position closing bug: `megadocs/BUG_POSITION_SIZING_RACE_CONDITION.md`
