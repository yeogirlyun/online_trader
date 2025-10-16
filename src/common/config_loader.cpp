#include "common/config_loader.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>

namespace sentio {
namespace config {

std::optional<OnlineEnsembleStrategy::OnlineEnsembleConfig>
load_best_params(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        utils::log_warning("Could not open config file: " + config_file);
        return std::nullopt;
    }

    // Parse JSON manually (simple key-value extraction)
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    // Helper to extract double value from JSON
    auto extract_double = [&json_content](const std::string& key) -> std::optional<double> {
        std::string search_key = "\"" + key + "\":";
        size_t pos = json_content.find(search_key);
        if (pos == std::string::npos) return std::nullopt;

        // Move past the key
        pos += search_key.length();

        // Skip whitespace
        while (pos < json_content.length() && std::isspace(json_content[pos])) {
            pos++;
        }

        // Extract number
        size_t end = pos;
        while (end < json_content.length() &&
               (std::isdigit(json_content[end]) || json_content[end] == '.' ||
                json_content[end] == '-' || json_content[end] == 'e' || json_content[end] == 'E')) {
            end++;
        }

        if (end == pos) return std::nullopt;

        try {
            return std::stod(json_content.substr(pos, end - pos));
        } catch (...) {
            return std::nullopt;
        }
    };

    // Extract parameters
    auto buy_threshold = extract_double("buy_threshold");
    auto sell_threshold = extract_double("sell_threshold");
    auto ewrls_lambda = extract_double("ewrls_lambda");
    auto bb_amplification_factor = extract_double("bb_amplification_factor");

    if (!buy_threshold || !sell_threshold || !ewrls_lambda || !bb_amplification_factor) {
        utils::log_error("Failed to parse parameters from " + config_file);
        return std::nullopt;
    }

    // Create config with loaded parameters
    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.buy_threshold = *buy_threshold;
    config.sell_threshold = *sell_threshold;
    config.ewrls_lambda = *ewrls_lambda;
    config.bb_amplification_factor = *bb_amplification_factor;

    // Set other defaults
    config.neutral_zone = config.buy_threshold - config.sell_threshold;
    config.warmup_samples = 960;  // 2 days of 1-min bars
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_bb_amplification = true;
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    utils::log_info("Loaded best parameters from " + config_file);
    utils::log_info("  buy_threshold: " + std::to_string(config.buy_threshold));
    utils::log_info("  sell_threshold: " + std::to_string(config.sell_threshold));
    utils::log_info("  ewrls_lambda: " + std::to_string(config.ewrls_lambda));
    utils::log_info("  bb_amplification_factor: " + std::to_string(config.bb_amplification_factor));

    return config;
}

OnlineEnsembleStrategy::OnlineEnsembleConfig get_production_config() {
    // Try to load from config file
    auto loaded_config = load_best_params();
    if (loaded_config) {
        utils::log_info("✅ Using optimized parameters from config/best_params.json");
        return *loaded_config;
    }

    // Fallback to hardcoded defaults
    utils::log_warning("⚠️  Using hardcoded default parameters (config/best_params.json not found)");

    OnlineEnsembleStrategy::OnlineEnsembleConfig config;
    config.buy_threshold = 0.55;
    config.sell_threshold = 0.45;
    config.neutral_zone = 0.10;
    config.ewrls_lambda = 0.995;
    config.warmup_samples = 960;
    config.prediction_horizons = {1, 5, 10};
    config.horizon_weights = {0.3, 0.5, 0.2};
    config.enable_bb_amplification = true;
    config.bb_amplification_factor = 0.10;
    config.enable_adaptive_learning = true;
    config.enable_threshold_calibration = true;

    return config;
}

} // namespace config
} // namespace sentio
