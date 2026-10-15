<!-- workline
sources: [DomoticsCore-WebUI/library.json, DomoticsCore-WebUI/include/DomoticsCore/WebUI.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI_HAL.h, DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h]
checked: c1eb8f3
verified: agent:claude-code
-->
# DomoticsCore-WebUI -- Project Context (AI Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI assistants working on the DomoticsCore-WebUI component. It covers identity, file inventory, architecture, dependencies, conventions, and constitution compliance.

---

## Pages

| Page | What it holds |
|------|---------------|
| [File Inventory](project-context-files.md) | every header and build file, its size and purpose |
| [Front End](project-context-frontend.md) | the `webui_src/` files, and what the front-end suite proves |
| [Key Classes and Dependencies](project-context-architecture.md) | how the classes relate, the libraries and Core symbols used |
| [Critical Development Conventions](project-context-conventions.md) | the conventions to follow when changing WebUI |
| [Constitution Compliance](project-context-constitution.md) | each principle, how WebUI meets it, the known gaps |
| [Modification Scenarios and Testing](project-context-modifying.md) | adding an endpoint, a widget, a field type or a provider; test suites and examples |

---

## 1. Component Identity

| Property | Value |
|----------|-------|
| **Name** | DomoticsCore-WebUI |
| **Version** | 1.5.0 |
| **Role** | Web dashboard, REST API, and real-time update server |
| **Size** | ~3,894 lines across 15 header files (largest single component in the framework) |
| **Platforms** | ESP32, ESP8266, ESP32-S2/S3/C3, native/stub |
| **License** | MIT |
| **Namespace** | `DomoticsCore::Components` (main), `DomoticsCore::Components::WebUI` (sub-modules) |

