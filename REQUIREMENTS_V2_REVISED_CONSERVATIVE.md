# OnlineTrader v2.0 - Revised Conservative Design
## Incorporating Expert Feedback

**Date:** 2025-10-15 (Revised)
**Status:** REVISED - Addressing Critical Risks
**Revision Author:** Based on expert quant review

---

## Executive Summary: What Changed

### Original Design Issues
1. ❌ **12 symbols immediately** - Too complex, too risky
2. ❌ **Equal-weight allocation** - Ignores volatility differences
3. ❌ **2s blocking synchronization** - Causes latency, missed bars
4. ❌ **Leveraged ETFs (TQQQ, SQQQ, UVXY)** - Decay, extreme volatility
5. ❌ **Simplistic rotation (min_hold=2)** - Doesn't account for signal decay, P&L
6. ❌ **No correlation monitoring** - Risk of concentrated bets
7. ❌ **No transaction cost modeling** - Critical for rotation strategies

### Revised Conservative Design
1. ✅ **Start with 3 symbols: SPY, QQQ, SH** - Proven, liquid, low decay
2. ✅ **Volatility-adjusted position sizing** - Inverse vol weighting
3. ✅ **Asynchronous data with staleness weighting** - No blocking
4. ✅ **Multi-factor rotation logic** - Rank change, signal decay, P&L-based
5. ✅ **Correlation monitoring** - Reject positions with corr > 0.85
6. ✅ **Transaction cost modeling** - Slippage, bid-ask spread
7. ✅ **Expand gradually** - Add symbols only after validation

---

## Phase 1 (Week 1-2): Foundation with 3 Symbols

### 1.1 Symbol Universe (Conservative)

**REQ-SYMBOLS-001:** Initial symbol set SHALL be limited to **3 symbols**:

| Symbol | Description | Leverage | Volatility (20-day) | Correlation to SPY |
|--------|-------------|----------|---------------------|-------------------|
| **SPY** | S&P 500 ETF | 1x | 15-20% annualized | 1.00 |
| **QQQ** | Nasdaq 100 ETF | 1x | 20-25% annualized | 0.85 |
| **SH** | Inverse S&P 500 | -1x | 15-20% annualized | -0.95 |

**Rationale:**
- **SPY, QQQ:** Liquid, low expense ratio, no decay
- **SH:** Provides inverse exposure without leverage decay
- **No TQQQ, SQQQ, UVXY:** Avoid daily rebalancing decay until core system validated

**REQ-SYMBOLS-002:** Expansion criteria for adding symbols:
- Phase 1 validation: MRD > +0.2% for 20 days on 3 symbols
- Low correlation to existing holdings (< 0.7)
- Sufficient liquidity (avg volume > 10M shares/day)
- No leveraged products until Phase 3

**REQ-SYMBOLS-003:** Phase 2 expansion (after Phase 1 success):
- **Add:** IWM (small-cap), TLT (bonds), GLD (gold)
- **Total:** 6 symbols (diversified across asset classes)
- **Correlation check:** Max pairwise correlation < 0.85

### 1.2 Asynchronous Data Synchronization

**REQ-DATA-SYNC-001:** Replace blocking synchronization with asynchronous snapshot:

```cpp
class AsyncMultiSymbolDataManager {
public:
    struct SymbolSnapshot {
        Bar latest_bar;
        uint64_t last_update_ms;
        double staleness_seconds;     // Age of data in seconds
        double staleness_weight;      // Weight reduction: exp(-staleness / 60)
    };

    // Non-blocking: return latest available snapshot
    std::map<std::string, SymbolSnapshot> get_latest_snapshot() {
        std::lock_guard<std::mutex> lock(data_mutex_);

        std::map<std::string, SymbolSnapshot> snapshot;
        uint64_t now = current_time_ms();

        for (const auto& [symbol, state] : symbol_states_) {
            SymbolSnapshot snap;
            snap.latest_bar = state.latest_bar;
            snap.last_update_ms = state.last_update_ms;
            snap.staleness_seconds = (now - state.last_update_ms) / 1000.0;

            // Exponential decay: fresh data = 1.0, 60s old = 0.37
            snap.staleness_weight = std::exp(-snap.staleness_seconds / 60.0);

            snapshot[symbol] = snap;
        }

        return snapshot;
    }

    // Background thread: continuously update from WebSocket
    void update_symbol(const std::string& symbol, const Bar& bar) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        symbol_states_[symbol].latest_bar = bar;
        symbol_states_[symbol].last_update_ms = bar.timestamp_ms;
    }

private:
    struct SymbolState {
        Bar latest_bar;
        uint64_t last_update_ms;
    };

    std::map<std::string, SymbolState> symbol_states_;
    std::mutex data_mutex_;
};
```

