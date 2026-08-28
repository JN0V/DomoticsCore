---
title: 'MEM-2 hot half: HomeAssistant parses inbound MQTT messages without the allocator'
type: 'refactor'
created: '2026-08-28'
status: 'done'
baseline_commit: 'bea43842409658d6e2a9d7a3fb09edaf26d0aad7'
review_loop_iteration: 0
context:
  - '{project-root}/_bmad-output/specs/spec-mem-2-hot-path-strings/SPEC.md'
  - '{project-root}/_bmad-output/specs/spec-mem-2-hot-path-strings/evidence.md'
  - '{project-root}/CLAUDE.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `HomeAssistant.h:150` wraps `ev.topic` and `ev.payload` — already `char[]` — into `String`s, and `handleCommand` extracts the entity id with `topic.substring()`, all before `findEntity` decides the message is not ours. The topic always exceeds the 14-character SSO threshold both Arduino cores use, so every inbound MQTT message costs at least one heap allocation for a parse that needs none.

**Approach:** Convert the private `handleCommand` to `const char*` parameters and extract the id into a stack buffer, leaving the public virtual `HAEntity::handleCommand(const String&)` untouched. Prove it with an on-device ESP8266 suite that brackets HomeAssistant's own work between two free-heap samples taken inside a single EventBus dispatch.

## Boundaries & Constraints

**Always:** Preserve `stats.commandsReceived++` at its current position (after the unknown-entity return, before payload validation), both truncation warnings and their thresholds (63 for the id, 127 for the command), the alarm_control_panel code parsing, and the switch auto-publish. Keep the text of the unknown-entity warning byte-for-byte — the device suite matches on it. Any string a test expects to allocate must exceed 14 characters. English conventional commit.

**Ask First:** Adding public API to `HomeAssistantComponent` or `HAEntity`; changing any `library.json` version; touching files outside HomeAssistant, its tests, `ci.yml` and `docs/CODE-ROADMAP.md`.

**Never:** Change `HAEntity::handleCommand(const String&)` or add a `const char*` sibling to it — refused on shape, with the reason recorded in the contract. Do not touch SystemInfo code. Do not flash or run anything on a board in this pass. No net-heap-growth assertions: they pass with the fix removed.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|---|---|---|---|
| Known entity | `homeassistant/switch/node/sw1/set`, `ON` | Entity id `sw1`, virtual dispatch runs, `ha/command` emitted, switch auto-publishes | N/A |
| Unknown entity | topic naming an unregistered id | Warning logged with the id, no event, `commandsReceived` unchanged | Early return |
| No trailing slash | `homeassistant` | `Invalid topic format - no trailing slash`, no event | Early return |
| One slash only | `homeassistant/set` | `Invalid topic format - missing entity ID`, no event | Early return |
| Id over 63 chars | topic with a 70-character id | Event carries the id truncated to 63, warning names the overflow | Truncate + warn |
| Payload over 127 chars | 200-character payload for a known entity | `ev.command` truncated to 127, warning fired | Truncate + warn |
| Alarm command | `ARM_AWAY 1234` on an alarm entity | `ev.command` = parsed command, `ev.code` = `1234` | N/A |

</frozen-after-approval>

## Code Map

