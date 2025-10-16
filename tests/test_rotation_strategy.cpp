/**
 * Integration test for Phase 2: Multi-Symbol Rotation Strategy
 *
 * Tests end-to-end flow:
 * 1. MultiSymbolOESManager generates signals for 6 symbols
 * 2. SignalAggregator ranks signals with leverage boost
 * 3. RotationPositionManager makes position decisions
 * 4. Positions rotate based on signal strength
 *
 * Usage:
 *   ./build/test_rotation_strategy
 */

#include "strategy/multi_symbol_oes_manager.h"
#include "strategy/signal_aggregator.h"
#include "strategy/rotation_position_manager.h"
#include "data/multi_symbol_data_manager.h"
#include "common/utils.h"
#include <iostream>
#include <cassert>

using namespace sentio;
using namespace sentio::data;

void test_oes_manager() {
    utils::log_info("=== Test 1: MultiSymbolOESManager ===");

    // Setup data manager
    MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    auto data_mgr = std::make_shared<MultiSymbolDataManager>(dm_config);

    // Setup OES manager
    MultiSymbolOESManager::Config oes_config;
    oes_config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    MultiSymbolOESManager oes_mgr(oes_config, data_mgr);

    // Feed some bars
    for (int i = 0; i < 150; i++) {
        Bar bar;
        bar.timestamp_ms = 1000000000 + i * 60000;
        bar.bar_id = i;
        bar.open = 50.0 + i * 0.1;
        bar.high = 51.0 + i * 0.1;
        bar.low = 49.0 + i * 0.1;
        bar.close = 50.5 + i * 0.1;
        bar.volume = 1000000;

        data_mgr->update_symbol("TQQQ", bar);
        data_mgr->update_symbol("SQQQ", bar);
        data_mgr->update_symbol("UPRO", bar);

        oes_mgr.on_bar();
    }

    // Check if ready
    assert(oes_mgr.all_ready());

    // Generate signals
    auto signals = oes_mgr.generate_all_signals();
    assert(signals.size() >= 1);

    utils::log_info("Generated " + std::to_string(signals.size()) + " signals");
    for (const auto& [symbol, signal] : signals) {
        utils::log_info("  " + symbol + ": " +
                       (signal.signal == SignalType::LONG ? "LONG" :
                        signal.signal == SignalType::SHORT ? "SHORT" : "NEUTRAL") +
                       " (prob=" + std::to_string(signal.probability) +
                       ", conf=" + std::to_string(signal.confidence) + ")");
    }

    utils::log_info("✓ OES Manager test passed");
}

void test_signal_aggregator() {
    utils::log_info("\n=== Test 2: SignalAggregator ===");

    SignalAggregator::Config config;
    config.leverage_boosts = {
        {"TQQQ", 1.5},
        {"SQQQ", 1.5},
        {"UPRO", 1.5}
    };
    config.min_strength = 0.40;

    SignalAggregator aggregator(config);

    // Create mock signals
    std::map<std::string, SignalOutput> signals;

    SignalOutput sig1;
    sig1.signal = SignalType::LONG;
    sig1.probability = 0.60;
    sig1.confidence = 0.70;
    signals["TQQQ"] = sig1;

    SignalOutput sig2;
    sig2.signal = SignalType::SHORT;
    sig2.probability = 0.55;
    sig2.confidence = 0.65;
    signals["SQQQ"] = sig2;

    SignalOutput sig3;
    sig3.signal = SignalType::LONG;
    sig3.probability = 0.52;
    sig3.confidence = 0.60;
    signals["UPRO"] = sig3;

    // Rank signals
    auto ranked = aggregator.rank_signals(signals);

    utils::log_info("Ranked " + std::to_string(ranked.size()) + " signals:");
    for (const auto& rs : ranked) {
        utils::log_info("  Rank " + std::to_string(rs.rank) + ": " + rs.symbol +
                       " (strength=" + std::to_string(rs.strength) +
                       ", leverage_boost=" + std::to_string(rs.leverage_boost) + ")");
    }

    // Check ranking
    assert(ranked.size() == 3);
    assert(ranked[0].rank == 1);
    assert(ranked[1].rank == 2);
    assert(ranked[2].rank == 3);

    // TQQQ should be strongest (0.60 * 0.70 * 1.5 = 0.63)
    assert(ranked[0].symbol == "TQQQ");

    utils::log_info("✓ Signal Aggregator test passed");
}

