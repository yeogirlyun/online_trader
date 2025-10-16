/**
 * @file test_regime_detector.cpp
 * @brief Test MarketRegimeDetector on synthetic multi-regime data
 *
 * This program tests the MarketRegimeDetector on synthetic data with known regimes.
 * It validates that the detector correctly identifies different market conditions.
 */

#include "strategy/market_regime_detector.h"
#include "common/types.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <iomanip>
#include <vector>
#include <string>

using namespace sentio;

// Simple CSV loader for test data (timestamp_ms,date,time,open,high,low,close,volume)
std::vector<Bar> load_csv_bars(const std::string& filepath) {
    std::vector<Bar> bars;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    std::string line;
    int line_num = 0;

    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::vector<std::string> fields;
        std::string field;

        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() < 8) {
            std::cerr << "Warning: Line " << line_num << " has only " << fields.size() << " fields\n";
            continue;
        }

        try {
            Bar bar;
            bar.timestamp_ms = std::stoll(fields[0]);
            bar.open = std::stod(fields[3]);
            bar.high = std::stod(fields[4]);
            bar.low = std::stod(fields[5]);
            bar.close = std::stod(fields[6]);
            bar.volume = std::stod(fields[7]);
            bar.bar_id = line_num;

            bars.push_back(bar);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line " << line_num << ": " << e.what() << "\n";
            continue;
        }
    }

    return bars;
}

struct RegimeSegment {
    int start_idx;
    int end_idx;
    MarketRegime detected_regime;
    int bars_count;
};

void print_regime_summary(const std::vector<RegimeSegment>& segments) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "REGIME DETECTION SUMMARY\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::map<MarketRegime, int> regime_counts;
    std::map<MarketRegime, int> total_bars;

    for (const auto& seg : segments) {
        regime_counts[seg.detected_regime]++;
        total_bars[seg.detected_regime] += seg.bars_count;
    }

    std::cout << "Regime distribution:\n";
    for (const auto& [regime, count] : regime_counts) {
        std::string regime_str = MarketRegimeDetector::regime_to_string(regime);
        int bars = total_bars[regime];
        double pct = (bars * 100.0) / 4800.0;  // Total 4800 bars
        std::cout << "  " << std::left << std::setw(20) << regime_str
                  << ": " << std::setw(4) << count << " segments, "
                  << std::setw(5) << bars << " bars ("
                  << std::fixed << std::setprecision(1) << pct << "%)\n";
    }
    std::cout << "\n";
}

