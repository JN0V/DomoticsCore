#!/usr/bin/env python3
"""Drive a real POST /api/ota/upload against a board and check what it reports.

TEST-8, hole 4. Every OTA test in this repository calls beginUpload() directly.
Nothing constructs an HTTP request, nothing goes through OTAWithWebUI's handler,
and nothing has ever seen a multipart/form-data envelope — which is how SEC-9
stayed invisible to 54 native tests and 19 on-device ones until somebody joined a
LAN and uploaded a file by hand.

This is that person, automated.

## What it checks, and why those things

The load-bearing case is the **rejected** upload, not the accepted one. A valid
image sent with a deliberately wrong digest is refused at the hash check — which
sits *after* SEC-9's narrowing and *before* the commit — so the device stays up,
does not reboot, and /api/ota/status remains readable. The figures it reports
then are the whole point:

    without SEC-9   total = 475452   (the multipart envelope)
    with SEC-9      total = 475232   (the firmware)

That is a difference this script can see, on a device it has not disturbed. Run
it against a build with the fix reverted and the assertion fails. Nothing else
here has that property: an accepted upload commits and reboots, so its figures
are gone before they can be read, and a refusal on size alone never reaches the
narrowing at all.

## Usage

    python3 tools/on-device/ota_upload_check.py http://192.168.1.224 firmware.bin

Add --commit to also send a correctly-hashed copy, which the device will install
and reboot into. Uploading the image the board is already running makes that
safe and repeatable: it comes back as what it was.
"""

import argparse
import hashlib
import json
import sys
import time
import urllib.error
import urllib.request

BOUNDARY = "----DomoticsCoreOtaUploadCheck"


def build_multipart(field: str, filename: str, payload: bytes) -> bytes:
    """Encode payload the way a browser's file input does.

    The framing is the entire subject of SEC-9: Content-Length counts all of it,
    and the upload handler receives only `payload`. Written out rather than
    delegated so the overhead is visible and countable.
    """
    head = (
        f"--{BOUNDARY}\r\n"
        f'Content-Disposition: form-data; name="{field}"; filename="{filename}"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    tail = f"\r\n--{BOUNDARY}--\r\n".encode()
    return head + payload + tail


def post_upload(base: str, payload: bytes, sha256: str, timeout: int):
    body = build_multipart("firmware", "firmware.bin", payload)
    req = urllib.request.Request(
        f"{base}/api/ota/upload",
        data=body,
        method="POST",
        headers={
            "Content-Type": f"multipart/form-data; boundary={BOUNDARY}",
            "X-Firmware-SHA256": sha256,
        },
    )
    envelope = len(body)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode(errors="replace"), envelope
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(errors="replace"), envelope


def get_status(base: str, timeout: int = 10):
    with urllib.request.urlopen(f"{base}/api/ota/status", timeout=timeout) as resp:
        return json.loads(resp.read().decode())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="device base URL, e.g. http://192.168.1.224")
    ap.add_argument("firmware", help="path to a .bin to upload")
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--commit", action="store_true",
                    help="also send a correctly-hashed copy; the device installs it and reboots")
    args = ap.parse_args()

    base = args.url.rstrip("/")
    payload = open(args.firmware, "rb").read()
    digest = hashlib.sha256(payload).hexdigest()
    overhead = len(build_multipart("firmware", "firmware.bin", b""))

    print(f"firmware   : {args.firmware}  {len(payload)} bytes")
    print(f"sha256     : {digest}")
    print(f"envelope   : +{overhead} bytes of multipart framing")

    failures = []

    # --- the load-bearing case ------------------------------------------------
    # A valid image, a digest that cannot match. Refused after the narrowing and
    # before the commit, so the board stays up and keeps its figures.
    wrong = "de" + digest[2:]
    print("\n[1] valid image, wrong digest — must be refused, nothing committed")
    status, body, envelope = post_upload(base, payload, wrong, args.timeout)
    print(f"    announced Content-Length : {envelope}")
    print(f"    HTTP {status}: {body.strip()}")

    if "SHA256 mismatch" not in body:
        failures.append(f"refusal did not name the mismatch: {body.strip()!r}")

    time.sleep(2)
    st = get_status(base)
    print(f"    /api/ota/status -> state={st.get('state')} "
          f"downloaded={st.get('downloaded')} total={st.get('total')}")

    # SEC-9, end to end — and `total` is the only one of these two that
    # discriminates. Measured against a build with the narrowing removed:
    # total goes 475264 -> 475452, while downloaded stays 475264 either way,
    # because a refused digest never reaches finalizeUpdateOperation() and so
    # never hits the assignment that would have overwritten it. The second
    # assertion is a consistency check, not evidence; saying otherwise would be
    # the vacuity this whole item exists to stop.
    if st.get("total") != len(payload):
        failures.append(
            f"total is {st.get('total')}, expected {len(payload)} — "
            f"the device is reporting the multipart envelope, not the firmware "
            f"(SEC-9; the envelope here is {envelope})")
    if st.get("downloaded") != len(payload):
        failures.append(
            f"downloaded is {st.get('downloaded')}, expected {len(payload)} "
            f"(consistency check — this one does not move when SEC-9 is reverted)")

    # --- the destructive case, opt-in ----------------------------------------
    if args.commit:
        print("\n[2] valid image, correct digest — installs and reboots")
        status, body, envelope = post_upload(base, payload, digest, args.timeout)
        print(f"    HTTP {status}: {body.strip()}")
        if '"success":true' not in body.replace(" ", ""):
            failures.append(f"a correctly-hashed upload was not accepted: {body.strip()!r}")
        else:
            # Two phases, and the first one is the point. Waiting only for the
            # device to answer would pass just as well if it had never rebooted
            # at all — it answers throughout. Requiring it to go away first is
            # what separates "installed and restarted" from "said yes and sat
            # there". autoReboot fires 2 s after completion and the ESP8266 takes
            # a few more to boot, so the gap is seconds wide; polled fast enough
            # to see it.
            went_away = False
            deadline = time.time() + 30
            while time.time() < deadline:
                try:
                    get_status(base, timeout=1)
                except Exception:
                    went_away = True
                    print("    it stopped answering — rebooting")
                    break
                time.sleep(0.3)

            if not went_away:
                failures.append(
                    "the device never stopped answering: it accepted the image and did not "
                    "reboot into it, or rebooted faster than this could observe")

            came_back = False
            deadline = time.time() + 60
            while time.time() < deadline:
                try:
                    get_status(base, timeout=2)
                    came_back = True
                    print("    it answered again")
                    break
                except Exception:
                    time.sleep(1)
            if not came_back:
                failures.append("the device never came back after committing")

    print()
    if failures:
        for f in failures:
            print(f"FAIL: {f}")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
