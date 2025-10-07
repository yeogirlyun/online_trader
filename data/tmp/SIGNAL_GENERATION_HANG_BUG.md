# Bug Report: Signal Generation Hangs on Datasets Larger Than ~500 Bars

**Date:** October 7, 2025
**Severity:** Critical
**Status:** Unresolved
**Reporter:** System Analysis

---

## Summary

The `sentio_cli generate-signals` command hangs indefinitely (infinite loop or deadlock) when processing datasets larger than approximately 500-1000 bars. The program does not produce any output, not even the initial header messages, indicating the hang occurs very early in the execution, likely during initialization or data loading.

---

## Reproduction Steps

1. **Working Case (500 bars):**
   ```bash
   ./sentio_cli generate-signals \
     --data ../data/equities/SPY_test_small.csv \  # 500 bars
     --output ../data/tmp/spy_test_signals.jsonl \
     --verbose
   ```
   - **Result:** Completes successfully in ~15 seconds
   - **Output:** 499 signals generated correctly

2. **Hanging Case (9,600 bars - 20 blocks):**
   ```bash
   ./sentio_cli generate-signals \
     --data ../data/equities/SPY_20blocks.csv \  # 9,601 bars
     --output ../data/tmp/spy_20block_signals.jsonl \
     --verbose
   ```
   - **Result:** Hangs indefinitely (>2 minutes with no output)
   - **Output:** No output file created, log file completely empty

3. **Hanging Case (48,000 bars - 100 blocks):**
   ```bash
   ./sentio_cli generate-signals \
     --data ../data/equities/SPY_100blocks.csv \  # 48,001 bars
     --output ../data/tmp/spy_100block_signals.jsonl \
     --verbose
   ```
   - **Result:** Hangs indefinitely (>5 minutes with no output)
   - **Output:** No output file created, log file completely empty

---

## Symptoms

1. **No console output:** The program produces zero output, not even the header:
   ```
   === OnlineEnsemble Signal Generation ===
   ```
   This indicates the hang occurs before line 30 of `generate_signals_command.cpp`

2. **High CPU usage:** Process consumes 95-98% CPU initially, then drops to 40-50%

3. **No output file created:** The output JSONL file is never created

4. **Log files empty:** When redirected to log files, they remain completely empty

5. **Process never terminates:** Must be killed manually with `pkill`

---

## Analysis

### Timeline of Execution
Based on the code structure, the execution should follow this path:

1. **Parse arguments** (lines 16-22)
2. **Print header** (lines 30-33) ← **NEVER REACHES HERE**
3. **Load market data** (line 37: `utils::read_csv_data(data_path)`)
4. **Process bars** (lines 68-99)
5. **Save output** (lines 101+)

Since no output is produced at all, the hang must occur in steps 1-2, most likely:
- During static initialization before `main()`
- During argument parsing
- **Most likely:** During the CSV data loading function (but before the "Loading market data..." message prints)

### Hypothesis: CSV Reader Deadlock/Infinite Loop

The most probable cause is in the `utils::read_csv_data()` function, which appears to hang on files larger than ~500 bars but smaller than ~1000 bars. Potential issues:

1. **Memory allocation issue:** Large CSV files might trigger a memory allocation bug
2. **Buffer overflow:** String parsing might have buffer issues with large files
3. **Infinite loop in parser:** CSV parsing logic may have edge cases with certain data patterns
4. **I/O blocking:** File reading might block indefinitely on large files
5. **Static initialization order fiasco:** Global object construction might deadlock

### Data Format Verification

The CSV format appears correct:
```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2025-04-09T14:34:00-04:00,1744223640,537.35,537.35,535.1,536.26,671820.0
```

- Working file: 500 lines (499 data rows)
- Hanging file: 9,601 lines (9,600 data rows)
- Hanging file: 48,001 lines (48,000 data rows)

---

## Relevant Source Modules

### Primary Suspects

1. **`src/cli/generate_signals_command.cpp`** (lines 12-48)
   - Main entry point for signal generation
   - Calls `utils::read_csv_data()` at line 37
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/generate_signals_command.cpp`

2. **`src/common/utils.cpp`** (location unknown - needs investigation)
   - Contains `utils::read_csv_data()` function
   - **CRITICAL:** This is likely where the hang occurs
   - Need to find and examine this implementation

3. **`include/common/utils.h`** (location unknown)
   - Header declaring `read_csv_data()`
   - Need to examine function signature and dependencies

### Secondary Modules (Strategy Initialization)

4. **`src/strategy/online_ensemble_strategy.cpp`**
   - Constructor may have initialization issues
   - Created at line 61 of generate_signals_command.cpp
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/online_ensemble_strategy.cpp`

5. **`src/strategy/online_ensemble_strategy.h`**
   - Config struct definition
   - May have static initialization issues
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/online_ensemble_strategy.h`

### Support Modules

6. **`src/cli/command_interface.cpp`**
   - Command line argument parsing
   - Possible early hang location
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/command_interface.cpp`

