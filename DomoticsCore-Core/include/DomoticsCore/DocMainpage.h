#pragma once

/**
 * \mainpage DomoticsCore-Core
 *
 * Core runtime for DomoticsCore: component model, registry, lifecycle, configuration,
 * logging, timers, and the event bus.
 *
 * \section features Features
 * - Component base class and lifecycle (begin, loop, shutdown)
 * - Central ComponentRegistry for add/lookup by name and type
 * - ComponentConfig for strongly-typed parameters
 * - EventBus (publish/subscribe) for decoupled communication
 * - Utilities: Logger, Timer (non-blocking delay)
 *
 * \section headers Public Headers
 * - Core.h, IComponent.h, ComponentRegistry.h
 * - ComponentConfig.h, EventBus.h, Timer.h, Logger.h
 */
