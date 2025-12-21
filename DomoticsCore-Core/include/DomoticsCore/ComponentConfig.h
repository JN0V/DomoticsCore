#pragma once

#include <vector>
#include <map>
#include <climits>

namespace DomoticsCore {
namespace Components {

/**
 * Component status enumeration for detailed error reporting
 */
enum class ComponentStatus {
    Success = 0,
    ConfigError,
    HardwareError,
    DependencyError,
    NetworkError,
    MemoryError,
    TimeoutError,
    InvalidState,
    NotSupported
};

/**
 * Convert ComponentStatus to human-readable string
 */
inline const char* statusToString(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::Success: return "Success";
        case ComponentStatus::ConfigError: return "Configuration Error";
        case ComponentStatus::HardwareError: return "Hardware Error";
        case ComponentStatus::DependencyError: return "Dependency Error";
        case ComponentStatus::NetworkError: return "Network Error";
        case ComponentStatus::MemoryError: return "Memory Error";
        case ComponentStatus::TimeoutError: return "Timeout Error";
        case ComponentStatus::InvalidState: return "Invalid State";
        case ComponentStatus::NotSupported: return "Not Supported";
        default: return "Unknown Error";
    }
}

/**
 * Configuration parameter types
 */
enum class ConfigType {
    String,
    Integer,
    Float,
    Boolean,
    IPAddress,
    Port
};

/**
 * Configuration parameter definition
 */
struct ConfigParam {
    String name;
    ConfigType type;
    bool required;
    String defaultValue;
    String description;
    
    // Validation constraints
    int minValue = INT_MIN;
    int maxValue = INT_MAX;
    size_t maxLength = 0;
    std::vector<String> allowedValues;
    
    ConfigParam(const String& n, ConfigType t, bool req = false, 
                const String& def = "", const String& desc = "")
        : name(n), type(t), required(req), defaultValue(def), description(desc) {}
    
    // Fluent interface for constraints
    ConfigParam& min(int minVal) { minValue = minVal; return *this; }
    ConfigParam& max(int maxVal) { maxValue = maxVal; return *this; }
    ConfigParam& length(size_t maxLen) { maxLength = maxLen; return *this; }
    ConfigParam& options(const std::vector<String>& opts) { allowedValues = opts; return *this; }
};

/**
 * Component metadata information
 */
struct ComponentMetadata {
    String name;
    String version;
    String author;
    String description;
    String category;
    std::vector<String> tags;
    
    ComponentMetadata(const String& n = "", const String& v = "1.0.0", 
                     const String& a = "", const String& d = "")
        : name(n), version(v), author(a), description(d) {}
};

/**
 * Configuration validation result
 */
struct ValidationResult {
    ComponentStatus status;
    String errorMessage;
    String parameterName;
    
    ValidationResult(ComponentStatus s = ComponentStatus::Success, 
                    const String& msg = "", const String& param = "")
        : status(s), errorMessage(msg), parameterName(param) {}
    
    bool isValid() const { return status == ComponentStatus::Success; }
    
    String toString() const {
        if (isValid()) return "Valid";
        String result = statusToString(status);
        if (parameterName.length() > 0) {
            result += " (" + parameterName + ")";
        }
        if (errorMessage.length() > 0) {
            result += ": " + errorMessage;
        }
        return result;
    }
};

/**
 * Component configuration base class
 */
class ComponentConfig {
private:
    std::map<String, String> values;
    std::vector<ConfigParam> parameters;
    
public:
    /**
     * Define a configuration parameter
     */
    void defineParameter(const ConfigParam& param) {
        parameters.push_back(param);
        if (!param.defaultValue.isEmpty()) {
            values[param.name] = param.defaultValue;
        }
    }
    
    /**
     * Set configuration value
     */
    void setValue(const String& name, const String& value) {
        values[name] = value;
    }
    
    /**
     * Get configuration value
     */
    String getValue(const String& name, const String& defaultVal = "") const {
        auto it = values.find(name);
        return (it != values.end()) ? it->second : defaultVal;
    }
    
    /**
     * Get configuration value as integer
     */
    int getInt(const String& name, int defaultVal = 0) const {
        String val = getValue(name);
        return val.isEmpty() ? defaultVal : val.toInt();
    }
    
