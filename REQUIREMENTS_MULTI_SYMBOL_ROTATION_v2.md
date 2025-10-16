# Requirements Document: Multi-Symbol Rotation Trading System v2.0

**Project:** OnlineTrader Multi-Symbol Rotation Architecture
**Version:** 2.0
**Date:** 2025-10-15
**Author:** Architecture Design Team
**Status:** DRAFT - Awaiting Approval

---

## Executive Summary

This document specifies requirements for a **major architectural redesign** of the OnlineTrader system, transitioning from a **single-symbol (SPY) Position State Machine** to a **12-symbol momentum rotation strategy**.

### Key Changes

| Aspect | Current (v1.x) | Target (v2.0) |
|--------|---------------|---------------|
| **Symbols Monitored** | 1 (SPY only) | 12 (SVIX, SVXY, TQQQ, UPRO, QQQ, SPY, SH, PSQ, SDS, SQQQ, UVXY, UVIX) |
| **OES Instances** | 1 | 12 (one per symbol) |
| **Data Pipeline** | Single-symbol bars | Multi-symbol synchronized bars |
| **Live Data Source** | Alpaca REST (SPY only) | Alpaca WebSocket (12 symbols) |
| **Backend** | 7-state PSM + Q-learning | Simple rotation manager |
| **Position Management** | Complex state transitions | Top-N rotation |
| **Code Complexity** | ~3200 lines (backend) | ~650 lines (backend) |

### Target Performance

- **Current MRD:** +0.046% per block
- **Target MRD:** +0.5% per block (10.8x improvement)
- **Conservative Goal:** +0.3% per block (6.5x improvement)

---

## Table of Contents

