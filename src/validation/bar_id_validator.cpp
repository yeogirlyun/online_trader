#include "validation/bar_id_validator.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace sentio {

std::string BarIdValidator::ValidationResult::to_string() const {
    std::ostringstream oss;
    oss << "=== Bar ID Validation Result ===\n";
    oss << "Status: " << (passed ? "PASSED" : "FAILED") << "\n\n";
    
    oss << "Statistics:\n";
    oss << "  Total Signals: " << total_signals << "\n";
    oss << "  Total Trades: " << total_trades << "\n";
    oss << "  Signals with Trades: " << signals_with_trades << "\n";
    oss << "  Signals with HOLD: " << signals_with_hold << "\n";
    oss << "  Orphan Trades: " << orphan_trades << "\n";
    oss << "  Duplicate Signal IDs: " << duplicate_signal_ids << "\n";
    oss << "  Duplicate Trade IDs: " << duplicate_trade_ids << "\n";
    oss << "  Missing Bar IDs: " << missing_bar_ids << "\n\n";
    
    if (!errors.empty()) {
        oss << "ERRORS (" << errors.size() << "):\n";
        for (const auto& err : errors) {
            oss << "  ✗ " << err << "\n";
        }
        oss << "\n";
    }
    
    if (!warnings.empty()) {
        oss << "WARNINGS (" << warnings.size() << "):\n";
        for (const auto& warn : warnings) {
            oss << "  ⚠ " << warn << "\n";
        }
        oss << "\n";
    }
    
    if (passed && errors.empty() && warnings.empty()) {
        oss << "✓ All validations passed - perfect one-to-one correspondence\n";
    }
    
    return oss.str();
}

BarIdValidator::ValidationResult BarIdValidator::validate(
    const std::vector<SignalOutput>& signals,
    const std::vector<BackendComponent::TradeOrder>& trades,
    bool strict
) {
    ValidationResult result;
    result.total_signals = signals.size();
    result.total_trades = trades.size();
    
    // Build signal bar_id map
    std::unordered_map<uint64_t, const SignalOutput*> signal_map;
    std::unordered_map<uint64_t, size_t> signal_count;
    
    for (const auto& signal : signals) {
        if (signal.bar_id == 0) {
            result.missing_bar_ids++;
            result.errors.push_back(
                "Signal has missing bar_id (timestamp=" + 
                std::to_string(signal.timestamp_ms) + 
                ", symbol=" + signal.symbol + ")"
            );
            continue;
        }
        
        signal_count[signal.bar_id]++;
        if (signal_count[signal.bar_id] > 1) {
            result.duplicate_signal_ids++;
            result.errors.push_back(
                "Duplicate signal bar_id=" + std::to_string(signal.bar_id) + 
                " (count=" + std::to_string(signal_count[signal.bar_id]) + ")"
            );
        }
        
        signal_map[signal.bar_id] = &signal;
    }
    
    // Build trade bar_id map
    std::unordered_map<uint64_t, const BackendComponent::TradeOrder*> trade_map;
    std::unordered_map<uint64_t, size_t> trade_count;
    std::unordered_set<uint64_t> hold_bar_ids;
    
    for (const auto& trade : trades) {
        if (trade.bar_id == 0) {
            result.missing_bar_ids++;
            result.errors.push_back(
                "Trade has missing bar_id (timestamp=" + 
                std::to_string(trade.timestamp_ms) + 
                ", symbol=" + trade.symbol + ")"
            );
            continue;
        }
        
        // Track HOLD decisions separately
        if (trade.action == TradeAction::HOLD) {
            hold_bar_ids.insert(trade.bar_id);
            result.signals_with_hold++;
            continue;
        }
        
        trade_count[trade.bar_id]++;
        if (trade_count[trade.bar_id] > 1) {
            result.duplicate_trade_ids++;
            result.errors.push_back(
                "Duplicate trade bar_id=" + std::to_string(trade.bar_id) + 
                " (count=" + std::to_string(trade_count[trade.bar_id]) + ")"
            );
        }
        
        trade_map[trade.bar_id] = &trade;
    }
    
    // Validate one-to-one correspondence
    for (const auto& [bar_id, signal] : signal_map) {
        auto trade_it = trade_map.find(bar_id);
        bool has_hold = hold_bar_ids.count(bar_id) > 0;
        
        if (trade_it != trade_map.end()) {
            // Signal has corresponding trade
            result.signals_with_trades++;
            const auto* trade = trade_it->second;
            
            // Validate timestamp match
            if (signal->timestamp_ms != trade->timestamp_ms) {
                result.errors.push_back(
                    "Timestamp mismatch for bar_id=" + std::to_string(bar_id) + 
                    ": signal=" + std::to_string(signal->timestamp_ms) + 
                    ", trade=" + std::to_string(trade->timestamp_ms)
                );
            }
            
            // Validate symbol match
            if (signal->symbol != trade->symbol) {
                result.warnings.push_back(
                    "Symbol mismatch for bar_id=" + std::to_string(bar_id) + 
                    ": signal=" + signal->symbol + 
                    ", trade=" + trade->symbol + 
                    " (may be intentional for leveraged instruments)"
                );
            }
            
        } else if (!has_hold) {
            // Signal without trade or HOLD - this is suspicious
            result.warnings.push_back(
                "Signal bar_id=" + std::to_string(bar_id) + 
                " has no corresponding trade or HOLD decision " +
                "(symbol=" + signal->symbol + 
                ", timestamp=" + std::to_string(signal->timestamp_ms) + ")"
            );
        }
    }
    
    // Check for orphan trades (trades without corresponding signal)
    for (const auto& [bar_id, trade] : trade_map) {
        if (signal_map.find(bar_id) == signal_map.end()) {
            result.orphan_trades++;
            result.errors.push_back(
                "Orphan trade bar_id=" + std::to_string(bar_id) + 
                " has no corresponding signal " +
                "(symbol=" + trade->symbol + 
                ", timestamp=" + std::to_string(trade->timestamp_ms) + ")"
            );
        }
    }
    
    // Determine pass/fail
    result.passed = result.errors.empty();
    if (strict && !result.warnings.empty()) {
        result.passed = false;
    }
    
    return result;
}