**Benefits:**
- ✅ No blocking - use whatever data is available
- ✅ Staleness penalty - reduce rank for old data
- ✅ Better latency - process immediately
- ✅ Resilient to missing/delayed data

**REQ-DATA-SYNC-002:** Apply staleness weighting to signal ranking:

```cpp
double RankedSignal::adjusted_strength() const {
    // Base strength = probability × confidence
    double base_strength = signal.probability * signal.confidence;

    // Apply staleness penalty
    double adjusted = base_strength * snapshot.staleness_weight;

    // Warn if data is very stale (> 120s)
    if (snapshot.staleness_seconds > 120) {
        log_warning("Symbol " + symbol + " data is stale: " +
                   std::to_string(snapshot.staleness_seconds) + "s");
    }

    return adjusted;
}
```

### 1.3 Volatility-Adjusted Position Sizing

**REQ-SIZING-001:** Replace equal-weight with **volatility-adjusted allocation**:

```cpp
class VolatilityAdjustedAllocator {
public:
    struct AllocationResult {
        std::map<std::string, double> weights;  // Symbol → weight (sum = 1.0)
        std::map<std::string, double> vols;     // Symbol → 20-day volatility
        double total_portfolio_vol;             // Portfolio volatility
    };

    AllocationResult calculate(
        const std::vector<RankedSignal>& ranked_signals,
        int max_positions
    ) {
        // Step 1: Calculate 20-day realized volatility for each symbol
        std::map<std::string, double> volatilities;
        for (const auto& signal : ranked_signals) {
            volatilities[signal.symbol] = calculate_realized_vol(
                signal.symbol, 20  // 20-day lookback
            );
        }

        // Step 2: Allocate inversely proportional to volatility
        // (Risk Parity approach)
        std::map<std::string, double> inverse_vols;
        double sum_inverse_vol = 0.0;

        for (int i = 0; i < max_positions && i < ranked_signals.size(); ++i) {
            const auto& signal = ranked_signals[i];
            double vol = volatilities[signal.symbol];

            // Inverse vol: 1/vol (high vol → small weight)
            double inv_vol = 1.0 / std::max(vol, 0.10);  // Floor at 10% vol
            inverse_vols[signal.symbol] = inv_vol;
            sum_inverse_vol += inv_vol;
        }

        // Step 3: Normalize to sum to 1.0
        AllocationResult result;
        for (const auto& [symbol, inv_vol] : inverse_vols) {
            result.weights[symbol] = inv_vol / sum_inverse_vol;
            result.vols[symbol] = volatilities[symbol];
        }

        // Step 4: Calculate portfolio volatility (with correlations)
        result.total_portfolio_vol = calculate_portfolio_vol(
            result.weights, result.vols
        );

        return result;
    }

private:
    double calculate_realized_vol(const std::string& symbol, int days) {
        // Get recent bars
        auto bars = get_recent_bars(symbol, days);
        if (bars.size() < days) {
            return 0.20;  // Default 20% annual vol
        }

        // Calculate daily returns
        std::vector<double> returns;
        for (size_t i = 1; i < bars.size(); ++i) {
            double ret = std::log(bars[i].close / bars[i-1].close);
            returns.push_back(ret);
        }

        // Standard deviation
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double sq_sum = 0.0;
        for (double ret : returns) {
            sq_sum += (ret - mean) * (ret - mean);
        }
        double daily_vol = std::sqrt(sq_sum / returns.size());

        // Annualize (252 trading days)
        return daily_vol * std::sqrt(252.0);
    }

    double calculate_portfolio_vol(
        const std::map<std::string, double>& weights,
        const std::map<std::string, double>& vols
    ) {
        // Simplified: assume correlations from historical data
        // Full implementation would use correlation matrix

        double var = 0.0;
        for (const auto& [sym1, w1] : weights) {
            for (const auto& [sym2, w2] : weights) {
                double corr = get_correlation(sym1, sym2);
                var += w1 * w2 * vols.at(sym1) * vols.at(sym2) * corr;
            }
        }

        return std::sqrt(var);
    }
};
```

