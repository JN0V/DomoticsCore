/**
 * Two tasks inside one EventBus: a publisher against poll() on the application
 * core, which is what an ESP32 does on every WebUI settings write. Sixteen topics keep pendingByTopic
 * a tree — the crash that opened this was inside its rebalancing.
 *
 * A panic is the answer; surviving the reader's window is the other one.
 */
#include <Arduino.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Utils;

static EventBus bus;
static volatile uint32_t g_published = 0;
static volatile uint32_t g_dispatched = 0;

static const size_t kTopics = 16;

// The vTaskDelay feeds IDLE0, or the task watchdog answers before the race does.
static void publisher(void*) {
    uint32_t n = 0;
    for (;;) {
        String topic("probe/");
        topic += (n % kTopics);
        bus.publish(topic, n);
        ++g_published;
        if (++n % 64 == 0) vTaskDelay(1);
    }
}

void setup() {
    HAL::Platform::initializeLogging();
    delay(300);
    Serial.println();
    Serial.println("[probe] EventBus race: publisher on core 0, poll on the app core");

    for (size_t i = 0; i < kTopics; ++i) {
        String topic("probe/");
        topic += i;
        bus.subscribe(topic, [](const void*) { ++g_dispatched; });
    }

    // Above loopTask's priority 1 and on the other core; AsyncTCP's own task is
    // priority 10 on whichever core is free (AsyncTCP.h:37,50).
    xTaskCreatePinnedToCore(publisher, "pub", 4096, nullptr, 3, nullptr, 0);
}

void loop() {
    bus.poll(8);

    static uint32_t lastPrint = 0;
    const uint32_t now = millis();
    if (now - lastPrint >= 1000) {
        lastPrint = now;
        Serial.printf("[probe] t=%lus published=%lu dispatched=%lu dropped=%lu heap=%lu\n",
                      (unsigned long)(now / 1000), (unsigned long)g_published,
                      (unsigned long)g_dispatched, (unsigned long)bus.getDroppedCount(),
                      (unsigned long)HAL::Platform::getFreeHeap());
    }
}
