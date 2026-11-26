/**
 * @file test_mqtt_log_ascii.cpp
 * @brief The MQTT component's log lines hold plain ASCII, readable on a serial console.
 *
 * The success line is captured as the component logs it. The failure line cannot be
 * reached through the native client, which never refuses a write while connected, so
 * the log statements of the MQTT headers are read from their source too.
 */

#include <unity.h>
#include <cstdio>
#include <string>
#include <vector>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

std::vector<std::string> g_lines;

bool isAscii(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return false;
    }
    return true;
}

// The header's text, looked up from the suite's working directory (the MQTT
// project) or from the repository's root.
bool readHeader(const char* name, std::string& out) {
    const char* dirs[] = {
        "include/DomoticsCore/",
        "../DomoticsCore-MQTT/include/DomoticsCore/",
        "DomoticsCore-MQTT/include/DomoticsCore/",
    };
    for (const char* dir : dirs) {
        std::string path = std::string(dir) + name;
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) continue;
        char buf[4096];
        size_t n;
        out.clear();
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
        std::fclose(f);
        return true;
    }
    return false;
}

// Every log statement, from its DLOG_ macro to the semicolon ending it, that holds a
// byte outside ASCII; the first of them, or "" when all are plain.
std::string firstNonAsciiLogStatement(const std::string& text) {
    size_t pos = 0;
    while ((pos = text.find("DLOG_", pos)) != std::string::npos) {
        size_t end = text.find(';', pos);
        if (end == std::string::npos) end = text.size();
        const std::string statement = text.substr(pos, end - pos);
        if (!isAscii(statement)) return statement;
        pos = end;
    }
    return "";
}

}  // namespace

void setUp() {}
void tearDown() {
    HAL::WiFiImpl::setConnectedForTest(false);
}

// Given the MQTT component logs a success or a failure,
// When I read the line,
// Then it holds only ASCII characters.
void test_mqtt_log_lines_hold_only_ascii() {
    // A success, as the component logs it.
    g_lines.clear();
    auto id = LoggerCallbacks::addCallback([](LogLevel, const char* tag, const char* msg) {
        if (tag && std::string(tag) == "MQTT") g_lines.emplace_back(msg ? msg : "");
    });
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    HAL::WiFiImpl::setConnectedForTest(true);
    TEST_ASSERT_TRUE(mqtt.connect());
    mqtt.loop();
    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_TRUE(mqtt.publish("a/b", "hello"));
    mqtt.shutdown();
    LoggerCallbacks::removeCallback(id);

    TEST_ASSERT_TRUE_MESSAGE(g_lines.size() > 0, "the component logged nothing");
    for (const auto& line : g_lines) {
        TEST_ASSERT_TRUE_MESSAGE(isAscii(line), line.c_str());
    }

    // Every log statement of the component, the failure line among them.
    const char* headers[] = {"MQTT.h", "MQTT_impl.h", "MQTTWebUI.h", "MQTT_Stub.h",
                             "MQTT_ESP32.h", "MQTT_ESP8266.h", "MQTT_HAL.h"};
    bool readImpl = false;
    for (const char* name : headers) {
        std::string text;
        if (!readHeader(name, text)) continue;
        if (std::string(name) == "MQTT_impl.h") readImpl = true;
        const std::string bad = firstNonAsciiLogStatement(text);
        TEST_ASSERT_TRUE_MESSAGE(bad.empty(), bad.c_str());
    }
    TEST_ASSERT_TRUE_MESSAGE(readImpl, "MQTT_impl.h could not be read from the suite's working directory");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mqtt_log_lines_hold_only_ascii);
    return UNITY_END();
}