void test_rotation_manager() {
    utils::log_info("\n=== Test 3: RotationPositionManager ===");

    RotationPositionManager::Config config;
    config.max_positions = 2;
    config.min_strength_to_enter = 0.40;
    config.min_strength_to_hold = 0.35;

    RotationPositionManager rpm(config);

    // Create ranked signals
    std::vector<SignalAggregator::RankedSignal> ranked_signals;

    SignalAggregator::RankedSignal rs1;
    rs1.symbol = "TQQQ";
    rs1.signal.signal = SignalType::LONG;
    rs1.signal.probability = 0.60;
    rs1.signal.confidence = 0.70;
    rs1.leverage_boost = 1.5;
    rs1.strength = 0.63;
    rs1.rank = 1;
    ranked_signals.push_back(rs1);

    SignalAggregator::RankedSignal rs2;
    rs2.symbol = "SQQQ";
    rs2.signal.signal = SignalType::SHORT;
    rs2.signal.probability = 0.55;
    rs2.signal.confidence = 0.65;
    rs2.leverage_boost = 1.5;
    rs2.strength = 0.54;
    rs2.rank = 2;
    ranked_signals.push_back(rs2);

    SignalAggregator::RankedSignal rs3;
    rs3.symbol = "UPRO";
    rs3.signal.signal = SignalType::LONG;
    rs3.signal.probability = 0.52;
    rs3.signal.confidence = 0.60;
    rs3.leverage_boost = 1.5;
    rs3.strength = 0.47;
    rs3.rank = 3;
    ranked_signals.push_back(rs3);

    // Current prices
    std::map<std::string, double> current_prices = {
        {"TQQQ", 50.0},
        {"SQQQ", 20.0},
        {"UPRO", 80.0}
    };

    // Make decisions (should enter top 2)
    auto decisions = rpm.make_decisions(ranked_signals, current_prices, 100);

    utils::log_info("Decisions:");
    for (const auto& decision : decisions) {
        std::string decision_str;
        switch (decision.decision) {
            case RotationPositionManager::Decision::ENTER_LONG:
                decision_str = "ENTER_LONG";
                break;
            case RotationPositionManager::Decision::ENTER_SHORT:
                decision_str = "ENTER_SHORT";
                break;
            case RotationPositionManager::Decision::HOLD:
                decision_str = "HOLD";
                break;
            case RotationPositionManager::Decision::EXIT:
                decision_str = "EXIT";
                break;
            default:
                decision_str = "OTHER";
        }

        utils::log_info("  " + decision.symbol + ": " + decision_str +
                       " - " + decision.reason);
    }

    // Should have 2 ENTER decisions
    int enter_count = 0;
    for (const auto& decision : decisions) {
        if (decision.decision == RotationPositionManager::Decision::ENTER_LONG ||
            decision.decision == RotationPositionManager::Decision::ENTER_SHORT) {
            enter_count++;

            // Execute decision
            rpm.execute_decision(decision, current_prices.at(decision.symbol));
        }
    }

    assert(enter_count == 2);
    assert(rpm.get_position_count() == 2);

    utils::log_info("Position count: " + std::to_string(rpm.get_position_count()));

    utils::log_info("✓ Rotation Manager test passed");
}

