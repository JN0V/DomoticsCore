---
title: 'SEC-9 — the upload path sizes itself from the multipart envelope'
type: 'bugfix'
created: '2026-08-27'
status: 'in-review'
review_loop_iteration: 0
baseline_commit: '2076270e3101b900f50d955e9ddf3e30e9798fcd'
context:
  - '{project-root}/_bmad-output/implementation-artifacts/brief-sec-9-multipart-envelope-sizing.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `OTAWebUI.h:399` passes `request->contentLength()` to `beginUpload()`. On a
`multipart/form-data` POST that measures the encoded body, not the firmware — 220 bytes more, on
both boards. SEC-9 records three consequences and a fix; the brief refutes the fix and two of the
three. What is genuinely wrong is narrow: the SEC-8 ceiling is compared against the envelope, and
the completion event reports the envelope as the bytes received.

**Approach:** Keep the envelope where it is safe — it is an upper bound, so `Update.begin()` never
truncates and the pre-write refusal can only over-refuse. Make every operator-visible number say
what it measured. Then close the two things that let this hide: pin `end(true)` with a test, and
file the missing multipart coverage.

## Boundaries & Constraints

**Always:** `acceptUploadChunk()` stays authoritative on the ceiling. The pre-write refusal stays,
and stays before `HAL::OTAUpdate::begin()`. Roadmap rows keep summing to the total.

**Ask First:** any signature change to `beginUpload()` / `finalizeUpload()`; any change to the
`end(true)` argument itself; any change to `installFromUrl()`.

**Never:** rejected option A (pass `0`) — it removes the pre-write refusal from the browser path.
Rejected option B (a size-source / upper-bound parameter) — permanent public API for a ~220-byte
window. No version bump, no new config field.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Behavior |
|---|---|---|
| Over ceiling, announced | `maxDownloadSize=100000`, `beginUpload(475452)` | refused before `HAL::OTAUpdate::begin()`; log names the announced figure and that a multipart announcement includes framing |
| Over ceiling, unannounced | `maxDownloadSize=2048`, `beginUpload(0)`, 4 KB streamed | refused by the running total; update released, nothing staged (unchanged) |
| Within ceiling | 16 B firmware announced as 236 B | accepted; completion reports `bytes=16`, `total=16` |
| Successful finalize | any accepted upload | `progress == 100.0f`; `EVENT_COMPLETED` reports bytes received, not announced |
| `end()` without `evenIfRemaining` | hypothetical regression | native suite fails |

</frozen-after-approval>

## Code Map

- `OTA.cpp:187-193` — SEC-8 pre-write refusal. Message and comment only; the comparison is
  deliberately unchanged.
- `OTA.cpp:208,214` — `uploadSession.expected` and `totalBytes` take the announced figure.
  `expected` stays: it is the progress denominator and `EVENT_PROGRESS.total`.
- `OTA.cpp:388` — `HAL::OTAUpdate::end(true)`. Load-bearing: `isFinished()` is `_progress == _size`
  (ESP32 `Update.h:116`, ESP8266 `Updater.h:165`) and `end()` refuses an unfinished image without
  `evenIfRemaining` (ESP32 `Updater.cpp:289`, ESP8266 `Updater.cpp:226`). **Read-only — pin it.**
- `OTA.cpp:402-408` — upload finalize, calls `finalizeUpdateOperation()`.
- `OTA.cpp:685-687` — already sets `progress = 100.0f`, then `downloadedBytes = totalBytes`. This
  is where the envelope leaks into `EVENT_COMPLETED` (`doc["bytes"]`/`doc["total"]`, 704-705).
  Shared with the download path — **do not change it there.**
- `OTAWebUI.h:399` — the call site. **Unchanged by design**; both rejected options started here.
- `Update_Stub.h:27-42` — existing `s_stubEndCalls`/`s_stubAbortCalls`; the pattern to extend.
- `test_ota_component.cpp:610-660` — SEC-8's ceiling tests; the neighbourhood for the new ones.
- `docs/CODE-ROADMAP.md:248-280` (SEC-9), `:1462-1478` (Tracking Summary).

## Tasks & Acceptance

**Execution:**
- [x] `DomoticsCore-OTA/src/OTA.cpp` — `beginUpload()`: the refusal log says what it compared, with
  the envelope contract in a comment above the check. `finalizeUpload()`: set
  `totalBytes = uploadSession.received` before `finalizeUpdateOperation()`.
- [x] `DomoticsCore-OTA/include/DomoticsCore/Update_Stub.h` — record the `evenIfRemaining`
  argument; the native suite cannot otherwise observe the one argument every upload depends on.
- [x] `DomoticsCore-OTA/test/test_ota_component/test_ota_component.cpp` — assert `end()` took
  `evenIfRemaining == true`; assert completion reports received bytes for an over-announced upload.
- [x] `docs/CODE-ROADMAP.md` — rewrite SEC-9 with the three refutations and their core line
  numbers, MEDIUM → LOW; file TEST-8 (nothing traverses `POST /api/ota/upload`); update the
  Tracking Summary.

**Acceptance Criteria:**
- Given a ceiling and an announced size above it, when `beginUpload()` refuses, then no
  `HAL::OTAUpdate::begin()` occurred and the log names the announced figure.
- Given an upload announced larger than what arrives, when it finalizes, then `EVENT_COMPLETED`
  reports `bytes` and `total` equal to the bytes received.
- Given the suite runs, when `end()` is reached on a successful finalize, then it took
  `evenIfRemaining == true` — and flipping that to `false` makes the suite fail on that assertion.
- Given the roadmap after this lot, when the Tracking Summary rows are summed, then they equal the
  stated total, and each of the three item figures moves by exactly one.

## Spec Change Log

## Design Notes

The envelope is an upper bound, which is why it stays. `Update.begin(size)` over-estimated never
truncates and keeps ESP32's `size > _partition->size` rejection (`Updater.cpp:161-164`) that
`UPDATE_SIZE_UNKNOWN` loses. The pre-write ceiling over-estimated can only over-refuse, and
dropping it costs what `DomoticsCore-OTA/platformio.ini:32-37` records: on a single-slot ESP32,
`Update` erases the code it is executing from.

The residue is `downloadedBytes = totalBytes` at `OTA.cpp:687` — correct for a download, where
`totalBytes` is the server's announced size; wrong for an upload, where it is the envelope. The fix
belongs in `finalizeUpload()`, narrowing `totalBytes` to what was counted, not in the shared helper.

## Verification

**Commands:**
- `rm -rf DomoticsCore-OTA/.pio && cd DomoticsCore-OTA && pio test -e native` — all pass. The
  `.pio` removal is mandatory: `file://` libdeps are copied once and never refreshed.
- `cd DomoticsCore-OTA && pio test -e esp32cam --without-uploading --without-testing` (and
  `-e esp8266dev`) — both compile. **Not `pio run`** — these are test environments with no
  `src/main.cpp`, so `pio run` cannot link. **Not `--without-testing` alone** either: that builds
  *and uploads*, to whatever board `upload_port` names.
- Removal check: flip `end(true)` to `end(false)`, re-run native — expected **red on the new
  assertion**, not by cascade. Revert.

**Manual checks:**
- `git diff -- DomoticsCore-OTA/include/DomoticsCore/OTAWebUI.h` is empty. A diff there means a
  rejected option crept back in.
