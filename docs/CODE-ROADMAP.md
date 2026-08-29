# DomoticsCore — Code Remediation Roadmap v2

> Generated from adversarial code review of all 12 components + root (2026-03-10).
> 13 parallel review agents — 196 total findings: 6 CRITICAL, 54 HIGH, 79 MEDIUM, 57 LOW.
> Priority order follows the constitution. Each item is a separate commit/PR.
>
> **Previous roadmap (v1, 2026-03-04 → 2026-03-09): ALL 10 PRIORITIES COMPLETE.** Archived at bottom.

---

## Delivery — how these items land

The remaining items are grouped into lots, one pull request per lot, rather than
landed one at a time. They come from a single oversized commit that mixed
unrelated concerns; splitting it by component is the point of the exercise.

No version bumps inside the lots — `library.json` versions and the CHANGELOG
move once, at the end, when the series is complete. That is why component
versions may sit still while fixes land.

| Lot | Items | State |
|---|---|---|
| **CI** | CI-1, CI-2 | **Merged** — PR #2, 2026-08-22 |
| **CI (deps + actions)** | CI-3, CI-5 | **Merged** — PR #6, 2026-08-22 |
| **NTP** | BUG-4, BUG-5, BUG-6 | **Merged** — PR #1 |
| OTA | SEC-3, DC-7, DC-6 | **Merged** — PR #4 |
| Storage | BUG-15, BUG-16, BUG-17, F1, F10 | **Merged** — PR #5 |
| Isolated — System | BUG-23 | **Merged** — PR #7, 2026-08-22 |
| Isolated — MQTT, RemoteConsole | BUG-8, BUG-22 | **Merged** — PR #10, 2026-08-23, landed before the `esp32-ethernet` sync so it merges once |
| ESP8266 on-device suites | CI-10 | **Merged** — PR #19, 2026-08-23, first run on real hardware; raised STOR-ESP-1, later withdrawn |
| System | TEST-1, ARCH-3 | **Merged** — PR #18, 2026-08-23 |
| LED | BUG-19, DC-5, TEST-2, LO-11 | PR #17 — chosen for having no file in common with `esp32-ethernet` |
| OTA | SEC-2 | 2026-08-26 — reopened: the v2.0.1 fix was inert on both cores. Raised SEC-7 |
| OTA | SEC-7 | **Merged** — PR #28, 2026-08-26, with the ESP32 suite in PR #29 |
| MQTT | BUG-29 | **Merged** — PR #30, 2026-08-26, filed and fixed the same day |
| OTA | BUG-21, SEC-8, TEST-3 | 2026-08-27 — BUG-21 was open all along while the tracking row said `0H`; SEC-8 filed from an observation SEC-7 left loose |
| OTA | SEC-9 | PR #36 — 2026-08-27, filed by the real-conditions campaign and closed the same day, minus all three of its recorded consequences; raised TEST-8. Stacked on PR #35, where SEC-9 was filed |
| OTA | TEST-8 (part) | **Merged** — PR #37, 2026-08-27, the two holes reachable without HTTP, closed on both boards. Found that the ESP8266 suite's one-sector payload had been hiding the Updater's flush |
| OTA | TEST-8 hole 3 | **Merged** — PR #39, 2026-08-27, the download path reported the size the server announced, in both directions |
| Core, Wifi, Storage, OTA | BUG-30 | 2026-08-28 — the guard, and the ten sites the assert enumerated |
| Core | BUG-2 | 2026-08-28 — both recorded fixes measured impossible; contract narrowed, severity re-argued HIGH → MEDIUM, no code change |
| OTA | TEST-8 hole 4 (part) | PR #40 — 2026-08-27, a real multipart POST against a board, refused and accepted paths both run, each with a removal check that discriminates. What remains of TEST-8 is the browser's own view |

The first series closed with v2.1.0 and v2.1.1. **The second ships as v2.2.0**
(2026-08-26): SEC-2, SEC-7 and BUG-29, plus the on-device suites that now run on
both an ESP8266 and an ESP32. Six components move; `System` and `Storage` do not,
having gained only a comment and a test respectively.

The no-version-bump rule held throughout — component versions and the CHANGELOG
moved once, here, at the end. It is why OTA sat at 1.5.0 while SEC-2 and SEC-7
landed.

`main` requires seven checks: `test-install`, `check-versions`,
`Unit tests (native)`, `Build esp32dev`, `Build esp8266dev`, `Build esp32c3`,
`Build on-device suites`, plus an up-to-date branch and resolved conversations.
The seventh became required on 2026-08-23 with CI-10; the count here still said
six.

### What CI proves, and what it does not

Worth knowing before a green tick is read for more than it is worth.

| | |
|---|---|
| ✅ | The 13 native projects run — 747 test cases, discovered from the tracked `platformio.ini` files rather than a hard-coded list. **Re-derive this figure, do not trust it**: it read 729 on 2026-08-28 and 739 after MEM-2's hot half; 747 is the sum of the thirteen runs on 2026-08-29, the eight new ones being the scan-format net |
| ✅ | The three declared targets compile: `esp32dev`, `esp8266dev`, `esp32c3`, via the FullStack example, the only one pulling all twelve components |
| ✅ | `library.json` versions agree with `metadata.version` |
| ✅ | The install-from-GitHub path builds **both** declared platforms — the only thing in CI that resolves through the root `library.json` rather than `file://` paths (CI-8) |
| ⚠️ | **The eight on-device suites compile in CI, and nothing runs them.** No runner has a board (CI-10). Run them by hand: `cd DomoticsCore-Storage && pio test -e esp8266dev`. Seven are ESP8266 — `DomoticsCore-Wifi` is the newest, added with MEM-2's closing lot, and it is also the only one whose result depends on the runner being in radio range of something — and `DomoticsCore-OTA` also carries an `esp32cam` one |
| ❌ | **No test runs on hardware in CI.** A host build proves compilation, not behaviour on a board — BUG-4 is what that costs, and STOR-ESP-1 is what a board proves when nobody checks what the suite is actually measuring |

That last line is not a formality. BUG-4, the SNTP server-name use-after-free,
**compiles cleanly and passes the native suites**; it only shows itself on a
board, after a configuration change. It was found by reading the code. Native
tests and cross-compilation cover what breaks most often, not what costs most.

---

## Priority 1: Security (CRITICAL — OTA & WebUI)

Unenforced security configurations are the most dangerous class of defect — users believe they are protected when they are not.

### SEC-1 — OTA: security config fields never enforced [CRITICAL] — **DONE (2026-08-22, PR #4 and #9)**

