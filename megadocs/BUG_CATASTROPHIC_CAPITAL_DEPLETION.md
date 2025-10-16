# CRITICAL BUG REPORT: Catastrophic Capital Depletion
## 99.9% Daily Losses in Rotation Trading System

**Status**: 🔴 CRITICAL
**Severity**: P0 - System Unusable
**Discovered**: 2025-10-15
**Impact**: Complete capital loss within minutes of trading

---

## Executive Summary

The rotation trading system is experiencing **catastrophic capital depletion**, losing 99.9% of capital ($100,000 → $12-67) within the first 30-40 minutes of every trading day. This affects 100% of tested trading days (Oct 1-14, 2025).

### Critical Metrics
- **Average Daily Loss**: -99.96%
- **Average Final Equity**: $34.03 (from $100,000 start)
- **Time to Depletion**: 30-40 minutes (~30-40 bars)
- **Affected Days**: 10/10 (100%)
- **MRD**: 0.00% (zero profitability)

---

## Bug Description

### Observable Symptoms

1. **Rapid Initial Capital Deployment**
   - First 3 positions consume 95% of capital (~$95,000)
   - Positions sized at $31,607, $31,657, $31,616

2. **Immediate Losses on Exit**
   - Positions close after 5 bars (5 minutes)
   - Small losses compound: -0.27%, -0.19%, -0.68% per position

3. **Continued Trading with Depleted Capital**
   - After first 3 exits, capital drops to $5k-$12k
   - System continues opening positions with remaining scraps
   - Positions 4-5: $4,863, $203 (microscopic)

4. **Zero Recovery Capability**
   - Once capital depleted, system cannot recover
   - Tiny positions (< $1k) have no meaningful impact
   - Final equity: $12-67 remaining

### Evidence from Oct 1, 2025 Test

**Timeline of Capital Destruction:**

```
Bar 21 (09:30:00): Entry TQQQ @ $101.63, shares=311, value=$31,607
                   Cash remaining: $68,393

Bar 22 (09:31:00): Entry UVXY @ $10.57, shares=2,995, value=$31,657
                   Cash remaining: $36,736

Bar 23 (09:32:00): Entry SPXL @ $209.38, shares=151, value=$31,616
                   Cash remaining: $5,119  ← 95% DEPLOYED

Bar 26 (09:35:00): Exit TQQQ @ $101.91, P&L=-0.27%, value=$1,631
                   Exit UVXY @ $10.55, P&L=-0.19%, value=$2,226
                   Exit SPXL @ $210.06, P&L=+0.68%, value=$2,941
                   Cash after exits: $11,917  ← 88% LOSS IN 5 MINUTES

Bar 31 (09:40:00): Entry SVXY @ $50.72, shares=223, value=$11,311
                   Cash remaining: $606  ← 99.4% DEPLOYED

Bar 32 (09:41:00): Entry SDS @ $14.54, shares=39, value=$567
                   Cash remaining: $39  ← 99.96% DEPLOYED

Bar 36-37:         Exit SVXY, Exit SDS
                   Final equity: $39.06  ← 99.96% TOTAL LOSS
```

---

## Root Cause Analysis

### Primary Causes

#### 1. **Catastrophic Over-Allocation (95% Rule)**
**Location**: `src/backend/rotation_trading_backend.cpp:519-522`

```cpp
// FIX 2: Use FIXED allocation per position to prevent acceleration effect
int max_positions = config_.rotation_config.max_positions;
double fixed_allocation = (config_.starting_capital * 0.95) / max_positions;
```

**Issue**:
- Allocates 95% of STARTING capital, not CURRENT capital
- With 3 max positions: $100k × 0.95 / 3 = $31,667 per position
- Deploys $95k in first 3 positions, leaving only $5k buffer
- No reserve for losses or new opportunities

**Impact**: System commits 95% of capital with zero safety margin

---

#### 2. **Premature Exit Strategy (5-Bar Hold)**
**Location**: `src/strategy/rotation_position_manager.cpp:88-119`

```cpp
// Check exit conditions
Decision decision = check_exit_conditions(position, ranked_signals, current_time_minutes);
```

**Issue**: Positions exit after just 5 bars (5 minutes) due to:
- Rank falling below threshold
- Strength decay during warmup period
- No consideration of P&L or market conditions

**Evidence**:
```
Bar 21: Entry TQQQ (rank=1, strength=0.024)
Bar 22: Hold TQQQ (rank=2, strength=0.024)
Bar 23: Hold TQQQ (rank=3, strength=0.022)
Bar 24: Hold TQQQ (rank=2, strength=0.021)
Bar 25: Hold TQQQ (rank=4, strength=0.032)
Bar 26: EXIT TQQQ (rank fell below threshold)
```

**Impact**: System never allows positions to develop profit potential

---

