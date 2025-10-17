# Quick Fixes for Data Consistency Issues

## Critical Issues Requiring Immediate Action

### Issue 1: Missing Data Files
**Current**: Config specifies 20 symbols but only 13 have data files
**Action**: Edit `/Volumes/ExternalSSD/Dev/C++/online_trader/config/rotation_strategy.json`

Remove lines 9 and 24-31 to delete these symbols:
```json
"AAPL", "MSFT", "AMZN", "TSLA", "NVDA", "META", "BRK.B", "GOOGL"
```

Also remove corresponding lines from `leverage_boosts` section (lines 24-31).

**Alternative**: Provide data files for these 8 symbols in `data/equities/`

---

### Issue 2: Empty Stub CSV Files
**Files to remove**:
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/QQQ.csv`
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SQQQ_NH.csv`
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SQQQ.csv`
- `/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/TQQQ.csv`

**Reason**: These contain only headers and will cause empty bar vectors to be returned

---

### Issue 3: CSV Parsing Without Exception Handling
**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/common/utils.cpp`
**Lines**: 118-184

**Fix**: Wrap the parsing loop in try-catch:

```cpp
// Process each data row according to the detected format
size_t sequence_index = 0;
size_t parse_errors = 0;

while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string item;
    Bar b{};

    try {
        // Parse timestamp and symbol based on detected format
        if (is_qqq_format) {
            // ... existing code ...
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
        
    } catch (const std::exception& e) {
        parse_errors++;
        // Log and skip malformed line
        continue;
    }
}

if (parse_errors > 0) {
    utils::log_warning("read_csv_data: skipped " + std::to_string(parse_errors) + 
                       " malformed lines from " + path);
}
```

---

## Important Issues Requiring Changes Before Next Release

### Issue 4: Git Tracking Log Files
**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/.gitignore`

**Add these lines**:
```gitignore
# Test output logs - should be generated, not tracked
logs/**/*.jsonl
logs/**/*.log
logs/**/
!logs/.gitkeep

# Temporary test data
data/tmp/**/
!data/tmp/.gitkeep
```

**Also run**:
```bash
git rm --cached logs/**/*.jsonl
git rm --cached logs/**/*.log
git commit -m "Remove tracked log files"
```

---

### Issue 5: Configuration Validation
**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/src/cli/rotation_trade_command.cpp`
**Function**: `load_config()` (lines 147-248)

**Add after line 222**:
```cpp
// VALIDATION: Check configuration bounds
if (config.oes_config.buy_threshold <= 0.0 || config.oes_config.buy_threshold > 1.0) {
    throw std::runtime_error("buy_threshold must be in (0, 1], got " + 
                            std::to_string(config.oes_config.buy_threshold));
}
if (config.oes_config.sell_threshold <= 0.0 || config.oes_config.sell_threshold > 1.0) {
    throw std::runtime_error("sell_threshold must be in (0, 1], got " + 
                            std::to_string(config.oes_config.sell_threshold));
}
if (config.oes_config.buy_threshold <= config.oes_config.sell_threshold) {
    throw std::runtime_error("buy_threshold (" + 
                            std::to_string(config.oes_config.buy_threshold) + 
                            ") must be > sell_threshold (" + 
                            std::to_string(config.oes_config.sell_threshold) + ")");
}
if (config.symbols.empty()) {
    throw std::runtime_error("No symbols configured");
}
```

---

### Issue 6: Clean Up Temporary Test Data
**Directory**: `/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/todays_bars/`

**Action**: Either remove or document the purpose:
```bash
# Option 1: Remove if not needed
rm -rf data/tmp/todays_bars/

# Option 2: Add to gitignore if it's a live data cache
echo "data/tmp/todays_bars/" >> .gitignore
```

---

## Unused Configuration Parameters to Remove or Implement

**File**: `/Volumes/ExternalSSD/Dev/C++/online_trader/config/rotation_strategy.json`

### Parameters to Document/Implement:
1. **Line 54-57**: Threshold calibration - decide if needed
   - `enable_threshold_calibration`
   - `calibration_window`
   - `threshold_step`

2. **Line 59-61**: Kelly sizing - decide if needed
   - `enable_kelly_sizing`
   - `kelly_fraction`
   - `max_position_size`

3. **Line 69-70**: Correlation filtering - decide if needed
   - `enable_correlation_filter`
   - `max_correlation`

4. **Line 78-85**: Rotation mechanics - check if implemented
   - `min_rank_to_hold`
   - `rotation_cooldown_bars`
   - `minimum_hold_bars`

5. **Line 88**: Volatility weighting
   - `volatility_weight` - Currently has hardcoded values in code

### Quick Decision Matrix:

| Parameter | Status | Action |
|-----------|--------|--------|
| kelly_sizing | Disabled | REMOVE from config unless implementing |
| correlation_filter | Disabled | REMOVE from config unless implementing |
| threshold_calibration | Not used | REMOVE from config unless implementing |
| volatility_weight | Hardcoded | Load from config instead of hardcoding |
| data_manager_config | Not loaded | Add config loading in `load_config()` |

---

## Verification Checklist

After applying fixes, verify:

- [ ] All 13 symbols in config have matching data files
- [ ] No empty CSV stub files in `data/equities/`
- [ ] `read_csv_data()` has try-catch around all numeric conversions
- [ ] `.gitignore` blocks log files
- [ ] `rotation_strategy.json` contains only implemented parameters
- [ ] Configuration validation runs in `load_config()`
- [ ] No tracked log files in git
- [ ] System can load and run with rotation trading without missing data errors

---

## Testing

After fixes, run:
```bash
# Test configuration loading
./sentio_cli rotation-trade --mode mock --date 2025-10-15

# Expected result: No missing symbol warnings for the 8 deleted symbols
# Expected result: Configuration validation passes
```