**Example Allocation:**

| Symbol | 20-day Vol | 1/Vol | Weight | Equal-Weight (old) |
|--------|-----------|-------|--------|-------------------|
| **SPY** | 15% | 6.67 | 38% | 33.3% |
| **QQQ** | 20% | 5.00 | 29% | 33.3% |
| **SH** | 18% | 5.56 | 33% | 33.3% |

**Benefit:** More capital to low-vol SPY, less to high-vol QQQ → balanced risk contribution

### 1.4 Multi-Factor Rotation Logic

**REQ-ROTATION-001:** Replace simplistic `min_hold=2` with **multi-factor rotation**:

```cpp
class MultiFactorRotationDecider {
public:
    struct RotationSignal {
        bool should_rotate;
        std::string reason;
        double confidence;  // 0.0-1.0
    };

    RotationSignal evaluate_position(
        const ManagedPosition& position,
        const std::vector<RankedSignal>& current_rankings,
        uint64_t current_bar_id
    ) {
        RotationSignal result;
        result.should_rotate = false;
        result.confidence = 0.0;

        // Factor 1: Minimum hold period (hard constraint)
        int bars_held = current_bar_id - position.entry_bar_id;
        if (bars_held < min_hold_bars_) {
            result.reason = "Min hold not satisfied (" +
                           std::to_string(bars_held) + "/" +
                           std::to_string(min_hold_bars_) + ")";
            return result;  // HOLD - too early
        }

        // Factor 2: Rank degradation
        int old_rank = position.entry_rank;
        int current_rank = get_current_rank(position.symbol, current_rankings);
        int rank_change = current_rank - old_rank;

        if (rank_change > 2) {
            // Rank dropped by >2 positions → strong rotation signal
            result.should_rotate = true;
            result.reason = "Rank dropped: " + std::to_string(old_rank) +
                           " → " + std::to_string(current_rank);
            result.confidence = 0.8;
            return result;
        }

        // Factor 3: Signal strength decay
        double entry_strength = position.entry_signal_strength;
        double current_strength = get_current_strength(position.symbol, current_rankings);
        double strength_decay = (entry_strength - current_strength) / entry_strength;

        if (strength_decay > 0.30) {
            // Signal weakened by >30% → moderate rotation signal
            result.should_rotate = true;
            result.reason = "Signal decay: " +
                           std::to_string(strength_decay * 100) + "%";
            result.confidence = 0.6;
            return result;
        }

        // Factor 4: Stop-loss (P&L-based)
        double unrealized_pnl_pct = position.unrealized_pnl /
                                   (position.quantity * position.entry_price);

        if (unrealized_pnl_pct < -0.02) {
            // Down >2% → stop-loss triggered
            result.should_rotate = true;
            result.reason = "Stop-loss: " +
                           std::to_string(unrealized_pnl_pct * 100) + "%";
            result.confidence = 0.9;
            return result;
        }

        // Factor 5: Profit-taking (optional)
        if (unrealized_pnl_pct > 0.05 && bars_held > 20) {
            // Up >5% and held >20 bars → take profits
            result.should_rotate = true;
            result.reason = "Profit-taking: " +
                           std::to_string(unrealized_pnl_pct * 100) + "%";
            result.confidence = 0.5;
            return result;
        }

        // Factor 6: Not in top N anymore
        if (current_rank > max_positions_) {
            result.should_rotate = true;
            result.reason = "Not in top " + std::to_string(max_positions_);
            result.confidence = 0.7;
            return result;
        }

        // Default: HOLD
        result.reason = "All factors OK - holding";
        return result;
    }

private:
    int min_hold_bars_ = 5;  // Increased from 2
    int max_positions_ = 2;  // Conservative: max 2 positions initially
};
```

### 1.5 Correlation Monitoring & Risk Management

**REQ-RISK-001:** Add **correlation-aware risk management**:

