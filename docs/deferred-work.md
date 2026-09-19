# Deferred work

Defects and observations found by a lot and deliberately not fixed in it,
each with its evidence, until a roadmap item picks it up or a lot closes it.
The `source_spec` names refer to working documents that are no longer in
the tree (`git log --all -- _bmad-output/`); the evidence lines stand on
their own.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md` — **DONE 2026-09-06 as BUG-36 (OBS Lot B)**
  summary: `EventBus::enqueue` never decrements `pendingByTopic` for the event it drops on overflow, so the counter drifts up permanently and sticky replay is inhibited for any topic that has ever overflowed.
  evidence: `EventBus.h:236-252` increments on every push including the overflow path; `poll()` decrements only for events it dispatches (`:203-207`); the `queue.pop()` in the overflow branch decrements nothing. The plateau test drives 80 undrained writes on one topic, of which at most 32 can ever be dispatched.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md` — **DONE 2026-09-06 with BUG-36 (entries erased at zero)**
  summary: `EventBus` keeps a topic's `pendingByTopic` entry forever — `poll()` decrements the counter but never erases the entry — which is one of the two sources of the 128 B one-time residue the reclaim test had to be restructured around.
  evidence: measured on the board: a first fill-and-drain cycle leaves 128 B behind, a second identical cycle leaves under 64 B.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
  summary: `HeapTracker::checkpoint()` samples the heap before inserting its own map node, so every measured window absorbs the previous checkpoint's allocation — material against the suite's 64 B thresholds.
  evidence: `HeapTracker.h:96-101`. Forced the reclaim test to read mid-window occupancy through `ESP.getFreeHeap()` rather than a third checkpoint.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
  summary: `HeapTracker::getDelta` reports a multi-kilobyte leak instead of an error when given a checkpoint name that does not exist.
  evidence: `getCheckpoint` returns a zero-initialised snapshot for an unknown name (`HeapTracker.h:108-114`) and `getDelta` subtracts it verbatim (`:131-135`), so a typo yields `start.freeHeap - 0`.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
  summary: The ESP8266 heap suite leaves its LittleFS namespaces resident between runs and never shuts down the `Core` instances it builds, so successive runs start from a progressively more fragmented heap and a different filesystem state.
  evidence: namespaces `heaptest`, `churn`, `overwrite`, `undrained`, `reclaim`, `nswarm` and `ns0..ns4` are created and never removed; `OpenStorage` has no destructor beyond the implicit one.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
  summary: `CLAUDE.md` tells the next person the Storage suite fails three tests on purpose and lists STOR-ESP-1 as an open HIGH. Both statements become false with this change.
  evidence: the suite now passes 7/7 on hardware with `Storage_ESP8266.h` unchanged. The file is currently untracked in the working tree, so it is the maintainer's to update.

- source_spec: `SEC-2 re-fix (docs/CODE-ROADMAP.md), 2026-08-26`
  summary: The OTA **upload** path commits whatever arrived with no integrity check of any kind — tracked as SEC-7. `finalizeUpload()` calls `end(true)` directly; `OTAConfig` has no field for an expected digest and the WebUI form collects none. SEC-3 authenticates the endpoint, which stops a stranger pushing firmware, but does nothing about a corrupt transfer from a legitimate one.
  evidence: `OTA.cpp:283-318` — no `HAL::SHA256` anywhere in `beginUpload`/`acceptUploadChunk`/`finalizeUpload`, unlike `installFromUrl` which hashes every chunk it writes.

- source_spec: `SEC-2 re-fix (docs/CODE-ROADMAP.md), 2026-08-26`
  summary: `OTAComponent::broadcastProgress()` is defined and never called, and `loop()` still drives `HAL::OTAUpdate::hasPendingData()`/`processBuffer()`, which are no-ops on all three platforms since the buffering strategy was removed. Both were already recorded in `docs/components/ota/project-context.md`; noting them here because reading the download path for SEC-2 confirmed they are genuinely unreachable rather than merely unused.
  evidence: `OTA.cpp:98-126` (the buffering branch), `OTA.cpp:~587` (`broadcastProgress`); `requiresBuffering()`/`hasPendingData()` return false unconditionally in `Update_ESP32.h`, `Update_ESP8266.h` and `Update_Stub.h`.

