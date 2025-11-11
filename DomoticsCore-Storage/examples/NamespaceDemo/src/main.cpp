/**
 * @file main.cpp
 * @brief Storage Namespace Demo
 * 
 * Demonstrates using multiple Storage components with different namespaces
 * to isolate application data from configuration data.
 */

#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/Logger.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

#define LOG_APP "APP"

Core core;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "Storage Namespace Demo");
    DLOG_I(LOG_APP, "========================================");
    
    // Create two storage components with different namespaces
    StorageConfig configStorage;
    configStorage.namespace_name = "config";
    
    StorageConfig dataStorage;
    dataStorage.namespace_name = "appdata";
    
    auto storageConfig = std::make_unique<StorageComponent>(configStorage);
    auto storageData = std::make_unique<StorageComponent>(dataStorage);
    
    auto* configPtr = storageConfig.get();
    auto* dataPtr = storageData.get();
    
    core.addComponent(std::move(storageConfig));
    core.addComponent(std::move(storageData));
    
    // Initialize components
    core.begin();
    
    // Store configuration in "config" namespace
    DLOG_I(LOG_APP, "\nStoring config data...");
    configPtr->putString("wifi_ssid", "MyNetwork");
    configPtr->putString("wifi_pass", "MyPassword");
    configPtr->putInt("port", 8080);
    configPtr->putBool("enabled", true);
    
    // Store application data in "appdata" namespace
    DLOG_I(LOG_APP, "\nStoring application data...");
    dataPtr->putInt("sensor_reading", 42);
    dataPtr->putFloat("temperature", 23.5);
    dataPtr->putString("status", "running");
    
    // Read back and display
    DLOG_I(LOG_APP, "\n=== Config Namespace ===");
    DLOG_I(LOG_APP, "WiFi SSID: %s", configPtr->getString("wifi_ssid", "").c_str());
    DLOG_I(LOG_APP, "WiFi Pass: %s", configPtr->getString("wifi_pass", "").c_str());
    DLOG_I(LOG_APP, "Port: %d", configPtr->getInt("port", 0));
    DLOG_I(LOG_APP, "Enabled: %s", configPtr->getBool("enabled", false) ? "yes" : "no");
    
    DLOG_I(LOG_APP, "\n=== AppData Namespace ===");
    DLOG_I(LOG_APP, "Sensor: %d", dataPtr->getInt("sensor_reading", 0));
    DLOG_I(LOG_APP, "Temp: %.1f°C", dataPtr->getFloat("temperature", 0.0));
    DLOG_I(LOG_APP, "Status: %s", dataPtr->getString("status", "").c_str());
    
    DLOG_I(LOG_APP, "\n=== Stats ===");
    DLOG_I(LOG_APP, "Config entries: %d", configPtr->getEntryCount());
    DLOG_I(LOG_APP, "Data entries: %d", dataPtr->getEntryCount());
    
    DLOG_I(LOG_APP, "\nDemo complete! Data persists across reboots.");
}

void loop() {
    core.loop();
    delay(1000);
}