```cpp
class CorrelationRiskManager {
public:
    struct RiskCheckResult {
        bool approved;
        std::string rejection_reason;
        double max_correlation;
        double total_leverage;
    };

    RiskCheckResult validate_portfolio(
        const std::vector<std::string>& proposed_symbols
    ) {
        RiskCheckResult result;
        result.approved = true;

        // Check 1: Pairwise correlation
        double max_corr = 0.0;
        for (size_t i = 0; i < proposed_symbols.size(); ++i) {
            for (size_t j = i + 1; j < proposed_symbols.size(); ++j) {
                double corr = get_correlation(
                    proposed_symbols[i],
                    proposed_symbols[j],
                    60  // 60-day lookback
                );

                max_corr = std::max(max_corr, std::abs(corr));

                if (std::abs(corr) > 0.85) {
                    result.approved = false;
                    result.rejection_reason =
                        "High correlation: " + proposed_symbols[i] +
                        " vs " + proposed_symbols[j] + " = " +
                        std::to_string(corr);
                    result.max_correlation = corr;
                    return result;
                }
            }
        }

        // Check 2: Aggregate leverage
        double total_leverage = 0.0;
        for (const auto& symbol : proposed_symbols) {
            double lev = get_leverage(symbol);  // SPY=1.0, SH=-1.0, TQQQ=3.0
            total_leverage += std::abs(lev);
        }

        if (total_leverage > 2.5) {
            result.approved = false;
            result.rejection_reason =
                "Total leverage too high: " + std::to_string(total_leverage);
            result.total_leverage = total_leverage;
            return result;
        }

        // Check 3: Sector concentration (for Phase 2+ with more symbols)
        // (Skipped for 3-symbol phase)

        result.max_correlation = max_corr;
        result.total_leverage = total_leverage;
        return result;
    }

private:
    double get_correlation(const std::string& sym1,
                          const std::string& sym2,
                          int days) {
        // Get returns for both symbols
        auto returns1 = get_returns(sym1, days);
        auto returns2 = get_returns(sym2, days);

        if (returns1.size() != returns2.size() || returns1.size() < days) {
            return 0.0;  // Insufficient data
        }

        // Calculate Pearson correlation
        double mean1 = std::accumulate(returns1.begin(), returns1.end(), 0.0) / returns1.size();
        double mean2 = std::accumulate(returns2.begin(), returns2.end(), 0.0) / returns2.size();

        double cov = 0.0, var1 = 0.0, var2 = 0.0;
        for (size_t i = 0; i < returns1.size(); ++i) {
            double d1 = returns1[i] - mean1;
            double d2 = returns2[i] - mean2;
            cov += d1 * d2;
            var1 += d1 * d1;
            var2 += d2 * d2;
        }

        return cov / std::sqrt(var1 * var2);
    }

    double get_leverage(const std::string& symbol) {
        // Metadata from config
        static const std::map<std::string, double> leverage_map = {
            {"SPY", 1.0},
            {"QQQ", 1.0},
            {"SH", -1.0},
            {"IWM", 1.0},
            {"TLT", 1.0},
            {"GLD", 1.0}
        };

        auto it = leverage_map.find(symbol);
        return (it != leverage_map.end()) ? it->second : 1.0;
    }
};
```

### 1.6 Transaction Cost Modeling

**REQ-COST-001:** Model transaction costs explicitly:

