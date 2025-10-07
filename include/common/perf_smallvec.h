#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>

namespace sentio {

// Inline vector with fixed capacity, no heap allocations
template<typename T, std::size_t N>
struct InlinedVec {
    static_assert(N > 0, "N must be > 0");
    using size_type = std::uint8_t;

    T data_[N];
    size_type sz_{0};

    inline void clear() noexcept { sz_ = 0; }
    inline size_type size() const noexcept { return sz_; }
    inline bool empty() const noexcept { return sz_ == 0; }
    inline size_type capacity() const noexcept { return N; }

    inline void push_back(const T& v) noexcept { data_[sz_++] = v; }
    inline void push_back(T&& v) noexcept { data_[sz_++] = std::move(v); }

    inline T& operator[](size_type i) noexcept { return data_[i]; }
    inline const T& operator[](size_type i) const noexcept { return data_[i]; }

    inline T* begin() noexcept { return data_; }
    inline T* end() noexcept { return data_ + sz_; }
    inline const T* begin() const noexcept { return data_; }
    inline const T* end() const noexcept { return data_ + sz_; }
};

} // namespace sentio
