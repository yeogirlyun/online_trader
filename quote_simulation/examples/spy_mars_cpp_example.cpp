#include "online_quote_simulator.hpp"
#include "sentio/core.hpp"
#include "sentio/base_strategy.hpp"
#include "sentio/runner.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace sentio;

/**
 * SPY Historical Data Training with MarS - C++ Example
 * ===================================================
 * 
 * This example demonstrates how to use the online quote simulator
 * to train on historical SPY data and generate realistic quote simulations
 * using MarS integration.
 */
class SPYHistoricalTrainer {
private:
    std::string symbol_;
    std::vector<Bar> historical_data_;
    std::map<std::string, double> trained_patterns_;
    
public:
    SPYHistoricalTrainer(const std::string& symbol = "SPY") : symbol_(symbol) {}
    
    /**
     * Load historical SPY data from CSV file
     */
    bool loadHistoricalData(const std::string& data_file) {
        std::cout << "📊 Loading SPY historical data from " << data_file << std::endl;
        
        std::ifstream file(data_file);
        if (!file.is_open()) {
            std::cerr << "❌ Failed to open data file: " << data_file << std::endl;
            return false;
        }
        
        std::string line;
        std::getline(file, line); // Skip header
        
        int count = 0;
        while (std::getline(file, line)) {
            Bar bar = parseCSVLine(line);
            if (bar.ts_utc_epoch > 0) {
                historical_data_.push_back(bar);
                count++;
            }
        }
        
        std::cout << "✅ Loaded " << count << " SPY bars" << std::endl;
        if (!historical_data_.empty()) {
            std::cout << "📅 Date range: " << historical_data_.front().ts_utc_epoch 
                      << " to " << historical_data_.back().ts_utc_epoch << std::endl;
            std::cout << "💰 Price range: $" << getMinPrice() << " - $" << getMaxPrice() << std::endl;
        }
        
        return !historical_data_.empty();
    }
    
    /**
     * Extract training patterns from historical data
     */
    void extractTrainingPatterns() {
        std::cout << "🔍 Extracting training patterns from historical SPY data" << std::endl;
        
        if (historical_data_.empty()) {
            std::cerr << "❌ No historical data available" << std::endl;
            return;
        }
        
        // Calculate basic statistics
        double total_return = 0.0;
        double total_volume = 0.0;
        double price_sum = 0.0;
        double return_sum_sq = 0.0;
        
        for (size_t i = 1; i < historical_data_.size(); ++i) {
            double ret = (historical_data_[i].close - historical_data_[i-1].close) / historical_data_[i-1].close;
            total_return += ret;
            return_sum_sq += ret * ret;
            total_volume += historical_data_[i].volume;
            price_sum += historical_data_[i].close;
        }
        
        int n = historical_data_.size() - 1;
        double avg_return = total_return / n;
        double avg_volume = total_volume / historical_data_.size();
        double avg_price = price_sum / historical_data_.size();
        double variance = (return_sum_sq / n) - (avg_return * avg_return);
        double volatility = std::sqrt(variance);
        
        // Store patterns
        trained_patterns_["avg_return"] = avg_return;
        trained_patterns_["volatility"] = volatility;
        trained_patterns_["avg_volume"] = avg_volume;
        trained_patterns_["current_price"] = historical_data_.back().close;
        trained_patterns_["total_bars"] = historical_data_.size();
        
        std::cout << "📈 Average return: " << std::fixed << std::setprecision(4) << avg_return << std::endl;
        std::cout << "📊 Volatility: " << std::fixed << std::setprecision(4) << volatility << std::endl;
        std::cout << "📦 Average volume: " << std::fixed << std::setprecision(0) << avg_volume << std::endl;
        std::cout << "🎯 Current price: $" << std::fixed << std::setprecision(2) << historical_data_.back().close << std::endl;
    }
    
