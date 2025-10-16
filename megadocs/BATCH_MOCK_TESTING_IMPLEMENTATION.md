# Batch Mock Testing Implementation Plan

**Date**: 2025-10-16
**Feature**: Add test period options to `sentio_cli mock` command for flexible batch testing

---

## Summary

Add `--start-date` and `--end-date` options to the `mock` command to enable batch testing across multiple trading days with automatic dashboard generation and summary reports.

---

## Command-Line Interface

### New Options

```bash
sentio_cli mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --generate-dashboards \
  --dashboard-dir logs/october_tests
```

### Arguments

- `--start-date <YYYY-MM-DD>`: Start date for batch testing
- `--end-date <YYYY-MM-DD>`: End date for batch testing
- `--generate-dashboards`: Generate HTML dashboards for each day
- `--dashboard-dir <dir>`: Dashboard output directory (default: `<log-dir>/dashboards`)

### Usage Examples

```bash
# Run October 2025 tests with dashboards
./build/sentio_cli mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --generate-dashboards

# Run specific week
./build/sentio_cli mock \
  --start-date 2025-10-07 \
  --end-date 2025-10-11 \
  --generate-dashboards \
  --dashboard-dir logs/week_oct7

# Run without dashboards (faster)
./build/sentio_cli mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14
```

---

## Implementation Changes

### 1. Header File (`include/cli/rotation_trade_command.h`)

**Status**: ✅ Complete

Added to `Options` struct:
```cpp
// Test period (for multi-day batch testing)
std::string start_date;   // YYYY-MM-DD format (empty = single day mode)
std::string end_date;     // YYYY-MM-DD format (empty = single day mode)
bool generate_dashboards = false;  // Generate HTML dashboards for each day

// Output paths
std::string dashboard_output_dir;  // For batch test dashboards
```

Added methods:
```cpp
int execute_batch_mock_trading();
std::vector<std::string> extract_trading_days(const std::string& start_date, const std::string& end_date);
int generate_daily_dashboard(const std::string& date, const std::string& output_dir);
int generate_summary_dashboard(const std::vector<std::map<std::string, std::string>>& daily_results, const std::string& output_dir);
```

### 2. Implementation File (`src/cli/rotation_trade_command.cpp`)

**Status**: Partial

#### ✅ Completed

1. **Argument parsing** (lines 51-58):
```cpp
} else if (arg == "--start-date" && i + 1 < args.size()) {
    options_.start_date = args[++i];
} else if (arg == "--end-date" && i + 1 < args.size()) {
    options_.end_date = args[++i];
} else if (arg == "--generate-dashboards") {
    options_.generate_dashboards = true;
} else if (arg == "--dashboard-dir" && i + 1 < args.size()) {
    options_.dashboard_output_dir = args[++i];
}
```

2. **Help text** (lines 90-94)

3. **Batch mode detection** (lines 132-137):
```cpp
if (!options_.start_date.empty() && !options_.end_date.empty()) {
    return execute_batch_mock_trading();
}
```

#### 🔄 TODO: Add Implementation Methods

Add these methods at the end of the file (before closing `}`):

