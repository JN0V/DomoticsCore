# 0001 — `publish()` defers, it never drops; and QoS 0 is the ceiling

**Status**: accepted
**Date**: 2026-09-14

## Context

`MQTTComponent::publish()` accepts a QoS argument, validates it (values
above 2 are clamped, with a warning), and the wire ignores it: the HAL's
`publish()` has no QoS parameter, and PubSubClient publishes at QoS 0 only —
its `publish()` signature carries `retained`, nothing else. `subscribe()` is
different: the HAL passes the QoS through, and the client honours 0 and 1.
The observability design of 2026-09-05 prescribed a "retained QoS 1" crash
record; the code could not deliver it and the sentence was corrected rather
than the client replaced.

The rate limiter (`publishRateLimit`, 10 messages per tumbling second)
used to *discard* what exceeded it. Measured on a live broker on 2026-08-26
(BUG-29): a device declaring twelve Home Assistant entities lost the
configs of the last three, order-dependently and silently — the discovery
path is fire-and-forget across the EventBus, so no return value could have
carried the refusal back. The offline path already queued into
`messageQueue`, bounded by `maxQueueSize`, and drained it on every connected
`loop()`.

## Decision

`publish()` never drops a message the caller handed it: over the rate
limit it queues, exactly as it does offline, and returns `true`; the queue
governs the wire at the sustained rate. The single exception is a message
that can never be sent — a packet larger than the client's buffer
(`topic + payload + 7` against `getBufferSize()`, 768 bytes on ESP8266),
which is dropped where it is detected, named in a warning and counted in
`publishErrors` (BUG-39), because retrying it forever held everything queued
behind it. QoS 0 is the only delivery the component offers on publish; the
parameter is kept for the API's shape and for `subscribe()`, and the
documentation says so instead of promising acknowledgements.

## Consequences

A caller can burst a connect's worth of messages and lose none; the cost is
that a message may leave later than it was handed over, and a caller that
needs *now-or-never* uses `publishNow()`, which does not queue and returns
`false`. Offline, `maxQueueSize` is the bound, not the rate limit. Retained
delivery ("the broker keeps the last one") is the only persistence available;
"at least once" is not, and a design that needs it needs another client.
The drain loop must not call a function that re-queues into the vector it
iterates — it checks the limit itself and stops for the next `loop()`.
