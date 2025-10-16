# Optuna Expert Improvements - Implementation Plan

**Date:** 2025-10-08
**Status:** READY FOR IMPLEMENTATION
**Objective:** Implement expert AI recommendations for robust walk-forward Optuna optimization

---

## EXECUTIVE SUMMARY

Expert review identified 6 critical gaps in current Optuna approach:
1. ❌ Walk-forward leakage - Single evaluation window
2. ❌ Single-metric brittleness - Pure MRB optimization
3. ❌ Reproducibility gaps - No seeding/persistence
4. ❌ Parallel safety - Temp file collisions
5. ❌ Missing parameter validation - No constraint enforcement
6. ❌ Trial artifacts - No per-trial logging

**Solution:** Implement walk-forward folds, soft penalties, SQLite storage, and enhanced CLI.

---

## PHASE 1: C++ CLI ENHANCEMENTS (PRIORITY 1)

### 1.1 Add `--skip-blocks` to backtest_command.cpp

**Purpose:** Enable walk-forward fold creation by skipping N blocks from end before selecting windows.

**Changes:**
```cpp
// In backtest_command.cpp:51-120

// Add parameter
int skip_blocks = std::stoi(get_arg(args, "--skip-blocks", "0"));

// Modify data loading logic (line 115-125)
uint64_t total_bars = reader.get_bar_count();
uint64_t bars_to_skip = skip_blocks * BARS_PER_BLOCK;
uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;
uint64_t start_offset = 0;

if (total_bars > bars_to_skip + bars_needed) {
    // Skip from end: read bars at offset (total - skip - needed)
    start_offset = total_bars - bars_to_skip - bars_needed;
} else {
    std::cerr << "⚠️  Warning: Not enough bars for skip_blocks=" << skip_blocks << "\n";
    // Fall back to last available bars
}

bars = reader.read_bars_at_offset(start_offset, bars_needed);
```

**New Method Needed:**
```cpp
// Add to binary_data.h/cpp
std::vector<Bar> BinaryDataReader::read_bars_at_offset(uint64_t offset, uint64_t count);
```

---

### 1.2 Add `--params` JSON Override

**Purpose:** Allow Optuna to override strategy parameters without recompiling.

**Implementation:**
```cpp
// In backtest_command.cpp:51-80
std::string params_json = get_arg(args, "--params", "");

// Parse JSON and create config overrides
OnlineEnsembleStrategy::OnlineEnsembleConfig config;
if (!params_json.empty()) {
    auto overrides = parse_params_json(params_json);
    apply_overrides(config, overrides);

    // Log for reproducibility
    std::cout << "Parameter overrides:\n" << params_json << "\n\n";
}
```

**Helper Functions:**
```cpp
// Add to backtest_command.cpp private methods
std::map<std::string, double> parse_params_json(const std::string& json);
void apply_overrides(OnlineEnsembleStrategy::OnlineEnsembleConfig& cfg,
                     const std::map<std::string, double>& overrides);
```

**Supported Override Keys:**
- `ewrls_lambda`
- `buy_threshold`
- `sell_threshold`
- `bb_amplification_factor`
- `kelly_fraction`
- `regularization`
- `initial_variance`
- `warmup_samples`

---

### 1.3 Add `--json` Output to analyze_trades_command.cpp

**Purpose:** Eliminate regex fragility in Python parser.

**Changes:**
```cpp
// In analyze_trades_command.cpp
bool json_output = has_flag(args, "--json");

if (json_output) {
    // Emit structured JSON
    nlohmann::json result;
    result["mrb"] = mrb;
    result["win_rate"] = overall_win_rate;
    result["max_dd"] = max_drawdown;
    result["trades_per_block"] = trades_per_block;
    result["trades"] = total_trades;
    result["sharpe"] = sharpe_ratio;
    result["total_return_pct"] = total_return_pct;

    std::cout << result.dump(2) << std::endl;
    return 0;
}

// Otherwise, keep existing pretty text output
```

**Output Example:**
```json
{
  "mrb": 0.0043,
  "win_rate": 0.531,
  "max_dd": 0.0102,
  "trades_per_block": 3.1,
  "trades": 142,
  "sharpe": 0.42,
  "total_return_pct": 0.0172
}
```