- source_spec: `SEC-2 re-fix (docs/CODE-ROADMAP.md), 2026-08-26`
  summary: The roadmap's tracking summary cannot reconcile its own item count, and could not before this change. 49 resolved + 70 remaining is 119; the stated total is 109; counting the ID ranges in the Items column gives 113. The per-severity columns do sum correctly across the rows — it is the item count alone that is adrift.
  evidence: `docs/CODE-ROADMAP.md`, Tracking Summary table. Left as found; corrections need a decision about what the column counts.

- source_spec: `spec-sec-9-multipart-envelope-sizing.md`
  summary: On the browser path, SEC-8's streaming refusal never reaches the operator. A failing `acceptUploadChunk()` sets `uploadState.error = "Firmware too large"` but leaves `uploadState.rejected` false, so every following chunk re-enters `acceptUploadChunk()` on a closed session and overwrites the message with `"Upload not active"` — as does the `final` call to `finalizeUpload()`. The JSON response therefore reports `"Upload not active"` for a cap violation, which says nothing about why. Same shape as the defect SEC-7 fixed for `beginUpload()`, on the sibling call.
  evidence: `OTAWebUI.h:426-450` — `if (uploadState.rejected) return;` guards the begin-time refusal only; the chunk-time failure sets no such flag. `OTA.cpp:262-264` closes the session, and `OTA.cpp:363-365`/`OTA.cpp:253-255` then answer `"Upload not active"`. Surfaced by the SEC-9 adversarial review; not caused by it, and not reachable from any suite — it is the handler TEST-8 says nothing traverses.

- source_spec: `spec-mem-2-ha-message-strings.md`
  summary: `pio test -e esp32dev` in `DomoticsCore-HomeAssistant` does not build: `test_ha_events` and `test_ha_entity` include `Platform_Stub.h`, which collides with the real Arduino core, and the run dies with 41 errors. Pre-existing — reproduced at `bea43842` with the MEM-2 changes stashed. It is CI-11's defect mirrored on the ESP32 environment, and no workflow runs that command, so nothing reports it.
  evidence: the environment declares `test_framework`, `test_build_src` and a `test/` directory, which is all `pio test` needs to try; the MEM-2 lot added a `test_ignore` for its own device suite there, which excludes one suite and leaves the two colliding ones untouched.

- source_spec: `spec-mem-2-ha-message-strings.md`
  summary: `HomeAssistantComponent::handleCommand` opens with an unconditional `DLOG_I` of the topic and the payload, before any parsing or filtering. `DLOG_I` has no level gate, so every message on the shared MQTT client — including every message that belongs to another component — costs a 128-byte stack buffer, an `snprintf` over a payload of up to 699 bytes and a walk of the `LoggerCallbacks` vector. The MEM-2 lot's own argument for removing allocations from that path applies to this line verbatim, and the lot did not touch it.
  evidence: `HomeAssistant.h:648`; `Logger.h:87` broadcasts unconditionally; `DOMOTICS_DLOG_BUF_SIZE` is 128 on ESP8266 (`Platform_ESP8266.h:49`). Surfaced by the MEM-2 review, not caused by it.

- source_spec: `spec-mem-2-ha-message-strings.md`
  summary: The switch auto-publish re-enters the entity lookup it already performed. `publishState(id, state)` calls `findEntity(id)` a second time — a second linear scan by `String` — on a path that is already holding the `HAEntity*`. Pre-existing on both sides of the MEM-2 conversion; the lot removed the parse allocations and left this one, which is the larger of the two on the accepted path.
  evidence: `HomeAssistant.h:735` calls into the `(const String&, const char*)` overload at `:359`, which delegates to `publishState(id, String(state))` at `:324` and looks the entity up again.

- source_spec: `spec-mem-2-ha-message-strings.md`
  summary: A topic with two adjacent slashes (`homeassistant/switch/node//set`) yields an empty entity id, and the lookup matches any entity whose id is empty rather than refusing the message. Identical in both versions of the parse — `strncmp` with `n == 0` returns 0, exactly as the `String` comparison against `""` did — so it is neither caused nor worsened by MEM-2, and no entity in the shipped code can have an empty id. Recorded because the lot added the malformed-topic tests and covered neither this shape nor a trailing-slash topic.
  evidence: `HomeAssistant.h:540` (`findEntity(const char*, size_t)`) and the pre-change `findEntity(const String&)`; surfaced by two independent reviewers of the MEM-2 diff.

