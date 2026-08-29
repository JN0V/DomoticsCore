---
title: 'MEM-2 closing lot: fix the WiFi scan concatenation, resolve the other rows, correct the SSO constant'
type: 'refactor'
created: '2026-08-29'
status: 'done'
baseline_commit: 'baf5151d5a868f0f3338e7c3fd42b77eb681def1'
review_loop_iteration: 0
context:
  - '{project-root}/_bmad-output/specs/spec-mem-2-cold-rows/SPEC.md'
  - '{project-root}/_bmad-output/specs/spec-mem-2-cold-rows/rows.md'
  - '{project-root}/CLAUDE.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** MEM-2's eight remaining rows were reasoned against a threshold that is wrong on the board they matter on — `String`'s small-string buffer is 10 characters on ESP8266, not 14 — and six sentences of the merged roadmap assert the wrong number. On the corrected count, one site is worth fixing (the WiFi scan concatenation, which exists in two places, not one), one is worth a one-line change, and six rows are not defects.

**Approach:** Fix both scan loops and measure the fix on a board with instruments that already work. Change the help handler to a stored literal. Resolve the remaining six rows in the roadmap by argument — on cadence, on cost, by re-pointing, or by reclassification — correct the threshold everywhere it is asserted, and close MEM-2 with the three-criteria sweep rather than by re-summing.

## Boundaries & Constraints

**Always:** State which core a character count is about — 10 on ESP8266 (`WString.h:309-316`), 14 on ESP32 (`WString.h:299-305`). Keep the help handler's returned text byte-for-byte identical. `rm -rf .pio` before any run whose result is quoted, including both directions of the removal check. Identify the board by adapter (`/dev/serial/by-id/*FTDI_FT232R_USB_UART_A5069RR4*`), never by device node. English conventional commit.

**Ask First:** Adding a `char*` overload to the WiFi HAL; deleting or deprecating any public method; changing any `library.json` version; touching files outside DomoticsCore-Wifi, DomoticsCore-RemoteConsole, `ci.yml` and `docs/CODE-ROADMAP.md`.

**Never:** Build an allocation-counting harness — out of scope, its own lot if wanted. Do not fix `getDetailedStatus`, `dumpContents`, `OTA::transition()`, the System NTP rows, `getLEDStatus` or the `BaseWebUIComponents` helpers; they are resolved by argument. Do not flash or run anything on a board in this pass. No assertion whose subject is free heap measured after the loop returns — it passes with the fix removed.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|---|---|---|---|
| Async scan, networks found | `res` networks pending | `lastScanSummary_` identical text to today, built through a stack buffer | N/A |
| Async scan, zero networks | `res == 0` | Empty summary, loop not entered | N/A |
| Async scan failure | `res < 0` | `"Scan failed"`, unchanged | Existing branch |
| More than ten networks | `res > 10` | Summary caps at ten entries, as today | N/A |
| Long SSID | 32-character SSID | Entry not truncated by the stack buffer | Buffer sized for 32 + suffix |
| `scanNetworks()` | `n` networks | Same vector contents as today, one fewer copy per entry | N/A |
| `help` command | any registered command set | Text byte-for-byte what it returns today | N/A |
| Device suite, no networks | scan returns 0 | Test fails loudly, naming the empty scan | Must not pass empty |

</frozen-after-approval>

## Code Map

- `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h:305-308` -- the async summary loop; `summary +=` with a three-deep temporary chain per entry.
- `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h:508-512` -- **the second, identical expression**, inside public `scanNetworks()`; also `push_back(network)`, a copy a `std::move` removes.
- `DomoticsCore-Wifi/include/DomoticsCore/Wifi_HAL.h:92-93`, `Wifi_ESP8266.h:71-72` -- `getScannedSSID` returns `String` **by value**: one allocation per network survives by design, and changing that is Ask-First.
- `DomoticsCore-Wifi/platformio.ini` -- **declares only `[env:native]`**. The board environment does not exist and must be created, following `DomoticsCore-Storage/platformio.ini:18-40` (platform, `board = nodemcuv2`, `test_filter`), with `test_ignore` added to `[env:native]`.
- `DomoticsCore-Wifi/test/test_wifi_component/test_wifi_component.cpp:228-232` -- calls `startScanAsync()` and never `loop()`, so the loop body is untested today; `:508` exercises `scanNetworks()`.
- `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h:396-417` -- ten literal appends then a per-command loop; `registerBuiltInCommands()` is private and the lambda is reachable only through the telnet read loop, so there is nothing to measure and the roadmap must say so.
- `DomoticsCore-Storage/test/test_heap_esp8266/test_heap_esp8266.cpp` -- device-suite shape to copy: fixture struct, warm-up before the first checkpoint, non-vacuity assert first, figures carried in the assert message, Arduino `setup()`/`loop()` runner.
- `.github/workflows/ci.yml:210-217` -- the hard-coded on-device list, currently six projects; `:78-84` the native discovery loop that would compile a device suite for the host without `test_ignore`.
- `docs/CODE-ROADMAP.md` -- `:526` the section heading ("across 9 components"), `:534-544` the row table, `:545` the Fix line, `:555-558` and `:564-565` and `:580` the wrong-threshold sentences, `:1892` the Memory Safety row with its stale "nine cold rows", `:1901` the totals, `:1904` the sentence enumerating the seven HIGH.

## Tasks & Acceptance

