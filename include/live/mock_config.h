#ifndef SENTIO_MOCK_CONFIG_H
#define SENTIO_MOCK_CONFIG_H

#include "live/broker_client_interface.h"
#include "live/bar_feed_interface.h"
#include <string>
#include <memory>

namespace sentio {

/**
 * Mock Mode Enumeration
 *
 * Defines different mock trading scenarios:
 * - LIVE: Real production trading (no mocking)
 * - REPLAY_HISTORICAL: Exact replay of historical session
 * - STRESS_TEST: Add market stress scenarios (high volatility, gaps)
 * - PARAMETER_SWEEP: Rapid parameter optimization
 * - REGRESSION_TEST: Verify bug fixes and features
 */
enum class MockMode {
    LIVE,                   // Real trading (Alpaca + Polygon)
    REPLAY_HISTORICAL,      // Replay historical data
    STRESS_TEST,           // Add market stress
    PARAMETER_SWEEP,       // Fast parameter testing
    REGRESSION_TEST        // Verify bug fixes
};

/**
 * Mock Configuration
 *
 * Configuration for mock trading infrastructure
 */
struct MockConfig {
    MockMode mode = MockMode::LIVE;

    // Data source
    std::string csv_data_path;
    double speed_multiplier = 1.0;  // 1x = real-time, 39x = accelerated

    // Broker simulation
    double initial_capital = 100000.0;
    double commission_per_share = 0.0;
    FillBehavior fill_behavior = FillBehavior::IMMEDIATE_FULL;

    // Market simulation
    bool enable_market_impact = true;
    double market_impact_bps = 5.0;
    double bid_ask_spread_bps = 2.0;

    // Stress testing (STRESS_TEST mode only)
    bool enable_random_gaps = false;
    bool enable_high_volatility = false;
    double volatility_multiplier = 1.0;

    // Session control
    std::string crash_simulation_time;  // ET time to simulate crash (empty = no crash)
    bool enable_checkpoints = true;
    std::string checkpoint_file;

    // Output
    std::string session_name = "mock_session";
    std::string output_dir = "data/mock_sessions";
    bool save_state_on_exit = true;
};

/**
 * Trading Infrastructure Factory
 *
 * Creates broker and feed clients based on configuration.
 * Enables easy switching between live and mock modes.
 */
class TradingInfrastructureFactory {
public:
    /**
     * Create broker client based on config
     */
    static std::unique_ptr<IBrokerClient> create_broker(const MockConfig& config,
                                                        const std::string& alpaca_key = "",
                                                        const std::string& alpaca_secret = "");

    /**
     * Create bar feed based on config
     */
    static std::unique_ptr<IBarFeed> create_bar_feed(const MockConfig& config,
                                                     const std::string& polygon_url = "",
                                                     const std::string& polygon_key = "");

    /**
     * Parse mock mode from string
     */
    static MockMode parse_mode(const std::string& mode_str);

    /**
     * Convert mock mode to string
     */
    static std::string mode_to_string(MockMode mode);
};

} // namespace sentio

#endif // SENTIO_MOCK_CONFIG_H
