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

## `ota_upload_check.py`

Drive a real `POST /api/ota/upload` and check what the device reports back.

```
python3 tools/on-device/ota_upload_check.py http://192.168.1.218 firmware.bin
```

TEST-8, hole 4. Every OTA test in the repository calls `beginUpload()` directly,
so nothing had ever constructed an HTTP request or seen a `multipart/form-data`
envelope — which is how SEC-9 stayed invisible to 54 native tests and 19
on-device ones until somebody uploaded a file by hand.

**The load-bearing case is the refused upload, not the accepted one.** A valid
image sent with a deliberately wrong digest is refused at the hash check, which
sits *after* SEC-9's narrowing and *before* the commit — so the device stays up,
does not reboot, and `/api/ota/status` remains readable. Measured on a
`nodemcuv2` against builds with and without the fix:

| | announced `Content-Length` | `total` reported |
|---|---|---|
| with SEC-9 | 475452 | **475264** — the firmware |
| without | 475452 | **475452** — the envelope |

`downloaded` reads 475264 either way and proves nothing here: a refused digest
never reaches `finalizeUpdateOperation()`, so it never hits the assignment that
would have overwritten it. Only `total` discriminates, and the script says so.

`--no-token` sends without the SEC-10 CSRF token: the expected result is a
403 before anything OTA-shaped runs, and that IS the check.

`--disconnect-at N` is BUG-35's check: it sends the headers and N bytes of
the multipart body over a raw socket, then closes abruptly (SO_LINGER 0 →
RST, the shape of the accident that filed the bug). The device must end in
`state=error` with the disconnect reason and accept a follow-up upload —
before the fix it froze in `downloading` and refused every retry with
"Upload already in progress" until a power-cycle. Note that `total` reads
the ENVELOPE size on an abort, by design: the SEC-9 narrowing runs at
finalize, which an abort never reaches — that is not a SEC-9 regression.
The follow-up upload deliberately carries a wrong digest so it is refused
at the hash check without rebooting anything: "SHA256 mismatch" is the
pass, "already in progress" is the lock.

`--commit` also sends a correctly-hashed copy, which the device installs and
reboots into. Uploading the image the board is already running makes that safe
and repeatable.

**It requires the device to stop answering before it answers again.** Waiting
only for a reply would pass just as well if the device had never rebooted — it
answers throughout. Verified by setting `autoReboot = false` in the example and
watching the check fail with *"the device never stopped answering"*.

### A staged eboot command outlives a serial reflash

After any successful upload on an ESP8266 the bootloader is armed, and it acts on
the **next reset** — including the one `esptool` performs at the end of a serial
flash. So the sequence

```
upload firmware A over HTTP   (staged, device does not reboot)
flash firmware B over serial  (written, then reset)
```

leaves the board running **A**, not B: eboot copied the staged image over the one
just written. It looks exactly like a flash that silently did not take, and the
build log says SUCCESS.

Flashing a second time works, because the command was consumed by the first
reset. Cost two flashes and one wrong conclusion on 2026-08-27 before
`/api/ota/status` was asked what the device thought its own config was.

Needs `DC_OTA_PREFER_STA` in `secrets.h` and `OTAWithWebUI` flashed, so the
endpoint is on the LAN rather than behind the device's own access point.

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

## `read_noreset.py` and `read_acm.py`

Two readers the OBS session (2026-09-05) needed and `readserial.py` cannot be:

```
python3 tools/on-device/read_noreset.py /dev/ttyUSB0 15   # do NOT reset: soak checks
python3 tools/on-device/read_acm.py     /dev/ttyACM0 60   # USB-Serial-JTAG (ESP32-C3)
```

`read_noreset.py` asserts DTR and RTS together before opening, which keeps EN
high on the NodeMCU-style auto-reset circuit — that is how the one-hour soak
read three boards without restarting them. `read_acm.py` is for a chip that
*is* the USB device (the C3's native USB-Serial-JTAG): the port disappears and
comes back on every reset, so it reopens in a loop instead of dying on the
first drop. A reset pulse on that port (`DTR=False; RTS=True; sleep; RTS=False`)
works through the JTAG unit's emulation, reads as reason `Unknown`, and keeps
`RTC_NOINIT_ATTR` — unlike the WROOM's EN reset.

## `probes/` — the OBS session's board probes

Not tests, not examples: throwaway sketches that produced the figures in the
roadmap's Priority 11 entries and in `spec-obs-crash-observability.md`. Kept so
the measurements can be repeated. Each has its own `platformio.ini` with
`file://` paths relative to the repository root; `rm -rf .pio` before a run.

| probe | what it measures |
|---|---|
| `obs-probe/` (`main-esp8266.cpp`, `main-esp32.cpp`, `main-esp32cam.cpp`, each with its `platformio-<target>.ini`; copy the pair to `platformio.ini` + `src/main.cpp` to run one) | the platform alone: what each death leaves for the next boot — abort, OOM in `new`, null dereference, soft/hardware WDT, restart, external reset; RTC survival; core dump presence; the failed-alloc hook. Steps advance through RTC; `START_STEP`/`PROBE_MAGIC` build flags pick where to start |
| `obs-lota-probe/` | a real `System` (Storage + SystemInfo) from the working tree: what the boot diagnostics carry, the old keys removed, the loop watchdog at 5 s abating a hang and silent at 0 |
| `obs-loopmax/` | FullStack with a stopwatch around `System::loop()`: the longest iteration idle and during an HTTP upload (OBS-7 residual 6); `LOOPMAX_WDT=0` disables the watchdog for a discriminating run; the upload it drives is what filed BUG-37 |

Credentials for `obs-loopmax` come from the repository's untracked `secrets.h`,
like the examples. Never paste its output with the network name in it.
