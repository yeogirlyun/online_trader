#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

// Simple JSON parsing utilities for basic use cases
// This is a minimal fallback when nlohmann/json is not available

namespace sentio {
namespace json_utils {

/**
 * @brief Simple JSON value type
 */
enum class JsonType {
    STRING,
    NUMBER,
    ARRAY,
    OBJECT,
    BOOLEAN,
    NULL_VALUE
};

/**
 * @brief Simple JSON value class
 */
class JsonValue {
private:
    JsonType type_;
    std::string string_value_;
    double number_value_ = 0.0;
    bool bool_value_ = false;
    std::vector<JsonValue> array_value_;
    std::map<std::string, JsonValue> object_value_;

public:
    JsonValue() : type_(JsonType::NULL_VALUE) {}
    JsonValue(const std::string& value) : type_(JsonType::STRING), string_value_(value) {}
    JsonValue(double value) : type_(JsonType::NUMBER), number_value_(value) {}
    JsonValue(bool value) : type_(JsonType::BOOLEAN), bool_value_(value) {}
    
    JsonType type() const { return type_; }
    
    // String access
    std::string as_string() const { return string_value_; }
    
    // Number access
    double as_double() const { return number_value_; }
    int as_int() const { return static_cast<int>(number_value_); }
    
    // Boolean access
    bool as_bool() const { return bool_value_; }
    
    // Array access
    const std::vector<JsonValue>& as_array() const { return array_value_; }
    void add_to_array(const JsonValue& value) {
        type_ = JsonType::ARRAY;
        array_value_.push_back(value);
    }
    
    // Object access
    const std::map<std::string, JsonValue>& as_object() const { return object_value_; }
    void set_object_value(const std::string& key, const JsonValue& value) {
        type_ = JsonType::OBJECT;
        object_value_[key] = value;
    }
    
    bool has_key(const std::string& key) const {
        return type_ == JsonType::OBJECT && object_value_.find(key) != object_value_.end();
    }
    
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_value;
        if (type_ != JsonType::OBJECT) return null_value;
        auto it = object_value_.find(key);
        return it != object_value_.end() ? it->second : null_value;
    }
    
    // Convenience methods
    std::vector<double> as_double_array() const {
        std::vector<double> result;
        if (type_ == JsonType::ARRAY) {
            for (const auto& val : array_value_) {
                if (val.type() == JsonType::NUMBER) {
                    result.push_back(val.as_double());
                }
            }
        }
        return result;
    }
    
    std::vector<std::string> as_string_array() const {
        std::vector<std::string> result;
        if (type_ == JsonType::ARRAY) {
            for (const auto& val : array_value_) {
                if (val.type() == JsonType::STRING) {
                    result.push_back(val.as_string());
                }
            }
        }
        return result;
    }
};

/**
 * @brief Simple JSON parser for basic metadata files
 * This is a minimal implementation for parsing ML metadata
 */
class SimpleJsonParser {
private:
    std::string json_text_;
    size_t pos_ = 0;
    
    // Forward declarations to break circular dependencies
    JsonValue parse_value();
    JsonValue parse_array();
    JsonValue parse_object();
    
    void skip_whitespace() {
        while (pos_ < json_text_.size() && std::isspace(json_text_[pos_])) {
            pos_++;
        }
    }
    
    // Method declarations (implementations moved to json_utils.cpp to break circular dependencies)
    std::string parse_string();
    double parse_number();

public:
    JsonValue parse(const std::string& json_text);
};

/**
 * @brief Load and parse JSON from file
 */
inline JsonValue load_json_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return JsonValue();
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    SimpleJsonParser parser;
    return parser.parse(buffer.str());
}

} // namespace json_utils
} // namespace sentio
