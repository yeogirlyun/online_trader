// =============================================================================
// Executable: generate_leverage_data
// Purpose: Generates theoretical data for TQQQ, SQQQ, PSQ from a base QQQ file.
//          Fulfills requirements from Section 3 of the Leverage Trading doc.
//
// Core Architecture & Design:
// This tool implements the corrected daily return compounding model that
// accurately captures the path-dependent nature of leveraged ETFs through
// proper daily rebalancing simulation rather than simple price multiplication.
//
// Key Features:
// - Daily return compounding: Models actual ETF rebalancing mechanism
// - Path dependency: Captures volatility decay and compounding effects
// - Realistic pricing: Generates data that matches actual ETF behavior
// - Accurate OHLC: Maintains proper intraday relationships
// - Volume scaling: Realistic volume adjustments for leverage instruments
//
// Usage:
//   generate_leverage_data --input data/equities/QQQ_RTH_NH.csv --output-dir data/equities/
// =============================================================================

#include "common/utils.h"
#include "common/types.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

/// Leverage specification for data generation
struct LeverageSpec {
    double factor;      /// Leverage multiplier (1.0 or 3.0)
    bool is_inverse;     /// Whether instrument is inverse
    std::string description; /// Human-readable description
};

/// Calculate leveraged bar using corrected daily return compounding model
/// 
/// This implementation uses the accurate daily return compounding approach
/// that correctly models the path-dependent nature of leveraged ETFs.
/// 
/// @param qqq_bar Current QQQ bar
/// @param prev_qqq_bar Previous QQQ bar
/// @param prev_lev_bar Previous leveraged bar
/// @param spec Leverage specification
/// @param daily_decay Daily decay rate
/// @return Generated leveraged bar
sentio::Bar calculate_leveraged_bar(const sentio::Bar& qqq_bar, 
                                   const sentio::Bar& prev_qqq_bar,
                                   const sentio::Bar& prev_lev_bar, 
                                   const LeverageSpec& spec,
                                   double daily_decay) {
    sentio::Bar lev_bar = qqq_bar; // Copy timestamps and symbol initially
    
    // 1. Calculate daily return of base asset (QQQ)
    double qqq_return = (qqq_bar.close / prev_qqq_bar.close) - 1.0;
    
    // 2. Apply leverage factor to daily return
    double leveraged_return = qqq_return * spec.factor;
    
    // 3. Apply inversion if necessary
    if (spec.is_inverse) {
        leveraged_return = -leveraged_return;
    }
    
    // 4. Apply daily costs (decay)
    double total_daily_cost = daily_decay;
    
    // 5. Calculate new leveraged price using compounding
    lev_bar.close = prev_lev_bar.close * (1.0 + leveraged_return - total_daily_cost);
    
    // Ensure positive price
    lev_bar.close = std::max(0.01, lev_bar.close);
    
    // 6. Generate OHLC based on intraday movements
    double open_move = (qqq_bar.open - prev_qqq_bar.close) / prev_qqq_bar.close;
    lev_bar.open = prev_lev_bar.close * (1.0 + open_move * spec.factor * (spec.is_inverse ? -1.0 : 1.0));
    
    // Calculate high and low based on intraday movements
    double high_move = (qqq_bar.high - qqq_bar.open) / qqq_bar.open;
    double low_move = (qqq_bar.low - qqq_bar.open) / qqq_bar.open;
    
    if (spec.is_inverse) {
        // For inverse ETFs, high becomes low and low becomes high
        lev_bar.high = lev_bar.open * (1.0 + low_move * spec.factor * -1.0);
        lev_bar.low = lev_bar.open * (1.0 + high_move * spec.factor * -1.0);
    } else {
        lev_bar.high = lev_bar.open * (1.0 + high_move * spec.factor);
        lev_bar.low = lev_bar.open * (1.0 + low_move * spec.factor);
    }
    
    // Ensure OHLC integrity
    double max_oc = std::max(lev_bar.open, lev_bar.close);
    double min_oc = std::min(lev_bar.open, lev_bar.close);
    lev_bar.high = std::max(lev_bar.high, max_oc);
    lev_bar.low = std::min(lev_bar.low, min_oc);
    
    // Ensure all prices are positive
    lev_bar.open = std::max(0.01, lev_bar.open);
    lev_bar.high = std::max(0.01, lev_bar.high);
    lev_bar.low = std::max(0.01, lev_bar.low);
    
    // Model volume as a fraction (typically lower for leverage instruments)
    double volume_scaling = (spec.factor == 3.0) ? 0.3 : 0.5;
    lev_bar.volume = static_cast<int>(qqq_bar.volume * volume_scaling);
    
    return lev_bar;
}