void test_regime_detector_on_segment(const std::vector<Bar>& bars,
                                     int start, int end,
                                     const std::string& expected_regime,
                                     MarketRegimeDetector& detector) {
    // Get segment
    std::vector<Bar> segment(bars.begin() + start, bars.begin() + end);

    // Detect regime
    MarketRegime detected = detector.detect_regime(segment);
    std::string detected_str = MarketRegimeDetector::regime_to_string(detected);

    // Get features for analysis (new API)
    auto features = detector.last_features();
    auto vol_thresholds = detector.vol_thresholds();

    // Print results
    std::cout << "Bars [" << start << "-" << end << "] "
              << "(" << (end - start) << " bars)\n";
    std::cout << "  Expected:  " << expected_regime << "\n";
    std::cout << "  Detected:  " << detected_str;

    // Check if correct
    bool correct = false;
    if (expected_regime == "TRENDING_UP" && detected == MarketRegime::TRENDING_UP) correct = true;
    if (expected_regime == "TRENDING_DOWN" && detected == MarketRegime::TRENDING_DOWN) correct = true;
    if (expected_regime == "CHOPPY" && detected == MarketRegime::CHOPPY) correct = true;
    if (expected_regime == "HIGH_VOLATILITY" && detected == MarketRegime::HIGH_VOLATILITY) correct = true;
    if (expected_regime == "LOW_VOLATILITY" && detected == MarketRegime::LOW_VOLATILITY) correct = true;

    std::cout << (correct ? " ✓" : " ✗") << "\n";

    // Print features (new adaptive detector)
    std::cout << "  Features:\n";
    std::cout << "    Vol (std log-ret): " << std::fixed << std::setprecision(6) << features.vol << "\n";
    std::cout << "    Slope (log-price): " << features.slope << "\n";
    std::cout << "    R²:                " << std::setprecision(3) << features.r2 << "\n";
    std::cout << "    CHOP:              " << std::setprecision(1) << features.chop << "\n";
    std::cout << "    Vol thresholds:    [" << std::setprecision(6) << vol_thresholds.first
              << ", " << vol_thresholds.second << "]\n";
    std::cout << "\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file>\n";
        std::cerr << "Example: " << argv[0] << " data/equities/SPY_regime_test.csv\n";
        return 1;
    }

    std::string data_file = argv[1];

    std::cout << std::string(80, '=') << "\n";
    std::cout << "REGIME DETECTOR VALIDATION TEST\n";
    std::cout << std::string(80, '=') << "\n\n";
    std::cout << "Data file: " << data_file << "\n\n";

    // Load data
    std::vector<Bar> bars;

    try {
        bars = load_csv_bars(data_file);
    } catch (const std::exception& e) {
        std::cerr << "Error loading data: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Loaded " << bars.size() << " bars\n\n";

    // Create detector with adaptive thresholds
    MarketRegimeDetector detector;  // Uses default params (96/120/48 windows)

    // Expected regime segments (from generate_regime_test_data.py)
    // Each regime = 2 blocks = 960 bars
    const int BARS_PER_REGIME = 960;
    struct ExpectedRegime {
        std::string name;
        int start;
        int end;
    };

    std::vector<ExpectedRegime> expected_regimes = {
        {"TRENDING_UP",      0,    BARS_PER_REGIME},
        {"TRENDING_DOWN",    BARS_PER_REGIME,   2 * BARS_PER_REGIME},
        {"CHOPPY",           2 * BARS_PER_REGIME, 3 * BARS_PER_REGIME},
        {"HIGH_VOLATILITY",  3 * BARS_PER_REGIME, 4 * BARS_PER_REGIME},
        {"LOW_VOLATILITY",   4 * BARS_PER_REGIME, 5 * BARS_PER_REGIME}
    };

    std::cout << std::string(80, '=') << "\n";
    std::cout << "TESTING REGIME DETECTION\n";
    std::cout << std::string(80, '=') << "\n\n";

    int correct_detections = 0;
    int total_tests = 0;

    // Test each regime segment
    for (const auto& expected : expected_regimes) {
        if (expected.end > static_cast<int>(bars.size())) {
            std::cerr << "Warning: Not enough data for regime " << expected.name << "\n";
            continue;
        }

        std::cout << "Testing " << expected.name << " segment...\n";

        // Test at multiple points within the segment
        std::vector<int> test_points = {
            expected.start + 100,      // Near start
            (expected.start + expected.end) / 2,  // Middle
            expected.end - 100          // Near end
        };

        for (int test_point : test_points) {
            int start = std::max(0, test_point - 100);
            int end = std::min(static_cast<int>(bars.size()), test_point + 100);

            if (end - start < 100) continue;

            std::vector<Bar> segment(bars.begin() + start, bars.begin() + end);
            MarketRegime detected = detector.detect_regime(segment);
            std::string detected_str = MarketRegimeDetector::regime_to_string(detected);

            // Check correctness
            bool correct = false;
            if (expected.name == "TRENDING_UP" && detected == MarketRegime::TRENDING_UP) correct = true;
            if (expected.name == "TRENDING_DOWN" && detected == MarketRegime::TRENDING_DOWN) correct = true;
            if (expected.name == "CHOPPY" && detected == MarketRegime::CHOPPY) correct = true;
            if (expected.name == "HIGH_VOLATILITY" && detected == MarketRegime::HIGH_VOLATILITY) correct = true;
            if (expected.name == "LOW_VOLATILITY" && detected == MarketRegime::LOW_VOLATILITY) correct = true;

            std::cout << "  Bar " << test_point << ": " << detected_str;
            std::cout << (correct ? " ✓\n" : " ✗\n");

            if (correct) correct_detections++;
            total_tests++;
        }

        std::cout << "\n";
    }

    // Calculate accuracy
    double accuracy = (total_tests > 0) ? (100.0 * correct_detections / total_tests) : 0.0;

    std::cout << std::string(80, '=') << "\n";
    std::cout << "VALIDATION RESULTS\n";
    std::cout << std::string(80, '=') << "\n\n";
    std::cout << "Total tests:         " << total_tests << "\n";
    std::cout << "Correct detections:  " << correct_detections << "\n";
    std::cout << "Accuracy:            " << std::fixed << std::setprecision(1)
              << accuracy << "%\n\n";

    if (accuracy >= 80.0) {
        std::cout << "✅ VALIDATION PASSED (accuracy >= 80%)\n";
    } else if (accuracy >= 60.0) {
        std::cout << "⚠️  VALIDATION WARNING (60% <= accuracy < 80%)\n";
        std::cout << "   Detector works but may need tuning\n";
    } else {
        std::cout << "❌ VALIDATION FAILED (accuracy < 60%)\n";
        std::cout << "   Detector parameters need adjustment\n";
    }

    std::cout << "\n";

    // Continuous detection test
    std::cout << std::string(80, '=') << "\n";
    std::cout << "CONTINUOUS REGIME DETECTION\n";
    std::cout << std::string(80, '=') << "\n\n";
    std::cout << "Detecting regimes every 100 bars...\n\n";

    std::vector<RegimeSegment> segments;
    MarketRegime current_regime = MarketRegime::CHOPPY;
    int segment_start = 0;

    for (size_t i = 100; i < bars.size(); i += 100) {
        std::vector<Bar> window(bars.begin() + i - 100, bars.begin() + i);
        MarketRegime detected = detector.detect_regime(window);

        if (detected != current_regime || i == bars.size() - 1) {
            // Regime changed or end of data
            RegimeSegment seg;
            seg.start_idx = segment_start;
            seg.end_idx = i;
            seg.detected_regime = current_regime;
            seg.bars_count = i - segment_start;

            if (seg.bars_count > 0) {
                segments.push_back(seg);
            }

            if (detected != current_regime) {
                std::cout << "Bar " << std::setw(5) << i << ": "
                          << std::setw(20) << std::left
                          << MarketRegimeDetector::regime_to_string(current_regime)
                          << " → "
                          << MarketRegimeDetector::regime_to_string(detected) << "\n";
            }

            current_regime = detected;
            segment_start = i;
        }
    }

    print_regime_summary(segments);

    std::cout << std::string(80, '=') << "\n";
    std::cout << "TEST COMPLETE\n";
    std::cout << std::string(80, '=') << "\n\n";

    return (accuracy >= 60.0) ? 0 : 1;
}
