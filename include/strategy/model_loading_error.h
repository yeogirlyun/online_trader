#pragma once

#include <stdexcept>
#include <string>

namespace sentio {

/**
 * Exception thrown when model loading or validation fails
 * This ensures model failures are explicit rather than silent
 */
class ModelLoadingError : public std::runtime_error {
public:
    explicit ModelLoadingError(const std::string& what) 
        : std::runtime_error("Model Loading Error: " + what) {}
    
    explicit ModelLoadingError(const std::string& model_path, const std::string& details)
        : std::runtime_error("Model Loading Error [" + model_path + "]: " + details) {}
};

/**
 * Exception thrown when model inference fails during runtime
 */
class ModelInferenceError : public std::runtime_error {
public:
    explicit ModelInferenceError(const std::string& what)
        : std::runtime_error("Model Inference Error: " + what) {}
        
    explicit ModelInferenceError(const std::string& model_path, const std::string& details)
        : std::runtime_error("Model Inference Error [" + model_path + "]: " + details) {}
};

} // namespace sentio
