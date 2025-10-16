#include "cli/backtest_command.h"
#include "cli/command_registry.h"
#include "common/binary_data.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <memory>
#include <map>
#include <regex>

namespace sentio {
namespace cli {

// Helper: Parse simple JSON object {"key": value, ...}
static std::map<std::string, std::string> parse_simple_json(const std::string& json) {
    std::map<std::string, std::string> result;
    std::regex pair_regex(R"#("([^"]+)"\s*:\s*([^,}]+))#");
    auto begin = std::sregex_iterator(json.begin(), json.end(), pair_regex);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::smatch match = *i;
        std::string key = match[1].str();
        std::string value = match[2].str();

        // Trim whitespace and quotes from value
        value.erase(0, value.find_first_not_of(" \t\n\r\""));
        value.erase(value.find_last_not_of(" \t\n\r\"") + 1);

        result[key] = value;
    }
    return result;
}

void BacktestCommand::show_help() const {
    std::cout << "Backtest Command - Run end-to-end strategy backtest\n";
    std::cout << "======================================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  sentio_cli backtest --blocks <N> [OPTIONS]\n\n";
    std::cout << "Required Arguments:\n";
    std::cout << "  --blocks <N>           Number of blocks to test (1 block = 480 bars = 1 trading day)\n\n";
    std::cout << "Optional Arguments:\n";
    std::cout << "  --data <path>          Data file path (default: " << DEFAULT_DATA_PATH << ")\n";
    std::cout << "                         Supports both CSV and BIN formats (auto-detected)\n";
    std::cout << "  --warmup-blocks <N>    Warmup blocks before test period (default: 10)\n";
    std::cout << "                         These blocks are used for learning but not counted in results\n";
    std::cout << "  --warmup <N>           Additional warmup bars within first warmup block (default: 100)\n";
    std::cout << "  --skip-blocks <N>      Skip last N blocks from dataset (for walk-forward, default: 0)\n";
    std::cout << "                         Useful for creating non-overlapping test windows in optimization\n";
    std::cout << "  --output-dir <dir>     Output directory for results (default: data/tmp)\n";
    std::cout << "  --verbose, -v          Verbose output\n\n";
    std::cout << "Warmup Behavior:\n";
    std::cout << "  The strategy uses TWO warmup phases:\n";
    std::cout << "  1. Block warmup: --warmup-blocks N loads N extra blocks before test period\n";
    std::cout << "     - Strategy learns from these blocks (continuous online learning)\n";
    std::cout << "     - Trades executed during warmup blocks are NOT counted in results\n";
    std::cout << "  2. Bar warmup: --warmup N skips first N bars of the warmup period\n";
    std::cout << "     - Allows feature calculation to stabilize before starting learning\n\n";
    std::cout << "Examples:\n";
    std::cout << "  # Test 20 blocks with 10-block warmup (default)\n";
    std::cout << "  sentio_cli backtest --blocks 20\n\n";
    std::cout << "  # Test 20 blocks with 5-block warmup + 200 bar warmup\n";
    std::cout << "  sentio_cli backtest --blocks 20 --warmup-blocks 5 --warmup 200\n\n";
    std::cout << "  # Test 100 blocks with 20-block warmup (for extensive learning)\n";
    std::cout << "  sentio_cli backtest --blocks 100 --warmup-blocks 20\n\n";
    std::cout << "Output:\n";
    std::cout << "  - Signals JSONL file\n";
    std::cout << "  - Trades JSONL file\n";
    std::cout << "  - Performance analysis report\n";
    std::cout << "  - MRB (Mean Return per Block) calculation\n";
}

int BacktestCommand::execute(const std::vector<std::string>& args) {
    // Parse arguments
    std::string blocks_str = get_arg(args, "--blocks", "");
    if (blocks_str.empty()) {
        std::cerr << "❌ Error: --blocks is required\n\n";
        show_help();
        return 1;
    }

    int num_blocks = std::stoi(blocks_str);
    if (num_blocks <= 0) {
        std::cerr << "❌ Error: --blocks must be positive (got " << num_blocks << ")\n";
        return 1;
    }

    std::string data_path = get_arg(args, "--data", DEFAULT_DATA_PATH);
    int warmup_blocks = std::stoi(get_arg(args, "--warmup-blocks", "10"));
    int warmup_bars = std::stoi(get_arg(args, "--warmup", "100"));
    int skip_blocks = std::stoi(get_arg(args, "--skip-blocks", "0"));
    std::string params_json = get_arg(args, "--params", "");
    std::string output_dir = get_arg(args, "--output-dir", "data/tmp");
    bool verbose = has_flag(args, "--verbose") || has_flag(args, "-v");

    // Parse parameter overrides from JSON
    std::map<std::string, std::string> param_overrides;
    if (!params_json.empty()) {
        param_overrides = parse_simple_json(params_json);
        std::cout << "📝 Parameter overrides:\n";
        for (const auto& [key, value] : param_overrides) {
            std::cout << "   " << key << ": " << value << "\n";
        }
        std::cout << "\n";
    }

    if (warmup_blocks < 0) {
        std::cerr << "❌ Error: --warmup-blocks must be non-negative (got " << warmup_blocks << ")\n";
        return 1;
    }

    if (skip_blocks < 0) {
        std::cerr << "❌ Error: --skip-blocks must be non-negative (got " << skip_blocks << ")\n";
        return 1;
    }

    // Create output directory
    std::filesystem::create_directories(output_dir);

    // Output file paths
    std::string signals_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_signals.jsonl";
    std::string trades_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_trades.jsonl";
    std::string analysis_file = output_dir + "/backtest_" + std::to_string(num_blocks) + "blocks_analysis.txt";

    // Print header
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  🎯 BACKTEST - " << num_blocks << " Blocks + " << warmup_blocks << " Warmup Blocks\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "Data:          " << data_path << "\n";
    std::cout << "Test Blocks:   " << num_blocks << " (=" << (num_blocks * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Warmup Blocks: " << warmup_blocks << " (=" << (warmup_blocks * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Warmup Bars:   " << warmup_bars << " bars (initial feature stabilization)\n";
    std::cout << "Total Data:    " << (num_blocks + warmup_blocks) << " blocks (="
              << ((num_blocks + warmup_blocks) * BARS_PER_BLOCK) << " bars)\n";
    std::cout << "Output:        " << output_dir << "/\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";

    // Step 1: Load data (test blocks + warmup blocks)
    std::cout << "📊 Step 1: Loading data...\n";
    std::vector<Bar> bars;
    int total_blocks = num_blocks + warmup_blocks;

    // Auto-detect format (CSV vs BIN)
    std::filesystem::path p(data_path);
    bool is_binary = (p.extension() == ".bin");

    if (is_binary) {
        // Load from binary file
        binary_data::BinaryDataReader reader(data_path);
        if (!reader.open()) {
            std::cerr << "❌ Failed to open binary data file: " << data_path << "\n";
            return 1;
        }

        uint64_t total_bars = reader.get_bar_count();
        uint64_t bars_to_skip = skip_blocks * BARS_PER_BLOCK;
        uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;

        // Check if we have enough data after skipping
        if (total_bars < bars_to_skip + bars_needed) {
            std::cerr << "❌ Error: Insufficient data for skip_blocks=" << skip_blocks << "\n";
            std::cerr << "   Available: " << total_bars << " bars\n";
            std::cerr << "   Needed: " << (bars_to_skip + bars_needed) << " bars "
                      << "(skip " << bars_to_skip << " + test " << bars_needed << ")\n";
            return 1;
        }

        // Read bars before the skip window: [total - skip - needed, total - skip)
        uint64_t start_pos = total_bars - bars_to_skip - bars_needed;
        bars = reader.read_last_n_bars(bars_needed + bars_to_skip);

        // Remove the skipped bars from the end
        if (bars_to_skip > 0 && bars.size() > bars_needed) {
            bars.erase(bars.end() - bars_to_skip, bars.end());
        }

        if (verbose) {
            std::cout << "   Symbol: " << reader.get_symbol() << "\n";
            std::cout << "   Total available: " << total_bars << " bars\n";
        }
        std::cout << "   Loaded: " << bars.size() << " bars ("
                  << std::fixed << std::setprecision(2)
                  << (bars.size() / static_cast<double>(BARS_PER_BLOCK)) << " blocks)\n";
    } else {
        // Load from CSV file
        bars = utils::read_csv_data(data_path);
        if (bars.empty()) {
            std::cerr << "❌ Failed to load CSV data from: " << data_path << "\n";
            return 1;
        }

        // Extract last N blocks (warmup + test), accounting for skip
        uint64_t bars_to_skip = skip_blocks * BARS_PER_BLOCK;
        uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;

        if (bars.size() < bars_to_skip + bars_needed) {
            std::cerr << "❌ Error: Insufficient CSV data for skip_blocks=" << skip_blocks << "\n";
            std::cerr << "   Available: " << bars.size() << " bars\n";
            std::cerr << "   Needed: " << (bars_to_skip + bars_needed) << " bars\n";
            return 1;
        }

        // Extract the window: [end - skip - needed, end - skip)
        if (bars.size() > bars_to_skip + bars_needed) {
            bars.erase(bars.begin(), bars.end() - bars_to_skip - bars_needed);
        }
        if (bars_to_skip > 0 && bars.size() > bars_needed) {
            bars.erase(bars.end() - bars_to_skip, bars.end());
        }

        std::cout << "   Loaded: " << bars.size() << " bars ("
                  << std::fixed << std::setprecision(2)
                  << (bars.size() / static_cast<double>(BARS_PER_BLOCK)) << " blocks)\n";
    }

    if (bars.empty()) {
        std::cerr << "❌ No data loaded\n";
        return 1;
    }

    // Prepare data for workflow commands
    // Extract symbol from binary file (or use SPY as default for CSV)
    std::string symbol = "SPY";
    if (is_binary) {
        binary_data::BinaryDataReader reader_for_symbol(data_path);
        if (reader_for_symbol.open()) {
            symbol = reader_for_symbol.get_symbol();
        }
    }

    // Determine source directory and required instruments
    // Derive data source directory from the provided data file path
    std::filesystem::path data_file_path(data_path);
    std::string data_source_dir = data_file_path.parent_path().string();
    std::vector<std::string> required_symbols;
    if (symbol == "SPY") {
        required_symbols = {"SPY", "SPXL", "SH", "SDS"};
    } else if (symbol == "QQQ") {
        required_symbols = {"QQQ", "TQQQ", "PSQ", "SQQQ"};
    } else {
        std::cerr << "❌ Unsupported symbol: " << symbol << "\n";
        std::cerr << "   Only SPY and QQQ are supported for backtesting\n";
        return 1;
    }

    // Write truncated CSV files for all instruments
    // Execute-trades needs ALL 4 instruments with matching timestamps
    std::cout << "Preparing " << required_symbols.size() << " instrument CSV files...\n";
    uint64_t bars_needed = total_blocks * BARS_PER_BLOCK;

    for (const auto& sym : required_symbols) {
        std::string source_file = data_source_dir + "/" + sym + "_RTH_NH.csv";
        std::string target_file = output_dir + "/" + sym + "_RTH_NH.csv";

        // Load and truncate data for this instrument
        auto instrument_bars = utils::read_csv_data(source_file);
        if (instrument_bars.empty()) {
            std::cerr << "❌ Failed to load " << sym << " data from " << source_file << "\n";
            return 1;
        }

        // Extract last N blocks (warmup + test)
        if (instrument_bars.size() > bars_needed) {
            instrument_bars.erase(instrument_bars.begin(),
                                 instrument_bars.end() - bars_needed);
        }

        // Write truncated CSV
        std::ofstream csv_out(target_file);
        if (!csv_out.is_open()) {
            std::cerr << "❌ Failed to create file: " << target_file << "\n";
            return 1;
        }

        csv_out << "ts_utc,ts_nyt_epoch,open,high,low,close,volume\n";
        for (const auto& bar : instrument_bars) {
            csv_out << utils::ms_to_timestamp(bar.timestamp_ms) << ","
                    << (bar.timestamp_ms / 1000) << ","
                    << std::fixed << std::setprecision(4)
                    << bar.open << "," << bar.high << "," << bar.low << "," << bar.close << ","
                    << bar.volume << "\n";
        }
        csv_out.close();

        std::cout << "  " << sym << ": " << instrument_bars.size() << " bars written\n";
    }

    std::string temp_data_file = output_dir + "/" + symbol + "_RTH_NH.csv";
    std::cout << "✅ Data prepared: " << total_blocks << " blocks (" << bars_needed << " bars) for 4 instruments\n\n";

    // Step 2: Generate signals (delegate to generate-signals command)
    std::cout << "🔧 Step 2: Generating signals...\n";
    auto& registry = CommandRegistry::instance();
    auto generate_cmd = registry.get_command("generate-signals");
    if (!generate_cmd) {
        std::cerr << "❌ Failed to get generate-signals command\n";
        return 1;
    }

    std::vector<std::string> gen_args = {
        "--data", temp_data_file,
        "--output", signals_file,
        "--warmup", std::to_string(warmup_bars)
    };

    // Add parameter overrides
    if (param_overrides.count("buy_threshold")) {
        gen_args.push_back("--buy-threshold");
        gen_args.push_back(param_overrides["buy_threshold"]);
    }
    if (param_overrides.count("sell_threshold")) {
        gen_args.push_back("--sell-threshold");
        gen_args.push_back(param_overrides["sell_threshold"]);
    }
    if (param_overrides.count("ewrls_lambda")) {
        gen_args.push_back("--lambda");
        gen_args.push_back(param_overrides["ewrls_lambda"]);
    }
    if (param_overrides.count("bb_amplification_factor")) {
        gen_args.push_back("--bb-amp");
        gen_args.push_back(param_overrides["bb_amplification_factor"]);
    }

    // Phase 2 parameters
    if (param_overrides.count("h1_weight")) {
        gen_args.push_back("--h1-weight");
        gen_args.push_back(param_overrides["h1_weight"]);
    }
    if (param_overrides.count("h5_weight")) {
        gen_args.push_back("--h5-weight");
        gen_args.push_back(param_overrides["h5_weight"]);
    }
    if (param_overrides.count("h10_weight")) {
        gen_args.push_back("--h10-weight");
        gen_args.push_back(param_overrides["h10_weight"]);
    }
    if (param_overrides.count("bb_period")) {
        gen_args.push_back("--bb-period");
        gen_args.push_back(param_overrides["bb_period"]);
    }
    if (param_overrides.count("bb_std_dev")) {
        gen_args.push_back("--bb-std-dev");
        gen_args.push_back(param_overrides["bb_std_dev"]);
    }
    if (param_overrides.count("bb_proximity")) {
        gen_args.push_back("--bb-proximity");
        gen_args.push_back(param_overrides["bb_proximity"]);
    }
    if (param_overrides.count("regularization")) {
        gen_args.push_back("--regularization");
        gen_args.push_back(param_overrides["regularization"]);
    }

    if (verbose) gen_args.push_back("--verbose");

    int ret = generate_cmd->execute(gen_args);
    if (ret != 0) {
        std::cerr << "❌ Signal generation failed\n";
        return ret;
    }
    std::cout << "✅ Signals generated\n\n";

    // Step 3: Execute trades (delegate to execute-trades command)
    std::cout << "💼 Step 3: Executing trades...\n";
    auto execute_cmd = registry.get_command("execute-trades");
    if (!execute_cmd) {
        std::cerr << "❌ Failed to get execute-trades command\n";
        return 1;
    }

    // Calculate total warmup: warmup blocks + warmup bars
    // This tells execute-trades to skip the warmup period when calculating results
    int total_warmup_bars = (warmup_blocks * BARS_PER_BLOCK) + warmup_bars;

    std::vector<std::string> exec_args = {
        "--signals", signals_file,
        "--data", temp_data_file,
        "--output", trades_file,
        "--warmup", std::to_string(total_warmup_bars)
    };
    if (verbose) exec_args.push_back("--verbose");

    ret = execute_cmd->execute(exec_args);
    if (ret != 0) {
        std::cerr << "❌ Trade execution failed\n";
        return ret;
    }
    std::cout << "✅ Trades executed\n\n";

    // Step 4: Analyze performance (delegate to analyze-trades command)
    std::cout << "📈 Step 4: Analyzing performance...\n";
    auto analyze_cmd = registry.get_command("analyze-trades");
    if (!analyze_cmd) {
        std::cerr << "❌ Failed to get analyze-trades command\n";
        return 1;
    }

    std::vector<std::string> analyze_args = {
        "--trades", trades_file,
        "--data", temp_data_file,
        "--output", analysis_file,
        "--blocks", std::to_string(total_blocks),
        "--json"  // Output JSON metrics to stdout for Optuna parsing
    };

    ret = analyze_cmd->execute(analyze_args);
    if (ret != 0) {
        std::cerr << "❌ Performance analysis failed\n";
        return ret;
    }
    std::cout << "✅ Analysis complete\n\n";

    // Clean up temp data files
    for (const auto& sym : required_symbols) {
        std::string temp_file = output_dir + "/" + sym + "_RTH_NH.csv";
        std::filesystem::remove(temp_file);
    }

    // Final summary
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  ✅ BACKTEST COMPLETE - " << num_blocks << " Blocks\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "📁 Results:\n";
    std::cout << "   Signals:  " << signals_file << "\n";
    std::cout << "   Trades:   " << trades_file << "\n";
    std::cout << "   Analysis: " << analysis_file << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";

    return 0;
}

} // namespace cli
} // namespace sentio
