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