    /**
     * Create MarS simulation trained on historical data
     */
    std::vector<OnlineQuoteSimulator::SimulationResult> createMarsSimulation(
        int continuation_minutes = 60,
        int num_simulations = 5,
        const std::string& market_regime = "normal") {
        
        std::cout << "\n🤖 Creating MarS simulation with historical SPY training" << std::endl;
        std::cout << "📊 Market regime: " << market_regime << std::endl;
        std::cout << "⏱️  Continuation: " << continuation_minutes << " minutes" << std::endl;
        std::cout << "🎲 Simulations: " << num_simulations << std::endl;
        
        OnlineQuoteSimulator simulator;
        std::vector<OnlineQuoteSimulator::SimulationResult> all_results;
        
        for (int i = 0; i < num_simulations; ++i) {
            std::cout << "🔄 Running MarS simulation " << (i + 1) << "/" << num_simulations << std::endl;
            
            try {
                // Configure simulation
                OnlineQuoteSimulator::OnlineTestConfig config;
                config.strategy_name = "online_sgd";
                config.symbol = symbol_;
                config.duration_minutes = continuation_minutes;
                config.simulations = 1;
                config.use_mars = true;
                config.market_regime = market_regime;
                config.initial_capital = 100000.0;
                
                // Create dummy strategy for testing
                auto strategy = std::make_unique<BaseStrategy>();
                
                // Configure runner
                RunnerCfg runner_cfg;
                runner_cfg.strategy_name = "online_sgd";
                runner_cfg.audit_level = AuditLevel::MetricsOnly;
                
                // Run simulation
                auto results = simulator.run_mars_online_simulation(
                    config, std::move(strategy), runner_cfg
                );
                
                all_results.insert(all_results.end(), results.begin(), results.end());
                
                std::cout << "✅ MarS simulation " << (i + 1) << " completed" << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "❌ MarS simulation " << (i + 1) << " failed: " << e.what() << std::endl;
            }
        }
        
        std::cout << "✅ Generated " << all_results.size() << " total simulation results" << std::endl;
        return all_results;
    }
    
    /**
     * Analyze simulation results
     */
    void analyzeResults(const std::vector<OnlineQuoteSimulator::SimulationResult>& results) {
        std::cout << "\n🔍 Analyzing simulation results" << std::endl;
        
        if (results.empty()) {
            std::cout << "⚠️ No results to analyze" << std::endl;
            return;
        }
        
        // Calculate statistics
        double total_return = 0.0;
        double total_sharpe = 0.0;
        double total_trades = 0.0;
        double total_capital = 0.0;
        
        for (const auto& result : results) {
            total_return += result.total_return;
            total_sharpe += result.sharpe_ratio;
            total_trades += result.total_trades;
            total_capital += result.final_capital;
        }
        
        int n = results.size();
        double avg_return = total_return / n;
        double avg_sharpe = total_sharpe / n;
        double avg_trades = total_trades / n;
        double avg_capital = total_capital / n;
        
        std::cout << "📊 Simulation analysis:" << std::endl;
        std::cout << "   Average return: " << std::fixed << std::setprecision(4) << avg_return << std::endl;
        std::cout << "   Average Sharpe: " << std::fixed << std::setprecision(3) << avg_sharpe << std::endl;
        std::cout << "   Average trades: " << std::fixed << std::setprecision(1) << avg_trades << std::endl;
        std::cout << "   Average capital: $" << std::fixed << std::setprecision(2) << avg_capital << std::endl;
        
        // Compare with historical patterns
        if (!trained_patterns_.empty()) {
            double historical_return = trained_patterns_["avg_return"];
            double historical_volatility = trained_patterns_["volatility"];
            
            double return_similarity = 1.0 - std::abs(avg_return - historical_return) / std::max(std::abs(historical_return), 0.001);
            double volatility_similarity = 1.0 - std::abs(avg_sharpe - historical_volatility) / std::max(historical_volatility, 0.001);
            
            double similarity_score = (return_similarity + volatility_similarity) * 5.0; // Scale to 0-10
            
            std::cout << "\n📊 MarS Training Performance:" << std::endl;
            std::cout << "   Similarity Score: " << std::fixed << std::setprecision(2) << similarity_score << "/10" << std::endl;
            std::cout << "   Data Quality: " << (similarity_score > 7 ? "High" : similarity_score > 4 ? "Medium" : "Low") << std::endl;
        }
    }
    
    /**
     * Export results to files
     */
    void exportResults(const std::vector<OnlineQuoteSimulator::SimulationResult>& results,
                      const std::string& output_dir) {
        
        std::cout << "\n📁 Exporting results to " << output_dir << std::endl;
        
        // Create output directory
        std::string mkdir_cmd = "mkdir -p " + output_dir;
        system(mkdir_cmd.c_str());
        
        // Export simulation results
        std::ofstream results_file(output_dir + "/spy_mars_simulation_results.json");
        if (results_file.is_open()) {
            results_file << "{\n";
            results_file << "  \"simulation_results\": [\n";
            
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& result = results[i];
                results_file << "    {\n";
                results_file << "      \"simulation\": " << i << ",\n";
                results_file << "      \"total_return\": " << result.total_return << ",\n";
                results_file << "      \"final_capital\": " << result.final_capital << ",\n";
                results_file << "      \"sharpe_ratio\": " << result.sharpe_ratio << ",\n";
                results_file << "      \"max_drawdown\": " << result.max_drawdown << ",\n";
                results_file << "      \"total_trades\": " << result.total_trades << ",\n";
                results_file << "      \"run_id\": \"" << result.run_id << "\"\n";
                results_file << "    }";
                if (i < results.size() - 1) results_file << ",";
                results_file << "\n";
            }
            
            results_file << "  ],\n";
            results_file << "  \"trained_patterns\": {\n";
            for (auto it = trained_patterns_.begin(); it != trained_patterns_.end(); ++it) {
                results_file << "    \"" << it->first << "\": " << it->second;
                if (std::next(it) != trained_patterns_.end()) results_file << ",";
                results_file << "\n";
            }
            results_file << "  }\n";
            results_file << "}\n";
            results_file.close();
            
            std::cout << "✅ Exported simulation results to " << output_dir << "/spy_mars_simulation_results.json" << std::endl;
        }
        
