#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "common/types.h"

namespace sentio {

class DataManager {
public:
    DataManager() = default;

    // Load market data from CSV or BIN and register bars with immutable IDs
    void load_market_data(const std::string& path);

    // Lookup by immutable bar id; returns nullptr if not found
    const Bar* get_bar(uint64_t bar_id) const;

    // Lookup by positional index (legacy compatibility)
    const Bar* get_bar_by_index(size_t index) const;

    // Access all loaded bars (ordered)
    const std::vector<Bar>& all_bars() const { return bars_; }

private:
    std::unordered_map<uint64_t, size_t> id_to_index_;
    std::vector<Bar> bars_;
};

} // namespace sentio



