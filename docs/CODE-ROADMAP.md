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

The specifications, plans and reviews that entries below name (`spec-*.md`,
`review-*.md`, `tech-spec-*.md`) are working documents: since 2026-09-14
they stay out of the tree, and the ones committed before that are in the
history (`git log --all -- _bmad-output/`). A decision that outlives its lot
goes to `docs/decisions/`; a decision a lot parks rather than takes to
`docs/deferred-work.md` — a finding with a consequence takes an id here instead.

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
| HomeAssistant | BUG-31 | 2026-08-29 — TEST-6's lot A. The HA settings handler read six parameters the dispatcher never sends; filed and closed in the same lot, raising TEST-9 and DC-14. TEST-6 stayed open for lot B |
| WebUI, SystemInfo, Storage | BUG-32, TEST-6 | 2026-08-31 — TEST-6's lot B. `device_name` was interpolated raw into every update; escaped at the sink (an extracted `buildSystemHeader`, the first slice of SIZE-1), with `SystemInfoWebUI` validation and Storage coverage. **BUG-32 filed and closed, TEST-6 closed** (6 → 5 open HIGH) |
| Core, Wifi | TEST-4 | 2026-08-31 — the stubs were the blocker: scriptable millis/heap/restart in `Platform_Stub.h`, a stateful mode/AP/connect record in `Wifi_Stub.h` (defaults byte-identical), and a 16-case behavioural suite over the fallback ladder, AP mode and reconnection; five mutations all caught; the MEM-2 device suite finally ran, 3/3 on a real radio. **TEST-4 closed** (5 → 4 open HIGH), DC-15 filed |
| WebUI | SIZE-2, BUG-26, BUG-28, BUG-33 | 2026-08-31 — 933 → 756 + a 216-line `JsonStreamWriter.h`, extracted as a privately-inherited base so the fork's serializer hunks still land, with marianorenzi's own streaming-multiselect design adopted and credited. **SIZE-2 closed** (4 → 3 open HIGH), **BUG-28 closed** with it, **BUG-26 found already fixed** since July (`dc8886f1`, his), **BUG-33 filed and fixed** (host-only UTF-8 mangling, char signedness — measured against all three toolchains). Board: schema-memory suite 3/3, heap drift zero with a multiselect in the loop |
| WebUI | SIZE-1, BUG-34 | 2026-08-31 — WebUI.h 1008 → 769 + `SchemaMemProbe.h` (121) + `UpdateBuilder.h` (90); the schema chunk loop, which existed three times, deduplicated into `SchemaChunkState::writeChunk` (ProviderRegistry.h 345 → 441) and finally testable — ten new native tests, component count 92 → 102. **SIZE-1 closed** (3 → 2 open HIGH — only ARCH-1/ARCH-2 remain), **BUG-34 filed and fixed**: `/api/ui/schema` had never received v1.5.0's truncation fix, and the tests found the stall fires at ordinary chunk sizes, not only below the escape floor. Fork's two WebUI.h regions untouched. Board: heap suite 6/6 and schema-memory 3/3 on the nodemcuv2 |
| LED (docs only) | ARCH-2 | 2026-08-31 — closed **by measurement, no code change**: the prescribed remedy already existed (LEDWebUI.h separate since ≥2025-09-26, so "WebUI in one class" was false at filing; the pure effect engine landed with PR #17). BUG-19 cited as the structure's one recorded defect, closed by tests not by splitting. LED-F1, the cited source, does not exist in the repository. **ARCH-2 closed** (2 → 1 open HIGH — ARCH-1 is the last) |
| System (docs only) | ARCH-1 | 2026-08-31 — **re-argued HIGH → MEDIUM, open, dual trigger**: SYS-F6 unreadable (the HIGH was inherited, never argued); one XIII indicator exceeded (`begin()` 62, stable since filing) against a file inside every other measurement; the fork rewrites two of three prescribed extraction zones, the third declined as YAGNI. **Open HIGH reaches zero — by reclassification, stated in those words.** marianorenzi notification drafted for the maintainer |
| Tooling, harness | CI-13, CI-15, BUG-35 (filed) | 2026-09-01 — the second real-conditions campaign's lot: `clean_examples.py` learns `test/*/.pio` after a 19 GB recursion (**CI-13 closed**), the harness learns the SEC-10 token, **CI-15 filed** (no `export.exclude` anywhere — root cause, release-aware), **BUG-35 filed** from the disconnect accident (open HIGH 0 → 1 — the campaign doing its job) |
| Core, SystemInfo, System | OBS-2, OBS-6, OBS-7, OBS-1 (boot check) | PR #60 — 2026-09-05, **Lot A** (opened as #59, stacked on #57, auto-closed when #57's branch was deleted at merge, reopened as #60): the reset registers the ESP8266 kept, the ESP32 core dump nobody read, the heap keys that lied about which boot they described, and a loop watchdog on ESP32 (default 30 s, a behaviour change for the next release to announce). Measured on three boards from the branch (the C3's first run ever); two mutations caught natively; FullStack green on all three targets. **Adversarial review run after opening** (13 findings): the watchdog armed one task and fed another — fixed and re-measured on both ESP32s; the maintainer's own review had already caught the one Constitution IX `#if` outside the HAL. Native 819 → 851 |
| WebUI, RemoteConsole, System | SEC-4, SEC-6, SEC-14 | **PR #69** — 2026-09-14, the afternoon after Lot D: the plan went through the adversarial lens before any code (20 findings: three guard sites where the plan had assumed one, the same empty-password defect in the console, no `OPTIONS` handler so the preflight headers are dead letters, a global lockout as a denial-of-service lever), and the maintainer ruled the brute-force defence **delay-only, never block, never disconnect**. The console's `auth` wait (1, 2, 4, 8 s, per address, held not refused), the CORS header withheld under auth with the entry's premise corrected, the empty password refused on the WebUI and the console with `SystemConfig::consolePassword` new. Native: removal checks passed all four on a stale libdeps copy, then went red from a fresh `.pio`. Boards: red-then-green SEC-14 on the WROOM-32D (200 with an empty password before, refused after), the console wait timed from the outside on the nodemcuv2, CORS with and without auth in curl and Chromium |
| Core | TEST-7, BUG-40, DC-17 | **PR #70** — 2026-09-15: the plan went through the adversarial lens before any code (17 findings; among them that the first draft said the WebUI's memory-profile reduction fires on no board, which CI-14's own log refutes, and that the float removal check would have gone red for the stub's six decimals rather than the board's two). The reading came first: `ComponentConfig` has no caller in the tree and is documented public API (DC-17, with eight `MemoryManager` queries), its validators were wrong for any sketch (BUG-40, fixed), the header did not stand alone, and the native `String` stub threw where Arduino returns 0. Core 154 → 201 cases from a fresh `.pio`, 13 projects 1028, six removal checks red for the right reason, intermediate commits each verified in a worktree. No board leg |
| HomeAssistant | BUG-63, BUG-13, BUG-62 | **Merged** — PR #85, 2026-09-23: a code-less alarm panel published no `cod_arm_req` and could not be armed from Home Assistant, filed from a consumer's production panel and measured 527 → 567 bytes on a WROOM-32D; BUG-13's recorded fix would have been inert, the ceiling being MQTT's 128-byte event field rather than the component's buffer; the review caught the regression the first version introduced, a permanent refusal stalling the held-state store. Raised BUG-65 (the 188 characters of `pl_*` keys restating Home Assistant's defaults) and BUG-66
| RemoteConsole, Wifi (HAL) | BUG-67 | **PR #88** — 2026-09-24: a console opened its telnet server whatever was under it, which stops an ESP32 inside lwIP and boot-loops it. `HAL::canOpenServer()` answers per platform — always on ESP8266, the count of network interfaces on ESP32, so a transport that is not WiFi counts — and `loop()` opens the server when a stack appears. Red-then-green on the WROOM-32D, with the abort reproduced by removing the guard. **The lot's own review found the fix's defect**: the deferred open was gated on `config.enabled`, which `shutdown()` does not clear, so a console disabled from the web UI got its port back on the next loop, serving commands with no logs; gated now on what `begin()` sets and `shutdown()` clears. Filed in its place: **BUG-69**, the WebUI's own `AsyncWebServer`, which is the other socket opened in a `begin()` and does not ask. 70 → 73 native cases |
| HomeAssistant, RemoteConsole, CI | BUG-65, TEST-12, CI-11, CI-19 | **PR #87** — 2026-09-24: what the required checks compile, a board now runs. The alarm panel's seven `pl_*` keys stated the payloads Home Assistant assumes for an absent key — 740 → 552 on the shape that was refused whole, 613 → 541 on the production one, and the vocabulary they stated is now a native pin because `handleCommand()` compares nothing. RemoteConsole gets its first device environments and HomeAssistant its first real board coverage: 7/7 and 5/5 on the WROOM-32D, 7/7 and 9/9 on the nodemcuv2, each with removal checks that discriminate — on ESP32 the capture reports itself installed while nothing arrives, which is BUG-64's disjoint sinks, executed. The first flash found **BUG-67**, an ESP32 that boot-loops when the console starts before the network stack; the lot's own review found the sharper hole, a `test_filter` naming a suite that no longer exists building nothing and exiting 0, so the check created to catch silent rot could have gone green over an empty run — guarded, and the guard proved non-vacuous. Opened besides: **BUG-68** (the same restating-the-defaults rule broken by every other entity) and **CI-20**. Native 1 248 → **1 249** |
| RemoteConsole | BUG-64 | **PR #86** — 2026-09-24, the platform's own log lines, captured behind a new `CoreLog` HAL. Both ESP32 sinks are needed and disjoint, the ESP8266's SDK printing is off at the source and stays off behind a `core` command, and the board found our own `DLOG_*` being captured twice, and the adversarial review found nine more defects in the accounting and the two sinks' interaction. Eighteen native tests, twelve removal checks, red-then-green on both boards
| CI, every manifest | CI-15 | **PR #71** — 2026-09-15: the plan went through the adversarial lens before any code (16 findings; among them that the first draft claimed no overlap with marianorenzi's branch where there are 31 manifests, and that the `../../` shape was unmeasured — measuring it found the lot's real obstacle, a symlinked component that contains the project claims every include under it). `symlink://` in all 49 manifests, the self lines gone, libdeps of the 31 nested projects at the repository root, `clean_examples.py` retired, `embed_webui.py` reading the `.pio-link`, two CI guards. Native 1070 from a fresh `.pio`; the stale-copy trap red-then-green; FullStack esp8266dev byte-identical to CI; 92/92 examples and probes; OTA 10/10 on the nodemcuv2; a project depending on the checkout through `symlink://../DomoticsCore` builds |
| System, MQTT, HomeAssistant, WebUI, OTA, Core (HAL), SystemInfo, the bench tools | OBS-5, OBS-1 (transport), SEC-13 | **PR #67** — 2026-09-14, **Lot D**: what leaves the device — `{clientId}/telemetry` once a minute and `{clientId}/crash` retained at connect, allocation-free on the device's side through the new `publishNow()`; Home Assistant discovers both as eight diagnostic entities through `value_template` (five discovery fields new on `HAEntity`); the core dump streamed, decoded against its ELF and erased through the WebUI; SEC-13's two routes gated, and `/` freed of a stale copy of `enableAuth`. Planned locally (24 review findings folded before S0; the plan and the review stay out of the tree by the 2026-09-14 rule), six slices, every mechanism reachable natively proven with a scripted sink and three mutation checks. Both boards plus a Home Assistant container on the host, onboarded through its API. **The four-layer code review before the PR** (Blind Hunter 21, Edge Case Hunter 18, Verification Gap 9 with two surviving mutations measured, Acceptance Auditor 20 plus the 24 plan-review folds checked one by one): ~68 raw findings, 46 patched — among them `publishState()` writing to the generated topic while discovery advertised the override, the crash record announcing an erased dump until reboot, a 49.7-day `uptime` wrap into a `total_increasing` sensor, `rssi` 0 dBm without a link — 4 deferred, the rest dismissed; the review also filed BUG-38 (the alarm panel's document over the event field, cut for months) and DC-16, and measured two of the plan review's findings the campaign had skipped. **All three fixed on the branch before the PR, the same day**, BUG-38 after its decision memo went through the adversarial review — which refuted the wider field on ESP8266 and filed BUG-39, fixed with it. Native **934 → 972**, both ends read from the CI runs of `main` and of PR #67 — the handoff's 936 → 972 was a projection, off at both ends |
| Core, System, the bench tools | OBS-4 | **PR #66** — 2026-09-07, **Lot C**: the failed-allocation group in the flight record (layout 2, outside the CRC, stored straight to RTC from the ESP32 heap hook or the ESP8266 latch-and-clear of the core's globals), the ESP8266 diagnostic profile measured, the survived crash kinds. Planned in `spec-obs-lot-c-oom-moment.md` v2 (19 review findings folded before S1), five slices in three commits, the four-layer review of the diff before the PR (`review-obs-lot-c-code.md`: 41 raw, 24 patched — a survived failure meeting an exception was reported as the death's cause). Both boards: a healthy WROOM never fires, squeezed to 12 KB it dies of the task watchdog after 168 survived failures; the ESP8266 per-loop cost turned out link-layout-bound. Native 913 → 936 |
| Core, System, SystemInfo, Storage (stub) | OBS-3, BUG-36, ARCH-1's residue | **PR #65** — 2026-09-06, **Lot B**: the flight recorder in RTC memory (Core-owned, promotion first, the record held until persisted), the `bootdiag` blob, the crash commands, ARCH-1's boot-diagnostics formatter out of `System.h`. Planned in `spec-obs-lot-b-flight-recorder.md` v2 (22 review findings folded before S1), five slices, three-layer review of the diff before the PR (the hold blocked a fresh record during bring-up; the ring printed in slot order after a wrap; the loop wiring unobserved — all fixed with tests). Both boards' death sequences read back; four defects the boards found that 115 native tests had not. Native 851 → 913 |
| Observability, Core (docs only) | OBS-1 to OBS-7, BUG-36 filed | PR #57 — 2026-09-05, the post-mortem observability design, adversarially reviewed (22 findings) and measured on the nodemcuv2, the WROOM-32D and the ESP32-CAM the same day; eight items filed, none closed. The review's first finding — an OOM in `new` reaches the next ESP8266 boot as "Software/System restart" — was confirmed on the board within the hour and reshaped the design |
| OTA | BUG-35 | 2026-09-01 — **filed in the morning, fixed in the afternoon**: `onDisconnect` → `abortUpload`, gated on the upload-active discriminator because onDisconnect fires after every request. Red-then-green with the same `--disconnect-at` script on the WROOM-32D and the nodemcuv2 (the ESP8266's buffer-release path included), `--commit` cycle re-proven, OTA suite 10/10. **BUG-35 closed** (1 → 0 open HIGH, by a fix this time). Two limits written at the site: the shared-state two-client blind spot (pre-existing), the TCP half-open residual |

The first series closed with v2.1.0 and v2.1.1. **The second ships as v2.2.0**
(2026-08-26): SEC-2, SEC-7 and BUG-29, plus the on-device suites that now run on
both an ESP8266 and an ESP32. Six components move; `System` and `Storage` do not,
having gained only a comment and a test respectively.

The no-version-bump rule held throughout — component versions and the CHANGELOG
moved once, here, at the end. It is why OTA sat at 1.5.0 while SEC-2 and SEC-7
landed.

**The third ships as v2.3.0** (2026-09-05): everything from SEC-8 to BUG-35 —
SEC-10 and SEC-11, BUG-21, BUG-30 to BUG-35, MEM-2, TEST-4, TEST-6, TEST-8's
four holes, SIZE-1 and SIZE-2, CI-13. CI-15 was to ride it and does not: its
prescribed `export.exclude` was measured inert on both paths while the branch
was being prepared (see the entry). **OBS Lot A was folded in the same day**
(PR #59), so the fleet pays one OTA window rather than two. Nine components
move; LED, MQTT and NTP do not, having changed only in examples and tests.
Three public contracts change (the CSRF token on state-changing routes, the
EventBus payload guard, the loop watchdog armed by default on ESP32 with the
`BootDiagnostics` rename) and the CHANGELOG says so at the top of the entry, as 2.1.0 and
2.2.0 did.

**The fourth ships as v2.4.0** (2026-09-14): BUG-37, the three OBS lots B, C
and D (OBS-1, OBS-3, OBS-4, OBS-5), SEC-13, BUG-36, BUG-38, BUG-39 and DC-16
— the observability series closed end to end. Seven components move: Core,
System, HomeAssistant, MQTT, OTA, WebUI, SystemInfo; Storage and Wifi do
not, having gained only a stub counter and a test comment. Five public
contracts change (telemetry and the retained crash topic on by default with
MQTT, the abbreviated discovery keys, the auth gate on the info route and the
stream, `uploadIdleTimeoutSec`, the strong crash callback with the `bootdiag`
blob) and the CHANGELOG says so at the top of the entry. Native 844 → 972,
both read from CI runs rather than projected.

**The fifth ships as v2.5.0** (2026-09-16): SEC-4, SEC-6, SEC-14, TEST-7 with
BUG-40 and DC-17, and CI-15. Four components move: Core (the stub `String`,
the validators), WebUI (the CORS header withheld under auth, the
empty-password refusal), RemoteConsole (the `auth` wait, `authDelayMaxMs`),
System (`consolePassword`); the other eight gained manifests and comments
only. Three public contracts change (an empty password can no longer enable
authentication, two new config fields, the stub's parsing) and the CHANGELOG
says so at the top. The README's *Installation* gains the local-checkout
recipe and an *Upgrading* pointer to the CHANGELOG's top notes; the
observation page gains the first-boot checklist. Native 1070, read from CI.

**The sixth ships as v2.6.0** (2026-09-21): BUG-41 with BUG-42, BUG-43,
BUG-44, BUG-45, BUG-47, BUG-51 to BUG-55, TEST-8 and TEST-9, and the
deferred-work purge. **Eleven components move**, all by a minor: every one of
them changed what a client reads back from a settings POST, which is the
release's headline — a bare `{"success":false}` now carries the reason, and the
page draws it under the field instead of in a modal. Core (the queue bounded by
bytes, `QueueCost`, `digitsOnly()`), WebUI (the refusal channel and the two
parsing helpers), Wifi (the scan card), MQTT (the loss announced at the
transition), HomeAssistant (the availability topic the will writes), NTP (the
interval in seconds), RemoteConsole, LED, OTA, Storage, SystemInfo; System
gained tests and examples only and stays at 1.8.0. Eight contract changes are
named at the top of the entry. Native 1 197, read from CI.

**The seventh ships as v2.6.1** (2026-09-23), and it is corrective rather than
the close of a series: SEC-16 was a HIGH left unreleased, with `/api/ota/update`
gated by the CSRF token alone, so a URL install needed no password where an
upload did. It carries SEC-17, BUG-60 and BUG-46 with it. **Three components
move** — Core 1.10.0 (the EventBus serialised between the two tasks that use
it), HomeAssistant 2.3.0 (the outage store, and two new public symbols), OTA
1.9.1 (the six routes behind `authorize()`); Storage and System changed only a
test and an example. **The root takes a patch while two components take a
minor**: the number says what the release is *for*. Its top note also reverses
2.6.0's, which had told consumers to republish entity states from their own
handler. Native 1 220, read from CI. The release gate — every open MEDIUM
arbitrated — is **not** met and was not claimed: 3 of 23 open MEDIUM headings
carry an arbitration, and that is the work the next series starts from.

| WebUI | BUG-45 | 2026-09-20 — what five settings fields accept. A numeric field is digits only (BUG-40's rule on the WebUI surface) and the range policy stays the field's: refused where a range exists, clamped where a slider clamps. Six providers answer `"{}"` for a context they ignore and `UpdateBuilder` tolerates `"null"` at the sink. The refusal *shape* was left alone — it contradicts `ba901c4` — and became BUG-51; the adversarial review opened BUG-52 and BUG-53 and caught a vacuous width guard, an unpinned schema/dispatch coupling and a forced `String` copy on the broadcast path. Native 1 160 → 1 173; a browser on the WROOM-32D each side of two fixes; ESP8266 +164 B RAM, +580 B flash |
| HomeAssistant, three examples | BUG-46, BUG-61 (filed) | **PR #83** — 2026-09-23: the decision memo went through the adversarial lens before any code, and the diff went through it after — which found the fix reintroducing its own defect (a live publish not invalidating the slot the outage still held) and the three examples gating the event-driven publish on `isMQTTConnected()`, which would have made it inert. A coalescing store in the component, 12 bytes standing and a 2 048-byte budget, drained four per `loop()`. Native 1 217 → 1 220 locally, eight removal checks; WROOM-32D `pending` 0 → 6 → 2 → 0 against a broker stopped and started, and nothing at all with the hold removed. **BUG-46 closed, BUG-61 filed**: 28 → 27 M, 53 → 54 L, 183 → 184 items, 115 → 116 resolved |
| Docs | deferred-work purge | 2026-09-20 — `docs/deferred-work.md` had 62 entries and 5 ever closed, 42 of them added in fifteen days, none of them counted by the tracking summary. Read one by one: **34 became nineteen items** — SEC-15 and BUG-47 (MEDIUM); MEM-7, MEM-8, BUG-48, BUG-49, BUG-50, TEST-10, CI-16, CI-17, CI-18, DC-18, DC-19, OBS-8, DOC-2, LO-33 to LO-36 (LOW) — 5 folded into the entries that parked them (SEC-4, SEC-6, CI-11, BUG-45, LO-2), 6 became `project-context.md` or ADR notes, 15 deleted as done or already recorded, 2 kept as decisions. Rule in the file's header: a finding with a visible consequence takes an id when it is written. Docs only, no code |
| OTA, Core (HAL) | SEC-16, SEC-17, BUG-60 | 2026-09-22 — who may tell the device to install firmware. `/api/ota/check` and `/api/ota/update` took the CSRF token and nothing else, so uploading the bytes needed the password and pointing the device at a URL did not; the four read routes beside them answered a stranger with that URL, the running version and the update state. All six now take `authorize()`. **The board run proved the fix and broke the device**: an intermittent panic on core 0 inside the EventBus, decoded to an insert from the web task racing an erase from the loop — filed as BUG-60, then made reproducible on demand by a probe that boot-loops unfixed, and closed by a recursive lock in the platform HAL with handlers dispatched outside it. 1.88 million publishes in 89 s green, +1 336 B flash on ESP32 and a byte-identical ESP8266 image. Native 1 197 → 1 207 `[PASSED]` — read from CI's native job, +10 from this lot (three on the bus lock, seven on the OTA routes) — and four removal checks; on the WROOM-32D 2/2 triggers and 3/3 reads refused without credentials, against 0/2 and 0/3 before |
| Wifi | BUG-47 (+ BUG-59 filed) | 2026-09-21 — the scan an operator clicks while provisioning. Three halves, not the two filed: `loop()` returned before the poll whenever no SSID was configured, the provider kept a write-only copy of the summary, and the button was a field of no context. The scan now has its own always-interactive card, fed from the component, with a second scan refused by name; a locked settings card could not have drawn the result where it was clicked. Twelve tests, three removal checks — not one per half, but the display is proved with the harvest reverted — 1 185 → 1 197 `[PASSED]`; a board probe harvesting two scans on the nodemcuv2 and hanging for twenty seconds with the call moved back; ESP8266 +140 B RAM, +628 B flash; a browser on the WROOM-32D, where the button needs no Edit and the Display redraws 8.1 s after the click. Its own review found four defects in the lot, all fixed here, one of them the shape BUG-55 had closed elsewhere the day before. The WROOM-32D never delivers the first scan of a process in AP mode — BUG-59, surfaced and not caused |
| WebUI, RemoteConsole, LED, NTP, OTA, Wifi, Storage, SystemInfo, HomeAssistant, Core | BUG-51, BUG-52, BUG-53, BUG-54, BUG-55 (+ BUG-56 to BUG-58, TEST-11 filed) | 2026-09-20 — how a refusal reaches the person typing. Twenty-six bare `{"success":false}` returns across seven providers said nothing at all; the fourteen that did name a reason, over four providers, raised a modal that held the page's own updates. The reason is now drawn under the field, on the card when no field is in cause, and for a transport refusal too, which has no body to name it. The three non-numeric surfaces BUG-45's rule did not reach are closed (the telnet `level` command: digits only, 0-5, the range its own WebUI route already offered; LED's `effect` and `led_select`), the NTP field is denominated in seconds, and Cancel restores what it saved. **The adversarial review is half this lot**: it found the header rename posting a context `SystemInfoWebUI` never accepted — never worked, and about to become a permanent banner on an unrelated card — plus five more regressions fixed here and four items filed. One pinning test was vacuous, caught by its removal check; fourteen of the twenty-six reasons were asserted by nothing and now are, OTA's action handler having had no test at all. Native 1 173 → 1 185 `[PASSED]`; fourteen removal checks; board pairs on the WROOM-32D for both refusal paths, the silent case and Cancel; ESP8266 +1 568 B flash, +160 B RAM |

`main` requires seven checks: `test-install`, `check-versions`,
`Unit tests (native)`, `Build esp32dev`, `Build esp8266dev`, `Build esp32c3`,
`Build on-device suites`, plus an up-to-date branch and resolved conversations.
The seventh became required on 2026-08-23 with CI-10; the count here still said
six.

### What CI proves, and what it does not

Worth knowing before a green tick is read for more than it is worth.

| | |
|---|---|
| ✅ | The 14 native projects run — **1 249** `[PASSED]` lines on 2026-09-24, read from the native job of PR #87 and not from a whole-run grep, which counts the on-device job's banners too; discovered from the tracked `platformio.ini` files rather than a hard-coded list. **Re-derive this figure, do not trust it**: it read 729 on 2026-08-28, 739 after MEM-2's hot half, 747 after its closing lot, 767 after BUG-31's provider suite on 2026-08-29, 788 after BUG-32's twenty-one, and 804 after TEST-4's sixteen in `test_wifi_behaviour`; 809 adds SIZE-2's five in `test_streaming_serializer` on 2026-08-31 (that suite runs 15: the UTF-8 pin, two chunk sweeps, an empty-multiselect sweep and a serializer-reuse test on top of the original ten); **819** adds SIZE-1's ten on the same day — `test_schema_chunking` (5) and `test_update_builder` (5), both new directories in the WebUI project's own `test_filter`, driving the chunk-assembly loop and the update builder that no test could compile before the extraction; **851** after Lot A's thirty-two; **913** after Lot B's sixty-two (`test_flight_recorder` 40, `test_system_ready` +6, `test_eventbus` +2, `test_system_lifecycle` +6, `test_system_persistence` +8), counted as RUN_TEST lines against `main`; **1 070** at the CI-15 lot, counted properly for the first time as `[PASSED]` lines from a full run rather than as RUN_TEST lines; **1 085** after the BUG-41/42 lot: the fourteenth project (`DomoticsCore-Core/test/test_queue_budget_override`, 3), 11 added in `test_eventbus`, and the connect-cliff pair in `test_ha_component` rewritten to derive from `QueueCost`; **1 096** after the BUG-43 lot, eleven cases in `test_ha_component`; **1 107** after TEST-8's, which is ten cases plus the suite's own verdict line; **1 151** after TEST-9's three provider suites (41 cases and their three verdict lines) — **measured from a full run, not derived from the previous figure**, which is the only reason it is trustworthy: the same arithmetic done by hand disagreed with the run twice in the BUG-43 lot alone. **Run it with the generated `WebUIAssets.h` out of the tree** since TEST-8: a host suite that reaches `WebUI.h` compiles against a gitignored artefact a local WebUI build left behind, and passes here while failing on any clean checkout — which is how TEST-8's first CI run failed. **Count `[PASSED]` lines, not pio's "test cases" summary** — the two disagree, and the summary is what put a wrong figure in this lot's first commit message |
| ✅ | The three declared targets compile: `esp32dev`, `esp8266dev`, `esp32c3`, via the FullStack example, the only one pulling all twelve components |
| ✅ | `library.json` versions agree with `metadata.version` |
| ✅ | The install-from-GitHub path builds **both** declared platforms — the only thing in CI that resolves through the root `library.json` rather than `file://` paths (CI-8) |
| ⚠️ | **The eleven on-device builds compile in CI, and nothing runs them.** No runner has a board (CI-10). Run them by hand: `cd DomoticsCore-Storage && pio test -e esp8266dev`. Eight are ESP8266 — `DomoticsCore-RemoteConsole` is the newest, added with TEST-12; `DomoticsCore-Wifi` is the only one whose result depends on the runner being in radio range of something — and three are ESP32: `DomoticsCore-OTA` on an `esp32cam`, `DomoticsCore-RemoteConsole` and `DomoticsCore-HomeAssistant` on an `esp32dev`, the last two carrying suites the ESP8266 loop does not reach |
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

### SEC-4 — RemoteConsole: no brute-force protection on auth [MEDIUM] — **DONE (2026-09-14)**

- **Ref**: RC-F5
- **Problem**: Plain-text password comparison with no rate-limiting. Attackable via rapid telnet reconnections.
- **Fix, as filed**: Add exponential backoff after N failed attempts, or IP-based lockout.
- **Fixed 2026-09-14 — delay only, by the maintainer's rule: never block,
  never disconnect for failing.** The plan's first draft had a lockout and
  a disconnect on the third failure; its adversarial review showed a
  global lockout to be a denial-of-service lever any LAN host can hold
  with one wrong password a minute, and the maintainer ruled both out
  ("on doit jamais bloquer et juste ajouter du délai"). What ships: a
  failed `auth` sets a wait — 1 s, doubling per consecutive failure, capped
  by `authDelayMaxMs` (8 s; `0` disables) — before the next attempt from
  that **address** is *read*: the line is held in the client's state, not
  refused, and nothing else is read from that client until it has been
  evaluated. The wait is the address's, remembered in a table of four
  entries (LRU-evicted), so a reconnection inherits it and a second
  address is unaffected; one success or a minute of quiet clears it. With
  the 10 s `authTimeoutMs`, one connection gets about three attempts
  (0, 1, 3 s) before the existing timeout closes it — the rate limit the
  entry asked for, without a mechanism that can lock the owner out. The
  four per-client maps became one `ClientState` map on the way, so every
  erase site is one line (MEM-1's row for this file moves with it).
- **Verification, native**: the wait holds the next attempt for exactly
  one second, doubles and caps (1, 2, 4, 8, 8), survives a reconnection
  from the same address and not from another, is forgotten after a minute,
  is off at `0`, and the state map is empty after disconnects; the
  existing flow test now waits its second. Removal checks from a fresh
  `.pio`: the hold removed → the one-second test fails; the per-address
  carry removed → the reconnection test fails. The console suite gained
  the millis override (`setMillisForTest` in `setUp`). **The first run of
  the removal checks passed all four**: it compiled the stale libdeps
  copy of the header, the trap CLAUDE.md records; redone from a fresh
  `.pio`, each check went red as it should.
- **Measured, nodemcuv2** (`obs-lotb-probe` built with
  `-DDC_CONSOLE_PASSWORD`, `tools/on-device/console_auth_check.py`): on
  one connection three wrong passwords answered after **0.02 s, 1.03 s,
  2.00 s**, the connection still open; a fresh connection from the same
  address with the right password answered after **3.92 s** — the 4 s the
  third failure set, inherited; another fresh connection after that
  success answered in 0.02 s. Nothing refused, nothing cut.
- **Declined in the lot**: a constant-time password compare — a timing
  oracle over Telnet on Wi-Fi, behind the auth wait, is not the threat this
  entry closed.

### SEC-5 — WebUI: GET for state-changing operations [MEDIUM] — **DONE (2026-09-21, by the triage pass — closed by SEC-10 and never marked)**

- **Verified against the code**: `WebUI.h:509` registers `/api/ui/action` as
  `HTTP_POST` behind `checkCsrf()`. SEC-10 did what this entry asked for a
  release earlier; the heading was never updated.

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

### SEC-6 — WebUI: CORS `Access-Control-Allow-Origin: *` with auth enabled [MEDIUM] — **DONE (2026-09-14), the premise corrected**

> **The problem as filed is wrong on its mechanism, and the fix below is
> defence in depth, not the closing of an open door.** A browser never
> attaches automatic credentials (Basic/Digest, cookies) to a response
> whose origin is `*` — that needs `Access-Control-Allow-Credentials:
> true`, which nothing sends — and no route in the WebUI answers
> `OPTIONS`, so any non-simple cross-origin request (a hand-set
> `Authorization`, `X-DC-Token`, `X-API-Key`) fails its preflight with a
> `404` before it reaches a handler. What `*` exposes is what the owner
> asked for by setting `enableCORS`: unauthenticated reads of the WebUI's
> ten JSON routes, and nothing else — the flag never applied to `/`, the
> token route, the stream or routes other components register, so the
> reference row "on all responses" was wrong too, and the HeadlessAPI
> example promised a header its own routes do not send.

- **Ref**: WEB-F10
- **File**: `WebUI.h:437-443` (`addCorsHeaders`), `WebUIConfig.h:34`
- **Problem, as filed**: Any website can make authenticated API requests to the device (CSRF/data exfiltration). Especially dangerous when `enableAuth` is true.
- **Fix (2026-09-14)**: `addCorsHeaders()` emits nothing while
  `enableAuth` is on — the headers never enabled anything a browser would
  honour with credentials, and a protected device should not advertise
  `*` to a scanner or to the next reader of the code — with a WARN at the
  three places the pair can be set (`begin()`, `setConfig()`, the
  settings card's `enable_auth`). Both reference rows say the precedence;
  the HeadlessAPI README says which routes carry the header and that a
  custom header fails the preflight. The two dead headers (`-Methods`,
  `-Headers`) stay: removing them changes nothing a browser sees. An
  `allowedOrigin` with credentials, for a cross-origin dashboard against
  an authenticated device, was declined — a behaviour change nobody has
  asked for — and is not built.
- **Measured, WROOM-32D** (FullStack built with `-DDC_WEBUI_CORS=1`;
  `curl -i`, and Chromium through Playwright from a page on another
  origin): **auth on** — no `Access-Control-Allow-Origin` on
  `/api/system/info`, and the browser's three fetches (simple,
  `credentials: 'include'`, a hand-set `Authorization`) all fail; **auth
  off** — `Access-Control-Allow-Origin: *` on `/api/system/info`, none on
  `/api/ntp/timezones` (a `registerApiRoute()` route) nor on
  `/api/ui/token`, the browser's simple fetch reads the JSON, and
  **`credentials: 'include'` still fails against `*`** — the premise the
  entry was filed on, refuted on silicon — as does the hand-set
  `Authorization` (no `OPTIONS` handler, so the preflight is a `404`).
  Removal check: with the `if` removed, the header is back under auth.

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
- **Arbitrated 2026-09-21 (architect brief, maintainer validated)**: add a
  `triggerUpdateFromUrl(url, sha256, force)` overload and an
  `OTAConfig::requireDownloadHash` defaulting to `false`. The reason is not the
  flag but the overload — `beginUpload()` takes an expected digest and the URL
  path takes none, `installFromUrl` is private, and the hash is computed inside
  the component, so a caller who *wants* a verified URL install has no API for
  it today. Default-off means it is a capability, not a mitigation, and the
  entry must say so. Refusing every empty digest outright was refused as a
  registry-visible break for a MEDIUM. **SEC-16 changes the stakes**: without an
  auth gate on the trigger route, an unverified URL stops being a corruption
  risk and becomes arbitrary firmware.
- **Filed, not fixed.** Requiring a digest for a URL install is a behaviour change
  for legitimate users and needs a decision about where the hash comes from (a
  manifest).

### SEC-13 — WebUI: the SSE stream and `/api/system/info` ignore `enableAuth` [MEDIUM] — **DONE (2026-09-14, Lot D)**

- **File**: `WebUI.h` (`/api/system/info`, `:532`), `WebSocketHandler.h` (the
  `AsyncEventSource` created at `:66`, added at `:72` with nothing between it
  and the network)
- **Problem**: `/api/system/info` has no auth gate at all, and the SSE source is
  added without `setAuthentication`/an auth middleware, so both serve an
  unauthenticated client full live state even when `enableAuth` is on.
- **Fix (Lot D)**: the info route takes the nine-sibling idiom. The stream
  takes an `AsyncMiddlewareFunction` (`sseSource->addMiddleware`) that calls
  the WebUI's gate **live** — not `setAuthentication(user, pass)`, which
  copies the credentials at `begin()` and misses a password changed through
  `webui_settings`, and not `setFilter()`, which turns a refusal into a 404 the
  browser never re-prompts on. The gate itself, `authenticate()`, was private
  (OTAWebUI inlined a copy for that reason) and is now public as
  `WebUIComponent::authorize()`; OTAWebUI's three copies call it. **Found on
  the way, fixed with it**: `WebServerManager` gated `/` on its own copy of
  `enableAuth` taken at construction *and* the live handler, so a runtime
  `enable_auth` flip left the page on the old value while every API route
  followed the new one — the first admin to enable auth from the settings
  card would have locked the page they were on. `/` now gates on the live
  handler alone.
- **Measured on the WROOM-32D** (`tools/on-device/webui_auth_check.py`), auth
  enabled at runtime through `webui_settings` without a reboot: before the
  fix `/api/system/info` and the stream read `200` to nobody; after,
  `401` without credentials and `200` with, the stream flowing events
  (`/api/ui/updates` and `/api/components` read the same pair, the reference
  the fix had to match). **The stream is only reached with
  `Accept: text/event-stream`**: without the header the handler never matches
  and the server answers `404` whether or not a gate exists, which is why a
  bare `curl -N` reading of the route proves nothing and the script prints
  that reading separately. **The browser side, measured in Chromium**
  (Playwright, the harness `webui_check.py` already uses): `app.js` falls
  back to polling on a closed `EventSource` without a word, so the
  discriminating number is `/api/system/info`'s `clients`. With auth on and
  the page opened with credentials, the console read `Connecting SSE to
  /api/ui/events` then `SSE connected — stopping polling`, and
  `/api/system/info` reported `clients: 1`. Firefox, whose Digest
  preemption differs, was not run; recorded as owed. The page `/` itself is
  in the script's route list, since its gate now reads the live
  configuration.
- **Residual, recorded not fixed**: the other reads with no gate —
  `GET`+`POST /api/ota/unified` and `/api/ota/status` (OTAWebUI),
  `/api/ntp/timezones` (was registered twice — DC-16, fixed on the same
  branch), `/api/console/loglevels`, and the static
  `/style.css`/`/app.js` (defensible). `/api/ota/check` and `/api/ota/update`
  honour the token and not `enableAuth`, transitively gated through
  `/api/ui/token`. The WebUI README's route table now carries an auth column
  from this sweep. `WebSocketHandler` and `WebServerManager` still hold config
  copies for the fields other than `enableAuth`.

### SEC-14 — WebUI: authentication can be enabled with an empty password [MEDIUM] — **DONE (2026-09-14)**

- **File**: `WebUI.h:374` — `handleWebUIRequest`, `webui_settings`
- **Problem**: the password setter skips an empty value (`if (value.length() > 0)`),
  so `enable_auth=true` with no password yields a device that looks protected and
  accepts `admin` with an empty password. And an attacker who can set credentials
  then enable auth (pre-SEC-10) could lock the owner out.
- **Filed, not fixed** — SEC-10 closes the unauthenticated route to it; the
  empty-password guard itself is a separate one-line refusal, deferred.
- **Fixed 2026-09-14, in a lot with SEC-4 and SEC-6, after the plan's
  adversarial review found three sites where the plan had assumed one.**
  `handleWebUIRequest` writes `config` directly and never goes through
  `setConfig()`; the persistence load path reaches `setConfig()` *after*
  `core.begin()`; and the console had the same defect — with `requireAuth`
  and an empty password a bare `auth` line matched, `"" == ""`. So the rule
  lives on `WebUIConfig`, where the native suite reaches it:
  `applySetting(field, value, error)` is the settings card's field switch,
  and it refuses `enable_auth` before a password is set, an empty password
  while auth is on, and no longer answers `success:true` to an empty
  password while auth is off; `normalizeAuth()` clears `enableAuth` when
  the password is empty and is called by `setConfig()` and `begin()` with
  a WARN — the stored field is written, not a derived predicate, so
  OTAWebUI's three inlined checks, the settings card and the persistence
  callback all read the same answer. The decision on the load path:
  **disable and warn**, once per boot; "refuse the config" is the same
  thing there (the previous config is the constructor default), and
  "fail closed" bricks a device with no other way to set a password. The
  next settings save then persists the cleared flag — the reference says
  so. The page reverts a toggle the device refused (`app.js` left it drawn
  as accepted, with an `alert`). The console: `begin()` clears
  `requireAuth` with a WARN when the password is empty, and an empty
  `auth` line never authenticates; `SystemConfig::consolePassword` is new,
  so a System user can turn console authentication on at all.
- **Verification, native**: `test_webui_component` — the four refusals
  and their strings, the other fields applied, `normalizeAuth()`;
  `test_remoteconsole_empty_password` rewritten from "constructs" to the
  protocol: the WARN at `begin()`, `auth` answered "not required", `heap`
  open. Removal checks from a fresh `.pio` (the first run of them
  compiled a stale libdeps copy and passed all four — the trap CLAUDE.md
  records, met again): the WebUI refusal removed → `enable_auth` test
  fails; the console guards removed → the empty-password test fails.
- **Measured, WROOM-32D (FullStack)**, red then green. The NVS erased
  for an empty stored password (which also erased the LAN credentials the
  board had lived on — FullStack reads `secrets.h` only with the
  repository root on the include path, recorded in `tools/on-device`).
  **Unfixed** (`main`): `enable_auth=true` with no password →
  `{"success":true}`; `/api/system/info` **401** without credentials and
  **200 with `admin` and an empty password**; `webui_auth = true` stored,
  no `webui_pass`. **Fixed**, same NVS: boot line `enableAuth with an
  empty password: authentication disabled` from the load path, `200`
  without credentials; then `enable_auth=true` → `Set a password before
  enabling authentication`, `password=…` then `enable_auth=true` →
  `401`/`200`, `password=""` while enabled → `Authentication is enabled;
  the password cannot be empty` and still `401`, `enable_auth=false` →
  `200`. The page reverts a refused toggle (`app.js`).

### SEC-17 — OTA: the read routes serve device state to an unauthenticated client [MEDIUM] — **DONE (2026-09-22, filed the same day)**

- **Files**: `OTAWebUI.h:234-277` (`/api/ota/unified` GET and POST,
  `/api/ota/status`), and the no-parameter branch of `/api/ota/update`.
- **Measured, auth on, no credentials, no token**: `/api/ota/status` and
  `/api/ota/unified` answer `200` with the component's full state —
  `state`, `lastResult`, `lastVersion`, `update_url`, `autoReboot` — while
  `/api/ui/token` answers `401` in the same run.
- **The same class as SEC-13**, which gated `/api/system/info` and the SSE
  stream for this reason and left these three untouched: an unauthenticated
  reader learns the configured firmware URL, the running version and whether
  an update is in flight.
- **MEDIUM by parity with SEC-13.** It reads; it changes nothing, which is
  what separates it from SEC-16.
- **The question the entry posed, answered by reading `app.js`**: nothing in
  the page fetches these routes — the OTA card is drawn from `/api/ui/updates`,
  which is gated — so the comment claiming the no-parameter branch is the
  card's own poll described a caller that does not exist.
- **Fix**: one gate per read handler, and on `/api/ota/update` a single gate
  above both branches, which is also SEC-16's for that route. The CSRF check
  stays where a state change is.
- **Measured**: natively, the three reads challenged without credentials and
  answered with them, every route still open with `enableAuth` off (17 → 19
  cases in `test_ota_http` — one case the gate made wrong was rewritten as two —
  two of them red with the gates removed). On the
  WROOM-32D `ota_trigger_auth_check.py` now reads **2/2 triggers and 3/3 reads
  refused** without credentials, `/api/ota/status` answering `200` with them.
- **Refs**: SEC-13 (the shape and the `authorize()` gate), SEC-16 (the
  state-changing half of the same table), DC-14 (the declared endpoints with no
  caller).

### SEC-16 — OTA: the two state-changing routes are gated by CSRF alone, not by authentication [HIGH] — **DONE (2026-09-22, filed 2026-09-21)**

- **Files**: `OTAWebUI.h:280-291` (`/api/ota/check`), `:293-334`
  (`/api/ota/update`), against `WebUI.h:224-226` (`registerApiRoute`, which
  applies no gate of its own) and `:249` (`authorize()`, public since SEC-13
  precisely so siblings stop copying it).
- **Measured, auth on, no credentials, with a CSRF token taken before auth was
  enabled**: `GET /api/ui/token` → 401, `POST /api/ui/action` → 401,
  **`POST /api/ota/update` → 200 `{"success":true}`**, and the component moved
  (`[OTA] State -> error | No downloader set`). The attacker's URL is read: the
  handler only takes its action branch when a `url`/`force`/`action` parameter
  is present in the body.
- **The bypass is unconditional; its worst case is not.** Nothing downloaded
  here because FullStack wires no downloader — an application that uses the
  documented URL update does, and SEC-12 leaves that download unverified. So
  uploading firmware needs the password while telling the device to fetch some
  does not.
- **The token is the whole credential and it is weak as one**: per boot, no
  expiry, served over plain HTTP on port 80 and accepted from a query string,
  so anyone who observes one keeps it until the next reboot.
- **HIGH**: authentication bypass on the most dangerous operation the device
  offers. The first HIGH since 2026-09-01, and graded from the demonstration
  rather than from reading.
- **Fix**: `authorize()` after each `checkCsrf`, which is what the upload route
  next door already does — five lines each, and the read branch of
  `/api/ota/update` left open, since the OTA card polls it once a second.
- **Verified natively** (`test_ota_http`, 12 → 17 cases): a tokened request
  without credentials is challenged and, after `loop()`, the component has not
  moved; with credentials the same request reaches the install path and stops
  at `No downloader set`; the card's poll is never challenged. Removal check:
  the two discriminating cases go red on the challenge assertion with the gates
  taken out, the other three stay green.
- **Verified on the WROOM-32D** (`tools/on-device/ota_trigger_auth_check.py`,
  auth enabled at runtime, token taken before it was): **before, 0 of 2 routes
  refused** — two `200`s and six `[OTA] State ->` lines on the serial, three of
  them from the unauthenticated pair; **after, 2 of 2 refused** with `401`, and
  the serial carries only the three transitions of the credentialed pair. The
  script prints `/api/ui/token` without credentials as its own witness, or a
  run against a device whose auth never came on would read as a pass.
- **Refs**: SEC-13 (which made `authorize()` public and claims OTAWebUI's copies
  call it — true for three routes, false for these two), SEC-12 (which composes
  with it), SEC-10 (which added the CSRF gate these two do have).

### SEC-15 — WebUI: HTTP authentication has no brute-force delay [MEDIUM] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: the SEC-4 / SEC-6 / SEC-14 lot, 2026-09-14.
- **File**: `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h`, `authorize()`.
- **Problem**: ESPAsyncWebServer answers the Basic-auth challenge itself, so a
  wrong password costs the caller nothing and the next attempt is read at
  once. SEC-4 closed the same absence on the console; the WebUI's password is
  the same kind of secret over the same LAN.
- **MEDIUM by parity with SEC-4**, which was graded for the same attack.
- **Fix**: a wait per address in `authorize()`, doubling and capped, under the
  maintainer's rule — delay, never block, never disconnect.
- **Refs**: SEC-4 (the shape), SEC-13 (`authorize()` as the one gate), DC-19
  (the three OTA copies the wait would not cover).

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
| `RemoteConsole.h` | `clients` vector and the `clientState` map (one record per client since SEC-4's lot; it was four maps) | `erase()` on disconnect |
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
  checkpoint node to the window that follows it. The first became BUG-36;
  the second is LO-33.

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
- **The measurement, shipped as a suite — and run at last on 2026-08-31, 3/3
  on a `nodemcuv2`, by TEST-4's closing lot.** (This bullet said "not yet run"
  for the two days in between.)
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
- **Three more were parked, and two of them bear on this lot's own
  justification** (BUG-47 since 2026-09-20). `WifiComponent::loop()` returns early whenever the SSID is
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
  neither done nor argued (MEM-7 since 2026-09-20).
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
  has built its summary (`pollScanCompletion()`, and since BUG-47 also in
  `shutdown()`). The synchronous `scanNetworks()` does
  not, so the SDK's scan-result list — every SSID, BSSID, channel and RSSI it
  found — stays allocated after the function returns, until the next scan
  replaces it or something else deletes it. That is a larger and far
  longer-lived cost than the per-entry copy this lot removed from the same
  function, which is the uncomfortable part: the loop was optimised and the
  allocation next to it was not.
- **And the async path has one left**: the `-2` branch releases the flag without
  calling `scanDelete()`, where the success branch does. Noted by BUG-47's
  review and left here rather than changed blind, for the same reason as below —
  on an ESP32 a timed-out scan may still deliver its list afterwards.
- **Why it is not fixed here**: adding `scanDelete()` is a behaviour change, not
  a memory fix. Anything that calls `getScannedSSID()` after `scanNetworks()`
  returns — legitimate, since both are public HAL surface — would start reading
  a freed list. Deciding that needs a look at what users do with the HAL, and
  `Wifi.h` is being rewritten on `esp32-ethernet` in any case.
- **Fix, when it is decided**: either delete inside `scanNetworks()`, since it
  has already copied everything it needs into the caller's vector, or document
  the ownership and give the component an explicit release. Not both.

### MEM-7 — ESP8266: string literals link into DRAM, and nothing moves them [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: MEM-2's lot (2026-08-29) and OBS Lot D (2026-09-14).
- **Files**: `RemoteConsole.h` (`HELP_TEXT`, 427 bytes); the entity table and
  the two JSON formats Lot D added to System (~1 KB); every non-`PROGMEM`
  literal like them.
- **Problem**: the ESP8266 core places `.rodata` in DRAM, so a `static const
  char[]` costs RAM for the life of the process — Lot D measured **+1 216 B**
  on `esp8266dev` against +8 B on either ESP32. `PSTR`/`FPSTR`/`F()` are the
  lever; their portability across the ESP32 core, where they are no-ops, is
  the question that decides the shape: a HAL macro, not an `#ifdef` in the
  caller (Constitution IX).
- **Not universal, measured 2026-09-20**: BUG-51's lot added ~700 B of refusal
  strings across seven headers and moved DRAM by **160 B** against +1 568 B of
  flash. In that ELF the same literal sits once in `.rodata` (0x3ffe86d0, DRAM)
  and eight times in `.irom0.text` (0x40201010, flash), so where a literal lands
  is per translation unit. Whatever puts one in DRAM is the thing to find first;
  a sweep of every literal would mostly move bytes that are already in flash.
- **Fix**: one HAL macro, then the sites, RAM measured on the nodemcuv2.
- **Refs**: MEM-2, OBS-5, BUG-51 (the measurement above).

### MEM-8 — Core: what the EventBus byte budget does not bound [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-41's lot and its code review, 2026-09-18.
- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`.
- **Three things sit outside `QueueCost::kBudgetBytes`**:
  1. the sticky store — `publishSticky` keeps the last payload per topic in
     `lastByTopic`, its payload-less overload clears the vector and keeps the
     key, and only `reset()` erases the map, so it grows with every distinct
     sticky topic ever published; a caller composing topic names at run time
     grows it without limit. Evicting changes what a late subscriber replays,
     which is why BUG-41 left it (ADR 0003).
  2. `pendingByTopic`'s map nodes and their `String` keys, while the entry
     guard rail went from 32 to 256: a burst of small events on distinct
     topics holds eight times as many of them as before, outside the budget
     that exists to bound exactly that.
  3. the budget itself, when `-DDOMOTICS_EVENTBUS_QUEUE_BYTES` is passed
     through `build_src_flags` rather than `build_flags`: `enqueue()` is
     inline, the application's translation units compile against one budget
     and the library's against another, and the linker picks one silently.
     Nothing refuses that spelling, and the reference does not say where the
     flag goes.
- **Fix**: count the two maps or state the bound they have; name
  `build_flags` in the reference and in the header.
- **Refs**: BUG-41, ADR 0003, TEST-10.

---

## Priority 3: Code Safety & Critical Bugs

### BUG-1 — Core: `reinterpret_cast` without trivially_copyable check [HIGH]

- **Ref**: CORE-F4
- **File**: `EventBus.h:101-107`
- **Problem**: `publish()` template uses `reinterpret_cast` to byte-copy payloads. Publishing a `std::string` or `String` results in dangling pointers in the event queue.
- **Fix**: Add `static_assert(std::is_trivially_copyable<PayloadT>::value, ...)`.

### BUG-2 — Core: `getComponent<T>()` promises a safety it does not provide [MEDIUM] — **DONE (2026-08-28 by argument; the marker is from 2026-09-21)**

- **Closed by narrowing the contract, not the code**, and the heading never
  said so. Both remedies the entry recommended were measured and found
  impossible — `dynamic_cast` does not compile under `-fno-rtti` on either
  platform, and `getTypeKey()` is overridden by two components of twelve.
  This item was never counted in the tracking summary either, so the marker
  moves no figure; it stops the next sweep from finding it again.

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

### BUG-3 — Core: `removeCallback()` nukes ALL callbacks [MEDIUM] — **DONE (2026-09-21, by the triage pass — fixed earlier and never marked)**

- **Verified against the code**: `Logger.h:33-37` erases by id with
  `remove_if` and then `shrink_to_fit()`. The `clear()` the entry describes
  is gone.

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

### BUG-13 — HA: `HA_TOPIC_BUF_SIZE` too small [MEDIUM] — **DONE (2026-09-23, in BUG-63's lot)**

- **Ref**: HA-F8
- **Problem**: 128-byte buffer can be exceeded with long discovery prefixes + `alarm_control_panel` component + long entity IDs. Silent truncation breaks HA discovery.
- **Measured** on the native harness: a 32-character prefix and node id with a
  35-character entity id give a 128-character topic, cut to 127 — the config
  lands on `…/confi` and Home Assistant never reads it. A 40-character id loses
  `/config` entirely. Two ids sharing a prefix cut to the *same* state topic and
  overwrite each other.
- **Both recorded fixes were wrong, and the second would have been inert.**
  `mqttPublish()` copies the topic into `MQTTPublishEvent::topic`, which is 128
  bytes, with a `strncpy` that cuts in silence — while the payload over the same
  kind of ceiling has been refused aloud since OBS Lot D. Raising the component's
  buffer to 256 would only have moved the cut one step later, into MQTT's event
  field; raising *that* is MQTT's public event ABI and the EventBus byte budget.
- **Fixed**: the topic builders return the length the topic needed, the buffer
  stays 128 by deliberate alignment with the field it crosses, and the three
  publish sites refuse a topic that does not fit — one warning naming the entity
  and the length, counted in `discoveryRefused`, symmetric with the payload
  refusal. No stack cost, no static RAM.
- **The first version of the test was vacuous** and its removal check said so: on
  an alarm panel with a 32-character prefix and node, the *payload* guard fires
  first (788 characters against 699), so nothing was published for a reason that
  had nothing to do with the topic. Rewritten on a sensor, whose document stays
  far under the payload ceiling, the removal check goes red on the publish itself.
- **The adversarial review found the regression the first version introduced.**
  The held-state store keeps a refused slot and retries it on the next loop,
  which is right for a broker that is away and wrong for a topic that will never
  fit: one such state at the head of the queue would have stalled every value
  behind it for the life of the process. The store now refuses that state on the
  way in and drops it at the flush, counted in `statesRefused` like every other
  refused state; the same pass added the counter on the live path, the state
  topic to the discovery check (a `stateTopicOverride` longer than the field made
  the document advertise a cut topic), the availability topic, a negative
  `snprintf` return, and a `static_assert` that the buffer and the field it
  crosses stay one size.

### BUG-14 — Storage: `putBlob()` nullptr dereference [HIGH]

- **Ref**: STOR-F3
- **Problem**: No null check on `data` pointer. `nullptr` with `length > 0` = crash.
- **Fix**: Add null guard at entry point.

### BUG-15 — Storage: cache never consulted by getters [HIGH]

- **Ref**: STOR-F4
- **Problem**: `getString()`, `getInt()`, etc. always go to HAL storage, bypassing the cache entirely. Cache only used for dirty-checking in `put*()`.
- **Fix**: Check cache first, fall through to HAL on miss.

### BUG-16 — Storage: RAMOnlyStorage truncates uint64 [MEDIUM] — **DONE (2026-09-21, by the triage pass — fixed earlier and never marked)**

- **Verified against the code**: `Storage_Stub.h:117-119` formats with
  `snprintf("%llu")` into a 21-byte buffer. The fix even carries this id in a
  comment, which is how long it went unmarked.

- **Ref**: STOR-F6
- **Problem**: `putULong64` stores via `String((unsigned long)value)`, losing upper 32 bits.
- **Fix**: Use proper uint64 serialization.

### BUG-17 — Storage: RAMOnlyStorage putBytes discards data [MEDIUM] — **DONE (2026-09-21, by the triage pass — fixed earlier and never marked)**

- **Verified against the code**: `Storage_Stub.h:134-148` hex-encodes the
  bytes behind a prefix, and `getBytes` decodes them back.

- **Ref**: STOR-F7
- **Problem**: Only stores the length, not the actual byte data. `getBytes` always returns 0.
- **Fix**: Store actual bytes (e.g., base64-encoded or raw vector).

### BUG-18 — LED: phase calculation loses accumulated time [MEDIUM] — **DONE (2026-09-21, by the triage pass — fixed earlier and never marked)**

- **Verified against the code**: `LED.h:523-524` wraps with
  `fmod(state.effectPhase, 1.0f)`, which is the fix this entry asked for.

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

### BUG-24 — WiFi: dead config fields [MEDIUM] — **FOLDED into DC-15 (2026-09-21, by the triage pass)**

- **The same defect, filed twice.** `WifiConfig::connectionTimeout`
  (`Wifi.h:50`) is the field, `CONNECTION_TIMEOUT` (`:103`) the constant the
  code actually reads (`:275`), and `:624` copies the constant back into the
  field so a reader sees agreement. DC-15 describes that exact pair, with
  `reconnectInterval` beside it, and holds the decision.

- **Ref**: WIFI-F6
- **Problem**: `connectionTimeout` and `CONNECTION_TIMEOUT` static const are both declared. `WifiConfig::connectionTimeout` may shadow the static constant, creating confusion.
- **Fix**: Remove one, standardize on a single timeout source.

### BUG-25 — WebUI: blocking component lifecycle in HTTP handler [MEDIUM]

- **Ref**: WEB-F11
- **File**: `ProviderRegistry.h:204-263`
- **Problem**: `enableComponent()` calls `component->shutdown()` or `begin()` directly inside an async HTTP handler. Blocking I/O in ESPAsyncWebServer context causes watchdog resets.
- **Fix**: Queue enable/disable action, process in `loop()`.

### BUG-26 — WebUI: OptionLabelPair serialization bug [MEDIUM] — **DONE (2026-08-31, fixed 2026-07-16)**

- **Ref**: WEB-F12
- **File**: `StreamingContextSerializer.h:849-879` (as filed)
- **Problem (as filed)**: Key+colon+value written in one iteration. Partial value writes with small buffer cause malformed JSON because `optionIndex` not incremented but key already consumed.
- **The row was stale at filing.** The recommended fix — split key, colon and
  value into separate states — had already shipped in marianorenzi's
  `dc8886f1` (2026-07-16, "Fixed split elements not being resumable"), which
  also brought the test that pins it,
  `test_option_labels_across_chunk_boundaries` (chunk sizes 2–32). SIZE-2's
  lot found this while auditing the file it was splitting: the states
  `OptionLabelKey`/`OptionLabelColon`/`OptionLabelValue` were already
  separate, and the sweep already ran green. Same shape as TEST-6's
  LED-F14 — an item describing code that no longer existed by the time the
  sweep filed it. Closed on that evidence; nothing in this lot changed the
  option-label states beyond the mechanical `arrayIndex` rename.

### BUG-28 — WebUI: Multiselect value resumes from a destroyed `String` [MEDIUM] — **DONE (2026-08-31)**

- **Closed by SIZE-2's lot, with marianorenzi's own design.** His
  `esp32-ethernet` branch had already replaced the block this entry blames —
  `JsonDocument` + a case-local `String`, rebuilt on every resume — with
  streaming array states; the lot adopted them (credited, adapted to the
  current field API), so the destroyed-`String` resume and the indeterminate
  pointer comparison no longer exist to go wrong. The test this entry said
  the fix needs — one that forces a chunk boundary inside a multiselect
  value — exists twice now: `test_multiselect_across_chunk_boundaries`
  (chunk sizes 2–32, byte-identical against an unsplit reference) and the
  maximal-context sweep. **Measured before the fix, the probe stayed green**:
  on the native allocator the rebuilt temporary lands on the same address
  every resume, so the UB's usual outcome was the accidental correctness
  this entry predicted, not the malformed JSON — which is why no dashboard
  ever showed the failure and why the entry's own "correct only by accident"
  was the accurate half.
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


### BUG-31 — HomeAssistant: the settings handler reads parameters nothing sends, and writes flash on every request [HIGH] — **DONE (2026-08-29)**

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h:145-179`
  (before the fix).
- **Found by**: TEST-6, 2026-08-29, while establishing what the four untested
  WebUI providers actually do before writing tests over them. Writing the tests
  first would have encoded this as expected behaviour.
- **Problem**: the handler read six parameters by their own names — `node_id`,
  `device_name`, `manufacturer`, `model`, `discovery_prefix`, `suggested_area` —
  at `:150,154,157,160,163,166`. The framework's only dispatcher
  (`WebUI.h:865-876`) builds exactly two entries, `field` and `value`, for both
  the HTTP route and the WebSocket action callback. **No read could hit**, so no
  Home Assistant setting could be saved from the UI at all. Verified against the
  other nine providers: `HomeAssistantWebUI` was the only one reading other
  names.
- **And the miss was not silent.** Every request still ran `ha->setConfig(newCfg)`
  with `newCfg` identical to the current config (`:168`), the persistence callback
  (`:172`) — three `putString` calls to flash at `SystemWebUISetup.h:383-385` —
  and `ha->publishDiscovery()` (`:176`), then answered
  `{"success":true,"message":"Configuration updated and discovery republished"}`.
  A browser was told the save worked; what actually happened was a flash write and
  an MQTT discovery republish carrying the unchanged identity.
- **Why HIGH, argued rather than inherited.** Two independent counts. It is a
  user-facing feature that cannot work at all, on a settings card the WebUI
  advertises. And the side effects are reachable by anyone on the network at the
  caller's rate: the route is registered **`HTTP_GET`** (`WebUI.h:544`) — the
  string `"POST"` the handler checked is fabricated by the dispatcher at `:871`
  — and `enableAuth` defaults to **false** (`WebUIConfig.h:27`), so
  `<img src="http://device/api/ui/action?contextId=ha_settings&field=x&value=y">`
  on any page a user opens needs no credentials and no CORS cooperation. Flash
  endurance is finite. It opens and shuts in this lot, so the HIGH column does not
  move.
- **Fixed (2026-08-29)**: the handler follows the convention every other provider
  uses (`LEDWebUI.h:116-200`) — one `field`/`value` pair, method and context
  checked, unknown fields refused, per-field application, **and mutation only when
  the value actually changed**. The comparison happens *after* `HA::setField`, so
  an over-long value that truncates to what is already stored counts as unchanged.
- **Refusals carry no `error` key.** `app.js` inspects only `data.error`, so an
  error key pops a modal alert where a silent refusal is meant. The old handler
  returned `{"error":"Unsupported operation"}` and `{"error":"Component not
  available"}`; both are now `{"success":false}`.
- **Making `node_id` settable would have broken the thing it names.**
  `setConfig` (`HomeAssistant.h:465-475`) regenerates `availabilityTopic` only
  when it is empty, so a changed node id would have left the device publishing
  availability on the previous node's topic — a regression the fix would have
  introduced rather than a defect it found. The handler now clears the topic
  before `setConfig` when `node_id` or `discovery_prefix` moved, **and only when
  the stored topic is the generated one**: a topic somebody set deliberately is
  not the handler's to rewrite, which is the contract
  `test_ha_component.cpp:126-135` already holds `setConfig` to.
- **The provider could not be compiled by any native test, and that is why it had
  none.** `HomeAssistantWebUI.h:5` included `WebUI.h`, which includes
  `<ESPAsyncWebServer.h>` (`WebUI.h:11`) — a header present nowhere in the
  repository or the toolchain. The file already included `IWebUIProvider.h`, which
  carries everything it uses, so the line was one too many rather than one
  missing. Dropped. The four sibling providers with the same over-include are
  **TEST-9**.
- **Tests**: `DomoticsCore-HomeAssistant/test/test_ha_webui/` — 20 cases, and
  `DomoticsCore-HomeAssistant/platformio.ini` gains the WebUI include path and
  `file://` dependency, with `ESPAsyncWebServer, AsyncTCP, ESP Async WebServer`
  appended to `lib_ignore` (the pattern at `DomoticsCore-LED/platformio.ini:7`).
  The `esp32dev` environment ignores the new suite: it does not depend on WebUI,
  and the provider is compiled for all three targets by the FullStack example
  through `SystemWebUISetup.h:377`.
- **The open question the design left, settled by running it**: yes,
  `getStatistics().discoveryCount` increments natively with no MQTT.
  `publishDiscovery()` increments it before any publish, and the publish path goes
  through `emit()`, which returns immediately without an EventBus
  (`IComponent.h:223-227`). `test_the_discovery_counter_is_a_usable_observable`
  pins that, because the rest of the suite rests on it. Flash itself is not
  observable natively — the `putString` calls live in a lambda no native project
  compiles — so the tests observe the `onConfigSaved` callback instead.
- **Removal check, run rather than reasoned**: the whole suite against the
  unfixed handler (the original file with only the include line removed, from a
  cleaned `.pio`) — **18 of 20 fail**. The two that pass are marked in the file as
  guards, not evidence: the discovery-counter observable, which tests the
  component; and "changing the model leaves the availability topic alone", which
  passes because the unfixed handler changes nothing at all. Three tests that
  would have passed on outcome — non-POST, wrong context, null component — fail
  only because they also assert the absence of the `error` key. That is the
  difference between a guard and a test.
- **Verified**: `DomoticsCore-HomeAssistant` native, cleaned `.pio` — six suites,
  **111 cases green**, the device suite still ignored. `pio test -e esp8266dev
  --without-uploading --without-testing` compiles the device suite alone.
  FullStack on `esp8266dev`: RAM 50808 → **50660** bytes, flash 716967 →
  **716787** — the fix is smaller than the code it replaces, six `find()` calls
  and a long success message being more than a switch. **No board run in this
  lot**: every claim here is behavioural and native.
- **Three residuals this fix does not bound**, all recorded rather than implied:
  1. **A caller that varies the value.** The change-guard removes the idempotent
     repeat, not the caller. A script alternating `node_id=a` / `node_id=b` still
     costs one `setConfig`, three `putString` and one `publishDiscovery` per
     request. Rate-limiting `/api/ui/action` is not attempted here.
  2. **Normal use now costs more, not less.** `app.js` sends one request per
     changed field (`applySave` iterates `card.dataset.pending`) and the
     persistence callback writes all three keys every time, so a user changing six
     fields gets **six republishes and eighteen `putString`**. Coalescing the
     callback to the next `loop()` — the pattern `WifiWebUI.h:157,165,209` already
     uses — is the proposal, and it is not done here.
  3. **A degenerate value is accepted.** `HA::setField` is the only validation, so
     `field=node_id&value=` (empty) counts as a change, blanks the node id, and
     regenerates `availabilityTopic` as `homeassistant//availability` with an empty
     MQTT client id; `value=a/b` yields `homeassistant/a/b/availability`. The
     handler answers `{"success":true}`. This is now page-only — SEC-10's token
     gates the route, so it is a footgun for the owner, not an attacker vector —
     and charset/emptiness validation on `node_id`/`discovery_prefix` is deferred,
     not done here. No test in the suite pins it; the twin custom-availability-topic
     survives-a-change assertion is likewise present for `node_id` and not for
     `discovery_prefix`, though the code path is the same flag.
- **Design**: `_bmad-output/implementation-artifacts/spec-bug-31-ha-settings-handler.md`,
  from `_bmad-output/specs/spec-test-6-webui-provider-inputs/`.


### BUG-32 — WebUI: the device name is interpolated raw into every update [MEDIUM] — **DONE (2026-08-31)**

- **Filed and fixed**: 2026-08-31, TEST-6's Lot B.
- **File**: `WebUI.h` `buildUpdateJson` (the sink), `SystemInfoWebUI.h:117` (one source).
- **Problem**: `device_name` was interpolated with a raw `%s` into a JSON string
  literal, so a `"` or `\` corrupted every WebSocket and polling update for every
  client, persistently after the next reboot; a name shaped `","injected":"`
  inserted keys into a payload the browser parses. The name reaches that sink from
  the WebUI settings handler, from `SystemConfig` and from persistence, so the fix
  had to be at the sink, not at one handler.
- **Severity MEDIUM, argued.** It corrupts the whole UI, but only for a name
  containing a quote or backslash — not a default, and after SEC-10 the route is
  token-gated, so it is a footgun for the owner rather than an attacker vector. It
  opens and shuts inside this lot, so no severity column moves for it.
- **Fix**: a stateless `jsonEscape` in `WebUI/JsonEscape.h` (replacing the dead
  `printJsonEscaped` nothing called), applied by `buildSystemHeader` in
  `WebUI/SystemHeader.h` — the header step extracted from the private
  `buildUpdateJson` so a native test can reach it, which is also the first slice of
  SIZE-1. `SystemInfoWebUI` gained a 31-char cap, an empty-name refusal and a null
  guard as a second, separate measure. The escaper never emits a partial escape
  sequence; the worst case is a `char[32]` name at 31×6+1 = 188 bytes.
- **Verified natively**, with removal checks: `test_system_header` (8 tests, five
  red against the raw-`%s` builder), `test_systeminfo_webui` (8 tests; cap and
  empty go red, the null test SEGFAULTs without its guard), `test_storage_webui`
  (4 coverage tests), and the persisted-quote round trip split across
  `test_system_persistence` and `test_system_header`. The ESP8266 crowding — up to
  ~155 escape bytes pushing contexts out of a 1024-byte update — is pinned as a
  header-length cost natively and recorded as an owed board observation.
- **Design**: `_bmad-output/specs/spec-test-6-webui-provider-inputs/` (SPEC.md,
  companion providers.md).

### BUG-35 — OTA: a client disconnect mid-upload locks OTA out until a power-cycle [HIGH] — **DONE (2026-09-01, same day it was filed)**

- **Filed**: 2026-09-01, by the second real-conditions campaign — found by an
  accident, kept because it reproduces by construction: an upload of the
  FullStack image to the WROOM-32D died with a broken pipe at 12%, and the
  device never recovered.
- **Measured, on silicon**: after the disconnect, `/api/ota/status` reports
  `state=downloading, progress=12.09` **frozen — still identical 75 seconds
  later** — and every subsequent upload, token and hash correct, is refused
  with `"Upload already in progress"`. Only a reboot clears it. (The same
  boot then accepted a clean commit end-to-end: upload, install, reboot,
  back `idle` — the path itself is healthy.)
- **Mechanism, located**: the upload handler in `OTAWebUI.h` calls
  `ota->abortUpload(...)` on CSRF and auth failures but registers **no
  `request->onDisconnect`** — when the client vanishes, nothing aborts the
  open update. SEC-8 taught the *test* suites to release an open update in
  `tearDown()`; production never got the equivalent.
- **Severity HIGH, argued**: OTA exists precisely for devices no one can
  walk to, a WiFi drop during a 1.3 MB upload is an ordinary event, and the
  consequence is the permanent loss of the one capability that would have
  avoided the trip. The device otherwise keeps running, which hides it.
- **Fixed**: `request->onDisconnect` registered once at upload index 0,
  gated on `uploadState.active` — the discriminator that is false before
  the index-0 gates pass and false again once `final` ran, because
  **onDisconnect fires after every request, successful ones included**; an
  unconditional abort would wreck completed uploads. `abortUpload()`'s own
  `uploadSession.active` guard is the second layer; after the abort,
  `state == Error` and `isIdle()` accepts the next `beginUpload()` — the
  recovery this fix exists for. Handler and callback share the AsyncTCP
  task (no interleaving), and the call site joins the existing
  async-context posture (`beginUpload`/`acceptUploadChunk` already run
  there), not a new one. Invariant written at the site: `active` is never
  set before every refusal path has returned.
- **Measured, red then green, same script both times**
  (`ota_upload_check.py --disconnect-at N`, raw socket + SO_LINGER-0 RST —
  the accident's shape, now a repeatable check documented in the harness
  README): unfixed WROOM-32D from a fresh idle boot — RST after 200 KB →
  status frozen `downloading`, follow-up refused `"Upload already in
  progress"`. Fixed firmware, same script → `state=error,
  lastResult='Client disconnected mid-upload'`, follow-up **accepted into
  the pipeline** (refused at the hash, by design — the follow-up carries a
  wrong digest so nothing reboots). **Green on both platforms**: the
  WROOM-32D over FullStack, and the nodemcuv2 over `OTAWithWebUI` — the
  ESP8266's `abort()` releases the Updater buffer, the platform-divergent
  half. Happy path re-proven after the fix: a full `--commit` cycle
  (upload, install, reboot, back idle) — the callback's no-op arm. The
  on-device OTA suite ran 10/10 after — stated plainly: it calls
  `beginUpload()` directly and discriminates nothing about this handler
  change (TEST-8); it is compile-plus-adjacent non-regression. `total`
  reads the envelope size on an abort, by design: the SEC-9 narrowing
  runs at finalize, which an abort never reaches.
- **Known limits, stated**: `uploadState` is one shared field, so a second
  concurrent uploader's index-0 reset can blind the predicate — the
  pre-existing single-session assumption of this handler (SEC-3's reset),
  out of scope here and now written at the site. And the TCP half-open
  case (a client dying with no FIN and no RST) resolves only via
  AsyncTCP's own ack timeout into the same onDisconnect path — slower but
  bounded, unverified on a board: **this entry's residual**.
- **Recorded alongside, deliberately NOT filed**: one occurrence of
  `"Could Not Activate The Firmware"` on a correctly-hashed upload that
  followed a SHA-mismatch refusal — **1 occurrence in 2 attempts**; the
  identical refuse-then-commit sequence passed cleanly when re-run, and
  passed again during this fix's verification. The SEC-9 discipline
  applies: an unreplicated observation is a note in this entry, not a
  finding.
- Design: `_bmad-output/implementation-artifacts/spec-bug-35-ota-disconnect-lock.md`
  (v2 after adversarial review — which surfaced the two-client limit, the
  ESP8266 HTTP-level proof this entry now carries, and the half-open
  residual).

### BUG-44 — MQTT: `mqtt/disconnected` is emitted only for a deliberate `disconnect()`, never for a link the broker or the network drops [MEDIUM] — **DONE (filed 2026-09-19, fixed 2026-09-20)**

- **Files**: `MQTT_impl.h` (`loop()`, `announceConnectionLost()`), `MQTT.h`
  (`isConnected()`), `HomeAssistant.h:148`.
- **Problem**: `EVENT_DISCONNECTED` had one emission site, inside `disconnect()`.
  `loop()` read `if (isConnected()) {…} else if (config.enabled &&
  config.autoReconnect) handleReconnection();`, so a dropped link went straight
  to reconnection; `disconnect()` would have refused it anyway, opening on
  `if (!isConnected()) return;` against a client already down. `EVENT_CONNECTED`
  fired on every reconnection, so the pair was asymmetric and only half of it
  observable.
- **Fix**: the loss is announced where it is noticed — `loop()`'s transition out
  of `Connected` sets the state, logs `[MQTT] Connection lost` and emits, *before*
  `handleReconnection()` can announce its own success. `disconnect()` is
  untouched and remains the only site for a deliberate one.
- **A second path, found by this lot's code review and not by the bench**: the
  `if (config.broker.isEmpty()) return;` at the top of `loop()` sat in front of
  the new transition, so clearing the broker at runtime left `state == Connected`
  for ever — the same defect, on a path the WebUI reaches (`MQTTWebUI.h` writes
  `cfg.broker = value` with no check for an empty one). The announcement moved
  in front of that return too.
- **A third symptom the filing had not named**: between the drop and the first
  reconnection attempt `getState()` returned `Connected`. It returns
  `Disconnected` now, and the WebUI's connection panel with it. The reference had
  been *claiming* this since long before it was true: its transition table read
  `Connected → Disconnected | disconnect() called or network loss detected`.
- **Measured on the bench** — WROOM-32D, a local mosquitto restarted under it at
  t≈44.5 s, `tools/on-device/probes/mqtt-link-loss`; one capture each side of the
  fix, same vehicle, same script, read on the 5 s ticks rather than inside a
  handler (subscriber order decides what a handler sees mid-dispatch):

  | | before | after |
  |---|---|---|
  | `[MQTT] Connection lost` | 0 | 1 |
  | `MQTT disconnected (via EventBus)` | 0 | 1 |
  | `MQTT connected (via EventBus)` | 2 | 2 |
  | `HomeAssistant::isReady()` on the tick inside the outage | true | false |

- **The filing overstated the cost; the review found the opposite one.** Outage
  publishes were not "lost in silence" — they were queued against `maxQueueSize`
  (100) and arrived stale. But making the guard fire costs what the filing never
  asked: **HomeAssistant never republishes entity state**, so a state published
  during an outage is now skipped and never re-sent. `republishEntity()` rebuilds
  the *discovery* document and `HAEntity` holds no state, so for an entity that
  publishes only on change, that change is gone. Filed as **BUG-46** — the lot
  files what its own fix opens — and announced meanwhile in the CHANGELOG's top
  note with the application-side workaround.
- **Tests**: six native cases in `test_mqtt_component` — emitted once, not
  repeated, the state it reports, the order against the reconnection's own
  `mqtt/connected`, a deliberate `disconnect()` still emitting once, the broker
  cleared at runtime — and three at the seam in `test_ha_component`: `isReady()`
  falling, a state published during the outage reaching nothing, HomeAssistant
  ready again once the link returns. The last two exist because the review showed
  the first was not enough: `isReady()` is a conjunction, so clearing
  `availabilityPublished` instead of `mqttConnected` keeps it green while every
  guard stays open.
- **Four mutations, each isolating one assertion**: the empty-broker announcement
  removed (2 red); the one-shot guard removed so it emits every loop (2 red —
  the repeat case at its *final* assertion, and the deliberate-disconnect case
  the first removal could not reach); the handler clearing the wrong member (1);
  the reconnection's `mqtt/connected` never sent (1). **1 151 → 1 160 `[PASSED]`,
  both ends measured** — the baseline by stashing the lot and re-running the
  fourteen native projects, not by subtracting.
- **Refs**: BUG-43 (same component pair, availability and Last Will), BUG-29
  (the offline queue this entry corrects itself against).

### BUG-46 — HomeAssistant: entity state is never republished after a reconnection, so a state published during an outage is lost [MEDIUM] — **DONE (2026-09-23)**

- **Files**: `HomeAssistant.h` — the `mqtt/connected` handler, `publishState`,
  `publishStateJson`, `publishAttributes`, `loop()`.
- **Opened by the fix, not found beside it.** Before BUG-44 `mqttConnected`
  never fell, so the guard in `publishState()` never fired. BUG-44 made it work,
  and the guard was wrong: the publish was skipped and nothing sent it after.
- **The loss is silent, and it opens after the reconnection, not during the
  outage.** BUG-43's Last Will shows the device unavailable while the link is
  down, which is honest. When it returns, Home Assistant is handed the
  *retained* pre-outage value — `HAEntity::retained` defaults true — and
  displays it until the entity changes again. A door contact that opened during
  the outage reads closed; `useAvailability = false` shows it stale throughout.
- **The scope was wider than the filing.** `publishStateJson()` carried the same
  guard, so **every `HALight` lost its state too**; `publishAttributes()`
  carried none, so attributes were queued while states were dropped — two
  policies in one component for one outage. All three now share one.
- **MEDIUM, re-argued**: it needs an outage *and* a change during it, and a
  periodic sensor republishes within its own interval — the exposure is the
  event-driven entity.
- **Arbitrated 2026-09-23 (decision memo, adversarially reviewed, maintainer
  validated)**: the last unpublished payload is held **in the component**, not
  on `HAEntity` — coalesced per entity and topic, empty while the link is up,
  drained after `publishDiscovery()` and **paced at four per `loop()`**.
  Unconditional. The numbers that decided it: a component-side `std::vector` is
  **12 bytes standing** against the 96 (ESP8266) or 128 (ESP32) of a `String`
  member on eight entities, it leaves MEM-3 free to take five `String`s off
  `HAEntity`, and last-value-wins is what a state is. The drain ends with
  `shrink_to_fit()` or the standing figure is a lie after the first outage.
- **Four shapes refused.** An opt-in flag buys nothing at twelve bytes. MQTT's
  offline queue — what shipped before 2.6.0 — **drops the newest** when full,
  backwards for a last-value-wins datum, at some 8 KB on a 40 KB heap.
  Republishing from `HASwitch::state` is actively harmful and became BUG-61.
  Leaving it to the application is 2.6.0's workaround, and it depends on an
  ordering nothing states.
- **Why paced.** `publishDiscovery()` emits one `mqtt/publish` per entity in one
  handler against a 32-event EventBus budget that **evicts the oldest**;
  draining in the same pass takes the burst from 1 + N to 1 + 2N. Pacing keeps
  the lossless connect near thirty entities instead of fifteen — above that the
  discovery burst alone is what evicts, with or without this store.
- **The review of the diff found a defect the fix had introduced, and it was the
  same one.** A publish made over a live link did not invalidate the slot the
  outage still held for that topic, and the drain spans several loops: an
  application republishing on `isReady()` — which all three examples do — had
  its fresh value overwritten by the outage's older one, retained. A live
  publish now drops the matching slot first. **The same review found the
  examples would have made the fix inert**: all three gated the event-driven
  publish on `isMQTTConnected()`, so the component never saw the value. And
  **a slot count is not a memory bound** — BUG-41's lesson one component over —
  so the store carries a 2 048-byte budget, refused aloud and counted in a new
  `HAStatistics::statesRefused`. The store also takes the HAL recursive lock
  BUG-60 introduced three days earlier: no path in the framework publishes a
  state from the web server's task, but a consumer can, and the arbitration
  chose the lock over a documented constraint.
- **Measured red then green on the WROOM-32D**, broker stopped and started under
  it, event-driven entities only. Six slots into the outage across all three
  entry points — a binary sensor, a light's JSON state, three sensors, one
  attributes document: `pending` 0 → 6, 6 through the sixty-second outage, then
  **6 → 2 → 0** at the reconnection, which is the pacing read on the board
  rather than inferred; the broker receives all six, 69 bytes. With the hold
  removed and nothing else changed, two minutes later in the same room:
  `pending` never leaves 0 and the broker receives **nothing** in 125 s.
  **Eight native removal checks discriminate**, two only after the tests were
  rewritten — the oversize check had published its oversized payload and a good
  one to the *same* entity, so the coalescing hid the refusal; the pacing
  assertion counted what the bus had dispatched, the same number either way.
- **Announced meanwhile**: 2.6.0's top note gives the application-side
  republish as the workaround. **The release that ships this reverses that note
  and must say so**; an application that took the workaround then publishes
  twice, which is harmless — same payload, last one wins.
- **Refs**: BUG-44 (opened it), BUG-43, BUG-61, BUG-41, MEM-2, MEM-3, BUG-29.

### BUG-61 — HomeAssistant: `HASwitch::state` and `HALight::state` describe what Home Assistant commanded, not what the device published [LOW] — **NEW (2026-09-23, by BUG-46's decision memo)**

- **Files**: `HASwitch.h:26`, `HALight.h:21-22`, against
  `HomeAssistant.h:340` (`publishState`) and `:383` (`publishStateJson`).
- **Problem**: both fields are public, documented "Current switch state" /
  "Current light state", and written **only** by `handleCommand()` — that is,
  only by a command arriving from Home Assistant. A relay toggled locally goes
  out through `publishState("relay", true)`, which never touches the field. An
  application that reads `sw->state` after a local change is told `false`.
- **LOW, argued.** Nothing in the tree reads either field outside the command
  path that has just written it (`autoPublishState`) and four native tests. The
  failure needs a consumer who chose to read a field the command path owns, and
  it costs a wrong boolean, not a crash — graded below BUG-2, which promised a
  safety it did not provide and settled at MEDIUM.
- **What it cost anyway**: it made BUG-46's shape (e) — republish from the
  entity's own state, no store at all — look free. It is not free, it is
  harmful: a republish from the field would overwrite the correct retained
  value with the stale default. Any future feature that trusts these fields
  inherits the same trap.
- **Fix**: have `publishState()` / `publishStateJson()` update the entity's
  field when it has one, so it finally describes what the device published; or
  state in the reference that the field tracks commands only. Not both.
- **Refs**: BUG-46 (which found it), the v2.0.0 command flow in
  `docs/components/home-assistant/project-context.md`.

### BUG-62 — HomeAssistant: every MQTT message that is not its own is reported at error level [LOW] — **DONE (2026-09-23, in BUG-63's lot)**

- **Files**: `HomeAssistant.h:160` (the `EVENT_MESSAGE` subscription),
  `:943` (the entry log), `:963` (`Invalid topic format - missing entity ID`),
  against `:128` (`Discovery prefix: %s`) and `subscribeToCommands()`, which
  already builds `commandTopicFilter` as `%s/+/%s/+/set`.
- **Problem**: `begin()` hands **every** message the shared MQTT client receives
  to `handleCommand()` — the comment at the subscription says so itself: *"every
  message the shared client receives arrives here — before findEntity has decided
  the message concerns HomeAssistant at all"*. `handleCommand()` then parses the
  topic as `homeassistant/{component}/{node_id}/{entity_id}/set` and reports the
  parse failing. A consuming application with its own MQTT topics therefore pays
  two log lines per message, **one of them `[E]`**, for messages the component was
  never addressed by.
- **Measured** on AlarmControl against DomoticsCore 2.6.1, whose documented
  command topic is `alarm/command` — one slash, so no second-to-last slash:
  ```
  [I][HA] Received MQTT command - Topic: alarm/command, Payload: arm
  [E][HA] Invalid topic format - missing entity ID
  ```
  Every `arm`, every `disarm`, on a live alarm panel.
- **LOW, argued.** Nothing malfunctions and MEM-2 already removed the allocation,
  so the cost is not memory or time — it is the **meaning of the error level**.
  `[E]` is what an operator greps on a deployed device when something is wrong,
  and here it is produced by normal traffic from the application that hosts the
  component. It is graded below BUG-61 in consequence but it is the one a field
  session actually trips over.
- **Fix**: return from `handleCommand()` before the parse when the topic does not
  begin with `config.discoveryPrefix` followed by `/`. The component already holds
  the prefix and prints it at `begin()`; foreign traffic then costs nothing and
  says nothing. Demoting the two lines to `DEBUG` would hide the symptom while
  still parsing every message, and would also silence the genuine case — a
  malformed topic **under** the component's own prefix.
- **Note for the same pass**: a foreign topic that *does* have two slashes lands
  one branch further on, at `Command for unknown entity: %.*s` (`[W]`). The prefix
  check closes both.
- **Fixed**: `handleCommand()` returns before the parse when the topic does not
  begin with the discovery prefix followed by `/`. A malformed topic *under* the
  prefix keeps its error line, which is the half worth protecting; an empty
  prefix disables the check rather than making the component deaf. The test
  asserts silence at every level — error, warning and info — on three shapes,
  including a prefix that is a prefix of the component's own.
- **Refs**: filed by AlarmControl (`jn0v/AlarmControl`, TD-006 piece 2), which
  carries its own command topic by design; MEM-2, which made this path
  allocation-free without changing what it logs.

### BUG-63 — HomeAssistant: an alarm panel with no code is published as one that requires a code, and cannot be armed from Home Assistant [HIGH] — **DONE (2026-09-23, filed and fixed the same day)**

- **Files**: `HAAlarmControlPanel.h:102-111` (`buildDiscoveryPayload`, the
  `hasCodeConfig` gate), against `HomeAssistant.h:336-343`
  (`addAlarmControlPanel`, whose `codeArmRequired` / `codeDisarmRequired`
  default to `false`).
- **Problem**: the code keys are written to the discovery document **only** when
  a code is set or some code flag is true:
  ```cpp
  bool hasCodeConfig = !code.isEmpty() || codeArmRequired || codeDisarmRequired
                                       || codeTriggerRequired;
  ```
  So a panel configured with no code at all — the library's own defaults —
  publishes **no `cod_arm_req` key**. **Home Assistant defaults
  `code_arm_required` to `true` when the key is absent.** The library's default
  and Home Assistant's default are opposites, and the wire cannot tell them
  apart: `false` and `absent` are the same bytes and mean the reverse of each
  other. A caller who wants "no code required" cannot express it.
- **Consequence, measured on a live alarm panel**: the entity is created,
  reports state correctly, shows as available, and **cannot be armed from the
  Home Assistant UI at all**. Home Assistant refuses before publishing:
  ```
  Arming requires a code but none was given for
  alarm_control_panel.gaia_alarmcontrol_alarm_control
  ```
  and because `code_format` is null — no code was configured, so there is
  nothing to format — the card cannot even offer a keypad to enter the code it
  insists on. There is no way through from the UI.
- **Disarm is unaffected**, measured over the same path while arming was broken:
  Home Assistant applies the `code_arm_required` guard to arming only, so
  `DISARM` reached the device normally. The device therefore looks healthy from
  every angle an operator checks — entities present, none unavailable, state
  correct, disarm working — while the one action that matters is unreachable.
- **HIGH.** It makes the alarm panel entity unusable for its primary purpose, on
  the library's own default configuration, with no error anywhere on the device
  and no clue in the discovery document that an operator would think to read.
- **Fixed**: `cod_arm_req` and `cod_dis_req` are written on every panel, and
  `cod_trig_req` on a panel that declares the `Trigger` feature. `code` and
  `cmd_tpl` stay where they were, behind a code actually travelling. The rule the
  reference now states: a key is written where its value can change what Home
  Assistant does, for an action the panel offers.
- **The filing was right on the arm key and one short.** Home Assistant documents
  **all three** `code_*_required` as defaulting to `true`, not only the arm one.
  Disarm works today because the MQTT platform's own `_validate_code()` passes
  when no code is configured — an implementation detail of that platform, not a
  documented guarantee, and 20 characters is not the price of that bet. Gating
  `cod_trig_req` as this entry first recommended would have left the same defect
  one action further on, for any code-less panel offering `Trigger`.
- **The budget, measured before the fix rather than predicted.** A code-less
  panel's document runs from 476 characters (default identifiers, one arm mode)
  to 686 at six modes, and 753 at six modes with a 17-character node id. The two
  keys add 40, so a code-less panel at five or six modes crosses the 699-character
  event field and is refused aloud. Nothing that works today stops working: every
  code-less panel is unusable now, in silence. The panel this was filed from —
  two arm modes — goes from 573 to 613, with 86 characters to spare.
- **A pinned figure moved, and the fix makes documents smaller.** The suite's
  BUG-38 panel is 617 characters, not 638: it declares no `Trigger`, so it no
  longer carries `cod_trig_req`. Every panel that does not offer Trigger loses 21
  characters. Flash, FullStack built on both platforms against `1de6f08`:
  **−1 316 B on ESP32, +620 B on ESP8266**, static RAM unchanged on both.
- **Board, and the half that is not this repository's to run.** On the WROOM-32D
  against the bench broker, the retained document a code-less two-mode panel
  publishes went from **527 characters with no `cod_arm_req` at all** to **567
  with `cod_arm_req: false` and `cod_dis_req: false`** — the +40 measured
  natively, on silicon. Probe: `tools/on-device/probes/ha-codeless-panel`.
- **The oracle, and it is not this repository's to run.** The key in the document
  is only the cause; the measurement that matters is Home Assistant arming the
  panel. The maintainer rebuilt the consumer's panel on this branch and armed it
  from the Home Assistant interface on 2026-09-23 — the refusal quoted above,
  from that same panel before the fix, is the red. Home Assistant's own defaults
  were read from its MQTT alarm panel documentation rather than observed, which
  the entry says rather than implies; the repository still acquires no Home
  Assistant instance of its own.
- **Tests**: the case that had no coverage — a panel built with no code,
  asserting both keys present and the document's length — and the trigger key
  following the feature in both directions. Removal check: with the gate
  restored, four tests go red, one of them the suite's own pinning of the defect
  (`test_alarm_panel_discovery_code_fields` asserted the keys absent). The
  adversarial review then found the one key with no value assertion — the suite
  would have passed with `cod_trig_req` written as a constant — and the shape
  where the fix bites, a code-less panel with all six arm modes, which is over
  the field and now refused: both pinned. Nine tests across the lot, native
  **1 220 → 1 229 `[PASSED]`** read from the CI native job.
- **The next release owes three lines**: a code-less alarm panel's discovery
  document now states `code_arm_required` and `code_disarm_required` (and
  `code_trigger_required` where the panel offers `Trigger`); a topic over the
  event field is refused rather than published cut; a discovery prefix changed
  at runtime re-subscribes immediately.
- **Found the hard way**: the consumer's first instinct was to force the keys out
  by setting `codeTriggerRequired = true` — the only lever an application has.
  That works and is what production ran for one flash, but it is a workaround in
  the wrong repository and the consumer's PO rejected it on sight. Recorded here
  because the lever exists and someone else will reach for it.
- **Refs**: filed by AlarmControl (`jn0v/AlarmControl`, TD-017). See also BUG-64,
  filed in the same session: the device-side console could not have shown this
  either.

### BUG-64 — RemoteConsole: ESP-IDF and Arduino core log lines never reach the remote console [MEDIUM] — **DONE (2026-09-23, filed and fixed the same day)**

- **Files**: `DomoticsCore-RemoteConsole` (the log sink), against the serial path
  that Arduino's `log_e` / ESP-IDF's `ESP_LOGE` already reach.
- **Problem**: the remote console carries the lines DomoticsCore itself emits
  through `DLOG_*`, and **nothing else**. Log lines written by the Arduino core
  and by ESP-IDF — `Preferences.cpp`, `Update.cpp`, `esp_ota_ops`, `esp_image` —
  go to the UART only. A device without a serial line has no way to show them.
- **Measured**, same firmware, comparable boots, one over the console and one
  over the serial line:
  ```
  console (telnet, level 4) : 0 lines matching [E]
  serial  (same build)      : 15 lines matching [E]
  ```
- **What it costs, concretely**: a production device that failed an OTA with
  Arduino's `Could Not Activate The Firmware` — the string `UPDATE_ERROR_ACTIVATE`,
  raised when `esp_ota_set_boot_partition()` fails — could not be diagnosed. That
  call's own `ESP_LOGE` says *which* error it was (image validation, rollback
  state, partition not found); on a device reachable only over the network, that
  line does not exist. The operator sees a symptom with no cause, and the only
  honest next step is to move the investigation to a board with a UART.
- **MEDIUM**: nothing malfunctions because of it, but it removes the evidence at
  exactly the moment something else has gone wrong, on exactly the devices that
  cannot be opened up. An OTA-updated device is the normal case for this library.
- **The recorded fix was half a fix, and the buffer figure was wrong.** Measured on
  a WROOM-32D before any code: `esp_log_set_vprintf` captured **nothing** this
  firmware printed. An Arduino build remaps `ESP_LOGx` onto ARDUHAL, and the
  core's `log_e` goes through `log_printf` to `ets_printf`, which the vprintf hook
  never sees; only lines from **precompiled ESP-IDF libraries** reach it — which
  is the one that matters here, since `esp_ota_set_boot_partition()` is one of
  them. The two sinks are disjoint: `ets_install_putc1` carries the core's lines
  and never the library's. The ring was not "~10 lines" either: 5 on ESP8266, 50
  on ESP32, both HAL constants, and the component's own comment said 100.
- **The ESP8266 half was off at the source, which nothing had noticed.**
  `HardwareSerial::begin()` calls `end()`, which calls `uart_set_debug(UART_NO)`
  and `system_set_os_print(0)`: a complete failed join prints **not one line**
  (`uart_get_debug` reads −1). With `system_set_os_print(1)` and the sink taken,
  the same join yields **13 lines / 277 characters in ten seconds**.
- **Arbitrated after that measurement, because it inverted the question.** Thirteen
  lines in ten seconds against a 20-entry ring means the SDK's narration would
  evict everything else, and it buys nothing for the failure that opened this
  entry: the OTA cause is an ESP-IDF line on ESP32, and the ESP8266's Updater
  logs through `ets_printf`, so through the character sink, with SDK printing off.
  So: ESP32 takes both sinks by default; ESP8266 installs the sink but leaves SDK
  printing **off**, switchable from the console with `core on`, which reinstalls
  the sink because an application's `Serial.setDebugOutput()` takes it back.
- **Fixed**: a `CoreLog` HAL — `CoreLog_HAL.h` with the ring and its drain,
  `CoreLog_ESP32.h`, `CoreLog_ESP8266.h` and `CoreLog_Stub.h` with the mechanisms,
  every platform test inside them (Constitution IX). The sink neither allocates
  nor writes to a client: it copies into a fixed ring (16 slots on ESP32, 4 on
  ESP8266) that `loop()` drains four lines at a time, refusing and counting what
  it cannot hold. The level is read from both line shapes so an error stays
  greppable, captured lines carry a tag of their own (`PLATFORM`), and the rings
  move to 20 (ESP8266) and 150 (ESP32) — measured at 137 and 152 bytes per
  80-character line, so 2.7 KB and 23 KB when full.
- **The board found what no native test could.** Our own `DLOG_*` output goes
  through the Arduino core's logger on ESP32, so the character sink captured it
  too: every DomoticsCore line would have reached the client twice, once under its
  component's tag and once as a platform line. Six of them did, on the first run.
  `Logger` now brackets its own output with a suppression the sink honours.
- **Boards, red then green on each, re-run against the final code.** WROOM-32D:
  with the capture released through public API, the `gpio` and `Preferences.cpp`
  lines reached the serial port and **nothing** reached the console; with it
  installed, **19 lines in 26 seconds**, both shapes at ERROR, and **zero**
  duplicates of our own — `core` reporting 16 slots, 0 dropped, 0 ignored.
  nodemcuv2, counted from a `clear` so the console's own replay cannot flatter it:
  `core off` **0 lines** over 45 seconds and two scans, `core on` **9 lines
  including 3 `scandone`**. The ESP8266 sink's placement was read out of the
  linked image rather than asserted by an attribute: `putcHook` at `0x40100138`,
  with `pushChar`, `pushLine` and the critical section inlined into it, so the
  whole chain is in IRAM. On ESP32 it is flash-resident, which is where that core
  keeps its own sink — forcing the chain into IRAM there makes the xtensa linker
  refuse the literal pool.
- **Cost**, FullStack against `4ec82bd`: ESP8266 **+1 040 B static RAM** (the
  intake: four 128-byte slots, a 128-byte assembly buffer and the counters) and
  **+1 732 B flash**; ESP32 **+2 240 B** and **+16 388 B**.
  The ESP32 flash is the suppression guard at every `DLOG_*` site, about thirty
  bytes each — 0.8 % of the partition to stop the console showing every framework
  line twice. The guard is a HAL macro, so the ESP8266, whose own logger writes
  through `Serial` rather than the captured sink, pays nothing for it: it cost
  6 144 B there until it moved. The alternative — routing this framework's own
  ESP32 output through `Serial` instead of the core's logger, so the sink never
  sees it — was refused: ARDUHAL's file-and-line decoration and its compile-time
  level filtering are what every other line on the serial port carries, and
  changing that to save flash would change every existing consumer's log format.
  No per-loop cost is claimed: the drain returns before taking the lock when no
  capture is installed, and this repository's own figures say an ESP8266 loop
  cannot be A/B'd across two builds.
- **Eighteen native tests** in a suite of its own, **twelve removal checks** red
  for the right reason, and the intake asserted allocation-free with `HeapTracker`.
  Native **1 229 → 1 248 `[PASSED]`**, read from the CI native job.
- **The adversarial review found nine defects in the first version**, four of them
  in the accounting this entry claims: the drop counter was unreachable on the
  character sink — the only one ESP8266 has — because a line refused before its
  newline left nothing to count; the two ESP32 sinks shared one slot, so an
  ESP-IDF line landing between two characters of an Arduino line silently ate its
  head; `installCapture()` was not idempotent, and a second call chained the
  vprintf hook to itself, which is unbounded recursion on the next log line; the
  ESP8266 critical section used `ets_intr_lock`/`ets_intr_unlock`, which lower the
  interrupt level rather than restoring the caller's, eight lines below a sibling
  that already did it properly. Also fixed: `drainLine()` consumed a line into a
  buffer too small to hold it, the once-a-minute report wrapped through four
  billion after a reinstall and treated `millis() == 0` as "never reported", the
  level parser lost every level when the core's log colours are on, and the
  character sink was taken even on a USB-CDC build with no UART console. Nine
  tests were added for those paths; the suite went from nine cases to eighteen.
- **The next release owes four lines**: the platform-line capture and its
  `PLATFORM` tag, the `core` command, the console buffer's new default sizes (20
  and 150), and — on ESP8266 — that enabling the SDK narration also returns it to
  the serial port the core had silenced.
- **Refs**: filed by AlarmControl (`jn0v/AlarmControl`), whose production panel
  has no serial line. See BUG-63, filed the same session. Probes:
  `tools/on-device/probes/core-log-sinks` (which sink carries what) and
  `tools/on-device/probes/console-platform-lines` (the console leg), with
  `tools/on-device/console_watch.py`.

### BUG-65 — HomeAssistant: an alarm panel's discovery document spends 188 characters restating Home Assistant's own defaults [MEDIUM] — **DONE (2026-09-24)**

- **Files**: `HAAlarmControlPanel.h`, the seven `pl_*` keys, against the
  699-character event field the component reference documents.
- **Problem**: every payload constant the panel published — `pl_arm_home`:
  `ARM_HOME`, `pl_disarm`: `DISARM`, five more — was the value Home Assistant
  already uses when the key is absent. The document stated what the other side
  assumes and, until BUG-63, omitted the three keys where the two sides
  disagree. It was exactly the wrong way round.
- **Where they came from**, since the question is fair: `887c3f8`, 2026-03-05,
  the commit that created the entity. Its tech spec asked for *"all 7 command
  payload fields with correct HA protocol values"* — a complete document,
  mirroring the documentation. Nobody measured it, and the 699-character field
  only became a known constraint with BUG-38 six months later.
- **Measured, and the entry's own figures were wrong by 14.** It said 726 → 538;
  the run says **740 → 552** for a code-less panel with six arm modes on a
  9-character node id — refused whole before, published now. The production
  shape it was filed from, two arm modes and no code, goes **613 → 541**. The
  188 is confirmed independently by the refusal message of the sibling test,
  974 → 786 on a 32-character node id.
- **What replaced them.** The keys were the only place the device stated the
  vocabulary it accepts, so a native test now holds the seven
  `AlarmPanelCommand` constants equal to the payloads Home Assistant documents:
  a change on this side fails the build rather than a wire. A change on Home
  Assistant's side is not visible from this repository, and `handleCommand()`
  echoes whatever it is handed — it answers `true` to `BANANA` — so nothing
  else compares them. The reference says both.
- **Verification**: 156 → 157 native cases; with the keys put back, five tests
  fail, one of them at `Expected 552 Was 0`, which is the refused document
  itself. The pin was checked non-vacuous by spelling `ARM_NIGHT` as
  `ARM_NITE`. A test asserting that a documented payload parses was written and
  deleted: it reached nothing. On the boards, `test_ha_device` measures 541 and
  552 to the character on both the WROOM-32D and the nodemcuv2.
- **A CHANGELOG line is owed**: the seven `pl_*` keys leave every alarm panel's
  discovery document. No behaviour changes while Home Assistant's defaults are
  what they are today.
- **Refs**: BUG-38, which abbreviated every key for this same budget; BUG-63,
  whose lot measured it; TEST-12, whose lot ran it on a board.

### BUG-66 — HomeAssistant: what `shutdown()` publishes never leaves the device [LOW] — **NEW (2026-09-23, filed by BUG-63's lot)**

- **Files**: `HomeAssistant.h` (`shutdown()` → `setAvailable(false)` and
  `removeDiscovery()`), against `Core.cpp:loop()`, which returns on
  `!initialized`, and `Core.cpp:shutdown()`, which clears that flag.
- **Problem**: both publish through the EventBus, which dispatches in
  `Core::loop()`. `Core::shutdown()` clears `initialized` before anything drains
  the queue, so the `offline` availability and one empty retained payload per
  entity — the messages that withdraw the device from Home Assistant — are
  queued and discarded. Found by a test written for BUG-13's lot, which counted
  zero removals where the code plainly publishes two.
- **Consequence**: a device shut down cleanly stays in Home Assistant with every
  entity present, and stays `online` there: a graceful MQTT disconnect does not
  fire the Last Will that BUG-43 put in place for a dropped link.
- **LOW, argued**: nothing calls `Core::shutdown()` on a normal device path —
  a reboot and an OTA restart do not — so this is reachable only from an
  application that shuts the stack down deliberately. The test added in BUG-63's
  lot calls the component's own `shutdown()` so it pins what the component does,
  not what the Core swallows.
- **It is also a question, not only a defect**: whether a shutdown *should*
  remove the entities is undecided. Withdrawing them on every deliberate stop
  would make an application that restarts the stack lose its entities in Home
  Assistant, where the retained configs are precisely what brings them back.
  Answer that before making the messages leave.

### BUG-67 — RemoteConsole: on ESP32 a console started before the network stack aborts the device [MEDIUM] — **DONE (2026-09-24)**

- **Files**: `RemoteConsole.h`, `begin()` — `telnetServer = new HAL::WiFiServer(port); telnetServer->begin();`,
  above the comment that says it does not require WiFi to be connected yet.
- **Problem**: that is true on ESP8266 and false on ESP32. `WiFiServer::begin()`
  reaches lwIP, which does not exist until the radio has been brought up at
  least once; before that the firmware stops on `assert failed:
  tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid
  mbox)` and restarts, again at the next boot.
- **Consequence**: a device that never finishes booting, with no log of its own
  to say why — the console being the thing that would have carried it.
- **How it was found**: the first flash of the console's new device suite, on a
  WROOM-32D. It had never happened before because nothing had ever started this
  component alone on a board, which is TEST-12.
- **Reachable how**: the shipped stack registers WiFi first, so `FullStack` and
  the examples do not meet it. What meets it is a console without the WiFi
  component, or registered before it — the shape a transport that is not WiFi
  makes ordinary.
- **Fixed by asking the platform first.** `HAL::canOpenServer()` is new, one
  answer per platform: ESP8266 says yes, since the SDK starts lwIP before the
  sketch; ESP32 answers `esp_netif_get_nr_of_ifs() > 0`, which is true of any
  transport's first interface and not of the radio alone — the console defers
  rather than aborting, says so once, and `loop()` opens the server on the first
  iteration where a stack exists. That shape was chosen over keying on WiFi
  because an Ethernet-only device has a stack and no mode.
- **Verification**: three native cases on the scriptable stub, each red when its
  own guard is removed; on the WROOM-32D the console suite dropped the
  station-mode line it had needed to run at all and prints 8/8, and with the
  guard removed the same board stops at `tcpip_send_msg_wait_sem … (Invalid
  mbox)` and reboots. On the nodemcuv2 the new case is **skipped**, not passed:
  a platform with a stack from boot defers nothing, and a green tick there would
  have meant nothing. 70 → 73 native cases.
- **The lot's own review found the fix's own defect**, which is why the third
  case exists: the deferred open was gated on `config.enabled`, which
  `shutdown()` does not clear, so a console disabled from the web UI — a path
  that calls the component's `shutdown()` without deactivating it — reopened its
  telnet port on the next `loop()`, serving commands with no log stream. It is
  now gated on what `begin()` sets and `shutdown()` clears. The review also took
  the helper out of the public section it had landed in, put back the
  `restarted` wording `setPort()` had lost, and made the allocation `nothrow`,
  since it now runs from `loop()`.
- **A CHANGELOG line is owed**: `getServer()` can return `nullptr` after a
  successful `begin()`, and the port starts listening from `loop()`.
- **Refs**: TEST-12, whose suite found it; CI-14, the other case of a component
  that compiles and cannot run.

### BUG-69 — WebUI: the other listening socket does not ask whether it can open [MEDIUM] — **NEW (2026-09-24, filed by BUG-67's review)**

- **Files**: `WebUI.h` (`begin()` → `webServer->start()`) and
  `WebUI/WebServerManager.h:43`, against `HAL::canOpenServer()`.
- **Problem**: BUG-67 was a component opening a listening socket in `begin()`
  before lwIP existed. There are exactly two such sites in the tree — the
  console's `WiFiServer`, fixed, and the WebUI's `AsyncWebServer`, which still
  calls `begin()` unconditionally. Same lifecycle position, same abort.
- **Why nothing says so**: `System.h` composes WiFi before WebUI, which hides it
  the way the console was hidden until a suite registered it alone. WebUI
  declares no ESP32 test environment, and on ESP8266 `canOpenServer()` is always
  true, so no suite can reach the deferral.
- **Consequence**: an ESP32 with a WebUI and no WiFi component — the
  transport-neutral shape — stops in lwIP at boot, exactly as BUG-67 did.
- **Fix**: the answer already exists and has one caller. Ask it in `begin()`,
  open from `loop()`, and gate the retry on what `shutdown()` clears — BUG-67's
  review found that gate is the part easy to get wrong.
- **Not verified, and stated as such**: `NTPComponent::begin()` calls
  `HAL::NTP::init()`, which reaches `esp_sntp_init()` on ESP32. Whether that
  aborts without an interface was not measured; it is worth one board run before
  this is taken.
- **Refs**: BUG-67, the same defect at the other socket; TEST-12, the manifest
  gap that hid it.

### BUG-68 — HomeAssistant: every entity's document still restates Home Assistant's own payloads [MEDIUM] — **NEW (2026-09-24, filed by BUG-65's review)**

- **Files**: `HAEntity.h:108` and `HAButton.h:46` (`pl_avail`: `online`,
  `pl_not_avail`: `offline`), `HALight.h:35` (`pl_on`: `ON`, `pl_off`: `OFF`),
  against the 699-character event field.
- **Problem**: BUG-65 removed the seven payload keys of one entity type because
  each stated the value Home Assistant assumes for an absent key. The same keys
  are written by every other entity in the component, as literals: **45
  characters of availability payloads on every document of every entity**, and
  **28 more on every light**. The rule was established and applied once.
- **It is not one decision but two.** `HALight` writes its pair as literals, so
  it is BUG-65's case exactly. `HASwitch`, `HABinarySensor` and `HAButton` write
  a configurable field, which the panel never had: there the answer is the third
  branch BUG-65 could not take — emit the key only where a caller overrode it.
  The availability pair sits in the base class and in `HAButton`, which
  duplicates it.
- **Consequence**: the same one BUG-65 measured. A document is refused whole
  when it crosses the field, so a light or a switch on a long node id is an
  entity Home Assistant never creates, for bytes that assert nothing. No test
  reads `pl_on` anywhere in the repository, and the one test that reads the
  availability pair asserts it is **present** — it pins the behaviour this would
  change.
- **Before touching it**: the availability payloads are what BUG-43's Last Will
  publishes, so the document and the will must keep agreeing; that is the check
  the panel's own vocabulary pin is the model for.
- **Refs**: BUG-65, which set the rule and measured its cost; BUG-38, the
  abbreviation lot; BUG-43, the Last Will that shares those two payloads.

### TEST-12 — RemoteConsole declares no device environment, so its platform halves are compiled and never run [MEDIUM] — **DONE (2026-09-24)**

- **Files**: `DomoticsCore-RemoteConsole/platformio.ini`, whose only environment
  was `[env:native]`, against `CoreLog_ESP32.h` and `CoreLog_ESP8266.h`.
- **Problem**: the two platform mechanisms BUG-64 added reach every target
  through `Logger.h`, so the required cross-compilation checks built them — and
  nothing executed them. The evidence for both halves was a pair of hand-run
  probes whose figures live in a roadmap entry.
- **Fix**: `[env:esp8266dev]` and `[env:esp32dev]` on the pattern Storage and
  OTA use, and one suite for both boards. Every line is provoked with
  `ets_printf`, which is how the Arduino core's macros and the SDK reach the
  sink, so what runs is the mechanism rather than a stub: the arrival and its
  level from both known shapes, an overlong line keeping its head, a second
  install that must not chain the hook to itself, a removal that gives the sink
  back, a full intake refusing the newest line and counting it, and the SDK
  switch taking the sink back from an application that grabbed it.
- **What the first flash found, before any assertion ran.** On ESP32 the
  component aborts: `begin()` opens its telnet server, which reaches lwIP, and
  lwIP does not exist until the radio has been brought up. `assert failed:
  tcpip_send_msg_wait_sem … (Invalid mbox)`, in a boot loop — filed as BUG-67.
  It is also why `pio test -e esp32dev` sat silent for eighteen minutes: it was
  waiting for Unity output from a firmware that was restarting. The suite worked
  around it by putting the radio in station mode; BUG-67's own lot removed that
  line the next day and the suite starts with no stack at all.
- **Verification**: 7/7 on the nodemcuv2 and on the WROOM-32D, and 8 apiece once
  BUG-67's lot added its own case. With the platform's sink not taken, five fail
  on each; on ESP32 the capture
  still reports itself installed while nothing arrives, which is the vprintf
  hook and the character sink being disjoint — BUG-64's central claim, executed
  rather than argued. The SDK-switch test is the one that survives both
  removals, because `setSdkOutput(true)` reinstalls the sink itself, which is
  what it measures. The project joined `Build on-device suites` for both
  environments, and the four deprecated `ICACHE_RAM_ATTR` declarations the build had
  been warning about at every use are now `IRAM_ATTR` — three in the HAL, the
  fourth in the probe BUG-64 was measured with.
- **Refs**: BUG-64, which built the mechanisms; BUG-67, which this found;
  CI-11 and CI-19, closed in the same lot.

### BUG-45 — WebUI providers: the settings handlers disagree about what they refuse [MEDIUM] — **DONE (2026-09-20)**

- **Problem**: four divergences between providers doing the same job, all
  visible by reading and none read until TEST-9's lot wrote these pages their
  first native suites. MQTT took a port with no range check, so `0` landed in
  the config and `65536` wrapped through `uint16_t` to `0` — a broker the device
  can never reach, stored as if it had saved. MQTT answered `"success":true` for
  a field it did not know, the dispatch having no `else`. NTP's `if (hours > 0)`
  guarded the assignment but not the answer, so `0` and `"soon"` reported a save
  that had not happened. And six providers serialised an untouched
  `JsonDocument` to `"null"`, which `UpdateBuilder` skipped for `"{}"` and not
  for `"null"`.
- **Three more found while fixing.** `RemoteConsoleWebUI` refused `"telnet"`
  only because `toInt()` reads it as `0` — it took `"2424x"` as `2424`;
  `LEDWebUI::getWebUIData()` dereferenced its component with no guard, alone of
  the nine; and NTP's true ceiling is the SNTP client's, since `begin()` hands
  the interval over as milliseconds in a `uint32_t`, so the cap is **1193
  hours**, not `2^32 / 3600`.
- **And the WebUI bound was not enough**, which the second review pass caught:
  `setConfig()` and `SystemPersistence.h:246` reach `syncInterval` without
  passing the page at all, so the conversion still wrapped — 1194 hours to 57
  minutes, and `4294968` seconds to **704 ms**, measured on the component rather
  than argued. `NTP.h` now holds the value at the client's ceiling instead, and
  the stub records what it was given, a host suite having no SNTP client to
  interrogate. All four shut in-lot, so no column moves.
- **Rule**: a numeric settings field is digits only, BUG-40's rule applied to
  the WebUI surface. What happens next belongs to the field, not the parser — a
  port, a log level and a sync interval out of range are refused; a brightness
  that parses and overshoots is clamped, because a slider never sends a value of
  the wrong shape. `webUIParseUnsigned()` and `webUIContextData()` are free
  `inline` functions in `IWebUIProvider.h`: the contract's own header, no
  virtual, no vtable, nothing a fork inherits.
- **The refusal *shape* is deliberately not touched**, and that is the lot's
  main finding. Its filed "Next step" — every refusal naming its reason —
  contradicts `ba901c4` (BUG-32), which left `SystemInfoWebUI`'s refusal
  nameless on purpose because `app.js` turns any `error` key into a blocking
  alert. Each file keeps the vocabulary it has, so MQTT and NTP name their new
  refusals and the seven silent providers stay silent. Filed as BUG-51.
- **Tests**: **four** suites pinned `"null"` — LED, MQTT, NTP, RemoteConsole —
  and moved, as they were written to. Storage and HomeAssistant had none and
  gained one assertion each, labelled a regression guard and not evidence: no
  declared context is unhandled, so a per-provider test detects a change, not a
  behaviour. The behaviour test goes where the behaviour is, in
  `test_update_builder` with a stub provider answering `"null"`, RED first.
  `LEDWebUI`'s null guard segfaults rather than failing red, so its test runs
  last, as BUG-32's did.
- **Three tests exist because the review asked what would still pass.** The
  parser's width guard was pinned by nothing: delete `acc > 0xFFFFFFFF` and
  `"4294967296"` truncates to `0`, so brightness darkens the LED and a log level
  silences the console, both answering success, with every suite green — it now
  turns three red. The new `else` makes the MQTT card and its dispatch agree or
  fail loudly, so a test walks the declared fields and counts them. And the NTP
  conversion above. The second pass then found two of those three tests
  vacuous at their own boundary and they were tightened.
- **Measured with a browser on the WROOM-32D** through a new
  `tools/on-device/webui_settings_refusal_check.py`, which reads the stored value
  back from `/api/ui/updates` and not from the DOM. `65536` into the MQTT port
  **stored `0` with nothing said**, and now leaves `1883` standing and says
  `Invalid port`; an NTP interval of `0` stored nothing and said nothing, and
  now says `Invalid sync interval`; `1194` hours is refused the same way. All
  three are range cases: the fields are `<input type="number">`, so a browser
  cannot type `"8883x"` into one and the digits-only half has native coverage
  only, as does everything else.
- **Cost**, FullStack, both images built with `-ffile-prefix-map` so the
  comparison is not one of path lengths: ESP8266 **+164 B RAM, +580 B flash**;
  ESP32 **0 RAM, +968 B flash**. The DRAM is the new refusal literals, the same
  shape as every refusal already in the tree — MEM-7's subject.
- **Verification**: 1 160 -> 1 173 `[PASSED]` over the fourteen native projects,
  0 failed, the baseline re-derived from a run on `main` rather than taken from
  this file, which said 1 151 and was two lots old. FullStack compiles on the
  three targets; the eight on-device suites compile.
- **Refs**: BUG-40 (the digits-only rule), BUG-32 (the shape it contradicts),
  BUG-51, BUG-52, BUG-53 (opened here), TEST-9 (which filed it).

### BUG-43 — HomeAssistant: the availability topic has no Last Will, so Home Assistant never sees a device go away [MEDIUM] — **DONE (2026-09-18)**

- **Problem**: Home Assistant watches one topic per device, the one `avty_t`
  names, and only the broker can write `offline` to it once a device has crashed
  or lost the link. The Last Will sat on `{clientId}/status` while `avty_t`
  pointed at `{discoveryPrefix}/{nodeId}/availability`, an ordinary retained
  publish — so the watched topic was the one the broker would never correct and
  **no entity ever went unavailable**. Seen on a production broker: both topics
  retained and contradicting each other, on a device with 21 days of uptime whose
  HTTP API still answered. On an alarm panel that is a silent fault.
- **Arbitration — (c) of the three the filing listed.** (a), setting the will to
  the HA topic, inverts the layering: MQTT ships without HomeAssistant. (b), both
  topics in an `avty` array, is refused on measurement — ~45 characters more than
  `avty_t`, which puts BUG-38's 638-character panel back over the 699-character
  event field; and a list's default `avty_mode` is `latest`, so two retained
  topics arriving in an uncontrolled order would make the device flap. The filing
  preferred (b) as "the only one that keeps `{clientId}/status` meaningful", but
  nothing ever published `online` there, so it was half a signal; under (c) it
  becomes a complete one.
- **Fix**: one topic, symmetric — whichever of `HAConfig::availabilityTopic` and
  `MQTTConfig::lwtTopic` the application names moves the other, at `begin()` and
  through `setConfig()` afterwards. A session already open is reopened on the
  spot: the will is sent in CONNECT, and `MQTTComponent::loop()`'s reconnection
  sits behind a backoff timer and never fires at all with `autoReconnect` off.
  `lwtRetain` is forced true, or a transient will leaves the retained `online`
  standing. Four configurations cannot be reconciled and are named in the log
  rather than advertised as if they worked: no MQTT component, no will, a will
  with no topic, and a payload other than `offline`.
- **Tests**: RED first, two cases — which the lot's review then found proved less
  than they looked, since both left `broker` empty, so the stub never connected
  and the reconnect branch never ran: **deleting it kept all 122 cases green**.
  Nine more give MQTT a broker and a link and assert the will *the broker was
  handed*, not the config field. Eleven cases, five removal checks.
- **Upgrading**: `avty_t` changes for every entity and Home Assistant follows it
  from the retained discovery document with no action. The retained `online` on
  the old topic is orphaned — clear it with an empty retained publish.
- **Refs**: BUG-38 (the same document, cut at 699 characters).

### BUG-42 — Core: `Core::begin()` reserves a kilobyte of the ESP8266's 4 KB cont stack [HIGH] — **DONE (2026-09-18)**

- **Problem**: the Storage ESP8266 suite panicked at `core_esp8266_main.cpp:191
  __yield`, rebooted and started over, printing no verdict line. Five of seven
  tests passed on every pass, so the scrollback looked healthy.
- **Cause**: `char text[1024]` in `Core::begin()` is reserved by the prologue
  **whether or not its branch runs**, and is still held while `initializeAll()`
  runs every component on top of it. `begin()` measured 1 216 B against a 4 096 B
  cont stack; the suite reached its sixth test with **32 bytes** free. The
  component, not the test — which is the grade the filing was provisional about.
- **Fix**: the block moves to a `noinline` function that returns before the
  components are initialised, so the two frames no longer sum. Same output, no
  allocation. `begin()` 1 216 → **208 B**, free cont stack 32 → **896 B**.
- **Why HIGH**: any ESP8266 application with a `setup()` frame a hundred bytes
  deeper would have hit the same panic, in the library, on a shipped platform.
- **Board**: 7/7 with a verdict line on a `nodemcuv2`, a first for this suite.
- **Refs**: OBS-4 (the buffer and the test that sizes it); BUG-41, whose board
  leg found this.

### BUG-41 — Core: the EventBus queue counted entries, and every boot dropped eight events [MEDIUM] — **DONE (2026-09-18)**

- **Problem**: `enqueue()` capped the queue at 32 entries, and a FullStack
  application emits about forty events between the first component `begin()` and
  the first `Core::loop()`, which is when nothing drains the bus. Bench ESP32:
  **8 dropped at every boot** in AP mode, 6 in STA — harmless only because
  nothing had subscribed yet. Ten more emissions and a device stops listening to
  its own MQTT commands, in silence. The 32 stood for bytes: it exists because an
  `MQTTPublishEvent` is 830 B, so on a 4-byte event it measured nothing.
- **Fix**: the bound is the bytes the queue holds. `QueueCost` models one event's
  heap cost from measured per-platform constants; the budget is the same 32
  reference events, oldest evicted first, an oversized event refused before its
  payload is copied, and a 256-entry guard rail against model drift. New:
  `getQueuedBytes()`, `getQueueHighWaterPct()`, `-DDOMOTICS_EVENTBUS_QUEUE_BYTES`.
- **Board**: 8 and 6 dropped → **0 in both modes**. 32 of 48 reference events
  survive a deliberate burst, and 28 192 B is exactly 32 × 881. **1 844 bytes
  smaller in flash** than the entry cap, static RAM unchanged.
- **The model is a close estimate, not a ceiling.** Nothing had ever weighed it
  against real heap — the suites recompute their expectations from the same
  constants, which is self-consistency. A full queue measures **29 096 B on an
  ESP32-C3 against 28 192 modelled, 29 064 B on a nodemcuv2 against 28 576** —
  2 to 3 % under. Whether the residue is per-event or per-queue needs a second
  point per board (TEST-10).
- **Calibration**: `kNode`, `kOverhead` and `kSsoChars` are measured per chip
  (`tools/on-device/probes/allocator-shape/`). The C3 matches the xtensa ESP32 on
  all three; ESP32-S2 and -S3 are unmeasured and take the conservative arm.
- **Tests**: the rewrite list came from *removing* the literal 32 and letting the
  suites name what fell — four went red, and a fifth stayed green while going
  vacuous. Eleven new cases, five rewritten, four removal checks.
  `-DDOMOTICS_EVENTBUS_QUEUE_BYTES` had been announced in three places and read in
  none; a compile-time constant cannot be exercised by a suite built without it,
  hence `test/test_queue_budget_override`.
- **What it does not fix**: the MQTT connect burst is full-size events; its cliff
  moved by exactly one entity (29 sensors to 30). Shrinking it is **LO-2**.
- **Refs**: LO-5, BUG-36, LO-2, `docs/decisions/0003-the-eventbus-queue-is-bounded-by-bytes-not-by-count.md`.

### BUG-40 — Core: `ComponentConfig`'s numeric validators refused most floats and accepted `"4x"`, and a redefined parameter was validated twice [MEDIUM] — **DONE (2026-09-15, filed the same day by TEST-7's reading)**

- **File**: `ComponentConfig.h`, `validateInteger`, `validateFloat`,
  `validateIPAddress`, `validatePort`, `defineParameter`.
- **Problem**: `validateFloat()` accepted a value only if
  `value == String(value.toFloat())`, and Arduino's `String(float)` prints
  two decimals by default on both cores (esp32 `WString.h:76`, esp8266
  `WString.h:105`), so `"1.5"` came back as `"1.50"` and was refused as
  `Invalid float format` — `"1.50"` and `"3.00"` passed, `"3"` and `"1.5"`
  did not; `"nan"` passed. `validateInteger()` used the same round-trip and
  refused `"+5"` and `"05"`. `validateIPAddress()` and `validatePort()`
  parsed with `toInt()`, which is `atol()` on both cores, so `"1.2.3.4x"`
  (4) and `"80x"` (80) validated. `defineParameter()` with a name already
  defined pushed a second entry and `validate()` checked both. Nothing in
  the tree calls the class (DC-17); every sketch that follows the Core
  reference's §4 does.
- **Fix**: the rule, now stated in the reference — *an integer is an
  optional sign then decimal digits, nothing else, and fits in 32 bits; a
  float is an optional sign, decimal digits with an optional point, an
  optional decimal exponent, nothing else, and is finite; an IP octet and
  a port are decimal digits only* — implemented as three private helpers
  (a character scan, then `strtol`/`strtof` with `ERANGE` and
  `std::isfinite`) replacing the four round-trips; a port that is not
  digits answers `Invalid port format`. `defineParameter()` replaces the
  definition it finds by name and applies the new default the same way.
  The header gains `<cstdlib>`, `<cerrno>`, `<cmath>` and `Platform_HAL.h`.
  Compiled by CI's three board targets through `IComponent.h`, never
  linked there: nothing in FullStack calls it.
- **Verification**: seven cases in `test_component_config` — `"1.50"`,
  `"1.5"`, `"3"`, `"+1.5"`, `"1e3"`, `".5"`, `"5."` accepted; `" 5"`,
  `"5 "`, `"5x"`, `"nan"`, `"inf"`, `"0x10"`, `"1e50"` refused; `"+5"`,
  `"05"`, both `int32` ends accepted and both overflows refused;
  `"1.2.3.4x"`, `"1.2.3.+4"`, `"1.2.3.-0"`, `" 1.2.3.4"` refused as IPs
  and `"1.2.3.04"` accepted; `"80x"`, `"+80"`, `" 80"` refused as ports;
  a redefinition validated once under the new constraints, and one without
  a default keeping the stored value. Removal checks from a fresh `.pio`:
  the round-trip restored fails on `"1.5"` after `"1.50"` passed (and
  accepts `"nan"`); `toInt()` restored in the octet fails on `"1.2.3.4x"`;
  `push_back` restored fails both redefinition cases.

### BUG-38 — HomeAssistant: the alarm control panel's discovery document is longer than the event field, and was published cut [MEDIUM] — **DONE (2026-09-14, filed the same day by OBS Lot D's review)**

- **File**: `HomeAssistant.h` (`mqttPublish()`, the `strncpy` into
  `MQTTPublishEvent::payload`), `HAAlarmControlPanel.h`
  (`buildDiscoveryPayload()`); `MQTT.h:38` (`MQTT_EVENT_PAYLOAD_SIZE = 700`).
- **Problem**: every message the HA component sends crosses the EventBus as
  an `MQTTPublishEvent` whose payload field holds 699 characters. The alarm
  control panel's discovery document — the arm modes, the code fields, the
  command template, the device block — is **774 characters** with the
  default device block and a nine-character node id (measured by
  `test_alarm_panel_add_method`, `test_ha_alarm_panel.cpp`, 2026-09-14). It
  was cut at 699 and published anyway, retained; Home Assistant discards a
  JSON that does not parse, so the panel was never discovered. The bench
  broker still serves a 699-byte panel config from an earlier session,
  cut. **Found because Lot D made the cut a refusal**: the suite's panel
  test had passed for months because ArduinoJson returns the keys parsed
  before the cut and the test asserted only those; it now asserts the
  refusal and the 774.
- **Fix, not done**: shorten the document (Home Assistant's documented
  abbreviations — `stat_t`, `cmd_t`, `dev`, `uniq_id`, `avty_t`, … — take a
  config well under the field, for every entity type at once), or widen
  the field (each `MQTTPublishEvent` grows the EventBus's per-event copy,
  830 bytes today, by the same amount). A decision, not a patch.
- **There are two gates, not one** (found 2026-09-14 by the adversarial
  review of the decision memo): PubSubClient refuses any packet larger
  than its buffer — `MQTT_MAX_PACKET_SIZE`, **768 on ESP8266**
  (`MQTT_ESP8266.h:12`), 2048 on ESP32 — and its test is
  `5 + 2 + strlen(topic) + payload`. The tested panel's topic is 56
  characters, so the 774-byte document is an 837-byte packet: widening
  the event field alone changes nothing on an ESP8266, the refusal just
  moves from the HA WARN to MQTT's `Publish failed! Client state: 0,
  buffer size: 768`. Abbreviated, the same panel is 638 bytes and a
  701-byte packet, which fits with 67 bytes to spare. Reconstructed sizes
  (the reconstruction reproduces the tested 774 exactly): six arm modes
  977 → 812; six modes with a 20-character node id, a configuration URL
  and an area 1081 → 889 — over either gate, abbreviated or not.
- **Fixed, the same day, by the first lever — with the decision memo put
  through the adversarial review first.** Every discovery key is now
  written in the short form Home Assistant documents and expands on
  receipt (`uniq_id`, `stat_t`, `cmd_t`, `avty_t`, `pl_avail`, `dev` with
  `ids`/`mf`/`mdl`/`sw`/`cu`/`sa`, `cod_arm_req`, `sup_feat`, …), for all
  seven entity types and the device block at once — 62 keys in seven
  headers. The review is what settled it: widening the field to 1024 was
  refuted on ESP8266 by the second gate (the 774-byte panel is an 837-byte
  packet against 768), it would have grown every EventBus copy by 324
  bytes on both platforms, and it edits the `MQTT.h` line marianorenzi's
  `esp32-ethernet` carries; the abbreviations touch nothing he rewrites.
  The tested panel is **774 → 638** characters (a 701-byte packet, 67 to
  spare on ESP8266); a panel with six arm modes, a 32-character node id, a
  configuration URL and an area is 974 and stays refused. The refusal is
  now honest: `mqttPublish()` returns `false`, `publishEntityDiscovery()`
  no longer logs "Discovery queued for publish" after it, and
  `HAStatistics::discoveryRefused` counts it, shown on the WebUI detail
  card. Home Assistant reads both spellings, so retained long-key configs
  on a broker are replaced at the next connect with no migration.
- **Verification**: the two exact-string baselines pinned before OBS-5
  are rewritten abbreviated and still pin bytes; `test_alarm_panel_add_method`
  now asserts the two-mode panel reaches the bus whole at 638 with no
  warning; `test_alarm_panel_over_the_event_field_is_refused_and_counted`
  pins the 974-byte refusal, the warning text, the counter and the absence
  of the INFO line. Both numbers were predicted by a reconstruction that
  reproduced the 774, then measured by the tests. On the bench: the
  WROOM-32D reflashed with the FullStack build, its eleven configs came
  back **336–468 bytes (401–568 before)**, every one parses, and the Home
  Assistant container kept all eleven entity ids and took the next
  values (boot 71, uptime 60 s) without a re-creation. FullStack sizes,
  same flags: esp8266dev RAM 54 132 → 54 068 B, flash 733 731 → 734 051 B;
  esp32dev RAM unchanged at 56 872 B, flash 1 359 481 → 1 359 937 B — the
  shorter literals give back 64 bytes of ESP8266 DRAM, and the size check,
  the counter and the card field cost a few hundred bytes of flash.

### BUG-39 — MQTT: a queued message larger than the client buffer stalls the queue for good [MEDIUM] — **DONE (2026-09-14, filed the same day by the adversarial review of BUG-38's decision memo)**

- **File**: `MQTT_impl.h`, `processMessageQueue()` and `enqueueMessage()`.
- **Problem**: `processMessageQueue()` publishes from the front of the queue
  and `break`s on the first `false` without erasing it, so the next
  `loop()` retries the same message. That is right for a transient
  failure and wrong for a permanent one: PubSubClient refuses any packet
  over its buffer (`5 + 2 + topic + payload` against 768 on ESP8266) with
  a bare `false`, and nothing checks the size on the way into the queue —
  `enqueueMessage()` checks only `maxQueueSize`. Since BUG-29, the eleventh
  and later publishes of a connect burst go through this queue
  (`publishRateLimit` = 10), so FullStack's twelve discovery messages
  exercise it on every connect. A discovery document that passes the HA
  component's 699-character gate and still makes a packet over 768 —
  today that takes a node id longer than 17 characters on the alarm
  panel's topic, `MAX_NODE_ID` allows 32 — is queued, refused on every
  loop, and every state update queued behind it waits forever. The only
  trace is `Publish failed! Client state: 0, buffer size: 768` at ERROR,
  which names no topic and reads as a connection problem.
- **Fix**: one `packetFits()` — `strlen(topic) + payload + 7` against
  `getBufferSize()`, PubSubClient's own test — shared by `publish()`,
  `publishNow()` and `processMessageQueue()`; it logs the topic and the two
  sizes at WARN and counts a `publishErrors`. The queue drops what never
  fits and moves on; a transient `false` still stops the drain as before.
  `publishNow()` had been counting `+5` where the client tests `+7`, so it
  accepted two packet sizes the client then refused — corrected in the
  same edit, with the MQTT reference's arithmetic.
- **Verification**: `test_an_oversized_queued_message_is_dropped_and_the_queue_keeps_draining`
  queues three messages offline, the middle one 1100 bytes against the
  stub's 1024-byte buffer, connects and loops: the last one reaches the
  client, `publishCount` 2, `publishErrors` 1 and not 2 on the next loop.
  Mutation checked: with the drop removed the suite reports the failure
  (the last message never leaves, the count stays at 1).

### BUG-37 — OTA: an HTTP firmware upload dies with a client-side broken pipe whenever the link is quiet for 3 s [MEDIUM] — **DONE (2026-09-05)**

- **Opened by**: OBS-7's residual-6 measurement, which needed a full OTA
  cycle on the WROOM-32D and never got one. Filed in the morning, fixed the
  same day.
- **Measured at filing**: three uploads of a 1.33 MB image through
  `tools/on-device/ota_upload_check.py` against a FullStack build on the
  WROOM-32D: all three ended in `BrokenPipeError: [Errno 32]` on the client
  while the device logged `Client disconnected mid-upload` — at about 30 %,
  roughly 20 KB/s, some 25–30 s in. Two runs with the loop watchdog at 30 s,
  one at **0**: identical, so this is not OBS-7. The second real-conditions
  campaign saw the same shape on 2026-09-01 — "an upload to the WROOM-32D
  died with a broken pipe at 12%" — and filed the *consequence* (BUG-35, the
  updater staying locked); the cause of the disconnect was never established.
- **Cause, measured**: **ESPAsyncWebServer 3.12.0 arms a 3 s receive-idle
  timeout on every accepted client** (`WebServer.cpp:50`, `setRxTimeout(3)`)
  and clears it only when a response starts (`WebRequest.cpp:1053,1063`), so
  it runs for the whole body of an upload; AsyncTCP 3.5.0's 500 ms poll
  closes the connection the first time `now - _rx_last_packet` reaches it.
  (The entry as filed said "ESPAsyncWebServer 3.5.0" — 3.5.0 is AsyncTCP's
  version.) **And this link is quiet for that long on its own.** A streaming
  probe that sampled the client socket twice a second (`ss -tin`) during an
  unfixed upload shows the last seconds before the close: a lost segment, the
  retransmission timer backing off 611 → 1 222 → 2 444 ms, `lost:5`, and the
  last ACK 2.9 s old at the sample before the socket went `CLOSE-WAIT` — the
  device's FIN. The device logged the disconnect 39.3 s after `Upload
  started`; the client's last data had gone out ~3.5 s earlier. A second
  natural run died the same way at 68 % during the *fourth* backoff (RTO
  5 888 ms), with 148 retransmissions on the session by then. Five natural
  uploads in six died like this across the day; the sixth got through by not
  losing a segment at the wrong moment.
- **Made deterministic, and the first attempt was vacuous**: a probe that
  stops sending for 4 s at body offset 200 000 was expected to kill the
  upload and did not — the kernel kept draining its 36 KB socket buffer
  through the "pause" and the device never saw silence. With a 4 KB
  `SO_SNDBUF` (8 KB once Linux doubles it) and a wait for `ss` to report
  nothing unsent or unacked before the pause, **4 s of real silence closed
  the upload every time (2 of 2 on unfixed code), 2 s did not.** That
  drained pause is now `ota_upload_check.py --silence SECONDS`; the drain is
  the check, and the script watches the socket during the silence so a
  close is timed from outside rather than read off a serial log.
- **"Discriminate against `OTAWithWebUI` first" — done, and it was not the
  discriminator.** The minimal example on the same board shows the same link:
  idle ping 8.8 / 126.6 / 609.8 ms (min/avg/max) against FullStack's
  8.0 / 114.2 / 307.4, 52 retransmissions in one upload, backoff episodes of
  one step. That upload happened to complete, at 40 KB/s; nothing in it
  went quiet for 3 s. The components are not the cause; the server's idle
  limit meeting the link's retransmission gaps is.
- **Fix**: `OTAConfig::uploadIdleTimeoutSec` (default **30**) and one call
  in `OTAWebUI.h`'s upload handler at `index == 0`, after the CSRF and auth
  gates so an unauthenticated client gets no longer a hold than before:
  `request->client()->setRxTimeout(uploadIdleTimeoutSec)`. The response
  path resets it to 0 as it always did. **Not 0 by default**: a client that
  vanishes without a RST would otherwise leave the update open, which is
  BUG-35's lock through another door — `--silence 35 --expect-abort` pins
  that the ceiling still exists.
- **Red then green on the WROOM-32D, same script both times**: `--silence 4`
  FAIL on unfixed code ("the device dropped the upload after 4s of
  silence"), PASS on fixed (upload completes, refused at the hash as
  intended); `--silence 35 --expect-abort` PASS — the device closed **29.9
  to 30.1 s into the silence as timed from the host** over three runs (the
  serial log says 30.5 s after the last logged chunk) and `lastResult`
  reads `Client disconnected mid-upload`. **The field is wired, not a
  constant**: an `OTAWithWebUI` build with `uploadIdleTimeoutSec = 5` on
  the nodemcuv2 closed **4.9 s** in (`--silence 9 --expect-abort
  --idle-timeout 5`), the default build 29.9–30.0 s — a tenth under is the
  device stamping its clock at the last packet it processed, before the
  host has seen its queue empty. Then three natural uploads of the 1.35 MB FullStack image,
  **3 of 3 complete** (73 s, 64 s, 25 s — each with backoff episodes that
  used to be fatal), and one `--commit`: installed, rebooted (Boot #28,
  `Software reset`), answered again. **The first full HTTP OTA cycle this
  board has completed since the campaign began.** **ESP8266, same script on
  the nodemcuv2** (`OTAWithWebUI` esp8266dev, a 476 KB image): `--silence 4`
  FAIL on the unfixed build, PASS on the fixed one, `--silence 35
  --expect-abort` PASS — ESPAsyncTCP 2.0.0 has the same `setRxTimeout`
  semantics (`_poll`, 500 ms) and the same 3 s from the same server code, and the
  board confirmed it rather than the reading. The adversarial review of the
  diff asked for that run; the entry had said "compiles, not run" with the
  board plugged in. Native: 54/54, with the default and the `setConfig()`
  round trip of the new field now pinned in `test_ota_config_defaults` /
  `test_ota_config_get_set` (no native test can reach the handler itself —
  `OTAWebUI.h` is not compiled natively, TEST-8's family). **The second
  review, the code-review skill's three layers, found what the script
  itself could get wrong**: a close arriving *before* the silence began
  would have been reported as the idle limit's doing — and one such close
  happened on the WROOM during the re-measurement, on fixed code, cause
  unrecorded (no serial was being read); the script now names the phase
  and calls that run "never ran" rather than a verdict. It also found that
  `--expect-abort` passed on unfixed code (any close plus the abort reason
  satisfied it): the script now times the close from the host and bounds
  it, so the unfixed 3 s fails it and the wired value is what is measured.
- **Why MEDIUM, as filed and unchanged by the fix**: OTA over HTTP to the
  flagship configuration did not complete on the bench, three times out of
  three; BUG-35 made that recoverable, not successful. Not HIGH: the URL
  path (`installFromUrl`) was unaffected, a retry cost nothing but time, and
  no image was ever half-committed — the abort ran before `end(true)` every
  time.
- **Not fixed, recorded — the link itself.** Throughput is 20–54 KB/s
  because the device's receive window is lwIP's default 5 760 bytes and
  the round trip averages 114–127 ms idle (max 307–610 ms), so
  window / RTT is the ceiling; and the link loses segments by the dozen per
  megabyte. The RTT profile is what WiFi modem sleep looks like, and the
  Arduino core's default is `WIFI_PS_MIN_MODEM`; nothing in `Wifi_ESP32.h`
  sets it either way. **Unmeasured**: one build with `WiFi.setSleep(false)`
  would say whether that is the RTT, the loss, both, or neither. Recorded
  with these figures rather than filed at the time; LO-35 since 2026-09-20.
- **The release that ships this must say**: `OTAConfig::uploadIdleTimeoutSec`
  is new public API, and every upload through `OTAWebUI` now tolerates 30 s
  of client silence where it tolerated 3 s — a default behaviour change.
- **Refs**: BUG-35, SEC-9, TEST-8; `tools/on-device/README.md`.

### BUG-36 — Core: `EventBus::enqueue` never decrements `pendingByTopic` for the event it drops on overflow [MEDIUM] — **DONE (2026-09-06, Lot B)**

- **Opened by**: the observability prioritisation of 2026-09-05, from a
  finding STOR-ESP-1's withdrawal had left in
  `docs/deferred-work.md` with no roadmap
  identifier. **Filed, not fixed** — it belongs with OBS-3's lot, whose
  recorder wants the drop counter this fix has to add.
- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`,
  `enqueue()` (`:251-267`), `poll()` (`:218-222`), `subscribe()` (`:60-67`).
- **Problem**: the queue is capped at 32. On overflow `enqueue()` pops the
  oldest event and pushes the new one, then increments `pendingByTopic` for
  the new one — and never decrements it for the one it dropped. `poll()`
  decrements only for events it dispatches. So every dropped event leaves
  its topic's counter one higher, permanently. `subscribe(…, replayLast)`
  reads that counter and **skips the sticky replay whenever it is above
  zero**, so any topic that has ever had an event dropped — including a
  topic that merely happened to be oldest when another topic stormed the
  queue — never replays again for the life of the process.
- **Why it matters here**: this is a defect that grows with uptime and shows
  as "the device behaves differently after a while", the family the
  observability work exists to make visible. The drop itself is silent
  (LO-5); the counter drift is its lasting trace.
- **Fix**: decrement the dropped event's topic before popping it, erase the
  entry at zero (the second deferred-work item), and count drops — per bus
  and per topic — where OBS-3's record and LO-5's log line can read them.
  Native test: fill past the cap on topic A with a B event oldest, drain,
  subscribe to B with `replayLast` and require the replay; run it against
  the unfixed code first.
- **Fixed 2026-09-06 in Lot B (S2)**: `enqueue()` releases the dropped
  event's topic before popping it, entries are erased at zero (in `poll()`
  too), and a **per-bus** drop counter — not per topic, as this entry
  asked: a `String`-keyed map for a diagnostic is declined on the ESP8266
  — that `reset()` clears, the flight recorder stores in its record
  (high half of the boot-sequence word) and `Core::loop()` logs at most
  once a minute (LO-5's silent drop, no longer silent). The replay test
  the entry prescribed read **"Expected 7 Was 0"** against the unfixed
  code; `test_eventbus` 25 → 27. Drops stayed at 0 through the board
  campaign.
- **Refs**: the two STOR-ESP-1 entries deferred-work carried until 2026-09-20; LO-5.

### BUG-34 — WebUI: `/api/ui/schema` truncates when the serializer cannot make progress [MEDIUM] — **DONE (2026-08-31)**

- **Filed and fixed**: 2026-08-31, SIZE-1's lot — found by the extraction that
  deduplicated the schema chunk-assembly loop, which existed once per route
  lambda and had drifted.
- **File**: `WebUI.h`, the `/api/ui/schema` chunked-response callback.
- **Problem**: v1.5.0 (`b961e43a`, "Fix chunked schema truncation: return
  RESPONSE_TRY_AGAIN instead of 0 when serializer can't write") fixed the
  poll route's `?schema=1` lambda — and `/api/ui/schema`'s copy of the same
  loop never received the fix. A zero return from a chunked callback ends the
  response: truncated, unparseable JSON.
- **Wider than filed, measured natively**: the spec assumed the stall needed
  a buffer smaller than an atomic `\u00XX` escape (six bytes). The new
  `test_schema_chunking` suite found the easy shape: the serializer's
  check-state guard tests the POST-transition state, so any call that starts
  at a check state and exits the check chain returns 0 with a **fully free
  buffer** — at ordinary chunk sizes, on ordinary content, wherever a chunk
  boundary lands right after a completed literal. The sweep asserts these
  transient zeros occur (`zeroReturns > 0` across sizes 6–64); the
  escape-atomicity shape is pinned separately at `maxLen` 1, permanent until
  the buffer grows.
- **Severity MEDIUM, argued**: the shipped frontend has fetched only
  `/api/ui/updates?schema=1` since v1.5.0, so no shipped UI breaks. But the
  route is documented public API (`docs/components/webui/README.md:96`,
  `technical-reference.md:596`), and DOC-1 records it being fetched by hand
  on both an ESP8266 and an ESP32 during the campaign — it works when no
  chunk boundary lands badly, which is what made the drift invisible.
- **Fix**: the retry policy is hoisted into one private helper,
  `writeSchemaChunkHttp()`, and **both** routes call it — the fix does not
  clone the line into the second lambda, which would re-create the
  drifted-duplicate shape that caused this. `SchemaChunkState::writeChunk()`
  itself stays policy-free: retry-vs-end belongs to HTTP, not serialization.
- **The removal check is structurally silent, stated rather than hidden**:
  no native test compiles `WebUI.h` (ESPAsyncWebServer is `lib_ignore`d) and
  no on-device suite fetches the route over HTTP — verified against both
  `test_schema_memory` and `test_heap_esp8266`, which each *simulate* the
  chunking. The pin is at the core (both stall shapes reproduced and
  recovery asserted in `test_schema_chunking`); the wrapper's mapping to
  `RESPONSE_TRY_AGAIN` rests on inspection and on the poll route's identical,
  field-proven line. An HTTP-level fetch of `/api/ui/schema` joins TEST-8's
  family of envelope-level gaps.

### BUG-33 — WebUI: the streaming escaper's control-char test depends on char signedness [LOW] — **DONE (2026-08-31)**

- **Filed and fixed**: 2026-08-31, SIZE-2's lot — surfaced by that spec's
  adversarial review, which asked what the escaper does to a byte ≥ 0x80.
- **File**: `StreamingContextSerializer.h`, both `writeJsonString` overloads
  (now the one core in `WebUI/JsonStreamWriter.h`).
- **Problem**: the control-character check was `char c = …; if (c < 0x20)`.
  With signed `char`, every UTF-8 continuation byte sign-extends negative and
  is emitted as `\u00XX` of its low byte: `"é"` came back from a parse as
  `"Ã©"`, `"°C"` as `"Â°C"` — for titles, labels, values, units, options and
  option labels alike.
- **Severity LOW because no device was ever affected, and that is measured,
  not assumed**: `__CHAR_UNSIGNED__` is defined by all three shipped
  toolchains (xtensa-esp32, xtensa-lx106, riscv32-esp) and absent on host
  gcc. The divergence was host-only — which means a native UTF-8 test
  written before this fix would have pinned behaviour no board has. The
  danger was to the test suite's fidelity, not to production.
- **Fix**: `static_cast<unsigned char>(c) < 0x20` — one line, making host
  match boards. Pinned by `test_utf8_survives_serialization` (title, unit
  and a multiselect value round-trip through a parse), which was **red
  natively before the fix** and describes what every board already did.
  It opens and shuts inside its lot, so no severity column moves for it.


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

### BUG-51 — WebUI: a refused settings field either shouted or said nothing [MEDIUM] — **DONE (filed and fixed 2026-09-20)**

- **Problem**: a settings field had one way to report a refusal —
  `sendUICommand()`'s `alert(data.error)` — so a provider chose between a
  blocking modal and silence. **Twenty-six** bare `{"success":false}` returns
  across **seven** providers were therefore invisible (RemoteConsole 7,
  HomeAssistant 4, LED 4, OTA 4, SystemInfo 3, Wifi 3, Storage 1). The other
  fourteen answers, spread over **four** providers — MQTT 5, NTP 5, the WebUI's
  own settings 3, Wifi 1 — named their reason and so popped a modal that also
  held the page's own live updates until it was dismissed. The filing said two;
  the tree says four, which one grep of `\"error\":` settles.
- **Not an oversight on either side.** `ba901c4` chose silence for
  `SystemInfoWebUI` deliberately, and `HomeAssistantWebUI` carried the same
  note. The recorded reason was circular — *no error key, because an error key
  would pop a modal* — so the decision was a channel, not a payload.
- **Fix**: the reason is written under the field the value was typed into, on
  the card when no field is in cause; `alert()` is gone from that path. It is
  cleared when the next attempt starts, on entering edit mode and on cancel —
  **never on a redraw**, which happens every tick and would wipe the message
  within a second. All twenty-six refusals name a reason, from a shared
  vocabulary: `Component not available`, `Method not allowed`, `Invalid
  request`, `Unknown context`, `Unknown field`, and a reason of the field's own.
- **Measured on the WROOM-32D, each side of the fix**, on both paths, with
  `webui_settings_refusal_check.py` — which gained the second path here, since
  the one BUG-51 called invisible has no Save button:
  - MQTT port `65536` (Edit/Save): modal `Invalid port` → the same words under
    the field, no dialog.
  - OTA Start Update with no URL (posts on change): modal → message under the
    button.
  - SystemInfo empty device name, one of the twenty-six: **nothing at all** →
    `Device name cannot be empty`.
- **Left where it was, deliberately**: `WebUIField::range()` cannot be enforced
  in the browser as the entry proposed. `minValue`/`maxValue` ship for every
  field but default to 0 and 100, so a page cannot tell "no range" from
  "0..100" and would refuse a port of 1883; doing it needs a third state on the
  wire, on every context. And `applySave()` still posts field by field, so a
  refused `port` does not undo a `broker` stored a moment earlier — that is the
  edit-mode-versus-post-on-change question, still open.
- **The adversarial review found what the fix had broken**, and it is in the lot:
  a transport refusal (a 403 from the per-boot CSRF token after a reboot, a 401,
  an unreachable device) was still silent, since the message was drawn only
  inside `if (resp.ok)`; a provider answering `success:false` with no reason at
  all — a third-party one, or the WebUIOnly example — still showed nothing;
  `clearActionErrors(field)` removed the card banner against its own comment;
  BUG-54's baseline collapsed a multi-select to one option; a refused *select*
  kept displaying the value the device had refused, the shape SEC-14 fixed for
  checkboxes; and the message carried no `role="alert"`, so what replaced an
  unmissable modal was silent to a screen reader. Four providers reported an
  unknown context or a bad method as `Unknown field`, against the vocabulary the
  reference had just written down.
- **Cost**, measured on the FullStack example: ESP8266 **+1 568 B flash, +160 B
  RAM**; ESP32 +1 596 B flash, no RAM; ESP32-C3 +1 648 B flash. The RAM figure
  is smaller than MEM-7 would predict for ~700 B of new literals, and the ELF
  says why: the same refusal string sits once in `.rodata` (DRAM, 0x3ffe86d0)
  and eight times in `.irom0.text` (flash, 0x40201010). Where a literal lands is
  per translation unit on this core, not universal — worth knowing before MEM-7
  is taken.
- **Also measured on the board**, with the request refused at the browser rather
  than by the device — one that vanishes takes the page's own state with it, so
  it measures the reboot: a 401 draws `Authentication required` and an aborted
  fetch `Device unreachable`, both under the field, about 1.5 s after Save.
- **Refs**: BUG-45 (which filed it), BUG-32 and BUG-31 (the providers whose
  comments recorded the standing choice), SEC-14 (the checkbox revert, the only
  refusal the UI drew before).

### BUG-52 — Accept, discard, report success: the shape BUG-45's rule did not reach [MEDIUM] — **DONE (filed and fixed 2026-09-20)**

- **Problem**: BUG-45 removed that shape from every *numeric* settings field.
  Three non-numeric surfaces kept it.
  1. **The telnet `level` command** parsed with `toInt()`, so `level 3x` set 3
     and `level abc` set `0` — `LOG_LEVEL_NONE`, logging off — and answered
     `Log level set to: 0`. It also refused anything above **4** while
     `RemoteConsoleWebUI` accepts **0-5** and `/api/console/loglevels` lists six.
  2. **`LEDWebUI`'s `effect`**: any unknown string became `Solid`, answered
     success.
  3. **`LEDWebUI`'s `led_select`**: an unknown name left the selection where it
     was, answered success.
- **Fix**: `ComponentConfig::digitsOnly` — the rule BUG-40 wrote, now public
  rather than copied a fourth time — guards the command, whose range becomes the
  Logger's own 0-5, help text included. The LED handler refuses an effect or a
  name it does not know, and `stringToEffect` becomes `parseEffect`, so the
  fallback is gone from the code rather than guarded at one call site.
- **One of the two tests that pinned the old behaviour was vacuous**:
  `test_an_unknown_effect_name_falls_back_to_solid` started from Solid, so it
  passed with the fallback *and* with the refusal. Both are rewritten to assert
  the answer and start from a state the refusal has to preserve.
- **Tests**: four new cases on the real telnet dispatch
  (`test_system_lifecycle` already drives it), three rewritten or added on LED,
  four removal checks.
- **Refs**: BUG-45 (which found all three), BUG-40 (the rule they were outside
  of).

### BUG-53 — NTP: an interval below an hour could be stored, shown as 0, and not saved back [LOW] — **DONE (filed and fixed 2026-09-20)**

- **Problem**: the settings field was denominated in hours and `getWebUIData()`
  rendered `cfg.syncInterval / 3600`, so a stored 1000 seconds showed as **0** —
  and since BUG-45 that displayed 0 was refused on save. The round trip was
  broken before too, silently: saving 0 was accepted and discarded.
- **Arbitrated (a) of three**: the field is denominated in seconds, 3600 to
  4294967, which is what the component stores. (c) — holding a short interval at
  one hour wherever it comes from, symmetric with the ceiling BUG-45 added — is
  the tidier rule and silently moves a value the public API accepts; (b) needs a
  string where the field is a Number.
- **Measured on the WROOM-32D**: the field shows 3600, `1000` is refused with
  `Interval must be 3600-4294967 seconds`, `7200` is stored and read back.
- **Tests**: the ceiling case re-expressed in seconds, a floor case, and a
  display case for an interval the page cannot type but `NTPConfig` accepts —
  which is the defect itself. Two removal checks, one per half.
- **Still open, same family**: a port or interval **already persisted** out of
  range by the old handlers is never repaired — validation sits on the write
  path only. NTP is the exception, at the ceiling only.
- **Refs**: BUG-45 (which made the display defect visible).

### BUG-54 — WebUI: Cancel restored nothing [LOW] — **DONE (filed and fixed 2026-09-20, in BUG-51's lot)**

- **Problem**: `enterEdit` saved its baseline with `card.querySelector('#' +
  f.name)` and `cancelEdit` read it back the same way, while fields render as
  `${contextId}_${field.name}`. No static id in `index.html` collides with any
  field name, so **the baseline was always `{}`**: Cancel emptied the pending
  buffer and left what had been typed on screen. Nothing was ever stored — the
  next tick's redraw undid it, and not at all while the field held focus.
- **Found by reading `app.js` for BUG-51**, not by a report: the defect is
  display-only, which is why nobody filed it.
- **Measured on the WROOM-32D, each side**: type `Zzz`, click Cancel, read the
  field at once — `Zzz` before, the stored name after. The assertion is taken
  immediately, because eight seconds later the tick makes both look identical,
  which is what a slower check would have concluded.
- **Refs**: BUG-51 (the lot), BUG-45 (the placeholder-until-the-first-tick
  measurement that says why a redraw cannot be the remedy).

### BUG-55 — WebUI: renaming the device from the page header has never worked [LOW] — **DONE (filed and fixed 2026-09-20, in BUG-51's lot)**

- **Problem**: `makeDeviceNameEditable()` posts to contextId `system_info`,
  which `SystemInfoWebUI` does not handle — its only arm is `system_settings`.
  The rename fell through to a refusal, the header painted the new name anyway,
  and the next tick put the old one back. Every test posts `system_settings`, so
  nothing saw it.
- **Found by BUG-51's own review**, which caught it as a *regression this lot
  would have shipped*: once the fall-through names its reason, that silent
  no-op becomes a permanent red `Unknown field` banner on the Device
  Information card — a different card, on a different tab, cleared by nothing,
  since a dashboard card has neither edit mode nor a listener that could clear
  it.
- **Fix**: the header posts `system_settings`, the context that renames. A test
  posts `system_info` and asserts the refusal names the *context*, then renames
  through the right one and reads the name back off the component.
- **Refs**: BUG-51 (the lot, and the reason it became visible).

### BUG-56 — WebUI: a refusal message outlives its cause [LOW] — **NEW (2026-09-20, by BUG-51's review)**

- **Problem**: a message is cleared when the next attempt starts, on entering
  edit mode and on cancel. It is deliberately not cleared by a redraw — the card
  redraws every tick and would wipe it within a second — so after a refused save
  the reason sits under a field the tick has already put back to a valid value,
  with no way to dismiss it. A multi-field save can leave several at once.
- **The decision**: a close control, a lifetime, or clearing on the next *edit*
  of that field (an `input` event, which the buffering listener does not use
  today). Each is a different answer to "how long is a refusal true for".
- **Refs**: BUG-51 (which chose the rule), SEC-14.

### BUG-57 — A boolean settings field takes anything and answers success [LOW] — **NEW (2026-09-20, by BUG-51's review)**

- **Problem**: the accept-discard-report-success shape BUG-52 removed from three
  surfaces survives on the boolean ones, and the two providers disagree about
  the vocabulary. `LEDWebUI`'s `enabled_toggle` is `value == "true"`, so `"1"`
  and `"on"` turn the LED **off** and answer success; `WifiWebUI`'s
  `wifi_enabled` accepts `"true"`, `"1"` and `"on"`, and NTP's `enabled` reads
  anything else as false — its suite pins that as "not a refusal".
- **Reachable** by a hand-made POST only: the page renders a checkbox and sends
  `true`/`false`. That is why it is LOW and not part of BUG-52.
- **Refs**: BUG-52 (the shape), BUG-45 (the numeric half of the same rule).

### BUG-58 — LED: the dropdown's names are latched, the refusal reads the live ones [LOW] — **NEW (2026-09-20, by BUG-51's review)**

- **Problem**: `getCachedNames()` latches the LED names at the first schema
  build and nothing invalidates it; `led_select` now matches the posted value
  against `led->getLEDNames()`. An `addLED()` or a rename after the schema is
  built makes the two lists disagree, and the page then offers a name its own
  handler refuses with `Unknown LED`.
- **Before the refusal existed the same drift was silent**: the selection simply
  did not move. Filing it is the cost of making the refusal visible.
- **The decision**: match the list the page was given, or invalidate the cache
  when the LED list changes and rebuild the schema.
- **Refs**: BUG-52 (which added the refusal), BUG-51.

### BUG-60 — Core: the EventBus is mutated from the async web task and has no lock [HIGH] — **DONE (2026-09-22, filed the same day by SEC-16's board run)**

- **Files**: `EventBus.h:319-346` (`enqueue`, and `pendingByTopic[...]` at the
  end of it), `:238` where the single-threaded assumption is written down;
  `Storage.h:303` (`putBool` emits), `SystemWebUISetup.h:450` (the
  `webui_settings` save), `WebUI.h:748` (`handleUIAction`).
- **Measured on the WROOM-32D**: `Guru Meditation Error: Core 0 panic'ed
  (LoadProhibited)`, `EXCVADDR 0x00000001`, on an ordinary settings write.
  Backtrace decoded against its ELF: `String::compareTo` ← `std::map<String,
  int>::operator[]` ← `EventBus::enqueue` ← `publish<StorageChangedEvent>` ←
  `StorageComponent::putBool` ← the settings save ←
  `AsyncCallbackWebHandler::handleRequest`. **The core number is the finding**:
  `loopTask` runs on core 1, so the task that crashed inside the queue is not
  the one that drains it.
- **It is the shape, not the route.** Every WebUI settings write publishes from
  the `async_tcp` task while `Core::loop()` polls the same queue and the same
  map from `loopTask`; the header states the single-threaded assumption and
  nothing enforces it. OTA is not involved: the backtrace holds no OTA frame,
  and the code SEC-16 changed is not on this path.
- **Intermittent**: once in two runs of the same script that same morning —
  which is what a race looks like, and why the crash that is visible matters
  less than the corruption that is not.
- **HIGH**: an unhandled panic on a documented operation of the settings page,
  with a silently corrupted map as its other outcome.
- **Reproduced on demand first**, because a crash seen once in two runs proves
  nothing about a fix: `tools/on-device/probes/eventbus-race` publishes from a
  task pinned to core 0 at the web server's own priority while `poll()` runs on
  the application core, over sixteen topics so the map is a tree. Unfixed, the
  WROOM-32D boot-loops before printing its first line, and the two decoded
  backtraces are the mechanism itself — `_Rb_tree_insert_and_rebalance` from
  `enqueue()` against `_Rb_tree_rebalance_for_erase` from `poll()`.
- **Fix**: the queue, the sticky store and the pending map move under a
  recursive lock — `HAL::Platform::RecursiveLock`, a statically allocated
  FreeRTOS mutex on ESP32 and nothing at all on the ESP8266, which has one
  execution context (Constitution IX: the platform header carries the fact).
  **Handlers run outside it**, so a publisher on the web task is never held
  behind a slow subscriber; a handler that publishes re-enters.
- **Measured after**: the same probe, 89 s, **1 883 273 publishes and 1 368 536
  dispatches, no panic, heap flat at 329 004 B**. Native: three cases on the
  counting stub lock (a publish takes it, `poll()` takes it and has released it
  before the handler runs, a re-entrant publish leaves it balanced), the bus
  suite 37 → 40 cases. Cost on FullStack esp32dev: **+1 336 B of flash, no
  static RAM**; on esp8266dev the image is byte-identical, the bus itself
  growing 172 → 176 B — which is the size the Storage device suite asserts, the
  place a layout check belongs.
- **What is still the loop's**: subscribing. `poll()` reads the handler list
  outside the lock, and the asserts still refuse subscription during dispatch.
  The reference says so.
- **Refs**: BUG-41 and BUG-36 (the same queue's accounting), BUG-30 (its
  payload contract), MEM-8.

### BUG-59 — ESP32: the first async scan of a process in AP mode is never delivered [MEDIUM] — **NEW (2026-09-21, by BUG-47's board runs)**

- **Files**: `Wifi_ESP32.h:68` (`scanNetworks`), against the Arduino core's
  `WiFiScan.cpp` (`_scanTimeout = max_ms_per_chan * 20`, so 6 s by default).
- **Problem**: on the WROOM-32D in AP mode, the first `scanNetworks(true)` of a
  process always ends as `WIFI_SCAN_FAILED` at 6.0-6.5 s; every scan after it
  succeeds in 4-5 s. So the WebUI card's first click reports `Scan failed` and
  the second works. Reproduced on five consecutive boots. **AP mode is the
  condition**: the same board joined to a network answers its first scan, which
  is how the card was driven in a browser.
- **What it is not**, each ruled out on the board with a one-line edit to the
  probe or the ESP32 HAL, none of them kept in the tree: not the AP having just
  started (a scan deferred to t+10 s fails identically); not the station
  interface coming up inside the scan (`WiFi.enableSTA(true)` before the call
  does not help, and hoisting it made a second scan fail too); not the core's
  6 s deadline alone (with a 500 ms dwell, so a 10 s deadline, the first scan
  still ended at 7.0 s — that one is an abort, neither bit set, not a timeout).
- **MEDIUM**: the first use of the scan card fails on one of the two platforms,
  visibly and every time. It is recoverable by clicking again and the page names
  the reason, which is the argument for grading it below BUG-47.
- **The decision**: an automatic single retry in the component (which shows
  `"Scanning..."` for ~11 s), a longer dwell in the ESP32 HAL, or a warm-up scan
  at AP start. Any of the three needs a measurement on the board, since four
  hypotheses have already been refuted there.
- **Arbitrated 2026-09-21 (test-architect brief, maintainer validated)**: one
  automatic retry in the component, plus the deadline below. The longer dwell is
  already measured not to work and would slow every ESP32 scan; a warm-up scan
  at AP start burns six seconds of radio on every boot for a fault one click
  recovers from. **Do the free measurement first**: `startScanAsync()` discards
  the return of `HAL::WiFiHAL::scanNetworks(true)` — log it. If it is `-2` the
  SDK never started the scan, the six seconds are our own poll, and the fix is
  propagating that return rather than retrying. **The discriminating run**: five
  boots, two arms in the same room, retry on and retry off by build flag; the
  remedy is proved only by attempt 1 returning `-2` and attempt 2 returning
  entries *on the same boot*. A boot whose first attempt succeeds proves nothing
  and must be discarded, not counted.
- **And a deadline belongs with it.** `scanInProgress` is released only by an
  answer from the SDK. This platform answers `-2`, so the card recovers — but a
  scan that is never answered at all would leave the summary at `"Scanning..."`
  and refuse every later scan, which is BUG-47's symptom from another cause.
- **Refs**: BUG-47 (which surfaced it), MEM-6.

### BUG-47 — Wifi: the WebUI scan cannot complete in AP mode, and its result is displayed nowhere [MEDIUM] — **DONE (2026-09-21)**

- **Problem, in three halves; the filing described two.** `WifiComponent::loop()`
  returned at the empty-SSID check forty-six lines before the `scanInProgress`
  poll, so a scan started during AP provisioning — the one state with a reason
  to scan — was never harvested: the summary stayed `"Scanning..."` and the flag
  never cleared, which refused every later scan for the life of the component.
  Nothing displayed the result either: `WifiWebUI` kept its own
  `lastScanSummary`, wrote `"Scanning..."` into it and never read it back, and
  the component's `getLastScanSummary()` had no caller outside two suites. **And
  the button did not exist.** `scan_networks` was a field of no context — the
  only one in the repository is a WebUI test fixture — so the handler branch was
  reachable by a hand-made POST and nothing else, while the reference documented
  it as an action of the page.
- **The suite had written the first half into its own fixture.** `makeIdle()` in
  `test_wifi_component.cpp` configures a fake SSID, with a comment stating the
  early return, so that the five async-scan tests reach the code under test at
  all.
- **The scan gets its own always-interactive card.** In a settings card every
  control is locked until Edit is pressed, and the update tick skips a card
  being edited, so a Scan button in `wifi_settings` would have needed Edit to be
  clickable and could not have drawn its result where it was clicked.
  `wifi_scan` carries the button and a Display fed from the component; the
  provider's copy is deleted, leaving one source of truth. A second scan is now
  refused by name — `startScanAsync()` returns `bool` — where it was a silent
  no-op.
- **Not taken**: a Select of the scanned SSIDs filling the `ssid` field is the
  useful shape, and it needs options pushed after the schema was built, which is
  BUG-58's open question. A second answer to it was not invented here.
- **Tests**: twelve, RED first, three removal checks — the poll's placement,
  the card, the data wiring. They are not cleanly one per half: reverting the
  poll fails four cases across both suites. What is load-bearing is the other
  direction, and it holds — one case reads `scan_result` while the summary still
  says `"Scanning..."`, so the display is proved with the harvest reverted.
  1 185 → 1 197 `[PASSED]`.
- **On the boards, and the removal check is the evidence.** A probe runs the
  component AP-only with no network and no browser
  (`tools/on-device/probes/wifi-scan-ap`), printing the number of entries and
  the summary's length, never its text. On the nodemcuv2: two scans harvested,
  4 then 3 networks, the second call refused each time, heap flat. With the call
  moved back below the early return: `pending=1` for twenty seconds and
  `round=2 started=0` — the SDK had delivered in 2.4 s, so the hang was ours.
- **In a browser, on the WROOM-32D.** The card renders in Settings, the button
  is **enabled with no Edit** — which is what the separate card was for — and
  8.1 s after the click the Display redraws with four entries, 90 characters,
  without leaving the card. `/api/ui/updates` moved with it, from `No scan yet`
  to a five-network summary. `webui_settings_refusal_check.py` gained `--watch`,
  since a button stores nothing of its own. The device was on the LAN: AP mode
  is reachable from the bench only by moving the bench's own network, and the
  mode-dependent half is what the serial probe proves.
- **Cost on ESP8266**: +140 B RAM and +628 B flash for the lot, of which the
  fourth context is +20 B and +188 B, measured by building FullStack with and
  without it; on the heap one `WebUIContext` (148 B) and two `WebUIField`
  (144 B each), plus the delta snapshot, which is the same `LazyState` shape the
  three other contexts already use. The card is 452 of the 15 469 bytes the
  board serves at `/api/ui/schema`; that route has been chunked since BUG-34, so
  there is no buffer for a fourth context to overflow.
- **Four defects in this lot's own code, found by its adversarial review and
  fixed here.** The card answered for fields it does not declare, so a
  credential posted under `wifi_scan` was applied — the shape BUG-55 closed
  elsewhere a day earlier. A scan that found nothing left the summary empty,
  which the card drew as `No scan yet`: a finished scan and a click that did
  nothing read the same, so `getLastScanSummary()` now answers
  `No networks found`. `shutdown()` left a scan in flight holding the SDK's
  result list and the flag that refuses the next one. And the fixture comment
  that documented the defect — in the native suite and in the device suite,
  where it also pointed at a `docs/deferred-work.md` entry the purge had
  removed — still claimed it was live.
- **Opened by the board runs, and open: BUG-59** — on the WROOM-32D the first
  scan of a process in AP mode is never delivered, and the card's first click
  reports `Scan failed`. That is the behaviour the fix now surfaces promptly
  instead of hanging; it is not caused by it.
- **Refs**: MEM-2, MEM-6 (the other scan path), TEST-4, BUG-51 (the refusal
  channel this card uses), BUG-58.

### BUG-48 — MQTT WebUI: editing the Last Will topic leaves every discovery document pointing at the old one [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-43's lot, 2026-09-18.
- **Files**: `MQTTWebUI.h:78-80`, `:238-241` (the settings fields and their
  save); `HomeAssistant.h:111-116` (the reconciliation at `begin()`).
- **Problem**: BUG-43 made the availability topic and the will one topic,
  reconciled from HomeAssistant's side at `begin()` and through
  `setConfig()`. The MQTT settings page exposes `lwt_topic`, `lwt_enabled`
  and `lwt_message` as first-class fields and writes them straight into the
  component, and nothing tells HomeAssistant the will moved — so an operator
  who edits that field reopens BUG-43 from the UI: the retained discovery
  documents advertise a topic no will writes.
- **LOW**: BUG-43's consequence, reachable only by an operator editing one
  field. **A decision, not a patch**: an event, a direct call, or refusing
  the edit while a HomeAssistant component is present.
- **Refs**: BUG-43, BUG-45 (the same settings page).

### BUG-49 — MQTT: what a dropped link still gets wrong after BUG-44 [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-44's code review, 2026-09-20.
- **Files**: `MQTT_impl.h` (`resetReconnect()` `:223`, `updateStatistics()`
  `:608`, `announceConnectionLost()` `:115`); `MQTT.h` (`MQTTStatistics`).
- **Three leftovers, one file**:
  1. `resetReconnect()` is the last silent exit from Connected — it sets
     `Disconnected` without touching the client or emitting, so a caller
     using it after changing the broker leaves every subscriber believing the
     link is up, and the next `connect()` sends a second `mqtt/connected` with
     no `mqtt/disconnected` between. Public API; nothing in the tree calls it.
  2. `MQTTStatistics` counts connections and never losses, so neither the
     WebUI nor the telemetry can see a link flap; and `uptime` is refreshed
     only while connected, so it freezes at its last value and reads as live
     for the whole outage.
  3. the loss is announced once per drop, and a `mqtt/disconnected` the
     EventBus drops on a full queue is never re-sent — HomeAssistant's
     `mqttConnected` then stays true for the life of the process. Sticky
     publication would fix it and changes replay for every subscriber.
- **Fix**: (1) announce, or close the session first — a design question;
  (2) a loss counter and an uptime that stops; (3) decide sticky or not.
- **Refs**: BUG-44, BUG-46, ADR 0001.

### BUG-50 — OTA: a chunk-time refusal reaches the operator as "Upload not active" [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: SEC-9's adversarial review, 2026-08-27.
- **Files**: `OTAWebUI.h:509-516`, `OTA.cpp:262`, `:274`.
- **Problem**: a failing `acceptUploadChunk()` — the streaming cap, SEC-8 —
  sets `uploadState.error = "Firmware too large"` and closes the session, but
  leaves `uploadState.rejected` false, so every following chunk re-enters
  `acceptUploadChunk()` on a closed session and overwrites the message with
  "Upload not active", as does the `final` call. The JSON answer names a cap
  violation as an inactive session. The begin-time refusal got this right
  (`:504`); the sibling call did not.
- **Fix**: set `rejected` on the chunk-time failure too. TEST-8's mocks can
  drive it natively — announce N, deliver more than N.
- **Refs**: SEC-8, SEC-9, TEST-8.

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

### TEST-4 — WiFi: superficial coverage [HIGH] — **DONE (2026-08-31)**

- **Ref**: WIFI-F13
- **Problem (as filed)**: Key untested paths: STA fallback timer, AP mode, scan failure handling, reconnection logic.
- **One path left the list on 2026-08-29, and the item stayed open.** MEM-2's
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
- **The other three paths closed on 2026-08-31, and the blocker was the stubs,
  not the tests.** Every remaining path reads state the native stubs could not
  script: the clock was the wall clock (a 15 s timeout test would take 15 real
  seconds), the heap was frozen at 65536 (the fallback ladder's
  2000/2500/3500/6500/10000 guards are all "heap too low" branches, so **none
  was reachable on any platform CI can run**), and the WiFi stub was stateless —
  `getMode()` always `Off`, `startAP()` always `false`, which makes the
  skip-restart branch (`Wifi.h:856`) unreachable and AP success flows
  unobservable. The lot is therefore two seams plus one suite:
  `Platform_Stub.h` gains a scriptable millis/heap/restart-count,
  `Wifi_Stub.h` gains a mode/AP/connect record — every default byte-identical
  to the old stubs, `ForTest` suffix, the pattern MEM-2's scan table set — and
  `test/test_wifi_behaviour/` runs 16 cases: reconnection (success events,
  the 15 s timeout with its **same-tick retry**, deliberately pinned; disconnect
  stops retrying; the 2500-byte connect guard defers then retries), the STA
  fallback timer (arming at low heap, stay-STA-only vs restart-AP on the 6500
  boundary, the 30 s timeout with its anti-boot-loop `autoConnect=false` save,
  reboot-to-STA counted via the restart seam, the <2000 guard proven to make
  zero WiFi-HAL calls, channel-sync vs direct AP+STA), and AP mode (the
  autogenerated open `DomoticsCore-000000` from the MAC, the preconfigured-AP
  branch, the skip-restart branch as a **count delta**, the stale-config guard).
  **Five deliberate mutations were run against the suite and every one was
  caught by exactly the test that names it** (invert the 6500 comparison, drop
  the anti-boot-loop save, stretch the connect timeout, delete the skip-restart
  branch, delete the connect heap guard), `rm -rf .pio` between runs.
- **The contract was adversarially reviewed before implementation, and the
  review refuted two tests as first specified** — the fallback-success-with-AP
  test pinned the wrong branch (heap 20000 never arms the fallback timer; every
  observable it asserted was satisfied by the direct AP+STA path, so the
  removal check would not have moved it), and the skip-restart test asserted a
  total where correct code produces two calls before the scenario starts. Both
  were repaired at spec time. `spec-test-4-wifi-behaviour-coverage.md` v2.
- **The device-suite debt is paid**: `test_wifi_scan_esp8266` ran against a
  real radio for the first time — 3/3 on the `nodemcuv2`, 2026-08-31.
- **What it is not**: radio-level behaviour. Association timing, real AP
  bring-up, channel sync against a physical router remain untested on silicon —
  the suite pins the component's decision logic through `isConnected()`/heap/
  millis, which is host-testable by construction; what a radio adds has no
  assertion a hidden neighbourhood AP cannot corrupt. Filed out of the work:
  **DC-15** — `WifiConfig.reconnectInterval` and `connectionTimeout` are
  accepted and ignored, found while writing the spec's non-goals.

### TEST-5 — NTP: inadequate coverage [MEDIUM] — **narrowed 2026-09-21 by the triage pass**

- **Half of it is covered now**, by suites written since: the timezone path has
  four cases (`test_ntp_timezone_config`, `_presets`, `get_`, `set_`, plus
  `test_ntp_multiple_timezone_changes`) and the wrap-around has one, added by
  BUG-45. **What remains is DST transitions and multi-server failover** —
  `test_ntp_config_servers_update` and `test_ntp_empty_servers` configure the
  list, they do not fail one server over to the next.

- **Ref**: NTP-F9
- **Problem**: Missing: DST transitions, timezone edge cases, multi-server failover, wrap-around scenarios.

### TEST-6 — WebUI-related tests: zero coverage [HIGH] — **DONE (2026-08-31)**

- **Refs**: SI-F4, STOR-F16, LED-F14, HA-F17
- **Problem (as filed)**: `SystemInfoWebUI`, `StorageWebUI`, `LEDWebUI`, and `HomeAssistantWebUI` have zero tests. WebUI providers handle user input (device name, settings, config mutations) with no validation testing.
- **The row was wrong in both directions, and closing it corrected both.**
  `LEDWebUI` was never uncovered — `DomoticsCore-LED/test/test_led_webui/` is a
  dedicated 23-test suite (brightness clamp, effect whitelist, unknown name,
  non-POST refusal, null guard). For `HomeAssistantWebUI` and `SystemInfoWebUI`
  the validation was not untested, it was **absent**, and writing tests without
  fixing would have encoded two browser-reachable defects — BUG-31 (the HA handler
  reads parameters nothing sends) and BUG-32 (a device name corrupts every update)
  — as expected behaviour. So this closed as two lots, not a test-writing exercise.
- **What became of the four Refs:**
  - **HA-F17** → BUG-31 (Lot A): the handler fixed and pinned by
    `DomoticsCore-HomeAssistant/test/test_ha_webui/` (20 tests).
  - **SI-F4** → BUG-32 (Lot B): validation added and pinned by
    `DomoticsCore-SystemInfo/test/test_systeminfo_webui/` (7 tests).
  - **STOR-F16** → `DomoticsCore-Storage/test/test_storage_webui/` (4 tests),
    recorded as coverage: `StorageWebUI` has no input surface to validate.
  - **LED-F14** → already covered by the existing `test_led_webui` suite; the row
    was stale.
- **Filed, not fixed — needs a board, and this was a behaviour lot.**
  `StorageWebUI`, `LEDWebUI` and `HomeAssistantWebUI` do not override
  `hasDataChanged`, so all three inherit `return true` (`IWebUIProvider.h:525`) and
  re-serialize every context on every tick. That is a cost claim, measurable only
  on hardware; recorded here rather than given a bookkeeping ID it cannot yet earn.

### TEST-7 — Core: MemoryManager + ComponentConfig untested [MEDIUM] — **DONE (2026-09-15)**

- **Ref**: CORE-F17
- **Problem (as filed)**: Zero tests for MemoryManager singleton and ComponentConfig validation logic.
- **What the reading found before any test was written.** `ComponentConfig`
  — `defineParameter`, `setValue`, the typed getters, `validate()` — has
  **no caller anywhere in the tree**: no component, no example (the Core
  example's `TestComponentConfig` is an unrelated struct), no test. Only
  `ComponentStatus`, `statusToString()` and `ComponentMetadata` are live.
  It is documented public API with a validation contract (Core reference
  §4, README), which is DC-13's shape; filed as **DC-17** together with
  the eight `MemoryManager` queries nothing calls. And the validators were
  wrong for any sketch that did call them — **BUG-40**, filed and fixed in
  this lot, because pinning them as found would have made the defect the
  contract (TEST-6's lesson).
- **The header did not stand alone**: `ComponentConfig.h` used `String`
  and `HAL::substring` without including `Platform_HAL.h`; it compiled only
  behind `IComponent.h`. `g++ -fsyntax-only` with the header first failed
  at `:59` (`'String' does not name a type`). It now includes the HAL, and
  the new suite includes it first, so CI proves it on every run — a board
  build never would, since no board translation unit includes it first.
- **The native `String` stub was not Arduino where these tests needed it**:
  `toInt()` was `std::stoi` returning `int` (throws on `"abc"` and on
  overflow), `toFloat()` was `std::stof`, `String(float)` printed six
  decimals where both cores print two. No native test had ever called
  `toInt()`/`toFloat()`, so nothing had noticed; but a refusal path cannot
  be tested against a stub that throws, and BUG-40's float defect cannot be
  reproduced against one that prints six decimals. `Platform_Stub.h` now
  parses with `strtol`/`strtof` (`0` on garbage, `long` saturated to the
  boards' 32-bit range) and prints `String(value, 2)`; pinned by
  `test_platform_stub` (5 cases). Every native project was re-run from a
  fresh `.pio` after the change.
- **Suites**: `test_memory_manager` (18 cases) — the lazy first
  `getProfile()` as the first `RUN_TEST` of the process (the singleton has
  no reset), the six threshold boundaries, the cache versus
  `detectProfile()`, `getHeapAtBoot()` as the classified sample,
  `isLowMemory()`/`isCriticalMemory()` live against the cached profile,
  `setThresholds()` reclassifying only at the next detection, every public
  table (`getMaxWsClients`, `getChartHistoryPoints`, `getWsUpdateInterval`,
  `getBufferSize`, `shouldEnable`), and the three boots read from the bench
  logs (a WROOM-32D FullStack boot 276220 → FULL; the nodemcuv2 probe
  26064 → STANDARD; the nodemcuv2 FullStack 15088 → MINIMAL, the CI-14
  boot where the WebUI cut its client limit 3 → 2). *Max Providers* and
  *Heap Check Interval*, two columns of the reference's table, have no
  public accessor and are not pinned — recorded in DC-17.
  `test_component_config` (24 cases) — `statusToString` for all nine and
  the unknown, `ComponentMetadata`, the fluent `ConfigParam`, the typed
  getters with a non-zero default so "parsed to 0" and "fell back" are
  distinguishable, `getBool`'s eight tokens, `validate()` per type with the
  first failing parameter reported, `ValidationResult::toString()`,
  `hasParameter()` as the reference documents it (about values), and
  BUG-40's seven.
- **Verification**: Core native from a fresh `.pio` 154 → **201 cases,
  10 suites**. Six removal checks, each from a fresh `.pio`: `>=` → `>` at
  `fullMin` fails the 30720 case; `getProfile()` detecting unconditionally
  fails the cache case and two others; the `String(float)` round-trip
  restored in `validateFloat` fails on `"1.5"` **after `"1.50"` passed** —
  the defect as filed, reproducible only once the stub printed two decimals
  — and accepts `"nan"`; `toInt()` restored in the octet fails on
  `"1.2.3.4x"`; `push_back` restored in `defineParameter` fails both
  redefinition cases; `std::to_string` restored in the stub fails the
  two-decimal case. No board leg: the changes are host-visible, and the
  board figures are read from existing logs, named as such.

### TEST-8 — OTA: nothing traverses `POST /api/ota/upload` [MEDIUM] — **DONE (2026-09-19)**

- **Files**: `DomoticsCore-OTA/include/DomoticsCore/OTAWebUI.h:405-523`,
  `DomoticsCore-OTA/test/test_ota_http/`, `tests/mocks/libraries/`
- **Found by**: SEC-9, 2026-08-27. Filed when the lot that closed it asked why a
  220-byte defect on the primary upload path needed a human with a browser to find.
- **Problem**: every OTA test called `beginUpload()`, `acceptUploadChunk()` and
  `finalizeUpload()` **directly**, one layer below the upload handler. The gates
  live in the handler — SEC-10's CSRF check at two points, SEC-3's credentials
  check and the state reset that must precede it, SEC-7's rejected flag, BUG-35's
  abort on a vanished client, BUG-37's widened receive-idle limit, SEC-9's
  narrowing of the announced size — and none of them had ever run under test.
- **The title overstated it, and the correction is the useful part.** Our code
  never sees a `multipart/form-data` envelope, on a board either: the server
  parses it and calls the handler with `(request, filename, index, data, len,
  final)`. What the handler sees of the envelope is `contentLength()`, which is
  why SEC-9's 220 bytes exist. So the gap was the gates, not the parsing, and the
  gates are host-testable.
- **Holes 1 to 3 were closed by SEC-9's own lot (2026-08-27)** — `evenIfRemaining`
  below the HAL, `installFromUrl()`'s `end(true)` on an unknown length, and the
  download path's byte count, which reported `getDownloadedBytes() == 0` for a
  completed 8192-byte chunked download on a `nodemcuv2`. Hole 4, the HTTP path
  itself, is what this lot closes natively; `tools/on-device/ota_upload_check.py`
  already covered it on a board, where the discriminating case is a **refused**
  upload (`total` 475264 with SEC-9's narrowing, 475452 without; `downloaded`
  reads 475264 either way and proves nothing).
- **Mechanism**: `WebUI.h` includes `<ESPAsyncWebServer.h>`, so `OTAWebUI.h` did
  not compile natively at all. `tests/mocks/libraries/` now carries mocks under
  the real file names — `ESPAsyncWebServer.h`, `AsyncEventSource.h`, `FS.h`,
  `pgmspace.h` — reproducing the eleven types the WebUI headers use and recording
  what a handler does. Only OTA's `[env:native]` puts the directory on its
  include path. The real `WebUIComponent` and the real `OTAWebUI` then register
  their eighteen routes on the host, and the suite drives the registered lambdas.
- **What it cannot prove, and this is a limit, not a caveat.** The mock is our
  code: a test through it measures handler logic and never the server's own
  behaviour. Multipart parsing, the real `setRxTimeout` and every timing question
  stay with the board script.
- **Ten cases, nine removal checks** — eight per gate plus one that removes two
  guards together. **Two of the ten were vacuous when first written, and the
  removal checks are what said so**, not review:
  - the headline SEC-3 test asserted `sentCode == 200`, but `respondJson` answers
    200 either way and the verdict is in the body. It passed with the reset
    removed. It now asserts `"success":true` and goes red.
  - the disconnect test moved under no single removal, because the handler's
    `uploadState.active` check and `OTAComponent::abortUpload()`'s own
    inactive-session guard (`OTA.cpp:453`) sit **in series**. Removing both gives
    `Expected 0 Was 1`. The test is non-vacuous against the conjunction and
    cannot attribute which guard holds; the handler's is belt-and-braces.
- **Verification**: OTA native 55 → **66** `[PASSED]` lines, fourteen projects
  **1 096 → 1 107**, counted from a full run. The eight other gates each fail
  exactly the test that names them, and the CSRF removals fail two apiece —
  the second being the timeout test, which is the ordering it was written to pin.
- **Found while doing it**: `tests/mocks/MockAsyncWebServer.h` was included by
  nothing and advertised by three documents. It was an invented parallel API
  (`MockHttpRequest`, static members) that never had ESPAsyncWebServer's shape,
  so no test could ever have driven real code with it. Deleted, and the three
  documents corrected.

### TEST-9 — Four WebUI providers cannot be compiled by any native test [MEDIUM] — **DONE (2026-09-19)**

- **Files**: `MQTTWebUI.h`, `NTPWebUI.h`, `OTAWebUI.h`, `RemoteConsoleWebUI.h`;
  `DomoticsCore-{MQTT,NTP,RemoteConsole}/test/test_*_webui/`
- **Opened by**: BUG-31, 2026-08-29, which hit the same thing in
  `HomeAssistantWebUI.h` and fixed it there.
- **Problem**: each includes `DomoticsCore/WebUI.h`, which includes
  `<ESPAsyncWebServer.h>`, so a native test including any of the four failed at
  the first line. That was the mechanical reason four of the six untested
  providers were untested, and it was not a coverage decision anybody made.
- **"Present nowhere in the repository or the host toolchain" was wrong**,
  corrected 2026-09-19 by looking. The package sits in
  `.pio/libdeps/native/ESPAsyncWebServer/` of several projects, sources and all;
  it is `lib_ignore`d in every native environment and needs `Arduino.h`, `FS.h`
  and lwIP, which the host has not. The conclusion held; the reason did not.
- **Closed by TEST-8's mechanism, not by the fix this entry proposed.** That fix
  was to drop the redundant `#include` per file, which would have worked for
  `MQTTWebUI` alone — the other three call `registerApiRoute` inside `init()`
  and need the complete type. `tests/mocks/libraries/` supplies the async server
  under its real name instead, so **no production header changed at all**. Each
  project opts in with one include path and a `lib_ignore` line.
- **The compile problem was the smaller half.** These pages write values into a
  device's configuration and nothing had ever looked at what they accept. Three
  suites — **41 cases**: RemoteConsole 13, NTP 15, MQTT 13. What they cover:
  the two registered routes (`/api/console/loglevels`, `/api/ntp/timezones`,
  both streamed by hand rather than serialised), the port and log-level range
  checks, NTP's comma-separated server parsing with its trimming and its
  dropping of empty entries, the hours-to-seconds conversion, the `"true"`/`"1"`
  boolean coercion that reads every other value as false, MQTT's empty-password
  field that means "unchanged", and `hasDataChanged`, which decides whether the
  UI is pushed anything at all.
- **Four divergences found and filed as BUG-45**, each pinned by a test asserting
  the behaviour as it is with a message telling whoever fixes it that the test
  moves too: MQTT's missing port range check, MQTT's `"success":true` for an
  unknown field, NTP's accepted-then-discarded sync interval, and the `null`
  that six providers return where `IWebUIProvider` promises `{}`.
- **OTAWebUI needed nothing here**: TEST-8's suite already compiles and drives
  it. `MQTTWebUI` remains the one provider with no `init()`, which is why its
  `WebUI.h` include is the only one that could still be removed outright — left
  alone, since the mocks make it harmless.
- **Verification**: fourteen native projects, 1 107 → **1 151** `[PASSED]`,
  0 failed, measured with the generated `WebUIAssets.h` moved out of the tree and
  every `.pio/build` wiped, because a host suite that depends on a gitignored
  artefact passes here and fails on a clean checkout — which is exactly how
  TEST-8's first CI run failed. **Ten removal checks**, each turning red exactly
  the tests that name the behaviour: both routes, both range checks, the change
  detector, NTP's trimming, its empty-entry drop, its hours conversion, its
  unknown-field refusal, and MQTT's empty-password guard.

### TEST-11 — Nothing in the repository executes a line of `app.js` [MEDIUM] — **NEW (2026-09-20, by BUG-51's review)**

- **Problem**: 1 400 lines of JavaScript ship in every firmware, and no test
  runs any of it. There is no JS runner in the tree — no `package.json`, no
  spec file, nothing in `ci.yml`. The only evidence any of it works is a
  person driving a board with `tools/on-device/`, which CI does not run either.
- **What that costs, measured**: BUG-54 was a selector built from a bare field
  name where every field renders as `${contextId}_${name}`. It made Cancel
  inert from the day the feature was written, and it survived every review,
  every release and 1 173 native tests, because nothing looks. Reverting it
  today leaves all of them green.
- **Now larger than it was**: BUG-51 moved the whole refusal channel into that
  file. Delete the message-drawing call and no test in the repository moves.
- **The decision, and it is not "add a JS runner" by default**: a Node
  toolchain in a PlatformIO repository is a real cost — CI minutes, a second
  dependency tree, a second thing to keep green on a library whose users build
  firmware. The cheaper shapes are a jsdom check of the handful of pure
  functions, or making `webui_settings_refusal_check.py` assertive and running
  it from a bench script with a board. Pick one deliberately.
- **Refs**: BUG-54 (the defect nothing caught), BUG-51 (the surface it now
  covers), TEST-8 (the same argument for the upload envelope, answered with
  host mocks rather than a new runtime).

### TEST-10 — Core: no test confronts the EventBus byte model with a board's heap [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-41's code review, 2026-09-18.
- **Problem**: every expected value in every suite is computed by calling
  `QueueCost::of()` or dividing `kBudgetBytes` — the suites say so
  themselves ("these test the model, not the silicon"). Set `kNode` to 12 and
  `kOverhead` to 0 and nothing turns red. The only thing that ever compared
  the model to the heap is `tools/on-device/probes/allocator-shape`, which
  prints a staircase for a human and asserts nothing.
- **What one confrontation found**: a full queue measures 29 096 B on an
  ESP32-C3 against 28 192 modelled and 29 064 B on a nodemcuv2 against
  28 576 — 2 to 3 % under, the per-event constants exact, the residue a term
  `QueueCost::of()` does not have. One point per board cannot say whether it
  is per event or per queue; a half-full and a full queue would.
- **Also unpinned**: `DomoticsCore-Storage`'s `esp8266dev` sets the budget to
  4 096 for the whole environment, so no board exercises the shipped
  32-reference-event budget end to end; `sizeof(EventBus)` is pinned for the
  ESP8266 only (`test_heap_esp8266.cpp:40`) and for the ESP32 by nothing; and
  `test_ha_component`'s connect-cliff helper computes `refEvents - 2` on a
  `size_t`, which wraps under a tight override.
- **Fix**: promote the probe's fill block into the board suites as a Unity
  test — `getQueuedBytes()` against a measured free-heap delta, asserting the
  model is an upper bound and not a loose one — at two fill levels.
- **Refs**: BUG-41, MEM-8, ADR 0003.

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

### SIZE-1 — WebUI.h (950 lines) [HIGH] — **DONE (2026-08-31)**

- **Refs**: WEB-F2, R-F1
- **Fix (as filed)**: Extract route setup + SSE broadcasting into
  `WebUIRoutes.h` / `BroadcastManager.h`.
- **Measured**: 1008 raw lines before (950 when filed; SEC-10 and BUG-32 grew
  it) → **769**, plus `WebUI/SchemaMemProbe.h` at **121** and
  `WebUI/UpdateBuilder.h` at **90**; `ProviderRegistry.h` takes the chunk
  loop, 345 → **441**. All under Constitution VII's 800. The metric is raw
  `wc -l`, the campaign's precedent (SIZE-2 measured the same way), stated
  because the constitution's own wording excludes blanks and comments — this
  closes against the stricter count.
- **The filed remedy was reshaped by two facts it predates**: the modular
  split already existed (`WebServerManager`, `WebSocketHandler`,
  `ProviderRegistry`, `SystemHeader` hold what those names would hold), and
  the real fat was a loop that existed **three times** — the schema
  chunk-assembly loop, once per route lambda and hand-rolled a third time in
  the tests. It is now `SchemaChunkState::writeChunk()` beside its state;
  the route lambdas are wrappers owning only the retry-vs-end policy. The
  dedup surfaced **BUG-34** (the `/api/ui/schema` copy had never received
  v1.5.0's truncation fix — see its entry), filed and fixed in-lot.
- **Shaped for the fork, like SIZE-2**: marianorenzi's `esp32-ethernet`
  touches WebUI.h in two regions (+12/−11) — the `onComponentsReady`
  subscription block and `serializeContext`'s multiselect block — and both
  stay byte-identical, as does the include block his hunk contexts use. His
  `IWebUIProvider` renames are not adopted, the SIZE-2 decision again.
- **Evidence**: native tests never compile WebUI.h (ESPAsyncWebServer is
  `lib_ignore`d), so the extraction is what made testing possible: two new
  suites, `test_schema_chunking` (5 — chunk-size sweep 6–64 byte-identical
  to one-shot, both stall shapes, provider/empty-id skips, comma coverage)
  and `test_update_builder` (5 — delta/forceNext/empty skips, header parse,
  a crowding squeeze over every buffer size asserting dropped-never-corrupted
  with a >512-byte payload in the fixture). Component native count 92 → 102
  (+15 serializer, unchanged). Named mutations, `rm -rf .pio` between:
  needComma dropped → 2 red; `began` never set → 3 red (livelock detector);
  per-context bound dropped → red only once the fixture carries a >512-byte
  payload — below that the 512-byte early break masks it, recorded; delta
  check ignored → red. **One survivor, argued rather than hidden**: dropping
  the comma backtrack (`contextIndexInProvider--`) reds nothing because the
  branch is structurally dead — the `needComma` path is reached only straight
  from the loop's `written < maxLen` condition and nothing writes in between.
  True in the original lambdas too; kept verbatim, recorded here.
- Dead members removed with the probe extraction: `heapAfterSend` /
  `maxAfterSend`, written once and read only by the log line that now
  samples directly.
- Cross-compilation: WebUIOnly `esp8266dev` green on both commits. Board:
  both WebUI on-device suites ran on the nodemcuv2 against this refactor —
  `test_heap_esp8266` 6/6 (its `chunked_large_schema` exercises the moved
  serialization on silicon) and `test_schema_memory` 3/3.
- Design: `_bmad-output/implementation-artifacts/spec-size-1-webui-split.md`
  (v2, amended after adversarial review — which falsified the fork figures,
  the size arithmetic, and an undefined "verbatim" in the v1 draft).

### SIZE-2 — StreamingContextSerializer.h (921 lines) [HIGH] — **DONE (2026-08-31)**

- **Ref**: WEB-F3
- **Fix (as filed)**: Unify duplicated `writeJsonString` overloads (const char* vs String&), extract field serialization helpers.
- **Measured**: 933 lines before (it had grown since filing) → **756**, plus a
  new `WebUI/JsonStreamWriter.h` at **216** — both under Constitution VII's
  800. The two 70-line `writeJsonString` near-copies are one core over
  `(data, len)` with two thin adapters; `writeLiteral`/`isLiteralComplete`
  and the streaming state (`currentLiteral`/`literalOffset`/`stringOffset`/
  `numBuf`) moved with them.
- **The shape was chosen for the fork, not for taste.** marianorenzi's
  `esp32-ethernet` modifies this exact file (+49/−25: streaming multiselect
  states, an `optionIndex` → `arrayIndex` rename). The primitives were
  therefore extracted as a **privately-inherited base** — not one call site
  in either state machine changed name or shape, so his hunks still land —
  and his multiselect design was **adopted outright, credited**, adapted to
  the current field API (`selectedValues`, `type == Multiselect`), his
  rename included. His rebase of this file now approaches a no-op. The
  BUG-30 pattern — the fork had already written the mechanism — applied
  before the conflict instead of after.
- **Closing it closed two Code Safety items in the same file**: BUG-28 (the
  multiselect `JsonDocument`-into-a-temporary rebuilt per resume, resuming on
  an indeterminate pointer — his streaming states remove the temporary
  entirely) and BUG-26 (found **already fixed** by his `dc8886f1` of
  2026-07-16; the row was stale at filing). BUG-33 was filed and fixed on
  the way (host-only UTF-8 mangling — see its entry).
- **Evidence**: the 10-test suite grew to 15 before the refactor ran —
  UTF-8 round-trip (BUG-33's pin, red then green), a maximal-context byte
  sweep at chunk sizes 6–64 (floor 6 because escape sequences are atomic
  within one write and the content deliberately carries a control character
  whose escape is six bytes; below the longest escape in the content, the
  sweep livelocks — the review's second pass caught the v1 content whose
  longest escape was two bytes, which made the floor dishonest and left the
  six-byte early-out untested), an
  empty-multiselect sweep, a serializer-reuse test (production reuses one
  serializer across contexts; no prior test did) — all green on the old code
  first, all green after, `rm -rf .pio` between. **Five named mutations on
  the new code**: pointer-identity resume dropped → 7 red; the two-byte
  escape early-out loses `stringOffset` → maximal sweep red at chunk 6
  (duplicated bytes visible in the diff); `resetWriter()` forgets
  `stringOffset` → the reuse test red, *alone* — proving the rest of the
  suite cannot see it; `String&` adapter measured with `strlen` instead of
  `length()` → **nothing red**, recorded: the embedded-NUL asymmetry is
  unpinned, deliberately, rather than silently assumed; the six-byte
  `\u00XX` early-out loses `stringOffset` → maximal sweep red — a mutation
  that was **undetectable before the review's second pass** added a control
  character to the content, because no test had ever entered that branch. Board:
  `test_schema_memory` on the nodemcuv2 with a multiselect field added to
  its cached provider (without one, the suite never entered the changed
  path): 3/3, **heap drift zero** — flat at 44176 bytes across the
  20-iteration cached run and all 50 stress iterations, per-iteration diff 0.
- Dead member removed: `currentString`, declared and reset and never read.

### SIZE-3 — Wifi.h (880 lines) [MEDIUM] — **DONE (2026-09-21, by the triage pass — not a violation on the constitution's own metric)**

- **Measured**: 962 lines raw, **621 excluding blanks and comments**, which is
  what Principle VII counts ("Line count excludes blank lines and comments").
  The 880 in this heading was a raw count. The file has never breached the
  800-line limit by the metric that defines it — and the 341 lines of comment
  are the other conversation, held by the rule restated 2026-09-19.

- **Refs**: WIFI-F1, R-F11
- **Fix**: Extract AP-mode logic or WebUI provider code into separate header.

### SIZE-4 — test_mqtt_component.cpp (851 code lines) [MEDIUM] — **re-measured 2026-09-21**

- **Still over, and further than when it was filed**: 1 307 lines raw, **851**
  excluding blanks and comments, against the 800 hard limit. The 899 in the
  old heading was a raw count of a smaller file.

- **Ref**: MQTT-F1
- **Fix**: Split test file by feature area (connection, publish, subscribe, queue, WebUI).

### SIZE-5 — test_ha_component.cpp (1 465 code lines) [MEDIUM] — **re-measured 2026-09-23**

- **The worst breach in the tree**: 2 163 lines raw, **1 465** excluding blanks
  and comments — 83 % over the limit, and it grew while the entry sat open:
  1 355 on 2026-09-21, and BUG-63's lot added four cases to it rather than
  splitting the file, which is this entry's own work.

- **Ref**: HA-F10
- **Fix**: Split into `test_ha_config.cpp`, `test_ha_entity_management.cpp`, `test_ha_switch_integration.cpp`, `test_ha_publish_overloads.cpp`.

### SIZE-6 — test_webui_component.cpp (2520 lines) [LOW]

- **Ref**: WEB-F16
- **Fix**: Split into config tests, field tests, context tests, registry tests, serializer tests (3x over limit).

---

## Priority 7: Architecture / God Objects (Constitution I, XIII)

### ARCH-1 — System: God Object with 6+ responsibilities [MEDIUM] — re-argued HIGH → MEDIUM 2026-08-31; **restated 2026-09-06, open: the residue rides OBS Lot B**

- **Ref**: SYS-F6 — **which cannot be read**: no document in the repository
  carries it (BUG-23's SYS-F4 shows the family existed in the originating
  sweep; F6's reasoning did not survive). The HIGH was inherited, never
  argued in this entry — the BUG-2 discovery, though **not BUG-2's
  authority**: his remedies were measured impossible, these three are
  feasible, and this demotion rests on the weaker ground of harm-assessment
  and timing, owned as such.
- **Problem (as filed)**: Orchestrates 10 components, handles registration,
  persistence, events, state, console commands, boot diagnostics.
- **Fix (as filed)**: Extract `SystemComponentRegistrar`,
  `SystemEventOrchestrator`, `SystemConsoleCommands`.
- **The dating, precise in both halves**: config persistence and the WebUI
  provider setup have been delegated (`SystemPersistence.h` 340,
  `SystemWebUISetup.h` 417) since 2025-11-30, three months before filing —
  but boot-diagnostics persistence remains in System.h proper, so "handles
  persistence" was and is partially true. "Orchestrates 10 components" is
  the meta-orchestrator's definition, not its defect; the assembler
  maximizes coupling **by design**, which is exactly why TEST-1 was HIGH
  and why its suites are the operative mitigation.
- **Measured (same brace-scan as ARCH-2's closure, largest hand-checked)**:
  System.h 643 lines (623 at filing). One XIII indicator exceeded and
  stable since filing: `begin()` 62 lines (62 then too) — a linear
  numbered six-step sequence, but exceeded is exceeded.
  `getBootDiagnostics()` 50, `setupEventOrchestration()` 45. Roughly 220
  of the 643 lines (console wiring, boot diagnostics, state/LED mapping)
  are the real judgment call — MEDIUM-shaped: worth improving at the right
  moment, harming nothing measured today.
- **Defects, a checked list rather than a claimed zero**: BUG-23
  (Early-Init in registration, HIGH, DONE 2026-08-22) — closed by a
  narrower contract in place (`// BUG-23: register only`) plus TEST-1's
  suites; a `SystemComponentRegistrar` **as prescribed** would have
  relocated the same call, bug included — only the contract change
  prevents the class, and it already happened where the code is.
  PERSIST-1 (MEDIUM, open) sits in `SystemPersistence.h` — dead-code
  shaped, found *by* TEST-1's suite: evidence the delegated file gets
  audited, not structural harm here.
- **Why not execute now — measured against the fork** (`git diff
  --numstat`: System.h +13/−21, SystemConfig.h +4, SystemWebUISetup.h
  +25): `esp32-ethernet` inserts `registerNetworkComponent()` into the
  register-method list (his patch depends on the pattern the registrar
  would relocate) and deletes `setupEventOrchestration`'s WiFi→MQTT block
  outright (`NetworkEvents` replace it). Two of the three prescribed
  extractions would conflict with the most structural contributor branch
  this repository has; coordinate rather than surprise him. **The third,
  `SystemConsoleCommands`, overlaps none of his hunks and is declined on
  its own merits**: four one-line `registerCommand` lambdas over status
  getters, no defect ever attributed — a new header to save ~20 lines of
  wiring is YAGNI by the constitution's own list.
- **The trigger fired 2026-09-01 and the decision was taken 2026-09-06:
  restated.** marianorenzi's reply to the notification lifted the restraint
  himself ("do not restrain yourself because of my fork… I will merge and
  adapt", resuming mid/late October), so the fork argument fell. The merit
  argument stands and now carries the declines on its own: a registrar
  would have relocated BUG-23 with it (the contract prevented it, not a
  class); the orchestrator would extract the block his `NetworkEvents`
  delete and re-found; the console header is YAGNI. **What remains
  measurable is the boot-diagnostics residue** — `initBootDiagnosticsPersistence()`
  and `getBootDiagnostics()`, ~95 lines still in `System.h` — and OBS Lot B
  rewrites exactly that path: promotion of the RTC record is the first act
  of `System::begin()` and ownership of boot diagnostics moves to Core
  (`spec-obs-crash-observability.md` §L1). One change on the boot path
  before his October rebase rather than two. Measured 2026-09-06:
  `System.h` 676 lines (643 at re-argument; Lot A's watchdog block),
  `begin()` ~70 lines (62), the indicator still exceeded.
- **Re-measured at Lot B's closure, 2026-09-06**: `System.h` 676 → **629
  lines** (the boot-diagnostics formatter moved to SystemInfo, the
  persistence helper to one call), `begin()` ~70 → **79 lines** (the
  recorder's first act and its acknowledgement). The indicator is on
  `begin()`, so ARCH-1 **restates** rather than closes, as the plan
  predicted: what remains is `begin()`'s six heap-guarded steps, and the
  candidate is one helper that runs them — a change on the boot path that
  waits for marianorenzi's October rebase, since his diff against today's
  `main` is +47/−104 on this file. MEDIUM, open.
- **Suites at re-argument**: the three System suites ran **55/55 green
  from clean** on 2026-08-31 (`rm -rf .pio` first; `test_system_config` /
  `test_system_lifecycle` / `test_system_persistence`) — measured, not
  grepped.
- Argument:
  `_bmad-output/implementation-artifacts/spec-arch-1-system-god-object.md`
  (v2 after adversarial review — which caught the borrowed BUG-2
  authority, the fork covering only two extractions of three, and two
  half-false dating claims).

### ARCH-2 — LED: God Object [HIGH] — **DONE (2026-08-31, by measurement — the prescribed remedy already existed)**

- **Ref**: LED-F1 — **which does not exist in the repository**; like BUG-2's,
  this severity was inherited from a sweep whose reasoning cannot be read,
  and was never argued in the entry.
- **Problem (as filed)**: Combines config management, hardware pin control,
  PWM, effect calculations, state management, WebUI in one class.
- **Fix (as filed)**: Extract effect engine and WebUI into separate classes.
- **Both halves of that remedy exist, dated**: "WebUI in one class" was
  **false at filing** (2026-03-10) — `LEDWebUI.h` has been a separate class
  since at least 2025-09-26, the filing-era LED.h (482 lines) does not even
  override `getWebUIProvider()`, and `LEDWebUI` is referenced by no
  production code, only its 23-test suite and the `LEDWithWebUI` example
  that pairs it manually — the opt-in design LED.h's own comment describes.
  The effect engine was extracted by **PR #17 (2026-08-23)** as pure statics
  (`effectBrightness`/`rainbowColor`/`scaleToMax`/`pwmValue` — "no hardware
  and no member state"), `updateEffects()` calls them with no curve math
  left inline, and `test_led_effects` (29 tests) tests them directly. The
  entry never moved — the BUG-26 staleness shape.
- **The structure's one recorded defect is cited, not hidden**: BUG-19
  (`addLED()` after `begin()` desynchronized `ledConfigs` from `ledStates`,
  MEDIUM) is exactly the shared-state harm a god-object entry predicts — and
  it was closed in PR #17 by a local fix and a 35-test suite; no split of
  this class would have prevented it, the two vectors would still have to
  agree across whatever boundary separated them.
- **What remains passes every measurement Constitution XIII provides** (its
  Code Smell Indicators — the qualitative principle reduced to its stated
  measurables, explicitly): 544 lines (< 800); largest function
  `updateEffects` ~33 (< 50); inheritance depth 1 (< 3); declared
  dependencies 0 (< 5). Several responsibilities remain by SRP's
  reason-to-change test — conceded; KISS and YAGNI carry the same
  constitutional force, no measured harm remains, and the component holds
  **103 native tests, run green from clean at this closure** (35/29/23/16
  across its four suites).
- **Not ARCH-1's template**: System orchestrates ten components across three
  files behind 54 `__has_include` directives with persistence, console and
  boot diagnostics — the indicators that all pass here are the ones System
  strains. This closure rests on ARCH-2's prescribed extractions existing
  and being tested, which ARCH-1 does not share.
- No code changed in this lot. Argument:
  `_bmad-output/implementation-artifacts/spec-arch-2-led-god-object.md`
  (v2 after adversarial review — whose central catch was BUG-19, thirty
  lines from this entry, contradicting v1's "no defect" claim).

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

### CI-4 — Wifi vs WiFi naming inconsistency [MEDIUM] — **DONE (2026-09-21, by the triage pass)**

- **Measured**: `DomoticsCore-WiFi` appears three times in the repository, all
  three in `CHANGELOG.md` and this file, describing what was written at the
  time. `docs/` and every manifest say `DomoticsCore-Wifi`. Rewriting the
  three would be editing the record, not fixing the code.

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
- **Arbitrated 2026-09-21 (test-architect brief, maintainer validated)**: keep
  the target, fix the claim, and move the behavioural assertion to the only
  place that can hold it. `Build esp8266dev` is a *link* witness — it is the
  only project pulling all twelve components, so dropping it would trade a wrong
  sentence in a table for a real hole across the ten ESP8266-specific files. The
  two "what CI proves" tables gain the caveat, FullStack's README says it is
  ESP32-class, and the FullStack `esp8266dev` on-device suite gains a test that
  fails when post-`begin()` free heap is under the WiFi guard's 2 500 bytes. CI
  keeps compiling it and the required-check set does not change.
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

### CI-13 — `clean_examples.py` does not know about the `test/` projects [MEDIUM] — **DONE (2026-09-01)**

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
- **Fixed 2026-09-01, after being paid for a second time and dearer**: the
  2026-09-01 campaign's FullStack ESP8266 build recursed to **19 GB and
  145,135 object files** (against the 4.3 GB that filed this) and sat for
  1h47 with zero fresh writes before anyone looked — the two `.pio` it
  aspirated were exactly the ones the previous day's SIZE-1/SIZE-2 baselines
  had left under `DomoticsCore-WebUI/test/`. The fix is the third
  fixed-depth pass this entry prescribed (`comp/test/*/.pio`, same
  `_safe_to_clean` guard, no `rglob`). Measured after: the same build from
  the same purged tree lands at 30 MB, and a nested-`.pio` watch during it
  stayed quiet but for the bounded self-inclusion now filed as CI-15.
  **The cleaner is retired by CI-15 (2026-09-15)**: with `symlink://`
  dependencies nothing is copied, so there is no `.pio` to keep out of a copy.

### CI-15 — no `library.json` declares `export.exclude`, so PlatformIO copies `.pio` with every `file://` component [MEDIUM] — **DONE (2026-09-15), on the re-pointed defect**

- **The heading names the framing this entry refuted on 2026-09-05, kept
  as filed.** The copy was the defect, and the fix is to stop copying:
  every `file://../DomoticsCore-X` in the 49 tracked `platformio.ini`
  (337 lines: 12 component roots, the 2 WebUI test projects, 29 examples,
  3 under `tests/unit/`, 3 probes) is now `symlink://`. PlatformIO writes a
  `<name>.pio-link` file into libdeps and compiles the component where it
  lives — the warnings cite the working tree — so nothing is copied,
  nothing goes stale, nothing nests, and `clean_examples.py` is retired
  with the 33 `extra_scripts` lines that named it. Measured on
  6.1.19 (CI's pin) and 6.2.0.
- **Three things the migration had to learn, each by a build that failed.**
  (1) A `symlink://` to the project's *own* directory breaks the test build
  (`unity.h: No such file or directory`); the self line is gone from the
  18 environments that had one, and the two OTA device environments take
  `test_build_src = true` to compile `src/` instead. A project's own
  `library.json` is never read, so the one dependency that had reached a
  suite only through that self copy — `ESPAsyncWebServer` and `ESPAsyncTCP`
  for WebUI's `esp8266dev` — is listed. (2) A symlinked component that
  **contains** the project claims every include under it: the LDF maps an
  include to the first library whose directory is a path prefix of the
  header, in `sorted(listdir)` order, and for the 31 nested projects (29
  examples, 2 WebUI test projects) the component's real directory contains
  the project's `.pio/libdeps`. `DomoticsCore-*` sorts before
  `ESPAsyncTCP`, so every ESP8266 build lost `ESPAsyncTCP.h`; ESP32 passed
  only because `AsyncTCP` sorts first. Those 31 manifests carry
  `[platformio] libdeps_dir` pointing at the repository root's `.pio/`, the
  build directory stays where it was, and FullStack `esp8266dev` then
  matches CI's build of `main` to the byte (RAM 54196, Flash 733983).
  The ESP32 targets keep their RAM and gain 864 (`esp32dev`) and 880
  (`esp32c3`) bytes of flash: the ESP32 core's log macros embed source
  paths, and the image now carries 30 of the components' real paths in
  place of `.pio/libdeps/...` ones — the same figure will move again on
  the runner, whose checkout path differs (deferred-work names
  `-ffile-prefix-map` as the way to make it path-independent).
  (3) `embed_webui.py` created `DomoticsCore-WebUI` directories under
  libdeps unconditionally — manifest-less libraries beside the link; it
  now writes only into a package directory that exists.
- **The trap, measured closed.** On `main`, `Platform_HAL.h` broken with an
  `#error` and NTP's native suite re-run *without* clearing `.pio`: 37/37
  green — the stale copy compiled. On this branch the same sequence goes
  red, and green again once the header is restored. Two CI guards keep it
  so: no `file://` in a tracked manifest, no `DomoticsCore-*` directory
  under any libdeps after the builds, in all three jobs.
- **Verification**: the 13 native projects from a fresh `.pio` (1070
  `[PASSED]` lines, CI's own count on `main`), the seven on-device suites
  and OTA `esp32cam` compiled, FullStack on the three targets with both
  flag sets, every example and probe built once per declared environment,
  the OTA device images' figures before and after `test_build_src`, and
  one run of the OTA suite on the nodemcuv2 for the image whose recipe
  changed.
- **Recorded in the CHANGELOG's `[Unreleased]` section** (opened 2026-09-16
  with the lines PR #69 and PR #70 owed): the local-checkout recipe in four
  READMEs changed from `file://` to `symlink://`, and `clean_examples.py`
  is gone.


- **Filed**: 2026-09-01, by the campaign that paid CI-13 twice. Verified:
  **zero of the twelve `library.json` files carry an `export` key.**
- **This is the root cause of the whole family.** CI-13's recursion, and a
  second mechanism the campaign watched live: FullStack depends on
  `DomoticsCore-System`, whose copy includes `examples/FullStack/.pio` —
  **the live `.pio` of the very build in progress**, the one directory
  `_safe_to_clean` rightly refuses to touch. Every build re-copies the
  current `.pio` state into its own `libdeps` (~5 MB observed, bounded only
  because the cleaner's part 2 razes `libdeps/DomoticsCore-*` first on
  every run).
- **Why not fixed in the campaign's tooling lot**: `export.exclude` changes
  what the PlatformIO Registry packages — people install this library by
  version, and packaging is a release decision, not a hotfix. The remedy
  (`"export": {"exclude": [".pio/**", "examples/**/.pio/**",
  "test/**/.pio/**"]}` across the twelve manifests, or excluding
  `examples`/`test` from export outright) belongs to a lot that verifies
  the published tarball, ideally alongside the pending corrective release.
- **Measured 2026-09-05, while the v2.3.0 branch was being prepared, and the
  prescribed remedy is inert — on both paths.** With a 1 MB `junk/` directory
  planted in `DomoticsCore-Core` and `"junk/**"` in its `export.exclude`:
  `pio pkg pack` left it out of the tarball (and leaves `.pio` out **by
  default**, exclude or no exclude — 0 `.pio` entries either way), while a
  native build of `DomoticsCore-NTP`, which pulls Core as
  `file://../DomoticsCore-Core`, copied `junk/` into `libdeps` regardless. So
  the registry never had the problem and the local copy does not read the
  field. The twelve `export.exclude` blocks were added to the release branch
  and removed again before the PR, the same afternoon.
- **Remedy, re-pointed**: the copy is the problem, so stop copying.
  PlatformIO 6.1 accepts `symlink://../DomoticsCore-Core` where the test and
  example manifests say `file://`; a symlink carries no `.pio` and — the
  other defect this would close — cannot go stale, which is the
  `pio-libdeps-stale-copies` trap CLAUDE.md warns about (`rm -rf .pio` before
  any run you intend to trust). That is a CI/test-project lot with its own
  validation on the runner, not a packaging one, and it no longer needs to
  ride a release.
- Until then `clean_examples.py` remains the guard, now with CI-13's third
  pass — and its header's "never build examples in parallel" warning is
  load-bearing: the campaign violated it once and the cleaner of one
  example deleted the firmware another script was about to upload.

### CI-20 — the shipping ESP32 build compiles C++17 inline variables as a GNU extension [LOW] — **NEW (2026-09-24, by TEST-12's lot)**

- **Files**: `HAEvents.h:15,18,21` and `MQTT_impl.h:8`, against
  `DomoticsCore-System/examples/FullStack/platformio.ini:18` (`-std=gnu++14`),
  which is the required `Build esp32dev` check.
- **Problem**: four `inline` variables in headers are a C++17 feature the build
  accepts only as a GNU extension, and says so at every use. They are what the
  warnings that remain after this lot's cleanup are: the six structured bindings
  in `RemoteConsole.h` and `Storage.h` were rewritten, these cannot be — an
  inline variable in a header is there to have one definition across translation
  units, so removing it means the class-template static `CoreLog_HAL.h` uses for
  exactly this reason.
- **LOW**: every toolchain this repository builds with accepts the extension, and
  the fork's Arduino core 3.x compiles in a later mode where the feature is
  standard. What it costs is four warnings in a required check, which is where a
  real one would have to be noticed.
- **Fix**: the `Storage<Tag>` shape from `CoreLog_HAL.h`, or raise that example
  to `gnu++17` — the second is a decision about what the shipped build is, not a
  cleanup, and `esp32c3` in the same file already uses `gnu++17`.
- **Refs**: TEST-12, whose lot made the ESP8266 and ESP32 test builds silent and
  left these standing.

### CI-19 — HomeAssistant's `esp32dev` test environment cannot compile the suite it declares [LOW] — **DONE (2026-09-24)**

- **Files**: `DomoticsCore-HomeAssistant/platformio.ini` (`[env:esp32dev]`,
  `test_ignore = test_ha_heap_esp8266, test_ha_webui`),
  `test/test_ha_component/test_ha_component.cpp`, `Wifi_Stub.h:19`.
- **Problem**: `test_ha_component` calls `HAL::WiFiImpl::setConnectedForTest()`,
  which exists only in the native stub, so the environment fails at compile in
  three places. `test_ignore` excludes two other suites and not this one, so
  `pio test -e esp32dev` in that component has never built — and `Build
  on-device suites` does not list it, which is why nothing said so.
- **LOW**: no shipped artefact depends on it and the native run covers the same
  cases; the cost is a developer's minute and a red run that means nothing.
- **Fixed by giving the environment a suite of its own** rather than by hiding
  the one it could not build: `test_filter = test_ha_device` means it compiles
  and runs the device suite and nothing else, so neither `test_ha_component` nor
  the `Platform_Stub.h` collision is reachable there. The environment now builds
  in `Build on-device suites` and runs 5/5 on a WROOM-32D. The sibling of CI-11,
  from the other direction — that one declared an environment with no suite,
  this one declared a suite the environment could not build; both closed in the
  same lot.
- **Refs**: CI-11 (the same manifest, the mirror defect).

### CI-11 — HomeAssistant declares an ESP8266 test env with no ESP8266 tests [MEDIUM] — **DONE (2026-09-24)**

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
- **The same defect existed on `[env:esp32dev]`** and was filed separately as
  CI-19; both are closed here.
- **Closed 2026-09-24 with the coverage it was about.** `test_ha_device` runs on
  both boards and measures what no host can settle: the discovery documents the
  board's own `String` and ArduinoJson build, 541 and 552 characters to the
  character, the requirement keys on the wire, a document over the field refused
  whole and counted, and five republishes leaving the heap where it was. 9/9 on
  the nodemcuv2 beside the heap suite, 5/5 on the WROOM-32D.
- **The job still lists its projects, deliberately.** Measured while closing
  this, with this lot's own additions counted: six components declare
  `esp8266dev` and carry a `test/`, while the job runs eleven builds — two of
  them nested projects (`WebUI/test/test_schema_memory`, `FullStack`) that no
  such rule discovers, and three named ESP32 environments. Discovery would cover
  half the run and would lose what the list gives, which is failing loudly when
  a listed project disappears.
- **What the review found instead, and it is the sharper hole**: a `test_filter`
  naming a suite that no longer exists is not an error to PlatformIO. It builds
  nothing, runs nothing and exits 0 — verified with `-f no_such_suite`, which
  prints `0 test cases` and returns zero. Six environments in the repository are
  filtered, four of them added by this lot, so the job could have gone green
  over nothing at all: exactly what it was created to catch. A step now checks
  every `test_filter` in every tracked manifest against the directory it names,
  and was proved non-vacuous by renaming a suite.

- **The ESP32 mirror, from MEM-2's lot (2026-08-29)**: `pio test -e esp32dev`
  dies the same way — `test_ha_events` includes `Platform_Stub.h`, the
  environment ignores only the heap and WebUI suites, so the native ones are
  cross-compiled for the board, and no workflow runs the command, so nothing
  reports it. Reproduced at `bea43842`. Whatever this entry decides for
  `esp8266dev` applies to `esp32dev` in the same edit.

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

### CI-16 — the build's leftovers: four unbuilt `tests/unit` projects, a glob that matches nothing, and the `-I` flags CI-15 made redundant [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: CI-15's lot, 2026-09-15.
- **Files**: `tests/unit/{01,02,05,06}`, `tools/local_ci.sh:28`, the
  `[env:native]` of twelve component manifests.
- **Three findings, one decision each**:
  1. `tests/unit/01`, `02`, `05` and `06` are `esp32dev` projects no workflow
     builds; `06-webui-refactor` reaches the components through
     `lib_extra_dirs = ../../../../` with `deep+` — a third in-place mechanism
     the CI guards do not see. Whether the four should exist is the decision.
  2. `tools/local_ci.sh:28` globs `tests/unit/test_*_isolated`, which matches
     nothing.
  3. the `-I../DomoticsCore-X/include` flags in every native manifest are a
     second route to every header now that the LDF reads the components in
     place; removing them is its own change with its own check — every
     include resolving through the LDF alone.
- **Refs**: CI-15, CI-7 (the same script).

### CI-17 — the generated `WebUIAssets.h` is a gitignored input the host build can read [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: CI-15's lot, 2026-09-15; the trap fired on TEST-8's first CI
  run, 2026-09-19.
- **Files**: `DomoticsCore-WebUI/embed_webui.py:282-301`,
  `DomoticsCore-WebUI/.gitignore:26`,
  `tests/mocks/libraries/DomoticsCore/Generated/WebUIAssets.h`.
- **Problem**: `embed_webui.py` writes the header into
  `DomoticsCore-WebUI/include/DomoticsCore/Generated/` on every example
  build; under `symlink://` it is the only copy the compiler reads, a stale
  one from a previous build is what the next starts from, and a native suite
  that includes it passes on a machine that has built the WebUI and fails on
  a clean checkout — 1 107 local green against a red runner. TEST-8's mock
  exists so the suites do not need it; nothing stops them from finding the
  real one first.
- **Fix**: a CI guard, or the mock directory ahead of the component's include
  path in every native manifest. Until then the sweep that counts `[PASSED]`
  moves the file out first.
- **Refs**: CI-15, TEST-8, TEST-9.

### CI-18 — no build-time check reads the ESP8266 boot path's stack frames [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-42's lot, 2026-09-18.
- **Problem**: `Core::begin()` was one deep frame among several on the 4 KB
  cont stack, and BUG-42 measured only the one overflowing it. With that
  kilobyte gone the Storage suite's deepest point leaves 896 B — margin, not
  comfort: `-fstack-usage` puts `logPromotedRecord` at 1 168 B and
  `app_entry_redefinable` at 4 160, and nothing in CI reads either number. A
  board found BUG-42; a build could have.
- **Fix**: `PLATFORMIO_BUILD_FLAGS="-fstack-usage"` on the `esp8266dev`
  FullStack build, and a script that fails on any frame over a stated figure
  in the paths an application runs at boot.
- **Refs**: BUG-42, CI-14 (the same build).

### DOC-2 — the 2.5.0 release note's "13 entities" is derived nowhere [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-41's code review, 2026-09-18.
- **File**: `CHANGELOG.md`, the 2.5.0 block ("The EventBus's 32-entry cap
  leaves **13 entities** to an application at connect").
- **Problem**: the sentence describes a cap BUG-41 removed, and the figure
  replaced 21 in uncommitted work that predates the review; neither number is
  derived in the repository, and the suites now compute the connect-burst
  capacity from `QueueCost`. A shipped note is not edited; the next release's
  entry can say what the figure is now and how it was obtained.
- **Refs**: BUG-41, LO-2.

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

### DC-14 — Every provider declares a REST endpoint that is never registered [MEDIUM] — **NEW (2026-08-29)**

- **Opened by**: BUG-31, 2026-08-29. It is the reason the wrong belief behind
  BUG-31 was plausible in the first place.
- **Files**: `HomeAssistantWebUI.h:56,68,81,95` (`/api/ha/status`,
  `/api/ha/dashboard`, `/api/ha/settings`, `/api/ha/detail`),
  `WifiWebUI.h:118` (`/api/wifi`), and the other providers' `.withAPI(...)` calls.
- **Problem**: `.withAPI("/api/ha/settings")` and its siblings declare per-context
  REST endpoints, and **none is ever registered** — the only HTTP routes in the
  framework are the ones `WebUI.h` sets up itself, of which
  `/api/ui/action?contextId=&field=&value=` (`WebUI.h:544-566`) is the single path
  from a browser to a provider.
- **It is worse than unused, because it is published.** `serializeContext`
  (`WebUI.h:827`) ships `apiEndpoint` to every connected client, so a third-party
  front end reading the context schema would reasonably POST a whole form to an
  address that answers 404. The handler that expected such a form is what BUG-31
  turned out to be.
- **The remedy is a choice, not a cleanup**: register the endpoints, or stop
  declaring and serialising them. Removing `apiEndpoint` from the wire is a schema
  change and belongs with DC-12's decision about what the context schema is for.

### DC-15 — WifiConfig's advanced settings are accepted and ignored [MEDIUM] — **NEW (2026-08-31)**

- **Opened by**: TEST-4's closing lot, 2026-08-31, while writing the spec's
  non-goals.
- **Files**: `Wifi.h:49-50` (the fields, commented "Advanced settings"),
  `Wifi.h:684-701` (`setConfig` — never reads them), `Wifi.h:675-676`
  (`getConfig` — returns the hardcoded 5000 and `CONNECTION_TIMEOUT` regardless
  of what was set).
- **Problem**: `WifiConfig.reconnectInterval` and `WifiConfig.connectionTimeout`
  are dead. `setConfig()` applies neither — `reconnectTimer` keeps the
  constructor's 5000 ms and the timeout is a `static const` — and `getConfig()`
  hands back the constants, so a round trip silently discards what the caller
  wrote. An integrator who sets `reconnectInterval = 60000` to save battery
  gets a retry every five seconds and no error. The only readers in the
  repository are the struct's own defaults and one test asserting them.
- **Same family as the C1–C3 dead config fields** removed by
  `tech-spec-remove-dead-config-fields-c1-c3`. The remedy is a choice: plumb
  the two fields through (`NonBlockingDelay::setInterval` exists; the timeout
  would have to stop being `static const`), or remove them from the struct.
  Removing is an API change for anyone who sets them today — which, per the
  defect itself, changes nothing at runtime.

**DC-5, on keeping `ledConfigs`.** "Clear the internal vectors" was filed as one
action; it is two. `ledStates` is runtime churn and is released, per Constitution
XIV. `ledConfigs` is the user's registration — the pins, names and brightness
ceilings passed to `addSingleLED()`. `ComponentRegistry::shutdownAll()` leaves a
component able to be initialised again, and clearing the configuration would have
left that second `begin()` driving nothing at all, silently. The vector that
holds user input outlives the shutdown; the vector that holds machine state does
not.

---

### DC-16 — `/api/ntp/timezones` is registered twice [LOW] — **DONE (2026-09-14, filed the same day by OBS Lot D's route sweep)**

- **File**: `NTPWebUI.h:99` and `SystemWebUISetup.h:310` both called
  `registerApiRoute("/api/ntp/timezones", …)` with the same streaming
  handler; ESPAsyncWebServer keeps both handlers and the first match wins.
  `NTPWebUI::init()` existed for exactly this and nothing in System called
  it — the NTPWithWebUI example does, so a FullStack build registered the
  handler twice and the example once.
- **Fix**: the System's copy is gone; `SystemWebUISetup.h` calls
  `providers.ntp->init(webuiComponent)` after registering the provider,
  the way it already does for OTA and RemoteConsole. The System reference
  no longer describes the route as its own; the NTP reference always did.
- **Verification**: header-only, so a FullStack build per platform is the
  check; `grep -rn 'api/ntp/timezones' --include=*.h` finds one
  `registerApiRoute` call.

### DC-17 — Core: `ComponentConfig` and eight `MemoryManager` queries have no caller in the tree, and both are documented public API [MEDIUM] — **NEW (2026-09-15)**

- **Opened by**: TEST-7's reading, before its tests were written.
- **Files**: `DomoticsCore-Core/include/DomoticsCore/ComponentConfig.h`
  (the `ComponentConfig` class: `defineParameter`, `setValue`, `getValue`,
  `getInt`, `getFloat`, `getBool`, `validate`, `getParameters`,
  `hasParameter`); `DomoticsCore-Core/include/DomoticsCore/MemoryManager.h`
  (`getBufferSize`, `shouldEnable`, `getWsUpdateInterval`,
  `getChartHistoryPoints`, `getCurrentFreeHeap`, `isCriticalMemory`,
  `setThresholds`, `getThresholds`).
- **The claim is "no caller in the tree", DC-13's claim**: no component, no
  example, no test called any of these before TEST-7 pinned them —
  `ComponentStatus`, `statusToString()` and `ComponentMetadata` are live,
  and `MemoryManager`'s other four queries are called from `Core.cpp:78-81`,
  `WebUI.h:108-112`, `WebServerManager.h:92` and `SystemInfoWebUI.h:86`.
  All of it is documented: the Core reference §4 states `ComponentConfig`'s
  validation contract, §8 tabulates every `MemoryManager` query, and the
  README hands users `getBufferSize()` as the example — so the callers are
  other people's sketches, installed by version from the registry.
- **Two columns of §8's limits table — *Max Providers* and *Heap Check
  Interval* — have no public accessor at all**: `getLimits()` and
  `getIntervals()` are private. Documented, unreachable, and therefore
  unpinned by TEST-7.
- **The profile, as observed**: a WROOM-32D running FullStack boots FULL
  (276 200–276 220 bytes over four logged boots); a nodemcuv2 boots
  STANDARD (26064) with the observability probe and MINIMAL (15088) with
  FullStack, where the WebUI's client-limit reduction fires (CI-14's boot
  log) — so the profile is sketch-dependent on an ESP8266 and the one
  adaptive path with a caller is live there. The thresholds were "conservative
  starting points" never measured for purpose (`MemoryManager.h:58-60`);
  TEST-7 pins them as they are.
- **The remedy is DC-13's, and constrained the same way**: keep and pin
  (done), deprecate, or delete — and removing documented public methods
  belongs on a major boundary. `ComponentConfig` has BUG-40's fix behind it
  now, so keeping it is no longer keeping a defect.

### DC-18 — OTA: the buffering path is a no-op on all three platforms [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: the SEC-2 re-fix, 2026-08-26.
- **Files**: `OTA.cpp:98-126` (the branch in `loop()`), `:321`, `:441`;
  `Update_ESP32.h:68-98`, `Update_ESP8266.h:97-125`, `Update_Stub.h:74-78`.
- **Problem**: `loop()` still drives `HAL::OTAUpdate::hasPendingData()` and
  `processBuffer()`, and two sites test `requiresBuffering()`; all three
  return false or zero unconditionally on every platform since the ESP8266
  went to `Update.runAsync(true)`. The branch, its error handling and the
  three HAL functions are unreachable, and the OTA reference still tabulates
  them.
- **Fix**: delete the branch and the HAL functions; the HAL contract loses
  three names, which is a release note.
- **Refs**: SEC-2, DC-6, DC-7 (the same file's earlier dead fields).

### DC-19 — OTAWebUI carries three inlined copies of the WebUI's credential check [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: the SEC-4 / SEC-6 / SEC-14 lot, 2026-09-14.
- **Files**: `OTAWebUI.h:357`, `:384`, `:438`; `WebUI.h`, `authorize()`.
- **Problem**: SEC-13 made `WebUIComponent::authorize()` the one gate, and
  SEC-14 made the three OTA copies read the same stored flag — but they are
  still copies, and SEC-15's delay, when it lands in `authorize()`, will not
  cover them. OTA can call the shared gate once its `library.json` can
  require the WebUI version that has it (Constitution XV).
- **Fix**: one call, and a dependency floor.
- **Refs**: SEC-13, SEC-14, SEC-15.

## Priority 10: Minor Issues (LOW)

| ID | Component | Issue |
|----|-----------|-------|
| LO-1 | Multiple | Hardcoded fallback versions in `getWebUIVersion()` drift from actual version |
| LO-2 | MQTT | `MQTTPublishEvent` is 830 bytes (`sizeof`, both cores; this row said 832 until 2026-09-20) — consider reducing field sizes |
| LO-3 | Core | `__dc_*` field names use reserved double-underscore prefix |
| LO-4 | Core | `nextId` overflow after 4B subscribe/unsubscribe cycles (theoretical) |
| ~~LO-5~~ | Core | ~~Backpressure silently drops oldest event with no logging~~ — **DONE**: logged since OBS Lot B, and the line now carries the budget and the peak occupancy (BUG-41) |
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
| LO-33 | Core, Storage | The ESP8266 heap measurements (filed 2026-09-20 by the deferred-work purge): `HeapTracker::checkpoint()` samples the heap before inserting its own map node, so every window absorbs the previous checkpoint's allocation — material against the suite's 64 B thresholds; `getDelta()` on a name that does not exist subtracts a zero snapshot and reports a multi-kilobyte leak instead of an error; and the Storage heap suite leaves its namespaces (`heaptest`, `churn`, `overwrite`, `undrained`, `reclaim`, `nswarm`, `ns0..ns4`) resident and never shuts down the `Core` instances it builds, so successive runs start from a more fragmented heap |
| LO-34 | HA | Three costs on the command path, left by MEM-2 (filed 2026-09-20 by the deferred-work purge): `handleCommand()` opens with an unconditional `DLOG_I` of topic and payload — a 128-byte stack buffer, an `snprintf` over up to 699 bytes and a callback walk for every message on the shared client, other components' included; the switch auto-publish calls `findEntity()` a second time on a path already holding the `HAEntity*` (`:885` → `:376` → `:340`); and a topic with two adjacent slashes yields an empty id that `findEntity(const char*, 0)` matches against any entity with an empty id rather than refusing — none can have one today, and no test covers the shape |
| LO-35 | Wifi (ESP32) | The upload link is window/RTT bound on its own, apart from BUG-37 (filed 2026-09-20 by the deferred-work purge): 20–54 KB/s, `rwnd_limited` 58 % of the time on a 5 760-byte window, idle RTT 114–127 ms averaged (max 307–610 ms), dozens of retransmissions per megabyte on the WROOM-32D — the profile of WiFi modem sleep, and the core's default is `WIFI_PS_MIN_MODEM`. Unmeasured: one build with `WiFi.setSleep(false)` would say whether that is the RTT, the loss, both or neither |
| LO-36 | WebUI | `WebUI.h:18` includes `<pgmspace.h>` directly (filed 2026-09-20 by the deferred-work purge) — a platform header named by a component file rather than reached through a HAL one; Constitution IX is written about `#ifdef`, so adjacent, not a breach — and the reason the host build needs a `pgmspace.h` mock at all. `WebResponse_HAL.h` already dispatches the one API that differs per platform |

---

## Priority 11: Observability — post-mortem and remote telemetry

> Filed 2026-09-05 from a design discussion, not from a review sweep. The
> maintainer's production devices reboot at a regular interval and nobody has
> ever seen one die: the RemoteConsole is a Telnet stream fed by the Logger,
> and a heap exhaustion never goes through the Logger. Design in
> `_bmad-output/implementation-artifacts/spec-obs-crash-observability.md`
> (v2), adversarially reviewed the same day (`review-obs-v1-adversarial.md`,
> 22 findings) and then **measured on both boards before these entries were
> written in their present form** — the review's first finding reshaped the
> design within the hour. Seven items, four lots; **Lot A (OBS-2, OBS-6,
> OBS-1's boot check, OBS-7) is the recommended first lot**: it builds no
> new mechanism and it is honest about what it can and cannot see.
> **Lot A landed the same day** — OBS-2, OBS-6, OBS-7 closed and OBS-1's
> boot check, measured on both boards from the branch.
>
> **Severity, argued once for the section.** All seven are MEDIUM except
> OBS-6 (LOW). HIGH in this roadmap has meant a defect in shipped behaviour;
> these are missing instruments for a live production problem, which is a
> priority argument, not a severity one. The priority is carried by the lot
> order and by the maintainer's decision of 2026-09-05 that this is the next
> subject. Anyone who wants OBS-3 at HIGH should argue it from a defect it
> exposes, and one is on the table: the periodic reboots themselves, once
> Lot A has said what they are.

### OBS-1 — ESP32: every panic already writes a core dump to flash, and nothing reads it [MEDIUM] — **DONE (2026-09-14): the boot check in Lot A, the transport in Lot D**

- **Measured** on the WROOM-32D with the stock `default.csv`: a null
  dereference, a `malloc`-until-NULL dereference and an `abort()` each left
  an ELF dump of ~9 KB in the `coredump` partition at `0x3F0000`;
  `esp_core_dump_image_check()` returned OK, `esp_core_dump_image_get()` the
  size, `esp_core_dump_image_erase()` cleared it. `grep -r coredump` over
  the sources finds only partition CSV references.
- **The fleet caveat, corrected after review**: 23 of the 24 stock
  partition tables carry the partition (`bare_minimum_2MB.csv` is the
  exception) and PlatformIO's default for `esp32dev` is `default.csv`. **A
  project loses it only by choosing a custom CSV** — as FullStack did before
  CI-9 (PR #11, 2026-08-23), whose entry listed "64 KB coredump" among the
  reasons to switch and which nothing followed up. The maintainer's fleet
  runs his own project; unless it set a custom table, the dumps have been
  accumulating. v1 of this entry had that backwards.
- **Fix, in two lots**: **Lot A** — at boot, say whether the partition
  exists and whether a dump is waiting, with its size, at WARN. **Lot D** —
  two HAL seams beside `getCoreDumpStatus()`, `coreDumpRead(offset, buf,
  len)` and `coreDumpErase()` (ESP32 real, ESP8266 `0`/`false`, the stub
  scripted); `GET /api/system/coredump` streams the image as a
  **fixed-length** response (`beginResponse(type, size, filler)`, not the
  chunked shape whose zero-return trap BUG-34 recorded) with
  `Content-Disposition: attachment; filename="coredump.bin"` (this entry
  first said `coredump-<build id>.bin`; the route never named the build,
  since the image belongs to whichever build panicked — corrected 2026-09-15);
  `POST /api/system/coredump/erase` checks SEC-10's token **first**, then
  auth (the OTA upload's order), refuses `409` while a download is in
  flight (an erase under the reader would stream `0xFF` into a full-length
  file that fails the host's CRC), then erases and refreshes SystemInfo's
  boot diagnostics so `bootdiag` stops announcing the dump without a
  reboot. Both routes are registered from `SystemWebUISetup.h` beside
  `/api/ntp/timezones`, on every platform: `404 {"error":"no core dump"}`
  where nothing waits. The image is the partition's own bytes from offset 0
  — length word, version, task count, the ELF, a CRC32 (the `addr` that
  `esp_core_dump_image_get()` returns is the partition's address; the plan
  had modelled it as an offset and the arithmetic survived it).
- **Measured on the WROOM-32D, 2026-09-14** (`tools/on-device/coredump_check.py`):
  the first download was **Lot C's task-watchdog death**, whose ELF is gone —
  `200`, 18 020 bytes received, `Content-Length` 18 020, the image's own
  length word 18 020, the transport check. Then `crash null` on the Lot D
  build: `bootdiag` says `WAITING — 18052 bytes`; download 18 052 = length
  word; **decoded against the build's ELF**: `StoreProhibitedCause`,
  `excvaddr 0x0`, crashed task `loopTask`, `#0 crashForTest
  (Platform_ESP32.h:331)` ← `#1 System::registerConsoleComponent()`'s
  `crash` lambda ← `#4 RemoteConsoleComponent::handleClient
  (RemoteConsole.h:655)` ← `#8 Core::loop` ← `#9 System::loop` ← `loop()
  (main.cpp:262)` — the site, from the ELF, as the entry asked. Erase `200`
  in 0.05 s; `GET` after it `404`; `bootdiag` reads `partition present, no
  dump waiting` without a reboot; a serial reset then boots with **no
  `Core dump` WARN** — the removal check. Negatives: no credentials →
  `401` on `GET` and `403` on `POST` (the token is checked first and cannot
  be minted without credentials); credentials without the token → `403`
  `Bad or missing CSRF token`. The image's header carries the app's
  SHA-256 and `esp-coredump` checked it against the ELF before decoding.
- **What the decode needs, learned on the host**: `esp-coredump` (v1.17.1
  via `uvx`) drives a target GDB. PlatformIO's `toolchain-xtensa-esp32` GDB
  links against Python 2.7 and does not start here — the tool then prints
  its banner and exits 1 with nothing on stderr; `pio pkg install -g -t
  platformio/tool-xtensa-esp-elf-gdb` (14.2) decodes. The script looks for
  the second before the first and the bench README says so.
- **Not exercised on the board**: the `409` (an erase timed inside a
  16 KB download); the ESP32-C3, unplugged since Lot A. Native:
  `test_systeminfo_boot` reads a scripted image through the seam in chunks
  and ends at its size, and reads the refreshed status after an erase.
- **Lot A half, done**: `HAL::Platform::CoreDumpStatus` / `getCoreDumpStatus()`
  (partition present, dump waiting, size); `BootDiagnostics::coreDump`; a WARN
  at boot and a `Core dump:` line in `bootdiag`. **Measured on the WROOM-32D
  from the branch**: `partition=1 dump=0` on a clean boot, then
  `⚠ A core dump from a previous panic is waiting: 15268 bytes` after a null
  dereference and `15748 bytes` after OBS-7's watchdog panic. **ESP32-C3**:
  partition present on `esp32-c3-devkitm-1`'s default table, `7044 bytes`
  after a null dereference, `9476` after the watchdog panic — smaller dumps,
  one core's worth of tasks. The download and erase endpoints landed with
  SEC-13 in Lot D, above.

### OBS-2 — ESP8266: the exception cause and address survive the reset and `getResetReason()` throws them away [MEDIUM] — **DONE (2026-09-05, Lot A, the day it was filed)**

- **File**: `Platform_ESP8266.h:215-230`; consumers `SystemInfo.h:224-240`,
  `System.h:590-625` (`bootdiag`).
- **Measured** on the nodemcuv2: after a null dereference the next boot's
  `rst_info` carries `exccause=28, epc1=0x40201297`; after a soft WDT,
  `exccause=4` and **epc1 pointing into the loop that hung**; after a
  hardware WDT the SDK still reports an epc1 in the loop (one sample). The
  core already formats all of it — `ESP.getResetInfo()` (`Esp.cpp:506-512`)
  — and our HAL keeps only the enum.
- **What it cannot see, measured**: an `abort()`, an `assert`, a `panic()`
  and **an out-of-memory `new`** all reach the next boot as reason 4,
  "Software/System restart", with nothing in `rst_info` — the postmortem's
  user-exception reason (254) is never written back. Those deaths are
  indistinguishable from `ESP.restart()` by reset reason; only OBS-3's crash
  callback separates them. The entry says so, so Lot A is not sold on a
  reading it cannot deliver.
- **Fix**: `ESP.getResetInfo()` in the boot log; the six words in the boot
  diagnostics struct (zero on other platforms) and in `bootdiag`. And a
  documentation line: "Software/System restart" on an ESP8266 you did not
  restart is an abort or an OOM, not a clean restart.
- **Verification**: the probe's null-dereference and soft-WDT rows are the
  expected values; the native stub reports zeros and says so.
- **Fixed (Lot A)**: `HAL::Platform::ResetDetail` / `getResetDetail()` on all
  three platforms — the boot line is formatted from the struct, one source of
  truth (the review found the first shape re-reading the SDK for the string,
  so log and `bootdiag` could disagree); `BootDiagnostics::resetDetail`;
  a WARN at boot and a `Reset detail:` block in `bootdiag` with the addr2line
  hint; on ESP8266 an INFO line saying that "Software/System restart" is also
  what abort(), assert and an OOM `new` report. **Measured on the nodemcuv2
  with a System built from the branch**: a null dereference boots into
  `Fatal exception:28 ... epc1:0x40217580`, a hung loop into
  `Software Watchdog ... epc1:0x40217599` — the loop itself. Native: the stub
  seams (`setResetDetailForTest`, `setResetReasonForTest`) drive seven new
  SystemInfo tests and the `bootdiag` path in the System suite.

### OBS-3 — a flight recorder in RTC memory: heap trend, phase marker, last loop timestamp, crash-callback record [MEDIUM] — **DONE (2026-09-06, Lot B)**

- **Problem**: nothing on either platform records what the firmware was
  doing or how its heap was moving before a reset — and on ESP8266 nothing
  can tell an OOM death from a clean restart (OBS-2).
- **Design** (spec v2 §4 L1), the four points the review changed:
  **owned by Core, not SystemInfo** — SystemInfo is optional and last, the
  recorder must be mandatory and first; **promotion is the first act of
  `System::begin()`**, before WiFi, so a device that dies again during
  bring-up still gets its record out on the boot that connects;
  **the discriminator is the record's own "crash callback ran" flag**, not
  `wasUnexpectedReset()`, which the board showed would skip `abort()` and
  OOM-in-`new`; **whole 32-bit words** — ESP8266 RTC user memory is
  word-addressable, from word 32 because words 0–31 are `eboot_command`.
  A fast ring (16 × 10 s) and a slow ring (8 × 10 min) of allocatable free
  heap and largest block in 16-byte units, each tick carrying the
  minimum since the previous tick (a one-second cliff must not read as a
  plateau); a one-store phase marker; a build id; on ESP8266 the callback's
  reason, exccause, epc1, excvaddr, failed-alloc caller and size, and 16
  stack words. Persisted to Storage as a ~40-byte summary, deduplicated —
  the ESP8266 backend hex-encodes blobs into a resident JSON document.
- **Measured foundations**: RTC user memory survived all seven deaths on
  the nodemcuv2 (exception, soft and hardware WDT, abort, OOM, restart,
  external reset); `RTC_NOINIT_ATTR` on the WROOM-32D survived three panics
  and **was lost on an EN-pin reset, which reports POWERON**; on the
  **ESP32-C3** the USB-Serial-JTAG reset pulse reads reason `Unknown` (0) and
  **keeps** `RTC_NOINIT_ATTR` — the two ESP32 families differ on the one
  reset a bench uses most. Brownout
  survival could not be measured: on the ESP32-CAM from FTDI 3.3 V the sag
  took the USB adapter off the bus at 112 s and it did not come back, so the
  design assumes the worst case (reason only, no record) for brownouts.
- **Hooks are opt-in or chainable**: a strong `custom_crash_callback` in a
  published library is a link error for every EspSaveCrash user; the
  ESP32 failed-alloc slot is single. Both behind a define System sets and
  bare-Core users can leave off, both forwarding to a user hook.
- **Verification**: forced-crash console commands (`crash abort|oom|null|
  swdt|hwdt|hang`) **compile-gated** — SEC-4 is open and these are a remote
  reboot — on in the test environments, off in every shipped default. Per
  platform, per command, the probe's rows are the expected values. Three
  removal checks recorded once each: no callback → empty exception block
  after `crash abort`; no sampler → empty rings; no hook → zero `last fail`.
- **Shipped 2026-09-06 as Lot B**, plan `spec-obs-lot-b-flight-recorder.md`
  v2 (adversarially reviewed before S1: 22 findings, four of which changed
  the design — the record stays in RTC until persisted, a software reset
  without the `ours` flag is promoted, the phase marker is a direct RTC
  store, the LittleFS write count restated from the source). Five slices:
  `FlightRecorder.h/.cpp` in Core with the platform primitives; Core
  wiring and BUG-36; the hooks and the build id; System, the `bootdiag`
  blob, the crash commands; the board campaign. **What the boards found
  that the 115 native tests had not**: every promoted record read *torn*
  on the WROOM-32D, because the phase marker sat inside the CRC while
  being stored to RTC between flushes — it is outside the CRC now, self-
  validated by its complement, and a native test pins the shape;
  `crash oom` on ESP8266 ran into the soft WDT twice before it ran out of
  memory, the compiler having elided an unobservable chain of `new`s (a
  `volatile` global fixed it); the rings were printed nowhere, so no
  removal check could see them (`bootdiag` prints the fast ring, eight
  samples per line, because the ESP8266 log buffer is 128 bytes and the
  first version's boot line was cut at "build 7251aa").
- **Measured, nodemcuv2 (`obs-lotb-probe`), the next boot's `bootdiag`
  after each command**: `abort` → `crash callback`, reason 254; `oom` →
  reason 254, **last failed alloc 1024 B from 0x4020b41c, min free
  1488 B** where the ring's plateau was 32 KB; `null` → reason 2, exccause
  29, epc1; `swdt` → reason 3, exccause 4, epc1 on the loop; `hwdt` →
  `unexpected reset` (no code ran) with the phase still there and the
  SDK's epc1 in the reset detail — F7's second sample; `reboot` → `none
  recorded`. Every epc1 resolves to `crashForTest` with `addr2line` on the
  kept ELF (no line numbers without `-g`, as the spec said; the ESP32 ELF
  gives file:line). **WROOM-32D (FullStack)**: `abort`, `null`, `oom` →
  `unexpected reset`, phase 5 (the console), Panic; `hang` → the 30 s loop
  watchdog aborts `loopTask`, next boot reads Task watchdog with the
  record intact, uptime 401 s; `reboot` → `none recorded` through the
  restart hook, and a raw `esp_restart()` (`crash restart`, not through the
  HAL) → `none recorded` through the shutdown handler alone. **Removal
  checks, each once**: `-DDOMOTICS_CRASH_HOOKS=0`
  → `abort` reads `software reset not requested by the firmware` with no
  callback line; `-DDOMOTICS_FLIGHT_RECORDER_TICK=0` → `ring: (empty)`
  after a 30 s run; the restart hook mutated to install nothing → the boot
  log says `Restart hook not registered` and `reboot` is promoted. The
  record survived every death class on both boards; the WROOM's EN reset
  still loses it (design §7).
- **Cost, measured with the probe's stopwatch around `System::loop()`**:
  nodemcuv2 idle loop **39 µs** without the recorder's tick, **46 µs**
  with — the phase marker's four RTC stores ≈ 4 µs, the free-heap read
  ≈ 3 µs; maximum unchanged (0.3–1.5 ms). WROOM-32D: 74, 90–93 and
  113 µs across the three builds (tick off, tick on, marker off) in no
  consistent order, maximum ~249 ms in all three — that loop is dominated
  by something other than the recorder, unmeasured here. The free-heap
  read is gated to once per millisecond either way. Flash: +2.5 KB on
  ESP8266, +2.2 KB on ESP32 (S2, before the hooks).
- **Storage**: one `bootdiag` blob (64 B) beside `boot_count` — two writes
  per boot on a backend that rewrites its file per changed key, measured
  on the stub (residual 9 restated, not the "three to one" the spec
  hoped). Dedup on (build, callback reason, epc1 or fail caller, reset
  reason): on ESP32, where no callback fills the record, a panic and a
  task watchdog differ only by that last field, and two panics from the
  same phase count as one — the core dump is what tells them apart until
  Lot C.
- **The three-layer review of the diff, before the PR** (Blind Hunter,
  Edge Case Hunter, Verification Gap with mutations): the hold guarded RTC
  instead of the promoted record, so a death in a component's `begin()`
  — the boot loop this exists for — left nothing (fixed; pinned through
  `System::begin()` with a component that records a second death and the
  blob keeping the first); the ring printed in slot order once wrapped
  (ordered by uptime now); the first log line and the ring lines exceeded
  the ESP8266's 128-byte log buffer (every line under 128 now); the loop's
  wiring to the recorder — tick, phase, drops — was observed by no test
  (four added, asserting from inside components and through the logger);
  the running-minimum test was satisfied by its own crash-time value
  (burst, recover, crash now); the dedup key conflated every bare death
  (reset reason and, without a site, the phase are in it); `--expect-drop`
  passed on silence (it reconnects to tell "down" from "ignored me");
  `initializeAll()` carried no marker (0x100 | index now, decoded by name
  in `bootdiag`). Residuals it recorded: deaths during an unacknowledged
  crash loop are not counted (the held record keeps the first, the
  sequence number stands still); `umm_last_fail_alloc` is "since boot", a
  handled `nothrow` failure earlier in the run is what a later unrelated
  death reports (**closed by Lot C**: latched into the record's group and
  cleared at every sample, measured on the board); residual 8 of the design
  (the `cont` stack during `bootdiag`) now stands against a 1 KB buffer,
  unmeasured.
- **What it does not do, still**: read the ESP32 core dump or publish
  anything (Lot D); the ESP32 failed-allocation hook (Lot C: `last fail`
  stays zero there); the WebUI/OTA/MQTT sub-phase markers; brownout or
  EN-reset survival on ESP32. The release that ships this must announce:
  `DOMOTICS_CRASH_HOOKS` and the strong `custom_crash_callback` (a sketch
  defining its own fails at link unless it sets the define to 0),
  `DOMOTICS_ENABLE_CRASH_COMMANDS`, the `bootdiag` blob replacing three
  Storage keys, the `bootdiag` text change, the new per-loop cost.

### OBS-4 — the moment memory ran out is never recorded [MEDIUM] — **DONE (2026-09-06, Lot C)**

- **Problem**: the record said how the heap *trended* before a death and,
  on ESP8266 only, which allocation *killed* the process. An allocation
  that failed and was survived — the way a WiFi stack or a JSON document
  degrades for minutes before something dies — left nothing; on ESP32 the
  `last fail` fields were always zero, and on ESP8266 the core's
  `umm_last_fail_alloc_addr/size` were "since boot", so a handled failure
  early in the run is what a later unrelated death reported (Lot B's
  recorded residual).
- **Measured before the lot (2026-09-05)**: on the nodemcuv2 a failing
  `new` recorded its caller and size on a stock build (`abi.cpp:38-46`);
  on the WROOM-32D `heap_caps_register_failed_alloc_callback()` fired once,
  at the terminal failure (4 096 B, caps `INTERNAL|DEFAULT`) while
  `ESP.getFreeHeap()` reported 91 264 B "free" of IRAM `malloc` cannot use;
  zero firings in 60 s idle without WiFi.
- **Read for the plan, and what the review corrected** (plan
  `spec-obs-lot-c-oom-moment.md` v2, `review-obs-lot-c-adversarial.md`, 19
  findings): the ESP32 hook slot is a single `.bss` word with no getter;
  `heap_caps_alloc_failed` runs on the failing task on either core after the
  heap lock is released, and sits in `.iram1` where the two heap reads sit
  in `.text`; the shipped `sdkconfig` has `CONFIG_SPIRAM` and
  `ALWAYSINTERNAL 4096` — one firing per failed `malloc` on the bench
  boards only because `esp_spiram_init()` fails without the chip, **two per
  request over 4 096 B on a PSRAM board**, the first with caps `0x1400`. On
  ESP8266 **the stock build records almost nothing for the C paths**:
  `operator new` (`nothrow` included) and newlib's `_malloc_r` family set
  the globals; `String` (`WString.cpp:246`), ArduinoJson, lwIP and the SDK
  allocate through umm's own `malloc`/`realloc`/`pvPortMalloc`, whose
  recording macro the stock build compiles to nothing (`heap.cpp:209-217`,
  "64 more bytes of IRAM to turn on") — `-DDEBUG_ESP_OOM` is what makes
  them visible. This repository has zero `nothrow` sites.
- **Fix (Lot C, PR #66)**: **layout 2** of the RTC record — `w56-60`
  become the failed-allocation group (a count word validated by its
  complement, size, `free16|largest16`, the site — ESP8266 caller / ESP32
  caps — and uptime), **outside the CRC like the phase marker and never
  written by the tick's flush**, because the ESP32 hook stores it straight
  to RTC from whichever task failed, under a `portMUX` critical section,
  count word last; a layout-1 record is read with its own CRC rule once and
  rewritten (the first boot after the upgrade keeps its death — measured:
  `boot #29` continuing Lot B's `#27` where a rejected layout restarts at
  `#0`). ESP32: the hook is taken as the **last act** of `begin()`, so a
  failure on another task during the fresh record's write is not erased
  (pinned natively: "Expected 1 Was 0" when registered first); a user hook
  chains through `onFailedAlloc()`; opt-out `DOMOTICS_CRASH_HOOKS=0`.
  ESP8266: every millisecond-gated sample of `tick()` takes the two globals
  into the group under `xt_rsil(15)` and **clears them**, so the crash
  callback's fail fields mean "the allocation that killed us, or nothing"
  — and the core's own postmortem line `last failed alloc call` now means
  the same narrower thing. One largest-block walk per tick interval, shared
  with the cliff walk. `Core::loop()` logs `Allocation failures since boot:
  N, last S B` at most once a minute; `bootdiag` prints the promoted
  record's group before the ring and a `this boot: failed allocs N` line
  for the current run. Two buffers from Lot B fixed on the way:
  `Core::begin()`'s 768 bytes against a saturated 942, and
  `getBootDiagnostics()`'s unclamped cursor (eight bytes short of
  reachable, kept as a defence). Crash commands `nothrow`, `squeeze`,
  `release` (survived; the console answers `done:`), `crash_check.sh`'s
  no-drop mode.
- **Measured, nodemcuv2 (`obs-lotb-probe`)**: `oom` → callback `last
  failed alloc 1024 B from 0x4020b948`, group empty; `nothrow` → `this
  boot: failed allocs 1` in the same run, then `abort` → the callback's
  fields **zero** and the group `1 | last 1048576 B from 0x4020b864 | free
  29696 B, largest 28880 B` — Lot B's residual closed on the board;
  `nothrow` then `hwdt` → the group survives a reset that ran no code.
  **Removal check**: `-DDOMOTICS_FLIGHT_RECORDER_TICK=0` (no latch) →
  `nothrow` then `abort` reports the stale 1 048 576 B failure as the
  death's (`from 0x4020b62c`) — the residual reproduced on demand. Loop
  cost: 39 µs with the tick off, 46 with it on, as Lot B measured; the
  latch adds nothing resolvable — **but two boots of the stock build read
  81–82 µs where two of the HWDT build read 46 on the same day**, and the split follows the build, not the boot (three boots each: 81–82 vs 46). **Then one flash settled it**: the `-g` image of the same source — the same code, laid out 416 B differently — reads 58 µs where the stock image reads 87–88. **The ESP8266's per-loop cost depends on the link layout**, by up to a factor of two: the loop's hot code sits in flash behind a 32 KB instruction cache, and where the linker puts it decides the miss rate. So no per-loop cost below ~40 µs can be resolved by comparing two builds on this platform — Lot B's 39/46 figures were two builds too, and the recorder's "7 µs" is inside this spread. What holds: the tick-off build of this lot reads 39 like Lot B's, and nothing this lot adds per loop is more than two loads and a branch per millisecond.
- **The diagnostic profile, measured** (five probe builds with a fixed
  build id; `.irom0.text` is flash code, `.text1` is IRAM):

  | build | `firmware.bin` | `.irom0.text` | `.text1` (IRAM) | `.rodata` / `.bss` | heap at boot | what it bought |
  |---|---|---|---|---|---|---|
  | stock | 413 695 | 375 380 | 28 717 | 7 780 / 31 864 | 34 752–34 800 | — |
  | `-g` | 413 279 (**−416**) | 374 964 | 28 717 | same | — | `addr2line` on the `oom` caller: `Platform_ESP8266.h:399` where the stock ELF gives `crashForTest ??:?`; the group's `bigstring` site: `String::changeBuffer` `WString.cpp:246`. **Not byte-identical**, against the plan's "by construction": two stock builds of one source hash identically (`d0a9764a…`), the `-g` build differs (`.irom0.text` −416 B, twice); the delta sits in ArduinoJson template instantiations whose sizes move both ways between the two ELFs — the shape of GCC's identical-code folding (`-fipa-icf`, on at `-Os`) choosing different representatives once debug info tells functions apart. Hypothesis, not chased further; the point stands that `-g` is free in RAM and IRAM, not invisible in flash |
  | `-DDEBUG_ESP_OOM` | 415 011 (+1 316) | +440 | **+840** | +36 / +8 | 34 760 | `:oom(1048576)@abi.cpp:72` on the console at each `nothrow` (debug output on); **`bigstring` — a `String` grown until its `realloc` fails — counts under the profile (`failed allocs 3, last 30032 B`) and not on the stock build (no line)**; loop 70 µs, inside the boot-to-boot spread |
  | `-DDEBUG_ESP_HWDT` | 415 079 (+1 384) | +1 208 | +212 | −16 `.bss` | 34 704 (−48) | after `crash hwdt`: `Hardware WDT reset`, then 372 stack lines (`ctx: sys`, `ctx: cont`) for the exception decoder; the greeting prints at the ROM's 74 880 baud and reads as noise at 115 200 |
  | `-DDEBUG_ESP_HWDT_NOEXTRA4K` | 415 095 | +1 224 | +212 | same | 34 704 | the same; the 4 KB the name is about moves between the `sys` stack and the dump, not the heap at boot — the review expected −4 096 here and measured none |

  Recipe: `[env:nodemcuv2-diag]` in the probe's `platformio.ini`
  (`-DDEBUG_ESP_OOM -g`, the fixed id), `EXTRA_BUILD_FLAGS` for any
  example; the flags must be `build_flags` so they reach every library.
- **Measured, WROOM-32D (FullStack with the crash commands, WiFi + MQTT
  connected, HA entities published)**: `oom` → `unexpected reset` with the
  group `1 | last 4096 B caps 0x1800 at 40.221 s | free 6720 B, largest
  2800 B` — the failure stamped 9 s after the record's last tick, which is
  the panic-right-after shape the RTC-direct store exists for; `nothrow` →
  `Allocation failures since boot: 1, last 1048576 B` on serial and `this
  boot: failed allocs 1` in the same run; `crash restart` → `none
  recorded`, unchanged. **The rate on a healthy device** (the reading D6
  wanted before one firing is believed): **zero** in ten minutes idle on
  the LAN, **zero** through one natural 1.35 MB OTA upload, **zero**
  through a 60 s WebUI session of 240 requests over six routes with the SSE
  stream open, MQTT connected throughout. **Squeezed to 12 KB allocatable**:
  13 firings within seconds, the Telnet console stopped answering (a dying
  console is not the device dying — the plan said so, and the readings
  taken through it in that state are void), and **twelve minutes later the
  device died of the task watchdog after 168 survived failures**, the last
  2 308 B at 4 752 B free and 1 264 B largest, phase 5 — the record of a
  death by starvation, which is what OBS-4 was for. So on ESP32 one firing
  is news, not weather: a healthy FullStack never fires; a device that
  fires is on its way down, and the count says how far. 12 KB is past the
  cliff on this platform (lwIP and WiFi fail at once); a slower leak was
  not staged. **Removal check**: `-DDOMOTICS_CRASH_HOOKS=0` → `oom` reads
  `unexpected reset` with no group. Flash: +1 032 B esp32dev, +572 B
  esp8266dev (S2 over S1); the hook path itself +148 B.
- **Verification, native**: 23 cases over the lot (21 in Core, 2 in System; 913 → 936 on the count the Lot B handoff used): the count word by literal (`0x0003FFFC`, `0xFFFF0000`), a
  firing with no tick then a boot, the flush never writing the group (red
  on the single `memcpy`), the group outside the CRC (red inside it), one
  walk for a burst and a cliff, RAM-only while a death is held, the layout-1
  migration, the saturated `format()` under 128 per line and 1024 in all,
  registration last, the once-a-minute line, one failure across twenty
  samples counting once (red without the clear: "Expected 1 Was 20").
- **Residuals recorded, unfiled**: a PSRAM board counts attempts (two per
  request over 4 096 B, the caps word of the last); a panic between the
  group's field stores and its count store leaves the previous count over
  new fields; two ESP32 writers serialise on the spinlock and the second's
  fields win; survived failures during a bring-up that dies before
  `acknowledge()` stay in RAM and are lost — by design, since writing them
  over a held death's group would attribute them to it (ADR 0002, Consequences);
  a slower ESP32 leak than "12 KB at once" was not staged; the ESP8266
  stock build's coverage is what it is — the profile is the answer.
- **The code review before the PR** (`review-obs-lot-c-code.md`, four
  layers, 41 raw findings, 24 patched): a survived failure that reaches the
  crash callback under an exception within the millisecond before the latch
  was reported as the death's cause — routed to the group now, pinned; a
  deliberate `DOMOTICS_CRASH_HOOKS=0` warned "hook not registered" on every
  ESP32 boot; `nothrow` and `squeeze` would have allocated from PSRAM;
  the once-a-minute gate, the count-last store, the per-platform site
  label and the boot-log buffer had no observer — four tests, each red on
  the mutation it names.
- **The release that ships this must announce**: layout 2 (a one-boot
  migration, nothing lost); the ESP32 failed-allocation slot taken unless
  `DOMOTICS_CRASH_HOOKS=0`; the ESP8266 postmortem's `last failed alloc
  call` now meaning "since the last sample"; what a stock ESP8266 build can
  count and what needs `-DDEBUG_ESP_OOM`; the new WARN line and the two
  `bootdiag` lines; the two buffer fixes.

### OBS-7 — ESP32: a stuck `loop()` never reboots [MEDIUM] — **DONE (2026-09-05, Lot A)**

- **Measured** on the WROOM-32D: a busy loop in `loop()` produced no reset
  in more than 40 s against a 5 s task watchdog. **Read**: `loopTask` is
  created with `loopTaskWDTEnabled = false` (`cores/esp32/main.cpp:34,69`),
  the TWDT watches `IDLE0` only (`CHECK_IDLE_TASK_CPU0=y`, `CPU1` unset), and
  the loop runs on core 1. When the TWDT does fire it panics
  (`TASK_WDT_PANIC=y`) and therefore leaves a core dump with the hang's
  backtrace.
- **Why it is filed on its own**: a hung ESP32 stays hung — LWT fires, WebUI
  dead, no recovery — where an ESP8266 resets in seconds. `enableLoopWDT()`
  is one line, and turning it on is a behaviour decision that affects every
  user's blocking loop, so it is decided in the open, in Lot A, default
  proposed **on** with a configurable timeout.
- **Verification**: `crash hang` reboots with reason PANIC and a dump;
  removal check: with the WDT off, the probe's >40 s silence.
- **Fixed (Lot A)**: `SystemConfig::loopWatchdogSeconds` (default **30**, `0`
  off) → `HAL::Platform::enableLoopWatchdog()` at the end of `System::begin()`
  — `esp_task_wdt_init(seconds, panic=true)`, then **the calling task**
  subscribed with `esp_task_wdt_add(NULL)` and verified with
  `esp_task_wdt_status(NULL)` — and `System::loop()` resets that same task's
  entry, so a sketch that drives System from its own FreeRTOS task is watched
  and fed consistently. The adversarial review (run after the PR opened,
  `review-obs-lot-a-adversarial.md`, 13 findings) caught the first shape,
  which armed `loopTask` through the Arduino wrapper and fed whatever task
  called `loop()`: a sketch on its own task would have panicked 30 s after
  boot, by default. Also from that review: `supportsLoopWatchdog()` so
  System WARNs "requested but did not arm" on ESP32 and stays quiet on
  ESP8266; the armed flag is a function-local static, not a C++17 inline
  variable, because the core compiles this header as gnu++11/14; on IDF 5
  the function compiles with a `#warning`, arms nothing and says so
  (residual: port to `esp_task_wdt_reconfigure`). Re-measured after the
  change on the WROOM-32D and the C3: `task_wdt: loopTask` in ~5 s at 5 s,
  silent at 0. No-op on ESP8266 (the soft WDT already
  resets a hang in ~3 s, re-measured) and on the stub, which records the
  call for four new System tests (one fails when the feed is deleted:
  `Expected 3 Was 0`). **Measured on the WROOM-32D at 5 s**: the hang is
  aborted by `task_wdt` (`loopTask (CPU 1)`) in ~5 s, the next boot reads
  `Task watchdog` and finds a 15 748-byte core dump; with the timeout at `0`
  the same hang was silent for the remaining ~40 s of the capture. The
  default is a behaviour change for ESP32 users and the CHANGELOG entry of
  the release that ships it must say so at the top — and it does, in #58,
  with the two facts the review added: the task that is armed, and that the
  TWDT timeout moves globally (ESP32's idle task goes 5 → 30 s with it).
- **Residual 6, answered 2026-09-05 before the tag.** A FullStack build with
  a stopwatch around `System::loop()` on the WROOM-32D, watchdog at its
  shipped 30 s: longest iteration **0.37 s idle, 0.80 s during an HTTP
  firmware upload**; `System::begin()` 1.7 s; a synchronous WiFi scan called
  from `loop()` **8.1 s**. And the fact that closes the question by reading:
  the image verification and commit — `finalizeUpload()` / `end(true)` —
  run in OTAWebUI's `final` chunk on the async server task
  (`OTAWebUI.h:498-500`), never on the loop task. 30 s is 37× the longest
  loop-side figure. The C3 was not measured: it was unplugged mid-session.
  The upload itself never completed — that is BUG-37, filed from this run
  and fixed the same evening (the server's 3 s receive-idle limit, not Lot A),
  and not Lot A's: it fails identically with the watchdog at 0.
- **Residuals still open, none blocking**: the IDF 5 port; the hardware-WDT
  `epc1` on ESP8266 rests on one sample and the log line says so.
- **ESP32-C3, measured the same day — the first thing ever run on that
  target.** The hypothesis was that a single core would starve `IDLE0` and
  trip the TWDT without help. Wrong: the C3's precompiled sdkconfig has **no
  `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` at all**, so nothing is on the
  task watchdog and a busy `loop()` was silent there too (generic probe,
  >55 s). With Lot A at 5 s: `task_wdt: loopTask (CPU 0)` aborted the hang
  in ~5 s and the next boot found a 9 476-byte dump; with the timeout at 0,
  silent again. Same fix, same result, different reason.

### OBS-5 — nothing leaves the device: no crash record on MQTT, no heap telemetry anywhere [MEDIUM] — **DONE (2026-09-14, Lot D)**

- **Problem**: `SystemInfo` computes heap every 5 s for the WebUI only
  (`SystemInfo.h:178-200`); MQTT publishes an LWT and its own stats; the
  HomeAssistant component has `addSensor()` (`HomeAssistant.h:189`) and no
  diagnostic entity. A slow leak over a week is visible on a curve and on
  nothing this library produces.
- **Fix (Lot D) — one topic scheme, and Home Assistant discovers it.** The
  design as filed prescribed two publishers (HA entities, raw topics
  otherwise) and "retained QoS 1"; the code refused both. `MQTTComponent`
  never forwards QoS to the HAL (`MQTT_HAL.h:70` has no parameter;
  PubSubClient publishes QoS 0 only), so QoS 1 is not achievable without
  replacing the client and the sentence is corrected here rather than
  implemented. And seven separate state publishes per tick would spend most
  of BUG-29's 10/s window on telemetry. So: **always** `{clientId}/telemetry`
  every `telemetryIntervalSec` (default 60, `0` disables) and
  `{clientId}/crash` retained at every connect; when HA is present, eight
  `diagnostic` sensors read those two topics through `value_template`
  (`free_heap`, `sys_heap_largest`, `sys_heap_min`, `uptime`,
  `sys_boot_count`, `wifi_signal`, `sys_alloc_failures`, `sys_last_death`
  with the crash payload as its `json_attributes_topic`). That needed what
  HA did not have — `entity_category`, `value_template`,
  `json_attributes_topic`, a per-entity state topic, an availability
  opt-out — five fields on `HAEntity` emitted only when set and honoured by
  `publishState()`/`publishAttributes()` too (the code review's first
  finding: discovery advertised the override and the publishers wrote to
  the generated topic), an `entity(id)` accessor, and a config that can at
  last name the topic `publishAttributes()` had always written to. **`sys_last_death` carries no
  availability block** (review finding 1): the LWT would mark it
  unavailable at the moment the device dies, the moment the record is
  wanted. **Three ids are the ones FullStack used to declare** (finding 2):
  configs are retained and only `shutdown()` clears one, so retiring
  `free_heap`/`uptime`/`wifi_signal` would leave three frozen entities on
  every upgraded device; FullStack drops its hand-rolled three and keeps
  the unique_ids. A duplicate id now warns at registration (HA keeps the
  first config).
- **The publisher** is `SystemHelpers::SystemTelemetry`, a pure component in
  System with a sink — it knows nothing of MQTT, HA or the HAL, which is
  what lets the System native suite (which compiles without MQTT and HA by
  design, `platformio.ini:10-12`) prove every rule with a scripted sink.
  The tick is `snprintf` into a stack buffer and
  **`MQTTComponent::publishNow()`** (new: no queue, no `String`; offline or
  over the rate limit it returns `false` and the sample is gone — a sample
  delivered late is wrong, and the old path would have allocated two
  `String`s and a vector slot to deliver it stale). "Allocation-free" ends
  at PubSubClient: lwIP allocates the pbuf, and on ESP8266
  `ClientContext::_write_from_source()` can wait up to 5 s per segment on a
  starved link (finding 7) — measured below rather than argued. Fields:
  `heap` and `largest` from the HAL's **allocatable** gauges (finding 6:
  `getFreeHeap()` on ESP32 counts IRAM `malloc` cannot hand out — 116 068 B
  allocatable against the 165 292 B the old `free_heap` sensor reported on
  the same board), `min` the recorder's trough inside its last 10 s window,
  `boot`, `rssi`, `fails`/`fail_last` from `failedAllocSnapshot()`,
  **`skipped`** (ticks the 4 096 B floor refused, on the allocatable gauge)
  and **`failed`** (ticks the transport refused) as two counters (finding
  8), each carried by the next payload that goes out. The crash payload
  names its `source` (`persisted` from the `bootdiag` blob held in System
  since boot — no Storage read per reconnect; `rtc` from the promoted
  record without Storage; `none`), emits two fields and `torn:1` for a torn
  record (finding 12), `same_count` when persisted, `reset` when from RTC,
  and the core dump state on ESP32. An MQTT client id over 80 characters
  disables telemetry with one WARN, so the topics the publisher builds and
  the ones the discovery configs name cannot diverge (finding 10).
- **Measured on the WROOM-32D (FullStack, WiFi + MQTT + HA + WebUI),
  2026-09-14**, the broker's view through `tools/on-device/mqtt_watch.py`:
  at connect, `FullStackDevice/crash` retained, 265 B — Lot C's death from
  the persisted record, `unexpected reset`, `coredump.waiting: true, size
  18020`; then the eight configs, **401 to 568 B** each against the
  699-byte event cap (the largest is `sys_heap_largest`; `sys_last_death`
  is the smallest, without its availability block; a `configuration_url`
  or a long device name is what would push one over, and `mqttPublish()`
  now warns at the cap); the first telemetry at uptime 60, **144 B**. After
  `crash null` the crash topic republished with the new death and
  `coredump.waiting: true, size 18052`; after the erase and a reboot, the
  same death with `waiting: false`. **A Home Assistant read it**
  (`homeassistant/home-assistant:stable` in a container on the host,
  onboarded and pointed at the bench broker through its API): eleven
  entities for the device, the eight with `entity_category: diagnostic` in
  its registry, `data_size` with `B` and `duration` with `s` accepted,
  `uptime` as `total_increasing` recorded `60 → 120 → (reboot) → 60 → 120 →
  180` as history without complaint, `last_death`'s state `unexpected
  reset` with the whole crash payload as attributes. **With the device's
  availability topic at `offline`** (its LWT, published by hand on the
  broker): the seven telemetry sensors and `temperature` read
  `unavailable`, `last_death` kept `unexpected reset` — review finding 1,
  measured. A `configuration_url` of `http://192.168.1.224/` would add 42
  bytes to each config (610 for the largest), computed: nothing in
  `SystemWebUISetup` sets one today. **Squeezed**
  (`crash squeeze`, ~100 KB held, 13 KB allocatable left, largest block
  5 620 B): **no death in 21 minutes**. Telemetry left every minute at
  12.4–13.2 KB allocatable; the survived-failure count climbed from 0 to
  9 (`fail_last` 2 308 B, the same allocation Lot C's hook caught) and the
  payload carried it as it rose — `fails` 2 at uptime 600 s, 4 at 960, 8
  at 1020, 9 at 1260 — with `skipped` and `failed` at 0: the floor never
  engaged (13 KB against 4 KB) and the tick never failed. Lot C's squeeze
  died of the task watchdog after 168 failures in 12 minutes with the
  console dead and the WebUI in use; this run had no WebUI client and a
  live console, and failed a hundred times less often. The `min` field
  read 5–9 KB while `heap` read 12.4–13.2: the trough between samples
  is what the point sample hides — which is why, after the review, the
  payload carries both: `min` as the boot-long minimum the entity name
  promises, `trough` as the last window's low.
- **Measured on the nodemcuv2** (`probes/obs-lotb-probe` with MQTT, no HA):
  telemetry 142 B at uptime 60 (`heap` 19 896, `largest` 19 176); the crash
  topic at connect carried Lot C's `nothrow`+`abort` death from the
  persisted record. `crash nothrow` then `crash abort`: the next connect
  published `crash callback`, `reason 254`, `fail_size 0` (the latch had
  cleared the survived failure, as Lot C specified), `same_count 1`; a
  second `abort` → `same_count 2`. **The tick's cost under `crash squeeze`**
  (12 632 B allocatable): the loop stopwatch's maximum iteration in the 5 s
  windows holding a tick reads **2 207 and 2 211 µs**, against 462–473 µs in
  the windows without one — about 1.7 ms for the tick and its TCP write,
  no 5 s stall, telemetry flowing at 12.6 KB (`heap` 12 632, `largest`
  11 616, `min` 10 784).
- **Budgets and costs**: the EventBus's 32-entry cap let a device declare
  **29 sensors** before the connect burst dropped a config (pinned by a
  test on today's code before the change); the eight system entities leave
  an application **21**. FullStack against `main`, same flags, after the
  review's patches: flash **+10 908 B esp32dev, +6 596 B esp8266dev,
  +10 492 B esp32c3** (the publisher, the routes, the HAL seams, the five
  fields, the middleware); RAM **+8 B esp32dev, +1 216 B esp8266dev, +8 B
  esp32c3**. The ESP8266
  figure is the string literals — the entity table and the two JSON
  formats, ~1 KB of them — which that core places in DRAM (`.rodata`),
  the lever being `PROGMEM`/`PSTR` behind a HAL macro — MEM-7, with
  RemoteConsole's help text, which has the same cost; recorded, not done.
- **Announcements for the next release** (seven): telemetry on by default
  with MQTT (a message a minute and a retained topic per device; `0` turns
  it off); eight diagnostic entities per device in HA, once a minute in its
  recorder; three of their ids are FullStack's old ones, and an id the
  application declares first is left to it; the retained crash topic
  outlives a decommissioned device until someone clears it
  (`mqtt_watch.py --clear`); the entity budget of 21; `/api/system/info`
  and the SSE stream now require credentials when `enableAuth` is on, and
  `/` follows a runtime `enable_auth` change like every other route
  (SEC-13); two new routes, `GET /api/system/coredump` and
  `POST /api/system/coredump/erase` (OBS-1); a discovery document over the
  699-byte event field is now refused rather than published cut (BUG-38
  names the one entity type this touches). QoS 0 where the roadmap said 1.
  **System-only**: a sketch driving `Core` without `System` gets none of
  this. **For marianorenzi's rebase**: `System.h` gains three members and
  five one-line calls into the new `SystemTelemetrySetup.h`; `MQTT.h` and
  `MQTT_impl.h` gain `publishNow()` and a test accessor beside the
  untouched overloads; the RSSI sample assumes a WiFi station link and
  publishes `null` without one, which is what a transport-neutral layer
  would want.
- **Residuals, recorded**: the crash topic carries no failed-allocation
  group (Lot C chose not to persist it); a device that dies and never
  reconnects never publishes its record (`bootdiag` has it); discovery
  payload size on ESP8266 with a `configUrl` or a long device name was not
  measured (FullStack never joins a network there, CI-14); the browser side
  of SEC-13 (see it); the `409` of the erase route.
- **Depends on**: OBS-3. Carried OBS-1's transport half.

### OBS-6 — System persists `last_heap` and `last_minheap` that describe the new boot, not the run that died [LOW] — **DONE (2026-09-05, Lot A)**

- **File**: `System.h:423-437`, `SystemInfo.h:224-228`,
  `Platform_ESP8266.h:159`.
- **Problem**: `initBootDiagnostics()` captures `getFreeHeap()` and
  `getMinFreeHeap()` at the start of the new run, and `System` persists
  them under names that claim they describe the previous one; `bootdiag`
  prints them as "Boot Heap" / "Boot Min Heap". On ESP8266
  `getMinFreeHeap()` is defined as `ESP.getFreeHeap()`, so the two are equal
  by construction.
- **Why LOW**: no behaviour depends on the values; the cost is a diagnostic
  that misleads during exactly the investigation this section exists for.
- **Fix**: rename to what they are, drop the min on ESP8266, **and remove
  the old keys on the first boot of the new build** — they would otherwise
  sit in NVS and LittleFS forever. In Lot A.
- **Fixed (Lot A)**: `BootDiagnostics` says `bootHeap` / `bootMinHeap` /
  `bootMinHeapTracked` (`HAL::Platform::tracksMinFreeHeap()`, false on
  ESP8266); Storage keys are `boot_heap` and, only where tracked,
  `boot_minheap`; `last_heap` / `last_minheap` are removed on the first
  persist. The persistence moved into `SystemHelpers::persistBootDiagnostics()`
  so a test can drive it: four new System tests, one of which fails when the
  removal is deleted (run, `Expected FALSE Was TRUE`); the tracked-minimum
  branch (the ESP32 shape) is driven natively through a stub seam since the
  review, not only on the board. **Both boards**: seeded
  `last_heap`/`last_minheap` on one boot, gone on the next; the ESP8266
  writes no `boot_minheap`, the ESP32 does.

### OBS-8 — the queue's high-water mark reaches no telemetry [LOW] — **NEW (2026-09-20, by the deferred-work purge)**

- **Parked by**: BUG-41's code review, 2026-09-18.
- **Files**: `Core.cpp:121` (the drop line), `FlightRecorder.h:74`
  (`eventDrops()`), OBS-5's telemetry document.
- **Problem**: `EventBus::getQueueHighWaterPct()` exists since BUG-41 and is
  printed only on the drop line, which fires only when the drop counter
  changes — a queue that peaks at 95 % and never drops is invisible. The
  flight record carries drops and no occupancy; the telemetry carries
  neither. The one number that lets an application see the cliff coming is
  surfaced nowhere.
- **Fix**: the peak in the telemetry document and, if a word can be spared,
  in the flight record.
- **Refs**: BUG-41, OBS-3, OBS-5.

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
| 1. Security | SEC-1 to SEC-17 | OTA, Remote, WebUI | 0C, **0H**, 2M (**SEC-17 filed and fixed 2026-09-22 by SEC-16's own lot** — with authentication on, `/api/ota/status` and `/api/ota/unified` answered `200` with the firmware URL, the running version and the update state to a client `/api/ui/token` refused in the same run; all four OTA reads now take `authorize()`, and reading `app.js` answered the question the entry had parked — nothing in the page fetches them; **SEC-16 filed 2026-09-21 and fixed 2026-09-22** — `/api/ota/update` and `/api/ota/check` were gated by the CSRF token alone, so telling the device to fetch firmware from a URL needed no password while uploading the bytes did; both routes now take `authorize()` like the upload route beside them, with five native cases, a removal check, and the WROOM-32D reading 0 of 2 routes refused before and 2 of 2 after; **the triage pass of 2026-09-21** read every open MEDIUM heading against the code; **SEC-5 was closed by SEC-10 a release earlier and never marked** — `/api/ui/action` is POST behind a CSRF check; **SEC-15 filed 2026-09-20 by the deferred-work purge** — the WebUI's HTTP authentication has no brute-force delay, MEDIUM by parity with SEC-4; **SEC-4, SEC-6 and SEC-14 done 2026-09-14 in one lot, after its plan's adversarial review and the maintainer's rule that a brute-force defence delays and never blocks** — the console's `auth` wait, the CORS header withheld under auth with the entry's premise corrected, the empty password refused on both the WebUI and the console; **SEC-13 done 2026-09-14 in OBS Lot D** — the SSE stream behind a live middleware, `/api/system/info` gated, and `/` no longer reading a stale copy of `enableAuth`; **SEC-1, SEC-3, SEC-7, SEC-8, SEC-9 done; SEC-2 done twice** — the v2.0.1 fix was inert, re-fixed 2026-08-26; **SEC-9 fixed 2026-08-27 and downgraded MEDIUM → LOW**, two of its three recorded consequences refuted against the Arduino cores; **SEC-10 CRITICAL and SEC-11 HIGH filed and fixed 2026-08-29** — a per-boot CSRF token, board-measured both directions; **SEC-12/SEC-14 MEDIUM filed and open** — SEC-12 re-argued HIGH → MEDIUM by parity with SEC-7; **SEC-5 re-pointed** onto the cross-origin axis SEC-10 measured, its history-leak point kept) |
| 2. Memory Safety | MEM-1 to MEM-8, STOR-ESP-1 | XIV (ABSOLUTE) | 0C, **0H**, 4M, 2L (**MEM-7 and MEM-8 filed 2026-09-20 by the deferred-work purge** — ESP8266 literals in DRAM, and the three things the EventBus byte budget does not bound; **MEM-1 done; STOR-ESP-1 withdrawn** — the suite measured an undrained EventBus; **MEM-2 closed 2026-08-29** across both halves — three rows fixed, one one-line change, four refuted, one re-pointed, two moved out, and the 14-character threshold the whole finding was reasoned against corrected to 10 on the ESP8266; the board run that was owed here happened 2026-08-31, 3/3 under TEST-4's closing lot; **MEM-5 and MEM-6 new and open**, both filed by the rows MEM-2 re-pointed) |
| 3. Code Safety | BUG-1 to BUG-26, BUG-28 to BUG-69 | Multiple | 0C, **0H**, 3M, 8L (**55 done**; **BUG-69 filed 2026-09-24 by BUG-67's review**, MEDIUM — the WebUI's `AsyncWebServer` is the other listening socket opened in a `begin()`, and it does not ask the question BUG-67 added; the shipped composition puts WiFi first, which hides it exactly as the console was hidden; **BUG-67 fixed 2026-09-24**, the day after it was filed — a console opening its telnet server before any IP stack existed stopped an ESP32 inside lwIP and boot-looped it; a new `HAL::canOpenServer()` answers per platform (ESP8266 always, ESP32 on its first network interface, so an Ethernet-only device counts) and `loop()` opens the server when one appears, red-then-green on the WROOM-32D with the abort reproduced by removing the guard; **BUG-68 filed 2026-09-24 by BUG-65's own review**, MEDIUM — the rule BUG-65 established was applied to one entity type and broken by every other: `pl_avail` and `pl_not_avail` are written as literals on every document of every entity, 45 characters, and `HALight` adds 28 more, with the twist that three entity types write a configurable field where the panel had none, so the answer there is the branch BUG-65 could not take; **BUG-67 filed 2026-09-24 by TEST-12's lot**, MEDIUM — `RemoteConsole::begin()` opens a telnet server, which on ESP32 reaches lwIP before the radio has ever been brought up: the device stops on an invalid mbox and boot-loops, with no console to say so, found by the first flash of that component's first device suite; **BUG-65 fixed 2026-09-24** — the seven `pl_*` keys left the alarm panel's discovery document, which had been stating the values Home Assistant assumes for an absent key: 740 → 552 on the code-less six-mode panel that was refused whole, 613 → 541 on the production shape, the entry's own 726 → 538 corrected from the run, and the vocabulary the keys stated now held by a native test because `handleCommand()` compares nothing; **BUG-64 filed and fixed 2026-09-23** — the remote console carried only `DLOG_*`, so the Arduino core's, the SDK's and ESP-IDF's own lines reached the UART and nothing else, which cost a production OTA failure its cause; the recorded single-mechanism fix was measured to capture **nothing** this firmware prints, the two ESP32 sinks being disjoint, and the ESP8266 half was off at the source because `HardwareSerial::begin()` disables SDK printing; now a `CoreLog` HAL with a fixed intake drained in `loop()`, the SDK narration off by default on ESP8266 behind a `core` command after 13 lines in 10 s were measured against a 20-entry ring, and our own lines excluded at the source after a board showed six of them captured twice; **BUG-66 filed 2026-09-23 by BUG-63's lot**, LOW — the `offline` availability and the discovery removals `shutdown()` publishes are queued on the EventBus after the Core has stopped dispatching, so a device shut down cleanly stays in Home Assistant, online and complete; whether it should withdraw them at all is the question inside it; **BUG-63 filed and fixed 2026-09-23**, from a consumer's production alarm panel — a panel configured with no code published no `cod_arm_req`, and Home Assistant reads an absent key as `true`, so the library's default and Home Assistant's were opposites and the wire could not tell `false` from missing: the entity was created, reported state, showed available, disarmed normally, and could not be armed from the UI at all, with `code_format` null so the card could not even offer a keypad. Both requirement keys now leave the gate and `cod_trig_req` follows the `Trigger` feature — the documentation gives all three a default of `true`, where the filing had seen only arming — and the panel it was filed from measures 613 characters against a 699-character field; **BUG-13 done in the same lot** — the topic crosses a 128-byte event field and was cut there in silence, so raising the component's own buffer would have been inert: the builders report the length they needed and the publish sites refuse what does not fit, counted like a refused payload; **BUG-62 done in the same lot** — a topic outside the discovery prefix is dropped before the parse and says nothing at any level; **BUG-65 filed by that lot**, MEDIUM — the seven `pl_*` keys restate Home Assistant's own defaults, 188 characters at six arm modes, which is exactly the headroom a code-less panel lacks; **BUG-62 filed 2026-09-23 from AlarmControl's bench**, LOW — `handleCommand()` is handed every message the shared MQTT client receives and reports the ones that are not its own at error level, two lines per foreign message, measured on `alarm/command`; the prefix the component already holds and prints at `begin()` closes it; **BUG-61 filed 2026-09-23 by BUG-46's decision memo**, LOW — `HASwitch::state` and `HALight::state` are public, documented as the current state, and written only by `handleCommand()`, so a relay toggled locally leaves them at the stale default; nothing in the tree reads them outside the command path, which is the argument for LOW, and what it cost was making BUG-46 look as though it had a free remedy; **BUG-46 filed 2026-09-20 and fixed 2026-09-23** — the decision memo went through the adversarial lens before the maintainer ruled: the last unpublished payload is held in the component, not on `HAEntity` (12 standing bytes against 96 on eight entities, and MEM-3 stays free), coalesced per entity, drained after discovery and paced across loops because the flush takes the connect-time EventBus burst from 1 + N to 1 + 2N against a budget of 32 events that evicts the oldest; the review widened the scope twice — `publishStateJson()` carries the same guard so every light loses its state, and `publishAttributes()` carries none, so attributes queue while states are dropped; **BUG-60 filed and fixed 2026-09-22 by SEC-16's own lot** — a settings write publishes a Storage event from the `async_tcp` task while `Core::loop()` drains the same queue from `loopTask`, and the EventBus said in its own header that it assumed one thread: an intermittent panic on core 0 inside `pendingByTopic` became a dedicated probe that boot-loops on demand, then a recursive lock in the HAL with handlers dispatched outside it — 1.88 million publishes in 89 s on the WROOM-32D, +1 336 B of flash and nothing on ESP8266; **the triage pass of 2026-09-21 found four of this row's MEDIUM already fixed in the code and never marked** — BUG-3 (`removeCallback()` erases by id), BUG-16 (`putULong64` formats with `%llu`), BUG-17 (`putBytes` hex-encodes the bytes, and the fix carries the id in a comment), BUG-18 (`fmod` wraps the phase); **BUG-24 was the same defect as DC-15 filed twice** and is folded into it; **BUG-2 was closed by argument on 2026-08-28** and its heading finally says so, moving no figure since it was never counted; **BUG-47 fixed 2026-09-21 and BUG-59 filed by its board runs** — the WebUI scan in AP mode was broken in three places, not the two it was filed for: `loop()` returned forty-six lines before the poll, so a scan started while provisioning was never harvested and the flag never cleared; the provider kept a write-only copy of the summary and never read the component's; and the button was a field of no context, reachable only by a hand-made POST while the reference documented it as an action of the page. The scan now has its own always-interactive card — a settings card is locked until Edit and receives no update while being edited, so the result could not have been drawn where it was clicked — with a Display fed from the component and a second scan refused by name. Twelve tests, three removal checks, 1 185 → 1 197 `[PASSED]`, and a board probe on the nodemcuv2 that harvests two scans and, with the call moved back, hangs at `pending=1` for twenty seconds while the SDK had delivered in 2.4 s; the lot's own review found four defects in it, fixed here. On the WROOM-32D the first scan of a process in AP mode is never delivered, which is BUG-59 — surfaced by the fix, not caused by it, and four hypotheses were refuted on the board before it was filed; **BUG-55 to BUG-58 filed 2026-09-20 by BUG-51's adversarial review** — the header rename that posts a context `SystemInfoWebUI` does not take, so it has never worked and the named refusal would have painted a permanent banner on an unrelated card (fixed in-lot); a refusal message with no lifetime and no way to dismiss it; the boolean fields that still take anything and answer success, with two providers disagreeing on the vocabulary; and LED's latched name cache against the live list its new refusal reads; **BUG-51, BUG-52 and BUG-53 filed 2026-09-20 and fixed the same day, with BUG-54 filed and shut in the same lot** — a refusal had one channel, `alert(data.error)`, so a provider chose between a blocking modal and silence and seven chose silence: twenty-six bare refusals were invisible, and the reason is now drawn under the field it belongs to, measured on the WROOM-32D each side on both paths, the silent case going from nothing at all to `Device name cannot be empty`; the accept-discard-report-success shape left the three non-numeric surfaces — the telnet `level` command, which read `abc` as NONE and stopped at 4 where its own WebUI route goes to 5, and LED's `effect` and `led_select`, one of whose pinning tests turned out to pass with and without the fix; the NTP field is denominated in seconds, so a stored interval below an hour is shown as it is rather than as a 0 the page would refuse to take back; and Cancel restored nothing, looking each field up by a bare name no element carries; **BUG-45 filed 2026-09-19 and fixed 2026-09-20** — the four settings-page divergences, plus four found while fixing them and shut in-lot: RemoteConsole read `"2424x"` as 2424, `LEDWebUI::getWebUIData()` dereferenced a null component, NTP's true ceiling is the SNTP client's 1193 hours, and the conversion wrapped anyway on the `setConfig()` and persistence paths the page never sees; a numeric settings field is now digits only (BUG-40's rule) with the range policy left to the field — refused where a range exists, clamped where a slider clamps — and `"{}"` is answered at the source and tolerated at the sink; the refusal *shape* was deliberately not touched and became BUG-51; twelve removal checks, 1 160 -> 1 173 `[PASSED]`, and a browser on the WROOM-32D each side of two fixes: `65536` in the MQTT port stored `0` in silence and now leaves `1883` standing with `Invalid port`; **BUG-47 to BUG-50 filed 2026-09-20 by the deferred-work purge** — the WebUI scan that cannot complete in AP mode and shows nothing (MEDIUM); the Last Will edit that reopens BUG-43 from the UI, BUG-44's three leftovers, and the chunk-time OTA refusal reported as "Upload not active" (LOW); **BUG-46 filed 2026-09-20 and open** — opened by BUG-44's own fix: with the `mqttConnected` guard finally firing, an entity state published during an outage is skipped and nothing republishes it, because `republishEntity()` rebuilds the discovery document and `HAEntity` holds no state; the remedy is a decision between a `String` per entity, an application-side hook and an opt-in flag, and the CHANGELOG announces the change meanwhile; **BUG-44 filed 2026-09-19 and fixed 2026-09-20** — `mqtt/disconnected` had one emission site, inside `disconnect()`, which a dropped link never reaches and which would have refused it anyway, so `HomeAssistant::isReady()` answered true over a dead link; the loss is now announced at `loop()`'s transition out of Connected, before the reconnection announces its own success, and `getState()` stops reading Connected over a dead link; one bench capture each side of the fix on the WROOM-32D, nine tests and four mutations; the code review found a second path the bench could not reach — a broker cleared at runtime — and the cost the filing had inverted: outage publishes were queued and arrived stale, not lost, but now that the guard fires an entity state published during an outage is skipped and never re-sent, which the CHANGELOG announces at the top; **BUG-43 filed and fixed 2026-09-18** — the Home Assistant availability topic that every discovery document advertises had no Last Will, so a device that dropped off stayed green in Home Assistant indefinitely; filed from a read-only observation of a production broker, which held `status: offline` and `availability: online` retained side by side; arbitrated to option (c) — one topic, symmetric, whichever the application names moves the other — with (b) refused on BUG-38's own character budget and the entry's reason for preferring it corrected; RED first, two cases, removal check re-run against the final fix; **BUG-42 filed and fixed 2026-09-18**, HIGH, in BUG-41's lot — the panic was placed in the component, not in the test, and the row moved up a grade as its filing said it would: `Core::begin()` reserved `char text[1024]` in its prologue whether or not a flight record was promoted, and still held it while `initializeAll()` ran, leaving 32 bytes of the ESP8266's 4 096-byte cont stack; the block moved to a `noinline` function, `begin()` 1 216 → 208 B, the suite's free cont stack 32 → 896 B, and it prints 7/7 with a verdict line for the first time; filed and shut inside its lot, so no column moves; **BUG-41 filed and fixed 2026-09-18** — the EventBus queue counted entries, so every boot dropped eight events, and its cost model was wrong in six places before a board settled it; **BUG-40 filed and fixed 2026-09-15** — filed by TEST-7's reading: `ComponentConfig`'s float validator refused `"1.5"` and accepted `"1.50"` because it round-tripped through Arduino's two-decimal `String(float)`, octets and ports parsed `"4x"` as 4, a redefined parameter was validated twice; digits-only rule now stated in the reference, seven cases, three removal checks; **BUG-39 filed and fixed 2026-09-14** — filed by the adversarial review of BUG-38's decision memo: a queued message over PubSubClient's 768-byte ESP8266 buffer was retried forever and everything behind it waited; now dropped, named and counted; **BUG-38 filed and fixed 2026-09-14** — every discovery key abbreviated the way Home Assistant documents, the panel 774 → 638, the refusal counted; by OBS Lot D's review — the alarm control panel's discovery document is 774 characters against a 699-character event field and was published cut, so Home Assistant never created the panel; now refused aloud, the fix is a decision between abbreviating the documents and widening the field; **BUG-36 fixed 2026-09-06 in OBS Lot B** — released before the pop, per-bus drop counter in the flight record, "Expected 7 Was 0" on unfixed code; **BUG-37 filed and fixed 2026-09-05**, MEDIUM — an HTTP upload died with a broken pipe whenever the link was quiet for 3 s: ESPAsyncWebServer's receive-idle limit meeting TCP retransmission backoff; the upload handler now sets `uploadIdleTimeoutSec` (30 s), red-then-green with a drained-silence probe on both boards, 3 of 3 natural uploads and one full commit on the WROOM-32D — **new public field and a 3 s → 30 s default the next release must announce at the top**; **BUG-36 filed 2026-09-05**, MEDIUM — the `pendingByTopic` drift on queue overflow that STOR-ESP-1's withdrawal had left in deferred-work without an identifier, fixed with OBS-3's lot the next day; **BUG-35 filed 2026-09-01 by the second real-conditions campaign and fixed the same day** — a client disconnect mid-upload locked OTA out until a power-cycle; onDisconnect→abortUpload gated on the upload-active discriminator, red-then-green with the same script on both boards; **BUG-34 filed and fixed 2026-08-31**, MEDIUM, in SIZE-1's lot — the `/api/ui/schema` truncation drift its dedup exposed, opening and shutting in-lot so no column moves; BUG-29 filed and fixed same day, **BUG-21 done 2026-08-27 after this row claimed it for months**, **BUG-30 filed and fixed 2026-08-28** — this cell said "new and open" for a day after it was closed, corrected 2026-08-29 — **BUG-31 filed and fixed 2026-08-29**, HIGH, **BUG-32 filed and fixed 2026-08-31**, MEDIUM, and **BUG-33 filed and fixed 2026-08-31**, LOW, host-only, each opening and shutting inside its lot so no column moves; **BUG-26 and BUG-28 closed by SIZE-2's lot 2026-08-31** — BUG-26 had been fixed by marianorenzi's `dc8886f1` since July and was stale at filing, BUG-28 closed with his fork's own streaming design — **BUG-2 never closed and never counted** — see below) |
| 4. Test Coverage | TEST-1 to TEST-12 | II (NON-NEGOTIABLE) | 0C, **0H**, 2M, 1L (**TEST-12 filed and fixed 2026-09-24** — RemoteConsole had no device environment at all, so the two platform halves of the core-log capture were compiled by the required checks and executed by nothing; one suite now runs on both boards, provoking its lines the way the core and the SDK do, 7/7 on each, with five of seven failing when the platform's sink is not taken and the ESP32 capture still reporting itself installed while nothing arrives — the two sinks disjoint, measured by a test at last. Its first flash found BUG-67; **TEST-11 filed 2026-09-20 by BUG-51's review** — nothing in the repository executes a line of `app.js`, which is where BUG-54 hid since the feature was written and where the refusal channel now lives; **TEST-10 filed 2026-09-20 by the deferred-work purge** — no test confronts the EventBus byte model with a board's heap; **TEST-9 done 2026-09-19** — four providers no native test could compile now have three suites and 41 cases, closed by TEST-8's mocks rather than by the per-file include removal this entry proposed, so no production header changed; the compile problem was the smaller half, and reading what these settings pages accept filed BUG-45; ten removal checks, 1 107 → 1 151 `[PASSED]`; **TEST-8 done 2026-09-19** — the OTA upload handler's gates had never run under test, because `OTAWebUI.h` did not compile natively at all; mocks under the real library names in `tests/mocks/libraries/` let the real `WebUIComponent` register its eighteen routes on the host, ten cases and nine removal checks, two of them written vacuous and caught by those checks rather than by review; it also corrected TEST-9's premise and deleted a dead `MockAsyncWebServer.h` three documents advertised; **TEST-7 done 2026-09-15** — 42 cases over `MemoryManager` and `ComponentConfig`, plus 5 pinning the native `String` stub, which had to be made Arduino-like first (`toInt()` threw on garbage, `String(float)` printed six decimals); the reading filed BUG-40 and DC-17, and compiled one quarter of TEST-9's hypothesis; **TEST-1, TEST-2, TEST-3 done; TEST-6 done 2026-08-31** — its row was wrong in both directions, LEDWebUI already had a 23-test suite and the other three are now covered or inert; **TEST-4 done 2026-08-31** — the blocker was the stubs, not the tests: scriptable millis/heap/restart and a stateful WiFi stub opened the fallback ladder, AP mode and reconnection to a 16-case native suite, five mutations all caught, and the device scan suite ran 3/3 against a real radio at last; **TEST-8 open, three holes closed and the fourth nearly** — a real multipart POST now runs against a board, refused and accepted, each with a discriminating removal check; what remains is what a browser renders; **TEST-9 open, re-read 2026-09-15** — MQTTWebUI compiles without `WebUI.h`, the other three need a host double of the WebUI component) |
| 5. SSE Bug | SSE-1 | — | **DONE** |
| 6. File Size | SIZE-1 to SIZE-6 | VII (800 lines) | 0C, **0H**, 2M, 1L (**the triage pass of 2026-09-21** read every open MEDIUM heading against the code; **SIZE-3 was never a violation** — `Wifi.h` is 962 lines raw and **621 excluding blanks and comments**, which is what Principle VII counts; **SIZE-2 done 2026-08-31** — 933 → 756 + a 216-line `JsonStreamWriter.h`, shaped so the fork's serializer hunks still land; closing it closed BUG-26 and BUG-28. **SIZE-1 done 2026-08-31, same day** — 1008 → 769 + two new headers, the chunk loop deduplicated into `ProviderRegistry.h`; closing it filed and closed BUG-34. **File Size joins the zero-HIGH sections**) |
| 7. Architecture | ARCH-1 to ARCH-3 | I, XIII | 0C, **0H**, 1M (**ARCH-3 done**; **ARCH-2 done 2026-08-31 by measurement** — both halves of its prescribed remedy already existed, one false at filing, one delivered by PR #17; no code changed. **ARCH-1 re-argued HIGH → MEDIUM 2026-08-31, restated 2026-09-06 and open** — SYS-F6 unreadable so the HIGH was never argued, one XIII indicator exceeded against a file otherwise inside every measurement; the fork trigger fired with marianorenzi's reply of 2026-09-01 and the three extractions are declined on merit; the boot-diagnostics residue (~95 lines) rides OBS Lot B, which rewrites that path, and `begin()` is re-measured at Lot B's closure) |
| 8. CI/Infrastructure | CI-1 to CI-20 | II, XII | 0C, 0H, 2M, 5L (**CI-20 filed 2026-09-24 by TEST-12's lot**, LOW — four C++17 inline variables the shipping ESP32 build accepts only as a GNU extension, the warnings left standing after the six structured bindings in RemoteConsole and Storage were rewritten; **CI-11 and CI-19 both done 2026-09-24** — HomeAssistant declared test settings on both device environments and carried a suite for neither, so one ran a single heap measurement and the other could not compile at all; a device suite measuring the discovery documents on the board now runs 9/9 on the nodemcuv2 and 5/5 on the WROOM-32D, and the job builds both. The job still lists its projects rather than discovering them, and the entry says why: six components would match the rule against eleven builds the job runs; **CI-19 filed 2026-09-23 by BUG-46's lot**, LOW — HomeAssistant's `esp32dev` test environment declares `test_ha_component`, which calls a native-stub-only helper and cannot compile there; `Build on-device suites` does not list it, so nothing said so; **the triage pass of 2026-09-21** read every open MEDIUM heading against the code; **CI-4 is done** — `DomoticsCore-WiFi` survives in three lines of `CHANGELOG.md` and this file, describing what was written at the time; **CI-16, CI-17 and CI-18 filed 2026-09-20 by the deferred-work purge** — the build's leftovers, the generated header the host build can read, no stack-frame check; **CI-1, CI-2, CI-3, CI-5, CI-8, CI-9, CI-10, CI-12 done**; CI-11 open, **CI-13 done 2026-09-01** — paid a second time at 19 GB before the fix its entry prescribed was finally applied; **CI-14** — FullStack is green in CI and unusable on an ESP8266; **CI-15 done 2026-09-15** — filed as a packaging item, re-pointed 2026-09-05 when `export.exclude` measured inert, closed by `symlink://` in all 49 manifests, the 31 nested projects' libdeps moved out of the component that contains them, `clean_examples.py` retired) |
| 9. Dead Code | DC-1 to DC-19, PERSIST-1 | IV (YAGNI) | 0C, 0H, 11M, 2L (**DC-18 and DC-19 filed 2026-09-20 by the deferred-work purge** — OTA's no-op buffering path and its three inlined credential checks; **DC-17 filed 2026-09-15** — `ComponentConfig` and eight `MemoryManager` queries: no caller in the tree, documented public API, DC-13's decision on a major boundary; **DC-16 filed and fixed 2026-09-14**, LOW — `/api/ntp/timezones` was registered twice, the System's copy removed and the provider's `init()` finally called; **DC-3b, DC-4, DC-5, DC-6, DC-7, DC-8, DC-11 done**; PERSIST-1 new, DC-12 new, DC-13 new, **DC-14 new** — every provider declares a REST endpoint nothing registers, and the schema ships it to every client; **DC-15 new** — WifiConfig's two "advanced settings" are accepted and ignored) |
| 10. Minor | LO-1 to LO-36, DOC-1, DOC-2 | Various | 0C, 0H, 0M, 36L (**LO-33 to LO-36 and DOC-2 filed 2026-09-20 by the deferred-work purge**; **LO-5 done 2026-09-18 in BUG-41's lot** — the silent drop has been logged since OBS Lot B and this row had not caught up; the line now carries the budget and the peak occupancy; **LO-11 done**; **DOC-1 new**) |
| 11. Observability | OBS-1 to OBS-8 | XIV (its instrument) | 0C, 0H, 0M, 1L — **the seven of 2026-09-05 all closed; OBS-8 filed 2026-09-20 by the deferred-work purge**, the queue's high-water mark reaching no telemetry (**all seven filed 2026-09-05** from a design discussion, adversarially reviewed and board-measured the same day; **OBS-5 and OBS-1's transport closed by Lot D on 2026-09-14** — telemetry and the retained crash record on MQTT, discovered by Home Assistant through one topic scheme, the core dump downloaded, decoded against its ELF and erased through the WebUI, a Home Assistant container reading the entities; **OBS-4 closed by Lot C on 2026-09-06** — the failed-allocation group in the record, the ESP32 heap hook, the ESP8266 latch-and-clear, the diagnostic profile measured; **OBS-3 closed by Lot B on 2026-09-06** — the recorder in Core, promotion first, the record held until persisted, both boards' death sequences read back, three removal checks; **OBS-2, OBS-6, OBS-7 closed by Lot A the same day**, with OBS-1's boot check; OBS-7 — a stuck ESP32 `loop()` never reboots — was filed by the review, confirmed on the WROOM-32D, and fixed with a 30 s default the next release must announce) |
| **Total** | **195 items** | | **0C, 0H, 27M, 56L** (125 resolved) |

The severity columns sum across the rows. **It went to one and back to zero on
2026-09-23**, hours after v2.6.1 was tagged, and the source is new: BUG-63
was filed from a consumer's *production* alarm panel, not from this
repository's bench, and fixed the same day with BUG-13 and BUG-62 in one lot. Twice before, the column was refilled by a board run of
our own; this time the library had shipped, been installed, and had its
default configuration meet Home Assistant's opposite default. The release
that announced zero HIGH was honest when it was tagged. **The rows also said
`0H` for half a day while the entry sat in the file** — BUG-63's filing
updated neither the row nor the total, which is the failure this section
warns about two paragraphs down, committed by the very commit that filed it.

**The HIGH column moved four times on 2026-09-22 and ended at zero**: SEC-16 was closed by a fix, the
board run that proved it panicked on an unrelated path and filed BUG-60,
and BUG-60 was closed the same day once a probe made its race reproducible
on demand. That is the 2026-09-01 sequence repeating — the board refills
the column the fix emptied — and the argument for running one rather than
trusting the native suite. BUG-35 was filed by the 2026-09-01
campaign nineteen hours after the column first read zero (that zero was by
reclassification, said so at the time) and closed the same day with a
board-measured red-then-green on both platforms. The sequence is the
system working: the campaign refilled the column, the fix emptied it. The
rows were checked against the section headings rather than only re-summed
— the sweep below, re-run for the BUG-35 lot, reports **35 `[HIGH]`
headings, 35 with evidence, 0 open**; BUG-42 makes that 36 and 36, filed
and shut inside one lot. The MEDIUM column sums to 28:
2 + 4 + 2 + 3 + 2 + 1 + 3 + 11 + 0 + 0 — the enumeration the triage pass of
2026-09-21 left on its pre-triage figures, re-derived here — and the LOW column to 56:
0 + 2 + 8 + 1 + 1 + 0 + 5 + 2 + 36 + 1 — re-derived for BUG-63's lot, which
found the enumeration two behind the rows it claims to add up; the deferred-work purge on
2026-09-20 read that file's 62 entries one by one and filed nineteen items, two
MEDIUM (SEC-15, BUG-47) and seventeen LOW (34 → 36 M, 33 → 50 L, total
152 → 171, resolved unchanged at 98); SEC-16's lot on 2026-09-22 closed the HIGH the triage pass had opened and filed two more items its own board run opened, then closed both in the same lot (SEC-16, BUG-60 HIGH and SEC-17 MEDIUM: the HIGH column 1 → 0 → 1 → 0, the MEDIUM 28 → 29 → 28, total 181 → 183, resolved 112 → 115); the triage pass on 2026-09-21 — the first act under the new release gate — read all 32 open MEDIUM headings against the code and closed seven that were already fixed or were never violations (BUG-3, BUG-16, BUG-17, BUG-18, SEC-5, SIZE-3, CI-4), folded one duplicate out of the roster (BUG-24 into DC-15) and narrowed one (TEST-5): 36 → 28 M, total 181 → 180, resolved 105 → 112. **Six of those headings were in no handoff's actionable list**, which is how four fixed items sat in the column for weeks; BUG-47's lot on 2026-09-21 closed one MEDIUM and filed one MEDIUM its own board runs opened (BUG-47 and BUG-59: the column holds at 36, total 180 → 181, resolved 104 → 105); BUG-51's lot on 2026-09-20 closed two MEDIUM and one LOW, filed two LOW it fixed in the same lot, and had four more opened by its own adversarial review (BUG-51, BUG-52 and BUG-53 closed, BUG-54 and BUG-55 filed and shut, BUG-56 to BUG-58 and TEST-11 open: 37 → 36 M, 51 → 53 L, total 174 → 180, resolved 99 → 104); BUG-45's lot on 2026-09-20 closed one MEDIUM and filed three items, two of them by its own adversarial review (BUG-45 closed; BUG-51, BUG-52 and BUG-53 open: 36 → 37 M, 50 → 51 L, total 171 → 174, resolved 98 → 99); BUG-44's lot on 2026-09-20 closed one and filed one MEDIUM that stays open, opened by its own fix (BUG-44 and BUG-46: the column holds at 34, total 151 → 152, resolved 97 → 98); TEST-9's lot on 2026-09-19 closed one and filed one MEDIUM that stays open (TEST-9 and BUG-45: the column holds at 34, total 150 → 151, resolved 96 → 97); TEST-8's lot the same day closed one (35 → 34, resolved 95 → 96); a bench capture the same day filed one MEDIUM that stayed open until the next day (BUG-44, 34 → 35, total 149 → 150, resolved unchanged at 95; closed 2026-09-20 by the lot above); a read-only observation of a production broker on 2026-09-18 filed one MEDIUM and it was fixed the same day (BUG-43, no column move, total 148 → 149, resolved 94 → 95); BUG-41's lot on 2026-09-18 filed one MEDIUM
and fixed it in place (BUG-41, no column move), closed one LOW (LO-5: 32 → 31
in the Minor row, 34 → 33 overall) and filed one item that was graded MEDIUM,
re-graded HIGH on measurement and fixed in the same lot (BUG-42, no column
move), for a total of 146 → 148 and resolved 91 → 94; the CI-15 lot on 2026-09-15 closed
one (35 → 34, resolved 90 → 91); TEST-7's lot on 2026-09-15 closed one
and filed one MEDIUM (DC-17) and one filed-and-fixed (BUG-40): 35 → 35,
total 144 → 146, resolved 88 → 90; the SEC-4/SEC-6/SEC-14 lot closed three on
2026-09-14 (38 → 35, resolved 85 → 88); BUG-39 was filed on 2026-09-14 by
the adversarial review of BUG-38's decision memo (39 → 40, total 143 → 144),
and both were fixed the same day on Lot D's branch before its PR (40 → 38,
resolved 83 → 85); Lot D closed SEC-13, OBS-1 and
OBS-5 on 2026-09-14 (41 → 38, resolved 79 → 82; Observability's column
reads zero) and its code review filed BUG-38 (38 → 39, total 141 → 142)
and DC-16 (LOW, 34 → 35, total 143), fixed on the same branch before the PR
(35 → 34, resolved 82 → 83); Lot C closed OBS-4 on 2026-09-06 (42 → 41,
resolved 78 → 79); Lot B closed OBS-3 and BUG-36 the same day (44 → 42,
resolved 76 → 78). BUG-37 was Code Safety's eighth
for one day: filed by OBS-7's residual-6 measurement (total 140 → 141,
45M) and fixed the same evening (44M, resolved 75 → 76).
The seven OBS items and BUG-36 were filed 2026-09-05 and moved the total
132 → 140; Lot A closed OBS-2 and OBS-7 (MEDIUM) and OBS-6 (LOW) the same
day, which is how the resolved column went 72 → 75.

One lot earlier (TEST-4's): the total row read **128 items, 0C, 4H, 40M, 34L,
63 resolved**, the four open HIGH being SIZE-1, SIZE-2, ARCH-1, ARCH-2 — kept
here as the before-state the SIZE-2 arithmetic moves from, since both lots
landed the same day.

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
never enters this `[HIGH]` sweep at all. Re-run once more after BUG-31 (TEST-6's
lot A) landed on top: **35 headings, 29 with evidence, 6 open**. BUG-31 is the
thirty-fifth `[HIGH]` heading and the twenty-ninth with evidence — it opens and
shuts inside its lot — and the open six are still the six named above. The two
lots each independently computed a 34/28/6 sweep against the pre-SEC-10 base; the
combined figure is 35/29/6, not 34/28/6, because both add a distinct closed HIGH
heading (SEC-11 and BUG-31) — a collision the rebase had to resolve rather than
copy either lot's number. Re-run after BUG-32 (TEST-6's lot B): **35 headings, 30
with evidence, 5 open**. No heading is added or removed — BUG-32 is `[MEDIUM]` —
but TEST-6 gains a DONE marker, so it crosses from the open column to the evidenced
one: open 6 → 5 (TEST-4, SIZE-1, SIZE-2, ARCH-1, ARCH-2), with-evidence 29 → 30.
Re-run after TEST-4's closing lot (2026-08-31): **35 headings, 31 with evidence,
4 open**. Again no heading is added or removed — DC-15, the lot's one new ID, is
`[MEDIUM]` — and TEST-4 crosses from open to evidenced: open 5 → 4 (SIZE-1,
SIZE-2, ARCH-1, ARCH-2), with-evidence 30 → 31. Re-run after SIZE-2's lot
(2026-08-31, same day): **35 headings, 32 with evidence, 3 open** — SIZE-2
crosses; BUG-33 (LOW) and the BUG-26/BUG-28 closures (MEDIUM) never enter this
sweep: open 4 → 3 (SIZE-1, ARCH-1, ARCH-2), with-evidence 31 → 32. Re-run
after SIZE-1's lot (2026-08-31, same day again): **35 headings, 33 with
evidence, 2 open** — SIZE-1 crosses; BUG-34 (MEDIUM) never enters this sweep:
open 3 → 2 (ARCH-1, ARCH-2), with-evidence 32 → 33. Re-run after ARCH-2's
lot (2026-08-31 still): **35 headings, 34 with evidence, 1 open** — ARCH-2
crosses on its new DONE marker, no heading added (the lot files no ID):
open 2 → 1 (ARCH-1), with-evidence 33 → 34. Of the 17 headings without a
marker, 15 clear on the resolved-table or merged-lot criteria as before;
the survivor is ARCH-1. Re-run after ARCH-1's re-argument (2026-08-31,
the day's fifth and last lot): **34 headings, 34 with evidence, 0 open** —
ARCH-1's heading leaves the `[HIGH]` count by becoming `[MEDIUM]`, the
SEC-12 mechanism, so the headings figure DROPS rather than a survivor
crossing; nothing is resolved by it and the item is still open, which is
why it appears in no evidence column either. Re-run after the 2026-09-01
campaign's tooling lot: **35 headings, 34 with evidence, 1 open** — BUG-35
is the thirty-fifth `[HIGH]` heading and it is open; the campaign refilled
the column the previous day emptied, which is its job. Re-run after the
BUG-35 fix lot, later the same day: **35 headings, 35 with evidence, 0
open** — BUG-35 crosses on its DONE marker, filed and fixed inside one
day, red and green measured by the same script.

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

The lot before this one (2026-08-29, MEM-2's closing lot): MEM-2 closes
(7 → 6 HIGH, and Memory Safety's row 1H → 0H), **MEM-5, MEM-6 and DC-13 are filed
and left open** (31 → 34 MEDIUM: two into Memory Safety, one into Dead Code).
Items 115 → 118 for the three new IDs, resolved 56 → 57. No item opened and shut
inside that lot, so the six remaining HIGH were the seven of the lot before it
minus MEM-2 and nothing else.

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

**The lot before this one (2026-08-29, TEST-6's lot A), stacked on the SEC-10 lot:**
**BUG-31 is filed and closed in the same lot**, HIGH — so no severity column moves,
exactly as SEC-8 did, and the six remaining HIGH were the same six. **TEST-9 and
DC-14 are filed and left open** (37 → 39 MEDIUM: one into Test Coverage, one into
Dead Code). Items 123 → 126 for the three new IDs, resolved 59 → 60 for BUG-31
alone. BUG-31 was authored before the SEC-10 lot and held; rebased on top of it,
its figures are re-derived here from the 123 items / 37 MEDIUM / 59 resolved the
SEC-10 lot left, not the 118 / 34 / 57 it was first written against.

**The lot before this one (2026-08-31, TEST-6's lot B — BUG-32):** **BUG-32 is
filed and closed in the same lot**, MEDIUM — so no severity column moves for it —
and **TEST-6 itself closes**, taking the open HIGH it held to resolved. Items
126 → 127 for the one new ID; resolved 60 → 62 (BUG-32 and TEST-6); the open HIGH
count falls **6 → 5**, the first drop in the run. The `hasDataChanged` cost claim
is recorded under TEST-6 without an ID — a board-owed observation, not a counted
item.

**The lot before this one (2026-08-31, TEST-4's closing lot):** **TEST-4
closes** — the open HIGH it held moves to resolved — and **DC-15 is filed and
left open**, MEDIUM (39 → 40 MEDIUM, into Dead Code). Items 127 → 128 for the
one new ID; resolved 62 → 63 (TEST-4 alone); the open HIGH count falls
**5 → 4**, the second drop in the run, and **Test Coverage joins Security,
Memory Safety and Code Safety at zero HIGH**. No code under test changed in
that lot: the diff is two test seams whose defaults are byte-identical to the
old stubs, one new native suite, and two tearDown lines.

**The lot before this one (2026-08-31, SIZE-2's lot):** **SIZE-2 closes** (HIGH 4 → 3),
and it takes two Code Safety MEDIUMs with it — **BUG-28** (closed with the
fork's own streaming multiselect design) and **BUG-26** (found already fixed
by marianorenzi's `dc8886f1` since July; the row was stale at filing) —
40 → 38 MEDIUM. **BUG-33 is filed and fixed in-lot**, LOW, host-only
(char-signedness in the escaper), so it adds an ID and a resolved item and
moves no severity column. Items 128 → 129; resolved 63 → 67 (SIZE-2, BUG-26,
BUG-28, BUG-33). Three HIGH remain: SIZE-1, ARCH-1, ARCH-2 — one file split
and the two architecture items that do not measure. One production behaviour
changed knowingly: multiselect values are now streamed by our escaper instead
of built through ArduinoJson — byte-identical for the content any provider
ships (pinned by parse-content tests; the 0x08/0x0C short-forms and the
embedded-NUL asymmetry are recorded as unpinned residue in the SIZE-2 entry).

**The lot before this one (2026-08-31, SIZE-1's lot — the third of the day):** **SIZE-1
closes** (HIGH 3 → 2), leaving the HIGH column entirely to Architecture, and
**File Size joins the zero-HIGH sections**. **BUG-34 is filed and fixed
in-lot**, MEDIUM — the `/api/ui/schema` truncation drift the dedup exposed,
wider than its spec assumed once the tests found the check-state shape — so
it adds an ID and a resolved item and moves no severity column. Items
129 → 130; resolved 67 → 69 (SIZE-1, BUG-34). Two HIGH remain: ARCH-1 and
ARCH-2, the two items that do not measure — the agreed order takes ARCH-2
next, and the decision to execute or re-argue severity (as BUG-2 was) is
made at spec time. One behaviour changed knowingly, and it is BUG-34's fix,
isolated in its own commit: `/api/ui/schema` now retries where it silently
truncated. The refactor commit changes none — both stall shapes, the skip
logic and the crowding are pinned by ten new native tests that could not
exist before the extraction.

**This change (2026-09-01, BUG-35's fix lot — filed in the morning, fixed
in the afternoon):** **BUG-35 closes** (open HIGH 1 → 0, by a fix this
time). One handler change in `OTAWebUI.h` (onDisconnect → abortUpload,
gated on the upload-active discriminator), the harness's `--disconnect-at`
mode that turns yesterday's accident into a repeatable check, red measured
on the unfixed firmware from a fresh boot, green on both boards including
the ESP8266's buffer-release path, the happy-path `--commit` cycle
re-proven, and the two known limits (shared-state two-client blind spot,
TCP half-open) written at the site and in the entry rather than
discovered later. Resolved 71 → 72; nothing filed.

**The lot before this one (2026-09-01, the second real-conditions campaign's tooling
lot):** the campaign paragraph above carries the runs; the lot itself
ships the CI-13 fix (`clean_examples.py` learns `test/*/.pio`), the
harness repairs (`ota_upload_check.py` fetches the SEC-10 token, with
`--no-token` as SEC-10's own check), and three roadmap movements:
**CI-13 closes** (fixed and measured — 19 GB → 30 MB on the same build),
**BUG-35 is filed and left open**, HIGH — the first HIGH filed since the
column read zero, nineteen hours earlier — and **CI-15 is filed and left
open**, MEDIUM, the packaging root cause deferred to a release-aware lot.
No component code changes; the BUG-35 fix gets its own lot with a board
test.

**The lot before this one (2026-08-31, ARCH-1's lot — the fifth of the day):**
**ARCH-1 is re-argued HIGH → MEDIUM and stays open** — nothing is resolved,
nothing is filed, no code changes. SYS-F6 cannot be read, so the HIGH was
inherited rather than argued (BUG-2's discovery, explicitly not BUG-2's
authority — his remedies were measured impossible, these are feasible, and
the demotion rests on harm-assessment and timing). One XIII indicator is
exceeded and was at filing too (`begin()` 62 lines); BUG-23 and PERSIST-1
are cited as the checked defect list; the fork rewrites two of the three
prescribed extraction zones and the third is declined as YAGNI on its own
merits; a dual trigger (fork landing, or next release-series planning)
prevents the deferral from stalling. **The open-HIGH count reaches zero by
reclassification, and the summary says so in those words.** The
marianorenzi notification the maintainer owes at this milestone is drafted
— not sent — **as a local, untracked file** (it was briefly committed here
by mistake and removed on 2026-09-01: a draft of outbound communication is
the maintainer's to send, and a public repository is not the place for it
before that decision — `draft-*.md` is now gitignored under
implementation-artifacts). It is constrained to state ARCH-1 as
re-argued-and-open, never as resolved.

**The lot before this one (2026-08-31, ARCH-2's lot — the fourth of the day, and the
first to close a HIGH without changing code):** **ARCH-2 closes by
measurement** (HIGH 2 → 1 — **ARCH-1 is the last HIGH in the roadmap**).
Its prescribed remedy was found already delivered: the WebUI half was false
at filing (LEDWebUI.h separate since at least 2025-09-26), the effect-engine
half landed with PR #17 on 2026-08-23, and the entry had simply never moved
— the BUG-26 staleness shape, on a HIGH this time. The one recorded
structural defect (BUG-19) is cited in-entry rather than hidden, with the
argument for why the split would have relocated rather than removed it. No
new ID is filed; the LED suites ran 103/103 green from clean as the
closure's measurement. The decision standard is BUG-2's — severity and
remedy checked against the code they blame — with the opposite outcome:
nothing needed re-arguing, because the remedy existed.

**Why TEST-6 waited for lot B.** BUG-31 was the first of its two lots and did not
close TEST-6: doing so then would have claimed coverage of three providers it never
touched. Lot B fixes `SystemInfoWebUI`'s unescaped device name at the sink, covers
`StorageWebUI`, and only then corrects the row — which was wrong in both directions,
`LEDWebUI` having had a dedicated 23-test suite the whole time.

Both new findings came out of the work rather than out of a review, which is the
pattern every lot has repeated: SEC-2's re-fix raised SEC-7, SEC-7 raised SEC-8,
and BUG-21's tests raised BUG-30 by being the first code in the repository to
subscribe to a topic OTA publishes on.

**The second real-conditions campaign (2026-09-01) ran both boards against
the real network and found the roadmap's next HIGH.** On the nodemcuv2:
Storage 7/7, OTA 10/10, FullStack 8/8, WebUI heap 6/6, schema-memory 3/3 —
every ESP8266 on-device suite green against post-campaign `main`. On the
WROOM-32D: FullStack booted, joined the real WiFi and broker; a real
browser loaded the dashboard with zero console errors, zero failed
requests, and the SEC-10 flow visible on the wire (`/api/ui/token` then
`?schema=1`); `/api/ui/schema` — never before fetched over HTTP by
anything in this repository — returned 19 parseable contexts; a tokenless
OTA upload got 403, a wrong-hash upload was refused with `total` naming
the firmware and not the envelope (SEC-9 on ESP32), and a clean commit
installed, rebooted and came back idle, twice. What it found: **BUG-35**
(the disconnect lock, HIGH, above — fixed later the same day, its own
lot), **CI-13 paid a second time at 19 GB** (fixed in this lot), **CI-15 filed** (the packaging root cause), one
unreplicated `"Could Not Activate"` recorded inside BUG-35's entry rather
than filed, and BUG-32's owed measurement partially settled on silicon:
the WebUIOnly update runs 558 of its 1024 bytes with headroom 466, so the
worst-case escaped name (186 bytes) cannot crowd *that* deployment — the
drop scenario needs a context-dense ESP8266 network deployment, which
CI-14 currently makes unreachable with shipped examples. Not covered:
the ESP32-CAM (absent from the bus, its OTA suite's last silicon run
stays 2026-08-27) and the harness's toggle click (found the settings
checkbox `disabled` — a harness limit recorded, not a device defect; the
POST-with-token path was proven by hand instead). The harness itself
needed repair before it could test anything: `ota_upload_check.py` had
been silently un-runnable since SEC-10 shipped the token it never heard
of — patched in this lot, with `--no-token` turning the 403 into SEC-10's
own check. Suite durations measured 10–30× shorter than the previous
day's runs of the same suites on the same board: yesterday's figures were
inflated by host contention from parallel native builds, a reminder that
Unity durations are wall-clock at the host. And one warning was validated
the expensive way: building two examples in parallel let one example's
cleaner delete the firmware the other was uploading — the header's
"sequential builds are required" is load-bearing.

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
56 resolved + 72 remaining is 128, against a stated 115, while counting the ID
ranges in the Items column gives 119 (118 with STOR-ESP-1 withdrawn). Three
figures, three answers. (This line read "55 … is 127" — a gap of 12 — until the
BUG-32 lot's audit caught it against the constant 13 the paragraphs below assert
and the "56 of 115" CLAUDE.md recorded; corrected here, a pre-existing slip, not
this lot's.) Left as found rather than re-baselined to whichever one
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

**TEST-6's lot A (BUG-31) moves all three by exactly three**, on top of the SEC-10
lot: three new IDs (BUG-31, TEST-9, DC-14), so the stated total goes 123 → 126;
resolved plus remaining goes 59 + 77 = 136 to 60 + 79 = 139, because BUG-31 opens
and shuts and lands in resolved without entering remaining, while TEST-9 and DC-14
enter remaining; and the ID ranges in the Items column gain the same three. The
13-item gap is still 13. Ordering note: BUG-31 was authored before the SEC-10 lot
and held; rebased on top of it, its figures are re-derived here from the 123/136
the SEC-10 lot left, not the 118/131 it was first written against.

**TEST-6's lot B (BUG-32) closes the item and adds one.** One new ID, BUG-32,
filed and fixed in-lot; and **TEST-6 itself closes**, the open HIGH it held moving
to resolved. The stated total goes 126 → 127 (BUG-32); resolved plus remaining
goes 60 + 79 = 139 to 62 + 78 = 140 — BUG-32 opens and shuts (resolved +1,
remaining +0), while TEST-6 crosses from remaining to resolved (resolved +1,
remaining −1); the ID ranges gain BUG-32. The 13-item gap is still 13. This is the
first lot in the run to **lower** the open HIGH count, 6 → 5: the six lots before
it either held it flat or, once, raised it. Remaining 78 = 5 HIGH + 39 MEDIUM + 34
LOW. The `hasDataChanged` cost claim is recorded under TEST-6 without an ID, so it
moves none of the three figures — a board-owed observation, not a counted item.

**TEST-4's closing lot moves all three by exactly one.** One new ID, DC-15, filed
and left open; and **TEST-4 closes**. The stated total goes 127 → 128; resolved
plus remaining goes 62 + 78 = 140 to 63 + 78 = 141 — TEST-4 crosses from
remaining to resolved (resolved +1, remaining −1) while DC-15 enters remaining
(remaining +1), so remaining nets to zero movement; the ID ranges gain DC-15. The
13-item gap is still 13. Remaining 78 = 4 HIGH + 40 MEDIUM + 34 LOW — the HIGH
that left was replaced in the count by the MEDIUM that entered, which is why
remaining is flat while both severity columns moved.

**SIZE-2's lot moves the total by one and resolved by four.** One new ID,
BUG-33, filed and fixed in-lot (resolved +1, remaining +0); SIZE-2, BUG-26 and
BUG-28 cross from remaining to resolved (resolved +3, remaining −3). The
stated total goes 128 → 129; resolved plus remaining goes 63 + 78 = 141 to
67 + 75 = 142; the ID ranges gain BUG-33. The 13-item gap is still 13
(142 − 129). Remaining 75 = 3 HIGH + 38 MEDIUM + 34 LOW. This is the first
lot in the run to close items it never set out to close: BUG-26 and BUG-28
were Code Safety's, and both fell out of reading the file SIZE-2 was
splitting against the fork that had already rewritten it.

**SIZE-1's lot moves the total by one and resolved by two.** One new ID,
BUG-34, filed and fixed in-lot (resolved +1, remaining +0); SIZE-1 crosses
from remaining to resolved (resolved +1, remaining −1). The stated total goes
129 → 130; resolved plus remaining goes 67 + 75 = 142 to 69 + 74 = 143; the
ID ranges gain BUG-34. The 13-item gap is still 13 (143 − 130). Remaining
74 = 2 HIGH + 38 MEDIUM + 34 LOW. Like SIZE-2's, this lot closed a Code
Safety item it never set out to find: BUG-34 fell out of diffing the two
route lambdas before deduplicating them, the drift visible only once the two
copies sat side by side.

**ARCH-2's lot moves only the crossing.** No new ID — the first lot in the
run to file nothing — so the stated total stays 130; ARCH-2 crosses from
remaining to resolved (resolved +1, remaining −1): 69 + 74 = 143 to
70 + 73 = 143. The 13-item gap is still 13 (143 − 130). Remaining 73 =
1 HIGH + 38 MEDIUM + 34 LOW. All three families move by zero or by the one
crossing, which is what a no-code, no-ID lot should look like in the
arithmetic.

**ARCH-1's re-argument moves nothing at all in this family.** No new ID,
nothing resolved, nothing crossing: 130 stated, 70 + 73 = 143, gap still
13. The only movement is *within* remaining — 73 = 0 HIGH + 39 MEDIUM +
34 LOW, one item having changed severity column without changing state.
A re-argument that moved any total would be mislabeled as one.

**The campaign's tooling lot moves all three by exactly two.** Two new IDs
(BUG-35, CI-15), so the stated total goes 130 → 132; CI-13 crosses from
remaining to resolved: resolved plus remaining goes 70 + 73 = 143 to
71 + 74 = 145 (BUG-35 and CI-15 enter remaining, CI-13 leaves it). The
13-item gap is still 13 (145 − 132). Remaining 74 = 1 HIGH + 39 MEDIUM +
34 LOW.

**BUG-35's fix lot moves only the crossing.** No new ID; BUG-35 crosses
from remaining to resolved: 71 + 74 = 145 to 72 + 73 = 145. The gap is
still 13. Remaining 73 = 0 HIGH + 39 MEDIUM + 34 LOW.

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
