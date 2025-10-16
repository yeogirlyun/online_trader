#include "live/state_persistence.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <thread>

namespace sentio {

namespace fs = std::filesystem;

StatePersistence::StatePersistence(const std::string& state_dir)
    : state_dir_(state_dir)
    , primary_file_(state_dir + "/trading_state.json")
    , backup_file_(state_dir + "/trading_state.backup.json")
    , temp_file_(state_dir + "/trading_state.tmp.json")
    , lock_file_(state_dir + "/.state.lock")
    , lock_fd_(-1) {

    // Create state directory if it doesn't exist
    try {
        fs::create_directories(state_dir);
    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Failed to create state dir: " << e.what() << "\n";
    }
}

nlohmann::json StatePersistence::TradingState::to_json() const {
    nlohmann::json j;
    j["psm_state"] = static_cast<int>(psm_state);
    j["bars_held"] = bars_held;
    j["entry_equity"] = entry_equity;
    j["last_bar_timestamp"] = last_bar_timestamp;
    j["last_bar_time_str"] = last_bar_time_str;
    j["session_id"] = session_id;
    j["save_timestamp"] = save_timestamp;
    j["save_count"] = save_count;

    nlohmann::json positions_json = nlohmann::json::array();
    for (const auto& pos : positions) {
        nlohmann::json p;
        p["symbol"] = pos.symbol;
        p["quantity"] = pos.quantity;
        p["avg_entry_price"] = pos.avg_entry_price;
        p["entry_timestamp"] = pos.entry_timestamp;
        positions_json.push_back(p);
    }
    j["positions"] = positions_json;

    // Calculate and add checksum (excluding checksum field itself)
    j["checksum"] = calculate_checksum();

    return j;
}

StatePersistence::TradingState StatePersistence::TradingState::from_json(const nlohmann::json& j) {
    TradingState state;
    state.psm_state = static_cast<PositionStateMachine::State>(j.value("psm_state", 0));
    state.bars_held = j.value("bars_held", 0);
    state.entry_equity = j.value("entry_equity", 100000.0);
    state.last_bar_timestamp = j.value("last_bar_timestamp", 0ULL);
    state.last_bar_time_str = j.value("last_bar_time_str", "");
    state.session_id = j.value("session_id", "");
    state.save_timestamp = j.value("save_timestamp", 0ULL);
    state.save_count = j.value("save_count", 0);
    state.checksum = j.value("checksum", "");

    if (j.contains("positions")) {
        for (const auto& p : j["positions"]) {
            PositionDetail pos;
            pos.symbol = p.value("symbol", "");
            pos.quantity = p.value("quantity", 0.0);
            pos.avg_entry_price = p.value("avg_entry_price", 0.0);
            pos.entry_timestamp = p.value("entry_timestamp", 0ULL);
            state.positions.push_back(pos);
        }
    }

    return state;
}

std::string StatePersistence::TradingState::calculate_checksum() const {
    // Create string representation of critical fields
    std::stringstream ss;
    ss << static_cast<int>(psm_state) << "|" << bars_held << "|" << entry_equity << "|"
       << last_bar_timestamp << "|" << positions.size();

    for (const auto& pos : positions) {
        ss << "|" << pos.symbol << ":" << pos.quantity << ":" << pos.avg_entry_price;
    }

    // Calculate SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    std::string str = ss.str();
    SHA256_Update(&sha256, str.c_str(), str.length());
    SHA256_Final(hash, &sha256);

    // Convert to hex string
    std::stringstream hex_ss;
    hex_ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hex_ss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }

    return hex_ss.str();
}

bool StatePersistence::TradingState::validate_checksum() const {
    return checksum == calculate_checksum();
}

bool StatePersistence::save_state(const TradingState& state) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Acquire file lock for cross-process safety
    if (!acquire_file_lock()) {
        std::cerr << "[STATE_PERSIST] Failed to acquire file lock for save\n";
        return false;
    }

    try {
        // Update save metadata
        TradingState state_to_save = state;
        auto now = std::chrono::system_clock::now();
        state_to_save.save_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        state_to_save.save_count++;

        nlohmann::json j = state_to_save.to_json();

        // Step 1: Write to temp file
        if (!write_atomic(temp_file_, j)) {
            std::cerr << "[STATE_PERSIST] Failed to write temp file\n";
            release_file_lock();
            return false;
        }

        // Step 2: Backup current primary to timestamped file
        if (fs::exists(primary_file_)) {
            std::string backup_name = generate_backup_filename();
            try {
                fs::copy_file(primary_file_, backup_name,
                             fs::copy_options::overwrite_existing);
            } catch (const std::exception& e) {
                std::cerr << "[STATE_PERSIST] Failed to create timestamped backup: " << e.what() << "\n";
                // Non-fatal - continue
            }
        }

        // Step 3: Move current primary to backup
        if (fs::exists(primary_file_)) {
            try {
                fs::rename(primary_file_, backup_file_);
            } catch (const std::exception& e) {
                std::cerr << "[STATE_PERSIST] Failed to rotate to backup: " << e.what() << "\n";
                // Try to continue anyway
            }
        }

        // Step 4: Move temp to primary (atomic on most filesystems)
        fs::rename(temp_file_, primary_file_);

        // Step 5: Clean up old backups
        cleanup_old_backups();

        // Release file lock
        release_file_lock();

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Save failed: " << e.what() << "\n";
        release_file_lock();
        return false;
    }
}

