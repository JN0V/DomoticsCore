<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/ComponentConfig.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/MemoryManager.h, DomoticsCore-Core/include/DomoticsCore/Platform_HAL.h, DomoticsCore-Core/include/DomoticsCore/Platform_Stub.h, DomoticsCore-Core/src/Core.cpp, DomoticsCore-Core/src/IComponent.cpp]
checked: e6ad62e
verified: agent:claude-code
-->
# DomoticsCore-Core -- Constitution Compliance Reminders

Part of the [DomoticsCore-Core Project Context](project-context.md) (AI agent reference).

The following constitution principles are especially relevant to Core development:

## TDD (Section II) -- NON-NEGOTIABLE
- Every new method, branch, and edge case in Core MUST have corresponding tests.
- Core tests run in the `native` environment using `Platform_Stub.h`.
- Use `HeapTracker` in tests to verify heap stability.

## Memory Leak Prevention (Section XIV) -- ABSOLUTE PRIORITY
- Core manages component ownership via `std::unique_ptr` -- never use raw `new` for components.
- `EventBus` copies payloads into `std::vector<uint8_t>` -- be mindful of large payload sizes.
- `ComponentRegistry::removeComponent()` cleans up EventBus subscriptions via `unsubscribeOwner()`.
- After removing items from vectors, call `shrink_to_fit()` per constitution.
- No `String` concatenation in hot paths (e.g., `loop()`, event handlers).

## HAL Isolation (Section IX) -- NON-NEGOTIABLE
- `#ifdef` for platform detection is FORBIDDEN outside `Platform_HAL.h`, `Platform_ESP32.h`, `Platform_ESP8266.h`, `Platform_Stub.h`, and the `Testing/HeapTracker_*.h` files.
- Business logic in `Core.cpp`, `IComponent.cpp`, `ComponentRegistry.h`, `EventBus.h` must be completely platform-agnostic.
- All platform-specific functions are accessed via the `DomoticsCore::HAL::` namespace.

## File Size Limits (Section VII)
- Target: 200-500 lines per file.
- Hard limit: 800 lines.
- `ComponentRegistry.h` is currently ~378 lines. Watch for growth.
- `EventBus.h` is currently ~283 lines.
- `ComponentConfig.h` is currently ~65 lines.
- `MemoryManager.h` is currently ~201 lines.
- `Platform_HAL.h` is currently ~368 lines.
- `Platform_Stub.h` is currently ~630 lines (includes full `String` class stub). Watch the 800-line limit.

## SOLID Principles (Section I)
- `IComponent` is the interface (ISP, DIP).
- `ComponentRegistry` has one job: manage component collection (SRP).
- `EventBus` has one job: message delivery (SRP).
- Keep inheritance depth to 3 levels maximum.

## Non-Blocking Timer Pattern (Section X)
- `delay()` is FORBIDDEN except in boot sequences.
- `loop()` must complete in < 10ms.
- Core's heartbeat uses `HAL::getMillis()` comparison, not `delay()`.

