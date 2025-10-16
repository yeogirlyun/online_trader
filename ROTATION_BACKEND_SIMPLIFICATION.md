# Backend Simplification for Rotation System

## TL;DR: We Can Drastically Simplify

**Current Backend:**
- ✅ Position State Machine (PSM) - 7 states, complex transitions
- ✅ Kelly Criterion allocation
- ✅ Adaptive Trading Mechanism (Q-learning/MAB)
- ✅ Dynamic Allocation Manager
- ✅ Enhanced PSM with dynamic hysteresis

**Rotation Backend (Minimal):**
- ✅ **RotationPositionManager** - Track top N positions, enforce min hold
- ⚠️ **Equal-weight allocation** (start simple)
- ❌ **No PSM** - States are implicit (cash vs holding N symbols)
- ❌ **No Q-learning/MAB** - Use Optuna for static optimization instead
- ✅ **Basic risk checks** - Capital, max position size

---

## Decision Matrix

| Component | Current System | Rotation System | Keep? | Reason |
|-----------|---------------|-----------------|-------|--------|
| **Position State Machine** | 7-state PSM with complex transitions | Simple position list (0-N holdings) | ❌ **NO** | States are implicit in rotation logic |
| **Kelly Allocation** | Kelly criterion per position | Equal-weight or signal-strength-weight | ⚠️ **MAYBE** | Start equal-weight, add Kelly later |
| **Adaptive Thresholds (Q-learning)** | Real-time threshold learning | Static thresholds (Optuna-optimized) | ❌ **NO** | Optuna does offline optimization better |
| **Dynamic Allocation Manager** | State-aware allocation scaling | Fixed 1/N or signal-weight | ❌ **NO** | Over-engineering for rotation |
| **Risk Management** | Stop-loss, profit-taking, drawdown limits | Min hold, max positions, capital checks | ✅ **YES (simplified)** | Core risk controls needed |
| **Order Generator** | Converts PSM transitions to orders | Converts rotation decisions to orders | ✅ **YES (redesigned)** | Essential for execution |

---

## Deep Dive: Do We Need PSM?

### Current PSM Architecture

```cpp
enum class State {
    CASH_ONLY,      // 0% invested
    QQQ_ONLY,       // 100% QQQ (1x long)
    TQQQ_ONLY,      // 100% TQQQ (3x long)
    PSQ_ONLY,       // 100% PSQ (1x short)
    SQQQ_ONLY,      // 100% SQQQ (3x short)
    QQQ_TQQQ,       // 50% QQQ + 50% TQQQ (dual long)
    PSQ_SQQQ        // 50% PSQ + 50% SQQQ (dual short)
};

// 7 states × 5 signal types = 35 possible transitions
StateTransition get_optimal_transition(
    const PortfolioState& portfolio,
    const SignalOutput& signal,
    const MarketState& market,
    double confidence_threshold
);
```

**Complexity:**
- 7 explicit states
- 35 transition rules (state × signal → target state)
- State-aware thresholds (buy/sell thresholds vary by state)
- Dual-state logic (QQQ_TQQQ, PSQ_SQQQ)
- Coupled long/short management

**Problems:**
1. **Over-engineering** - Managing 4 symbols doesn't need 7 states
2. **Rigid** - Hard to add new symbols (would need more states)
3. **Opaque** - Hard to reason about why transitions happen
4. **Overfitting risk** - Many transition rules → many places to overfit

### Rotation System "States"

In the rotation system, **states are implicit**:

```cpp
// State is just: "Which symbols am I holding?"
struct RotationState {
    std::vector<ManagedPosition> holdings;  // 0-N positions
    double cash;

    // That's it! No explicit state enum needed.
};

// Example states:
// State 1: holdings = [] (100% cash)
// State 2: holdings = [TQQQ] (100% TQQQ)
// State 3: holdings = [TQQQ, UVXY] (50% TQQQ, 50% UVXY)
// State 4: holdings = [SVIX, TQQQ, UPRO] (33% each)
```

**Simplicity:**
- **No state enum** - State is defined by current holdings
- **No transition matrix** - Just rotation logic: "hold, rotate, or add"
- **Unlimited flexibility** - Can hold any combination of 0-N symbols
- **Transparent** - Easy to understand: "I'm holding these 3 symbols because they're the strongest"

### Rotation Logic (Replaces PSM)