std::vector<SignalOutput> BarIdValidator::load_signals_from_file(const std::string& path) {
    std::vector<SignalOutput> signals;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open signal file: " + path);
    }
    
    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;
        
        try {
            signals.push_back(SignalOutput::from_json(line));
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Failed to parse signal at line " + std::to_string(line_num) + 
                ": " + e.what()
            );
        }
    }
    
    return signals;
}

std::vector<BackendComponent::TradeOrder> BarIdValidator::load_trades_from_file(const std::string& path) {
    std::vector<BackendComponent::TradeOrder> trades;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open trade file: " + path);
    }
    
    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;
        
#ifdef NLOHMANN_JSON_AVAILABLE
        try {
            auto j = json::parse(line);
            
            BackendComponent::TradeOrder trade;
            trade.bar_id = j.value("bar_id", 0ULL);
            trade.timestamp_ms = j.value("timestamp_ms", 0LL);
            trade.bar_index = j.value("bar_index", 0);
            trade.symbol = j.value("symbol", "");
            
            std::string action_str = j.value("action", "HOLD");
            if (action_str == "BUY") {
                trade.action = TradeAction::BUY;
            } else if (action_str == "SELL") {
                trade.action = TradeAction::SELL;
            } else {
                trade.action = TradeAction::HOLD;
            }
            
            trade.quantity = j.value("quantity", 0.0);
            trade.price = j.value("price", 0.0);
            
            trades.push_back(trade);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Failed to parse trade at line " + std::to_string(line_num) + 
                ": " + e.what()
            );
        }
#else
        throw std::runtime_error("nlohmann::json not available - cannot parse trade file");
#endif
    }
    
    return trades;
}

BarIdValidator::ValidationResult BarIdValidator::validate_signal_file(
    const std::string& signal_file_path,
    bool strict
) {
    auto signals = load_signals_from_file(signal_file_path);
    
    ValidationResult result;
    result.total_signals = signals.size();
    
    std::unordered_set<uint64_t> seen_ids;
    
    for (const auto& signal : signals) {
        if (signal.bar_id == 0) {
            result.missing_bar_ids++;
            result.errors.push_back(
                "Signal has missing bar_id (timestamp=" + 
                std::to_string(signal.timestamp_ms) + ")"
            );
        } else if (seen_ids.count(signal.bar_id) > 0) {
            result.duplicate_signal_ids++;
            result.errors.push_back(
                "Duplicate signal bar_id=" + std::to_string(signal.bar_id)
            );
        } else {
            seen_ids.insert(signal.bar_id);
        }
    }
    
    result.passed = result.errors.empty();
    if (strict && !result.warnings.empty()) {
        result.passed = false;
    }
    
    return result;
}

BarIdValidator::ValidationResult BarIdValidator::validate_trade_file(
    const std::string& trade_file_path,
    bool strict
) {
    auto trades = load_trades_from_file(trade_file_path);
    
    ValidationResult result;
    result.total_trades = trades.size();
    
    std::unordered_set<uint64_t> seen_ids;
    
    for (const auto& trade : trades) {
        if (trade.action == TradeAction::HOLD) {
            result.signals_with_hold++;
            continue;
        }
        
        if (trade.bar_id == 0) {
            result.missing_bar_ids++;
            result.errors.push_back(
                "Trade has missing bar_id (timestamp=" + 
                std::to_string(trade.timestamp_ms) + ")"
            );
        } else if (seen_ids.count(trade.bar_id) > 0) {
            result.duplicate_trade_ids++;
            result.errors.push_back(
                "Duplicate trade bar_id=" + std::to_string(trade.bar_id)
            );
        } else {
            seen_ids.insert(trade.bar_id);
        }
    }
    
    result.passed = result.errors.empty();
    if (strict && !result.warnings.empty()) {
        result.passed = false;
    }
    
    return result;
}

BarIdValidator::ValidationResult BarIdValidator::validate_files(
    const std::string& signal_file_path,
    const std::string& trade_file_path,
    bool strict
) {
    auto signals = load_signals_from_file(signal_file_path);
    auto trades = load_trades_from_file(trade_file_path);
    
    return validate(signals, trades, strict);
}

void BarIdValidator::assert_one_to_one(
    const std::vector<SignalOutput>& signals,
    const std::vector<BackendComponent::TradeOrder>& trades
) {
    auto result = validate(signals, trades, true);
    
    if (!result.passed) {
        std::ostringstream oss;
        oss << "Bar ID validation FAILED - one-to-one correspondence violated!\n\n";
        oss << result.to_string();
        throw std::runtime_error(oss.str());
    }
}

} // namespace sentio
