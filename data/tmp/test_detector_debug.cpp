// Quick debug test for regime detector
#include "strategy/market_regime_detector.h"
#include "common/types.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace sentio;

std::vector<Bar> load_csv_bars(const std::string& filepath, int max_bars = 200) {
    std::vector<Bar> bars;
    std::ifstream file(filepath);
    std::string line;
    int count = 0;

    while (std::getline(file, line) && count < max_bars) {
        std::stringstream ss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(ss, field, ',')) fields.push_back(field);

        if (fields.size() >= 8) {
            Bar bar;
            bar.timestamp_ms = std::stoll(fields[0]);
            bar.open = std::stod(fields[3]);
            bar.high = std::stod(fields[4]);
            bar.low = std::stod(fields[5]);
            bar.close = std::stod(fields[6]);
            bar.volume = std::stod(fields[7]);
            bar.bar_id = count++;
            bars.push_back(bar);
        }
    }
    return bars;
}

int main() {
    std::cout << "Loading TRENDING_UP segment (bars 0-200)...\n\n";
    auto bars = load_csv_bars("../data/equities/SPY_regime_test.csv", 200);

    std::cout << "Loaded " << bars.size() << " bars\n";
    std::cout << "Price range: $" << bars.front().close << " → $" << bars.back().close << "\n\n";

    MarketRegimeDetector detector(100);
    auto indicators = detector.calculate_indicators(bars);

    std::cout << "INDICATOR VALUES:\n";
    std::cout << "  ADX:         " << indicators.adx << " (threshold: 25.0 for trending)\n";
    std::cout << "  ATR:         " << indicators.atr << "\n";
    std::cout << "  Slope:       " << indicators.slope << "\n";
    std::cout << "  Chopiness:   " << indicators.chopiness << "\n";
    std::cout << "  Volatility:  " << indicators.volatility << " (high > 1.2, low < 0.8)\n";
    std::cout << "\n";

    auto regime = detector.detect_regime(bars);
    std::cout << "Detected regime: " << MarketRegimeDetector::regime_to_string(regime) << "\n";
    std::cout << "Expected regime: TRENDING_UP\n";

    return 0;
}
