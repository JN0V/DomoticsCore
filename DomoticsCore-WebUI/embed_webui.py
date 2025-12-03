import os
import gzip
import re
from pathlib import Path

Import("env")

# ============================================================================
# Minification functions (no external dependencies)
# ============================================================================

def minify_js(content: str) -> str:
    """
    Simple JS minification without external dependencies.
    Removes comments, unnecessary whitespace while preserving strings.
    """
    result = []
    i = 0
    in_string = None
    in_single_comment = False
    in_multi_comment = False
    
    while i < len(content):
        c = content[i]
        next_c = content[i + 1] if i + 1 < len(content) else ''
        
        # Handle string literals
        if in_string:
            result.append(c)
            if c == '\\':
                i += 1
                if i < len(content):
                    result.append(content[i])
            elif c == in_string:
                in_string = None
            i += 1
            continue
        
        # Handle comments
        if in_single_comment:
            if c == '\n':
                in_single_comment = False
                result.append('\n')
            i += 1
            continue
        
        if in_multi_comment:
            if c == '*' and next_c == '/':
                in_multi_comment = False
                i += 2
            else:
                i += 1
            continue
        
        # Detect comment start
        if c == '/' and next_c == '/':
            in_single_comment = True
            i += 2
            continue
        
        if c == '/' and next_c == '*':
            in_multi_comment = True
            i += 2
            continue
        
        # Detect string start
        if c in '"\'`':
            in_string = c
            result.append(c)
            i += 1
            continue
        
        # Collapse whitespace
        if c in ' \t':
            if result and result[-1] not in ' \t\n':
                result.append(' ')
            i += 1
            continue
        
        if c == '\n':
            if result and result[-1] not in '\n ':
                result.append('\n')
            i += 1
            continue
        
        result.append(c)
        i += 1
    
    text = ''.join(result)
    
    # Additional cleanup passes
    text = re.sub(r'\s*([{}\[\];:,<>=+\-*/%&|^!?()])\s*', r'\1', text)
    # Restore space after keywords
    text = re.sub(r'\b(return|typeof|new|delete|throw|in|of|const|let|var|if|else|for|while|do|switch|case|break|continue|function|class|extends|async|await|yield|import|export|from|default)\b([^\s\w])', r'\1 \2', text)
    text = re.sub(r'\n+', '\n', text)
    return text.strip()

def minify_css(content: str) -> str:
    """
    Simple CSS minification.
    """
    # Remove comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Remove whitespace around symbols
    content = re.sub(r'\s*([{};:,>~+])\s*', r'\1', content)
    # Collapse whitespace
    content = re.sub(r'\s+', ' ', content)
    # Remove space before {
    content = re.sub(r'\s*{\s*', '{', content)
    # Remove trailing semicolons before }
    content = re.sub(r';}', '}', content)
    # Remove newlines
    content = content.replace('\n', '')
    return content.strip()

def minify_html(content: str) -> str:
    """
    Simple HTML minification.
    """
    # Remove HTML comments (but not conditional comments)
    content = re.sub(r'<!--(?!\[).*?-->', '', content, flags=re.DOTALL)
    # Collapse whitespace between tags
    content = re.sub(r'>\s+<', '><', content)
    # Collapse multiple spaces
    content = re.sub(r'\s+', ' ', content)
    return content.strip()

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

def process_asset(filename: str, src_dir: Path) -> tuple:
    """
    Read, minify, and gzip an asset file.
    Returns (original_size, minified_size, gzipped_bytes)
    """
    path = src_dir / filename
    if not path.exists():
        raise RuntimeError(f"Missing asset: {path}")
    
    raw_content = path.read_text(encoding='utf-8')
    original_size = len(raw_content.encode('utf-8'))
    
    # Apply minification based on file type
    if filename.endswith('.js'):
        minified = minify_js(raw_content)
    elif filename.endswith('.css'):
        minified = minify_css(raw_content)
    elif filename.endswith('.html'):
        minified = minify_html(raw_content)
    else:
        minified = raw_content
    
    minified_bytes = minified.encode('utf-8')
    minified_size = len(minified_bytes)
    gzipped = gzip.compress(minified_bytes, compresslevel=9)
    
    return original_size, minified_size, gzipped

# Process all assets
processed_assets = {}
for filename, sym in assets:
    original_size, minified_size, gzipped = process_asset(filename, src_dir)
    processed_assets[filename] = (sym, original_size, minified_size, gzipped)
    reduction = 100 * (1 - minified_size / original_size) if original_size > 0 else 0
    print(f"[WebUI] {filename}: {original_size} -> {minified_size} bytes ({reduction:.1f}% minified) -> {len(gzipped)} bytes gzipped")

header_lines = [
    "#pragma once",
    "#include <Arduino.h>",
    "",
    "// Auto-generated by embed_webui.py. DO NOT EDIT MANUALLY.",
]

# Generate header declarations
for filename, sym in assets:
    sym, original_size, minified_size, gzipped = processed_assets[filename]
    header_lines.append("")
    header_lines.append(f"// {filename}: original {original_size} -> minified {minified_size} -> gzip {len(gzipped)} bytes")
    header_lines.append(f"extern const uint8_t {sym}[] PROGMEM;")
    header_lines.append(f"extern const size_t {sym}_LEN;")

# Append definitions
header_lines.append("")
header_lines.append("// Definitions")

for filename, sym in assets:
    sym, original_size, minified_size, gzipped = processed_assets[filename]
    header_lines.append("")
    header_lines.append(f"const uint8_t {sym}[] PROGMEM = {{")
    header_lines.append(f"        {to_c_array(gzipped)}")
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
