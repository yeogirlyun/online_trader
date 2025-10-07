#pragma once
#include "perf_smallvec.h"
#include <vector>
#include <cstdint>

namespace sentio {

// Lightweight update record (no feature copy)
struct Update {
    uint32_t entry_index;  // Bar index when prediction was made
    uint8_t horizon;       // 1, 5, or 10
    double entry_price;    // For label computation
};

// Contiguous bucket storage for pending updates (O(1) access, inline capacity)
class PendingBuckets {
public:
    PendingBuckets() = default;

    // Pre-allocate buckets for [0, size)
    void init(std::size_t size) {
        buckets_.assign(size, InlinedVec<Update, 3>{});
    }

    // Add update to target bar's bucket (O(1), inline, no heap)
    inline void add(uint32_t target_index, const Update& u) noexcept {
        buckets_[target_index].push_back(u);
    }

    // Get bucket for target bar
    inline InlinedVec<Update, 3>& at(uint32_t idx) noexcept {
        return buckets_[idx];
    }

    inline std::size_t size() const noexcept { return buckets_.size(); }

private:
    std::vector<InlinedVec<Update, 3>> buckets_;
};

} // namespace sentio
