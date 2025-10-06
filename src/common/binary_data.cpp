#include "common/binary_data.h"
#include "common/utils.h"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <limits>

namespace sentio {
namespace binary_data {

// =============================================================================
// BinaryBar Implementation
// =============================================================================

Bar BinaryBar::to_bar(const std::string& symbol) const {
    Bar bar;
    bar.timestamp_ms = timestamp_ms;
    bar.symbol = symbol;
    bar.open = open;
    bar.high = high;
    bar.low = low;
    bar.close = close;
    bar.volume = volume;
    return bar;
}

BinaryBar BinaryBar::from_bar(const Bar& bar) {
    BinaryBar binary_bar;
    binary_bar.timestamp_ms = bar.timestamp_ms;
    binary_bar.open = bar.open;
    binary_bar.high = bar.high;
    binary_bar.low = bar.low;
    binary_bar.close = bar.close;
    binary_bar.volume = bar.volume;
    return binary_bar;
}

// =============================================================================
// BinaryDataReader Implementation
// =============================================================================

BinaryDataReader::BinaryDataReader(const std::string& binary_file_path)
    : file_path_(binary_file_path), bar_count_(0), data_offset_(0) {
}

BinaryDataReader::~BinaryDataReader() {
    close();
}

bool BinaryDataReader::open() {
    close(); // Ensure clean state
    
    file_.open(file_path_, std::ios::binary);
    if (!file_.is_open()) {
        utils::log_error("Failed to open binary data file: " + file_path_);
        return false;
    }
    
    if (!read_header()) {
        close();
        return false;
    }
    
    utils::log_info("Opened binary data file: " + file_path_ + 
                    " (symbol=" + symbol_ + ", bars=" + std::to_string(bar_count_) + ")");
    return true;
}

void BinaryDataReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool BinaryDataReader::read_header() {
    BinaryHeader header;
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (file_.gcount() != sizeof(header)) {
        utils::log_error("Failed to read binary file header");
        return false;
    }
    
    if (header.magic != BINARY_DATA_MAGIC) {
        utils::log_error("Invalid binary file magic number");
        return false;
    }
    
    if (header.version != BINARY_DATA_VERSION) {
        utils::log_error("Unsupported binary file version: " + std::to_string(header.version));
        return false;
    }
    
    symbol_ = std::string(header.symbol);
    bar_count_ = header.bar_count;
    data_offset_ = sizeof(BinaryHeader);
    
    return true;
}

std::vector<Bar> BinaryDataReader::read_range(uint64_t start_index, uint64_t count) const {
    std::vector<Bar> bars;
    
    if (!validate_range(start_index, count)) {
        utils::log_error("Invalid range: start=" + std::to_string(start_index) + 
                         ", count=" + std::to_string(count) + 
                         ", total=" + std::to_string(bar_count_));
        return bars;
    }
    
    // Seek to start position
    uint64_t byte_offset = data_offset_ + (start_index * sizeof(BinaryBar));
    file_.seekg(byte_offset);
    
    if (file_.fail()) {
        utils::log_error("Failed to seek to position: " + std::to_string(byte_offset));
        return bars;
    }
    
    // Read binary bars in batch
    std::vector<BinaryBar> binary_bars(count);
    file_.read(reinterpret_cast<char*>(binary_bars.data()), count * sizeof(BinaryBar));
    
    if (file_.gcount() != static_cast<std::streamsize>(count * sizeof(BinaryBar))) {
        utils::log_error("Failed to read expected number of bars");
        return bars;
    }
    
    // Convert to standard Bar format
    bars.reserve(count);
    for (const auto& binary_bar : binary_bars) {
        bars.push_back(binary_bar.to_bar(symbol_));
    }
    
    utils::log_debug("Read " + std::to_string(count) + " bars from index " + 
                    std::to_string(start_index) + " (symbol=" + symbol_ + ")");
    
    return bars;
}

std::vector<Bar> BinaryDataReader::read_last_n_bars(uint64_t count) const {
    if (count > bar_count_) {
        count = bar_count_;
    }
    
    uint64_t start_index = bar_count_ - count;
    return read_range(start_index, count);
}

Bar BinaryDataReader::read_single_bar(uint64_t index) const {
    auto bars = read_range(index, 1);
    if (bars.empty()) {
        return Bar{}; // Return default-constructed bar on error
    }
    return bars[0];
}

// =============================================================================
// BinaryDataWriter Implementation
// =============================================================================

BinaryDataWriter::BinaryDataWriter(const std::string& binary_file_path)
    : file_path_(binary_file_path), written_count_(0) {
}

BinaryDataWriter::~BinaryDataWriter() {
    close();
}

bool BinaryDataWriter::create(const std::string& symbol) {
    close(); // Ensure clean state
    
    symbol_ = symbol;
    written_count_ = 0;
    
    // Create directory if needed
    std::filesystem::path file_path(file_path_);
    std::filesystem::create_directories(file_path.parent_path());
    
    file_.open(file_path_, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        utils::log_error("Failed to create binary data file: " + file_path_);
        return false;
    }
    
    if (!write_header()) {
        close();
        return false;
    }
    
    utils::log_info("Created binary data file: " + file_path_ + " (symbol=" + symbol_ + ")");
    return true;
}

bool BinaryDataWriter::write_header() {
    BinaryHeader header;
    header.symbol_length = symbol_.length();
    std::strncpy(header.symbol, symbol_.c_str(), sizeof(header.symbol) - 1);
    header.bar_count = 0; // Will be updated in finalize()
    
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return file_.good();
}

bool BinaryDataWriter::write_bars(const std::vector<Bar>& bars) {
    if (!file_.is_open()) {
        utils::log_error("Binary file not open for writing");
        return false;
    }
    
    for (const auto& bar : bars) {
        BinaryBar binary_bar = BinaryBar::from_bar(bar);
        file_.write(reinterpret_cast<const char*>(&binary_bar), sizeof(binary_bar));
        
        if (file_.fail()) {
            utils::log_error("Failed to write bar at index " + std::to_string(written_count_));
            return false;
        }
        
        written_count_++;
    }
    
    utils::log_debug("Wrote " + std::to_string(bars.size()) + " bars (total=" + 
                    std::to_string(written_count_) + ")");
    return true;
}

bool BinaryDataWriter::finalize() {
    if (!file_.is_open()) {
        return false;
    }
    
    // Update header with final bar count
    file_.seekp(0);
    if (file_.fail()) {
        utils::log_error("Failed to seek to header for finalization");
        return false;
    }
    
    BinaryHeader header;
    header.symbol_length = symbol_.length();
    std::strncpy(header.symbol, symbol_.c_str(), sizeof(header.symbol) - 1);
    header.bar_count = written_count_;
    
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file_.flush();
    
    if (file_.fail()) {
        utils::log_error("Failed to update header");
        return false;
    }
    
    utils::log_info("Finalized binary file: " + file_path_ + 
                   " (bars=" + std::to_string(written_count_) + ")");
    return true;
}

void BinaryDataWriter::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

// =============================================================================
// Conversion Utilities Implementation
// =============================================================================

namespace converter {

bool csv_to_binary(const std::string& csv_path, const std::string& binary_path) {
    utils::log_info("Converting CSV to binary: " + csv_path + " -> " + binary_path);
    
    // Load CSV data using existing utility
    auto bars = utils::read_csv_data(csv_path);
    if (bars.empty()) {
        utils::log_error("Failed to load CSV data or file is empty");
        return false;
    }
    
    // Extract symbol from first bar (should be consistent)
    std::string symbol = bars[0].symbol;
    if (symbol.empty() || symbol == "UNKNOWN") {
        utils::log_error("Invalid symbol in CSV data: " + symbol);
        return false;
    }
    
    // Create binary writer
    BinaryDataWriter writer(binary_path);
    if (!writer.create(symbol)) {
        return false;
    }
    
    // Write bars in batches for memory efficiency
    const size_t BATCH_SIZE = 10000;
    for (size_t i = 0; i < bars.size(); i += BATCH_SIZE) {
        size_t end = std::min(i + BATCH_SIZE, bars.size());
        std::vector<Bar> batch(bars.begin() + i, bars.begin() + end);
        
        if (!writer.write_bars(batch)) {
            utils::log_error("Failed to write batch starting at index " + std::to_string(i));
            return false;
        }
    }
    
    if (!writer.finalize()) {
        return false;
    }
    
    utils::log_info("Successfully converted " + std::to_string(bars.size()) + 
                   " bars to binary format");
    return true;
}

bool convert_directory(const std::string& csv_dir, const std::string& binary_dir) {
    utils::log_info("Converting directory: " + csv_dir + " -> " + binary_dir);

    if (!std::filesystem::exists(csv_dir)) {
        utils::log_error("Source CSV directory does not exist: " + csv_dir);
        return false;
    }
    std::filesystem::create_directories(binary_dir);

    // Create manifest for traceability
    std::string manifest_path = binary_dir + "/manifest.txt";
    std::ofstream manifest(manifest_path, std::ios::trunc);
    if (!manifest.is_open()) {
        utils::log_error("Failed to create manifest: " + manifest_path);
        return false;
    }

    int converted = 0;
    int failed = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(csv_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".csv") continue;
        std::string csv_file = entry.path().string();
        std::filesystem::path rel = std::filesystem::relative(entry.path(), csv_dir);
        std::filesystem::path out_path = std::filesystem::path(binary_dir) / rel;
        out_path.replace_extension(".bin");
        std::filesystem::create_directories(out_path.parent_path());

        bool ok = csv_to_binary(csv_file, out_path.string());
        bool valid = ok && validate_binary_file(out_path.string());
    manifest << (valid ? "OK  " : "FAIL") << " " << csv_file << " -> " << out_path.string() << "\n";
        if (valid) converted++; else failed++;
    }

