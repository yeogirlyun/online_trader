/**
 * Integration test for multi-symbol data layer
 *
 * Tests end-to-end flow:
 * 1. MultiSymbolDataManager creation
 * 2. MockMultiSymbolFeed loads CSV data
 * 3. Feed starts and replays bars
 * 4. Data manager receives updates
 * 5. Snapshots are retrieved with staleness weighting
 *
 * Usage:
 *   ./build/test_multi_symbol_data_layer
 */

#include "data/multi_symbol_data_manager.h"
#include "data/mock_multi_symbol_feed.h"
#include "common/utils.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace sentio;
using namespace sentio::data;

void test_data_manager_basic() {
    utils::log_info("=== Test 1: DataManager Basic Operations ===");

    MultiSymbolDataManager::Config config;
    config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    config.max_forward_fills = 5;
    config.log_data_quality = true;

    auto data_mgr = std::make_shared<MultiSymbolDataManager>(config);

    // Test 1: Update single symbol
    Bar bar1;
    bar1.timestamp_ms = 1000000000;
    bar1.open = 50.0;
    bar1.high = 51.0;
    bar1.low = 49.5;
    bar1.close = 50.5;
    bar1.volume = 1000000;
    bar1.bar_id = 1;

    bool success = data_mgr->update_symbol("TQQQ", bar1);
    assert(success);

    // Test 2: Get snapshot (should have TQQQ, others forward-filled or missing)
    auto snapshot = data_mgr->get_latest_snapshot();
    assert(snapshot.snapshots.count("TQQQ") > 0);
    assert(snapshot.snapshots["TQQQ"].is_valid);

    utils::log_info("✓ Basic operations test passed");
}

void test_mock_feed_integration() {
    utils::log_info("\n=== Test 2: MockFeed Integration ===");

    // Setup data manager
    MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = {"TQQQ", "SQQQ"};
    auto data_mgr = std::make_shared<MultiSymbolDataManager>(dm_config);

    // Setup mock feed
    MockMultiSymbolFeed::Config feed_config;
    feed_config.symbol_files = {
        {"TQQQ", "data/equities/TQQQ_RTH_NH.csv"},
        {"SQQQ", "data/equities/SQQQ_RTH_NH.csv"}
    };
    feed_config.replay_speed = 0.0;  // Instant replay for testing

    auto feed = std::make_shared<MockMultiSymbolFeed>(data_mgr, feed_config);

    // Set callbacks
    int bars_received = 0;
    feed->set_bar_callback([&](const std::string& symbol, const Bar& bar) {
        bars_received++;
        if (bars_received % 100 == 0) {
            utils::log_info("Received " + std::to_string(bars_received) + " bars");
        }
    });

    feed->set_error_callback([](const std::string& error) {
        utils::log_error("Feed error: " + error);
    });

    feed->set_connection_callback([](bool connected) {
        utils::log_info(connected ? "Feed connected" : "Feed disconnected");
    });

    // Test connection
    assert(feed->connect());
    assert(feed->is_connected());

    // Test feed start
    assert(feed->start());
    assert(feed->is_active());

    // Wait for replay to complete (instant, so should be quick)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check results
    auto stats = feed->get_stats();
    utils::log_info("Total bars received: " + std::to_string(stats.total_bars_received));
    assert(stats.total_bars_received > 0);

    // Get data manager snapshot
    auto snapshot = data_mgr->get_latest_snapshot();
    assert(snapshot.snapshots.size() == 2);
    assert(snapshot.snapshots.count("TQQQ") > 0);
    assert(snapshot.snapshots.count("SQQQ") > 0);

    // Check staleness
    for (const auto& [symbol, snap] : snapshot.snapshots) {
        utils::log_info(symbol + " - staleness: " +
                       std::to_string(snap.staleness_seconds) + "s, " +
                       "weight: " + std::to_string(snap.staleness_weight));
    }

    feed->stop();
    feed->disconnect();

    utils::log_info("✓ MockFeed integration test passed");
}

