#pragma once
#include <vector>
#include <cstddef>
#include <cstring>

namespace sentio {

// Ring buffer for double-precision features (zero-copy, no heap in hot path)
class FeatureRingD {
public:
    FeatureRingD(std::size_t dim, std::size_t capacity)
        : D_(dim), C_(capacity), buf_(dim * capacity, 0.0) {}

    // Store features at bar index (O(1), no allocation)
    inline void put(int bar_index, const std::vector<double>& x) noexcept {
        double* __restrict dst = &buf_[static_cast<std::size_t>(bar_index % C_) * D_];
        const double* __restrict src = x.data();
        std::memcpy(dst, src, D_ * sizeof(double));
    }

    // Get pointer to features at bar index (zero-copy)
    inline const double* get(int bar_index) const noexcept {
        return &buf_[static_cast<std::size_t>(bar_index % C_) * D_];
    }

    inline std::size_t dim() const noexcept { return D_; }
    inline std::size_t capacity() const noexcept { return C_; }

private:
    const std::size_t D_;  // Feature dimension (126)
    const std::size_t C_;  // Ring capacity (max_horizon + 2)
    std::vector<double> buf_;  // Contiguous storage [C × D]
};

} // namespace sentio