    utils::log_info("Directory conversion complete: " + std::to_string(converted) +
                   " converted, " + std::to_string(failed) + " failed");
    return failed == 0;
}

bool validate_binary_file(const std::string& binary_path) {
    BinaryDataReader reader(binary_path);
    if (!reader.open()) return false;

    const uint64_t n = reader.get_bar_count();
    if (n == 0) {
        utils::log_error("Binary file is empty");
        return false;
    }

    // Check symbol and monotonic timestamps with sampling
    const std::string symbol = reader.get_symbol();
    if (symbol.empty()) {
        utils::log_error("Binary file has empty symbol");
        return false;
    }

    // File size sanity: header + n * sizeof(BinaryBar)
    std::error_code ec;
    auto file_sz = std::filesystem::file_size(binary_path, ec);
    if (ec) {
        utils::log_error("Failed to get file size for: " + binary_path);
        return false;
    }
    const uint64_t expected = sizeof(BinaryHeader) + n * sizeof(BinaryBar);
    if (file_sz != expected) {
        utils::log_error("Binary size mismatch: expected=" + std::to_string(expected) +
                         ", actual=" + std::to_string(file_sz));
        return false;
    }

    uint64_t step = std::max<uint64_t>(1, n / 1024);
    int64_t prev_ts = -1;
    for (uint64_t i = 0; i < n; i += step) {
        Bar b = reader.read_single_bar(i);
        if (b.symbol != symbol) {
            utils::log_error("Symbol mismatch inside binary file");
            return false;
        }
        if (b.timestamp_ms <= prev_ts) {
            utils::log_error("Non-monotonic timestamps in binary file");
            return false;
        }
        // Field sanity checks
        if (!(b.open > 0.0 && b.high > 0.0 && b.low > 0.0 && b.close > 0.0)) {
            utils::log_error("Non-positive OHLC encountered in binary file");
            return false;
        }
        if (b.high < std::max(b.open, b.close) || b.low > std::min(b.open, b.close)) {
            utils::log_error("Inconsistent OHLC ranges in binary file");
            return false;
        }
        if (!(b.volume >= 0.0) || !std::isfinite(b.volume)) {
            utils::log_error("Invalid volume encountered in binary file");
            return false;
        }
        prev_ts = b.timestamp_ms;
    }

    // Spot check edges
    Bar first = reader.read_single_bar(0);
    Bar last = reader.read_single_bar(n - 1);
    if (first.timestamp_ms >= last.timestamp_ms) {
        utils::log_error("Invalid edge timestamps in binary file");
        return false;
    }

    utils::log_info("Binary file validation passed: " + binary_path +
                   " (symbol=" + symbol + ", bars=" + std::to_string(n) + ")");
    return true;
}

} // namespace converter
} // namespace binary_data
} // namespace sentio
