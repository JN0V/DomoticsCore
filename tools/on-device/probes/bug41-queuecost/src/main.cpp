/**
 * Calibration of QueueCost, one constant per series.
 *
 * The queue is measured undrained and then given back, the way firmware does:
 * a series that never drains measures occupancy, and the drain line is what
 * says the occupancy was held rather than lost.
 *
 * Written against the entry cap this lot replaced, so every N stays under the
 * 32 entries that cap allowed. The staircase across N is the measurement: its
 * slope is the per-event cost, its steps are the deque chunk.
 */
#include <Arduino.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Utils;

static EventBus bus;
static volatile uint32_t g_heapInRefusal = 0;   // sampled from inside the refusal frame

static uint32_t freeHeap() { return HAL::Platform::getAllocatableFreeHeap(); }

struct Sample { uint32_t before, afterFill, afterDrain; };

// One point: N events of `payload` bytes on `topic`, never drained, then drained.
static Sample point(size_t N, size_t payload, const char* topic) {
    bus.reset();
    for (int i = 0; i < 8; ++i) bus.poll();          // let reset's own frees settle
    std::vector<uint8_t> buf(payload ? payload : 1, 0x5A);
    Sample s{};
    s.before = freeHeap();
    for (size_t i = 0; i < N; ++i) {
        if (payload) bus.publish(String(topic), buf.data(), payload);
        else         bus.publish(String(topic));
    }
    s.afterFill = freeHeap();
    for (int i = 0; i < 64; ++i) bus.poll();          // maxPerPoll is 8
    s.afterDrain = freeHeap();
    return s;
}

