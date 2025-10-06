#include "core/data_io.h"
#include "common/types.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <iostream>

namespace sentio {

std::vector<std::vector<double>> load_detector_outputs(const std::string& path) {
    std::vector<std::vector<double>> outputs;

    if (std::filesystem::is_directory(path)) {
        for (int i = 0; i < 8; ++i) {
            std::string detector_file = path + "/detector_" + std::to_string(i) + ".csv";
            if (!std::filesystem::exists(detector_file)) {
                throw std::runtime_error("Detector file not found: " + detector_file);
            }

            std::vector<double> detector_output;
            std::ifstream file(detector_file);
            std::string line;

            if (std::getline(file, line)) {
                if (line.find("probability") == std::string::npos) {
                    detector_output.push_back(std::stod(line));
                }
            }

            while (std::getline(file, line)) {
                detector_output.push_back(std::stod(line));
            }

            outputs.push_back(detector_output);
        }
    } else {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open detector outputs file: " + path);
        }

        std::string line;
        std::getline(file, line); // skip header

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::vector<double> row;
            std::string value;
            while (std::getline(iss, value, ',')) {
                row.push_back(std::stod(value));
            }
            if (row.size() != 8) {
                throw std::runtime_error("Invalid detector output format: expected 8 values per row");
            }
            if (outputs.empty()) outputs.resize(8);
            for (size_t i = 0; i < 8; ++i) outputs[i].push_back(row[i]);
        }
    }

    std::cout << "Loaded detector outputs: " << outputs.size() << " detectors, "
              << (outputs.empty() ? 0 : outputs[0].size()) << " samples" << std::endl;
    return outputs;
}

std::vector<Bar> load_market_data(const std::string& path) {
    std::vector<Bar> bars;

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".csv") {
        std::cerr << "DEBUG [data_io]: Loading CSV from " << path << "\n";
        std::cerr.flush();
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open market data file: " + path);
        }
        std::string line;
        std::getline(file, line); // header
        std::cerr << "DEBUG [data_io]: Header: " << line << "\n";
        std::cerr.flush();
        int line_num = 1;
        while (std::getline(file, line)) {
            line_num++;
            std::istringstream iss(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(iss, field, ',')) fields.push_back(field);
            if (fields.size() < 7) continue;
            Bar bar{};
            try {
                bar.timestamp_ms = std::stoll(fields[0]);
                bar.symbol = fields[1];
                bar.open = std::stod(fields[2]);
                bar.high = std::stod(fields[3]);
                bar.low = std::stod(fields[4]);
                bar.close = std::stod(fields[5]);
                bar.volume = std::stod(fields[6]);
            } catch (const std::exception& e) {
                std::cerr << "ERROR: Failed to parse CSV line " << line_num << ": " << e.what() << "\n";
                std::cerr << "Line: " << line << "\n";
                std::cerr << "Fields: [";
                for (size_t i = 0; i < fields.size(); ++i) {
                    if (i > 0) std::cerr << ", ";
                    std::cerr << "\"" << fields[i] << "\"";
                }
                std::cerr << "]\n";
                throw;
            }
            // Defer push_back to assign derived fields after parsing
            bars.push_back(bar);
        }
    } else if (path.size() >= 4 && path.substr(path.size() - 4) == ".bin") {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open binary market data: " + path);
        }
        size_t count{};
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        bars.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            Bar bar{};
            file.read(reinterpret_cast<char*>(&bar.timestamp_ms), sizeof(bar.timestamp_ms));
            size_t symbol_len{};
            file.read(reinterpret_cast<char*>(&symbol_len), sizeof(symbol_len));
            bar.symbol.resize(symbol_len);
            file.read(&bar.symbol[0], symbol_len);
            file.read(reinterpret_cast<char*>(&bar.open), sizeof(bar.open));
            file.read(reinterpret_cast<char*>(&bar.high), sizeof(bar.high));
            file.read(reinterpret_cast<char*>(&bar.low), sizeof(bar.low));
            file.read(reinterpret_cast<char*>(&bar.close), sizeof(bar.close));
            file.read(reinterpret_cast<char*>(&bar.volume), sizeof(bar.volume));
            bars.push_back(bar);
        }
    } else {
        throw std::runtime_error("Unsupported file format: " + path);
    }

    // Populate immutable id and derived fields
    for (size_t i = 0; i < bars.size(); ++i) {
        Bar& b = bars[i];
        b.bar_id = utils::generate_bar_id(b.timestamp_ms, b.symbol);
        b.sequence_num = static_cast<uint32_t>(i);
        b.block_num = static_cast<uint16_t>(i / STANDARD_BLOCK_SIZE);
        // Derive date_str (YYYY-MM-DD) from timestamp
        std::string ts = utils::ms_to_timestamp(b.timestamp_ms);
        if (ts.size() >= 10) {
            b.date_str = ts.substr(0, 10);
        } else {
            b.date_str.clear();
        }
    }

    std::cout << "Loaded " << bars.size() << " market data bars" << std::endl;
    return bars;
}

void save_detector_outputs(const std::vector<std::vector<double>>& outputs,
                          const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create output file: " + path);
    }
    file << "sgo1,sgo2,sgo3,sgo4,sgo5,sgo6,sgo7,awr\n";
    if (outputs.empty() || outputs[0].empty()) return;
    size_t n_samples = outputs[0].size();
    for (size_t i = 0; i < n_samples; ++i) {
        for (size_t j = 0; j < outputs.size(); ++j) {
            file << outputs[j][i];
            if (j < outputs.size() - 1) file << ",";
        }
        file << "\n";
    }
    std::cout << "Saved detector outputs to " << path << std::endl;
}

nlohmann::json load_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open JSON file: " + path);
    }
    nlohmann::json j;
    file >> j;
    return j;
}

void save_json(const nlohmann::json& data, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create JSON file: " + path);
    }
    file << data.dump(2);
}

} // namespace sentio