- **Ref**: OTA-F1
- **File**: `DomoticsCore-OTA/include/DomoticsCore/OTA.h`
- **Problem**: `requireTLS`, `bearerToken`, `basicAuthUser`, `basicAuthPassword`, `rootCA`, and `signaturePublicKey` are declared in `OTAConfig` but **never checked** by any code path. A user setting `requireTLS = true` believes HTTPS is enforced — it isn't. Firmware can be replaced by any network client via plain HTTP.
- **Fixed** by the third of the options above: the fields are removed (PR #4,
  with DC-7) and the documentation now states plainly where transport security
  actually lives — in the fetcher and downloader callbacks the application
  installs (PR #9).
- **Both halves were needed.** Removing the fields without correcting the
  documentation left four documents still describing `requireTLS` as rejecting
  non-HTTPS URLs and `bearerToken` as authenticating uploads. A reader following
  them would have believed in a protection twice over: once from a field that did
  nothing, then from a field that no longer existed.
- **What replaces it**: the upload endpoints are genuinely authenticated (SEC-3),
  and SHA-256 integrity is verified before the image is ever committed to flash
  (SEC-2 — the mismatch path used to run *after* the commit, and could not undo
  it). Firmware
  signature verification remains unimplemented, and is now documented as such
  rather than advertised by a config field.

### SEC-2 — OTA: SHA256 failure doesn't rollback firmware [CRITICAL] — **DONE (2026-08-26)**, after a first fix that did nothing

- **Ref**: OTA-F2
- **File**: `DomoticsCore-OTA/src/OTA.cpp`, `Update_ESP8266.h`
- **Problem**: After `HAL::OTAUpdate::end(true)` succeeds and SHA256 verification fails, the code transitions to `State::Error` but does NOT call `HAL::OTAUpdate::abort()`. Corrupted firmware may persist in the OTA partition.
- **What v2.0.1 shipped** (commit `e081940`): the missing `abort()`, added after
  the failed check, with the comment *"Rollback corrupted firmware from OTA
  partition"*. It was inert on both platforms, and stayed inert for two releases.
  `end(true)` **is** the commit, and neither Arduino core lets an application
  undo it:
  - **ESP32** — `end(true)` → `_verifyEnd()` → `esp_ota_set_boot_partition()`,
    then `_reset()`. `Update.abort()` is `_reset()` plus an error code: no flash
    write, boot partition still pointing at the rejected image.
  - **ESP8266** — `end()` writes an eboot `ACTION_COPY_RAW` staging a copy over
    the running sketch. The HAL's `abort()` called `Update.end(false)`, which
    returns early once `_size == 0`. The staged copy survived, and the next
    reboot flashed the rejected image over the good one.

  The component reported `State::Error` and skipped its own reboot, so nothing
  looked wrong — until any watchdog reset or power cycle.
- **Fix**: verify before committing, rather than trying to undo a commit. The
  digest is complete the moment the download loop returns; nothing required
  `end(true)` to run first. Aborting *before* `end()` genuinely works — ESP32
  withholds the image's first 16 bytes until `_verifyEnd()` precisely so a
  half-written partition is unbootable, and ESP8266 stages nothing until `end()`
  succeeds. There is now no ordering in which a rejected image is briefly
  bootable.
- **The HAL had to move too.** Reordering alone would have made ESP8266 *worse*:
  with every announced byte written, `Update.end(false)` inside `abort()` clears
  the `!isFinished()` guard and reaches `eboot_command_write()` — `abort()` would
  have committed the image it was asked to discard, which is exactly the state a
  failed hash leaves behind. `Update_ESP8266.h::abort()` now calls
  `eboot_command_clear()` after `end(false)`.
- **The contract is written down** in `Update_HAL.h`: `abort()` is only
  meaningful before `end()`. That is the sentence whose absence cost two
  releases.
- **Verified on hardware** (2026-08-26, `nodemcuv2` on `/dev/ttyUSB0`).
  `DomoticsCore-OTA/test/test_ota_esp8266/` drives a download through the public
  path with a synthetic downloader — no network — and reads back the eboot
  command, which is the only thing that decides what boots next. 3/3 pass. Run
  against the pre-fix code, **2 of the 3 fail**: after a SHA-256 mismatch a copy
  command *was* armed (`Expected FALSE Was TRUE`), and so was it after the old
  `abort()` on a fully-written image. That is SEC-2 reproduced on silicon rather
  than argued from the core sources, and the reason this suite exists at all —
  a host build has no bootloader to arm.

  The suite deliberately commits one good image and reads the staged command
  back, because three tests all asserting "nothing is staged" would pass just as
  well if staging were impossible. It disarms before asserting, and again in
  `tearDown()`.
- **The ESP32 half too** (2026-08-26, ESP32-CAM). `test_ota_esp32/` asserts on
  `esp_ota_get_boot_partition()`, since on ESP32 the commit *is* the boot
  partition switch. 6/6 pass; with the fix removed, **3 of the 6 fail** and two
  of those fail on the partition having moved — a mismatched image really did
  become what the bootloader would start next.

  **That test only became meaningful after it was rewritten.** It first fed a
  synthetic payload, and both mismatch tests passed without the fix: the failure
  was `Could Not Activate The Firmware`, because `esp_ota_set_boot_partition()`
  verifies the image and ESP-IDF had rejected it on its own. The test was
  measuring the platform's protection, not ours. Feeding a *valid* image with the
  wrong hash separates the two — and that is also the real threat model, since
  ESP-IDF stops a corrupt download but cannot stop a well-formed firmware that is
  not the one requested.
- **Tests**: on the host, `test_ota_sha_mismatch_never_commits` asserts `end()`
  is never reached, via call counters added to `Update_Stub.h` (commit and
  discard are otherwise indistinguishable on a host). It too fails on the pre-fix
  ordering.

### SEC-3 — OTA: upload endpoint has no authentication [HIGH]

- **Ref**: OTA-F5
- **File**: `DomoticsCore-OTA` — `/api/ota/upload` route handler
- **Problem**: Any network client can POST firmware to the upload endpoint with zero authentication.
- **Fix**: Gate the upload handler behind the WebUI authentication system, or add a dedicated OTA token check.

### SEC-4 — RemoteConsole: no brute-force protection on auth [MEDIUM]

- **Ref**: RC-F5
- **Problem**: Plain-text password comparison with no rate-limiting. Attackable via rapid telnet reconnections.
- **Fix**: Add exponential backoff after N failed attempts, or IP-based lockout.

### SEC-5 — WebUI: GET for state-changing operations [MEDIUM]

- **Ref**: WEB-F9
- **File**: `WebUI.h` — `/api/ui/action` endpoint
- **Problem** (original framing, preserved): HTTP GET used for mutations (enable/disable, password changes). Passwords appear in query strings, browser history, and server logs.
- **Fix**: At minimum add `Cache-Control: no-store` and document the security trade-off. Ideally use POST for mutations.
- **Re-pointed by SEC-10 (2026-08-29), not closed.** This filed the right route on
  the wrong axis: the GET is dangerous less because passwords land in history than
  because it makes `/api/ui/action` reachable as a cross-origin `<img>` with no
  CSRF defence — the CRITICAL that SEC-10 measured and closed. SEC-10 moved the
  route to `POST` and added a per-boot token, which resolves this item's "ideally
  use POST" as a side effect. What remains under SEC-5's own heading is the narrow
  original point — `Cache-Control` on responses that echo secrets — kept MEDIUM
  and open, so the history-leak observation is not lost inside the CSRF fix.

### SEC-6 — WebUI: CORS `Access-Control-Allow-Origin: *` with auth enabled [MEDIUM]

- **Ref**: WEB-F10
- **File**: `WebUI.h:429`
- **Problem**: Any website can make authenticated API requests to the device (CSRF/data exfiltration). Especially dangerous when `enableAuth` is true.
- **Fix**: Restrict CORS origin when auth is enabled, or disable wildcard CORS entirely.

### SEC-7 — OTA: the upload path has no integrity check at all [MEDIUM] — **DONE (2026-08-26)**

- **File**: `DomoticsCore-OTA/src/OTA.cpp` — `finalizeUpload()`
- **Found by**: the SEC-2 re-fix, 2026-08-26.
- **Problem**: SEC-2 concerns the *download* path, which hashes what it writes.
  The **upload** path does not hash anything: `finalizeUpload()` calls
  `end(true)` on whatever arrived. `OTAConfig` has no field for an expected
  digest and the WebUI form collects none, so a truncated or mangled upload is
  committed and booted. SEC-3 authenticates the endpoint, which stops a stranger
  pushing firmware; it does nothing about a corrupt transfer from a legitimate
  one.
- **Fix**: accept an optional expected SHA-256 alongside the upload (form field
  or header), hash the chunks in `acceptUploadChunk()` as the downloader does,
  and verify **before** `finalizeUpload()` calls `end(true)` — the same ordering
  SEC-2 now depends on.
- **Not scope creep into SEC-2**: this needs a new input from the caller, which
  is a feature, not a correction.
- **Fixed**: `beginUpload(size_t, const String& expectedSha256 = "")` — a
  defaulted parameter, so every existing caller still compiles.
  `acceptUploadChunk()` hashes what it writes, and `finalizeUpload()` verifies
  **before** `end(true)`, on the ordering SEC-2 established.
- **`OTAConfig::requireUploadHash`** (default `false`) refuses an upload that
  carries no digest — **before** `HAL::OTAUpdate::begin()`, which erases flash.
  Refusing afterwards would have destroyed the running firmware to reject an
  upload that was never going to be installed. Default `false` because mandatory
  would break every existing uploader, and this library is installed by version
  from the registry.

  The field is read on the only path that can start an upload. SEC-1 was six
  `OTAConfig` fields that were never checked while four documents said they were;
  a flag that does nothing is the defect this component has already paid for.
- **Transport**: `X-Firmware-SHA256` header, or `?sha256=` for clients that
  cannot set headers. Both are parsed before the body, so both are available when
  the upload starts — a multipart field would arrive wherever it sits in the body.
  ESPAsyncWebServer 3.12 stores every header unconditionally
  (`WebRequest.cpp:698`) and the ESP32Async fork dropped `collectHeaders()`;
  checked in the resolved package rather than assumed.
- **The browser form is unaffected and stays unverified.** It cannot send a
  digest: a header needs JavaScript, and `crypto.subtle` needs a secure context,
  which a device answering plain HTTP on a LAN is not. With `requireUploadHash`
  set, that form is rejected. That is the trade, and it is documented rather than
  engineered around.
- **Also fixed**: `OTAWebUI` discarded `beginUpload()`'s return value, so a
  refusal surfaced one chunk later as the misleading "Upload not active". Its
  `authFailed` flag is now `rejected` — auth was no longer the only way in.
- **Verified on both platforms** (2026-08-26). `nodemcuv2`: 6/6, and with SEC-7
  removed the two new tests fail at `copyCommandStaged()` — a mismatched
  **upload** armed the bootloader, exactly as a mismatched download did before
  SEC-2. ESP32-CAM: 6/6, and removing SEC-7 moves the boot partition to an
  unverified upload and lets `requireUploadHash` accept an upload with no digest.
  On the host, 4 more tests via the `Update_Stub.h` call counters, including one
  asserting that a `requireUploadHash` refusal never reaches
  `HAL::OTAUpdate::begin()`.

### SEC-9 — OTA: the upload path sizes itself from the multipart envelope [LOW] — **DONE (2026-08-27)**

> **Do not "fix" this by passing `0`.** That was this entry's own recommendation
> for a day, and it is a regression on three counts — see *What the recorded fix
> would have done* below. The envelope stays where it is. If you are here to
> change `OTAWebUI.h:399`, you are in the wrong place.

- **File**: `DomoticsCore-OTA/include/DomoticsCore/OTAWebUI.h:399`,
  `DomoticsCore-OTA/src/OTA.cpp`
- **Found by**: the real-conditions campaign of 2026-08-27, on a `nodemcuv2` and a
  WROOM-32D. Suspected while reading the code, then measured on both.
- **Problem**: `size_t expectedSize = request->contentLength();` measures the
  whole `multipart/form-data` body — boundary, part headers, trailing boundary —
  and not the firmware. The upload handler receives only the firmware bytes,
  ESPAsyncWebServer having stripped the framing.
- **Measured, and the figure is a constant**:

  | board | announced to `beginUpload()` | actually received | delta |
  |---|---|---|---|
  | `nodemcuv2` | 475 452 | 475 232 | **220 B** |
  | WROOM-32D | 982 508 | 982 288 | **220 B** |

- **This entry originally listed three consequences. Two of them were wrong**, and
  they were checked against the installed Arduino cores rather than argued:

  1. ~~Progress never reaches 100 %.~~ **The component's does.**
     `finalizeUpdateOperation()` sets `progress = 100.0f` at `OTA.cpp:711`,
     `requiresBuffering()` returns `false` on both platforms, and
     `test_ota_upload_settles_in_idle_without_autoreboot` has asserted
     `getProgress() == 100.0f` since TEST-3 — so the value the device holds is not
     the problem. **What a browser displays was never measured, and is not the
     same claim.** The bar is fed by an SSE broadcast on a ~5.4 s cadence and
     `autoReboot` restarts the device 2 s after completion, so a bar that stops
     short is entirely possible without `progress` ever being wrong. The original
     observation was made on two boards through a browser; this refutes the
     mechanism it named, not the thing it saw. Whether a browser ever renders
     100 % belongs to TEST-8.
  2. ~~`Update.begin()` is opened 220 bytes too large, so the image is
     incomplete.~~ **True, and not a defect of the envelope.** A finished image is
     an exact equality — ESP32 `_progress == _size` (`Update.h:116`), ESP8266
     `_currentAddress == (_startAddress + _size)` (`Updater.h:165`) — and `end()`
     refuses anything short of it without `evenIfRemaining` (ESP32
     `Updater.cpp:289`, ESP8266 `Updater.cpp:226`). A
     streaming upload never knows its length before the last chunk, so **no
     announced figure removes this dependency**. Passing `0` widens it: `_size`
     becomes the whole partition (`Updater.cpp:159`) or the whole free sketch
     space (`Update_ESP8266.h:34-36`), turning a 220-byte shortfall into hundreds
     of kilobytes.
  3. **SEC-8's ceiling is compared against the envelope.** True, and the only real
     one. The envelope is an upper bound, so the pre-write check can over-refuse a
     firmware that would have fitted, and the message quoted a number that is not
     the firmware size.

- **What the recorded fix would have done.** Passing `0` fixes (3) by removing the
  check that causes it — and with it SEC-8's pre-write refusal on the browser
  path, the only path a human uses. That ordering is not decoration:
  `HAL::OTAUpdate::begin()` targets `esp_ota_get_next_update_partition()`
  (`Updater.cpp:134`), the partition holding the image `canRollBack()` would boot
  (`Updater.cpp:98`) — and on a single-slot ESP32 the running code itself, as
  `DomoticsCore-OTA/platformio.ini:32-37` already recorded. It also turns
  `progress` from 99.954 % into a flat 0 %, since `expected == 0` takes the
  `progress = 0.0f` branch at `OTA.cpp:329`.
- **Fixed**, four changes, none of them to the value passed at the call site:
  1. The refusal in `beginUpload()` says what it compared, and the comment above it
     records the contract: `acceptUploadChunk()` is authoritative on the ceiling
     because it counts what arrives; the pre-write check is a deliberately
     conservative fast-fail on the announced envelope. The message is 96
     characters at its widest, against the 128-byte `DOMOTICS_DLOG_BUF_SIZE` an
     ESP8266 formats it into — a longer sentence would have lost its qualifier
     first, on the platform where the ceiling is tightest.
  2. `finalizeUpload()` narrows `totalBytes` to `uploadSession.received` **above
     `EVENT_END`**, not merely before `EVENT_COMPLETED`. Narrowing later was the
     first attempt and it made one upload announce 236 on `ota/end` and 16 on
     `ota/completed` — self-contradictory, and worse than the single wrong figure
     it replaced.
  3. `OTAWebUI.h:399` carries a comment saying why the envelope is passed on
     deliberately. Every other decision on that handler is annotated in place, and
     whoever deletes it will be reading that line rather than this file.
  4. `Update_Stub.h` records the `evenIfRemaining` argument, and the native suite
     asserts it on both the upload and the download path. Consequence (2) is
     inherent, so it is **pinned rather than fixed**.
- **What the pin does not cover, stated because a reviewer had to find it.** The
  stub's `end()` returns `true` whatever it is passed, so the assertion observes
  the argument `OTA.cpp` passes and nothing further. Changing `Update_ESP8266.h`
  or `Update_ESP32.h` to hand `false` to the real Updater leaves every test in
  this repository green and breaks every browser upload on a board. Neither device
  suite reaches it either: both announce exactly what they deliver, so their
  images are already finished when `end()` is called and the flag is never
  load-bearing. **The one shape that would prove it — announce N + 220, deliver
  N — exists nowhere.** Filed under TEST-8.
- **Also not pinned**: the ordering in (2). The EventBus dispatches after publish,
  so a subscriber reading `getTotalBytes()` sees the narrowed value whichever
  order the code is in, and reading the payload itself needs `on<String>` —
  BUG-30's use-after-free. A test for it was written, **proved vacuous by moving
  the line back and watching it stay green**, and deleted rather than kept as
  decoration.
- **Verified** (2026-08-27): 52/52 native from a cleaned `.pio`, both device suites
  cross-compile. Four removal checks, each red on one assertion and no other:
  `finalizeUpload()`'s `end(true)` → `end(false)` fails only the upload
  `evenIfRemaining` assertion; `installFromUrl()`'s fails only the download one;
  dropping the `totalBytes` narrowing fails only with `Expected 16 Was 236`;
  shortening the warning fails only the qualifier check. A fifth check is why the
  ordering above is recorded as unpinned — it found the test for it was vacuous.
  An earlier run of the first check proved nothing at all, having been aimed at a
  line number that had moved; it was redone by matching the text.
  **Nothing here ran on a board**, which for a defect that only a board produced is
  the honest limit of this lot.
- **Downgraded MEDIUM → LOW** on what survived: one over-refusal window of ~220
  bytes against a ceiling nobody sets to the byte, and one overstated figure in a
  completion event. The severity says what the defect is; the warning at the top
  of this entry does the scheduling.
- **Left open by this lot, and closed by the next**: `finalizeUpdateOperation()`
  did `downloadedBytes = totalBytes` for downloads too, where `totalBytes` is the
  size the *server* announced — so a server that announced 64 and streamed 32
  completed reporting 64, and one that announced nothing completed reporting
  nothing. SEC-8 exists because servers lie about that number, so calling the
  download side "correct" would have been the same overstatement this entry
  removed from uploads. Recorded as TEST-8 hole 3 rather than fixed here, on the
  grounds that it is a different path with a different lying party and deserves
  its own removal check. It got one, the same day.

### SEC-8 — OTA: `maxDownloadSize` was enforced on downloads and not on uploads [MEDIUM] — **DONE (2026-08-27)**

- **File**: `DomoticsCore-OTA/src/OTA.cpp` — `beginUpload()`, `acceptUploadChunk()`,
  `installFromUrl()`
- **Found by**: SEC-7, 2026-08-26. Recorded then as a loose observation with no ID;
  filed here when the OTA lot that fixes it was opened.
- **Problem**: the ceiling applied to the transfer this device *initiates* and to
  nothing else. `installFromUrl()` checked `config.maxDownloadSize` against the
  announced size at `OTA.cpp:495`; `beginUpload()` took `request->contentLength()`
  straight from `OTAWebUI.h:399` and never consulted the field, and
  `acceptUploadChunk()` bounded nothing. A deployment that set a ceiling had it
  apply to the path it controls and not to the one anybody authenticated could
  POST to. Same shape as SEC-1: a config field that reads as a policy and is not
  one on every path.
- **Second half of the same defect**: even on the download path the check read the
  size the *server announced*. A server that announces a small image and streams a
  large one was never stopped.
- **Fixed**, three checks:
  1. `beginUpload()` refuses an announced size over the ceiling **before**
     `HAL::OTAUpdate::begin()` erases flash — the ordering SEC-7 established, for
     the same reason.
  2. `acceptUploadChunk()` refuses the chunk that would carry the running total
     past the ceiling. This is the check that matters: `Content-Length` is
     optional, `beginUpload(0)` means "size unknown", and the announced-size check
     cannot see that case coming at all.
  3. `installFromUrl()`'s chunk callback does the same against `downloadedBytes`,
     which closes the lying-server hole above.
- **Verified on both boards** (2026-08-27). `nodemcuv2`: 8/8. ESP32-CAM: 8/8,
  with both refusals visible in the log for the right reasons
  (`65536 bytes announced against a 32768 byte ceiling`, then
  `33280 bytes would pass a 32768 byte ceiling` for the upload that announced no
  size at all). **That first line is a record of what was observed on the day**;
  SEC-9 reworded the message on 2026-08-27 and the code now emits
  `65536 announced bytes (framing included) against a 32768 byte ceiling`.
- Both device tests assert on the error *message*, not merely on a refusal: the
  Updater has size limits of its own, and a test content with "it said no" would
  be crediting the platform for our check. That is the trap the ESP32 suite
  walked into with the synthetic payload on 2026-08-26.
- **The removal check, and a correction.** With SEC-8 disabled, the ESP32 fails
  both new tests independently: test 7 logs
  `Upload started | expected bytes=65536` against a 32 KB ceiling, and test 8
  opens a session of unknown size and reports `64 KB went past a 32 KB ceiling`
  — its own assertion message.

  **It did not read that way at first, and the first reading was wrong.** A
  failing Unity assertion longjmps out of the test, so test 7's failure left an
  update open, and test 8 died at `beginUpload()` with
  `Updater.cpp:116 begin(): already running` before reaching anything it was
  meant to measure. Its failure was a cascade. Both device suites were briefly
  documented — and PR #32 merged — claiming the second test had demonstrated
  something it never ran. Line 314 of the ESP8266 suite is the same assertion, so
  the same cascade applied there.

  Fixed by releasing any open update in `tearDown()` on both suites. This is the
  2026-08-26 lesson in mirror image: a hardware test can *fail* for the
  platform's reasons rather than yours, and a red result invites no scrutiny at
  all. Ask what a failure actually reached, not only whether it failed.
- **Redone on the ESP8266** (2026-08-27, `nodemcuv2` on `/dev/ttyUSB1`). With
  SEC-8 disabled on the corrected suite, both tests fail independently — test 7
  on `Expected FALSE Was TRUE`, test 8 on its own message,
  `4 KB went past a 2 KB ceiling`. No cascade, and the six pre-existing tests
  still pass. SEC-8 restored: 8/8.

  That message is the claim PR #32 made before anything had measured it. It
  happens to be true. It was still an inference dressed as a reading, and it took
  a third run on the corrected suite to become a fact.
- **Both tests are therefore proven non-vacuous on both platforms.** Test 7 always
  was; test 8 became so on the ESP32 with PR #33 and on the ESP8266 here.
- **On the ESP32-CAM dropping off the USB bus.** It happened once, mid-check, and
  was written up here as the documented brownout. On a replug the same firmware
  ran to completion, and four consecutive flash-and-run cycles followed without
  incident — so it was a one-off link event and the brownout attribution was
  confidence the evidence did not support. Worth knowing that a stalled sketch
  cannot explain it either: the FTDI is a separate chip on USB power, so a hung
  ESP32 gives silence on the port, not a port that ceases to exist.

  Both boards were then run with both adapters attached at once — ESP32-CAM on
  `FTB6SPL3`/`ttyUSB0`, `nodemcuv2` on `A5069RR4`/`ttyUSB1`, including the cable
  that was under suspicion — and neither left the bus across the flash-and-run
  cycles this entry describes. Nothing here supports blaming a cable.
- **Not covered**: the ceiling is a byte count, not a rate limit or a concurrency
  bound. An upload within the ceiling can still be repeated.

### SEC-10 — WebUI: unauthenticated cross-origin state change reaches firmware install [CRITICAL] — **DONE (2026-08-29)**

- **File**: `WebUI.h` (dispatcher, `/api/ui/action`), `OTAWebUI.h` (`/api/ota/upload`)
- **Found**: 2026-08-29, while scoping TEST-6; measured the same day on a `nodemcuv2`.
- **Problem**: every state-changing WebUI route was reachable by a request that did
  not originate from the device's own page. `enableAuth` defaults false, so no
  credential is required; and authentication would not close it anyway, because a
  browser attaches cached Basic credentials to cross-origin requests. The
  dispatcher registered `/api/ui/action` as `HTTP_GET` (`WebUI.h:544`) and
  synthesised the method string `"POST"` (`WebUI.h:871`), so every provider's
  method guard passed a GET — an `<img>` tag could drive any provider mutation.
- **The measured worst effect is firmware install, via `/api/ota/upload`.** An
  unauthenticated `multipart/form-data` POST — no auth, no `X-Firmware-SHA256`
  header, no `sha256` parameter — installs arbitrary firmware and reboots
  (`requireUploadHash` and `enableAuth` both default false; `enableWebUIUpload`
  defaults true). It is reachable cross-origin because `multipart/form-data` is
  CORS-safelisted and the request carries no custom header, so no preflight
  stands in the way of `fetch(url, {mode:'no-cors', body: formData})`. Confirmed
  on the board: uptime dropped 212567→47177, the device rebooted into the image.
- **What was refuted.** The finding was first written as "one `<img>` tag installs
  firmware" via `/api/ui/action?field=start_update`. Measured, that path is inert
  as shipped: the request is accepted but `installFromUrl` returns at `"No
  downloader set"` (`OTA.cpp:566`), because nothing wires a downloader outside the
  test suites. It installs only for a downstream app that supplies one — kept as
  SEC-11/SEC-12, not the CRITICAL. **A filed finding is a hypothesis**; this one's
  scariest sentence fell at the first curl, the same lesson as SEC-9.
- **Fix**: a per-boot CSRF token, minted in `WebUIComponent::begin()` from
  `HAL::Platform::getRandomBytes` (`esp_random` / `os_get_random`, never Arduino
  `random()`). `GET /api/ui/token` serves it and never carries CORS headers, so
  cross-origin script cannot read it. A public `checkCsrf()` (header `X-DC-Token`
  or `token` query, checked unconditionally, independent of `enableAuth`) gates
  every state-changing route: `/api/ui/action` (now `POST`, parameters still in
  the query string), `/api/ota/upload` (at the completion handler and at upload
  chunk index 0, before flash is erased), `/api/ota/check`, the action branch of
  `/api/ota/update`, and `/api/components/enable`. `app.js` fetches the token at
  init and refetches once on a 403, because this lot's own paths reboot the device
  and rotate the token.
- **Verified on the board, both directions** (2026-08-29, `nodemcuv2` running
  `OTAWithWebUI`; FullStack cannot serve — CI-14). Old GET `/api/ui/action` shape
  → 404; POST action without token → 403; upload without token → 403 and flash
  untouched. With the token: action takes effect (`theme` flips live), upload
  installs and reboots. Token rotates per boot and the stale one is then refused.
  Nothing here compiles natively (`WebUI.h` needs `<ESPAsyncWebServer.h>`), so
  there is no native test; the browser-origin leg is reasoned from the CORS
  safelist and curl-confirmed for auth/hash. Compiles esp32dev + esp32c3.
- **The token route never calls `addCorsHeaders`**, so its response carries no
  `Access-Control-Allow-Origin` and stays unreadable to cross-origin script
  regardless of `enableCORS`. Keeping `enableCORS` false is still correct (SEC-6),
  but the missing CORS header at this call site — not the flag — is what protects
  the token.

### SEC-11 — OTA: `/api/ota/update` and `/api/ota/check` have no authentication [HIGH] — **DONE (2026-08-29)**

- **File**: `OTAWebUI.h` — `/api/ota/update` (`:287`), `/api/ota/check` (`:280`)
- **Problem**: both carried **no authentication check at all**, even when
  `enableAuth` was true, while the sibling upload routes checked inline — so the
  omission read as an oversight, not a decision. Reachable cross-origin by an
  auto-submitted form.
- **Downgraded from CRITICAL to HIGH by measurement.** Their firmware payload runs
  through `triggerUpdateFromUrl` → `installFromUrl`, which is downloader-gated and
  inert as shipped (see SEC-10). The auth gap is real; the takeover through it is
  conditional on a downstream downloader.
- **Fix**: `checkCsrf()` now gates `/api/ota/check` and the action branch of
  `/api/ota/update` (the no-parameter read branch stays open, since `ota_manager`
  polls it). The token closes the unauthenticated-reach hole regardless of
  `enableAuth`. Verified with the SEC-10 board run.

### SEC-12 — OTA: a URL install has no integrity check [MEDIUM]

- **File**: `DomoticsCore-OTA/src/OTA.cpp` — `installFromUrl(url, "", …)` (`:135`)
- **Problem**: when a downstream app wires a downloader, `triggerUpdateFromUrl`
  installs from a URL with `expectedSha256 = ""` — no hash, no signature, plain
  HTTP. The download counterpart to SEC-7's upload gap.
- **MEDIUM, by parity with SEC-7, argued rather than inherited.** It was first
  filed HIGH; re-argued 2026-08-29 it is MEDIUM, for the same reason its upload
  counterpart SEC-7 is MEDIUM. It is a legitimate-user-installs-corrupt-firmware
  risk, not an unauthenticated takeover: it is **doubly conditional** — it needs a
  downloader wired to be reachable at all, and once SEC-10 lands it needs
  credentials — so the unauthenticated half of the risk is already closed by
  SEC-10. Rating it HIGH while its identical-shape sibling SEC-7 sits at MEDIUM one
  entry away was the unargued inflation this repository has corrected before (BUG-2).
- **Filed, not fixed.** Requiring a digest for a URL install is a behaviour change
  for legitimate users and needs a decision about where the hash comes from (a
  manifest).

### SEC-13 — WebUI: the SSE stream and `/api/system/info` ignore `enableAuth` [MEDIUM]

- **File**: `WebUI.h` (`/api/system/info`, `:569`), `WebSocketHandler.h` (`AsyncEventSource`, `:65`)
- **Problem**: `/api/system/info` has no auth gate at all, and the SSE source is
  added without `setAuthentication`/an auth middleware, so both serve an
  unauthenticated client full live state even when `enableAuth` is on.
- **Filed, not fixed** — out of this lot's scope (it closes the state-change path,
  not every read). The SSE handler can take an auth middleware; `/api/system/info`
  needs the same gate as its siblings.

### SEC-14 — WebUI: authentication can be enabled with an empty password [MEDIUM]

- **File**: `WebUI.h:374` — `handleWebUIRequest`, `webui_settings`
- **Problem**: the password setter skips an empty value (`if (value.length() > 0)`),
  so `enable_auth=true` with no password yields a device that looks protected and
  accepts `admin` with an empty password. And an attacker who can set credentials
  then enable auth (pre-SEC-10) could lock the owner out.
- **Filed, not fixed** — SEC-10 closes the unauthenticated route to it; the
  empty-password guard itself is a separate one-line refusal, deferred.

---

## Priority 2: Memory Safety (Constitution XIV — ABSOLUTE PRIORITY)

IoT devices run 24/7. Any memory leak eventually causes OOM crash.

### MEM-1 — Missing `shrink_to_fit()` across 6 components [HIGH]

Multiple `clear()`/`erase()` operations without `shrink_to_fit()`, violating Constitution XIV.

| Location | Container | Operation |
|----------|-----------|-----------|
| `ComponentRegistry.h:212,216,258` | `initializationOrder`, `components`, `listeners` | `erase(remove(...))` |
| `EventBus.h:146` | `lastByTopic[topic]` | `clear()` in `publishSticky` |
| `EventBus.h` (reset) | all subscription maps | `clear()` in `reset()` |
| `IWebUIProvider.h:632` | `cachedContexts_` | `clear()` in `invalidateContextCache()` |
| `RemoteConsole.h` | `clients`, `clientBuffers`, `clientAuthenticated`, `clientConnectTime` maps | `erase()` on disconnect |
| `Wifi.h` | `scanNetworks` result vector | never shrunk |
| `System.h:99` | `stateCallbacks` | unbounded `push_back()`, no size limit |

- **Refs**: CORE-F1, CORE-F5, CORE-F11, WEB-F4, RC-F8, RC-F9, WIFI-F4, SYS-F3
- **Fix**: Add `shrink_to_fit()` after every size-reducing operation. Add bounds check on `stateCallbacks` (max 8).

### STOR-ESP-1 — Storage did not leak; the suite measured the EventBus [WITHDRAWN]

- **Filed**: 2026-08-23 as HIGH, from the first run of the on-device suites on
  real hardware. **Withdrawn 2026-08-25** — the defect was in the measurement,
  not in Storage. Kept rather than deleted: someone will read 3,904 bytes off a
  board again, and this entry is what tells them why.
- **What was measured**, 20 iterations each, and it was reproducible to the byte:

  | Pattern | Heap delta | Per iteration |
  |---|---|---|
  | put + get + remove, 20 distinct keys | 3,856 B | 192 B |
  | put + get + remove, **one** key | 3,904 B | 195 B |
  | put only, **one** key overwritten | 2,448 B | 122 B |
  | open / use / close a namespace (×5) | 64 B | 12 B — passed |

- **What it actually was**: `StorageComponent` emits `storage/changed` on every
  put and remove. `EventBus` queues each event at roughly 122 B and releases it
  only when `poll()` dispatches it, which firmware reaches through `Core::loop()`
  on every pass. **The suite never called `Core::loop()`**, so it measured queue
  occupancy and charged it to Storage.
- **The arithmetic closes.** 2,448 B over 20 undrained events is 122.4 B each.
  The two put/get/remove tests queued 40 against a cap of 32 (`EventBus.h:238`)
  and measured 3,904 B and 3,856 B — 32 events' worth. That is the ceiling, not
  a slope.
- **Why "per operation, not per key" was wrong.** Twenty iterations never reached
  the 32-entry cap, so bounded growth was indistinguishable from unbounded. The
  two tests that appeared *worse* were simply the ones that hit the ceiling. The
  seven-hour heap-exhaustion figure derived from that reading does not hold: the
  queue is capped, drops the oldest to make room, and real firmware drains it
  every pass.
- **Verified on hardware**: adding `Core::loop()` to the three measuring loops
  turns every failing test green against library code byte-identical to
  `07cb37a9`. Candidates `doc.shrinkToFit()` per mutation and per-operation
  document loading were both measured on the board and moved the figure by zero
  bytes. `Storage_ESP8266.h` was never changed.
- **Closed by** `test(storage): the ESP8266 heap suite measured the EventBus, not
  Storage`. The suite now runs 7/7 on a `nodemcuv2`, and two new tests hold the
  behaviour in place: undrained growth must plateau at the queue cap, and
  draining must give the memory back. Both open with a floor assertion, so
  neither can pass by measuring nothing.
- **Left open behind it**: `EventBus::enqueue` never decrements `pendingByTopic`
  for the event it drops on overflow, and `HeapTracker` charges its own
  checkpoint node to the window that follows it. Both are recorded in
  `_bmad-output/implementation-artifacts/deferred-work.md`.

