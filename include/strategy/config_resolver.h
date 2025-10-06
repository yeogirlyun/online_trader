// include/strategy/config_resolver.h
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace sentio {

/**
 * Global config path resolver for strategies
 * Allows setting custom config paths per strategy dynamically
 */
class ConfigResolver {
public:
    /**
     * Set a custom config path for a strategy
     * @param strategy_name Name of the strategy
     * @param config_path Path to custom config file
     */
    static void set_config_path(const std::string& strategy_name, const std::string& config_path);
    
    /**
     * Get config path for a strategy (custom or default)
     * @param strategy_name Name of the strategy
     * @param default_path Default config path to use if no custom path set
     * @return Resolved config path
     */
    static std::string get_config_path(const std::string& strategy_name, const std::string& default_path);
    
    /**
     * Clear custom config path for a strategy
     * @param strategy_name Name of the strategy
     */
    static void clear_config_path(const std::string& strategy_name);
    
    /**
     * Clear all custom config paths
     */
    static void clear_all();

private:
    static std::unordered_map<std::string, std::string> custom_config_paths_;
    static std::mutex mutex_;
};

} // namespace sentio

