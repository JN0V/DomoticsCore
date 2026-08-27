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
| OTA | SEC-9 | 2026-08-27 — filed by the real-conditions campaign and closed the same day, minus two of its three consequences; raised TEST-8 |

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
| ✅ | The 13 native projects run — 729 test cases, discovered from the tracked `platformio.ini` files rather than a hard-coded list |
| ✅ | The three declared targets compile: `esp32dev`, `esp8266dev`, `esp32c3`, via the FullStack example, the only one pulling all twelve components |
| ✅ | `library.json` versions agree with `metadata.version` |
| ✅ | The install-from-GitHub path builds **both** declared platforms — the only thing in CI that resolves through the root `library.json` rather than `file://` paths (CI-8) |
| ⚠️ | **The six on-device suites compile in CI, and nothing runs them.** No runner has a board (CI-10). Run them by hand: `cd DomoticsCore-Storage && pio test -e esp8266dev`. Five are ESP8266; `DomoticsCore-OTA` also carries an `esp32cam` one |
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
- **Problem**: HTTP GET used for mutations (enable/disable, password changes). Passwords appear in query strings, browser history, and server logs.
- **Fix**: At minimum add `Cache-Control: no-store` and document the security trade-off. Ideally use POST for mutations.

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
- **Left open by this lot**: `finalizeUpdateOperation()` does
  `downloadedBytes = totalBytes` for downloads too, where `totalBytes` is the size
  the *server* announced — so a server that announces 8 and streams 6 completes
  reporting 8. SEC-8 exists because servers lie about that number, so calling the
  download side "correct" would be the same overstatement this entry just removed
  from uploads. Recorded in TEST-8, not fixed here: it is a different path, with a
  different lying party, and it deserves its own removal check.

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

### MEM-2 — String concatenation in hot paths across 9 components [HIGH]

Constitution XIV bans `String` concatenation in loops and hot paths. Use `snprintf()` with static buffers.

| Component | Location | Hot path? | Description |
|-----------|----------|-----------|-------------|
| **HomeAssistant** | `HomeAssistant.h:150` | YES (every MQTT msg) | `String(ev.topic)` + `String(ev.payload)` in mqtt/message handler |
| **HomeAssistant** | `HomeAssistant.h:623-690` | YES | `topic.substring()` in handleCommand |
| **SystemInfo** | formatBytes/getFormattedUptime | YES (every 5s) | String concat in metrics formatting |
| **WiFi** | scan loop, `getDetailedStatus()` | Moderate | `summary += ...` in for loop |
| **LED** | `getLEDStatus()` | Moderate | 6+ String concats |
| **RemoteConsole** | `help` handler, telnet negotiation | Cold | Character-by-character String building |
| **NTP** | `getFormattedUptime()` | Cold | `result += String(days) + "d "...` |
| **OTA** | `transition()`, `broadcastProgress()` | Moderate | String concat + JsonDocument per broadcast |
| **System** | NTP server parsing/saving | Cold (boot) | String operations in loop |
| **Storage** | `dumpContents()` | Cold | `String +=` in loop |
| **WebUI** | BaseWebUIComponents methods | Cold (setup) | String concat in selectDropdown/radioGroup loops |

- **Refs**: HA-F1, HA-F6, SI-F1, WIFI-F2, WIFI-F3, LED-F3, RC-F1, RC-F2, NTP-F8, OTA-F8, OTA-F9, SYS-F8, SYS-F9, STOR-F8, WEB-F7
- **Fix**: Replace with `snprintf()` + stack buffers, or `String::reserve()` before loops. Priority: hot-path items first (HA, SystemInfo).

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

---

## Priority 3: Code Safety & Critical Bugs

### BUG-1 — Core: `reinterpret_cast` without trivially_copyable check [HIGH]

- **Ref**: CORE-F4
- **File**: `EventBus.h:101-107`
- **Problem**: `publish()` template uses `reinterpret_cast` to byte-copy payloads. Publishing a `std::string` or `String` results in dangling pointers in the event queue.
- **Fix**: Add `static_assert(std::is_trivially_copyable<PayloadT>::value, ...)`.

