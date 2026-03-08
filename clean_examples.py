#!/usr/bin/env python3

# PlatformIO pre-build script: clean .pio directories to prevent recursive
# nesting and ensure fresh DomoticsCore-* library resolution.
#
# PROBLEM: When PlatformIO resolves file:// dependencies (e.g., file://../DomoticsCore-Core),
# it copies the ENTIRE source directory — including any .pio/ subdirectory. If that .pio/
# contains libdeps with more DomoticsCore-* copies (which have their own .pio/), this creates
# an exponentially growing recursive nesting. We've observed 12,000+ nested .pio directories.
#
# SOLUTION: Before each build, remove .pio/ directories from:
#   1. Each DomoticsCore-* source dir at repo root (prevents them being copied into libdeps)
#   2. Each DomoticsCore-*/examples/*/ dir (prevents nesting from example builds)
#   3. Stale DomoticsCore-* libs in the current project's libdeps (forces re-resolution)
#
# IMPORTANT: This script does NOT use rglob('.pio') because that would find .pio directories
# inside the current project's own libdeps/build dirs, causing .sconsign312 corruption.
# It only cleans at known, fixed directory levels.
#
# IMPORTANT: Never build examples in parallel — each build's pre-script cleans other
# projects' .pio dirs, which would corrupt parallel builds.

import shutil
from pathlib import Path

try:
    Import('env')  # type: ignore  # Provided by PlatformIO
except Exception:
    raise SystemExit(0)

current_project_dir = Path(env['PROJECT_DIR']).resolve()  # type: ignore

# Find repo root by walking up
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

current_pio = (current_project_dir / '.pio').resolve()
deleted_count = 0

# 1. Clean .pio from DomoticsCore-* source dirs at repo root
#    and from their examples subdirectories (two fixed levels only, no rglob)
for comp_dir in repo_root.iterdir():
    if not comp_dir.is_dir() or not comp_dir.name.startswith('DomoticsCore-'):
        continue

    # Component-level .pio (from running component tests)
    # Skip if the current project is this component or a subdirectory of it
    try:
        current_project_dir.relative_to(comp_dir.resolve())
        # current_project_dir is inside comp_dir — skip to avoid corruption
        continue
    except ValueError:
        pass  # Not a subdirectory — safe to clean

    pio_dir = comp_dir / '.pio'
    if pio_dir.exists():
        shutil.rmtree(pio_dir, ignore_errors=True)
        deleted_count += 1

    # Example-level .pio dirs
    examples_dir = comp_dir / 'examples'
    if examples_dir.is_dir():
        for example_dir in examples_dir.iterdir():
            if not example_dir.is_dir():
                continue
            # Never touch the directory that is currently being built
            try:
                if example_dir.resolve() == current_project_dir:
                    continue
            except Exception:
                pass
            pio_dir = example_dir / '.pio'
            if pio_dir.exists():
                shutil.rmtree(pio_dir, ignore_errors=True)
                deleted_count += 1

# 2. Clean stale DomoticsCore-* libs from current project's libdeps
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
    except Exception:
        pass

if deleted_count > 0:
    print(f"  Cleaned {deleted_count} .pio director(ies) to prevent recursive nesting")