- source_spec: `spec-mem-2-close-scan-and-rows.md`
  summary: The WebUI scan button cannot complete a scan in the state a user presses it. `scan_networks` is what an operator clicks during AP provisioning — when no SSID is configured — and `WifiComponent::loop()` returns at `Wifi.h:251-254` whenever `ssid.isEmpty()`, forty lines before the `if (scanInProgress)` poll that reads the results and builds the summary. In that state the scan is started and never harvested, so the UI sits at "Scanning..." indefinitely.
  evidence: `WifiWebUI.h:176` starts the scan; `Wifi.h:251-254` is the early return; `Wifi.h:296-343` is the poll it never reaches. Pre-existing on both sides of the MEM-2 rewrite — surfaced because the new device suite's fixture hit the same early return and could not reach the loop it was written to measure.

- source_spec: `spec-mem-2-close-scan-and-rows.md`
  summary: Nothing displays the async scan summary. `WifiWebUI` declares its own `String lastScanSummary` (`WifiWebUI.h:24`), sets it to "Scanning..." when the scan is requested (`:176-177`), and never reads `WifiComponent::getLastScanSummary()` back. Outside `test_wifi_component.cpp:231` and the new device suite, that getter has no caller, so the summary the component builds reaches no user.
  evidence: repository-wide search for `getLastScanSummary`. This bears on the MEM-2 lot's own justification for fixing that site first, and is recorded rather than quietly relied upon.

- source_spec: `spec-mem-2-close-scan-and-rows.md`
  summary: RemoteConsole's help text is 427 bytes of ESP8266 DRAM. The MEM-2 lot turned ten run-time appends into one stored literal, but a non-`PROGMEM` literal links into `.rodata`, which the ESP8266 places in DRAM — which is what `F()`/`FPSTR` exist to avoid. Moving it off DRAM was neither done nor argued; the portability of `FPSTR` across the ESP32 core is the question that decides it.
  evidence: `RemoteConsole.h`, the help handler's constant part; the lot reports "RAM unchanged at 50,808" without touching this.

- source_spec: `docs/CODE-ROADMAP.md` BUG-37 (no spec; fixed from the bench 2026-09-05)
  summary: The ESP32 upload link is slow and lossy on its own, independently of the idle-timeout defect BUG-37 fixed. Throughput 20–54 KB/s because the device's receive window is lwIP's default 5 760 bytes and the round trip averages 114–127 ms idle (max 307–610 ms), so window / RTT is the ceiling; dozens of retransmissions per megabyte (52 in one OTAWithWebUI upload, 148 in one FullStack session). The RTT profile is what WiFi modem sleep looks like, the Arduino core's default is `WIFI_PS_MIN_MODEM`, and nothing in `Wifi_ESP32.h` sets it either way. Hypothesis, not a finding: one build with `WiFi.setSleep(false)` would say whether that is the RTT, the loss, both, or neither. Same shape on FullStack and on the minimal OTAWithWebUI, so not a component's doing.
  evidence: `ss -tin` samples in the BUG-37 probes (`rwnd_limited` 58 % of the time, `snd_wnd:5760`, `rto` backoff to 5 888 ms); `ping -c 30` idle 8.0/114.2/307.4 ms on FullStack, 8.8/126.6/609.8 on OTAWithWebUI, both on the WROOM-32D, 2026-09-05.

## Deferred from: code review (2026-09-05, BUG-37 diff)

- TEST-8's Problem paragraph still says "52 native, 8 per board"; the OTA native suite has 54 cases (BUG-37 added assertions to two of them, not cases) and its `Files` line points at `OTAWebUI.h:353-443`, a handler that now runs past line 500. Pre-existing staleness in an entry BUG-37 cites; not re-derived in that lot.
- `OTAComponent::setConfig()` logs three of the nine `OTAConfig` fields (`updateUrl`, `autoReboot`, `enableWebUIUpload`) and omits `maxDownloadSize`, `requireUploadHash` and now `uploadIdleTimeoutSec`; a runtime change to a security-relevant field leaves no trace. Pre-existing; the ESP8266 log buffer (128 bytes) is the constraint any fuller line has to fit.

## Deferred from: code review of spec-obs-lot-c-oom-moment (2026-09-06)

