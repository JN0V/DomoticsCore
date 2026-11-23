#!/usr/bin/env python3
"""Fail on any stack frame over a limit in the ESP8266 boot path (CI-18).

Reads the .su files `-fstack-usage` writes next to the objects, one line per
function: `file:line:col:name<TAB>bytes<TAB>qualifier`.

  tools/check_stack_usage.py [--limit BYTES] [--path SUBSTR ...] DIR [DIR ...]

Only functions whose source file path contains one of the --path substrings
count as boot path (default: the components' own sources, DomoticsCore-*/src).
Exit 1, naming each offending function, when a frame exceeds the limit.
"""
import argparse
import os
import re
import sys

# The ESP8266 cont stack is 4 KB. -fstack-usage measured logPromotedRecord at
# 1168 B; the limit leaves headroom above it. (Core::begin() once overflowed
# that stack on a real board, found only by running it; this check exists so
# a build finds such frames first.)
DEFAULT_LIMIT = 1280
DEFAULT_PATHS = ["DomoticsCore-"]


_HEAD = re.compile(r"^(.*?):\d+:\d+:(.+)$")


def parse_line(line):
    parts = line.rstrip("\n").split("\t")
    if len(parts) < 2:
        return None
    # file:line:col:name, where name may hold "::" (C++ scopes), so anchor on
    # the two numeric fields after the path instead of splitting on ":".
    m = _HEAD.match(parts[0])
    if not m:
        return None
    try:
        return m.group(1), m.group(2), int(parts[1])
    except ValueError:
        return None


def frames(dirs):
    for d in dirs:
        for root, _, files in os.walk(d):
            for f in files:
                if f.endswith(".su"):
                    with open(os.path.join(root, f), errors="replace") as fh:
                        for line in fh:
                            r = parse_line(line)
                            if r:
                                yield r


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--limit", type=int, default=DEFAULT_LIMIT)
    ap.add_argument("--path", action="append", default=None)
    ap.add_argument("dirs", nargs="+")
    a = ap.parse_args(argv)
    paths = a.path or DEFAULT_PATHS
    seen = 0
    bad = []
    for src, name, size in frames(a.dirs):
        if not any(p in src for p in paths):
            continue
        seen += 1
        if size > a.limit:
            bad.append((size, name, src))
    if seen == 0:
        print("error: no stack-usage frame found on the boot path; "
              "was the build run with -fstack-usage?", file=sys.stderr)
        return 1
    for size, name, src in sorted(bad, reverse=True):
        print(f"error: {name} uses {size} B of stack, over the {a.limit} B limit ({src})")
    if bad:
        return 1
    print(f"ok: {seen} boot-path frames, none over {a.limit} B")
    return 0


if __name__ == "__main__":
    sys.exit(main())
