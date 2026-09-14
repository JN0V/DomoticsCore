#!/usr/bin/env python3
"""Measure the RemoteConsole's auth wait from the outside.

Opens Telnet connections to a device whose console requires a password and
times each `auth` answer: wrong attempts back to back on one connection,
then a reconnection with the right password at once. The wait doubles per
consecutive failure (1, 2, 4, 8 s cap), is the address's — a reconnection
inherits it — and never refuses or disconnects: the answer just comes later.

    python3 tools/on-device/console_auth_check.py 192.168.1.218 --password probe

Prints one line per answer with the seconds since the line was sent.
"""
import argparse
import socket
import sys
import time


def recv_until(sock, needles, timeout):
    """Read until one of `needles` appears or `timeout` seconds pass. Returns (text, seconds)."""
    sock.settimeout(0.2)
    start = time.monotonic()
    buf = b""
    while time.monotonic() - start < timeout:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                return buf.decode(errors="replace"), time.monotonic() - start
            buf += chunk
            text = buf.decode(errors="replace")
            if any(n in text for n in needles):
                return text, time.monotonic() - start
        except socket.timeout:
            continue
    return buf.decode(errors="replace"), time.monotonic() - start


def connect(host, port):
    sock = socket.create_connection((host, port), timeout=5)
    welcome, _ = recv_until(sock, ["auth <password>", "help"], 3)
    return sock, welcome


def attempt(sock, password, label, wait_for=12):
    sent = time.monotonic()
    sock.sendall(f"auth {password}\n".encode())
    text, secs = recv_until(sock, ["Authentication successful", "Authentication failed", "not required"], wait_for)
    verdict = ("OK" if "successful" in text else "FAILED" if "failed" in text else "no answer")
    print(f"  {label:34s} -> {verdict:9s} after {secs:5.2f} s")
    return verdict, secs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=23)
    ap.add_argument("--password", required=True, help="the console's real password")
    ap.add_argument("--wrong", type=int, default=3, help="wrong attempts on the first connection")
    args = ap.parse_args()

    print(f"# {args.host}:{args.port}")
    sock, welcome = connect(args.host, args.port)
    if "auth <password>" not in welcome:
        print("the console did not ask for a password — is DC_CONSOLE_PASSWORD set on this build?")
        return 1
    print("connection 1: wrong attempts back to back")
    results = []
    for i in range(args.wrong):
        results.append(attempt(sock, f"nope{i}", f"wrong #{i + 1}"))
    still = True
    try:
        sock.sendall(b"heap\n")
        text, _ = recv_until(sock, ["Free Heap", "Authentication required"], 3)
        still = "Authentication required" in text or "Free Heap" in text
    except OSError:
        still = False
    print(f"  connection still open after the failures: {still}")
    sock.close()

    print("connection 2: same address, right password at once — inherits the wait the failures set")
    sock2, _ = connect(args.host, args.port)
    results.append(attempt(sock2, args.password, "right, fresh connection"))
    sock2.close()

    print("connection 3: after a success the address is clear")
    sock3, _ = connect(args.host, args.port)
    results.append(attempt(sock3, args.password, "right, another fresh connection"))
    sock3.close()

    ok = all(v != "no answer" for v, _ in results)
    print("# every attempt was answered — nothing refused, nothing cut" if ok else "# an attempt got no answer")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
