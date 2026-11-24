#!/usr/bin/env python3
"""Fixture-tree tests for check_versions.py (Constitution XV)."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "check_versions.py"


def make_tree(root, root_v="1.2.0", comp_v="1.1.0", code_v="1.1.0", props_v="1.2.0"):
    (root / "library.json").write_text(json.dumps({"name": "R", "version": root_v}))
    (root / "library.properties").write_text(f"name=R\nversion={props_v}\n")
    c = root / "DomoticsCore-X"
    (c / "src").mkdir(parents=True)
    (c / "library.json").write_text(json.dumps({"name": "X", "version": comp_v}))
    (c / "src" / "x.cpp").write_text(f'void f(){{ metadata.version = "{code_v}"; }}\n')


def run(root, *extra):
    return subprocess.run([sys.executable, str(SCRIPT), "--repo-root", str(root), *extra],
                          capture_output=True, text=True)


class CheckVersions(unittest.TestCase):
    def check(self, rule, extra=(), **kw):
        with tempfile.TemporaryDirectory() as d:
            make_tree(Path(d), **kw)
            r = run(d, *extra)
        if rule is None:
            self.assertEqual(r.returncode, 0, r.stdout)
            self.assertIn("All version checks passed.", r.stdout)
        else:
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn(f"Constitution XV [{rule}]", r.stdout)

    def test_conforming(self):
        self.check(None)

    def test_component_vs_code(self):
        self.check("component-metadata", code_v="1.0.0")

    def test_root_vs_tag(self):
        self.check("root-tag", extra=("--tag", "v9.9.9"))

    def test_root_vs_components(self):
        self.check("root-vs-components", root_v="1.0.0", props_v="1.0.0")

    def test_properties_vs_root(self):
        self.check("properties-vs-root", props_v="1.0.0")


if __name__ == "__main__":
    unittest.main()
