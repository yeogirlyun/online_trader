# Development Principles for Scripts Folder

These principles apply to all scripts in this folder and the entire project.

## 1. No Fallback - Crash Fast ⚡

**Rule**: If something is wrong, fail immediately and loudly.

### Never Do This (Silent Fallbacks):
```bash
# BAD - Silent fallback to default
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="default.json"  # DON'T
fi

# BAD - Continue with degraded functionality  
if ! connect_to_api; then
    echo "Warning: Using cached data"  # DON'T
    use_cached_data
fi

# BAD - Retry silently
for i in {1..5}; do
    if fetch_data; then break; fi  # DON'T
    sleep 1
done
```

### Always Do This (Crash Fast):
```bash
# GOOD - Crash immediately with clear error
if [ ! -f "$CONFIG_FILE" ]; then
    echo "ERROR: Config file not found: $CONFIG_FILE"
    echo "Create it with: cp config.example.json config.json"
    exit 1
fi

# GOOD - Fail on first error
if ! connect_to_api; then
    echo "ERROR: Failed to connect to API"
    echo "Check credentials in config.env"
    exit 1
fi

# GOOD - Explicit error on missing data
if ! fetch_data; then
    echo "ERROR: Failed to fetch data"
    exit 1
fi
```

### Why Crash Fast?
1. **Forces immediate fix** - No "works sometimes" bugs
2. **Clear error messages** - Know exactly what's wrong
3. **No silent corruption** - Bad data doesn't spread
4. **Faster debugging** - Fail at source, not downstream
5. **Production safety** - Don't trade with bad state

---

## 2. No Duplicate Code 🚫

**Rule**: ONE script per task. Never create versions or variants.

### Never Do This:
```bash
# DON'T create multiple versions
launch_trading.sh
launch_trading_v2.sh        # NO
launch_trading_enhanced.sh  # NO
launch_trading_improved.sh  # NO
launch_trading_new.sh       # NO

# DON'T keep backups
script.py
script.py.bak              # NO
script.py.old              # NO
script_backup.py           # NO

# DON'T create parallel implementations
optimize.py
optimize_fast.py           # NO
optimize_better.py         # NO
optimize_alternative.py    # NO
```

### Always Do This:
```bash
# DO have ONE canonical version
launch_trading.sh          # YES - The only version

# DO use git for history
git log launch_trading.sh  # See old versions
git checkout <hash> -- launch_trading.sh  # Restore old version

# DO use git branches for experiments
git checkout -b experiment-new-feature
# Edit launch_trading.sh
# Test, test, test
git checkout main
git merge experiment-new-feature
```

### Why No Duplicates?
1. **Single source of truth** - No confusion about which to use
2. **Easier maintenance** - Fix bugs in one place
3. **No divergence** - Scripts don't get out of sync
4. **Cleaner codebase** - Less clutter
5. **Git history** - All versions tracked properly

---

## 3. Direct Modification Only ✏️

**Rule**: To improve a script, edit it directly. Delete old code.

### Never Do This:
```python
# DON'T comment out old code
# def old_function():
#     return old_logic()

def new_function():
    return new_logic()

# DON'T keep both implementations
def optimize_v1(data):
    pass

def optimize_v2(data):  # NO - Just update optimize_v1
    pass

# DON'T add version flags
if USE_NEW_VERSION:
    new_implementation()
else:
    old_implementation()  # NO
```

### Always Do This:
```python
# DO replace old code completely
def optimize(data):
    """New improved implementation"""
    return new_logic(data)

# DO delete dead code
# (If you need old version, use git history)

# DO update in place
def fetch_data(url):
    # Implementation improved 2025-10-09
    return better_fetch(url)
```

### Why Direct Modification?
1. **No dead code** - Cleaner, easier to read
2. **No confusion** - One implementation to understand
3. **Git is your backup** - Old code is in history
4. **Forces commitment** - No hedging with fallbacks
5. **Production clarity** - What you see is what runs

---

## 4. New Scripts Only for New Tasks 🆕

**Rule**: Create new script ONLY if it's a genuinely different task.

### When to Create New Script:
```bash
# YES - Completely different task
launch_trading.sh          # Launch trading sessions
generate_report.sh         # Generate reports (NEW TASK)
backup_database.sh         # Backup data (NEW TASK)
```

### When NOT to Create New Script:
```bash
# NO - Just an improvement
launch_trading.sh          # Original
launch_trading_fast.sh     # NO - Just make launch_trading.sh faster

# NO - Just an alternative approach
optimize.py                # Original  
optimize_gradient.py       # NO - Just change optimize.py's algorithm

# NO - Just adding features
trading.sh                 # Original
trading_with_logging.sh    # NO - Just add logging to trading.sh
```

