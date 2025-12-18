#pragma once

/**
 * @file MockEventBus.h
 * @brief Mock EventBus for isolated unit testing
 * 
 * This mock allows testing components that use EventBus without
 * depending on the real EventBus implementation.
 */

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Recorded event for test verification
 */
struct MockEvent {
    String eventName;
    String payload;  // Serialized for verification
    bool sticky;
};

/**
 * @brief Mock EventBus that records emit/subscribe calls
 */
class MockEventBus {
public:
    // Recorded operations
    static std::vector<MockEvent> emittedEvents;
    static std::map<String, std::vector<std::function<void()>>> subscribers;
    static std::map<String, MockEvent> stickyEvents;
    
    // Emit tracking
    template<typename T>
    static void emit(const String& eventName, const T& data, bool sticky = false) {
        MockEvent ev;
        ev.eventName = eventName;
        ev.payload = String(sizeof(T));  // Just track size for now
        ev.sticky = sticky;
        emittedEvents.push_back(ev);
        
        if (sticky) {
            stickyEvents[eventName] = ev;
        }
        
        // Trigger subscribers
        auto it = subscribers.find(eventName);
        if (it != subscribers.end()) {
            for (auto& callback : it->second) {
                callback();
            }
        }
    }
    
    // Simple string emit for easy testing
    static void emitString(const String& eventName, const String& payload, bool sticky = false) {
        MockEvent ev;
        ev.eventName = eventName;
        ev.payload = payload;
        ev.sticky = sticky;
        emittedEvents.push_back(ev);
        
        if (sticky) {
            stickyEvents[eventName] = ev;
        }
    }
    
    // Subscribe tracking
    static void subscribe(const String& eventName, std::function<void()> callback) {
        subscribers[eventName].push_back(callback);
    }
    
    // Test control methods
    static void reset() {
        emittedEvents.clear();
        subscribers.clear();
        stickyEvents.clear();
    }
    
    // Verification helpers
    static bool wasEmitted(const String& eventName) {
        for (const auto& ev : emittedEvents) {
            if (ev.eventName == eventName) return true;
        }
        return false;
    }
    
    static int getEmitCount(const String& eventName) {
        int count = 0;
        for (const auto& ev : emittedEvents) {
            if (ev.eventName == eventName) count++;
        }
        return count;
    }
    
    static int getTotalEmitCount() { return emittedEvents.size(); }
    
    static bool hasStickyEvent(const String& eventName) {
        return stickyEvents.find(eventName) != stickyEvents.end();
    }
    
    static int getSubscriberCount(const String& eventName) {
        auto it = subscribers.find(eventName);
        return it != subscribers.end() ? it->second.size() : 0;
    }
};

// Static member initialization
std::vector<MockEvent> MockEventBus::emittedEvents;
std::map<String, std::vector<std::function<void()>>> MockEventBus::subscribers;
std::map<String, MockEvent> MockEventBus::stickyEvents;

} // namespace Mocks
} // namespace DomoticsCore