void test_rotation_integration() {
    utils::log_info("\n=== Test 4: End-to-End Rotation Integration ===");

    // Setup complete stack
    MultiSymbolDataManager::Config dm_config;
    dm_config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    auto data_mgr = std::make_shared<MultiSymbolDataManager>(dm_config);

    MultiSymbolOESManager::Config oes_config;
    oes_config.symbols = {"TQQQ", "SQQQ", "UPRO"};
    MultiSymbolOESManager oes_mgr(oes_config, data_mgr);

    SignalAggregator::Config agg_config;
    agg_config.leverage_boosts = {
        {"TQQQ", 1.5},
        {"SQQQ", 1.5},
        {"UPRO", 1.5}
    };
    SignalAggregator aggregator(agg_config);

    RotationPositionManager::Config rpm_config;
    rpm_config.max_positions = 2;
    rpm_config.min_strength_to_enter = 0.40;
    RotationPositionManager rpm(rpm_config);

    // Warmup phase (150 bars)
    utils::log_info("Warming up...");
    for (int i = 0; i < 150; i++) {
        Bar bar;
        bar.timestamp_ms = 1000000000 + i * 60000;
        bar.bar_id = i;
        bar.open = 50.0 + (i % 10) * 0.5;
        bar.high = 52.0 + (i % 10) * 0.5;
        bar.low = 48.0 + (i % 10) * 0.5;
        bar.close = 51.0 + (i % 10) * 0.5;
        bar.volume = 1000000;

        data_mgr->update_symbol("TQQQ", bar);
        data_mgr->update_symbol("SQQQ", bar);
        data_mgr->update_symbol("UPRO", bar);

        oes_mgr.on_bar();
    }

    assert(oes_mgr.all_ready());
    utils::log_info("✓ Warmup complete");

    // Trading phase (50 bars)
    utils::log_info("Trading phase...");
    int total_trades = 0;

    for (int i = 0; i < 50; i++) {
        // Update bars
        Bar bar;
        bar.timestamp_ms = 1000000000 + (150 + i) * 60000;
        bar.bar_id = 150 + i;
        bar.open = 50.0 + (i % 10) * 0.5;
        bar.high = 52.0 + (i % 10) * 0.5;
        bar.low = 48.0 + (i % 10) * 0.5;
        bar.close = 51.0 + (i % 10) * 0.5;
        bar.volume = 1000000;

        data_mgr->update_symbol("TQQQ", bar);
        data_mgr->update_symbol("SQQQ", bar);
        data_mgr->update_symbol("UPRO", bar);

        oes_mgr.on_bar();

        // Generate signals
        auto signals = oes_mgr.generate_all_signals();

        // Rank signals
        auto ranked = aggregator.rank_signals(signals);

        // Make position decisions
        std::map<std::string, double> current_prices = {
            {"TQQQ", bar.close},
            {"SQQQ", bar.close * 0.4},
            {"UPRO", bar.close * 1.6}
        };

        auto decisions = rpm.make_decisions(ranked, current_prices, i);

        // Execute decisions
        for (const auto& decision : decisions) {
            if (decision.decision != RotationPositionManager::Decision::HOLD) {
                rpm.execute_decision(decision, current_prices.at(decision.symbol));
                total_trades++;
            }
        }

        // Update prices
        rpm.update_prices(current_prices);
    }

    utils::log_info("Total trades executed: " + std::to_string(total_trades));
    utils::log_info("Final position count: " + std::to_string(rpm.get_position_count()));

    auto stats = rpm.get_stats();
    utils::log_info("Stats:");
    utils::log_info("  Entries: " + std::to_string(stats.entries));
    utils::log_info("  Exits: " + std::to_string(stats.exits));
    utils::log_info("  Holds: " + std::to_string(stats.holds));
    utils::log_info("  Rotations: " + std::to_string(stats.rotations));

    assert(total_trades > 0);

    utils::log_info("✓ End-to-end integration test passed");
}

int main(int argc, char** argv) {
    utils::log_info("========================================");
    utils::log_info("Phase 2: Multi-Symbol Rotation Strategy Test");
    utils::log_info("========================================\n");

    try {
        test_oes_manager();
        test_signal_aggregator();
        test_rotation_manager();
        test_rotation_integration();

        utils::log_info("\n========================================");
        utils::log_info("✅ ALL TESTS PASSED");
        utils::log_info("========================================");

        return 0;

    } catch (const std::exception& e) {
        utils::log_error("Test failed: " + std::string(e.what()));
        return 1;
    }
}
