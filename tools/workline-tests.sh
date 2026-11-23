#!/usr/bin/env bash
# The project's tests for workline's tester and developer
# (.workline/config.yaml): every native PlatformIO suite of the components
# the given paths fall in, or of every component when none is given, in one
# JUnit report.
#
#   tools/workline-tests.sh <report> [path ...]
#
# Each component is a PlatformIO project of its own, so `pio test` runs once
# per component and this script merges their reports. A test case is named
# <component>/<suite> in its classname, and its file is given from the
# repository's root; compiler errors are rewritten so too, so that a build
# broken by a new test points at the test's file.
set -uo pipefail

report=${1:?usage: tools/workline-tests.sh <report> [path ...]}
shift
root=$(git rev-parse --show-toplevel)
cd "$root" || exit 1
case $report in /*) ;; *) report="$root/$report" ;; esac

# The PlatformIO projects holding a native environment: each component, and
# the few suites that are projects of their own, as CI's native job runs them.
all=()
for ini in $(git ls-files '*platformio.ini'); do
  grep -q '^\[env:native\]' "$ini" && all+=("$(dirname "$ini")")
done

# The projects the paths fall in, the innermost holding each; none named, or
# a path outside every project, runs them all.
picked=()
if [ $# -gt 0 ]; then
  for p in "$@"; do
    p=${p#./}; p=${p%/}
    found=""
    for c in "${all[@]}"; do
      case "$p/" in "$c"/*) [ ${#c} -gt ${#found} ] && found=$c ;; esac
    done
    if [ -z "$found" ]; then picked=(); break; fi
    case " ${picked[*]} " in *" $found "*) ;; *) picked+=("$found") ;; esac
  done
fi
[ ${#picked[@]} -gt 0 ] || picked=("${all[@]}")

parts=$(mktemp -d)
trap 'rm -rf "$parts"' EXIT
status=0
for c in "${picked[@]}"; do
  echo "== $c"
  (cd "$c" && pio test -e native --junit-output-path "$parts/${c//\//__}.xml") 2>&1 |
    sed -E "s#^((test|src|include|lib)/)#$c/\\1#"
  [ "${PIPESTATUS[0]}" -eq 0 ] || status=1
done

# The Python checker's fixture-tree tests, merged into the report as one suite.
echo "== tools (check_versions)"
python3 -m unittest discover -s tools -p 'test_check_versions.py' -v 2>&1 | tee "$parts/tools.log"
[ "${PIPESTATUS[0]}" -eq 0 ] || status=1

python3 - "$report" "$parts" <<'PY'
import os, sys, xml.etree.ElementTree as ET
report, parts = sys.argv[1], sys.argv[2]
out = ET.Element("testsuites")
for name in sorted(os.listdir(parts)):
    if not name.endswith(".xml"):
        continue
    comp = name[:-len(".xml")].replace("__", "/")
    for suite in ET.parse(os.path.join(parts, name)).getroot().iter("testsuite"):
        env, _, sname = suite.get("name", "").partition(":")
        if env != "native":
            continue
        suite.set("name", f"{comp}/{sname}")
        for case in suite.iter("testcase"):
            case.set("classname", f"{comp}/{sname}")
            if case.get("file"):
                case.set("file", f"{comp}/{case.get('file')}")
        out.append(suite)
import re
log = open(os.path.join(parts, "tools.log")).read()
m = re.search(r"^Ran (\d+) tests?", log, re.M)
fails = re.findall(r"^(?:FAIL|ERROR): (\S+) \(", log, re.M)
suite = ET.SubElement(out, "testsuite", name="tools/check_versions",
                      tests=str(int(m.group(1)) if m else 0), failures=str(len(fails)))
for t in re.findall(r"^(\S+) \(\S+\) \.\.\. ", log, re.M):
    case = ET.SubElement(suite, "testcase", classname="tools/check_versions", name=t)
    if t in fails:
        ET.SubElement(case, "failure", message="failed")
ET.ElementTree(out).write(report, encoding="utf-8", xml_declaration=True)
PY
exit $status
