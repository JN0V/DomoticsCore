"""Read a USB-Serial-JTAG console (/dev/ttyACM*) for N seconds, surviving the
re-enumeration that every chip reset causes: on any error, wait and reopen.
Does not touch DTR/RTS deliberately (no auto-reset circuit to drive)."""
import sys, time, serial
port, seconds = sys.argv[1], float(sys.argv[2])
deadline = time.time() + seconds
ser = None
while time.time() < deadline:
    if ser is None:
        try:
            ser = serial.Serial(port, 115200, timeout=0.2)
        except Exception:
            time.sleep(0.3)
            continue
    try:
        chunk = ser.read(4096)
        if chunk:
            sys.stdout.write(chunk.decode("utf-8", "replace")); sys.stdout.flush()
    except Exception:
        try: ser.close()
        except Exception: pass
        ser = None
        sys.stdout.write("\n[port dropped, reopening]\n"); sys.stdout.flush()
        time.sleep(0.3)
