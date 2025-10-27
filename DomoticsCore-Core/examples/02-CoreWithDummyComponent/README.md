# Component Test Example

This example demonstrates how to build **custom components** using the DomoticsCore library. It shows developers how to create their own components that integrate with the framework.

## What This Example Shows

- **Custom Component Development**: How to build new components using `IComponent` interface
- **Component Lifecycle**: Proper `begin()`, `loop()`, `shutdown()` implementation
- **Dependency Resolution**: Components with dependencies initialize in correct order
- **Hardware Interaction**: LED control component example
- **Runtime Interaction**: Getting components by name and calling their methods
- **Non-blocking Timers**: Using `Utils::NonBlockingDelay` for timing
- **Logging Integration**: Proper logging patterns for components

## Custom Components in This Example

### TestComponent (in CustomComponents.h)
- **Purpose**: Demonstrates basic component patterns and lifecycle
- **ComponentA**: No dependencies, 3-second heartbeat
- **ComponentB**: Depends on ComponentA, 4-second heartbeat  
- **ComponentC**: Depends on ComponentB, 6-second heartbeat
- **Features**: Counter simulation, configurable intervals, status reporting

### LEDBlinkerComponent (in CustomComponents.h)
- **Purpose**: Shows hardware interaction patterns
- **Features**: Built-in LED blinking, configurable intervals, enable/disable control
- **Hardware**: Uses `LED_BUILTIN` pin with 500ms blink interval

### Library Components (Optional)
- **WifiComponent**: Uncomment in main.cpp to add Wifi from library

## Expected Output

```
[INFO] Adding test components...
[INFO] Starting core with 3 components...
[INFO] Registered component: ComponentA v1.0.0-test
[INFO] Registered component: ComponentB v1.0.0-test
[INFO] Registered component: ComponentC v1.0.0-test
[INFO] Initializing component: ComponentA
[INFO] TestComponent 'ComponentA' initialized successfully
[INFO] Initializing component: ComponentB
[INFO] TestComponent 'ComponentB' initialized successfully
[INFO] Initializing component: ComponentC
[INFO] TestComponent 'ComponentC' initialized successfully
[INFO] All components initialized successfully (3 components)
[INFO] Setup complete - all components initialized
```

## Build and Run

```bash
cd examples/02-CoreWithDummyComponent
pio run -t upload -t monitor
```

## Key Learning Points

1. **Zero Overhead**: Only included components are compiled
2. **Dependency Order**: Components initialize in dependency-resolved order
3. **Type Safety**: Template methods provide compile-time type checking
4. **Clean API**: Simple `addComponent()` and `getComponent<T>()` interface
5. **Modular Design**: Each component is self-contained and optional
