"""Drive a DomoticsCore WebUI with a real browser and report what actually happened.

Loading a page is not evidence. This records console errors, failed requests and
HTTP status per response, then asserts the page rendered something, so a blank
page served with 200 cannot pass.

usage: webui_check.py URL OUTDIR [--click-toggles]
"""
import sys, json, time
from playwright.sync_api import sync_playwright

url = sys.argv[1]
outdir = sys.argv[2]
click = "--click-toggles" in sys.argv

console_errors, page_errors, failed, responses = [], [], [], []

with sync_playwright() as p:
    browser = p.chromium.launch()
    ctx = browser.new_context(viewport={"width": 1280, "height": 900})
    page = ctx.new_page()

    page.on("console", lambda m: console_errors.append(f"{m.type}: {m.text}")
            if m.type in ("error", "warning") else None)
    page.on("pageerror", lambda e: page_errors.append(str(e)))
    page.on("requestfailed", lambda r: failed.append(f"{r.method} {r.url} :: {r.failure}"))
    page.on("response", lambda r: responses.append((r.status, r.url)))

    page.goto(url, wait_until="networkidle", timeout=45000)
    time.sleep(18)                     # SSE broadcasts every ~5.4 s; 3 s screenshotted before the first one
    page.screenshot(path=f"{outdir}/webui-initial.png", full_page=True)

    title = page.title()
    text = page.inner_text("body")
    controls = {
        "buttons":  page.locator("button").count(),
        "inputs":   page.locator("input").count(),
        "selects":  page.locator("select").count(),
        "cards":    page.locator("[class*=card], .card, section").count(),
    }

    clicked = None
    if click:
        # These are styled toggles: the real <input> is hidden behind a label,
        # so clicking the input itself times out. Drive the label a user would
        # actually hit, and fall back to the input with force only if there is
        # no label — never let the interaction failure lose the report.
        try:
            toggles = page.locator("input[type=checkbox]")
            if toggles.count():
                inp = toggles.first
                before = inp.is_checked()
                cid = inp.get_attribute("id")
                target, how = None, None
                if cid:
                    lab = page.locator(f'label[for="{cid}"]')
                    if lab.count() and lab.first.is_visible():
                        target, how = lab.first, "label[for]"
                if target is None:
                    par = inp.locator("xpath=..")
                    if par.is_visible():
                        target, how = par, "parent element"
                if target is not None:
                    target.click(timeout=10000)
                else:
                    inp.click(force=True, timeout=10000); how = "forced on hidden input"
                time.sleep(2)
                clicked = {"via": how, "before": before, "after": inp.is_checked()}
                page.screenshot(path=f"{outdir}/webui-after-click.png", full_page=True)
        except Exception as e:
            clicked = {"error": f"{type(e).__name__}: {str(e)[:200]}"}

    browser.close()

bad = [(s, u) for s, u in responses if s >= 400]
print(json.dumps({
    "title": title,
    "body_chars": len(text.strip()),
    "body_head": text.strip()[:300],
    "controls": controls,
    "responses": len(responses),
    "http_4xx_5xx": bad[:10],
    "all_responses": [f"{s} {u}" for s,u in responses][:20],
    "console_errors": console_errors[:10],
    "page_errors": page_errors[:10],
    "requests_failed": failed[:10],
    "toggle": clicked,
}, indent=2, ensure_ascii=False))
