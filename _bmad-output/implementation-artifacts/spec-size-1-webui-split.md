# SIZE-1 — WebUI.h under 800 lines, without surprising the fork

Status: v2 — amended after adversarial review, 2026-08-31. The review
falsified three sentences of the v1 draft: the fork's WebUI.h diff is
**+12/−11** (`--numstat`), not the +23/−23 v1 announced; the size projection
netted region removals without netting the wrapper/forwarder lines added back;
and "the loop moves verbatim" was undefined while two source texts with known
divergences existed. It also caught v1 re-creating the duplication it
diagnoses: after BUG-34's fix the two route wrappers would have been
byte-identical twins — the retry policy now lives in **one** shared helper.

Roadmap item: SIZE-1 [HIGH], refs WEB-F2, R-F1.
Filed at 950 lines; **1008 today** (it grew through SEC-10 and BUG-32, as
SIZE-2's file did between filing and fixing). Constitution VII caps at 800.
**The metric is raw `wc -l`, by campaign precedent** — SIZE-2 measured
933 → 756 raw — stated here because the constitution's own wording excludes
blanks and comments; the campaign has always measured the stricter raw count,
and this entry closes against that.

The filed remedy — "extract route setup + SSE broadcasting into `WebUIRoutes.h`
/ `BroadcastManager.h`" — predates the modular split that already happened
(`WebServerManager`, `WebSocketHandler`, `ProviderRegistry`, `SystemHeader` all
exist and hold what those names would hold), and predates two facts this design
turns on.

## Constraint 1: `esp32-ethernet` touches WebUI.h in exactly two regions

Measured against the fork's merge-base (`git diff --numstat b17f0a1c
fork/esp32-ethernet -- …/WebUI.h`: **+12/−11**, three regions):

1. **`onComponentsReady`** (today's lines 296–312): he replaces the
   `on<bool>("wifi/ap/enabled", …)` subscription with two `NetworkEvents`
   subscriptions. Plus one `#include "DomoticsCore/NetworkEvents.h"` added in
   the include block.
2. **`serializeContext`'s multiselect block** (today's lines 892–894): his
   `field.isMultiValue()` / `field.values` renames.

Both regions — and the include block lines his hunks use as context — **stay in
WebUI.h, byte-identical**. New includes join the existing "New modular headers"
group (lines 29–33), below his context. His `IWebUIProvider` renames are **not
adopted**, the same decision SIZE-2 made and for the same reason: adapting to a
moving branch's API is his rebase's job in one direction only.

`serializeContext`'s comment ("Duplicated helper to keep compilation working
until I move it…") is stale — the twin it refers to no longer exists — and it
stays stale: it sits nine lines above his hunk context and touching that
neighbourhood buys nothing but conflict risk.

## Constraint 2: native tests never compile WebUI.h

The native environment `lib_ignore`s ESPAsyncWebServer; `test_webui_component`
includes `ProviderRegistry.h` and friends, never `WebUI.h`. Consequence, both
ways: whatever moves out of WebUI.h into an AsyncWebServer-free header becomes
natively testable for the first time; whatever stays (the route lambdas, the
`RESPONSE_TRY_AGAIN` policy) remains untestable off-device.

## The finding that reshapes the remedy: three copies of the schema loop, one drifted

The ~80-line chunk-assembly loop (provider iteration, comma placement, the
`contextIndexInProvider--` backtrack when a comma doesn't fit, the
`vector().swap` release) exists **three times**:

- **Lambda A**, `/api/ui/updates?schema=1` (lines 474–538). Carries the
  v1.5.0 truncation fix — `b961e43a`: "Fix chunked schema truncation: return
  RESPONSE_TRY_AGAIN instead of 0 when serializer can't write".
- **Lambda B**, `/api/ui/schema` (lines 774–855). **Never received that fix.**
  When the serializer legitimately stalls — escape atomicity: fewer than six
  bytes of buffer remain and the next unit is a `\u00XX` escape, so `write()`
  returns 0 with nothing yet written this callback — B returns 0 from the
  chunked callback, which ends the response: malformed JSON, exactly the
  field-diagnosed defect v1.5.0 fixed in A.
