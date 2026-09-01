# BUG-35 — a client disconnect mid-upload must abort the update, not entomb it

Status: v2 — amended after adversarial review, 2026-09-01. The review's
three substantive catches, folded in: the two-concurrent-clients limit of
the shared uploadState (pre-existing, documented, and the alternative
predicate argued worse); the missing ESP8266 HTTP-level proof (added —
abort() releases the Updater buffer there, the platform-divergent half);
and the TCP half-open case (a client dying without closing), recorded as
the entry's residual pending AsyncTCP-timeout verification.

Originally v1 draft, 2026-09-01. Roadmap item: BUG-35 [HIGH], filed by the
second real-conditions campaign. The measured defect: an upload to the
WROOM-32D died with a broken pipe at 12%; `/api/ota/status` stayed frozen at
`state=downloading, progress=12.09` (identical 75 s later), and every
subsequent upload — token and hash correct — was refused
`"Upload already in progress"` until a power-cycle. Branch
`bug-35-ota-disconnect-abort`, one PR, no version movement.

## Mechanism, located and verified

`OTAWebUI.h`'s upload chunk handler calls `ota->abortUpload(...)` on CSRF
and auth refusals but registers **no `request->onDisconnect`**. When the
client vanishes mid-body, AsyncWebServer never delivers a `final` chunk, so
`ota`'s `uploadSession.active` stays true, `beginUpload()` refuses forever
(`isIdle()` is false in `Downloading`), and nothing else ever runs to clear
it. SEC-8 taught the *test* suites to release an open update in
`tearDown()`; production never got the equivalent.

## The fix, and the trap it must not fall into

**`onDisconnect` fires after every request, including successful ones** —
it is the socket closing, not an error signal. An unconditional abort would
wreck the just-completed upload's state (and race the autoReboot). The
predicate already exists: `uploadState.active`, set true after the index-0
gates pass, set false at `final`. Registered once, at index 0, before the
gates (a refused upload leaves `active` false, so the callback no-ops):

```cpp
if (index == 0) {
    uploadState = UploadState{};
    // BUG-35: a client that vanishes mid-body leaves the update open
    // forever — beginUpload() then refuses until a power-cycle. active
    // is the discriminator: it is false before the gates pass and false
    // again after `final`, so a disconnect after a completed (or refused)
    // upload is a no-op here.
    request->onDisconnect([this]() {
        if (!uploadState.active) return;
        uploadState.active = false;
        uploadState.success = false;
        uploadState.error = "Client disconnected mid-upload";
        ota->abortUpload("Client disconnected mid-upload");
    });
    ...
```

Safety arguments, each checked against the code rather than assumed:

- **No interleaving**: the chunk handler and `onDisconnect` both run on the
  AsyncTCP task — they never preempt each other; the `active` ordering
  below is defence-in-depth, not the load-bearing argument. And this call
  site adds to an existing concurrency posture, not a new one:
  `beginUpload`/`acceptUploadChunk`/the existing `abortUpload` calls
  already run from the same async context while `OTAComponent::loop()`
  runs on the Arduino task.
