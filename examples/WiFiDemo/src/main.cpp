#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WiFi.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core* core = nullptr;

/**
 * WiFi demonstration component showcasing enhanced WiFi functionality
 */
class WiFiDemoComponent : public IComponent {
private:
    std::unique_ptr<WiFiComponent> wifiManager;
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay scanTimer;
    Utils::NonBlockingDelay reconnectTestTimer;
    int demoPhase;
    bool scanInProgress;
    
public:
    WiFiDemoComponent() : statusTimer(5000), scanTimer(30000), reconnectTestTimer(120000), 
                         demoPhase(0), scanInProgress(false) {
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
        DLOG_I(LOG_CORE, "[WiFiDemo] Demo will cycle through different phases:");
        DLOG_I(LOG_CORE, "[WiFiDemo] - Phase 1: Connection monitoring");
        DLOG_I(LOG_CORE, "[WiFiDemo] - Phase 2: Network scanning");
        DLOG_I(LOG_CORE, "[WiFiDemo] - Phase 3: Reconnection testing");
        
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
        DLOG_I(LOG_CORE, "=== WiFi Status Report ===");
        
        if (wifiManager->isConnected()) {
            DLOG_I(LOG_CORE, "Status: Connected");
            DLOG_I(LOG_CORE, "SSID: %s", wifiManager->getSSID().c_str());
            DLOG_I(LOG_CORE, "IP Address: %s", wifiManager->getLocalIP().c_str());
            DLOG_I(LOG_CORE, "Signal Strength: %d dBm (%s)", 
                   wifiManager->getRSSI(), getSignalQuality(wifiManager->getRSSI()).c_str());
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
        if (!wifiManager->isConnected()) {
            DLOG_I(LOG_CORE, "Skipping network scan - not connected");
            return;
        }
        
        scanInProgress = true;
        DLOG_I(LOG_CORE, "=== Network Scan Demo ===");
        DLOG_I(LOG_CORE, "Scanning for available WiFi networks...");
        
        std::vector<String> networks;
        if (wifiManager->scanNetworks(networks)) {
            DLOG_I(LOG_CORE, "Found %d networks:", networks.size());
            for (size_t i = 0; i < networks.size() && i < 10; i++) {
                DLOG_I(LOG_CORE, "  %d. %s", i + 1, networks[i].c_str());
            }
            if (networks.size() > 10) {
                DLOG_I(LOG_CORE, "  ... and %d more networks", networks.size() - 10);
            }
        } else {
            DLOG_E(LOG_CORE, "Network scan failed");
        }
        
        scanInProgress = false;
    }
    
    void performReconnectionTest() {
        demoPhase++;
        
        DLOG_I(LOG_CORE, "=== Reconnection Test Demo (Phase %d) ===", demoPhase);
        
        if (wifiManager->isConnected()) {
            DLOG_I(LOG_CORE, "Testing reconnection capability...");
            DLOG_I(LOG_CORE, "Triggering manual reconnect (this will briefly disconnect)");
            wifiManager->reconnect();
        } else {
            DLOG_I(LOG_CORE, "Currently disconnected - attempting reconnection");
            wifiManager->reconnect();
        }
        
        // Reset timer for next test
        reconnectTestTimer.setInterval(180000); // 3 minutes for next test
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
    
    core = new Core();
    
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
    DLOG_I(LOG_CORE, "Features demonstrated:");
    DLOG_I(LOG_CORE, "- Non-blocking WiFi connection");
    DLOG_I(LOG_CORE, "- Automatic reconnection");
    DLOG_I(LOG_CORE, "- Network scanning");
    DLOG_I(LOG_CORE, "- Connection status monitoring");
    DLOG_I(LOG_CORE, "- Signal quality assessment");
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