```cpp
class TransactionCostModel {
public:
    struct CostEstimate {
        double slippage_bps;       // Basis points (1 bp = 0.01%)
        double spread_cost_bps;    // Bid-ask spread cost
        double commission;         // $0 for Alpaca
        double total_cost_pct;     // Total as percentage
        double total_cost_dollars; // Total in dollars
    };

    CostEstimate estimate_cost(
        const std::string& symbol,
        double quantity,
        double price,
        TradeAction action
    ) {
        CostEstimate est;

        // Slippage: ~5 bps for SPY/QQQ, ~10 bps for less liquid
        est.slippage_bps = get_slippage_bps(symbol, quantity);

        // Spread: half-spread for market orders
        double spread_pct = get_bid_ask_spread_pct(symbol);
        est.spread_cost_bps = spread_pct * 100 * 0.5;  // Half-spread

        // Commission: $0 for Alpaca
        est.commission = 0.0;

        // Total
        est.total_cost_pct = (est.slippage_bps + est.spread_cost_bps) / 10000.0;
        est.total_cost_dollars = quantity * price * est.total_cost_pct;

        return est;
    }

    // Update: Track actual vs estimated costs
    void record_actual_cost(
        const std::string& symbol,
        double estimated_fill_price,
        double actual_fill_price,
        double quantity
    ) {
        double actual_slippage = std::abs(actual_fill_price - estimated_fill_price) /
                                estimated_fill_price;

        // Update running average for this symbol
        slippage_history_[symbol].push_back(actual_slippage);
        if (slippage_history_[symbol].size() > 100) {
            slippage_history_[symbol].erase(slippage_history_[symbol].begin());
        }
    }

private:
    double get_slippage_bps(const std::string& symbol, double quantity) {
        // Base slippage
        double base_bps = 5.0;  // 5 bps for SPY/QQQ

        // Adjust for symbol liquidity
        if (symbol == "SH") base_bps = 10.0;  // Less liquid

        // Adjust for quantity (market impact)
        double avg_volume = get_avg_daily_volume(symbol);
        double volume_pct = quantity / avg_volume;
        if (volume_pct > 0.01) {
            base_bps *= (1.0 + volume_pct * 10);  // Linear impact
        }

        // Historical adjustment
        if (slippage_history_.count(symbol)) {
            auto& history = slippage_history_[symbol];
            double avg_actual = std::accumulate(history.begin(), history.end(), 0.0) /
                               history.size();
            base_bps = avg_actual * 10000;  // Convert to bps
        }

        return base_bps;
    }

    double get_bid_ask_spread_pct(const std::string& symbol) {
        // Typical spreads
        static const std::map<std::string, double> spreads = {
            {"SPY", 0.0001},  // 1 bp
            {"QQQ", 0.0001},  // 1 bp
            {"SH", 0.0005}    // 5 bps (wider)
        };

        auto it = spreads.find(symbol);
        return (it != spreads.end()) ? it->second : 0.001;  // Default 10 bps
    }

    std::map<std::string, std::vector<double>> slippage_history_;
};
```

**REQ-COST-002:** Rotation frequency target with cost awareness:

```cpp
// Estimate daily turnover and costs
struct TurnoverEstimate {
    int expected_rotations_per_day;
    double expected_turnover_pct;       // % of portfolio rotated
    double expected_cost_per_day_bps;   // Transaction costs in bps
};

TurnoverEstimate estimate_daily_turnover(const RotationConfig& config) {
    TurnoverEstimate est;

    // With min_hold=5 bars, max_positions=2
    // Conservative: ~20 rotations/day (vs 100 in original design)
    est.expected_rotations_per_day = 390 / config.min_hold_bars *
                                     config.max_positions * 0.3;  // 30% rotation rate

    // Each rotation: sell 1 position (50%), buy 1 position (50%) = 100% turnover
    est.expected_turnover_pct = est.expected_rotations_per_day * 1.0;

    // Cost: ~10 bps per round-trip (5 bps buy + 5 bps sell)
    est.expected_cost_per_day_bps = est.expected_turnover_pct * 10.0;

    return est;
}

// Example with min_hold=5, max_positions=2:
// Rotations: 390 / 5 * 2 * 0.3 = 47 rotations/day
// Turnover: 47 * 1.0 = 47x (4700% - too high!)
// Cost: 47 * 10 bps = 470 bps = 4.7% per day (UNSUSTAINABLE)

// This shows min_hold=5 is still too aggressive!
// Recommendation: min_hold=10-20 bars for sustainable costs
```

---

## Revised Success Metrics

### Phase 1 Targets (Conservative, 3 symbols)

| Metric | Original Target | Revised Target | Rationale |
|--------|----------------|----------------|-----------|
| **MRD** | +0.5% | **+0.2%** | More realistic for 3 symbols + costs |
| **Sharpe Ratio** | 2.0 | **1.5** | Conservative with transaction costs |
| **Max Drawdown** | 3% | **5%** | Rotation strategies have drawdowns |
| **Rotation Frequency** | 50-100/day | **10-20/day** | Reduce costs (min_hold=10-20) |
| **Transaction Costs** | Not modeled | **< 20 bps/day** | Critical for profitability |
| **Win Rate** | 60% | **52%** | Momentum strategies ~50-55% |

### Cost-Adjusted MRD Target

```
Gross MRD target: +0.3% per day
Transaction costs: -0.10% per day (20 rotations × 10 bps / 2)
Net MRD target: +0.2% per day

Annualized: 0.2% × 252 days = 50.4% per year
```