### BUG-2 — Core: unsafe `static_cast` downcast [HIGH]

- **Ref**: CORE-F2
- **File**: `Core.h:104`
- **Problem**: `getComponent<T>()` uses `static_cast<T*>` with no runtime type check. Wrong type = undefined behavior.
- **Fix**: Add type-key verification before cast, or use `dynamic_cast` (if RTTI enabled).

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

### BUG-30 — Core: the topic overload of `EventBus::publish` has no trivially-copyable guard [HIGH]

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
- **Latent today, and documented as though it were not.** `OTA.cpp`'s
  `publishStatusEvent()` is the only `emit<String>` caller in the repository, and
  nothing subscribes with `on<String>` — the examples use `on<bool>` and
  `on<MQTTMessageEvent>`, both trivially copyable. But
  `docs/components/ota/technical-reference.md` has always listed the payload
  fields of all six OTA events, which is an invitation to read a payload that
  cannot be read. A warning now sits above that table.
- **Not a one-liner, which is why it is not in the BUG-21 lot.** Adding the
  assert breaks both `emit<String>` call sites at compile time by design. The
  repair is to publish the bytes rather than the object — the
  `publish(topic, const void*, size_t)` overload already deep-copies, so
  `emit(topic, payload.c_str(), payload.length() + 1, sticky)` gives subscribers
  a null-terminated `const char*` they can actually use. That changes the payload
  contract of every OTA event and touches Core, so it wants its own lot and its
  own decision.
- **Check before fixing**: whether MQTT, WebUI or RemoteConsole publish any other
  non-trivially-copyable payload on a topic. The assert will find them all at
  once, which is the cheapest way to enumerate them.

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
- **Four specific holes**, each one SEC-9 could not close:
  1. **`evenIfRemaining` below the HAL.** SEC-9 pins the argument `OTA.cpp` passes;
     the stub's `end()` ignores it. Handing `false` to the real Updater in
     `Update_ESP8266.h` or `Update_ESP32.h` keeps every test green. Both device
     suites announce exactly what they deliver, so their images are finished and
     the flag never matters. The shape that would prove it — **announce N + 220,
     deliver N** — exists nowhere, and it is the shape every browser upload has.
  2. **`installFromUrl()`'s own `end(true)`** at `OTA.cpp:658`, now asserted
     natively but never against a real Updater, for the same reason.
  3. **The download path overstates its byte count.** `totalBytes = announcedSize`
     is the server's claim and `finalizeUpdateOperation()` copies it into
     `downloadedBytes`, so an announce-8-send-6 server completes reporting 8. SEC-8
     exists because servers lie about exactly that number.
  4. **What the browser actually renders.** SEC-9's first recorded consequence was
     a bar that stopped short. `progress` reaches 100, but the SSE cadence is
     ~5.4 s and `autoReboot` restarts the device 2 s after completion — so the
     original observation may have been right about the screen and wrong about the
     cause. Nothing measures the screen.
- **The harness already exists** — `tools/on-device/` drives a real browser, and
  `OTAWithWebUI` reaches a LAN behind `DC_OTA_PREFER_STA`. What is missing is a
  test that uses them, not a means of writing one.

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
- **Why it matters now**: it is the reason `build-device-tests` lists its four
  projects instead of discovering them. "Declares `esp8266dev` and has a `test/`
  directory" is otherwise the right rule, and it matches this one too.
