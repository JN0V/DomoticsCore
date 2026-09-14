#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["paho-mqtt>=2.0"]
# ///
"""Watch what a device publishes, with the retained flag and the size.

OBS-5's transport check: subscribe to a device's topics on the bench broker
and print each message as it arrives, timestamped, with its size and whether
the broker served it as retained. The discovery configs Home Assistant reads
are on `homeassistant/#`; the device's own topics are `{clientId}/#`.

    uv run tools/on-device/mqtt_watch.py 192.168.1.253 --topic 'FullStackDevice/#' \\
        --topic 'homeassistant/#' --for 660 --log campaign/wroom/mqtt.log

`--sizes` prints one line per discovery config with its payload length, the
figure the 699-byte event cap and the ESP8266's 768-byte packet are measured
against. `--clear TOPIC` publishes an empty retained message to a topic and
exits: what a decommissioned device's retained crash record needs.
"""

import argparse
import sys
import time

import paho.mqtt.client as mqtt


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("broker")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--topic", action="append", default=[], help="subscription, repeatable (default: '#')")
    ap.add_argument("--for", dest="duration", type=float, default=60.0, help="seconds to watch")
    ap.add_argument("--log", default=None, help="append every line here as well")
    ap.add_argument("--sizes", action="store_true", help="discovery configs: topic and payload length only")
    ap.add_argument("--clear", default=None, metavar="TOPIC",
                    help="publish an empty retained message to TOPIC and exit")
    args = ap.parse_args()
    topics = args.topic or ["#"]
    log = open(args.log, "a") if args.log else None

    def out(line: str):
        print(line, flush=True)
        if log:
            log.write(line + "\n")
            log.flush()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    if args.clear:
        client.connect(args.broker, args.port, 30)
        client.loop_start()
        info = client.publish(args.clear, payload=None, qos=0, retain=True)
        info.wait_for_publish(5)
        client.loop_stop()
        out(f"cleared retained {args.clear}")
        return 0

    t0 = time.monotonic()
    counts = {"messages": 0, "retained": 0, "bytes": 0}

    def on_connect(c, userdata, flags, reason_code, properties):
        for t in topics:
            c.subscribe(t, qos=0)
        out(f"# connected to {args.broker}:{args.port}, subscribed to {topics}")

    def on_message(c, userdata, msg):
        counts["messages"] += 1
        counts["retained"] += 1 if msg.retain else 0
        counts["bytes"] += len(msg.payload)
        stamp = f"{time.monotonic() - t0:8.2f}s"
        flag = "R" if msg.retain else " "
        if args.sizes:
            if msg.topic.endswith("/config"):
                out(f"{stamp} {flag} {len(msg.payload):5d} B  {msg.topic}")
            return
        payload = msg.payload.decode(errors="replace")
        out(f"{stamp} {flag} {len(msg.payload):5d} B  {msg.topic}  {payload}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.broker, args.port, 30)
    client.loop_start()
    try:
        while time.monotonic() - t0 < args.duration:
            time.sleep(0.2)
    except KeyboardInterrupt:
        pass
    client.loop_stop()
    out(f"# {counts['messages']} messages, {counts['retained']} retained, {counts['bytes']} bytes in "
        f"{time.monotonic() - t0:.0f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