```cpp
class RotationPositionManager {
public:
    struct Config {
        int max_positions = 3;      // Max simultaneous positions
        int min_hold_bars = 2;      // Min bars before rotation
        double buy_threshold = 0.55; // Min probability to enter
    };

    std::vector<TradeOrder> update(
        const std::vector<RankedSignal>& ranked_signals,  // All 12 signals, sorted by strength
        uint64_t current_bar_id,
        double available_capital
    ) {
        std::vector<TradeOrder> orders;

        // Step 1: Check each current position - should we hold or rotate?
        for (auto& pos : holdings_) {
            if (should_rotate(pos, ranked_signals, current_bar_id)) {
                // Liquidate this position
                orders.push_back(create_sell_order(pos));
                mark_for_removal(pos);
            }
        }

        // Step 2: Remove liquidated positions
        remove_marked_positions();

        // Step 3: Fill up to max_positions with top-ranked signals
        int positions_to_add = max_positions_ - holdings_.size();
        for (int i = 0; i < positions_to_add && i < ranked_signals.size(); ++i) {
            const auto& signal = ranked_signals[i];

            // Only buy if signal is strong enough
            if (signal.signal.probability >= buy_threshold_) {
                orders.push_back(create_buy_order(signal, available_capital));
                add_position(signal, current_bar_id);
            }
        }

        return orders;
    }

private:
    bool should_rotate(const ManagedPosition& pos,
                      const std::vector<RankedSignal>& new_signals,
                      uint64_t current_bar_id) {
        // Rule 1: Enforce minimum hold
        int bars_held = current_bar_id - pos.entry_bar_id;
        if (bars_held < min_hold_bars_) {
            return false;  // Must hold - too early to exit
        }

        // Rule 2: Check if position is still in top N
        for (int i = 0; i < max_positions_ && i < new_signals.size(); ++i) {
            if (new_signals[i].symbol == pos.symbol) {
                return false;  // Still in top N - keep holding
            }
        }

        // Rule 3: Signal is now weak (below buy threshold)
        auto it = std::find_if(new_signals.begin(), new_signals.end(),
                              [&](const auto& s) { return s.symbol == pos.symbol; });
        if (it != new_signals.end() && it->signal.probability < buy_threshold_) {
            return true;  // Signal too weak - rotate out
        }

        // Not in top N → rotate
        return true;
    }

    std::vector<ManagedPosition> holdings_;
    int max_positions_;
    int min_hold_bars_;
    double buy_threshold_;
};
```

**No state machine needed!** The logic is simple:
1. "Should I keep holding?" → Check min_hold + rank + signal strength
2. "What should I buy?" → Top N strongest signals
3. "How much?" → Equal weight (or signal-weighted)

**Verdict: ❌ PSM Not Needed**

---

## Deep Dive: Do We Need Kelly Allocation?

### Kelly Criterion Basics

Kelly formula for position sizing:
```
f* = (p × b - q) / b

where:
  p = probability of win
  b = win/loss ratio (avg_win / avg_loss)
  q = 1 - p = probability of loss
  f* = optimal fraction of capital to bet
```

**Example:**
- Signal probability: p = 0.60
- Avg win/loss ratio: b = 1.5 (win $150 when right, lose $100 when wrong)
- Kelly fraction: f* = (0.60 × 1.5 - 0.40) / 1.5 = (0.90 - 0.40) / 1.5 = 0.33

→ **Allocate 33% of capital to this position**

### Current System: Kelly Per Position

```cpp
double BackendComponent::calculate_kelly_position_size(
    double signal_probability,
    double expected_return,
    double risk_estimate,
    double available_capital
) {
    double edge = 2.0 * signal_probability - 1.0;  // Convert prob to edge
    double kelly_fraction = edge / risk_estimate;

    // Apply fractional Kelly (e.g., 0.25 of full Kelly for safety)
    kelly_fraction *= 0.25;

    // Cap at max position size (e.g., 50%)
    kelly_fraction = std::min(kelly_fraction, 0.50);

    return available_capital * kelly_fraction;
}
```

**Use case:** Allocate more to high-probability signals, less to low-probability.

### Rotation System: Do We Need Kelly?

**Option 1: Equal Weight (Simplest)**
```cpp
double position_size = available_capital / max_positions;

// Example: $100K capital, max 3 positions
// → $33,333 per position
```

**Pros:**
- ✅ Simple, transparent
- ✅ No risk of over-concentration
- ✅ Well-studied (equal-weight portfolios often outperform)
- ✅ Reduces parameter overfitting

**Cons:**
- ❌ Doesn't differentiate between strong vs weak signals
- ❌ May under-allocate to high-conviction trades

