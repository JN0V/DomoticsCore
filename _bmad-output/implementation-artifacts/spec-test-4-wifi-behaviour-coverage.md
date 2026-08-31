# TEST-4 — WiFi behavioural coverage: STA fallback, AP mode, reconnection

Status: v2 — amended after adversarial review, 2026-08-31. The review refuted
two tests as first specified: F3 pinned the wrong branch (heap 20000 never arms
the fallback timer — every observable it asserted was satisfied by the direct
AP+STA path at L805, so mutation 1 would not have moved it), and A3 was red
against correct code (the A2 flow legitimately calls `startAP` twice before the
skip branch is ever reached). Both are repaired below; fixture rules the review
forced are in **Fixture invariants**.
Roadmap item: TEST-4 [HIGH], ref WIFI-F13. Remaining scope after MEM-2's lot:
STA fallback timer, AP mode, reconnection logic. Scan failure left the list on
2026-08-29.

## Why these paths have never been tested

Every one of them reads state the native stubs cannot script:

| Path | Reads | Stub today |
|---|---|---|
| Reconnection (`loop()` L258-296) | `getMillis()` (15 s timeout, 5 s retry), `isConnected()` | millis = real clock → a native test of the timeout takes 15 s wall time |
| STA fallback (`loop()` L198-250, `updateWifiMode()` L771-837) | `getFreeHeap()` (2000/2500/3500/6500/10000 thresholds), millis (30 s), `getAPStationCount()`, `restart()` | heap frozen at 65536 → **no low-heap branch is reachable at all**; restart invisible |
| AP mode (`begin()` L131-151, `updateWifiMode()` L849-876, `connectToWifi()` L911-931) | `getMode()`, `getAPSSID()`, `startAP()` result | stateless: mode always `Off`, `startAP` always `false`, `getAPSSID` always `""` → the skip-restart branch (L856) is unreachable, AP success flows unobservable |

So the lot is two seams plus one suite. Same shape as MEM-2's scan-table seam:
inline, header-only, `ForTest` suffix, default behaviour byte-identical to the
current stub so the twelve other native projects see nothing.

## Slice 1 — Platform_Stub.h seams (DomoticsCore-Core)

- `setMillisForTest(ms)` / `advanceMillisForTest(ms)` / `resetMillisForTest()`.
  Override flag, default off → `getMillis()` unchanged for every suite that does
  not opt in. Tests must set the override **before constructing the component**:
  `NonBlockingDelay` captures `getMillis()` at construction, and an override
  installed after that makes every timer fire spuriously once (unsigned wrap →
  ready; the timer then re-anchors to the override clock). One spurious ready
  tick is enough to corrupt a scenario, so the rule stands.
  `HAL::getMillis()` (Timer.h) and `HAL::Platform::getMillis()` (Wifi.h) both
  resolve to `Platform::getMillis()` — one seam covers both.
- `setFreeHeapForTest(bytes)` / `resetFreeHeapForTest()`. Default 65536,
  unchanged.
- `restart()` increments `restartCountForTest` (readable + resettable). The
  native body was empty; counting is the only observable a no-op can offer.

Blast radius: Platform_Stub compiles into all 13 native projects, via the
`file://` libdeps copy. Defaults preserve behaviour; the proof is running the
full native set, not arguing it.

## Slice 2 — Wifi_Stub.h statefulness (DomoticsCore-Wifi)

A single `StubWifiState` (function-local static, like `stubbedNetworks()`):

- `setMode` records; `getMode` returns the record. Was: always `Off`.
- `startAP(ssid, pw)` records the ssid, records **whether pw was nullptr as a
  tri-state flag** (`String(nullptr)` on the native String is UB — the stub
  must never construct from the raw pointer without a null check), counts the
  call, returns `stubbedStartAPResult` (**default `false`, the current
  behaviour**); on success marks AP active. `getAPSSID()` returns the recorded
  SSID while active (was `""`). `stopAP()` deactivates and counts.
- `connect()` counts calls, records last ssid/password. `disconnect()`,
  `disconnectAndOff()` count.
- `getAPStationCount()` / `getRawStatus()` scriptable, defaults 0.
- `resetWifiStateForTest()` restores every default. Called in `tearDown()`
  alongside `resetScanForTest()`.

