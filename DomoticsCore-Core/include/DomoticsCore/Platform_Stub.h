#ifndef DOMOTICS_CORE_PLATFORM_STUB_H
#define DOMOTICS_CORE_PLATFORM_STUB_H

/**
 * @file Platform_Stub.h
 * @brief Stub platform utilities for unsupported platforms.
 * 
 * This file provides empty/default implementations for platforms
 * that are not fully supported (AVR, ARM, unknown).
 */

#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <chrono>
#include <thread>

// Define NAN if not available
#ifndef NAN
#define NAN (0.0f/0.0f)
#endif

// Arduino String compatibility for stub platforms
class String {

private:
    std::string data;

public:
    String() = default;
    String(const char* str) : data(str) {}
    String(const std::string& str) : data(str) {}
    String(int value) : data(std::to_string(value)) {}
    String(unsigned int value) : data(std::to_string(value)) {}
    String(long value) : data(std::to_string(value)) {}
    String(unsigned long value) : data(std::to_string(value)) {}
    String(float value) : data(std::to_string(value)) {}
    String(double value) : data(std::to_string(value)) {}
    String(uint32_t value, int format) {
        switch (format) {
            case 16: // HEX
                char hex_buf[16];
                snprintf(hex_buf, sizeof(hex_buf), "%08X", value);
                data = hex_buf;
                break;
            case 10: // DEC
            default:
                data = std::to_string(value);
                break;
        }
    }
    
    const char* c_str() const { return data.c_str(); }
    bool isEmpty() const { return data.empty(); }
    int length() const { return static_cast<int>(data.length()); }
    
    void reserve(int size) { data.reserve(size); }
    
    int toInt() const { return std::stoi(data); }
    float toFloat() const { return std::stof(data); }
    String toLowerCase() const {
        std::string lower = data;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return String(lower);
    }
    String toUpperCase() const {
        std::string upper = data;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        return String(upper);
    }
    
    char operator[](int index) const { return data[index]; }
    
    String& operator+=(const String& other) { data += other.data; return *this; }
    String operator+(const String& other) const { return String(data + other.data); }
    
    // Allow const char* + String
    friend String operator+(const char* lhs, const String& rhs) {
        return String(std::string(lhs) + rhs.data);
    }
    
    bool operator==(const String& other) const { return data == other.data; }
    bool operator!=(const String& other) const { return data != other.data; }
    bool operator<(const String& other) const { return data < other.data; }
    
    operator std::string() const { return data; }
};

// Basic Arduino types for stub platforms
using byte = uint8_t;
using word = uint16_t;

// Arduino constants
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HEX 16
#define DEC 10

// Stub logging macros for native/unsupported platforms
#ifndef log_e
#define log_e(fmt, ...) ((void)0)
#define log_w(fmt, ...) ((void)0)
#define log_i(fmt, ...) ((void)0)
#define log_d(fmt, ...) ((void)0)
#define log_v(fmt, ...) ((void)0)
#endif

