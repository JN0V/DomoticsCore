- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
  summary: `EventBus::enqueue` never decrements `pendingByTopic` for the event it drops on overflow, so the counter drifts up permanently and sticky replay is inhibited for any topic that has ever overflowed.
  evidence: `EventBus.h:236-252` increments on every push including the overflow path; `poll()` decrements only for events it dispatches (`:203-207`); the `queue.pop()` in the overflow branch decrements nothing. The plateau test drives 80 undrained writes on one topic, of which at most 32 can ever be dispatched.

- source_spec: `spec-stor-esp-1-esp8266-storage-write-leak.md`
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

- source_spec: `_bmad-output/implementation-artifacts/spec-sec-9-multipart-envelope-sizing.md`
  summary: On the browser path, SEC-8's streaming refusal never reaches the operator. A failing `acceptUploadChunk()` sets `uploadState.error = "Firmware too large"` but leaves `uploadState.rejected` false, so every following chunk re-enters `acceptUploadChunk()` on a closed session and overwrites the message with `"Upload not active"` — as does the `final` call to `finalizeUpload()`. The JSON response therefore reports `"Upload not active"` for a cap violation, which says nothing about why. Same shape as the defect SEC-7 fixed for `beginUpload()`, on the sibling call.
  evidence: `OTAWebUI.h:426-450` — `if (uploadState.rejected) return;` guards the begin-time refusal only; the chunk-time failure sets no such flag. `OTA.cpp:262-264` closes the session, and `OTA.cpp:363-365`/`OTA.cpp:253-255` then answer `"Upload not active"`. Surfaced by the SEC-9 adversarial review; not caused by it, and not reachable from any suite — it is the handler TEST-8 says nothing traverses.

- source_spec: `_bmad-output/implementation-artifacts/spec-mem-2-ha-message-strings.md`
  summary: `pio test -e esp32dev` in `DomoticsCore-HomeAssistant` does not build: `test_ha_events` and `test_ha_entity` include `Platform_Stub.h`, which collides with the real Arduino core, and the run dies with 41 errors. Pre-existing — reproduced at `bea43842` with the MEM-2 changes stashed. It is CI-11's defect mirrored on the ESP32 environment, and no workflow runs that command, so nothing reports it.
  evidence: the environment declares `test_framework`, `test_build_src` and a `test/` directory, which is all `pio test` needs to try; the MEM-2 lot added a `test_ignore` for its own device suite there, which excludes one suite and leaves the two colliding ones untouched.

- source_spec: `_bmad-output/implementation-artifacts/spec-mem-2-ha-message-strings.md`
  summary: `HomeAssistantComponent::handleCommand` opens with an unconditional `DLOG_I` of the topic and the payload, before any parsing or filtering. `DLOG_I` has no level gate, so every message on the shared MQTT client — including every message that belongs to another component — costs a 128-byte stack buffer, an `snprintf` over a payload of up to 699 bytes and a walk of the `LoggerCallbacks` vector. The MEM-2 lot's own argument for removing allocations from that path applies to this line verbatim, and the lot did not touch it.
  evidence: `HomeAssistant.h:648`; `Logger.h:87` broadcasts unconditionally; `DOMOTICS_DLOG_BUF_SIZE` is 128 on ESP8266 (`Platform_ESP8266.h:49`). Surfaced by the MEM-2 review, not caused by it.

- source_spec: `_bmad-output/implementation-artifacts/spec-mem-2-ha-message-strings.md`
  summary: The switch auto-publish re-enters the entity lookup it already performed. `publishState(id, state)` calls `findEntity(id)` a second time — a second linear scan by `String` — on a path that is already holding the `HAEntity*`. Pre-existing on both sides of the MEM-2 conversion; the lot removed the parse allocations and left this one, which is the larger of the two on the accepted path.
  evidence: `HomeAssistant.h:735` calls into the `(const String&, const char*)` overload at `:359`, which delegates to `publishState(id, String(state))` at `:324` and looks the entity up again.

- source_spec: `_bmad-output/implementation-artifacts/spec-mem-2-ha-message-strings.md`
  summary: A topic with two adjacent slashes (`homeassistant/switch/node//set`) yields an empty entity id, and the lookup matches any entity whose id is empty rather than refusing the message. Identical in both versions of the parse — `strncmp` with `n == 0` returns 0, exactly as the `String` comparison against `""` did — so it is neither caused nor worsened by MEM-2, and no entity in the shipped code can have an empty id. Recorded because the lot added the malformed-topic tests and covered neither this shape nor a trailing-slash topic.
  evidence: `HomeAssistant.h:540` (`findEntity(const char*, size_t)`) and the pre-change `findEntity(const String&)`; surfaced by two independent reviewers of the MEM-2 diff.
