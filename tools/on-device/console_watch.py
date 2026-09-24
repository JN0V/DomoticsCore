#!/usr/bin/env python3
"""Watch a RemoteConsole over Telnet, optionally sending commands first.

    python3 tools/on-device/console_watch.py 192.168.1.224 --for 30
    python3 tools/on-device/console_watch.py 192.168.1.224 --send "core on" --for 30
    python3 tools/on-device/console_watch.py 192.168.1.224 --send "core off" --send clear \
        --reset-on "buffer cleared" --count PLATFORM --for 45

Prints every line the console streams, so a capture can be counted rather than
eyeballed: `--count PATTERN` reports how many lines matched, which is the figure
a red-then-green run is made of. Exit 0 when the connection stayed up.
"""
import argparse
import re
import socket
import sys
import time


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=23)
    ap.add_argument("--send", action="append", default=[], help="a command to send before watching")
    ap.add_argument("--for", dest="seconds", type=float, default=20.0)
    ap.add_argument("--count", action="append", default=[], help="regex to count in the stream")
    ap.add_argument("--show-ssid", action="store_true",
                    help="print the lines that carry the network name; they are dropped by default, "
                         "because these captures end up quoted in pull requests")
    ap.add_argument("--reset-on", dest="reset_on",
                    help="zero the counters when a line matches this regex — the console replays its "
                         "recent buffer to a new client, so counting from the connection counts history")
    args = ap.parse_args()

    counts = {pattern: 0 for pattern in args.count}
    deadline = time.time() + args.seconds
    with socket.create_connection((args.host, args.port), timeout=5) as sock:
        sock.settimeout(1.0)
        time.sleep(1.0)
        for command in args.send:
            sock.sendall((command + "\n").encode())
            time.sleep(1.0)
        buffered = b""
        while time.time() < deadline:
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                print("# connection closed by the device", file=sys.stderr)
                return 1
            buffered += chunk
            while b"\n" in buffered:
                raw, buffered = buffered.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").rstrip("\r")
                if not args.show_ssid and re.search(r"SSID|Connecting to Wifi", line):
                    continue
                print(line, flush=True)
                if args.reset_on and re.search(args.reset_on, line):
                    counts = {pattern: 0 for pattern in counts}
                    continue
                for pattern in counts:
                    if re.search(pattern, line):
                        counts[pattern] += 1

    for pattern, n in counts.items():
        print(f"# {pattern}: {n} lines", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
