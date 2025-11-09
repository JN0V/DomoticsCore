#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Logger.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

// Topics
static constexpr const char* TOPIC_A_READY = "service.a.ready";
static constexpr const char* TOPIC_B_READY = "service.b.ready";
static constexpr const char* TOPIC_SYSTEM_READY = "system.ready";

// Simulated component A: becomes ready after a short delay and publishes sticky ready=true
class ServiceAComponent : public IComponent {
public:
  ServiceAComponent() {
    metadata.name = "ServiceA";
    metadata.version = "1.0.0";
  }
  ComponentStatus begin() override {
    start_ = millis();
    return ComponentStatus::Success;
  }
  void loop() override {
    if (!done_ && millis() - start_ > 1500) {
      done_ = true;
      bool ready = true;
      eventBus().publishSticky(TOPIC_A_READY, ready);
      DLOG_I(LOG_APP, "[A] published %s=true", TOPIC_A_READY);
    }
  }
  ComponentStatus shutdown() override { return ComponentStatus::Success; }
private:
  unsigned long start_ = 0;
  bool done_ = false;
};

// Simulated component B: becomes ready after a slightly longer delay and publishes sticky ready=true
class ServiceBComponent : public IComponent {
public:
  ServiceBComponent() {
    metadata.name = "ServiceB";
    metadata.version = "1.0.0";
  }
  ComponentStatus begin() override {
    start_ = millis();
    return ComponentStatus::Success;
  }
  void loop() override {
    if (!done_ && millis() - start_ > 3000) {
      done_ = true;
      bool ready = true;
      eventBus().publishSticky(TOPIC_B_READY, ready);
      DLOG_I(LOG_APP, "[B] published %s=true", TOPIC_B_READY);
    }
  }
  ComponentStatus shutdown() override { return ComponentStatus::Success; }
private:
  unsigned long start_ = 0;
  bool done_ = false;
};

// Coordinator listens to A and B readiness and publishes system.ready when both are true.
class CoordinatorComponent : public IComponent {
public:
  CoordinatorComponent() {
    metadata.name = "Coordinator";
    metadata.version = "1.0.0";
  }
  ComponentStatus begin() override {
    // Replay sticky so if A/B already published, we react immediately
    subA_ = eventBus().subscribe(TOPIC_A_READY, [this](const void* p){ onA(p); }, this, true);
    subB_ = eventBus().subscribe(TOPIC_B_READY, [this](const void* p){ onB(p); }, this, true);
    return ComponentStatus::Success;
  }
  void loop() override {}
  ComponentStatus shutdown() override {
    eventBus().unsubscribeOwner(this);
    return ComponentStatus::Success;
  }
private:
  void onA(const void* p) {
    auto* v = static_cast<const bool*>(p);
    aReady_ = (v && *v);
    maybePublishSystemReady();
  }
  void onB(const void* p) {
    auto* v = static_cast<const bool*>(p);
    bReady_ = (v && *v);
    maybePublishSystemReady();
  }
  void maybePublishSystemReady() {
    if (!sent_ && aReady_ && bReady_) {
      sent_ = true;
      bool ready = true;
      eventBus().publishSticky(TOPIC_SYSTEM_READY, ready);
      DLOG_I(LOG_APP, "[Coordinator] published %s=true", TOPIC_SYSTEM_READY);
    }
  }
  uint32_t subA_ = 0, subB_ = 0;
  bool aReady_ = false, bReady_ = false, sent_ = false;
};

// A consumer that reacts to system.ready and toggles LED
class ReadyLEDConsumer : public IComponent {
public:
  ReadyLEDConsumer() {
    metadata.name = "ReadyLED";
    metadata.version = "1.0.0";
  }
  ComponentStatus begin() override {
    pinMode(LED_BUILTIN, OUTPUT);
    sub_ = eventBus().subscribe(TOPIC_SYSTEM_READY, [this](const void* p){
      auto* v = static_cast<const bool*>(p);
      bool on = (v && *v);
      digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
      DLOG_I(LOG_APP, "[ReadyLED] %s -> LED %s", TOPIC_SYSTEM_READY, on ? "ON" : "OFF");
    }, this, true); // replay in case already ready
    return ComponentStatus::Success;
  }
  void loop() override {}
  ComponentStatus shutdown() override {
    eventBus().unsubscribeOwner(this);
    digitalWrite(LED_BUILTIN, LOW);
    return ComponentStatus::Success;
  }
private:
  uint32_t sub_ = 0;
};

Core core;

void setup() {
  CoreConfig cfg;
  cfg.deviceName = "EventBusCoordinators";
  cfg.logLevel = 3;

  // Core initialized

  core.addComponent(std::unique_ptr<IComponent>(new ServiceAComponent()));
  core.addComponent(std::unique_ptr<IComponent>(new ServiceBComponent()));
  core.addComponent(std::unique_ptr<IComponent>(new CoordinatorComponent()));
  core.addComponent(std::unique_ptr<IComponent>(new ReadyLEDConsumer()));

  core.begin(cfg);
}

void loop() {
  core.loop();
}
