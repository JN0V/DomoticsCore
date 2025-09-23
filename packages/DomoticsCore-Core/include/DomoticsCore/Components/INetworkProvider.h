#pragma once

#include <Arduino.h>

namespace DomoticsCore {
namespace Components {

/**
 * Network provider interface for abstracting network connectivity
 * Allows WebUI and other components to work with any network transport
 * (WiFi, Ethernet, cellular, etc.) without tight coupling
 */
class INetworkProvider {
public:
    virtual ~INetworkProvider() = default;
    
    /**
     * Check if network is connected
     * @return true if connected, false otherwise
     */
    virtual bool isConnected() const = 0;
    
    /**
     * Get local IP address
     * @return IP address as string, empty if not connected
     */
    virtual String getLocalIP() const = 0;
    
    /**
     * Get network type identifier
     * @return Network type (e.g., "WiFi", "Ethernet", "Cellular")
     */
    virtual String getNetworkType() const = 0;
    
    /**
     * Get human-readable connection status
     * @return Status string (e.g., "Connected", "Disconnected", "Connecting")
     */
    virtual String getConnectionStatus() const = 0;
    
    /**
     * Get detailed network information as JSON
     * @return JSON string with network-specific details
     */
    virtual String getNetworkInfo() const = 0;
    
    /**
     * Optional: Set callback for connection state changes
     * @param callback Function to call when connection state changes
     */
    virtual void setConnectionCallback(std::function<void(bool connected)> callback) {
        // Default implementation does nothing
        (void)callback; // Suppress unused parameter warning
    }
    
    /**
     * Optional: Get signal strength (if applicable)
     * @return Signal strength in dBm, 0 if not applicable
     */
    virtual int32_t getSignalStrength() const {
        return 0; // Default: no signal strength info
    }
    
    /**
     * Optional: Get MAC address
     * @return MAC address as string, empty if not available
     */
    virtual String getMacAddress() const {
        return ""; // Default: no MAC address
    }
};

} // namespace Components
} // namespace DomoticsCore
