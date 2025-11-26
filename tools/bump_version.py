#!/usr/bin/env python3
"""Semantic version bumper for DomoticsCore components and root library.

Usage examples:

  python tools/bump_version.py DomoticsCore-MQTT minor
  python tools/bump_version.py MQTT patch
  python tools/bump_version.py root major

Behavior:
- Bumps the selected component's library.json version.
- Updates all `metadata.version = "X.Y.Z";` assignments under that component's
  include/ and src/ directories to the new version.
- Independently bumps the root DomoticsCore version in the top-level
  library.json by the same level (patch/minor/major) whenever the
  target is a sub-component.
- If the target is the root itself, only the root version is bumped.

The script is intentionally conservative and only uses the standard library.
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Tuple


REPO_ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bump DomoticsCore component and root versions")
    parser.add_argument(
        "target",
        help=(
            "Component to bump: 'root', 'DomoticsCore', a full component name like "
            "'DomoticsCore-MQTT', or a short name like 'MQTT'"
        ),
    )
    parser.add_argument(
        "level",
        choices=["patch", "minor", "major"],
        help="Semantic version bump level",
    )
    parser.add_argument(
        "--repo-root",
        type=str,
        default=str(REPO_ROOT),
        help="Repository root (default: script parent)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show planned changes without modifying any files",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print extra diagnostic information",
    )
    return parser.parse_args()


def parse_semver(version: str) -> Tuple[int, int, int]:
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)$", version.strip())
    if not m:
        raise ValueError(f"Invalid semantic version: {version!r}")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def bump_semver(version: str, level: str) -> str:
    major, minor, patch = parse_semver(version)
    if level == "patch":
        patch += 1
    elif level == "minor":
        minor += 1
        patch = 0
    elif level == "major":
        major += 1
        minor = 0
        patch = 0
    else:
        raise ValueError(f"Unknown bump level: {level}")
    return f"{major}.{minor}.{patch}"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def resolve_target(root: Path, target: str) -> Tuple[Path, str, bool]:
    """Resolve target to a directory and name.

    Returns (directory, display_name, is_root_target).
    """

    t = target.strip()
    lower = t.lower()

    # Root aliases
    if lower in {"root", "domoticscore"}:
        return root, "DomoticsCore", True

    # Full directory name specified
    if t.startswith("DomoticsCore-"):
        comp_dir = root / t
        if not comp_dir.is_dir():
            raise SystemExit(f"Component directory not found: {comp_dir}")
        return comp_dir, t, False

    # Short name like "MQTT" -> DomoticsCore-MQTT
    short = t
    matches = []
    for d in root.iterdir():
        if not d.is_dir():
            continue
        name = d.name
        if not name.lower().startswith("domoticscore-"):
            continue
        if name.lower() == f"domoticscore-{short.lower()}":
            matches.append(d)

    if not matches:
        raise SystemExit(f"Could not resolve component name '{target}' to a DomoticsCore-* directory")
    if len(matches) > 1:
        names = ", ".join(sorted(d.name for d in matches))
        raise SystemExit(f"Ambiguous component name '{target}', matches: {names}")

    comp_dir = matches[0]
    return comp_dir, comp_dir.name, False


def read_library_version(lib_json: Path) -> str:
    text = read_text(lib_json)
    m = re.search(r'"version"\s*:\s*"(\d+\.\d+\.\d+)"', text)
    if not m:
        raise SystemExit(f"Could not find semantic version in {lib_json}")
    return m.group(1)


def update_library_version(lib_json: Path, new_version: str, dry_run: bool, verbose: bool) -> None:
    text = read_text(lib_json)
    pattern = re.compile(r'"version"\s*:\s*"(\d+\.\d+\.\d+)"')
    replacement = f'"version": "{new_version}"'
    new_text, count = pattern.subn(replacement, text, count=1)
    if count == 0:
        raise SystemExit(f"Failed to update version in {lib_json}: pattern not found")
    if dry_run:
        if verbose:
            print(f"[dry-run] Would update {lib_json} to version {new_version}")
        return
    write_text(lib_json, new_text)
    if verbose:
        print(f"Updated {lib_json} to version {new_version}")


def iter_source_files(component_dir: Path):
    exts = {".h", ".hpp", ".hh", ".hxx", ".cpp", ".cxx", ".cc", ".ino"}
    for sub in ("include", "src"):
        base = component_dir / sub
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() not in exts:
                continue
            lower_parts = {p.lower() for p in path.parts}
            if {".pio", "tests", "test", "examples"} & lower_parts:
                continue
            yield path


def update_metadata_versions(component_dir: Path, new_version: str, dry_run: bool, verbose: bool) -> None:
    pattern = re.compile(r'(metadata\.version\s*=\s*")(.*?)(")')
    any_changes = False

    for src in iter_source_files(component_dir):
        text = read_text(src)
        new_text, count = pattern.subn(rf'\1{new_version}\3', text)
        if count > 0:
            any_changes = True
            if dry_run:
                if verbose:
                    print(f"[dry-run] Would update metadata.version in {src} ({count} occurrence(s))")
            else:
                write_text(src, new_text)
                if verbose:
                    print(f"Updated metadata.version in {src} ({count} occurrence(s))")

    if verbose and not any_changes:
        print(f"[info] No metadata.version assignments found under {component_dir} (this may be normal for orchestrator packages)")


def main() -> None:
    args = parse_args()
    root = Path(args.repo_root).resolve()
    if not root.is_dir():
        raise SystemExit(f"Repository root does not exist: {root}")

    target_dir, display_name, is_root_target = resolve_target(root, args.target)

    root_lib = root / "library.json"
    if not root_lib.is_file():
        raise SystemExit(f"Root library.json not found at {root_lib}")

    root_version = read_library_version(root_lib)

    if is_root_target:
        new_root_version = bump_semver(root_version, args.level)
        print(f"Root DomoticsCore: {root_version} -> {new_root_version} [{args.level}]")
        update_library_version(root_lib, new_root_version, args.dry_run, args.verbose)
        return

    # Component bump + root propagation
    lib_json = target_dir / "library.json"
    if not lib_json.is_file():
        raise SystemExit(f"library.json not found for component {display_name} at {lib_json}")

    comp_version = read_library_version(lib_json)
    new_comp_version = bump_semver(comp_version, args.level)
    new_root_version = bump_semver(root_version, args.level)

    print(f"Component {display_name}: {comp_version} -> {new_comp_version} [{args.level}]")
    print(f"Root DomoticsCore: {root_version} -> {new_root_version} [{args.level}] (propagated)")

    # Apply updates
    update_library_version(lib_json, new_comp_version, args.dry_run, args.verbose)
    update_metadata_versions(target_dir, new_comp_version, args.dry_run, args.verbose)
    update_library_version(root_lib, new_root_version, args.dry_run, args.verbose)


if __name__ == "__main__":
    main()