### Decision Tree:
```
Is this a completely different task?
├─ YES → Create new script
│  ├─ Different input/output
│  ├─ Different purpose
│  └─ Used in different contexts
│
└─ NO → Edit existing script
   ├─ Same task, different approach → Edit
   ├─ Same task, better implementation → Edit
   ├─ Same task, added features → Edit
   └─ Same task, optimization → Edit
```

### Why Be Strict About This?
1. **Prevents script sprawl** - Don't end up with 50 scripts
2. **Clear separation of concerns** - Each script = one task
3. **Easier to find** - Know which script does what
4. **Forces good design** - Make scripts flexible enough
5. **Maintainable codebase** - Less duplication

---

## 5. Naming Conventions 📛

**Rule**: Use descriptive, permanent names. Never include version info.

### Good Names:
```bash
launch_trading.sh           # Clear, permanent
run_2phase_optuna.py       # Describes what it does
comprehensive_warmup.sh    # Clear purpose
professional_trading_dashboard.py  # Descriptive
```

### Bad Names (Never Use):
```bash
# NO version numbers
script_v1.sh
script_v2.sh
script_2024.py

# NO status indicators
script_old.sh
script_new.sh
script_tmp.py
script_test.sh

# NO quality indicators
script_enhanced.sh
script_improved.py
script_fast.sh
script_better.py
script_final.sh         # (It's never final)

# NO dates
script_20241009.sh
script_oct_2024.py
```

### Why Permanent Names?
1. **Stable imports** - Code that uses script doesn't break
2. **Stable documentation** - READMEs stay accurate
3. **Stable automation** - Cron jobs don't need updating
4. **Professional** - Looks like production code
5. **Mental model** - Always know where to find things

---

## Real-World Example

### ❌ Wrong Way (What NOT to Do):
```bash
# Initial implementation
launch_trading.sh

# Later: "improvements"
launch_trading_v2.sh        # Created new version
launch_trading_enhanced.sh  # Added features
launch_trading_fast.sh      # Performance optimization
launch_trading_stable.sh    # Bug fixes
launch_trading_production.sh # "Final" version

# Now you have 6 scripts, which one to use? 😱
```

### ✅ Right Way (How to Do It):
```bash
# Initial implementation
launch_trading.sh

# Later: improvements (all in same file)
git commit -m "Add midday optimization"
git commit -m "Improve error handling"
git commit -m "Add performance optimizations"
git commit -m "Fix edge case bugs"
git commit -m "Production hardening"

# Still one script! 😊
# Git history shows all changes
# Everyone knows which script to use
```

---

## Enforcement

### Pre-Commit Checklist:
- [ ] No files with `_v2`, `_new`, `_enhanced`, etc.
- [ ] No commented-out code blocks
- [ ] No `if OLD_VERSION` / `if NEW_VERSION` flags
- [ ] All errors cause immediate exit (no silent fallbacks)
- [ ] No duplicate scripts for the same task

### Code Review Checklist:
- [ ] New script is genuinely new task? (Not improvement of existing)
- [ ] Errors fail fast with clear messages?
- [ ] No fallback logic to hide errors?
- [ ] Old code deleted (not commented out)?
- [ ] Script name is permanent (no version/status)?

---

## 6. Code Quality Enforcement Tools 🔍

**Rule**: Regularly run automated tools to detect violations of principles 1-5.

### 6.1 Fallback Detection (cpp_analyzer.py)

**Purpose**: Detect code that violates "Crash Fast" principle #1.

#### Running the Tool:
```bash
# Scan entire src/ directory for fallbacks
python3 tools/cpp_analyzer.py src/ --fail-on-fallback

# Generate text report
python3 tools/cpp_analyzer.py src/ --fail-on-fallback \
    --format text -o data/tmp/cpp_fallback_report.txt
```

#### What It Detects:

**CRITICAL Violations** (145 found in Oct 2025 cleanup):
```cpp
// BAD - Silent continuation after exception
try {
    parse_csv_line(line);
} catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";  // Flagged as fallback
    // Missing: throw; or exit(1);
}

// BAD - Exception without re-throw
try {
    load_data();
} catch (...) {
    log_error("Failed to load");
    return empty_vector;  // CRITICAL: Fallback instead of crash
}
```

**CORRECT Implementations**:
```cpp
// GOOD - Re-throw after logging
try {
    parse_csv_line(line);
} catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    std::cerr << "Line: " << line << "\n";
    throw;  // ✅ Re-throws to crash fast
}

// GOOD - Exit immediately
if (!file.is_open()) {
    std::cerr << "FATAL: Cannot open file: " << path << "\n";
    exit(1);  // ✅ Crashes immediately
}
```

