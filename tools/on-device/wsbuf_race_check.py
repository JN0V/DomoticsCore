#!/usr/bin/env python3
"""Poll /api/ui/updates while an SSE client is attached, and flag any document
that the other task's build could have written over.

    wsbuf_race_check.py <ip> [seconds] [pollers]

The verdict rests on the _sse hint, which only the poll path writes: an SSE
event carrying it, or a poll answer without exactly one, is the other task's
build read back. Exit 1 on either, 2 when the run is too thin to say anything
(fewer than 100 polls or SSE events, or more than 1 % of polls failing).

Malformed SSE events and polls with fewer contexts than the first are printed
but do not decide: SSE events also interleave on the wire with no poller at
all (BUG-98), and a provider may legitimately drop a context. The vehicle that
makes the two builds meet is probes/webui-buffer-race; against a stock
firmware the SSE interval leaves the window almost always shut.
"""
import json, sys, threading, time, urllib.request, http.client

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.224"
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 120
POLLERS = int(sys.argv[3]) if len(sys.argv) > 3 else 3

stop = time.time() + DURATION
lock = threading.Lock()
stats = {"poll": 0, "poll_bad_json": 0, "poll_partial": 0, "poll_sse_hint": 0, "poll_err": 0,
         "sse": 0, "sse_bad_json": 0, "sse_poll_hint": 0}
samples = []
full_contexts = None


def flag(kind, body):
    with lock:
        stats[kind] += 1
        if len(samples) < 6:
            samples.append((kind, body[:160] + " ... " + body[-120:]))


def poller():
    while time.time() < stop:
        try:
            with urllib.request.urlopen(f"http://{HOST}/api/ui/updates", timeout=5) as r:
                body = r.read().decode("utf-8", "replace")
        except Exception:
            with lock:
                stats["poll_err"] += 1
            time.sleep(0.2)
            continue
        with lock:
            stats["poll"] += 1
        try:
            d = json.loads(body)
        except ValueError:
            flag("poll_bad_json", body)
            continue
        if len(d.get("contexts", {})) < full_contexts:
            flag("poll_partial", body)
        if body.count('"_sse"') != 1:
            flag("poll_sse_hint", body)


def sse():
    while time.time() < stop:
        try:
            c = http.client.HTTPConnection(HOST, 80, timeout=10)
            c.request("GET", "/api/ui/events", headers={"Accept": "text/event-stream"})
            r = c.getresponse()
            while time.time() < stop:
                line = r.fp.readline()
                if not line:
                    break
                line = line.decode("utf-8", "replace").rstrip("\r\n")
                if line.startswith("data:"):
                    payload = line[5:].strip()
                    with lock:
                        stats["sse"] += 1
                    try:
                        json.loads(payload)
                    except ValueError:
                        flag("sse_bad_json", payload)
                        continue
                    if '"_sse"' in payload:
                        flag("sse_poll_hint", payload)
            c.close()
        except Exception:
            time.sleep(0.5)


with urllib.request.urlopen(f"http://{HOST}/api/ui/updates", timeout=5) as r:
    full_contexts = len(json.loads(r.read())["contexts"])
print(f"baseline: {full_contexts} contexts; {POLLERS} pollers + 1 SSE client for {DURATION:.0f}s")

threads = [threading.Thread(target=sse, daemon=True)] + \
          [threading.Thread(target=poller, daemon=True) for _ in range(POLLERS)]
for t in threads:
    t.start()
for t in threads:
    t.join(DURATION + 15)

print(stats)
for k, s in samples:
    print(k, "|", s)
if stats["sse"] < 100 or (POLLERS and stats["poll"] < 100) or stats["poll_err"] > stats["poll"] * 0.01:
    print("VACUOUS: too few events or too many failed polls to judge")
    sys.exit(2)
crossed = stats["sse_poll_hint"] + stats["poll_sse_hint"]
print("CROSSED" if crossed else "CLEAN", crossed)
sys.exit(1 if crossed else 0)
