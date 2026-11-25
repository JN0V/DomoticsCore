#!/usr/bin/env python3
"""Build the copy of the library the Arduino IDE (and its registry) ships.

The Arduino 1.5 format compiles one `src/` at the library's root; this
repository keeps a modular layout (every header under
DomoticsCore-*/include/, every source under DomoticsCore-*/src/). The copy
flattens it:

    <out>/library.properties  (the root's, version unchanged)
    <out>/src/DomoticsCore.h  (umbrella header)
    <out>/src/DomoticsCore/** (every component's headers, so that
                               <DomoticsCore/X.h> and System.h's __has_include
                               guards resolve as in PlatformIO)
    <out>/src/DomoticsCore/Generated/WebUIAssets.h (embed_webui.py's output)
    <out>/src/*.cpp           (every component's sources)
    <out>/examples/           (the root's)

DocMainpage.h (Doxygen only, one per component, same name) is not copied.

Usage: tools/make_arduino_library.py <output folder>
"""
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_HEADERS = {"DocMainpage.h"}
UMBRELLA = """#pragma once
// Umbrella header of the Arduino copy of DomoticsCore: System.h pulls in every
// component present in src/ (see its __has_include guards).
#include <DomoticsCore/System.h>
"""


def generate_web_assets(dest: Path) -> None:
    """Run embed_webui.py outside PlatformIO and write WebUIAssets.h to dest."""
    webui = ROOT / "DomoticsCore-WebUI"
    with tempfile.TemporaryDirectory(prefix="arduino_webui_") as tmp_s:
        tmp = Path(tmp_s)
        pkg = tmp / "pkg" / "DomoticsCore-WebUI"
        shutil.copytree(webui / "webui_src", pkg / "webui_src")
        shutil.copy2(webui / "embed_webui.py", pkg / "embed_webui.py")
        proj = tmp / "proj"
        (proj / "src").mkdir(parents=True)
        (proj / "src" / "main.cpp").write_text("#include <DomoticsCore/System.h>\n")

        class Done(Exception):
            pass

        class Env(dict):
            def Append(self, **_kw):
                pass

        script = pkg / "embed_webui.py"

        def _return(*_a):
            raise Done()

        ns = {
            "__file__": str(script),
            "__name__": "embed_webui",
            "Import": lambda *_a: None,
            "Return": _return,
            "env": Env(PROJECT_DIR=str(proj)),
        }
        try:
            exec(compile(script.read_text(encoding="utf-8"), str(script), "exec"), ns)
        except Done:
            raise SystemExit("embed_webui.py skipped the embedding")
        generated = pkg / "include" / "DomoticsCore" / "Generated" / "WebUIAssets.h"
        if not generated.is_file():
            raise SystemExit("embed_webui.py wrote no WebUIAssets.h")
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copy2(generated, dest / "WebUIAssets.h")


def build(out: Path) -> None:
    if out.exists():
        shutil.rmtree(out)
    src = out / "src"
    src.mkdir(parents=True)

    for comp in sorted(ROOT.glob("DomoticsCore-*")):
        inc = comp / "include"
        if inc.is_dir():
            for f in sorted(inc.rglob("*")):
                rel = f.relative_to(inc)
                if not f.is_file() or "Generated" in rel.parts or f.name in SKIP_HEADERS:
                    continue
                target = src / rel
                if target.exists():
                    raise SystemExit(f"two components ship {rel}")
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, target)
        csrc = comp / "src"
        if csrc.is_dir():
            for f in sorted(csrc.rglob("*")):
                if not f.is_file():
                    continue
                target = src / f.relative_to(csrc)
                if target.exists():
                    raise SystemExit(f"two components ship src/{f.relative_to(csrc)}")
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, target)

    (src / "DomoticsCore.h").write_text(UMBRELLA, encoding="utf-8")
    generate_web_assets(src / "DomoticsCore" / "Generated")

    for name in ("library.properties", "LICENSE", "README.md"):
        if (ROOT / name).is_file():
            shutil.copy2(ROOT / name, out / name)
    for ex in ("CoreOnly", "Minimal"):
        if (ROOT / "examples" / ex).is_dir():
            shutil.copytree(ROOT / "examples" / ex, out / "examples" / ex)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    build(Path(sys.argv[1]).resolve())


if __name__ == "__main__":
    main()