#### Common False Positives:

The analyzer may flag each `std::cerr` line in a catch block individually:
```cpp
// This is CORRECT (has throw at end) but may flag lines 2-4
try {
    parse_data();
} catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";  // Flagged
    std::cerr << "Line: " << line << "\n";       // Flagged
    std::cerr << "Context: " << ctx << "\n";     // Flagged
    throw;  // Tool may not detect this properly
}
```

**Fix**: Verify catch blocks end with `throw;` or `exit()` before assuming violation.

#### Fixing Violations:

1. **Identify real violations** (ignore false positives)
2. **Replace fallback logic** with crash-fast error handling
3. **Remove default values** that mask errors
4. **Re-throw exceptions** after logging
5. **Exit with clear error messages**

---

### 6.2 Duplicate Code Detection (dupdef_scan_cpp.py)

**Purpose**: Detect code that violates "No Duplicates" principle #2.

#### Running the Tool:
```bash
# Scan include/ and src/ for duplicates
python3 tools/dupdef_scan_cpp.py include/ src/

# Exit with error code if issues found (for CI)
python3 tools/dupdef_scan_cpp.py --fail-on-issues include/ src/

# Save JSON report
python3 tools/dupdef_scan_cpp.py include/ src/ \
    --json-out dupdef_report.json
```

#### What It Detects:

**1. Duplicate Class Definitions**:
```cpp
// BAD - Same class defined multiple times
// File: polygon_client.cpp
class PolygonClient { ... };

// File: polygon_client_old.cpp  // ❌ Duplicate file!
class PolygonClient { ... };

// File: polygon_websocket.cpp   // ❌ Another duplicate!
class PolygonClient { ... };
```

**Fix**: Delete duplicate files. Keep only ONE implementation.

**2. Duplicate Method Implementations (Identical Bodies)**:
```cpp
// BAD - 8 classes with identical reset() methods
class OptimizedSigorStrategyAdapter {
    void reset() override {
        if (strategy_) { strategy_->reset(); }
    }
};

class XGBoostStrategyAdapter {
    void reset() override {
        if (strategy_) { strategy_->reset(); }  // ❌ Duplicate!
    }
};

// ... 6 more identical implementations ...
```

**Fix**: Extract to shared base class or macro:
```cpp
// GOOD - Single macro definition
#define STRATEGY_ADAPTER_RESET_IMPL \
    void reset() override { \
        if (strategy_) { strategy_->reset(); } \
    }

// Use in each class
class OptimizedSigorStrategyAdapter {
    STRATEGY_ADAPTER_RESET_IMPL  // ✅ No duplication
};
```

**3. Version Suffix Files (PROJECT_RULES.md Violation)**:
```
config/rotation_strategy_v2.json      ❌ Has version suffix
config/test_aggressive_v1.json        ❌ Has version suffix
config/test_fixed_final.json          ❌ Temporal naming
config/test_simplified_fresh.json     ❌ Temporal naming
```