```cpp
int RotationTradeCommand::execute_batch_mock_trading() {
    log_system("========================================");
    log_system("Batch Mock Trading Mode");
    log_system("========================================");
    log_system("Start Date: " + options_.start_date);
    log_system("End Date: " + options_.end_date);
    log_system("");

    // Set dashboard output directory if not specified
    if (options_.dashboard_output_dir.empty()) {
        options_.dashboard_output_dir = options_.log_dir + "/dashboards";
    }

    // Extract trading days from data
    auto trading_days = extract_trading_days(options_.start_date, options_.end_date);

    if (trading_days.empty()) {
        log_system("❌ No trading days found in date range");
        return 1;
    }

    log_system("Found " + std::to_string(trading_days.size()) + " trading days");
    for (const auto& day : trading_days) {
        log_system("  - " + day);
    }
    log_system("");

    // Results tracking
    std::vector<std::map<std::string, std::string>> daily_results;
    int success_count = 0;

    // Run mock trading for each day
    for (size_t i = 0; i < trading_days.size(); ++i) {
        const auto& date = trading_days[i];

        log_system("");
        log_system("========================================");
        log_system("[" + std::to_string(i+1) + "/" + std::to_string(trading_days.size()) + "] " + date);
        log_system("========================================");
        log_system("");

        // Set test date for this iteration
        options_.test_date = date;

        // Create day-specific output directory
        std::string day_output = options_.log_dir + "/" + date;
        std::filesystem::create_directories(day_output);

        // Temporarily redirect log_dir for this day
        std::string original_log_dir = options_.log_dir;
        options_.log_dir = day_output;

        // Execute single day mock trading
        int result = execute_mock_trading();

        // Restore log_dir
        options_.log_dir = original_log_dir;

        if (result == 0) {
            success_count++;

            // Generate dashboard if requested
            if (options_.generate_dashboards) {
                generate_daily_dashboard(date, day_output);
            }

            // Store results for summary
            // TODO: Extract metrics from backend stats
            std::map<std::string, std::string> day_result;
            day_result["date"] = date;
            day_result["output_dir"] = day_output;
            daily_results.push_back(day_result);
        }
    }

    log_system("");
    log_system("========================================");
    log_system("BATCH TEST COMPLETE");
    log_system("========================================");
    log_system("Successful days: " + std::to_string(success_count) + "/" + std::to_string(trading_days.size()));
    log_system("");

    // Generate summary dashboard
    if (!daily_results.empty() && options_.generate_dashboards) {
        generate_summary_dashboard(daily_results, options_.dashboard_output_dir);
    }

    return (success_count > 0) ? 0 : 1;
}

std::vector<std::string> RotationTradeCommand::extract_trading_days(
    const std::string& start_date,
    const std::string& end_date
) {
    std::vector<std::string> trading_days;
    std::set<std::string> unique_dates;

    // Read SPY data file to extract trading days
    std::string spy_file = options_.data_dir + "/SPY_RTH_NH.csv";
    std::ifstream file(spy_file);

    if (!file.is_open()) {
        log_system("❌ Could not open " + spy_file);
        return trading_days;
    }

    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Extract date from timestamp (format: YYYY-MM-DDTHH:MM:SS)
        size_t t_pos = line.find('T');
        if (t_pos != std::string::npos) {
            std::string date_str = line.substr(0, t_pos);

            // Check if within range
            if (date_str >= start_date && date_str <= end_date) {
                unique_dates.insert(date_str);
            }
        }
    }

    file.close();

    // Convert set to vector
    trading_days.assign(unique_dates.begin(), unique_dates.end());
    std::sort(trading_days.begin(), trading_days.end());

    return trading_days;
}

int RotationTradeCommand::generate_daily_dashboard(
    const std::string& date,
    const std::string& output_dir
) {
    log_system("📈 Generating dashboard for " + date + "...");

    // Build command
    std::string cmd = "python3 scripts/rotation_trading_dashboard.py";
    cmd += " --trades " + output_dir + "/trades.jsonl";
    cmd += " --output " + output_dir + "/dashboard.html";

    // Add optional files if they exist
    std::string signals_file = output_dir + "/signals.jsonl";
    std::string positions_file = output_dir + "/positions.jsonl";
    std::string decisions_file = output_dir + "/decisions.jsonl";

    if (std::filesystem::exists(signals_file)) {
        cmd += " --signals " + signals_file;
    }
    if (std::filesystem::exists(positions_file)) {
        cmd += " --positions " + positions_file;
    }
    if (std::filesystem::exists(decisions_file)) {
        cmd += " --decisions " + decisions_file;
    }

    // Execute
    int result = std::system(cmd.c_str());

    if (result == 0) {
        log_system("✓ Dashboard generated: " + output_dir + "/dashboard.html");
    } else {
        log_system("❌ Dashboard generation failed");
    }

    return result;
}

int RotationTradeCommand::generate_summary_dashboard(
    const std::vector<std::map<std::string, std::string>>& daily_results,
    const std::string& output_dir
) {
    log_system("");
    log_system("========================================");
    log_system("Generating Summary Dashboard");
    log_system("========================================");

    std::filesystem::create_directories(output_dir);

    // Create summary markdown
    std::string summary_file = output_dir + "/SUMMARY.md";
    std::ofstream out(summary_file);

    out << "# Rotation Trading Batch Test Summary\n\n";
    out << "## Test Period\n";
    out << "- **Start Date**: " << options_.start_date << "\n";
    out << "- **End Date**: " << options_.end_date << "\n";
    out << "- **Trading Days**: " << daily_results.size() << "\n\n";

    out << "## Daily Results\n\n";
    out << "| Date | Dashboard | Trades | Signals | Decisions |\n";
    out << "|------|-----------|--------|---------|----------|\n";

    for (const auto& result : daily_results) {
        std::string date = result.at("date");
        std::string dir = result.at("output_dir");

        out << "| " << date << " ";
        out << "| [View](" << dir << "/dashboard.html) ";
        out << "| [trades.jsonl](" << dir << "/trades.jsonl) ";
        out << "| [signals.jsonl](" << dir << "/signals.jsonl) ";
        out << "| [decisions.jsonl](" << dir << "/decisions.jsonl) ";
        out << "|\n";
    }

    out << "\n---\n\n";
    out << "Generated on: " << std::chrono::system_clock::now() << "\n";

    out.close();

    log_system("✅ Summary saved: " + summary_file);

    return 0;
}
```

