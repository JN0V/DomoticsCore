#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Logger.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

static constexpr const char* T_ORDER = "test.order";
static constexpr const char* T_UNSUB = "test.unsub";
static constexpr const char* T_WC_A  = "sensor.update";
static constexpr const char* T_WC_B  = "sensor.temp";
static constexpr const char* T_STICKY = "test.sticky";
static constexpr const char* T_BP = "test.backpressure";

class EventBusTestComponent : public IComponent {
public:
  String getName() const override { return "EventBusTests"; }

  ComponentStatus begin() override {
    Serial.begin(115200);
    delay(50);
    DLOG_I(LOG_CORE, "[Tests] Starting EventBus tests...");

    bool ok = true;
    ok &= testOrder();
    ok &= testUnsubscribeOwner();
    ok &= testWildcards();
    ok &= testSticky();
    ok &= testBackpressure();

    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] ALL PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] SOME FAIL");
    }
    done_ = true;
    return ComponentStatus::Success;
  }

  void loop() override {}
  ComponentStatus shutdown() override { return ComponentStatus::Success; }

private:
  bool done_ = false;

  bool testOrder() {
    results_.clear();
    uint32_t sub = eventBus().subscribe(T_ORDER, [this](const void* p){
      auto* v = static_cast<const int*>(p);
      if (v) results_.push_back(*v);
    }, this);

    for (int i=1;i<=5;i++) {
      eventBus().publish(T_ORDER, i);
    }
    // Process queue
    for (int i=0;i<2;i++) eventBus().poll();

    eventBus().unsubscribe(sub);
    bool ok = (results_.size()==5);
    for (int i=0;i<5 && ok;i++) ok &= (results_[i] == i+1);
    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] Order: PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] Order: FAIL");
    }
    return ok;
  }

  bool testUnsubscribeOwner() {
    count_ = 0;
    eventBus().subscribe(T_UNSUB, [this](const void* p){ count_++; }, this);
    eventBus().publish(T_UNSUB, 1);
    eventBus().poll();
    eventBus().unsubscribeOwner(this);
    eventBus().publish(T_UNSUB, 2);
    eventBus().poll();
    bool ok = (count_ == 1);
    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] UnsubscribeOwner: PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] UnsubscribeOwner: FAIL");
    }
    return ok;
  }

  bool testWildcards() {
    results_.clear();
    // sensor.* wildcard
    uint32_t sub = eventBus().subscribe(String("sensor.*"), [this](const void* p){
      auto* v = static_cast<const int*>(p);
      if (v) results_.push_back(*v);
    }, this);
    eventBus().publish(T_WC_A, 10);
    eventBus().publish(T_WC_B, 20);
    eventBus().poll();
    eventBus().unsubscribe(sub);
    bool ok = (results_.size()==2 && results_[0]==10 && results_[1]==20);
    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] Wildcards: PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] Wildcards: FAIL");
    }
    return ok;
  }

  bool testSticky() {
    results_.clear();
    // Publish sticky first
    eventBus().publishSticky(T_STICKY, 42);
    // Now subscribe with replayLast=true; should receive 42 immediately
    uint32_t sub = eventBus().subscribe(T_STICKY, [this](const void* p){
      auto* v = static_cast<const int*>(p);
      if (v) results_.push_back(*v);
    }, this, true);
    // Publish another update
    eventBus().publish(T_STICKY, 43);
    eventBus().poll();
    eventBus().unsubscribe(sub);
    bool ok = (results_.size()==2 && results_[0]==42 && results_[1]==43);
    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] Sticky: PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] Sticky: FAIL");
    }
    return ok;
  }

  bool testBackpressure() {
    results_.clear();
    // Subscribe first to count deliveries
    uint32_t sub = eventBus().subscribe(T_BP, [this](const void* p){
      auto* v = static_cast<const int*>(p);
      if (v) results_.push_back(*v);
    }, this);
    // Enqueue more than queue capacity (32) before polling
    for (int i=0;i<100;i++) eventBus().publish(T_BP, i);
    // Poll enough times to drain
    for (int i=0;i<10;i++) eventBus().poll();
    eventBus().unsubscribe(sub);
    // Expect only the last 32 (0..99 -> keep 68..99) due to drop-oldest policy
    bool ok = (results_.size()==32);
    if (ok) {
      for (int i=0;i<32;i++) {
        if (results_[i] != (68 + i)) { ok = false; break; }
      }
    }
    if (ok) {
      DLOG_I(LOG_CORE, "[Tests] Backpressure: PASS");
    } else {
      DLOG_I(LOG_CORE, "[Tests] Backpressure: FAIL");
    }
    return ok;
  }

  std::vector<int> results_;
  int count_ = 0;
};

Core core;

void setup() {
  CoreConfig cfg;
  cfg.deviceName = "EventBusTests";
  cfg.logLevel = 3;

  // Core initialized
  core.addComponent(std::unique_ptr<IComponent>(new EventBusTestComponent()));
  core.begin(cfg);
}

void loop() {
  core.loop();
}