**The stateful mode breaks one existing test unless its tearDown is extended.**
Stub state is process-global; `test_wifi_component.cpp`'s tearDown resets only
the scan table, so a recording `setMode` leaks `AccessPoint` from any AP
fixture into `test_wifi_mode_detection_initial`, whose
`TEST_ASSERT_FALSE(isAPMode())` on a never-begun component then reads the
previous test's mode. `resetWifiStateForTest()` is therefore added to the
tearDown of **both** existing Wifi suites — `test_wifi_component` and
`test_wifi_webui` (the third suite in this project; its assertions read
component flags, not HAL mode, but the reset costs nothing and the leak is
real). Audited by running Wifi + System native suites; CI runs the rest.
`startAP` default stays `false` precisely to keep `updateWifiMode()`'s return
values and event emissions where they are today.

## Slice 3 — `test/test_wifi_behaviour/` (native only)

New Unity project so the scan/format suite stays what MEM-2 left. Fixtures use
`Core` + `addComponent` when events are asserted (payloads read through the
bus), standalone `WifiComponent` elsewhere (`emit` guards on null bus —
IComponent.h:224). `setUp` installs millis override at 0 **then** builds;
`tearDown` resets all seams.

### Fixture invariants (from the adversarial review)

- **F-scenarios construct with STA credentials** — `WifiComponent("Net","pw")`.
  A default-constructed component's `begin()` is `begin("")`: it starts the
  autogenerated AP, bumps the `startAP` count and overwrites the AP identity,
  poisoning every "no `startAP`" assertion downstream.
- **`Core::loop()` is a no-op before `Core::begin()`.** Every event-asserting
  test calls `testCore->begin()`; standalone tests drive `wifi.loop()`
  directly.
- **Counters are asserted as deltas, not totals**, snapshotted after setup —
  `begin()` and pre-begin `enableAP()` calls legitimately touch the HAL before
  the scenario starts.
- **Subscribe before `begin()`** — events emitted during `begin()` are queued,
  not dispatched (`initializeAll` never polls); they arrive on the first
  `loop()`.
- F1 heap is **3000**, stated as in [2000, 3500): below 2000 the ultra-low
  guard runs instead of the branch F1 pins.

### Reconnection

| # | Test | Pins |
|---|---|---|
| R1 | connect success | `begin()` with SSID → 1 `connect()` call; script connected + advance 100 ms → `Success`, `EVENT_STA_CONNECTED` true, `network/ready` payload is the NUL-terminated IP |
| R2 | timeout → TimeoutError, and same-tick retry | never connected, advance 15 001 ms → `TimeoutError`, `EVENT_STA_CONNECTED` false, **and `connect()` count = 2 in the same `loop()`**: the reconnect timer (5 s) is always ready when the 15 s timeout fires. A deliberate behaviour pin — if someone reorders `loop()`, this goes red and the retry latency changed |
| R3 | disconnect() stops retrying | after R2 state, `disconnect()`, advance 20 s, loop → connect count unchanged |
| R4 | heap guard defers connect | heap 2000, `setCredentials(reconnectNow=true)` → 0 connect calls, not connecting; heap back to 65536, advance 5 001 ms, loop → 1 call. Pins the "retry on next tick" comment at Wifi.h:948 |

### STA fallback timer

Arming scenarios enter through `setConfig` (ssid + AP + autoConnect) →
`pendingModeUpdate_` → one `loop()` under scripted heap. **The two fallback-
success tests are two-phase by necessity** (review finding 1): the timer is
only armed by the heap-constrained branches, so both must arm at low heap
first, then re-script the heap before the success loop — heap 20000 on the
arming loop takes the direct AP+STA branch and never arms anything.

