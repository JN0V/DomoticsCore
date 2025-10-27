#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/ComponentRegistry.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Logger.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

// We will use a literal topic and a plain integer payload for simplicity.
static constexpr const char* SENSOR_UPDATE_TOPIC = "sensor.update";

// --- Simple publisher using a sawtooth signal ---
// Keep components minimal and easy to read for beginners.
// Publisher publishes a number every 2s on the topic "sensor.update".
class PublisherComponent : public IComponent {
public:
    // No-arg constructor; sensible defaults inside the class
    PublisherComponent() = default;

    String getName() const override { return "Publisher"; }

    ComponentStatus begin() override {
        lastTick_ = millis();
        return ComponentStatus::Success;
    }

    void loop() override {
        unsigned long now = millis();
        if (now - lastTick_ >= intervalMs_) {
            lastTick_ = now;
            counter_ = (counter_ + 128) & 0x3FF; // 0..1023
            int value = counter_;
            // Sticky publish so late subscribers can get the latest immediately
            eventBus().publishSticky(SENSOR_UPDATE_TOPIC, value);
        }
    }

    ComponentStatus shutdown() override { return ComponentStatus::Success; }

private:
    uint32_t intervalMs_ = 2000; // 2 seconds
    unsigned long lastTick_ = 0;
    int counter_ = 0;
};

// --- Wildcard consumer that logs any "sensor.*" topic ---
class WildcardConsumer : public IComponent {
public:
    WildcardConsumer() = default;

    String getName() const override { return "Wildcard"; }

    ComponentStatus begin() override {
        // Subscribe to any topic starting with "sensor."
        subId_ = eventBus().subscribe(String("sensor.*"), [this](const void* payload){
            auto* p = static_cast<const int*>(payload);
            if (!p) return;
            DLOG_I(LOG_APP, "[Wildcard] sensor.* value=%d", *p);
        }, this);
        return ComponentStatus::Success;
    }

    void loop() override {}

    ComponentStatus shutdown() override {
        eventBus().unsubscribeOwner(this);
        return ComponentStatus::Success;
    }

private:
    uint32_t subId_ = 0;
};

// --- Consumer that toggles the LED based on threshold ---
// Consumer subscribes at begin() and toggles LED based on the incoming value.
class ConsumerComponent : public IComponent {
public:
    // No-arg constructor; uses built-in LED and a default threshold
    ConsumerComponent() = default;

    String getName() const override { return "Consumer"; }

    ComponentStatus begin() override {
        pinMode(ledPin_, OUTPUT);
        // Subscribe via framework-provided EventBus helper (replayLast=true to get last sticky value immediately)
        subId_ = eventBus().subscribe(SENSOR_UPDATE_TOPIC, [this](const void* payload){
            auto* p = static_cast<const int*>(payload);
            if (!p) return;
            bool on = (*p >= threshold_);
            DLOG_I(LOG_APP, "[Consumer] sensor.update value=%d -> LED %s", *p, on ? "ON" : "OFF");
            digitalWrite(ledPin_, on ? HIGH : LOW);
        }, this, true);
        return ComponentStatus::Success;
    }

    void loop() override {}

    ComponentStatus shutdown() override {
        eventBus().unsubscribeOwner(this);
        digitalWrite(ledPin_, LOW);
        return ComponentStatus::Success;
    }

private:
    int ledPin_ = LED_BUILTIN;
    int threshold_ = 500;
    uint32_t subId_ = 0;
};

Core core;

void setup() {
    CoreConfig cfg;
    cfg.deviceName = "EventBusBasics";
    cfg.logLevel = 3;

    // Core initialized

    // Register demo components that communicate via EventBus topics
    // Publisher: emits sensor.update every 2s (sawtooth value)
    std::unique_ptr<IComponent> pub(new PublisherComponent());
    core.addComponent(std::move(pub));
    // Consumer: listens to sensor.update and toggles LED when value >= threshold
    std::unique_ptr<IComponent> cons(new ConsumerComponent());
    core.addComponent(std::move(cons));
    // Wildcard consumer: logs any sensor.* topic
    std::unique_ptr<IComponent> wc(new WildcardConsumer());
    core.addComponent(std::move(wc));

    if (!core.begin(cfg)) {
        return;
    }
}

void loop() {
    core.loop();
}
