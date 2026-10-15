<!-- workline
sources: [DomoticsCore-WebUI/webui_src/index.html, DomoticsCore-WebUI/webui_src/style.css, DomoticsCore-WebUI/webui_src/app.js, DomoticsCore-WebUI/webui_test/harness.js, DomoticsCore-WebUI/webui_test/app.test.js, DomoticsCore-WebUI/webui_test/package.json]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- Front End

Part of the [DomoticsCore-WebUI Project Context](project-context.md) (AI reference).

## Frontend Source (`webui_src/`)

| File | Lines | Purpose |
|------|-------|---------|
| `index.html` | 171 | Dashboard HTML shell |
| `style.css` | 527 | Responsive theme-aware styles (dark/light/auto) |
| `app.js` | 1349 | Frontend logic: schema fetch, SSE/polling, dynamic rendering, actions |

## Front-end Suite

The front-end suite runs `webui_src/app.js` in jsdom, the one npm dependency, pinned by `package-lock.json`: `cd webui_test && npm ci && npm test`. Its harness replaces the app's `init()` and removes the page's `DOMContentLoaded` bootstrap, so nothing polls; a test builds the cards it needs and drives them, and an exception inside a handler fails the case. It proves what the page draws and sends, not how a browser lays it out, and it does not run the custom JavaScript a component injects.

A rendered settings field shows the schema's placeholder, not the stored value, until the first SSE tick redraws it — roughly five seconds after load. Anything that drives the page must wait for the field to carry the stored value before editing it: setting a field to the value the device already holds fires no `change` event, so `applySave()` posts nothing and the run looks like a clean negative while measuring nothing.

A refused field is drawn refused: the answer's `"error"` reason is written under the field the value was typed into, or on the card when no field is in cause. It is cleared when the next attempt starts, on entering edit mode and on cancel — never on a redraw, which happens every tick and would wipe it. A refusal that names nothing shows nothing: the field still reverts from the stored value at the next tick, and outside edit mode (`alwaysInteractive`, and every dashboard card) only once focus leaves it, since `updateCardData` skips the input holding focus.