1. [System Architecture](#1-system-architecture)
2. [Data Pipeline Requirements](#2-data-pipeline-requirements)
3. [Live Trading Data Interface](#3-live-trading-data-interface)
4. [Mock Testing Data Interface](#4-mock-testing-data-interface)
5. [Warmup and Historical Data](#5-warmup-and-historical-data)
6. [Multi-Symbol OES Manager](#6-multi-symbol-oes-manager)
7. [Rotation Position Manager](#7-rotation-position-manager)
8. [Backend Simplification](#8-backend-simplification)
9. [Configuration Management](#9-configuration-management)
10. [Testing Strategy](#10-testing-strategy)
11. [Implementation Phases](#11-implementation-phases)
12. [Success Metrics](#12-success-metrics)
13. [Risks and Mitigations](#13-risks-and-mitigations)

---

## 1. System Architecture

### 1.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      DATA LAYER                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────┐ │
│  │ Live Data Feed   │  │ Mock Data Feed   │  │ Warmup Data  │ │
│  │ (Alpaca WS)      │  │ (Historical CSV) │  │ (Downloaded) │ │
│  │ 12 symbols       │  │ 12 symbols       │  │ 12 symbols   │ │
│  └────────┬─────────┘  └────────┬─────────┘  └──────┬───────┘ │
│           │                     │                     │         │
│           └─────────────────────┼─────────────────────┘         │
│                                 v                               │
│                    ┌─────────────────────────┐                  │
│                    │ MultiSymbolDataManager  │                  │
│                    │ - Synchronize bars      │                  │
│                    │ - Validate data         │                  │
│                    │ - Cache management      │                  │
│                    └────────────┬────────────┘                  │
└─────────────────────────────────┼──────────────────────────────┘
                                  │
┌─────────────────────────────────┼──────────────────────────────┐
│                      STRATEGY LAYER                             │
├─────────────────────────────────┼──────────────────────────────┤
│                                 v                               │
│              ┌────────────────────────────────┐                 │
│              │ MultiSymbolOESManager          │                 │
│              │ - 12 OES instances             │                 │
│              │ - Feature engines (12)         │                 │
│              │ - EWRLS predictors (12 × 3)    │                 │
│              └────────────┬───────────────────┘                 │
│                           v                                     │
│              ┌────────────────────────────────┐                 │
│              │ SignalAggregator               │                 │
│              │ - Collect 12 signals/bar       │                 │
│              │ - Rank by strength             │                 │
│              │ - Filter by threshold          │                 │
│              └────────────┬───────────────────┘                 │
└─────────────────────────────────┼──────────────────────────────┘
                                  │
┌─────────────────────────────────┼──────────────────────────────┐
│                      EXECUTION LAYER                            │
├─────────────────────────────────┼──────────────────────────────┤
│                                 v                               │
│              ┌────────────────────────────────┐                 │
│              │ RotationPositionManager        │                 │
│              │ - Hold top N signals           │                 │
│              │ - Enforce min hold period      │                 │
│              │ - Equal-weight allocation      │                 │
│              └────────────┬───────────────────┘                 │
│                           v                                     │
│              ┌────────────────────────────────┐                 │
│              │ OrderGenerator                 │                 │
│              │ - Create buy/sell orders       │                 │
│              │ - Risk validation              │                 │
│              └────────────┬───────────────────┘                 │
│                           v                                     │
│              ┌────────────────────────────────┐                 │
│              │ BrokerInterface                │                 │
│              │ - Alpaca API (live)            │                 │
│              │ - MockBroker (testing)         │                 │
│              └────────────────────────────────┘                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Component Responsibilities

| Component | Responsibility | Input | Output |
|-----------|---------------|-------|--------|
| **MultiSymbolDataManager** | Synchronize and validate multi-symbol data | Symbol bars (async) | Synchronized bar set |
| **MultiSymbolOESManager** | Manage 12 OES instances, generate signals | Synchronized bars | 12 signals/bar |
| **SignalAggregator** | Rank signals by strength | 12 signals | Top N ranked signals |
| **RotationPositionManager** | Rotation logic, position tracking | Ranked signals | Trade orders |
| **OrderGenerator** | Convert rotation decisions to orders | Rotation decisions | Executable orders |
| **BrokerInterface** | Execute orders, manage positions | Orders | Fill confirmations |

---

## 2. Data Pipeline Requirements

### 2.1 Symbol Universe

**REQ-DATA-001:** The system SHALL support the following 12 symbols:

| Category | Symbols | Description |
|----------|---------|-------------|
| **Equity Indices** | SPY, QQQ | S&P 500, Nasdaq 100 |
| **Leveraged Long** | UPRO, TQQQ | 3x SPY, 3x QQQ |
| **Inverse (Short)** | SH, PSQ, SDS, SQQQ | -1x SPY, -1x QQQ, -2x SPY, -3x QQQ |
| **Volatility** | SVIX, SVXY, UVXY, UVIX | Short VIX (2x), Short VIX, Long VIX, 2x Long VIX |

**REQ-DATA-002:** The symbol list SHALL be configurable via `config/symbols.json`:
```json
{
  "active_symbols": [
    "SVIX", "SVXY", "TQQQ", "UPRO", "QQQ", "SPY",
    "SH", "PSQ", "SDS", "SQQQ", "UVXY", "UVIX"
  ],
  "symbol_metadata": {
    "TQQQ": { "leverage": 3.0, "direction": "long", "underlying": "QQQ" },
    "SQQQ": { "leverage": -3.0, "direction": "short", "underlying": "QQQ" },
    ...
  }
}
```

### 2.2 Data Synchronization

**REQ-DATA-003:** The system SHALL synchronize bars across all symbols with the following rules:

1. **Timestamp Alignment:** All bars MUST have identical timestamps (1-minute resolution)
2. **Missing Data Handling:**
   - If symbol X is missing for timestamp T, use last known close price
   - Log warning: `"Symbol X missing at T - using forward fill"`
   - Limit: Max 5 consecutive forward fills, then skip timestamp
3. **Late Arrivals:**
   - Wait up to 2 seconds for all symbols to arrive
   - After 2s timeout, process available symbols
4. **Staleness Check:**
   - Reject bars older than 5 minutes (prevent replay attacks)

**REQ-DATA-004:** The system SHALL maintain a synchronized bar buffer:
```cpp
struct SynchronizedBarSet {
    int64_t timestamp_ms;                      // Common timestamp
    std::map<std::string, Bar> bars;           // Symbol → Bar
    std::set<std::string> missing_symbols;     // Symbols not available
    bool is_complete;                          // All 12 symbols present?
    int forward_fill_count;                    // How many symbols forward-filled?
};
```

### 2.3 Data Validation

**REQ-DATA-005:** All incoming bars SHALL be validated against:

| Check | Criteria | Action on Failure |
|-------|----------|-------------------|
| **Price Sanity** | 0.01 < price < 10,000 | Reject bar, log error |
| **Volume Sanity** | volume >= 0 | Reject bar, log warning |
| **Timestamp Order** | timestamp > previous | Reject bar, log error |
| **Duplicate Detection** | timestamp not in cache | Reject bar, log warning |
| **OHLC Consistency** | low ≤ open,close ≤ high | Correct if possible, else reject |

**REQ-DATA-006:** The system SHALL log all data quality issues to:
- `logs/data_quality/YYYYMMDD.log` (daily rotation)
- Include: symbol, timestamp, issue type, raw data

---

## 3. Live Trading Data Interface

### 3.1 Alpaca WebSocket Integration

**REQ-LIVE-001:** The system SHALL use Alpaca WebSocket API v2 for real-time data:
- Endpoint: `wss://stream.data.alpaca.markets/v2/iex`
- Feed: IEX (free tier, real-time)
- Subscription: 1-minute aggregated bars for all 12 symbols

**REQ-LIVE-002:** The live data interface SHALL implement:

```cpp
class AlpacaMultiSymbolFeed {
public:
    // Initialize connection
    void connect(const std::vector<std::string>& symbols,
                const std::string& api_key,
                const std::string& secret_key);

    // Subscribe to symbols
    void subscribe_bars(const std::vector<std::string>& symbols,
                       const std::string& timeframe = "1Min");

    // Get next synchronized bar set (blocking)
    SynchronizedBarSet get_next_bar_set(int timeout_ms = 2000);

    // Non-blocking: check if bar set ready
    bool is_bar_set_ready() const;

    // Disconnect
    void disconnect();

private:
    // WebSocket handler
    void on_bar_received(const std::string& symbol, const Bar& bar);
    void on_error(const std::string& error);
    void on_disconnect();

    // Synchronization
    void buffer_bar(const std::string& symbol, const Bar& bar);
    SynchronizedBarSet assemble_bar_set(int64_t timestamp);

    std::map<std::string, std::deque<Bar>> symbol_buffers_;
    std::mutex buffer_mutex_;
    std::condition_variable bar_ready_cv_;
};
```

**REQ-LIVE-003:** Connection resilience:
- **Auto-reconnect:** Reconnect on disconnect (max 5 retries, exponential backoff)
- **Heartbeat:** Send ping every 30s, expect pong within 5s
- **Subscription recovery:** Re-subscribe to all symbols on reconnect
- **Gap detection:** Log warning if timestamp gap > 2 minutes

**REQ-LIVE-004:** Rate limiting:
- Respect Alpaca rate limits: 200 requests/minute (REST), unlimited WebSocket
- If rate-limited, log warning and wait 60s before retry

### 3.2 Polygon Fallback (Optional)

**REQ-LIVE-005:** If Alpaca WebSocket fails, the system MAY fall back to Polygon REST API:
- Use `tools/polygon_client.py` to fetch latest bars
- Poll every 60s (1-minute bars)
- Higher latency (10-30s delay) but more reliable

---

## 4. Mock Testing Data Interface

### 4.1 Historical Data Replay

**REQ-MOCK-001:** The system SHALL support mock trading mode using historical CSV data:

```bash
# Mock trading with specific date
./build/sentio_cli live-trade \
    --mock \
    --mock-date 2025-10-07 \
    --mock-speed 39.0  # 39x real-time (10s per 390-bar day)
```

**REQ-MOCK-002:** Mock data files SHALL be organized as:
```
data/equities/
├── SPY_RTH_NH.csv      (master file, all dates)
├── QQQ_RTH_NH.csv
├── TQQQ_RTH_NH.csv
├── ...
└── UVIX_RTH_NH.csv

data/tmp/
├── SPY_session_20251007.csv    (extracted single day)
├── QQQ_session_20251007.csv
└── ...
```

**REQ-MOCK-003:** The system SHALL extract session data for a specific date:

```cpp
class MultiSymbolSessionExtractor {
public:
    // Extract single day data for all 12 symbols
    void extract_session(
        const std::string& date,                    // "YYYY-MM-DD"
        const std::vector<std::string>& symbols,    // 12 symbols
        const std::string& master_dir,              // "data/equities"
        const std::string& output_dir               // "data/tmp"
    );

    // Generate warmup data (20 blocks before target date)
    void extract_warmup(
        const std::string& date,
        const std::vector<std::string>& symbols,
        int warmup_blocks,                          // Default: 20
        const std::string& output_file              // "data/equities/warmup_latest.csv"
    );
};
```

**REQ-MOCK-004:** Mock replay SHALL support configurable speed:
- **0x (instant):** Process all bars as fast as possible (backtesting)
- **1x (real-time):** 1 bar per second (60-second simulation)
- **39x (accelerated):** 39 bars per second (10-second simulation, default)
- **0.1x (slow-motion):** 1 bar per 10 seconds (detailed observation)

**REQ-MOCK-005:** Mock broker SHALL simulate:
- **Order fills:** Market orders filled at current bar close
- **Slippage:** 0.05% per trade (configurable)
- **Commissions:** $0 (Alpaca is commission-free)
- **Position tracking:** Maintain positions across bars
- **P&L calculation:** Mark-to-market every bar

### 4.2 Leveraged ETF Data Generation

**REQ-MOCK-006:** If leveraged/inverse ETF data is missing, the system SHALL generate synthetic data:

```python
# tools/generate_multi_symbol_data.py
import pandas as pd

def generate_leveraged_data(spy_df, leverage_factor, symbol):
    """
    Generate synthetic leveraged ETF data from SPY

    Args:
        spy_df: SPY price data (OHLCV)
        leverage_factor: 3.0 (UPRO/TQQQ), -1.0 (SH), -3.0 (SQQQ)
        symbol: Target symbol name

    Returns:
        Synthetic OHLCV data for leveraged symbol
    """
    spy_returns = spy_df['close'].pct_change()
    lev_returns = spy_returns * leverage_factor

    lev_close = 100 * (1 + lev_returns).cumprod()
    # Scale OHLV proportionally...

    return lev_df
```

**REQ-MOCK-007:** Generated data SHALL include a metadata tag:
- Add `is_synthetic = true` to bar metadata
- Log warning on first use: `"Using synthetic data for TQQQ (generated from QQQ)"`

---

## 5. Warmup and Historical Data

### 5.1 Data Downloader Integration

**REQ-WARMUP-001:** The system SHALL use `tools/data_downloader.py` to fetch recent data:

```bash
# Download last 30 days for all 12 symbols
python3 tools/data_downloader.py \
    --symbols SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX \
    --days 30 \
    --timeframe 1Min \
    --output data/equities \
    --source polygon  # or alpaca
```

**REQ-WARMUP-002:** Data downloader SHALL:
1. **Check cache first:** If data exists and is < 24 hours old, skip download
2. **Download in parallel:** Use thread pool (4 threads) to download 12 symbols concurrently
3. **Validate data:** Check for gaps, duplicates, and price sanity
4. **Merge with existing:** Append new data to existing CSV files (deduplicate)
5. **Report status:** Log downloaded bars per symbol, gaps found, errors

**REQ-WARMUP-003:** Warmup data requirements:

| Component | Warmup Needed | Reason |
|-----------|---------------|--------|
| **Feature Engine** | 64 bars (max rolling window) | Initialize all rolling features |
| **EWRLS Predictor** | 50-100 samples | Train initial model |
| **OES Strategy** | 100 bars | Warmup samples |
| **Rotation Manager** | 20 blocks (7800 bars) | Learn symbol-specific patterns |

**Total warmup target:** **20 blocks = 20 × 390 bars = 7800 bars = ~13 trading days**

**REQ-WARMUP-004:** Warmup data preparation workflow:

```bash
# 1. Download latest data (if older than 24h)
./scripts/download_all_symbols.sh

# 2. Extract warmup data (20 blocks before today)
python3 tools/extract_warmup_data.py \
    --symbols all \
    --blocks 20 \
    --output data/equities/warmup_latest.csv

# 3. Run warmup (before live trading)
./build/sentio_cli warmup \
    --data data/equities/warmup_latest.csv \
    --symbols all \
    --output logs/warmup_state.bin
```

**REQ-WARMUP-005:** Warmup state persistence:
- **Save:** Feature engine state, EWRLS model weights, performance metrics
- **Format:** Binary protobuf or JSON (for debugging)
- **Location:** `logs/warmup_state_{YYYYMMDD}.bin`
- **Load:** On live trading start, load most recent warmup state

### 5.2 Comprehensive Warmup Script

**REQ-WARMUP-006:** The system SHALL provide `scripts/comprehensive_warmup_multi.sh`:

```bash
#!/bin/bash
# Comprehensive warmup for multi-symbol rotation system

set -e

PROJECT_ROOT="/Volumes/ExternalSSD/Dev/C++/online_trader"
cd "$PROJECT_ROOT"

SYMBOLS="SVIX,SVXY,TQQQ,UPRO,QQQ,SPY,SH,PSQ,SDS,SQQQ,UVXY,UVIX"
WARMUP_BLOCKS=20
TODAY=$(date '+%Y-%m-%d')

echo "=== Multi-Symbol Warmup Started ==="
echo "Date: $TODAY"
echo "Symbols: 12"
echo "Warmup: $WARMUP_BLOCKS blocks"

# Step 1: Download latest data for all symbols
echo "Step 1: Downloading latest data..."
python3 tools/data_downloader.py \
    --symbols "$SYMBOLS" \
    --days 30 \
    --output data/equities \
    --source polygon

# Step 2: Extract warmup data (20 blocks + today)
echo "Step 2: Extracting warmup data..."
python3 tools/extract_warmup_multi.py \
    --symbols "$SYMBOLS" \
    --blocks "$WARMUP_BLOCKS" \
    --include-today \
    --output data/equities/warmup_latest.csv

# Step 3: Run warmup (train all 12 OES instances)
echo "Step 3: Running warmup..."
./build/sentio_cli warmup-multi \
    --data data/equities/warmup_latest.csv \
    --symbols "$SYMBOLS" \
    --output logs/warmup_state_$TODAY.bin \
    --verbose

echo "=== Warmup Complete ==="
echo "State saved to: logs/warmup_state_$TODAY.bin"
echo "Ready for live trading!"
```

---

## 6. Multi-Symbol OES Manager

### 6.1 Architecture

**REQ-OES-001:** The system SHALL implement a `MultiSymbolOESManager` that manages 12 independent OES instances:

```cpp
class MultiSymbolOESManager {
public:
    struct Config {
        std::vector<std::string> symbols;  // 12 symbols
        OnlineEnsembleStrategy::OnlineEnsembleConfig oes_config;  // Shared config
        bool independent_thresholds = false;  // Per-symbol thresholds?
        std::string warmup_state_path;        // Load pre-trained state
    };

    // Initialize all OES instances
    explicit MultiSymbolOESManager(const Config& config);

    // Update all OES instances with synchronized bars
    void update_all(const SynchronizedBarSet& bar_set);

    // Generate signals for all symbols
    std::map<std::string, SignalOutput> generate_signals(
        const SynchronizedBarSet& bar_set
    );

    // Get individual OES instance (for diagnostics)
    const OnlineEnsembleStrategy& get_oes(const std::string& symbol) const;

    // Save/load state
    void save_state(const std::string& path) const;
    void load_state(const std::string& path);

    // Performance metrics
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
        get_all_metrics() const;

private:
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_map_;
    Config config_;
};
```

**REQ-OES-002:** Each OES instance SHALL be **independent:**
- **Separate feature engine:** Each symbol has its own feature engine
- **Separate predictor:** Each symbol has its own EWRLS predictor (3 horizons)
- **Separate learning state:** No shared state across symbols
- **Shared config:** Lambda, thresholds, BB settings shared (unless `independent_thresholds = true`)

**REQ-OES-003:** Memory efficiency:
- **Feature caching:** Pre-compute features for warmup data, cache to disk
- **Shared config objects:** Single config copied by reference
- **Lazy initialization:** Only create OES for symbols in active_symbols list

**REQ-OES-004:** Performance requirements:
- **Latency target:** < 50ms to generate 12 signals (multi-threaded)
- **Memory target:** < 500 MB for all 12 OES instances + feature engines
- **Warmup time:** < 60s to warmup all 12 OES from 7800 bars

### 6.2 Signal Aggregation

**REQ-OES-005:** The system SHALL implement `SignalAggregator`:

```cpp
struct RankedSignal {
    std::string symbol;
    SignalOutput signal;
    double strength;           // probability × confidence
    double predicted_return;   // From EWRLS
    int rank;                  // 1 = strongest, 12 = weakest

    bool operator<(const RankedSignal& other) const {
        return strength > other.strength;  // Descending order
    }
};

class SignalAggregator {
public:
    // Rank all signals by strength
    std::vector<RankedSignal> rank_signals(
        const std::map<std::string, SignalOutput>& signals,
        double min_probability = 0.50,    // Filter threshold
        double min_confidence = 0.30      // Min confidence
    );

    // Get top N strongest signals
    std::vector<RankedSignal> get_top_n(
        const std::vector<RankedSignal>& ranked,
        int n
    );

    // Analytics
    double get_average_strength() const;
    int get_signals_above_threshold(double threshold) const;
};
```

**REQ-OES-006:** Signal strength calculation:
- **Default:** `strength = probability × confidence`
- **Alternative 1:** `strength = predicted_return / predicted_volatility` (Sharpe-like)
- **Alternative 2:** `strength = kelly_fraction`

**REQ-OES-007:** The aggregator SHALL log signal distribution every 100 bars:
```
[SignalAggregator] Bar 1000:
  Top 3 signals: TQQQ (0.85), UVXY (0.72), SPY (0.65)
  Avg strength: 0.54
  Signals > 0.55: 5/12
  Weakest: SQQQ (0.32), SDS (0.38), PSQ (0.41)
```

---

## 7. Rotation Position Manager

### 7.1 Core Requirements

**REQ-ROTATION-001:** The system SHALL implement `RotationPositionManager`:

```cpp
struct ManagedPosition {
    std::string symbol;
    double quantity;
    double entry_price;
    uint64_t entry_bar_id;
    int bars_held;
    double unrealized_pnl;
    double signal_strength;       // Current signal strength
    int current_rank;             // Current rank (1-12)
};

class RotationPositionManager {
public:
    struct Config {
        int max_positions = 3;           // Max simultaneous positions
        int min_hold_bars = 2;           // Min bars before rotation allowed
        double buy_threshold = 0.55;     // Min probability to enter
        double rotation_threshold = 1.2; // Rotate if new signal 20% stronger

        enum class SizingMethod {
            EQUAL_WEIGHT,           // 1/N allocation (Phase 1)
            SIGNAL_STRENGTH,        // Proportional to strength (Phase 2)
            KELLY_CRITERION         // Kelly optimal (Phase 3)
        };
        SizingMethod sizing = EQUAL_WEIGHT;
    };

    explicit RotationPositionManager(const Config& config);

    // Main update loop
    std::vector<TradeOrder> update(
        const std::vector<RankedSignal>& ranked_signals,
        uint64_t current_bar_id,
        double available_capital,
        const std::map<std::string, double>& current_prices
    );

    // Query current state
    const std::vector<ManagedPosition>& get_holdings() const;
    int get_position_count() const;
    double get_cash_utilization() const;
    bool is_holding(const std::string& symbol) const;

private:
    bool should_rotate(const ManagedPosition& pos,
                      const std::vector<RankedSignal>& signals,
                      uint64_t bar_id);

    std::vector<TradeOrder> generate_rotation_orders(
        const std::vector<RankedSignal>& top_signals,
        double capital,
        const std::map<std::string, double>& prices
    );

    double calculate_position_size(const RankedSignal& signal,
                                   double available_capital);

    std::vector<ManagedPosition> holdings_;
    Config config_;
};
```

### 7.2 Rotation Logic

**REQ-ROTATION-002:** The rotation logic SHALL follow these rules:

```cpp
bool RotationPositionManager::should_rotate(
    const ManagedPosition& pos,
    const std::vector<RankedSignal>& new_signals,
    uint64_t current_bar_id
) {
    // Rule 1: Enforce minimum hold period
    int bars_held = current_bar_id - pos.entry_bar_id;
    if (bars_held < config_.min_hold_bars) {
        return false;  // HOLD - min period not satisfied
    }

    // Rule 2: Check if position still in top N
    for (int i = 0; i < config_.max_positions && i < new_signals.size(); ++i) {
        if (new_signals[i].symbol == pos.symbol) {
            // Still in top N - but check if signal is strong enough
            if (new_signals[i].signal.probability < config_.buy_threshold) {
                return true;  // ROTATE - signal too weak now
            }
            return false;  // HOLD - still in top N and strong
        }
    }

    // Rule 3: Not in top N → rotate out
    return true;  // ROTATE - not in top N anymore
}
```

**REQ-ROTATION-003:** Order generation:
1. **Liquidate rotated positions first** (sell orders)
2. **Calculate available capital** (cash + liquidation proceeds)
3. **Buy top N signals** (equal-weight or signal-weighted)
4. **Validate orders** (risk checks, capital constraints)

**REQ-ROTATION-004:** Position sizing (Equal Weight):
```cpp
double RotationPositionManager::calculate_position_size(
    const RankedSignal& signal,
    double available_capital
) {
    if (config_.sizing == SizingMethod::EQUAL_WEIGHT) {
        return available_capital / config_.max_positions;
    }
    // Other methods in Phase 2+
}
```

### 7.3 Risk Management Integration

**REQ-ROTATION-005:** Before executing orders, validate:

| Check | Limit | Action on Violation |
|-------|-------|---------------------|
| **Max position size** | 50% of capital per position | Reduce order size to 50% |
| **Max total exposure** | 100% of capital | Reject order, log error |
| **Min cash reserve** | 5% of capital | Reduce order size to leave 5% cash |
| **Max positions** | Config.max_positions | Reject order, log error |
| **Negative capital** | Capital > 0 | Reject all orders, emergency liquidate |

---

## 8. Backend Simplification

### 8.1 Components to Remove

**REQ-BACKEND-001:** The following components SHALL be **DELETED**:

| Component | File(s) | Lines | Reason |
|-----------|---------|-------|--------|
| **PositionStateMachine** | `position_state_machine.h/cpp` | ~800 | Replaced by RotationPositionManager |
| **EnhancedPositionStateMachine** | `enhanced_position_state_machine.h/cpp` | ~400 | Not needed for rotation |
| **AdaptiveThresholdManager** | `adaptive_trading_mechanism.h/cpp` | ~1200 | Use Optuna offline |
| **QLearningOptimizer** | `adaptive_trading_mechanism.cpp` | ~500 | Use Optuna offline |
| **MultiArmedBandit** | `adaptive_trading_mechanism.cpp` | ~300 | Use Optuna offline |
| **DynamicAllocationManager** | `dynamic_allocation_manager.h/cpp` | ~600 | Equal-weight is sufficient |
| **DynamicHysteresisManager** | `dynamic_hysteresis_manager.h/cpp` | ~400 | Not needed for rotation |
| **Total** | **7 files** | **~4200 lines** | **To be deleted** |

**REQ-BACKEND-002:** Create migration script `tools/archive_old_backend.sh`:
```bash
#!/bin/bash
# Archive old backend components before deletion

mkdir -p archive/v1_backend
mv include/backend/position_state_machine.* archive/v1_backend/
mv include/backend/enhanced_position_state_machine.* archive/v1_backend/
mv include/backend/adaptive_trading_mechanism.* archive/v1_backend/
mv include/backend/dynamic_*.* archive/v1_backend/
mv src/backend/position_state_machine.* archive/v1_backend/
mv src/backend/adaptive_*.* archive/v1_backend/
mv src/backend/dynamic_*.* archive/v1_backend/

echo "Old backend archived to archive/v1_backend/"
echo "Review and delete after validating v2.0"
```

### 8.2 New Backend Structure

**REQ-BACKEND-003:** New backend SHALL consist of:

```
backend/
├── rotation_position_manager.h         (NEW - 300 lines)
├── rotation_position_manager.cpp       (NEW)
├── simple_risk_manager.h               (NEW - 150 lines)
├── simple_risk_manager.cpp             (NEW)
├── order_generator.h                   (SIMPLIFIED - 200 lines)
├── order_generator.cpp                 (SIMPLIFIED)
└── backend_component.h/cpp             (REFACTORED - remove PSM deps)

Total: ~650 lines (vs 4200 in v1.x)
```

---

## 9. Configuration Management

### 9.1 Configuration Files

**REQ-CONFIG-001:** The system SHALL use the following configuration files:

| File | Purpose | Format |
|------|---------|--------|
| `config/symbols.json` | Active symbols, metadata | JSON |
| `config/rotation_params.json` | Rotation parameters (Optuna-optimized) | JSON |
| `config/oes_config.json` | OES shared config | JSON |
| `config.env` | API keys, secrets | Shell env |

**REQ-CONFIG-002:** `config/symbols.json` structure:
```json
{
  "active_symbols": [
    "SVIX", "SVXY", "TQQQ", "UPRO", "QQQ", "SPY",
    "SH", "PSQ", "SDS", "SQQQ", "UVXY", "UVIX"
  ],
  "symbol_metadata": {
    "TQQQ": {
      "name": "ProShares UltraPro QQQ",
      "leverage": 3.0,
      "direction": "long",
      "underlying": "QQQ",
      "expense_ratio": 0.0086,
      "min_price": 10.0,
      "max_price": 200.0
    },
    "SQQQ": {
      "name": "ProShares UltraPro Short QQQ",
      "leverage": -3.0,
      "direction": "short",
      "underlying": "QQQ",
      "expense_ratio": 0.0095,
      "min_price": 5.0,
      "max_price": 100.0
    }
  }
}
```

**REQ-CONFIG-003:** `config/rotation_params.json` (Optuna-optimized):
```json
{
  "version": "2.0",
  "optimized_date": "2025-10-15",
  "optimization_blocks": 100,
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
    "sell_threshold": 0.443,
    "bb_amplification": true,
    "enable_regime_detection": false
  },
  "performance": {
    "mrd_train": 0.52,
    "mrd_validation": 0.47,
    "sharpe_train": 2.1,
    "sharpe_validation": 1.8
  }
}
```

### 9.2 Runtime Configuration Updates

**REQ-CONFIG-004:** The system SHALL support hot-reload of configuration:
- **SIGUSR1 signal:** Reload rotation parameters without restart
- **Validation:** New params must pass sanity checks before applying
- **Logging:** Log old vs new params, timestamp of reload

---

## 10. Testing Strategy

### 10.1 Unit Tests

**REQ-TEST-001:** The following components SHALL have unit tests (>80% coverage):

| Component | Test File | Coverage Target |
|-----------|-----------|-----------------|
| `MultiSymbolDataManager` | `test_multi_symbol_data.cpp` | 90% |
| `MultiSymbolOESManager` | `test_multi_oes.cpp` | 85% |
| `SignalAggregator` | `test_signal_aggregator.cpp` | 90% |
| `RotationPositionManager` | `test_rotation_manager.cpp` | 95% |
| `SimpleRiskManager` | `test_risk_manager.cpp` | 100% |

**REQ-TEST-002:** Critical test scenarios:

1. **Multi-Symbol Synchronization:**
   - Test: 12 symbols arrive out of order → correct synchronization
   - Test: 2 symbols missing → forward fill + log warning
   - Test: All symbols arrive simultaneously → immediate assembly

2. **Rotation Logic:**
   - Test: Top 3 signals unchanged → HOLD all positions
   - Test: Signal #4 becomes #1 → ROTATE position #3 out, buy #4
   - Test: All signals below buy_threshold → LIQUIDATE all
   - Test: Min hold = 2 bars, position held 1 bar → HOLD (no rotation)

3. **Risk Management:**
   - Test: Order exceeds 50% capital → reduced to 50%
   - Test: Total exposure would exceed 100% → reject order
   - Test: Negative capital → emergency liquidate all

### 10.2 Integration Tests

**REQ-TEST-003:** Integration tests SHALL cover:

1. **End-to-end mock trading:**
   ```bash
   ./build/sentio_cli live-trade \
       --mock \
       --mock-date 2025-10-07 \
       --mock-speed 0 \
       --config config/rotation_params.json
   ```
   - Verify: All 12 symbols loaded
   - Verify: All 12 OES instances initialized
   - Verify: Signals generated every bar
   - Verify: Rotation logic executes correctly
   - Verify: Final MRD > 0.2%

2. **Warmup pipeline:**
   ```bash
   ./scripts/comprehensive_warmup_multi.sh
   ```
   - Verify: All 12 symbols downloaded
   - Verify: Warmup data extracted (7800 bars × 12 symbols)
   - Verify: All 12 OES instances warmed up
   - Verify: State saved to disk

3. **Live trading (paper):**
   ```bash
   ./build/sentio_cli live-trade \
       --config config/rotation_params.json
   ```
   - Verify: Alpaca WebSocket connects
   - Verify: All 12 symbols subscribed
   - Verify: Bars synchronized correctly
   - Verify: Orders executed via Alpaca API
   - Verify: Positions tracked accurately

### 10.3 Performance Tests

**REQ-TEST-004:** Performance benchmarks:

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Signal generation latency** | < 50ms for 12 signals | `time_signal_generation()` |
| **Rotation decision latency** | < 10ms | `time_rotation_logic()` |
| **Memory usage** | < 500 MB | `measure_memory_footprint()` |
| **Warmup time** | < 60s (7800 bars) | `time_warmup_pipeline()` |
| **Bar processing rate** | > 100 bars/sec (mock) | `bars_processed / elapsed_time` |

---

## 11. Implementation Phases

### Phase 1: Data Infrastructure (Week 1)

**Deliverables:**
1. ✅ `MultiSymbolDataManager` - Synchronization logic
2. ✅ `AlpacaMultiSymbolFeed` - WebSocket integration for 12 symbols
3. ✅ `MockMultiSymbolFeed` - Historical data replay for 12 symbols
4. ✅ `MultiSymbolSessionExtractor` - Extract session data for all symbols
5. ✅ `tools/download_all_symbols.sh` - Download script for all 12 symbols
6. ✅ Unit tests for data layer

**Acceptance Criteria:**
- [ ] All 12 symbols download successfully from Polygon
- [ ] Mock replay works for 100-block test
- [ ] Alpaca WebSocket receives real-time bars for all 12 symbols
- [ ] Synchronization handles missing/late data correctly

### Phase 2: Multi-Symbol OES (Week 2)

**Deliverables:**
1. ✅ `MultiSymbolOESManager` - Manage 12 OES instances
2. ✅ `SignalAggregator` - Rank signals by strength
3. ✅ `scripts/comprehensive_warmup_multi.sh` - Warmup script
4. ✅ Warmup state persistence
5. ✅ Unit tests for OES layer

**Acceptance Criteria:**
- [ ] All 12 OES instances initialize successfully
- [ ] Warmup completes in < 60s for 7800 bars
- [ ] Signals generated every bar with correct ranking
- [ ] Top 3 signals identified correctly

### Phase 3: Rotation Backend (Week 2-3)

**Deliverables:**
1. ✅ `RotationPositionManager` - Rotation logic
2. ✅ `SimpleRiskManager` - Risk validation
3. ✅ `OrderGenerator` refactor - Remove PSM dependencies
4. ✅ Archive old backend components
5. ✅ Integration tests

**Acceptance Criteria:**
- [ ] Rotation logic passes all unit tests
- [ ] Mock trading generates rotation orders correctly
- [ ] Risk checks prevent invalid orders
- [ ] Old backend archived (not deleted yet)

### Phase 4: Backtesting & Optimization (Week 3)

**Deliverables:**
1. ✅ Backtest on 100-block dataset
2. ✅ Optuna optimization script for rotation params
3. ✅ Walk-forward validation
4. ✅ Performance report

**Acceptance Criteria:**
- [ ] MRD > +0.2% on 100-block test
- [ ] Optuna finds optimal params (200 trials)
- [ ] Out-of-sample validation MRD > +0.15%
- [ ] Sharpe ratio > 1.5

### Phase 5: Paper Trading (Week 4)

**Deliverables:**
1. ✅ Deploy to Alpaca paper account
2. ✅ Live monitoring dashboard
3. ✅ Daily performance reports
4. ✅ Email alerts for errors

**Acceptance Criteria:**
- [ ] Paper trading runs for 5 consecutive days
- [ ] No crashes or disconnects
- [ ] Actual MRD > +0.2%
- [ ] Orders execute correctly (no rejections)

### Phase 6: Production Deployment (Week 5+)

**Deliverables:**
1. ✅ Risk review and approval
2. ✅ Production deployment checklist
3. ✅ Monitoring and alerting setup
4. ✅ Rollback procedure

**Acceptance Criteria:**
- [ ] Paper trading MRD > +0.3% for 10 days
- [ ] All stakeholders approve production deployment
- [ ] Monitoring dashboard live
- [ ] Capital allocation: Start with $10K

---

## 12. Success Metrics

### 12.1 Performance Metrics

**REQ-METRICS-001:** The system SHALL track:

| Metric | Target (Phase 4) | Target (Production) | Current v1.x |
|--------|------------------|---------------------|--------------|
| **MRD (Mean Return per Day)** | +0.3% | +0.5% | +0.046% |
| **Sharpe Ratio** | 1.5 | 2.0 | 0.8 |
| **Max Drawdown** | < 5% | < 3% | 8% |
| **Win Rate** | 55% | 60% | 48% |
| **Rotation Frequency** | 50-100 rotations/day | 50-100 | N/A |
| **Avg Hold Period** | 3-10 bars | 3-10 bars | N/A |

### 12.2 Operational Metrics

**REQ-METRICS-002:** The system SHALL monitor:

| Metric | SLA | Alert Threshold |
|--------|-----|-----------------|
| **Uptime** | 99.5% during market hours | < 99% |
| **Data latency** | < 5s (WebSocket) | > 10s |
| **Order execution latency** | < 2s | > 5s |
| **Signal generation latency** | < 50ms | > 100ms |
| **WebSocket disconnects** | < 1/day | > 2/day |
| **Order rejections** | < 1% | > 5% |

### 12.3 Quality Metrics

**REQ-METRICS-003:** Code quality targets:

| Metric | Target |
|--------|--------|
| **Test coverage** | > 80% |
| **Lines of code (backend)** | < 1000 (vs 3200 in v1.x) |
| **Cyclomatic complexity** | < 10 per function |
| **Build time** | < 30s (clean build) |
| **Memory leaks** | 0 (valgrind clean) |

---

## 13. Risks and Mitigations

### 13.1 Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Data synchronization failures** | Medium | High | Forward-fill missing data, log warnings, skip bar if >5 symbols missing |
| **Alpaca API rate limits** | Low | Medium | Use WebSocket (unlimited), fall back to Polygon REST |
| **WebSocket disconnects** | Medium | High | Auto-reconnect (5 retries), save state before disconnect |
| **OES overfitting** | High | High | Walk-forward validation, regime testing, Optuna regularization |
| **Rotation overtrading** | Medium | Medium | Enforce min_hold_bars, optimize rotation_threshold |
| **Memory leaks (12 OES instances)** | Low | High | Valgrind testing, smart pointer discipline |

### 13.2 Market Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Flash crash** | Low | Critical | Circuit breaker (5% drawdown), emergency liquidate |
| **Correlated crashes (all 12 symbols)** | Low | High | Inverse ETFs (SH, SDS) rise, go to cash if all signals weak |
| **Leveraged ETF decay** | High | Medium | Rotate out quickly (min_hold=2 bars), monitor decay |
| **Low volatility (all signals weak)** | Medium | Low | Go to cash (0 positions), wait for strong signals |
| **High volatility (rapid rotations)** | Medium | Medium | Increase min_hold_bars in high vol regime |

### 13.3 Operational Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| **Insufficient warmup data** | Medium | High | Require 20 blocks minimum, else refuse to trade |
| **Stale parameters** | Medium | Medium | Re-optimize weekly, flag if params > 7 days old |
| **Monitoring gaps** | Low | High | Automated health checks every 5 min, email alerts |
| **Human error (config)** | Medium | High | Config validation on load, git version control |

---

## Appendices

### Appendix A: File Structure

```
online_trader/
├── config/
│   ├── symbols.json                    (NEW - 12 symbols)
│   ├── rotation_params.json            (NEW - Optuna-optimized)
│   ├── oes_config.json                 (NEW - OES shared config)
│   └── best_params.json                (DEPRECATED - remove)
│
├── include/
│   ├── data/
│   │   ├── multi_symbol_data_manager.h (NEW)
│   │   ├── alpaca_multi_feed.h         (NEW)
│   │   └── mock_multi_feed.h           (NEW)
│   │
│   ├── strategy/
│   │   ├── multi_symbol_oes_manager.h  (NEW)
│   │   ├── signal_aggregator.h         (NEW)
│   │   └── online_ensemble_strategy.h  (EXISTING)
│   │
│   └── backend/
│       ├── rotation_position_manager.h (NEW)
│       ├── simple_risk_manager.h       (NEW)
│       ├── position_state_machine.h    (ARCHIVE)
│       ├── adaptive_trading_*.h        (ARCHIVE)
│       └── dynamic_*.h                 (ARCHIVE)
│
├── src/
│   ├── data/
│   │   ├── multi_symbol_data_manager.cpp
│   │   ├── alpaca_multi_feed.cpp
│   │   └── mock_multi_feed.cpp
│   │
│   ├── strategy/
│   │   ├── multi_symbol_oes_manager.cpp
│   │   └── signal_aggregator.cpp
│   │
│   └── backend/
│       ├── rotation_position_manager.cpp
│       ├── simple_risk_manager.cpp
│       └── (archived files)
│
├── scripts/
│   ├── comprehensive_warmup_multi.sh   (NEW)
│   ├── download_all_symbols.sh         (NEW)
│   └── launch_trading.sh               (MODIFIED - support rotation)
│
├── tools/
│   ├── data_downloader.py              (EXISTING)
│   ├── extract_warmup_multi.py         (NEW)
│   ├── generate_multi_symbol_data.py   (NEW)
│   ├── optuna_rotation_optimizer.py    (NEW)
│   └── archive_old_backend.sh          (NEW)
│
├── tests/
│   ├── test_multi_symbol_data.cpp      (NEW)
│   ├── test_multi_oes.cpp              (NEW)
│   ├── test_signal_aggregator.cpp      (NEW)
│   ├── test_rotation_manager.cpp       (NEW)
│   └── test_risk_manager.cpp           (NEW)
│
├── data/
│   ├── equities/
│   │   ├── SPY_RTH_NH.csv              (EXISTING)
│   │   ├── QQQ_RTH_NH.csv              (NEW)
│   │   ├── TQQQ_RTH_NH.csv             (NEW)
│   │   ├── ...                         (9 more symbols)
│   │   └── warmup_latest.csv           (NEW - all 12 symbols)
│   │
│   └── tmp/
│       └── session_20251007/           (NEW - per-date session data)
│           ├── SPY.csv
│           ├── QQQ.csv
│           └── ...
│
├── archive/
│   └── v1_backend/                     (NEW - archived PSM code)
│       ├── position_state_machine.*
│       ├── adaptive_trading_mechanism.*
│       └── ...
│
└── docs/
    ├── REQUIREMENTS_MULTI_SYMBOL_ROTATION_v2.md  (THIS FILE)
    ├── MULTI_SYMBOL_ROTATION_ARCHITECTURE.md
    └── ROTATION_BACKEND_SIMPLIFICATION.md
```

### Appendix B: Data Format Specifications

**CSV Format (All Symbols):**
```csv
timestamp,open,high,low,close,volume
1696464600000,432.15,432.89,431.98,432.56,12345678
```

**JSONL Format (Signals):**
```jsonl
{"symbol":"TQQQ","timestamp_ms":1696464600000,"probability":0.78,"confidence":0.65,"signal":"LONG","strength":0.507}
{"symbol":"SPY","timestamp_ms":1696464600000,"probability":0.62,"confidence":0.55,"signal":"LONG","strength":0.341}
```

**Binary Format (Warmup State):**
```protobuf
message WarmupState {
    map<string, OESState> oes_states = 1;  // Symbol → OES state
    int64 last_bar_timestamp = 2;
    int32 bars_processed = 3;
}

message OESState {
    repeated double feature_engine_state = 1;  // Rolling window buffers
    repeated double ewrls_weights = 2;         // Predictor weights
    PerformanceMetrics metrics = 3;
}
```

### Appendix C: API Endpoints

**Alpaca WebSocket v2 (IEX):**
```
wss://stream.data.alpaca.markets/v2/iex

Subscribe message:
{
  "action": "subscribe",
  "bars": ["SVIX", "SVXY", "TQQQ", "UPRO", "QQQ", "SPY",
           "SH", "PSQ", "SDS", "SQQQ", "UVXY", "UVIX"]
}

Bar message:
{
  "T": "b",
  "S": "TQQQ",
  "o": 43.21,
  "h": 43.45,
  "l": 43.15,
  "c": 43.32,
  "v": 1234567,
  "t": "2025-10-15T14:32:00Z",
  "n": 123,
  "vw": 43.28
}
```

**Polygon REST API (Fallback):**
```
GET https://api.polygon.io/v2/aggs/ticker/{symbol}/range/1/minute/{from}/{to}

Example:
GET https://api.polygon.io/v2/aggs/ticker/TQQQ/range/1/minute/2025-10-15/2025-10-15
```

---

## Approval Sign-Off

| Role | Name | Signature | Date |
|------|------|-----------|------|
| **Project Lead** | | | |
| **Lead Developer** | | | |
| **QA Lead** | | | |
| **Risk Manager** | | | |

---

**Document Version History:**

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-10-15 | Architecture Team | Initial draft |
| 2.0 | TBD | | Post-review updates |

---

**END OF REQUIREMENTS DOCUMENT**
