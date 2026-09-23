#!/usr/bin/env python3
"""Publish from the web server's task while the loop drains the EventBus.

Every WebUI settings write reaches `EventBus::publish()` from the async web
task, while `Core::loop()` pops from the same queue and the same
`pendingByTopic` map on the application task. The bus assumes one thread. This
script makes the two meet: several connections writing a settings field as fast
as the device answers, while the device does its ordinary work.

    eventbus_race_check.py http://192.168.1.224 --writes 400 --workers 4

The verdict is the device's own uptime, read every few writes: a reboot means
it died. Read the serial port at the same time for the panic and its backtrace
— the register dump is what names the queue, and `Core 0` versus `Core 1` is
what says which task was inside it (`loopTask` runs on core 1).

**It does not discriminate, and that is measured**: 303 writes against firmware
with the race wide open survived, because a settings write spends its time in
flash rather than in the queue. What reproduces the race on demand is
`probes/eventbus-race`, which removes the web server and keeps the two calls
that meet. Use this one for what it is: the whole stack, under a burst of real
requests, watched for a death and for heap drift.

The field written is the WebUI theme, alternating between two values it
accepts, and it is restored at the end. Each write persists, so this leaves
NVS wear behind: keep the count to what the question needs.
"""

import argparse
import json
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request


def get_json(url: str, timeout: float = 5.0):
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            return json.loads(resp.read(512).decode("utf-8", "replace"))
    except Exception:
        return None


def post(url: str, timeout: float = 8.0) -> int:
    req = urllib.request.Request(url, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="device base URL, e.g. http://192.168.1.224")
    ap.add_argument("--writes", type=int, default=400, help="settings writes in total")
    ap.add_argument("--workers", type=int, default=4, help="connections writing at once")
    ap.add_argument("--field", default="theme")
    ap.add_argument("--values", default="auto,dark", help="two values the field accepts")
    ap.add_argument("--context", default="webui_settings")
    args = ap.parse_args()
    base = args.url.rstrip("/")
    values = args.values.split(",")

    token_doc = get_json(f"{base}/api/ui/token")
    if not token_doc or "token" not in token_doc:
        print("no CSRF token: is the device up, and is auth off?", file=sys.stderr)
        return 2
    token = token_doc["token"]

    start = get_json(f"{base}/api/system/info")
    if not start:
        print("device did not answer /api/system/info", file=sys.stderr)
        return 2
    print(f"uptime at start: {start['uptime']} ms, heap {start['heap']}")

    counts = {"sent": 0, "ok": 0, "failed": 0}
    died_at = [None]
    lock = threading.Lock()
    stop = threading.Event()

    def write(index: int) -> None:
        query = urllib.parse.urlencode({
            "token": token, "contextId": args.context,
            "field": args.field, "value": values[index % len(values)],
        })
        code = post(f"{base}/api/ui/action?{query}")
        with lock:
            counts["sent"] += 1
            counts["ok" if code == 200 else "failed"] += 1

    def worker(worker_id: int) -> None:
        index = worker_id
        while not stop.is_set():
            with lock:
                if counts["sent"] >= args.writes:
                    return
            write(index)
            index += args.workers

    def watchdog() -> None:
        last = start["uptime"]
        while not stop.is_set():
            time.sleep(1.0)
            info = get_json(f"{base}/api/system/info", timeout=3.0)
            if info is None:
                continue
            if info["uptime"] < last:
                with lock:
                    died_at[0] = counts["sent"]
                stop.set()
                return
            last = info["uptime"]

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(args.workers)]
    watch = threading.Thread(target=watchdog, daemon=True)
    began = time.time()
    watch.start()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    stop.set()
    elapsed = time.time() - began

    # Restore the field, and read the uptime one last time: a device that died
    # on the last write of the run has not been seen by the watchdog yet.
    time.sleep(2.0)
    post(f"{base}/api/ui/action?" + urllib.parse.urlencode({
        "token": token, "contextId": args.context, "field": args.field, "value": values[0]}))
    end = get_json(f"{base}/api/system/info")

    print(f"{counts['sent']} writes in {elapsed:.1f} s "
          f"({counts['ok']} answered 200, {counts['failed']} did not)")
    if end:
        print(f"uptime at end: {end['uptime']} ms, heap {end['heap']}")
    if died_at[0] is not None:
        print(f"\nDIED: uptime went backwards after {died_at[0]} writes — read the serial")
        return 1
    if end and end["uptime"] < start["uptime"]:
        print("\nDIED: the device rebooted during the run — read the serial")
        return 1
    print("\nsurvived: no reboot seen")
    return 0


if __name__ == "__main__":
    sys.exit(main())
