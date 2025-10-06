#include "analysis/temp_file_manager.h"
#include "common/utils.h"
#include <iostream>

namespace sentio {
namespace analysis {

// Static mutex initialization
std::mutex TempFileManager::mutex_;

TempFileManager::TempFileManager(
    const std::string& temp_directory,
    bool keep_files
) : temp_directory_(temp_directory),
    keep_files_(keep_files) {
}

TempFileManager::~TempFileManager() {
    cleanup();
}

TempFileManager::TempFileManager(TempFileManager&& other) noexcept
    : temp_files_(std::move(other.temp_files_)),
      temp_directory_(std::move(other.temp_directory_)),
      keep_files_(other.keep_files_) {
    other.temp_files_.clear();  // Prevent other from cleaning up
}

TempFileManager& TempFileManager::operator=(TempFileManager&& other) noexcept {
    if (this != &other) {
        cleanup();  // Clean up our current files
        temp_files_ = std::move(other.temp_files_);
        temp_directory_ = std::move(other.temp_directory_);
        keep_files_ = other.keep_files_;
        other.temp_files_.clear();
    }
    return *this;
}

std::string TempFileManager::create_temp_file(
    const std::string& prefix,
    const std::string& extension
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate unique filename
    std::string filename = temp_directory_ + "/" + 
                          prefix + "_" + 
                          utils::generate_run_id("temp") + 
                          extension;
    
    temp_files_.push_back(filename);
    return filename;
}

void TempFileManager::cleanup() {
    if (keep_files_) {
        std::cout << "[TempFileManager] Keeping " << temp_files_.size() 
                  << " temporary files for debugging" << std::endl;
        for (const auto& file : temp_files_) {
            std::cout << "  - " << file << std::endl;
        }
        return;
    }
    
    for (const auto& file : temp_files_) {
        if (std::remove(file.c_str()) != 0) {
            // Log error but don't throw - we're likely in destructor
            std::cerr << "[TempFileManager] Warning: Failed to delete " 
                     << file << std::endl;
        }
    }
    
    temp_files_.clear();
}

} // namespace analysis
} // namespace sentio