**Execution:**
- [ ] `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h` -- both scan loops: `snprintf` each entry into a stack buffer, one `reserve()` on the accumulator, `std::move` into the vector at the second site -- removes the temporary chain and the per-16-byte growth on the only site a user can trigger repeatedly.
- [ ] `DomoticsCore-Wifi/platformio.ini` -- create `[env:esp8266dev]` with `test_filter`, add `test_ignore` to `[env:native]` -- the project has no board environment today.
- [ ] `DomoticsCore-Wifi/test/test_wifi_scan_esp8266/` -- new device suite: `ESP.getCycleCount()` around the loop and an in-frame heap sample at the last iteration, failing loudly when the scan returns nothing -- the measurement, shipped as a suite.
- [ ] `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` -- help handler builds its constant part from one stored literal; per-command loop and returned text unchanged.
- [ ] `.github/workflows/ci.yml` -- add `DomoticsCore-Wifi` to the on-device list -- otherwise nothing compiles the new suite.
- [ ] `docs/CODE-ROADMAP.md` -- resolve all eight rows per `rows.md`, correct every sentence asserting a 14-character threshold on both cores, file the two new findings (OTA's real 1 Hz cost; the unused-but-public helpers with their major-version constraint), and close MEM-2 across all five dependent statements listed in the Code Map.

**Acceptance Criteria:**
- Given a cleared `.pio`, when `pio test -e native` runs in DomoticsCore-Wifi and DomoticsCore-RemoteConsole, then every existing suite passes and the device suite is not built.
- Given the help handler, when its output is compared against the pre-change text, then it is byte-for-byte identical including the per-command lines.
- Given `pio test -e esp8266dev --without-uploading --without-testing` in DomoticsCore-Wifi, then only the new suite compiles.
- Given the roadmap after the edit, when the three-criteria sweep runs over every `[HIGH]` heading, then MEM-2 is resolved and no sentence anywhere still lists it among the open HIGH items.

## Design Notes

The measurement replaces an instrument the adversarial pass killed. `String` allocates through `realloc`, never `malloc`, and `-Wl,-wrap` would have redirected the SDK's IRAM allocation path and counted the WiFi stack during the very scan being measured. What is left measures the work rather than the allocations:

```cpp
uint32_t before = ESP.getCycleCount();
// ... the scan loop under test ...
uint32_t cycles = ESP.getCycleCount() - before;   // reallocation + copy work
```

plus a free-heap sample taken at the last iteration, while the accumulator and any live temporaries still exist. Both need `rm -rf .pio` on either side of the removal check: these are header-only components pulled through `file://`, so a reverted header that is not recopied reports the fixed figure twice.

## Verification

**Commands:**
- `cd DomoticsCore-Wifi && rm -rf .pio && pio test -e native` -- expected: existing suites green, device suite absent
- `cd DomoticsCore-RemoteConsole && rm -rf .pio && pio test -e native` -- expected: green
- `cd DomoticsCore-Wifi && pio test -e esp8266dev --without-uploading --without-testing` -- expected: compiles; this does not upload, unlike `--without-testing` alone
- `cd DomoticsCore-System/examples/FullStack && pio run -e esp8266dev` -- expected: builds; record RAM and flash for the commit message

**Manual checks (if no CLI):**
- The board run is not in this pass. Report that the `nodemcuv2` run and the removal check against the reverted loops both still need hardware.

## Suggested Review Order

**The rewrite**

- The entry point: `snprintf` into a stack buffer, one reservation, same text.
  [`Wifi.h:330`](../../DomoticsCore-Wifi/include/DomoticsCore/Wifi.h#L330)

- The second site the original enumeration missed — and the `std::move` that ends the copy.
  [`Wifi.h:553`](../../DomoticsCore-Wifi/include/DomoticsCore/Wifi.h#L553)

- Pre-existing crash guard: `WIFI_SCAN_FAILED` is -2, and the old check only caught -1.
  [`Wifi.h:536`](../../DomoticsCore-Wifi/include/DomoticsCore/Wifi.h#L536)

- One stored literal replacing ten run-time appends; per-command loop untouched.
  [`RemoteConsole.h:396`](../../DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h#L396)

**The measurement**

- The in-frame sample: 40 B held before the fix, 8 or fewer after, on a board.
  [`test_wifi_scan_esp8266.cpp:357`](../../DomoticsCore-Wifi/test/test_wifi_scan_esp8266/test_wifi_scan_esp8266.cpp#L357)

- The non-vacuity gates: an empty scan and a short last entry both fail loudly.
  [`test_wifi_scan_esp8266.cpp:229`](../../DomoticsCore-Wifi/test/test_wifi_scan_esp8266/test_wifi_scan_esp8266.cpp#L229)

**The regression net that runs in CI**

- The stub became scriptable, so the loop body finally executes on the host.
  [`Wifi_Stub.h`](../../DomoticsCore-Wifi/include/DomoticsCore/Wifi_Stub.h)

- Eight native cases pinning the format, the join, the cap and both failure branches.
  [`test_wifi_component.cpp:502`](../../DomoticsCore-Wifi/test/test_wifi_component/test_wifi_component.cpp#L502)

**Wiring and record**

- `test_filter` on a board environment this project did not have at all.
  [`platformio.ini`](../../DomoticsCore-Wifi/platformio.ini)

- The suite joins the job that compiles what no runner can execute.
  [`ci.yml:216`](../../.github/workflows/ci.yml#L216)

- What DONE covers, what the board measured, and what the async loop still does not.
  [`CODE-ROADMAP.md:530`](../../docs/CODE-ROADMAP.md#L530)
