/**
 * BUG-41 — calibration of QueueCost, one constant per series (spec §7).
 *
 * The queue is measured undrained and then given back, the way firmware does:
 * a series that never drains measures occupancy, and the drain line is what
 * says the occupancy was held rather than lost (the STOR-ESP-1 lesson).
 *
 * The shipped EventBus still caps at 32 entries, so every N stays under it.
 * The staircase across N is the measurement: its slope is the per-event cost,
 * its steps are the deque chunk.
 */
#include <Arduino.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Utils;

static EventBus bus;

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
    Serial.printf("   %4s %10s %10s %8s %9s\n", "N", "fill(B)", "per evt", "residu", "rendu%");
    long prevN = 0, prevFill = 0;
    for (size_t k = 0; k < nN; ++k) {
        Sample s = point(Ns[k], payload, topic);
        long fill = (long)s.before - (long)s.afterFill;
        long residue = (long)s.before - (long)s.afterDrain;
        long given = fill - residue;
        // pente locale : ce que coûtent les événements ajoutés depuis le point précédent
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
    Serial.println("\n\n===== BUG-41 QueueCost calibration =====");
#if defined(DOMOTICS_PLATFORM_ESP32)
    Serial.println("plateforme: ESP32");
#else
    Serial.println("plateforme: ESP8266");
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
    // c — kOverhead + kMinBlock
    series("c  +4B",    4, "aa/bb/cc", Ns, 7);
    // d — la marche SSO et la regle (len+16)&~0xf, payload nul
    series("d  t9",   0, "aa/bb/cc9",            Ns, 7);
    series("d  t10",  0, "aa/bb/cc90",           Ns, 7);
    series("d  t11",  0, "aa/bb/cc901",          Ns, 7);
    series("d  t14",  0, "aa/bb/cc901234",       Ns, 7);
    series("d  t15",  0, "aa/bb/cc9012345",      Ns, 7);
    series("d  t16",  0, "aa/bb/cc90123456",     Ns, 7);
    series("d  t22",  0, "aa/bb/cc9012345678901",Ns, 7);
    // e — controle : l'evenement de reference
    series("e  ref830", 830, "mqtt/publish", NsBig, 4);


    // Mesure #8 : le budget tient-il en file sur cette carte, ou l'OOM precede-t-il
    // le drop ? On remplit jusqu'au budget avec des evenements de reference.
    Serial.println("\n[#8] remplissage jusqu'au budget");
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
            if (f < 4096) { Serial.printf("   ARRET a n=%u : heap %lu B\n", (unsigned)n, (unsigned long)f); break; }
        }
        Serial.printf("   budget=%u B  evenements avant 1er drop=%u  heap %lu -> %lu (plancher %lu)\n",
                      (unsigned)QueueCost::kBudgetBytes, (unsigned)n,
                      (unsigned long)before, (unsigned long)freeHeap(), (unsigned long)low);
        Serial.printf("   queuedBytes=%u  high-water=%u%%  drops=%lu\n",
                      (unsigned)bus.getQueuedBytes(), (unsigned)bus.getQueueHighWaterPct(),
                      (unsigned long)bus.getDroppedCount());
        for (int i = 0; i < 80; ++i) bus.poll();
        Serial.printf("   apres drain : queuedBytes=%u  heap %lu\n",
                      (unsigned)bus.getQueuedBytes(), (unsigned long)freeHeap());
    }

    Serial.println("\n===== fin =====");
}

void loop() { delay(1000); }
