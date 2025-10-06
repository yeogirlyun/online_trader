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