---

### 1.4 Constructor Validation (Defense in Depth)

**Purpose:** Enforce parameter constraints at C++ level before backtesting.

**Changes to online_ensemble_strategy.cpp:10-53:**
```cpp
OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // VALIDATION BLOCK (add before initialization)

    // 1. Threshold constraints
    if (config.buy_threshold <= config.sell_threshold) {
        throw std::invalid_argument(
            "buy_threshold (" + std::to_string(config.buy_threshold) +
            ") must be > sell_threshold (" + std::to_string(config.sell_threshold) + ")"
        );
    }

    double signal_spread = config.buy_threshold - config.sell_threshold;
    if (signal_spread < 0.02) {
        throw std::invalid_argument(
            "Signal spread (" + std::to_string(signal_spread) +
            ") must be >= 0.02 (2% neutral zone minimum)"
        );
    }

    // 2. EWRLS lambda range
    if (config.ewrls_lambda < 0.985 || config.ewrls_lambda > 0.9995) {
        throw std::invalid_argument(
            "ewrls_lambda (" + std::to_string(config.ewrls_lambda) +
            ") must be in [0.985, 0.9995]"
        );
    }

    // 3. Kelly fraction range
    if (config.kelly_fraction < 0.0 || config.kelly_fraction > 0.5) {
        throw std::invalid_argument(
            "kelly_fraction (" + std::to_string(config.kelly_fraction) +
            ") must be in [0.0, 0.5]"
        );
    }

    // 4. BB amplification range
    if (config.bb_amplification_factor < 0.0 || config.bb_amplification_factor > 0.25) {
        throw std::invalid_argument(
            "bb_amplification_factor (" + std::to_string(config.bb_amplification_factor) +
            ") must be in [0.0, 0.25]"
        );
    }

    // 5. Regularization range
    if (config.regularization < 0.0001 || config.regularization > 1.0) {
        throw std::invalid_argument(
            "regularization (" + std::to_string(config.regularization) +
            ") must be in [0.0001, 1.0]"
        );
    }

    // 6. Warmup samples
    if (config.warmup_samples < 20 || config.warmup_samples > 500) {
        throw std::invalid_argument(
            "warmup_samples (" + std::to_string(config.warmup_samples) +
            ") must be in [20, 500]"
        );
    }

    // Validation passed, log for transparency
    utils::log_info("OnlineEnsembleStrategy config validation passed:");
    utils::log_info("  buy/sell thresholds: " + std::to_string(config.buy_threshold) +
                   "/" + std::to_string(config.sell_threshold) +
                   " (spread=" + std::to_string(signal_spread) + ")");
    utils::log_info("  ewrls_lambda: " + std::to_string(config.ewrls_lambda));
    utils::log_info("  kelly_fraction: " + std::to_string(config.kelly_fraction));
    utils::log_info("  bb_amplification: " + std::to_string(config.bb_amplification_factor));

    // ... existing initialization code ...
}
```

---

### 1.5 Parallel Safety - Unique Output Paths

**Purpose:** Prevent temp file collisions when running trials in parallel.

**Changes to backtest_command.cpp:77-83:**
```cpp
// Generate unique run ID
auto now = std::chrono::system_clock::now();
auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()).count();
pid_t pid = getpid();

std::string run_id = std::to_string(pid) + "_" + std::to_string(timestamp);
std::string run_dir = output_dir + "/run_" + run_id;
std::filesystem::create_directories(run_dir);

// Output file paths with unique run_id
std::string signals_file = run_dir + "/signals.jsonl";
std::string trades_file = run_dir + "/trades.jsonl";
std::string analysis_file = run_dir + "/analysis.txt";
std::string metrics_json = run_dir + "/metrics.json";

// Symlink to "last_run" for convenience
std::string last_run_link = output_dir + "/last_run";
std::filesystem::remove(last_run_link);
std::filesystem::create_symlink(run_dir, last_run_link);
```

---

## PHASE 2: PYTHON OPTUNA RUNNER (PRIORITY 1)