- `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h:149-151` -- the subscriber; passes `String(ev.topic)`, `String(ev.payload)`. Becomes `handleCommand(ev.topic, ev.payload)`.
- `…/HomeAssistant.h:611-676` -- `handleCommand`, private (below the `private:` marker at :~600) so its signature is not public API. Anchors: `:612` entry log, `:616`/`:622` the two `lastIndexOf` scans, `:628` the `substring`, `:630` the extracted-id log, `:633` the unknown-entity warning (**load-bearing text**), `:637` `commandsReceived++`, `:642` the virtual call, `:646-660` event fill and truncation warnings, `:663-676` alarm code and switch auto-publish.
- `…/HAEvents.h:33-38` -- `HACommandEvent { char entityId[64]; char component[32]; char command[128]; char code[32]; }`; sizes the stack buffer and both truncation thresholds.
- `…/HAEntity.h:93` -- `virtual bool handleCommand(const String&)`. Read-only.
- `DomoticsCore-Core/include/DomoticsCore/EventBus.h:28-34, :58` -- `QueuedEvent` holds a `std::vector<uint8_t>`, so the queue copy allocates; dispatch is registration order, which is what puts the baseline subscriber ahead of the component.
- `DomoticsCore-Core/include/DomoticsCore/Logger.h:20-64` -- `LoggerCallbacks::addCallback/removeCallback`; the in-frame sampling hook.
- `DomoticsCore-OTA/test/test_ota_component/test_ota_component.cpp:723-764` -- the one existing log-capture test; copy its rules (static capture, `removeCallback` before the first assert).
- `DomoticsCore-Storage/test/test_heap_esp8266/test_heap_esp8266.cpp` -- device-suite shape: fixture struct, warm-up before the first checkpoint, `core.loop()` drain, non-vacuity assert first, figures carried in the assert message, Arduino `setup()`/`loop()` runner.
- `DomoticsCore-Storage/platformio.ini:6, :25` -- `test_ignore` in native, `test_filter` in esp8266dev. HomeAssistant's own `platformio.ini` has neither, and has a third env (`esp32dev`) that also needs the ignore.
- `DomoticsCore-HomeAssistant/test/test_ha_r24_r26/test_ha_r24_r26.cpp:29-45, :93-103` -- `simulateMqttConnect` / `simulateEntityCommand` helpers and the raw `ha/command` subscriber; reuse verbatim.
- `.github/workflows/ci.yml:205-235` -- the hard-coded on-device list; `:78-84` the native discovery loop that would otherwise compile the device suite for the host.
- `docs/CODE-ROADMAP.md:526-547` -- MEM-2's table; CI-11's entry elsewhere in the same file.

## Tasks & Acceptance

**Execution:**
- [x] `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` -- convert `handleCommand` to `(const char* topic, const char* payload)`, scan the two slashes with `strrchr`/`memrchr`-style pointer work, copy the id into `char entityId[sizeof(HAEvents::HACommandEvent::entityId)]`, update the subscriber at `:150` -- removes every allocation on the parse path while the public virtual keeps its `String`.
- [x] `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp` -- add the two truncation cases (id over 63, payload over 127) asserting both the truncated event field and the captured `DLOG_W`, plus the two malformed-topic cases -- pins the behaviours the rewrite can silently drop.
- [x] `DomoticsCore-HomeAssistant/test/test_ha_heap_esp8266/test_ha_heap_esp8266.cpp` -- new device suite implementing the two-sample protocol from `evidence.md`, with a non-vacuity assert that the warning fired and the queue-cap check -- the measurement, shipped as a suite.
- [x] `DomoticsCore-HomeAssistant/platformio.ini` -- `test_ignore = test_ha_heap_esp8266` in `[env:native]` and `[env:esp32dev]`, `test_filter = test_ha_heap_esp8266` in `[env:esp8266dev]` -- without the ignores the required native job compiles the device suite for the host.
- [x] `.github/workflows/ci.yml` -- add `DomoticsCore-HomeAssistant` to the on-device suite list -- otherwise nothing ever compiles the new suite.
- [x] `docs/CODE-ROADMAP.md` -- mark MEM-2's two HomeAssistant rows done with the SSO-corrected counts, rewrite the SystemInfo row as refuted-on-cost citing `examples/BasicSystemInfo/src/main.cpp:26-40` and the 14-character threshold, amend CI-11 with what this lot supplies while leaving it open -- the roadmap must not keep claiming what was measured false.

**Acceptance Criteria:**
- Given a fresh `.pio`, when `pio test -e native` runs in `DomoticsCore-HomeAssistant`, then the five existing suites plus the new cases pass and the device suite is not built.
- Given the converted parse, when a message names an unregistered entity, then the two in-dispatch samples differ by zero on a `nodemcuv2`, and by the topic and id allocations when the conversion is reverted.
- Given `pio test -e esp8266dev --without-uploading --without-testing`, when run in `DomoticsCore-HomeAssistant`, then only the device suite compiles.

## Design Notes

The sampling hook is the logger, not a test entity: `HomeAssistantComponent` exposes no `addEntity()` for a custom subclass, and adding one would be new public API. `LoggerCallbacks` fires synchronously inside `handleCommand`, with every temporary live.