- Survived allocation failures during a bring-up that dies before `acknowledge()` stay in RAM and are lost with the next reset: while a promoted death is held in RTC, the new run's group is not written over `w56-60`, or the next boot would attribute this run's failures to the old death. By design (D2 of the plan); sits beside Lot B's residual on uncounted deaths in a crash loop. Evidence: `FlightRecorder.cpp` `noteFailedAllocImpl`, the `rtcHeld()` branch; `test_group_stays_in_ram_while_a_death_is_held_and_lands_at_acknowledge`.

## Deferred from: code review of OBS Lot D (2026-09-14)

- A System native environment with MQTT and HomeAssistant: the telemetry sink, the connect subscription and the entity registration are proven on a board only; the native System suite compiles without both by design.
  source_spec: `spec-obs-lot-d-off-the-device.md`
- `PROGMEM` for the ESP8266 literals Lot D added: its RAM cost is `.rodata` in DRAM (+1 216 B measured against `main`).
  source_spec: `spec-obs-lot-d-off-the-device.md`
- `POST /api/system/coredump/erase` answering `409` while a download is in flight is never exercised; the race needs two clients on a board.
  source_spec: `spec-obs-lot-d-off-the-device.md`
- Discovery document sizes with a `configuration_url` on ESP8266 were reconstructed (974 characters for a maximal alarm panel, BUG-38), not measured on that board.
  source_spec: `spec-obs-lot-d-off-the-device.md`

## Deferred from: the SEC-4 / SEC-6 / SEC-14 lot (2026-09-14)

- A constant-time password compare in the console (`String ==`): a timing oracle over Telnet on Wi-Fi behind the auth wait is not the threat the lot closed.
  source_spec: `spec-sec-webui-console-lot.md`
- Hashing the stored WebUI and console passwords (Storage keys, a migration): a design question, not a patch.
  source_spec: `spec-sec-webui-console-lot.md`
- Rate-limiting the WebUI's HTTP authentication: ESPAsyncWebServer answers the challenge; a limiter would sit in `authorize()`.
  source_spec: `spec-sec-webui-console-lot.md`
- `allowedOrigin` with `Access-Control-Allow-Credentials: true`, for a cross-origin dashboard against an authenticated device; and `enableCORS` applied to every response through `DefaultHeaders` — both behaviour changes nobody has asked for.
  source_spec: `spec-sec-webui-console-lot.md`
- OTAWebUI's three inlined `authenticate()` copies should call `WebUIComponent::authorize()` once OTA can require the WebUI version that has it.
  source_spec: `spec-sec-webui-console-lot.md`

## Deferred from: the TEST-7 lot (2026-09-15)

- Twelve component call sites parse user input with `toInt()`/`toFloat()` (`OTA.cpp` ×4, `RemoteConsole.h`, `RemoteConsoleWebUI.h` ×2, `LEDWebUI.h`, `MQTTWebUI.h`, `NTPWebUI.h`, `Storage_Stub.h` ×2) and no native suite feeds any of them a non-numeric value; until this lot the stub threw on one, so none could. What each handler does with `"abc"` in a numeric field is unmeasured.
  source_spec: `spec-test-7-core-memory-config.md`
- `validateInteger` used to refuse `"+5"` and `"05"` (a canonical-form rule, stated nowhere); the digits-only rule accepts them. If a sketch relied on the refusal, it is one extra check in `parsesAsInt32`.
  source_spec: `spec-test-7-core-memory-config.md`
- TEST-9's three remaining providers (NTP, OTA, RemoteConsole) need a host double of `WebUIComponent::registerApiRoute` and `AsyncWebServerRequest`; MQTTWebUI needs only its `WebUI.h` line removed, with the MQTT suite that would then compile it.
  source_spec: `spec-test-7-core-memory-config.md`
- `MemoryManager`'s `maxProviders` and `heapCheckInterval` are tabulated in the Core reference and reachable through no public method (DC-17).
  source_spec: `spec-test-7-core-memory-config.md`


## Deferred from: the CI-15 lot (2026-09-15)

- `tests/unit/{01,02,05,06}` are `esp32dev` projects no workflow builds, and `tools/local_ci.sh:28` globs `tests/unit/test_*_isolated`, which matches nothing. `01`, `02` and `05` moved to `symlink://` with the rest; `06-webui-refactor` reaches the components through `lib_extra_dirs = ../../../../` with `deep+` and two `me-no-dev` git dependencies — a third in-place mechanism the CI guards do not see, and one that now meets the repository root's `.pio/` (the relocated libdeps) as a manifest-less directory. Whether the four should exist is a decision, not a fix.
  source_spec: `spec-ci-15-symlink-deps.md`
