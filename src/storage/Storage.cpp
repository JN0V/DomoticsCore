#include <DomoticsCore/Storage.h>
#include <DomoticsCore/Logger.h>

#define LOG_STORAGE "STORAGE"

Storage::Storage() : systemPrefs(nullptr), initialized(false), readOnly(false) {
}

Storage::~Storage() {
    end();
}

void Storage::begin(bool readOnly) {
    this->readOnly = readOnly;
    
    if (!appData.begin("app-data", readOnly)) {
        DLOG_E(LOG_STORAGE, "Failed to initialize application data storage");
        return;
    }
    
    initialized = true;
    DLOG_I(LOG_STORAGE, "Application data storage initialized (read-only: %s)", readOnly ? "yes" : "no");
}

void Storage::end() {
    if (initialized) {
        appData.end();
        initialized = false;
        DLOG_D(LOG_STORAGE, "Application data storage closed");
    }
}

// ========== APPLICATION DATA METHODS ==========

bool Storage::putBool(const char* key, bool value) {
    if (!initialized || readOnly) return false;
    return appData.putBool(key, value);
}

bool Storage::getBool(const char* key, bool defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getBool(key, defaultValue);
}

bool Storage::putUChar(const char* key, uint8_t value) {
    if (!initialized || readOnly) return false;
    return appData.putUChar(key, value);
}

uint8_t Storage::getUChar(const char* key, uint8_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getUChar(key, defaultValue);
}

bool Storage::putShort(const char* key, int16_t value) {
    if (!initialized || readOnly) return false;
    return appData.putShort(key, value);
}

int16_t Storage::getShort(const char* key, int16_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getShort(key, defaultValue);
}

bool Storage::putUShort(const char* key, uint16_t value) {
    if (!initialized || readOnly) return false;
    return appData.putUShort(key, value);
}

uint16_t Storage::getUShort(const char* key, uint16_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getUShort(key, defaultValue);
}

bool Storage::putInt(const char* key, int32_t value) {
    if (!initialized || readOnly) return false;
    return appData.putInt(key, value);
}

int32_t Storage::getInt(const char* key, int32_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getInt(key, defaultValue);
}

bool Storage::putUInt(const char* key, uint32_t value) {
    if (!initialized || readOnly) return false;
    return appData.putUInt(key, value);
}

uint32_t Storage::getUInt(const char* key, uint32_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getUInt(key, defaultValue);
}

bool Storage::putLong(const char* key, int32_t value) {
    if (!initialized || readOnly) return false;
    return appData.putLong(key, value);
}

int32_t Storage::getLong(const char* key, int32_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getLong(key, defaultValue);
}

bool Storage::putULong(const char* key, uint32_t value) {
    if (!initialized || readOnly) return false;
    return appData.putULong(key, value);
}

uint32_t Storage::getULong(const char* key, uint32_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getULong(key, defaultValue);
}

bool Storage::putLong64(const char* key, int64_t value) {
    if (!initialized || readOnly) return false;
    return appData.putLong64(key, value);
}

int64_t Storage::getLong64(const char* key, int64_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getLong64(key, defaultValue);
}

bool Storage::putULong64(const char* key, uint64_t value) {
    if (!initialized || readOnly) return false;
    return appData.putULong64(key, value);
}

uint64_t Storage::getULong64(const char* key, uint64_t defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getULong64(key, defaultValue);
}

bool Storage::putFloat(const char* key, float value) {
    if (!initialized || readOnly) return false;
    return appData.putFloat(key, value);
}

float Storage::getFloat(const char* key, float defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getFloat(key, defaultValue);
}

bool Storage::putDouble(const char* key, double value) {
    if (!initialized || readOnly) return false;
    return appData.putDouble(key, value);
}

double Storage::getDouble(const char* key, double defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getDouble(key, defaultValue);
}

bool Storage::putString(const char* key, const char* value) {
    if (!initialized || readOnly) return false;
    return appData.putString(key, value);
}

bool Storage::putString(const char* key, const String& value) {
    if (!initialized || readOnly) return false;
    return appData.putString(key, value);
}

String Storage::getString(const char* key, const String& defaultValue) {
    if (!initialized) return defaultValue;
    return appData.getString(key, defaultValue);
}

bool Storage::putBytes(const char* key, const void* value, size_t len) {
    if (!initialized || readOnly) return false;
    return appData.putBytes(key, value, len);
}

size_t Storage::getBytes(const char* key, void* buf, size_t maxLen) {
    if (!initialized) return 0;
    return appData.getBytes(key, buf, maxLen);
}

size_t Storage::getBytesLength(const char* key) {
    if (!initialized) return 0;
    return appData.getBytesLength(key);
}

bool Storage::remove(const char* key) {
    if (!initialized || readOnly) return false;
    return appData.remove(key);
}

bool Storage::clear() {
    if (!initialized || readOnly) return false;
    return appData.clear();
}

bool Storage::isKey(const char* key) {
    if (!initialized) return false;
    return appData.isKey(key);
}

size_t Storage::freeEntries() {
    if (!initialized) return 0;
    return appData.freeEntries();
}

// ========== SYSTEM PREFERENCES ACCESS ==========

Preferences& Storage::getSystemPreferences() {
    if (!systemPrefs) {
        DLOG_E(LOG_STORAGE, "System preferences not available - Storage not properly initialized by DomoticsCore");
        // Return a dummy preferences object to avoid crashes
        static Preferences dummy;
        return dummy;
    }
    return *systemPrefs;
}
