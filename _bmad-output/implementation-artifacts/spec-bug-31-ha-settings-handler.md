---
title: 'BUG-31: HA settings cannot be saved from the UI, and every request writes flash anyway'
type: 'bugfix'
created: '2026-08-29'
status: 'complete'
baseline_commit: '47da9adefd42a6cbc9afd9007b612b28687aaf6d'
review_loop_iteration: 0
context:
  - '{project-root}/_bmad-output/specs/spec-test-6-webui-provider-inputs/SPEC.md'
  - '{project-root}/_bmad-output/specs/spec-test-6-webui-provider-inputs/providers.md'
  - '{project-root}/CLAUDE.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `HomeAssistantWebUI::handleWebUIRequest` reads six parameters by name, but the framework's only dispatcher supplies exactly `field` and `value`. No read can hit, so HA settings cannot be saved from the UI — while `setConfig`, the flash-writing persistence callback and `publishDiscovery()` run on every request and the handler answers `success`. The route is a `HTTP_GET`, and `enableAuth` defaults to false, so any page on the LAN can fire it repeatedly with an `<img>` tag.

**Approach:** Move the handler to the single `field`/`value` convention every other provider already uses, mutate only when a known field's value actually changed, and regenerate the availability topic when the node id or discovery prefix moves. Make the provider testable on the host first — its `WebUI.h` include is what stops any native suite compiling.

## Boundaries & Constraints

**Always:** Refusals return `{"success":false}` with **no** `error` key — `app.js` inspects only `data.error`, so an error key pops a modal alert while a refusal shows nothing. Every new test must fail against the unfixed handler; record what would still pass. `rm -rf .pio` before any quoted run.

**Ask First:** Changing `WebUI.h`, `IWebUIProvider.h` or the dispatcher's parameter contract; adding public API to `HomeAssistantComponent`; any `library.json` version.

**Never:** Close TEST-6 — that is the next lot. Do not touch `SystemInfoWebUI`, `StorageWebUI` or the escaping; do not fix the sibling providers' over-includes, the unregistered `withAPI` endpoints or the inherited `hasDataChanged` — all three are filed. No rate limiting and no coalescing of the persistence callback: BUG-31 records both as residual.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|---|---|---|---|
| Known field, new value | `field=node_id&value=lab01` | `nodeId` becomes `lab01`; persistence callback fires once; discovery republished | N/A |
| Known field, same value | `value` equals current | No `setConfig`, no callback, no republish | `{"success":true}`, nothing done |
| Unknown field | `field=colour` | Nothing changes | `{"success":false}`, no `error` key |
| Missing field or value | one param absent | Nothing changes | `{"success":false}` |
| Non-POST method | `method="GET"` | Nothing changes | `{"success":false}` |
| Wrong context | `contextId=ha_status` | Nothing changes | `{"success":false}` |
| Node id changed | `field=node_id` on a config with a generated availability topic | `availabilityTopic` regenerated for the new node | N/A |
| Over-long value | 64-character `node_id` | Truncated by `HA::setField` to 32 with its existing warning | Truncate + warn |
| Null component | `ha == nullptr` | Returns a failure rather than dereferencing | Guard, as `LEDWebUI.h:116` |

</frozen-after-approval>

## Code Map

- `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h:5` -- `#include "DomoticsCore/WebUI.h"`, which pulls `<ESPAsyncWebServer.h>` (`WebUI.h:11`) and fails on the host. The file already includes `IWebUIProvider.h` at `:4`, so this line is one too many, not one missing. `LEDWebUI.h:4,7` is the working shape.
- `…/HomeAssistantWebUI.h:145-179` -- the handler. Six `params.find()` reads at `:150,154,157,160,163,166`; `ha->setConfig(newCfg)` `:168`; `onConfigSaved(newCfg)` `:172`; `ha->publishDiscovery()` `:176`; the success message `:178`.
- `…/HomeAssistantWebUI.h:81` -- `.withAPI("/api/ha/settings")`, never registered anywhere. File, do not fix.
- `DomoticsCore-LED/include/DomoticsCore/LEDWebUI.h:116-200` -- the convention to follow: method and context checked, `field`/`value` looked up, unknown field refused, per-field validation, mutate only on change.
- `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h:465-475` -- `setConfig` regenerates `availabilityTopic` **only when it is empty**; `:41-49` is `HA::setField`, the truncate-and-warn that is the only validation these fields have.
- `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h:544` (route, `HTTP_GET`), `:865-876` (dispatcher, two params, fabricates `"POST"` at `:871`), `WebUIConfig.h:27` (`enableAuth` false by default) -- the reason the side effects are reachable unauthenticated.
- `DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h:381-386` -- the `onConfigSaved` lambda's three `putString` calls; compiled by no native project, which is why the tests observe the callback rather than flash.
- `DomoticsCore-LED/test/test_led_webui/test_led_webui.cpp:42-51` -- model fixture: provider built over a bare component, no Core, requests driven by calling `handleWebUIRequest` directly.
- `DomoticsCore-HomeAssistant/platformio.ini:1-20` -- `[env:native]` has `lib_ignore = PubSubClient` and no WebUI include path or dependency; adding `file://../DomoticsCore-WebUI` requires appending `ESPAsyncWebServer, AsyncTCP, ESP Async WebServer` to that `lib_ignore`, as `DomoticsCore-LED/platformio.ini:6` does.

## Tasks & Acceptance

**Execution:**
- [x] `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h` -- drop the `WebUI.h` include, add `BaseWebUIComponents.h` if the file needs it -- nothing native can compile against this provider until then.
- [x] `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h` -- rewrite the `ha_settings` handler to the `field`/`value` convention: refuse unknown fields, apply per-field, mutate only on change, regenerate `availabilityTopic` when `node_id` or `discovery_prefix` moved, guard a null component.
- [x] `DomoticsCore-HomeAssistant/platformio.ini` -- add the WebUI include path and `file://` dependency to `[env:native]`, append the async-server names to `lib_ignore`.
- [x] `DomoticsCore-HomeAssistant/test/test_ha_webui/test_ha_webui.cpp` -- the suite: every matrix row, each stated as failing or not failing against the unfixed handler. Settle first whether `getStatistics().discoveryCount` increments natively with no MQTT; if it does not, name the observable that replaces it.
- [x] `docs/CODE-ROADMAP.md` -- file **BUG-31** and close it, recording the GET shape, the unauthenticated default, and the two residuals the fix does not bound (a caller that varies the value; six changed fields now costing six republishes and eighteen `putString`). File the sibling over-includes and the unregistered `withAPI` endpoints as their own item. Do not touch TEST-6's status.

**Acceptance Criteria:**
- Given a cleared `.pio`, when `pio test -e native` runs in DomoticsCore-HomeAssistant, then the new suite runs alongside the existing six and the device suite is still ignored.
- Given the unfixed handler, when the new suite runs against it, then every test the matrix marks discriminating fails.
- Given `field=node_id&value=lab01` twice, when the second request is handled, then no persistence callback and no discovery republish occur.

## Verification

**Commands:**
- `cd DomoticsCore-HomeAssistant && rm -rf .pio && pio test -e native` -- expected: all suites green, device suite absent
- `cd DomoticsCore-HomeAssistant && pio test -e esp8266dev --without-uploading --without-testing` -- expected: compiles, device suite only
- `cd DomoticsCore-System/examples/FullStack && pio run -e esp8266dev` -- expected: builds; record RAM and flash

**Manual checks (if no CLI):**
- No board run in this lot: the claims are behavioural. Say so rather than leaving it implied.
