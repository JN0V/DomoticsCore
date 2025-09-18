#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WiFi.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

/**
 * WiFi demonstration component showcasing enhanced WiFi functionality
 */
class WiFiDemoComponent : public IComponent {
private:
    std::unique_ptr<WiFiComponent> wifiManager;
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
    WiFiDemoComponent() : statusTimer(5000), scanTimer(15000), reconnectTestTimer(120000), 
                         apModeTimer(30000), staapModeTimer(60000), demoPhase(0), scanInProgress(false), 
                         apModeActive(false), apModeTestCompleted(false),
                         staapModeActive(false), staapModeTestCompleted(false) {
        // Set component metadata
        metadata.name = "WiFiDemo";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "WiFi component demonstration with connection management";
        metadata.category = "Demo";
        metadata.tags = {"wifi", "demo", "network", "connectivity"};
    }
    
    String getName() const override {
        return metadata.name;
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "[WiFiDemo] Initializing WiFi demonstration component...");
        
        // NOTE: Replace with your actual WiFi credentials
        String ssid = "YourWiFiSSID";
        String password = "YourWiFiPassword";
        
        // Create WiFi manager with credentials
        wifiManager.reset(new WiFiComponent(ssid, password));
        
        // Initialize WiFi manager
        ComponentStatus status = wifiManager->begin();
        if (status != ComponentStatus::Success) {
            DLOG_E(LOG_CORE, "[WiFiDemo] Failed to initialize WiFi manager: %s", 
                         statusToString(status));
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_CORE, "[WiFiDemo] WiFi manager initialized successfully");
        DLOG_I(LOG_CORE, "[WiFiDemo] === DEMO PHASES OVERVIEW ===");
        DLOG_I(LOG_CORE, "[WiFiDemo] Phase 1: Connection monitoring (every 5s)");
        DLOG_I(LOG_CORE, "[WiFiDemo] Phase 2: Network scanning (every 15s)");
        DLOG_I(LOG_CORE, "[WiFiDemo] Phase 3: AP mode test (at 30s for 15s)");
        DLOG_I(LOG_CORE, "[WiFiDemo] Phase 4: WiFi + AP mode test (at 60s for 15s)");
        DLOG_I(LOG_CORE, "[WiFiDemo] Phase 5: Reconnection testing (every 2min)");
        DLOG_I(LOG_CORE, "[WiFiDemo] =================================");
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;
        
        // Update WiFi manager
        wifiManager->loop();
        
