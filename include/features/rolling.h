#pragma once

#include <deque>
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

namespace sentio {
namespace features {
namespace roll {

// =============================================================================
// Welford's Algorithm for One-Pass Variance Calculation
// Numerically stable, O(1) updates, supports sliding windows
// =============================================================================

struct Welford {
    double mean = 0.0;
    double m2 = 0.0;
    int64_t n = 0;

    void add(double x) {
        ++n;
        double delta = x - mean;
        mean += delta / n;
        m2 += delta * (x - mean);
    }

    // Remove sample from sliding window (use with stored outgoing values)
    static void remove_sample(Welford& s, double x) {
        if (s.n <= 1) {
            static int reset_count = 0;
            if (reset_count < 10) {
                std::cerr << "[Welford] CRITICAL: remove_sample called with n=" << s.n
                          << ", resetting stats! This will cause NaN variance!" << std::endl;
                reset_count++;
            }
            s = {};
            return;
        }
        double mean_prev = s.mean;
        s.n -= 1;
        s.mean = (s.n * mean_prev - x) / s.n;
        s.m2 -= (x - mean_prev) * (x - s.mean);
        // Numerical stability guard - clamp to 0 if negative (variance can't be negative)
        // NOTE: Incremental removal can accumulate large numerical errors
        if (s.m2 < 0.0) {
            s.m2 = 0.0;
        }
    }

    inline double var() const {
        return (n > 1) ? (m2 / (n - 1)) : std::numeric_limits<double>::quiet_NaN();
    }

    inline double stdev() const {
        double v = var();
        return std::isnan(v) ? v : std::sqrt(v);
    }

    inline void reset() {
        mean = 0.0;
        m2 = 0.0;
        n = 0;
    }
};

// =============================================================================
// Ring Buffer with O(1) Min/Max via Monotonic Deques
// Perfect for Donchian Channels, Williams %R, rolling highs/lows
// =============================================================================

template<typename T>
class Ring {
public:
    explicit Ring(size_t capacity = 1) : capacity_(capacity) {
        buf_.reserve(capacity);
    }

    void push(T value) {
        if (size() == capacity_) pop();
        buf_.push_back(value);

        // Maintain monotonic deques for O(1) min/max
        while (!dq_max_.empty() && dq_max_.back() < value) {
            dq_max_.pop_back();
        }
        while (!dq_min_.empty() && dq_min_.back() > value) {
            dq_min_.pop_back();
        }
        dq_max_.push_back(value);
        dq_min_.push_back(value);

        // Update Welford statistics
        stats_.add(static_cast<double>(value));
    }

    void pop() {
        if (buf_.empty()) return;
        T out = buf_.front();
        buf_.erase(buf_.begin());

        // Remove from monotonic deques if it's the front element
        if (!dq_max_.empty() && dq_max_.front() == out) {
            dq_max_.erase(dq_max_.begin());
        }
        if (!dq_min_.empty() && dq_min_.front() == out) {
            dq_min_.erase(dq_min_.begin());
        }

        // Update Welford statistics
        Welford::remove_sample(stats_, static_cast<double>(out));
    }

    size_t size() const { return buf_.size(); }
    size_t capacity() const { return capacity_; }
    bool full() const { return size() == capacity_; }
    bool empty() const { return buf_.empty(); }

    T min() const {
        return dq_min_.empty() ? buf_.front() : dq_min_.front();
    }

    T max() const {
        return dq_max_.empty() ? buf_.front() : dq_max_.front();
    }

    double mean() const { return stats_.mean; }
    double stdev() const { return stats_.stdev(); }
    double variance() const { return stats_.var(); }
    size_t welford_n() const { return stats_.n; }  // For debugging
    double welford_m2() const { return stats_.m2; }  // For debugging

    void reset() {
        buf_.clear();
        dq_min_.clear();
        dq_max_.clear();
        stats_.reset();
    }

private:
    size_t capacity_;
    std::vector<T> buf_;
    std::vector<T> dq_min_;
    std::vector<T> dq_max_;
    Welford stats_;
};

// =============================================================================
// Exponential Moving Average (EMA)
// O(1) updates, standard α = 2/(period+1) smoothing
// =============================================================================

struct EMA {
    double val = std::numeric_limits<double>::quiet_NaN();
    double alpha = 0.0;

    explicit EMA(int period = 14) {
        set_period(period);
    }

    void set_period(int p) {
        alpha = (p <= 1) ? 1.0 : (2.0 / (p + 1.0));
    }

    double update(double x) {
        if (std::isnan(val)) {
            val = x;
        } else {
            val = alpha * x + (1.0 - alpha) * val;
        }
        return val;
    }

    double get_value() const { return val; }
    bool is_ready() const { return !std::isnan(val); }

    void reset() {
        val = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Wilder's Smoothing (for ATR, RSI)
// First N values: SMA seed, then Wilder smoothing
// =============================================================================

struct Wilder {
    double val = std::numeric_limits<double>::quiet_NaN();
    int period = 14;
    int i = 0;

    explicit Wilder(int p = 14) : period(p) {}

    double update(double x) {
        if (i < period) {
            // SMA seed phase
            if (std::isnan(val)) val = 0.0;
            val += x;
            ++i;
            if (i == period) {
                val /= period;
            }
        } else {
            // Wilder smoothing: ((prev * (n-1)) + new) / n
            val = ((val * (period - 1)) + x) / period;
        }
        return val;
    }

    double get_value() const { return val; }
    bool is_ready() const { return i >= period; }

    void reset() {
        val = std::numeric_limits<double>::quiet_NaN();
        i = 0;
    }
};

} // namespace roll
} // namespace features
} // namespace sentio
