#!/usr/bin/env python3

# PlatformIO pre-build script: ensure fresh DomoticsCore-* library resolution
# and prevent recursive .pio nesting.
#
# PROBLEM 1 — STALE CACHES: When PlatformIO resolves file:// dependencies
# (e.g., file://../DomoticsCore-Core), it copies the source directory into
# .pio/libdeps/. These cached copies can become stale after code changes.
#
# PROBLEM 2 — RECURSIVE NESTING: If a DomoticsCore-* source dir has its own
# .pio/ when PIO copies it, that .pio/ gets copied too — and if THAT .pio/
# contains libdeps with more DomoticsCore-* copies, it creates exponentially
# growing recursive nesting. We've observed 12,000+ nested .pio directories.
#
# SOLUTION (two-part):
#   1. Delete .pio/ from each DomoticsCore-* SOURCE directory at repo root,
#      both at the component level AND inside their examples/ subdirs.
#      This prevents .pio/ from being included when PIO copies the source dir.
#      SAFETY: Never delete the current project's own .pio (current_pio).
#      This is the ONLY directory that would cause .sconsign312 corruption.
#   2. Delete cached DomoticsCore-* libraries from the CURRENT project's
#      .pio/libdeps/<env>/. This forces PIO to re-copy from source.
#
# IMPORTANT: Never use rglob('.pio') — that would find .pio directories inside
# the current project's own libdeps/build dirs, causing .sconsign312 corruption.
#
# IMPORTANT: Never build examples in parallel — sequential builds are required
# because part 1 cleans source-level .pio dirs that would conflict.

import shutil
from pathlib import Path

try:
    Import('env')  # type: ignore  # Provided by PlatformIO
except Exception:
    raise SystemExit(0)

current_project_dir = Path(env['PROJECT_DIR']).resolve()  # type: ignore
current_pio = (current_project_dir / '.pio').resolve()
deleted_count = 0

# Find repo root by walking up from current project
repo_root = None
probe = current_project_dir
for _ in range(6):
    if (probe / 'DomoticsCore-Core').exists() and (probe / 'DomoticsCore-WebUI').exists():
        repo_root = probe
        break
    if probe.parent == probe:
        break
    probe = probe.parent

if repo_root is None:
    repo_root = current_project_dir

# --- Part 1: Clean .pio from DomoticsCore-* source dirs at repo root ---
# This prevents .pio/ from being copied into libdeps when PIO resolves file:// deps.
# Cleans both component-level .pio AND example-level .pio within each component.
# The ONLY .pio we must never touch is current_pio (the active build's .pio).

def _safe_to_clean(pio_path: Path) -> bool:
    """Return True if this .pio dir is NOT the current project's own .pio."""
    try:
        return pio_path.resolve() != current_pio
    except Exception:
        return False  # If we can't resolve, don't risk it

for comp_dir in repo_root.iterdir():
    if not comp_dir.is_dir() or not comp_dir.name.startswith('DomoticsCore-'):
        continue

    # Component-level .pio
    pio_dir = comp_dir / '.pio'
    if pio_dir.exists() and _safe_to_clean(pio_dir):
        shutil.rmtree(pio_dir, ignore_errors=True)
        deleted_count += 1

    # Example-level .pio dirs (fixed depth: comp/examples/*/.pio)
    examples_dir = comp_dir / 'examples'
    if examples_dir.is_dir():
        for example_dir in examples_dir.iterdir():
            if not example_dir.is_dir():
                continue
            pio_dir = example_dir / '.pio'
            if pio_dir.exists() and _safe_to_clean(pio_dir):
                shutil.rmtree(pio_dir, ignore_errors=True)
                deleted_count += 1

# --- Part 2: Clean stale DomoticsCore-* libs from current project's libdeps ---
# This forces PlatformIO to re-copy them from source on every build.
try:
    env_name = env['PIOENV']  # type: ignore
except Exception:
    env_name = 'native'

current_libdeps = current_pio / 'libdeps' / env_name
if current_libdeps.exists():
    try:
        for lib_dir in current_libdeps.iterdir():
            if lib_dir.is_dir() and 'DomoticsCore-' in lib_dir.name:
                shutil.rmtree(lib_dir, ignore_errors=True)
                deleted_count += 1
    except Exception:
        pass

if deleted_count > 0:
    print(f"  Cleaned {deleted_count} .pio/lib dir(s) to prevent stale caches & recursive nesting")