7. **`src/cli/command_registry.cpp`**
   - Command registration system
   - May have static initialization order issues
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/command_registry.cpp`

8. **`CMakeLists.txt`** and build configuration
   - Compiler optimization flags may cause issues
   - Debug vs Release build differences
   - File: `/Volumes/ExternalSSD/Dev/C++/online_trader/CMakeLists.txt`

---

## Test Data Files

### Working
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_test_small.csv` (500 lines)
- Size: ~36 KB
- Status: ✅ Works

### Hanging
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_20blocks.csv` (9,601 lines)
- Size: ~692 KB
- Status: ❌ Hangs

- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_30blocks.csv` (14,401 lines)
- Size: ~1.0 MB
- Status: ❌ Hangs (untested but likely)

- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_100blocks.csv` (48,001 lines)
- Size: ~3.5 MB
- Status: ❌ Hangs

---

## Impact

**Critical:** This bug completely blocks the ability to:
1. Generate signals for realistic test datasets (20-100 blocks)
2. Run backtests on historical data
3. Validate strategy performance
4. Generate dashboards with performance metrics

---

## Root Cause Analysis - CSV Reader Implementation

### Found: `src/common/utils.cpp::read_csv_data()` (lines 87-186)

The CSV reader implementation has been located. Key findings:

**Missing Symbol Detection for SPY:**
Lines 111-113 only check for QQQ, SQQQ, TQQQ:
```cpp
if (filename.find("QQQ") != std::string::npos) default_symbol = "QQQ";
else if (filename.find("SQQQ") != std::string::npos) default_symbol = "SQQQ";
else if (filename.find("TQQQ") != std::string::npos) default_symbol = "TQQQ";
// SPY is MISSING! Defaults to "UNKNOWN"
```

**Potential Hang Locations:**
1. **Line 133:** `std::stoll(trim(item)) * 1000` - Could throw exception or hang on malformed data
2. **Line 177:** `generate_bar_id(b.timestamp_ms, b.symbol)` - Unknown function, could be expensive
3. **Line 180:** `ms_to_timestamp(b.timestamp_ms)` - Time conversion could be slow
4. **Line 182:** `bars.push_back(b)` - Vector reallocation could be inefficient for large datasets

**No Progress Reporting:**
The CSV reader has zero progress output, making it impossible to determine where it's stuck.

### Suspect Functions to Investigate:

1. **`trim()`** - Called 7 times per row
2. **`generate_bar_id()`** - Called once per row (unknown implementation)
3. **`ms_to_timestamp()`** - Called once per row
4. **`std::stoll()` and `std::stod()`** - Called 6 times per row

With 48,000 rows, any function that's O(n²) or has performance issues will cause severe slowdowns.

## Recommended Investigation Steps

1. **Add debug logging to CSV reader (CRITICAL):**
   - Print before/after file open
   - Print progress every N lines
   - Print memory usage

3. **Run with debugger:**
   ```bash
   lldb ./sentio_cli
   (lldb) breakpoint set -n main
   (lldb) run generate-signals --data ../data/equities/SPY_20blocks.csv ...
   ```

4. **Check for static initialization order:**
   - Look for global objects with complex constructors
   - Check for circular dependencies in static initializers

5. **Test with binary format:**
   - SPY_RTH_NH.bin (36 MB) exists
   - May bypass CSV parsing issue

6. **Memory profiling:**
   ```bash
   valgrind --tool=massif ./sentio_cli generate-signals ...
   ```

7. **Compare with QQQ data:**
   - QQQ data files are known to work in other parts of the system
   - Test if QQQ exhibits the same issue

---

## Workarounds

**None available.** The signal generation is completely blocked for realistic dataset sizes.

---

## Next Steps

1. Find and examine `utils::read_csv_data()` implementation
2. Add extensive debug logging to identify exact hang location
3. Test with binary data format if supported
4. Consider rewriting CSV reader with proven library (e.g., fast-cpp-csv-parser)
5. Add timeout and progress reporting to prevent silent hangs

---

## Detailed Source Code Analysis

### 1. CSV Reader Implementation
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp`
**Function:** `read_csv_data()` (lines 87-186)

**Performance Characteristics:**
- Called once per file
- Iterates through all rows (line 118: `while (std::getline(file, line))`)
- Per row operations:
  - 7x `trim()` calls (lines 133, 138, 141, 147, 150, 155, 157, 162, 165, 168, 171, 174)
  - 1x `std::hash<std::string>{}(symbol)` call (line 177 via `generate_bar_id()`)
  - 1x `ms_to_timestamp()` call (line 180)
  - 1x `bars.push_back(b)` (line 182)

**Issues Found:**
1. **No progress reporting** - Silent operation makes debugging impossible
2. **Missing SPY symbol detection** - SPY defaults to "UNKNOWN" (lines 111-113)
3. **Inefficient vector growth** - No `.reserve()` call before loop
4. **Hash computation in tight loop** - Line 177: `generate_bar_id()` hashes symbol on every row