- **The two-client limit, stated**: `uploadState` is one shared field —
  client B's index-0 reset (SEC-3's, not this lot's) erases A's `active`
  while A is mid-body, so an A-disconnect after that no-ops and A's lock
  survives. Pre-existing single-session assumption, out of scope; the
  OTA-side predicate would be worse (a refused B's disconnect would abort
  A's legitimate session). Invariant the comment relies on, written down:
  **`active` must never be set before every refusal path has returned.**
- **The half-open residual**: a client that dies without closing (no FIN,
  no RST) hangs until AsyncTCP's own ack timeout eventually fires
  `onDisconnect` — slower but bounded, and unverified on a board today;
  recorded as the entry's residual rather than claimed closed.
- `abortUpload()` (OTA.cpp:452) is a no-op when `uploadSession.active` is
  false — a second layer under the WebUI predicate.
- After the abort, `state == Error` and `isIdle()` returns true
  (OTA.cpp:470), so the next `beginUpload()` is accepted — that is the
  recovery this lot exists to provide.
- `HAL::OTAUpdate::abort()` is "only meaningful before end()"
  (Update_HAL.h) and inert after — a disconnect racing the final chunk's
  `end(true)` cannot un-commit an installed image; it would only flip the
  *reported* state, and `uploadState.active` is already false by then (set
  false before `finalizeUpload()` runs... **order checked**: `final` sets
  `active = false` *before* calling `finalizeUpload()`, so even a
  disconnect delivered mid-finalize sees `active == false`).
- Lambda lifetime: captures `this` (OTAWebUI), same lifetime the route
  lambdas already rely on.
- Both platforms take this path — the ESP8266's `abort()` releases the
  Updater buffer (SEC-8's mechanism), the ESP32's calls `Update.abort()`.

## Measurement (TEST-8 family: this is HTTP-envelope behaviour, invisible to every Unity suite)

No native or on-device Unity suite compiles this handler against a real
request — that is TEST-8's standing observation — so the test is the
harness, made reproducible rather than left as yesterday's accident:

1. **`ota_upload_check.py --disconnect-at N`**: raw-socket variant that
   sends the headers and the first N bytes of the multipart body, then
   **closes abruptly (SO_LINGER 0 → RST, the accident's shape; FIN also
   funnels into onDisconnect)**, waits, then reports `/api/ota/status` and
   attempts a full correct upload. Status assertions: `state == error`
   with the disconnect reason after the fix — and **`total` will read the
   envelope size on an abort, by design**: the SEC-9 narrowing happens at
   finalize, which an abort never reaches; asserted as such so nobody
   files a SEC-9-flavoured false alarm. Documented in the harness README
   in this lot.
2. **Red first, on the unfixed firmware** (the WROOM-32D still runs it),
   **from a fresh boot**: serial reset, verify status `idle`, then the
   scripted disconnect — so the frozen status is attributable to this
   disconnect alone, not yesterday's session. Expected: status frozen
   `downloading`, retry refused `"Upload already in progress"`. The red
   run ends with the board locked by design; **the fix flash (serial) is
   also the unlock**.
3. **Green after**: rebuild FullStack esp32dev with the fix, flash
   (sequentially — the campaign's parallel-build lesson is one day old),
   re-run: status shows `error` with the disconnect reason, and the
   follow-up upload succeeds end-to-end (install, reboot, back idle).
4. **ESP8266 HTTP-level proof, not only compile-level**: flash
   `OTAWithWebUI` on the nodemcuv2 and run `--disconnect-at` there too —
   `abort()` on the ESP8266 *releases the Updater buffer* (SEC-8's
   mechanism), the platform-divergent half of this fix, on the constrained
   platform. Then the on-device OTA suite (10/10 baseline) — stated
   plainly: that suite calls `beginUpload()` directly and discriminates
   nothing about the handler change; it is compile-plus-adjacent-behaviour
   non-regression only. The ESP32 OTA Unity suite cannot run (ESP32-CAM
   absent); the ESP32 evidence is the HTTP-level green above, which
   exercises more of this change than the Unity suite would.
5. The happy path re-proven after the fix: one clean `--commit` cycle on
   the flashed WROOM-32D (upload, install, reboot, back idle) — proving
   the new callback's no-op arm on the success path, not only the abort
   arm.

## Order

1. Harness: `--disconnect-at` added; red measured on the unfixed WROOM-32D.
2. Fix in `OTAWebUI.h`.
3. Rebuild esp32dev, flash, green: disconnect→abort→recovery, then a clean
   `--commit` cycle.
4. nodemcuv2 OTA suite 10/10.
5. Roadmap: BUG-35 → DONE with measurements; HIGH 1 → 0 open again — by a
   fix this time; sweep re-run; both reconciliation families.
