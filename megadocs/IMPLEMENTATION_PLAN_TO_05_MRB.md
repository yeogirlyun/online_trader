# Implementation Plan: Path to 0.5% MRB

**Date**: 2025-10-08
**Current MRB**: 0.22% (Phase 1, 4 blocks)
**Target MRB**: 0.5%
**Gap**: +0.28% (+127% improvement needed)

---

## Quick Reference: Priority Order

| Priority | Action | Effort | Impact | Expected MRB | Timeline |
|----------|--------|--------|--------|--------------|----------|
| 🥇 **P1** | Enable adaptive thresholds | **1 line** | ⭐⭐⭐ High | 0.30-0.35% | **Today** |
| 🥈 **P2** | Increase param ranges | **5 lines** | ⭐⭐ Medium | 0.32-0.38% | **Today** |
| 🥉 **P3** | Stratified optimization | **50 lines** | ⭐⭐ Medium | 0.35-0.40% | Week 1 |
| 🎯 **P4** | Regime detection | **300 lines** | ⭐⭐⭐ High | **0.50-0.55%** ✅ | Week 2-3 |

---

## Phase 1: Quick Wins (Today - Immediate Results)

### 1.1 Enable Adaptive Threshold Calibration

**Current State**: `enable_threshold_calibration = false` (line 146 in `generate_signals_command.cpp`)

**Change Required**:
```cpp
// src/cli/generate_signals_command.cpp:146
// OLD:
config.enable_threshold_calibration = false;  // Disable auto-calibration for Optuna

// NEW:
config.enable_threshold_calibration = true;   // Enable adaptive thresholds
```

**Why This Works**:
- Thresholds adapt based on recent win rate
- Automatically tightens when losing, loosens when winning
- Already implemented in codebase, just disabled for Optuna!

**Implementation**:
```bash
# Single line change
sed -i 's/enable_threshold_calibration = false/enable_threshold_calibration = true/' \
    src/cli/generate_signals_command.cpp

# Rebuild
cd build && make -j8

# Test immediately with Phase 1 best params
./sentio_cli generate-signals \
    --data ../data/equities/SPY_20blocks.csv \
    --output /tmp/test_adaptive.jsonl \
    --warmup 780 \
    --buy-threshold 0.55 \
    --sell-threshold 0.43 \
    --lambda 0.992 \
    --bb-amp 0.08

./sentio_cli execute-trades \
    --signals /tmp/test_adaptive.jsonl \
    --data ../data/equities/SPY_20blocks.csv \
    --output /tmp/test_adaptive_trades.jsonl \
    --warmup 780

./sentio_cli analyze-trades \
    --trades /tmp/test_adaptive_trades.jsonl \
    --data ../data/equities/SPY_20blocks.csv \
    --output /tmp/test_adaptive_analysis.json
```

**Expected Result**: MRB improves from 0.104% (20 blocks) → 0.20-0.25%

---

### 1.2 Increase Parameter Search Ranges

**Current Ranges** (conservative):
```python
# tools/adaptive_optuna.py:371-375
'buy_threshold': trial.suggest_float('buy_threshold', 0.51, 0.60, step=0.01),
'sell_threshold': trial.suggest_float('sell_threshold', 0.40, 0.49, step=0.01),
'bb_amplification_factor': trial.suggest_float('bb_amplification_factor', 0.05, 0.15, step=0.01)
```

**Proposed Ranges** (more aggressive):
```python
# tools/adaptive_optuna.py:371-375
'buy_threshold': trial.suggest_float('buy_threshold', 0.51, 0.70, step=0.01),  # Max 0.70 (was 0.60)
'sell_threshold': trial.suggest_float('sell_threshold', 0.30, 0.49, step=0.01),  # Min 0.30 (was 0.40)
'bb_amplification_factor': trial.suggest_float('bb_amplification_factor', 0.05, 0.30, step=0.01)  # Max 0.30 (was 0.15)
```

**Rationale**:
- Current ranges may be too conservative
- More aggressive strategies might capture larger moves
- BB amplification up to 0.30 allows stronger signal boost

**Implementation**:
```bash
# Edit tools/adaptive_optuna.py lines 371-375
# Then re-run Phase 1 optimization

python3 tools/adaptive_optuna.py \
    --strategy C \
    --data data/equities/SPY_4blocks.csv \
    --build-dir build \
    --output data/tmp/aggressive_phase1_results.json \
    --n-trials 100 \  # More trials for larger space
    --n-jobs 4
```

