#!/usr/bin/env python3
"""Time how long a wrong WebUI password makes its address wait.

With `enableAuth` on, a wrong password makes the address wait before its next
attempt is read: 1 s after the first failure, doubling per consecutive failure
up to `authDelayMaxMs` (8 s by default). Inside the wait a request is answered
401 without its credentials being read, so the right password sent too early
is refused. The script enables authentication through `webui_settings`, reads
each case, and restores the previous auth state before it exits.

    webui_auth_delay_check.py http://device.local --user admin --password secret

Cases, each started from a clean address (a success clears it):

* the right password is read at once;
* after one wrong password, the right one is refused at +0.3 s and read at +1.2 s;
* after two, it is still refused at +1.5 s and read at +2.3 s;
* a request with no credentials is not a failure: the right password follows at once;
* an upload with the right password and a valid token, sent inside the wait,
  is refused 401 — the upload takes the same gate. Its body is not a firmware
  image, so a device *without* the wait opens an update and aborts it on the
  first chunk, and the serial line says `Upload` either way; read the port.

Latency on the bench LAN is tens of milliseconds, so the margins are wide.
A device without the wait reads every "refused" row as 200.
"""

import argparse
import base64
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


def request(url: str, method: str = "GET", body: bytes | None = None, headers: dict | None = None,
            user: str | None = None, password: str = "", timeout: float = 8.0):
    """Return (status, body text). Basic preemptively: the device accepts it."""
    hdrs = dict(headers or {})
    if user is not None:
        token = base64.b64encode(f"{user}:{password}".encode()).decode()
        hdrs["Authorization"] = f"Basic {token}"
    req = urllib.request.Request(url, data=body, headers=hdrs, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read(256).decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read(256).decode("utf-8", "replace")
    except Exception as e:  # URLError, timeouts
        return 0, str(e)


def action(base: str, token: str, field: str, value: str, **auth):
    """One webui_settings write. Parameters go in the query string for this route."""
    query = urllib.parse.urlencode({
        "token": token, "contextId": "webui_settings", "field": field, "value": value,
    })
    return request(f"{base}/api/ui/action?{query}", "POST", **auth)


def upload(base: str, token: str, **auth):
    """A multipart upload of 64 bytes that are not a firmware image."""
    boundary = "----dcAuthDelay"
    payload = bytes(range(64))
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; "
            f"filename=\"probe.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n").encode()
    body += payload + f"\r\n--{boundary}--\r\n".encode()
    headers = {"Content-Type": f"multipart/form-data; boundary={boundary}", "X-DC-Token": token}
    return request(f"{base}/api/ota/upload", "POST", body=body, headers=headers, **auth)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="device base URL, e.g. http://device.local")
    ap.add_argument("--user", default="admin")
    ap.add_argument("--password", default="bench-secret",
                    help="password to set and then authenticate with")
    ap.add_argument("--keep-auth", action="store_true",
                    help="leave authentication enabled instead of restoring it")
    ap.add_argument("--no-upload", action="store_true", help="skip the upload case")
    args = ap.parse_args()
    base = args.url.rstrip("/")
    good = {"user": args.user, "password": args.password}
    bad = {"user": args.user, "password": args.password + "-wrong"}
    info = f"{base}/api/system/info"

    status, body = request(f"{base}/api/ui/token")
    if status == 401:
        status, body = request(f"{base}/api/ui/token", **good)
    if status != 200:
        print(f"no CSRF token: /api/ui/token answered {status} {body}", file=sys.stderr)
        return 2
    token = json.loads(body).get("token", "")

    print("-- enabling authentication through webui_settings --")
    for field, value in (("username", args.user), ("password", args.password),
                         ("enable_auth", "true")):
        code, text = action(base, token, field, value)
        if code != 200:
            code, text = action(base, token, field, value, **good)
        shown = "(set)" if field == "password" else value
        print(f"  {field}={shown}: {code} {text.strip()[:60]}")
    gate = request(info)[0]
    print(f"  gate witness: {info} without credentials -> {gate} "
          f"({'auth is on' if gate == 401 else 'AUTH IS NOT ON — nothing below means anything'})")
    if gate != 401:
        return 2

    rows = []   # (label, expected, got)

    def check(label, expected, **auth):
        code = request(info, **auth)[0]
        rows.append((label, expected, code))

    def settle():
        time.sleep(9)                      # past any wait the previous case left
        request(info, **good)              # a success clears the address

    settle()
    check("right password, clean address", 200, **good)

    settle()
    check("one wrong password", 401, **bad)
    t0 = time.monotonic()
    time.sleep(0.3)
    check("  right at +0.3 s (inside 1 s)", 401, **good)
    time.sleep(max(0.0, 1.2 - (time.monotonic() - t0)))
    check("  right at +1.2 s (past 1 s)", 200, **good)

    settle()
    check("first wrong password", 401, **bad)
    time.sleep(1.2)
    check("second wrong password, read", 401, **bad)
    t0 = time.monotonic()
    time.sleep(1.5)
    check("  right at +1.5 s (inside 2 s)", 401, **good)
    time.sleep(max(0.0, 2.3 - (time.monotonic() - t0)))
    check("  right at +2.3 s (past 2 s)", 200, **good)

    settle()
    check("no credentials", 401)
    check("  right password at once", 200, **good)

    if not args.no_upload:
        settle()
        check("wrong password before the upload", 401, **bad)
        code = upload(base, token, **good)[0]
        rows.append(("  upload, right password, inside 1 s", 401, code))

    print()
    for label, expected, got in rows:
        mark = "ok " if expected == got else "BAD"
        print(f"{mark} {label:40s} expected {expected}, got {got}")

    if not args.keep_auth:
        time.sleep(9)
        code, text = action(base, token, "enable_auth", "false", **good)
        print(f"\nrestoring: enable_auth=false: {code} {text.strip()[:60]}")

    bad_rows = [r for r in rows if r[1] != r[2]]
    print(f"\nverdict: {len(rows) - len(bad_rows)}/{len(rows)} as expected")
    return 0 if not bad_rows else 1


if __name__ == "__main__":
    sys.exit(main())
