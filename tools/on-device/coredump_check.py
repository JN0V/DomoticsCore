#!/usr/bin/env python3
"""Get an ESP32 core dump off the device, decode it, and erase it.

OBS-1's transport: `GET /api/system/coredump` streams the image the panic
left in the `coredump` partition, `POST /api/system/coredump/erase` clears it
under SEC-10's token; both behind `enableAuth`. This script downloads the
image beside the ELF it should be decoded against, runs `esp-coredump` on it
(via `uvx`, nothing to install), and — only with `--erase` — erases it and
reads the route again to see it gone.

    coredump_check.py http://192.168.1.224 --user admin --password secret \\
        --elf .pio/build/esp32dev/firmware.elf --out campaign/wroom

Negatives, each one a check in its own right:
    --no-auth    no credentials: 401 on GET; 403 on POST, since the token is checked first and cannot be minted without credentials
    --no-token   POST without the CSRF token: 403 before the erase runs
    --expect-none  the device must answer 404 (no dump, or not an ESP32)

The image starts with its own length word; the script compares that to the
Content-Length it received, which is the transport check the first campaign
runs before any ELF matches (Lot C's death was left by an image whose ELF
is gone).
"""

import argparse
import base64
import json
import os
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request


def _headers(user, password, token=None):
    h = {}
    if user is not None:
        h["Authorization"] = "Basic " + base64.b64encode(f"{user}:{password or ''}".encode()).decode()
    if token is not None:
        h["X-DC-Token"] = token
    return h


def request(url, method="GET", user=None, password=None, token=None, timeout=60):
    """(status, headers, body). A short body — the device ended a fixed-length
    response early, which is coreDumpRead()'s error path — comes back as the
    bytes received, so the length-word comparison below can report it."""
    import http.client
    req = urllib.request.Request(url, method=method, headers=_headers(user, password, token),
                                 data=b"" if method == "POST" else None)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            try:
                return resp.status, dict(resp.headers), resp.read()
            except http.client.IncompleteRead as e:
                return resp.status, dict(resp.headers), e.partial
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()
    except urllib.error.URLError as e:
        raise SystemExit(f"{url}: {e.reason}")


def get_csrf_token(base, user, password, timeout=10):
    code, _, body = request(f"{base}/api/ui/token", user=user, password=password, timeout=timeout)
    if code != 200:
        raise SystemExit(f"/api/ui/token -> {code}: cannot mint a token (auth?)")
    return json.loads(body.decode())["token"]


def find_gdb():
    """esp-coredump needs a target GDB; PlatformIO's toolchain ships one."""
    import glob
    # tool-xtensa-esp-elf-gdb first: the older toolchain-xtensa-esp32 GDB links
    # against Python 2.7 and does not start on a current host.
    # Install with: pio pkg install -g -t platformio/tool-xtensa-esp-elf-gdb
    for pattern in ("~/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32-elf-gdb",
                    "~/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp-elf-gdb",
                    "~/.platformio/packages/toolchain-xtensa-esp32*/bin/xtensa-esp32-elf-gdb"):
        hits = glob.glob(os.path.expanduser(pattern))
        if hits:
            return hits[0]
    return None


