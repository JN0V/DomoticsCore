/**
 * @file test_ha_events.cpp
 * @brief Tests for ha/entity_added event emission in all addXxx() methods (M19)
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/Platform_Stub.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Utils;
using namespace DomoticsCore::Components::HomeAssistant;

void setUp(void) {}
void tearDown(void) {}

void test_add_sensor_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addSensor("temp_sensor", "Temperature", "°C");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("temp_sensor", received.id);
    TEST_ASSERT_EQUAL_STRING("sensor", received.component);
}

void test_add_binary_sensor_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addBinarySensor("door_contact", "Door");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("door_contact", received.id);
    TEST_ASSERT_EQUAL_STRING("binary_sensor", received.component);
}

void test_add_switch_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addSwitch("relay_1", "Relay");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("relay_1", received.id);
    TEST_ASSERT_EQUAL_STRING("switch", received.component);
}

void test_add_light_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addLight("led_strip", "LED Strip");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("led_strip", received.id);
    TEST_ASSERT_EQUAL_STRING("light", received.component);
}

void test_add_button_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addButton("restart_btn", "Restart");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("restart_btn", received.id);
    TEST_ASSERT_EQUAL_STRING("button", received.component);
}

void test_add_alarm_control_panel_emits_entity_added(void) {
    Core core;
    auto ha = std::make_unique<HomeAssistantComponent>();
    core.addComponent(std::move(ha));
    core.begin();

    HAEntityAddedEvent received{};
    bool fired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_ENTITY_ADDED), [&](const void* data) {
        if (data) {
            received = *reinterpret_cast<const HAEntityAddedEvent*>(data);
            fired = true;
        }
    }, nullptr);

    core.getComponent<HomeAssistantComponent>("HomeAssistant")->addAlarmControlPanel(
        "alarm_1", "Home Alarm");
    core.getEventBus().poll();

    TEST_ASSERT_TRUE(fired);
    TEST_ASSERT_EQUAL_STRING("alarm_1", received.id);
    TEST_ASSERT_EQUAL_STRING("alarm_control_panel", received.component);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_add_sensor_emits_entity_added);
    RUN_TEST(test_add_binary_sensor_emits_entity_added);
    RUN_TEST(test_add_switch_emits_entity_added);
    RUN_TEST(test_add_light_emits_entity_added);
    RUN_TEST(test_add_button_emits_entity_added);
    RUN_TEST(test_add_alarm_control_panel_emits_entity_added);

    return UNITY_END();
}
