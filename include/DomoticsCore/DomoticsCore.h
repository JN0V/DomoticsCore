#pragma once

/**
 * DomoticsCore - Main header file
 * Include this to get access to the core framework
 */

#include "Core.h"
#include "Logger.h"
#include "Components/IComponent.h"
#include "Components/ComponentConfig.h"
#include "Components/ComponentRegistry.h"
#include "Utils/Timer.h"

// Version information
#ifndef DOMOTICSCORE_VERSION
#define DOMOTICSCORE_VERSION "0.2.5"
#endif

// Main namespace is already defined in individual headers
