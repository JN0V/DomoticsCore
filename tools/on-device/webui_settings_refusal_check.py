"""Type a value into a settings field with a real browser, and report what the
device stored and what the page said.

A native suite can assert the handler's answer. It cannot show whether the
refusal reaches the person typing: app.js turns an "error" key into a blocking
alert and leaves a text field alone otherwise, so "refused" and "silently
refused" look identical until a browser drives the page.

Reads the stored value back from /api/ui/updates rather than from the DOM, so a
field that merely redrew cannot be mistaken for one that never moved.

Drives a Settings card through its Edit/Save buttons, so it reaches only text
and number fields on a card that has them. The path BUG-51 calls invisible —
`alwaysInteractive`, and dashboard fields such as LED brightness — posts on
change with no Save button and is not covered here.

Exits non-zero when the measurement could not be taken, so a bench script can
tell "refused" from "never ran".

usage: webui_settings_refusal_check.py URL CONTEXT FIELD VALUE OUTDIR
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright

if len(sys.argv) != 6:
    sys.exit(__doc__)
url, context, field, value, outdir = sys.argv[1:6]
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
if before is None:
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

        page.goto(url, wait_until="networkidle", timeout=45000)
        page.click("[data-section='settings'], a[href='#settings']", timeout=10000)

        # The section renders asynchronously, so wait for the card before
        # counting it — a count taken on the empty section always reads zero.
        card = page.locator(f".card[data-context-id='{context}']")
        try:
            card.locator(".btn-edit").wait_for(state="attached", timeout=15000)
        except PlaywrightTimeoutError:
            sys.exit(f"{context} has no settings card with an Edit button — "
                     f"this probe drives Settings cards only")
        if card.count() != 1:
            sys.exit(f"{context} matches {card.count()} cards, expected one")
        # A freshly rendered field carries the schema's placeholder, not the
        # stored value, until the first SSE tick redraws it. Editing before that
        # makes a fill() of the stored value a no-op change, so nothing is ever
        # posted and the run measures nothing while looking like a clean result.
        try:
            page.wait_for_function(
                "([id, want]) => document.getElementById(id)?.value === String(want)",
                arg=[f"{context}_{field}", before], timeout=30000)
        except PlaywrightTimeoutError:
            sys.exit(f"{context}_{field} never showed the stored value {before!r} — "
                     f"text and number fields only")

        card.locator(".btn-edit").click()
        page.fill(f"#{context}_{field}", value)
        card.locator(".btn-save").click()

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
        page.screenshot(path=f"{outdir}/settled-{context}-{field}.png", full_page=True)
    finally:
        browser.close()

print(json.dumps({
    "field": f"{context}.{field}",
    "typed": value,
    "stored_before": before,
    "stored_after": after,
    "changed": before != after,
    "dialogs": dialogs,
    "console_errors": console_errors,
}, indent=2))
