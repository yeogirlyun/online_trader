#include "common/eod_state.h"
#include <fstream>
#include <iostream>

namespace sentio {

EodStateStore::EodStateStore(std::string path) : state_file_(std::move(path)) {}

std::optional<std::string> EodStateStore::last_eod_date() const {
    std::ifstream in(state_file_);
    if (!in) {
        return std::nullopt;
    }

    std::string line;
    if (std::getline(in, line) && !line.empty()) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            return line.substr(start, end - start + 1);
        }
    }

    return std::nullopt;
}

void EodStateStore::mark_eod_complete(const std::string& et_date) {
    std::ofstream out(state_file_, std::ios::trunc);
    if (!out) {
        std::cerr << "ERROR: Failed to write EOD state to " << state_file_ << "\n";
        std::cerr << "       EOD liquidation completed but NOT recorded - manual intervention required\n";
        return;
    }

    out << et_date << "\n";
    out.flush();

    if (!out) {
        std::cerr << "ERROR: Failed to flush EOD state to disk\n";
    }
}

bool EodStateStore::is_eod_complete(const std::string& et_date) const {
    auto last = last_eod_date();
    return last.has_value() && last.value() == et_date;
}

} // namespace sentio