**Expected Result**: Find more aggressive params, MRB 0.22% → 0.25-0.30%

---

## Phase 2: Stratified Optimization (Week 1)

### 2.1 Implement Stratified Block Sampling

**Problem**: Optimizing on sequential 4 blocks (0-3) captures single regime

**Solution**: Sample blocks from different time periods

**Code** (`tools/adaptive_optuna.py`):
```python
class AdaptiveOptunaFramework:
    def stratified_block_sampling(self, total_blocks=20, sample_size=4):
        """
        Sample blocks across time periods instead of sequential.

        Example: Instead of blocks [0,1,2,3], sample [0,5,10,15]
        This captures multiple market regimes.
        """
        if total_blocks < sample_size:
            return list(range(total_blocks))

        # Evenly spaced sampling
        step = total_blocks // sample_size
        sampled = [i * step for i in range(sample_size)]

        # Add randomness to avoid bias
        import random
        sampled = [min(b + random.randint(-1, 1), total_blocks-1) for b in sampled]

        return sorted(sampled)

    def create_stratified_data(self, blocks_to_sample):
        """Create training data from sampled blocks"""
        all_bars = []
        for block_idx in blocks_to_sample:
            start_bar = block_idx * self.bars_per_block
            end_bar = (block_idx + 1) * self.bars_per_block
            block_bars = self.all_bars[start_bar:end_bar]
            all_bars.extend(block_bars)

        # Save to temp file
        temp_file = os.path.join(self.output_dir, 'stratified_train_data.csv')
        self.save_bars_to_csv(all_bars, temp_file)
        return temp_file
```

**Usage**:
```python
# In tune_on_window():
if use_stratified:
    sampled_blocks = self.stratified_block_sampling(total_blocks=20, sample_size=4)
    print(f"Stratified sampling: blocks {sampled_blocks}")
    train_data = self.create_stratified_data(sampled_blocks)
else:
    # Original sequential sampling
    train_data = self.create_block_data(block_start, block_end, train_data)
```

**Expected Result**: Params generalize better, MRB on 20 blocks: 0.104% → 0.15-0.20%

---

## Phase 3: Regime Detection (Week 2-3) 🎯 **KEY TO 0.5% TARGET**

### 3.1 Create Regime Detector

**New files to create**:
- `include/strategy/market_regime_detector.h`
- `src/strategy/market_regime_detector.cpp`

**Header** (`include/strategy/market_regime_detector.h`):
```cpp
#pragma once

#include <vector>
#include <string>
#include "data/bar.h"

namespace strategy {

enum class MarketRegime {
    TRENDING_UP,
    TRENDING_DOWN,
    CHOPPY,
    HIGH_VOLATILITY,
    LOW_VOLATILITY
};

std::string regime_to_string(MarketRegime regime);

class MarketRegimeDetector {
public:
    struct RegimeIndicators {
        double trend_strength;      // ADX (Average Directional Index)
        double volatility_ratio;    // Current ATR / Historical ATR
        double price_momentum;      // Price relative to moving averages
        double volume_trend;        // Volume relative to average
        double chopiness_index;     // Choppiness indicator
    };

    MarketRegimeDetector() = default;

    // Main regime detection
    MarketRegime detect_regime(const std::vector<Bar>& recent_bars);

    // Get detailed indicators
    RegimeIndicators calculate_indicators(const std::vector<Bar>& bars);

    // Configuration
    void set_trend_threshold(double threshold) { trend_threshold_ = threshold; }
    void set_volatility_threshold(double threshold) { volatility_threshold_ = threshold; }

private:
    // Technical indicators
    double calculate_adx(const std::vector<Bar>& bars, int period = 14);
    double calculate_atr(const std::vector<Bar>& bars, int period = 14);
    double calculate_sma(const std::vector<Bar>& bars, int period);
    double calculate_chopiness(const std::vector<Bar>& bars, int period = 14);

    // Thresholds
    double trend_threshold_ = 25.0;      // ADX > 25 = trending
    double volatility_threshold_ = 1.5;  // ATR ratio > 1.5 = high vol
    double chopiness_threshold_ = 61.8;  // Chopiness > 61.8 = choppy

    static constexpr size_t MIN_BARS = 100;
};

} // namespace strategy
```

