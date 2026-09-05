// OBS probe, ESP32. Each boot prints the reset reason, whether a core dump
// is waiting in the coredump partition, and what the RTC_NOINIT record kept.
// Then it provokes the next scenario.
#include <Arduino.h>
#include "esp_system.h"
#include "esp_core_dump.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"

RTC_NOINIT_ATTR struct {
    uint32_t magic;
    uint32_t step;
    uint32_t hookCount;
    uint32_t lastSize, lastCaps, lastFree;
    uint32_t marker;      // incremented by the busy loop so a later boot can see it ran
} rec;
static const uint32_t MAGIC = 0x0B5C0DE4;
#ifndef START_STEP
#define START_STEP 0
#endif

static void hook(size_t size, uint32_t caps, const char* fn) {
    rec.hookCount++;
    rec.lastSize = size;
    rec.lastCaps = caps;
    rec.lastFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== OBS-PROBE-32 BOOT ===");
    Serial.printf("esp_reset_reason=%d\n", (int)esp_reset_reason());
    if (rec.magic != MAGIC) { memset(&rec, 0, sizeof(rec)); rec.magic = MAGIC; rec.step = START_STEP; Serial.println("rec: fresh (magic mismatch), step reset to START_STEP"); }
    Serial.printf("rec.step(prev)=%u hookCount=%u lastSize=%u lastCaps=0x%x lastFree=%u marker=%u\n",
                  rec.step, rec.hookCount, rec.lastSize, rec.lastCaps, rec.lastFree, rec.marker);

    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    Serial.printf("coredump partition: %s", part ? "present" : "ABSENT");
    if (part) Serial.printf(" addr=0x%x size=%u", part->address, part->size);
    Serial.println();
    size_t addr = 0, size = 0;
    esp_err_t chk = esp_core_dump_image_check();
    esp_err_t get = esp_core_dump_image_get(&addr, &size);
    Serial.printf("coredump image_check=0x%x image_get=0x%x addr=0x%x size=%u\n", chk, get, addr, size);
    if (get == ESP_OK) { esp_err_t er = esp_core_dump_image_erase(); Serial.printf("coredump erased -> 0x%x\n", er); }

    heap_caps_register_failed_alloc_callback(hook);
    rec.hookCount = 0; rec.lastSize = 0; rec.lastCaps = 0; rec.lastFree = 0; rec.marker = 0;
    Serial.printf("heap internal free=%u min=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));

    uint32_t s = rec.step;
    rec.step = s + 1;
    const char* names[] = {"null deref", "busy loop in loop() (hang?)", "malloc until NULL, then deref", "abort()", "idle 30s watching hook", "esp_restart()", "idle"};
    uint32_t idx = s < 6 ? s : 6;
    Serial.printf("STEP %u -> %s in 2s\n", s, names[idx]);
    Serial.flush();
    delay(2000);

    switch (s) {
        case 0: { volatile uint32_t* p = nullptr; Serial.println(*p); break; }
        case 1: { rec.marker = 1; for (;;) { rec.marker++; } }
        case 2: {
            size_t n = 0;
            for (;;) { void* p = malloc(4096); if (!p) break; n++; }
            Serial.printf("malloc(4096) x%u then NULL; hookCount=%u lastSize=%u lastCaps=0x%x lastFree=%u\n", (unsigned)n, rec.hookCount, rec.lastSize, rec.lastCaps, rec.lastFree);
            Serial.flush();
            delay(200);
            volatile uint32_t* q = nullptr; *q = 1;
            break;
        }
        case 3: abort();
        case 4: {
            for (int i = 0; i < 6; i++) { delay(5000); Serial.printf("idle t=%ds hookCount=%u heap=%u\n", (i + 1) * 5, rec.hookCount, heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); }
            Serial.println("STEP 4 DONE");
            break;
        }
        case 5: esp_restart(); break;
        default: Serial.println("ALL STEPS DONE"); break;
    }
}

void loop() { delay(5000); Serial.printf("idle heap=%u hookCount=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL), rec.hookCount); }