#### 3. **Capital Waterfall Effect**
**Location**: `src/backend/rotation_trading_backend.cpp:516-578`

```cpp
// But still check against available cash
double available_cash = current_cash_;
double allocation = std::min(fixed_allocation, available_cash * 0.95);
```

**Issue**: After initial losses:
- Position 1-3 exits leave ~$12k (88% loss)
- Position 4 uses: min($31,667, $12k × 0.95) = $11,400
- Position 5 uses: min($31,667, $600 × 0.95) = $570
- System keeps trading with microscopic amounts

**Impact**: Compounds losses by continuing to trade with insufficient capital

---

#### 4. **No Risk Management**
**Location**: Multiple modules

**Missing Safeguards**:
- ❌ No maximum drawdown limit
- ❌ No minimum capital threshold to stop trading
- ❌ No profit target before exit
- ❌ No adaptive position sizing based on losses
- ❌ No circuit breaker for rapid capital depletion

---

#### 5. **Wrong Parameters for Leveraged Instruments**
**Location**: Strategy configuration

**Issue**: Trading 3x leveraged ETFs (TQQQ, SPXL) and volatility products (UVXY, SVXY) with:
- Micro-holding periods (5 minutes)
- No volatility adjustment
- Fixed exit thresholds designed for regular equities

**Impact**: Leveraged instruments require longer hold times and different thresholds

---

## Source Modules Involved

### 1. Position Sizing Module
**Path**: `src/backend/rotation_trading_backend.cpp`

**Lines 516-578**: `calculate_position_size()`

**Critical Code**:
```cpp
int RotationTradingBackend::calculate_position_size(
    const RotationPositionManager::PositionDecision& decision
) {
    // FIX 2: Use FIXED allocation per position to prevent acceleration effect
    // This ensures consistent position sizing regardless of when positions are opened
    int max_positions = config_.rotation_config.max_positions;
    double fixed_allocation = (config_.starting_capital * 0.95) / max_positions;

    // But still check against available cash
    double available_cash = current_cash_;
    double allocation = std::min(fixed_allocation, available_cash * 0.95);

    if (allocation <= 100.0) {
        utils::log_warning("Insufficient cash for position: $" +
                          std::to_string(allocation));
        return 0;
    }

    double price = get_execution_price(decision.symbol, "BUY");
    if (price <= 0) {
        utils::log_error("Invalid price for position sizing: " +
                        std::to_string(price));
        return 0;
    }

    int shares = static_cast<int>(allocation / price);

    // Final validation - ensure position doesn't exceed available cash
    double position_value = shares * price;
    if (position_value > available_cash) {
        shares = static_cast<int>(available_cash / price);
    }

    return shares;
}
```

**Issues**:
1. Line 519-520: Uses `starting_capital * 0.95` instead of adaptive sizing
2. Line 524: Takes 95% of available_cash AGAIN, creating double over-allocation
3. No check for minimum viable position size
4. No consideration of current drawdown

---

### 2. Exit Condition Module
**Path**: `src/strategy/rotation_position_manager.cpp`

**Lines 88-119**: Exit decision logic in `make_decisions()`

**Critical Code**:
```cpp
// Check exit conditions
Decision decision = check_exit_conditions(position, ranked_signals, current_time_minutes);

if (decision != Decision::HOLD) {
    PositionDecision pd;
    pd.symbol = symbol;
    pd.decision = decision;
    pd.position = position;

    switch (decision) {
        case Decision::EXIT:
            pd.reason = "Rank fell below threshold (" +
                       std::to_string(position.current_rank) + ")";
            stats_.exits++;
            break;
        // ... other cases ...
    }

    decisions.push_back(pd);
    symbols_to_exit.insert(symbol);
}
```

**Lines 537-601**: `check_exit_conditions()` implementation

