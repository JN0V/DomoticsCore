#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

// Credentials. Put a `secrets.h` at the repository root — or anywhere on the
// include path — to run this example against a real network without editing,
// and without committing, anything. Build with the root on the include path:
//     PLATFORMIO_BUILD_FLAGS="-I/path/to/DomoticsCore" pio run -e esp8266dev
// Without the file the placeholders below are unchanged.
#if defined(__has_include)
#  if __has_include(<secrets.h>)
#    include <secrets.h>
#  endif
#endif

#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID "YourWifiSSID"
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD "YourWifiPassword"
#endif

Core core;

/**
 * Wifi demonstration component showcasing enhanced Wifi functionality
 */
class WifiDemoComponent : public IComponent {
private:
    std::unique_ptr<WifiComponent> wifiComp;
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay scanTimer;
    Utils::NonBlockingDelay reconnectTestTimer;
    Utils::NonBlockingDelay apModeTimer;
    Utils::NonBlockingDelay staapModeTimer;
    int demoPhase;
    bool scanInProgress;
    bool apModeActive;
    bool apModeTestCompleted;
    bool staapModeActive;
    bool staapModeTestCompleted;
    
public:
    WifiDemoComponent() : statusTimer(5000), scanTimer(15000), reconnectTestTimer(120000), 
                         apModeTimer(30000), staapModeTimer(60000), demoPhase(0), scanInProgress(false), 
                         apModeActive(false), apModeTestCompleted(false),
                         staapModeActive(false), staapModeTestCompleted(false) {
        // Set component metadata
        metadata.name = "WifiDemo";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Wifi component demonstration with connection management";
        metadata.category = "Demo";
        metadata.tags = {"wifi", "demo", "network", "connectivity"};
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "[WifiDemo] Initializing Wifi demonstration component...");
        
        // Credentials come from secrets.h when it is on the include path, and
        // fall back to the placeholders above otherwise.
        String ssid = DC_WIFI_SSID;
        String password = DC_WIFI_PASSWORD;
        
        // Create Wifi manager with credentials
        wifiComp.reset(new WifiComponent(ssid, password));
        
        // Initialize Wifi manager
        ComponentStatus status = wifiComp->begin();
        if (status != ComponentStatus::Success) {
            DLOG_E(LOG_APP, "[WifiDemo] Failed to initialize Wifi manager: %s", 
                         statusToString(status));
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_APP, "[WifiDemo] Wifi manager initialized successfully");
        DLOG_I(LOG_APP, "[WifiDemo] === DEMO PHASES OVERVIEW ===");
        DLOG_I(LOG_APP, "[WifiDemo] Phase 1: Connection monitoring (every 5s)");
        DLOG_I(LOG_APP, "[WifiDemo] Phase 2: Network scanning (every 15s)");
        DLOG_I(LOG_APP, "[WifiDemo] Phase 3: AP mode test (at 30s for 15s)");
        DLOG_I(LOG_APP, "[WifiDemo] Phase 4: Wifi + AP mode test (at 60s for 15s)");
        DLOG_I(LOG_APP, "[WifiDemo] Phase 5: Reconnection testing (every 2min)");
        DLOG_I(LOG_APP, "[WifiDemo] =================================");
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;
        
        // Update Wifi manager
        wifiComp->loop();
        
        // Regular status reporting
        if (statusTimer.isReady()) {
            reportWifiStatus();
        }
        
        // Network scanning demonstration
        if (scanTimer.isReady() && !scanInProgress) {
            performNetworkScan();
        }
        
        // AP mode switching demonstration
        if (apModeTimer.isReady() && !apModeTestCompleted) {
            performAPModeTest();
        }
        
        // STA+AP mode switching demonstration
        if (staapModeTimer.isReady() && !staapModeTestCompleted) {
            performSTAAPModeTest();
        }
        
        // Reconnection testing demonstration
        if (reconnectTestTimer.isReady()) {
            performReconnectionTest();
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "[WifiDemo] Shutting down Wifi demonstration component...");
        
