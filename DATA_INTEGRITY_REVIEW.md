# Data Files and Data Management Code Consistency Review

## Executive Summary
This review identified multiple critical and moderate consistency violations in data files and data management code. The most significant issues are missing data files for configured symbols, inadequate exception handling in CSV parsing, and several unused/deprecated configuration parameters.

---

## 1. DATA FILE INTEGRITY ISSUES

### 1.1 Missing Data Files for Configured Symbols (CRITICAL)

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/config/rotation_strategy.json` (lines 6-32)

**Issue**: Configuration defines 20 active symbols but only 13 have corresponding data files.

**Configured Symbols**: ERX, ERY, FAS, FAZ, SDS, SSO, SQQQ, SVIX, TNA, TQQQ, TZA, UVXY, AAPL, MSFT, AMZN, TSLA, NVDA, META, BRK.B, GOOGL

**Missing Data Files**:
- AAPL_RTH_NH.csv
- MSFT_RTH_NH.csv
- AMZN_RTH_NH.csv
- TSLA_RTH_NH.csv
- NVDA_RTH_NH.csv
- META_RTH_NH.csv
- BRK.B_RTH_NH.csv
- GOOGL_RTH_NH.csv

**Data Files Available**:
- ERX_RTH_NH.csv (5,617 lines, 363K)
- ERY_RTH_NH.csv (6,042 lines, 389K)
- FAS_RTH_NH.csv (7,011 lines, 482K)
- FAZ_RTH_NH.csv (8,035 lines, 536K)
- SDS_RTH_NH.csv (8,610 lines, 615K)
- SSO_RTH_NH.csv (8,793 lines, 630K)
- SQQQ_RTH_NH.csv (8,994 lines, 641K)
- SVIX_RTH_NH.csv (8,291 lines, 545K)
- SVXY_RTH_NH.csv (7,896 lines, 512K)
- TNA_RTH_NH.csv (8,994 lines, 603K)
- TQQQ_RTH_NH.csv (8,994 lines, 661K)
- TZA_RTH_NH.csv (8,979 lines, 609K)
- UVXY_RTH_NH.csv (8,984 lines, 630K)

**Impact**: System will fail at runtime when attempting to load warmup data for missing symbols. Code has detection at `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp` (lines 377-392) that validates all symbols are loaded.

**Recommendation**: Either remove unused symbols from config or provide data files.

### 1.2 Empty/Stub Data Files (CRITICAL)

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/`

**Issue**: Four files contain only headers with no data:
- QQQ.csv (44 bytes, 1 line) - Header only: `timestamp,symbol,open,high,low,close,volume`
- SQQQ_NH.csv (44 bytes, 1 line) - Header only
- SQQQ.csv (44 bytes, 1 line) - Header only
- TQQQ.csv (44 bytes, 1 line) - Header only

**Impact**: These are stubs that should either be removed or populated with valid data. They will return empty bar vectors from `read_csv_data()`.

**Recommendation**: Remove or populate these stub files.

### 1.3 Inconsistent Row Counts Across Symbol Data Files

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/`

**Issue**: Data files have varying row counts, suggesting data collection started/ended at different times:

```
ERX:    5,617 lines (shortest - ~30 days)
ERY:    6,042 lines
FAS:    7,011 lines
FAZ:    8,035 lines
SDS:    8,610 lines
SSO:    8,793 lines
SQQQ:   8,994 lines
SVIX:   8,291 lines
SVXY:   7,896 lines
TNA:    8,994 lines
TQQQ:   8,994 lines
TZA:    8,979 lines
UVXY:   8,984 lines (longest - ~45+ days)
```

**Range**: 5,617 to 8,994 lines (57% difference between shortest and longest)

**Impact**: 
- ERX has significantly less data than other symbols
- Data timestamps may not align across symbols
- Rotation strategy may have insufficient warmup data for early entry signals

**Recommendation**: Verify data collection consistency; pad shorter datasets or document why data ranges differ.

---

## 2. CONFIGURATION CONSISTENCY VIOLATIONS

### 2.1 Unused Configuration Parameters (MODERATE)

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/config/rotation_strategy.json`

**Issue**: Configuration file includes parameters that are not used by the code:

**Not Implemented**:
1. `enable_kelly_sizing` (line 59) - Disabled in config but code references generic Kelly calculations
2. `kelly_fraction` (line 60) - Configured but not loaded from rotation_strategy.json
3. `enable_correlation_filter` (line 69) - Disabled in config, feature not implemented
4. `max_correlation` (line 70) - Configured but not used
5. `enable_threshold_calibration` (line 54) - Configured but not loaded from JSON
6. `calibration_window` (line 55) - Not used by rotation trading backend
7. `threshold_step` (line 57) - Not used by rotation trading backend
8. `target_win_rate` (line 56) - In "performance_targets" but not used for validation
9. `min_rank_to_hold` (line 78) - Not loaded by config parser
10. `rotation_cooldown_bars` (line 84) - Not loaded by config parser
11. `minimum_hold_bars` (line 85) - Not loaded by config parser
12. `volatility_weight` (line 88) - Disabled but referenced in code with hardcoded values
13. `capital_per_position` (line 89) - Configured but may not be used

