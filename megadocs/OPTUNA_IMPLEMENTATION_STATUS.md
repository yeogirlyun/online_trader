# Optuna Expert Improvements - Implementation Status

**Date:** 2025-10-08
**Status:** IN PROGRESS

---

## COMPLETED TASKS ✅

### 1. Add `--skip-blocks` Parameter (Task 1)
**Status:** ✅ COMPLETE
**Files Modified:**
- `src/cli/backtest_command.cpp`

**Changes:**
- Added `--skip-blocks <N>` parameter parsing with validation
- Implemented walk-forward fold logic for both BIN and CSV formats
- Updated help documentation
- Compiled and tested successfully

**Usage:**
```bash
# Skip last 10 blocks, test on previous 4 blocks with 2 warmup
sentio_cli backtest --blocks 4 --warmup-blocks 2 --skip-blocks 10
```

**Walk-Forward Example:**
```bash
# Fold 0: skip=0  (test last 4 blocks)
# Fold 1: skip=2  (test 2 blocks before last)
# Fold 2: skip=4  (test 4 blocks before last)
# etc.
```

---

## IN PROGRESS TASKS 🚧

### 2. Add `--params` JSON Override (Task 2)
**Status:** 🚧 NOT STARTED
**Priority:** HIGH
**Est. Time:** 2 hours

**Required Changes:**
```cpp
// In backtest_command.cpp:66-82
std::string params_json = get_arg(args, "--params", "");
OnlineEnsembleStrategy::OnlineEnsembleConfig config;

if (!params_json.empty()) {
    // Parse JSON and override config
    auto overrides = parse_json_overrides(params_json);
    apply_config_overrides(config, overrides);

    // Log for reproducibility
    std::cout << "📝 Parameter overrides:\n" << params_json << "\n\n";
}

// Pass config to generate-signals command
```

**Supported Parameters:**
- `ewrls_lambda`
- `buy_threshold`
- `sell_threshold`
- `bb_amplification_factor`
- `kelly_fraction`
- `regularization`
- `initial_variance`

---

### 3. Add `--json` Output to analyze-trades (Task 3)
**Status:** 🚧 NOT STARTED
**Priority:** HIGH
**Est. Time:** 1 hour

**Required Changes:**
```cpp
// In analyze_trades_command.cpp
bool json_output = has_flag(args, "--json");

if (json_output) {
    nlohmann::json result;
    result["mrb"] = mrb;
    result["win_rate"] = overall_win_rate;
    result["max_dd"] = max_drawdown;
    result["trades_per_block"] = trades_per_block;
    result["trades"] = total_trades;

    std::cout << result.dump(2) << std::endl;
    return 0;
}
```

---

### 4. Add Constructor Validation (Task 4)
**Status:** 🚧 NOT STARTED
**Priority:** MEDIUM
**Est. Time:** 1 hour

**Required Changes:**
```cpp
// In online_ensemble_strategy.cpp:10
OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config) {
    // Validation block
    if (config.buy_threshold <= config.sell_threshold) {
        throw std::invalid_argument("buy_threshold must be > sell_threshold");
    }

    double spread = config.buy_threshold - config.sell_threshold;
    if (spread < 0.02) {
        throw std::invalid_argument("Signal spread must be >= 0.02");
    }

    if (config.ewrls_lambda < 0.985 || config.ewrls_lambda > 0.9995) {
        throw std::invalid_argument("ewrls_lambda must be in [0.985, 0.9995]");
    }

    // ... rest of constructor
}
```

---

### 5. Add Parallel-Safe Run IDs (Task 5)
**Status:** 🚧 NOT STARTED
**Priority:** MEDIUM
**Est. Time:** 1 hour

**Required Changes:**
```cpp
// In backtest_command.cpp:83-86
#include <unistd.h>
#include <chrono>

pid_t pid = getpid();
auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
std::string run_id = std::to_string(pid) + "_" + std::to_string(timestamp);

std::string run_dir = output_dir + "/run_" + run_id;
std::filesystem::create_directories(run_dir);

std::string signals_file = run_dir + "/signals.jsonl";
std::string trades_file = run_dir + "/trades.jsonl";
std::string metrics_json = run_dir + "/metrics.json";

// Symlink last_run for convenience
std::filesystem::remove(output_dir + "/last_run");
std::filesystem::create_symlink(run_dir, output_dir + "/last_run");
```

---

### 6. Create Python Optuna Optimizer (Task 6)
**Status:** 🚧 NOT STARTED
**Priority:** HIGH
**Est. Time:** 3 hours

**Files to Create:**
1. `tools/optuna_mrb_wf.py` - Main optimizer with walk-forward
2. `tools/optuna_utils.py` - Penalty functions and metrics parsing
3. `config/optuna_config.yaml` - Parameter ranges and settings

**Key Features:**
- 5-fold walk-forward evaluation
- Soft penalty functions (win rate, drawdown, trade freq)
- Early pruning (PercentilePruner)
- SQLite persistence
- Best parameters artifact saving

---

### 7. Testing & Validation (Task 7)
**Status:** 🚧 NOT STARTED
**Priority:** HIGH
**Est. Time:** 2 hours