**Option 2: Signal Strength Weighting**
```cpp
// Allocate proportional to signal strength
double total_strength = 0.0;
for (const auto& signal : top_N_signals) {
    total_strength += signal.strength;  // strength = probability × confidence
}

for (const auto& signal : top_N_signals) {
    double weight = signal.strength / total_strength;
    double position_size = available_capital * weight;
    // Example: strengths = [0.50, 0.30, 0.20]
    // → allocations = [50%, 30%, 20%]
}
```

**Pros:**
- ✅ Still simple
- ✅ Differentiates signal strength
- ✅ Natural weighting (stronger signals get more capital)

**Cons:**
- ⚠️ Can concentrate in one position (e.g., 80% to strongest)
- ⚠️ Requires min/max caps for safety

**Option 3: Kelly Criterion**
```cpp
// Calculate Kelly for each signal
std::vector<double> kelly_fractions;
for (const auto& signal : top_N_signals) {
    double p = signal.signal.probability;
    double edge = 2.0 * p - 1.0;
    double kelly = edge / signal.signal.volatility;  // Risk-adjusted
    kelly *= 0.25;  // Fractional Kelly for safety
    kelly_fractions.push_back(std::max(0.0, kelly));
}

// Normalize to 100%
double total_kelly = std::accumulate(kelly_fractions.begin(), kelly_fractions.end(), 0.0);
for (size_t i = 0; i < kelly_fractions.size(); ++i) {
    double weight = kelly_fractions[i] / total_kelly;
    double position_size = available_capital * weight;
}
```

**Pros:**
- ✅ Theoretically optimal (maximizes log wealth)
- ✅ Risk-adjusted (accounts for volatility)
- ✅ Over-betting protection (fractional Kelly)

**Cons:**
- ❌ Complex
- ❌ Requires accurate probability + volatility estimates (hard!)
- ❌ Sensitive to estimation errors (small error in p → big change in f*)
- ❌ Can be unstable with noisy signals

### Recommendation: **Start Equal-Weight, Add Kelly Later**

**Phase 1: Equal Weight**
```cpp
struct RotationConfig {
    enum class SizingMethod { EQUAL_WEIGHT };
    SizingMethod sizing = EQUAL_WEIGHT;
};

double position_size = available_capital / max_positions;
```

**Phase 2: Signal Strength Weighting (if equal-weight underperforms)**
```cpp
enum class SizingMethod {
    EQUAL_WEIGHT,
    SIGNAL_STRENGTH
};

if (sizing == SIGNAL_STRENGTH) {
    weight = signal.strength / total_strength;
} else {
    weight = 1.0 / max_positions;
}
```

**Phase 3: Kelly (if we have accurate probability estimates)**
```cpp
enum class SizingMethod {
    EQUAL_WEIGHT,
    SIGNAL_STRENGTH,
    KELLY_CRITERION
};

if (sizing == KELLY_CRITERION) {
    kelly_fraction = calculate_kelly(signal);
    weight = kelly_fraction / total_kelly;
}
```

**Verdict: ⚠️ Kelly Optional - Start Equal-Weight**

---

## Deep Dive: Do We Need Adaptive Thresholds (Q-Learning/MAB)?

### Current System: Real-Time Learning

```cpp
class AdaptiveThresholdManager {
    // Q-learning: Learn optimal thresholds based on reward
    ThresholdPair get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
        MarketState state = regime_detector_->analyze(bar);
        ThresholdAction action = q_learner_->select_action(state, current_thresholds_);
        return q_learner_->apply_action(current_thresholds_, action);
    }

    void process_trade_outcome(double pnl, bool was_profitable) {
        double reward = calculate_reward(pnl, was_profitable);
        q_learner_->update_q_value(prev_state_, action_, reward, new_state_);
    }
};
```

**Use case:** Adapt buy/sell thresholds in real-time based on trade outcomes.

**Problems:**
1. **Slow convergence** - Q-learning needs thousands of samples
2. **Exploration/exploitation tradeoff** - Hard to balance
3. **Non-stationary environment** - Markets change, old Q-values stale
4. **Overfitting** - Can learn to exploit noise in specific market regime

### Alternative: Static Optimization with Optuna

```python
def objective(trial):
    # Define search space
    buy_threshold = trial.suggest_float('buy_threshold', 0.50, 0.70)
    sell_threshold = trial.suggest_float('sell_threshold', 0.30, 0.50)
    min_hold_bars = trial.suggest_int('min_hold_bars', 1, 10)
    max_positions = trial.suggest_int('max_positions', 1, 3)

    # Run backtest with these parameters
    results = run_rotation_backtest(
        data=spy_100blocks,
        config=RotationConfig(
            buy_threshold=buy_threshold,
            sell_threshold=sell_threshold,
            min_hold_bars=min_hold_bars,
            max_positions=max_positions
        )
    )

    # Optimize for MRD
    return results.mrd

# Run optimization
study = optuna.create_study(direction='maximize')
study.optimize(objective, n_trials=200)

# Use best parameters (static, no runtime learning)
best_params = study.best_params
```

