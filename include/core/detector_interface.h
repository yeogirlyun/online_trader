#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace sentio {

// Forward declaration from common types
struct Bar;

// Base detector interface
class IDetector {
public:
    virtual ~IDetector() = default;

    // Process a bar and return probability [0, 1]
    virtual double process(const ::sentio::Bar& bar) = 0;

    // Reset internal state
    virtual void reset() = 0;

    // Get detector name
    virtual std::string get_name() const = 0;

    // Check if detector wants to abstain
    virtual bool should_abstain() const { return false; }
};

// Factory function declaration
std::unique_ptr<IDetector> create_detector(
    const std::string& type,
    const nlohmann::json& config);

} // namespace sentio