**Implementation** (`src/strategy/market_regime_detector.cpp`):
```cpp
#include "strategy/market_regime_detector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace strategy {

std::string regime_to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::TRENDING_UP: return "TRENDING_UP";
        case MarketRegime::TRENDING_DOWN: return "TRENDING_DOWN";
        case MarketRegime::CHOPPY: return "CHOPPY";
        case MarketRegime::HIGH_VOLATILITY: return "HIGH_VOLATILITY";
        case MarketRegime::LOW_VOLATILITY: return "LOW_VOLATILITY";
        default: return "UNKNOWN";
    }
}

MarketRegime MarketRegimeDetector::detect_regime(const std::vector<Bar>& recent_bars) {
    if (recent_bars.size() < MIN_BARS) {
        return MarketRegime::LOW_VOLATILITY;  // Safe default
    }

    auto indicators = calculate_indicators(recent_bars);

    // Priority 1: High volatility regime
    if (indicators.volatility_ratio > volatility_threshold_) {
        return MarketRegime::HIGH_VOLATILITY;
    }

    // Priority 2: Choppy regime
    if (indicators.chopiness_index > chopiness_threshold_) {
        return MarketRegime::CHOPPY;
    }

    // Priority 3: Trending regimes
    if (indicators.trend_strength > trend_threshold_) {
        // Determine direction using price momentum
        if (indicators.price_momentum > 0) {
            return MarketRegime::TRENDING_UP;
        } else {
            return MarketRegime::TRENDING_DOWN;
        }
    }

    // Default: Low volatility
    return MarketRegime::LOW_VOLATILITY;
}

MarketRegimeDetector::RegimeIndicators MarketRegimeDetector::calculate_indicators(
    const std::vector<Bar>& bars) {

    RegimeIndicators indicators;

    // Calculate ADX for trend strength
    indicators.trend_strength = calculate_adx(bars, 14);

    // Calculate volatility ratio (current vs historical)
    double atr_20 = calculate_atr(bars, 20);
    double atr_100 = calculate_atr(bars, 100);
    indicators.volatility_ratio = (atr_100 > 0) ? (atr_20 / atr_100) : 1.0;

    // Calculate price momentum (current price vs SMA)
    double sma_50 = calculate_sma(bars, 50);
    double current_price = bars.back().close;
    indicators.price_momentum = (current_price - sma_50) / sma_50;

    // Calculate volume trend
    double recent_volume = 0;
    double historical_volume = 0;
    int recent_window = std::min(20, (int)bars.size());
    int hist_window = std::min(100, (int)bars.size());

    for (int i = bars.size() - recent_window; i < bars.size(); ++i) {
        recent_volume += bars[i].volume;
    }
    for (int i = bars.size() - hist_window; i < bars.size(); ++i) {
        historical_volume += bars[i].volume;
    }

    indicators.volume_trend = (historical_volume > 0) ?
        (recent_volume / recent_window) / (historical_volume / hist_window) : 1.0;

    // Calculate choppiness index
    indicators.chopiness_index = calculate_chopiness(bars, 14);

    return indicators;
}

double MarketRegimeDetector::calculate_adx(const std::vector<Bar>& bars, int period) {
    if (bars.size() < period + 1) return 0.0;

    // Simplified ADX calculation
    // Full implementation would use +DI, -DI, and ATR

    std::vector<double> true_ranges;
    std::vector<double> plus_dm;
    std::vector<double> minus_dm;

    for (size_t i = 1; i < bars.size(); ++i) {
        double high_diff = bars[i].high - bars[i-1].high;
        double low_diff = bars[i-1].low - bars[i].low;

        double tr = std::max({
            bars[i].high - bars[i].low,
            std::abs(bars[i].high - bars[i-1].close),
            std::abs(bars[i].low - bars[i-1].close)
        });

        true_ranges.push_back(tr);
        plus_dm.push_back((high_diff > low_diff && high_diff > 0) ? high_diff : 0);
        minus_dm.push_back((low_diff > high_diff && low_diff > 0) ? low_diff : 0);
    }

    // Calculate smoothed averages
    double atr = std::accumulate(
        true_ranges.end() - period, true_ranges.end(), 0.0) / period;
    double plus_di = std::accumulate(
        plus_dm.end() - period, plus_dm.end(), 0.0) / period;
    double minus_di = std::accumulate(
        minus_dm.end() - period, minus_dm.end(), 0.0) / period;

    if (atr == 0) return 0.0;

    double di_diff = std::abs(plus_di - minus_di);
    double di_sum = plus_di + minus_di;

    double dx = (di_sum > 0) ? (di_diff / di_sum) * 100.0 : 0.0;

    return dx;  // Simplified - full ADX would smooth this further
}

double MarketRegimeDetector::calculate_atr(const std::vector<Bar>& bars, int period) {
    if (bars.size() < period + 1) return 0.0;

    double sum_tr = 0.0;
    for (size_t i = bars.size() - period; i < bars.size(); ++i) {
        double tr = std::max({
            bars[i].high - bars[i].low,
            std::abs(bars[i].high - bars[i-1].close),
            std::abs(bars[i].low - bars[i-1].close)
        });
        sum_tr += tr;
    }

    return sum_tr / period;
}

double MarketRegimeDetector::calculate_sma(const std::vector<Bar>& bars, int period) {
    if (bars.size() < period) return 0.0;

    double sum = 0.0;
    for (size_t i = bars.size() - period; i < bars.size(); ++i) {
        sum += bars[i].close;
    }

    return sum / period;
}

double MarketRegimeDetector::calculate_chopiness(const std::vector<Bar>& bars, int period) {
    if (bars.size() < period + 1) return 0.0;

    // Choppiness Index = 100 * LOG10(Sum(ATR) / (High-Low)) / LOG10(period)

    double sum_atr = 0.0;
    double highest = bars[bars.size() - period].high;
    double lowest = bars[bars.size() - period].low;

    for (size_t i = bars.size() - period; i < bars.size(); ++i) {
        double tr = std::max({
            bars[i].high - bars[i].low,
            (i > 0) ? std::abs(bars[i].high - bars[i-1].close) : 0.0,
            (i > 0) ? std::abs(bars[i].low - bars[i-1].close) : 0.0
        });
        sum_atr += tr;

        highest = std::max(highest, bars[i].high);
        lowest = std::min(lowest, bars[i].low);
    }

    double range = highest - lowest;
    if (range == 0) return 100.0;  // Max choppiness

    double chopiness = 100.0 * std::log10(sum_atr / range) / std::log10(period);

    return std::clamp(chopiness, 0.0, 100.0);
}

} // namespace strategy
```