**This is still excellent performance!** Much more realistic than 500%+ in original design.

---

## Revised Implementation Timeline

### Phase 1: Foundation (Week 1-2)
**Symbols:** SPY, QQQ, SH (3 total)
**Max Positions:** 2
**Min Hold:** 10 bars

**Deliverables:**
1. ✅ Async data manager with staleness weighting
2. ✅ Volatility tracker (20-day realized vol)
3. ✅ Volatility-adjusted allocator
4. ✅ Multi-factor rotation logic
5. ✅ Correlation risk manager
6. ✅ Transaction cost model
7. ✅ Unit tests

**Success Criteria:**
- Backtest MRD > +0.15% (100 blocks)
- Max drawdown < 5%
- Correlation checks working
- Cost tracking accurate

### Phase 2: Risk Layer (Week 3)
**Symbols:** Add IWM, TLT, GLD (6 total)
**Max Positions:** 3
**Min Hold:** 10 bars

**Deliverables:**
1. ✅ Expand to 6 symbols (diversified asset classes)
2. ✅ Portfolio volatility monitoring
3. ✅ Sector concentration checks
4. ✅ Enhanced correlation matrix
5. ✅ Walk-forward validation

**Success Criteria:**
- Backtest MRD > +0.18% (6 symbols)
- Correlation < 0.7 between any 2 positions
- Portfolio vol < 25%
- Out-of-sample Sharpe > 1.2

### Phase 3: Careful Expansion (Week 4)
**Symbols:** Consider adding TQQQ, but ONLY if Phase 2 validated
**Max Positions:** 3
**Min Hold:** 15 bars (for leveraged products)

**Deliverables:**
1. ✅ Decay modeling for leveraged ETFs
2. ✅ Dynamic min_hold based on volatility
3. ✅ Advanced cost model (market impact)
4. ✅ Regime-aware parameter adjustment

**Success Criteria:**
- Backtest MRD > +0.20% with leveraged products
- Decay costs modeled accurately
- No blowups in high-vol regimes

### Phase 4: Paper Trading (Week 5)
**Run parallel with v1.x system**

**Deliverables:**
1. ✅ Deploy to Alpaca paper account
2. ✅ Real-time monitoring dashboard
3. ✅ Slippage tracking (est vs actual)
4. ✅ Daily performance reports

**Success Criteria:**
- Paper MRD > +0.15% (5 days)
- Actual slippage < estimated × 1.5
- No correlation violations
- Max 15 rotations/day

---

## Critical Code Architecture Changes

### Cleaner Component Design

```cpp
// Single Responsibility Principle applied

class RotationEngine {
private:
    // Data layer
    std::unique_ptr<AsyncMultiSymbolDataManager> data_mgr_;

    // Strategy layer
    std::unique_ptr<MultiSymbolOESManager> oes_mgr_;
    std::unique_ptr<SignalAggregator> signal_agg_;

    // Allocation layer
    std::unique_ptr<VolatilityTracker> vol_tracker_;
    std::unique_ptr<VolatilityAdjustedAllocator> allocator_;

    // Risk layer
    std::unique_ptr<CorrelationRiskManager> risk_mgr_;
    std::unique_ptr<TransactionCostModel> cost_model_;

    // Execution layer
    std::unique_ptr<MultiFactorRotationDecider> rotation_decider_;
    std::unique_ptr<RotationExecutor> executor_;

public:
    std::vector<TradeOrder> process_bar(uint64_t bar_id) {
        // Step 1: Get latest data (async, non-blocking)
        auto snapshot = data_mgr_->get_latest_snapshot();

        // Step 2: Generate signals for all symbols
        auto signals = oes_mgr_->generate_signals(snapshot);

        // Step 3: Rank signals (with staleness weighting)
        auto ranked = signal_agg_->rank_signals(signals);

        // Step 4: Allocate positions (volatility-adjusted)
        auto allocation = allocator_->calculate(ranked, max_positions_);

        // Step 5: Risk checks (correlation, leverage)
        auto risk_check = risk_mgr_->validate_portfolio(
            get_symbols(allocation.weights)
        );
        if (!risk_check.approved) {
            log_warning("Risk check failed: " + risk_check.rejection_reason);
            return {};  // No trades
        }

        // Step 6: Rotation decisions (multi-factor)
        std::vector<TradeOrder> orders;
        for (const auto& pos : current_positions_) {
            auto decision = rotation_decider_->evaluate_position(
                pos, ranked, bar_id
            );

            if (decision.should_rotate) {
                // Liquidate position
                auto sell_order = executor_->create_sell_order(pos);

                // Estimate cost
                auto cost_est = cost_model_->estimate_cost(
                    pos.symbol, pos.quantity, pos.current_price, TradeAction::SELL
                );
                sell_order.metadata["est_cost_bps"] = cost_est.total_cost_pct * 10000;

                orders.push_back(sell_order);
            }
        }

        // Step 7: Fill vacancies with top-ranked signals
        int vacancies = max_positions_ - (current_positions_.size() - orders.size());
        for (int i = 0; i < vacancies && i < ranked.size(); ++i) {
            auto buy_order = executor_->create_buy_order(
                ranked[i],
                allocation.weights.at(ranked[i].symbol),
                available_capital_
            );

            auto cost_est = cost_model_->estimate_cost(
                ranked[i].symbol,
                buy_order.quantity,
                buy_order.price,
                TradeAction::BUY
            );
            buy_order.metadata["est_cost_bps"] = cost_est.total_cost_pct * 10000;

            orders.push_back(buy_order);
        }

        return orders;
    }
};
```