### MEM-2 — String concatenation in hot paths across 10 components [HIGH] — **DONE (2026-08-29)**

Constitution XIV bans `String` concatenation in loops and hot paths. Use `snprintf()` with static buffers.

> **What DONE means here, stated at the top rather than six bullets down.** Every
> row has a verdict, the code changes are landed, and the text both rewritten
> loops produce is pinned by tests that run in CI — including a mutation check
> proving those tests fail against a wrong rewrite. **The `nodemcuv2` run has
> happened** (2026-08-29): 3/3, and the removal check against the reverted loops
> puts **40 bytes live at the last iteration** where the fix holds 8 or fewer.
> One qualification the suite makes itself: the *synchronous* loop is what that
> figure measures. The async summary loop has no per-iteration hook, so its test
> passes in both versions and its fix rests on the two loops being the same
> rewrite — stated here rather than folded into the number.

The heading said *9 components* from the day it was filed and the table below has
always listed ten — HomeAssistant, SystemInfo, WiFi, LED, RemoteConsole, NTP,
OTA, System, Storage, WebUI — across its eleven rows. Corrected here rather than
left to be re-counted by the next reader.

**The threshold this finding was reasoned against was wrong on the board it
matters on, and correcting it is what resolved most of the rows.** `String`'s
small-string buffer is **10 characters on ESP8266 and 14 on ESP32**, not 14 on
both. `SSOSIZE = sizeof(struct _ptr) + 4 - 1`, and `_ptr` differs by core:
`{char*, uint16_t, uint16_t}` — 8 bytes — on the 8266 (`WString.h:309-316`,
`SSOSIZE` 11, `capacity()` 10), and `{char*, uint32_t, uint32_t}` — 12 bytes — on
the ESP32 (`WString.h:299-305`, 15 and 14). The 8266 header's "up to 11 (10 +
\0)" comment is correct, and it was dismissed as stale because the *ESP32*
header carries the same words and there it is wrong. Both were read; only one was
believed, and it was the wrong one. **Every count below states which core it is
about.** The measured board is a `nodemcuv2`.

The second fact governing every row is that growth costs less than it looks.
`WString.cpp:229` rounds capacity to a 16-byte multiple, so an accumulator
reallocates once per 16 bytes crossed — but the ESP8266 core builds `umm_malloc`
with `UMM_REALLOC_DEFRAG` (`umm_malloc_cfg.h:514`, `UMM_REALLOC_MINIMIZE_COPY`
commented out just above it), and that path assimilates the following block when
it is free, growing **in place with no copy**. For a string growing at the top of
the heap — the shape of all four accumulators this finding names — that is the
ordinary case, not the exception. An early draft of this lot costed the rows at
"~2.5 KB of cumulative `memcpy`"; that is a worst-case upper bound the allocator
usually does not perform, and it is quoted here only to be withdrawn.

| Component | Location | Hot path? | Description |
|-----------|----------|-----------|-------------|
| **HomeAssistant** | `HomeAssistant.h:157` | YES (every MQTT msg) | **DONE (2026-08-28)** — was `String(ev.topic)` + `String(ev.payload)`; the subscriber now hands `handleCommand` the `char[]` it was given |
| **HomeAssistant** | `HomeAssistant.h:647-737` | YES | **DONE (2026-08-28)** — was `topic.substring()`; the id is scanned with pointers and copied into a stack buffer the size of the event field |
| **SystemInfo** | `formatBytes` / `getFormattedUptime` | Cadence yes, cost no | **REFUTED ON COST (2026-08-28)** — the 5-second cadence is real and lives in `DomoticsCore-SystemInfo/examples/BasicSystemInfo/src/main.cpp:26-40`, which calls the two public wrappers eight times a tick. The component itself never calls them: the only other sites are `test/test_systeminfo_metrics` and the wrappers at `SystemInfo.h:213-214`. Every result — `"45.3 KB"`, `"12d 5h"`, `"1.8 MB"` — is at most nine characters, `"4096.0 MB"` being the widest a `uint32_t` can produce, and every intermediate is narrower still. All of it fits both cores' small-string buffer — **10 characters on ESP8266, 14 on ESP32**, corrected 2026-08-29 from "14 on both" — so none of it ever reaches the allocator. Nine is under ten, so the refutation survives the correction unchanged |
| **WiFi** | scan loop, `getDetailedStatus()` | Moderate | **DONE (2026-08-29) for the scan loop; `getDetailedStatus()` REFUTED ON CADENCE** — the row named one scan loop and there were **two**, character for character identical: the async summary at `Wifi.h:305-308` and the same expression inside public `scanNetworks()` at `:510`. Both now `snprintf` each entry into a stack buffer with one `reserve()` on the accumulator, and `scanNetworks()` moves the entry into the vector instead of copying it. `getDetailedStatus()` (`Wifi.h:471-494`) is five `+=` totalling ~105-120 characters, run once per `wifi` typed at a telnet prompt (registered at `System.h:300`, dispatched at `:576`) — cadence, not cost |
| **LED** | `getLEDStatus()` | Moderate | **MOVED OUT (2026-08-29) → DC-13** — 75-85 characters, and no caller inside the library. That is not the same as no caller: this library is installed by version from the PlatformIO registry, so its callers are other people's sketches, and `getLEDStatus` is documented at `docs/components/led/technical-reference.md:216`. Not a memory fix; a release decision |
| **RemoteConsole** | `help` handler, telnet negotiation | Cold | **DONE (2026-08-29)** — the help handler's 427-character constant part was ten run-time appends of compile-time literals and is now one stored literal, the returned text byte for byte what it was. The row's other half was stale: the "character-by-character String building" it describes was fixed in place long ago (`RemoteConsole.h:572-583` uses a fixed `char[]`, `:665-671` reserves once per line). Not measured on a board, deliberately — `registerBuiltInCommands()` is private, the lambda is reachable only through the telnet read loop, and this component declares no ESP8266 environment |
| **NTP** | `getFormattedUptime()` | Cold | **REFUTED ON COST (2026-08-29), against the corrected boundary** — on the ESP8266 the free cases are `"15s"` (3), `"32m 15s"` (7) and `"5h 32m 15s"` (10, exactly at capacity); the first allocating case is `"1d 0h 0m 0s"` (11), because `days > 0` forces every field to be emitted (`NTP.h:409-413`). So the boundary is **one day of uptime, not the ten days a 14-character reading gives**, and above it the cost is one 16-byte allocation per call. It has no caller inside the library — only `examples/BasicNTP/src/main.cpp:150`, `examples/NTPWithWebUI/src/main.cpp:127` and `test_ntp_component.cpp:345` |
| **OTA** | `transition()`, `broadcastProgress()` | Moderate | **RE-POINTED (2026-08-29) → MEM-5** — `broadcastProgress()` does not exist: removed by `bea43842`, absent from the tree outside planning documents, so half this row named a function that had already been deleted. `transition()` builds `String(" | ") + reason`; `" | Downloading firmware"` is 23 characters, so one ~32-byte allocation per state change, four or five per update — cadence, not cost. What actually costs is `publishStatusEvent` (`OTA.cpp:772-786`) at 1 Hz during an upload, and `snprintf` is not its fix. Filed as MEM-5 |
| **System** | NTP server parsing/saving | Cold (boot) | **REFUTED ON CADENCE (2026-08-29), not on the threshold** — an earlier draft had this backwards. `"pool.ntp.org"` is 12 characters, above the 8266's 10, so even a single default server allocates on Save (`SystemWebUISetup.h:270-276`) and again on load, where `substring` then `push_back` copies it twice (`SystemPersistence.h:247-261`). What closes the row is that it runs once per WebUI Save and once at boot. No test references `ntp_servers` anywhere |
| **Storage** | `dumpContents()` | Cold | **REFUTED ON CADENCE (2026-08-29)** — each entry already goes through `snprintf` into `headerBuf[128]`/`entryBuf[256]`; what remains is `result += entryBuf` per key plus four literal appends, 400-800 characters for 10-20 keys, run once per `storage` typed at a telnet prompt (`System.h:579-586`). An earlier draft said no test calls it, which was false: `test_system_persistence.cpp:242, 247, 258, 268` calls it four times and asserts on its formatted output — a regression net if this row is ever revisited, recorded as an asset rather than a gap |
| **WebUI** | BaseWebUIComponents methods | Cold (setup) | **MOVED OUT (2026-08-29) → DC-13** — `radioGroup` ~840 bytes for four options, `selectDropdown` ~310, both unused inside this repository and both handed to users as copy-paste calls at `DomoticsCore-WebUI/README.md:103,110`, with signatures documented at `docs/components/webui/technical-reference.md:341,345`. Same verdict as the LED row and the same reason |

- **Refs**: HA-F1, HA-F6, SI-F1, WIFI-F2, WIFI-F3, RC-F1, RC-F2, NTP-F8, SYS-F8, SYS-F9, STOR-F8. `LED-F3` and `WEB-F7` went to DC-13 with the rows they describe; `OTA-F8` and `OTA-F9` went to MEM-5. A closed finding should not still be carrying the references its successors answer to.
- **Fix**: Replace with `snprintf()` + stack buffers, or `String::reserve()` before loops — where anything reaches the allocator at all, which the corrected threshold is what decides. Of the eleven rows: **three fixed** (two HomeAssistant, the WiFi scan loop), **one one-line change** (RemoteConsole help), **four refuted** (SystemInfo on cost, NTP on cost against the corrected boundary, Storage dump and the System pair on cadence), **one re-pointed** (OTA → MEM-5) and **two moved out** (LED and WebUI → DC-13). That is 3 + 1 + 4 + 1 + 2 = 11. The WiFi row is counted **once**, under "fixed", although it carries two verdicts — its scan loop was fixed and its `getDetailedStatus()` half was refuted on cadence — because it is one row. An earlier draft of this line counted it in both columns and totalled twelve for eleven rows, in a file whose surrounding prose is about exactly that.
- **Coordination**: four rows live in files `marianorenzi`'s `esp32-ethernet` branch is rewriting — `Wifi.h`, `NTP.h`, `RemoteConsole.h` and `System.h` — and two of those, `Wifi.h` and `RemoteConsole.h`, were changed here. That is part of what he is owed when the notification goes out.

#### The hot half, 2026-08-28 — one row fixed, one refuted

**Open at the time, and closed by the lot below.** Three of the eleven rows moved
— two HomeAssistant rows fixed and the SystemInfo row refuted — leaving eight, and
four of those live in `Wifi.h`, `NTP.h`, `RemoteConsole.h` and `System.h`, which
`marianorenzi`'s `esp32-ethernet` branch is rewriting.

- **The threshold this sweep was filed without — and got wrong, corrected
  2026-08-29.** This bullet used to read: "Both cores give `String` a small-string
  buffer of 14 characters — `WString.h:316` on ESP8266, `WString.h:303` on ESP32
  — and the ESP32 header's 'up to 11' comment is stale." The first half is false
  and the parenthesis is backwards. `SSOSIZE = sizeof(struct _ptr) + 4 - 1` on
  both, but `_ptr` is `{char*, uint16_t, uint16_t}` = 8 bytes on the 8266 and
  `{char*, uint32_t, uint32_t}` = 12 on the ESP32, so `capacity()` is **10 on
  ESP8266 and 14 on ESP32**. The "up to 11 (10 + \0)" comment is *correct* in the
  8266 header and stale only in the ESP32 one, where it was copied. Both headers
  were opened; the wrong one was believed. The point the bullet was making
  survives — every row above was written as if every `String` allocates, and that
  is why one of these two cost nothing at all — but every figure it implied for
  the 8266 was four characters too generous.
- **What HomeAssistant actually cost.** Not three allocations per message —
  **one to three**, by length, and the length that matters is per core. The topic
  always allocates (38 characters and up). The entity id allocates above the
  buffer: `"living_room"` is 11, so it is free on an ESP32 and **allocates on the
  ESP8266**, which the original wording had wrong; `"living_room_ceiling"` is 19
  and allocates on both. The payload likewise (`"ON"` is 2, a light's
  `{"state":"ON","brightness":128}` is 31). So on the measured `nodemcuv2` a
  switch command with a short id cost **1** and now costs **0**; a light command
  with a long id cost **3** and now costs **1** — the surviving one being the
  temporary bound to `HAEntity::handleCommand(const String&)`, which keeps its
  signature. **The 112-byte figure below is unaffected**: it was measured on a
  board with a 26-character id, not derived from the threshold.
- **Why it was worth doing anyway**: all of it ran *before* `findEntity` decided
  the message was HomeAssistant's. `MQTT_impl.h:552` emits `mqtt/message` for
  every message on the shared client, so the framework was asking the allocator
  for memory on its most repeated path to parse a string it had been handed as a
  `char[]`.
- **What the fix is not**: `HAEntity::handleCommand(const String&)` was left
  alone, and no `const char*` sibling was added. The overload would have worked —
  a default implementation delegating to the `String` one keeps every user
  override live — and it was refused on shape rather than feasibility: two
  virtuals for one concept, on a base class users are documented to subclass, to
  remove an allocation that only happens for payloads over the small-string
  buffer — over 10 characters on the ESP8266, over 14 on the ESP32; this line
  said "over 14 characters" on both until 2026-08-29. Recorded here so the next
  reader argues with it instead of rediscovering it.
- **Measured on a `nodemcuv2`** (2026-08-28, `A5069RR4` on `/dev/ttyUSB1`).
  `DomoticsCore-HomeAssistant/test/test_ha_heap_esp8266` runs **4/4** in about 46
  seconds. **112 bytes** is the figure: with the parse reverted to `bea43842` and
  `.pio` cleared so the run could not compile the fixed copy, the same suite
  reports `Expected 0 Was 112` — 45 360 bytes free at the dispatch sample, 45 248
  at the warning — for a 26-character id in a 60-character topic. Two tests go
  red and **both reach the assertion they were written for**, printing their own
  measurements rather than dying earlier; the two that do not measure allocation
  stay green, so the failure is not a cascade. Restored and re-run: 4/4 again.
  CI's `Build on-device suites` job compiles the suite and cannot run it.
- **What the suite does, so the run is reproducible.** Two free-heap samples
  **inside a single EventBus dispatch** — the first from an `mqtt/message`
  subscriber registered before the component, the second from the
  `LoggerCallbacks` hook that fires on the unknown-entity warning, with the topic
  and the extracted id both still live — asserted to differ by zero. Both are
  inside one dispatch because the queue's own `std::vector` copy of the 828-byte
  event (`EventBus.h:28-34`) is live for both and cancels. A net-heap assertion
  would have been vacuous: the Strings were function-local temporaries, freed
  before the function returned, so free heap returns to baseline in both
  versions — the suite carries one anyway, across twenty messages, and says in
  place that it is a leak check and not evidence for this row. The data is
  deliberately on the allocating side of SSO — a 26-character id — because a
  removal check built on an 11-character id and a payload of `"ON"` passes green
  against unfixed code.
- **Native coverage is behaviour, never cost**: the native `String` is
  `std::string` (`Platform_Stub.h:27`). Five suites, 91 cases, five of them added
  here for what the rewrite could silently drop — the two truncation points (63
  for the id, 127 for the command, both with their warnings), the two
  malformed-topic refusals, and `stats.commandsReceived` counting after the
  unknown-entity return and before validation. Two of them are non-vacuous by
  construction rather than by hope: the id test registers a 70-character entity,
  so it fails if the lookup is ever done on the copy already cut to fit the event
  field (verified by making exactly that change); and the switch auto-publish test
  now commands OFF as well as ON, because every assertion in the file passed while
  the call site was made to resolve to `publishState(id, bool)`, which publishes
  `"ON"` for everything.
- **Checked while there, and not worth a row**: `SystemInfoWebUI.h:84-85` builds
  `String((uint32_t)metrics.cpuFreq) + " MHz"`, which the table never listed. It
  sits under `contextId == "system_info"`, whose `hasDataChanged()` returns
  `false` unconditionally (`SystemInfoWebUI.h:139-141`), so it serializes on
  context load rather than on a timer — and the result is 8 characters, so it is
  free either way.
- **Size**: FullStack `esp8266dev` goes from 717,075 to 717,127 bytes of flash —
  **52 bytes more, not less** — with RAM unchanged at 50,808. Removing the
  Strings saved code; printing the entity id as it was delivered rather than as
  it was stored (`%.*s` over the topic, so a log line still matches what the
  broker sent) cost more than that back. The lot is a heap change, and the flash
  figure is recorded as measured rather than as hoped for.

#### The closing lot, 2026-08-29 — one site fixed, one one-liner, six rows resolved by argument

**MEM-2 is closed.** The eight cold rows were verified individually at
`baf5151d`, then attacked by an adversarial pass that overturned three of the
verification's own foundations — the threshold, the cost of growth, and the
instrument. What was left, on the corrected numbers, is one site worth fixing,
one worth a one-line change, and six rows that are not defects.

- **The correction is the finding.** Three of this lot's own conclusions were
  wrong before the pass and are recorded above as withdrawn, not quietly
  replaced: the 14-character threshold (it is 10 on the ESP8266), the "~2.5 KB of
  cumulative `memcpy`" (`UMM_REALLOC_DEFRAG` grows a top-of-heap string in place),
  and the planned allocation counter (`String` grows through `realloc`, never
  `malloc` — `WString.cpp:246` — so a `malloc` counter would have read zero and
  been believed).
- **A grep found one scan loop and there were two.** `Wifi.h:305-308` is the
  async summary; `Wifi.h:510`, inside public `scanNetworks()`, is the same
  expression character for character, exercised by `test_wifi_component.cpp:508`
  and called from `examples/BasicWifi/src/main.cpp:207`. The second site also did
  `networks.push_back(network)` on an lvalue, copying the entry it had just
  built. This is the third time in three lots that a grep has been mistaken for
  an enumeration.
- **What the fix is.** `snprintf` each entry into a 64-byte stack buffer, one
  `reserve()` on the accumulator instead of a reallocation every 16 bytes
  crossed, and `std::move` into the vector at the second site. Text unchanged:
  `"<ssid> (<rssi> dBm)"`, joined by `", "`, capped at ten entries.
- **Residue, stated rather than hidden.** One allocation per network survives,
  because `HAL::WiFiHAL::getScannedSSID` returns `String` by value (`Wifi_HAL.h:92`,
  `Wifi_ESP8266.h:71`), and on the 8266 every scan entry is above the 10-character
  buffer — the shortest possible, `"X (-70 dBm)"`, is 11. Removing it needs a
  `getScannedSSID(char*, size_t)` overload across three platform headers. Out of
  scope, and it is the reason the target here is the temporary chain and the
  copy, not a single-digit allocation count.
- **What runs in CI, which is the half that is actually proven.** The WiFi stub
  returned a hard `0` from `scanNetworks()` (`Wifi_Stub.h:33`), so **neither loop
  body had ever executed on a platform CI can run** — a rewrite could have
  changed the separator, truncated an entry or dropped the ten-entry cap and all
  seven required checks would have stayed green. The stub now takes a scripted
  result table, and eight native cases pin the exact entry text, the `", "` join,
  the ten-entry cap, the zero-network branch, both scan-failure codes, and that
  the async summary is exactly the join of the synchronous entries. **They were
  shown to catch a wrong rewrite** rather than assumed to, with four mutations
  run from a cleared `.pio` each time: shrinking the stack buffer to 16 bytes
  fails three cases, dropping the separator argument fails two, removing the
  ten-entry cap fails one, and restoring the old `n == -1` guard aborts the
  runner outright on `reserve(4294967294)`. That check is what makes these tests
  a net rather than decoration.
- **The measurement, shipped as a suite and not yet run.**
  `DomoticsCore-Wifi/test/test_wifi_scan_esp8266` — `ESP.getCycleCount()` across
  the loop, and a free-heap sample taken *inside* the last iteration while the
  entry String is still live. `DomoticsCore-Wifi` had **no board environment at
  all** before this lot: it declared `[env:native]` and nothing else. It now has
  an `esp8266dev` environment, a `test_ignore` on the native side, and a place in
  CI's `Build on-device suites` list.
- **The async loop has no self-contained discriminating assertion, and the suite
  says so in place.** It has no per-iteration log line, so there is nothing to
  hook inside it; and a free-heap sample *after* it would read **higher** for the
  fixed code, which reserves the worst case up front — a naive threshold there
  would reward the unfixed version. What stands in for it: the native suite pins
  that the async summary is character-for-character the join of the synchronous
  entries, so the loop that *can* be measured and the loop that cannot are held
  to the same output; and the async cycle figure is reported for the two-run
  removal check rather than compared against a threshold. Recorded because the
  honest answer was to state the gap, not to invent a fourth instrument after
  three had already been withdrawn.