        if (wifiComp) {
            wifiComp->shutdown();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
private:
    void reportWifiStatus() {
        // Determine current demo phase
        String currentPhase = "Phase 1: Connection Monitoring";
        if (!apModeTestCompleted && millis() > 25000) currentPhase = "Phase 3: AP Mode Test";
        else if (!staapModeTestCompleted && millis() > 55000) currentPhase = "Phase 4: STA+AP Mode Test";
        else if (millis() > 15000) currentPhase = "Phase 2: Network Scanning";
        
        DLOG_I(LOG_APP, "=== Wifi Status Report [%s] ===", currentPhase.c_str());
        
        if (wifiComp->isSTAConnected() || wifiComp->isAPEnabled()) {
            String mode = "Station";
            if (wifiComp->isSTAAPMode()) mode = "STA+AP";
            else if (wifiComp->isAPMode()) mode = "AP Only";
            
            DLOG_I(LOG_APP, "Status: Connected (%s mode)", mode.c_str());
            
            if (wifiComp->isSTAAPMode()) {
                // In STA+AP mode, show both connections (station details + AP summary)
                DLOG_I(LOG_APP, "Station SSID: %s", wifiComp->getSSID().c_str());
                DLOG_I(LOG_APP, "Station IP: %s", wifiComp->getLocalIP().c_str());
                DLOG_I(LOG_APP, "Station Signal: %d dBm (%s)", 
                       wifiComp->getRSSI(), getSignalQuality(wifiComp->getRSSI()).c_str());
                DLOG_I(LOG_APP, "AP Info: %s", wifiComp->getAPInfo().c_str());
            } else {
                DLOG_I(LOG_APP, "SSID: %s", wifiComp->getSSID().c_str());
                DLOG_I(LOG_APP, "IP Address: %s", wifiComp->getLocalIP().c_str());
                
                if (!wifiComp->isAPMode()) {
                    DLOG_I(LOG_APP, "Signal Strength: %d dBm (%s)", 
                           wifiComp->getRSSI(), getSignalQuality(wifiComp->getRSSI()).c_str());
                }
                
                if (wifiComp->isAPMode()) {
                    DLOG_I(LOG_APP, "AP Info: %s", wifiComp->getAPInfo().c_str());
                }
            }
            
            DLOG_I(LOG_APP, "MAC Address: %s", wifiComp->getMacAddress().c_str());
        } else if (wifiComp->isConnectionInProgress()) {
            DLOG_I(LOG_APP, "Status: Connecting...");
            DLOG_I(LOG_APP, "Please wait for connection to complete");
        } else {
            DLOG_W(LOG_APP, "Status: Disconnected");
            DLOG_W(LOG_APP, "Detailed status: %s", wifiComp->getDetailedStatus().c_str());
        }
        
        DLOG_I(LOG_APP, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_APP, "Uptime: %lu seconds", millis() / 1000);
    }
    
    void performNetworkScan() {
        // Skip scanning during AP or STA+AP mode tests to avoid conflicts
        if (apModeActive || staapModeActive) {
            return;
        }
        
        // Only scan when not in AP (station-only preferred)
        if (wifiComp->isAPMode()) {
            DLOG_W(LOG_APP, "⚠️ Skipping network scan - AP active");
            return;
        }
        
        DLOG_I(LOG_APP, "=== Phase 2: Network Scanning ===");
        DLOG_I(LOG_APP, "🔍 Scanning for available networks...");
        
        std::vector<String> networks;
        if (!wifiComp->scanNetworks(networks)) {
            DLOG_W(LOG_APP, "❌ Network scan failed");
            return;
        }
        
        if (networks.empty()) {
            DLOG_I(LOG_APP, "📡 No networks found");
            return;
        }
        
        int networkCount = (int)networks.size();
        DLOG_I(LOG_APP, "📡 Found %d networks:", networkCount);
        for (int i = 0; i < networkCount && i < 10; i++) {
            // scanNetworks already returns formatted entries like "SSID (RSSI dBm)"
            DLOG_I(LOG_APP, "  %d: %s", i + 1, networks[i].c_str());
        }
        if (networkCount > 10) {
            DLOG_I(LOG_APP, "  ... and %d more networks", networkCount - 10);
        }
    }
    
    void performReconnectionTest() {
        demoPhase++;
        
        DLOG_I(LOG_APP, "=== Phase 5: Reconnection Test Demo (Cycle %d) ===", demoPhase);
        
        if (wifiComp->isSTAConnected() && !wifiComp->isAPMode()) {
            DLOG_I(LOG_APP, "🔄 Testing reconnection capability...");
            DLOG_I(LOG_APP, "⚡ Triggering manual reconnect (brief disconnect expected)");
            wifiComp->reconnect();
        } else if (wifiComp->isAPMode()) {
            DLOG_I(LOG_APP, "⏭️  Skipping reconnection test - in AP mode");
        } else {
            DLOG_I(LOG_APP, "🔗 Currently disconnected - attempting reconnection");
            wifiComp->reconnect();
        }
        
        // Reset timer for next test
        reconnectTestTimer.setInterval(120000); // 2 minutes for next test
    }
    
    void performAPModeTest() {
        DLOG_I(LOG_APP, "=== Phase 3: AP Mode Test ===");
        
        if (!apModeActive) {
            DLOG_I(LOG_APP, "🔄 Testing AP-only mode...");
            DLOG_I(LOG_APP, "📡 Enabling AP mode for 15 seconds");
            DLOG_I(LOG_APP, "📶 AP Name: WifiDemo_AP");
            DLOG_I(LOG_APP, "🔐 AP Password: demo12345");
            DLOG_I(LOG_APP, "🌐 Connect to: http://192.168.4.1");
            
            // Disable Wifi and enable AP only
            wifiComp->enableWifi(false);
            if (wifiComp->enableAP("WifiDemo_AP", "demo12345")) {
                DLOG_I(LOG_APP, "✅ Successfully enabled AP-only mode");
                DLOG_I(LOG_APP, "📊 AP Info: %s", wifiComp->getAPInfo().c_str());
                apModeActive = true;
                apModeTimer.setInterval(15000); // 15 seconds in AP mode
            } else {
                DLOG_E(LOG_APP, "❌ Failed to enable AP mode");
                apModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_APP, "🔄 AP mode test completed, returning to Wifi mode");
            
            // Disable AP and re-enable Wifi
            wifiComp->disableAP();
            if (wifiComp->enableWifi(true)) {
                DLOG_I(LOG_APP, "✅ Successfully returned to Wifi mode");
                DLOG_I(LOG_APP, "🔗 Wifi connection will resume automatically");
            } else {
                DLOG_E(LOG_APP, "❌ Failed to return to Wifi mode");
            }
            
            apModeActive = false;
            apModeTestCompleted = true;
        }
    }
    
    void performSTAAPModeTest() {
        DLOG_I(LOG_APP, "=== Phase 4: Wifi + AP Mode Test ===");
        
        if (!staapModeActive) {
            DLOG_I(LOG_APP, "🔄 Testing Wifi + AP simultaneous mode...");
            DLOG_I(LOG_APP, "📡 Enabling both Wifi and AP for 15 seconds");
            DLOG_I(LOG_APP, "📶 AP Name: WifiDemo_Both");
            DLOG_I(LOG_APP, "🔐 AP Password: demo12345");
            DLOG_I(LOG_APP, "🌐 AP: http://192.168.4.1 + Wifi connection maintained");
            
            // Enable both Wifi and AP - component handles STA+AP mode internally
            wifiComp->enableWifi(true);
            if (wifiComp->enableAP("WifiDemo_Both", "demo12345")) {
                DLOG_I(LOG_APP, "✅ Successfully enabled Wifi + AP mode");
                DLOG_I(LOG_APP, "📊 AP Info: %s", wifiComp->getAPInfo().c_str());
                staapModeActive = true;
                staapModeTimer.setInterval(15000); // 15 seconds in dual mode
            } else {
                DLOG_E(LOG_APP, "❌ Failed to enable Wifi + AP mode");
                staapModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_APP, "🔄 Wifi + AP test completed, returning to Wifi-only mode");
            
            // Disable AP, keep Wifi enabled
            if (wifiComp->disableAP()) {
                DLOG_I(LOG_APP, "✅ Successfully returned to Wifi-only mode");
                DLOG_I(LOG_APP, "🔗 Wifi connection maintained");
            } else {
                DLOG_E(LOG_APP, "❌ Failed to return to Wifi-only mode");
            }
            
            staapModeActive = false;
            staapModeTestCompleted = true;
        }
    }
    
    String getSignalQuality(int32_t rssi) const {
        if (rssi > -50) return "Excellent";
        else if (rssi > -60) return "Good";
        else if (rssi > -70) return "Fair";
        else if (rssi > -80) return "Poor";
        else return "Very Poor";
    }
};

void setup() {
    // Create core with custom device name
    CoreConfig config;
    config.deviceName = "WifiDemoDevice";
    config.logLevel = 3; // INFO level
    
    // Core initialized
    
    // Add Wifi demonstration component
    DLOG_I(LOG_APP, "Adding Wifi demonstration component...");
    core.addComponent(std::unique_ptr<WifiDemoComponent>(new WifiDemoComponent()));
    
    DLOG_I(LOG_APP, "Starting core with %d components...", core.getComponentCount());
    
    if (!core.begin(config)) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_APP, "=== DomoticsCore Wifi Demo Ready ===");
    DLOG_I(LOG_APP, "IMPORTANT: Update Wifi credentials in main.cpp before use!");
    DLOG_I(LOG_APP, "🚀 Features demonstrated:");
    DLOG_I(LOG_APP, "📡 - Non-blocking Wifi connection");
    DLOG_I(LOG_APP, "🔄 - Automatic reconnection");
    DLOG_I(LOG_APP, "🔍 - Network scanning");
    DLOG_I(LOG_APP, "📶 - Access Point mode");
    DLOG_I(LOG_APP, "🌐 - Wifi + AP simultaneous mode");
    DLOG_I(LOG_APP, "📊 - Connection status monitoring");
    DLOG_I(LOG_APP, "📈 - Signal quality assessment");
}

void loop() {
    core.loop();
    
    // Status reporting using non-blocking delay
    static Utils::NonBlockingDelay statusTimer(60000); // 1 minute
    if (statusTimer.isReady()) {
        DLOG_I(LOG_SYSTEM, "=== Wifi Demo System Status ===");
        DLOG_I(LOG_SYSTEM, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_SYSTEM, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_SYSTEM, "Wifi demo running...");
    }
}