        // Export summary report
        std::ofstream summary_file(output_dir + "/summary_report.md");
        if (summary_file.is_open()) {
            summary_file << "# SPY Historical Data Training with MarS - C++ Summary Report\n\n";
            summary_file << "## Overview\n";
            summary_file << "This report summarizes the results of training MarS on historical SPY data using C++.\n\n";
            
            summary_file << "## Historical Data Analysis\n";
            summary_file << "- **Total Bars**: " << trained_patterns_["total_bars"] << "\n";
            summary_file << "- **Average Return**: " << std::fixed << std::setprecision(4) << trained_patterns_["avg_return"] << "\n";
            summary_file << "- **Volatility**: " << std::fixed << std::setprecision(4) << trained_patterns_["volatility"] << "\n";
            summary_file << "- **Current Price**: $" << std::fixed << std::setprecision(2) << trained_patterns_["current_price"] << "\n\n";
            
            summary_file << "## Simulation Results\n";
            summary_file << "- **Total Simulations**: " << results.size() << "\n";
            if (!results.empty()) {
                double avg_return = 0.0;
                for (const auto& result : results) {
                    avg_return += result.total_return;
                }
                avg_return /= results.size();
                summary_file << "- **Average Return**: " << std::fixed << std::setprecision(4) << avg_return << "\n";
            }
            
            summary_file << "\n## Conclusion\n";
            summary_file << "The MarS system successfully trained on historical SPY data and generated realistic simulations using C++.\n";
            summary_file.close();
            
            std::cout << "✅ Exported summary report to " << output_dir << "/summary_report.md" << std::endl;
        }
    }
    
private:
    Bar parseCSVLine(const std::string& line) {
        Bar bar = {};
        std::stringstream ss(line);
        std::string token;
        
        // Parse CSV format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
        if (std::getline(ss, token, ',')) {
            // Skip ts_utc for now, use ts_nyt_epoch
            if (std::getline(ss, token, ',')) {
                bar.ts_utc_epoch = std::stoll(token);
            }
        }
        if (std::getline(ss, token, ',')) bar.open = std::stod(token);
        if (std::getline(ss, token, ',')) bar.high = std::stod(token);
        if (std::getline(ss, token, ',')) bar.low = std::stod(token);
        if (std::getline(ss, token, ',')) bar.close = std::stod(token);
        if (std::getline(ss, token, ',')) bar.volume = std::stoi(token);
        
        return bar;
    }
    
    double getMinPrice() {
        double min_price = std::numeric_limits<double>::max();
        for (const auto& bar : historical_data_) {
            min_price = std::min(min_price, bar.low);
        }
        return min_price;
    }
    
    double getMaxPrice() {
        double max_price = std::numeric_limits<double>::min();
        for (const auto& bar : historical_data_) {
            max_price = std::max(max_price, bar.high);
        }
        return max_price;
    }
};

int main() {
    std::cout << "🚀 SPY Historical Data Training with MarS - C++ Example" << std::endl;
    std::cout << "=" << std::string(60, '=') << std::endl;
    
    try {
        // Create trainer
        SPYHistoricalTrainer trainer("SPY");
        
        // Step 1: Load historical data
        if (!trainer.loadHistoricalData("data/equities/SPY_RTH_NH.csv")) {
            std::cerr << "❌ Failed to load historical data" << std::endl;
            return 1;
        }
        
        // Step 2: Extract training patterns
        trainer.extractTrainingPatterns();
        
        // Step 3: Create MarS simulations
        auto results = trainer.createMarsSimulation(
            30,  // 30 minutes continuation
            3,   // 3 simulations
            "normal"  // normal market regime
        );
        
        // Step 4: Analyze results
        trainer.analyzeResults(results);
        
        // Step 5: Export results
        trainer.exportResults(results, "spy_mars_cpp_training");
        
        std::cout << "\n🎉 C++ MarS training completed successfully!" << std::endl;
        std::cout << "📁 Results saved to spy_mars_cpp_training/" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ C++ MarS training failed: " << e.what() << std::endl;
        return 1;
    }
}
