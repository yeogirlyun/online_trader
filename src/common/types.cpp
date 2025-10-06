#include "common/types.h"
#include "common/utils.h"

// =============================================================================
// Implementation: common/types.cpp
// Provides serialization helpers for PortfolioState.
// =============================================================================

namespace sentio {

// Serialize a PortfolioState to a minimal JSON representation.
// The structure is designed for audit logs and DB storage via adapters.
std::string PortfolioState::to_json() const {
    // Flatten positions into a simple key/value map for lightweight JSON.
    // For a richer schema, replace with a full JSON library in adapters.
    std::map<std::string, std::string> m;
    m["cash_balance"] = std::to_string(cash_balance);
    m["total_equity"] = std::to_string(total_equity);
    m["unrealized_pnl"] = std::to_string(unrealized_pnl);
    m["realized_pnl"] = std::to_string(realized_pnl);
    m["timestamp_ms"] = std::to_string(timestamp_ms);

    // Encode position count; individual positions can be stored elsewhere
    // or serialized as a separate artifact for brevity in logs.
    m["position_count"] = std::to_string(positions.size());
    return utils::to_json(m);
}

// Parse JSON into PortfolioState. Only top-level numeric fields are restored.
PortfolioState PortfolioState::from_json(const std::string& json_str) {
    PortfolioState s;
    auto m = utils::from_json(json_str);
    if (m.count("cash_balance")) s.cash_balance = std::stod(m["cash_balance"]);
    if (m.count("total_equity")) s.total_equity = std::stod(m["total_equity"]);
    if (m.count("unrealized_pnl")) s.unrealized_pnl = std::stod(m["unrealized_pnl"]);
    if (m.count("realized_pnl")) s.realized_pnl = std::stod(m["realized_pnl"]);
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    return s;
}

} // namespace sentio


