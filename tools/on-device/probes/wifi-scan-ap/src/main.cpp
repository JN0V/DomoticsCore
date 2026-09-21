// Board probe: a scan started in AP-only mode, harvested by loop().
// The component is given no SSID, which is what provisioning looks like, and
// which used to make loop() return before the poll. Two scans in a row: the
// second only starts if the first released the flag.
#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;

static Core* core = nullptr;
static Components::WifiComponent* wifi = nullptr;

static uint8_t round_ = 0;
static uint32_t startedAt = 0;
static bool waiting = false;

// The summary carries the names of every network in the room. Report its shape,
// never its text.
static void report(const char* what, const String& summary) {
    // Count the suffix every entry ends with; an SSID may itself contain ", ".
    int entries = 0;
    for (int at = summary.indexOf(" dBm)"); at >= 0; at = summary.indexOf(" dBm)", at + 5)) entries++;
    const bool pending = (summary == "Scanning...");
    const bool failed = (summary == "Scan failed");
    Serial.printf("SCANAP round=%u t=%lums %s entries=%d len=%u pending=%d failed=%d heap=%u\n",
                  (unsigned)round_, (unsigned long)(millis() - startedAt), what,
                  entries, (unsigned)summary.length(),
                  (int)pending, (int)failed, (unsigned)HAL::Platform::getFreeHeap());
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("SCANAP probe: AP-only, no configured SSID");

    core = new Core();
    auto w = std::unique_ptr<Components::WifiComponent>(new Components::WifiComponent("", ""));
    wifi = w.get();
    core->addComponent(std::move(w));

    CoreConfig cfg;
    cfg.deviceName = "ScanProbe";
    core->begin(cfg);

    Serial.printf("SCANAP mode=%d apEnabled=%d heap=%u\n",
                  (int)HAL::WiFiHAL::getMode(), (int)wifi->isAPEnabled(),
                  (unsigned)HAL::Platform::getFreeHeap());
}

void loop() {
    core->loop();

    if (!waiting && round_ < 2) {
        round_++;
        startedAt = millis();
        const bool started = wifi->startScanAsync();
        Serial.printf("SCANAP round=%u started=%d\n", (unsigned)round_, (int)started);
        if (!started) {
            // The previous round never released the flag. Waiting here would
            // report its summary as this round's harvest.
            Serial.println("SCANAP FAILED: the scan was refused, the flag was not released");
            round_ = 2;
            return;
        }
        // A second call while one runs must be refused.
        Serial.printf("SCANAP round=%u second_call=%d (expected 0)\n",
                      (unsigned)round_, (int)wifi->startScanAsync());
        waiting = true;
    }

    if (waiting) {
        static uint32_t lastTick = 0;
        if (millis() - lastTick >= 1000) {
            lastTick = millis();
            const String summary = wifi->getLastScanSummary();
            report("tick", summary);
            if (summary != "Scanning...") {
                report("harvested", summary);
                waiting = false;
                delay(2000);
            } else if (millis() - startedAt > 20000) {
                Serial.println("SCANAP FAILED: still Scanning... after 20 s");
                waiting = false;
            }
        }
    }
}
