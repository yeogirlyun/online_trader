#include "live/mock_config.h"
#include "live/alpaca_client_adapter.h"
#include "live/polygon_client_adapter.h"
#include "live/mock_broker.h"
#include "live/mock_bar_feed_replay.h"
#include <algorithm>
#include <cctype>

namespace sentio {

std::unique_ptr<IBrokerClient> TradingInfrastructureFactory::create_broker(
    const MockConfig& config,
    const std::string& alpaca_key,
    const std::string& alpaca_secret) {

    if (config.mode == MockMode::LIVE) {
        // Real Alpaca broker
        return std::make_unique<AlpacaClientAdapter>(alpaca_key, alpaca_secret, true);
    } else {
        // Mock broker for all other modes
        auto mock_broker = std::make_unique<MockBroker>(
            config.initial_capital,
            config.commission_per_share
        );

        mock_broker->set_fill_behavior(config.fill_behavior);

        return mock_broker;
    }
}

std::unique_ptr<IBarFeed> TradingInfrastructureFactory::create_bar_feed(
    const MockConfig& config,
    const std::string& polygon_url,
    const std::string& polygon_key) {

    if (config.mode == MockMode::LIVE) {
        // Real Polygon feed
        return std::make_unique<PolygonClientAdapter>(polygon_url, polygon_key);
    } else {
        // Mock replay feed for all other modes
        auto mock_feed = std::make_unique<MockBarFeedReplay>(
            config.csv_data_path,
            config.speed_multiplier
        );

        return mock_feed;
    }
}

MockMode TradingInfrastructureFactory::parse_mode(const std::string& mode_str) {
    std::string lower_mode = mode_str;
    std::transform(lower_mode.begin(), lower_mode.end(), lower_mode.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    if (lower_mode == "live") {
        return MockMode::LIVE;
    } else if (lower_mode == "replay" || lower_mode == "replay_historical") {
        return MockMode::REPLAY_HISTORICAL;
    } else if (lower_mode == "stress" || lower_mode == "stress_test") {
        return MockMode::STRESS_TEST;
    } else if (lower_mode == "param" || lower_mode == "parameter_sweep") {
        return MockMode::PARAMETER_SWEEP;
    } else if (lower_mode == "regression" || lower_mode == "regression_test") {
        return MockMode::REGRESSION_TEST;
    } else {
        return MockMode::LIVE;  // Default to live
    }
}

std::string TradingInfrastructureFactory::mode_to_string(MockMode mode) {
    switch (mode) {
        case MockMode::LIVE:
            return "LIVE";
        case MockMode::REPLAY_HISTORICAL:
            return "REPLAY_HISTORICAL";
        case MockMode::STRESS_TEST:
            return "STRESS_TEST";
        case MockMode::PARAMETER_SWEEP:
            return "PARAMETER_SWEEP";
        case MockMode::REGRESSION_TEST:
            return "REGRESSION_TEST";
        default:
            return "UNKNOWN";
    }
}

} // namespace sentio
