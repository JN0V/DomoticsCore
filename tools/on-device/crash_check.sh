#!/bin/bash
# OBS-3: kill a board one way, read what the next boot says, and check it.
#
#   tools/on-device/crash_check.sh <serial-port> <ip> <kind> [expected-substring]
#
# Holds the serial port open across the death (no reset pulse), sends
# `crash <kind>` over Telnet, waits for the device to rejoin, reads `bootdiag`
# and greps it for the expected substring — the default per kind is the
# value the campaign measured (docs/CODE-ROADMAP.md, OBS-3). The firmware must
# carry -DDOMOTICS_ENABLE_CRASH_COMMANDS=1 (EXTRA_BUILD_FLAGS for
# run-example.sh); no shipped build does. Exit 0 when the line is there.
#
#   for k in abort oom null swdt hwdt reboot; do
#       tools/on-device/crash_check.sh /dev/ttyUSB1 192.168.1.218 $k || break
#   done
set -u
PORT=$1; IP=$2; KIND=$3; EXPECT=${4:-}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT=${CRASH_CHECK_OUT:-/tmp/crash-check}; mkdir -p "$OUT"

# What each death must read as on the next boot. ESP32 has no crash callback:
# every panic is "unexpected reset" there, and `oom` reaches the null store.
# `nothrow` (OBS-4) is a survived failure: no reboot, the "this boot" line of
# the same run's bootdiag is the check.
DROP=1
case "$KIND" in nothrow|squeeze|release) DROP=0 ;; esac
if [ -z "$EXPECT" ]; then
  case "$KIND" in
    abort)   EXPECT="Last death: crash callback" ;;
    oom)     EXPECT="last failed alloc 1024 B" ;;
    nothrow) EXPECT="+1" ;;                      # one more than the run had before (delta, see below)
    squeeze|release) EXPECT="done: $KIND" ;;    # the console's reply; the heap line of bootdiag is the reading
    null)    EXPECT="callback: reason 2 exccause 29" ;;
    swdt)    EXPECT="callback: reason 3 exccause 4" ;;
    hwdt)    EXPECT="Last death: unexpected reset" ;;
    hang)    EXPECT="Last death: unexpected reset" ;;
    reboot|restart) EXPECT="Last death: none recorded" ;;
    *) echo "unknown kind: $KIND"; exit 2 ;;
  esac
fi
case "$KIND" in hang) WAIT=55 ;; hwdt) WAIT=20 ;; *) WAIT=15 ;; esac

LOG="$OUT/serial-$KIND.txt"
echo "### $KIND on $IP"
if [ "$KIND" = reboot ]; then CMD=reboot; else CMD="crash $KIND"; fi
if [ $DROP = 0 ]; then
  # The serial capture keeps the diag profile's ":oom(size)@file:line" line.
  timeout 30 python3 "$HERE/read_noreset.py" "$PORT" 12 > "$LOG" 2>&1 &
  RPID=$!
  before=$(python3 "$HERE/console_cmd.py" "$IP" bootdiag --timeout 6 2>/dev/null | grep -ao "this boot: failed allocs [0-9]*" | grep -o "[0-9]*$"); before=${before:-0}
  reply=$(python3 "$HERE/console_cmd.py" "$IP" "$CMD" --timeout 6 | tail -1); echo "$reply"
  sleep 2
  python3 "$HERE/console_cmd.py" "$IP" bootdiag --timeout 6 > "$OUT/bootdiag-$KIND.txt" 2>&1 || { echo "FAIL: no answer to bootdiag"; exit 1; }
  wait $RPID 2>/dev/null
  grep -aE "this boot|Last death|Heap at this boot" "$OUT/bootdiag-$KIND.txt" | head -3
  grep -a "oom(" "$LOG" | head -2
  if [ "$EXPECT" = "+1" ]; then
    after=$(grep -ao "this boot: failed allocs [0-9]*" "$OUT/bootdiag-$KIND.txt" | grep -o "[0-9]*$"); after=${after:-0}
    if [ "$after" = "$((before + 1))" ]; then echo "PASS: failed allocs $before -> $after"; exit 0; fi
    echo "FAIL: failed allocs $before -> $after, expected +1"; exit 1
  fi
  if echo "$reply" | grep -aq "$EXPECT"; then echo "PASS: console says '$EXPECT'"; exit 0; fi
  echo "FAIL: '$EXPECT' not in the reply"; exit 1
fi
timeout $((WAIT + 40)) python3 "$HERE/read_noreset.py" "$PORT" $((WAIT + 30)) > "$LOG" 2>&1 &
RPID=$!
sleep 1
python3 "$HERE/console_cmd.py" "$IP" "$CMD" --expect-drop --timeout 4 | tail -1

# Wait for the console to come back rather than for a fixed time.
deadline=$((SECONDS + WAIT + 30)); back=0
sleep 5
while [ $SECONDS -lt $deadline ]; do
  if python3 "$HERE/console_cmd.py" "$IP" bootdiag --timeout 6 > "$OUT/bootdiag-$KIND.txt" 2>&1; then back=1; break; fi
  sleep 3
done
kill $RPID 2>/dev/null; wait $RPID 2>/dev/null
if [ $back = 0 ]; then echo "FAIL: the device did not come back within $((WAIT + 30)) s"; exit 1; fi

grep -aE "Last death|callback:|failed alloc|ring:|phase .* =" "$OUT/bootdiag-$KIND.txt" | head -8
if grep -aq "$EXPECT" "$OUT/bootdiag-$KIND.txt"; then
  echo "PASS: bootdiag says '$EXPECT'"; exit 0
fi
echo "FAIL: '$EXPECT' not in bootdiag (serial in $LOG)"; exit 1