**Critical Code**:
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

    // Exit if rank falls below threshold
    if (position.current_rank > config_.min_rank_to_hold) {
        return Decision::EXIT;
    }

    // Exit if strength falls below threshold
    if (position.current_strength < config_.min_strength_to_hold) {
        return Decision::EXIT;
    }

    return Decision::HOLD;
}
```

**Issues**:
1. Rank-based exit too aggressive for volatile instruments
2. No minimum hold time to allow position development
3. No consideration of unrealized P&L when deciding to exit
4. Strength decay in warmup period (lines 69-84) causes premature exits

---

### 3. Configuration Module
**Path**: `config/rotation_strategy.json` (implied)

**Critical Parameters**:
```json
{
  "max_positions": 3,
  "min_strength_to_enter": 0.02,
  "min_strength_to_hold": 0.02,
  "min_rank_to_hold": 3,
  "profit_target_pct": 0.05,
  "stop_loss_pct": 0.03,
  "rotation_strength_delta": 0.05
}
```

**Issues**:
1. `min_rank_to_hold: 3` too strict (causes exit when rank > 3)
2. `profit_target_pct: 5%` rarely reached in 5 minutes
3. `stop_loss_pct: 3%` easily triggered in volatile instruments
4. No min_hold_bars parameter

---

### 4. Trade Execution Module
**Path**: `src/backend/rotation_trading_backend.cpp`

**Lines 391-490**: `execute_decision()`

**Critical Code**:
```cpp
bool RotationTradingBackend::execute_decision(
    const RotationPositionManager::PositionDecision& decision
) {
    if (!config_.enable_trading) {
        return false;
    }

    // ... validation ...

    // Calculate position size
    int shares = 0;
    double position_cost = 0.0;

    if (decision.decision == ENTER_LONG || decision.decision == ENTER_SHORT) {
        shares = calculate_position_size(decision);

        if (shares == 0) {
            utils::log_warning("Position size is 0 for " + decision.symbol + " - skipping");
            return false;
        }

        // CRITICAL FIX: Validate we have sufficient cash BEFORE proceeding
        position_cost = shares * execution_price;

        if (position_cost > current_cash_) {
            utils::log_error("INSUFFICIENT FUNDS: Need $" + std::to_string(position_cost) +
                           " but only have $" + std::to_string(current_cash_));
            return false;
        }

        // PRE-DEDUCT cash to prevent over-allocation race condition
        current_cash_ -= position_cost;
        // ...
    }

    // ... execution ...
}
```

**Issues**:
1. No check for minimum viable position size (e.g., $1000 minimum)
2. Allows trading with < $100 remaining
3. No emergency stop when capital drops below threshold

---

### 5. Backend Configuration Module
**Path**: `src/backend/rotation_trading_backend.cpp`

**Lines 25-80**: `RotationTradingBackend::Config` structure

```cpp
struct Config {
    // Symbols to trade
    std::vector<std::string> symbols = {
        "TQQQ", "SQQQ", "UPRO", "SDS", "UVXY", "SVIX"
    };

    // Component configurations
    OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;
    SignalAggregator::Config aggregator_config;
    RotationPositionManager::Config rotation_config;
    data::MultiSymbolDataManager::Config data_config;

    // Trading parameters
    double starting_capital = 100000.0;
    bool enable_trading = true;
    // ...
};
```

**Issues**:
1. No `max_drawdown_pct` parameter
2. No `min_trading_capital` parameter
3. No `emergency_stop_enabled` flag

---

### 6. Position Manager State Module
**Path**: `src/strategy/rotation_position_manager.cpp`

**Lines 37-86**: Position update and strength decay logic

**Critical Code**:
```cpp
// Don't immediately mark for exit - keep previous rank/strength
// During cold-start (first 200 bars), don't decay - allow predictor to stabilize
if (current_bar_ > 200) {
    utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                   " applying decay (post-warmup)");
    // Only decay strength gradually to allow time for signal to return
    position.current_strength *= 0.95;  // 5% decay per bar

    // Only mark for exit if strength decays below hold threshold
    if (position.current_strength < config_.min_strength_to_hold) {
        position.current_rank = 9999;
        utils::log_debug("[BAR " + std::to_string(current_bar_) + "] " + symbol +
                       " strength fell below hold threshold -> marking for exit");
    }
}
```

**Issues**:
1. Even in warmup (< 200 bars), positions can exit due to rank changes
2. 5% strength decay per bar is too aggressive
3. Marking rank=9999 guarantees immediate exit

---

## Execution Flow Leading to Bug

### Phase 1: Initial Deployment (Bars 21-23, ~3 minutes)
```
1. Backend::on_bar() called
   ↓
2. Backend::make_decisions()
   ↓
3. RotationPositionManager::make_decisions()
   → Current positions: 0, Available slots: 3
   → Finds 3 strong signals
   ↓
4. Backend::execute_decision() for each position
   ↓
5. Backend::calculate_position_size()
   → fixed_allocation = $100k * 0.95 / 3 = $31,667
   → allocation = min($31,667, $68k * 0.95) = $31,667
   → Returns: 311 shares @ $101.63 = $31,607
   ↓
6. Pre-deduct cash: $100k → $68,393
   ↓
7. Repeat for positions 2-3
   → Final cash: $5,119 (95% deployed)
```

### Phase 2: Premature Exits (Bars 24-26, ~3 minutes later)
```
1. Backend::on_bar() called
   ↓
2. RotationPositionManager::update prices and P&L
   → TQQQ: rank changes from 1 → 2 → 3 → 4
   → Strength drops from 0.024 → 0.021
   ↓
3. RotationPositionManager::check_exit_conditions()
   → Position rank (4) > min_rank_to_hold (3)
   → Returns: Decision::EXIT
   ↓