- The `-I../DomoticsCore-X/include` build flags in every native manifest are a second route to every header now that the LDF reads the components in place; kept because removing them is its own change with its own check (every include resolving through the LDF alone).
  source_spec: `spec-ci-15-symlink-deps.md`
- `embed_webui.py` writes the generated header into `DomoticsCore-WebUI/include/DomoticsCore/Generated/` (ignored by that package's `.gitignore`) on every example build, as before the lot; under `symlink://` it is the only copy the compiler reads, and a stale one from a previous example build is what the next one starts from until the script rewrites it.
  source_spec: `spec-ci-15-symlink-deps.md`
- An ESP32 image embeds the source paths the core's log macros expand from `__FILE__` (30 strings in FullStack esp32dev); with `symlink://` they are the components' real, absolute paths, so the image and its flash figure depend on where the checkout lives (+864 bytes locally against `main`'s `.pio/libdeps/...` paths). `-ffile-prefix-map=<repo root>=` in the build flags would make the image path-independent; not done here because it changes what a crash log names.
  source_spec: `spec-ci-15-symlink-deps.md`

## Deferred from: the BUG-41 / BUG-42 lot (2026-09-18)

- The byte budget bounds the **queue** and not the sticky store. `publishSticky`
  writes the topic's last payload into `lastByTopic`, its payload-less overload
  clears the vector and keeps the key, and only `reset()` erases the map — so the
  store grows with the number of distinct sticky topics ever published and
  nothing counts it against `QueueCost::kBudgetBytes`. Bounded in practice by a
  firmware's fixed topic set rather than by the code: a caller that composes topic
  names at run time (per entity, per session) grows it without limit. Not fixed
  here because the bound the lot argued and measured is the queue's, and evicting
  a sticky payload changes what a late subscriber replays — a behaviour decision,
  not a cap.
  evidence: `EventBus.h:236`, `:242` (the two stores), `:247-248` (the clear that
  keeps the key), `:316` (`reset()`, the only erase), `:408` (the map);
  `docs/decisions/0003-the-eventbus-queue-is-bounded-by-bytes-not-by-count.md`.

- `Core::begin()` is one deep frame among several on the ESP8266's 4 KB cont
  stack, and BUG-42 measured only the one that was overflowing it. With that
  kilobyte out of the way the Storage suite's deepest point still leaves 896 B,
  which is margin, not comfort: `-fstack-usage` puts `logPromotedRecord` at
  1 168 B and `app_entry_redefinable` at 4 160, and nothing in CI reads either
  number. A build-time check on the frames of the paths an application runs at
  boot would have caught this before a board did.
  evidence: `.su` files from `PLATFORMIO_BUILD_FLAGS="-fstack-usage"`, 2026-09-18;
  `ESP.getFreeContStack()` 32 B before the fix, 896 B after, same suite.

## Deferred from: code review of the BUG-41 / BUG-42 lot (2026-09-18)

- `static_assert(sizeof(EventBus) == 172)` no longer ships in a public header —
  **DONE the same day**: it broke the build of any consumer whose toolchain lays
  the object out differently, and the fork builds Arduino core 3.x. It moved to
  `DomoticsCore-Storage/test/test_heap_esp8266`, which CI compiles for the board.
  That pins the ESP8266 layout only; the ESP32 side is now unpinned, and pinning
  it needs a suite that compiles for that target.

- **The byte model runs 2 to 3 % under a full queue** — measured 2026-09-19,
  after this entry first said the confrontation had never happened. 32 reference
  events cost 29 096 B on an ESP32-C3 against 28 192 modelled and 29 064 B on a
  nodemcuv2 against 28 576. The per-event constants are exact on both boards, so
  the residue is something `QueueCost::of()` has no term for — plausibly the
  deque's map array, or an allocation per queue rather than per event, which one
  point per board cannot separate. Two points per board (a half-full queue and a
  full one) would. Until then the budget is a close estimate and not a ceiling,
  which the header and ADR 0003 now say. The rest of this entry stands: no
  automated check compares the two, and the probe still asserts nothing.

- **The byte model is never confronted with real heap by any test.** Every expected value in
  every suite is computed by calling `QueueCost::of()` or dividing
  `kBudgetBytes` — the suites say so themselves ("these test the model, not the
  silicon"). Set `kNode` to 12 and `kOverhead` to 0 and nothing turns red: the
  native suites recompute their capacities from the new constants and still agree
  with the bus, and the Storage board suite derives `HALF` the same way. The only
  thing that ever compared the model to the heap is
  `tools/on-device/probes/allocator-shape`, which prints a staircase for a human,
  asserts nothing, and is not in CI's device-build list. The shape that would
  close it: promote the probe's fill block into the board suites as a Unity test —
  `getQueuedBytes()` against a measured free-heap delta, asserting the model is an
  upper bound and not a loose one. The same gap explains why the two literals this
  lot removed from the HomeAssistant connect-cliff test (29 and 30) left nothing
  independent behind: expectation and code now share one model.
  evidence: `git grep QueueCost`; `test_eventbus.cpp` header comment.

- `-DDOMOTICS_EVENTBUS_QUEUE_BYTES` is a compile-time constant in a header whose
  `enqueue()` is inline. Passed through `build_src_flags` rather than
  `build_flags`, an application's translation units would compile against one
  budget and the library's against another, and the linker would pick one
  silently. Nothing in the header, the reference or CI refuses that spelling, and
  the documentation names the flag without saying where it has to be set.

- The entry guard rail went from 32 to 256 while `pendingByTopic`'s map nodes and
  their `String` keys are counted by nothing. A burst of small events on distinct
  topics can now hold eight times as many of them as before, outside the budget
  that exists to bound exactly that.
  evidence: `EventBus.h`, `kMaxEntries` and the `pendingByTopic` insert.

- `publishSticky` stores a payload the queue itself would refuse — **DONE the
  same day**, with the oversized guard moved from `enqueue()` to the publish
  entry points: an event over the whole budget is now refused before anything is
  copied, `publishSticky` included, so it is neither stored nor replayed. The
  unbounded sticky store recorded above is a different item and stays open.

- The high-water mark is logged only when the drop counter changes, so a queue
  that peaks at 95 % and never drops is invisible. `FlightRecord` carries
  `eventDrops()` and no occupancy, and OBS-5's telemetry carries neither — the one
  number that would let an application see the cliff coming is surfaced nowhere.
  evidence: `Core.cpp`, the drop line's `drops != lastDropsLogged_` guard.

- The shipped 2.5.0 release note says "The EventBus's 32-entry cap leaves **13
  entities** to an application at connect". The sentence describes a cap this lot
  removes, and the figure is derived nowhere in the repository — it replaced 21 in
  uncommitted work that predates the review. Settling it needs whoever produced
  21 and 13; the suites now derive the connect-burst capacity from `QueueCost`
  instead, so nothing pins either number.
  evidence: `CHANGELOG.md`, the 2.5.0 block.

- `DomoticsCore-Storage`'s `esp8266dev` environment sets
  `-DDOMOTICS_EVENTBUS_QUEUE_BYTES=4096` for the whole environment, so after this
  lot no board anywhere exercises the shipped 32-reference-event budget end to
  end. The plateau test now derives its own numbers from `QueueCost`, which
  arguably removes the need for the flag there at all — but dropping it lengthens
  the run and that is a judgement about board time.

- `sizeof(MQTTPublishEvent)` is written 830 in `QueueCost`'s comment, ADR 0003 and
  the CHANGELOG, and 832 in LO-2. `block()` rounds both to 848, so nothing
  computes differently and the disagreement will survive until someone states it
  as `sizeof(MQTTPublishEvent)` in prose the way the tests already do in code.

- `test_ha_component`'s connect-cliff helper computes `refEvents - 2` on a
  `size_t`. `kBudgetBytes >= kReference` is asserted, so `refEvents >= 1`, but a
  tight override makes the subtraction wrap and the test then asks for a negative
  number of sensors. One clamp, whenever that helper is next touched.


## Deferred from: the BUG-43 lot (2026-09-18)

- The MQTT WebUI exposes `lwt_topic`, `lwt_enabled` and `lwt_message` as
  first-class settings and writes them straight into the component. BUG-43 ties
  the availability topic to the will from HomeAssistant's side; nothing tells
  HomeAssistant when the will moves from MQTT's side, so a user editing that field
  leaves every retained discovery document pointing at the previous topic. The
  mechanism is a decision — an event, a direct call, or refusing the edit while a
  HomeAssistant component is present — not a patch.
  evidence: `MQTTWebUI.h`, the settings context and its save handler.
