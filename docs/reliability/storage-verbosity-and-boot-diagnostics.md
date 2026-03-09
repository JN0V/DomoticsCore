# Storage Verbosity and Boot Diagnostics

## Boot Diagnostics

The System module implements boot count tracking and reset reason detection via the Storage component. Key capabilities:

- **Boot count**: An incrementing counter persisted via Storage, incremented on each boot.
- **Reset reason detection**: SystemInfo captures the ESP32 reset reason code at boot and maps it to a human-readable string (Power-on, Software reset, Panic/Exception, Watchdog, Deep sleep wake, Brownout, etc.).
- **Boot heap snapshot**: Free heap and minimum free heap are captured at boot time and persisted.

The `BootDiagnostics` struct (exposed via `SystemInfoComponent::getBootDiagnostics()`) provides:

```cpp
const BootDiagnostics& diag = systemInfo->getBootDiagnostics();
diag.bootCount;                // Persisted boot counter
diag.getResetReasonString();   // Human-readable reset reason
diag.wasUnexpectedReset();     // True if previous boot ended unexpectedly
```

The RemoteConsole `bootdiag` command displays all persisted diagnostics.

## Storage Verbosity

Storage status logs were reduced from 30-second intervals to 5-minute intervals (v1.4.0) to avoid log noise while still providing periodic health information.

For full details, see:
- [CHANGELOG.md v1.4.0](../../CHANGELOG.md) for the original implementation
- [SystemInfo Technical Reference](../components/system-info/technical-reference.md)
