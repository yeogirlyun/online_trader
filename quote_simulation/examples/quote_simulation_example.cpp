#include "online_quote_simulator.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace sentio;

int main() {
    std::cout << "🚀 Online Quote Simulation Example" << std::endl;
    std::cout << "==================================" << std::endl;
    
    // Create quote simulator
    OnlineQuoteSimulator simulator;
    
    // Example 1: Generate real-time quotes
    std::cout << "\n📊 Example 1: Generating real-time quotes" << std::endl;
    auto quotes = simulator.generate_realtime_quotes("QQQ", 5, 1000); // 5 minutes, 1-second intervals
    
    std::cout << "Generated " << quotes.size() << " quotes:" << std::endl;
    for (size_t i = 0; i < std::min(quotes.size(), size_t(5)); ++i) {
        const auto& quote = quotes[i];
        std::cout << "  " << quote.symbol << ": Bid=" << quote.bid_price 
                  << " Ask=" << quote.ask_price << " Last=" << quote.last_price 
                  << " Volume=" << quote.volume << std::endl;
    }
    
    // Example 2: MarS integration
    std::cout << "\n🤖 Example 2: MarS integration" << std::endl;
    OnlineTestConfig config;
    config.strategy_name = "online_sgd";
    config.symbol = "QQQ";
    config.duration_minutes = 10;
    config.simulations = 3;
    config.use_mars = true;
    config.market_regime = "normal";
    
    auto results = simulator.run_mars_online_simulation(
        config, 
        std::make_unique<BaseStrategy>(), // Dummy strategy for example
        RunnerCfg{}
    );
    
    std::cout << "MarS simulation completed: " << results.size() << " results" << std::endl;
    
    // Example 3: Mock Polygon API
    std::cout << "\n🔌 Example 3: Mock Polygon API" << std::endl;
    MockPolygonAPI mock_api("mock_key");
    
    auto quote = mock_api.get_realtime_quote("QQQ");
    std::cout << "Mock Polygon quote: " << quote.symbol << " Bid=" << quote.bid 
              << " Ask=" << quote.ask << " Last=" << quote.last << std::endl;
    
    // Example 4: Real-time streaming
    std::cout << "\n📡 Example 4: Real-time streaming (5 seconds)" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(5);
    
    simulator.start_realtime_streaming("QQQ", [](const QuoteData& quote) {
        std::cout << "📊 " << quote.symbol << ": Bid=" << quote.bid_price 
                  << " Ask=" << quote.ask_price << " Last=" << quote.last_price << std::endl;
    }, 1000); // 1-second intervals
    
    // Let it run for 5 seconds
    while (std::chrono::steady_clock::now() < end_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    simulator.stop_realtime_streaming();
    
    std::cout << "\n✅ Example completed successfully!" << std::endl;
    return 0;
}
