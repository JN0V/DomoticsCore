#pragma once

// Shared by the HomeAssistant component suites: includes, the connect helper,
// the log probe, and the setUp/tearDown that clears what a failed test leaves.

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/ArduinoJsonString.h>  // String converters for ArduinoJson 7
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/MQTT.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;
using namespace DomoticsCore::Testing;

// Helper: emit mqtt/connected and drain the event queue
static void simulateMqttConnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    // Drain all queued events (connect -> availability -> discovery -> subscribe)
    for (int i = 0; i < 5; i++) core.loop();
}

// Log capture into static buffers: a failed assertion longjmps out of the test,
// so the callback must not reference its stack, and tearDown() removes it again.
static String g_capturedWarn;
static String g_capturedError;
static String g_capturedInfo;
static bool g_captureActive = false;
static LoggerCallbacks::CallbackId g_captureId = 0;

static void startLogCapture() {
    if (g_captureActive) LoggerCallbacks::removeCallback(g_captureId);
    g_capturedWarn = "";
    g_capturedError = "";
    g_capturedInfo = "";
    g_captureId = LoggerCallbacks::addCallback(
        [](LogLevel level, const char* tag, const char* message) {
            if (strcmp(tag, LOG_HA) != 0) return;
            if (level == LOG_LEVEL_WARN) {
                g_capturedWarn += message;
                g_capturedWarn += "\n";
            } else if (level == LOG_LEVEL_ERROR) {
                g_capturedError += message;
                g_capturedError += "\n";
            } else if (level == LOG_LEVEL_INFO) {
                g_capturedInfo += message;
                g_capturedInfo += "\n";
            }
        });
    g_captureActive = true;
}

static void stopLogCapture() {
    if (!g_captureActive) return;
    LoggerCallbacks::removeCallback(g_captureId);
    g_captureActive = false;
}

void setUp() {}

// The log probe and the WiFi stub are process-wide: a failed test leaves both.
void tearDown() {
    stopLogCapture();
    // A failed assertion longjmps past whatever a test meant to restore, and the
    // stub is process-wide: leave it down so the next test states its own needs.
    HAL::WiFiImpl::setConnectedForTest(false);
}
