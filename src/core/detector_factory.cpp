#include "core/detector_interface.h"
#include "detectors/sgo_detector.h"
#include "detectors/awr_detector.h"
#include <stdexcept>

namespace sentio {

std::unique_ptr<IDetector> create_detector(
    const std::string& type,
    const nlohmann::json& config) {
    if (type == "sgo1" || type == "sgo2" || type == "sgo3" ||
        type == "sgo4" || type == "sgo5" || type == "sgo6" || type == "sgo7") {
        return std::make_unique<SGODetector>(type, config);
    } else if (type == "awr") {
        return std::make_unique<AWRDetector>(config);
    }
    throw std::runtime_error("Unknown detector type: " + type);
}

} // namespace sentio





