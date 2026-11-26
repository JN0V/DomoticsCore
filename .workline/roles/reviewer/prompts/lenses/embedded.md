---
# The judge of an important finding of this lens: asked whether the code
# fails on the boards this library targets, not on the host.
judge:
  question: >-
    Is this finding about the code true on the boards this library targets
    (ESP32, ESP32-C3, ESP8266 with about 40 KB of free heap): does the code
    quoted, as it reads, waste or exhaust memory, overflow a task's stack,
    hold up the main loop, or fail to build or misread flash on one of the
    boards, the way the finding says? Answer no if the code runs only on
    the host (a native test, a tool), if the size is bounded and small, or
    if the finding only guesses; no too for an allocation or a `delay()`
    that runs once at start-up.
---
**Embedded.** This library runs on ESP32, ESP32-C3 and ESP8266 boards, in
Arduino's `setup()` and `loop()`, for weeks without a reboot. Read what the
change adds or touches as code on a board, not on the host:

- **Heap fragmentation**: `new`, `malloc`, `std::vector`/`std::string`
  growth, `String` built or copied in `loop()`, a callback or a handler
  that runs again and again; a buffer that could be reserved once, or
  fixed in size.
- **`String` concatenation**: `+` or `+=` in a loop or a hot path, each a
  new allocation; a `String` passed by value; `String(x).c_str()` kept
  past its statement.
- **Flash, not RAM**: a constant string literal or table left in RAM
  where `F("…")`, `PROGMEM` or `const` in flash would do (ESP8266 above
  all, where literals live in RAM); a `PROGMEM` value read without
  `pgm_read_*` or `_P` functions. A `DLOG_*` format string is already in
  flash on ESP8266 (`DLOG_SNPRINTF` wraps it in `PSTR`): never a finding.
- **Stack size**: a large local array or struct, recursion, or a deep
  call in a FreeRTOS task, a timer or an interrupt callback, against the
  task's stack (8 KB for `loop()` on ESP32, about 4 KB on ESP8266).
- **ESP8266 against ESP32**: an API, a header or a feature present on one
  only (FreeRTOS tasks, `xTaskCreate`, mutexes, `esp_*` calls, a second
  core: the ESP32-C3 has one) used without a guard for the other; a
  buffer sized for the ESP32's heap that the ESP8266 cannot hold.
- **Blocking calls in `loop()`**: `delay()`, a busy wait, a network or
  flash call with no timeout, a `while` waiting on a peer; on ESP8266 a
  loop that never yields trips the watchdog, and `delay()` or `yield()`
  in an async web server's handler panics.

For each, say on which board it fails, and how: heap exhausted, a
fragment too small for the next allocation, a stack overflow, a
watchdog reset, a missed event.