    /**
     * Get configuration value as float
     */
    float getFloat(const String& name, float defaultVal = 0.0f) const {
        String val = getValue(name);
        return val.isEmpty() ? defaultVal : val.toFloat();
    }
    
    /**
     * Get configuration value as boolean
     */
    bool getBool(const String& name, bool defaultVal = false) const {
        String val = getValue(name);
        if (val.isEmpty()) return defaultVal;
        val.toLowerCase();
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }
    
    /**
     * Validate all configuration parameters
     */
    ValidationResult validate() const {
        for (const auto& param : parameters) {
            auto result = validateParameter(param);
            if (!result.isValid()) {
                return result;
            }
        }
        return ValidationResult();
    }
    
    /**
     * Get all defined parameters
     */
    const std::vector<ConfigParam>& getParameters() const {
        return parameters;
    }
    
    /**
     * Check if parameter exists
     */
    bool hasParameter(const String& name) const {
        return values.find(name) != values.end();
    }
    
private:
    /**
     * Validate a single parameter
     */
    ValidationResult validateParameter(const ConfigParam& param) const {
        String value = getValue(param.name);
        
        // Check required parameters
        if (param.required && value.isEmpty()) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Required parameter missing", param.name);
        }
        
        // Skip validation if value is empty and not required
        if (value.isEmpty()) {
            return ValidationResult();
        }
        
        // Type-specific validation
        switch (param.type) {
            case ConfigType::Integer:
                return validateInteger(param, value);
            case ConfigType::Float:
                return validateFloat(param, value);
            case ConfigType::Boolean:
                return validateBoolean(param, value);
            case ConfigType::String:
                return validateString(param, value);
            case ConfigType::IPAddress:
                return validateIPAddress(param, value);
            case ConfigType::Port:
                return validatePort(param, value);
        }
        
        return ValidationResult();
    }
    
    ValidationResult validateInteger(const ConfigParam& param, const String& value) const {
        int intVal = value.toInt();
        if (value != String(intVal)) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Invalid integer format", param.name);
        }
        if (intVal < param.minValue || intVal > param.maxValue) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Value out of range", param.name);
        }
        return ValidationResult();
    }
    
    ValidationResult validateFloat(const ConfigParam& param, const String& value) const {
        float floatVal = value.toFloat();
        if (value != String(floatVal)) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Invalid float format", param.name);
        }
        return ValidationResult();
    }
    
    ValidationResult validateBoolean(const ConfigParam& param, const String& value) const {
        String lower = value;
        lower.toLowerCase();
        if (lower != "true" && lower != "false" && lower != "1" && lower != "0" &&
            lower != "yes" && lower != "no" && lower != "on" && lower != "off") {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Invalid boolean format", param.name);
        }
        return ValidationResult();
    }
    
    ValidationResult validateString(const ConfigParam& param, const String& value) const {
        if (param.maxLength > 0 && value.length() > param.maxLength) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "String too long", param.name);
        }
        if (!param.allowedValues.empty()) {
            bool found = false;
            for (const auto& allowed : param.allowedValues) {
                if (value == allowed) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return ValidationResult(ComponentStatus::ConfigError, 
                                      "Value not in allowed list", param.name);
            }
        }
        return ValidationResult();
    }
    
    ValidationResult validateIPAddress(const ConfigParam& param, const String& value) const {
        // Simple IP address validation
        int parts = 0;
        unsigned int start = 0;
        for (unsigned int i = 0; i <= value.length(); i++) {
            if (i == value.length() || value[i] == '.') {
                if (i == start) {
                    return ValidationResult(ComponentStatus::ConfigError, 
                                          "Invalid IP address format", param.name);
                }
                String part = HAL::substring(value, start, i);
                int num = part.toInt();
                if (num < 0 || num > 255) {
                    return ValidationResult(ComponentStatus::ConfigError, 
                                          "Invalid IP address range", param.name);
                }
                parts++;
                start = i + 1;
            }
        }
        if (parts != 4) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Invalid IP address format", param.name);
        }
        return ValidationResult();
    }
    
    ValidationResult validatePort(const ConfigParam& param, const String& value) const {
        int port = value.toInt();
        if (port < 1 || port > 65535) {
            return ValidationResult(ComponentStatus::ConfigError, 
                                  "Port out of range (1-65535)", param.name);
        }
        return ValidationResult();
    }
};

} // namespace Components
} // namespace DomoticsCore
