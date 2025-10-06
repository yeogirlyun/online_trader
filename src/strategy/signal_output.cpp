#include "strategy/signal_output.h"
#include "common/utils.h"

#include <sstream>
#include <iostream>

#ifdef NLOHMANN_JSON_AVAILABLE
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif

namespace sentio {

std::string SignalOutput::to_json() const {
#ifdef NLOHMANN_JSON_AVAILABLE
    nlohmann::json j;
    j["version"] = "2.0";  // Version field for migration
    if (bar_id != 0) j["bar_id"] = bar_id;  // Numeric
    j["timestamp_ms"] = timestamp_ms;  // Numeric
    j["bar_index"] = bar_index;  // Numeric
    j["symbol"] = symbol;
    j["probability"] = probability;  // Numeric - CRITICAL FIX
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        j["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        j["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        j["signal_type"] = "NEUTRAL";
    }
    
    j["strategy_name"] = strategy_name;
    j["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        j["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        j["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        j["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        j["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        j["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        j["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        j["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        j["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        j["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return j.dump();
#else
    // Fallback to string-based JSON (legacy format v1.0)
    std::map<std::string, std::string> m;
    m["version"] = "1.0";
    if (bar_id != 0) m["bar_id"] = std::to_string(bar_id);
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["probability"] = std::to_string(probability);
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        m["signal_type"] = "LONG";
    } else if (signal_type == SignalType::SHORT) {
        m["signal_type"] = "SHORT";
    } else if (signal_type == SignalType::NEUTRAL) {
        m["signal_type"] = "NEUTRAL";
    }
    
    m["strategy_name"] = strategy_name;
    m["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        m["market_data_path"] = it->second;
    }
    
    // Include calibration method for debugging
    auto cal_it = metadata.find("calibration_method");
    if (cal_it != metadata.end()) {
        m["calibration_method"] = cal_it->second;
    }
    
    // Include raw and calibrated probabilities for debugging
    auto raw_it = metadata.find("raw_probability");
    if (raw_it != metadata.end()) {
        m["raw_probability"] = raw_it->second;
    }
    
    auto cal_prob_it = metadata.find("calibrated_probability");
    if (cal_prob_it != metadata.end()) {
        m["calibrated_probability"] = cal_prob_it->second;
    }
    
    // Include optimization metadata
    auto opt_config_it = metadata.find("optimized_config");
    if (opt_config_it != metadata.end()) {
        m["optimized_config"] = opt_config_it->second;
    }
    
    auto eff_conf_it = metadata.find("effective_confidence_threshold");
    if (eff_conf_it != metadata.end()) {
        m["effective_confidence_threshold"] = eff_conf_it->second;
    }
    
    auto bear_thresh_it = metadata.find("bear_threshold");
    if (bear_thresh_it != metadata.end()) {
        m["bear_threshold"] = bear_thresh_it->second;
    }
    
    auto bull_thresh_it = metadata.find("bull_threshold");
    if (bull_thresh_it != metadata.end()) {
        m["bull_threshold"] = bull_thresh_it->second;
    }
    
    // Include minimum_hold_bars for PSM hold period control
    auto hold_bars_it = metadata.find("minimum_hold_bars");
    if (hold_bars_it != metadata.end()) {
        m["minimum_hold_bars"] = hold_bars_it->second;
    }
    
    return utils::to_json(m);
#endif
}

std::string SignalOutput::to_csv() const {
    std::ostringstream os;
    os << timestamp_ms << ','
       << bar_index << ','
       << symbol << ','
       << probability << ',';
    
    // Add signal_type
    if (signal_type == SignalType::LONG) {
        os << "LONG,";
    } else if (signal_type == SignalType::SHORT) {
        os << "SHORT,";
    } else {
        os << "NEUTRAL,";
    }
    
    os << strategy_name << ','
       << strategy_version;
    return os.str();
}

SignalOutput SignalOutput::from_json(const std::string& json_str) {
    SignalOutput s;
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        
        // Handle both numeric (v2.0) and string (v1.0) formats
        if (j.contains("bar_id")) {
            if (j["bar_id"].is_number()) {
                s.bar_id = j["bar_id"].get<uint64_t>();
            } else if (j["bar_id"].is_string()) {
                s.bar_id = static_cast<uint64_t>(std::stoull(j["bar_id"].get<std::string>()));
            }
        }
        
        if (j.contains("timestamp_ms")) {
            if (j["timestamp_ms"].is_number()) {
                s.timestamp_ms = j["timestamp_ms"].get<int64_t>();
            } else if (j["timestamp_ms"].is_string()) {
                s.timestamp_ms = std::stoll(j["timestamp_ms"].get<std::string>());
            }
        }
        
        if (j.contains("bar_index")) {
            if (j["bar_index"].is_number()) {
                s.bar_index = j["bar_index"].get<int>();
            } else if (j["bar_index"].is_string()) {
                s.bar_index = std::stoi(j["bar_index"].get<std::string>());
            }
        }
        
        if (j.contains("symbol")) s.symbol = j["symbol"].get<std::string>();
        
        // Parse signal_type
        if (j.contains("signal_type")) {
            std::string type_str = j["signal_type"].get<std::string>();
            std::cerr << "DEBUG: Parsing signal_type='" << type_str << "'" << std::endl;
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
                std::cerr << "DEBUG: Set to LONG" << std::endl;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
                std::cerr << "DEBUG: Set to SHORT" << std::endl;
            } else {
                s.signal_type = SignalType::NEUTRAL;
                std::cerr << "DEBUG: Set to NEUTRAL (default)" << std::endl;
            }
        } else {
            std::cerr << "DEBUG: signal_type field NOT FOUND in JSON" << std::endl;
        }
        
        if (j.contains("probability")) {
            if (j["probability"].is_number()) {
                s.probability = j["probability"].get<double>();
            } else if (j["probability"].is_string()) {
                std::string prob_str = j["probability"].get<std::string>();
                if (!prob_str.empty()) {
                    try {
                        s.probability = std::stod(prob_str);
                    } catch (const std::exception& e) {
                        std::cerr << "ERROR: Failed to parse probability '" << prob_str << "': " << e.what() << "\n";
                        std::cerr << "JSON: " << json_str << "\n";
                        throw;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // Fallback to string-based parsing
        std::cerr << "WARNING: nlohmann::json parsing failed, falling back to string parsing: " << e.what() << "\n";
        auto m = utils::from_json(json_str);
        if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
        if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
        if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
        if (m.count("symbol")) s.symbol = m["symbol"];
        if (m.count("signal_type")) {
            std::string type_str = m["signal_type"];
            if (type_str == "LONG") {
                s.signal_type = SignalType::LONG;
            } else if (type_str == "SHORT") {
                s.signal_type = SignalType::SHORT;
            } else {
                s.signal_type = SignalType::NEUTRAL;
            }
        }
        if (m.count("probability") && !m["probability"].empty()) {
            try {
                s.probability = std::stod(m["probability"]);
            } catch (const std::exception& e2) {
                std::cerr << "ERROR: Failed to parse probability from string map '" << m["probability"] << "': " << e2.what() << "\n";
                std::cerr << "Original JSON: " << json_str << "\n";
                throw;
            }
        }
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("bar_id")) s.bar_id = static_cast<uint64_t>(std::stoull(m["bar_id"]));
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
    if (m.count("symbol")) s.symbol = m["symbol"];
    if (m.count("signal_type")) {
        std::string type_str = m["signal_type"];
        if (type_str == "LONG") {
            s.signal_type = SignalType::LONG;
        } else if (type_str == "SHORT") {
            s.signal_type = SignalType::SHORT;
        } else {
            s.signal_type = SignalType::NEUTRAL;
        }
    }
    if (m.count("probability") && !m["probability"].empty()) {
        s.probability = std::stod(m["probability"]);
    }
#endif
    
    // Parse additional metadata (strategy_name, strategy_version, etc.)
    // Note: signal_type is already parsed above in the main parsing section
#ifdef NLOHMANN_JSON_AVAILABLE
    try {
        auto j = nlohmann::json::parse(json_str);
        if (j.contains("strategy_name")) s.strategy_name = j["strategy_name"].get<std::string>();
        if (j.contains("strategy_version")) s.strategy_version = j["strategy_version"].get<std::string>();
        if (j.contains("market_data_path")) s.metadata["market_data_path"] = j["market_data_path"].get<std::string>();
        if (j.contains("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = j["minimum_hold_bars"].get<std::string>();
    } catch (...) {
        // Fallback to string map
        auto m = utils::from_json(json_str);
        if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
        if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
        if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
        if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
    }
#else
    auto m = utils::from_json(json_str);
    if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
    if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
    if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
    if (m.count("minimum_hold_bars")) s.metadata["minimum_hold_bars"] = m["minimum_hold_bars"];
#endif
    return s;
}

} // namespace sentio


