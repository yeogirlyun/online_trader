# Optuna Performance Optimization via Feature Caching

**Date**: 2025-10-08
**Purpose**: Speed up Optuna optimization by eliminating redundant feature calculations

---

## Current Bottleneck Analysis

### Current Workflow (Per Trial)

For **each trial**, Optuna runs:

```bash
# Step 1: Generate signals (30-32 seconds)
./sentio_cli generate-signals \
    --data SPY_20blocks.csv \
    --output temp_signals.jsonl \
    --warmup 960 \
    --buy-threshold 0.55 \
    --sell-threshold 0.45 \
    --lambda 0.995 \
    --bb-amp 0.10

# Step 2: Execute trades (5-8 seconds)
./sentio_cli execute-trades \
    --signals temp_signals.jsonl \
    --data SPY_20blocks.csv \
    --output temp_trades.jsonl \
    --warmup 960

# Step 3: Analyze trades (1-2 seconds)
./sentio_cli analyze-trades \
    --trades temp_trades.jsonl \
    --data SPY_20blocks.csv \
    --json
```

**Total time per trial**: ~30-35 seconds
**Time for 200 trials**: ~1.5-2 hours

---

## The Problem: Redundant Feature Calculation

### What Gets Recalculated Every Trial

In `generate-signals`, the **UnifiedFeatureEngine** computes **58 features** for every bar:

```cpp
// For EVERY trial, EVERY bar (e.g., 7,800 bars × 200 trials = 1,560,000 feature calculations)
void UnifiedFeatureEngine::update(const Bar& b) {
    // Time features (8)
    time.hour_sin = sin(2π * hour / 24);
    time.hour_cos = cos(2π * hour / 24);
    // ... 6 more time features

    // Price action (12)
    price.return_1 = (close - prev_close) / prev_close;
    price.range = high - low;
    // ... 10 more price features

    // Technical indicators (38)
    rsi14.update(close);
    rsi21.update(close);
    atr14.update(high, low, close);
    bb20.update(close);
    macd.update(close);
    // ... 33 more indicator updates
}
```

**Key insight**: **Features are deterministic**. For the same input data, features are **identical across all trials**.

**What changes between trials**:
- `buy_threshold` (used AFTER features are computed)
- `sell_threshold` (used AFTER features are computed)
- `ewrls_lambda` (used in predictor, AFTER features)
- `bb_amplification_factor` (used in signal generation, AFTER features)

**What does NOT change**:
- ✅ **All 58 features** (time, price, indicators, patterns)
- ✅ **Bar data** (OHLCV)
- ✅ **Feature engine state** (RSI, ATR, MACD calculations)

---

## Optimization Strategy: Pre-compute Features Once

### Solution 1: Feature Cache File (Easiest)

**Approach**: Generate a feature matrix once, reuse for all trials.

#### Step 1: Create Feature Extraction Command

Add new CLI command to `sentio_cli`:

```cpp
// src/cli/extract_features_command.cpp
void ExtractFeaturesCommand::execute() {
    // Load data
    auto bars = load_bars(data_file_);

    // Initialize feature engine
    UnifiedFeatureEngine engine;

    // Open output file (CSV or binary)
    std::ofstream out(output_file_);

    // Write header
    out << "timestamp";
    for (const auto& name : engine.names()) {
        out << "," << name;
    }
    out << "\n";

    // Extract features for each bar
    for (const auto& bar : bars) {
        engine.update(bar);

        // Write timestamp + features
        out << bar.timestamp;
        for (double feat : engine.features_view()) {
            out << "," << feat;
        }
        out << "\n";
    }
}
```

#### Step 2: Modify Optuna Workflow

