// include/testing/synthetic_market_generator.h
#pragma once

#include "common/types.h"
#include <vector>
#include <cmath>
#include <string>
#include <fstream>

namespace sentio {
namespace testing {

/**
 * @brief Generates deterministic synthetic market data for testing
 * 
 * Creates mathematically predictable patterns with known outcomes
 * for comprehensive validation of trading system calculations.
 */
class SyntheticMarketGenerator {
public:
    enum Pattern {
        SINE_WAVE,           // Predictable oscillation
        STEP_FUNCTION,       // Clear directional moves
        SAWTOOTH,            // Linear trends with resets
        DETERMINISTIC_CHAOS  // Complex but deterministic
    };

    struct Config {
        Pattern pattern = SINE_WAVE;
        double base_price = 100.0;
        double amplitude = 5.0;      // Price swing magnitude
        int period = 20;             // Bars per cycle
        double trend = 0.0;          // Drift per bar
        uint32_t seed = 12345;       // For reproducibility
        bool add_volume = true;
        int num_bars = 1000;
        std::string symbol = "TEST";
        int64_t start_timestamp_ms = 1609459200000;  // Jan 1, 2021
        int bar_interval_sec = 300;  // 5-min bars
    };

    /**
     * @brief Generate deterministic market data with predictable patterns
     */
    static std::vector<Bar> generate(const Config& config) {
        std::vector<Bar> data;
        data.reserve(config.num_bars);
        
        for (int i = 0; i < config.num_bars; ++i) {
            Bar bar;
            bar.timestamp_ms = config.start_timestamp_ms + (static_cast<int64_t>(i) * config.bar_interval_sec * 1000);
            bar.symbol = config.symbol;
            
            // Calculate close price based on pattern
            double close_price = calculate_pattern_value(config, i);
            
            // OHLC with small deterministic variations
            bar.open = (i > 0) ? data[i-1].close : config.base_price;
            bar.high = close_price + std::abs(std::sin(i * 0.7)) * 0.5;
            bar.low = close_price - std::abs(std::cos(i * 0.9)) * 0.5;
            bar.close = close_price;
            
            // Deterministic volume based on price movement
            if (config.add_volume) {
                double price_change = std::abs(bar.close - bar.open);
                bar.volume = 1000000.0 * (1.0 + price_change / config.base_price);
            } else {
                bar.volume = 1000000.0;
            }
            
            data.push_back(bar);
        }
        
        return data;
    }
    
    /**
     * @brief Write generated market data to CSV file
     */
    static void write_to_csv(const std::vector<Bar>& data, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        
        file << "timestamp_ms,symbol,open,high,low,close,volume\n";
        for (const auto& bar : data) {
            file << bar.timestamp_ms << ","
                 << bar.symbol << ","
                 << bar.open << ","
                 << bar.high << ","
                 << bar.low << ","
                 << bar.close << ","
                 << bar.volume << "\n";
        }
        file.close();
    }

private:
    static double calculate_pattern_value(const Config& config, int index) {
        double value = config.base_price;
        
        switch (config.pattern) {
            case SINE_WAVE:
                // Smooth oscillation
                value += config.amplitude * std::sin(2.0 * M_PI * index / config.period);
                value += config.trend * index;
                break;
                
            case STEP_FUNCTION:
                // Price moves up for period/2, then down for period/2
                value += ((index % config.period) < (config.period / 2)) ? 
                         config.amplitude : -config.amplitude;
                break;
                
            case SAWTOOTH:
                // Linear rise then sharp drop
                value += config.amplitude * (2.0 * (index % config.period) / config.period - 1.0);
                break;
                
            case DETERMINISTIC_CHAOS: {
                // Logistic map - deterministic but chaotic
                double x = 0.5 + (config.seed % 1000) / 10000.0;  // Slight variation per seed
                for (int j = 0; j <= index; ++j) {
                    x = 3.9 * x * (1.0 - x);  // Chaotic regime
                }
                value += config.amplitude * (x - 0.5) * 2.0;
                break;
            }
        }
        
        return value;
    }
};

} // namespace testing
} // namespace sentio