4. Backend::execute_decision(EXIT)
   → Closes position, returns cash
   → TQQQ exit: returns $1,631 (loss: -$29,976)
   ↓
5. Repeat for UVXY and SPXL
   → Total cash after all exits: $11,917
   → Total loss: $88,083 (88%)
```

### Phase 3: Continued Trading with Depleted Capital (Bars 27-37)
```
1. Backend::make_decisions()
   ↓
2. RotationPositionManager::make_decisions()
   → Current positions: 0, Available slots: 3
   → Finds new signals
   ↓
3. Backend::calculate_position_size()
   → fixed_allocation = $31,667 (unchanged!)
   → allocation = min($31,667, $11,917 * 0.95) = $11,321
   → Opens position 4: $11,311
   ↓
4. Cash drops to $606
   ↓
5. Opens position 5: min($31,667, $606 * 0.95) = $575
   ↓
6. Cash drops to $31
   ↓
7. Positions 4-5 exit after 5 bars
   ↓
8. Final equity: $39
```

---

## Impact Assessment

### Severity: P0 - Critical
- **Production Risk**: System would lose all capital in live trading
- **Business Impact**: Complete destruction of trading capital
- **Recovery Time**: Immediate - system cannot recover once capital depleted
- **Blast Radius**: Affects 100% of trading sessions

### Affected Components
1. ✅ Position sizing logic
2. ✅ Exit condition logic
3. ✅ Risk management (missing)
4. ✅ Capital management (missing)
5. ✅ Configuration parameters

---

## Reproduction Steps

1. Build system: `cmake --build build --target sentio_cli`
2. Run mock test: `build/sentio_cli mock --mode mock --date 2025-10-01`
3. Observe:
   - Capital depletes from $100k → $39 in 30 minutes
   - 95% deployed in first 3 positions
   - Positions exit after 5 bars
   - System continues trading with < $100

---

## Recommended Fixes

### Critical (Must Fix)

#### Fix 1: Adaptive Position Sizing
**Location**: `src/backend/rotation_trading_backend.cpp:519-522`

```cpp
// BEFORE (WRONG):
double fixed_allocation = (config_.starting_capital * 0.95) / max_positions;

// AFTER (CORRECT):
double current_equity = current_cash_ + allocated_capital_;
double max_risk_per_position = 0.25;  // 25% max per position
double fixed_allocation = (current_equity * max_risk_per_position);
```

#### Fix 2: Minimum Hold Time
**Location**: `src/strategy/rotation_position_manager.cpp:537-601`

```cpp
// Add to check_exit_conditions():
// Minimum hold time: 30 bars (30 minutes)
if (position.bars_held < config_.min_hold_bars) {
    return Decision::HOLD;
}
```

#### Fix 3: Circuit Breaker
**Location**: `src/backend/rotation_trading_backend.cpp:391-490`

```cpp
// Add to execute_decision():
double current_equity = current_cash_ + allocated_capital_;
double drawdown_pct = (config_.starting_capital - current_equity) / config_.starting_capital;

if (drawdown_pct > config_.max_drawdown_pct) {
    utils::log_error("CIRCUIT BREAKER: Max drawdown reached (" +
                    std::to_string(drawdown_pct * 100) + "%)");
    liquidate_all_positions("Emergency stop - max drawdown");
    return false;
}
```

#### Fix 4: Minimum Trading Capital
**Location**: `src/backend/rotation_trading_backend.cpp:516-578`

```cpp
// Add to calculate_position_size():
if (current_cash_ < config_.min_trading_capital) {
    utils::log_warning("Below minimum trading capital - stopping new positions");
    return 0;
}

if (allocation < config_.min_position_size) {
    utils::log_warning("Position size below minimum - skipping");
    return 0;
}
```

### Important (Should Fix)

1. **Adjust configuration parameters** for leveraged instruments
2. **Implement profit-aware exit logic** (don't exit if position is profitable)
3. **Add volatility-adjusted position sizing**
4. **Implement Kelly criterion** for optimal position sizing

---

## Testing Plan

### Unit Tests Required
1. Position sizing with 50% capital loss
2. Exit conditions with profitable positions
3. Circuit breaker activation
4. Minimum capital enforcement

### Integration Tests Required
1. Full day simulation with new fixes
2. Multi-day simulation
3. Stress test with extreme volatility

### Acceptance Criteria
- [ ] Daily loss limited to < 20%
- [ ] MRD > 0.1%
- [ ] Positions held minimum 30 minutes
- [ ] Circuit breaker triggers at 30% drawdown
- [ ] No trading when equity < $50k

---

## Related Bugs
- [FIXED] #001: Position closing bug (0 positions closed)
- [OPEN] #002: This catastrophic capital depletion bug

---

## Appendix: Full Test Results

See `OCTOBER_MOCK_RESULTS_SUMMARY.md` for complete data across all 10 trading days.