**Fix**:
1. Rename to remove suffix: `rotation_strategy_v2.json` → `rotation_strategy.json`
2. Delete old versions (they're in git history)
3. Update all references in code

#### Acceptable "Duplicates" (Not Violations):

Some duplicates are legitimate and should NOT be "fixed":

**1. Small Getters in Different Classes**:
```cpp
// OK - Different classes can have same simple getter
class MockBroker {
    bool is_open() const { return open_; }
};

class DataFeed {
    bool is_open() const { return open_; }  // OK - different class
};
```

**2. Polymorphic Methods (Same Name, Different Logic)**:
```cpp
// OK - Different implementations of same interface
class BBIndicator {
    bool is_ready() const { return win.full(); }
};

class RSIIndicator {
    bool is_ready() const { return count >= 14; }  // OK - different logic
};
```

**3. Code Fragments (1-3 Lines)**:
```cpp
// OK - Common patterns are acceptable
if (i > 0) std::cerr << ", ";  // Common separator pattern
```

#### Fixing Duplicate Violations:

**Example: Oct 2025 Cleanup Results**
- **Before**: ~37 duplicate items
- **Actions**:
  1. Deleted 3 duplicate PolygonClient files (not in CMake)
  2. Consolidated 8 reset() methods into single macro
  3. Removed 4 config files with version suffixes
- **After**: ~24 duplicate items (35% reduction)
- **Result**: ✅ All critical violations eliminated

---

### 6.3 Enforcement Workflow

#### Pre-Commit Checklist (For AI Models):
```bash
# 1. Check for fallback violations
python3 tools/cpp_analyzer.py src/ --fail-on-fallback
# Review output, fix real violations (ignore false positives)

# 2. Check for duplicate code
python3 tools/dupdef_scan_cpp.py include/ src/ --fail-on-issues
# Fix critical duplicates (version files, identical methods)

# 3. Verify build still works
cmake --build build --target sentio_cli -j8

# 4. Document changes
echo "Cleaned up X violations" >> data/tmp/cleanup_summary.md
```

#### Regular Maintenance (Weekly):
```bash
# Run both tools and track progress
python3 tools/cpp_analyzer.py src/ --format text \
    -o data/tmp/weekly_fallback_report.txt

python3 tools/dupdef_scan_cpp.py include/ src/ \
    --json-out data/tmp/weekly_dupdef_report.json

# Compare with previous week
diff data/tmp/weekly_fallback_report.txt \
     data/tmp/last_week_fallback_report.txt
```

#### Integration with CI/CD:
```yaml
# .github/workflows/code-quality.yml
- name: Check for fallbacks
  run: python3 tools/cpp_analyzer.py src/ --fail-on-fallback

- name: Check for duplicates
  run: python3 tools/dupdef_scan_cpp.py include/ src/ --fail-on-issues
```

---

### 6.4 Understanding Tool Output

#### cpp_analyzer.py Output:
```
╔════════════════════════════════════════════════════════════════╗
║     FALLBACK DETECTION ANALYSIS REPORT                         ║
╚════════════════════════════════════════════════════════════════╝

Files Analyzed: 88
Functions Analyzed: 108
Total Issues Found: 190

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
SUMMARY BY SEVERITY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  CRITICAL: 145 issues    ← Fix these immediately
  HIGH: 17 issues         ← Review and fix
  MEDIUM: 28 issues       ← Consider fixing
```

**Priority**: Focus on CRITICAL issues first.

#### dupdef_scan_cpp.py Output:
```
== Duplicate class/struct/enum definitions ==
  PolygonClient
    - src/live/polygon_client_old.cpp:18    ← DELETE THIS FILE
    - src/live/polygon_client.cpp:21        ← DELETE THIS FILE
    - src/live/polygon_websocket.cpp:207    ← DELETE THIS FILE

== Duplicate function/method definitions (identical bodies) ==
  void reset() override [identical_noninline]
    - include/strategy/strategy_adapters.h:100   ← Fix with macro
    - include/strategy/strategy_adapters.h:174   ← Fix with macro
    - include/strategy/strategy_adapters.h:276   ← Fix with macro
    ... (8 total)
```

**Action**: Delete duplicate files, consolidate duplicate methods.

---

### 6.5 Tool Limitations and Gotchas

#### cpp_analyzer.py Limitations:
1. **May flag individual lines** in a catch block that properly re-throws
2. **Cannot detect semantic fallbacks** (e.g., returning empty vector on error)
3. **Requires manual review** of each flagged issue

#### dupdef_scan_cpp.py Limitations:
1. **Substring matching** can cause false positives (e.g., "ureFactory" matches in method names)
2. **Line-based comparison** may miss semantically identical code with different formatting
3. **Doesn't understand context** - flags legitimate polymorphism

#### Best Practice:
**Always manually review tool output before making changes.**

---

### 6.6 Historical Cleanup Example

**Date**: October 15, 2025
**Summary**: Major cleanup after adding these tools

**cpp_analyzer.py Results**:
- 145 CRITICAL issues flagged in `src/core/data_io.cpp`
- Investigation: All false positives (catch blocks properly re-throw)
- Verdict: Analyzer bug - flags each `std::cerr` line individually

**dupdef_scan_cpp.py Results**:
- 37 duplicate items found
- Fixed:
  - Removed 3 duplicate PolygonClient files
  - Consolidated 8 reset() methods with macro
  - Removed 4 config files with version suffixes
- Result: 24 items remaining (35% reduction)

**Build Status**: ✅ All changes compile successfully

**Documentation**: See `data/tmp/duplicate_cleanup_summary.md`

---

## Summary

1. **Crash Fast** - Fail immediately, loudly, clearly
2. **No Duplicates** - ONE script per task, forever
3. **Edit Directly** - Improve in place, delete old code
4. **New = Different** - New script only for new task
5. **Permanent Names** - No versions, no status, no dates
6. **Enforce Quality** - Run cpp_analyzer & dupdef_scan regularly

**Result**: Clean, maintainable, production-ready codebase.

---

## Quick Reference: Tool Commands

```bash
# Detect fallback mechanisms
python3 tools/cpp_analyzer.py src/ --fail-on-fallback

# Detect duplicate code
python3 tools/dupdef_scan_cpp.py include/ src/ --fail-on-issues

# Verify build after cleanup
cmake --build build --target sentio_cli -j8
```

---

**These are not suggestions. These are requirements.**

**Last Updated**: 2025-10-15
