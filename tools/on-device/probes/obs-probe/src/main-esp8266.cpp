// OBS probe, ESP8266. Each boot prints what the previous death left behind,
// then provokes the next scenario. Step counter and crash record live in
// RTC user memory from word 32 (words 0-31 are eboot's).
#include <Arduino.h>
extern "C" {
#include <user_interface.h>
extern void* umm_last_fail_alloc_addr;
extern int umm_last_fail_alloc_size;
}

struct Rec {
    uint32_t magic;
    uint32_t step;
    uint32_t cbFlag;      // 1 if custom_crash_callback ran since last clear
    uint32_t cbReason;    // rst_info->reason as seen by the callback
    uint32_t exccause, epc1, excvaddr;
    uint32_t failAddr, failSize;
    uint32_t nStack;
    uint32_t stack[8];
};
static const uint32_t MAGIC = 0x0B5C0DE3;
static const uint32_t OFFSET_WORDS = 32;
static Rec rec;

static void readRec() {
    ESP.rtcUserMemoryRead(OFFSET_WORDS, (uint32_t*)&rec, sizeof(rec));
    if (rec.magic != MAGIC) { memset(&rec, 0, sizeof(rec)); rec.magic = MAGIC; }
}
static void writeRec() { ESP.rtcUserMemoryWrite(OFFSET_WORDS, (uint32_t*)&rec, sizeof(rec)); }

extern "C" void custom_crash_callback(struct rst_info* r, uint32_t stack, uint32_t stack_end) {
    rec.cbFlag = 1;
    rec.cbReason = r->reason;
    rec.exccause = r->exccause;
    rec.epc1 = r->epc1;
    rec.excvaddr = r->excvaddr;
    rec.failAddr = (uint32_t)umm_last_fail_alloc_addr;
    rec.failSize = (uint32_t)umm_last_fail_alloc_size;
    rec.nStack = 0;
    for (uint32_t a = stack; a < stack_end && rec.nStack < 8; a += 4) rec.stack[rec.nStack++] = *(uint32_t*)a;
    writeRec();
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("=== OBS-PROBE-8266 BOOT ===");
    readRec();
    const rst_info* ri = ESP.getResetInfoPtr();
    Serial.printf("sdk.reason=%u (%s)\n", ri->reason, ESP.getResetReason().c_str());
    Serial.printf("sdk.info=%s\n", ESP.getResetInfo().c_str());
    Serial.printf("sdk.exccause=%u epc1=0x%08x excvaddr=0x%08x\n", ri->exccause, ri->epc1, ri->excvaddr);
    Serial.printf("rec.step(prev)=%u cbFlag=%u cbReason=%u exccause=%u epc1=0x%08x excvaddr=0x%08x failAddr=0x%08x failSize=%u nStack=%u\n",
                  rec.step, rec.cbFlag, rec.cbReason, rec.exccause, rec.epc1, rec.excvaddr, rec.failAddr, rec.failSize, rec.nStack);
    Serial.printf("heap=%u\n", ESP.getFreeHeap());

    uint32_t s = rec.step;
    if (ri->reason == REASON_EXT_SYS_RST) { s = 0; Serial.println("external reset -> sequence restarts at step 0"); }
    rec.step = s + 1;
    rec.cbFlag = 0; rec.cbReason = 0; rec.exccause = 0; rec.epc1 = 0; rec.excvaddr = 0;
    rec.failAddr = 0; rec.failSize = 0; rec.nStack = 0;
    writeRec();

    const char* names[] = {"abort()", "OOM via new", "null deref", "soft WDT (busy loop)", "hardware WDT (wdtDisable + busy loop)", "ESP.restart()", "idle"};
    uint32_t idx = s < 6 ? s : 6;
    Serial.printf("STEP %u -> %s in 2s\n", s, names[idx]);
    Serial.flush();
    delay(2000);

    switch (s) {
        case 0: abort();
        case 1: { for (;;) { volatile uint8_t* p = new uint8_t[1024]; p[0] = 1; } }
        case 2: { volatile uint32_t* p = nullptr; Serial.println(*p); break; }
        case 3: { for (;;) {} }
        case 4: { ESP.wdtDisable(); for (;;) {} }
        case 5: ESP.restart(); break;
        default: Serial.println("ALL STEPS DONE"); break;
    }
}

void loop() { delay(5000); Serial.printf("idle heap=%u\n", ESP.getFreeHeap()); }
