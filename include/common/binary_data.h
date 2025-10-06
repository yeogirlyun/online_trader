#pragma once

// =============================================================================
// Module: common/binary_data.h
// Purpose: High-performance binary market data storage and retrieval system
//
// This module provides a fast, efficient alternative to CSV processing by:
// 1. Storing market data in compact binary format for instant loading
// 2. Supporting index-based range queries without temporary files
// 3. Maintaining consistent indexing across the entire trading pipeline
// 4. Eliminating symbol extraction issues and data alignment problems
//
// Binary Format:
// - Header: [magic][version][symbol_len][symbol][bar_count][reserved]
// - Data: [Bar1][Bar2]...[BarN] (fixed-size structs)
// - Each Bar: timestamp_ms(8) + open(8) + high(8) + low(8) + close(8) + volume(8) = 48 bytes
//
// Performance Benefits:
// - Loading: ~100x faster than CSV parsing
// - Memory: Compact binary representation
// - Access: O(1) random access by index
// - Consistency: Single data source eliminates alignment issues
// =============================================================================

#include "common/types.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

namespace sentio {
namespace binary_data {

// Magic number for file format validation
static constexpr uint32_t BINARY_DATA_MAGIC = 0x53454E54; // "SENT"
static constexpr uint32_t BINARY_DATA_VERSION = 1;

// Binary file header structure
struct BinaryHeader {
    uint32_t magic;           // File format magic number
    uint32_t version;         // Format version
    uint32_t symbol_length;   // Length of symbol string
    char symbol[16];          // Symbol name (null-terminated, max 15 chars)
    uint64_t bar_count;       // Number of bars in file
    uint64_t reserved[4];     // Reserved for future use
    
    BinaryHeader() : magic(BINARY_DATA_MAGIC), version(BINARY_DATA_VERSION), 
                     symbol_length(0), bar_count(0) {
        std::memset(symbol, 0, sizeof(symbol));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

// Binary bar structure (48 bytes, cache-friendly)
struct BinaryBar {
    uint64_t timestamp_ms;    // Unix timestamp in milliseconds
    double open;              // Opening price
    double high;              // High price  
    double low;               // Low price
    double close;             // Closing price
    double volume;            // Trading volume
    
    // Convert to/from standard Bar structure
    Bar to_bar(const std::string& symbol) const;
    static BinaryBar from_bar(const Bar& bar);
};

// High-performance binary data reader
class BinaryDataReader {
public:
    explicit BinaryDataReader(const std::string& binary_file_path);
    ~BinaryDataReader();
    
    // Core functionality
    bool open();
    void close();
    bool is_open() const { return file_.is_open(); }
    
    // Metadata access
    const std::string& get_symbol() const { return symbol_; }
    uint64_t get_bar_count() const { return bar_count_; }
    
    // High-performance data access
    std::vector<Bar> read_range(uint64_t start_index, uint64_t count) const;
    std::vector<Bar> read_last_n_bars(uint64_t count) const;
    Bar read_single_bar(uint64_t index) const;
    
    // Utility functions
    bool validate_index(uint64_t index) const { return index < bar_count_; }
    bool validate_range(uint64_t start_index, uint64_t count) const {
        return start_index < bar_count_ && (start_index + count) <= bar_count_;
    }
    
private:
    std::string file_path_;
    mutable std::ifstream file_;
    std::string symbol_;
    uint64_t bar_count_;
    uint64_t data_offset_;    // Offset to first bar in file
    
    bool read_header();
};

// Binary data writer (for CSV conversion)
class BinaryDataWriter {
public:
    explicit BinaryDataWriter(const std::string& binary_file_path);
    ~BinaryDataWriter();
    
    // Core functionality
    bool create(const std::string& symbol);
    bool write_bars(const std::vector<Bar>& bars);
    bool finalize();
    void close();
    
    // Utility
    bool is_open() const { return file_.is_open(); }
    uint64_t get_written_count() const { return written_count_; }
    
private:
    std::string file_path_;
    std::ofstream file_;
    std::string symbol_;
    uint64_t written_count_;
    
    bool write_header();
    bool update_header();
};

// Conversion utilities
namespace converter {
    // Convert CSV file to binary format
    bool csv_to_binary(const std::string& csv_path, const std::string& binary_path);
    
    // Batch convert all CSV files in directory
    bool convert_directory(const std::string& csv_dir, const std::string& binary_dir);
    
    // Validate binary file integrity
    bool validate_binary_file(const std::string& binary_path);
}

} // namespace binary_data
} // namespace sentio
