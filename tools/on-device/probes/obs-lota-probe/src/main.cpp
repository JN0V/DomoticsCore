// OBS Lot A board probe. Boots a real System (Storage + SystemInfo, no radio,
// no console) built from the working tree, prints what the boot diagnostics
// carry, then provokes the next scenario:
//   step 0: seed the old misnamed keys, then null dereference
//   step 1: hang with the loop watchdog at 5 s   (ESP32: expect PANIC in ~5 s)
//   step 2: hang with the loop watchdog off      (ESP32: expect silence — removal check)
//   step 3+: idle
// The step lives in RTC (ESP32 RTC_NOINIT; ESP8266 user word 40).
#include <Arduino.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/SystemInfo.h>
using namespace DomoticsCore;

#ifndef PROBE_MAGIC
#define PROBE_MAGIC 0x0B5C0DE6
#endif
static const uint32_t MAGIC = PROBE_MAGIC;
#if DOMOTICS_PLATFORM_ESP32
RTC_NOINIT_ATTR uint32_t g_magic;
RTC_NOINIT_ATTR uint32_t g_step;
#ifndef START_STEP
#define START_STEP 0
#endif
static uint32_t loadStep() { if (g_magic != MAGIC) { g_magic = MAGIC; g_step = START_STEP; } return g_step; }
static void saveStep(uint32_t s) { g_step = s; }
#else
static uint32_t loadStep() { uint32_t w[2]; ESP.rtcUserMemoryRead(40, w, 8); return (w[0] == MAGIC) ? w[1] : 0; }
static void saveStep(uint32_t s) { uint32_t w[2] = {MAGIC, s}; ESP.rtcUserMemoryWrite(40, w, 8); }
#endif

static System* sys = nullptr;
static uint32_t step = 0;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("=== OBS-LOTA-PROBE ===");
    step = loadStep();
#if !DOMOTICS_PLATFORM_ESP32
    if (ESP.getResetInfoPtr()->reason == REASON_EXT_SYS_RST) { step = 0; Serial.println("PROBE external reset: sequence restarts at step 0"); }
#endif

    SystemConfig cfg;
    cfg.enableLED = false;
    cfg.enableConsole = false;
    cfg.wifiAutoConfig = false;
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    cfg.loopWatchdogSeconds = (step == 2) ? 0 : 5;
    sys = new System(cfg);
    sys->begin();

    auto* si = sys->getCore().getComponent<Components::SystemInfoComponent>("System Info");
    auto* st = sys->getCore().getComponent<Components::StorageComponent>("Storage");
    if (si) {
        const auto& d = si->getBootDiagnostics();
        Serial.printf("PROBE step=%u reason=%s unexpected=%d | detail valid=%d exccause=%u epc1=0x%08x | coredump supported=%d partition=%d dump=%d size=%u | bootHeap=%u minTracked=%d bootMinHeap=%u\n",
                      step, d.getResetReasonString().c_str(), (int)d.wasUnexpectedReset(),
                      (int)d.resetDetail.valid, d.resetDetail.exccause, d.resetDetail.epc1,
                      (int)d.coreDump.supported, (int)d.coreDump.partitionPresent, (int)d.coreDump.dumpPresent, d.coreDump.size,
                      d.bootHeap, (int)d.bootMinHeapTracked, d.bootMinHeap);
    }
    if (st) {
        Serial.printf("PROBE keys: boot_count=%d boot_heap=%d boot_minheap=%d last_heap=%d last_minheap=%d\n",
                      st->getInt("boot_count", -1), (int)st->exists("boot_heap"), (int)st->exists("boot_minheap"),
                      (int)st->exists("last_heap"), (int)st->exists("last_minheap"));
        if (step == 0) {
            // A device upgraded from an older build carries these; the next boot must remove them.
            st->putInt("last_heap", 1);
            st->putInt("last_minheap", 1);
            Serial.println("PROBE seeded last_heap/last_minheap for the next boot to remove");
        }
    }
    saveStep(step + 1);
    const char* next = step == 0 ? "null dereference" : step == 1 ? "hang, loop watchdog 5 s" : step == 2 ? "hang, loop watchdog OFF" : "idle";
    Serial.printf("PROBE next: %s in 3 s\n", next);
    Serial.flush();
    uint32_t t0 = millis();
    while (millis() - t0 < 3000) { sys->loop(); delay(10); }

    if (step == 0) { volatile uint32_t* p = nullptr; Serial.println(*p); }
    if (step == 1 || step == 2) { Serial.println("PROBE hanging now"); Serial.flush(); for (;;) {} }
}

void loop() {
    sys->loop();
    delay(10);
    static uint32_t last = 0;
    if (millis() - last > 5000) { last = millis(); Serial.printf("PROBE idle t=%lus\n", (unsigned long)(millis() / 1000)); }
}
