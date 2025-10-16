#include "cli/extract_features_command.h"
#include "common/utils.h"
#include "features/unified_feature_engine.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace sentio {
namespace cli {

int ExtractFeaturesCommand::execute(const std::vector<std::string>& args) {
    // Check for help flag
    if (std::find(args.begin(), args.end(), "--help") != args.end() ||
        std::find(args.begin(), args.end(), "-h") != args.end()) {
        show_help();
        return 0;
    }

    // Parse arguments
    std::string data_file;
    std::string output_file;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--data" && i + 1 < args.size()) {
            data_file = args[++i];
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            output_file = args[++i];
        }
    }

    // Validate required arguments
    if (data_file.empty()) {
        std::cerr << "Error: --data is required" << std::endl;
        show_help();
        return 1;
    }

    if (output_file.empty()) {
        std::cerr << "Error: --output is required" << std::endl;
        show_help();
        return 1;
    }

    try {
        std::cout << "[ExtractFeatures] Loading data from: " << data_file << std::endl;

        // Load OHLCV bars
        auto bars = utils::read_csv_data(data_file);
        if (bars.empty()) {
            std::cerr << "Error: Could not load data from " << data_file << std::endl;
            return 1;
        }
        std::cout << "[ExtractFeatures] Loaded " << bars.size() << " bars" << std::endl;

        // Initialize feature engine with default config
        features::UnifiedFeatureEngine engine;

        // Open output file
        std::ofstream out(output_file);
        if (!out.is_open()) {
            throw std::runtime_error("Failed to open output file: " + output_file);
        }

        // Write CSV header: timestamp + feature names
        out << "timestamp";
        for (const auto& name : engine.names()) {
            out << "," << name;
        }
        out << "\n";

        std::cout << "[ExtractFeatures] Extracting " << engine.names().size()
                  << " features..." << std::endl;

        // Extract features for each bar
        size_t count = 0;
        for (const auto& bar : bars) {
            engine.update(bar);

            // Write timestamp
            out << bar.timestamp_ms;

            // Write features (with high precision to preserve values)
            for (double feat : engine.features_view()) {
                out << "," << std::fixed << std::setprecision(10) << feat;
            }
            out << "\n";

            ++count;
            if (count % 1000 == 0) {
                std::cout << "[ExtractFeatures] Processed " << count << " bars..." << std::endl;
            }
        }

        out.close();

        std::cout << "[ExtractFeatures] Features saved to: " << output_file << std::endl;
        std::cout << "[ExtractFeatures] Total bars: " << count << std::endl;
        std::cout << "[ExtractFeatures] Total features: " << engine.names().size() << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

void ExtractFeaturesCommand::show_help() const {
    std::cout << "Usage: sentio_cli extract-features --data <file> --output <file>\n\n";
    std::cout << "Extract features from OHLCV data and save to CSV for Optuna caching.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --data <file>      Input CSV file with OHLCV data\n";
    std::cout << "  --output <file>    Output CSV file for features\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Example:\n";
    std::cout << "  sentio_cli extract-features \\\n";
    std::cout << "    --data data/equities/SPY_4blocks.csv \\\n";
    std::cout << "    --output /tmp/spy_features.csv\n\n";
    std::cout << "Output format:\n";
    std::cout << "  CSV with timestamp + 58 features (time, price, indicators, patterns)\n";
    std::cout << "  Can be reused across multiple Optuna trials for 4-5x speedup\n";
}

} // namespace cli
} // namespace sentio