- **Fix**: decide what that environment is for. Either give it a `test_filter`
  naming a real ESP8266 suite, or drop the test settings and leave it a build
  environment. Then the CI job can discover rather than list.

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
| 1. Security | SEC-1 to SEC-9 | OTA, Remote, WebUI | 0C, 0H, 3M (**SEC-1, SEC-3, SEC-7, SEC-8, SEC-9 done; SEC-2 done twice** — the v2.0.1 fix was inert, re-fixed 2026-08-26; **SEC-9 fixed 2026-08-27 and downgraded MEDIUM → LOW**, two of its three recorded consequences having been refuted against the Arduino cores) |
| 2. Memory Safety | MEM-1 to MEM-4, STOR-ESP-1 | XIV (ABSOLUTE) | 0C, 1H, 2M (**MEM-1 done; STOR-ESP-1 withdrawn** — the suite measured an undrained EventBus) |
| 3. Code Safety | BUG-1 to BUG-26, BUG-28 to BUG-30 | Multiple | 0C, **2H**, 7M (**20 done**; BUG-28 new, BUG-29 filed and fixed same day, **BUG-21 done 2026-08-27 after this row claimed it for months**, BUG-30 new and open, **BUG-2 never closed and never counted** — see below) |
| 4. Test Coverage | TEST-1 to TEST-8 | II (NON-NEGOTIABLE) | 0C, 2H, 3M (**TEST-1, TEST-2, TEST-3 done**; **TEST-8 new** — nothing traverses `POST /api/ota/upload`) |
| 5. SSE Bug | SSE-1 | — | **DONE** |
| 6. File Size | SIZE-1 to SIZE-6 | VII (800 lines) | 0C, 2H, 3M, 1L |
| 7. Architecture | ARCH-1 to ARCH-3 | I, XIII | 0C, 2H, 0M (**ARCH-3 done**) |
| 8. CI/Infrastructure | CI-1 to CI-14 | II, XII | 0C, 0H, 5M, 1L (**CI-1, CI-2, CI-3, CI-5, CI-8, CI-9, CI-10, CI-12 done**; CI-11, CI-13 new, **CI-14 new** — FullStack is green in CI and unusable on an ESP8266) |
| 9. Dead Code | DC-1 to DC-12, PERSIST-1 | IV (YAGNI) | 0C, 0H, 7M (**DC-3b, DC-4, DC-5, DC-6, DC-7, DC-8, DC-11 done**; PERSIST-1 new, DC-12 new) |
| 10. Minor | LO-1 to LO-32, DOC-1 | Various | 0C, 0H, 0M, 32L (**LO-11 done**; **DOC-1 new**) |
| **Total** | **115 items** | | **0C, 9H, 30M, 34L** (55 resolved) |

The severity columns sum across the rows: 1 + 2 + 2 + 2 + 2 = 9 HIGH, in Memory
Safety, Code Safety, Test Coverage, File Size and Architecture. The nine are
MEM-2, **BUG-2**, BUG-30, TEST-4, TEST-6, SIZE-1, SIZE-2, ARCH-1, ARCH-2.

**BUG-2 was found on 2026-08-27 by the same method that found BUG-21, one day
later, in a file that had just been edited to warn about exactly this.** It has
no DONE marker, appears in no release table and in no merged lot, and
`Core.h:104` still reads `return component ? static_cast<T*>(component) : nullptr;`
— no type check, as filed. The rows and the total had been re-summed by script
hours earlier and agreed, because the item was missing from both.

The lesson stands and needs sharpening: **summing the rows proves nothing about
items that are in neither.** The only check that finds this class is enumerating
every `[HIGH]` section heading and demanding, for each, a DONE marker or a row in
a resolved table or a merged lot — then verifying the survivors against the code.
That sweep is cheap, it is scriptable, and it should be run before any statement
about how many items remain.

**They summed before this change too, and both figures were wrong.** The Code
Safety row said `0H` while BUG-21 sat open — no DONE marker, in no release table,
and its constants still unreferenced in `OTA.cpp`. The real count on 2026-08-26
was nine HIGH against a stated eight, and the rows agreed with the total only
because the same item was missing from both. That is worse than the BUG-29 slip
recorded here previously, which at least made the two disagree loudly: this one
was self-consistent and false. Adding up the rows is necessary and not sufficient
— an item that is in neither the row nor the total balances perfectly.

This change: BUG-21 and TEST-3 close (9 → 7 HIGH), **BUG-30 is filed and left
open** (7 → 8 HIGH), SEC-8 is filed and closed in the same lot (no change to any
severity column since it opens and shuts here). Items 109 → 111 for the two new
IDs, resolved 51 → 54.

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
be corrected. Each of the three moved by exactly one here — one new ID, TEST-8 —
so the gaps are unchanged; nothing was hidden and nothing was fixed.

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