- **Why not a free-heap assertion after the loop.** Because it passes with the
  fix removed. Net free heap is identical either way — the copy the `std::move`
  removes is a *transient*, freed when `network` goes out of scope at the end of
  each iteration. The only moment it is visible is inside the iteration, which is
  why the suite hooks `LoggerCallbacks` on the per-entry log line rather than
  measuring around the call. Same reason the cycle count is sampled from that
  hook and not around `scanNetworks()`: that call blocks for ~2 s inside the SDK
  scan, and 160 M cycles of radio would bury the ~10⁴ the loop costs.
- **Measured on a `nodemcuv2`** (2026-08-29, FTDI `A5069RR4`, identified by
  adapter rather than by device node). The suite runs **3/3** in about 49
  seconds. Removal check, with `Wifi.h` restored to `baf5151d` and `rm -rf .pio`
  in *both* directions — these headers arrive through a `file://` dependency that
  `pio` copies once and never refreshes, so a reverted header that is not
  recopied reports the fixed figure twice and concludes the opposite of the
  truth: `scanNetworks() still copies each entry: 40 B were live at the last
  iteration and freed by the return, over 4 networks, threshold=8 B, loop cost
  335188 cycles`. Restored and re-run: 3/3. The derived threshold survived its
  own caveat — the failure message says to suspect the threshold if the figure
  is small and the fix if it is ~24 B or more, and 40 B answers that without
  ambiguity. **What the check does not cover**: the async loop, whose test passes
  in both versions because there is no per-iteration hook to sample from.
- **RemoteConsole's help handler** is one line: 427 characters of compile-time
  literals that were appended ten times at run time are now one stored constant.
  **The method, since the conclusion is only worth what produced it**: both
  bodies — the ten appends and the single literal — were compiled on the host
  with the per-command loop attached and a twelve-command registry, and the two
  results compared byte for byte. 454 bytes each, identical. Not measured on a
  board and the row above says why.
- **Two pre-existing defects fixed in passing**, both inside the function being
  rewritten and both recorded so they are not mistaken for part of the MEM-2
  argument: `scanNetworks()` guarded `n == -1` while `WIFI_SCAN_FAILED` is `-2`,
  and then did `reserve(static_cast<size_t>(n))` — a failed scan reserved
  4,294,967,294 entries on a 40 KB heap. And its loop index is an `int` passed to
  a `uint8_t` parameter, so more than 255 networks wrapped to 0 and returned
  duplicates. Both are now guarded and both are pinned natively.
