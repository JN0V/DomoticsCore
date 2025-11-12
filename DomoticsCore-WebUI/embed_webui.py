import os
import gzip
from pathlib import Path

Import("env")

# Force C++14 for library compilation (required for std::make_unique)
# Must be applied early before any compilation
env.Append(CXXFLAGS=["-std=gnu++14"])
env.Append(CCFLAGS=["-std=gnu++14"])

# Also try to modify the global build environment
try:
    global_env = DefaultEnvironment()
    global_env.Append(CXXFLAGS=["-std=gnu++14"])
    global_env.Append(CCFLAGS=["-std=gnu++14"])
except:
    pass

print("[DomoticsCore] Forcing GNU C++14 standard for library compilation")

def find_webui_sources(env):
    """
    Find WebUI sources in both local development and PlatformIO installation.
    
    Detection logic:
    1. Local development: search for 'DomoticsCore' folder by walking up the tree
    2. PlatformIO installation: use libdeps directory from environment
    3. Fallback: search for webui_src/ relative to various known locations
    """
    # Try to get libdeps directory from environment (PlatformIO installation)
    libdeps_dir = Path(env.get("PROJECT_LIBDEPS_DIR", ""))
    pioenv = env.get("PIOENV", "")
    
    if libdeps_dir and pioenv:
        # Case 1: PlatformIO installation - look in libdeps
        candidates = [
            libdeps_dir / pioenv / "DomoticsCore" / "DomoticsCore-WebUI" / "webui_src",
            libdeps_dir / pioenv / "DomoticsCore-WebUI" / "webui_src",
        ]
        for candidate in candidates:
            if candidate.exists():
                print(f"[WebUI] Found PlatformIO sources: {candidate}")
                return candidate
    
    # Case 2: Try using __file__ if available (local development)
    try:
        script_path = Path(__file__).resolve()
        
        # Search for 'DomoticsCore' folder by walking up
        current = script_path.parent
        while current.parent != current:
            if current.name == "DomoticsCore" and (current / "DomoticsCore-WebUI").exists():
                webui_src = current / "DomoticsCore-WebUI" / "webui_src"
                if webui_src.exists():
                    print(f"[WebUI] Found local dev sources: {webui_src}")
                    return webui_src
                break
            current = current.parent
        
        # Maybe script is directly in DomoticsCore-WebUI/
        webui_src = script_path.parent / "webui_src"
        if webui_src.exists():
            print(f"[WebUI] Found sources relative to script: {webui_src}")
            return webui_src
            
    except NameError:
        # __file__ not available in SCons environment
        pass
    
    # Case 3: Try project dir (local development without __file__)
    project_dir = Path(env.get("PROJECT_DIR", ""))
    if project_dir:
        # Walk up from project to find DomoticsCore
        current = project_dir
        while current.parent != current:
            candidate = current / "DomoticsCore-WebUI" / "webui_src"
            if candidate.exists():
                print(f"[WebUI] Found sources via project search: {candidate}")
                return candidate
            current = current.parent
    
    # If nothing worked, detailed error
    raise RuntimeError(
        f"Could not locate WebUI sources.\n"
        f"Searched in:\n"
        f"  - PlatformIO libdeps: {libdeps_dir / pioenv if libdeps_dir and pioenv else 'N/A'}\n"
        f"  - Project directory: {project_dir if project_dir else 'N/A'}\n"
        f"Please ensure 'webui_src/' directory exists in DomoticsCore-WebUI package."
    )

# Locate WebUI sources
src_dir = find_webui_sources(env)

# Project dir for the example
example_dir = Path(env["PROJECT_DIR"])

# Determine output directories for generated assets
# Local dev: write to DomoticsCore-WebUI/include/DomoticsCore/Generated
# PlatformIO: write to ALL relevant libdeps instances to avoid header not found
libdeps_dir = Path(env.get("PROJECT_LIBDEPS_DIR", ""))
pioenv = env.get("PIOENV", "")
out_header_paths = []

if libdeps_dir and pioenv:
    # PlatformIO installation: consider both the direct lib and nested under the meta DomoticsCore lib
    pio_candidates = [
        libdeps_dir / pioenv / "DomoticsCore-WebUI" / "include" / "DomoticsCore" / "Generated",
        libdeps_dir / pioenv / "DomoticsCore" / "DomoticsCore-WebUI" / "include" / "DomoticsCore" / "Generated",
    ]
    for out_dir in pio_candidates:
        try:
            out_dir.mkdir(parents=True, exist_ok=True)
            out_header_paths.append(out_dir / "WebUIAssets.h")
            print(f"[WebUI] Output directory: {out_dir}")
        except Exception:
            pass

