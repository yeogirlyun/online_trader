#pragma once

// =============================================================================
// Module: common/config.h
// Purpose: Lightweight configuration access for application components.
//
// Core idea:
// - Provide a simple key/value configuration store with typed getters.
// - Parsing from YAML/JSON is delegated to thin adapter functions that load
//   files and populate the internal map. Heavy parsing libs can be added later
//   behind the same interface without impacting dependents.
// =============================================================================

#include <string>
#include <map>
#include <memory>

namespace sentio {

class Config {
public:
    // Factory: load configuration from YAML file path.
    static std::shared_ptr<Config> from_yaml(const std::string& path);
    // Factory: load configuration from JSON file path.
    static std::shared_ptr<Config> from_json(const std::string& path);

    // Typed getter: throws std::out_of_range if key missing (no default).
    template<typename T>
    T get(const std::string& key) const;

    // Typed getter with default fallback if key missing or conversion fails.
    template<typename T>
    T get(const std::string& key, const T& default_value) const;

    // Existence check.
    bool has(const std::string& key) const;

private:
    std::map<std::string, std::string> data_; // canonical storage as strings
    friend std::shared_ptr<Config> parse_yaml_file(const std::string&);
    friend std::shared_ptr<Config> parse_json_file(const std::string&);
};

} // namespace sentio


