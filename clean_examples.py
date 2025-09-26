#!/usr/bin/env python3

# This runs before any build steps
print("\n🔍 Running pre-dependency cleanup script...")

import os
import shutil
from pathlib import Path

"""Clean all PlatformIO build directories (.pio) across the repo, except the current project's .pio.
Also clean current project's libdeps if it contains DomoticsCore-* libs.

We must set root_dir to the repository root, not the current example's project dir,
otherwise we only ever see the current .pio and end up skipping everything.
"""

# Determine repository root in a way compatible with PlatformIO pre-scripts
root_dir = None
env_available = False
try:
    # PlatformIO provides the build environment via SCons
    Import('env')  # type: ignore  # Provided by PlatformIO
    env_available = True
    current_project_dir = Path(env['PROJECT_DIR']).resolve()  # type: ignore
    # Walk up until we find the repo root (folder containing multiple DomoticsCore-* packages)
    probe = current_project_dir
    max_up = 6
    for _ in range(max_up):
        # Heuristic: repo root has these subfolders
        if (probe / 'DomoticsCore-Core').exists() and (probe / 'DomoticsCore-WebUI').exists():
            root_dir = probe
            break
        if probe.parent == probe:
            break
        probe = probe.parent
    if root_dir is None:
        # Fallback to current project dir
        root_dir = current_project_dir
except Exception:
    # Fallback: use current working directory
    try:
        root_dir = Path(os.getcwd()).resolve()
        current_project_dir = None
    except Exception:
        # Last resort: use relative path
        root_dir = Path('.')
        current_project_dir = None
deleted_count = 0

print(f"🔍 Searching for all .pio directories under: {root_dir}")

# Find every directory named '.pio' anywhere under the repo root
pio_dirs = [p for p in root_dir.rglob('.pio') if p.is_dir()]

if current_project_dir is not None:
    # Do NOT delete the current project's working .pio during this build
    current_pio = (current_project_dir / '.pio').resolve()
    filtered = []
    for d in pio_dirs:
        try:
            if d.resolve() == current_pio:
                print(f"⏭️  Skipping current build directory: {d.relative_to(root_dir)}")
                continue
        except Exception:
            pass
        filtered.append(d)
    pio_dirs = filtered

    # Only clean libdeps containing DomoticsCore-*
    current_libdeps = current_pio / 'libdeps' / 'esp32dev'
    if current_libdeps.exists():
        try:
            has_domotics_libs = any('DomoticsCore-' in d.name for d in current_libdeps.iterdir() if d.is_dir())
            if has_domotics_libs:
                print(f"🧹 Removing current project's libdeps (contains DomoticsCore libraries): {current_libdeps.relative_to(root_dir)}")
                shutil.rmtree(current_libdeps, ignore_errors=True)
            else:
                print(f"ℹ️  Skipping libdeps (no DomoticsCore libraries found): {current_libdeps.relative_to(root_dir)}")
        except Exception as e:
            print(f"⚠️  Error checking/cleaning libdeps {current_libdeps}: {e}")

if not pio_dirs:
    print("ℹ️  No .pio directories found. Nothing to clean.")
else:
    print(f"📁 Found {len(pio_dirs)} .pio directorie(s) to remove:")
    for p in pio_dirs:
        print(f"  - {p.relative_to(root_dir)}")

    for pio_dir in pio_dirs:
        try:
            if 'node_modules' in str(pio_dir):
                # Safety: skip any accidental matches inside node_modules
                continue
            print(f"🧹 Removing: {pio_dir.relative_to(root_dir)}")
            shutil.rmtree(pio_dir, ignore_errors=True)
            deleted_count += 1
        except Exception as e:
            print(f"⚠️  Error cleaning {pio_dir}: {e}")

print(f"\n✅ Cleanup complete! Removed {deleted_count} .pio directorie(s)")