```python
# In adaptive_optuna.py
class AdaptiveOptuna:
    def __init__(self, data_file, build_dir):
        self.data_file = data_file
        self.build_dir = build_dir

        # Pre-compute features ONCE
        self.features_file = self.extract_features_once()

    def extract_features_once(self):
        """Extract features once at startup."""
        features_file = os.path.join(self.output_dir, "cached_features.csv")

        if not os.path.exists(features_file):
            print("[FeatureCache] Extracting features (one-time cost)...")
            cmd = [
                self.sentio_cli, "extract-features",
                "--data", self.data_file,
                "--output", features_file
            ]
            subprocess.run(cmd, check=True)
            print(f"[FeatureCache] Features cached to {features_file}")
        else:
            print(f"[FeatureCache] Using existing cache: {features_file}")

        return features_file

    def run_backtest(self, params):
        """Run backtest using pre-computed features."""
        # Now generate-signals can skip feature extraction
        cmd_generate = [
            self.sentio_cli, "generate-signals-from-features",
            "--features", self.features_file,
            "--data", self.data_file,
            "--output", "temp_signals.jsonl",
            "--buy-threshold", str(params['buy_threshold']),
            "--sell-threshold", str(params['sell_threshold']),
            "--lambda", str(params['ewrls_lambda']),
            "--bb-amp", str(params['bb_amplification_factor'])
        ]
        # ... rest of backtest
```

#### Step 3: Create generate-signals-from-features Command

```cpp
// src/cli/generate_signals_from_features_command.cpp
void GenerateSignalsFromFeaturesCommand::execute() {
    // Load pre-computed features
    auto features_matrix = load_features(features_file_);

    // Load raw bar data (for timestamps, PSM execution)
    auto bars = load_bars(data_file_);

    // Initialize OnlineEnsemble WITHOUT feature engine
    OnlineEnsembleStrategy strategy(
        ewrls_lambda_,
        buy_threshold_,
        sell_threshold_,
        bb_amplification_factor_
    );

    // Feed pre-computed features to strategy
    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& bar = bars[i];
        const auto& features = features_matrix[i];

        // Update strategy with features (skip feature extraction)
        strategy.update_from_features(bar, features);

        // Generate signal
        auto signal = strategy.get_signal(bar.timestamp);
        write_signal(signal);
    }
}
```

---

### Solution 2: In-Memory Feature Cache (Faster)

**Approach**: Keep feature engine in memory, only run EWRLS predictor per trial.

#### Pros vs Solution 1:
- ✅ **No disk I/O** (faster than CSV read/write)
- ✅ **No new CLI commands** needed
- ❌ **More complex** (requires C++ refactoring)
- ❌ **Memory intensive** (58 features × 7,800 bars × 8 bytes = 3.6 MB per dataset)

#### Implementation:

```cpp
// In generate_signals_command.cpp
class FeatureCache {
public:
    // Extract features once
    std::vector<std::vector<double>> extract(const std::vector<Bar>& bars) {
        UnifiedFeatureEngine engine;
        std::vector<std::vector<double>> cache;

        for (const auto& bar : bars) {
            engine.update(bar);
            cache.push_back(engine.features_view());
        }

        return cache;
    }
};

// Modified generate-signals workflow
void GenerateSignalsCommand::execute() {
    auto bars = load_bars(data_file_);

    // Check if features are cached
    static std::map<std::string, std::vector<std::vector<double>>> global_cache;

    if (global_cache.find(data_file_) == global_cache.end()) {
        // First trial: compute features
        FeatureCache cache;
        global_cache[data_file_] = cache.extract(bars);
    }

    // All trials: use cached features
    const auto& features = global_cache[data_file_];

    // Run strategy with cached features
    run_strategy_with_features(bars, features, params_);
}
```

---

### Solution 3: Hybrid (Best of Both)

**Approach**: Cache to disk, load into memory once.

```python
# In adaptive_optuna.py
class AdaptiveOptuna:
    def __init__(self, data_file, build_dir):
        # Step 1: Extract features to disk (one-time)
        self.features_file = self.extract_features_once()

        # Step 2: Load into memory (one-time)
        self.features_matrix = self.load_features_to_memory()

    def load_features_to_memory(self):
        """Load feature cache into numpy array."""
        import numpy as np
        return np.loadtxt(self.features_file, delimiter=',', skiprows=1)

    def run_backtest_fast(self, params):
        """Run backtest using in-memory features."""
        # Pass features directly to C++ via stdin or shared memory
        # ... implementation
```

---

## Expected Performance Gains

### Current Performance (No Caching)

