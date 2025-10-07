# Signal Generation Hang Bug Report - Complete Analysis

**Generated**: 2025-10-07 14:37:10
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: File list (12 files)
**Description**: Complete bug report with all related source modules for signal generation hanging issue on datasets >500 bars

**Total Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [CMakeLists.txt](#file-1)
2. [data/tmp/BUG_REPORT_SOURCE_MODULES.md](#file-2)
3. [data/tmp/SIGNAL_GENERATION_HANG_BUG.md](#file-3)
4. [include/cli/ensemble_workflow_command.h](#file-4)
5. [include/common/types.h](#file-5)
6. [include/common/utils.h](#file-6)
7. [include/strategy/online_ensemble_strategy.h](#file-7)
8. [src/cli/command_interface.cpp](#file-8)
9. [src/cli/command_registry.cpp](#file-9)
10. [src/cli/generate_signals_command.cpp](#file-10)
11. [src/common/utils.cpp](#file-11)
12. [src/strategy/online_ensemble_strategy.cpp](#file-12)

---

## 📄 **FILE 1 of 12**: CMakeLists.txt

**File Information**:
- **Path**: `CMakeLists.txt`

- **Size**: 339 lines
- **Modified**: 2025-10-07 13:37:11

- **Type**: .txt

```text
cmake_minimum_required(VERSION 3.16)
project(online_trader VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Performance optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling performance optimizations for Release build")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -funroll-loops -DNDEBUG")
    add_compile_definitions(NDEBUG)
    
    # Enable OpenMP for parallel processing if available
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found - enabling parallel processing")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fopenmp")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find Eigen3 for online learning (REQUIRED for this project)
find_package(Eigen3 3.3 REQUIRED)
message(STATUS "Eigen3 found - Online learning support enabled")
message(STATUS "Eigen3 version: ${EIGEN3_VERSION}")
message(STATUS "Eigen3 include: ${EIGEN3_INCLUDE_DIR}")

# Find nlohmann/json for JSON parsing
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json found - enabling robust JSON parsing")
    add_compile_definitions(NLOHMANN_JSON_AVAILABLE)
else()
    message(STATUS "nlohmann/json not found - using header-only fallback")
endif()

# =============================================================================
# Common Library
# =============================================================================
add_library(online_common
    src/common/types.cpp
    src/common/utils.cpp
    src/common/json_utils.cpp
    src/common/trade_event.cpp
    src/common/binary_data.cpp
    src/core/data_io.cpp
    src/core/data_manager.cpp
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_common PRIVATE nlohmann_json::nlohmann_json)
endif()

# =============================================================================
# Strategy Library (Base Framework for Online Learning)
# =============================================================================
set(STRATEGY_SOURCES
    src/strategy/istrategy.cpp
    src/strategy/ml_strategy_base.cpp
    src/strategy/online_strategy_base.cpp
    src/strategy/strategy_component.cpp
    src/strategy/signal_output.cpp
    src/strategy/trading_state.cpp
    src/strategy/online_ensemble_strategy.cpp
)

# Add unified feature engine for online learning
list(APPEND STRATEGY_SOURCES src/features/unified_feature_engine.cpp)

add_library(online_strategy ${STRATEGY_SOURCES})
target_link_libraries(online_strategy PRIVATE online_common)
target_link_libraries(online_strategy PUBLIC Eigen3::Eigen)
target_include_directories(online_strategy PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_strategy PRIVATE nlohmann_json::nlohmann_json)
endif()

# Link OpenMP if available for performance optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND OpenMP_CXX_FOUND)
    target_link_libraries(online_strategy PRIVATE OpenMP::OpenMP_CXX)
endif()

# =============================================================================
# Backend Library (Ensemble PSM for Online Learning)
# =============================================================================
add_library(online_backend
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
    src/backend/audit_component.cpp
    src/backend/leverage_manager.cpp
    src/backend/adaptive_portfolio_manager.cpp
    src/backend/adaptive_trading_mechanism.cpp
    src/backend/position_state_machine.cpp
    # Enhanced Dynamic PSM components
    src/backend/dynamic_hysteresis_manager.cpp
    src/backend/dynamic_allocation_manager.cpp
    src/backend/enhanced_position_state_machine.cpp
    src/backend/enhanced_backend_component.cpp
    # Ensemble PSM for online learning (KEY COMPONENT)
    src/backend/ensemble_position_state_machine.cpp
)
target_link_libraries(online_backend PRIVATE online_common)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_backend PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_backend PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Online Learning Library (Core Focus of This Project)
# =============================================================================
add_library(online_learning
    src/learning/online_predictor.cpp
)
target_link_libraries(online_learning PUBLIC 
    online_common 
    online_strategy
    Eigen3::Eigen
)
target_include_directories(online_learning PUBLIC
    ${EIGEN3_INCLUDE_DIR}
)
message(STATUS "Created online_learning library with Eigen3 support")

# =============================================================================
# Testing Framework
# =============================================================================
add_library(online_testing_framework STATIC
    # Core Testing Framework
    src/testing/test_framework.cpp
    src/testing/test_result.cpp
    src/testing/enhanced_test_framework.cpp

    # Validation
    src/validation/strategy_validator.cpp
    src/validation/validation_result.cpp
    src/validation/walk_forward_validator.cpp
    src/validation/bar_id_validator.cpp

    # Analysis
    src/analysis/performance_metrics.cpp
    src/analysis/performance_analyzer.cpp
    src/analysis/temp_file_manager.cpp
    src/analysis/statistical_tests.cpp
    src/analysis/enhanced_performance_analyzer.cpp
)

target_include_directories(online_testing_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(online_testing_framework
    PUBLIC
        online_strategy      # Strategy implementation library
        online_backend       # Backend components
    PRIVATE
        online_common        # Common utilities (only needed internally)
)

# Link nlohmann/json if available
if(nlohmann_json_FOUND)
    target_link_libraries(online_testing_framework PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(online_testing_framework PRIVATE /opt/homebrew/include)
endif()

# =============================================================================
# Live Trading Library (Alpaca + Polygon WebSocket Integration)
# =============================================================================
find_package(CURL REQUIRED)

# Find libwebsockets for real-time Polygon data
find_library(WEBSOCKETS_LIB websockets HINTS /opt/homebrew/lib)
if(WEBSOCKETS_LIB)
    message(STATUS "libwebsockets found: ${WEBSOCKETS_LIB}")
else()
    message(FATAL_ERROR "libwebsockets not found - install with: brew install libwebsockets")
endif()

add_library(online_live
    src/live/alpaca_client.cpp
    src/live/polygon_websocket.cpp
)
target_link_libraries(online_live PRIVATE
    online_common
    CURL::libcurl
    ${WEBSOCKETS_LIB}
)
target_include_directories(online_live PRIVATE /opt/homebrew/include)
if(nlohmann_json_FOUND)
    target_link_libraries(online_live PRIVATE nlohmann_json::nlohmann_json)
endif()
message(STATUS "Created online_live library for live trading (Alpaca + Polygon WebSocket)")

# =============================================================================
# CLI Executable (sentio_cli for online learning)
# =============================================================================
add_executable(sentio_cli
    src/cli/sentio_cli_main.cpp
    src/cli/command_interface.cpp
    src/cli/command_registry.cpp
    src/cli/parameter_validator.cpp
    # Online learning commands (commented out - missing XGBFeatureSet implementations)
    # src/cli/online_command.cpp
    # src/cli/online_sanity_check_command.cpp
    # src/cli/online_trade_command.cpp
    # OnlineEnsemble workflow commands
    src/cli/generate_signals_command.cpp
    src/cli/execute_trades_command.cpp
    src/cli/analyze_trades_command.cpp
    # Live trading command
    src/cli/live_trade_command.cpp
)

# Link all required libraries
target_link_libraries(sentio_cli PRIVATE
    online_learning
    online_strategy
    online_backend
    online_common
    online_testing_framework
    online_live
)

# Add nlohmann/json include for CLI
if(nlohmann_json_FOUND)
    target_link_libraries(sentio_cli PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(sentio_cli PRIVATE /opt/homebrew/include)
endif()

message(STATUS "Created sentio_cli executable with online learning support")

# Create standalone test executable for online learning
add_executable(test_online_trade tools/test_online_trade.cpp)
target_link_libraries(test_online_trade PRIVATE 
    online_learning
    online_strategy
    online_backend
    online_common
)
message(STATUS "Created test_online_trade executable")

# =============================================================================
# Utility Tools
# =============================================================================
# CSV to Binary Converter Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/csv_to_binary_converter.cpp")
    add_executable(csv_to_binary_converter tools/csv_to_binary_converter.cpp)
    target_link_libraries(csv_to_binary_converter PRIVATE online_common)
    message(STATUS "Created csv_to_binary_converter tool")
endif()

# Dataset Analysis Tool
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/analyze_dataset.cpp")
    add_executable(analyze_dataset tools/analyze_dataset.cpp)
    target_link_libraries(analyze_dataset PRIVATE online_common)
    message(STATUS "Created analyze_dataset tool")
endif()

# =============================================================================
# Unit Tests (optional)
# =============================================================================
if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        
        # Framework tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_framework_test.cpp")
            add_executable(test_framework_tests
                tests/test_framework_test.cpp
            )
            target_link_libraries(test_framework_tests
                PRIVATE
                    online_testing_framework
                    GTest::gtest_main
            )
            add_test(NAME TestFrameworkTests COMMAND test_framework_tests)
        endif()
        
        # Dynamic PSM Tests
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_dynamic_hysteresis.cpp")
            add_executable(test_dynamic_hysteresis
                tests/test_dynamic_hysteresis.cpp
            )
            target_link_libraries(test_dynamic_hysteresis
                PRIVATE
                    online_backend
                    online_strategy
                    online_common
                    GTest::gtest_main
            )
            add_test(NAME DynamicHysteresisTests COMMAND test_dynamic_hysteresis)
        endif()
        
        message(STATUS "Testing framework enabled with GTest")
    else()
        message(STATUS "GTest not found - skipping testing targets")
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================
# Add quote simulation module
# =============================================================================
add_subdirectory(quote_simulation)

# =============================================================================
install(TARGETS online_testing_framework online_learning online_strategy online_backend online_common quote_simulation
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "========================================")
message(STATUS "Online Trader Configuration Summary:")
message(STATUS "  - Eigen3: ${EIGEN3_VERSION}")
message(STATUS "  - Online Learning: ENABLED")
message(STATUS "  - Ensemble PSM: ENABLED")
message(STATUS "  - Strategy Framework: ENABLED")
message(STATUS "  - Testing Framework: ENABLED")
message(STATUS "  - Quote Simulation: ENABLED")
message(STATUS "  - MarS Integration: ENABLED")
message(STATUS "========================================")

```

## 📄 **FILE 2 of 12**: data/tmp/BUG_REPORT_SOURCE_MODULES.md

**File Information**:
- **Path**: `data/tmp/BUG_REPORT_SOURCE_MODULES.md`

- **Size**: 182 lines
- **Modified**: 2025-10-07 14:35:18

- **Type**: .md

```text
# Signal Generation Hang - Source Module Reference

**Bug Report:** See `SIGNAL_GENERATION_HANG_BUG.md` for full details

---

## Core Modules (Primary Suspects)

### 1. CSV Data Reader
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp`
- **Line 87-186:** `read_csv_data()` - Main CSV parser
- **Line 47-52:** `generate_bar_id()` - Bar ID generation with string hashing
- **Line 67-73:** `trim()` - String trimming helper (called 7x per row)
- **Line 330-336:** `ms_to_timestamp()` - Time conversion (thread-unsafe `gmtime()`)

**Issues:**
- No progress reporting
- Missing SPY symbol detection (defaults to "UNKNOWN")
- No vector `.reserve()` before loading
- Hash computed 48,000 times for identical symbol

**Header:**
`/Volumes/ExternalSSD/Dev/C++/online_trader/include/common/utils.h`

---

### 2. Signal Generation Command
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/generate_signals_command.cpp`
- **Line 12-48:** `execute()` - Main entry point
- **Line 37:** Call to `utils::read_csv_data(data_path)` - **HANG LOCATION**

**Execution Flow:**
```
execute() → read_csv_data() → [HANGS HERE] → strategy init → signal generation
```

**Header:**
`/Volumes/ExternalSSD/Dev/C++/online_trader/include/cli/ensemble_workflow_command.h`

---

## Secondary Modules

### 3. Strategy Implementation
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/src/strategy/online_ensemble_strategy.cpp`
- Strategy constructor (executed AFTER CSV load completes)
- Not the cause of hang, but relevant for full workflow

**Header:**
`/Volumes/ExternalSSD/Dev/C++/online_trader/include/strategy/online_ensemble_strategy.h`

---

### 4. Common Types
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/include/common/types.h`
- `Bar` struct definition
- Used for storing each CSV row
- Potential initialization issues

---

### 5. Command Infrastructure
**Files:**
- `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/command_interface.cpp`
  - Command line argument parsing

- `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/command_registry.cpp`
  - Command registration system
  - Potential static initialization order issues

---

### 6. Build Configuration
**File:** `/Volumes/ExternalSSD/Dev/C++/online_trader/CMakeLists.txt`
- Compiler optimization flags
- Debug vs Release build settings
- May affect performance characteristics

---

## Helper Functions Called in Hot Path

From `src/common/utils.cpp`:

1. **`trim()` (line 67)**
   - Called: 336,000 times for 48,000 rows (7x per row)
   - Allocates new string each time

2. **`generate_bar_id()` (line 47)**
   - Called: 48,000 times
   - Computes `std::hash<std::string>{}(symbol)` each time
   - Should be memoized

3. **`ms_to_timestamp()` (line 330)**
   - Called: 48,000 times
   - Uses `gmtime()` which is thread-unsafe and slow

4. **`std::vector::push_back()` (line 182)**
   - Called: 48,000 times
   - Causes multiple reallocations without `.reserve()`

---

## Data Files for Testing

### Working (Control)
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_test_small.csv`
  - Size: 500 lines (~36 KB)
  - Status: ✅ Works in ~15 seconds

### Failing (Reproduce Bug)
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_20blocks.csv`
  - Size: 9,601 lines (~692 KB)
  - Status: ❌ Hangs (>2 minutes)

- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_100blocks.csv`
  - Size: 48,001 lines (~3.5 MB)
  - Status: ❌ Hangs (>5 minutes)

### Full Dataset
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_RTH_NH.csv`
  - Size: 488,904 lines (~35 MB)
  - Status: ❌ Hangs indefinitely

### Binary Format (Alternative)
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_RTH_NH.bin`
  - Size: 36 MB
  - Status: ⚠️ Untested (may bypass CSV reader issue)

---

## Investigation Commands

### Find CSV reader calls
```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
grep -r "read_csv_data" src/ include/
```

### Find generate_bar_id calls
```bash
grep -r "generate_bar_id" src/ include/
```

### Check for static initialization
```bash
grep -r "static.*=" src/ include/ | grep -v "inline"
```

### Build with debug symbols
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make sentio_cli
```

### Run with debugger
```bash
lldb ./sentio_cli
(lldb) breakpoint set -f utils.cpp -l 87
(lldb) run generate-signals --data ../data/equities/SPY_20blocks.csv --output test.jsonl
(lldb) continue
# Step through to find hang location
```

---

## Quick Reference: Line Numbers

| File | Function | Lines | Issue |
|------|----------|-------|-------|
| `utils.cpp` | `read_csv_data()` | 87-186 | Main hang location |
| `utils.cpp` | `generate_bar_id()` | 47-52 | Unnecessary hash computation |
| `utils.cpp` | `trim()` | 67-73 | String allocation overhead |
| `utils.cpp` | `ms_to_timestamp()` | 330-336 | Thread-unsafe `gmtime()` |
| `generate_signals_command.cpp` | `execute()` | 12-48 | Entry point |
| `generate_signals_command.cpp` | CSV load call | 37 | Hang occurs here |

---

**Last Updated:** October 7, 2025
**Status:** Unresolved - awaiting performance fix or binary format support

```

## 📄 **FILE 3 of 12**: data/tmp/SIGNAL_GENERATION_HANG_BUG.md

**File Information**:
- **Path**: `data/tmp/SIGNAL_GENERATION_HANG_BUG.md`

- **Size**: 440 lines
- **Modified**: 2025-10-07 14:34:43

- **Type**: .md

```text
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

```

## 📄 **FILE 4 of 12**: include/cli/ensemble_workflow_command.h

**File Information**:
- **Path**: `include/cli/ensemble_workflow_command.h`

- **Size**: 263 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "cli/command_interface.h"
#include "strategy/online_ensemble_strategy.h"
#include "backend/adaptive_portfolio_manager.h"
#include "backend/position_state_machine.h"
#include <string>
#include <vector>
#include <memory>

namespace sentio {
namespace cli {

/**
 * @brief Complete workflow command for OnlineEnsemble experiments
 *
 * Workflow:
 * 1. generate-signals: Create signal file from market data
 * 2. execute-trades: Simulate trading with portfolio manager
 * 3. analyze: Generate performance reports
 * 4. run-all: Execute complete workflow
 */
class EnsembleWorkflowCommand : public Command {
public:
    std::string get_name() const override { return "ensemble"; }
    std::string get_description() const override {
        return "OnlineEnsemble workflow: generate signals, execute trades, analyze results";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    // Sub-commands
    int generate_signals(const std::vector<std::string>& args);
    int execute_trades(const std::vector<std::string>& args);
    int analyze(const std::vector<std::string>& args);
    int run_all(const std::vector<std::string>& args);

    // Configuration structures
    struct SignalGenConfig {
        std::string data_path;
        std::string output_path;
        int warmup_bars = 100;
        int start_bar = 0;
        int end_bar = -1;  // -1 = all

        // Strategy config
        std::vector<int> horizons = {1, 5, 10};
        std::vector<double> weights = {0.3, 0.5, 0.2};
        double lambda = 0.995;
        bool verbose = false;
    };

    struct TradeExecConfig {
        std::string signal_path;
        std::string data_path;
        std::string output_path;

        double starting_capital = 100000.0;
        double buy_threshold = 0.53;
        double sell_threshold = 0.47;
        double kelly_fraction = 0.25;
        bool enable_kelly = true;
        bool verbose = false;
    };

    struct AnalysisConfig {
        std::string trades_path;
        std::string output_path;
        bool show_detailed = true;
        bool show_trades = false;
        bool export_csv = false;
        bool export_json = true;
    };

    // Parsing helpers
    SignalGenConfig parse_signal_config(const std::vector<std::string>& args);
    TradeExecConfig parse_trade_config(const std::vector<std::string>& args);
    AnalysisConfig parse_analysis_config(const std::vector<std::string>& args);
};

/**
 * @brief Signal generation command (standalone)
 */
class GenerateSignalsCommand : public Command {
public:
    std::string get_name() const override { return "generate-signals"; }
    std::string get_description() const override {
        return "Generate OnlineEnsemble signals from market data";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct SignalOutput {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        double probability;
        double confidence;
        SignalType signal_type;
        int prediction_horizon;

        // Multi-horizon data
        std::map<int, double> horizon_predictions;
        double ensemble_agreement;
    };

    void save_signals_jsonl(const std::vector<SignalOutput>& signals,
                           const std::string& path);
    void save_signals_csv(const std::vector<SignalOutput>& signals,
                         const std::string& path);
};

/**
 * @brief Trade execution command (standalone)
 */
class ExecuteTradesCommand : public Command {
public:
    std::string get_name() const override { return "execute-trades"; }
    std::string get_description() const override {
        return "Execute trades from signal file and generate portfolio history";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

public:
    struct TradeRecord {
        uint64_t bar_id;
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        TradeAction action;
        double quantity;
        double price;
        double trade_value;
        double fees;
        std::string reason;

        // Portfolio state after trade
        double cash_balance;
        double portfolio_value;
        double position_quantity;
        double position_avg_price;
    };

    struct PortfolioHistory {
        std::vector<TradeRecord> trades;
        std::vector<double> equity_curve;
        std::vector<double> drawdown_curve;

        double starting_capital;
        double final_capital;
        double max_drawdown;
        int total_trades;
        int winning_trades;
    };

    void save_trades_jsonl(const PortfolioHistory& history, const std::string& path);
    void save_trades_csv(const PortfolioHistory& history, const std::string& path);
    void save_equity_curve(const PortfolioHistory& history, const std::string& path);

    // PSM helper functions
    static double get_position_value(const PortfolioState& portfolio, double current_price);
    static std::map<std::string, double> calculate_target_positions(
        PositionStateMachine::State state,
        double total_capital,
        double price);

    // Multi-instrument versions (use correct price per instrument)
    static double get_position_value_multi(
        const PortfolioState& portfolio,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index);

    // Symbol mapping for PSM (to support both QQQ and SPY)
    struct SymbolMap {
        std::string base;      // QQQ or SPY
        std::string bull_3x;   // TQQQ or SPXL
        std::string bear_1x;   // PSQ or SH
        std::string bear_nx;   // SQQQ (-3x) or SDS (-2x)
    };

    static std::map<std::string, double> calculate_target_positions_multi(
        PositionStateMachine::State state,
        double total_capital,
        const std::map<std::string, std::vector<Bar>>& instrument_bars,
        size_t bar_index,
        const SymbolMap& symbol_map);
};

/**
 * @brief Analysis and reporting command (standalone)
 */
class AnalyzeTradesCommand : public Command {
public:
    std::string get_name() const override { return "analyze-trades"; }
    std::string get_description() const override {
        return "Analyze trade history and generate performance reports";
    }

    int execute(const std::vector<std::string>& args) override;
    void show_help() const override;

private:
    struct PerformanceReport {
        // Returns
        double total_return_pct;
        double annualized_return;
        double monthly_return;
        double daily_return;

        // Risk metrics
        double max_drawdown;
        double avg_drawdown;
        double volatility;
        double downside_deviation;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;

        // Trading metrics
        int total_trades;
        int winning_trades;
        int losing_trades;
        double win_rate;
        double profit_factor;
        double avg_win;
        double avg_loss;
        double avg_trade;
        double largest_win;
        double largest_loss;

        // Position metrics
        double avg_holding_period;
        double max_holding_period;
        int total_long_trades;
        int total_short_trades;

        // Kelly metrics
        double kelly_criterion;
        double avg_position_size;
        double max_position_size;

        // Time analysis
        int trading_days;
        int bars_traded;
        std::string start_date;
        std::string end_date;
    };

    PerformanceReport calculate_metrics(const std::vector<ExecuteTradesCommand::TradeRecord>& trades);
    void print_report(const PerformanceReport& report);
    void save_report_json(const PerformanceReport& report, const std::string& path);
    void generate_plots(const std::vector<double>& equity_curve, const std::string& output_dir);
};

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 5 of 12**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`

- **Size**: 113 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// System Constants
// -----------------------------------------------------------------------------

/// Standard block size for backtesting and signal processing
/// One block represents approximately 8 hours of trading (480 minutes)
/// This constant ensures consistency across strattest, trade, and audit commands
static constexpr size_t STANDARD_BLOCK_SIZE = 480;

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    // Immutable, globally unique identifier for this bar
    // Generated from timestamp_ms and symbol at load time
    uint64_t bar_id = 0;
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
    // Derived fields for traceability/debugging (filled by loader)
    uint32_t sequence_num = 0;   // Position in original dataset
    uint16_t block_num = 0;      // STANDARD_BLOCK_SIZE partition index
    std::string date_str;        // e.g. "2025-09-09" for human-readable logs
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio

```

## 📄 **FILE 6 of 12**: include/common/utils.h

**File Information**:
- **Path**: `include/common/utils.h`

- **Size**: 205 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/utils.h
// Purpose: Comprehensive utility library for the Sentio Trading System
//
// Core Architecture & Recent Enhancements:
// This module provides essential utilities that support the entire trading
// system infrastructure. It has been significantly enhanced with robust
// error handling, CLI utilities, and improved JSON parsing capabilities.
//
// Key Design Principles:
// - Centralized reusable functionality to eliminate code duplication
// - Fail-fast error handling with detailed logging and validation
// - UTC timezone consistency across all time-related operations
// - Robust JSON parsing that handles complex data structures correctly
// - File organization utilities that maintain proper data structure
//
// Recent Major Improvements:
// - Added CLI argument parsing utilities (get_arg) to eliminate duplicates
// - Enhanced JSON parsing to prevent field corruption from quoted commas
// - Implemented comprehensive logging system with file rotation
// - Added robust error handling with crash-on-error philosophy
// - Improved time utilities with consistent UTC timezone handling
//
// Module Categories:
// 1. File I/O: CSV/JSONL reading/writing with format detection
// 2. Time Utilities: UTC-consistent timestamp conversion and formatting
// 3. JSON Utilities: Robust parsing that handles complex quoted strings
// 4. Hash Utilities: SHA-256 and run ID generation for data integrity
// 5. Math Utilities: Financial metrics (Sharpe ratio, drawdown analysis)
// 6. Logging Utilities: Structured logging with file rotation and levels
// 7. CLI Utilities: Command-line argument parsing with flexible formats
// =============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <map>
#include <cstdint>
#include "types.h"

namespace sentio {
namespace utils {
// ------------------------------ Bar ID utilities ------------------------------
/// Generate a stable 64-bit bar identifier from timestamp and symbol
/// Layout: [16 bits symbol hash][48 bits timestamp_ms]
uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol);

/// Extract timestamp (lower 48 bits) from bar id
int64_t extract_timestamp(uint64_t bar_id);

/// Extract 16-bit symbol hash (upper bits) from bar id
uint16_t extract_symbol_hash(uint64_t bar_id);


// ----------------------------- File I/O utilities ----------------------------
/// Advanced CSV data reader with automatic format detection and symbol extraction
/// 
/// This function intelligently handles multiple CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// Key Features:
/// - Automatic format detection by analyzing header row
/// - Symbol extraction from filename for QQQ format files
/// - Timestamp conversion from seconds to milliseconds for QQQ format
/// - Robust error handling with graceful fallbacks
/// 
/// @param path Path to CSV file (supports both relative and absolute paths)
/// @return Vector of Bar structures with OHLCV data and metadata
std::vector<Bar> read_csv_data(const std::string& path);

/// High-performance binary data reader with index-based range queries
/// 
/// This function provides fast access to market data stored in binary format:
/// - Direct index-based access without loading entire dataset
/// - Support for range queries (start_index, count)
/// - Automatic fallback to CSV if binary file doesn't exist
/// - Consistent indexing across entire trading pipeline
/// 
/// @param data_path Path to binary file (or CSV as fallback)
/// @param start_index Starting index for data range (0-based)
/// @param count Number of bars to read (0 = read all from start_index)
/// @return Vector of Bar structures for the specified range
/// @throws Logs errors and returns empty vector on failure
std::vector<Bar> read_market_data_range(const std::string& data_path, 
                                       uint64_t start_index = 0, 
                                       uint64_t count = 0);

/// Get total number of bars in a market data file
/// 
/// @param data_path Path to binary or CSV file
/// @return Total number of bars, or 0 on error
uint64_t get_market_data_count(const std::string& data_path);

/// Get the most recent N bars from a market data file
/// 
/// @param data_path Path to binary or CSV file  
/// @param count Number of recent bars to retrieve
/// @return Vector of the most recent bars
std::vector<Bar> read_recent_market_data(const std::string& data_path, uint64_t count);

/// Write data in JSON Lines format for efficient streaming and processing
/// 
/// JSON Lines (JSONL) format stores one JSON object per line, making it ideal
/// for large datasets that need to be processed incrementally. This format
/// is used throughout the Sentio system for signals and trade data.
/// 
/// @param path Output file path
/// @param lines Vector of JSON strings (one per line)
/// @return true if write successful, false otherwise
bool write_jsonl(const std::string& path, const std::vector<std::string>& lines);

/// Write structured data to CSV format with proper escaping
/// 
/// @param path Output CSV file path
/// @param data 2D string matrix where first row typically contains headers
/// @return true if write successful, false otherwise
bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data);

// ------------------------------ Time utilities -------------------------------
// Parse ISO-like timestamp (YYYY-MM-DD HH:MM:SS) into milliseconds since epoch
int64_t timestamp_to_ms(const std::string& timestamp_str);

// Convert milliseconds since epoch to formatted timestamp string
std::string ms_to_timestamp(int64_t ms);


// ------------------------------ JSON utilities -------------------------------
/// Convert string map to JSON format for lightweight serialization
/// 
/// This function creates simple JSON objects from string key-value pairs.
/// It's designed for lightweight serialization of metadata and configuration.
/// 
/// @param data Map of string keys to string values
/// @return JSON string representation
std::string to_json(const std::map<std::string, std::string>& data);

/// Robust JSON parser for flat string maps with enhanced quote handling
/// 
/// This parser has been significantly enhanced to correctly handle complex
/// JSON structures that contain commas and colons within quoted strings.
/// It prevents the field corruption issues that were present in earlier versions.
/// 
/// Key Features:
/// - Proper handling of commas within quoted values
/// - Correct parsing of colons within quoted strings
/// - Robust quote escaping and state tracking
/// - Graceful error handling with empty map fallback
/// 
/// @param json_str JSON string to parse (must be flat object format)
/// @return Map of parsed key-value pairs, empty map on parse errors
std::map<std::string, std::string> from_json(const std::string& json_str);

// -------------------------------- Hash utilities -----------------------------

// Generate an 8-digit numeric run id (zero-padded). Unique enough per run.
std::string generate_run_id(const std::string& prefix);

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate = 0.0);
double calculate_max_drawdown(const std::vector<double>& equity_curve);

// -------------------------------- Logging utilities -------------------------- 
// Minimal file logger. Writes to logs/debug.log and logs/errors.log.
// Messages should be pre-sanitized (no secrets/PII).
void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

// Leverage conflict detection utility (consolidates duplicate code)
bool would_instruments_conflict(const std::string& proposed, const std::string& existing);

// -------------------------------- CLI utilities ------------------------------- 
/// Flexible command-line argument parser supporting multiple formats
/// 
/// This utility function was extracted from duplicate implementations across
/// multiple CLI files to eliminate code duplication and ensure consistency.
/// It provides flexible parsing that accommodates different user preferences.
/// 
/// Supported Formats:
/// - Space-separated: --name value
/// - Equals-separated: --name=value
/// - Mixed usage within the same command line
/// 
/// Key Features:
/// - Robust argument validation (prevents parsing flags as values)
/// - Consistent behavior across all CLI tools
/// - Graceful fallback to default values
/// - No external dependencies or complex parsing libraries
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including -- prefix)
/// @param def Default value returned if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def = "");

} // namespace utils
} // namespace sentio



```

## 📄 **FILE 7 of 12**: include/strategy/online_ensemble_strategy.h

**File Information**:
- **Path**: `include/strategy/online_ensemble_strategy.h`

- **Size**: 182 lines
- **Modified**: 2025-10-07 13:37:12

- **Type**: .h

```text
#pragma once

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "learning/online_predictor.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <memory>
#include <deque>
#include <vector>
#include <map>

namespace sentio {

/**
 * @brief Full OnlineEnsemble Strategy using EWRLS multi-horizon predictor
 *
 * This strategy achieves online learning with ensemble methods:
 * - Real-time EWRLS model adaptation based on realized P&L
 * - Multi-horizon predictions (1, 5, 10 bars) with weighted ensemble
 * - Continuous performance tracking and adaptive calibration
 * - Target: 10% monthly return @ 60%+ signal accuracy
 *
 * Key Features:
 * - Incremental learning without retraining
 * - Adaptive learning rate based on market volatility
 * - Self-calibrating buy/sell thresholds
 * - Kelly Criterion position sizing integration
 * - Real-time performance metrics
 */
class OnlineEnsembleStrategy : public StrategyComponent {
public:
    struct OnlineEnsembleConfig : public StrategyConfig {
        // EWRLS parameters
        double ewrls_lambda = 0.995;          // Forgetting factor (0.99-0.999)
        double initial_variance = 100.0;       // Initial parameter uncertainty
        double regularization = 0.01;          // L2 regularization
        int warmup_samples = 100;              // Minimum samples before trading

        // Multi-horizon ensemble parameters
        std::vector<int> prediction_horizons = {1, 5, 10};  // Prediction horizons (bars)
        std::vector<double> horizon_weights = {0.3, 0.5, 0.2};  // Ensemble weights

        // Adaptive learning parameters
        bool enable_adaptive_learning = true;
        double min_lambda = 0.990;             // Fast adaptation limit
        double max_lambda = 0.999;             // Slow adaptation limit

        // Signal generation thresholds
        double buy_threshold = 0.53;           // Initial buy threshold
        double sell_threshold = 0.47;          // Initial sell threshold
        double neutral_zone = 0.06;            // Width of neutral zone

        // Bollinger Bands amplification (from WilliamsRSIBB strategy)
        bool enable_bb_amplification = true;   // Enable BB-based signal amplification
        int bb_period = 20;                    // BB period (matches feature engine)
        double bb_std_dev = 2.0;               // BB standard deviations
        double bb_proximity_threshold = 0.30;  // Within 30% of band for amplification
        double bb_amplification_factor = 0.10; // Boost probability by this much

        // Adaptive calibration
        bool enable_threshold_calibration = true;
        int calibration_window = 200;          // Bars for threshold calibration
        double target_win_rate = 0.60;        // Target 60% accuracy
        double threshold_step = 0.005;         // Calibration step size

        // Risk management
        bool enable_kelly_sizing = true;
        double kelly_fraction = 0.25;          // 25% of full Kelly
        double max_position_size = 0.50;       // Max 50% capital per position

        // Performance tracking
        int performance_window = 200;          // Window for metrics
        double target_monthly_return = 0.10;   // Target 10% monthly return

        // Volatility filter (prevent trading in flat markets)
        bool enable_volatility_filter = false;  // Enable ATR-based volatility filter
        int atr_period = 20;                    // ATR calculation period
        double min_atr_ratio = 0.001;           // Minimum ATR as % of price (0.1%)

        OnlineEnsembleConfig() {
            name = "OnlineEnsemble";
            version = "2.0";
        }
    };

    struct PerformanceMetrics {
        double win_rate = 0.0;
        double avg_return = 0.0;
        double monthly_return_estimate = 0.0;
        double sharpe_estimate = 0.0;
        double directional_accuracy = 0.0;
        double recent_rmse = 0.0;
        int total_trades = 0;
        bool targets_met = false;
    };

    explicit OnlineEnsembleStrategy(const OnlineEnsembleConfig& config);
    virtual ~OnlineEnsembleStrategy() = default;

    // Main interface
    SignalOutput generate_signal(const Bar& bar);
    void update(const Bar& bar, double realized_pnl);
    void on_bar(const Bar& bar);

    // Performance and diagnostics
    PerformanceMetrics get_performance_metrics() const;
    std::vector<double> get_feature_importance() const;
    bool is_ready() const { return samples_seen_ >= config_.warmup_samples; }

    // State persistence
    bool save_state(const std::string& path) const;
    bool load_state(const std::string& path);

private:
    OnlineEnsembleConfig config_;

    // Multi-horizon EWRLS predictor
    std::unique_ptr<learning::MultiHorizonPredictor> ensemble_predictor_;

    // Feature engineering
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;

    // Bar history for feature generation
    std::deque<Bar> bar_history_;
    static constexpr size_t MAX_HISTORY = 500;

    // Horizon tracking for delayed updates
    struct HorizonPrediction {
        int entry_bar_index;
        int target_bar_index;
        int horizon;
        std::vector<double> features;
        double entry_price;
        bool is_long;
    };
    std::map<int, HorizonPrediction> pending_updates_;

    // Performance tracking
    struct TradeResult {
        bool won;
        double return_pct;
        int64_t timestamp;
    };
    std::deque<TradeResult> recent_trades_;
    int samples_seen_;

    // Adaptive thresholds
    double current_buy_threshold_;
    double current_sell_threshold_;
    int calibration_count_;

    // Private methods
    std::vector<double> extract_features(const Bar& current_bar);
    void calibrate_thresholds();
    void track_prediction(int bar_index, int horizon, const std::vector<double>& features,
                         double entry_price, bool is_long);
    void process_pending_updates(const Bar& current_bar);
    SignalType determine_signal(double probability) const;
    void update_performance_metrics(bool won, double return_pct);

    // BB amplification
    struct BollingerBands {
        double upper;
        double middle;
        double lower;
        double bandwidth;
        double position_pct;  // 0=lower band, 1=upper band
    };
    BollingerBands calculate_bollinger_bands() const;
    double apply_bb_amplification(double base_probability, const BollingerBands& bb) const;

    // Volatility filter
    double calculate_atr(int period) const;
    bool has_sufficient_volatility() const;

    // Constants
    static constexpr int MIN_FEATURES_BARS = 100;  // Minimum bars for features
    static constexpr size_t TRADE_HISTORY_SIZE = 500;
};

} // namespace sentio

```

## 📄 **FILE 8 of 12**: src/cli/command_interface.cpp

**File Information**:
- **Path**: `src/cli/command_interface.cpp`

- **Size**: 79 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "cli/command_interface.h"
#include <iostream>
#include <algorithm>

namespace sentio {
namespace cli {

std::string Command::get_arg(const std::vector<std::string>& args, 
                            const std::string& name, 
                            const std::string& default_value) const {
    auto it = std::find(args.begin(), args.end(), name);
    if (it != args.end() && (it + 1) != args.end()) {
        return *(it + 1);
    }
    return default_value;
}

bool Command::has_flag(const std::vector<std::string>& args, 
                      const std::string& flag) const {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

void CommandDispatcher::register_command(std::unique_ptr<Command> command) {
    commands_.push_back(std::move(command));
}

int CommandDispatcher::execute(int argc, char** argv) {
    // Validate minimum arguments
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    std::string command_name = argv[1];
    Command* command = find_command(command_name);
    
    if (!command) {
        std::cerr << "Error: Unknown command '" << command_name << "'\n\n";
        show_help();
        return 1;
    }
    
    // Convert remaining arguments to vector
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    try {
        return command->execute(args);
    } catch (const std::exception& e) {
        std::cerr << "Error executing command '" << command_name << "': " << e.what() << std::endl;
        return 1;
    }
}

void CommandDispatcher::show_help() const {
    std::cout << "Usage: sentio_cli <command> [options]\n\n";
    std::cout << "Available commands:\n";
    
    for (const auto& command : commands_) {
        std::cout << "  " << command->get_name() 
                  << " - " << command->get_description() << "\n";
    }
    
    std::cout << "\nUse 'sentio_cli <command> --help' for detailed command help.\n";
}

Command* CommandDispatcher::find_command(const std::string& name) const {
    auto it = std::find_if(commands_.begin(), commands_.end(),
        [&name](const std::unique_ptr<Command>& cmd) {
            return cmd->get_name() == name;
        });
    
    return (it != commands_.end()) ? it->get() : nullptr;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 9 of 12**: src/cli/command_registry.cpp

**File Information**:
- **Path**: `src/cli/command_registry.cpp`

- **Size**: 650 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "cli/command_registry.h"
// #include "cli/canonical_commands.h"  // Not implemented yet
// #include "cli/strattest_command.h"    // Not implemented yet
// #include "cli/audit_command.h"        // Not implemented yet
// #include "cli/trade_command.h"        // Not implemented yet
// #include "cli/full_test_command.h"    // Not implemented yet
// #include "cli/sanity_check_command.h" // Not implemented yet
// #include "cli/walk_forward_command.h" // Not implemented yet
// #include "cli/validate_bar_id_command.h" // Not implemented yet
// #include "cli/train_xgb60sa_command.h" // Not implemented yet
// #include "cli/train_xgb8_command.h"   // Not implemented yet
// #include "cli/train_xgb25_command.h"  // Not implemented yet
// #include "cli/online_command.h"  // Commented out - missing implementations
// #include "cli/online_sanity_check_command.h"  // Commented out - missing implementations
// #include "cli/online_trade_command.h"  // Commented out - missing implementations
#include "cli/ensemble_workflow_command.h"
#include "cli/live_trade_command.hpp"
#ifdef XGBOOST_AVAILABLE
#include "cli/train_command.h"
#endif
#ifdef TORCH_AVAILABLE
// PPO training command removed from this project scope
#endif
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sentio::cli {

// ================================================================================================
// COMMAND REGISTRY IMPLEMENTATION
// ================================================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::register_command(const std::string& name, 
                                      std::shared_ptr<Command> command,
                                      const CommandInfo& info) {
    CommandInfo cmd_info = info;
    cmd_info.command = command;
    if (cmd_info.description.empty()) {
        cmd_info.description = command->get_description();
    }
    
    commands_[name] = cmd_info;
    
    // Register aliases
    for (const auto& alias : cmd_info.aliases) {
        AliasInfo alias_info;
        alias_info.target_command = name;
        aliases_[alias] = alias_info;
    }
}

void CommandRegistry::register_alias(const std::string& alias, 
                                    const std::string& target_command,
                                    const AliasInfo& info) {
    AliasInfo alias_info = info;
    alias_info.target_command = target_command;
    aliases_[alias] = alias_info;
}

void CommandRegistry::deprecate_command(const std::string& name, 
                                       const std::string& replacement,
                                       const std::string& message) {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        it->second.deprecated = true;
        it->second.replacement_command = replacement;
        it->second.deprecation_message = message.empty() ? 
            "This command is deprecated. Use '" + replacement + "' instead." : message;
    }
}

std::shared_ptr<Command> CommandRegistry::get_command(const std::string& name) {
    // Check direct command first
    auto cmd_it = commands_.find(name);
    if (cmd_it != commands_.end()) {
        if (cmd_it->second.deprecated) {
            show_deprecation_warning(name, cmd_it->second);
        }
        return cmd_it->second.command;
    }
    
    // Check aliases
    auto alias_it = aliases_.find(name);
    if (alias_it != aliases_.end()) {
        if (alias_it->second.deprecated) {
            show_alias_warning(name, alias_it->second);
        }
        
        auto target_it = commands_.find(alias_it->second.target_command);
        if (target_it != commands_.end()) {
            return target_it->second.command;
        }
    }
    
    return nullptr;
}

bool CommandRegistry::has_command(const std::string& name) const {
    return commands_.find(name) != commands_.end() || 
           aliases_.find(name) != aliases_.end();
}

std::vector<std::string> CommandRegistry::get_available_commands() const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

std::vector<std::string> CommandRegistry::get_commands_by_category(const std::string& category) const {
    std::vector<std::string> commands;
    for (const auto& [name, info] : commands_) {
        if (info.category == category && !info.deprecated) {
            commands.push_back(name);
        }
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

const CommandRegistry::CommandInfo* CommandRegistry::get_command_info(const std::string& name) const {
    auto it = commands_.find(name);
    return (it != commands_.end()) ? &it->second : nullptr;
}

void CommandRegistry::show_help() const {
    std::cout << "Sentio CLI - Advanced Trading System Command Line Interface\n\n";
    std::cout << "Usage: sentio_cli <command> [options]\n\n";
    
    // Group commands by category
    std::map<std::string, std::vector<std::string>> categories;
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            categories[info.category].push_back(name);
        }
    }
    
    // Show each category
    for (const auto& [category, commands] : categories) {
        std::cout << category << " Commands:\n";
        for (const auto& cmd : commands) {
            const auto& info = commands_.at(cmd);
            std::cout << "  " << std::left << std::setw(15) << cmd 
                     << info.description << "\n";
        }
        std::cout << "\n";
    }
    
    std::cout << "Global Options:\n";
    std::cout << "  --help, -h         Show this help message\n";
    std::cout << "  --version, -v      Show version information\n\n";
    
    std::cout << "Use 'sentio_cli <command> --help' for detailed command help.\n";
    std::cout << "Use 'sentio_cli --migration' to see deprecated command alternatives.\n\n";
    
    EnhancedCommandDispatcher::show_usage_examples();
}

void CommandRegistry::show_category_help(const std::string& category) const {
    auto commands = get_commands_by_category(category);
    if (commands.empty()) {
        std::cout << "No commands found in category: " << category << "\n";
        return;
    }
    
    std::cout << category << " Commands:\n\n";
    for (const auto& cmd : commands) {
        const auto& info = commands_.at(cmd);
        std::cout << "  " << cmd << " - " << info.description << "\n";
        
        if (!info.aliases.empty()) {
            std::cout << "    Aliases: " << format_command_list(info.aliases) << "\n";
        }
        
        if (!info.tags.empty()) {
            std::cout << "    Tags: " << format_command_list(info.tags) << "\n";
        }
        std::cout << "\n";
    }
}

void CommandRegistry::show_migration_guide() const {
    std::cout << "Migration Guide - Deprecated Commands\n";
    std::cout << "=====================================\n\n";
    
    bool has_deprecated = false;
    
    for (const auto& [name, info] : commands_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "❌ " << name << " (deprecated)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            if (!info.replacement_command.empty()) {
                std::cout << "   ✅ Use instead: " << info.replacement_command << "\n";
            }
            std::cout << "\n";
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (info.deprecated) {
            has_deprecated = true;
            std::cout << "⚠️  " << alias << " (deprecated alias)\n";
            std::cout << "   " << info.deprecation_message << "\n";
            std::cout << "   ✅ Use instead: " << info.target_command << "\n";
            if (!info.migration_guide.empty()) {
                std::cout << "   📖 Migration: " << info.migration_guide << "\n";
            }
            std::cout << "\n";
        }
    }
    
    if (!has_deprecated) {
        std::cout << "✅ No deprecated commands or aliases found.\n";
        std::cout << "All commands are up-to-date!\n";
    }
}

int CommandRegistry::execute_command(const std::string& name, const std::vector<std::string>& args) {
    auto command = get_command(name);
    if (!command) {
        std::cerr << "❌ Unknown command: " << name << "\n\n";
        
        auto suggestions = suggest_commands(name);
        if (!suggestions.empty()) {
            std::cerr << "💡 Did you mean:\n";
            for (const auto& suggestion : suggestions) {
                std::cerr << "  " << suggestion << "\n";
            }
            std::cerr << "\n";
        }
        
        std::cerr << "Use 'sentio_cli --help' to see available commands.\n";
        return 1;
    }
    
    try {
        return command->execute(args);
    } catch (const std::exception& e) {
        std::cerr << "❌ Command execution failed: " << e.what() << "\n";
        return 1;
    }
}

std::vector<std::string> CommandRegistry::suggest_commands(const std::string& input) const {
    std::vector<std::pair<std::string, int>> candidates;
    
    // Check all commands and aliases
    for (const auto& [name, info] : commands_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, name);
            if (distance <= 2 && distance < static_cast<int>(name.length())) {
                candidates.emplace_back(name, distance);
            }
        }
    }
    
    for (const auto& [alias, info] : aliases_) {
        if (!info.deprecated) {
            int distance = levenshtein_distance(input, alias);
            if (distance <= 2 && distance < static_cast<int>(alias.length())) {
                candidates.emplace_back(alias, distance);
            }
        }
    }
    
    // Sort by distance and return top suggestions
    std::sort(candidates.begin(), candidates.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::vector<std::string> suggestions;
    for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
        suggestions.push_back(candidates[i].first);
    }
    
    return suggestions;
}

void CommandRegistry::initialize_default_commands() {
    // Canonical commands and legacy commands commented out - not implemented yet
    // TODO: Implement these commands when needed

    /* COMMENTED OUT - NOT IMPLEMENTED YET
    // Register canonical commands (new interface)
    CommandInfo generate_info;
    generate_info.category = "Signal Generation";
    generate_info.version = "2.0";
    generate_info.description = "Generate trading signals (canonical interface)";
    generate_info.tags = {"signals", "generation", "canonical"};
    register_command("generate", std::make_shared<GenerateCommand>(), generate_info);

    CommandInfo analyze_info;
    analyze_info.category = "Performance Analysis";
    analyze_info.version = "2.0";
    analyze_info.description = "Analyze trading performance (canonical interface)";
    analyze_info.tags = {"analysis", "performance", "canonical"};
    register_command("analyze", std::make_shared<AnalyzeCommand>(), analyze_info);

    CommandInfo execute_info;
    execute_info.category = "Trade Execution";
    execute_info.version = "2.0";
    execute_info.description = "Execute trades from signals (canonical interface)";
    execute_info.tags = {"trading", "execution", "canonical"};
    register_command("execute", std::make_shared<TradeCanonicalCommand>(), execute_info);

    CommandInfo pipeline_info;
    pipeline_info.category = "Workflows";
    pipeline_info.version = "2.0";
    pipeline_info.description = "Run multi-step trading workflows";
    pipeline_info.tags = {"workflow", "automation", "canonical"};
    register_command("pipeline", std::make_shared<PipelineCommand>(), pipeline_info);

    // Register legacy commands (backward compatibility)
    CommandInfo strattest_info;
    strattest_info.category = "Legacy";
    strattest_info.version = "1.0";
    strattest_info.description = "Generate trading signals (legacy interface)";
    strattest_info.deprecated = false;  // Keep for now
    strattest_info.tags = {"signals", "legacy"};
    register_command("strattest", std::make_shared<StrattestCommand>(), strattest_info);

    CommandInfo audit_info;
    audit_info.category = "Legacy";
    audit_info.version = "1.0";
    audit_info.description = "Analyze performance with reports (legacy interface)";
    audit_info.deprecated = false;  // Keep for now
    audit_info.tags = {"analysis", "legacy"};
    register_command("audit", std::make_shared<AuditCommand>(), audit_info);
    END OF COMMENTED OUT SECTION */

    // All legacy and canonical commands commented out above - not implemented yet

    // Register OnlineEnsemble workflow commands
    CommandInfo generate_signals_info;
    generate_signals_info.category = "OnlineEnsemble Workflow";
    generate_signals_info.version = "1.0";
    generate_signals_info.description = "Generate trading signals using OnlineEnsemble strategy";
    generate_signals_info.tags = {"ensemble", "signals", "online-learning"};
    register_command("generate-signals", std::make_shared<GenerateSignalsCommand>(), generate_signals_info);

    CommandInfo execute_trades_info;
    execute_trades_info.category = "OnlineEnsemble Workflow";
    execute_trades_info.version = "1.0";
    execute_trades_info.description = "Execute trades from signals with Kelly sizing";
    execute_trades_info.tags = {"ensemble", "trading", "kelly", "portfolio"};
    register_command("execute-trades", std::make_shared<ExecuteTradesCommand>(), execute_trades_info);

    CommandInfo analyze_trades_info;
    analyze_trades_info.category = "OnlineEnsemble Workflow";
    analyze_trades_info.version = "1.0";
    analyze_trades_info.description = "Analyze trade performance and generate reports";
    analyze_trades_info.tags = {"ensemble", "analysis", "metrics", "reporting"};
    register_command("analyze-trades", std::make_shared<AnalyzeTradesCommand>(), analyze_trades_info);

    // Register live trading command
    CommandInfo live_trade_info;
    live_trade_info.category = "Live Trading";
    live_trade_info.version = "1.0";
    live_trade_info.description = "Run OnlineTrader v1.0 with paper account (SPY/SPXL/SH/SDS)";
    live_trade_info.tags = {"live", "paper-trading", "alpaca", "polygon"};
    register_command("live-trade", std::make_shared<LiveTradeCommand>(), live_trade_info);

    // Register training commands if available
// XGBoost training now handled by Python scripts (tools/train_xgboost_binary.py)
// C++ train command disabled

#ifdef TORCH_AVAILABLE
    // PPO training command intentionally removed
#endif
}

void CommandRegistry::setup_canonical_aliases() {
    // Canonical command aliases commented out - canonical commands not implemented yet
    /* COMMENTED OUT - CANONICAL COMMANDS NOT IMPLEMENTED
    // Setup helpful aliases for canonical commands
    AliasInfo gen_alias;
    gen_alias.target_command = "generate";
    gen_alias.migration_guide = "Use 'generate' instead of 'strattest' for consistent interface";
    register_alias("gen", "generate", gen_alias);

    AliasInfo report_alias;
    report_alias.target_command = "analyze";
    report_alias.migration_guide = "Use 'analyze report' instead of 'audit report'";
    register_alias("report", "analyze", report_alias);

    AliasInfo run_alias;
    run_alias.target_command = "execute";
    register_alias("run", "execute", run_alias);

    // Deprecate old patterns
    AliasInfo strattest_alias;
    strattest_alias.target_command = "generate";
    strattest_alias.deprecated = true;
    strattest_alias.deprecation_message = "The 'strattest' command interface is being replaced";
    strattest_alias.migration_guide = "Use 'generate --strategy <name> --data <path>' for the new canonical interface";
    // Don't register as alias yet - keep original command for compatibility
    */
}

// ================================================================================================
// PRIVATE HELPER METHODS
// ================================================================================================

void CommandRegistry::show_deprecation_warning(const std::string& command_name, const CommandInfo& info) {
    std::cerr << "⚠️  WARNING: Command '" << command_name << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    if (!info.replacement_command.empty()) {
        std::cerr << "   Use '" << info.replacement_command << "' instead.\n";
    }
    std::cerr << "\n";
}

void CommandRegistry::show_alias_warning(const std::string& alias, const AliasInfo& info) {
    std::cerr << "⚠️  WARNING: Alias '" << alias << "' is deprecated.\n";
    std::cerr << "   " << info.deprecation_message << "\n";
    std::cerr << "   Use '" << info.target_command << "' instead.\n";
    if (!info.migration_guide.empty()) {
        std::cerr << "   Migration: " << info.migration_guide << "\n";
    }
    std::cerr << "\n";
}

std::string CommandRegistry::format_command_list(const std::vector<std::string>& commands) const {
    std::ostringstream ss;
    for (size_t i = 0; i < commands.size(); ++i) {
        ss << commands[i];
        if (i < commands.size() - 1) ss << ", ";
    }
    return ss.str();
}

int CommandRegistry::levenshtein_distance(const std::string& s1, const std::string& s2) const {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = static_cast<int>(j);
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    
    return dp[len1][len2];
}

// ================================================================================================
// ENHANCED COMMAND DISPATCHER IMPLEMENTATION
// ================================================================================================

int EnhancedCommandDispatcher::execute(int argc, char** argv) {
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    
    // Handle global flags
    if (handle_global_flags(args)) {
        return 0;
    }
    
    std::string command_name = argv[1];
    
    // Handle special cases
    if (command_name == "--help" || command_name == "-h") {
        show_help();
        return 0;
    }
    
    if (command_name == "--version" || command_name == "-v") {
        show_version();
        return 0;
    }
    
    if (command_name == "--migration") {
        CommandRegistry::instance().show_migration_guide();
        return 0;
    }
    
    // Execute command through registry
    auto& registry = CommandRegistry::instance();
    return registry.execute_command(command_name, args);
}

void EnhancedCommandDispatcher::show_help() {
    CommandRegistry::instance().show_help();
}

void EnhancedCommandDispatcher::show_version() {
    std::cout << "Sentio CLI " << get_version_string() << "\n";
    std::cout << "Advanced Trading System Command Line Interface\n";
    std::cout << "Copyright (c) 2024 Sentio Trading Systems\n\n";
    
    std::cout << "Features:\n";
    std::cout << "  • Multi-strategy signal generation (SGO, AWR, XGBoost, CatBoost)\n";
    std::cout << "  • Advanced portfolio management with leverage\n";
    std::cout << "  • Comprehensive performance analysis\n";
    std::cout << "  • Automated trading workflows\n";
    std::cout << "  • Machine learning model training (Python-side for XGB/CTB)\n\n";
    
    std::cout << "Build Information:\n";
#ifdef TORCH_AVAILABLE
    std::cout << "  • PyTorch/LibTorch: Enabled\n";
#else
    std::cout << "  • PyTorch/LibTorch: Disabled\n";
#endif
#ifdef XGBOOST_AVAILABLE
    std::cout << "  • XGBoost: Enabled\n";
#else
    std::cout << "  • XGBoost: Disabled\n";
#endif
    std::cout << "  • Compiler: " << __VERSION__ << "\n";
    std::cout << "  • Build Date: " << __DATE__ << " " << __TIME__ << "\n";
}

bool EnhancedCommandDispatcher::handle_global_flags(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            show_help();
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            show_version();
            return true;
        }
        if (arg == "--migration") {
            CommandRegistry::instance().show_migration_guide();
            return true;
        }
    }
    return false;
}

void EnhancedCommandDispatcher::show_command_not_found_help(const std::string& command_name) {
    std::cerr << "Command '" << command_name << "' not found.\n\n";
    
    auto& registry = CommandRegistry::instance();
    auto suggestions = registry.suggest_commands(command_name);
    
    if (!suggestions.empty()) {
        std::cerr << "Did you mean:\n";
        for (const auto& suggestion : suggestions) {
            std::cerr << "  " << suggestion << "\n";
        }
        std::cerr << "\n";
    }
    
    std::cerr << "Use 'sentio_cli --help' to see all available commands.\n";
}

void EnhancedCommandDispatcher::show_usage_examples() {
    std::cout << "Common Usage Examples:\n";
    std::cout << "======================\n\n";
    
    std::cout << "Signal Generation:\n";
    std::cout << "  sentio_cli generate --strategy sgo --data data/equities/QQQ_RTH_NH.csv\n\n";
    
    std::cout << "Performance Analysis:\n";
    std::cout << "  sentio_cli analyze summary --signals data/signals/sgo-timestamp.jsonl\n\n";
    
    std::cout << "Automated Workflows:\n";
    std::cout << "  sentio_cli pipeline backtest --strategy sgo --blocks 20\n";
    std::cout << "  sentio_cli pipeline compare --strategies \"sgo,xgb,ctb\" --blocks 20\n\n";
    
    std::cout << "Legacy Commands (still supported):\n";
    std::cout << "  sentio_cli strattest --strategy sgo --blocks 20\n";
    std::cout << "  sentio_cli audit report --signals data/signals/sgo-timestamp.jsonl\n\n";
}

std::string EnhancedCommandDispatcher::get_version_string() {
    return "2.0.0-beta";  // Update as needed
}

// ================================================================================================
// COMMAND FACTORY IMPLEMENTATION
// ================================================================================================

std::map<std::string, CommandFactory::CommandCreator> CommandFactory::factories_;

void CommandFactory::register_factory(const std::string& name, CommandCreator creator) {
    factories_[name] = creator;
}

std::shared_ptr<Command> CommandFactory::create_command(const std::string& name) {
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        return it->second();
    }
    return nullptr;
}

void CommandFactory::register_builtin_commands() {
    // Canonical commands and legacy commands not implemented - commented out
    /* COMMENTED OUT - NOT IMPLEMENTED
    // Register factory functions for lazy loading
    register_factory("generate", []() { return std::make_shared<GenerateCommand>(); });
    register_factory("analyze", []() { return std::make_shared<AnalyzeCommand>(); });
    register_factory("execute", []() { return std::make_shared<TradeCanonicalCommand>(); });
    register_factory("pipeline", []() { return std::make_shared<PipelineCommand>(); });

    register_factory("strattest", []() { return std::make_shared<StrattestCommand>(); });
    register_factory("audit", []() { return std::make_shared<AuditCommand>(); });
    register_factory("trade", []() { return std::make_shared<TradeCommand>(); });
    register_factory("full-test", []() { return std::make_shared<FullTestCommand>(); });
    */

    // Online learning strategies - commented out (missing implementations)
    // register_factory("online", []() { return std::make_shared<OnlineCommand>(); });
    // register_factory("online-sanity", []() { return std::make_shared<OnlineSanityCheckCommand>(); });
    // register_factory("online-trade", []() { return std::make_shared<OnlineTradeCommand>(); });

    // OnlineEnsemble workflow commands
    register_factory("generate-signals", []() { return std::make_shared<GenerateSignalsCommand>(); });
    register_factory("execute-trades", []() { return std::make_shared<ExecuteTradesCommand>(); });
    register_factory("analyze-trades", []() { return std::make_shared<AnalyzeTradesCommand>(); });

    // Live trading command
    register_factory("live-trade", []() { return std::make_shared<LiveTradeCommand>(); });
    
// XGBoost training now handled by Python scripts

#ifdef TORCH_AVAILABLE
    register_factory("train_ppo", []() { return std::make_shared<TrainPpoCommand>(); });
#endif
}

} // namespace sentio::cli

```

## 📄 **FILE 10 of 12**: src/cli/generate_signals_command.cpp

**File Information**:
- **Path**: `src/cli/generate_signals_command.cpp`

- **Size**: 236 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "cli/ensemble_workflow_command.h"
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace sentio {
namespace cli {

int GenerateSignalsCommand::execute(const std::vector<std::string>& args) {
    using namespace sentio;

    // Parse arguments
    std::string data_path = get_arg(args, "--data", "");
    std::string output_path = get_arg(args, "--output", "signals.jsonl");
    int warmup_bars = std::stoi(get_arg(args, "--warmup", "100"));
    int start_bar = std::stoi(get_arg(args, "--start", "0"));
    int end_bar = std::stoi(get_arg(args, "--end", "-1"));
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");
    bool csv_output = has_flag(args, "--csv");

    if (data_path.empty()) {
        std::cerr << "Error: --data is required\n";
        show_help();
        return 1;
    }

    std::cout << "=== OnlineEnsemble Signal Generation ===\n";
    std::cout << "Data: " << data_path << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "Warmup: " << warmup_bars << " bars\n\n";

    // Load market data
    std::cout << "Loading market data...\n";
    auto bars = utils::read_csv_data(data_path);
    if (bars.empty()) {
        std::cerr << "Error: Could not load data from " << data_path << "\n";
        return 1;
    }

    if (end_bar < 0 || end_bar > static_cast<int>(bars.size())) {
        end_bar = static_cast<int>(bars.size());
    }

    std::cout << "Loaded " << bars.size() << " bars\n";
    std::cout << "Processing range: " << start_bar << " to " << end_bar << "\n\n";

    // Create OnlineEnsembleStrategy
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.warmup_samples = warmup_bars;
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.ewrls_lambda = 0.995;
    config.buy_threshold = 0.53;
    config.sell_threshold = 0.47;
    config.enable_threshold_calibration = false;  // Disable auto-calibration
    config.enable_adaptive_learning = true;

    OnlineEnsembleStrategy strategy(config);

    // Generate signals
    std::vector<SignalOutput> signals;
    int progress_interval = (end_bar - start_bar) / 20;  // 5% increments

    std::cout << "Generating signals...\n";
    for (int i = start_bar; i < end_bar; ++i) {
        // Update strategy with bar (processes pending updates)
        strategy.on_bar(bars[i]);

        // Generate signal from strategy
        sentio::SignalOutput strategy_signal = strategy.generate_signal(bars[i]);

        // Convert to CLI output format
        SignalOutput output;
        output.bar_id = strategy_signal.bar_id;
        output.timestamp_ms = strategy_signal.timestamp_ms;
        output.bar_index = strategy_signal.bar_index;
        output.symbol = strategy_signal.symbol;
        output.probability = strategy_signal.probability;
        output.signal_type = strategy_signal.signal_type;
        output.prediction_horizon = strategy_signal.prediction_horizon;

        // Calculate ensemble agreement from metadata
        output.ensemble_agreement = 0.0;
        if (strategy_signal.metadata.count("ensemble_agreement")) {
            output.ensemble_agreement = std::stod(strategy_signal.metadata.at("ensemble_agreement"));
        }

        signals.push_back(output);

        // Progress reporting
        if (verbose && progress_interval > 0 && (i - start_bar) % progress_interval == 0) {
            double pct = 100.0 * (i - start_bar) / (end_bar - start_bar);
            std::cout << "  Progress: " << std::fixed << std::setprecision(1)
                     << pct << "% (" << (i - start_bar) << "/" << (end_bar - start_bar) << ")\n";
        }
    }

    std::cout << "Generated " << signals.size() << " signals\n\n";

    // Save signals
    std::cout << "Saving signals to " << output_path << "...\n";
    if (csv_output) {
        save_signals_csv(signals, output_path);
    } else {
        save_signals_jsonl(signals, output_path);
    }

    // Print summary
    int long_signals = 0, short_signals = 0, neutral_signals = 0;
    for (const auto& sig : signals) {
        if (sig.signal_type == SignalType::LONG) long_signals++;
        else if (sig.signal_type == SignalType::SHORT) short_signals++;
        else neutral_signals++;
    }

    std::cout << "\n=== Signal Summary ===\n";
    std::cout << "Total signals: " << signals.size() << "\n";
    std::cout << "Long signals:  " << long_signals << " (" << (100.0 * long_signals / signals.size()) << "%)\n";
    std::cout << "Short signals: " << short_signals << " (" << (100.0 * short_signals / signals.size()) << "%)\n";
    std::cout << "Neutral:       " << neutral_signals << " (" << (100.0 * neutral_signals / signals.size()) << "%)\n";

    // Get strategy performance - not implemented in stub
    // auto metrics = strategy.get_performance_metrics();
    std::cout << "\n=== Strategy Metrics ===\n";
    std::cout << "Strategy: OnlineEnsemble (stub version)\n";
    std::cout << "Note: Full metrics available after execute-trades and analyze-trades\n";

    std::cout << "\n✅ Signals saved successfully!\n";
    return 0;
}

void GenerateSignalsCommand::save_signals_jsonl(const std::vector<SignalOutput>& signals,
                                               const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    for (const auto& sig : signals) {
        // Convert signal_type enum to string
        std::string signal_type_str;
        switch (sig.signal_type) {
            case SignalType::LONG: signal_type_str = "LONG"; break;
            case SignalType::SHORT: signal_type_str = "SHORT"; break;
            default: signal_type_str = "NEUTRAL"; break;
        }

        // Create JSON line
        out << "{"
            << "\"bar_id\":" << sig.bar_id << ","
            << "\"timestamp_ms\":" << sig.timestamp_ms << ","
            << "\"bar_index\":" << sig.bar_index << ","
            << "\"symbol\":\"" << sig.symbol << "\","
            << "\"probability\":" << std::fixed << std::setprecision(6) << sig.probability << ","
            << "\"signal_type\":\"" << signal_type_str << "\","
            << "\"prediction_horizon\":" << sig.prediction_horizon << ","
            << "\"ensemble_agreement\":" << sig.ensemble_agreement
            << "}\n";
    }
}

void GenerateSignalsCommand::save_signals_csv(const std::vector<SignalOutput>& signals,
                                             const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    // Header
    out << "bar_id,timestamp_ms,bar_index,symbol,probability,confidence,signal_type,prediction_horizon,ensemble_agreement\n";

    // Data
    for (const auto& sig : signals) {
        out << sig.bar_id << ","
            << sig.timestamp_ms << ","
            << sig.bar_index << ","
            << sig.symbol << ","
            << std::fixed << std::setprecision(6) << sig.probability << ","
            // << sig.confidence << ","  // Field not in SignalOutput
            << static_cast<int>(sig.signal_type) << ","
            << sig.prediction_horizon << ","
            << sig.ensemble_agreement << "\n";
    }
}

void GenerateSignalsCommand::show_help() const {
    std::cout << R"(
Generate OnlineEnsemble Signals
================================

Generate trading signals from market data using OnlineEnsemble strategy.

USAGE:
    sentio_cli generate-signals --data <path> [OPTIONS]

REQUIRED:
    --data <path>              Path to market data file (CSV or binary)

OPTIONS:
    --output <path>            Output signal file (default: signals.jsonl)
    --warmup <bars>            Warmup period before trading (default: 100)
    --start <bar>              Start bar index (default: 0)
    --end <bar>                End bar index (default: all)
    --csv                      Output in CSV format instead of JSONL
    --verbose, -v              Show progress updates

EXAMPLES:
    # Generate signals from data
    sentio_cli generate-signals --data data/SPY_1min.csv --output signals.jsonl

    # With custom warmup and range
    sentio_cli generate-signals --data data/QQQ.bin --warmup 200 --start 1000 --end 5000

    # CSV output with verbose progress
    sentio_cli generate-signals --data data/futures.bin --csv --verbose

OUTPUT FORMAT (JSONL):
    Each line contains:
    {
        "bar_id": 12345,
        "timestamp_ms": 1609459200000,
        "probability": 0.6234,
        "confidence": 0.82,
        "signal_type": "1",  // 0=NEUTRAL, 1=LONG, 2=SHORT
        "prediction_horizon": 5,
        "ensemble_agreement": 0.75
    }

)" << std::endl;
}

} // namespace cli
} // namespace sentio

```

## 📄 **FILE 11 of 12**: src/common/utils.cpp

**File Information**:
- **Path**: `src/common/utils.cpp`

- **Size**: 553 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "common/utils.h"
#include "common/binary_data.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

// =============================================================================
// Module: common/utils.cpp
// Purpose: Implementation of utility functions for file I/O, time handling,
//          JSON parsing, hashing, and mathematical calculations.
//
// This module provides the concrete implementations for all utility functions
// declared in utils.h. Each section handles a specific domain of functionality
// to keep the codebase modular and maintainable.
// =============================================================================

// ============================================================================
// Helper Functions to Fix ODR Violations
// ============================================================================

/**
 * @brief Convert CSV path to binary path (fixes ODR violation)
 * 
 * This helper function eliminates code duplication that was causing ODR violations
 * by consolidating identical path conversion logic used in multiple places.
 */
static std::string convert_csv_to_binary_path(const std::string& data_path) {
    std::filesystem::path p(data_path);
    if (!p.has_extension()) {
        p += ".bin";
    } else {
        p.replace_extension(".bin");
    }
    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    return p.string();
}

namespace sentio {
namespace utils {
// ------------------------------ Bar ID utilities ------------------------------
uint64_t generate_bar_id(int64_t timestamp_ms, const std::string& symbol) {
    uint64_t timestamp_part = static_cast<uint64_t>(timestamp_ms) & 0xFFFFFFFFFFFFULL; // lower 48 bits
    uint32_t symbol_hash = static_cast<uint32_t>(std::hash<std::string>{}(symbol));
    uint64_t symbol_part = (static_cast<uint64_t>(symbol_hash) & 0xFFFFULL) << 48; // upper 16 bits
    return timestamp_part | symbol_part;
}

int64_t extract_timestamp(uint64_t bar_id) {
    return static_cast<int64_t>(bar_id & 0xFFFFFFFFFFFFULL);
}

uint16_t extract_symbol_hash(uint64_t bar_id) {
    return static_cast<uint16_t>((bar_id >> 48) & 0xFFFFULL);
}


// --------------------------------- Helpers ----------------------------------
namespace {
    /// Helper function to remove leading and trailing whitespace from strings
    /// Used internally by CSV parsing and JSON processing functions
    static inline std::string trim(const std::string& s) {
        const char* ws = " \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }
}

// ----------------------------- File I/O utilities ----------------------------

/// Reads OHLCV market data from CSV files with automatic format detection
/// 
/// This function handles two CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol extracted from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// The function automatically detects the format by examining the header row
/// and processes the data accordingly, ensuring compatibility with different
/// data sources while maintaining a consistent Bar output format.
std::vector<Bar> read_csv_data(const std::string& path) {
    std::vector<Bar> bars;
    std::ifstream file(path);
    
    // Early return if file cannot be opened
    if (!file.is_open()) {
        return bars;
    }

    std::string line;
    
    // Read and analyze header to determine CSV format
    std::getline(file, line);
    bool is_qqq_format = (line.find("ts_utc") != std::string::npos);
    bool is_standard_format = (line.find("symbol") != std::string::npos && line.find("timestamp_ms") != std::string::npos);
    bool is_datetime_format = (line.find("timestamp") != std::string::npos && line.find("timestamp_ms") == std::string::npos);
    
    // For QQQ format, extract symbol from filename since it's not in the CSV
    std::string default_symbol = "UNKNOWN";
    if (is_qqq_format) {
        size_t last_slash = path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
        
        // Pattern matching for common ETF symbols
        if (filename.find("QQQ") != std::string::npos) default_symbol = "QQQ";
        else if (filename.find("SQQQ") != std::string::npos) default_symbol = "SQQQ";
        else if (filename.find("TQQQ") != std::string::npos) default_symbol = "TQQQ";
    }

    // Process each data row according to the detected format
    size_t sequence_index = 0;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        Bar b{};

        // Parse timestamp and symbol based on detected format
        if (is_qqq_format) {
            // QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            b.symbol = default_symbol;

            // Parse ts_utc column (ISO timestamp string) but discard value
            std::getline(ss, item, ',');
            
            // Use ts_nyt_epoch as timestamp (Unix seconds -> convert to milliseconds)
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item)) * 1000;
            
        } else if (is_standard_format) {
            // Standard format: symbol,timestamp_ms,open,high,low,close,volume
            std::getline(ss, item, ',');
            b.symbol = trim(item);

            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));

        } else if (is_datetime_format) {
            // Datetime format: timestamp,symbol,open,high,low,close,volume
            // where timestamp is "YYYY-MM-DD HH:MM:SS"
            std::getline(ss, item, ',');
            b.timestamp_ms = timestamp_to_ms(trim(item));

            std::getline(ss, item, ',');
            b.symbol = trim(item);

        } else {
            // Unknown format: treat first column as symbol, second as timestamp_ms
            std::getline(ss, item, ',');
            b.symbol = trim(item);
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));
        }

        // Parse OHLCV data (same format across all CSV types)
        std::getline(ss, item, ',');
        b.open = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.high = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.low = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.close = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.volume = std::stod(trim(item));

        // Populate immutable id and derived fields
        b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
        b.sequence_num = static_cast<uint32_t>(sequence_index);
        b.block_num = static_cast<uint16_t>(sequence_index / STANDARD_BLOCK_SIZE);
        std::string ts = ms_to_timestamp(b.timestamp_ms);
        if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
        bars.push_back(b);
        ++sequence_index;
    }

    return bars;
}

bool write_jsonl(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << '\n';
    }
    return true;
}

bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            out << row[i];
            if (i + 1 < row.size()) out << ',';
        }
        out << '\n';
    }
    return true;
}

// --------------------------- Binary Data utilities ---------------------------

std::vector<Bar> read_market_data_range(const std::string& data_path, 
                                       uint64_t start_index, 
                                       uint64_t count) {
    // Try binary format first (much faster)
    // 🔧 ODR FIX: Use helper function to eliminate code duplication
    std::string binary_path = convert_csv_to_binary_path(data_path);
    
    if (std::filesystem::exists(binary_path)) {
        sentio::binary_data::BinaryDataReader reader(binary_path);
        if (reader.open()) {
            if (count == 0) {
                // Read from start_index to end
                count = reader.get_bar_count() - start_index;
            }
            
            auto bars = reader.read_range(start_index, count);
            if (!bars.empty()) {
                // Populate ids and derived fields for the selected range
                for (size_t i = 0; i < bars.size(); ++i) {
                    Bar& b = bars[i];
                    b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
                    uint64_t seq = start_index + i;
                    b.sequence_num = static_cast<uint32_t>(seq);
                    b.block_num = static_cast<uint16_t>(seq / STANDARD_BLOCK_SIZE);
                    std::string ts = ms_to_timestamp(b.timestamp_ms);
                    if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
                }
                log_debug("Loaded " + std::to_string(bars.size()) + " bars from binary file: " + 
                         binary_path + " (range: " + std::to_string(start_index) + "-" + 
                         std::to_string(start_index + count - 1) + ")");
                return bars;
            }
        }
    }
    
    // Read from CSV when binary is not available
    log_info("Binary file not found, reading CSV: " + data_path);
    auto all_bars = read_csv_data(data_path);
    
    if (all_bars.empty()) {
        return all_bars;
    }
    
    // Apply range selection
    if (start_index >= all_bars.size()) {
        log_error("Start index " + std::to_string(start_index) + 
                 " exceeds data size " + std::to_string(all_bars.size()));
        return {};
    }
    
    uint64_t end_index = start_index + (count == 0 ? all_bars.size() - start_index : count);
    end_index = std::min(end_index, static_cast<uint64_t>(all_bars.size()));
    
    std::vector<Bar> result(all_bars.begin() + start_index, all_bars.begin() + end_index);
    // Ensure derived fields are consistent with absolute indexing
    for (size_t i = 0; i < result.size(); ++i) {
        Bar& b = result[i];
        // bar_id should already be set by read_csv_data; recompute defensively if missing
        if (b.bar_id == 0) b.bar_id = generate_bar_id(b.timestamp_ms, b.symbol);
        uint64_t seq = start_index + i;
        b.sequence_num = static_cast<uint32_t>(seq);
        b.block_num = static_cast<uint16_t>(seq / STANDARD_BLOCK_SIZE);
        if (b.date_str.empty()) {
            std::string ts = ms_to_timestamp(b.timestamp_ms);
            if (ts.size() >= 10) b.date_str = ts.substr(0, 10);
        }
    }
    log_debug("Loaded " + std::to_string(result.size()) + " bars from CSV file: " + 
             data_path + " (range: " + std::to_string(start_index) + "-" + 
             std::to_string(end_index - 1) + ")");
    
    return result;
}

uint64_t get_market_data_count(const std::string& data_path) {
    // Try binary format first
    // 🔧 ODR FIX: Use helper function to eliminate code duplication
    std::string binary_path = convert_csv_to_binary_path(data_path);
    
    if (std::filesystem::exists(binary_path)) {
        sentio::binary_data::BinaryDataReader reader(binary_path);
        if (reader.open()) {
            return reader.get_bar_count();
        }
    }
    
    // Read from CSV when binary is not available
    auto bars = read_csv_data(data_path);
    return bars.size();
}

std::vector<Bar> read_recent_market_data(const std::string& data_path, uint64_t count) {
    uint64_t total_count = get_market_data_count(data_path);
    if (total_count == 0 || count == 0) {
        return {};
    }
    
    uint64_t start_index = (count >= total_count) ? 0 : (total_count - count);
    return read_market_data_range(data_path, start_index, count);
}

// ------------------------------ Time utilities -------------------------------
int64_t timestamp_to_ms(const std::string& timestamp_str) {
    // Strict parser for "YYYY-MM-DD HH:MM:SS" (UTC) -> epoch ms
    std::tm tm{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("timestamp_to_ms parse failed for: " + timestamp_str);
    }
    auto time_c = timegm(&tm); // UTC
    if (time_c == -1) {
        throw std::runtime_error("timestamp_to_ms timegm failed for: " + timestamp_str);
    }
    return static_cast<int64_t>(time_c) * 1000;
}

std::string ms_to_timestamp(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm* gmt = gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}


// ------------------------------ JSON utilities -------------------------------
std::string to_json(const std::map<std::string, std::string>& data) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& [k, v] : data) {
        if (!first) os << ',';
        first = false;
        os << '"' << k << '"' << ':' << '"' << v << '"';
    }
    os << '}';
    return os.str();
}

std::map<std::string, std::string> from_json(const std::string& json_str) {
    // Robust parser for a flat string map {"k":"v",...} that respects quotes and escapes
    std::map<std::string, std::string> out;
    if (json_str.size() < 2 || json_str.front() != '{' || json_str.back() != '}') return out;
    const std::string s = json_str.substr(1, json_str.size() - 2);

    // Split into top-level pairs by commas not inside quotes
    std::vector<std::string> pairs;
    std::string current;
    bool in_quotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            // toggle quotes unless escaped
            bool escaped = (i > 0 && s[i-1] == '\\');
            if (!escaped) in_quotes = !in_quotes;
            current.push_back(c);
        } else if (c == ',' && !in_quotes) {
            pairs.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) pairs.push_back(current);

    auto trim_ws = [](const std::string& str){
        size_t a = 0, b = str.size();
        while (a < b && std::isspace(static_cast<unsigned char>(str[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(str[b-1]))) --b;
        return str.substr(a, b - a);
    };

    for (auto& p : pairs) {
        std::string pair = trim_ws(p);
        // find colon not inside quotes
        size_t colon_pos = std::string::npos;
        in_quotes = false;
        for (size_t i = 0; i < pair.size(); ++i) {
            char c = pair[i];
            if (c == '"') {
                bool escaped = (i > 0 && pair[i-1] == '\\');
                if (!escaped) in_quotes = !in_quotes;
            } else if (c == ':' && !in_quotes) {
                colon_pos = i; break;
            }
        }
        if (colon_pos == std::string::npos) continue;
        std::string key = trim_ws(pair.substr(0, colon_pos));
        std::string val = trim_ws(pair.substr(colon_pos + 1));
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') key = key.substr(1, key.size() - 2);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        out[key] = val;
    }
    return out;
}

// -------------------------------- Hash utilities -----------------------------

std::string generate_run_id(const std::string& prefix) {
    // Collision-resistant run id: <prefix>-<YYYYMMDDHHMMSS>-<pid>-<rand16hex>
    std::ostringstream os;
    // Timestamp UTC
    std::time_t now = std::time(nullptr);
    std::tm* gmt = gmtime(&now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d%H%M%S", gmt);
    // Random 64-bit
    uint64_t r = static_cast<uint64_t>(now) ^ 0x9e3779b97f4a7c15ULL;
    r ^= (r << 13);
    r ^= (r >> 7);
    r ^= (r << 17);
    os << (prefix.empty() ? "run" : prefix) << "-" << ts << "-" << std::hex << std::setw(4) << (static_cast<unsigned>(now) & 0xFFFF) << "-";
    os << std::hex << std::setw(16) << std::setfill('0') << (r | 0x1ULL);
    return os.str();
}

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;
    double mean = 0.0;
    for (double r : returns) mean += r;
    mean /= static_cast<double>(returns.size());
    double variance = 0.0;
    for (double r : returns) variance += (r - mean) * (r - mean);
    variance /= static_cast<double>(returns.size());
    double stddev = std::sqrt(variance);
    if (stddev == 0.0) return 0.0;
    return (mean - risk_free_rate) / stddev;
}

double calculate_max_drawdown(const std::vector<double>& equity_curve) {
    if (equity_curve.size() < 2) return 0.0;
    double peak = equity_curve.front();
    double max_dd = 0.0;
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double e = equity_curve[i];
        if (e > peak) peak = e;
        if (peak > 0.0) {
            double dd = (peak - e) / peak;
            if (dd > max_dd) max_dd = dd;
        }
    }
    return max_dd;
}

// -------------------------------- Logging utilities --------------------------
namespace {
    static inline std::string log_dir() {
        return std::string("logs");
    }
    static inline void ensure_log_dir() {
        std::error_code ec;
        std::filesystem::create_directories(log_dir(), ec);
    }
    static inline std::string iso_now() {
        std::time_t now = std::time(nullptr);
        std::tm* gmt = gmtime(&now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
        return std::string(buf);
    }
}

void log_debug(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/debug.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " DEBUG common:utils:0 - " << message << '\n';
}

void log_info(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " INFO common:utils:0 - " << message << '\n';
}

void log_warning(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " WARNING common:utils:0 - " << message << '\n';
}

void log_error(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/errors.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " ERROR common:utils:0 - " << message << '\n';
}

bool would_instruments_conflict(const std::string& proposed, const std::string& existing) {
    // Consolidated conflict detection logic (removes duplicate code)
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},
        {"SQQQ", {"TQQQ", "QQQ"}},
        {"PSQ",  {"TQQQ", "QQQ"}},
        {"QQQ",  {"SQQQ", "PSQ"}}
    };
    
    auto it = conflicts.find(proposed);
    if (it != conflicts.end()) {
        return std::find(it->second.begin(), it->second.end(), existing) != it->second.end();
    }
    
    return false;
}

// -------------------------------- CLI utilities -------------------------------

/// Parse command line arguments supporting both "--name value" and "--name=value" formats
/// 
/// This function provides flexible command-line argument parsing that supports:
/// - Space-separated format: --name value
/// - Equals-separated format: --name=value
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including --)
/// @param def Default value to return if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == name) {
            // Handle "--name value" format
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (!next.empty() && next[0] != '-') return next;
            }
        } else if (arg.rfind(name + "=", 0) == 0) {
            // Handle "--name=value" format
            return arg.substr(name.size() + 1);
        }
    }
    return def;
}

} // namespace utils
} // namespace sentio

```

## 📄 **FILE 12 of 12**: src/strategy/online_ensemble_strategy.cpp

**File Information**:
- **Path**: `src/strategy/online_ensemble_strategy.cpp`

- **Size**: 532 lines
- **Modified**: 2025-10-07 13:37:13

- **Type**: .cpp

```text
#include "strategy/online_ensemble_strategy.h"
#include "common/utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace sentio {

OnlineEnsembleStrategy::OnlineEnsembleStrategy(const OnlineEnsembleConfig& config)
    : StrategyComponent(config),
      config_(config),
      samples_seen_(0),
      current_buy_threshold_(config.buy_threshold),
      current_sell_threshold_(config.sell_threshold),
      calibration_count_(0) {

    // Initialize feature engine first to get actual feature count
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>();

    // Initialize multi-horizon EWRLS predictor with correct feature count
    size_t num_features = feature_engine_->get_features().size();
    if (num_features == 0) {
        // Feature engine not ready yet, use default config total
        features::UnifiedFeatureEngineConfig default_config;
        num_features = default_config.total_features();  // 126 features
    }
    ensemble_predictor_ = std::make_unique<learning::MultiHorizonPredictor>(num_features);

    // Add predictors for each horizon with reduced warmup
    // EWRLS predictor warmup should be much smaller than strategy warmup
    // because updates are delayed by horizon length
    learning::OnlinePredictor::Config predictor_config;
    predictor_config.warmup_samples = 50;  // Lower warmup for EWRLS
    predictor_config.lambda = config_.ewrls_lambda;
    predictor_config.initial_variance = config_.initial_variance;
    predictor_config.regularization = config_.regularization;
    predictor_config.adaptive_learning = config_.enable_adaptive_learning;
    predictor_config.min_lambda = config_.min_lambda;
    predictor_config.max_lambda = config_.max_lambda;

    for (size_t i = 0; i < config_.prediction_horizons.size(); ++i) {
        int horizon = config_.prediction_horizons[i];
        double weight = config_.horizon_weights[i];
        // Need to pass config to add_horizon - but API doesn't support it
        // Will need to modify MultiHorizonPredictor
        ensemble_predictor_->add_horizon(horizon, weight);
    }

    utils::log_info("OnlineEnsembleStrategy initialized with " +
                   std::to_string(config_.prediction_horizons.size()) + " horizons, " +
                   std::to_string(num_features) + " features");
}

SignalOutput OnlineEnsembleStrategy::generate_signal(const Bar& bar) {
    SignalOutput output;
    output.bar_id = bar.bar_id;
    output.timestamp_ms = bar.timestamp_ms;
    output.bar_index = samples_seen_;
    output.symbol = "UNKNOWN";  // Set by caller if needed
    output.strategy_name = config_.name;
    output.strategy_version = config_.version;

    // Wait for warmup
    if (!is_ready()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Extract features
    std::vector<double> features = extract_features(bar);
    if (features.empty()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        return output;
    }

    // Check volatility filter (skip trading in flat markets)
    if (config_.enable_volatility_filter && !has_sufficient_volatility()) {
        output.signal_type = SignalType::NEUTRAL;
        output.probability = 0.5;
        output.metadata["skip_reason"] = "insufficient_volatility";
        return output;
    }

    // Get ensemble prediction
    auto prediction = ensemble_predictor_->predict(features);

    // Convert predicted return to probability
    // Predicted return is in decimal (e.g., 0.01 = 1% return)
    // Map to probability: positive return -> prob > 0.5, negative -> prob < 0.5
    double base_prob = 0.5 + std::tanh(prediction.predicted_return * 50.0) * 0.4;
    base_prob = std::clamp(base_prob, 0.05, 0.95);  // Keep within reasonable bounds

    // Apply Bollinger Bands amplification if enabled
    double prob = base_prob;
    if (config_.enable_bb_amplification) {
        BollingerBands bb = calculate_bollinger_bands();
        prob = apply_bb_amplification(base_prob, bb);

        // Store BB metadata
        output.metadata["bb_upper"] = std::to_string(bb.upper);
        output.metadata["bb_middle"] = std::to_string(bb.middle);
        output.metadata["bb_lower"] = std::to_string(bb.lower);
        output.metadata["bb_position"] = std::to_string(bb.position_pct);
        output.metadata["base_probability"] = std::to_string(base_prob);
    }

    output.probability = prob;
    output.signal_type = determine_signal(prob);

    // Track for multi-horizon updates (always, not just for non-neutral signals)
    // This allows the model to learn from all market data, not just when we trade
    bool is_long = (prob > 0.5);  // Use probability, not signal type
    for (int horizon : config_.prediction_horizons) {
        track_prediction(samples_seen_, horizon, features, bar.close, is_long);
    }

    // Add metadata
    output.metadata["confidence"] = std::to_string(prediction.confidence);
    output.metadata["volatility"] = std::to_string(prediction.volatility_estimate);
    output.metadata["buy_threshold"] = std::to_string(current_buy_threshold_);
    output.metadata["sell_threshold"] = std::to_string(current_sell_threshold_);

    return output;
}

void OnlineEnsembleStrategy::update(const Bar& bar, double realized_pnl) {
    // Update performance metrics
    if (std::abs(realized_pnl) > 1e-6) {  // Non-zero P&L
        double return_pct = realized_pnl / 100000.0;  // Assuming $100k base
        bool won = (realized_pnl > 0);
        update_performance_metrics(won, return_pct);
    }

    // Process pending horizon updates
    process_pending_updates(bar);
}

void OnlineEnsembleStrategy::on_bar(const Bar& bar) {
    // Add to history
    bar_history_.push_back(bar);
    if (bar_history_.size() > MAX_HISTORY) {
        bar_history_.pop_front();
    }

    // Update feature engine
    feature_engine_->update(bar);

    samples_seen_++;

    // Calibrate thresholds periodically
    if (config_.enable_threshold_calibration &&
        samples_seen_ % config_.calibration_window == 0 &&
        is_ready()) {
        calibrate_thresholds();
    }

    // Process any pending updates for this bar
    process_pending_updates(bar);
}

std::vector<double> OnlineEnsembleStrategy::extract_features(const Bar& current_bar) {
    if (bar_history_.size() < MIN_FEATURES_BARS) {
        return {};  // Not enough history
    }

    // UnifiedFeatureEngine maintains its own history via update()
    // Just get the current features after the bar has been added to history
    if (!feature_engine_->is_ready()) {
        return {};
    }

    return feature_engine_->get_features();
}

void OnlineEnsembleStrategy::track_prediction(int bar_index, int horizon,
                                              const std::vector<double>& features,
                                              double entry_price, bool is_long) {
    HorizonPrediction pred;
    pred.entry_bar_index = bar_index;
    pred.target_bar_index = bar_index + horizon;
    pred.horizon = horizon;
    pred.features = features;
    pred.entry_price = entry_price;
    pred.is_long = is_long;

    pending_updates_[pred.target_bar_index] = pred;
}

void OnlineEnsembleStrategy::process_pending_updates(const Bar& current_bar) {
    auto it = pending_updates_.find(samples_seen_);
    if (it != pending_updates_.end()) {
        const auto& pred = it->second;

        // Calculate actual return
        double actual_return = (current_bar.close - pred.entry_price) / pred.entry_price;
        if (!pred.is_long) {
            actual_return = -actual_return;  // Invert for short
        }

        // Update the appropriate horizon predictor
        ensemble_predictor_->update(pred.horizon, pred.features, actual_return);

        // Debug logging
        if (samples_seen_ % 100 == 0) {
            utils::log_debug("Processed update for horizon " + std::to_string(pred.horizon) +
                           ", return=" + std::to_string(actual_return) +
                           ", pending_count=" + std::to_string(pending_updates_.size()));
        }

        // Remove processed prediction
        pending_updates_.erase(it);
    }
}

SignalType OnlineEnsembleStrategy::determine_signal(double probability) const {
    if (probability > current_buy_threshold_) {
        return SignalType::LONG;
    } else if (probability < current_sell_threshold_) {
        return SignalType::SHORT;
    } else {
        return SignalType::NEUTRAL;
    }
}

void OnlineEnsembleStrategy::update_performance_metrics(bool won, double return_pct) {
    TradeResult result;
    result.won = won;
    result.return_pct = return_pct;
    result.timestamp = 0;  // Could add actual timestamp

    recent_trades_.push_back(result);
    if (recent_trades_.size() > TRADE_HISTORY_SIZE) {
        recent_trades_.pop_front();
    }
}

void OnlineEnsembleStrategy::calibrate_thresholds() {
    if (recent_trades_.size() < 50) {
        return;  // Not enough data
    }

    // Calculate current win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    double win_rate = static_cast<double>(wins) / recent_trades_.size();

    // Adjust thresholds to hit target win rate
    if (win_rate < config_.target_win_rate) {
        // Win rate too low -> make thresholds more selective (move apart)
        current_buy_threshold_ += config_.threshold_step;
        current_sell_threshold_ -= config_.threshold_step;
    } else if (win_rate > config_.target_win_rate + 0.05) {
        // Win rate too high -> trade more (move together)
        current_buy_threshold_ -= config_.threshold_step;
        current_sell_threshold_ += config_.threshold_step;
    }

    // Keep within reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.70);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.30, 0.49);

    // Ensure minimum separation
    double min_separation = 0.04;
    if (current_buy_threshold_ - current_sell_threshold_ < min_separation) {
        double center = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = center + min_separation / 2.0;
        current_sell_threshold_ = center - min_separation / 2.0;
    }

    calibration_count_++;
    utils::log_info("Calibrated thresholds #" + std::to_string(calibration_count_) +
                   ": buy=" + std::to_string(current_buy_threshold_) +
                   ", sell=" + std::to_string(current_sell_threshold_) +
                   " (win_rate=" + std::to_string(win_rate) + ")");
}

OnlineEnsembleStrategy::PerformanceMetrics
OnlineEnsembleStrategy::get_performance_metrics() const {
    PerformanceMetrics metrics;

    if (recent_trades_.empty()) {
        return metrics;
    }

    // Win rate
    int wins = std::count_if(recent_trades_.begin(), recent_trades_.end(),
                            [](const TradeResult& r) { return r.won; });
    metrics.win_rate = static_cast<double>(wins) / recent_trades_.size();
    metrics.total_trades = static_cast<int>(recent_trades_.size());

    // Average return
    double sum_returns = std::accumulate(recent_trades_.begin(), recent_trades_.end(), 0.0,
                                        [](double sum, const TradeResult& r) {
                                            return sum + r.return_pct;
                                        });
    metrics.avg_return = sum_returns / recent_trades_.size();

    // Monthly return estimate (assuming 252 trading days, ~21 per month)
    // If we have N trades over M bars, estimate monthly trades
    if (samples_seen_ > 0) {
        double trades_per_bar = static_cast<double>(recent_trades_.size()) / std::min(samples_seen_, 500);
        double bars_per_month = 21.0 * 390.0;  // 21 days * 390 minutes (6.5 hours)
        double monthly_trades = trades_per_bar * bars_per_month;
        metrics.monthly_return_estimate = metrics.avg_return * monthly_trades;
    }

    // Sharpe estimate
    if (recent_trades_.size() > 10) {
        double mean = metrics.avg_return;
        double sum_sq = 0.0;
        for (const auto& trade : recent_trades_) {
            double diff = trade.return_pct - mean;
            sum_sq += diff * diff;
        }
        double std_dev = std::sqrt(sum_sq / recent_trades_.size());
        if (std_dev > 1e-8) {
            metrics.sharpe_estimate = mean / std_dev * std::sqrt(252.0);  // Annualized
        }
    }

    // Check if targets met
    metrics.targets_met = (metrics.win_rate >= config_.target_win_rate) &&
                         (metrics.monthly_return_estimate >= config_.target_monthly_return);

    return metrics;
}

std::vector<double> OnlineEnsembleStrategy::get_feature_importance() const {
    // Get feature importance from first predictor (they should be similar)
    // Would need to expose this through MultiHorizonPredictor
    // For now return empty
    return {};
}

bool OnlineEnsembleStrategy::save_state(const std::string& path) const {
    try {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Save basic state
        file.write(reinterpret_cast<const char*>(&samples_seen_), sizeof(int));
        file.write(reinterpret_cast<const char*>(&current_buy_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&current_sell_threshold_), sizeof(double));
        file.write(reinterpret_cast<const char*>(&calibration_count_), sizeof(int));

        // Save trade history size
        size_t trade_count = recent_trades_.size();
        file.write(reinterpret_cast<const char*>(&trade_count), sizeof(size_t));

        // Save trades
        for (const auto& trade : recent_trades_) {
            file.write(reinterpret_cast<const char*>(&trade.won), sizeof(bool));
            file.write(reinterpret_cast<const char*>(&trade.return_pct), sizeof(double));
            file.write(reinterpret_cast<const char*>(&trade.timestamp), sizeof(int64_t));
        }

        file.close();
        utils::log_info("Saved OnlineEnsembleStrategy state to: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to save state: " + std::string(e.what()));
        return false;
    }
}

bool OnlineEnsembleStrategy::load_state(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // Load basic state
        file.read(reinterpret_cast<char*>(&samples_seen_), sizeof(int));
        file.read(reinterpret_cast<char*>(&current_buy_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&current_sell_threshold_), sizeof(double));
        file.read(reinterpret_cast<char*>(&calibration_count_), sizeof(int));

        // Load trade history
        size_t trade_count;
        file.read(reinterpret_cast<char*>(&trade_count), sizeof(size_t));

        recent_trades_.clear();
        for (size_t i = 0; i < trade_count; ++i) {
            TradeResult trade;
            file.read(reinterpret_cast<char*>(&trade.won), sizeof(bool));
            file.read(reinterpret_cast<char*>(&trade.return_pct), sizeof(double));
            file.read(reinterpret_cast<char*>(&trade.timestamp), sizeof(int64_t));
            recent_trades_.push_back(trade);
        }

        file.close();
        utils::log_info("Loaded OnlineEnsembleStrategy state from: " + path);
        return true;

    } catch (const std::exception& e) {
        utils::log_error("Failed to load state: " + std::string(e.what()));
        return false;
    }
}

// Bollinger Bands calculation
OnlineEnsembleStrategy::BollingerBands OnlineEnsembleStrategy::calculate_bollinger_bands() const {
    BollingerBands bb;
    bb.upper = 0.0;
    bb.middle = 0.0;
    bb.lower = 0.0;
    bb.bandwidth = 0.0;
    bb.position_pct = 0.5;

    if (bar_history_.size() < static_cast<size_t>(config_.bb_period)) {
        return bb;
    }

    // Calculate SMA (middle band)
    size_t start = bar_history_.size() - config_.bb_period;
    double sum = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        sum += bar_history_[i].close;
    }
    bb.middle = sum / config_.bb_period;

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = start; i < bar_history_.size(); i++) {
        double diff = bar_history_[i].close - bb.middle;
        variance += diff * diff;
    }
    double std_dev = std::sqrt(variance / config_.bb_period);

    // Calculate bands
    bb.upper = bb.middle + (config_.bb_std_dev * std_dev);
    bb.lower = bb.middle - (config_.bb_std_dev * std_dev);
    bb.bandwidth = bb.upper - bb.lower;

    // Calculate position within bands (0=lower, 1=upper)
    double current_price = bar_history_.back().close;
    if (bb.bandwidth > 1e-8) {
        bb.position_pct = (current_price - bb.lower) / bb.bandwidth;
        bb.position_pct = std::clamp(bb.position_pct, 0.0, 1.0);
    }

    return bb;
}

// Apply BB amplification to base probability
double OnlineEnsembleStrategy::apply_bb_amplification(double base_probability, const BollingerBands& bb) const {
    double amplified_prob = base_probability;

    // Only amplify if BB bands are valid
    if (bb.bandwidth < 1e-8) {
        return amplified_prob;
    }

    // LONG signals: amplify when near lower band (position < threshold)
    if (base_probability > 0.5) {
        if (bb.position_pct <= config_.bb_proximity_threshold) {
            // Near lower band - amplify LONG signal
            double proximity_factor = 1.0 - (bb.position_pct / config_.bb_proximity_threshold);
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob += amplification;

            // Extra boost for extreme oversold (position < 10%)
            if (bb.position_pct < 0.10) {
                amplified_prob += 0.05;
            }
        }
    }
    // SHORT signals: amplify when near upper band (position > 1 - threshold)
    else if (base_probability < 0.5) {
        if (bb.position_pct >= (1.0 - config_.bb_proximity_threshold)) {
            // Near upper band - amplify SHORT signal
            double proximity_factor = (bb.position_pct - (1.0 - config_.bb_proximity_threshold)) / config_.bb_proximity_threshold;
            double amplification = config_.bb_amplification_factor * proximity_factor;
            amplified_prob -= amplification;

            // Extra boost for extreme overbought (position > 90%)
            if (bb.position_pct > 0.90) {
                amplified_prob -= 0.05;
            }
        }
    }

    // Clamp to valid probability range
    amplified_prob = std::clamp(amplified_prob, 0.05, 0.95);

    return amplified_prob;
}

double OnlineEnsembleStrategy::calculate_atr(int period) const {
    if (bar_history_.size() < static_cast<size_t>(period + 1)) {
        return 0.0;
    }

    // Calculate True Range for each bar and average
    double sum_tr = 0.0;
    for (size_t i = bar_history_.size() - period; i < bar_history_.size(); ++i) {
        const auto& curr = bar_history_[i];
        const auto& prev = bar_history_[i - 1];

        // True Range = max(high-low, |high-prev_close|, |low-prev_close|)
        double hl = curr.high - curr.low;
        double hc = std::abs(curr.high - prev.close);
        double lc = std::abs(curr.low - prev.close);

        double tr = std::max({hl, hc, lc});
        sum_tr += tr;
    }

    return sum_tr / period;
}

bool OnlineEnsembleStrategy::has_sufficient_volatility() const {
    if (bar_history_.empty()) {
        return false;
    }

    // Calculate ATR
    double atr = calculate_atr(config_.atr_period);

    // Get current price
    double current_price = bar_history_.back().close;

    // Calculate ATR as percentage of price
    double atr_ratio = (current_price > 0) ? (atr / current_price) : 0.0;

    // Check if ATR ratio meets minimum threshold
    return atr_ratio >= config_.min_atr_ratio;
}

} // namespace sentio

```