static void series(const char* name, size_t payload, const char* topic, const size_t* Ns, size_t nN) {
    Serial.printf("\n[%s] payload=%u topic=\"%s\" (%u car.)\n",
                  name, (unsigned)payload, topic, (unsigned)strlen(topic));
    Serial.printf("   %4s %10s %10s %8s %9s\n", "N", "fill(B)", "per evt", "residue", "given%");
    long prevN = 0, prevFill = 0;
    for (size_t k = 0; k < nN; ++k) {
        Sample s = point(Ns[k], payload, topic);
        long fill = (long)s.before - (long)s.afterFill;
        long residue = (long)s.before - (long)s.afterDrain;
        long given = fill - residue;
        // local slope: what the events added since the previous point cost
        char slope[24];
        if (prevN && (long)Ns[k] > prevN) snprintf(slope, sizeof(slope), "%.1f", (double)(fill - prevFill) / (double)((long)Ns[k] - prevN));
        else snprintf(slope, sizeof(slope), "%.1f", Ns[k] ? (double)fill / (double)Ns[k] : 0.0);
        Serial.printf("   %4u %10ld %10s %8ld %8ld%%\n",
                      (unsigned)Ns[k], fill, slope, residue, fill ? (given * 100 / fill) : 0);
        prevN = (long)Ns[k]; prevFill = fill;
        delay(20);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2500);
    Serial.println("\n\n===== QueueCost calibration =====");
#if defined(DOMOTICS_PLATFORM_ESP32)
    Serial.println("platform: ESP32");
#else
    Serial.println("platform: ESP8266");
#endif
    Serial.printf("sizeof(EventBus)=%u sizeof(QueuedEvent)=%u sizeof(String)=%u free=%lu\n",
                  (unsigned)sizeof(EventBus), (unsigned)sizeof(EventBus::QueuedEvent),
                  (unsigned)sizeof(String), (unsigned long)freeHeap());

    // Les N restent sous le plafond de 32 de l'en-tête livré. 16 et 17 encadrent
    // la frontière de chunk de deque sur ESP32 (512/32 = 16 éléments).
    static const size_t Ns[]    = {1, 4, 8, 16, 17, 24, 30};
    static const size_t NsBig[] = {1, 4, 8, 12};        // 830 B : 30 x 830 noie un ESP8266

    // a — kNode seul : pas de payload, topic en SSO sur les deux cibles (8 car.)
    series("a  kNode", 0, "aa/bb/cc", Ns, 7);
    // b — kOverhead + 64
    series("b  +64B",  64, "aa/bb/cc", Ns, 7);
    // c - kOverhead, against a hypothetical minimum block (there is none)
    series("c  +4B",    4, "aa/bb/cc", Ns, 7);
    // d — la marche SSO et la regle (len+16)&~0xf, payload nul
    series("d  t9",   0, "aa/bb/cc9",            Ns, 7);
    series("d  t10",  0, "aa/bb/cc90",           Ns, 7);
    series("d  t11",  0, "aa/bb/cc901",          Ns, 7);
    series("d  t14",  0, "aa/bb/cc901234",       Ns, 7);
    series("d  t15",  0, "aa/bb/cc9012345",      Ns, 7);
    series("d  t16",  0, "aa/bb/cc90123456",     Ns, 7);
    series("d  t22",  0, "aa/bb/cc9012345678901",Ns, 7);
    // e - control: the reference event
    series("e  ref830", 830, "mqtt/publish", NsBig, 4);


    // #8: does the budget hold in the queue on this board, or does the OOM come
    // before the drop? Fill to the budget with reference events.
    Serial.println("\n[#8] fill up to the budget");
    {
        bus.reset();
        for (int i = 0; i < 8; ++i) bus.poll();
        const uint32_t before = freeHeap();
        std::vector<uint8_t> big(830, 0x5A);
        size_t n = 0;
        uint32_t low = before;
        while (bus.getDroppedCount() == 0 && n < 64) {
            bus.publish(String("mqtt/publish"), big.data(), big.size());
            n++;
            const uint32_t f = freeHeap();
            if (f < low) low = f;
            if (f < 4096) { Serial.printf("   STOP at n=%u: heap %lu B\n", (unsigned)n, (unsigned long)f); break; }
        }
        Serial.printf("   budget=%u B  events before the first drop=%u  heap %lu -> %lu (floor %lu)\n",
                      (unsigned)QueueCost::kBudgetBytes, (unsigned)n,
                      (unsigned long)before, (unsigned long)freeHeap(), (unsigned long)low);
        Serial.printf("   queuedBytes=%u  high-water=%u%%  drops=%lu\n",
                      (unsigned)bus.getQueuedBytes(), (unsigned)bus.getQueueHighWaterPct(),
                      (unsigned long)bus.getDroppedCount());
        for (int i = 0; i < 80; ++i) bus.poll();
        Serial.printf("   after drain: queuedBytes=%u  heap %lu\n",
                      (unsigned)bus.getQueuedBytes(), (unsigned long)freeHeap());
    }

    // #9: an oversized event must be refused BEFORE publish() copies its payload
    // onto the heap. The net heap is identical either way — the temporary is
    // freed — so the discriminator is a sample taken INSIDE the refusal frame.
    // The refusal logs, and a logger callback fires from that frame: with the
    // guard at the entry point nothing has been allocated yet; with the guard
    // only in enqueue(), qe.data already holds the whole payload.
    Serial.println("\n[#9] where the oversized refusal happens");
    {
        bus.reset();
        for (int i = 0; i < 8; ++i) bus.poll();
        const size_t OVER = QueueCost::kBudgetBytes + 1;
        std::vector<uint8_t> over(OVER, 0x5A);

        g_heapInRefusal = 0;
        const uint32_t before = freeHeap();
        LoggerCallbacks::CallbackId id = LoggerCallbacks::addCallback(
            [](uint8_t, const char*, const char*) { g_heapInRefusal = freeHeap(); });
        bus.publish(String("probe/oversized"), over.data(), over.size());
        LoggerCallbacks::removeCallback(id);

        const long dip = (long)before - (long)g_heapInRefusal;
        Serial.printf("   payload=%u B  heap before=%lu  in refusal=%lu  dip=%ld\n",
                      (unsigned)OVER, (unsigned long)before,
                      (unsigned long)g_heapInRefusal, dip);
        Serial.printf("   verdict: %s (dropped=%lu, queuedBytes=%u)\n",
                      (dip < (long)OVER / 2) ? "refused before the copy"
                                             : "THE PAYLOAD WAS COPIED FIRST",
                      (unsigned long)bus.getDroppedCount(), (unsigned)bus.getQueuedBytes());
    }

    Serial.println("\n===== end =====");
}

void loop() { delay(1000); }
