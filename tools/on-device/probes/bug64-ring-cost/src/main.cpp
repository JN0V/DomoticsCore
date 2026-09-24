// Board probe: the cost of one line in the remote console's ring.
//
// Measures the real LogEntry, at three line lengths, against the free heap on
// the board: the figure the ring-size decision needs, per platform.
#include <Arduino.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Platform_HAL.h>
#include <vector>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

static void measure(size_t entries, size_t lineLen) {
    String message;
    for (size_t i = 0; i < lineLen; ++i) message += 'x';

    const uint32_t before = HAL::Platform::getFreeHeap();
    {
        std::vector<LogEntry> ring;
        ring.reserve(entries);
        for (size_t i = 0; i < entries; ++i) {
            ring.push_back(LogEntry(HAL::Platform::getMillis(), LOG_LEVEL_INFO, "CONSOLE", message.c_str()));
        }
        const uint32_t held = HAL::Platform::getFreeHeap();
        Serial.printf("RING %u entries of %u chars: %u B total, %u B/entry\n",
                      (unsigned)entries, (unsigned)lineLen,
                      (unsigned)(before - held), (unsigned)((before - held) / entries));
    }
    const uint32_t after = HAL::Platform::getFreeHeap();
    Serial.printf("RING   returned: %d B outstanding\n", (int)(before - after));
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.printf("RING start free=%u sizeof(String)=%u sizeof(LogEntry)=%u default_ring=%u\n",
                  (unsigned)HAL::Platform::getFreeHeap(), (unsigned)sizeof(String),
                  (unsigned)sizeof(LogEntry), (unsigned)DOMOTICS_LOG_BUFFER_SIZE);
    measure(DOMOTICS_LOG_BUFFER_SIZE, 40);
    measure(DOMOTICS_LOG_BUFFER_SIZE, 80);
    measure(DOMOTICS_LOG_BUFFER_SIZE, 120);
    measure(50, 80);
    Serial.println("RING done");
}

void loop() { delay(1000); }
