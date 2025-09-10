/**
 * @file Storage.h
 * @brief Storage interface for ESP32 domotics applications
 * 
 * Provides separate namespaces for:
 * - System preferences (used internally by DomoticsCore)
 * - Application data (available to user applications)
 */

#ifndef DOMOTICS_CORE_STORAGE_H
#define DOMOTICS_CORE_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

class Storage {
public:
    Storage();
    ~Storage();

    /**
     * Initialize storage system
     * @param readOnly Set to true for read-only access
     */
    void begin(bool readOnly = false);

    /**
     * Close storage and free resources
     */
    void end();

    // ========== APPLICATION DATA METHODS ==========
    // These methods use the "app-data" namespace for user application storage

    /**
     * Store a boolean value in application data
     * @param key Storage key
     * @param value Boolean value to store
     * @return true if successful
     */
    bool putBool(const char* key, bool value);

    /**
     * Retrieve a boolean value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved boolean value or default
     */
    bool getBool(const char* key, bool defaultValue = false);

    /**
     * Store an unsigned char value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putUChar(const char* key, uint8_t value);

    /**
     * Retrieve an unsigned char value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    uint8_t getUChar(const char* key, uint8_t defaultValue = 0);

    /**
     * Store a short value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putShort(const char* key, int16_t value);

    /**
     * Retrieve a short value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    int16_t getShort(const char* key, int16_t defaultValue = 0);

    /**
     * Store an unsigned short value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putUShort(const char* key, uint16_t value);

    /**
     * Retrieve an unsigned short value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    uint16_t getUShort(const char* key, uint16_t defaultValue = 0);

    /**
     * Store an int value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putInt(const char* key, int32_t value);

    /**
     * Retrieve an int value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    int32_t getInt(const char* key, int32_t defaultValue = 0);

    /**
     * Store an unsigned int value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putUInt(const char* key, uint32_t value);

    /**
     * Retrieve an unsigned int value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0);

    /**
     * Store a long value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putLong(const char* key, int32_t value);

    /**
     * Retrieve a long value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    int32_t getLong(const char* key, int32_t defaultValue = 0);

    /**
     * Store an unsigned long value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putULong(const char* key, uint32_t value);

    /**
     * Retrieve an unsigned long value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    uint32_t getULong(const char* key, uint32_t defaultValue = 0);

    /**
     * Store a long long value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putLong64(const char* key, int64_t value);

    /**
     * Retrieve a long long value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    int64_t getLong64(const char* key, int64_t defaultValue = 0);

    /**
     * Store an unsigned long long value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putULong64(const char* key, uint64_t value);

    /**
     * Retrieve an unsigned long long value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0);

    /**
     * Store a float value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putFloat(const char* key, float value);

    /**
     * Retrieve a float value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    float getFloat(const char* key, float defaultValue = NAN);

    /**
     * Store a double value in application data
     * @param key Storage key
     * @param value Value to store
     * @return true if successful
     */
    bool putDouble(const char* key, double value);

    /**
     * Retrieve a double value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved value or default
     */
    double getDouble(const char* key, double defaultValue = NAN);

    /**
     * Store a string value in application data
     * @param key Storage key
     * @param value String value to store
     * @return true if successful
     */
    bool putString(const char* key, const char* value);

    /**
     * Store a string value in application data
     * @param key Storage key
     * @param value String value to store
     * @return true if successful
     */
    bool putString(const char* key, const String& value);

    /**
     * Retrieve a string value from application data
     * @param key Storage key
     * @param defaultValue Default value if key doesn't exist
     * @return Retrieved string value or default
     */
    String getString(const char* key, const String& defaultValue = String());

    /**
     * Store binary data in application data
     * @param key Storage key
     * @param value Pointer to data buffer
     * @param len Length of data in bytes
     * @return true if successful
     */
    bool putBytes(const char* key, const void* value, size_t len);

    /**
     * Retrieve binary data from application data
     * @param key Storage key
     * @param buf Buffer to store retrieved data
     * @param maxLen Maximum buffer size
     * @return Number of bytes retrieved, 0 if key doesn't exist
     */
    size_t getBytes(const char* key, void* buf, size_t maxLen);

    /**
     * Get the length of stored binary data
     * @param key Storage key
     * @return Length of stored data, 0 if key doesn't exist
     */
    size_t getBytesLength(const char* key);

    /**
     * Remove a key from application data
     * @param key Storage key to remove
     * @return true if successful
     */
    bool remove(const char* key);

    /**
     * Clear all application data
     * @return true if successful
     */
    bool clear();

    /**
     * Check if a key exists in application data
     * @param key Storage key to check
     * @return true if key exists
     */
    bool isKey(const char* key);

    /**
     * Get number of entries in application data
     * @return Number of stored key-value pairs
     */
    size_t freeEntries();

    // ========== SYSTEM PREFERENCES ACCESS ==========
    // These methods provide read-only access to system preferences
    // Used internally by DomoticsCore, but available for advanced users

    /**
     * Get access to system preferences (read-only)
     * Use with caution - this is for advanced users only
     * @return Reference to system preferences
     */
    Preferences& getSystemPreferences();

private:
    Preferences appData;        // Application data storage
    Preferences* systemPrefs;   // Pointer to system preferences (managed externally)
    bool initialized;
    bool readOnly;

    friend class DomoticsCore;  // Allow DomoticsCore to set system preferences pointer
    void setSystemPreferences(Preferences* prefs) { systemPrefs = prefs; }
};

#endif // DOMOTICS_CORE_STORAGE_H