### 2. Helper Functions
**Function:** `generate_bar_id()` (lines 47-52)
```cpp
uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol) {
    uint64_t timestamp_part = static_cast<uint64_t>(timestamp_ms) & 0xFFFFFFFFFFFFULL;
    uint32_t symbol_hash = static_cast<uint32_t>(std::hash<std::string>{}(symbol)); // EXPENSIVE
    uint64_t symbol_part = (static_cast<uint64_t>(symbol_hash) & 0xFFFFULL) << 48;
    return timestamp_part | symbol_part;
}
```
- Called once per row (48,000 times for 100-block dataset)
- Computes string hash every time (could be memoized)

**Function:** `trim()` (lines 67-73)
```cpp
static inline std::string trim(const std::string& s) {
    const char* ws = " \t\n\r\f\v";
    const auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}
```
- Called 7 times per row (336,000 times for 48,000 rows)
- Creates new string each time (inefficient)

**Function:** `ms_to_timestamp()` (lines 330-336)
```cpp
std::string ms_to_timestamp(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm* gmt = gmtime(&t);  // THREAD-UNSAFE, POTENTIALLY SLOW
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}
```
- Called once per row (48,000 times)
- Uses `gmtime()` which can be slow
- `gmtime()` is not thread-safe

### 3. Signal Generation Command
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/generate_signals_command.cpp`
**Function:** `execute()` (lines 12-48)

**Execution Flow:**
1. Parse arguments (lines 16-22)
2. Print header (lines 30-33) ← **NEVER PRINTS FOR LARGE FILES**
3. Load data: `utils::read_csv_data(data_path)` (line 37)
4. Process bars (lines 68-99)

**Issue:** No output before line 37 means hang occurs during or before `read_csv_data()` call.

### 4. Strategy Initialization
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/online_ensemble_strategy.cpp`
**Constructor:** `OnlineEnsembleStrategy(config)` (line 61 of generate_signals_command.cpp)

This occurs AFTER the CSV reader, so it's not the cause of the hang.

---

## Performance Calculation

For a 48,000-bar dataset:
- 48,000 × 7 `trim()` calls = 336,000 string operations
- 48,000 × 1 `generate_bar_id()` = 48,000 hash computations
- 48,000 × 1 `ms_to_timestamp()` = 48,000 time conversions
- 48,000 × 1 `push_back()` = multiple vector reallocations

**Estimated time at 10,000 rows/sec:** ~5 seconds
**Actual time:** >300 seconds (5+ minutes) = **60x slower than expected**

This suggests either:
1. Performance is worse than O(n) - possibly O(n²)
2. There's an exception handling loop
3. Memory allocation is thrashing

---

## Related Files - Complete List

### Core Implementation
1. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp`**
   - Lines 47-52: `generate_bar_id()` - Hash computation
   - Lines 67-73: `trim()` - String trimming
   - Lines 87-186: `read_csv_data()` - Main CSV reader
   - Lines 330-336: `ms_to_timestamp()` - Time conversion

2. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/common/utils.h`**
   - Declaration of `read_csv_data()`
   - Declaration of helper functions

3. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/generate_signals_command.cpp`**
   - Lines 12-48: Main execution logic
   - Line 37: Call to `read_csv_data()`

### Secondary Files
4. **`/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/online_ensemble_strategy.cpp`**
   - Strategy initialization (after CSV load)

5. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/online_ensemble_strategy.h`**
   - Config struct definition

6. **`/Volumes/ExternalSSD/Dev/C++/online_trader/include/common/types.h`**
   - Bar struct definition
   - Possible struct initialization issues

7. **`/Volumes/ExternalSSD/Dev/C++/online_trader/CMakeLists.txt`**
   - Build configuration
   - Optimization flags

---

## Recommended Fixes

### Immediate (Debugging):
1. Add progress logging to `read_csv_data()`:
   ```cpp
   if (sequence_index % 1000 == 0) {
       std::cout << "Loaded " << sequence_index << " bars...\r" << std::flush;
   }
   ```

2. Add SPY to symbol detection (line 113):
   ```cpp
   else if (filename.find("SPY") != std::string::npos) default_symbol = "SPY";
   else if (filename.find("SPXL") != std::string::npos) default_symbol = "SPXL";
   else if (filename.find("SH") != std::string::npos) default_symbol = "SH";
   else if (filename.find("SPXS") != std::string::npos) default_symbol = "SPXS";
   else if (filename.find("SDS") != std::string::npos) default_symbol = "SDS";
   ```

### Performance Improvements:
1. Reserve vector capacity before loop:
   ```cpp
   bars.reserve(50000); // Estimate based on typical file size
   ```

2. Memoize symbol hash:
   ```cpp
   static uint32_t symbol_hash = std::hash<std::string>{}(default_symbol);
   ```

3. Use binary format instead of CSV for large datasets

4. Replace `gmtime()` with `gmtime_r()` (thread-safe version)

---

**End of Bug Report**
