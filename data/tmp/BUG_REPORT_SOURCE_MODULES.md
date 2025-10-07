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
