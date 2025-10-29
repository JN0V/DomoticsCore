import os
import gzip
from pathlib import Path

Import("env")

def find_webui_sources():
    """
    Find WebUI sources in both local development and PlatformIO installation.
    
    Detection logic:
    1. Local development: search for 'DomoticsCore' folder by walking up the tree
    2. PlatformIO installation: use __file__ to locate script in libdeps
    3. Fallback: search for webui_src/ relative to script
    """
    try:
        # Try using __file__ first (works in both local and PlatformIO)
        script_path = Path(__file__).resolve()
        
        # Case 1: Local development - search for 'DomoticsCore' folder by walking up
        current = script_path.parent
        while current.parent != current:
            if current.name == "DomoticsCore" and (current / "DomoticsCore-WebUI").exists():
                # Local dev: return DomoticsCore/DomoticsCore-WebUI/webui_src
                webui_src = current / "DomoticsCore-WebUI" / "webui_src"
                if webui_src.exists():
                    print(f"[WebUI] Found local dev sources: {webui_src}")
                    return webui_src
                break
            current = current.parent
        
        # Case 2: PlatformIO installation - script is in DomoticsCore-WebUI/ in libdeps
        # Script path: .pio/libdeps/esp32dev/DomoticsCore/DomoticsCore-WebUI/embed_webui.py
        webui_root = script_path.parent
        
        # Search for webui_src/ in DomoticsCore-WebUI/
        webui_src = webui_root / "webui_src"
        if webui_src.exists():
            print(f"[WebUI] Found PlatformIO sources: {webui_src}")
            return webui_src
        
        # Case 3: Fallback alternative - maybe installed differently
        # Try walking up to find DomoticsCore then descend
        for parent in script_path.parents:
            candidate = parent / "DomoticsCore-WebUI" / "webui_src"
            if candidate.exists():
                print(f"[WebUI] Found sources via parent search: {candidate}")
                return candidate
        
        # If nothing worked, detailed error
        raise RuntimeError(
            f"Could not locate WebUI sources.\n"
            f"Script location: {script_path}\n"
            f"Searched locations:\n"
            f"  - Local dev: {current / 'DomoticsCore-WebUI' / 'webui_src'}\n"
            f"  - PlatformIO: {webui_src}\n"
            f"Please ensure 'webui_src/' directory exists in DomoticsCore-WebUI package."
        )
        
    except NameError:
        # __file__ not available (very unusual SCons environment)
        raise RuntimeError(
            "Unable to resolve paths: __file__ is not available.\n"
            "This is an unusual SCons environment."
        )

# Locate WebUI sources
src_dir = find_webui_sources()

# Project dir for the example
example_dir = Path(env["PROJECT_DIR"])

# Determine output directory for generated assets
# Local dev: write to DomoticsCore-WebUI/include/DomoticsCore/Generated
# PlatformIO: write to libdeps
libdeps_dir = Path(env.get("PROJECT_LIBDEPS_DIR", ""))
pioenv = env.get("PIOENV", "")
out_header_src = None

if libdeps_dir and pioenv:
    # PlatformIO installation: write to libdeps
    candidates = [
        libdeps_dir / pioenv / "DomoticsCore" / "DomoticsCore-WebUI" / "include" / "DomoticsCore" / "Generated",
        libdeps_dir / pioenv / "DomoticsCore-WebUI" / "include" / "DomoticsCore" / "Generated",
    ]
    for out_dir in candidates:
        try:
            out_dir.mkdir(parents=True, exist_ok=True)
            out_header_src = out_dir / "WebUIAssets.h"
            print(f"[WebUI] Output directory: {out_dir}")
            break
        except Exception as e:
            continue

if out_header_src is None:
    # Local development: write next to sources
    package_root = src_dir.parent  # DomoticsCore-WebUI/
    out_dir_src = package_root / "include" / "DomoticsCore" / "Generated"
    out_dir_src.mkdir(parents=True, exist_ok=True)
    out_header_src = out_dir_src / "WebUIAssets.h"
    print(f"[WebUI] Output directory (local dev): {out_dir_src}")

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
out_header_src.write_text(out_text)
print(f"[WebUI] Successfully embedded assets -> {out_header_src}")