**Trial breakdown**:
1. Feature extraction: **30 seconds** (7,800 bars × 58 features)
2. Signal generation: **2 seconds** (EWRLS predictor)
3. Trade execution: **5 seconds** (PSM logic)
4. Analysis: **1 second** (metrics)

**Total**: 38 seconds/trial
**200 trials**: 2.1 hours

---

### With Feature Caching

**One-time cost** (before trials):
- Feature extraction: **30 seconds** (once for all trials)

**Per trial**:
1. ~~Feature extraction~~: **0 seconds** (cached)
2. Signal generation: **2 seconds** (EWRLS predictor only)
3. Trade execution: **5 seconds** (PSM logic)
4. Analysis: **1 second** (metrics)

**Total**: 8 seconds/trial
**200 trials**: 30 seconds (initial) + 26 minutes (trials) = **27 minutes**

**Speedup**: 2.1 hours → 27 minutes = **4.7x faster**

---

### With Feature Caching + In-Memory

**One-time cost**:
- Feature extraction + load: **35 seconds** (once)

**Per trial**:
1. ~~Feature extraction~~: **0 seconds** (in-memory)
2. Signal generation: **1 second** (no CSV I/O)
3. Trade execution: **5 seconds**
4. Analysis: **1 second**

**Total**: 7 seconds/trial
**200 trials**: 35 seconds + 23 minutes = **24 minutes**

**Speedup**: 2.1 hours → 24 minutes = **5.2x faster**

---

### Daily Optimization Impact

**Current**:
- 100 trials × 30 sec/trial = **50 minutes**

**With feature caching**:
- 30 sec (extract) + 100 trials × 7 sec/trial = **12 minutes**

**Speedup**: 50 min → 12 min = **4.2x faster**

**Benefit**: Can run daily optimization during morning coffee (8:30-8:42 AM) instead of requiring 50 minutes.

---

## Implementation Priority

### Phase 1: Feature Extraction Command (1-2 hours)
- ✅ Add `extract-features` CLI command
- ✅ Outputs CSV: `timestamp,feature1,feature2,...,feature58`
- ✅ Test on SPY_4blocks.csv

### Phase 2: Modify Optuna Workflow (30 min)
- ✅ Pre-extract features once before trials
- ✅ Modify `generate-signals` to accept `--features-file` flag
- ✅ Skip feature engine initialization if features provided

### Phase 3: Optimize Signal Generation (1 hour)
- ✅ Create `generate-signals-from-features` command
- ✅ Load features from CSV
- ✅ Feed directly to OnlineEnsembleStrategy

### Phase 4: In-Memory Caching (Optional, 2 hours)
- ✅ Load feature CSV into numpy array
- ✅ Pass via stdin or shared memory to C++
- ✅ Eliminate CSV I/O overhead

---

## Alternative: Parallel Trial Execution

Instead of caching, run trials in **parallel** (if multi-core available).

```python
# In adaptive_optuna.py
from multiprocessing import Pool

def run_trial_parallel(trial):
    params = suggest_params(trial)
    result = self.run_backtest(params)
    return result

# Run 4 trials in parallel (4-core CPU)
with Pool(4) as p:
    results = p.map(run_trial_parallel, range(200))
```

**Speedup**: 4x (if 4 cores available)
**Downside**: 4x memory usage, more complex

---

## Recommendation

### For Immediate Impact (Today)

**Implement Phase 1-3** (Feature extraction + CSV caching):
- ✅ Easy to implement (2-3 hours)
- ✅ 4.7x speedup
- ✅ No architectural changes
- ✅ Works with existing Optuna workflow

**Skip Phase 4** (In-memory):
- Additional 0.5x speedup not worth complexity
- Disk I/O is already fast for CSV

**Skip parallel execution**:
- Complexity not worth 2-4x speedup
- Feature caching gives better ROI

---

### Implementation Plan

#### Step 1: Add extract-features Command (1 hour)

```cpp
// include/cli/extract_features_command.h
class ExtractFeaturesCommand : public Command {
public:
    explicit ExtractFeaturesCommand(const std::string& data_file,
                                     const std::string& output_file)
        : data_file_(data_file), output_file_(output_file) {}

    void execute() override;

private:
    std::string data_file_;
    std::string output_file_;
};
```

