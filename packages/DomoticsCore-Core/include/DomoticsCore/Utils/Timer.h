#pragma once

#include <Arduino.h>

namespace DomoticsCore {
namespace Utils {

/**
 * Non-blocking delay utility class
 * Provides timing functionality without blocking execution
 */
class NonBlockingDelay {
private:
    unsigned long previousMillis;
    unsigned long interval;
    bool enabled;
    
public:
    /**
     * Constructor
     * @param intervalMs Delay interval in milliseconds (default: 1000ms)
     */
    NonBlockingDelay(unsigned long intervalMs = 1000) 
        : previousMillis(0), interval(intervalMs), enabled(true) {}
    
    /**
     * Check if the delay period has elapsed
     * @return true if ready, false otherwise
     */
    bool isReady() {
        if (!enabled) return false;
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            return true;
        }
        return false;
    }
    
    /**
     * Reset the timer to current time
     */
    void reset() { 
        previousMillis = millis(); 
    }
    
    /**
     * Set new interval
     * @param intervalMs New interval in milliseconds
     */
    void setInterval(unsigned long intervalMs) { 
        interval = intervalMs; 
    }
    
    /**
     * Get current interval
     * @return Current interval in milliseconds
     */
    unsigned long getInterval() const { 
        return interval; 
    }
    
    /**
     * Enable the timer
     */
    void enable() { 
        enabled = true; 
    }
    
    /**
     * Disable the timer
     */
    void disable() { 
        enabled = false; 
    }
    
    /**
     * Check if timer is enabled
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const { 
        return enabled; 
    }
    
    /**
     * Get remaining time until next trigger
     * @return Remaining milliseconds (0 if ready or disabled)
     */
    unsigned long remaining() const {
        if (!enabled) return 0;
        unsigned long elapsed = millis() - previousMillis;
        return (elapsed >= interval) ? 0 : (interval - elapsed);
    }
    
    /**
     * Get elapsed time since last trigger
     * @return Elapsed milliseconds
     */
    unsigned long elapsed() const {
        return millis() - previousMillis;
    }
};

} // namespace Utils
} // namespace DomoticsCore
