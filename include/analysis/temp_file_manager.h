#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdio>

namespace sentio {
namespace analysis {

/**
 * @brief RAII-based temporary file manager
 * 
 * Ensures temporary files are cleaned up even if exceptions occur.
 * Thread-safe for concurrent usage.
 */
class TempFileManager {
public:
    /**
     * @brief Constructor
     * @param temp_directory Directory for temporary files (default: /tmp)
     * @param keep_files If true, don't delete files on destruction (for debugging)
     */
    explicit TempFileManager(
        const std::string& temp_directory = "/tmp",
        bool keep_files = false
    );
    
    /**
     * @brief Destructor - automatically cleans up all temporary files
     */
    ~TempFileManager();
    
    // Disable copying
    TempFileManager(const TempFileManager&) = delete;
    TempFileManager& operator=(const TempFileManager&) = delete;
    
    // Allow moving
    TempFileManager(TempFileManager&& other) noexcept;
    TempFileManager& operator=(TempFileManager&& other) noexcept;
    
    /**
     * @brief Create a unique temporary file path
     * @param prefix Prefix for the filename
     * @param extension File extension (e.g., ".jsonl", ".csv")
     * @return Absolute path to the temporary file
     */
    std::string create_temp_file(
        const std::string& prefix,
        const std::string& extension = ".tmp"
    );
    
    /**
     * @brief Manually cleanup all temporary files
     * Called automatically by destructor
     */
    void cleanup();
    
    /**
     * @brief Get count of managed temporary files
     */
    size_t file_count() const { return temp_files_.size(); }
    
    /**
     * @brief Get list of all temporary files
     */
    const std::vector<std::string>& get_files() const { return temp_files_; }

private:
    std::vector<std::string> temp_files_;
    std::string temp_directory_;
    bool keep_files_;
    static std::mutex mutex_;  // Thread safety
};

} // namespace analysis
} // namespace sentio

