#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

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
    
    String getName() const override {
        return metadata.name;
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "[WifiDemo] Initializing Wifi demonstration component...");
        
        // NOTE: Replace with your actual Wifi credentials
        String ssid = "YourWifiSSID";
        String password = "YourWifiPassword";
        
        // Create Wifi manager with credentials
        wifiComp.reset(new WifiComponent(ssid, password));
        
        // Initialize Wifi manager
        ComponentStatus status = wifiComp->begin();
        if (status != ComponentStatus::Success) {
            DLOG_E(LOG_CORE, "[WifiDemo] Failed to initialize Wifi manager: %s", 
                         statusToString(status));
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_CORE, "[WifiDemo] Wifi manager initialized successfully");
        DLOG_I(LOG_CORE, "[WifiDemo] === DEMO PHASES OVERVIEW ===");
        DLOG_I(LOG_CORE, "[WifiDemo] Phase 1: Connection monitoring (every 5s)");
        DLOG_I(LOG_CORE, "[WifiDemo] Phase 2: Network scanning (every 15s)");
        DLOG_I(LOG_CORE, "[WifiDemo] Phase 3: AP mode test (at 30s for 15s)");
        DLOG_I(LOG_CORE, "[WifiDemo] Phase 4: Wifi + AP mode test (at 60s for 15s)");
        DLOG_I(LOG_CORE, "[WifiDemo] Phase 5: Reconnection testing (every 2min)");
        DLOG_I(LOG_CORE, "[WifiDemo] =================================");
        
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
        DLOG_I(LOG_CORE, "[WifiDemo] Shutting down Wifi demonstration component...");
        
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
        
        DLOG_I(LOG_CORE, "=== Wifi Status Report [%s] ===", currentPhase.c_str());
        
        if (wifiComp->isConnected()) {
            String mode = "Station";
            if (wifiComp->isSTAAPMode()) mode = "STA+AP";
            else if (wifiComp->isAPMode()) mode = "AP Only";
            
            DLOG_I(LOG_CORE, "Status: Connected (%s mode)", mode.c_str());
            
            if (wifiComp->isSTAAPMode()) {
                // In STA+AP mode, show both connections (station details + AP summary)
                DLOG_I(LOG_CORE, "Station SSID: %s", wifiComp->getSSID().c_str());
                DLOG_I(LOG_CORE, "Station IP: %s", wifiComp->getLocalIP().c_str());
                DLOG_I(LOG_CORE, "Station Signal: %d dBm (%s)", 
                       wifiComp->getRSSI(), getSignalQuality(wifiComp->getRSSI()).c_str());
                DLOG_I(LOG_CORE, "AP Info: %s", wifiComp->getAPInfo().c_str());
            } else {
                DLOG_I(LOG_CORE, "SSID: %s", wifiComp->getSSID().c_str());
                DLOG_I(LOG_CORE, "IP Address: %s", wifiComp->getLocalIP().c_str());
                
                if (!wifiComp->isAPMode()) {
                    DLOG_I(LOG_CORE, "Signal Strength: %d dBm (%s)", 
                           wifiComp->getRSSI(), getSignalQuality(wifiComp->getRSSI()).c_str());
                }
                
                if (wifiComp->isAPMode()) {
                    DLOG_I(LOG_CORE, "AP Info: %s", wifiComp->getAPInfo().c_str());
                }
            }
            
            DLOG_I(LOG_CORE, "MAC Address: %s", wifiComp->getMacAddress().c_str());
        } else if (wifiComp->isConnectionInProgress()) {
            DLOG_I(LOG_CORE, "Status: Connecting...");
            DLOG_I(LOG_CORE, "Please wait for connection to complete");
        } else {
            DLOG_W(LOG_CORE, "Status: Disconnected");
            DLOG_W(LOG_CORE, "Detailed status: %s", wifiComp->getDetailedStatus().c_str());
        }
        
        DLOG_I(LOG_CORE, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_CORE, "Uptime: %lu seconds", millis() / 1000);
    }
    
    void performNetworkScan() {
        // Skip scanning during AP or STA+AP mode tests to avoid conflicts
        if (apModeActive || staapModeActive) {
            return;
        }
        
        // Only scan when not in AP (station-only preferred)
        if (wifiComp->isAPMode()) {
            DLOG_W(LOG_CORE, "⚠️ Skipping network scan - AP active");
            return;
        }
        
        DLOG_I(LOG_CORE, "=== Phase 2: Network Scanning ===");
        DLOG_I(LOG_CORE, "🔍 Scanning for available networks...");
        
        std::vector<String> networks;
        if (!wifiComp->scanNetworks(networks)) {
            DLOG_W(LOG_CORE, "❌ Network scan failed");
            return;
        }
        
        if (networks.empty()) {
            DLOG_I(LOG_CORE, "📡 No networks found");
            return;
        }
        
        int networkCount = (int)networks.size();
        DLOG_I(LOG_CORE, "📡 Found %d networks:", networkCount);
        for (int i = 0; i < networkCount && i < 10; i++) {
            // scanNetworks already returns formatted entries like "SSID (RSSI dBm)"
            DLOG_I(LOG_CORE, "  %d: %s", i + 1, networks[i].c_str());
        }
        if (networkCount > 10) {
            DLOG_I(LOG_CORE, "  ... and %d more networks", networkCount - 10);
        }
    }
    
    void performReconnectionTest() {
        demoPhase++;
        
        DLOG_I(LOG_CORE, "=== Phase 5: Reconnection Test Demo (Cycle %d) ===", demoPhase);
        
        if (wifiComp->isConnected() && !wifiComp->isAPMode()) {
            DLOG_I(LOG_CORE, "🔄 Testing reconnection capability...");
            DLOG_I(LOG_CORE, "⚡ Triggering manual reconnect (brief disconnect expected)");
            wifiComp->reconnect();
        } else if (wifiComp->isAPMode()) {
            DLOG_I(LOG_CORE, "⏭️  Skipping reconnection test - in AP mode");
        } else {
            DLOG_I(LOG_CORE, "🔗 Currently disconnected - attempting reconnection");
            wifiComp->reconnect();
        }
        
        // Reset timer for next test
        reconnectTestTimer.setInterval(120000); // 2 minutes for next test
    }
    
    void performAPModeTest() {
        DLOG_I(LOG_CORE, "=== Phase 3: AP Mode Test ===");
        
        if (!apModeActive) {
            DLOG_I(LOG_CORE, "🔄 Testing AP-only mode...");
            DLOG_I(LOG_CORE, "📡 Enabling AP mode for 15 seconds");
            DLOG_I(LOG_CORE, "📶 AP Name: WifiDemo_AP");
            DLOG_I(LOG_CORE, "🔐 AP Password: demo12345");
            DLOG_I(LOG_CORE, "🌐 Connect to: http://192.168.4.1");
            
            // Disable Wifi and enable AP only
            wifiComp->enableWifi(false);
            if (wifiComp->enableAP("WifiDemo_AP", "demo12345")) {
                DLOG_I(LOG_CORE, "✅ Successfully enabled AP-only mode");
                DLOG_I(LOG_CORE, "📊 AP Info: %s", wifiComp->getAPInfo().c_str());
                apModeActive = true;
                apModeTimer.setInterval(15000); // 15 seconds in AP mode
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to enable AP mode");
                apModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_CORE, "🔄 AP mode test completed, returning to Wifi mode");
            
            // Disable AP and re-enable Wifi
            wifiComp->disableAP();
            if (wifiComp->enableWifi(true)) {
                DLOG_I(LOG_CORE, "✅ Successfully returned to Wifi mode");
                DLOG_I(LOG_CORE, "🔗 Wifi connection will resume automatically");
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to return to Wifi mode");
            }
            
            apModeActive = false;
            apModeTestCompleted = true;
        }
    }
    
    void performSTAAPModeTest() {
        DLOG_I(LOG_CORE, "=== Phase 4: Wifi + AP Mode Test ===");
        
        if (!staapModeActive) {
            DLOG_I(LOG_CORE, "🔄 Testing Wifi + AP simultaneous mode...");
            DLOG_I(LOG_CORE, "📡 Enabling both Wifi and AP for 15 seconds");
            DLOG_I(LOG_CORE, "📶 AP Name: WifiDemo_Both");
            DLOG_I(LOG_CORE, "🔐 AP Password: demo12345");
            DLOG_I(LOG_CORE, "🌐 AP: http://192.168.4.1 + Wifi connection maintained");
            
            // Enable both Wifi and AP - component handles STA+AP mode internally
            wifiComp->enableWifi(true);
            if (wifiComp->enableAP("WifiDemo_Both", "demo12345")) {
                DLOG_I(LOG_CORE, "✅ Successfully enabled Wifi + AP mode");
                DLOG_I(LOG_CORE, "📊 AP Info: %s", wifiComp->getAPInfo().c_str());
                staapModeActive = true;
                staapModeTimer.setInterval(15000); // 15 seconds in dual mode
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to enable Wifi + AP mode");
                staapModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_CORE, "🔄 Wifi + AP test completed, returning to Wifi-only mode");
            
            // Disable AP, keep Wifi enabled
            if (wifiComp->disableAP()) {
                DLOG_I(LOG_CORE, "✅ Successfully returned to Wifi-only mode");
                DLOG_I(LOG_CORE, "🔗 Wifi connection maintained");
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to return to Wifi-only mode");
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
    DLOG_I(LOG_CORE, "Adding Wifi demonstration component...");
    core.addComponent(std::unique_ptr<WifiDemoComponent>(new WifiDemoComponent()));
    
    DLOG_I(LOG_CORE, "Starting core with %d components...", core.getComponentCount());
    
    if (!core.begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "=== DomoticsCore Wifi Demo Ready ===");
    DLOG_I(LOG_CORE, "IMPORTANT: Update Wifi credentials in main.cpp before use!");
    DLOG_I(LOG_CORE, "🚀 Features demonstrated:");
    DLOG_I(LOG_CORE, "📡 - Non-blocking Wifi connection");
    DLOG_I(LOG_CORE, "🔄 - Automatic reconnection");
    DLOG_I(LOG_CORE, "🔍 - Network scanning");
    DLOG_I(LOG_CORE, "📶 - Access Point mode");
    DLOG_I(LOG_CORE, "🌐 - Wifi + AP simultaneous mode");
    DLOG_I(LOG_CORE, "📊 - Connection status monitoring");
    DLOG_I(LOG_CORE, "📈 - Signal quality assessment");
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
