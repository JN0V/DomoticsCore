# Observing a device in production

How to build, configure and read a device so that a reboot, a crash or a
memory failure explains itself on the next boot, from the broker or the
console, without anyone watching the serial line. Everything below is
shipped in the library; this page says what to switch on and where to look.
The mechanisms themselves are described in the Core reference
(FlightRecorder) and the System reference (Boot Diagnostics, Telemetry,
Core Dump API).

## What you get

| Sink | Content | Platform | Needs |
|---|---|---|---|
| Serial line at boot | the last death, decoded (`bootdiag` block), the build id | all | `Core::begin()` |
| RTC memory | the flight record: heap trend over the last ~80 min, phase, registers, failed allocations | all | `Core::begin()` and `Core::loop()` |
| Storage `bootdiag` | this boot's figures and the last death, with a count of identical deaths | all | `System` + Storage + SystemInfo |
| `{clientId}/crash`, retained | the last death, published at every MQTT connect | all | `System` + MQTT |
| `{clientId}/telemetry`, every 60 s | heap, largest block, minimum, trough, uptime, boot count, RSSI, allocation failures | all | `System` + MQTT |
| Home Assistant | eight `diagnostic` sensors; `sys_last_death` carries the crash payload as attributes | all | `System` + MQTT + HomeAssistant |
| `GET /api/system/coredump` | the panic's core dump, decodable to a backtrace | ESP32 (exercised on a WROOM-32D; the C3 has the partition, the route has not been run there) | `System` + WebUI + a `coredump` partition |
| Console `bootdiag` | everything above, as text | all | RemoteConsole |

A sketch that drives `Core` without `System` gets the first two rows and
nothing on the wire.

## Before you flash

1. **Use `System`**, with the MQTT component and, for entities, the
   HomeAssistant component. Telemetry and the crash topic live there.
2. **Give every build an id and keep its ELF.** Pass
   `-DDOMOTICS_BUILD_ID='"2026-09-15-a"'` (the quotes are part of the flag)
   in `build_flags`; the id is logged at boot, its CRC32 rides in the
   record, and the crash payload's `build` field names it. Keep
   `.pio/build/<env>/firmware.elf` under that id: a core dump decodes only
   against the ELF of the build that panicked, and an ESP8266 `epc1`
   resolves to a line only with it. A device you cannot walk to has no
   other way back to the source.
3. **Build with `-g`.** No RAM cost; the ELF carries the file and line
   `addr2line` needs.
4. **ESP8266: add `-DDEBUG_ESP_OOM`** to `build_flags` (it has to reach
   every library). Without it the recorder counts only `operator new` and
   newlib failures; `String`, ArduinoJson, lwIP and the SDK allocate
   through paths that record nothing. Cost: +1 316 B flash, +840 B IRAM.
   `-DDEBUG_ESP_HWDT` adds a stack dump on the boot after a hardware
   watchdog, at +1 384 B flash and a greeting at 74 880 baud on every boot.
5. **ESP8266: choose the components.** The full set (every component with
   the WebUI) boots and never joins a network: the heap settles under
   2 KB. The set that has run with telemetry is System, RemoteConsole,
   Storage, SystemInfo, MQTT, Wifi and LED, at about 42 KB of RAM used
   at build. HomeAssistant with telemetry on an ESP8266 has not been
   measured; its discovery packets are bounded by the client's 768-byte
   buffer.
6. **ESP32: a partition table with a `coredump` partition.** The Arduino
   `default.csv` and `min_spiffs.csv` both have one (64 KB at
   `0x3F0000`); a custom table without it keeps no dump, and the crash
   payload's `coredump` field says so.
7. **ESP32: the Arduino core version.** The loop watchdog
   (`SystemConfig::loopWatchdogSeconds`, 30 s) arms on Arduino core 2.x
   (ESP-IDF 4). On core 3.x (IDF 5) it compiles with a `#warning` and arms
   nothing, and the boot log says so: a `loop()` that blocks forever stays
   blocked, with no reboot and no record, because the Arduino loop task
   is not on the task watchdog by default. A 3.x device needs its own
   watchdog until that port lands.
8. **An MQTT client id of at most 80 characters, without `+` or `#`.**
   Telemetry refuses to configure itself otherwise, with one warning.
9. **A sketch that defines its own `custom_crash_callback`** (ESP8266)
   fails to link: the library defines it. Either drop yours and chain
   through `FlightRecorder::instance().onCrash(...)`, or build with
   `-DDOMOTICS_CRASH_HOOKS=0` and lose the crash registers.

A `platformio.ini` fragment that does 2, 3 and 4 for an ESP8266 build:

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DDEBUG_ESP_OOM
    -g
    -DDOMOTICS_BUILD_ID='"2026-09-15-a"'
