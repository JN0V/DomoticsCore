#!/usr/bin/env python3
"""Send one RemoteConsole command over Telnet and print what came back.

    python3 tools/on-device/console_cmd.py 192.168.1.224 bootdiag
    python3 tools/on-device/console_cmd.py 192.168.1.224 "crash abort" --expect-drop

OBS-3's board checks: `crash <kind>` kills the device on purpose (the
connection drops, which --expect-drop treats as the pass), and `bootdiag` on
the next boot says what the flight recorder kept. Prints the reply verbatim;
exit 0 on a reply (or a drop when one was expected), 1 otherwise.
"""
import argparse
import socket
import sys
import time


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host")
    ap.add_argument("command")
    ap.add_argument("--port", type=int, default=23)
    ap.add_argument("--timeout", type=float, default=6.0)
    ap.add_argument("--expect-drop", action="store_true",
                    help="the command is meant to kill the device: pass when the connection drops")
    args = ap.parse_args()

    try:
        s = socket.create_connection((args.host, args.port), timeout=args.timeout)
    except OSError as e:
        print(f"connect failed: {e}")
        return 1
    s.settimeout(args.timeout)
    time.sleep(0.5)                       # the welcome banner
    try:
        s.recv(4096)
    except OSError:
        pass
    s.sendall((args.command + "\n").encode())
    reply = b""
    dropped = False
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            break
        except OSError:
            dropped = True
            break
        if not chunk:
            dropped = True
            break
        reply += chunk
    s.close()
    text = reply.decode("utf-8", "replace")
    if text.strip():
        print(text.rstrip())
    if args.expect_drop:
        if dropped or not text.strip():
            print("(connection dropped — the device is going down)")
            return 0
        print("FAIL: the device answered and stayed up")
        return 1
    return 0 if text.strip() else 1


if __name__ == "__main__":
    sys.exit(main())
