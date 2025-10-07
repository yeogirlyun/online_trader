#include "sentio/mars_data_loader.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace sentio {

std::vector<MarsDataLoader::MarsBar> MarsDataLoader::load_from_json(const std::string& filename) {
    std::vector<MarsBar> bars;
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return bars;
        }
        
        nlohmann::json json_data;
        file >> json_data;
        
        for (const auto& item : json_data) {
            if (item.is_object() && !item.empty()) {
                MarsBar bar;
                bar.timestamp = item["timestamp"];
                bar.open = item["open"];
                bar.high = item["high"];
                bar.low = item["low"];
                bar.close = item["close"];
                bar.volume = item["volume"];
                bar.symbol = item["symbol"];
                bars.push_back(bar);
            }
        }
        
        std::cout << "✅ Loaded " << bars.size() << " bars from MarS data" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading MarS data: " << e.what() << std::endl;
    }
    
    return bars;
}

Bar MarsDataLoader::convert_to_bar(const MarsBar& mars_bar) {
    Bar bar;
    bar.ts_utc_epoch = mars_bar.timestamp;
    bar.open = mars_bar.open;
    bar.high = mars_bar.high;
    bar.low = mars_bar.low;
    bar.close = mars_bar.close;
    bar.volume = mars_bar.volume;
    return bar;
}

std::vector<Bar> MarsDataLoader::convert_to_bars(const std::vector<MarsBar>& mars_bars) {
    std::vector<Bar> bars;
    bars.reserve(mars_bars.size());
    
    for (const auto& mars_bar : mars_bars) {
        bars.push_back(convert_to_bar(mars_bar));
    }
    
    return bars;
}

bool MarsDataLoader::generate_mars_data(const std::string& symbol,
                                       int duration_minutes,
                                       int bar_interval_seconds,
                                       int num_simulations,
                                       const std::string& market_regime,
                                       const std::string& output_file) {
    
    // Construct Python command
    std::stringstream cmd;
    cmd << "python3 tools/mars_bridge.py"
        << " --symbol " << symbol
        << " --duration " << duration_minutes
        << " --interval " << bar_interval_seconds
        << " --simulations " << num_simulations
        << " --regime " << market_regime
        << " --output " << output_file
        << " --quiet";
    
    // Suppress verbose command output
    
    return execute_python_command(cmd.str());
}

bool MarsDataLoader::generate_fast_historical_data(const std::string& symbol,
                                                  const std::string& historical_data_file,
                                                  int continuation_minutes,
                                                  const std::string& output_file) {
    
    // Construct Python command for fast historical bridge
    std::stringstream cmd;
    cmd << "python3 tools/fast_historical_bridge.py"
        << " --symbol " << symbol
        << " --historical-data " << historical_data_file
        << " --continuation-minutes " << continuation_minutes
        << " --output " << output_file
        << " --quiet";
    
    // Suppress verbose command output
    
    return execute_python_command(cmd.str());
}

std::vector<Bar> MarsDataLoader::load_mars_data(const std::string& symbol,
                                               int duration_minutes,
                                               int bar_interval_seconds,
                                               int num_simulations,
                                               const std::string& market_regime) {
    
    // Generate temporary filename
    std::string temp_file = "temp_mars_data_" + symbol + ".json";
    
    // Generate MarS data
    if (!generate_mars_data(symbol, duration_minutes, bar_interval_seconds, 
                           num_simulations, market_regime, temp_file)) {
        std::cerr << "Failed to generate MarS data" << std::endl;
        return {};
    }
    
    // Load and convert data
    auto mars_bars = load_from_json(temp_file);
    auto bars = convert_to_bars(mars_bars);
    
    // Clean up temporary file
    std::filesystem::remove(temp_file);
    
    return bars;
}

std::vector<Bar> MarsDataLoader::load_fast_historical_data(const std::string& symbol,
                                                          const std::string& historical_data_file,
                                                          int continuation_minutes) {
    
    // Generate temporary filename
    std::string temp_file = "temp_fast_historical_" + symbol + ".json";
    
    // Generate fast historical data
    if (!generate_fast_historical_data(symbol, historical_data_file, 
                                      continuation_minutes, temp_file)) {
        std::cerr << "Failed to generate fast historical data" << std::endl;
        return {};
    }
    
    // Load and convert data
    auto mars_bars = load_from_json(temp_file);
    auto bars = convert_to_bars(mars_bars);
    
    // Clean up temporary file
    std::filesystem::remove(temp_file);
    
    return bars;
}

bool MarsDataLoader::execute_python_command(const std::string& command) {
    int result = std::system(command.c_str());
    return result == 0;
}

} // namespace sentio
