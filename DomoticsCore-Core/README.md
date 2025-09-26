# DomoticsCore-Core

Minimal core runtime for DomoticsCore.

- Components registry and lifecycle
- Base interfaces (`IComponent`)
- Utilities (`EventBus`, `Timer`)
- Logging helpers (`Logger`)

## Public Headers

Include path prefix: `DomoticsCore/`

- `Core.h`
- `IComponent.h`
- `ComponentRegistry.h`
- `ComponentConfig.h`
- `EventBus.h`
- `Timer.h`
- `Logger.h`

## Examples

Located under `packages/DomoticsCore-Core/examples/`.

- `01-CoreOnly`
- `02-CoreWithDummyComponent`
- `03-EventBusBasics`
- `04-EventBusCoordinators`
- `05-EventBusTests`

## Build

Use PlatformIO from an example directory:

```
pio run -d packages/DomoticsCore-Core/examples/01-CoreOnly
```