---

### 3.2 Create Regime Parameter Manager

**Header** (`include/strategy/regime_parameter_manager.h`):
```cpp
#pragma once

#include <map>
#include <string>
#include "strategy/market_regime_detector.h"

namespace strategy {

struct RegimeParams {
    // Phase 1 params
    double buy_threshold;
    double sell_threshold;
    double ewrls_lambda;
    double bb_amplification_factor;

    // Phase 2 params
    double h1_weight;
    double h5_weight;
    double h10_weight;
    double bb_period;
    double bb_std_dev;
    double bb_proximity;
    double regularization;

    // Validation
    bool is_valid() const {
        return (buy_threshold > sell_threshold) &&
               (buy_threshold >= 0.5 && buy_threshold <= 0.9) &&
               (sell_threshold >= 0.1 && sell_threshold <= 0.5) &&
               (std::abs(h1_weight + h5_weight + h10_weight - 1.0) < 0.01);
    }
};

class RegimeParameterManager {
public:
    RegimeParameterManager();

    // Get parameters for regime
    RegimeParams get_params_for_regime(MarketRegime regime) const;

    // Load optimized params from file
    void load_from_file(const std::string& filename);

    // Save params to file
    void save_to_file(const std::string& filename) const;

    // Update params for a regime
    void set_params_for_regime(MarketRegime regime, const RegimeParams& params);

private:
    std::map<MarketRegime, RegimeParams> regime_params_;

    // Initialize with defaults
    void initialize_defaults();
};

} // namespace strategy
```