std::optional<StatePersistence::TradingState> StatePersistence::load_state() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Acquire file lock for cross-process safety
    if (!acquire_file_lock()) {
        std::cerr << "[STATE_PERSIST] Failed to acquire file lock for load\n";
        return std::nullopt;
    }

    try {
        // Try primary file first
        if (auto state = load_from_file(primary_file_)) {
            if (state->validate_checksum()) {
                std::cout << "[STATE_PERSIST] ✓ Loaded state from primary file\n";
                release_file_lock();
                return state;
            }
            std::cerr << "[STATE_PERSIST] ⚠️  Primary file checksum invalid\n";
        }

        // Try backup file
        if (auto state = load_from_file(backup_file_)) {
            if (state->validate_checksum()) {
                std::cout << "[STATE_PERSIST] ✓ Loaded state from backup file\n";
                release_file_lock();
                return state;
            }
            std::cerr << "[STATE_PERSIST] ⚠️  Backup file checksum invalid\n";
        }

        // Try recovery from timestamped backups
        auto recovered_state = recover_from_backup();
        release_file_lock();
        return recovered_state;

    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Load failed: " << e.what() << "\n";
        release_file_lock();
        return std::nullopt;
    }
}

std::optional<StatePersistence::TradingState> StatePersistence::load_from_file(
    const std::string& filepath) {

    if (!fs::exists(filepath)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;
        file.close();

        return TradingState::from_json(j);

    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Failed to load " << filepath << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

std::optional<StatePersistence::TradingState> StatePersistence::recover_from_backup() {
    // Find all backup files
    std::vector<fs::path> backup_files;

    try {
        for (const auto& entry : fs::directory_iterator(state_dir_)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("trading_state_") == 0 &&
                entry.path().extension() == ".json") {
                backup_files.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Failed to scan backup directory: " << e.what() << "\n";
        return std::nullopt;
    }

    if (backup_files.empty()) {
        std::cerr << "[STATE_PERSIST] No backup files found\n";
        return std::nullopt;
    }

    // Sort by modification time (newest first)
    std::sort(backup_files.begin(), backup_files.end(),
              [](const fs::path& a, const fs::path& b) {
                  return fs::last_write_time(a) > fs::last_write_time(b);
              });

    // Try each backup until we find a valid one
    for (const auto& backup_path : backup_files) {
        if (auto state = load_from_file(backup_path.string())) {
            if (state->validate_checksum()) {
                std::cout << "[STATE_PERSIST] ✓ Recovered state from backup: "
                         << backup_path.filename() << "\n";
                return state;
            }
        }
    }

    std::cerr << "[STATE_PERSIST] ❌ All backup files failed validation\n";
    return std::nullopt;
}

bool StatePersistence::write_atomic(const std::string& filepath, const nlohmann::json& data) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << data.dump(2);
        file.flush();
        file.close();

        return file.good();

    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Write failed: " << e.what() << "\n";
        return false;
    }
}

std::string StatePersistence::generate_backup_filename() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << state_dir_ << "/trading_state_"
       << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << ".json";
    return ss.str();
}

void StatePersistence::cleanup_old_backups(int keep_count) {
    std::vector<fs::path> backup_files;

    try {
        for (const auto& entry : fs::directory_iterator(state_dir_)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("trading_state_") == 0 &&
                entry.path().extension() == ".json") {
                backup_files.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[STATE_PERSIST] Failed to scan for cleanup: " << e.what() << "\n";
        return;
    }

    if (backup_files.size() <= static_cast<size_t>(keep_count)) {
        return;
    }

    // Sort by modification time (oldest first)
    std::sort(backup_files.begin(), backup_files.end(),
              [](const fs::path& a, const fs::path& b) {
                  return fs::last_write_time(a) < fs::last_write_time(b);
              });

    // Remove oldest files
    int to_remove = backup_files.size() - keep_count;
    for (int i = 0; i < to_remove; ++i) {
        try {
            fs::remove(backup_files[i]);
        } catch (const std::exception& e) {
            std::cerr << "[STATE_PERSIST] Failed to remove old backup: " << e.what() << "\n";
        }
    }
}

bool StatePersistence::acquire_file_lock(int timeout_ms) {
    // Open or create lock file
    lock_fd_ = open(lock_file_.c_str(), O_CREAT | O_RDWR, 0644);
    if (lock_fd_ < 0) {
        std::cerr << "[STATE_PERSIST] Failed to open lock file: " << lock_file_ << "\n";
        return false;
    }

    // Try to acquire exclusive lock with timeout
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (flock(lock_fd_, LOCK_EX | LOCK_NB) == 0) {
            return true;  // Lock acquired
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
            close(lock_fd_);
            lock_fd_ = -1;
            std::cerr << "[STATE_PERSIST] Failed to acquire lock within timeout\n";
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void StatePersistence::release_file_lock() {
    if (lock_fd_ >= 0) {
        flock(lock_fd_, LOCK_UN);
        close(lock_fd_);
        lock_fd_ = -1;
    }
}

} // namespace sentio
