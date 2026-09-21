"""Type a value into a settings field with a real browser, and report what the
device stored and what the page said.

A native suite can assert the handler's answer. It cannot show whether the
refusal reaches the person typing: app.js turns an "error" key into a blocking
alert and leaves a text field alone otherwise, so "refused" and "silently
refused" look identical until a browser drives the page.

Reads the stored value back from /api/ui/updates rather than from the DOM, so a
field that merely redrew cannot be mistaken for one that never moved.

Two paths, because a refusal travels differently on each:

  --mode save    a Settings card driven through its Edit/Save buttons
  --mode change  a card with no Save button — a dashboard card, or one
                 declared alwaysInteractive — which posts on change; with
                 --click, the field is a button and the value is not typed

Reports what the page shows for the refusal: the message under the field, the
card banner, and any modal dialog — a dialog is what this used to be, and it
stops the page's own polling until somebody dismisses it.

Exits non-zero when the measurement could not be taken, so a bench script can
tell "refused" from "never ran".

usage: webui_settings_refusal_check.py [--mode save|change] [--click]
                                       URL CONTEXT FIELD VALUE OUTDIR
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright

args = sys.argv[1:]
mode, click = "save", False
while args and args[0].startswith("--"):
    flag = args.pop(0)
    if flag == "--mode" and args:
        mode = args.pop(0)
    elif flag == "--click":
        click = True
    else:
        sys.exit(__doc__)
if len(args) != 5 or mode not in ("save", "change"):
    sys.exit(__doc__)
if click and mode != "change":
    sys.exit("--click drives a button, which only the change mode reaches: add --mode change")
url, context, field, value, outdir = args
url = url.rstrip("/")


def stored(strict=True):
    """The value the device holds.

    strict raises SystemExit with why it could not be read; otherwise a failed
    read answers None, which the poll below treats as "not yet" rather than as
    a change — a context crowded out of one broadcast must not read as a save.
    """
    try:
        with urllib.request.urlopen(f"{url}/api/ui/updates", timeout=8) as r:
            payload = json.load(r)
    except urllib.error.HTTPError as e:
        if not strict:
            return None
        # /api/ui/updates sits behind enableAuth; this probe does not
        # authenticate, so say that rather than dying on a traceback.
        sys.exit(f"{url}/api/ui/updates answered {e.code} — authentication is not supported here")
    except (urllib.error.URLError, OSError, ValueError) as e:
        if not strict:
            return None
        sys.exit(f"{url}/api/ui/updates unreachable: {e}")
    return (payload.get("contexts") or {}).get(context, {}).get(field)


os.makedirs(outdir, exist_ok=True)

before = stored()
if before is None and not click:
    sys.exit(f"{context}.{field} is not in /api/ui/updates — check the context and field names")

dialogs, console_errors = [], []

with sync_playwright() as p:
    browser = p.chromium.launch()
    try:
        page = browser.new_context(viewport={"width": 1280, "height": 900}).new_page()
        page.on("console", lambda m: console_errors.append(f"{m.type}: {m.text}")
                if m.type == "error" else None)
        # An alert blocks the page until it is answered; accepting it also
        # records the text, which is the whole point of the run.
        page.on("dialog", lambda d: (dialogs.append(d.message), d.accept()))

        updates, actions = [], []
        page.on("request", lambda r: (updates.append(time.time())
                                      if "/api/ui/updates" in r.url else None,
                                      actions.append(time.time())
                                      if "/api/ui/action" in r.url else None))
        # The page lives on SSE where the board supports it and falls back to
        # polling on an ESP8266; count both, or "still updating" reads as zero
        # on a device that was never polling in the first place.
        page.add_init_script("""
            window.__sse = 0;
            const Native = window.EventSource;
            if (Native) {
                window.EventSource = function (...a) {
                    const es = new Native(...a);
                    es.addEventListener('message', () => { window.__sse++; });
                    return es;
                };
                window.EventSource.prototype = Native.prototype;
                // The page reads EventSource.CLOSED to fall back to polling; a
                // wrapper without the statics silently disables that fallback.
                window.EventSource.CONNECTING = Native.CONNECTING;
                window.EventSource.OPEN = Native.OPEN;
                window.EventSource.CLOSED = Native.CLOSED;
            }
        """)

        page.goto(url, wait_until="networkidle", timeout=45000)
        card = page.locator(f".card[data-context-id='{context}']")
        if mode == "save" or card.count() == 0:
            page.click("[data-section='settings'], a[href='#settings']", timeout=10000)

        # The section renders asynchronously, so wait for the card before
        # counting it — a count taken on the empty section always reads zero.
        try:
            if mode == "save":
                card.locator(".btn-edit").wait_for(state="attached", timeout=15000)
            else:
                page.locator(f"#{context}_{field}").wait_for(state="attached", timeout=15000)
        except PlaywrightTimeoutError:
            sys.exit(f"{context}.{field} never appeared — in save mode the card needs an "
                     f"Edit button, in change mode the field needs an id")
        if card.count() != 1:
            sys.exit(f"{context} matches {card.count()} cards, expected one")
        # A freshly rendered field carries the schema's placeholder, not the
        # stored value, until the first SSE tick redraws it. Editing before that
        # makes a fill() of the stored value a no-op change, so nothing is ever
        # posted and the run measures nothing while looking like a clean result.
        if not click:
            try:
                page.wait_for_function(
                    "([id, want]) => document.getElementById(id)?.value === String(want)",
                    arg=[f"{context}_{field}", before], timeout=30000)
            except PlaywrightTimeoutError:
                sys.exit(f"{context}_{field} never showed the stored value {before!r} — "
                         f"text and number fields only")

        if mode == "save":
            card.locator(".btn-edit").click()
            page.fill(f"#{context}_{field}", value)
            card.locator(".btn-save").click()
        elif click:
            page.click(f"#{context}_{field}")
        else:
            # No Save button here: the listener posts on change, so the value
            # has to leave the field for anything to happen.
            page.fill(f"#{context}_{field}", value)
            page.locator(f"#{context}_{field}").blur()

        # Counted from the action, not from the end of the settle loop below: a
        # refusal never moves the stored value, so that loop always runs its full
        # deadline and a window opened after it would miss the answer entirely.
        settled = time.time()
        sse_before = page.evaluate("() => window.__sse || 0")

        # applySave() pauses polling 600 ms, posts, waits 400 ms, then resumes.
        # Poll rather than sleep: a flat wait that is a little too short reports
        # an accepted value as refused.
        after, deadline = before, time.time() + 20
        while time.time() < deadline:
            time.sleep(1)
            current = stored(strict=False)
            if current is not None and current != before:
                after = current
                break
        # What the page shows about the refusal, read where a person would see
        # it: under the field, then on the card.
        row = page.locator(f".field-row:has(#{context}_{field}) .field-error")
        field_error = row.first.inner_text() if row.count() else None
        banner = card.locator(".card-error")
        card_error = banner.first.inner_text() if banner.count() else None

        # Context, not evidence: this probe accepts a dialog the moment it opens,
        # so a modal never holds the page here. `dialogs` is what separates the
        # two shapes; this says the page went on living either way.
        updates_after = (len([t for t in updates if t > settled])
                         + page.evaluate("() => window.__sse || 0") - sse_before)

        # "Nothing was posted" must not be printable as "nothing came back": a
        # non-trusted event, a slider listening on `input`, a disabled control.
        if not [t for t in actions if t > settled - 2]:
            sys.exit(f"no POST to /api/ui/action was issued for {context}.{field} — "
                     f"the page never sent the value, so nothing was measured")

        page.screenshot(path=f"{outdir}/settled-{context}-{field}.png", full_page=True)
    finally:
        browser.close()

print(json.dumps({
    "field": f"{context}.{field}",
    "mode": "click" if click else mode,
    "typed": None if click else value,
    "stored_before": before,
    "stored_after": after,
    "changed": before != after,
    "field_error": field_error,
    "card_error": card_error,
    "dialogs": dialogs,
    "page_updates_after_answer": updates_after,
    "console_errors": console_errors,
}, indent=2))