**Advantages over Q-learning:**
- ✅ **Faster** - 200 trials in 10 minutes vs days of Q-learning
- ✅ **More robust** - Tests on historical data, not just recent trades
- ✅ **No exploration overhead** - Doesn't waste trades on exploration
- ✅ **Walk-forward validation** - Can test out-of-sample performance
- ✅ **Regime-aware** - Can optimize separately for bull/bear/choppy

**When to use Q-learning:**
- ⚠️ If market regime changes faster than we can re-optimize (unlikely)
- ⚠️ If we have 10,000+ trades to learn from (we don't)

**Verdict: ❌ Q-learning Not Needed - Use Optuna**

---

## Simplified Backend Architecture

### What We Actually Need

```cpp
// ================================================================
// 1. ROTATION POSITION MANAGER (Replaces PSM)
// ================================================================

class RotationPositionManager {
public:
    struct Config {
        int max_positions = 3;
        int min_hold_bars = 2;
        double buy_threshold = 0.55;

        enum class SizingMethod {
            EQUAL_WEIGHT,           // Start here
            SIGNAL_STRENGTH,        // Add later
            KELLY_CRITERION         // Add much later
        };
        SizingMethod sizing = EQUAL_WEIGHT;
    };

    // Main interface
    std::vector<TradeOrder> update(
        const std::vector<RankedSignal>& ranked_signals,
        uint64_t current_bar_id,
        double available_capital
    );

    // Position tracking
    const std::vector<ManagedPosition>& get_holdings() const { return holdings_; }
    int get_position_count() const { return holdings_.size(); }
    double get_cash_utilization() const;

private:
    bool should_rotate(const ManagedPosition& pos,
                      const std::vector<RankedSignal>& signals,
                      uint64_t bar_id);

    TradeOrder create_buy_order(const RankedSignal& signal, double capital);
    TradeOrder create_sell_order(const ManagedPosition& pos);

    double calculate_position_size(const RankedSignal& signal, double capital);

    std::vector<ManagedPosition> holdings_;
    Config config_;
};

// ================================================================
// 2. RISK MANAGER (Simplified)
// ================================================================

class SimpleRiskManager {
public:
    struct Limits {
        double max_position_size = 0.50;     // Max 50% per position
        double max_total_exposure = 1.00;    // Max 100% invested
        double min_cash_reserve = 0.05;      // Keep 5% cash
        int max_positions = 3;               // Max 3 positions
    };

    // Validate order before execution
    bool validate_order(const TradeOrder& order,
                       const PortfolioState& portfolio,
                       const Limits& limits);

    // Position size adjustments
    double apply_risk_limit(double requested_size,
                           const PortfolioState& portfolio,
                           const Limits& limits);
};

// ================================================================
// 3. ORDER GENERATOR (Simplified)
// ================================================================

class RotationOrderGenerator {
public:
    TradeOrder create_market_buy(
        const std::string& symbol,
        double dollar_amount,
        const Bar& current_bar
    );

    TradeOrder create_market_sell(
        const std::string& symbol,
        double quantity,
        const Bar& current_bar
    );

private:
    double round_quantity(double quantity, const std::string& symbol);
    double estimate_fill_price(const Bar& bar, TradeAction action);
};
```

### What We DON'T Need (Remove or Make Optional)

```cpp
// ❌ REMOVE: Complex 7-state PSM
class PositionStateMachine { ... };  // DELETE

// ❌ REMOVE: Q-learning adaptive thresholds
class QLearningThresholdOptimizer { ... };  // DELETE

// ❌ REMOVE: Multi-armed bandit
class MultiArmedBanditOptimizer { ... };  // DELETE

// ❌ REMOVE: Adaptive threshold manager
class AdaptiveThresholdManager { ... };  // DELETE

// ❌ REMOVE: Dynamic allocation manager
class DynamicAllocationManager { ... };  // DELETE

// ❌ REMOVE: Enhanced PSM
class EnhancedPositionStateMachine { ... };  // DELETE

// ❌ REMOVE: Dynamic hysteresis manager
class DynamicHysteresisManager { ... };  // DELETE
```

### Backend Comparison

| Component | Current System | Rotation System | Lines of Code |
|-----------|---------------|-----------------|---------------|
| Position State Machine | ✅ Required (7 states, 35 transitions) | ❌ Removed | ~800 → 0 |
| Adaptive Thresholds | ✅ Q-learning + MAB | ❌ Static (Optuna) | ~1200 → 0 |
| Rotation Manager | ❌ N/A | ✅ New (simple) | 0 → ~300 |
| Risk Manager | ✅ Complex (state-aware) | ✅ Simple (position limits) | ~600 → 150 |
| Order Generator | ✅ PSM-aware | ✅ Generic | ~400 → 200 |
| Kelly Allocation | ✅ Required | ⚠️ Optional | ~200 → 0 (start) |
| **Total Backend Code** | **~3200 lines** | **~650 lines** | **80% reduction** |

---

## Recommended Backend Structure

```
backend/
├── rotation_position_manager.h/cpp     (NEW - core rotation logic)
├── simple_risk_manager.h/cpp          (NEW - basic risk checks)
├── rotation_order_generator.h/cpp     (NEW - order creation)
│
├── position_state_machine.h/cpp       (DELETE)
├── enhanced_position_state_machine.h/cpp (DELETE)
├── adaptive_trading_mechanism.h/cpp   (DELETE)
├── dynamic_allocation_manager.h/cpp   (DELETE)
├── dynamic_hysteresis_manager.h/cpp   (DELETE)
```

---

## Migration Strategy

### Phase 1: Keep Minimal PSM (Backwards Compatibility)

For **testing only**, keep a minimal PSM that maps rotation logic to old interface:

```cpp
class RotationPSM : public PositionStateMachine {
public:
    StateTransition get_optimal_transition(
        const PortfolioState& portfolio,
        const SignalOutput& signal,
        const MarketState& market,
        double confidence_threshold
    ) override {
        // Delegate to RotationPositionManager
        auto orders = rotation_mgr_.update(ranked_signals_, current_bar_id_, capital_);

        // Convert orders to StateTransition (for compatibility)
        return convert_to_transition(orders);
    }

private:
    RotationPositionManager rotation_mgr_;
};
```

**Use case:** Test rotation logic without breaking existing code.

### Phase 2: Direct Rotation (Remove PSM)

Once rotation is validated, remove PSM entirely:

```cpp
// Old code (PSM-based)
auto transition = psm_->get_optimal_transition(portfolio, signal, market);
auto orders = convert_psm_transition_to_orders(transition);

// New code (direct rotation)
auto ranked_signals = signal_aggregator_->rank_signals(all_signals);
auto orders = rotation_mgr_->update(ranked_signals, bar_id, capital);
```

### Phase 3: Add Advanced Features (Optional)

If equal-weight underperforms, add:
1. **Signal strength weighting** (~50 lines)
2. **Kelly criterion** (~100 lines)
3. **Risk parity** (~150 lines)

---

## Final Recommendations

| Component | Verdict | Action |
|-----------|---------|--------|
| **PSM** | ❌ **Remove** | Delete `PositionStateMachine` and all subclasses |
| **Kelly Allocation** | ⚠️ **Optional** | Start equal-weight, add Kelly in Phase 2 if needed |
| **Q-Learning/MAB** | ❌ **Remove** | Use Optuna for static optimization instead |
| **Adaptive Mechanism** | ❌ **Remove** | Real-time learning not needed with rotation |
| **Rotation Manager** | ✅ **Build New** | Simple, ~300 lines, core rotation logic |
| **Risk Manager** | ✅ **Simplify** | Basic limits, no state-awareness needed |
| **Order Generator** | ✅ **Simplify** | Generic buy/sell, no PSM coupling |

**Net Result:**
- **Backend code:** 3200 lines → 650 lines (**80% reduction**)
- **Complexity:** High → Low
- **Maintainability:** Hard → Easy
- **Extensibility:** Rigid (must change PSM) → Flexible (just add symbols)

---

## Summary

**Q: Do we still need PSM?**
**A: ❌ NO - Replace with simple RotationPositionManager**

**Q: Do we still need Kelly allocation?**
**A: ⚠️ OPTIONAL - Start equal-weight, add Kelly later if equal-weight underperforms**

**Rotation backend is 80% simpler:**
- No complex state machine
- No real-time learning (use Optuna)
- No dynamic allocation (equal-weight is fine)
- Just: "Hold top N signals, rotate when rank changes"

**Code reduction:**
- DELETE: 2500+ lines (PSM, Q-learning, MAB, adaptive mechanism)
- ADD: 650 lines (rotation manager, simple risk, order generator)
- NET: -1850 lines (**80% less code**)

**Next step:** I can implement the `RotationPositionManager` if you approve this simplified architecture.
