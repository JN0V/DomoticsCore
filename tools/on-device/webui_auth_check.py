#!/usr/bin/env python3
"""Read what the WebUI serves an unauthenticated client, route by route.

SEC-13: `/api/system/info` and the SSE stream `/api/ui/events` served an
unauthenticated client even with `enableAuth` on. This script reads each
route twice, without and with credentials, and says whether the codes are
the ones an auth-enabled device must give. It reads; it changes nothing.

The stream is only reached with `Accept: text/event-stream`: without that
header the handler does not match and the server answers 404 whatever the
auth state, which is why the bare `curl -N` reading of the route proves
nothing. Both readings are printed so nobody mistakes the 404 for a gate.

    webui_auth_check.py http://192.168.1.224 --user admin --password secret
    webui_auth_check.py http://192.168.1.224 --auth-off      # a device with enableAuth false

`--clients` prints the SSE client count `/api/system/info` reports, which is
how the browser side is checked without Playwright: open the page in a
browser, then read 1 (the stream is up) or 0 (app.js fell back to polling
without a word).
"""

import argparse
import base64
import socket
import sys
import urllib.error
import urllib.request


def get(url: str, headers: dict | None = None, user: str | None = None,
        password: str | None = None, timeout: float = 10.0, stream_bytes: int = 0):
    """Return (status, first bytes). Basic preemptively: the device accepts it beside Digest."""
    hdrs = dict(headers or {})
    if user is not None:
        token = base64.b64encode(f"{user}:{password or ''}".encode()).decode()
        hdrs["Authorization"] = f"Basic {token}"
    req = urllib.request.Request(url, headers=hdrs, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            if stream_bytes:
                data = b""
                try:
                    while len(data) < stream_bytes:
                        chunk = resp.read1(256) if hasattr(resp, "read1") else resp.read(256)
                        if not chunk:
                            break
                        data += chunk
                except (socket.timeout, TimeoutError):
                    pass
                return resp.status, data
            return resp.status, resp.read(256)
    except urllib.error.HTTPError as e:
        return e.code, e.read(256)
    except (urllib.error.URLError, socket.timeout, TimeoutError) as e:
        return 0, str(e).encode()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="device base URL, e.g. http://192.168.1.224")
    ap.add_argument("--user", default=None)
    ap.add_argument("--password", default="")
    ap.add_argument("--auth-off", action="store_true",
                    help="the device runs with enableAuth false: every route must answer 200 to nobody")
    ap.add_argument("--clients", action="store_true",
                    help="print the SSE client count from /api/system/info and exit")
    ap.add_argument("--timeout", type=float, default=8.0)
    args = ap.parse_args()
    base = args.url.rstrip("/")

    if args.clients:
        code, body = get(f"{base}/api/system/info", user=args.user, password=args.password,
                         timeout=args.timeout)
        print(f"/api/system/info -> {code} {body.decode(errors='replace')}")
        return 0 if code == 200 else 1

    sse = {"Accept": "text/event-stream"}
    # (label, path, headers, stream) — the page, whose gate reads the live
    # config since SEC-13, the two routes SEC-13 names, and two siblings already
    # gated, as the reference the fix must match.
    routes = [
        ("GET /  (the page)", "/", {}, 0),
        ("GET /api/system/info", "/api/system/info", {}, 0),
        ("GET /api/ui/events  (Accept: text/event-stream)", "/api/ui/events", sse, 64),
        ("GET /api/ui/events  (no Accept header)", "/api/ui/events", {}, 0),
        ("GET /api/ui/updates (already gated)", "/api/ui/updates", {}, 0),
        ("GET /api/components (already gated)", "/api/components", {}, 0),
    ]
    expect_anon = 200 if args.auth_off else 401
    failures = 0
    print(f"{'route':52} {'anon':>5} {'creds':>6}  verdict")
    for label, path, headers, stream in routes:
        anon, _ = get(f"{base}{path}", headers=headers, timeout=args.timeout, stream_bytes=stream)
        if args.user is not None:
            auth, body = get(f"{base}{path}", headers=headers, user=args.user,
                             password=args.password, timeout=args.timeout, stream_bytes=stream)
        else:
            auth, body = "-", b""
        if "no Accept" in label:
            verdict = "404 both ways, by design (the handler never matches)" if anon == 404 else f"unexpected {anon}"
            ok = anon == 404
        else:
            ok = anon == expect_anon and (args.user is None or auth == 200)
            verdict = "ok" if ok else f"expected anon={expect_anon}" + ("" if args.user is None else ", creds=200")
            if stream and args.user is not None and auth == 200:
                verdict += " (stream: %s)" % ("events flowing" if b"data:" in body or b"event:" in body
                                               else "open, no event within the timeout")
        failures += 0 if ok else 1
        print(f"{label:52} {anon!s:>5} {auth!s:>6}  {verdict}")
    print("PASS" if failures == 0 else f"FAIL ({failures} route(s))")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
