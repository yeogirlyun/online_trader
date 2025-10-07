#pragma once
#include <vector>
#include <cstddef>
#include <memory>

namespace sentio {

// Ring buffer for storing feature vectors (avoid repeated allocations)
// Features are stored once per bar and shared across all horizons
class FeatureRing {
public:
    FeatureRing(std::size_t dim, std::size_t capacity)
        : dim_(dim), capacity_(capacity), buffer_(capacity * dim) {}

    // Store features for a given bar index
    inline void put(int bar_index, const std::vector<double>& features) {
        const std::size_t row = static_cast<std::size_t>(bar_index) % capacity_;
        std::copy(features.begin(), features.end(), buffer_.begin() + row * dim_);
    }

    // Get shared_ptr to features for a given bar index
    inline std::shared_ptr<const std::vector<double>> get_shared(int bar_index) const {
        const std::size_t row = static_cast<std::size_t>(bar_index) % capacity_;
        auto features = std::make_shared<std::vector<double>>(dim_);
        std::copy(buffer_.begin() + row * dim_,
                  buffer_.begin() + (row + 1) * dim_,
                  features->begin());
        return features;
    }

    inline std::size_t dim() const noexcept { return dim_; }

private:
    std::size_t dim_;
    std::size_t capacity_;
    std::vector<double> buffer_;  // Contiguous storage
};

} // namespace sentio
