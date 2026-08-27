# SEC-9 — analysis brief

**Status**: analysed 2026-08-27; decision recorded in §7. Implementation follows.
**Date**: 2026-08-27
**Item**: SEC-9, `docs/CODE-ROADMAP.md:248`, MEDIUM, open.

---

## 1. The finding, as filed

`DomoticsCore-OTA/include/DomoticsCore/OTAWebUI.h:399`

```cpp
size_t expectedSize = request->contentLength();
...
if (!ota->beginUpload(expectedSize, expectedSha)) { ... }
```

`contentLength()` on a `multipart/form-data` POST measures the whole encoded
body — boundary, part headers, trailing boundary — and not the firmware. The
upload handler receives only the firmware bytes; ESPAsyncWebServer has stripped
the framing.

Measured on 2026-08-27 by the real-conditions campaign, on both boards:

| board | announced to `beginUpload()` | actually received | delta |
|---|---|---|---|
| `nodemcuv2` | 475 452 | 475 232 | **220 B** |
| WROOM-32D | 982 508 | 982 288 | **220 B** |

The roadmap records three consequences and one fix:

> 1. Progress never reaches 100 %.
> 2. `Update.begin()` is opened 220 bytes too large … this works *only* because
>    `finalizeUpload()` calls `end(true)`. Anyone who changes that to `end(false)`
>    breaks every browser upload.
> 3. SEC-8's ceiling is compared against the envelope, so a firmware within
>    220 bytes of `maxDownloadSize` is refused when it should be accepted.
>
> **Fix**: … Passing `0` — "size unknown" — is enough … The only thing lost is a
> progress denominator, which is currently wrong anyway.

---

## 2. What checking the fix against the Arduino cores showed

Read from the installed frameworks, not from memory:
`~/.platformio/packages/framework-arduinoespressif8266/cores/esp8266/` and
`~/.platformio/packages/framework-arduinoespressif32/libraries/Update/src/`.

### 2.1 Consequence 2 is not fixed by passing `0`. It is made worse.

Both cores define completion as an exact equality:

| core | file | definition |
|---|---|---|
| ESP8266 | `Updater.h:165` | `bool isFinished(){ return _currentAddress == (_startAddress + _size); }` |
| ESP32 | `Update.h:116` | `bool isFinished(){ return _progress == _size; }` |

and both gate `end()` on it:

| core | file | code |
|---|---|---|
| ESP8266 | `Updater.cpp:226` | `if(hasError() \|\| (!isFinished() && !evenIfRemaining)){ … }` |
| ESP32 | `Updater.cpp:289` | `if(!isFinished() && !evenIfRemaining){ … }` |

`_size` is whatever `begin()` was given. With `UPDATE_SIZE_UNKNOWN` it becomes
the whole target:

- ESP32, `Updater.cpp:159-160` — `if(size == UPDATE_SIZE_UNKNOWN){ size = _partition->size; }`
- ESP8266, via this repository's HAL, `Update_ESP8266.h:34-36` —
  `if (size == 0) { size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000; }`

So the shortfall `end(true)` has to absorb grows from **220 bytes to the whole
unused partition** — hundreds of kilobytes. The dependency on `end(true)` is real,
but it is not caused by the 220 bytes and no pre-flight number removes it: an
exact byte count is unobtainable before the last chunk arrives, and the repository's
own device test already pins `beginUpload(0)` as the *unknown-size* path
(`test_ota_esp8266.cpp:319-342`).

**Consequence 2 as written is therefore not a defect of `contentLength()`. It is
a property of streaming an image whose length is not known in advance.**

### 2.2 Consequence 1 is made worse.

`OTA.cpp:309-313`

```cpp
if (uploadSession.expected > 0) {
    progress = (uploadSession.received * 100.0f) / static_cast<float>(uploadSession.expected);
} else {
    progress = 0.0f;
}
```

`OTAWebUI.h:97` feeds that straight to the WebUI progress field over SSE. Today
the bar reaches 220/475 452 short of the end — **99.954 %**. With `expected == 0`
it reads **0 % for the whole upload**. The roadmap calls the denominator "wrong
anyway"; it is wrong by 0.046 %, and what replaces it is not less wrong.

### 2.3 Passing `0` also removes SEC-8's pre-write refusal from the browser path.

