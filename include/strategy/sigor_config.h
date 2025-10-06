#pragma once

// =============================================================================
// Module: strategy/sigor_config.h
// Purpose: Configuration for SigorStrategy detectors and fusion.
//
// Format: JSON (flat map of key -> string/number), e.g.
// {
//   "k": 1.5,
//   "w_boll": 1.0, "w_rsi": 1.0, "w_mom": 1.0,
//   "w_vwap": 1.0, "w_orb": 0.5, "w_ofi": 0.5, "w_vol": 0.5,
//   "win_boll": 20, "win_rsi": 14, "win_mom": 10, "win_vwap": 20,
//   "orb_opening_bars": 30
// }
//
// Notes:
// - "k" controls sharpness in log-odds fusion: P = sigma(k * L).
// - w_* are detector reliabilities; set to 0 to disable a detector.
// - win_* are window lengths for respective detectors.
// =============================================================================

#include <string>

namespace sentio {

struct SigorConfig {
    // Fusion
    double k = 1.5; // sharpness/temperature

    // Detector weights (reliability)
    double w_boll = 1.0;
    double w_rsi  = 1.0;
    double w_mom  = 1.0;
    double w_vwap = 1.0;
    double w_orb  = 0.5;
    double w_ofi  = 0.5;
    double w_vol  = 0.5;

    // Windows / parameters
    int win_boll = 20;
    int win_rsi  = 14;
    int win_mom  = 10;
    int win_vwap = 20;
    int orb_opening_bars = 30;

    static SigorConfig defaults();
    static SigorConfig from_file(const std::string& path);
};

} // namespace sentio