### 2.1 Create tools/optuna_mrb_wf.py

**Purpose:** Walk-forward optimizer with soft penalties and persistence.

**Key Features:**
- Walk-forward folds (configurable, default 5)
- Soft penalty functions (win rate, drawdown, trade frequency)
- Early pruning (PercentilePruner after 2 folds)
- SQLite storage with resumption
- Seeded sampling for reproducibility
- Trial-level artifact saving
- Best parameters persistence with metadata

**Files to Create:**
1. `tools/optuna_mrb_wf.py` - Main optimizer (from expert's code)
2. `tools/optuna_utils.py` - Shared utilities (metrics parsing, penalty calc)
3. `config/optuna_config.yaml` - Configuration (ranges, folds, penalties)

---

### 2.2 Walk-Forward Fold Strategy

**Configuration:**
```yaml
walk_forward:
  warmup_blocks: 2
  test_blocks: 4
  folds: 5
  skip_step: 2  # Shift by 2 blocks per fold (50% overlap)

# Example fold layout (SPY data: 1018 blocks total)
# Fold 0: blocks 1000-1010 (warmup 1000-1001, test 1002-1005) skip=0
# Fold 1: blocks 1002-1012 (warmup 1002-1003, test 1004-1007) skip=2
# Fold 2: blocks 1004-1014 (warmup 1004-1005, test 1006-1009) skip=4
# Fold 3: blocks 1006-1016 (warmup 1006-1007, test 1008-1011) skip=6
# Fold 4: blocks 1008-1018 (warmup 1008-1009, test 1010-1013) skip=8
```

**Objective Function:**
```python
def objective(trial):
    params = suggest_params(trial)
    fold_results = []

    for fold_idx in range(FOLDS):
        metrics = run_fold(params, fold_idx)
        raw_mrb = metrics["mrb"]
        penalty = calc_penalty(metrics)
        adj_mrb = raw_mrb - penalty

        fold_results.append(adj_mrb)

        # Early pruning after 2 folds
        if fold_idx >= 1:
            trial.report(statistics.mean(fold_results), step=fold_idx)
            if trial.should_prune():
                raise optuna.TrialPruned()

    # Return mean adjusted MRB across all folds
    return statistics.mean(fold_results)
```

---

### 2.3 Soft Penalty Functions

**Formulation:**
```python
def calc_penalty(metrics: Dict[str, float]) -> float:
    penalty = 0.0

    # Win rate penalty (exponential beyond threshold)
    if metrics["win_rate"] < 0.50:
        shortfall = 0.50 - metrics["win_rate"]
        penalty += 0.50 * (shortfall / 0.10)  # 0.50% MRB per 10% shortfall

    # Drawdown penalty (linear)
    if metrics["max_dd"] > 0.030:  # 3%
        overage = metrics["max_dd"] - 0.030
        penalty += 1.00 * (overage / 0.030)  # 1.00% MRB per 3% overage

    # Trade frequency penalty (soft bounds)
    tpb = metrics["trades_per_block"]
    if tpb < 0.5:
        penalty += 0.15 * (0.5 - tpb)
    elif tpb > 8.0:
        penalty += 0.15 * (tpb - 8.0)

    return penalty
```

**Rationale:**
- Win rate <50% is fundamentally bad → steep penalty
- Drawdown >3% violates risk limits → moderate penalty
- Trade frequency extremes → light penalty (not fatal)

---

## PHASE 3: PARAMETER SPACE REFINEMENT (PRIORITY 2)

### 3.1 Coarse Phase (200 trials)

**Ranges:**
```python
{
    "ewrls_lambda": (0.990, 0.999, 0.0005),  # step
    "buy_threshold": (0.52, 0.60, None),
    "sell_threshold": (0.40, 0.48, None),
    "min_signal_spread": (0.02, 0.06, None),
    "bb_amplification": (0.05, 0.20, None),
    "kelly_fraction": (0.10, 0.40, None),
    "regularization": (0.001, 0.1, None),  # log scale
}
```

**Constraints:**
- `buy_threshold > sell_threshold + min_signal_spread`
- Prune infeasible trials early

---

### 3.2 Fine-Tune Phase (100 trials)

**After coarse phase, narrow to top performers:**
```python
# Example: if coarse found lambda~0.996, buy~0.55, sell~0.44
{
    "ewrls_lambda": (0.994, 0.998, 0.00025),
    "buy_threshold": (0.535, 0.565, None),
    "sell_threshold": (0.425, 0.455, None),
    "min_signal_spread": (0.025, 0.045, None),
    "bb_amplification": (0.08, 0.16, None),
    "kelly_fraction": (0.15, 0.30, None),
}
```

---

## PHASE 4: REPRODUCIBILITY & PERSISTENCE (PRIORITY 1)

### 4.1 Seeding Strategy

```python
OPTUNA_SEED = 20251008
sampler = TPESampler(
    seed=OPTUNA_SEED,
    multivariate=True,
    group=True,
    consider_endpoints=True
)
```

**Also seed:**
- C++ random generators (if any)
- Feature engine initialization (if stochastic)

---

### 4.2 SQLite Storage

```python
STORAGE_URI = "sqlite:///data/optuna_mrb_wf.sqlite3"
study = optuna.create_study(
    study_name="mrb_v2_wf_spy",
    storage=STORAGE_URI,
    load_if_exists=True,  # Resume if exists
    ...
)
```

**Benefits:**
- Resume interrupted runs
- Multi-process safe
- Query results with SQL
- No data loss

---

### 4.3 Best Parameters Artifact

**Save on every improvement:**
```python
def callback(study, trial):
    if study.best_trial == trial:
        artifact = {
            "schema_hash": "unified_v2_45feat_sha1",  # From feature engine
            "scaler_state_hash": "mean_std_v1_sha1",   # From scaler
            "params": trial.params,
            "value": trial.value,
            "datetime": datetime.now().isoformat(),
            "trial_number": trial.number,
            "fold_results": trial.user_attrs.get("fold_results", []),
        }
        Path("data/optuna_best/mrb_best.json").write_text(
            json.dumps(artifact, indent=2)
        )
```

---

## PHASE 5: VALIDATION & TESTING (PRIORITY 2)

### 5.1 Unit Tests

**Create tests/test_backtest_skip_blocks.cpp:**
```cpp
TEST(BacktestCommand, SkipBlocksOffByOne) {
    // Test skip_blocks=0 (use last N)
    // Test skip_blocks=10 (skip last 10, use previous N)
    // Test skip_blocks > available (error handling)
}
```

**Create tests/test_optuna_penalties.py:**
```python
def test_penalty_win_rate_shortfall():
    m = {"mrb": 0.01, "win_rate": 0.45, "max_dd": 0.02, "trades_per_block": 3.0}
    p = calc_penalty(m)
    assert p > 0.0  # Should penalize <50% win rate

def test_penalty_no_violations():
    m = {"mrb": 0.01, "win_rate": 0.55, "max_dd": 0.02, "trades_per_block": 3.0}
    p = calc_penalty(m)
    assert p == 0.0  # No penalties
```

---

### 5.2 Integration Test

**Run single trial end-to-end:**
```bash
# 1. Manual backtest with --skip-blocks
sentio_cli backtest --blocks 4 --warmup-blocks 2 --skip-blocks 10 \
  --params '{"buy_threshold":0.55,"sell_threshold":0.43}' \
  --output-dir data/tmp/test_wf

# 2. Verify output exists
ls data/tmp/test_wf/run_*/metrics.json

# 3. Parse JSON
cat data/tmp/test_wf/last_run/metrics.json | jq '.mrb'
```

---

## PHASE 6: DEPLOYMENT CHECKLIST

### Before Running Optuna:
- [ ] Compile C++ with new flags: `--skip-blocks`, `--params`, `--json`
- [ ] Test single fold manually
- [ ] Verify SQLite storage works
- [ ] Set environment variables (SENTIO_BIN, OPTUNA_DB, OPTUNA_SEED)
- [ ] Ensure data/optuna_best/ directory exists
- [ ] Check disk space (200 trials × 5 folds × ~50MB = ~50GB)

### During Optimization:
- [ ] Monitor SQLite DB size
- [ ] Check best_value progression (should improve over time)
- [ ] Verify no "inf" or "nan" values in results
- [ ] Inspect fold_results for variance (high variance = unstable params)

### After Phase 1 (Coarse):
- [ ] Run importance analysis: `optuna.importance.get_param_importances()`
- [ ] Plot optimization history
- [ ] Extract top 10 trials
- [ ] Narrow ranges for Phase 2 (fine-tune)

### Validation:
- [ ] Test best params on QQQ (different asset)
- [ ] Run with 1.5× fees/slippage (stress test)
- [ ] Verify schema_hash matches current feature engine
- [ ] Backtest on full 2020-2025 period (not just tuning window)

---

## EXPECTED OUTCOMES

### Success Criteria (Phase 1):
- MRB improvement: -0.165% → -0.05% or better (+0.115% gain)
- Win rate: ≥55%
- Max drawdown: ≤3%
- Consistent across folds (std dev <0.05%)

### Stretch Goals (Phase 2):
- MRB: ≥+0.05% per block (profitable)
- Win rate: ≥60%
- Sharpe ratio: ≥0.5

### Red Flags:
- All trials have MRB <-0.30% → Strategy fundamentally broken
- High variance across folds (std >0.10%) → Overfitting to noise
- Best params at edge of ranges → Search space too narrow
- No improvement after 100 trials → Need different features or architecture

---

## TIMELINE ESTIMATE

| Phase | Tasks | Est. Time |
|-------|-------|-----------|
| 1. C++ CLI | --skip-blocks, --params, --json, validation | 4 hours |
| 2. Python | optuna_mrb_wf.py, utils, config | 2 hours |
| 3. Testing | Unit tests, integration test | 2 hours |
| 4. Phase 1 Run | 200 trials × 5 folds @ 60s/trial | ~16 hours |
| 5. Analysis | Importance, narrowing, Phase 2 setup | 2 hours |
| 6. Phase 2 Run | 100 trials fine-tune | ~8 hours |
| 7. Validation | QQQ, stress tests, full backtest | 4 hours |
| **Total** | | **~38 hours** |

**Parallelization:** With n_jobs=4, reduce runtime by ~60% → ~15 hours total.

---

## NEXT STEPS

1. **Implement Phase 1 C++ changes** (this document provides exact code)
2. **Create Python runner** (expert provided drop-in code)
3. **Run manual test** (single trial, verify JSON output)
4. **Launch Phase 1 optimization** (200 trials, coarse search)
5. **Analyze and iterate**

---

## APPENDIX A: Expert Recommendations Summary

1. ✅ Walk-forward folds (5 folds, 50% overlap)
2. ✅ Soft penalties (win rate, drawdown, trade freq)
3. ✅ Reproducibility (seeded TPE, SQLite, artifacts)
4. ✅ Parallel safety (unique run IDs, no shared state)
5. ✅ Parameter validation (C++ constructor guards)
6. ✅ Trial artifacts (metrics JSON, logs per trial)
7. ✅ Early pruning (PercentilePruner after 2 folds)
8. ✅ Defense in depth (constraints at Python + C++ levels)

---

## APPENDIX B: File Manifest

**New Files:**
- `tools/optuna_mrb_wf.py` - Main optimizer
- `tools/optuna_utils.py` - Utilities
- `config/optuna_config.yaml` - Configuration
- `tests/test_backtest_skip_blocks.cpp` - Unit tests
- `tests/test_optuna_penalties.py` - Penalty tests
- `megadocs/OPTUNA_EXPERT_IMPROVEMENTS_PLAN.md` - This document

**Modified Files:**
- `src/cli/backtest_command.cpp` - Add --skip-blocks, --params
- `src/cli/analyze_trades_command.cpp` - Add --json
- `src/strategy/online_ensemble_strategy.cpp` - Constructor validation
- `include/common/binary_data.h` - Add read_bars_at_offset()
- `src/common/binary_data.cpp` - Implement read_bars_at_offset()

---

**END OF IMPLEMENTATION PLAN**