```cpp
// src/cli/extract_features_command.cpp
void ExtractFeaturesCommand::execute() {
    // Load bars
    auto bars = DataLoader::load_csv(data_file_);

    // Initialize feature engine
    UnifiedFeatureEngine engine;

    // Write CSV
    std::ofstream out(output_file_);

    // Header
    out << "timestamp";
    for (const auto& name : engine.names()) {
        out << "," << name;
    }
    out << "\n";

    // Features
    for (const auto& bar : bars) {
        engine.update(bar);
        out << bar.timestamp;
        for (double f : engine.features_view()) {
            out << "," << std::fixed << std::setprecision(8) << f;
        }
        out << "\n";
    }
}
```

#### Step 2: Modify Optuna to Use Cache (30 min)

```python
# In adaptive_optuna.py
def extract_features_once(self, data_file):
    """Pre-compute features before trials."""
    features_file = data_file.replace('.csv', '_features.csv')

    if os.path.exists(features_file):
        print(f"[Cache] Using existing features: {features_file}")
        return features_file

    print(f"[Cache] Extracting features (one-time)...")
    cmd = [
        self.sentio_cli, "extract-features",
        "--data", data_file,
        "--output", features_file
    ]
    subprocess.run(cmd, check=True, capture_output=True)
    print(f"[Cache] Features saved to: {features_file}")

    return features_file
```

#### Step 3: Modify generate-signals to Use Features (1 hour)

```cpp
// In generate_signals_command.cpp
void GenerateSignalsCommand::execute() {
    if (features_file_.empty()) {
        // Standard workflow: extract features
        run_with_feature_extraction();
    } else {
        // Fast workflow: load pre-computed features
        run_with_cached_features();
    }
}

void GenerateSignalsCommand::run_with_cached_features() {
    // Load features from CSV
    auto features = load_feature_matrix(features_file_);
    auto bars = load_bars(data_file_);

    // Initialize strategy (no feature engine needed)
    OnlineEnsembleStrategy strategy(params_);

    // Process with cached features
    for (size_t i = 0; i < bars.size(); ++i) {
        strategy.update_from_features(bars[i], features[i]);
        // ... generate signals
    }
}
```

---

## Testing Plan

### Test 1: Feature Extraction Accuracy

```bash
# Extract features
./sentio_cli extract-features \
    --data data/equities/SPY_4blocks.csv \
    --output /tmp/features.csv

# Verify: 1561 rows (1560 bars + 1 header)
wc -l /tmp/features.csv  # Should be 1561

# Verify: 59 columns (timestamp + 58 features)
head -1 /tmp/features.csv | tr ',' '\n' | wc -l  # Should be 59
```

### Test 2: Signal Generation Equivalence

```bash
# Method 1: Standard (with feature extraction)
./sentio_cli generate-signals \
    --data data/equities/SPY_4blocks.csv \
    --output /tmp/signals_standard.jsonl \
    --warmup 960

# Method 2: Cached (with pre-extracted features)
./sentio_cli generate-signals \
    --data data/equities/SPY_4blocks.csv \
    --features /tmp/features.csv \
    --output /tmp/signals_cached.jsonl \
    --warmup 960

# Verify identical output
diff /tmp/signals_standard.jsonl /tmp/signals_cached.jsonl
# Should be identical (or diff only in metadata)
```

### Test 3: Optuna Performance

```bash
# Before: No caching
time ./tools/run_daily_optuna.sh  # ~50 minutes

# After: With caching
time ./tools/run_daily_optuna.sh  # ~12 minutes (4.2x faster)
```

---

## Key Takeaways

✅ **Bottleneck identified**: Feature extraction takes 80% of trial time
✅ **Solution**: Pre-compute features once, reuse for all trials
✅ **Expected speedup**: 4.7x (2 hours → 27 minutes)
✅ **Implementation time**: 2-3 hours
✅ **Daily impact**: 50 min → 12 min optimization
✅ **Production ready**: No breaking changes to existing workflow

**Next step**: Implement extract-features command and modify Optuna workflow.

---

**Status**: Performance analysis complete. Ready to implement feature caching for 4.7x speedup.