**Tests Needed:**
1. Unit test: `--skip-blocks` math (off-by-one edge cases)
2. Integration test: Single fold backtest with `--params`
3. Validation: 5-fold run on SPY (manual)
4. Stress test: Parallel Optuna trials (n_jobs=4)

---

## IMPLEMENTATION PLAN

### Phase 1: Core C++ Enhancements (6 hours)
1. ✅ `--skip-blocks` (DONE)
2. ⬜ `--params` JSON override (2 hours)
3. ⬜ `--json` output (1 hour)
4. ⬜ Constructor validation (1 hour)
5. ⬜ Parallel-safe run IDs (1 hour)
6. ⬜ Compile and test (1 hour)

### Phase 2: Python Optuna Runner (4 hours)
1. ⬜ Create `optuna_mrb_wf.py` with walk-forward (2 hours)
2. ⬜ Create `optuna_utils.py` with penalties (1 hour)
3. ⬜ Create `optuna_config.yaml` (30 min)
4. ⬜ Test single trial end-to-end (30 min)

### Phase 3: Validation & Deployment (2 hours)
1. ⬜ Run manual 5-fold test (1 hour)
2. ⬜ Launch Phase 1 optimization (200 trials, ~16 hours runtime)
3. ⬜ Monitor and analyze results (1 hour)

---

## NEXT STEPS (Priority Order)

1. **Implement `--params` JSON override** (Task 2)
   - Most critical for Optuna integration
   - Blocks Python runner development

2. **Implement `--json` output** (Task 3)
   - Required for robust metrics parsing
   - Eliminates regex fragility

3. **Create Python Optuna runner** (Task 6)
   - Can start in parallel with C++ work
   - Core optimization logic

4. **Add constructor validation** (Task 4)
   - Defense in depth
   - Catches bad parameters early

5. **Add parallel-safe run IDs** (Task 5)
   - Required for n_jobs>1
   - Performance optimization

6. **Testing & validation** (Task 7)
   - Critical before production use
   - Verify walk-forward logic

---

## EXPERT RECOMMENDATIONS COVERAGE

| Recommendation | Status | Tasks |
|----------------|--------|-------|
| Walk-forward folds | ✅ Partial (--skip-blocks done) | Task 6 (Python) |
| Soft penalties | ⬜ Not started | Task 6 |
| Reproducibility | ⬜ Not started | Task 6 |
| Parallel safety | ⬜ Not started | Task 5 |
| Parameter validation | ⬜ Not started | Task 4 |
| Trial artifacts | ⬜ Not started | Task 3, 5, 6 |

---

## ESTIMATED TIMELINE

| Phase | Tasks | Hours | Parallel? |
|-------|-------|-------|-----------|
| Phase 1 | C++ (Tasks 2-5) | 6 | Sequential |
| Phase 2 | Python (Task 6) | 4 | Can overlap with Phase 1 |
| Phase 3 | Testing (Task 7) | 2 | After Phases 1 & 2 |
| **Total** | | **12** | ~8 hours with parallelization |

**Plus:** 16 hours for Phase 1 optimization run (200 trials)

---

## BLOCKERS & RISKS

### Current Blockers:
- None (Task 1 complete, ready for Task 2)

### Potential Risks:
1. **JSON parsing complexity** - May need nlohmann/json or manual parsing
2. **Parameter override scope** - May need to modify generate-signals command too
3. **Parallel safety testing** - Need multiple cores to validate properly
4. **Optuna trial runtime** - 60s per trial × 200 trials × 5 folds = ~16 hours

### Mitigation:
- Use nlohmann/json (already in codebase)
- Start with subset of overridable parameters
- Test parallel safety with n_jobs=2 first
- Use early pruning to reduce trial time

---

## SUCCESS CRITERIA

### Phase 1 (C++ Complete):
- [ ] Can run: `sentio_cli backtest --blocks 4 --skip-blocks 10 --params '{"buy_threshold":0.55}'`
- [ ] JSON output parseable by Python
- [ ] No parameter validation errors for valid configs
- [ ] Unique run IDs for parallel invocations

### Phase 2 (Python Complete):
- [ ] Single trial completes successfully
- [ ] 5-fold evaluation returns mean MRB
- [ ] Soft penalties working correctly
- [ ] SQLite database created and queryable

### Phase 3 (Validation Complete):
- [ ] MRB improvement ≥+0.10% over baseline
- [ ] Best parameters generalize to QQQ
- [ ] No "inf" or "nan" values
- [ ] Fold-to-fold variance <0.05%

---

## FILES MANIFEST

### Modified:
- ✅ `src/cli/backtest_command.cpp` (--skip-blocks)
- ⬜ `src/cli/backtest_command.cpp` (--params)
- ⬜ `src/cli/analyze_trades_command.cpp` (--json)
- ⬜ `src/strategy/online_ensemble_strategy.cpp` (validation)

### To Create:
- ⬜ `tools/optuna_mrb_wf.py`
- ⬜ `tools/optuna_utils.py`
- ⬜ `config/optuna_config.yaml`
- ⬜ `tests/test_skip_blocks.sh` (integration test)

---

**END OF STATUS REPORT**