def decode(core_path, elf, out_dir):
    cmd = ["uvx", "--from", "esp-coredump", "esp-coredump", "info_corefile",
           "--core", core_path, "--core-format", "raw"]
    gdb = find_gdb()
    if gdb:
        cmd += ["--gdb", gdb]
    cmd.append(elf)
    print("$ " + " ".join(cmd))
    try:
        res = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        print("uvx not found: the image is saved, not decoded")
        return 1
    report = os.path.join(out_dir, os.path.basename(core_path) + ".txt")
    with open(report, "w") as f:
        f.write(res.stdout)
        if res.stderr:
            f.write("\n--- stderr ---\n" + res.stderr)
    print(f"decode -> exit {res.returncode}, report {report}")
    for line in res.stdout.splitlines()[:40]:
        print("  " + line)
    return res.returncode


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url")
    ap.add_argument("--user", default=None)
    ap.add_argument("--password", default="")
    ap.add_argument("--elf", default=None, help="the ELF of the build that panicked; decoded when given")
    ap.add_argument("--out", default=".", help="where the image and the decode report land")
    ap.add_argument("--erase", action="store_true", help="erase after the download, then read the route again")
    ap.add_argument("--no-auth", action="store_true", help="negative: no credentials, expect 401 on GET and 403 on POST")
    ap.add_argument("--no-token", action="store_true", help="negative: POST without the token, expect 403")
    ap.add_argument("--expect-none", action="store_true", help="the device must answer 404: no dump waiting")
    ap.add_argument("--console", default=None, metavar="HOST",
                    help="also read `bootdiag` over the telnet console before and after the erase, "
                         "which is the reader the erase route refreshes")
    ap.add_argument("--timeout", type=int, default=60)
    args = ap.parse_args()
    base = args.url.rstrip("/")
    os.makedirs(args.out, exist_ok=True)

    if args.no_auth:
        # The erase checks the CSRF token before the credentials (the OTA upload's
        # order), and the token route itself needs credentials: a client without
        # them can only ever reach the 403, never the 401.
        g, _, _ = request(f"{base}/api/system/coredump", timeout=args.timeout)
        p, _, _ = request(f"{base}/api/system/coredump/erase", method="POST", timeout=args.timeout)
        ok = g == 401 and p == 403
        print(f"no credentials: GET -> {g}, POST erase -> {p}  {'ok' if ok else 'expected 401 and 403'}")
        return 0 if ok else 1

    if args.no_token:
        p, _, body = request(f"{base}/api/system/coredump/erase", method="POST",
                             user=args.user, password=args.password, timeout=args.timeout)
        ok = p == 403
        print(f"no token: POST erase -> {p} {body[:80]!r}  {'ok' if ok else 'expected 403 before any erase'}")
        return 0 if ok else 1

    code, headers, body = request(f"{base}/api/system/coredump", user=args.user,
                                  password=args.password, timeout=args.timeout)
    if args.expect_none:
        ok = code == 404
        print(f"GET /api/system/coredump -> {code} {body[:80]!r}  {'ok' if ok else 'expected 404'}")
        return 0 if ok else 1
    if code != 200:
        print(f"GET /api/system/coredump -> {code} {body[:120]!r}")
        return 1

    stamp = time.strftime("%Y%m%d-%H%M%S")
    core_path = os.path.join(args.out, f"coredump-{stamp}.bin")
    with open(core_path, "wb") as f:
        f.write(body)
    declared = headers.get("Content-Length")
    length_word = struct.unpack("<I", body[:4])[0] if len(body) >= 4 else 0
    print(f"GET /api/system/coredump -> 200, {len(body)} bytes received, Content-Length {declared}, "
          f"image length word {length_word}, Content-Disposition {headers.get('Content-Disposition')!r}")
    print(f"saved {core_path}")
    if length_word != len(body):
        print("MISMATCH: the image's own length word does not equal the bytes received")
    rc = 0
    if args.elf:
        rc = decode(core_path, args.elf, args.out)

    if args.erase:
        if length_word != len(body):
            print("refusing to erase: the image received is incomplete, and the device holds the only copy")
            return 1
        console_line = None
        if args.console:
            console_line = _bootdiag_coredump_line(args.console)
            print(f"bootdiag before: {console_line}")
        token = get_csrf_token(base, args.user, args.password)
        t0 = time.monotonic()
        p, _, pbody = request(f"{base}/api/system/coredump/erase", method="POST",
                              user=args.user, password=args.password, token=token, timeout=args.timeout)
        dt = time.monotonic() - t0
        print(f"POST erase -> {p} {pbody[:80]!r} in {dt:.2f} s")
        g, _, gbody = request(f"{base}/api/system/coredump", user=args.user,
                              password=args.password, timeout=args.timeout)
        print(f"GET after erase -> {g} {gbody[:80]!r}  {'ok' if g == 404 else 'expected 404'}")
        if p != 200 or g != 404:
            rc = 1
        if args.console:
            after = _bootdiag_coredump_line(args.console)
            ok = after is not None and "no dump waiting" in after
            print(f"bootdiag after:  {after}  {'ok' if ok else 'expected: partition present, no dump waiting'}")
            if not ok:
                rc = 1
    return rc


def _bootdiag_coredump_line(host):
    """The `Core dump:` line of `bootdiag`, through tools/on-device/console_cmd.py."""
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        res = subprocess.run([sys.executable, os.path.join(here, "console_cmd.py"), host, "bootdiag"],
                             capture_output=True, text=True, timeout=30)
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return f"(console read failed: {e})"
    lines = [l.strip() for l in res.stdout.splitlines()]
    for i, l in enumerate(lines):
        if l.startswith("Core dump:"):
            return l if "no dump" in l or "no coredump" in l else l + " " + (lines[i + 1] if i + 1 < len(lines) else "")
    return None


if __name__ == "__main__":
    sys.exit(main())
