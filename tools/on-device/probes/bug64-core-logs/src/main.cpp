// Board probe: which core log lines a hook can actually see, and through which
// mechanism. Three sinks are separated here, because the filed fix assumes one:
//   - a vprintf hook (esp_log_set_vprintf) sees esp_log_write, which is what the
//     precompiled ESP-IDF libraries call;
//   - an Arduino build remaps ESP_LOGx onto ARDUHAL, so a line written in THIS
//     translation unit goes to log_printf -> ets_printf and misses the hook;
//   - ets_install_putc1 sees everything ets_printf writes, character by
//     character, from any context.
#include <Arduino.h>
#include <esp_log.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <rom/ets_sys.h>
#include <rom/uart.h>
#include <driver/gpio.h>

static vprintf_like_t previous = nullptr;
static volatile int hookLines = 0;
static volatile int putcLines = 0;
static volatile bool putcActive = false;

static int captureHook(const char* fmt, va_list args) {
    hookLines++;
    return previous ? previous(fmt, args) : 0;
}

// Forwards every character to the UART so nothing is lost, and counts lines.
static void capturePutc(char c) {
    if (c == '\n') putcLines++;
    uart_tx_one_char((uint8_t)c);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println("PROBE start");

    previous = esp_log_set_vprintf(&captureHook);

    // --- 1. lines written here: remapped onto ARDUHAL by the Arduino core ----
    ESP_LOGE("probe", "an esp-idf macro in our own translation unit");
    log_e("an arduhal line");
    Serial.printf("PROBE after-own-lines hook=%d\n", hookLines);

    // --- 2. lines written inside precompiled ESP-IDF libraries --------------
    // These three return an error without logging anything, which is why the
    // section below is the one that discriminates: driver/gpio.c logs through
    // the IDF's own macros on an invalid pin.
    esp_err_t a = esp_ota_set_boot_partition(nullptr);
    esp_ota_handle_t h = 0;
    esp_err_t b = esp_ota_begin(nullptr, 0, &h);
    esp_err_t c = nvs_flash_init_partition("no_such_part");
    Serial.printf("PROBE silent-idf-calls %d %d %d hook=%d\n", (int)a, (int)b, (int)c, hookLines);

    esp_err_t g = gpio_set_direction((gpio_num_t)99, GPIO_MODE_OUTPUT);
    Serial.printf("PROBE gpio-in-library returned %d hook=%d\n", (int)g, hookLines);

    // --- 3. the other mechanism: the character sink ets_printf writes to ----
    putcActive = true;
    ets_install_putc1(&capturePutc);
    log_e("an arduhal line, with putc1 installed");
    Preferences prefs;
    prefs.begin("a_namespace_name_far_too_long", false);
    esp_err_t g2 = gpio_set_direction((gpio_num_t)98, GPIO_MODE_OUTPUT);
    Serial.printf("PROBE gpio-with-putc returned %d putc=%d hook=%d\n", (int)g2, putcLines, hookLines);
    Serial.printf("PROBE putc lines=%d hook=%d\n", putcLines, hookLines);
    Serial.println("PROBE done");
}

void loop() { delay(1000); }