**Implementation** (`src/strategy/regime_parameter_manager.cpp`):
```cpp
#include "strategy/regime_parameter_manager.h"
#include <fstream>
#include <iostream>

// JSON library (if available) or simple custom parser

namespace strategy {

RegimeParameterManager::RegimeParameterManager() {
    initialize_defaults();
}

void RegimeParameterManager::initialize_defaults() {
    // TRENDING_UP: Wide gap, moderate forgetting, high BB amp
    regime_params_[MarketRegime::TRENDING_UP] = {
        .buy_threshold = 0.55,
        .sell_threshold = 0.43,
        .ewrls_lambda = 0.992,
        .bb_amplification_factor = 0.08,
        .h1_weight = 0.15,
        .h5_weight = 0.60,
        .h10_weight = 0.25,
        .bb_period = 20,
        .bb_std_dev = 2.25,
        .bb_proximity = 0.30,
        .regularization = 0.016
    };

    // CHOPPY: Narrow gap, slow forgetting, low BB amp
    regime_params_[MarketRegime::CHOPPY] = {
        .buy_threshold = 0.52,
        .sell_threshold = 0.48,
        .ewrls_lambda = 0.995,
        .bb_amplification_factor = 0.05,
        .h1_weight = 0.40,
        .h5_weight = 0.40,
        .h10_weight = 0.20,
        .bb_period = 25,
        .bb_std_dev = 2.0,
        .bb_proximity = 0.35,
        .regularization = 0.025
    };

    // HIGH_VOLATILITY: Very wide gap, fast forgetting, high BB amp
    regime_params_[MarketRegime::HIGH_VOLATILITY] = {
        .buy_threshold = 0.58,
        .sell_threshold = 0.40,
        .ewrls_lambda = 0.990,
        .bb_amplification_factor = 0.12,
        .h1_weight = 0.50,
        .h5_weight = 0.30,
        .h10_weight = 0.20,
        .bb_period = 15,
        .bb_std_dev = 2.5,
        .bb_proximity = 0.25,
        .regularization = 0.010
    };

    // LOW_VOLATILITY: Tight thresholds, slow forgetting
    regime_params_[MarketRegime::LOW_VOLATILITY] = {
        .buy_threshold = 0.53,
        .sell_threshold = 0.47,
        .ewrls_lambda = 0.998,
        .bb_amplification_factor = 0.06,
        .h1_weight = 0.20,
        .h5_weight = 0.50,
        .h10_weight = 0.30,
        .bb_period = 30,
        .bb_std_dev = 2.0,
        .bb_proximity = 0.40,
        .regularization = 0.020
    };

    // TRENDING_DOWN: Similar to TRENDING_UP but optimize for short side
    regime_params_[MarketRegime::TRENDING_DOWN] = regime_params_[MarketRegime::TRENDING_UP];
}

RegimeParams RegimeParameterManager::get_params_for_regime(MarketRegime regime) const {
    auto it = regime_params_.find(regime);
    if (it != regime_params_.end()) {
        return it->second;
    }

    // Fallback to LOW_VOLATILITY (safest)
    return regime_params_.at(MarketRegime::LOW_VOLATILITY);
}

// TODO: Implement JSON save/load functions

} // namespace strategy
```

---

### 3.3 Integrate into OnlineEnsembleStrategy

**Modify** `include/strategy/online_ensemble_strategy.h`:
```cpp
#include "strategy/market_regime_detector.h"
#include "strategy/regime_parameter_manager.h"

class OnlineEnsembleStrategy : public Strategy {
private:
    // Add new members
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<RegimeParameterManager> param_manager_;
    MarketRegime current_regime_;
    int regime_check_interval_ = 390;  // Check every block

    // Update regime and parameters
    void update_regime_if_needed();
    void apply_regime_params(const RegimeParams& params);
};
```