- **The tests hand-roll a third**: `test_full_schema_array_valid_json` builds
  the array "like the chunked endpoint does"; the on-device
  `test_schema_memory` likewise "simulates schema generation (like
  /api/ui/schema does)". Nothing anywhere drives the real loop — its backtrack
  path has never been executed by a test.

**BUG-34, to be filed in-lot**: `/api/ui/schema` truncates on serializer
stall. Severity MEDIUM, argued: the shipped frontend has used only
`?schema=1` since v1.5.0 (measured: `app.js` fetches `/api/ui/updates?schema=1`
and nothing fetches `/api/ui/schema`), so no shipped UI breaks; but the route
is **documented public API** (`docs/components/webui/README.md:96`,
`docs/components/webui/technical-reference.md:596`), DOC-1 records it being
fetched by hand on both an ESP8266 and an ESP32 during the campaign (it works
in the non-stall case), the stall is **reachable** — determinism is the native
test's property, not the wire's — and the identical shape was a real field
defect once already (`b961e43a`).

## Design: three slices, none touching the fork's regions

### Slice A — `SchemaChunkState::writeChunk` (ProviderRegistry.h, +~80 → ~425 lines)

```cpp
// In ProviderRegistry::SchemaChunkState
size_t writeChunk(uint8_t* buffer, size_t maxLen);
```

**Why this home**: `SchemaChunkState` is `prepareSchemaGeneration()`'s return
type — the registry's API already owns the state's definition, and a writer
separated from its state would split one contract across two headers for no
reader's benefit. ProviderRegistry.h lands at ~425, under the cap with room;
if a later sweep flags it, state and writer move together.

**"Verbatim" is defined by a recorded diff, not by intent**: there are two
source texts, and step 1 of the Order is a textual diff of the two lambda
bodies with every divergence listed in the extraction commit message. Two are
known — A returns `RESPONSE_TRY_AGAIN` where B returns 0, both at the
`maxLen < 1`-before-`began` gate and at the end-of-body
`written == 0 && !finished` case. The core takes the shared text; **every
divergence the diff shows must end up owned by a wrapper**, and a third
divergence, if one exists, stops the lot until it is understood. The core
returns bytes written, sets `finished`, keeps the
`std::vector<IWebUIProvider*>().swap(providers)` release, and never references
`RESPONSE_TRY_AGAIN` — retry-vs-end is HTTP route policy, not serialization.
The `maxLen < 1`-before-`began` path maps exactly: core returns 0 with `began`
untouched.

The route lambdas become wrappers over the core:

```cpp
// A (?schema=1) — unchanged policy:
size_t w = state->writeChunk(buffer, maxLen);
if (w == 0 && !state->finished) return RESPONSE_TRY_AGAIN;
return w;

// B (/api/ui/schema) — FIRST exactly today's policy: return writeChunk's
// result as-is (still truncating). Then BUG-34's fix, its own commit.
```

**BUG-34's fix does not clone A's lines into B** — that would re-create the
drifted-duplicate shape that caused BUG-34. The fix commit hoists the retry
policy into one private helper both routes call:

```cpp
// WebUIComponent, private — the ONE place the retry decision exists:
size_t writeSchemaChunkHttp(WebUI::ProviderRegistry::SchemaChunkState& state,
                            uint8_t* buffer, size_t maxLen) {
    size_t w = state.writeChunk(buffer, maxLen);
    if (w == 0 && !state.finished) return RESPONSE_TRY_AGAIN;
    return w;
}
```

Extraction first (behaviour-preserving on both routes, B still truncating),
BUG-34's fix second — the SIZE-2 order, so the refactor commit changes no
behaviour and the fix commit is legible alone.

### Slice B — `WebUI/SchemaMemProbe.h` (new, ~110 lines)

The heap-staging diagnostics: the `SchemaMemProbe` struct, the 6-slot array +
sequence counters (lines 69–83), the staged +500ms/+2s/+10s `tick()` loop
(167–200), the arming block (748–767) and the queued-log (862–867).

