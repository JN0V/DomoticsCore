# On-device harness

Three scripts for running the shipped examples on real boards and reporting what
actually happened. Written for the 2026-08-27 real-conditions campaign, which
climbed from `01-CoreOnly` to `FullStack` on a `nodemcuv2` and an ESP32
WROOM-32D and produced SEC-9, CI-14 and DOC-1.

Nothing here runs in CI. CI proves the examples compile; these prove they work.

## `readserial.py`

Reset a board into its application and capture serial output.

```
python3 tools/on-device/readserial.py /dev/ttyUSB0 45 [--quiet-stop 5]
```

`pio device monitor` needs a TTY, which a non-interactive agent does not have.
More usefully, this **reads the ESP32-CAM**, which `pio test -e esp32cam` cannot:
for a normal application boot GPIO0 must be HIGH as reset rises — `DTR=False`,
then pulse `RTS`. The esptool sequence does the opposite and lands in the
bootloader, which is why that board comes back in `DOWNLOAD_BOOT`.

Note that PlatformIO filters serial down to Unity lines, so anything a test
prints outside an assertion message is invisible through `pio test`. Read the
port directly when the output matters.

## `run-example.sh`

Build, flash and capture one example on one board.

```
tools/on-device/run-example.sh DomoticsCore-Wifi/examples/BasicWifi esp8266dev /dev/ttyUSB1 40
```

Puts the repository root on the include path so examples pick up an untracked
`secrets.h` through `__has_include`. Logs land beside the script as
`log-<example>-<env>.txt`.

## `webui_check.py`

Drive a WebUI with a real browser and report console errors, failed requests and
per-response status.

```
uvx --with playwright python tools/on-device/webui_check.py http://192.168.1.224/ /tmp/out --click-toggles
```

Needs `uvx --from playwright playwright install chromium` once.

Loading a page is not evidence, which is the reason this exists rather than a
`curl`. It asserts the body rendered something, so a blank page served with 200
cannot pass — and the toggle click is driven through the visible label, because
these are styled toggles whose real `<input>` is hidden and times out.

**Wait long enough.** The dashboard populates from an SSE broadcast that fires
roughly every 5.4 s. A three-second screenshot shows every field empty and looks
exactly like a broken dashboard; it cost an afternoon before the cause turned out
to be impatience. The default wait is 18 s for that reason.

## Credentials

Put a `secrets.h` at the repository root — it is gitignored:

```c
#define DC_WIFI_SSID     "..."
#define DC_WIFI_PASSWORD "..."
#define DC_MQTT_BROKER   "192.168.1.253"
#define DC_MQTT_PORT     1883
#define DC_MQTT_USER     ""
#define DC_MQTT_PASSWORD ""
// #define DC_OTA_PREFER_STA   // OTAWithWebUI: join a network instead of serving an AP
```

Emptying `DC_WIFI_SSID` puts every example back into access-point mode at once.

**Verify the file is actually read.** Every example falls back to a placeholder
through `#ifndef`, so a build that merely succeeds proves nothing — it may have
compiled the placeholder and be testing AP mode while you believe you are testing
WiFi. Drop a `#error` into `secrets.h` and confirm the compiler reports it from
the root path.

## Identify the board before assuming

Both FTDI adapters take `/dev/ttyUSB*` in plug order.

```
ls -l /dev/serial/by-id/
esptool --port /dev/ttyUSB0 chip-id
```

The port is `root:dialout` and a process only picks up group membership at
start, so a shell opened before that change cannot open it —
`sudo chmod 666 /dev/ttyUSB0` unblocks it until the next replug.