---

## Key Takeaways (Addressing Reviewer Feedback)

### 1. ✅ Data Synchronization Fixed
- **Old:** Blocking 2s wait for all symbols
- **New:** Async with staleness weighting
- **Benefit:** Better latency, resilient to delays

### 2. ✅ Leveraged ETF Risks Addressed
- **Old:** 12 symbols including TQQQ, SQQQ, UVXY immediately
- **New:** Start with SPY, QQQ, SH only
- **Phase 3:** Add TQQQ only after validation + decay modeling

### 3. ✅ Volatility-Adjusted Sizing
- **Old:** Equal-weight (1/3 each)
- **New:** Inverse volatility weighting (Risk Parity)
- **Benefit:** Balanced risk contribution

### 4. ✅ Multi-Factor Rotation
- **Old:** Simplistic min_hold=2
- **New:** Multi-factor (rank, signal decay, P&L, min_hold=10)
- **Benefit:** Fewer rotations, lower costs, better risk management

### 5. ✅ Correlation Monitoring
- **Old:** No correlation checks
- **New:** Max correlation 0.85, reject correlated pairs
- **Benefit:** Avoid concentrated bets

### 6. ✅ Transaction Cost Modeling
- **Old:** No cost modeling
- **New:** Explicit slippage + spread modeling, cost tracking
- **Benefit:** Realistic MRD targets

### 7. ✅ Conservative Targets
- **Old:** MRD +0.5%, Sharpe 2.0
- **New:** MRD +0.2%, Sharpe 1.5
- **Benefit:** Achievable, realistic

---

## Summary: What Changed

| Aspect | Original Design | Revised Design |
|--------|----------------|----------------|
| **Symbols (Phase 1)** | 12 | **3 (SPY, QQQ, SH)** |
| **Leveraged ETFs** | Yes (TQQQ, SQQQ, UVXY) | **No (Phase 3 only)** |
| **Position Sizing** | Equal-weight | **Volatility-adjusted** |
| **Data Sync** | Blocking 2s | **Async + staleness** |
| **Rotation Logic** | min_hold=2 | **Multi-factor, min_hold=10** |
| **Risk Checks** | None | **Correlation + leverage** |
| **Cost Modeling** | None | **Explicit modeling** |
| **MRD Target** | +0.5% | **+0.2% (net of costs)** |
| **Rotation Frequency** | 50-100/day | **10-20/day** |

---

## Approval & Next Steps

**This revised design addresses all critical feedback:**
1. ✅ Start small (3 symbols)
2. ✅ Volatility-adjust everything
3. ✅ Monitor correlation
4. ✅ Model transaction costs
5. ✅ Avoid leveraged ETFs initially
6. ✅ Realistic MRD targets

**Ready for implementation?**
- Phase 1 (3 symbols) is well-defined
- All components specified with code examples
- Success criteria are achievable
- Risk management is comprehensive

**Shall I proceed with implementing Phase 1?**