/// Write leverage data to CSV file
/// @param bars Vector of leverage bars
/// @param symbol Symbol name
/// @param output_path Output file path
/// @return true if successful
bool write_leverage_csv(const std::vector<sentio::Bar>& bars, 
                       const std::string& symbol, 
                       const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot open output file: " << output_path << std::endl;
        return false;
    }
    
    // Write CSV header
    out << "ts_utc,ts_nyt_epoch,open,high,low,close,volume\n";
    out << std::fixed << std::setprecision(2);
    
    // Write data rows
    for (const auto& bar : bars) {
        out << sentio::utils::ms_to_timestamp(bar.timestamp_ms) << ","
            << bar.timestamp_ms / 1000 << ","
            << bar.open << "," << bar.high << "," << bar.low << "," << bar.close << ","
            << static_cast<long>(bar.volume) << "\n";
    }
    
    return true;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    const std::string input_path = sentio::utils::get_arg(argc, argv, "--input", "data/equities/QQQ_RTH_NH.csv");
    const std::string output_dir = sentio::utils::get_arg(argc, argv, "--output-dir", "data/equities/");
    const double daily_decay = std::stod(sentio::utils::get_arg(argc, argv, "--decay", "0.0001"));
    const int max_rows = std::stoi(sentio::utils::get_arg(argc, argv, "--max-rows", "0"));
    
    std::cout << "=============================================================================" << std::endl;
    std::cout << "Sentio Leverage Data Generator - Corrected Daily Return Compounding Model" << std::endl;
    std::cout << "=============================================================================" << std::endl;
    std::cout << "Input file: " << input_path << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    std::cout << "Daily decay rate: " << daily_decay << std::endl;
    if (max_rows > 0) {
        std::cout << "Max rows (testing): " << max_rows << std::endl;
    }
    std::cout << std::endl;
    
    // Load base QQQ data
    std::cout << "📊 Loading base QQQ data from: " << input_path << std::endl;
    auto qqq_bars = sentio::utils::read_csv_data(input_path);
    if (qqq_bars.empty()) {
        std::cerr << "❌ ERROR: Failed to load QQQ data from " << input_path << std::endl;
        return 1;
    }
    
    // Limit rows if specified (for testing)
    if (max_rows > 0 && max_rows < static_cast<int>(qqq_bars.size())) {
        qqq_bars.resize(max_rows);
        std::cout << "⚠️  Limited to " << max_rows << " rows for testing" << std::endl;
    }
    
    std::cout << "✅ Loaded " << qqq_bars.size() << " QQQ bars" << std::endl;
    std::cout << std::endl;
    
    // Define leverage specifications
    std::map<std::string, LeverageSpec> specs = {
        {"TQQQ", {3.0, false, "3x Long QQQ"}},
        {"SQQQ", {3.0, true, "3x Short QQQ"}},
        {"PSQ",  {1.0, true, "1x Short QQQ"}}
    };
    
    // Generate leverage data for each symbol
    for (const auto& pair : specs) {
        const std::string& symbol = pair.first;
        const LeverageSpec& spec = pair.second;
        
        std::cout << "🔧 Generating " << symbol << " data (" << spec.description << ")..." << std::endl;
        std::cout << "   Using corrected daily return compounding model" << std::endl;
        
        std::vector<sentio::Bar> leverage_bars;
        leverage_bars.reserve(qqq_bars.size());
        
        // Initialize the first bar
        sentio::Bar first_lev_bar = qqq_bars[0];
        first_lev_bar.symbol = symbol;
        
        // Set starting price based on instrument type
        double starting_price = spec.is_inverse ? 50.0 : 100.0;
        first_lev_bar.open = first_lev_bar.high = first_lev_bar.low = first_lev_bar.close = starting_price;
        
        leverage_bars.push_back(first_lev_bar);
        
        // Process remaining bars using daily return compounding
        for (size_t i = 1; i < qqq_bars.size(); ++i) {
            leverage_bars.push_back(
                calculate_leveraged_bar(qqq_bars[i], qqq_bars[i-1], leverage_bars.back(), spec, daily_decay)
            );
            leverage_bars.back().symbol = symbol;
            
            // Show progress for large datasets
            if (qqq_bars.size() > 10000 && i % 10000 == 0) {
                double progress = (static_cast<double>(i) / qqq_bars.size()) * 100.0;
                std::cout << "   Progress: " << std::fixed << std::setprecision(1) << progress << "%" << std::endl;
            }
        }
        
        // Write to CSV
        std::string out_path = output_dir + symbol + "_RTH_NH.csv";
        if (write_leverage_csv(leverage_bars, symbol, out_path)) {
            std::cout << "✅ Successfully saved " << leverage_bars.size() << " bars to " << out_path << std::endl;
            
            // Show sample data
            if (!leverage_bars.empty()) {
                const auto& sample = leverage_bars[0];
                std::cout << "   Sample: Open=" << sample.open << ", High=" << sample.high 
                         << ", Low=" << sample.low << ", Close=" << sample.close << std::endl;
            }
        } else {
            std::cerr << "❌ Failed to save " << symbol << " data" << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "🎯 Leverage Data Generation Complete!" << std::endl;
    std::cout << "Generated files:" << std::endl;
    std::cout << "  - TQQQ_RTH_NH.csv (3x Long QQQ)" << std::endl;
    std::cout << "  - SQQQ_RTH_NH.csv (3x Short QQQ)" << std::endl;
    std::cout << "  - PSQ_RTH_NH.csv (1x Short QQQ)" << std::endl;
    std::cout << std::endl;
    std::cout << "These files can now be used for leverage trading in sentio_cli trade command." << std::endl;
    
    return 0;
}
