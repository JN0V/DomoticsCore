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


def get_csrf_token(base: str, timeout: int = 10) -> str:
    # SEC-10: every state-changing route requires the per-boot token. This
    # script predates that lot and was silently un-runnable against a fixed
    # device until it learned to fetch one.
    with urllib.request.urlopen(f"{base}/api/ui/token", timeout=timeout) as resp:
        return json.loads(resp.read().decode())["token"]


def post_upload(base: str, payload: bytes, sha256: str, timeout: int,
                token: str | None = None):
    body = build_multipart("firmware", "firmware.bin", payload)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={BOUNDARY}",
        "X-Firmware-SHA256": sha256,
    }
    if token is not None:
        headers["X-DC-Token"] = token
    req = urllib.request.Request(
        f"{base}/api/ota/upload",
        data=body,
        method="POST",
        headers=headers,
    )
    envelope = len(body)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode(errors="replace"), envelope
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(errors="replace"), envelope


def disconnect_mid_upload(base: str, payload: bytes, sha256: str, token: str,
                          cut_at: int) -> None:
    """BUG-35: send the headers and cut_at bytes of the body, then close
    abruptly (SO_LINGER 0 -> RST, the shape of the accident that filed the
    bug; a FIN funnels into the same onDisconnect). The device must abort
    the open update, not entomb it."""
    import socket
    from urllib.parse import urlparse
    u = urlparse(base)
    body = build_multipart("firmware", "firmware.bin", payload)
    head = (
        f"POST /api/ota/upload HTTP/1.1\r\n"
        f"Host: {u.hostname}\r\n"
        f"Content-Type: multipart/form-data; boundary={BOUNDARY}\r\n"
        f"X-Firmware-SHA256: {sha256}\r\n"
        f"X-DC-Token: {token}\r\n"
        f"Content-Length: {len(body)}\r\n\r\n"
    ).encode()
    s = socket.create_connection((u.hostname, u.port or 80), timeout=20)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                 __import__("struct").pack("ii", 1, 0))
    s.sendall(head)
    sent = 0
    while sent < cut_at:
        n = s.send(body[sent:min(sent + 1024, cut_at)])
        if n == 0:
            break
        sent += n
        time.sleep(0.005)  # let the device start writing before the cut
    s.close()  # linger(1,0): RST
    print(f"    sent {sent} of {len(body)} body bytes, then RST")


class UploadClosed(OSError):
    """The device ended the connection. `phase` says when relative to the
    silence — 'connect', 'before', 'during', 'after' — and `elapsed` is the
    time from the start of the silence to the moment the close was seen
    (None when it happened before the silence began)."""
    def __init__(self, phase: str, elapsed, cause: BaseException):
        super().__init__(f"{type(cause).__name__}: {cause}")
        self.phase = phase
        self.elapsed = elapsed


def _sndq_state(local_port: int):
    """Linux `ss` view of the client socket: 'drained' when it holds nothing
    unsent or unacked, 'pending' when it does, 'gone' when `ss` no longer
    lists it — a vanished socket must not read as a drained one, or the pause
    runs against nothing. Without the drain a "pause" only empties the socket
    buffer and the device sees no silence at all — which is how the first
    attempt at this check passed against unfixed code."""
    import shutil
    import subprocess
    if not shutil.which("ss"):
        raise SystemExit("--silence needs `ss` (iproute2) to see the send queue; "
                         "without it the pause proves nothing")
    proc = subprocess.run(["ss", "-tin", f"sport = :{local_port}"],
                          capture_output=True, text=True)
    if proc.returncode != 0 or f":{local_port} " not in proc.stdout:
        return "gone"
    if "notsent:" in proc.stdout or "unacked:" in proc.stdout:
        return "pending"
    return "drained"


