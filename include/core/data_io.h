#pragma once
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace sentio {

// Forward declaration
struct Bar;

// Load detector outputs from files or database
std::vector<std::vector<double>> load_detector_outputs(const std::string& path);

// Load market data
std::vector<Bar> load_market_data(const std::string& path);

// Save detector outputs
void save_detector_outputs(const std::vector<std::vector<double>>& outputs,
                          const std::string& path);

// Load/Save JSON data
nlohmann::json load_json(const std::string& path);
void save_json(const nlohmann::json& data, const std::string& path);

} // namespace sentio


#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace sentio {

// Forward declaration
struct Bar;

// Load detector outputs from files or database
std::vector<std::vector<double>> load_detector_outputs(const std::string& path);

// Load market data
std::vector<Bar> load_market_data(const std::string& path);

// Save detector outputs
void save_detector_outputs(const std::vector<std::vector<double>>& outputs,
                          const std::string& path);

// Load/Save JSON data
nlohmann::json load_json(const std::string& path);
void save_json(const nlohmann::json& data, const std::string& path);

} // namespace sentio




