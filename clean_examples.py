#!/usr/bin/env python3

# This runs before any build steps
print("\n🔍 Running pre-dependency cleanup script...")

import os
import shutil
from pathlib import Path

# Determine repository root in a way compatible with PlatformIO pre-scripts
root_dir = None
try:
    # PlatformIO provides the build environment via SCons
    Import('env')  # type: ignore  # Provided by PlatformIO
    root_dir = Path(env['PROJECT_DIR']).resolve()  # type: ignore
except Exception:
    # Fallback: use current working directory
    try:
        root_dir = Path(os.getcwd()).resolve()
    except Exception:
        # Last resort: use relative path
        root_dir = Path('.')
deleted_count = 0

print(f"🔍 Searching for all .pio directories under: {root_dir}")

# Find every directory named '.pio' anywhere under the repo root
pio_dirs = [p for p in root_dir.rglob('.pio') if p.is_dir()]

# Do NOT delete the current project's working .pio during this build
current_project_dir = None
try:
    current_project_dir = Path(env['PROJECT_DIR']).resolve()  # type: ignore
except Exception:
    current_project_dir = None

def is_within(child: Path, parent: Path) -> bool:
    try:
        child.resolve()
        parent.resolve()
    except Exception:
        return False
    return parent == child or parent in child.parents

if current_project_dir is not None:
    current_pio = current_project_dir / '.pio'
    filtered = []
    for d in pio_dirs:
        if is_within(d, current_pio):
            print(f"⏭️  Skipping current build directory: {d.relative_to(root_dir)}")
            continue
        filtered.append(d)
    pio_dirs = filtered

    # However, we do want to clear dependency cache for the current project
    current_libdeps = current_pio / 'libdeps'
    if current_libdeps.exists():
        try:
            print(f"🧹 Removing current project's libdeps: {current_libdeps.relative_to(root_dir)}")
            shutil.rmtree(current_libdeps, ignore_errors=True)
        except Exception as e:
            print(f"⚠️  Error cleaning current libdeps {current_libdeps}: {e}")

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
