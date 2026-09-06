#!/bin/bash
# Build, flash and capture one example on one board.
# usage: rung.sh <example-dir> <env> <port> <seconds>
set -u
SP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SP/../.." && pwd)"
DIR="$1"; ENV="$2"; PORT="$3"; SECS="${4:-30}"
NAME="$(basename "$DIR")-$ENV"
PIO=$(command -v pio || echo "uvx --from platformio pio")

# Put the repository root on the include path so every example can pick up the
# untracked secrets.h through __has_include. PLATFORMIO_BUILD_FLAGS appends to
# each project's own build_flags rather than replacing them, so no tracked
# platformio.ini needs to change.
# OBS-3: the build id the flight recorder stamps into its record, so a record
# can be matched to the ELF that produced it. The quotes must reach the
# compiler: -DDOMOTICS_BUILD_ID='"<id>"'.
BUILD_ID=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)
export PLATFORMIO_BUILD_FLAGS="-I$ROOT -DDOMOTICS_BUILD_ID='\"$BUILD_ID\"'"

cd "$ROOT/$DIR" || exit 1
BUILD=$(timeout 420 $PIO run -e "$ENV" -t upload --upload-port "$PORT" 2>&1)
if ! echo "$BUILD" | grep -q "SUCCESS"; then
    echo "### $NAME: BUILD/UPLOAD FAILED"
    echo "$BUILD" | grep -aE "error|Error|FAILED" | head -8
    exit 1
fi
echo "### $NAME"
echo "$BUILD" | grep -aE "^RAM:|^Flash:" | sed 's/^/    /'
timeout $((SECS + 40)) python3 "$SP/readserial.py" "$PORT" "$SECS" > "$SP/log-$NAME.txt" 2>&1
echo "    --- serial ($(wc -l < "$SP/log-$NAME.txt") lines) ---"
