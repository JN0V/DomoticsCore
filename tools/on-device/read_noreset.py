"""Read a port WITHOUT resetting the board: assert DTR and RTS together before
open, so the NodeMCU-style auto-reset circuit keeps EN high (both transistors off)."""
import sys, time, serial
port, seconds = sys.argv[1], float(sys.argv[2])
ser = serial.Serial(); ser.port = port; ser.baudrate = 115200; ser.timeout = 0.2
ser.dtr = True; ser.rts = True
ser.open()
deadline = time.time() + seconds
while time.time() < deadline:
    chunk = ser.read(4096)
    if chunk: sys.stdout.write(chunk.decode("utf-8", "replace")); sys.stdout.flush()
