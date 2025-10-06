#include "cli/online_command.h"
#include "strategy/online_strategy_60sa.h"
#include "strategy/online_strategy_8detector.h"
#include "strategy/online_strategy_25intraday.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace sentio {
namespace cli {

std::string OnlineCommand::get_name() const {
    return "online";
}

std::string OnlineCommand::get_description() const {
    return "Run online learning strategy with multi-horizon ensemble (EWRLS)";
}

std::vector<std::string> OnlineCommand::get_usage() const {
    return {
        "online <strategy> <data_path> <output_path> [max_bars]",
        "",
        "Strategies:",
        "  60sa        - 60SA feature set (45 features)",
        "  8detector   - 8-detector feature set",
        "  25intraday  - 25-intraday feature set",
        "",
        "Examples:",
        "  online 60sa data/equities/QQQ_RTH_NH.csv data/signals/online_60sa.jsonl",
        "  online 8detector data/equities/QQQ_RTH_NH.csv data/signals/online_8det.jsonl 1000",
        "",
        "Features:",
        "  - Multi-horizon ensemble (1-bar, 5-bar, 10-bar predictions)",
        "  - Continuous learning with EWRLS (no training phase)",
        "  - Automatic state persistence",
        "  - No training/inference parity issues",
        "  - Adaptive horizon weighting based on performance"
    };
}

void OnlineCommand::show_help() const {
    std::cout << "\nUsage: " << get_name() << " - " << get_description() << "\n\n";
    for (const auto& line : get_usage()) {
        std::cout << line << "\n";
    }
    std::cout << std::endl;
}

void OnlineCommand::print_usage() const {
    show_help();
}

int OnlineCommand::execute(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Error: Insufficient arguments\n";
        print_usage();
        return 1;
    }
    
    std::string strategy_name = args[0];
    std::string data_path = args[1];
    std::string output_path = args[2];
    int max_bars = (args.size() > 3) ? std::stoi(args[3]) : 0;
    
    return run_online_strategy(strategy_name, data_path, output_path, max_bars);
}

int OnlineCommand::run_online_strategy(
    const std::string& strategy_name,
    const std::string& data_path,
    const std::string& output_path,
    int max_bars) {
    
    try {
        std::cout << "\n=== ONLINE LEARNING STRATEGY ===" << std::endl;
        std::cout << "Strategy: " << strategy_name << std::endl;
        std::cout << "Data: " << data_path << std::endl;
        std::cout << "Output: " << output_path << std::endl;
        
        // Create strategy based on name
        std::unique_ptr<IStrategy> strategy;
        
        if (strategy_name == "60sa") {
            strategy = std::make_unique<OnlineStrategy60SA>();
        } else if (strategy_name == "8detector") {
            strategy = std::make_unique<OnlineStrategy8Detector>();
        } else if (strategy_name == "25intraday") {
            strategy = std::make_unique<OnlineStrategy25Intraday>();
        } else {
            std::cerr << "Error: Unknown strategy '" << strategy_name << "'\n";
            std::cerr << "Available: 60sa, 8detector, 25intraday\n";
            return 1;
        }
        
        // Initialize strategy
        StrategyComponent::StrategyConfig config;
        config.metadata["market_data_path"] = data_path;
        
        if (!strategy->initialize(config)) {
            std::cerr << "Error: Failed to initialize strategy\n";
            return 1;
        }
        
        // Load market data
        std::cout << "\nLoading market data..." << std::endl;
        auto market_data = utils::read_csv_data(data_path);
        
        if (market_data.empty()) {
            std::cerr << "Error: No data loaded from " << data_path << "\n";
            return 1;
        }
        
        std::cout << "Loaded " << market_data.size() << " bars" << std::endl;
        
        // Limit bars if requested
        if (max_bars > 0 && market_data.size() > static_cast<size_t>(max_bars)) {
            market_data.resize(max_bars);
            std::cout << "Limited to " << max_bars << " bars" << std::endl;
        }
        
        // Process data - learning happens continuously
        std::cout << "\n=== PROCESSING (Continuous Learning) ===" << std::endl;
        std::cout << "Note: First 100 bars are warmup period (~50% accuracy)" << std::endl;
        std::cout << "      Convergence expected within 500-1000 bars" << std::endl;
        std::cout << "\nProcessing..." << std::endl;
        
        auto signals = strategy->process_data(market_data);
        
        // Count signal types
        int long_count = 0, short_count = 0, neutral_count = 0;
        double total_confidence = 0.0;
        int confident_signals = 0;
        
        for (const auto& sig : signals) {
            if (sig.signal_type == SignalType::LONG) long_count++;
            else if (sig.signal_type == SignalType::SHORT) short_count++;
            else neutral_count++;
            
            // Track confidence for non-neutral signals
            if (sig.signal_type != SignalType::NEUTRAL) {
                double conf = std::abs(sig.probability - 0.5) * 2.0;
                total_confidence += conf;
                confident_signals++;
            }
        }
        
        // Calculate average confidence
        double avg_confidence = (confident_signals > 0) ? 
            total_confidence / confident_signals : 0.0;
        
        std::cout << "\n=== SIGNAL GENERATION RESULTS ===" << std::endl;
        std::cout << "Total Signals: " << signals.size() << std::endl;
        std::cout << "LONG: " << long_count << " (" 
                  << (100.0 * long_count / signals.size()) << "%)" << std::endl;
        std::cout << "SHORT: " << short_count << " (" 
                  << (100.0 * short_count / signals.size()) << "%)" << std::endl;
        std::cout << "NEUTRAL: " << neutral_count << " (" 
                  << (100.0 * neutral_count / signals.size()) << "%)" << std::endl;
        std::cout << "Average Confidence: " << (avg_confidence * 100) << "%" << std::endl;
        
        // Save signals to file
        std::cout << "\nSaving signals to: " << output_path << std::endl;
        
        // Create output directory if needed
        std::filesystem::path out_path(output_path);
        if (out_path.has_parent_path()) {
            std::filesystem::create_directories(out_path.parent_path());
        }
        
        std::ofstream out_file(output_path);
        if (!out_file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << output_path << "\n";
            return 1;
        }
        
        for (const auto& signal : signals) {
            out_file << signal.to_json() << "\n";
        }
        out_file.close();
        
        std::cout << "Signals saved successfully!" << std::endl;
        
        // Print learning statistics
        std::cout << "\n=== LEARNING STATISTICS ===" << std::endl;
        std::cout << "Strategy: " << strategy->get_strategy_name() << std::endl;
        std::cout << "Version: " << strategy->get_strategy_version() << std::endl;
        std::cout << "Warmup Bars: " << strategy->get_warmup_bars() << std::endl;
        
        auto metadata = strategy->get_metadata();
        if (metadata.count("accuracy")) {
            std::cout << "Current Accuracy: " << metadata["accuracy"] << std::endl;
        }
        if (metadata.count("ensemble_horizons")) {
            std::cout << "Ensemble Horizons: " << metadata["ensemble_horizons"] << std::endl;
        }
        
        std::cout << "\n=== NEXT STEPS ===" << std::endl;
        std::cout << "1. Run backtest with ensemble PSM:" << std::endl;
        std::cout << "   (Not yet implemented - requires ensemble backend integration)" << std::endl;
        std::cout << "\n2. State saved for continuous learning in next run" << std::endl;
        std::cout << "   Location: " << metadata["feature_set"] << " state directory" << std::endl;
        
        std::cout << "\n✅ Online learning completed successfully!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace cli
} // namespace sentio