namespace DomoticsCore {
namespace HAL {
namespace Platform {

/**
 * @brief Initialize logging system for stub platforms (no-op)
 */
inline void initializeLogging(long baudrate = 115200) {
    (void)baudrate; // No logging on stub platforms
}

/**
 * @brief Check if logging system is ready for stub platforms
 */
inline bool isLoggerReady() {
    return false; // No logging on stub platforms
}

/**
 * @brief Get current milliseconds for stub platforms (native time)
 */
inline unsigned long getMillis() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

/**
 * @brief Delay execution for stub platforms (native sleep)
 */
inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Format chip ID as hexadecimal string for stub platforms
 */
inline String formatChipIdHex() {
    return String("00000000"); // Stub chip ID
}

/**
 * @brief Convert string to uppercase for stub platforms
 */
inline String toUpperCase(const String& str) {
    // Use std::string for stub implementation
    std::string upper = str.c_str();
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return String(upper.c_str());
}

/**
 * @brief Get substring of string for stub platforms
 */
inline String substring(const String& str, int start, int end = -1) {
    std::string s = str.c_str();
    if (end == -1) end = s.length();
    return String(s.substr(start, end - start).c_str());
}

/**
 * @brief Find index of character in string for stub platforms
 */
inline int indexOf(const String& str, char ch) {
    std::string s = str.c_str();
    return static_cast<int>(s.find(ch));
}

/**
 * @brief Check if string starts with prefix for stub platforms
 */
inline bool startsWith(const String& str, const String& prefix) {
    std::string s = str.c_str();
    std::string p = prefix.c_str();
    return s.rfind(p, 0) == 0;
}

/**
 * @brief Check if string ends with suffix for stub platforms
 */
inline bool endsWith(const String& str, const String& suffix) {
    std::string s = str.c_str();
    std::string suf = suffix.c_str();
    if (s.length() < suf.length()) return false;
    return s.compare(s.length() - suf.length(), suf.length(), suf) == 0;
}
inline String getChipModel() {
#if defined(__AVR__)
    return "ATmega";
#elif defined(__arm__) && defined(ARDUINO)
    return "ARM Cortex";
#else
    return "Unknown";
#endif
}

/**
 * @brief Get chip revision (stub - always 0)
 */
inline uint8_t getChipRevision() {
    return 0;
}

/**
 * @brief Get unique chip ID (stub - always 0)
 */
inline uint64_t getChipId() {
    return 0;
}

/**
 * @brief Get free heap memory (stub - always 0)
 */
inline uint32_t getFreeHeap() {
    return 0;
}

/**
 * @brief Get CPU frequency in MHz (stub)
 */
inline uint32_t getCpuFreqMHz() {
#if defined(__AVR__)
    return F_CPU / 1000000UL;
#else
    return 0;
#endif
}

/**
 * @brief Software reset (stub)
 */
inline void restart() {
#if defined(__AVR__)
    asm volatile ("jmp 0");
#endif
}

/**
 * @brief Get chip temperature (stub)
 * @return NAN (temperature sensor not available)
 */
inline float getTemperature() {
    return NAN;
}

/**
 * @brief Get the correct value to turn LED_BUILTIN ON for stub platform
 * @return HIGH (default active-high)
 */
inline int ledBuiltinOn() {
    return HIGH;  // Default: active-high
}

/**
 * @brief Get the correct value to turn LED_BUILTIN OFF for stub platform
 * @return LOW (default active-high)
 */
inline int ledBuiltinOff() {
    return LOW;  // Default: active-high
}

/**
 * @brief Write digital value to pin (stub implementation)
 */
inline void digitalWrite(uint8_t pin, uint8_t value) {
    // Stub implementation - no actual GPIO on native platform
    (void)pin;
    (void)value;
}

/**
 * @brief Set pin mode (stub implementation)
 */
inline void pinMode(uint8_t pin, uint8_t mode) {
    // Stub implementation - no actual GPIO on native platform
    (void)pin;
    (void)mode;
}

/**
 * @brief Write analog/PWM value to pin (stub implementation)
 */
inline void analogWrite(uint8_t pin, int value) {
    // Stub implementation - no actual PWM on native platform
    (void)pin;
    (void)value;
}

/**
 * @brief Map a number from one range to another (stub implementation)
 */
inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

/**
 * @brief Mathematical constant PI for stub platform
 */
constexpr double PI = 3.141592653589793238462643383279502884197169399375105820974944592307816406286;

/**
 * @brief Built-in LED pin number for stub platform
 */
#define LED_BUILTIN 0

/**
 * @brief Check if internal LED (LED_BUILTIN) has inverted logic on stub platform
 * @return false (default active-high)
 */
inline bool isInternalLEDInverted() {
    return false;  // Default: active-high
}

/**
 * @brief SHA256 hash computation (stub - no-op).
 */
class SHA256 {
public:
    SHA256() {}
    void begin() {}
    void update(const uint8_t*, size_t) {}
    void finish(uint8_t* digest) { memset(digest, 0, 32); }
    void abort() {}
    
    static String toHex(const uint8_t* digest, size_t len = 32) {
        static const char* hex = "0123456789abcdef";
        String out;
        out.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            out += hex[(digest[i] >> 4) & 0x0F];
            out += hex[digest[i] & 0x0F];
        }
        return out;
    }
};

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_PLATFORM_STUB_H
