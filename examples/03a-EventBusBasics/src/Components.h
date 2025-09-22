#pragma once

#include <Arduino.h>
#include <DomoticsCore/Components/IComponent.h>
#include <DomoticsCore/Components/ComponentRegistry.h>
#include <DomoticsCore/Utils/Timer.h>
#include <DomoticsCore/Utils/EventBus.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace Demo03aTopics {
  static constexpr const char* SensorUpdate = "sensor.update";
  struct SensorPayload { int value = 0; };
}

class PublisherComponent : public IComponent {
public:
    PublisherComponent(const String& name, uint32_t intervalMs)
        : name_(name), intervalMs_(intervalMs) {}

    String getName() const override { return name_; }
    String getVersion() const override { return "1.0.0"; }

    ComponentStatus begin() override {
        lastTick_ = millis();
        return ComponentStatus::Success;
    }

    void loop() override {
        unsigned long now = millis();
        if (now - lastTick_ >= intervalMs_) {
            lastTick_ = now;
            Demo03aTopics::SensorPayload p{ static_cast<int>(analogRead(34) % 1024) };
            if (registry_) {
                registry_->getEventBus().publish(Demo03aTopics::SensorUpdate, p);
            }
        }
    }

    ComponentStatus shutdown() override { return ComponentStatus::Success; }

    void setRegistry(ComponentRegistry* r) { registry_ = r; }

private:
    String name_;
    uint32_t intervalMs_;
    unsigned long lastTick_ = 0;
    ComponentRegistry* registry_ = nullptr;
};

class ConsumerComponent : public IComponent {
public:
    ConsumerComponent(const String& name, int ledPin, int threshold)
        : name_(name), ledPin_(ledPin), threshold_(threshold) {}

    String getName() const override { return name_; }
    String getVersion() const override { return "1.0.0"; }

    ComponentStatus begin() override {
        pinMode(ledPin_, OUTPUT);
        // Subscribe to sensor updates
        if (registry_) {
            subId_ = registry_->getEventBus().subscribe(Demo03aTopics::SensorUpdate, [this](const void* payload){
                auto* p = static_cast<const Demo03aTopics::SensorPayload*>(payload);
                if (!p) return;
                bool on = (p->value >= threshold_);
                digitalWrite(ledPin_, on ? HIGH : LOW);
            }, this);
        }
        return ComponentStatus::Success;
    }

    void loop() override {}

    ComponentStatus shutdown() override {
        if (registry_) registry_->getEventBus().unsubscribeOwner(this);
        digitalWrite(ledPin_, LOW);
        return ComponentStatus::Success;
    }

    void setRegistry(ComponentRegistry* r) { registry_ = r; }

private:
    String name_;
    int ledPin_;
    int threshold_;
    uint32_t subId_ = 0;
    ComponentRegistry* registry_ = nullptr;
};

// Factory helpers
static std::unique_ptr<IComponent> createPublisher(const String& name, uint32_t intervalMs, ComponentRegistry* reg) {
    auto up = std::make_unique<PublisherComponent>(name, intervalMs);
    static_cast<PublisherComponent*>(up.get())->setRegistry(reg);
    return up;
}

static std::unique_ptr<IComponent> createConsumer(const String& name, int ledPin, int threshold, ComponentRegistry* reg) {
    auto up = std::make_unique<ConsumerComponent>(name, ledPin, threshold);
    static_cast<ConsumerComponent*>(up.get())->setRegistry(reg);
    return up;
}
