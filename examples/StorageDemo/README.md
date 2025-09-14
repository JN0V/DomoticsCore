# Storage Demo

A comprehensive demonstration of the DomoticsCore Storage component showcasing key-value storage for preferences and application data using ESP32 NVS (Non-Volatile Storage).

## Features Demonstrated

### Core Storage Functionality
- **NVS Preferences**: ESP32 Non-Volatile Storage for persistent data
- **Multiple Data Types**: Strings, integers, floats, booleans, and binary blobs
- **Key-Value Operations**: Store, retrieve, update, and delete operations
- **Namespace Management**: Isolated storage namespaces for different applications

### Data Types Support
- **Strings**: User names, configuration values, device locations
- **Integers**: Counters, IDs, enumeration values
- **Floats**: Sensor calibration, thresholds, timing values
- **Booleans**: Feature flags, enable/disable settings
- **Binary Blobs**: Complex structures, calibration matrices, binary configs

### Demo Phases
1. **Basic Preferences** (every 8 seconds)
   - User preferences (name, level, theme)
   - Device settings (ID, location)
   - String and integer storage

2. **Advanced Data Types**
   - Sensor calibration data (floats)
   - Feature flags (booleans)
   - Network configuration

3. **Binary Data Storage**
   - Device configuration structures
   - Calibration matrices
   - Binary blob operations

4. **Data Management**
   - Key listing and enumeration
   - Existence checking
   - Cleanup operations
   - Storage statistics

## Hardware Requirements

- ESP32 development board (uses built-in NVS partition)

## Storage Architecture

### NVS (Non-Volatile Storage)
- Built into ESP32 flash memory
- Wear leveling and power-fail safety
- Key-value storage with type safety
- Namespace isolation between applications

### Storage Configuration
```cpp
StorageConfig config;
config.namespace_name = "demo_app";    // Max 15 characters
config.readOnly = false;               // Allow write operations
config.maxEntries = 50;                // Application limit
config.autoCommit = true;              // Auto-commit changes
```

## Build and Upload

```bash
# Navigate to the StorageDemo directory
cd examples/StorageDemo

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Expected Output

The demo will cycle through different phases showing various storage operations:

```
[INFO] === Demo Phase 1 ===
[INFO] Demo: Basic Preferences (Strings & Integers)
[INFO] Stored user preferences:
[INFO]   Name: User_1
[INFO]   Level: 2
[INFO]   Theme: light
[INFO] Stored device settings:
[INFO]   ID: 1001
[INFO]   Location: Room_2
[INFO] Verification: User_1 (level 2)
```

Storage status reporting:
```
[INFO] === Storage Status ===
[INFO] Storage: NVS Preferences
[INFO] Namespace: demo_app
[INFO] Open: Yes
[INFO] Read-only: No
[INFO] Entries: 12/50
[INFO] Cached: 12
[INFO] App: DomoticsCore Storage Demo (boots: 3, debug: on)
```

Key management output:
```
[INFO] Stored keys (12 total):
[INFO]   - app_name
[INFO]   - app_version
[INFO]   - boot_count
[INFO]   - user_name
[INFO]   - device_id
[INFO]   - temp_offset
[INFO]   - device_config
```

## Storage Component API

### Basic Operations
- `putString(key, value)`: Store string value
- `putInt(key, value)`: Store 32-bit integer
- `putFloat(key, value)`: Store float value
- `putBool(key, value)`: Store boolean value
- `putBlob(key, data, length)`: Store binary data

### Retrieval Operations
- `getString(key, default)`: Get string value
- `getInt(key, default)`: Get integer value
- `getFloat(key, default)`: Get float value
- `getBool(key, default)`: Get boolean value
- `getBlob(key, buffer, maxLength)`: Get binary data

### Management Operations
- `exists(key)`: Check if key exists
- `remove(key)`: Delete specific key
- `clear()`: Remove all keys in namespace
- `getKeys()`: List all cached keys

### Information Methods
- `getEntryCount()`: Number of stored entries
- `getFreeEntries()`: Available entry slots
- `getNamespace()`: Current namespace name
- `getStorageInfo()`: Comprehensive status string

## Use Cases

### Application Configuration
```cpp
// Store app settings
storage->putString("app_name", "MyApp");
storage->putBool("debug_enabled", true);
storage->putFloat("update_interval", 30.0f);

// Load settings on startup
String appName = storage->getString("app_name", "DefaultApp");
bool debug = storage->getBool("debug_enabled", false);
float interval = storage->getFloat("update_interval", 60.0f);
```

### User Preferences
```cpp
// User settings
storage->putString("user_name", "John");
storage->putInt("user_level", 5);
storage->putString("ui_theme", "dark");

// Restore user session
String name = storage->getString("user_name");
int level = storage->getInt("user_level", 1);
```

### Sensor Calibration
```cpp
// Store calibration data
storage->putFloat("temp_offset", -2.5f);
storage->putFloat("humidity_scale", 1.02f);
storage->putBool("auto_calibrate", true);

// Load calibration on startup
float tempOffset = storage->getFloat("temp_offset", 0.0f);
float humScale = storage->getFloat("humidity_scale", 1.0f);
```

### Binary Configuration
```cpp
struct Config {
    uint32_t magic;
    uint16_t version;
    uint8_t flags;
};

Config config = {0xDEADBEEF, 0x0100, 0x80};

// Store binary structure
storage->putBlob("config", (uint8_t*)&config, sizeof(config));

// Load binary structure
Config loadedConfig;
size_t read = storage->getBlob("config", (uint8_t*)&loadedConfig, sizeof(loadedConfig));
```

## Troubleshooting

### Storage Issues
1. Check namespace name (max 15 characters)
2. Verify ESP32 NVS partition is available
3. Monitor for storage full conditions
4. Check read-only mode settings

### Data Persistence
1. Ensure `autoCommit` is enabled or call commit manually
2. Verify data types match between store/retrieve operations
3. Check key name consistency (case-sensitive)

### Build Issues
1. Clean build cache: `pio run --target clean`
2. Verify ESP32 platform and framework versions
3. Check library dependencies

## Memory Usage

Typical memory usage for this demo:
- Flash: ~20-25% (including NVS partition)
- RAM: ~5-6% (storage cache and buffers)
- NVS: ~1-2KB (demo data)

## Integration Example

To use the Storage component in your own projects:

```cpp
#include <DomoticsCore/Components/Storage.h>

// Configure storage
StorageConfig config;
config.namespace_name = "myapp";
config.maxEntries = 100;

// Create storage component
auto storageComponent = std::make_unique<StorageComponent>(config);

// Add to core
core->addComponent(std::move(storageComponent));

// Use in your application
storageComponent->putString("setting", "value");
String setting = storageComponent->getString("setting", "default");
```

## Advanced Features

### Namespace Isolation
- Each application can use its own namespace
- Prevents key conflicts between different apps
- Supports multiple storage instances

### Type Safety
- Automatic type checking and conversion
- Consistent API across all data types
- Error handling for type mismatches

### Caching System
- In-memory cache for frequently accessed keys
- Reduces NVS access overhead
- Automatic cache management