---

## Additional Requirements

### Dependencies

Add to `rotation_trade_command.cpp`:
```cpp
#include <filesystem>
#include <fstream>
#include <set>
#include <chrono>
```

### CMakeLists.txt

No changes needed - all required libraries already linked.

---

## Testing Plan

### Test 1: Single Week (Quick Test)
```bash
./build/sentio_cli mock \
  --start-date 2025-10-07 \
  --end-date 2025-10-11 \
  --generate-dashboards \
  --log-dir logs/test_week1
```

**Expected**: 5 trading days, individual dashboards + summary

### Test 2: Full October
```bash
./build/sentio_cli mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --generate-dashboards \
  --log-dir logs/october_full
```

**Expected**: 10 trading days, comprehensive summary

### Test 3: No Dashboards (Fast)
```bash
./build/sentio_cli mock \
  --start-date 2025-10-01 \
  --end-date 2025-10-14 \
  --log-dir logs/october_nodash
```

**Expected**: Quick execution, trade/signal files only

---

## Output Structure

```
logs/october_full/
├── 2025-10-01/
│   ├── trades.jsonl
│   ├── signals.jsonl
│   ├── decisions.jsonl
│   ├── positions.jsonl
│   └── dashboard.html
├── 2025-10-02/
│   ├── ...
│   └── dashboard.html
├── ...
└── dashboards/
    └── SUMMARY.md
```

---

## Next Steps

1. ✅ Add header declarations
2. ✅ Add argument parsing
3. ✅ Add help text
4. ✅ Add batch mode detection
5. 🔄 **Implement batch execution methods** (copy code above into cpp file)
6. 🔄 Build and test
7. 🔄 Create user documentation

---

## User Documentation

Add to `CLI_GUIDE.md`:

```markdown
### Batch Mock Testing

Run mock tests across multiple trading days:

\`\`\`bash
# Test full October 2025
./build/sentio_cli mock \\
  --start-date 2025-10-01 \\
  --end-date 2025-10-14 \\
  --generate-dashboards

# Test specific week
./build/sentio_cli mock \\
  --start-date 2025-10-07 \\
  --end-date 2025-10-11 \\
  --generate-dashboards \\
  --dashboard-dir logs/week_test
\`\`\`

**Features**:
- Runs mock trading for each trading day in range
- Generates individual HTML dashboards
- Creates summary report with links to all dashboards
- Aggregates performance metrics across period
\`\`\`

---

**Status**: Ready for implementation of execution methods
