// ESP8266: which sink, if any, the SDK's own narration passes through.
//
// Serial.begin() calls end(), which calls uart_set_debug(UART_NO): putc1 becomes
// uart_ignore_char and system_set_os_print(0). Whatever still reaches the UART
// after that does not go through putc1, and a component cannot intercept it.
#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <ets_sys.h>
#include <user_interface.h>
int uart_get_debug();
}

static volatile int p1Lines = 0;
static volatile int p1Chars = 0;

static void ICACHE_RAM_ATTR capturePutc1(char c) {
    p1Chars++;
    if (c == '\n') p1Lines++;
    while (((USS(0) >> USTXC) & 0xff) >= 0x7e) { }
    if (c == '\n') USF(0) = '\r';
    USF(0) = c;
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.printf("P8 start uart_get_debug=%d\n", uart_get_debug());

    // Take the sink AND enable SDK printing: the core disabled it in begin(),
    // so without this there is nothing on the wire to carry.
    ets_install_putc1((void (*)(char))&capturePutc1);
    system_set_os_print(1);

    ets_printf("P8 ets_printf line\n");
    Serial.printf("P8 after-ets_printf p1=%d chars=%d\n", p1Lines, p1Chars);
    printf("P8 libc printf line\n");
    Serial.printf("P8 after-libc-printf p1=%d chars=%d\n", p1Lines, p1Chars);
    os_printf("P8 os_printf line\n");
    Serial.printf("P8 after-os_printf p1=%d chars=%d\n", p1Lines, p1Chars);

    WiFi.mode(WIFI_STA);
    WiFi.begin("no_such_network_here", "no_such_password");
    for (int i = 0; i < 10; i++) {
        delay(1000);
        Serial.printf("P8 join t=%d p1=%d chars=%d\n", i, p1Lines, p1Chars);
    }
    Serial.println("P8 done");
}

void loop() { delay(1000); }