if not out_header_paths:
    # Local development: write next to sources
    package_root = src_dir.parent  # DomoticsCore-WebUI/
    out_dir_src = package_root / "include" / "DomoticsCore" / "Generated"
    out_dir_src.mkdir(parents=True, exist_ok=True)
    out_header_paths.append(out_dir_src / "WebUIAssets.h")
    print(f"[WebUI] Output directory (local dev): {out_dir_src}")
else:
    # Also add the local workspace include output to ensure workspace headers can include it
    try:
        package_root = src_dir.parent  # DomoticsCore-WebUI/
        out_dir_src = package_root / "include" / "DomoticsCore" / "Generated"
        out_dir_src.mkdir(parents=True, exist_ok=True)
        local_header = out_dir_src / "WebUIAssets.h"
        if local_header not in out_header_paths:
            out_header_paths.append(local_header)
            print(f"[WebUI] Output directory (workspace): {out_dir_src}")
    except Exception:
        pass

assets = [
    ("index.html", "WEBUI_HTML_GZ"),
    ("style.css", "WEBUI_CSS_GZ"),
    ("app.js", "WEBUI_JS_GZ"),
]

# Detect whether this example actually uses WebUI
def example_uses_webui() -> bool:
    src = example_dir / "src"
    if not src.exists():
        return False
    for p in src.rglob("*.*"):
        if p.suffix.lower() not in {".cpp", ".c", ".hpp", ".h", ".ino"}:
            continue
        try:
            text = p.read_text(errors="ignore")
        except Exception:
            continue
        # Support both include paths and class name usage
        if (
            "DomoticsCore/WebUI.h" in text
            or "WebUIComponent" in text
            or "DomoticsCore/System.h" in text  # System.h includes WebUI
        ):
            return True
    return False

if not example_uses_webui():
    print("Skipping WebUI asset embedding: example does not include WebUI")
    Return()

def to_c_array(data: bytes) -> str:
    # Format as comma-separated hex bytes, wrapped to reasonable line length
    hex_bytes = [f"0x{b:02x}" for b in data]
    lines = []
    line = []
    for i, hb in enumerate(hex_bytes):
        line.append(hb)
        if (i + 1) % 16 == 0:
            lines.append(", ".join(line))
            line = []
    if line:
        lines.append(", ".join(line))
    body = ",\n        ".join(lines)
    return body

header_lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "// Auto-generated by scripts/embed_webui.py. DO NOT EDIT MANUALLY.",
]

for filename, sym in assets:
    path = src_dir / filename
    if not path.exists():
        raise RuntimeError(f"Missing asset: {path}")
    raw = path.read_bytes()
    gz = gzip.compress(raw, compresslevel=9)
    header_lines.append("")
    header_lines.append(f"// {filename} (gzip), original {len(raw)} bytes, gzip {len(gz)} bytes")
    header_lines.append(f"extern const uint8_t {sym}[] PROGMEM;")
    header_lines.append(f"extern const size_t {sym}_LEN;")

# Write externs header first (declarations)
# We will also write definitions alongside (in the same header for simplicity)
# Some compilers allow this as 'inline' style for constants; for clarity we emit both declarations and definitions.

# Append a separator and actual definitions
header_lines.append("")
header_lines.append("// Definitions")

for filename, sym in assets:
    path = src_dir / filename
    raw = path.read_bytes()
    gz = gzip.compress(raw, compresslevel=9)
    header_lines.append("")
    header_lines.append(f"const uint8_t {sym}[] PROGMEM = {{")
    header_lines.append(f"        {to_c_array(gz)}")
    header_lines.append("};")
    header_lines.append(f"const size_t {sym}_LEN = sizeof({sym});")

out_text = "\n".join(header_lines) + "\n"
for header_path in out_header_paths:
    header_path.write_text(out_text)
    print(f"[WebUI] Successfully embedded assets -> {header_path}")

# Ensure compiler can find the generated header regardless of compilation unit by adding include roots
include_roots = set()
for header_path in out_header_paths:
    # header_path .../include/DomoticsCore/Generated/WebUIAssets.h -> include root is two parents up
    try:
        root = header_path.parent.parent  # .../include
        include_roots.add(str(root))
    except Exception:
        continue

for root in sorted(include_roots):
    try:
        env.Append(CPPPATH=[root])
        print(f"[WebUI] Added include path: {root}")
    except Exception:
        pass