| # | Test | Pins |
|---|---|---|
| F1 | boot heap 3000 ∈ [2000, 3500), no AP client → STA-only attempt | `stopAP()` called, mode `Station`, no `startAP` |
| F2 | arm at heap 3000; connected, success loop still at heap 3000 | `EVENT_STA_CONNECTED` true, **no** `startAP` delta, mode stays `Station` (stay-STA-only branch L217, `heapNow > 6500` false — 6500 exactly also stays STA-only) |
| F3 | arm at heap 3000 (phase 1: assert `stopAP` delta, the arming discriminator); re-script heap 20000 + connected, success loop (phase 2) | mode `StationAndAP`, `startAP` delta +1 with the AP SSID, `EVENT_AP_ENABLED` true (restart-AP branch L209-216) |
| F4 | arm at heap 3000; never connected, advance 30 001 | mode `AccessPoint`, `startAP` delta +1, `wifiEnabled` false, `configSaveCallback_` receives `autoConnect=false, enableAP=true` (the anti-boot-loop save L238-248), `EVENT_AP_ENABLED` + `network/ready` |
| F5 | AP client + heap 3000 → reboot-to-STA | station count 1, loop → nothing torn down; advance 1 500 ms, loop → `restartCountForTest` = 1. The count is per-fire, not latched — a third loop after another 1 500 ms would read 2; don't add one |
| F6 | heap < 2000 ultra-low guard | standalone component, **no `begin()`** (the guard path must own every observable); `enableWifi(true)` → **returns false** (the loop() recipe discards the return — review finding 3), `wifiEnabled` false, config saved `autoConnect=false`, zero WiFi-HAL call deltas |
| F7 | heap 8000 ∈ [3500, 10000) channel-sync | `stopAP` + `Station`, no immediate `startAP` (distinguishes from F8's branch) |
| F8 | heap ≥ 10000 direct AP+STA | mode `StationAndAP`, `startAP` delta +1 immediately, scripted true → `EVENT_AP_ENABLED` |

### AP mode

| # | Test | Pins |
|---|---|---|
| A1 | `begin("")` autogenerated open AP | `startAP("DomoticsCore-000000", <null pw>)` — stub MAC `00:00:00:00:00:00` → colons stripped, `substring(6)`; the null-password tri-state flag set (open network); `apEnabled` true, `wifiEnabled` false, mode `AccessPoint`, both events |
| A2 | `begin("")` with preconfigured AP | `enableAP("MyAP","pw")` before `begin` → preconfigured branch L133: last `startAP` is `("MyAP","pw")`, `Success`. **Not asserted: `wifiEnabled` false** — the pre-begin `enableAP` already forced it false through the stale-config guard, so asserting it here would pin the wrong line (review finding 7). The pre-begin flow also calls `startAP` once itself (L864-868): totals are 2 by the time `begin()` returns — deltas only |
| A3 | skip-restart when AP already up | `startAP` scripted true, A2 flow, snapshot the `startAP` count **after `begin()`**, then re-`setConfig` same AP + loop → **count delta 0** (L856 skip branch; the dropout it prevents is the point). As first specified this asserted a total of 1 and was red against correct code — review finding 2 |
| A4 | stale config guard | `setConfig` empty ssid + `autoConnect=true` → loop → `isWifiEnabled()` false (L738 guard) |

### Non-goals

- No board suite in this lot. The fallback/reconnect logic reads the radio only
  through `isConnected()`/`getFreeHeap()`/millis — the branches are host-
  testable by construction, and what a radio adds (real association timing) has
  no assertion a hidden neighbourhood AP can't corrupt (board-tests-depend-on-
  the-room). The owed board run in this lot is the **existing**
  `test_wifi_scan_esp8266`, unrun since MEM-2 shipped it.
- No widening of `WifiConfig.reconnectInterval`/`connectionTimeout` plumbing
  (getConfig hardcodes 5000/CONNECTION_TIMEOUT — a latent config-ignored defect;
  **file it, don't fix it here** if confirmed).

## Removal checks (every discriminating test proven non-vacuous)

Five mutations, `rm -rf .pio` between each (libdeps copy trap):

1. Invert the 6500 comparison at L212 → F2 and F3 must both go red (F2 sees an
   unwanted `startAP`, F3 loses its restart). Only valid now that both arm the
   timer for real — against the v1 spec this mutation moved neither.
2. Delete the `configSaveCallback_` block at L238 → F4 red.
3. Make `CONNECTION_TIMEOUT` 1 h → R2 red.
4. Delete the skip-restart early-return at L856 → A3's delta goes nonzero → red.
5. Delete the heap guard at L946 → R4 red.

## Order & PR

Slices land as one PR (seams are inert without the suite; the suite does not
compile without the seams). Roadmap: TEST-4 → DONE with the same three-part
evidence style as TEST-6; HIGH 5→4; both reconciliation-paragraph families get
their entry; accounting audit in parallel with code review.
