#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/ComponentRegistry.h>
#include <memory>
#include "Components.h"

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

void setup() {
    CoreConfig cfg;
    cfg.deviceName = "EventBusBasics";
    cfg.logLevel = 3;

    core.reset(new Core());

    // Register demo components that communicate via EventBus topics
    // Publisher: emits sensor.update every 2s
    core->addComponent(createPublisher("Publisher", 2000, &core->getRegistry()));
    // Consumer: listens to sensor.update and toggles LED when value >= threshold
    core->addComponent(createConsumer("Consumer", LED_BUILTIN, 500, &core->getRegistry()));

    if (!core->begin(cfg)) {
        return;
    }
}

void loop() {
    if (core) core->loop();
}