```cpp
// baseline: subscribed BEFORE the component, so it runs first in the same dispatch
core.getEventBus().subscribe(String(DomoticsCore::MQTTEvents::EVENT_MESSAGE),
    [](const void*) { heapAtDispatch = ESP.getFreeHeap(); }, nullptr);
// second sample: fires at HomeAssistant.h:633, topic and id both still alive
id = LoggerCallbacks::addCallback([](LogLevel lvl, const char* tag, const char* msg) {
    if (lvl == LOG_LEVEL_WARN && strstr(msg, "unknown entity")) heapAtWarning = ESP.getFreeHeap();
});
```

Both samples sit inside one dispatch, so the `QueuedEvent` vector is live for both and cancels out.

## Verification

**Commands:**
- `cd DomoticsCore-HomeAssistant && rm -rf .pio && pio test -e native` -- expected: every native suite green, device suite absent from the run
- `cd DomoticsCore-HomeAssistant && pio test -e esp8266dev --without-uploading --without-testing` -- expected: compiles; this does **not** upload, unlike `--without-testing` alone
- `cd DomoticsCore-HomeAssistant && pio test -e esp32dev --without-uploading --without-testing` -- expected: compiles, device suite ignored
- `cd DomoticsCore-System/examples/FullStack && pio run -e esp8266dev` -- expected: builds; record the RAM and flash figures for the commit message

**Manual checks (if no CLI):**
- The board run is deliberately not in this pass. Report that `pio test -e esp8266dev` on a `nodemcuv2` and the removal check against the reverted parse both still need hardware.

## Suggested Review Order

**The parse**

- The entry point: the subscriber now hands the event's own buffers straight through.
  [`HomeAssistant.h:157`](../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h#L157)

- The conversion itself — pointer scan, stack buffer, no allocator on the path.
  [`HomeAssistant.h:647`](../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h#L647)

- New lookup compares the full extent, not the copy already cut to 63 — a longer id must still match.
  [`HomeAssistant.h:540`](../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h#L540)

- Load-bearing text: the device suite matches this warning, and `%.*s` prints the id as delivered.
  [`HomeAssistant.h:690`](../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h#L690)

- The one call whose comment was corrected: it still re-enters `findEntity` and still builds a `String`.
  [`HomeAssistant.h:739`](../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h#L739)

**The measurement**

- Baseline sample, subscribed before the component so it runs first in the same dispatch.
  [`test_ha_heap_esp8266.cpp:161`](../../DomoticsCore-HomeAssistant/test/test_ha_heap_esp8266/test_ha_heap_esp8266.cpp#L161)

- Second sample, inside the frame; 112 B held before the fix, 0 after.
  [`test_ha_heap_esp8266.cpp:202`](../../DomoticsCore-HomeAssistant/test/test_ha_heap_esp8266/test_ha_heap_esp8266.cpp#L202)

- Probe hygiene: a live probe is removed before a new one is installed, and again in `tearDown`.
  [`test_ha_heap_esp8266.cpp:98`](../../DomoticsCore-HomeAssistant/test/test_ha_heap_esp8266/test_ha_heap_esp8266.cpp#L98)

**Behaviour the rewrite could have dropped silently**

- Made discriminating by review: an OFF after the ON, so the bug-008 overload cannot resolve wrongly.
  [`test_ha_component.cpp:508`](../../DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp#L508)

- Truncation at 63 characters, asserting both the event field and the captured warning.
  [`test_ha_component.cpp:961`](../../DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp#L961)

- The helper asserts its fixture fits both event buffers rather than truncating in silence.
  [`test_ha_component.cpp:941`](../../DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp#L941)

**Wiring and record**

- `test_filter` on the board env; `test_ignore` in native and esp32dev, or the required native job compiles it for the host.
  [`platformio.ini:72`](../../DomoticsCore-HomeAssistant/platformio.ini#L72)

- The suite joins the job that compiles what no runner can execute.
  [`ci.yml:216`](../../.github/workflows/ci.yml#L216)

- The measured figures, the SystemInfo refutation on cost, and the corrected counts.
  [`CODE-ROADMAP.md:582`](../../docs/CODE-ROADMAP.md#L582)