```cpp
namespace WebUI {
class SchemaMemProbes {
public:
    struct Armed { uint32_t seq; uint32_t heapBefore; uint32_t maxBefore; };
    Armed arm();                 // claims a slot, samples heap, stage 0
    void tick();                 // the staged DLOG_D emissions
    void logQueued(const Armed&);// the "Schema queued" delta line
};
}
```

`Armed` is what the `onDisconnect` lambda captures (it already captures
seq/heapBefore/maxBefore by value today). **Diagnostics-only, argued**: every
emission is `DLOG_D`, no caller branches on any of it — the check is
cross-compilation on all three targets plus the existing suites, not a
behaviour test.

### Slice C — `WebUI/UpdateBuilder.h` (new, ~100 lines)

`buildUpdateJson` (lines 930–980) becomes a free function beside
`buildSystemHeader`, which BUG-32 already extracted as "the first slice of
SIZE-1":

```cpp
namespace WebUI {
int buildUpdateJson(char* buf, size_t bufSize,
                    const std::map<String, IWebUIProvider*>& contextProviders,
                    const char* deviceName,
                    uint32_t millisNow, uint32_t freeHeap, int wsClients,
                    bool forceFull, bool forceNext);
}
```

Includes: `SystemHeader.h`, `IWebUIProvider.h`, `Platform_HAL.h` (home of
`DSNPRINTF_P` — measured, `Platform_HAL.h:152`). WebUI.h keeps a thin private
forwarder assembling the member arguments; `sendWebSocketUpdates` and the
`forceNextUpdate` flag stay put. This is the function BUG-32's crowding lives
in — the ~155 bytes escaping can steal from the 1024-byte update, pinned today
only at the `buildSystemHeader` level. Extraction gives the crowding (contexts
silently dropped) a native home at the level where it actually happens.

### What this is not

- **Not a route-class extraction.** The handlers' substance is auth/CSRF/CORS
  policy wired to a dozen members; a `WebUIRoutes` class would be a callback
  bundle with no seam worth testing. The fat was the triplicated loop and the
  diagnostics, not the routing.
- **Not touching** `serializeContext`, `onComponentsReady`, the public API
  (`checkCsrf` is used by OTAWebUI; the facades by System), or his renames.

### Projected sizes — removed and added both counted

| WebUI.h | removed | added back |
|---|---|---|
| probe struct + members (69–83) | 15 | 1 (member) |
| probe tick loop (167–200) | 34 | 1 (`tick()`) |
| lambda A body (474–538) | 65 | 4 (wrapper) |
| probe arming (748–767) | 20 | 5 (arm + capture) |
| lambda B body (774–855) | 82 | 3 (wrapper) |
| queued-log (862–867) | 6 | 1 |
| `buildUpdateJson` (930–980) | 51 | 9 (forwarder) |
| shared retry helper (BUG-34 commit) | — | 6 |
| includes | — | 2 |
| **total** | **273** | **32** |

1008 − 273 + 32 = **~767** projected. **Fallback slice, named now**: if the
landing exceeds 780, the uptime formatter inside `getWebUIData` (lines
343–361, ~19 lines) moves to `UpdateBuilder.h` as `formatUptime(uint32_t)` —
it is pure and native-testable, and it does not neighbour either fork region.
ProviderRegistry.h ~425, SchemaMemProbe.h ~110, UpdateBuilder.h ~100 — all
under 800 raw, margin comparable to SIZE-2's landing (756).

## Measurement (what closes a SIZE item)

- `wc -l` on all four files, in the roadmap entry.
- Existing suites green before and after every slice, `rm -rf .pio` before any
  run that counts.