**Modify** `src/strategy/online_ensemble_strategy.cpp`:
```cpp
// In constructor:
OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : config_(config) {

    // ... existing initialization ...

    // Initialize regime detection
    regime_detector_ = std::make_unique<MarketRegimeDetector>();
    param_manager_ = std::make_unique<RegimeParameterManager>();
    current_regime_ = MarketRegime::LOW_VOLATILITY;

    utils::log_info("OnlineEnsembleStrategy initialized with regime detection enabled");
}

// In on_bar():
void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    samples_seen_++;

    // Update feature engine
    feature_engine_->update(bar);

    // Check if we should update regime
    update_regime_if_needed();

    // Process pending updates (existing code)
    process_pending_updates(bar);

    // ... rest of existing code ...
}

void OnlineEnsembleStrategy::update_regime_if_needed() {
    if (samples_seen_ % regime_check_interval_ == 0 && samples_seen_ >= 100) {
        // Get recent bars for regime detection
        // (Assume we maintain a circular buffer of recent bars)
        std::vector<Bar> recent_bars = get_recent_bars(100);

        MarketRegime new_regime = regime_detector_->detect_regime(recent_bars);

        if (new_regime != current_regime_) {
            utils::log_info("Regime changed from " +
                          regime_to_string(current_regime_) +
                          " to " + regime_to_string(new_regime));

            current_regime_ = new_regime;

            // Apply new parameters
            RegimeParams params = param_manager_->get_params_for_regime(new_regime);
            apply_regime_params(params);
        }
    }
}

void OnlineEnsembleStrategy::apply_regime_params(const RegimeParams& params) {
    if (!params.is_valid()) {
        utils::log_error("Invalid regime params, skipping update");
        return;
    }

    config_.buy_threshold = params.buy_threshold;
    config_.sell_threshold = params.sell_threshold;
    config_.ewrls_lambda = params.ewrls_lambda;
    config_.bb_amplification_factor = params.bb_amplification_factor;

    // Update horizon weights
    config_.horizon_weights = {
        params.h1_weight,
        params.h5_weight,
        params.h10_weight
    };

    // Update BB params
    config_.bb_period = params.bb_period;
    config_.bb_std_dev = params.bb_std_dev;
    config_.bb_proximity_threshold = params.bb_proximity;
    config_.regularization = params.regularization;

    // Update current thresholds
    current_buy_threshold_ = params.buy_threshold;
    current_sell_threshold_ = params.sell_threshold;

    utils::log_info("Applied regime params: buy=" +
                   std::to_string(params.buy_threshold) +
                   " sell=" + std::to_string(params.sell_threshold));
}
```

---

## Phase 4: Regime-Specific Optuna Optimization

**Create** `tools/regime_aware_optuna.py`:
```python
#!/usr/bin/env python3
"""
Regime-Aware Optuna Optimization

Optimizes parameters separately for each market regime.
"""

import json
import sys
from collections import defaultdict
sys.path.insert(0, 'tools')
from adaptive_optuna import AdaptiveOptunaFramework

class RegimeAwareOptuna(AdaptiveOptunaFramework):
    def classify_block_regime(self, block_idx):
        """
        Classify a block's regime.

        This is a simplified version - in production you'd use
        the MarketRegimeDetector C++ class.
        """
        # Load block data
        start_bar = block_idx * self.bars_per_block
        end_bar = (block_idx + 1) * self.bars_per_block
        bars = self.all_bars[start_bar:end_bar]

        # Simple regime classification based on volatility and trend
        returns = [bars[i].close / bars[i-1].close - 1
                   for i in range(1, len(bars))]
        volatility = np.std(returns)
        trend = (bars[-1].close - bars[0].close) / bars[0].close

        if volatility > 0.02:  # High vol threshold
            return "HIGH_VOLATILITY"
        elif abs(trend) < 0.01:  # Low trend threshold
            return "CHOPPY"
        elif trend > 0.03:  # Strong uptrend
            return "TRENDING_UP"
        elif trend < -0.03:  # Strong downtrend
            return "TRENDING_DOWN"
        else:
            return "LOW_VOLATILITY"

    def get_blocks_by_regime(self, total_blocks=20):
        """Group blocks by regime"""
        regime_blocks = defaultdict(list)

        for block_idx in range(total_blocks):
            regime = self.classify_block_regime(block_idx)
            regime_blocks[regime].append(block_idx)

        return regime_blocks

    def optimize_per_regime(self, n_trials_per_regime=50):
        """
        Optimize parameters for each regime separately.

        Returns: dict mapping regime to best params
        """
        regime_blocks = self.get_blocks_by_regime()
        regime_params = {}

        print("=" * 80)
        print("REGIME-AWARE OPTIMIZATION")
        print("=" * 80)

        for regime, blocks in regime_blocks.items():
            if len(blocks) < 2:
                print(f"Skipping regime {regime}: only {len(blocks)} blocks")
                continue

            print(f"\nOptimizing for regime: {regime}")
            print(f"Blocks: {blocks} ({len(blocks)} blocks)")

            # Create data file for this regime
            regime_data = self.create_regime_data(blocks)

            # Run Optuna
            best_params, best_mrb, tuning_time = self.tune_on_window(
                block_start=0,
                block_end=len(blocks),
                n_trials=n_trials_per_regime
            )

            regime_params[regime] = {
                'params': best_params,
                'mrb': best_mrb,
                'blocks': blocks,
                'n_blocks': len(blocks)
            }

            print(f"Best MRB for {regime}: {best_mrb:.4f}%")

        return regime_params

# Main execution
if __name__ == "__main__":
    optimizer = RegimeAwareOptuna(
        data_file='data/equities/SPY_20blocks.csv',
        build_dir='build',
        output_dir='data/tmp/regime_optuna',
        n_trials=50,
        n_jobs=4
    )

    # Optimize per regime
    results = optimizer.optimize_per_regime(n_trials_per_regime=50)

    # Save results
    output_file = 'data/tmp/regime_optuna/regime_specific_params.json'
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\nResults saved to: {output_file}")
```