def upload_with_silence(base: str, payload: bytes, sha256: str, token: str,
                        silence_at: int, silence: float, timeout: int):
    """BUG-37: stream the body, go completely quiet for `silence` seconds at
    body offset `silence_at`, then finish. Returns (status, body_text) if the
    device answered; raises UploadClosed when it closed instead, saying in
    which phase and — when the close arrived during the silence — how many
    seconds in, which is the device's idle ceiling measured from outside.

    ESPAsyncWebServer arms a 3 s receive-idle timeout on every client and only
    clears it when a response starts, so it runs for the whole upload body. A
    client in TCP retransmission backoff is quiet for longer than that on an
    ordinary WiFi link; this reproduces the quiet on purpose, without needing
    the link to lose anything."""
    import select
    import socket
    from urllib.parse import urlparse
    u = urlparse(base)
    body = build_multipart("firmware", "firmware.bin", payload)
    overhead = len(build_multipart("firmware", "firmware.bin", b""))
    if silence_at >= len(body):
        raise SystemExit(f"--silence-at {silence_at} is past the end of a {len(body)}-byte "
                         f"body: the pause would never happen and the check would pass "
                         f"for nothing")
    if silence_at < overhead + 4096:
        raise SystemExit(f"--silence-at must be at least {overhead + 4096}: the device's "
                         f"index-0 upload handler, which is what widens the limit, has "
                         f"not run before the first body chunk has landed")
    head = (
        f"POST /api/ota/upload HTTP/1.1\r\n"
        f"Host: {u.hostname}\r\n"
        f"Content-Type: multipart/form-data; boundary={BOUNDARY}\r\n"
        f"X-Firmware-SHA256: {sha256}\r\n"
        f"X-DC-Token: {token}\r\n"
        f"Content-Length: {len(body)}\r\n"
        f"Connection: close\r\n\r\n"
    ).encode()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # A small send buffer, so that when this stops sending the wire goes quiet
    # within a window or two instead of tens of kilobytes later. Asked for
    # 4 KB; Linux doubles it to 8 KB. The drain wait below is what actually
    # guarantees the silence — this only shortens the wait.
    s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
    s.settimeout(timeout)
    try:
        s.connect((u.hostname, u.port or 80))
    except OSError as e:
        raise UploadClosed("connect", None, e)
    local_port = s.getsockname()[1]
    t0 = time.time()
    phase = "before"
    t_pause = None
    try:
        s.sendall(head)
        sent = 0
        while sent < len(body):
            if phase == "before" and sent >= silence_at:
                for _ in range(200):
                    state = _sndq_state(local_port)
                    if state != "pending":
                        break
                    time.sleep(0.05)
                if state == "gone":
                    raise SystemExit("the client socket is no longer listed by `ss`; "
                                     "nothing to be silent on")
                if state != "drained":
                    raise SystemExit("the send queue did not drain in 10 s; the device is "
                                     "not taking data, so a pause here is not a silence")
                print(f"    {time.time() - t0:6.2f}s  quiet for {silence:.1f}s at "
                      f"{sent} body bytes (send queue drained)")
                phase = "during"
                t_pause = time.time()
                # Watch the socket while quiet: a FIN or RST from the device
                # makes it readable, and the moment it arrives is the
                # device's idle ceiling, measured.
                while True:
                    left = silence - (time.time() - t_pause)
                    if left <= 0:
                        break
                    readable, _, _ = select.select([s], [], [], left)
                    if readable:
                        try:
                            peek = s.recv(1, socket.MSG_PEEK)
                        except OSError as e:
                            raise UploadClosed("during", time.time() - t_pause, e)
                        if not peek:
                            raise UploadClosed("during", time.time() - t_pause,
                                               ConnectionError("EOF from the device"))
                        break  # the device is talking: let the reader below see it
                print(f"    {time.time() - t0:6.2f}s  resuming")
                phase = "after"
            n = s.send(body[sent:sent + 1460])
            if n == 0:
                raise ConnectionError("send returned 0")
            sent += n
        print(f"    {time.time() - t0:6.2f}s  body complete, waiting for the answer")
        raw = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            raw += chunk
    except TimeoutError:
        raise
    except UploadClosed:
        raise
    except OSError as e:
        raise UploadClosed(phase, None if t_pause is None else time.time() - t_pause, e)
    finally:
        s.close()
    if not raw:
        raise UploadClosed(phase, None if t_pause is None else time.time() - t_pause,
                           ConnectionError("EOF with no response"))
    text = raw.decode(errors="replace")
    status_line = text.split("\r\n", 1)[0]
    parts = status_line.split()
    status = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
    return status, text.rsplit("\r\n\r\n", 1)[-1]


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
    ap.add_argument("--no-token", action="store_true",
                    help="send without the SEC-10 CSRF token: the expected result is a 403 "
                         "before anything OTA-shaped runs, and that IS the check")
    ap.add_argument("--disconnect-at", type=int, metavar="N",
                    help="BUG-35: send N body bytes then RST; the device must end in "
                         "state=error and accept a follow-up upload. total will read the "
                         "ENVELOPE size on an abort, by design: the SEC-9 narrowing runs "
                         "at finalize, which an abort never reaches")
    ap.add_argument("--silence", type=float, metavar="SECONDS",
                    help="BUG-37: stream the body and go quiet for SECONDS once, at "
                         "--silence-at. The upload must survive: ESPAsyncWebServer's own "
                         "3 s idle limit used to close it. With --expect-abort the device "
                         "must instead close it and abort the update (the ceiling still "
                         "exists, BUG-35's lock stays shut)")
    ap.add_argument("--silence-at", type=int, default=200000, metavar="N",
                    help="body offset for --silence (default 200000)")
    ap.add_argument("--expect-abort", action="store_true",
                    help="with --silence: the device is expected to drop the client at "
                         "--idle-timeout seconds into the silence, measured from here")
    ap.add_argument("--idle-timeout", type=float, default=30.0, metavar="SECONDS",
                    help="the device's OTAConfig::uploadIdleTimeoutSec (default 30); "
                         "--expect-abort requires the close to land between half a second "
                         "under that and 2 s over it (the device's clock starts a little "
                         "before this one; AsyncTCP polls every 500 ms)")
    args = ap.parse_args()
    modes = [f for f in ("no_token", "disconnect_at", "silence", "commit")
             if getattr(args, f) is not None and getattr(args, f) is not False]
    if len(modes) > 1:
        ap.error(f"one mode at a time: {', '.join('--' + m.replace('_', '-') for m in modes)}")
    if args.silence is not None and args.silence <= 0:
        ap.error("--silence must be a positive number of seconds")
    if args.expect_abort and args.silence is None:
        ap.error("--expect-abort needs --silence")
    if args.expect_abort and args.silence < args.idle_timeout + 3:
        ap.error(f"--expect-abort needs --silence of at least {args.idle_timeout + 3:.0f}s "
                 f"to see a close at --idle-timeout {args.idle_timeout:.0f}s")

    base = args.url.rstrip("/")
    payload = open(args.firmware, "rb").read()
    digest = hashlib.sha256(payload).hexdigest()
    overhead = len(build_multipart("firmware", "firmware.bin", b""))

    print(f"firmware   : {args.firmware}  {len(payload)} bytes")
    print(f"sha256     : {digest}")
    print(f"envelope   : +{overhead} bytes of multipart framing")

    failures = []

    if args.no_token:
        # SEC-10's own check: the unauthenticated multipart install this route
        # allowed is what made it the campaign's CRITICAL. Without the token
        # the answer must be 403 and the OTA state must not so much as twitch.
        print("\n[0] no CSRF token — must be 403, OTA untouched")
        status, body, envelope = post_upload(base, payload, digest, args.timeout,
                                             token=None)
        print(f"    HTTP {status}: {body.strip()}")
        if status != 403:
            failures.append(f"tokenless upload was not refused with 403: HTTP {status}")
        if failures:
            print("\nFAIL"); [print(" -", f) for f in failures]
        else:
            print("\nPASS (tokenless refusal)")
        return 1 if failures else 0

    token = get_csrf_token(base)
    print(f"csrf token : {token}")

    if args.disconnect_at is not None:
        print(f"\n[D] disconnect after {args.disconnect_at} body bytes — the device "
              f"must abort, not entomb (BUG-35)")
        disconnect_mid_upload(base, payload, digest, token, args.disconnect_at)
        time.sleep(4)
        st = get_status(base)
        print(f"    /api/ota/status -> state={st.get('state')} "
              f"lastResult={st.get('lastResult')!r} total={st.get('total')}")
        if st.get("state") == "downloading":
            failures.append("update still open after the disconnect — the BUG-35 lock")
        print("\n[D2] follow-up upload, wrong digest (safe) — must be ACCEPTED into "
              "the pipeline, i.e. refused at the hash, not with 'already in progress'")
        status, body, envelope = post_upload(base, payload, "de" + digest[2:],
                                             args.timeout, token=token)
        print(f"    HTTP {status}: {body.strip()}")
        if "already in progress" in body:
            failures.append("follow-up upload refused 'already in progress' — lock persists")
        elif "SHA256 mismatch" not in body:
            failures.append(f"follow-up refused for an unexpected reason: {body.strip()!r}")
        if failures:
            print("\nFAIL"); [print(" -", f) for f in failures]
        else:
            print("\nPASS (disconnect aborted the update; pipeline free)")
        return 1 if failures else 0

    if args.silence is not None:
        wrong = "de" + digest[2:]
        want = "abort" if args.expect_abort else "survive"
        print(f"\n[S] {args.silence:.1f}s of silence at body byte {args.silence_at}, "
              f"wrong digest — the upload must {want} (BUG-37)")
        closed = None
        stalled = False
        status, body = 0, ""
        try:
            status, body = upload_with_silence(base, payload, wrong, token,
                                               args.silence_at, args.silence,
                                               args.timeout)
            print(f"    HTTP {status}: {body.strip()}")
        except TimeoutError:
            stalled = True
            failures.append(f"the device stalled without closing ({args.timeout}s): "
                            f"neither an answer nor a close — that is a hang, not a verdict")
        except UploadClosed as e:
            closed = e
            if e.phase == "connect":
                when = "at connect"
            elif e.phase == "before":
                when = "before the silence began"
            elif e.phase == "during":
                when = f"{e.elapsed:.1f}s into the silence"
            else:
                when = f"after the silence, on resume ({e.elapsed:.1f}s after it began)"
            print(f"    the device closed the connection {when}: {e}")
        time.sleep(4)
        try:
            st = get_status(base)
        except (OSError, urllib.error.URLError, ValueError) as e:
            st = {}
            failures.append(f"/api/ota/status unreadable after the run: {e}")
        print(f"    /api/ota/status -> state={st.get('state')} "
              f"lastResult={st.get('lastResult')!r} downloaded={st.get('downloaded')}")
        if closed is not None and closed.phase in ("connect", "before"):
            failures.append(f"the connection died {('at connect' if closed.phase == 'connect' else 'before the silence began')}: "
                            f"the check never ran, so this says nothing about the idle limit")
        elif args.expect_abort:
            if stalled:
                pass  # already a failure
            elif closed is None:
                failures.append(f"the upload survived {args.silence:.0f}s of silence: "
                                f"no idle ceiling, a vanished client would hold the update open")
            elif closed.phase != "during":
                failures.append("the device closed only once sending resumed, so the close "
                                "time could not be measured from here — cannot bound the ceiling")
            elif not (args.idle_timeout - 0.5 <= closed.elapsed <= args.idle_timeout + 2.0):
                # The device stamps its clock when it processed the last packet;
                # this side starts its own a little later, once `ss` has shown the
                # queue empty — so the close reads a tenth or two EARLY from here
                # (29.9 s for 30, 4.9 s for 5, measured on both boards).
                failures.append(f"the device closed {closed.elapsed:.1f}s into the silence; "
                                f"expected {args.idle_timeout - 0.5:.1f}–{args.idle_timeout + 2:.0f}s "
                                f"(uploadIdleTimeoutSec, seen from here, plus the 500 ms poll)")
            if closed is not None and st.get("lastResult") != "Client disconnected mid-upload":
                failures.append(f"the device closed but did not abort the update: "
                                f"lastResult={st.get('lastResult')!r} (BUG-35)")
        else:
            if closed is not None:
                at = f"{closed.elapsed:.1f}s" if closed.elapsed is not None else "?"
                failures.append(f"the device dropped the upload {at} into {args.silence:.0f}s "
                                f"of silence (BUG-37: a receive-idle limit below that is still "
                                f"in force)")
            elif not stalled and "SHA256 mismatch" not in body:
                failures.append(f"upload finished but was not refused at the hash: {body.strip()!r}")
        if failures:
            print("\nFAIL"); [print(" -", f) for f in failures]
        else:
            if args.expect_abort:
                print(f"\nPASS (the device closed {closed.elapsed:.1f}s into the silence and "
                      f"aborted the update)")
            else:
                print("\nPASS (the upload survived the silence)")
        return 1 if failures else 0

    # --- the load-bearing case ------------------------------------------------
    # A valid image, a digest that cannot match. Refused after the narrowing and
    # before the commit, so the board stays up and keeps its figures.
    wrong = "de" + digest[2:]
    print("\n[1] valid image, wrong digest — must be refused, nothing committed")
    status, body, envelope = post_upload(base, payload, wrong, args.timeout, token=token)
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
        status, body, envelope = post_upload(base, payload, digest, args.timeout, token=token)
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