- **Three findings filed, all out of MEM-2 rather than out of a review**: MEM-5
  (OTA's `publishStatusEvent` at 1 Hz — the cost the OTA row was pointing past),
  MEM-6 (the synchronous scan never frees the SDK's result list — a larger cost
  than the copy this lot removed from the same function, and a behaviour change
  to fix), and DC-13 (three public helpers with no non-test caller here and
  documented for other people's sketches — a release decision, not a memory fix).
- **Three more went to `deferred-work.md`, and two of them bear on this lot's own
  justification.** `WifiComponent::loop()` returns early whenever the SSID is
  empty, forty lines before the scan poll — which is the state a user is in when
  they press the WebUI scan button during AP provisioning, so that path cannot
  complete a scan at all. And nothing reads `getLastScanSummary()` back:
  `WifiWebUI` keeps its own copy and never asks the component for the one it
  built. **So "the site a user can trigger repeatedly", which is how the fix was
  prioritised, is not true today.** The rewrite is still right — the loop is
  public API, it runs, and `scanNetworks()` is reachable from a sketch — but the
  urgency was borrowed from a path that is broken upstream of it. Recorded rather
  than quietly dropped.
- **Size**: FullStack `esp8266dev` goes from 717,127 to **716,967** bytes of
  flash — 160 bytes smaller — with RAM unchanged at 50,808. Both figures measured
  from a cleared build tree on either side. Note what the RAM figure does *not*
  say: the help text is a non-`PROGMEM` literal, so its 427 bytes sit in ESP8266
  DRAM exactly as the ten separate literals did. Moving it with `FPSTR` was
  neither done nor argued, and is recorded in `deferred-work.md`.
- **Native**: 59 cases in `DomoticsCore-Wifi` — 51 before this lot, plus the
  eight that pin the scan format — and 32 in `DomoticsCore-RemoteConsole`, green
  from a cleared `.pio`. None of it proves cost: the native `String` is
  `std::string` (`Platform_Stub.h:27`). It proves the text, which is the half
  that can be proven without a board.

### MEM-3 — HomeAssistant entity String properties should be `char[]` [MEDIUM]

- **Ref**: HA-F5
- **File**: `HAEntity.h:28-32`
- **Problem**: 5 `String` members per entity (`id`, `name`, `component`, `icon`, `deviceClass`) fragment heap. With many entities, significant pressure on ESP8266.
- **Fix**: Convert to `char[]` fixed-size buffers, consistent with `HAConfig`/`HACommandEvent` approach.

### MEM-4 — WebUI: permanent static 8KB buffer [MEDIUM]

- **Ref**: WEB-F5
- **File**: `WebUI.h:60`
- **Problem**: `static char wsBuffer_[WEBUI_WS_BUFFER_SIZE]` (8KB ESP32, 1KB ESP8266) permanently allocated even when no clients connected.
- **Fix**: Lazy allocation on first use, or document rationale for permanent allocation.

### MEM-5 — OTA: `publishStatusEvent` allocates a document, a string and a queue entry at 1 Hz during an upload [MEDIUM] — **NEW (2026-08-29)**

- **Opened by**: MEM-2's closing lot, which re-pointed the OTA row here.
- **File**: `DomoticsCore-OTA/src/OTA.cpp:772-786`, driven from `:348-357`
- **What the MEM-2 row got wrong, and why this entry exists.** The row read
  "`transition()`, `broadcastProgress()` — String concat + JsonDocument per
  broadcast". `broadcastProgress()` **does not exist**: removed by `bea43842`,
  and absent from the tree outside planning documents — half the row named a
  deleted function. `transition()` is real and costs one ~32-byte allocation per
  state change, four or five per update, which is cadence rather than cost. The
  cost is in neither of them.
- **Problem**: `publishStatusEvent()` constructs a `JsonDocument`, calls
  `serializeJson` into a `String` that is never reserved, and hands the bytes to
  `emit(topic, payload.c_str(), payload.length() + 1, sticky)`. `EventBus`
  deep-copies into a `QueuedEvent` that owns a `String topic` and a
  `std::vector<uint8_t> data` (`EventBus.h:28-34`), so one call is an
  ArduinoJson pool allocation, a growing serialization buffer, and two more
  allocations inside the queue entry. `OTA.cpp:348-357` runs it **once per second
  for the whole duration of an upload**, on the platform with 40 KB of usable
  heap, while a firmware image is streaming through.
- **`snprintf` is not the fix**, which is why this is not a MEM-2 row. The
  candidates are reserving the serialization buffer to the payload's known
  ceiling, serializing straight into a stack buffer with
  `serializeJson(doc, buf, sizeof(buf))`, or lengthening the 1-second throttle
  during an upload.
- **Not measured.** The 1 Hz cadence and the allocation sites are read from the
  code; nothing has counted the bytes on a board. TEST-8's multipart harness is
  the place that could.
- **Refs**: OTA-F8, OTA-F9 (inherited from MEM-2's OTA row).

### MEM-6 — WiFi: the synchronous scan never frees the SDK's result list [MEDIUM] — **NEW (2026-08-29)**

- **Opened by**: MEM-2's closing lot, from reading the two scan paths side by
  side. **Filed, not fixed** — deliberately.
- **File**: `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h`, `scanNetworks()`
- **Problem**: the asynchronous path calls `HAL::WiFiHAL::scanDelete()` once it
  has built its summary (`Wifi.h:341`). The synchronous `scanNetworks()` does
  not, so the SDK's scan-result list — every SSID, BSSID, channel and RSSI it
  found — stays allocated after the function returns, until the next scan
  replaces it or something else deletes it. That is a larger and far
  longer-lived cost than the per-entry copy this lot removed from the same
  function, which is the uncomfortable part: the loop was optimised and the
  allocation next to it was not.
- **Why it is not fixed here**: adding `scanDelete()` is a behaviour change, not
  a memory fix. Anything that calls `getScannedSSID()` after `scanNetworks()`
  returns — legitimate, since both are public HAL surface — would start reading
  a freed list. Deciding that needs a look at what users do with the HAL, and
  `Wifi.h` is being rewritten on `esp32-ethernet` in any case.
- **Fix, when it is decided**: either delete inside `scanNetworks()`, since it
  has already copied everything it needs into the caller's vector, or document
  the ownership and give the component an explicit release. Not both.

---

## Priority 3: Code Safety & Critical Bugs

### BUG-1 — Core: `reinterpret_cast` without trivially_copyable check [HIGH]

- **Ref**: CORE-F4
- **File**: `EventBus.h:101-107`
- **Problem**: `publish()` template uses `reinterpret_cast` to byte-copy payloads. Publishing a `std::string` or `String` results in dangling pointers in the event queue.
- **Fix**: Add `static_assert(std::is_trivially_copyable<PayloadT>::value, ...)`.

### BUG-2 — Core: `getComponent<T>()` promises a safety it does not provide [MEDIUM]

- **Ref**: CORE-F2
- **File**: `DomoticsCore-Core/include/DomoticsCore/Core.h`
- **Both fixes this entry used to recommend are impossible**, measured on
  2026-08-28 rather than assumed:
  - **`dynamic_cast` does not compile.** Not "if RTTI enabled" — RTTI is off on
    *both* Arduino platforms: `error: 'dynamic_cast' not permitted with
    '-fno-rtti'`, from `pio run -e esp8266dev` and `-e esp32dev` on the `FullStack`
    example, the only one that pulls all twelve components.
  - **Type-key verification would check nothing.** `getTypeKey()` defaults to `""`
    (`IComponent.h:129`) and is documented as a WebUI mechanism. **Two of twelve
    components override it** — `OTAComponent` and `SystemInfoComponent`. For the
    other ten the comparison is `"" == ""`, which matches everything.
- **A probe that proved nothing, recorded because the pattern keeps recurring.**
  Building `test_ota_esp8266`, `test_ota_esp32` and `native` with a `dynamic_cast`
  in place succeeded on all three. The device suites construct `OTAComponent`
  directly and never call `getComponent<T>()`, so the template was never
  instantiated and never type-checked; the native environment does instantiate it,
  and the host toolchain has RTTI on. Three green builds, none of them about the
  question. Only `FullStack` answered it.
- **What the defect actually is.** Not "UB in theory". The function's own
  documentation promised *"Pointer to component cast to T or nullptr if not
  found"*. It honours that for an unregistered name and silently breaks it for a
  registered name held by another type. A public accessor that returns `nullptr`
  on one failure mode and undefined behaviour on the other is a contract that lies.
- **And the damage is worse than a wrong vtable on two components.**
  `WebUIComponent` (`IComponent`, `CachingWebUIProvider`,
  `IComponentLifecycleListener`) and `WifiComponent` (`IComponent`,
  `INetworkProvider`) inherit from more than one base, so `static_cast` performs a
  **pointer adjustment** for a subobject the real component does not have. The
  result points outside the object. `WebUIComponent` is among the most frequently
  fetched components in the repository.
- **What triggers it**: an application pairing a valid name with the wrong type —
  `getComponent<StorageComponent>("MQTT")`. A *misspelled* name returns `nullptr`,
  and that is the common slip. All 70 in-repo call sites pair name and type
  correctly, and nothing in the library calls it with a caller-supplied string.
  Component names are ordinary strings, so an application registering its own
  component as `"Storage"` and fetching it as one would reach it.
- **Fixed by narrowing the contract, not the code.** The documentation now states
  that the name is the lookup and `T` is an unchecked claim, names the two
  multiple-inheritance components, and says why no runtime check exists. Nothing
  else changes: there is no guard that can be added without a new concept.
- **Not done, and deliberately.** A static type identity on the component API
  would work — a macro in `IComponent.h`, one line per component, checked by
  SFINAE so non-adopters keep today's behaviour. It is rejected for now because it
  adds a concept to the public API of a library people install by version, and a
  partial adoption protects partially and *silently* — the same shape as SEC-2's
  inert fix, the `end(false)` pin that stopped at the HAL, and the probe above. If
  the component API moves for other reasons — `marianorenzi`'s transport-neutral
  rewrite is the obvious one — this grafts onto that change rather than preceding
  it.
- **Downgraded HIGH → MEDIUM (2026-08-28).** The severity had never been argued,
  only inherited: it arrived at HIGH on 2026-08-27 when enumerating the `[HIGH]`
  headings found it missing from both the rows and the total. Re-argued on merit:
  no path in the repository reaches it and the common failure mode is already
  safe, which rules out HIGH; the consequence on the two multiple-inheritance
  components is an out-of-object pointer rather than a theoretical curiosity,
  which rules out LOW.
- **No test.** Undefined behaviour cannot be asserted on, and the only assertion
  that would mean anything — "a wrong type returns `nullptr`" — is true only if
  the guard exists. Writing a test here would be circular.

### BUG-3 — Core: `removeCallback()` nukes ALL callbacks [MEDIUM]

- **Ref**: CORE-F6
- **Problem**: `LoggerCallbacks::removeCallback()` calls `getCallbacks().clear()` instead of removing the specific callback.
- **Fix**: Find and erase only the target callback.

### BUG-4 — NTP: use-after-free with `sntp_setservername` pointers [HIGH]

- **Ref**: NTP-F4
- **File**: NTP HAL implementation
- **Problem**: `sntp_setservername()` stores the raw `const char*` pointer. If the source `String` is destroyed or reallocated, the pointer dangles. SNTP will dereference freed memory on next sync.
- **Fix**: Use static `char[]` buffers for server names, or ensure `String` lifetime exceeds SNTP usage.

### BUG-5 — NTP: inconsistent sync thresholds [HIGH]

- **Ref**: NTP-F3
- **Problem**: Component uses `now > 1000000000` (2001) while HAL uses year 2020 threshold. Duplicate, inconsistent logic.
- **Fix**: Single source of truth — delegate to HAL's `isSynced()`.

### BUG-6 — NTP: `uint32_t` vs `unsigned long` mismatch [HIGH]

- **Ref**: NTP-F1
- **Problem**: `bootTime` is `uint32_t` but `HAL::Platform::getMillis()` returns `unsigned long`. On ESP32, `unsigned long` is 32-bit so it works, but on platforms where `unsigned long` is 64-bit, truncation occurs. Wrap-around bug after ~49 days.
- **Fix**: Use `unsigned long` consistently, or document wrap-around handling.

### BUG-7 — MQTT: ODR violation — static member in header [HIGH]

- **Ref**: MQTT-F3
- **File**: `MQTT_impl.h:8`
- **Problem**: `MQTTComponent* MQTTComponent::instance = nullptr;` defined in header. Multiple TU inclusion = linker error. Needs `inline` (C++17) or `.cpp` file.
- **Fix**: Add `inline` keyword or move to a `.cpp` compilation unit.

### BUG-8 — MQTT: dangling broker pointer [MEDIUM] — **DONE (2026-08-23, PR #10)**

- **Ref**: MQTT-F12
- **Problem**: `setBroker()` passes `broker.c_str()` to PubSubClient which stores the raw pointer. If the `String` is later destroyed, the pointer dangles.
- **Fix**: Use a persistent `char[]` buffer for the broker address.

### BUG-9 — MQTT: no QoS validation [HIGH]

- **Ref**: MQTT-F4
- **Problem**: `publish()`, `subscribe()`, and `MQTTConfig.lwtQoS` accept `uint8_t` with no range check. Values > 2 are invalid per MQTT spec.
- **Fix**: Clamp or reject values > 2.

### BUG-10 — HA: `mqttPublish()` always returns true [HIGH]

- **Ref**: HA-F2
- **Problem**: Fire-and-forget publish with a misleading `bool` return. Callers log "Published successfully" on a value that is always `true`.
- **Fix**: Make `void` and remove conditional checks, or implement real acknowledgment.

### BUG-11 — HA: `volatile bool publishing` is dead code [HIGH]

- **Ref**: HA-F3
- **Problem**: Set and cleared but never read. Not thread-safe on dual-core ESP32.
- **Fix**: Remove entirely, or implement actual re-entrancy guard with `std::atomic<bool>`.

### BUG-12 — HA: ODR violation in HAEvents.h [HIGH]

- **Ref**: HA-F4
- **Problem**: `static constexpr const char*` creates separate storage per TU in C++14. Pointer comparison in EventBus would fail across TUs.
- **Fix**: Use `inline constexpr` (C++17) or extern linkage.

### BUG-13 — HA: `HA_TOPIC_BUF_SIZE` too small [MEDIUM]

- **Ref**: HA-F8
- **Problem**: 128-byte buffer can be exceeded with long discovery prefixes + `alarm_control_panel` component + long entity IDs. Silent truncation breaks HA discovery.
- **Fix**: Increase to 256, or add truncation detection with `DLOG_W`.

### BUG-14 — Storage: `putBlob()` nullptr dereference [HIGH]

- **Ref**: STOR-F3
- **Problem**: No null check on `data` pointer. `nullptr` with `length > 0` = crash.
- **Fix**: Add null guard at entry point.

### BUG-15 — Storage: cache never consulted by getters [HIGH]

- **Ref**: STOR-F4
- **Problem**: `getString()`, `getInt()`, etc. always go to HAL storage, bypassing the cache entirely. Cache only used for dirty-checking in `put*()`.
- **Fix**: Check cache first, fall through to HAL on miss.

### BUG-16 — Storage: RAMOnlyStorage truncates uint64 [MEDIUM]

- **Ref**: STOR-F6
- **Problem**: `putULong64` stores via `String((unsigned long)value)`, losing upper 32 bits.
- **Fix**: Use proper uint64 serialization.

### BUG-17 — Storage: RAMOnlyStorage putBytes discards data [MEDIUM]

- **Ref**: STOR-F7
- **Problem**: Only stores the length, not the actual byte data. `getBytes` always returns 0.
- **Fix**: Store actual bytes (e.g., base64-encoded or raw vector).

### BUG-18 — LED: phase calculation loses accumulated time [MEDIUM]

- **Ref**: LED-F6
- **Problem**: When `effectPhase > 1.0`, reset to `0.0` instead of wrapping (`fmod`). Causes effect stuttering.
- **Fix**: Use `effectPhase = fmod(effectPhase, 1.0f)`.

### BUG-19 — LED: `addLED()` after `begin()` causes vector desync [MEDIUM] — **DONE (2026-08-23, PR #17)**

- **Ref**: LED-F8
- **Problem**: `begin()` calls `ledStates.resize()` once. Adding LEDs after `begin()` grows `ledConfigs` but not `ledStates`.
- **Fix as filed**: guard `addLED()` against post-begin calls, or auto-resize
  `ledStates`. **Auto-resizing alone would not have been enough**: `begin()` also
  runs the pin validation and calls `pinMode()`, so a state entry conjured for a
  late LED would have addressed a pin nobody had configured.
- **Fixed**: `addLED()` now does the whole of what `begin()` would have done for
  that one LED — validate, `initializePin()`, append the state — and returns
  `bool` so a rejected pin is reported where the mistake is made rather than
  swallowed. `addSingleLED()`/`addRGBLED()` forward the result; callers ignoring
  it still compile.
- **Silent, not fatal**: nothing read out of bounds. `getLEDCount()` and
  `getLEDNames()` counted from `ledConfigs` while every setter bounds-checked
  against `ledStates`, so the LED appeared in the WebUI dropdown and refused
  every command sent to it.
- **Also**: `begin()` now uses `assign()` instead of `resize()`, so a second
  `begin()` resets `effectPhase` and `lastUpdate` too — `resize()` kept them.
- **Verified**: four tests in `test_led_component` fail against the previous
  implementation and pass against this one.

### BUG-20 — OTA: static variables in header files [HIGH]

- **Ref**: OTA-F4
- **Problem**: `s_bytesWritten`, `s_updateActive`, `s_stubBytesWritten` are `static` in headers. Each TU gets its own copy.
- **Fix**: Add `inline` (C++17) or move to `.cpp`.

### BUG-21 — OTA: `EVENT_START` / `EVENT_END` never emitted [HIGH] — **DONE (2026-08-27)**

- **Ref**: OTA-F3
- **Problem**: Declared in `OTAEvents.h` but never emitted by any code path.
- **This row said `0H` for Code Safety while this item sat open.** BUG-21 has no
  DONE marker in any release table and the constants were still unreferenced in
  `OTA.cpp` on 2026-08-27. The other unmarked HIGH bugs in this section — BUG-1,
  BUG-7, BUG-9 through BUG-14, BUG-20 — are all in the v2.0.1 resolved table;
  BUG-21 is not, and never was. The count has been wrong since that table was
  written, which is a second instance of the arithmetic CLAUDE.md warns about:
  the row and the total are edited in different places.
- **Fixed**: emitted on both entry points. `EVENT_START` after the transition to
  `Downloading` in `installFromUrl()` and in `beginUpload()`; `EVENT_END` when the
  transfer finishes and **before** the SHA-256 verdict, in `installFromUrl()` and
  at the top of `finalizeUpload()`.
- **Why "before verification" is the whole content of the event.** A transfer that
  dies emits `ota/start` then `ota/error`. An image that arrives whole and fails
  its hash emits `ota/start`, `ota/end`, `ota/error`. Nothing else in the event
  stream distinguishes a dead connection from a rejected image, which is why an
  `ota/end` that only fired on success would be worth nothing — the completion
  event already says that.
- **`EVENT_INFO` is kept at the start of an upload.** The published reference names
  it as the upload-start signal and this library is installed by version;
  `ota/start` is added beside it, not in its place.
- **Also**: the six constants are `inline constexpr` rather than `static
  constexpr`, on the reasoning BUG-12 gave for `HAEvents.h`. This costs six
  `inline variables are only available with -std=c++17` warnings on the
  `esp32dev` FullStack target, which builds at `gnu++14`. That warning class is
  already present there from `HAEvents.h`, `MQTT_impl.h` and `Update_ESP32.h`;
  moving that target to `gnu++17` would clear all four at once and is not this
  lot's call to make.
- **Pinned by** five native tests, including the two orderings above. Not tested
  on hardware: nothing about emitting an event is platform-specific, and the
  device suites would only be re-proving what the host already proves.

### BUG-22 — RemoteConsole: unbounded client read [HIGH] — **DONE (2026-08-23, PR #10)**

- **Ref**: RC-F3
- **Problem**: `while (client.available())` with no upper bound. Continuous byte stream = infinite loop.
- **Fix**: Add per-iteration byte limit (e.g., 512 bytes max per `loop()` call).

### BUG-23 — System: Early-Init anti-pattern [HIGH] — **DONE (2026-08-22, PR #7)**

- **Ref**: SYS-F4
- **File**: `System.h:224`
- **Problem**: `registerLEDComponent()` calls `led->begin()` manually before `core.begin()`. Constitution XIII explicitly forbids this.
- **Fixed**: the manual `begin()` is gone; `core.begin()` initialises the component.
- **Worse than the title suggested**: `ComponentRegistry::initializeAll()` skips any
  component already initialised and active, and that skipped branch is the only place
  `__dc_setEventBus()` and `__dc_setCore()` are ever called. The early call removed the
  injection rather than merely reordering it: the LED ran with both framework pointers
  null for the life of the device. Latent only because LEDComponent uses neither —
  `eventBus()` dereferences with no null check, and `on<T>()` returns 0 instead of
  subscribing.
- **Verified by compilation only** — DomoticsCore-System has no test suite (TEST-1).

### BUG-24 — WiFi: dead config fields [MEDIUM]

- **Ref**: WIFI-F6
- **Problem**: `connectionTimeout` and `CONNECTION_TIMEOUT` static const are both declared. `WifiConfig::connectionTimeout` may shadow the static constant, creating confusion.
- **Fix**: Remove one, standardize on a single timeout source.

### BUG-25 — WebUI: blocking component lifecycle in HTTP handler [MEDIUM]

- **Ref**: WEB-F11
- **File**: `ProviderRegistry.h:204-263`
- **Problem**: `enableComponent()` calls `component->shutdown()` or `begin()` directly inside an async HTTP handler. Blocking I/O in ESPAsyncWebServer context causes watchdog resets.
- **Fix**: Queue enable/disable action, process in `loop()`.

### BUG-26 — WebUI: OptionLabelPair serialization bug [MEDIUM]

- **Ref**: WEB-F12
- **File**: `StreamingContextSerializer.h:849-879`
- **Problem**: Key+colon+value written in one iteration. Partial value writes with small buffer cause malformed JSON because `optionIndex` not incremented but key already consumed.
- **Fix**: Split key, colon, value into separate states for correct pause/resume.

### BUG-28 — WebUI: Multiselect value resumes from a destroyed `String` [MEDIUM]

- **Filed**: 2026-08-26, found while investigating DC-11. Not fixed there — it
  sits in the serializer, which is where marianorenzi's `Table` work will land.
  (BUG-27 is reserved for his separate WebUI report.)
- **File**: `StreamingContextSerializer.h` — `writeLiteral` at :431-449, the
  Multiselect value path at :700-708.
- **Problem**: `writeLiteral` caches its `str` argument in the member
  `currentLiteral` so a literal can resume across `write()` calls, keyed on the
  pointer comparing equal. The Multiselect branch builds `String
  serializedValues` **local to the `case` block** and passes
  `serializedValues.c_str()`. If the output buffer fills mid-literal, that
  `String` is destroyed before the next call, leaving `currentLiteral`
  dangling. On the next call the local is rebuilt, usually at a different
  address: the pointers compare unequal, `literalOffset` resets to 0, and the
  array is re-emitted from the start **after part of it was already written** —
  malformed JSON, and the dashboard fails to render the whole schema. When the
  rebuilt `String` happens to reuse the same address the resume is correct only
  by accident, and the equality test itself is a comparison against an
  indeterminate pointer.
- **Note**: this is the same failure mode as BUG-26 in the same file — a
  pause/resume boundary the state machine does not actually survive — which is
  why it is rated alongside it. No test forces a chunk boundary inside a
  multiselect value; the fix needs one.
- **Fix**: Hold the serialized text in a serializer **member** with a lifetime
  spanning the pause, not in a local, and clear it when the field completes.
  **Any future dynamic schema key must do the same** — see DC-12.

---

### BUG-30 — Core: the topic overload of `EventBus::publish` has no trivially-copyable guard [HIGH] — **DONE (2026-08-28)**

- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h:121-129`
- **Found by**: BUG-21, 2026-08-27, while writing tests that subscribe to the OTA
  lifecycle topics.
- **Problem**: BUG-1 added `static_assert(std::is_trivially_copyable<PayloadT>)`
  to `publish(EventType, const PayloadT&)`. The sibling
  `publish(const String& topic, const PayloadT&)` twenty lines below does the
  same `reinterpret_cast` byte copy and carries **no such guard**. It accepts a
  `String`, copies the object's bytes — pointer, length, capacity — into the
  queue, and dispatches them after the publisher's local has been destroyed and
  its buffer freed.
- **Measured, not argued** (2026-08-27, native): a subscriber registered with
  `core.on<String>("ota/start", …)` receives a `String` reporting
  `length() == 114` whose contents are `r\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd`.
  The length is right because it was copied out of the dead object; the bytes are
  freed heap. The native `String` wraps `std::string`, whose SSO buffer holds 15
  characters — every one of these JSON payloads is well past that, so every one
  of them is a heap read after free. On an ESP8266 that is a crash or silent
  corruption rather than mojibake.
- **"Latent" was wrong too, and the v1 review had already said so.** This entry
  claimed nothing dereferences the payload, on the strength of a grep for
  `on<String>` finding nothing. `test_storage_events.cpp:67` and `:94` do it by
  hand — `*static_cast<const String*>(payload)` — on `storage/ready`. Two running
  native tests, exactly as the review of v1 recorded on 2026-08-27 before v2 and v3
  lost the fact. They survived because Storage publishes a config *member* rather
  than a temporary, so the copied pointer stayed valid: the same defect on a slower
  fuse. But
  `docs/components/ota/technical-reference.md` has always listed the payload
  fields of all six OTA events, which is an invitation to read a payload that
  cannot be read. A warning now sits above that table.
- **This entry said `OTA.cpp` was the only caller. It was wrong, and wrong in the
  direction that mattered** (corrected 2026-08-28). That claim came from grepping
  `emit<String>`, which misses every site that lets the template deduce. There are
  **nine** live sites, and the seven this missed are the severe ones:

  | Site | Payload | Lifetime |
  |---|---|---|
  | `Wifi.h:141,221,234,268,740,799,834` | `emit(EVENT_NETWORK_READY, HAL::WiFiHAL::getAPIP())` | **temporary** — already destroyed when `publish` returns |
  | `OTA.cpp:791` | serialised JSON local | local, always heap |
  | `OTA.cpp:780` | serialised JSON local | dead code (`broadcastProgress`, DEAD-1) |

  A temporary is worse than a local: the local at least survives to the end of the
  enclosing statement. On ESP8266 the `String` SSO buffer means any IP of eleven
  characters or more is a heap read after free — which is most of them, and
  `192.168.4.1` exactly.
- **The blocking question was answered by reading the fork, not by asking**
  (2026-08-28). v2 of the design was contingent on whether `marianorenzi`'s
  transport-neutral rewrite needs the bus to carry non-POD payloads. It does not:
  it needs *variable-length* payloads, and **he already wrote that mechanism and it
  is already merged here** — his commit `b6660b78`, "feat(eventbus): publish
  overloads for variable-length payloads", is what put `publish(topic, const void*,
  size_t)` at `EventBus.h:133`. On his branch `Wifi.h` publishes no `String` at
  all; `EVENT_NETWORK_READY` is gone, replaced by `NetworkEvents::EVENT_PROVIDER_*`
  carrying structs. **Seven of the nine sites do not exist in his rewrite.**
- **So the fix is small, and the guard is finally landable.** Publish bytes at the
  nine sites through the overload that already exists, then put the
  `static_assert` on the topic overload. That assert was unlandable while handing
  the bus an object was the only way to publish something variable-length — it
  would have refused legitimate callers with nothing to offer them. `b6660b78`
  gave them the alternative; after the conversions there are no non-trivially-
  copyable publishers left, so the assert breaks nothing that exists and stops the
  next one being written. No POD event structs, no `= delete`d overloads, no
  version bump, **no public contract break** — which also retires the review
  finding that a root `v3.0.0` would protect nobody.
- **Design**: `_bmad-output/implementation-artifacts/spec-bug-30-v3-eventbus-payload-contract.md`.
  v2 is superseded — overtaken rather than refuted — and v1 was refuted by its own
  review. All four of v2's blocking findings were downstream of a shape that no
  longer applies.
- **Fixed (2026-08-28), and the assert enumerated the sites the grep could not.**
  v3 planned for nine. There were **ten**, and the three greps miss are the reason
  the entry recommended letting the compiler find them:

  | Site | Was | Now |
  |---|---|---|
  | `Wifi.h` ×7 | `emit(EVENT_NETWORK_READY, getAPIP())` — a **temporary** | `emitNetworkReady()`, one private helper that takes `const String&` and publishes `c_str(), length() + 1` |
  | `OTA.cpp:791` | `emit<String>(topic, payload, sticky)` | the sized overload |
  | `OTA.cpp:780` | `emit<String>` in dead `broadcastProgress()` | **deleted** — dead code still compiles, so the assert forced the choice v3 got wrong when it said to leave it |
  | `ComponentRegistry.h:128,175` | `publish(EVENT_SYSTEM_READY, String(""))`, same for `EVENT_SHUTDOWN_START` | the no-payload overload — an empty payload carried nothing to convert |
  | `Storage.h:149` | `emit(EVENT_READY, storageConfig.namespace_name)` | the sized overload |

  Grep found seven of ten. The three it missed spell neither `emit<String>` nor a
  String-returning call: two construct `String("")` inline on `eventBus.publish`,
  and one passes a `String` *member*. **This is the third time this week a grep has
  been mistaken for an enumeration.**
- **The payload contract changed, and one subscriber had to change with it.**
  `test_storage_events.cpp` read `*static_cast<const String*>(payload)`; it now
  reads `static_cast<const char*>(payload)`. That is the whole cost, and it is the
  cost the entry always predicted.
- **Verified**: twelve native suites, **724 cases, all green**, each from a cleaned
  `.pio`. FullStack builds on `esp8266dev` and `esp32dev`; both OTA device suites
  cross-compile. On a `nodemcuv2`, `OTAWithWebUI` boots, joins the network and
  serves — which is the converted `Wifi.h` STA path running on silicon.
  **Removal check**: a fresh `publish(String("t"), s)` fails to compile with the
  authored message, which names the alternative. The assert is not decoration.
- **What it did not cost**: no POD event structs, no `= delete`d overloads, no
  version bump, no public contract break beyond the payload type on topics that
  already documented themselves as unreadable. RAM on FullStack/esp8266dev moved
  50904 → 50888 bytes.


### BUG-29 — MQTT: the publish rate limit silently drops discovery [HIGH] — **DONE (2026-08-26)**

- **Filed**: 2026-08-26, found on hardware while validating a WaterMeter build
  against 2.1.1 — an ESP32-D0WD-V3 on a live broker.
- **File**: `MQTT_impl.h:228-239` (the limiter), `HomeAssistant.h:567-581`
  (the caller that ignores the result).
- **Problem**: a device declaring more than ten entities loses the discovery
  messages for whatever is declared last. `publish()` enforces
  `config.publishRateLimit` (default 10, tumbling 1 s window) and **discards**
  anything above it. `publishDiscovery()` sends one config per entity back to
  back, then the initial states — twelve entities is comfortably past ten in the
  same second.
- **Measured**: with 9 sensors and 3 buttons declared in that order, the broker
  retained 9 `sensor/*/config` and **0** `button/*/config`. The three buttons
  never reached Home Assistant.
- **Three things hide it**:
  - `HomeAssistantComponent` logs `Discovery published` after handing the
    message over, so the two layers disagree and only MQTT knows the truth;
  - `publish()` returns `false`, and the discovery path ignores it;
  - the failure is **order-dependent** — adding one sensor can silently remove
    an unrelated button.
- **Correction to the second point above.** There is no return value to ignore.
  `mqttPublish()` (`HomeAssistant.h:526`) is fire-and-forget: it emits
  `EVENT_PUBLISH` on the EventBus and MQTT consumes it later
  (`MQTT_impl.h:59`). The rejection happens in another component, after the
  fact, and nothing carries it back. So there was nothing to fix on the caller's
  side — **the whole fix is in MQTT**, and once nothing is discarded the
  HomeAssistant log stops being a lie by itself.
- **Fix**: the queue this needs already exists. When disconnected, `publish()`
  enqueues into `messageQueue` and drains later; when connected but over the
  limit, it threw the message away instead. The rate-limited case now takes that
  same queue, and `processMessageQueue()` — which already runs on **every**
  connected `loop()`, not just after a reconnect — drains the burst over the
  following seconds without changing the sustained rate.
- **The drain loop had to change too.** It called `publish()` on each entry; now
  that `publish()` defers rather than drops, doing so would `push_back` into the
  vector being iterated — invalidating the iterator and re-queueing what it was
  draining. It now checks the limit itself and stops, leaving the rest to the
  next `loop()`.
- **Behaviour change worth knowing**: `publishRateLimit` no longer rejects
  anything, it defers. And it no longer applies while disconnected — the counter
  advanced on queueing, which both capped a queue that sends nothing and charged
  every deferred message twice, once on the way in and once on the way out.
  Offline, `maxQueueSize` is the bound; the rate limit governs the wire.
- **Tests**: three in `test_mqtt_component.cpp`, and the pre-existing
  `test_mqtt_rate_limit_enforced` was rewritten — it asserted the discard. All
  three fail against the old behaviour; the clearest reads `Expected 5 Was 0`,
  five messages that no longer exist anywhere.
- **Not reproduced on hardware here**: that needs a broker and a device
  declaring more than ten entities. The original observation on a live broker
  remains the evidence for the defect.
- **Also affects production**: any device with more than ~10 entities. The
  entities go missing rather than break, so it reads as a Home Assistant problem
  rather than a firmware one.
- **Tracked publicly** as issue #27.

## Priority 4: Test Coverage (Constitution II — NON-NEGOTIABLE)

TDD with 100% coverage is a constitutional mandate. These components have critical gaps.

### TEST-1 — System: zero tests [CRITICAL] — **DONE (2026-08-23, PR #18)**

- **Ref**: SYS-F1
- **Problem**: No `test/` directory exists. Complex branching logic (heap guards, state machine, event orchestration) completely untested.
- **Fixed**: `DomoticsCore-System` gains a `native` environment — it was the last
  component without one — and 54 test cases in three projects:
  `test_system_config` (12: the presets the README tells users to write, and the
  `SystemState` names), `test_system_lifecycle` (24: boot, registration,
  double-`begin()`, the state callbacks, the status LED, the four console
  commands), `test_system_persistence` (18: every early return in the loaders).
- **The component set is the fixture.** Almost everything System does is
  selected by `__has_include`, so `platformio.ini` lists Core, LED,
  RemoteConsole and Wifi (the hard dependencies) plus Storage and SystemInfo,
  and deliberately omits WebUI, NTP, MQTT, OTA and HomeAssistant. Asking a
  `fullStack()` config for components that are not compiled in is itself a test:
  it must warn and boot, not fail.
- **How the console commands are reached**: `System` registers `status`, `wifi`,
  `storage` and `bootdiag` on the RemoteConsole and exposes none of them. The
  WiFi stub's simulated client — the harness the RemoteConsole suite already
  uses — drives them over a fake telnet session, which also covers
  `initBootDiagnosticsPersistence()` end to end: `bootdiag` reports
  `Boot Count: 1` and the values Storage received during `begin()`.
- **BUG-23 now has a real check.** It was closed "verified by compilation only"
  for want of this suite. `test_ready_drives_the_status_led` asserts the LED is
  named `status` and breathing after boot, which is only true if the component
  was registered and initialised by `core.begin()`.
- **Not covered**: the heap guards. The four `HAL::getFreeHeap() >= 3072`
  branches in `begin()` cannot be reached on a host that always has heap.
  Forcing them needs an injectable heap reading — a Core-level seam, filed
  nowhere yet. What is covered is the path taken when heap is plentiful, which
  is every path a healthy device takes.
- **Also**: `ERROR` is unreachable through `SystemConfig` alone. `ledPin` is
  `uint8_t`, so the LED pin validation that could fail `core.begin()` never sees
  a negative value. `WIFI_CONNECTING`, `WIFI_CONNECTED` and `SERVICES_STARTING`
  are likewise never entered — `test_boot_goes_straight_from_booting_to_ready`
  pins that, so the day one of them starts firing, a test says so.

### TEST-2 — LED: grossly inadequate coverage [CRITICAL] — **DONE (2026-08-23, PR #17)**

- **Ref**: LED-F5
- **Problem**: Only tests LED type definitions. Zero coverage for effects, PWM output, state management, WebUI interactions.
- **Fixed**: 18 test cases became 103, across four native projects —
  `test_led_types` (unchanged), `test_led_component` (lifecycle, pin validation,
  naming, every setter by index and by name, BUG-19 and DC-5 regressions),
  `test_led_effects` (the six effect curves, the rainbow ramp, the PWM
  arithmetic) and `test_led_webui` (every field the browser can post).
- **What made effects testable**: the stub HAL swallows `analogWrite()`, so a
  native test cannot observe a pin. The arithmetic was lifted out of
  `updateEffects()` into `effectBrightness()`, `rainbowColor()`, `scaleToMax()`
  and `pwmValue()` — pure, static, no member state — and the tests check the
  value computed for the pin. Behaviour is unchanged; the switch statement moved.
- **What made `LEDWebUI` testable**: `BaseWebUIComponents.h` places a CSS literal
  in `PROGMEM`, which no host toolchain defines, so the header could not be
  compiled by any native test. `Platform_HAL.h` now falls back to empty
  `PROGMEM` / identity `PSTR` — in the block that already carried the
  `DSNPRINTF_P` and `DLOG_SNPRINTF` fallbacks for the same reason. Both Arduino
  cores define the two macros before that point, so the `#ifndef` never fires on
  a board. This unblocks the same test for the other WebUI providers (TEST-6).
- **Still not covered**: `loop()` itself. Driving it needs a controllable clock
  and a recording `analogWrite()` — a Core-level test seam, not an LED one.
  What is proven is the arithmetic it applies, not the cadence at which it runs.
- **Two expectations the code corrected**: an LED is enabled by default at the
  component level, so `LEDWebUI` selecting another LED does not disable the
  first; and `getWebUIData()` answers an unknown context with `null`, not `{}`,
  because that is what ArduinoJson 7 makes of an untouched document. Both are
  recorded as tests rather than changed.

### TEST-3 — OTA: superficial coverage [HIGH] — **DONE (2026-08-27)**

- **Ref**: OTA-F6
- **Problem**: Critical gaps: no test for upload flow, SHA256 validation, state machine transitions, security enforcement, progress callbacks.
- **Most of it had already closed** without this item being touched. SEC-2 and
  SEC-7 brought the upload flow and SHA-256 validation in v2.2.0, on the host and
  on two boards. What was left was the state machine, the security enforcement of
  the size ceiling, and the events.
- **Added**: 14 native tests. Lifecycle events and their ordering on both paths
  (BUG-21, 5); the size ceiling on the upload and download paths (SEC-8, 4); and
  the state machine actually moving (5) — `Downloading` while a transfer runs,
  `Idle` versus `RebootPending` at the end depending on `autoReboot`, `Error`
  after an abort with a late chunk refused afterwards, and progress tracking the
  bytes written. The OTA native suite goes from 35 cases to 49.
- **Seven of the fourteen failed against unmodified code**, which is what says the
  suite measures the fixes rather than the platform. The other seven pin
  behaviour that was already correct; they are regression pins, and are labelled
  as such rather than counted as proof of anything.
- **Not covered, deliberately**: `EVENT_PROGRESS` emission. It is throttled on
  `millis()` (`> 1000` since the last publish), so a host test would have to
  control time to reach it, and the download path never emits it at all — only
  the upload path does. The progress *arithmetic* is covered. The dead
  `broadcastProgress()` that would have emitted it on the download path is
  recorded as DEAD-1 in `docs/components/ota/technical-reference.md` and is not
  this lot's to remove.

### TEST-4 — WiFi: superficial coverage [HIGH]

- **Ref**: WIFI-F13
- **Problem**: Key untested paths: STA fallback timer, AP mode, scan failure handling, reconnection logic.
- **One path left the list on 2026-08-29, and the item stays open.** MEM-2's
  closing lot gave `DomoticsCore-Wifi` its first board environment and its first
  device suite, `test/test_wifi_scan_esp8266`. Before it, the async scan branch
  in `WifiComponent::loop()` had **never been executed on any platform**:
  `test_wifi_component.cpp:228` calls `startScanAsync()` and never calls `loop()`,
  and the native stub returns zero networks so the loop body would not have been
  entered anyway. **Both loops now run in CI**, against a scriptable WiFi stub
  that replaces the hard `return 0` — eight cases covering exact entry text, the
  `", "` join, the ten-entry cap, the zero-network branch, both scan-failure
  codes, and that the async summary is the join of the synchronous entries. Four
  deliberately wrong rewrites were run against them and every one was caught.
  A suite that runs the same loops against a radio ships with the same lot and
  **has not been run yet**.
- **What it is not**: coverage of the rest of this list. STA fallback, AP mode
  and reconnection are all still untested, and the new device suite is a
  measurement of one loop that happens to need a board, not a WiFi behavioural
  suite. Scan failure has left the list — both `-1` and `-2` are now pinned
  natively — so what remains of this item is the other three paths.

### TEST-5 — NTP: inadequate coverage [MEDIUM]

- **Ref**: NTP-F9
- **Problem**: Missing: DST transitions, timezone edge cases, multi-server failover, wrap-around scenarios.

### TEST-6 — WebUI-related tests: zero coverage [HIGH]

- **Refs**: SI-F4, STOR-F16, LED-F14, HA-F17
- **Problem**: `SystemInfoWebUI`, `StorageWebUI`, `LEDWebUI`, and `HomeAssistantWebUI` have zero tests. WebUI providers handle user input (device name, settings, config mutations) with no validation testing.

### TEST-7 — Core: MemoryManager + ComponentConfig untested [MEDIUM]

- **Ref**: CORE-F17
- **Problem**: Zero tests for MemoryManager singleton and ComponentConfig validation logic.

### TEST-8 — OTA: nothing traverses `POST /api/ota/upload` [MEDIUM]

- **Files**: `DomoticsCore-OTA/include/DomoticsCore/OTAWebUI.h:353-443`,
  `DomoticsCore-OTA/test/test_ota_esp32/`, `DomoticsCore-OTA/test/test_ota_esp8266/`
- **Found by**: SEC-9, 2026-08-27. Filed when the lot that closed it asked why a
  220-byte defect on the primary upload path needed a human with a browser to find.
- **Problem**: every OTA test — 52 native, 8 per board — calls `beginUpload()`,
  `acceptUploadChunk()` and `finalizeUpload()` **directly**. Nothing constructs an
  HTTP request, nothing goes through `OTAWebUI`'s upload handler, and nothing has
  ever seen a `multipart/form-data` envelope. The suites therefore cover the OTA
  component and not the route a user actually posts to, which is where SEC-3's auth
  reset, SEC-7's rejected-flag handling and SEC-9's sizing all live.
- **Why it matters more than the item that raised it**: SEC-9 was not missed by
  code review. It was invisible to every automated check the repository has, and it
  surfaced only because someone joined a LAN and uploaded a file by hand. The same
  blind spot covers anything else on that handler.
- **Four specific holes**, each one SEC-9 could not close. **Three closed and the
  fourth part-closed (2026-08-27).** What remains of the fourth is the accepted
  upload and the browser's own view, both of which need a board that stays
  attached.
  1. ~~**`evenIfRemaining` below the HAL.**~~ **CLOSED.** SEC-9 pinned the argument
     `OTA.cpp` passes; the stub's `end()` ignores it, so handing `false` to the real
     Updater kept every test green. Both device suites now carry an upload that
     announces `PAYLOAD_SIZE + 220` and delivers `PAYLOAD_SIZE` — the shape every
     browser upload has. Removal check on both boards: `Update.end(evenIfRemaining)`
     → `Update.end(false)` in `Update_ESP8266.h` / `Update_ESP32.h` fails **only**
     the new tests. ESP8266 reports `No data supplied`, ESP32 `Aborted`.
  2. ~~**`installFromUrl()`'s own `end(true)`.**~~ **CLOSED on ESP8266.** A download
     that announces no size at all — which is what a chunked response gives you —
     opens the update at `UPDATE_SIZE_UNKNOWN` and is short by everything it does
     not fill. `test_download_of_unknown_length_still_stages_the_copy` covers it,
     and goes red under the same removal check. Not yet mirrored on ESP32.
  3. ~~**The download path overstates its byte count** — and understates it too.~~
     **CLOSED (2026-08-27).** Measured on a `nodemcuv2` rather than argued: an
     unknown-length download of 8192 bytes completed reporting
     **`getDownloadedBytes() == 0`**, because `finalizeUpdateOperation()` does
     `downloadedBytes = totalBytes` and `totalBytes` was the size the server
     announced — nothing, on a chunked response. The same assignment overstated
     when a server announced 64 and sent 32, and SEC-8 exists because that number
     is one a lying server picks. `installFromUrl()` now narrows `totalBytes` to
     what it counted, above `ota/end`, exactly as `finalizeUpload()` does for
     SEC-9. Two native tests, one per direction; the removal check fails only them,
     with `Expected 32 Was 0` and `Expected 32 Was 64`. The device assertion that
     had been left reading `0` on purpose now reads `PAYLOAD_SIZE`.
  4. **The HTTP path itself — partly closed (2026-08-27), and the part that is
     closed is the part that matters.** `tools/on-device/ota_upload_check.py`
     builds a `multipart/form-data` body by hand and POSTs it to
     `/api/ota/upload` on a board running `OTAWithWebUI`. Run on a `nodemcuv2`
     against builds with and without SEC-9's narrowing:

     | | announced `Content-Length` | `total` reported |
     |---|---|---|
     | with SEC-9 | 475452 | **475264** — the firmware |
     | without | 475452 | **475452** — the envelope |

     The discriminating case is the **refused** upload: a valid image with a
     deliberately wrong digest is refused after the narrowing and before the
     commit, so the device stays up and its figures stay readable. `downloaded`
     reads 475264 either way and proves nothing — a refused digest never reaches
     `finalizeUpdateOperation()`. Only `total` moves, and the script says so
     rather than claiming two assertions where there is one.

     **The accepted path now runs too** (2026-08-27): a correctly-hashed copy of
     the running image is installed and the device reboots into it. The check
     requires it to **stop** answering before it answers again — waiting only for
     a reply would pass if it had never rebooted, since it answers throughout.
     Verified by setting `autoReboot = false` and watching the check fail with
     *"the device never stopped answering"*.

     **Still open**: nothing yet measures what a *browser* renders. `progress`
     reaches 100, but the SSE cadence is ~5.4 s against an `autoReboot` that
     restarts the device 2 s after completion, so SEC-9's first recorded
     consequence may still have been right about the screen and wrong about the
     cause. The harness for that is `webui_check.py`, and it has not been pointed
     at an upload.
- **A staged eboot command outlives a serial reflash**, found while doing this and
  worth more than the item that found it. After any successful ESP8266 upload the
  bootloader is armed and acts on the *next* reset — including `esptool`'s at the
  end of a serial flash. Upload A over HTTP without rebooting, then flash B over
  serial, and the board runs **A**: eboot copied the staged image over the one just
  written. The build log says SUCCESS and it looks like a flash that did not take.
  Flashing twice works. Recorded in `tools/on-device/README.md`.
- **What closing the first two cost, and it is the finding worth keeping.** The
  ESP8266 payload was one flash sector, on the reasoning that one sector is enough
  for the Updater to fill and flush its buffer. It is not.
  `Updater.cpp:460` flushes the tail only when `_bufferLen == remaining()` — when
  the buffered bytes complete the *announced* size exactly. A 4096-byte payload
  announced as 4096 satisfies that on its very first buffer, **having never flushed
  once**, so `progress()` was 0 right up to `end()`. Every test passed because its
  announcement happened to match its delivery to the byte. Announce 4316 instead
  and the whole image stays in RAM, `progress()` stays 0, and `end()` refuses it as
  `UPDATE_ERROR_NO_DATA` — *before* reaching the flush three lines below. Real
  firmware is never one sector; a 475 KB upload flushes 115 times. The payload is
  two sectors now, and the suite behaves the way silicon does.
- **And the removal check had to be run twice.** The first one passed — 10/10 with
  `end(false)` in the HAL — because `pio` had built against the stale copy of
  `Update_ESP8266.h` in `.pio/libdeps`. The header edit was never compiled. This is
  the trap `CLAUDE.md` opens with, it was hit anyway, and it is why every figure
  above comes from a run preceded by `rm -rf .pio`.
- **The harness already exists** — `tools/on-device/` drives a real browser, and
  `OTAWithWebUI` reaches a LAN behind `DC_OTA_PREFER_STA`. What is missing is a
  test that uses them, not a means of writing one.
- **Verified on both boards** (2026-08-27). `nodemcuv2`: 10/10, up from 8.
  WROOM-32D: 9/9, up from 8 — and that board, not the ESP32-CAM, is what
  `/dev/ttyUSB0` was; its adapter is a CP2102, which `CLAUDE.md` did not list.
  52/52 native, unchanged. Each new test proved non-vacuous by the HAL removal
  check above, from a cleaned `.pio`.

---

## Priority 5: Known Bug — SSE Broadcast Warning Spam

### SSE-1 — WebUI: `DLOG_W` should be `DLOG_D` for routine broadcast [HIGH]

- **Refs**: WEB-F1, R-F2 (user-reported)
- **File**: `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h:939`
- **Problem**: Every ~5.4 seconds, the RemoteConsole is flooded with:
  ```
  [W][WEB] SSE broadcast: 1561 bytes, clients=1
  ```
  This is routine operational behavior logged at WARNING level, making real warnings invisible.
- **Fix**: Change `DLOG_W` to `DLOG_D` on line 939. Keep WARNING only for the heap-low skip case (line 933).
- **Effort**: One-line fix.

---

## Priority 6: File Size Violations (Constitution VII — 800 lines max)

### SIZE-1 — WebUI.h (950 lines) [HIGH]

- **Refs**: WEB-F2, R-F1
- **Fix**: Extract route setup + SSE broadcasting into `WebUIRoutes.h` / `BroadcastManager.h`.

### SIZE-2 — StreamingContextSerializer.h (921 lines) [HIGH]

- **Ref**: WEB-F3
- **Fix**: Unify duplicated `writeJsonString` overloads (const char* vs String&), extract field serialization helpers.

### SIZE-3 — Wifi.h (880 lines) [MEDIUM]

- **Refs**: WIFI-F1, R-F11
- **Fix**: Extract AP-mode logic or WebUI provider code into separate header.

### SIZE-4 — test_mqtt_component.cpp (899 lines) [MEDIUM]

- **Ref**: MQTT-F1
- **Fix**: Split test file by feature area (connection, publish, subscribe, queue, WebUI).

### SIZE-5 — test_ha_component.cpp (989 lines) [MEDIUM]

- **Ref**: HA-F10
- **Fix**: Split into `test_ha_config.cpp`, `test_ha_entity_management.cpp`, `test_ha_switch_integration.cpp`, `test_ha_publish_overloads.cpp`.

### SIZE-6 — test_webui_component.cpp (2520 lines) [LOW]

- **Ref**: WEB-F16
- **Fix**: Split into config tests, field tests, context tests, registry tests, serializer tests (3x over limit).

---

## Priority 7: Architecture / God Objects (Constitution I, XIII)

### ARCH-1 — System: God Object with 6+ responsibilities [HIGH]

- **Ref**: SYS-F6
- **Problem**: Orchestrates 10 components, handles registration, persistence, events, state, console commands, boot diagnostics.
- **Fix**: Extract `SystemComponentRegistrar`, `SystemEventOrchestrator`, `SystemConsoleCommands`.

### ARCH-2 — LED: God Object [HIGH]

- **Ref**: LED-F1
- **Problem**: Combines config management, hardware pin control, PWM, effect calculations, state management, WebUI in one class.
- **Fix**: Extract effect engine and WebUI into separate classes.

### ARCH-3 — System: `__has_include` count comment wrong [MEDIUM] — **DONE (2026-08-23, PR #18)**

- **Ref**: SYS-F5
- **Problem**: Comment says "20 directives" but actual count is 54 across 3 files.
- **Fixed**: the comment gives the breakdown — 22 in `System.h`, 16 in
  `SystemPersistence.h`, 16 in `SystemWebUISetup.h` — rather than one number
  that drifts silently. Corrected alongside TEST-1 because writing the suite
  meant counting them anyway: the count is the measure of how much of this
  component exists only in some builds.

---

## Priority 8: CI / Infrastructure

### CI-1 — No unit test CI workflow [HIGH] — **DONE (2026-08-22, PR #2)**

- **Ref**: R-F4
- **Problem**: GitHub Actions never runs native unit tests. Only compile test for ESP32.
- **Fixed**: `.github/workflows/ci.yml`, job `native-tests`. Projects are discovered
  from the tracked `platformio.ini` files rather than listed, which also picked up
  `DomoticsCore-WebUI/test/test_streaming_serializer` — a native project nested a
  level below the components, that nothing had ever run. An empty discovery is a
  failure, so the job cannot pass by testing nothing.
- **What it caught on first run**: three suites asserting their own component version
  as a stale literal, and one heap-stability test measuring the allocator rather than
  the code (it charged the first command's one-off cost to the loop, which passed on
  glibc 2.43 and failed at 384 bytes on the runner). Both fixed in the same PR.

### CI-2 — ESP8266 not tested in CI [HIGH] — **DONE (2026-08-22, PR #2)**

- **Ref**: R-F5
- **Problem**: Only ESP32 compilation tested. ESP8266 regressions go undetected.
- **Fixed**: `.github/workflows/ci.yml`, job `build-targets` — a matrix over
  `esp32dev`, `esp8266dev` and `esp32c3` building the FullStack example, the only one
  of the 29 examples pulling all twelve components and therefore the only one whose
  ESP8266 build reaches all ten ESP8266-specific headers.
- **Result**: the ten files compile clean. The platform the library had been promising
  without ever checking was sound — there was nothing to repair, only something to
  prove. `esp8266dev` RAM 61.7% / Flash 68.1%.
- **Since CI-8**: the install-from-GitHub witness builds both declared platforms too.

### CI-3 — Missing `DomoticsCore-Core` dependency in 3 library.json [HIGH] — **DONE (2026-08-22, PR #6)**

- **Ref**: R-F3
- **Files**: `DomoticsCore-Storage/library.json`, `DomoticsCore-SystemInfo/library.json`, `DomoticsCore-OTA/library.json`
- **Problem**: These include Core headers but don't declare the dependency.
- **Fixed**: `{ "name": "DomoticsCore-Core", "version": ">=1.4.0" }` added to each.
  In-tree builds never noticed, because every example lists the components
  explicitly as `file://` paths. Only someone installing a single component from
  the registry would have hit it — which is the case the manifest exists for.

### CI-4 — Wifi vs WiFi naming inconsistency [MEDIUM]

- **Ref**: R-F6
- **Problem**: Library name is `DomoticsCore-Wifi` but docs use `DomoticsCore-WiFi`.
- **Fix**: Standardize all documentation to match `library.json` name.

### CI-5 — Outdated GitHub Actions versions [MEDIUM] — **DONE (2026-08-22, PR #6)**

- **Ref**: R-F7
- **Fixed**: `version-check.yml` moved to `actions/checkout@v4` and
  `actions/setup-python@v5`. `test-github-install.yml` had already been updated
  and was left untouched — it carries the fork workaround for `pull_request`
  events, and rewriting it wholesale is how that gets lost.

### CI-8 — Root `library.json` pulls `AsyncTCP` unconditionally [HIGH] — **DONE (2026-08-23, PR #14)**

- **Problem**: the root manifest declares `ESP32Async/AsyncTCP` as a plain
  dependency. That package is ESP32-only; ESP8266 needs `ESP32Async/ESPAsyncTCP`.
  Every in-tree example works around it by listing the right one and adding
  `lib_ignore`, so nothing in this repository trips over it — but anyone
  installing the library from GitHub or the registry for an ESP8266 target does.
  It is also what keeps `test-github-install.yml` pinned to `esp32dev`: the
  witness project cannot be built for ESP8266 while the manifest resolves this
  way.
- **Fix as filed**: make the two TCP dependencies conditional on `platforms`.
  **That turned out to be the wrong fix.** `ESPAsyncWebServer` already declares
  them conditionally and correctly — `AsyncTCP` for `espressif32`/`libretiny`,
  `ESPAsyncTCP` for `espressif8266`, `RPAsyncTCP` for `raspberrypi`. The root
  entry was therefore redundant on ESP32 and harmful on ESP8266. Declaring a
  conditional pair here would have duplicated a correct declaration and pinned
  versions that can drift from what the web server wants.
- **Fixed**: the `AsyncTCP` entry is removed from the root manifest; the
  transitive dependency resolves the right backend per platform. The witness
  project now builds both declared platforms.
- **Proven, not assumed**: a witness resolving through the manifest fails on the
  previous root `library.json` with `AsyncTCP.h:22:10: fatal error: sdkconfig.h:
  No such file or directory` — an ESP-IDF header absent on ESP8266 — and builds
  clean once the entry is gone. ESP8266: 67.4% flash, 56.2% RAM.
- **Note**: filed 2026-08-22. Earlier revisions of this file pointed at CI-3 for
  this problem — that was wrong. CI-3 is about a missing `DomoticsCore-Core`
  dependency and has nothing to do with TCP backends.

### CI-10 — Nothing built the on-device test projects [HIGH] — **DONE (2026-08-23, PR #19)**

- **Filed and fixed together**, after a board was plugged in for the first time
  and none of the suites meant for it would run.
- **Problem**: four projects carry ESP8266 test suites. No workflow built any of
  them, and the native environments exclude them by `test_ignore` or
  `test_filter`. Three of the four had rotted where nobody could see it:
  Storage called a `setNamespace()` that has never existed in any commit — added
  2026-01-04 with the message "build issues to fix later"; WebUI passed `String`
  to `withCustomHtml(const char*)` after that API split into static and
  `Dynamic` overloads; and `test_schema_memory` had its suite outside `test/`
  and an `int main()` where the Arduino core supplies one, so it linked against
  nothing.
- **Worse than not compiling**: the Storage suite also constructed a bare
  `StorageComponent` and never opened it. Every put and get returns at
  `if (!isOpen)` without allocating, so had it compiled, three leak tests would
  have measured rejected calls and passed for the wrong reason.
- **Fixed**: the three suites repaired, and a `build-device-tests` job compiles
  all four on every push. It cannot run them — no runner has a board — but a
  suite that builds cannot rot silently for seven months. *(A fifth joined on
  2026-08-26: `DomoticsCore-OTA`, with the SEC-2 re-fix.)*
- **Every test now asserts it is measuring something** before it measures:
  a `putString()` must return true or the test fails with "Storage did not open
  — the rest would measure nothing".
- **What the first real run found**: STOR-ESP-1 — which turned out to be the
  suite measuring an undrained EventBus rather than Storage, and was withdrawn
  on 2026-08-25. The liveness assertions above were the right instinct applied
  one level too shallow: they proved an operation happened, not that the thing
  being measured was the thing under test. The WebUI suites pass, all nine cases.

### CI-14 — CI reports `FullStack` as green on ESP8266, where it cannot work [MEDIUM]

- **Filed**: 2026-08-27, from the real-conditions campaign. Measured on a
  `nodemcuv2`, and discriminated against a WROOM-32D.
- **Problem**: `Build esp8266dev` compiles `FullStack` and passes, and the "what
  CI proves" table in this file and in CLAUDE.md both list that as a ✅. On a real
  ESP8266 the example **boots and then cannot join a network**:

  ```
  Memory profile: MINIMAL (heap: 15088 bytes)
  Memory profile MINIMAL: WS clients 3 -> 2
  Polling mode (heap=14120 < 20000)          <- SSE disables itself
  Discovery done: 1 providers registered
  WebUI loop alive, heap=1856
  [W] [WIFI] Deferring WiFi connect: heap too low (1856 bytes, need 2500+)
  ```

  All twelve components register, the heap settles at **1 856 bytes**, and the
  WiFi guard needs 2 500. The device retries forever and never connects.
- **Not a crash, and the framework behaves well**: it detects the minimal
  profile, degrades the WebUI to two WS clients and polling, and declines to
  attempt a connection it cannot fund rather than panicking. The failure is
  legible and contained. It is still a device that does nothing.
- **The discriminator**: the same `FullStack`, same commit, on a WROOM-32D runs
  end to end — profile FULL (278 KB), WiFi up, MQTT connected, HA availability
  published, WebUI serving 200 with SSE enabled. This is a platform limit, not a
  defect.
- **Why CI cannot see it**: a host build proves compilation, not behaviour — the
  standing lesson of BUG-4, applied to a configuration rather than to a bug.
  `esp8266dev` is a declared target of the example, so the build job is doing
  exactly what it was asked.
- **Fix, one of**: document `FullStack` as ESP32-class and drop `esp8266dev` from
  its `platformio.ini`; or keep the target and make the shortfall loud — a boot
  banner naming the profile and what it disabled, rather than a `[W]` line inside
  a retry loop. Whichever is chosen, the ✅ row in both "what CI proves" tables
  needs a caveat: it proves the ESP8266 *build*, not an ESP8266 *device*.
- **Related**: `docs/components/webui/project-context.md:126` already records
  that ESP8266 free heap "can be as low as 2-3 KB during AP+STA mode". This is
  that note, measured, with the consequence attached.

### DOC-1 — two examples advertise addresses and endpoints that do not exist [LOW]

- **Filed**: 2026-08-27, same campaign.
- **`WebUIOnly` prints the wrong address.**
  `DomoticsCore-WebUI/examples/WebUIOnly/src/main.cpp:249` logs
  `WebUI available at: http://192.168.4.1` as a **hard-coded string**. The
  example's own logic connects to a station when credentials are present — it
  did, and served on `192.168.1.224` — so anyone following the log goes to the
  access-point address the device is not using. Use `HAL::WiFiHAL::getLocalIP()`
  when the station is up, as the same file already does at line 191.
- **The `webui_uptime` context advertises a dead endpoint.** `/api/ui/schema`
  declares `"apiEndpoint":"/api/webui/uptime"` for it; that path returns **404**
  on both an ESP8266 and an ESP32. The data reaches the browser over SSE and the
  polling endpoint, so nothing is broken today — but a schema is a contract, and
  anyone writing a client against it will follow the advertised path. Either
  serve it or stop declaring it.

### CI-13 — `clean_examples.py` does not know about the `test/` projects [MEDIUM]

- **Filed**: 2026-08-26, after a FullStack cross-compile recursed to 4.3 GB and
  was killed. Cleaning every `.pio` in the tree recovered 6.8 GB.
- **File**: `clean_examples.py:68-87`
- **Problem**: the script exists precisely to stop recursive `.pio` nesting, and
  every example wires it as `extra_scripts = pre:../../../clean_examples.py`. It
  enumerates two fixed depths — `DomoticsCore-*/.pio` and
  `DomoticsCore-*/examples/*/.pio` — and never looks under `test/`. But
  `DomoticsCore-WebUI/test/test_schema_memory` and
  `.../test_streaming_serializer` are **PlatformIO projects in their own right**,
  each generating a `.pio` the script cannot see. When PlatformIO resolves
  `file://../../../DomoticsCore-WebUI`, that unseen `.pio` is copied in with the
  source, and its own `libdeps` carry more copies. The header comment already
  records having observed 12,000+ nested directories from this mechanism.
- **Why the fixed depths are right**: the script forbids `rglob('.pio')` on
  purpose — it would match `.pio` directories inside the *current* project's own
  `libdeps`, and deleting those corrupts `.sconsign312`. The design is sound; the
  enumeration is simply incomplete.
- **Fix**: add a third fixed-depth pass over `DomoticsCore-*/test/*/.pio`,
  keeping the `_safe_to_clean` guard and without introducing `rglob`. Discovering
  the projects from tracked `platformio.ini` files, as the CI job does, would not
  drift the way a hard-coded list does.
- **The irony worth recording**: `test_streaming_serializer` is the project CI-1's
  discovery-based job found — "a native project nested a level below the
  components, that nothing had ever run". It was taught to the test runner and
  never to the cleanup.

### CI-11 — HomeAssistant declares an ESP8266 test env with no ESP8266 tests [MEDIUM]

- **Filed**: 2026-08-23, while writing the CI-10 job.
- **File**: `DomoticsCore-HomeAssistant/platformio.ini`
- **Problem**: the `esp8266dev` environment sets `test_framework` and
  `test_build_src` but carries no ESP8266 suite and no `test_filter`, so
  `pio test -e esp8266dev` cross-compiles the component's **native** suites for
  the board. They include `Platform_Stub.h` explicitly, which collides with the
  real platform header: `redefinition of 'class String'`, and a dozen more.
- **Why it matters now**: it is the reason `build-device-tests` lists its
  projects instead of discovering them — four when this was filed, six since
  2026-08-28, seven since 2026-08-29 when `DomoticsCore-Wifi` joined. "Declares
  `esp8266dev` and has a `test/` directory" is otherwise the right rule, and it
  matches this one too.
- **Fix**: decide what that environment is for. Either give it a `test_filter`
  naming a real ESP8266 suite, or drop the test settings and leave it a build
  environment. Then the CI job can discover rather than list.
- **Half supplied on 2026-08-28, and the item stays open.** MEM-2's hot half gave
  HomeAssistant its first ESP8266 suite —
  `test/test_ha_heap_esp8266`, the in-dispatch heap measurement — and with it the
  `test_filter` this entry asked for, plus `test_ignore` in `[env:native]` and
  `[env:esp32dev]` so neither of those builds it. `pio test -e esp8266dev` now
  compiles one suite instead of colliding with `Platform_Stub.h`, and
  `DomoticsCore-HomeAssistant` has joined the `build-device-tests` list.
- **What is still missing, and it is the part the entry is about**: one suite is
  not ESP8266 coverage of HomeAssistant. Nothing on a board exercises discovery,
  availability, the alarm panel or the entity types; the new suite measures the
  command parse and asserts one accepted command routes. And the CI job still
  lists its projects rather than discovering them — see the note in `ci.yml`.
- **The same defect exists on `[env:esp32dev]`, unfixed.** It also declares
  `test_framework` and `test_build_src` with no ESP32 suite, so
  `pio test -e esp32dev` cross-compiles `test_ha_events` and `test_ha_entity` for
  the board and dies on the same `redefinition of 'class String'` — 41 errors,
  verified at `bea43842` as well as after. No workflow runs that command, so it
  breaks nothing today; it is recorded so the next person does not read it as a
  regression.

### CI-12 — Every commit on a branch with an open PR ran the suite twice [MEDIUM] — **DONE (2026-08-26)**

- **Filed**: 2026-08-25, and fixed the day after, once two pull requests
  demonstrated it.
- **Files**: `ci.yml`, `test-github-install.yml` — both were `on: [push,
  pull_request]`.
- **Problem**: once a branch lives here and carries an open PR, each commit
  fires both events. The concurrency group is keyed on `github.ref`, which is
  `refs/heads/<branch>` for the push and `refs/pull/N/merge` for the PR, so
  neither run cancels the other. Two full matrices — native suites, three
  cross-compilations, and the on-device builds — for one commit.
- **Observed**: PRs #20 and #21 each ran fourteen jobs for seven required
  checks. `gh pr checks` reported `pass,pending` on three of them while the
  duplicate run caught up. `check-versions` was the exception, appearing once —
  because `version-check.yml` already scoped both its triggers to branches, and
  that is what made the mechanism visible.
- **Fixed**: `push` scoped to `main`, `pull_request` left bare. Branch commits
  are now covered once, by the PR event; `main` stays covered after every merge.
  A branch with no open PR gets no run, which is the intended trade — nothing
  lands here except through a pull request.
- **Why the pull_request half is the one to keep**: `test-github-install.yml`
  installs from `pull_request.head.repo` at `head.sha`, falling back to
  `GITHUB_SHA` off-PR. That path is what fork contributions exercise, and it is
  the half that would have been lost by scoping the other way.

### CI-6 — Missing `depends` in `library.properties` [MEDIUM]

- **Ref**: R-F8
- **Fix**: Add `depends=ArduinoJson,ESPAsyncWebServer,AsyncTCP,PubSubClient`.

### CI-9 — FullStack partition table wastes 384 KB per app slot [MEDIUM] — **DONE (2026-08-23, PR #11)**

- **Problem**: the example carries its own `partitions.csv` — `app0`/`app1` at
  `0x180000` each, plus a 960 KB `spiffs` partition — so the firmware ceiling is
  1,572,864 bytes. Measured on `main`, 2026-08-23, `esp32c3`: 1,302,694 bytes
  used, 82.8%, 270,170 free. That `spiffs` partition is dead weight in the
  default configuration: `embed_webui.py` gzips the WebUI into the firmware,
  `WebUIConfig::useFileSystem` defaults to `false`, `Storage` uses NVS on ESP32,
  and the only filesystem call sites in the whole tree are the two lines of the
  opt-in static-file fallback in `WebServerManager.h:139-140`. Nothing ever
  writes there.
- **Why it matters now**: the `esp32-ethernet` branch adds a Network component
  *and* moves to Arduino-ESP32 3.x, whose binaries are larger than 2.0.17's;
  FullStack no longer fits an ESP32-C3 4 MB part there. The ceiling is the
  example's own, not OTA's — OTA needs two app slots of equal size and does not
  dictate their size.
- **Fix**: switch the example to the stock `min_spiffs.csv` (app slots
  `0x1E0000` = 1,966,080 bytes, 128 KB spiffs, 64 KB coredump) or an equivalent
  local table. The same firmware then sits at 66.3%. Note that
  `docs/getting-started.md:68` already recommends `min_spiffs.csv` to users, so
  the example currently contradicts the documentation it ships with.
- **Note**: filed 2026-08-23, after marianorenzi reported that FullStack plus
  his Network component overflows an ESP32-C3. Answering him, we said we would
  make this change on `main` so his branch inherits it.
- **Fixed**: the example's local `partitions.csv` is deleted and both ESP32
  environments use the stock `min_spiffs.csv`. Measured, same firmware, same
  commit:

  | Target | Firmware | Before | After | Headroom |
  |---|---|---|---|---|
  | `esp32dev` | 1,333,617 B | 84.8% | **67.8%** | 269,247 → **632,463 B** |
  | `esp32c3` | 1,303,326 B | 82.9% | **66.3%** | 269,538 → **662,754 B** |

  Not a byte of firmware changed — only the ceiling. `esp8266dev` is unaffected
  (68.4%, no partition table on that platform). `DomoticsCore-OTA/examples/`
  `OTAWithWebUI` was already on `min_spiffs.csv`: FullStack was the only example
  in the tree carrying its own table, and the only one contradicting
  `docs/getting-started.md`.

### CI-7 — `local_ci.sh` counts total lines, not code lines [LOW]

- **Ref**: R-F10
- **Problem**: Constitution says exclude blanks/comments, but tool uses `wc -l`.
- **Fix**: Use `grep -cvE '^\s*(//.*)?$'` or update constitution to match tool behavior.

---

## Priority 9: Dead Code / YAGNI (Constitution IV)

| ID | Component | Dead Item | Action |
|----|-----------|-----------|--------|
| DC-1 | SystemInfo | `enableDetailedInfo`, `enableMemoryInfo` config flags — never enforced | Implement or remove |
| DC-2 | Storage | `StorageConfig::readOnly` — accepted but never checked by put/remove/clear | Implement or remove |
| DC-3 | HA | Residual `static_cast` in handleCommand despite virtual dispatch (R24) | Complete migration to virtual dispatch |
| DC-3b | HA | `volatile bool publishing` set/cleared but never read — dead code | Remove or implement guard (see BUG-11) |
| DC-4 | LED | `effectDirection` — confirmed dead, may have been missed in v1 cleanup | Remove if still present |
| DC-5 | LED | `shutdown()` doesn't clear internal vectors | **DONE** (PR #17) — `ledStates` cleared and shrunk; `ledConfigs` deliberately kept, see below |
| DC-6 | OTA | `EVENT_COMPLETE` vs `EVENT_COMPLETED` — confusing duplicate names | **DONE** (PR #4) — consolidated on `EVENT_COMPLETED` |
| DC-7 | OTA | `signaturePublicKey` — documented but never used | **DONE** (PR #4, docs PR #9) — removed with the five other unread fields |
| DC-8 | WebUI | Pointless `doc.shrinkToFit()` after serialization is complete | Remove |
| DC-9 | MQTT | `topicMatches()` allocates 2 vectors per call — use char* parsing | Refactor to zero-alloc |
| DC-10 | WebUI | `const_cast` in `onComponentsReady` — change API to accept non-const ref | Fix signature |
| DC-11 | WebUI | `WebUIField::configure()` and `WebUIContext::configure()` allocate a `JsonDocument` per call — no caller in the tree, and nothing serializes it | **DONE** (2026-08-26) — deleted, see below |
| DC-12 | WebUI | `presentation` is serialized on every context (`StreamingContextSerializer.h:233`, `WebUI.h:825`) and `app.js` never reads it | Honour it in the frontend — frontend work, sequenced behind marianorenzi; see below |
| PERSIST-1 | System | `loadWifiConfig()` AP-SSID generation appears unreachable | Investigate, then remove or move — see below |

### DC-11 and DC-12 — the WebUI extension channel, decided [MEDIUM] — **DC-11 done, DC-12 open**

- **Filed**: 2026-08-23. Kept together because they are the same hole seen from
  both ends: the C++ side can attach arbitrary JSON to a field or a context and
  never sends it, while the wire carries a presentation hint the browser
  ignores. `WebUIPresentation::Table` is one of the values it ignores.
- **Decision (2026-08-26): explicit schema keys, not a generic JSON blob.** The
  question was whether `configure()` becomes the extension channel — serialize
  it, and column definitions, status severity maps and chart options all travel
  the same way — or whether the schema grows explicit keys per field type and
  `configure()` goes. It goes. A generic blob costs an unbounded `JsonDocument`
  per field on a platform with 80 KB of RAM, and every field or context copy
  deep-copies it (Constitution IV and XIV). Explicit keys cost what they carry
  and no more.
- **DC-11 is closed by that decision.** Both `configure()` methods, the
  `WebUIField::config` and `WebUIContext::contextConfig` members, and the four
  copy-constructor / copy-assignment branches that deep-copied them are removed
  from `IWebUIProvider.h`. The sweep before deleting found zero call sites in
  production, examples, tests or docs code, and no serializer read either
  member — so the emitted JSON is byte-identical. **This lot removed the generic
  half only; it did not design the explicit half.**
- **What it actually saved, stated honestly.** Because nothing ever called
  `configure()`, both pointers were always null: no `JsonDocument` was ever
  allocated in shipped firmware, and there was no leak. The realised saving is
  the pointer itself — one per field and one per context, 4 bytes each on a
  32-bit target (measured on the host: `sizeof(WebUIField)` 368 → 360,
  `sizeof(WebUIContext)` 400 → 392) — plus four dead copy branches. The larger
  point is the one the decision settles: the cost the design *would* have had
  once used, not a cost it was already imposing.
- **This is a public API removal, and the sweep that justified it was too
  narrow.** "No caller in this repository" is not the same as "no caller", for a
  library published on the PlatformIO Registry that people install by version.
  Known downstream consumers were checked before landing and none calls it — but
  that check cannot be exhaustive, and its result is not what makes the change
  safe to ship quietly. **Record it as breaking for the release that ships this
  series**: removing a public method from a public struct requires a MAJOR bump,
  whether or not a caller can be enumerated today.
- **DC-12 stays open, and its fix is frontend work.** The backend already sends
  `presentation` correctly; the defect is that `app.js` ignores it. That is a
  change to `webui_src/app.js`, sequenced **behind marianorenzi's in-flight
  change to that same file** — he is writing a `Table` field for the provider
  status report (Provider / IP / Status), with the columns in the schema and
  complete rows in the value. Do not touch `app.js` ahead of him.
- **Constraint carried forward for whoever adds the explicit keys**:
  `WebUIField::value` is a `String`, so table rows would arrive as JSON inside a
  string unless the serializer learns to emit a raw array for that field type.
  See **BUG-28** — a dynamic schema key must hold its serialized text in a
  serializer member, never in a local, or the chunked writer will dangle.

### PERSIST-1 — System: the device-name AP SSID is never generated [MEDIUM]

- **Filed**: 2026-08-23, from writing the TEST-1 suite (PR #18).
- **File**: `SystemPersistence.h:189-192`
- **Problem**: `loadWifiConfig()` builds an AP SSID from `config.deviceName`
  when the stored one is empty. The condition looks reachable and is not:
  `System::begin()` runs `core.begin()` *before* `loadAllConfigs()`, and by then
  `WifiComponent::begin()` has already fallen back to AP mode and named itself
  `"DomoticsCore-" + <last 6 of MAC>` — a literal, not the device name. The SSID
  the loader inspects is therefore never empty.
- **The two ways in are mutually exclusive.** With `config.wifiSSID` empty, WiFi
  always ends `begin()` holding some AP SSID; with it set, `loadWifiConfig()`
  returns at its first guard. No configuration reaches the branch.
- **Consequence, if confirmed**: dead code, and a user who names their device
  still gets an access point called `DomoticsCore-XXXXXX`. Note that the *normal*
  path is unaffected — `System::registerWifiComponent()` sets a device-name-based
  AP SSID before `core.begin()`, and `Wifi.h:130` honours a pre-set one.
- **Not fixed here.** The fix belongs in the WiFi layer, not in a test PR, and
  `Wifi.h` is being rewritten on `esp32-ethernet`. Confirm against that branch
  before touching it.
- **Pinned meanwhile**: `test_an_absent_ap_ssid_keeps_the_one_the_component_already_has`
  asserts the behaviour as it is, not as the branch intends, so whoever settles
  this sees the test change with it.

### DC-13 — Three public helpers with no non-test caller here, and users are told to call them [MEDIUM] — **NEW (2026-08-29)**

- **Opened by**: MEM-2's closing lot, which moved two of its rows here.
- **Refs**: LED-F3, WEB-F7 (inherited from MEM-2's LED and WebUI rows).
- **The heading is precise on purpose.** An earlier draft said "no code in this
  repository calls", which the entry's own body then contradicts three lines
  later: `getLEDStatus` has twenty-odd calls across three test files. The claim
  that survives is **no non-test caller** — no component, no example, no
  library code.
- **Files**: `DomoticsCore-LED/include/DomoticsCore/LED.h:337-354`
  (`getLEDStatus`), `DomoticsCore-WebUI/include/DomoticsCore/BaseWebUIComponents.h:285-303`
  (`selectDropdown`) and `:358-380` (`radioGroup`).
- **Why they were MEM-2 rows**: each builds its result by `String` concatenation
  in a loop — `getLEDStatus` produces 75-85 characters, `selectDropdown` ~310,
  `radioGroup` ~840 for four options.
- **"No caller anywhere" was the wrong claim and the wrong test.** Wrong claim:
  `getLEDStatus` is called in **three** test files, not the one the draft named —
  `test_led_component.cpp`, `test_led_webui.cpp` and `test_system_lifecycle.cpp`
  — and it is the only window those suites have onto LED state, so deleting it
  guts them. Wrong test: this library is installed by version from the
  PlatformIO registry as `jn0v/DomoticsCore`, so its callers are **other people's
  sketches**. `DomoticsCore-WebUI/README.md:103,110` hands users copy-paste calls
  to exactly the two WebUI helpers, and all three signatures are documented —
  `docs/components/webui/technical-reference.md:341,345` and
  `docs/components/led/technical-reference.md:216`.
- **The finding is therefore**: unused within this repository, documented as
  public. Not a memory defect. Nothing here runs in a loop or on a timer, so
  optimising them buys a published library nothing measurable.
- **The remedy is a release decision, and it is constrained**: keep and fix, keep
  and document as unmeasured, or deprecate — and deprecating or deleting a
  documented public method at a patch or minor version breaks downstream
  sketches, so it belongs on a major boundary. The two WebUI helpers have no
  caller at all in the tree; `getLEDStatus` has three test files depending on it
  and would need a replacement first. That difference is what makes them one
  entry with two answers rather than one.

**DC-5, on keeping `ledConfigs`.** "Clear the internal vectors" was filed as one
action; it is two. `ledStates` is runtime churn and is released, per Constitution
XIV. `ledConfigs` is the user's registration — the pins, names and brightness
ceilings passed to `addSingleLED()`. `ComponentRegistry::shutdownAll()` leaves a
component able to be initialised again, and clearing the configuration would have
left that second `begin()` driving nothing at all, silently. The vector that
holds user input outlives the shutdown; the vector that holds machine state does
not.

---

## Priority 10: Minor Issues (LOW)

| ID | Component | Issue |
|----|-----------|-------|
| LO-1 | Multiple | Hardcoded fallback versions in `getWebUIVersion()` drift from actual version |
| LO-2 | MQTT | `MQTTPublishEvent` is 832 bytes — consider reducing field sizes |
| LO-3 | Core | `__dc_*` field names use reserved double-underscore prefix |
| LO-4 | Core | `nextId` overflow after 4B subscribe/unsubscribe cycles (theoretical) |
| LO-5 | Core | Backpressure silently drops oldest event with no logging |
| LO-6 | NTP | `TIMEZONE_LOOKUP` static constexpr duplicated per TU |
| LO-7 | NTP | `setSyncInterval()` is no-op on ESP8266 — not documented |
| LO-8 | RC | `nextClientId` wraps after 4B connections |
| LO-9 | WiFi | Member shadowing (`ssid`/`password` vs constructor params) |
| LO-10 | WiFi | Magic numbers for WiFi status codes |
| LO-11 | LED | French comment in test file ("Tests unitaires") — **DONE** (PR #17) |
| LO-12 | System | French comment in FullStack test |
| LO-13 | SI | Unicode emoji in log message (non-ASCII) |
| LO-14 | SI | `calculateCpuLoad()` uses unreliable heap-churn heuristic |
| LO-15 | MQTT | Unicode checkmark/cross in log messages |
| LO-16 | CHANGELOG | Missing version reference links |
| LO-17 | Storage | `nameStr_` backing String — move-safety risk |
| LO-18 | HA | `shutdown()` emits MQTT events without checking `mqttConnected` — wasted EventBus traffic |
| LO-19 | HA | EventBus subscriptions from `begin()` never unsubscribed in `shutdown()` — restart causes duplicates |
| LO-20 | HA | `HASwitch`/`HABinarySensor` payloadOn/Off are `String` holding "ON"/"OFF" — should be `const char*` |
| LO-21 | HA | `HAAlarmControlPanel::code` is `String` while sibling fields are `char[]` — inconsistent |
| LO-22 | HA | `HAButton::buildDiscoveryPayload` duplicates base class logic instead of calling super |
| LO-23 | HA | `HALight.h`, `HAButton.h`, `HAAlarmControlPanel.h` rely on transitive `Logger.h` include |
| LO-24 | WebUI | `volatile int pollingClients` — use `std::atomic<int>` or remove `volatile` |
| LO-25 | WebUI | Admitted tech debt: duplicated `serializeContext()` with "until I move it" comment |
| LO-26 | WebUI | Magic string `"wifi/ap/enabled"` instead of centralized event constant |
| LO-27 | WebUI | Zero tests for WebSocketHandler, route handlers, auth flow, SSE broadcast |
| LO-28 | Root | CHANGELOG references `docs/migration/` which doesn't exist |
| LO-29 | LED | `library.json` uses `^1.3.0` dep constraint while all others use `>=` — will break at Core 2.0 |
| LO-30 | Root | Root `examples/` only has README — Arduino Library Manager shows 0 examples |
| LO-31 | Root | `check_versions.py` doesn't enforce Constitution XV propagation rules |
| LO-32 | WebUI | `wsBuffer_[]` static member defined in header — ODR violation risk in multi-TU builds |

---

## Resolved in v2.0.1 (2026-03-11) — 20 items

| Item | Severity | Fix |
|------|----------|-----|
| **SEC-2** | CRITICAL | `abort()` + error event on SHA256 mismatch after OTA download — **the commit landed, the rollback did not work.** `abort()` after a successful `end(true)` is inert on both cores; properly fixed 2026-08-26 by verifying before committing |
| **MEM-1** | HIGH | `shrink_to_fit()` in ComponentRegistry, EventBus, IWebUIProvider, Wifi; bounded System stateCallbacks (max 8); Wifi reserve(n) |
| **BUG-1** | HIGH | `static_assert(is_trivially_copyable)` on EventBus publish |
| **BUG-3** | MEDIUM | LoggerCallbacks ID-based add/remove (was clearing all) |
| **BUG-7** | HIGH | `inline` on MQTTComponent::instance |
| **BUG-9** | HIGH | QoS >2 clamping in MQTT publish/subscribe/lwtQoS |
| **BUG-10** | HIGH | `mqttPublish()` → void (always returned true) |
| **BUG-11** | HIGH | Removed dead `volatile bool publishing` from HA |
| **BUG-12** | HIGH | `inline constexpr` on HAEvents topic strings |
| **BUG-14** | HIGH | Null guard on Storage::putBlob() |
| **BUG-18** | MEDIUM | LED effectPhase wrapping with fmod() |
| **BUG-20** | HIGH | `inline` on OTA static variables (ESP32/ESP8266/Stub) |
| **SSE-1** | HIGH | SSE broadcast log level WARN → DEBUG |
| **DC-3b** | MEDIUM | (same as BUG-11) |
| **DC-4** | MEDIUM | Already removed in v1 — confirmed absent |
| **DC-8** | MEDIUM | Removed pointless doc.shrinkToFit() (WebUI, MQTTWebUI) |

**Commit**: `e081940` — 580 unit tests pass across 11 components.

---

## Tracking Summary

| Priority | Items | Constitution | Remaining |
|----------|-------|-------------|-----------|
| 1. Security | SEC-1 to SEC-14 | OTA, Remote, WebUI | 0C, 0H, 6M (**SEC-1, SEC-3, SEC-7, SEC-8, SEC-9 done; SEC-2 done twice** — the v2.0.1 fix was inert, re-fixed 2026-08-26; **SEC-9 fixed 2026-08-27 and downgraded MEDIUM → LOW**, two of its three recorded consequences refuted against the Arduino cores; **SEC-10 CRITICAL and SEC-11 HIGH filed and fixed 2026-08-29** — a per-boot CSRF token, board-measured both directions; **SEC-12/SEC-13/SEC-14 MEDIUM filed and open** — SEC-12 re-argued HIGH → MEDIUM by parity with SEC-7; **SEC-5 re-pointed** onto the cross-origin axis SEC-10 measured, its history-leak point kept) |
| 2. Memory Safety | MEM-1 to MEM-6, STOR-ESP-1 | XIV (ABSOLUTE) | 0C, **0H**, 4M (**MEM-1 done; STOR-ESP-1 withdrawn** — the suite measured an undrained EventBus; **MEM-2 closed 2026-08-29** across both halves — three rows fixed, one one-line change, four refuted, one re-pointed, two moved out, and the 14-character threshold the whole finding was reasoned against corrected to 10 on the ESP8266, with the board run still owed; **MEM-5 and MEM-6 new and open**, both filed by the rows MEM-2 re-pointed) |
| 3. Code Safety | BUG-1 to BUG-26, BUG-28 to BUG-30 | Multiple | 0C, **0H**, 8M (**22 done**; BUG-28 new, BUG-29 filed and fixed same day, **BUG-21 done 2026-08-27 after this row claimed it for months**, **BUG-30 filed and fixed 2026-08-28** — this cell said "new and open" for a day after it was closed, corrected 2026-08-29 — **BUG-2 never closed and never counted** — see below) |
| 4. Test Coverage | TEST-1 to TEST-8 | II (NON-NEGOTIABLE) | 0C, 2H, 3M (**TEST-1, TEST-2, TEST-3 done**; **TEST-8 open, three holes closed and the fourth nearly** — a real multipart POST now runs against a board, refused and accepted, each with a discriminating removal check; what remains is what a browser renders) |
| 5. SSE Bug | SSE-1 | — | **DONE** |
| 6. File Size | SIZE-1 to SIZE-6 | VII (800 lines) | 0C, 2H, 3M, 1L |
| 7. Architecture | ARCH-1 to ARCH-3 | I, XIII | 0C, 2H, 0M (**ARCH-3 done**) |
| 8. CI/Infrastructure | CI-1 to CI-14 | II, XII | 0C, 0H, 5M, 1L (**CI-1, CI-2, CI-3, CI-5, CI-8, CI-9, CI-10, CI-12 done**; CI-11, CI-13 new, **CI-14 new** — FullStack is green in CI and unusable on an ESP8266) |
| 9. Dead Code | DC-1 to DC-13, PERSIST-1 | IV (YAGNI) | 0C, 0H, 8M (**DC-3b, DC-4, DC-5, DC-6, DC-7, DC-8, DC-11 done**; PERSIST-1 new, DC-12 new, **DC-13 new** — three public helpers this repository never calls and users are told to) |
| 10. Minor | LO-1 to LO-32, DOC-1 | Various | 0C, 0H, 0M, 32L (**LO-11 done**; **DOC-1 new**) |
| **Total** | **123 items** | | **0C, 6H, 37M, 34L** (59 resolved) |

The severity columns sum across the rows: 2 + 2 + 2 = 6 HIGH, in Test Coverage,
File Size and Architecture. The six are TEST-4, TEST-6, SIZE-1, SIZE-2, ARCH-1,
ARCH-2 — the same six as the previous lot: the SEC-10 lot closed a CRITICAL and
added **no** open HIGH, its one new HIGH candidate (SEC-12) having been re-argued
to MEDIUM by parity with SEC-7. **Security, Memory Safety and Code Safety all sit
at zero HIGH.** The rows were checked against the section headings rather than only
re-summed — the sweep below was re-run for this change and reports 34 `[HIGH]`
headings, 28 with evidence, 6 open, and the six are the six named here.

**Two stale sentences were found by re-running that sweep, and are corrected
above rather than left.** The Code Safety cell still said "BUG-30 new and open"
and "21 done" while BUG-30's own heading had carried **DONE (2026-08-28)** since
`bea43842` landed later the same day; and the sweep paragraph below still
reported "8 open" from the run made before that fix, against a summary that said
seven. Neither error changed the `0H` figure, which is exactly why neither was
noticed: **a cell can be self-consistent in the column that gets checked and
wrong in the prose that nobody re-reads.** That is the same shape as the BUG-21
slip recorded further down, one lot later.

**Correcting that prose moves no figure, and the reasoning is written down
because the alternative is not obviously wrong.** If the 56 had been computed
with BUG-30 still counted as open, correcting "21 done" to "22 done" would owe
the resolved column a second increment, and this lot would take it 56 → 58. It
does not, because the same 2026-08-28 edit that wrote the 56 also wrote the `0H`
in that row and named seven open HIGH items *excluding* BUG-30 — so BUG-30 was
already counted as closed everywhere the arithmetic touched it, and only the
narrative sentence beside it was left behind. **One item closes in this lot and
the resolved column moves by one.** Anyone who finds evidence the other way
should move it to 58 and say so here.

**BUG-2 was found on 2026-08-27 by the same method that found BUG-21, one day
later, in a file that had just been edited to warn about exactly this.** It had
no DONE marker, appeared in no release table and in no merged lot, and the rows
and the total had been re-summed by script hours earlier and agreed — because the
item was missing from both.

**It left the HIGH column on 2026-08-28, by argument rather than by fix.** Its two
recorded remedies were measured and found impossible, and its severity turned out
never to have been argued at all — only inherited from the sweep that discovered
it. Re-argued on merit it is MEDIUM: no path in the repository reaches it, and the
common slip already returns `nullptr`. `static_cast<T*>` is still there, now
above a contract that says so. See the entry.

The lesson stands and needs sharpening: **summing the rows proves nothing about
items that are in neither.** The only check that finds this class is enumerating
every `[HIGH]` section heading and demanding, for each, a DONE marker or a row in
a resolved table or a merged lot — then verifying the survivors against the code.
That sweep is cheap, it is scriptable, and it should be run before any statement
about how many items remain. It was run on 2026-08-28, all three criteria this
time rather than only the DONE marker: **33 `[HIGH]` headings, 25 with evidence,
8 open** — the eight being MEM-2, TEST-4, TEST-6, SIZE-1, SIZE-2, ARCH-1, ARCH-2
and BUG-30, which was still open when the sweep ran and was fixed later the same
day. This paragraph said "the eight are the eight this summary names" while the
summary named seven; **that sentence was stale for a day and is corrected here**,
which is the whole argument for re-running the sweep rather than editing the
number. Re-run on 2026-08-29 with MEM-2 closed: **33 headings, 27 with evidence,
6 open**. A first pass that checks only for a DONE marker reports 23 open — the
recipe needs all three or it cries wolf. Five of the survivors clear only on the
third criterion, having neither a marker nor a table row: SEC-3, BUG-4, BUG-5 and
BUG-6 are in the merged-lot table at the top of this file, and BUG-15 with them.
Re-run again after the SEC-10 lot: **34 headings, 28 with evidence, 6 open**. The
two new `[HIGH]` headings are SEC-11 (DONE, so evidence) and — transiently — a
SEC-12 filed HIGH; SEC-12 was re-argued to MEDIUM in the same lot, so it leaves
the `[HIGH]` count and the open six are unchanged (TEST-4, TEST-6, SIZE-1, SIZE-2,
ARCH-1, ARCH-2). SEC-10 is a `[CRITICAL]` heading, filed and DONE in-lot, so it
never enters this `[HIGH]` sweep at all.

**They summed before this change too, and both figures were wrong.** The Code
Safety row said `0H` while BUG-21 sat open — no DONE marker, in no release table,
and its constants still unreferenced in `OTA.cpp`. The real count on 2026-08-26
was nine HIGH against a stated eight, and the rows agreed with the total only
because the same item was missing from both. That is worse than the BUG-29 slip
recorded here previously, which at least made the two disagree loudly: this one
was self-consistent and false. Adding up the rows is necessary and not sufficient
— an item that is in neither the row nor the total balances perfectly.

The lot before this one: BUG-21 and TEST-3 close (9 → 7 HIGH), **BUG-30 is filed
and left open** (7 → 8 HIGH), SEC-8 is filed and closed in the same lot (no change
to any severity column since it opens and shuts here). Items 109 → 111 for the two
new IDs, resolved 51 → 54.

**This change (2026-08-29, MEM-2's closing lot):** MEM-2 closes (7 → 6 HIGH, and
Memory Safety's row 1H → 0H), **MEM-5, MEM-6 and DC-13 are filed and left open**
(31 → 34 MEDIUM: two into Memory Safety, one into Dead Code). Items 115 → 118 for
the three new IDs, resolved 56 → 57. No item opens and shuts inside this lot, so
the six remaining HIGH are the seven of the last lot minus MEM-2 and nothing else.

**The lot after it (2026-08-29, SEC-10 the WebUI CSRF lot):** five new IDs,
SEC-10 through SEC-14. **SEC-10 (CRITICAL) and SEC-11 (HIGH) are filed and fixed
in the same lot** — they open and shut here, like SEC-8 did, so neither the
CRITICAL nor the HIGH column moves for them; **SEC-12, SEC-13 and SEC-14 are filed
and left open, all MEDIUM (34 → 37 MEDIUM)** — SEC-12 was first filed HIGH and
re-argued to MEDIUM by parity with SEC-7 before this row was written, so it never
enters the HIGH column. Items 118 → 123 for the five new IDs, resolved 57 → 59 for
the two closed in-lot. The six remaining HIGH are unchanged from the last lot; this
lot closes a CRITICAL and adds no HIGH. **The CRITICAL column
returns to 0 in the same lot it left it**: SEC-10 is the first CRITICAL filed
since 2026-08-28, and it is fixed before the lot closes — "no CRITICAL remains"
was briefly false and is true again, which is the honest way for it to read, not
a claim that none was ever found. The board proof, both directions, is under the
SEC-10 entry; nothing here runs in CI, because `WebUI.h` does not compile
natively.

Both new findings came out of the work rather than out of a review, which is the
pattern every lot has repeated: SEC-2's re-fix raised SEC-7, SEC-7 raised SEC-8,
and BUG-21's tests raised BUG-30 by being the first code in the repository to
subscribe to a topic OTA publishes on.

**The 2026-08-27 real-conditions campaign added three more, and closed none.**
SEC-9, CI-14 and DOC-1 all came from running the shipped examples on a
`nodemcuv2` and a WROOM-32D against a real network, a real MQTT broker and a real
browser — thirteen examples per board, climbing from `01-CoreOnly` to
`FullStack`. Nothing shipped since v2.0 was found broken: SEC-2, SEC-7, SEC-8,
BUG-22 and BUG-29 were each confirmed by their effect on silicon rather than by
the absence of an error. What the campaign found instead was one latent trap
(SEC-9), one configuration CI reports as green and a device cannot run (CI-14),
and two advertised addresses that do not exist (DOC-1). None of the three is
reachable from a host build, and none would have been found by reading the code.

**SEC-9 closed on the same day it was filed, and closing it cost the entry two of
its three consequences.** Checked against the installed Arduino cores rather than
re-read, its consequence 1 was false — `finalizeUpdateOperation()` has set
`progress = 100.0f` all along, and a test asserting exactly that had been passing
since TEST-3 — and its consequence 2 turned out to be a property of streaming an
image of unknown length rather than a defect of the envelope. The fix the entry
recommended would have made both worse and removed SEC-8's pre-write refusal from
the browser path. **A filed finding is a hypothesis, and this one had been read
twice and measured on two boards before anybody checked what it claimed against
the code it blamed.** Severity columns are unchanged by the lot: SEC-9 leaves the
remaining count as it closes, TEST-8 enters it, and the MEDIUM → LOW downgrade
never reaches the table because a DONE item is not counted there — the same
convention SEC-8 was recorded under.

The **item count still does not reconcile**, and did not before this change:
55 resolved + 72 remaining is 127, against a stated 115, while counting the ID
ranges in the Items column gives 119 (118 with STOR-ESP-1 withdrawn). Three
figures, three answers. Left as found rather than re-baselined to whichever one
looks tidiest — someone has to decide what the column is counting before it can
be corrected. Each of the three moved by exactly one in that lot — one new ID,
TEST-8 — so the gaps were unchanged; nothing was hidden and nothing was fixed.

**MEM-2's closing lot moves all three by exactly three**, on the same principle:
three new IDs (MEM-5, MEM-6, DC-13), so the stated total goes 115 → 118; resolved
plus remaining goes 56 + 72 = 128 to 57 + 74 = 131; and the ID ranges in the Items
column gain the same three. The 13-item disagreement between the first two is
therefore exactly where it was. Closing MEM-2 moves one item from remaining to
resolved and changes no gap at all.

**The SEC-10 lot moves all three by exactly five**, the five new IDs SEC-10
through SEC-14: the stated total goes 118 → 123; resolved plus remaining goes
57 + 74 = 131 to 59 + 77 = 136 (resolved +2 for SEC-10 and SEC-11, remaining +3
for SEC-12/13/14); and the ID ranges in the Items column gain the same five. The
13-item disagreement is still exactly where it was — SEC-10 and SEC-11 opening and
shutting inside the lot moves resolved and remaining in step, not the gap between
them.

---

## Archived: Roadmap v1 (2026-03-04 → 2026-03-09) — ALL COMPLETE

| Priority | Items | Status |
|----------|-------|--------|
| 1. Memory Safety (R1-R7, M9-M10) | shrink_to_fit, String→snprintf, char[] migration | DONE |
| 2. Code Bugs (M11-M12, M15-M16, M19) | Core::emit sticky, LED name, Storage events, millis HAL, HA events | DONE |
| 3. HAL Isolation (R8-R10) | millis(), delay(), #ifdef in Storage | DONE |
| 4. File Splits (R11-R13) | Already compliant (excluding blanks/comments) | N/A |
| 5. Dead Code (R17-R23) | isValidTopic, config limits, retryDelayMs, otaPassword, RC auth, effectDirection, Storage WebUI | DONE |
| 6. Anti-Patterns (R14-R16) | Documented as accepted exceptions | DONE |
| 7. Progressive Refactoring (R24-R25) | Virtual dispatch, enum class | DONE |
| 8. EventBus Commands (R26) | ha/command event, callback removal (v2.0.0 breaking) | DONE |
| 9. Documentation Debt (D1-D12) | All 12 doc items resolved | DONE |
| 10. Dead Config Fields (C1-C3) | 8 fields removed from MQTT, System, Storage | DONE |

**Breaking change pending**: 8 dead config fields removed (C1-C3) + callback removal (R26) require MAJOR version bump.

---

*Generated from 13 parallel adversarial review agents cross-referencing all source code against the constitution v1.6.0.*