        // Regular status reporting
        if (statusTimer.isReady()) {
            reportWiFiStatus();
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
        DLOG_I(LOG_CORE, "[WiFiDemo] Shutting down WiFi demonstration component...");
        
        if (wifiManager) {
            wifiManager->shutdown();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
private:
    void reportWiFiStatus() {
        // Determine current demo phase
        String currentPhase = "Phase 1: Connection Monitoring";
        if (!apModeTestCompleted && millis() > 25000) currentPhase = "Phase 3: AP Mode Test";
        else if (!staapModeTestCompleted && millis() > 55000) currentPhase = "Phase 4: STA+AP Mode Test";
        else if (millis() > 15000) currentPhase = "Phase 2: Network Scanning";
        
        DLOG_I(LOG_CORE, "=== WiFi Status Report [%s] ===", currentPhase.c_str());
        
        if (wifiManager->isConnected()) {
            String mode = "Station";
            if (wifiManager->isSTAAPMode()) mode = "STA+AP";
            else if (wifiManager->isAPMode()) mode = "AP Only";
            
            DLOG_I(LOG_CORE, "Status: Connected (%s mode)", mode.c_str());
            
            if (wifiManager->isSTAAPMode()) {
                // In STA+AP mode, show both connections
                DLOG_I(LOG_CORE, "Station SSID: %s", WiFi.SSID().c_str());
                DLOG_I(LOG_CORE, "Station IP: %s", WiFi.localIP().toString().c_str());
                DLOG_I(LOG_CORE, "Station Signal: %d dBm (%s)", 
                       wifiManager->getRSSI(), getSignalQuality(wifiManager->getRSSI()).c_str());
                DLOG_I(LOG_CORE, "AP SSID: %s", WiFi.softAPSSID().c_str());
                DLOG_I(LOG_CORE, "AP IP: %s", WiFi.softAPIP().toString().c_str());
                DLOG_I(LOG_CORE, "AP Clients: %d", WiFi.softAPgetStationNum());
            } else {
                DLOG_I(LOG_CORE, "SSID: %s", wifiManager->getSSID().c_str());
                DLOG_I(LOG_CORE, "IP Address: %s", wifiManager->getLocalIP().c_str());
                
                if (!wifiManager->isAPMode()) {
                    DLOG_I(LOG_CORE, "Signal Strength: %d dBm (%s)", 
                           wifiManager->getRSSI(), getSignalQuality(wifiManager->getRSSI()).c_str());
                }
                
                if (wifiManager->isAPMode()) {
                    DLOG_I(LOG_CORE, "AP Info: %s", wifiManager->getAPInfo().c_str());
                }
            }
            
            DLOG_I(LOG_CORE, "MAC Address: %s", wifiManager->getMacAddress().c_str());
        } else if (wifiManager->isConnectionInProgress()) {
            DLOG_I(LOG_CORE, "Status: Connecting...");
            DLOG_I(LOG_CORE, "Please wait for connection to complete");
        } else {
            DLOG_W(LOG_CORE, "Status: Disconnected");
            DLOG_W(LOG_CORE, "Detailed status: %s", wifiManager->getDetailedStatus().c_str());
        }
        
        DLOG_I(LOG_CORE, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_CORE, "Uptime: %lu seconds", millis() / 1000);
    }
    
    void performNetworkScan() {
        // Skip scanning during AP or STA+AP mode tests to avoid conflicts
        if (apModeActive || staapModeActive) {
            return;
        }
        
        // Only scan in station mode to avoid conflicts
        if (WiFi.getMode() != WIFI_STA) {
            DLOG_W(LOG_CORE, "⚠️ Skipping network scan - not in station mode");
            return;
        }
        
        DLOG_I(LOG_CORE, "=== Phase 2: Network Scanning ===");
        DLOG_I(LOG_CORE, "🔍 Scanning for available networks...");
        
        // Clear any previous scan results first
        WiFi.scanDelete();
        delay(100);
        
        int networkCount = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
        
        if (networkCount == -2) {
            DLOG_W(LOG_CORE, "❌ Network scan in progress, try again later");
        } else if (networkCount == -1) {
            DLOG_W(LOG_CORE, "❌ Network scan failed");
        } else if (networkCount == 0) {
            DLOG_I(LOG_CORE, "📡 No networks found");
        } else {
            DLOG_I(LOG_CORE, "📡 Found %d networks:", networkCount);
            for (int i = 0; i < networkCount && i < 10; i++) {
                String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
                DLOG_I(LOG_CORE, "  %d: %s (%d dBm) [%s]", 
                       i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), security.c_str());
            }
            if (networkCount > 10) {
                DLOG_I(LOG_CORE, "  ... and %d more networks", networkCount - 10);
            }
        }
        
        // Clean up scan results
        WiFi.scanDelete();
    }
    
    void performReconnectionTest() {
        demoPhase++;
        
        DLOG_I(LOG_CORE, "=== Phase 5: Reconnection Test Demo (Cycle %d) ===", demoPhase);
        
        if (wifiManager->isConnected() && !wifiManager->isAPMode()) {
            DLOG_I(LOG_CORE, "🔄 Testing reconnection capability...");
            DLOG_I(LOG_CORE, "⚡ Triggering manual reconnect (brief disconnect expected)");
            wifiManager->reconnect();
        } else if (wifiManager->isAPMode()) {
            DLOG_I(LOG_CORE, "⏭️  Skipping reconnection test - in AP mode");
        } else {
            DLOG_I(LOG_CORE, "🔗 Currently disconnected - attempting reconnection");
            wifiManager->reconnect();
        }
        
        // Reset timer for next test
        reconnectTestTimer.setInterval(120000); // 2 minutes for next test
    }
    
    void performAPModeTest() {
        DLOG_I(LOG_CORE, "=== Phase 3: AP Mode Test ===");
        
        if (!apModeActive) {
            DLOG_I(LOG_CORE, "🔄 Testing AP-only mode...");
            DLOG_I(LOG_CORE, "📡 Enabling AP mode for 15 seconds");
            DLOG_I(LOG_CORE, "📶 AP Name: WiFiDemo_AP");
            DLOG_I(LOG_CORE, "🔐 AP Password: demo12345");
            DLOG_I(LOG_CORE, "🌐 Connect to: http://192.168.4.1");
            
            // Disable WiFi and enable AP only
            wifiManager->enableWiFi(false);
            if (wifiManager->enableAP("WiFiDemo_AP", "demo12345")) {
                DLOG_I(LOG_CORE, "✅ Successfully enabled AP-only mode");
                DLOG_I(LOG_CORE, "📊 AP Info: %s", wifiManager->getAPInfo().c_str());
                apModeActive = true;
                apModeTimer.setInterval(15000); // 15 seconds in AP mode
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to enable AP mode");
                apModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_CORE, "🔄 AP mode test completed, returning to WiFi mode");
            
            // Disable AP and re-enable WiFi
            wifiManager->disableAP();
            if (wifiManager->enableWiFi(true)) {
                DLOG_I(LOG_CORE, "✅ Successfully returned to WiFi mode");
                DLOG_I(LOG_CORE, "🔗 WiFi connection will resume automatically");
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to return to WiFi mode");
            }
            
            apModeActive = false;
            apModeTestCompleted = true;
        }
    }
    
    void performSTAAPModeTest() {
        DLOG_I(LOG_CORE, "=== Phase 4: WiFi + AP Mode Test ===");
        
        if (!staapModeActive) {
            DLOG_I(LOG_CORE, "🔄 Testing WiFi + AP simultaneous mode...");
            DLOG_I(LOG_CORE, "📡 Enabling both WiFi and AP for 15 seconds");
            DLOG_I(LOG_CORE, "📶 AP Name: WiFiDemo_Both");
            DLOG_I(LOG_CORE, "🔐 AP Password: demo12345");
            DLOG_I(LOG_CORE, "🌐 AP: http://192.168.4.1 + WiFi connection maintained");
            
            // Enable both WiFi and AP - component handles STA+AP mode internally
            wifiManager->enableWiFi(true);
            if (wifiManager->enableAP("WiFiDemo_Both", "demo12345")) {
                DLOG_I(LOG_CORE, "✅ Successfully enabled WiFi + AP mode");
                DLOG_I(LOG_CORE, "📊 AP Info: %s", wifiManager->getAPInfo().c_str());
                staapModeActive = true;
                staapModeTimer.setInterval(15000); // 15 seconds in dual mode
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to enable WiFi + AP mode");
                staapModeTestCompleted = true;
            }
        } else {
            DLOG_I(LOG_CORE, "🔄 WiFi + AP test completed, returning to WiFi-only mode");
            
            // Disable AP, keep WiFi enabled
            if (wifiManager->disableAP()) {
                DLOG_I(LOG_CORE, "✅ Successfully returned to WiFi-only mode");
                DLOG_I(LOG_CORE, "🔗 WiFi connection maintained");
            } else {
                DLOG_E(LOG_CORE, "❌ Failed to return to WiFi-only mode");
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
    config.deviceName = "WiFiDemoDevice";
    config.logLevel = 3; // INFO level
    
    core.reset(new Core());
    
    // Add WiFi demonstration component
    DLOG_I(LOG_CORE, "Adding WiFi demonstration component...");
    core->addComponent(std::unique_ptr<WiFiDemoComponent>(new WiFiDemoComponent()));
    
    DLOG_I(LOG_CORE, "Starting core with %d components...", core->getComponentCount());
    
    if (!core->begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "=== DomoticsCore WiFi Demo Ready ===");
    DLOG_I(LOG_CORE, "IMPORTANT: Update WiFi credentials in main.cpp before use!");
    DLOG_I(LOG_CORE, "🚀 Features demonstrated:");
    DLOG_I(LOG_CORE, "📡 - Non-blocking WiFi connection");
    DLOG_I(LOG_CORE, "🔄 - Automatic reconnection");
    DLOG_I(LOG_CORE, "🔍 - Network scanning");
    DLOG_I(LOG_CORE, "📶 - Access Point mode");
    DLOG_I(LOG_CORE, "🌐 - WiFi + AP simultaneous mode");
    DLOG_I(LOG_CORE, "📊 - Connection status monitoring");
    DLOG_I(LOG_CORE, "📈 - Signal quality assessment");
}

void loop() {
    if (core) {
        core->loop();
    }
    
    // Status reporting using non-blocking delay
    static Utils::NonBlockingDelay statusTimer(60000); // 1 minute
    if (statusTimer.isReady()) {
        DLOG_I(LOG_SYSTEM, "=== WiFi Demo System Status ===");
        DLOG_I(LOG_SYSTEM, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_SYSTEM, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_SYSTEM, "WiFi demo running...");
    }
}
