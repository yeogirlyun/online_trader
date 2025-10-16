#pragma once

#include "features/rolling.h"
#include <cmath>
#include <deque>
#include <limits>

namespace sentio {
namespace features {
namespace ind {

// =============================================================================
// MACD (Moving Average Convergence Divergence)
// Fast EMA (12), Slow EMA (26), Signal Line (9)
// =============================================================================

struct MACD {
    roll::EMA fast{12};
    roll::EMA slow{26};
    roll::EMA sig{9};
    double macd = std::numeric_limits<double>::quiet_NaN();
    double signal = std::numeric_limits<double>::quiet_NaN();
    double hist = std::numeric_limits<double>::quiet_NaN();

    void update(double close) {
        double fast_val = fast.update(close);
        double slow_val = slow.update(close);
        macd = fast_val - slow_val;
        signal = sig.update(macd);
        hist = macd - signal;
    }

    bool is_ready() const {
        return fast.is_ready() && slow.is_ready() && sig.is_ready();
    }

    void reset() {
        fast.reset();
        slow.reset();
        sig.reset();
        macd = signal = hist = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Stochastic Oscillator (%K, %D, Slow)
// Uses rolling highs/lows for efficient calculation
// =============================================================================

struct Stoch {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    roll::EMA d3{3};
    roll::EMA slow3{3};
    double k = std::numeric_limits<double>::quiet_NaN();
    double d = std::numeric_limits<double>::quiet_NaN();
    double slow = std::numeric_limits<double>::quiet_NaN();

    explicit Stoch(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            k = d = slow = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double denom = hi.max() - lo.min();
        k = (denom == 0) ? 50.0 : 100.0 * (close - lo.min()) / denom;
        d = d3.update(k);
        slow = slow3.update(d);
    }

    bool is_ready() const {
        return hi.full() && lo.full() && d3.is_ready() && slow3.is_ready();
    }

    void reset() {
        hi.reset();
        lo.reset();
        d3.reset();
        slow3.reset();
        k = d = slow = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Williams %R
// Measures overbought/oversold levels (-100 to 0)
// =============================================================================

struct WilliamsR {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double r = std::numeric_limits<double>::quiet_NaN();

    explicit WilliamsR(int lookback = 14) : hi(lookback), lo(lookback) {}

    void update(double high, double low, double close) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            r = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double range = hi.max() - lo.min();
        r = (range == 0) ? -50.0 : -100.0 * (hi.max() - close) / range;
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        r = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Bollinger Bands
// Mean ± k * StdDev with %B and bandwidth indicators
// =============================================================================

struct Boll {
    roll::Ring<double> win;
    int k = 2;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sd = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();
    double percent_b = std::numeric_limits<double>::quiet_NaN();
    double bandwidth = std::numeric_limits<double>::quiet_NaN();

    Boll(int period = 20, int k_ = 2) : win(period), k(k_) {}

    void update(double close) {
        win.push(close);

        if (!win.full()) {
            mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
            percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        mean = win.mean();
        sd = win.stdev();
        upper = mean + k * sd;
        lower = mean - k * sd;

        // %B: Position within bands (0 = lower, 1 = upper)
        double band_range = upper - lower;
        percent_b = (band_range == 0) ? 0.5 : (close - lower) / band_range;

        // Bandwidth: Normalized band width
        bandwidth = (mean == 0) ? 0.0 : (upper - lower) / mean;
    }

    bool is_ready() const {
        return win.full();
    }

    void reset() {
        win.reset();
        mean = sd = upper = lower = std::numeric_limits<double>::quiet_NaN();
        percent_b = bandwidth = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Donchian Channels
// Rolling high/low breakout levels
// =============================================================================

struct Donchian {
    roll::Ring<double> hi;
    roll::Ring<double> lo;
    double up = std::numeric_limits<double>::quiet_NaN();
    double dn = std::numeric_limits<double>::quiet_NaN();
    double mid = std::numeric_limits<double>::quiet_NaN();

    explicit Donchian(int lookback = 20) : hi(lookback), lo(lookback) {}

    void update(double high, double low) {
        hi.push(high);
        lo.push(low);

        if (!hi.full() || !lo.full()) {
            up = dn = mid = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        up = hi.max();
        dn = lo.min();
        mid = 0.5 * (up + dn);
    }

    bool is_ready() const {
        return hi.full() && lo.full();
    }

    void reset() {
        hi.reset();
        lo.reset();
        up = dn = mid = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// RSI (Relative Strength Index) - Wilder's Method
// Uses Wilder's smoothing for gains/losses
// =============================================================================

struct RSI {
    roll::Wilder avgGain;
    roll::Wilder avgLoss;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit RSI(int period = 14) : avgGain(period), avgLoss(period) {}

    void update(double close) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        double change = close - prevClose;
        prevClose = close;

        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? -change : 0.0;

        double g = avgGain.update(gain);
        double l = avgLoss.update(loss);

        if (!avgLoss.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double rs = (l == 0) ? INFINITY : g / l;
        value = 100.0 - 100.0 / (1.0 + rs);
    }

    bool is_ready() const {
        return avgGain.is_ready() && avgLoss.is_ready();
    }

    void reset() {
        avgGain.reset();
        avgLoss.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ATR (Average True Range) - Wilder's Method
// Volatility indicator using true range
// =============================================================================

struct ATR {
    roll::Wilder w;
    double prevClose = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ATR(int period = 14) : w(period) {}

    void update(double high, double low, double close) {
        double tr;
        if (std::isnan(prevClose)) {
            tr = high - low;
        } else {
            tr = std::max({
                high - low,
                std::fabs(high - prevClose),
                std::fabs(low - prevClose)
            });
        }
        prevClose = close;
        value = w.update(tr);

        if (!w.is_ready()) {
            value = std::numeric_limits<double>::quiet_NaN();
        }
    }

    bool is_ready() const {
        return w.is_ready();
    }

    void reset() {
        w.reset();
        prevClose = std::numeric_limits<double>::quiet_NaN();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// ROC (Rate of Change) %
// Momentum indicator: (close - close_n_periods_ago) / close_n_periods_ago * 100
// =============================================================================

struct ROC {
    std::deque<double> q;
    int period;
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit ROC(int p) : period(p) {}

    void update(double close) {
        q.push_back(close);
        if (static_cast<int>(q.size()) < period + 1) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }
        double past = q.front();
        q.pop_front();
        value = (past == 0) ? 0.0 : 100.0 * (close - past) / past;
    }

    bool is_ready() const {
        return static_cast<int>(q.size()) >= period;
    }

    void reset() {
        q.clear();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// CCI (Commodity Channel Index)
// Measures deviation from typical price mean
// =============================================================================

struct CCI {
    roll::Ring<double> tp; // Typical price ring
    double value = std::numeric_limits<double>::quiet_NaN();

    explicit CCI(int period = 20) : tp(period) {}

    void update(double high, double low, double close) {
        double typical_price = (high + low + close) / 3.0;
        tp.push(typical_price);

        if (!tp.full()) {
            value = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double mean = tp.mean();
        double sd = tp.stdev();

        if (sd == 0 || std::isnan(sd)) {
            value = 0.0;
            return;
        }

        // Approximate mean deviation using stdev (empirical factor ~1.25)
        // For exact MD, maintain parallel queue (omitted for O(1) performance)
        value = (typical_price - mean) / (0.015 * sd * 1.25331413732);
    }

    bool is_ready() const {
        return tp.full();
    }

    void reset() {
        tp.reset();
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// OBV (On-Balance Volume)
// Cumulative volume indicator based on price direction
// =============================================================================

struct OBV {
    double value = 0.0;
    double prevClose = std::numeric_limits<double>::quiet_NaN();

    void update(double close, double volume) {
        if (std::isnan(prevClose)) {
            prevClose = close;
            return;
        }

        if (close > prevClose) {
            value += volume;
        } else if (close < prevClose) {
            value -= volume;
        }
        // No change if close == prevClose

        prevClose = close;
    }

    void reset() {
        value = 0.0;
        prevClose = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// VWAP (Volume Weighted Average Price)
// Intraday indicator: cumulative (price * volume) / cumulative volume
// =============================================================================

struct VWAP {
    double sumPV = 0.0;
    double sumV = 0.0;
    double value = std::numeric_limits<double>::quiet_NaN();

    void update(double price, double volume) {
        sumPV += price * volume;
        sumV += volume;
        if (sumV > 0) {
            value = sumPV / sumV;
        }
    }

    void reset() {
        sumPV = 0.0;
        sumV = 0.0;
        value = std::numeric_limits<double>::quiet_NaN();
    }
};

// =============================================================================
// Keltner Channels
// EMA ± (ATR * multiplier)
// =============================================================================

struct Keltner {
    roll::EMA ema;
    ATR atr;
    double multiplier = 2.0;
    double middle = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    double lower = std::numeric_limits<double>::quiet_NaN();

    Keltner(int ema_period = 20, int atr_period = 10, double mult = 2.0)
        : ema(ema_period), atr(atr_period), multiplier(mult) {}

    void update(double high, double low, double close) {
        middle = ema.update(close);
        atr.update(high, low, close);

        if (!atr.is_ready()) {
            upper = lower = std::numeric_limits<double>::quiet_NaN();
            return;
        }

        double atr_val = atr.value;
        upper = middle + multiplier * atr_val;
        lower = middle - multiplier * atr_val;
    }

    bool is_ready() const {
        return ema.is_ready() && atr.is_ready();
    }

    void reset() {
        ema.reset();
        atr.reset();
        middle = upper = lower = std::numeric_limits<double>::quiet_NaN();
    }
};

} // namespace ind
} // namespace features
} // namespace sentio