---

## Testing & Validation Plan

### Test 1: Adaptive Thresholds
```bash
# Before: Adaptive OFF
./test_without_adaptive.sh
# MRB: 0.104%

# After: Adaptive ON
./test_with_adaptive.sh
# Expected MRB: 0.20-0.25%
```

### Test 2: Regime Detection Accuracy
```python
# Validate regime classification
python3 tools/validate_regime_detection.py \
    --data SPY_100blocks.csv \
    --output regime_accuracy_report.json

# Should show:
# - Regime distribution (30% TRENDING, 40% CHOPPY, etc.)
# - Regime transition frequency
# - Average duration per regime
```

### Test 3: Regime-Specific Performance
```python
# Compare MRB by regime
python3 tools/analyze_regime_performance.py \
    --trades trades.jsonl \
    --regimes regime_log.json \
    --output regime_mrb_breakdown.json

# Expected output:
# {
#   "TRENDING_UP": {"mrb": 0.45%, "trades": 150},
#   "CHOPPY": {"mrb": 0.10%, "trades": 80},
#   "HIGH_VOLATILITY": {"mrb": 0.60%, "trades": 50}
# }
```

---

## Success Metrics

### Week 1 Target: 0.35% MRB
- ✅ Adaptive thresholds enabled
- ✅ Wider parameter ranges tested
- ✅ Stratified sampling implemented

### Week 2-3 Target: 0.50% MRB ✅
- ✅ Regime detection implemented
- ✅ Regime-specific parameters optimized
- ✅ Integrated into strategy

### Week 4 Target: Validation
- ✅ A/B test regime vs non-regime strategy
- ✅ 100-block backtest showing 0.50%+ MRB
- ✅ Production deployment readiness

---

## Risk Mitigation

1. **Regime Detection Errors**:
   - Log all regime transitions
   - Manual review of classifications
   - A/B test against baseline

2. **Parameter Overfitting Per Regime**:
   - Use stratified sampling within regime
   - Cross-validate on unseen blocks
   - Minimum 5 blocks per regime for optimization

3. **Regime Switching Costs**:
   - Add hysteresis (don't switch on single bar)
   - Require 2 consecutive regime signals
   - Log frequency of switches

---

## Next Steps

1. **TODAY**: Enable adaptive thresholds (1 line change)
2. **Week 1**: Increase param ranges + stratified sampling
3. **Week 2**: Implement regime detection classes
4. **Week 3**: Integrate regime switching into strategy
5. **Week 4**: Optimize per regime and validate

**Expected final result**: 0.50-0.55% MRB ✅ **TARGET ACHIEVED**

---

**Document Status**: READY FOR IMPLEMENTATION
**Priority**: URGENT - Quick wins available today
**Owner**: Trading Team
**Estimated Completion**: 3-4 weeks to full implementation

---

## Appendix: CMakeLists.txt Updates

Add new regime detection files to build:

```cmake
# Add to CMakeLists.txt

set(STRATEGY_SOURCES
    ${STRATEGY_SOURCES}
    src/strategy/market_regime_detector.cpp
    src/strategy/regime_parameter_manager.cpp
)

set(STRATEGY_HEADERS
    ${STRATEGY_HEADERS}
    include/strategy/market_regime_detector.h
    include/strategy/regime_parameter_manager.h
)
```

---

**END OF IMPLEMENTATION PLAN**