`OTA.cpp:187-193` refuses an oversized upload *before* opening an update — the
ordering SEC-7 established and SEC-8 reused. It is driven entirely by
`expectedSize`, so `beginUpload(0)` skips it. The browser path — the only one a
human uses — would fall through to `acceptUploadChunk()`'s running-total check,
which stops the transfer only after flash has been opened and written. That is
precisely the case `test_ota_esp8266.cpp:319-342` documents as the one an
announced-size check cannot see, and it would become the default.

### 2.4 What erasing actually costs — worth pinning down, currently assumed

SEC-7 and SEC-8 both say `HAL::OTAUpdate::begin()` "erases flash". Reading the
cores, `begin()` allocates a buffer and computes an address; the sector erase
happens lazily in `_writeBuffer()`. Neither core writes to the running image —
ESP32 targets the inactive OTA partition, ESP8266 the free sketch space above the
current sketch. **The ordering principle may be right for a different reason than
the one recorded** (staging, wear and the stored rollback image, rather than
destroying the running firmware). The team should say which, because it decides
how much weight §2.3 carries.

### 2.5 One thing the envelope buys that `0` loses

ESP32 `Updater.cpp:161-164` rejects `size > _partition->size` with
`UPDATE_ERROR_SIZE`. With `UPDATE_SIZE_UNKNOWN` that check cannot fire.

---

## 3. So what is actually broken

Of the three recorded consequences, **only consequence 3 is a fixable defect**:
the envelope is an *upper bound* on the firmware, and SEC-8's ceiling check
treats it as the firmware size, so it refuses images that fit. The error message
quotes a number that is not the firmware size:
`475452 bytes announced against a 100000 byte ceiling`.

Consequence 1 is cosmetic and 0.046 % wide. Consequence 2 is inherent.

The tension: the pre-write check is *safe* on an upper bound (it can only
over-refuse, never under-refuse), and dropping it costs the SEC-7/SEC-8 ordering
on the main path. So the two live options both keep a check and differ in what
number it runs against.

---

## 4. Options on the table

**A — roadmap-literal.** `OTAWebUI` passes `0`. Fixes 3; regresses 1 (0 % bar) and
2 (larger gap); drops the pre-write refusal on the browser path.

**B — name the number an upper bound.** `beginUpload()` gains a size-source hint,
`Exact` (default; `installFromUrl` and every test) versus `MultipartEnvelope`
(`OTAWebUI` only). Under the hint the pre-write ceiling check runs against a
*lower* bound — envelope minus a documented framing allowance — so it can only
refuse an image that cannot fit whatever the framing was, while
`acceptUploadChunk()` stays authoritative on the exact count. The envelope stays
as the `Update.begin()` size (never truncates; keeps §2.5) and as the progress
denominator. Cost: a public API addition on a library installed by version, and a
constant that has to be argued for rather than measured — 220 B observed, and the
bound must cover any client.

**C — message only.** Leave the ~220 B false-rejection window; make the refusal
say the figure includes the multipart envelope. Three lines, no API change, no
regression, and consequence 3 stays open by choice rather than by oversight.

**D — re-file only.** Land the corrected SEC-9 entry now; open the fix as a later
lot.

---

## 5. Questions for the team

1. **Is consequence 3 worth an API change?** `maxDownloadSize` is a deployment
   policy number. Does anyone set it tight enough to the byte for a 220 B
   over-refusal to bite — and if not, is C the honest answer?
2. **What is the framing allowance in B, and how is it justified?** A generous
   constant only shifts work onto the exact chunk check, but "generous" needs a
   number and a reason. Computing it from the boundary and filename is exact and
   fragile; a constant is coarse and stable.
3. **How much does §2.3 matter, given §2.4?** If opening an update costs wear and
   the stored rollback image rather than the running firmware, does keeping a
   pre-write refusal on the browser path justify B over A at all?
4. **How does consequence 2 get closed rather than carried?** Proposal: record
   `evenIfRemaining` in `Update_Stub.h` and assert `end(true)` in the native
   suite, so changing it to `end(false)` goes red instead of silently breaking
   every browser upload. Is a test the right home for that invariant, or a
   comment at the call site, or both?
5. **What proves the chosen fix on hardware, non-vacuously?** Per the 2026-08-26
   lesson: what would still pass if the change were removed? The device suites
   call `beginUpload()` directly and never traverse multipart, so the only
   harness that exercises this path is a real browser upload against
   `OTAWithWebUI` — `tools/on-device/`, both boards.