**Code Location Issues**:
- `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp` (lines 147-248): Config loading code doesn't extract these parameters
- Parameters are defined in JSON but never extracted by `.value()` calls

**Recommendation**: Either implement missing features or remove unused config parameters.

### 2.2 Configuration Not Validated During Load

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp` (lines 147-248)

**Issue**: Configuration validation is minimal:
- No bounds checking on thresholds (buy_threshold, sell_threshold should be [0, 1])
- No verification that thresholds make sense (buy > sell expected)
- Leverage boost values not validated
- No check that symbols list is not empty
- Missing validation for numeric ranges

**Example Gap**: Line 174-175 loads buy/sell thresholds without validation:
```cpp
config.oes_config.buy_threshold = oes.value("buy_threshold", 0.53);
config.oes_config.sell_threshold = oes.value("sell_threshold", 0.47);
// No check: buy_threshold should be > sell_threshold
// No check: both should be in (0, 1) range
```

---

## 3. DATA VALIDATION ISSUES

### 3.1 No Exception Handling in CSV Parsing (HIGH)

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp` (lines 87-187)

**Issue**: `read_csv_data()` function uses `std::stod()`, `std::stoll()` without try-catch blocks:

```cpp
// Line 133: Can throw std::invalid_argument or std::out_of_range
b.timestamp_ms = std::stoll(trim(item)) * 1000;

// Lines 162-174: All can throw exceptions
b.open = std::stod(trim(item));
b.high = std::stod(trim(item));
b.low = std::stod(trim(item));
b.close = std::stod(trim(item));
b.volume = std::stod(trim(item));
```

**Impact**: Malformed CSV lines (e.g., "invalid_price" instead of "100.50") will crash the application instead of gracefully skipping rows.

**Contrast**: The warmup data loader in `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp` (lines 281-309) DOES have exception handling:
```cpp
try {
    // Parse fields
} catch (const std::exception& e) {
    continue;  // Skip malformed lines
}
```

**Recommendation**: Add similar try-catch blocks to `read_csv_data()`.

### 3.2 Incomplete Data Validation Before Use

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/data/multi_symbol_data_manager.cpp` (lines 253-315)

**Issue**: Good validation exists (`validate_bar()`) but inconsistently applied:
- ✓ `MultiSymbolDataManager` validates bars before adding (line 133)
- ✓ Checks OHLC consistency, price ranges, timestamps
- ✗ `read_csv_data()` in utils.cpp doesn't use this validation
- ✗ Warmup data loading applies minimal validation

**Gaps in validation**:
1. No check for missing columns in CSV (getline() may fail silently)
2. No validation that column count matches expected format
3. Timestamp monotonicity not checked during warmup load

---

## 4. FILE ORGANIZATION ISSUES

### 4.1 Tracked Log Files in Git (CRITICAL)

**Location**: Git status shows many log files tracked in `logs/` directory

**Issue**: Multiple log directories with JSONL files are tracked in git:
- `logs/october_12symbols/` (mentioned in .DS_Store)
- `logs/october_baseline_revert/`
- `logs/october_normalized_fix/` (with subdirectories containing signals.jsonl, trades.jsonl, etc.)
- `logs/october_svix_final/`, `logs/october_svix_fixed/`, etc.

**Problem**: These are test output files that shouldn't be version controlled (can be regenerated)

**Recommendation**: Add to .gitignore:
```
logs/**/*.jsonl
logs/**/*.log
```

### 4.2 Temporary Test Data Not Cleaned Up

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/todays_bars/`

**Issue**: Contains today's market data (created Oct 16 11:26):
- ERX_today.csv (79 bytes)
- ERY_today.csv (907 bytes)
- FAS_today.csv (259 bytes)
- FAZ_today.csv (591 bytes)
- SDS_today.csv (2,275 bytes)
- SQQQ_today.csv (4,767 bytes)
- SSO_today.csv (3,052 bytes)
- SVXY_today.csv (3,081 bytes)
- TNA_today.csv (3,378 bytes)
- TQQQ_today.csv (4,953 bytes)
- TZA_today.csv (3,980 bytes)
- UVXY_today.csv (4,571 bytes)

**Status**: These are marked as untracked (`??`) in git status but should be cleaned up.

**Recommendation**: Add explicit cleanup or document expected lifetime of these files.

