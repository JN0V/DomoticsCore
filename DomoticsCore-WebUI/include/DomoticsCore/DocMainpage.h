#pragma once

/**
 * \mainpage DomoticsCore-WebUI
 *
 * Web interface component for DomoticsCore. Serves a modern dashboard (HTML/CSS/JS),
 * exposes a WebSocket data channel, and allows components to publish UI via the
 * IWebUIProvider interface.
 *
 * \section features Features
 * - Static UI (HTML/CSS/JS) with gzip embedding
 * - WebSocket real-time updates (system + provider contexts)
 * - IWebUIProvider for multi-context UI per component
 * - Header status badges, dashboard cards, settings sections
 *
 * \section usage Usage
 * Add `WebUIComponent` to the Core and register providers implementing `IWebUIProvider`.
 */