6. **Does any of this touch `marianorenzi`'s `esp32-ethernet` work?** `OTA.cpp`
   and `OTAWebUI.h` are not in his fourteen overlapping files, but a public
   signature change is worth checking before, not after.

---

## 6. Constraints

- One lot, one PR, off `main`. PR #35 (the real-conditions campaign) is open and
  also edits `docs/CODE-ROADMAP.md`.
- No version bumps inside the lot.
- Whatever is decided, SEC-9's roadmap entry is rewritten: the fix it currently
  records would not have done what it says.

---

## 7. Decision (2026-08-27)

Taken after a round-table on this brief, and after reading a board.

### 7.1 What the hardware settled

`/dev/ttyUSB0` is a CP2102 / **ESP32-D0WD-V3 rev 3.1**, MAC `08:a6:f7:6b:0c:88` —
the WROOM-32D, not the ESP32-CAM (whose FTDI `FTB6SPL3` was not attached).
Partition table read from flash at `0x8000`:

```
nvs      data nvs     0x9000    20K
otadata  data ota     0xe000     8K
app0     app  ota_0   0x10000 1920K
app1     app  ota_1   0x1f0000 1920K
spiffs   data spiffs  0x3d0000 128K
coredump data coredump 0x3f0000 64K
```

Two OTA slots — the `min_spiffs` shape, inherited from whatever last flashed the
board rather than from the OTA suite's `default.csv`. Either way the question is
answered, and a tracked comment answers it harder than the board does.
`DomoticsCore-OTA/platformio.ini:32-37`:

> This board defaults to a single 3 MB app slot, and OTA has nowhere to write:
> `esp_ota_get_next_update_partition()` then returns the *running* partition
> rather than null, Update erases the code it is executing from, and ESP-IDF
> aborts inside spi_flash with no message.

So §2.4's open question is closed, and in the direction that makes the pre-write
refusal matter **more** than SEC-7 and SEC-8 claimed. `Updater.cpp:134` opens the
update on `esp_ota_get_next_update_partition()`, which is the partition
`canRollBack()` reads at `Updater.cpp:98`. Opening an update in order to refuse it
destroys the rollback image on a correctly partitioned ESP32, and erases the
running code on a badly partitioned one.

### 7.2 Options A and B are both rejected

**A (pass `0`) is rejected** — not for the 220 bytes, but because it removes the
pre-write refusal from the only path a human uses, with §7.1 as the cost. It also
regresses the progress bar from 99.954 % to 0 % for the whole upload, and widens
the `end(true)` shortfall it claims to fix.

**B (a `SizeSource` hint) is rejected** — a permanent public API concept, on a
library installed by version, bought for a ~220-byte window that no realistic
`maxDownloadSize` meets. The allowance it would subtract is written by the sender.

### 7.3 What is done instead

1. **The refusal says what it compared**, and the contract is written down: the
   ceiling is authoritative over *received* bytes (`acceptUploadChunk()`, SEC-8's
   second check); the pre-write check is a deliberately conservative fast-fail on
   the announced envelope.
2. **`progress` reaches 100 %** on a successful finalize. The only consequence
   anyone sees.
3. **`end(true)` becomes a guarded invariant** — `Update_Stub.h` records
   `evenIfRemaining`, and the native suite asserts it. Changing it to `end(false)`
   goes red instead of silently breaking every browser upload.
4. **A new TEST item is filed**: no suite traverses `POST /api/ota/upload`. The
   device suites call `beginUpload()` directly, so nothing in the repository has
   ever seen a multipart envelope. That absence is what left SEC-9 invisible, and
   it is worth more than SEC-9.
5. **SEC-9 is rewritten and downgraded MEDIUM → LOW**, because two of its three
   consequences are not what they say. The rewritten entry must carry, where it
   cannot be skipped, *why passing `0` is a regression* and the Arduino core lines
   that establish it — a severity is a scheduler, not a warning label, and this
   repository has already been bitten twice by a tracking field that meant
   something other than it appeared to.

### 7.4 Residual disagreement, left open

Whether the safety of this repository lives in its text or in its process. The
severity was argued both ways: honest-and-low with the warning in the prose,
versus inflated-to-schedule-the-fix. The first won here on the BUG-2 and BUG-29
precedent. It will come back on the next item.
