#include "backend/backend_component.h"
#include <iostream>
#include <fstream>

int main() {
    try {
        std::cout << "Creating BackendConfig..." << std::endl;
        sentio::BackendComponent::BackendConfig config;
        config.starting_capital = 100000.0;
        config.leverage_enabled = false;
        config.strategy_thresholds["buy_threshold"] = 0.6;
        config.strategy_thresholds["sell_threshold"] = 0.4;
        
        std::cout << "Creating BackendComponent..." << std::endl;
        auto backend = std::make_unique<sentio::BackendComponent>(config);
        
        std::cout << "Testing process_to_jsonl with debug output..." << std::endl;
        
        // Create a debug output file
        std::string debug_file = "debug_trades_output.jsonl";
        bool success = backend->process_to_jsonl(
            "data/signals/sgo-09-29-04-59-49.jsonl",
            "data/equities/QQQ_RTH_NH.csv",
            debug_file,
            "debug_run",
            0,
            SIZE_MAX,
            0.0
        );
        
        if (success) {
            std::cout << "process_to_jsonl succeeded!" << std::endl;
            
            // Check file size
            std::ifstream file(debug_file);
            file.seekg(0, std::ios::end);
            std::streampos fileSize = file.tellg();
            file.close();
            
            std::cout << "Debug file size: " << fileSize << " bytes" << std::endl;
            
            // Count lines
            std::ifstream count_file(debug_file);
            std::string line;
            int line_count = 0;
            while (std::getline(count_file, line)) {
                line_count++;
            }
            std::cout << "Debug file lines: " << line_count << std::endl;
            
        } else {
            std::cout << "process_to_jsonl failed!" << std::endl;
        }
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
