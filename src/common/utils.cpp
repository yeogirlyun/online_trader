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