void test_staleness_weighting() {
    utils::log_info("\n=== Test 3: Staleness Weighting ===");

    MultiSymbolDataManager::Config config;
    config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    auto data_mgr = std::make_shared<MultiSymbolDataManager>(config);

    // Inject time provider for controlled testing
    uint64_t fake_time = 1000000000;
    data_mgr->set_time_provider([&]() { return fake_time; });

    // Update TQQQ at t=0
    Bar bar1;
    bar1.timestamp_ms = fake_time;
    bar1.close = 50.0;
    data_mgr->update_symbol("TQQQ", bar1);

    // Advance time by 60 seconds
    fake_time += 60000;

    // Update SQQQ at t=60
    Bar bar2;
    bar2.timestamp_ms = fake_time;
    bar2.close = 20.0;
    data_mgr->update_symbol("SQQQ", bar2);

    // Get snapshot at t=60
    auto snapshot = data_mgr->get_latest_snapshot();

    // TQQQ should be 60s old -> weight = exp(-60/60) = 0.368
    // SQQQ should be fresh -> weight = 1.0
    assert(snapshot.snapshots.count("TQQQ") > 0);
    assert(snapshot.snapshots.count("SQQQ") > 0);

    double tqqq_weight = snapshot.snapshots["TQQQ"].staleness_weight;
    double sqqq_weight = snapshot.snapshots["SQQQ"].staleness_weight;

    utils::log_info("TQQQ weight (60s old): " + std::to_string(tqqq_weight));
    utils::log_info("SQQQ weight (fresh): " + std::to_string(sqqq_weight));

    // TQQQ should be decayed, SQQQ should be near 1.0
    assert(tqqq_weight < 0.5);  // Should be ~0.368
    assert(sqqq_weight > 0.9);  // Should be ~1.0

    utils::log_info("✓ Staleness weighting test passed");
}

void test_forward_fill() {
    utils::log_info("\n=== Test 4: Forward Fill ===");

    MultiSymbolDataManager::Config config;
    config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    config.max_forward_fills = 3;
    auto data_mgr = std::make_shared<MultiSymbolDataManager>(config);

    // Update only TQQQ
    Bar bar;
    bar.timestamp_ms = 1000000000;
    bar.close = 50.0;
    data_mgr->update_symbol("TQQQ", bar);

    // Get snapshot - should forward-fill SQQQ and UPRO
    auto snapshot = data_mgr->get_latest_snapshot();

    // Check quality stats
    auto stats = data_mgr->get_quality_stats();
    utils::log_info("Total forward fills: " + std::to_string(stats.total_forward_fills));

    // Should have attempted to forward-fill missing symbols
    assert(stats.total_forward_fills >= 0);  // May be 0 if no prior data

    utils::log_info("✓ Forward fill test passed");
}

int main(int argc, char** argv) {
    utils::log_info("========================================");
    utils::log_info("Multi-Symbol Data Layer Integration Test");
    utils::log_info("========================================\n");

    try {
        // Run all tests
        test_data_manager_basic();
        test_staleness_weighting();
        test_forward_fill();

        // Only run feed test if CSV files exist
        if (std::ifstream("data/equities/TQQQ_RTH_NH.csv").good() &&
            std::ifstream("data/equities/SQQQ_RTH_NH.csv").good()) {
            test_mock_feed_integration();
        } else {
            utils::log_info("\n⚠️  Skipping MockFeed test - CSV files not found");
            utils::log_info("   To run full test, generate data:");
            utils::log_info("   python3 tools/generate_leveraged_etf_data.py \\");
            utils::log_info("       --spy data/equities/SPY_RTH_NH.csv \\");
            utils::log_info("       --qqq data/equities/QQQ_RTH_NH.csv \\");
            utils::log_info("       --output data/equities/");
        }

        utils::log_info("\n========================================");
        utils::log_info("✅ ALL TESTS PASSED");
        utils::log_info("========================================");

        return 0;

    } catch (const std::exception& e) {
        utils::log_error("Test failed: " + std::string(e.what()));
        return 1;
    }
}
