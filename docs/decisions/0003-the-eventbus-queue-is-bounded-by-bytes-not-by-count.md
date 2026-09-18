# 0003 — the EventBus queue is bounded by bytes, not by a count of entries

**Status**: accepted
**Date**: 2026-09-18

## Context

`EventBus::enqueue()` capped the queue at 32 entries and dropped the oldest to
make room. That 32 was a stand-in for bytes: it exists because an
`MQTTPublishEvent` is 830 B and 32 of them is about what a device can afford to
hold. Applied to a 4-byte event it does not measure the resource it protects.

Nothing drains the queue between the first component `begin()` and the first
`Core::loop()`, and a FullStack application emits about forty events in that
window. Measured on a bench ESP32: eight were dropped at every boot. They were
harmless by an accident of ordering — the survivors happened to be the ones with
subscribers — and ten more emissions would have silently unsubscribed a device
from its own MQTT commands.

Three alternatives were measured and rejected. **Not queueing an event with no
subscriber** breaks the documented `on("component/ready", …)` pattern, since
subscribers are posted after the emission. **Draining at "safe" points inside
`begin()`** has no point at which sketch subscribers exist, and trips the
`assert(!dispatching_)` if a handler subscribes. **Leaving the queue unbounded
during `begin()`** is an undefined bound, which is the constraint this had to
respect in the first place.

## Decision

One thing changes: the unit of the bound. `QueueCost` models what a queued event
costs the heap — a deque slot, a payload block, a topic block — and the queue
holds `kBudgetBytes`, which is 32 events the size of an `MQTTPublishEvent`. The
oldest are evicted until a new event fits. An event larger than the whole budget
is refused, counted and named rather than emptying the queue for nothing, and a
second bound of 256 entries guards against the model drifting from the allocator.

The model takes the **shape** of the allocator, and its constants are **platform
constants** measured on the boards. That is what makes it safe to tighten: the
shape being right, each constant holds its measured value and the model is an
upper bound by equality on the target that runs it. A single cross-platform set
was tried and could not hold the memory constraint — it overcharges the
*reference* event, which inflates the budget rather than protecting anything,
while being exact for the classes that actually fill it. An unmeasured target
takes a conservative set and never aims for equality.

## Consequences

The boot burst costs about 6 KB against a 28 KB budget and loses nothing. The
worst case is what it always was — 32 reference events — so no device holds more
than before. A burst of *small* events is no longer truncated at 32, which is a
visible behaviour change and is announced at the top of the release entry.

This does not fix the MQTT connect burst. That one is made of full-size events,
and its cliff is where it was, one entity further out; shrinking it is LO-2's
job, not this one.

**What the bound does not cover**: the sticky store. `publishSticky` keeps the
last payload per topic in `lastByTopic`, its payload-less overload empties the
vector and keeps the key, and only `reset()` erases the map. That store grows
with the number of distinct sticky topics and no budget counts it. Recorded in
`docs/deferred-work.md`; the title of this record is about the queue.
