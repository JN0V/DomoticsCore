#pragma once

/**
 * @file IPAddress_Stub.h
 * @brief Stub IPAddress for native tests
 */

#include <cstdint>

namespace DomoticsCore {
namespace HAL {

/**
 * @class IPAddress
 * @brief Stub IP address representation for native tests
 */
class IPAddress {
private:
    uint32_t address = 0;

public:
    IPAddress() = default;
    IPAddress(uint32_t addr) : address(addr) {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        address = (a << 24) | (b << 16) | (c << 8) | d;
    }

    operator uint32_t() const { return address; }

    bool operator==(const IPAddress& other) const { return address == other.address; }
    bool operator!=(const IPAddress& other) const { return address != other.address; }

    uint8_t operator[](int index) const {
        switch (index) {
            case 0: return (address >> 24) & 0xFF;
            case 1: return (address >> 16) & 0xFF;
            case 2: return (address >> 8) & 0xFF;
            case 3: return address & 0xFF;
            default: return 0;
        }
    }

    uint32_t toUInt32() const { return address; }
};

} // namespace HAL
} // namespace DomoticsCore
