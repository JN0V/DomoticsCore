# 0002 — The device keeps its own black box, in RTC memory, owned by Core

**Status**: accepted
**Date**: 2026-09-14

## Context

Devices in the field rebooted at intervals and nobody had seen one die.
The RemoteConsole was built for this and is structurally blind to it: a
panic runs below the Logger, the Telnet connection dies with the firmware,
its buffer is RAM (five lines on ESP8266), and nobody is connected at
3 a.m. Measured on 2026-09-05 on a nodemcuv2, a WROOM-32D and an ESP32-C3:
an ESP8266 `abort()`, `assert` and out-of-memory `new` all reach the next
boot as "Software/System restart", indistinguishable from `ESP.restart()`
by reason alone; RTC user memory survives every reset but power loss; on
ESP32 `RTC_NOINIT_ATTR` survives panics and software resets but not an
EN-pin reset; a stuck `loop()` on ESP32 never reboots, because the Arduino
core keeps `loopTask` off the task watchdog; every ESP32 panic already
writes a core dump that nothing read. A first campaign found every promoted
record torn when the phase marker sat inside the CRC.

## Decision

The record of the last death is a fixed-layout block of whole 32-bit words
in RTC memory — heap trend, phase marker, last loop timestamp, the crash
callback's registers, the failed-allocation group — written by Core, which
runs first and always; SystemInfo, the console and the WebUI only display
it. Promotion is the first act of `System::begin()`, before any component,
and the promoted record is **held until persisted** (the `bootdiag` blob,
the retained `{clientId}/crash` topic), so a device that dies again during
bring-up still gets it out on the boot that connects. The discriminator is
the record's own "crash callback ran" flag, not the SDK's reset reason,
which cannot tell the deaths apart. Markers written outside the tick's
flush — the phase marker, the failed-allocation group — sit **outside the
CRC**, each self-validated by its complement. The hooks a published library
must not seize (`custom_crash_callback`, the ESP32 failed-allocation slot)
are behind `DOMOTICS_CRASH_HOOKS`, set by System, off for bare-Core users,
and chain to a user hook. Everything reachable natively is driven through
`Platform_Stub.h` seams; what is not is measured on a board and the figure
recorded.

## Consequences

A death is explainable on the next boot, from the serial line, the
`bootdiag` blob, the crash topic or the core dump, without anyone watching.
It is not prevented. The record does not survive power loss or an ESP32
EN-pin reset; a hardware watchdog on ESP8266 leaves only the phase marker,
the last tick and the SDK's `epc1`; an ESP32 flashed with a partition table
that has no `coredump` partition keeps no dump; nothing decodes without the
ELF of the deployed build. A layout change costs a one-boot migration that
must keep the previous death. The tick costs microseconds on the idle loop
of an ESP8266, and its exact figure belongs to the link layout of the build
that measured it, not to the recorder.
