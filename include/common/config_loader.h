#pragma once

#include <string>
#include <optional>
#include "strategy/online_ensemble_strategy.h"

namespace sentio {
namespace config {

/**
 * Load best parameters from JSON file.
 *
 * This function loads optimized parameters from config/best_params.json
 * which is updated by Optuna optimization runs.
 *
 * @param config_file Path to best_params.json (default: config/best_params.json)
 * @return OnlineEnsembleConfig with loaded parameters, or std::nullopt if file not found
 */
std::optional<OnlineEnsembleStrategy::OnlineEnsembleConfig>
load_best_params(const std::string& config_file = "config/best_params.json");

/**
 * Get default config with fallback to hardcoded values.
 *
 * Tries to load from config/best_params.json first, falls back to defaults.
 */
OnlineEnsembleStrategy::OnlineEnsembleConfig get_production_config();

} // namespace config
} // namespace sentio