- **New native suites, in new test directories** (`test_schema_chunking`,
  `test_update_builder`; `test_filter` in the native env grows — SIZE-6 is
  2520 lines already and gains nothing). **A suite absent from `test_filter`
  compiles nothing, runs nothing and says nothing** — the on-device suites'
  exact rot mode — so the check is positive: both new names visible in
  `pio test -e native` output, and the roadmap's native CI count (804 today)
  updated in the same lot:
  1. **Chunk-size sweep** over `writeChunk`, sizes 6–64 plus a one-shot 4096
     reference, byte-identical and parseable, on a maximal fixture: several
     providers, a disabled provider, a contextless provider, an empty-id
     context, escapes in values, a multiselect. Floor 6 for the same
     escape-atomicity reason as SIZE-2's sweep, with a control character in
     the content to make it honest.
  2. **Stall determinism** (BUG-34's core precondition): drive `writeChunk`
     with `maxLen` 1 into a title carrying a control character until it
     returns 0 with `finished == false`, then complete with a large buffer —
     proving the stall reachable and recoverable at the core level.
  3. **Skip logic**: disabled/null provider skipped, empty-id context skipped,
     zero providers yields `[]`.
  4. **UpdateBuilder**: crowding (small buffer, many contexts → valid JSON,
     later contexts dropped, no overflow), delta skip (`forceFull=false`,
     unchanged provider excluded), `forceNext` overriding the delta check,
     empty/`{}` data skipped, header fields parse. **The fixture provider must
     override `hasDataChanged`** with a controllable flag — the
     `IWebUIProvider` default returns `true` (`IWebUIProvider.h:525`), so an
     un-overridden fixture can never exhibit a delta skip and M5 would
     discriminate nothing; read the `LazyState` contract before asserting
     call counts, in case the real implementations consume-on-read.
- **Named mutations on the new code**, each expected red, `rm -rf .pio`
  between: (M1) drop the `contextIndexInProvider--` backtrack → sweep red;
  (M2) drop `needComma = true` on context completion → parse red; (M3) never
  set `began` → sweep red; (M4) drop the `pos + needed` bound in
  `buildUpdateJson` → crowding red; (M5) ignore `hasDataChanged` → delta red.
- **The recorded survivor, stated rather than hidden**: reverting BUG-34's
  helper call in route B reds **nothing** — the wrapper compiles only against
  ESPAsyncWebServer, so no native test can see it, and no on-device suite
  fetches the route over HTTP (**verified for both**: `test_schema_memory`
  "simulates schema generation" and `test_heap_esp8266`'s
  `test_esp8266_chunked_large_schema` "simulates chunked streaming" — neither
  constructs a request). The fix is
  argued by symmetry with the v1.5.0 field fix + the native stall proof; the
  wrapper gap joins TEST-8's family of envelope-level gaps and is recorded in
  the BUG-34 entry. If a board session runs, a manual fetch of
  `/api/ui/schema` on the nodemcuv2 is the closest available check and should
  be taken opportunistically.
- **Board**: WebUI on-device suites (`test_heap_esp8266`, `test_schema_memory`
  3/3) before/after on the nodemcuv2 — owed if no port is available this
  session, recorded either way.

## Fork coordination, stated

His two WebUI.h hunks and his include-block hunk apply to untouched regions;
his rebase of this file stays a no-op with respect to this lot. The owed
notification message grows one line: WebUI.h shed ~240 lines into
`ProviderRegistry.h`/`SchemaMemProbe.h`/`UpdateBuilder.h`, none of which his
branch modifies.

## Order

Branch `size-1-webui-split`, one PR for the lot, versions and CHANGELOG
untouched per series convention.

0. Baseline: `rm -rf .pio`, full native runs, suite counts recorded.
1. The lambda-body diff, divergences recorded (stop if a third appears). Then
   slice A extraction, wrappers preserving both routes' exact behaviour
   (B still truncating). Suites green.
2. `test_schema_chunking` written against the core: sweep, stall, skips.
   Green; both new suite names confirmed in the runner output.
3. BUG-34: entry filed; the shared `writeSchemaChunkHttp` helper, both routes
   on it; the removal check run and its silence recorded.
4. Slice B (probes). Green; cross-compilation spot-check.
5. Slice C (`UpdateBuilder.h`) + `test_update_builder`. Green.
6. Mutations M1–M5; `wc -l` all four files (fallback slice if WebUI.h > 780);
   full native runs from clean; native count re-derived for the roadmap.
7. Board runs if a port answers; owed otherwise.
8. Roadmap: SIZE-1 → DONE with measurements in-entry; BUG-34 filed and fixed
   in-lot; HIGH 3 → 2 with both reconciliation families and the sums checked
   section-against-code, not only rows-against-total.
