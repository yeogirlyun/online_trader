#include "common/json_utils.h"
#include <cctype>

namespace sentio {
namespace json_utils {

// Implementation of SimpleJsonParser methods
// Moved here to break circular dependencies

std::string SimpleJsonParser::parse_string() {
    if (pos_ >= json_text_.size() || json_text_[pos_] != '"') {
        return "";
    }
    pos_++; // Skip opening quote
    
    std::string result;
    while (pos_ < json_text_.size() && json_text_[pos_] != '"') {
        if (json_text_[pos_] == '\\' && pos_ + 1 < json_text_.size()) {
            pos_++; // Skip escape character
            switch (json_text_[pos_]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += json_text_[pos_]; break;
            }
        } else {
            result += json_text_[pos_];
        }
        pos_++;
    }
    
    if (pos_ < json_text_.size()) {
        pos_++; // Skip closing quote
    }
    
    return result;
}

double SimpleJsonParser::parse_number() {
    std::string number_str;
    while (pos_ < json_text_.size() && 
           (std::isdigit(json_text_[pos_]) || json_text_[pos_] == '.' || 
            json_text_[pos_] == '-' || json_text_[pos_] == '+' || 
            json_text_[pos_] == 'e' || json_text_[pos_] == 'E')) {
        number_str += json_text_[pos_];
        pos_++;
    }
    return std::stod(number_str);
}

JsonValue SimpleJsonParser::parse_array() {
    JsonValue array_value;
    pos_++; // Skip opening bracket
    
    skip_whitespace();
    if (pos_ < json_text_.size() && json_text_[pos_] == ']') {
        pos_++; // Skip closing bracket
        return array_value;
    }
    
    while (pos_ < json_text_.size()) {
        JsonValue element = parse_value();  // No circular dependency - forward declared
        array_value.add_to_array(element);
        
        skip_whitespace();
        if (pos_ >= json_text_.size()) break;
        
        if (json_text_[pos_] == ']') {
            pos_++;
            break;
        } else if (json_text_[pos_] == ',') {
            pos_++;
            skip_whitespace();
        }
    }
    
    return array_value;
}

JsonValue SimpleJsonParser::parse_object() {
    JsonValue object_value;
    pos_++; // Skip opening brace
    
    skip_whitespace();
    if (pos_ < json_text_.size() && json_text_[pos_] == '}') {
        pos_++; // Skip closing brace
        return object_value;
    }
    
    while (pos_ < json_text_.size()) {
        skip_whitespace();
        std::string key = parse_string();
        
        skip_whitespace();
        if (pos_ < json_text_.size() && json_text_[pos_] == ':') {
            pos_++; // Skip colon
        }
        
        skip_whitespace();
        JsonValue value = parse_value();  // No circular dependency - forward declared
        object_value.set_object_value(key, value);
        
        skip_whitespace();
        if (pos_ >= json_text_.size()) break;
        
        if (json_text_[pos_] == '}') {
            pos_++;
            break;
        } else if (json_text_[pos_] == ',') {
            pos_++;
            skip_whitespace();
        }
    }
    
    return object_value;
}

JsonValue SimpleJsonParser::parse_value() {
    skip_whitespace();
    if (pos_ >= json_text_.size()) {
        return JsonValue();
    }
    
    char c = json_text_[pos_];
    if (c == '"') {
        return JsonValue(parse_string());
    } else if (c == '[') {
        return parse_array();  // No circular dependency - forward declared
    } else if (c == '{') {
        return parse_object();  // No circular dependency - forward declared
    } else if (std::isdigit(c) || c == '-') {
        return JsonValue(parse_number());
    } else if (pos_ + 4 <= json_text_.size() && json_text_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return JsonValue(true);
    } else if (pos_ + 5 <= json_text_.size() && json_text_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return JsonValue(false);
    } else if (pos_ + 4 <= json_text_.size() && json_text_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return JsonValue();
    }
    
    return JsonValue();
}

JsonValue SimpleJsonParser::parse(const std::string& json_text) {
    json_text_ = json_text;
    pos_ = 0;
    return parse_value();
}

} // namespace json_utils
} // namespace sentio
