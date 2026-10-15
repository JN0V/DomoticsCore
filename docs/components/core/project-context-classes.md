<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/MemoryManager.h, DomoticsCore-Core/include/DomoticsCore/Timer.h, DomoticsCore-Core/include/DomoticsCore/Logger.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_Native.h]
checked: e6ad62e
verified: agent:claude-code
-->
# DomoticsCore-Core -- Key Classes and Their Responsibilities

Part of the [DomoticsCore-Core Project Context](project-context.md) (AI agent reference).

| Class | Responsibility | SRP Summary |
|-------|---------------|-------------|
| `Core` | Application entry point; owns registry; drives lifecycle | "Orchestrate the application" |
| `IComponent` | Abstract base for all components; defines lifecycle contract | "Define what a component is" |
| `ComponentRegistry` | Registration, dependency resolution, coordinated init/loop/shutdown | "Manage component collection and ordering" |
| `EventBus` | Queued publish/subscribe messaging | "Deliver messages between decoupled parties" |
| `MemoryManager` | Boot heap profile and the WebSocket client limit derived from it | "Adapt to available memory" |
| `NonBlockingDelay` | Interval timing without blocking | "Schedule periodic work" |
| `HeapTracker` | Checkpoint-based heap monitoring for tests | "Detect memory leaks" |
| `NativeAllocTracker` | Per-allocation heap tracking for native tests (singleton) | "Track individual allocations for leak diagnosis" |
| `ScopedAllocTracking` | RAII helper that enables/disables `NativeAllocTracker` around a test | "Scope allocation tracking to a test" |
| `AllocationRecord` | Record of a single allocation (ptr, size, source location, freed flag) | "Describe one allocation event" |
| `LoggerCallbacks` | Broadcast log messages to registered listeners (note: `removeCallback` currently clears ALL callbacks) | "Route log output" |
| `MemoryThresholds` | Configurable heap thresholds for profile detection | "Define profile boundaries" |
| `ProfileLimits` | Resource limits per profile (WS clients) | "Constrain resources per profile" |
| `HeapCheckpoint` | Named checkpoint wrapping a `HeapSnapshot` | "Label a heap measurement" |
| `MemoryTestResult` | Pass/fail result from heap stability assertions | "Report heap test outcome" |

