"""Reset a board into its application and capture serial output.

pio device monitor needs a TTY, which a non-interactive agent does not have.

Reset lines on both FTDI adapters here: DTR drives GPIO0, RTS drives EN/RST.
For a normal application boot GPIO0 must be HIGH as reset rises — DTR False,
pulse RTS. The esptool sequence does the opposite and lands in the bootloader,
which is why `pio test -e esp32cam` cannot read that board.

usage: readserial.py PORT SECONDS [--quiet-stop N]
       --quiet-stop N ends the capture after N seconds with no new bytes.
"""
import sys
import time

import serial

port = sys.argv[1]
seconds = float(sys.argv[2])
quiet_stop = None
if "--quiet-stop" in sys.argv:
    quiet_stop = float(sys.argv[sys.argv.index("--quiet-stop") + 1])

with serial.Serial(port, 115200, timeout=0.2) as ser:
    ser.dtr = False          # GPIO0 high: boot the app, not the bootloader
    ser.rts = True           # hold in reset
    time.sleep(0.15)
    ser.rts = False          # release; GPIO0 still high
    ser.reset_input_buffer()

    deadline = time.time() + seconds
    last_byte = time.time()
    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            sys.stdout.write(chunk.decode("utf-8", "replace"))
            sys.stdout.flush()
            last_byte = time.time()
        elif quiet_stop and (time.time() - last_byte) > quiet_stop:
            break
