<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/Events.h, DomoticsCore-Core/include/DomoticsCore/Logger.h, DomoticsCore-Core/include/DomoticsCore/MemoryManager.h, DomoticsCore-Core/include/DomoticsCore/Platform_Stub.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h, DomoticsCore-Core/src/Core.cpp, DomoticsCore-Core/src/IComponent.cpp]
checked: e6ad62e
verified: agent:claude-code
-->
# DomoticsCore-Core -- Important Conventions and Pitfalls

Part of the [DomoticsCore-Core Project Context](project-context.md) (AI agent reference).

## Conventions

1. **Include prefix.** Always use `#include <DomoticsCore/Header.h>`, never bare `#include "Header.h"` from outside the library.

2. **Metadata in constructor.** Set `metadata` in the component constructor, not in `begin()`. The registry reads `metadata.name` at registration time.

3. **Internal init in begin(), cross-component in afterAllComponentsReady().** The `begin()` method must only do internal initialization (GPIO, state, timers). Accessing other components must happen in `afterAllComponentsReady()` where all dependencies are guaranteed available.

4. **Declare all dependencies.** Use `getDependencies()` to declare both required and optional dependencies. Never access another component without declaring the dependency.

5. **Sticky events for state.** When publishing state that late subscribers need (e.g., "wifi is connected"), use `publishSticky()` so components starting later can receive the current state.

6. **Subscription ownership.** The `on<T>()` helper on IComponent automatically sets `this` as the subscription owner. This enables automatic cleanup during `shutdown()`.

7. **Event topic naming.** Use hierarchical slash-separated topics: `"component/ready"`, `"wifi/sta/connected"`, `"mqtt/message"`. Constants must be defined in an `Events.h` or `*Events.h` file, not scattered as magic strings.

## Pitfalls

1. **Do NOT subscribe/unsubscribe during EventBus dispatch.** The EventBus asserts single-threaded access. Calling `subscribe()` or `unsubscribe()` inside an event handler will trigger an assert failure. Queue work for the next `loop()` iteration instead.

2. **Do NOT call `begin()` manually on components.** Let `ComponentRegistry::initializeAll()` handle initialization order. Manual early-init is an anti-pattern that breaks dependency resolution.

3. **The EventBus queue is bounded by bytes**, not by a count: `QueueCost::kBudgetBytes`, 32 events the size of an `MQTTPublishEvent`. A burst of small events costs little and is not truncated at 32; a burst of large ones still is. Drops are counted, logged once a minute with the peak occupancy, and carried in the flight record.

4. **`poll()` processes max 8 events per call by default.** High-throughput event scenarios may need multiple loop iterations to clear the queue.

5. **`getCore()` returns nullptr before registration.** If you call `getCore()` in the component constructor, it will return `nullptr`. The registry reference is injected during `registerComponent()`, and the Core reference is injected during `initializeAll()`.

6. **HeapTracker checkpoints use `std::map<String, ...>`.** This allocates heap memory, but `checkpoint()` measures what storing its own node costs and gives it back (`ownBytes_`, reset by `clear()`), so no window is charged for it. Use coarse-grained checkpoints in real leak tests.

7. **MemoryManager is a singleton.** While the constitution warns against singleton abuse, `MemoryManager` is an intentional exception because memory profile must be globally accessible. Do not create additional global singletons for components.

8. **`static_cast` for component retrieval.** `Core::getComponent<T>(name)` uses `static_cast`, not `dynamic_cast`. If you request the wrong type, you get undefined behavior. Ensure the name matches the expected type.

9. **`Platform_Stub.h` `String` class.** The native stub `String` class mimics Arduino's `String` but has subtle differences (e.g., `toFloat()` uses `std::stof`, which may throw). Always test edge cases on the target platform too.

10. **`LoggerCallbacks` uses ID-based add/remove.** `addCallback()` returns a `CallbackId` (uint8_t) that must be passed to `removeCallback(id)` for targeted removal. This was fixed in v2.0.1 — the previous implementation cleared ALL callbacks on any remove call.

