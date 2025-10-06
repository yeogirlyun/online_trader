#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "common/types.h"

namespace sentio {

class XGBFeatureSet {
public:
    virtual ~XGBFeatureSet() = default;

    // Identification
    virtual const std::string& name() const = 0;                // e.g., "8Detector", "25Intraday"
    virtual size_t featureCount() const = 0;                    // N
    virtual const std::vector<std::string>& featureNames() const = 0; // ordered names

    // Runtime
    virtual void reset() = 0;
    virtual void update(const ::sentio::Bar& bar) = 0;
    virtual bool isReady() const = 0;
    virtual void extract(std::vector<float>& outFeatures) const = 0; // size == featureCount()

    // FNV-1a checksum over comma-separated featureNames
    uint64_t computeChecksum() const {
        uint64_t h = 14695981039346656037ULL;  // FNV-1a 64-bit offset basis
        for (size_t i = 0; i < featureNames().size(); ++i) {
            const auto& nm = featureNames()[i];
            for (unsigned char c : nm) { h ^= c; h *= 1099511628211ULL; }
            h ^= static_cast<unsigned char>(','); h *= 1099511628211ULL;
        }
        return h;
    }
};

} // namespace sentio