### 4.3 Multiple Test/Stub Data Directories

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/`

**Issue**: Numerous test directories in data/ that should be cleaned:
```
data/tmp/baseline_full_test/
data/tmp/extensive_phase1/
data/tmp/optuna_58features/
data/tmp/optuna_fixed_features/
data/tmp/quick_baseline/
data/tmp/single_day_test/
data/tmp/cache_test/
data/tmp/improvement_test/
... (many more)
```

**Impact**: Wastes disk space, may confuse developers about which is current test data.

---

## 5. SYMBOL DATA CONSISTENCY

### 5.1 SVIX Symbol Status Uncertain

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SVIX_RTH_NH.csv`

**File Status**:
- ✓ File exists with 8,291 lines
- ✓ Valid CSV format with proper headers
- ✓ Data range: 2025-09-15 to 2025-10-15 (31 days)
- ✓ Data appears complete and valid

**Question**: SVIX is in config but is it actually used in trading logic? Review shows no specific handling code for volatility symbols.

### 5.2 Data Files Not Referenced in Code

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/`

**Unused Data Files** (present but not in active config):
- PSQ_RTH_NH.csv (16 MB) - Not in config
- QQQ_RTH_NH.csv (21 MB) - Large, may be reference data
- QQQ_1min_merged.csv (29 MB) - Not referenced
- SPY_RTH_NH.csv and variants - Not in rotation config
- Various test files (QQQ_test_small.csv, SPY_*blocks.csv, etc.)

**Recommendation**: Document purpose of large files or remove if obsolete.

---

## 6. CODE-LEVEL DATA LOADING ISSUES

### 6.1 No Bounds Checking on Data Values

**Location**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp` (lines 160-174)

**Issue**: No validation of parsed numeric values:
```cpp
b.open = std::stod(trim(item));  // Could be negative, infinite, NaN
b.high = std::stod(trim(item));
b.low = std::stod(trim(item));
b.close = std::stod(trim(item));
b.volume = std::stod(trim(item)); // Could be negative
```

**Validation Applied Later**: `MultiSymbolDataManager::validate_bar()` does check these (lines 272-315):
- Price bounds: 0.01 < price < 10,000
- OHLC consistency: low <= close <= high, low <= open <= high
- Volume non-negative

**Problem**: Data is validated AFTER loading into system, not during CSV parsing. Invalid data can remain in vectors.

---

## 7. DATA MANAGER CONFIGURATION UNUSED

**Location**: `config/rotation_strategy.json` (lines 100-105)

```json
"data_manager_config": {
  "max_forward_fills": 5,
  "max_staleness_seconds": 300.0,
  "history_size": 500,
  "log_data_quality": true
}
```

**Issue**: These parameters are defined but not loaded by rotation trading command:
- No `.value()` calls for these in `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp`
- Values appear to be hardcoded in data manager initialization

---

## SUMMARY TABLE: Issues by Severity

| Issue | Severity | Type | File | Line | Status |
|-------|----------|------|------|------|--------|
| Missing data files for 8 configured symbols | CRITICAL | Config | rotation_strategy.json | 6-32 | Blocks execution |
| Empty stub CSV files (4 files) | CRITICAL | Data | data/equities/ | - | Returns empty vectors |
| No exception handling in CSV parsing | HIGH | Code | utils.cpp | 133, 162-174 | Can crash on malformed data |
| Tracked log files in git | CRITICAL | Git | logs/ | - | Version control bloat |
| Unused config parameters (13+ items) | MODERATE | Config | rotation_strategy.json | Multiple | Dead code/confusion |
| No config validation (bounds, ranges) | MODERATE | Code | rotation_trade_command.cpp | 147-248 | Invalid values accepted |
| Inconsistent data file row counts | MODERATE | Data | data/equities/ | - | Alignment issues |
| Unimplemented features referenced in config | MODERATE | Config | rotation_strategy.json | - | Config drift |
| Temporary files not cleaned | LOW | Files | data/tmp/todays_bars/ | - | Disk clutter |

---

## RECOMMENDATIONS (Priority Order)

1. **IMMEDIATE (Blocking)**:
   - Remove the 8 missing symbols from config OR provide data files
   - Remove or populate the 4 empty CSV stub files
   - Add try-catch blocks to `read_csv_data()` function

2. **URGENT (Before Next Release)**:
   - Add proper .gitignore rules for log files
   - Add configuration validation (bounds checking)
   - Document or implement unused config parameters
   - Align data file row counts

3. **MEDIUM (Next Sprint)**:
   - Implement data manager config loading
   - Add validation during CSV parsing (not after)
   - Document purpose of large/test data files
   - Clean up test directories

4. **LOW (Technical Debt)**:
   - Consolidate test data
   - Remove obsolete data files
   - Document data file naming conventions

