#include "strategy/ml_strategy_base.h"
#include <filesystem>
#include <thread>
#include <iostream>

namespace sentio {

bool MLStrategyBase::initialize_ml_base(const std::string& strategy_name) {
    utils::log_info("Loading " + strategy_name + " model and metadata...");
    
    // Load metadata first to get configuration
    MLBaseConfig base_config;
    if (!load_metadata_base(get_metadata_path(), base_config)) {
        utils::log_error("Failed to load " + strategy_name + " metadata");
        return false;
    }
    
    // Load the TorchScript model
    if (!load_model_base(get_model_path(), strategy_name)) {
        utils::log_error("Failed to load " + strategy_name + " model");
        return false;
    }
    
    // Clear common history buffers
    clear_history_buffers();
    
    // Allow derived classes to initialize their specific components
    if (!initialize_strategy_specific()) {
        utils::log_error("Failed to initialize " + strategy_name + " specific components");
        return false;
    }
    
    utils::log_info(strategy_name + " Strategy initialized successfully");
    return true;
}

bool MLStrategyBase::load_model_base(const std::string& model_path, const std::string& strategy_name) {
#ifdef TORCH_AVAILABLE
    try {
        utils::log_info("Loading TorchScript model from: " + model_path);
        
        // FIX #2: Threading Issues - Force single-threaded LibTorch
        torch::set_num_threads(1);
        torch::set_num_interop_threads(1);
        
        model_ = torch::jit::load(model_path);
        model_.eval();
        
        // Force model to CPU to avoid device issues
        model_.to(torch::kCPU);
        
        // Test a dummy forward pass to validate model
        size_t seq_len = get_sequence_length();
        int feature_count = get_feature_count(); // Get from derived strategy
        
        auto dummy_input = torch::randn({1, static_cast<int>(seq_len), feature_count}, torch::kCPU);
        
        // Test with timeout protection
        std::atomic<bool> dummy_completed{false};
        std::exception_ptr dummy_exception = nullptr;
        torch::jit::IValue dummy_output;
        
        std::thread dummy_thread([&]() {
            try {
                dummy_output = model_.forward({dummy_input});
                dummy_completed = true;
            } catch (...) {
                dummy_exception = std::current_exception();
                dummy_completed = true;
            }
        });
        
        // Wait for dummy inference with 10 second timeout
        auto dummy_start = std::chrono::steady_clock::now();
        while (!dummy_completed && 
               std::chrono::steady_clock::now() - dummy_start < std::chrono::seconds(10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!dummy_completed) {
            utils::log_error("Model validation timeout - model may be broken");
            dummy_thread.detach();
            model_loaded_ = false;
            return false;
        }
        
        dummy_thread.join();
        
        if (dummy_exception) {
            std::rethrow_exception(dummy_exception);
        }
        
        model_loaded_ = true;
        utils::log_info("TorchScript " + strategy_name + " model loaded and validated successfully");
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load TorchScript " + strategy_name + " model: " + std::string(e.what()));
        return false;
    }
#else
    utils::log_warning("LibTorch not available - " + strategy_name + " strategy will return neutral signals");
    model_loaded_ = false;
    return false;
#endif
}

bool MLStrategyBase::load_metadata_base(const std::string& metadata_path, MLBaseConfig& config) {
    try {
        utils::log_info("Loading metadata from: " + metadata_path);
        
        auto metadata = json_utils::load_json_file(metadata_path);
        if (metadata.type() == json_utils::JsonType::NULL_VALUE) {
            utils::log_error("Failed to parse metadata file: " + metadata_path);
            return false;
        }
        
        // Parse common metadata fields
        if (metadata.has_key("seq_len")) {
            config.sequence_length = metadata["seq_len"].as_int();
        } else if (metadata.has_key("sequence_length")) {
            config.sequence_length = metadata["sequence_length"].as_int();
        }
        
        if (metadata.has_key("feature_names")) {
            config.feature_names = metadata["feature_names"].as_string_array();
            config.feature_count = config.feature_names.size();
        } else if (metadata.has_key("feature_dim")) {
            config.feature_count = metadata["feature_dim"].as_int();
        }
        
        if (metadata.has_key("mean") || metadata.has_key("means") || metadata.has_key("scaler_mean")) {
            std::string means_key;
            if (metadata.has_key("mean")) means_key = "mean";
            else if (metadata.has_key("means")) means_key = "means";
            else if (metadata.has_key("scaler_mean")) means_key = "scaler_mean";
            config.feature_means = metadata[means_key].as_double_array();
        }
        
        if (metadata.has_key("std") || metadata.has_key("scales") || metadata.has_key("scaler_scale")) {
            std::string scales_key;
            if (metadata.has_key("std")) scales_key = "std";
            else if (metadata.has_key("scales")) scales_key = "scales";
            else if (metadata.has_key("scaler_scale")) scales_key = "scaler_scale";
            config.feature_scales = metadata[scales_key].as_double_array();
        }
        
        // Allow derived classes to handle strategy-specific metadata
        apply_metadata_config(metadata);
        
        utils::log_info("Metadata loaded successfully with " + 
                       std::to_string(config.feature_count) + " features, " + 
                       std::to_string(config.sequence_length) + " sequence length");
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("Failed to load metadata: " + std::string(e.what()));
        return false;
    }
}

void MLStrategyBase::clear_history_buffers() {
    feature_buffer_.clear();
    utils::log_info("History buffers cleared");
}

} // namespace sentio