```

## Configuration

Defaults do the work; these are the fields that matter.

| Field | Default | Effect |
|---|---|---|
| `SystemConfig::mqttClientId` | empty, so the MQTT component generates one | prefixes both topics; set it to something stable and readable |
| `SystemConfig::telemetryIntervalSec` | 60 | the tick; `0` turns telemetry and the crash topic off |
| `SystemConfig::telemetryHeapFloor` | 4096 | a tick with less allocatable heap is skipped and counted |
| `SystemConfig::loopWatchdogSeconds` | 30 | ESP32: a stuck `loop()` becomes a panic with a core dump; `0` off |
| `SystemInfoConfig::enableBootDiagnostics` | true | reset reason, ESP8266 exception registers, core dump status at boot |
| Storage enabled | yes | `boot_count` and `bootdiag` persist; two writes per boot |

`SystemConfig::firmwareVersion` is what the Home Assistant device block
shows as software version; it is a display string, not the build id.

## Where to look, in order

When a device has rebooted:

1. **The retained crash topic.** `{clientId}/crash` holds the last death
   even while the device is offline. `promotion` says why the record was
   kept (`crash callback`, `unexpected reset`, `software reset not
   requested by the firmware`), `phase` which component was initializing
   (a number; the boot log and `bootdiag` name it for that build),
   `build` which firmware, `uptime_ms` how long it ran, `same_count` how
   many boots recorded this identical death, `reason` and `epc1` the
   platform's own words. In Home Assistant the same payload is the
   attributes of `sys_last_death`, which stays available when the device
   goes offline.
   ```
   uv run tools/on-device/mqtt_watch.py <broker> --topic '<clientId>/#' --for 10
   ```
2. **The telemetry before the death.** `sys_heap_min` and `free_heap` in
   Home Assistant's history show a leak over hours or days; the flight
   record's rings show the last 80 minutes at 10-second and 10-minute
   resolution, printed by `bootdiag`. A rising `sys_alloc_failures` on a
   device that is not yet dead is the warning.
3. **`bootdiag` on the console** (telnet, `auth <password>` if set):
   the promoted record decoded, the component behind the phase marker,
   this boot's diagnostics, the persisted blob. This is also where a
   device that never reconnects keeps its record.
4. **ESP32: the core dump.** `bootdiag` and the crash payload say whether
   one waits. Download, decode against the kept ELF, then erase:
   ```
   python3 tools/on-device/coredump_check.py http://<device> --user admin --password <pw> \
       --elf <kept firmware.elf> --erase
   ```
   or by hand: `GET /api/system/coredump` (saved as `coredump.bin`) and
   `esp-coredump info_corefile --core coredump.bin --core-format raw firmware.elf`.
   The dump's header carries the app's SHA-256; a mismatch with the ELF
   is reported, not decoded.
5. **ESP8266: `epc1` to a source line.**
   `xtensa-lx106-elf-addr2line -e <kept firmware.elf> 0x<epc1>`; the
   failed allocation's `fail_caller` resolves the same way.

## What you will not see

- **Power loss and brownout** leave no record; a brownout has never been
  measured on the bench.
- **ESP32: an EN-pin reset** clears the record and reads as a power-on.
  The ESP32 record has no exception registers: the crash site comes
  from the core dump only.
- **ESP8266: a hardware watchdog** leaves the phase marker, the last
  tick and a single `epc1` sample; `abort()`, `assert` and an
  out-of-memory `new` reach the SDK's reset reason as a software reset,
  and only the crash callback tells them apart (so keep
  `DOMOTICS_CRASH_HOOKS` on).
- **A crash loop** keeps the first death until a boot gets far enough to
  persist it; the deaths in between are not counted. A device that dies
  before `System::begin()` completes writes nothing to Storage and
  publishes nothing; the console still has the RTC record if the device
  ever stays up long enough to answer.
- **A device that never reconnects** never publishes; its record is in
  RTC and, if a boot got that far, in `bootdiag`.
- **Nothing on the device holds more than 80 minutes of heap history.**
  A slow leak is a job for the telemetry in Home Assistant's recorder,
  or for a subscriber on the broker.
- **Telemetry is QoS 0 and unretained**; a sample the transport refuses
  (offline, rate-limited, oversize) is dropped and counted in `failed`.
  The retained crash topic is the only message that waits.
- **Torn record**: when the CRC fails, only `phase` is reported and the
  crash payload carries `"torn":1`.

## Cost on the device

Recorder tick and phase marker: 46–88 µs per idle loop on an ESP8266
(the spread belongs to the link layout, not the recorder), unmeasurable
against the ESP32's loop. Telemetry: one 140–150 byte MQTT publish per
minute, built on the stack. Storage: two writes per boot. ESP8266 with
telemetry and the crash topic: about 1.2 KB more `.rodata` in DRAM than
without.
