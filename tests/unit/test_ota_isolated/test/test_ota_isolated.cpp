#include <unity.h>
#include <string>
#include <vector>
#include <functional>
#include <cstring>

#ifdef NATIVE_BUILD
typedef std::string String;
inline unsigned long millis() { static unsigned long t = 0; return t += 10; }

namespace DomoticsCore {
namespace Mocks {

// ============================================================================
// MockEventBus
// ============================================================================
class MockEventBus {
public:
    static std::vector<std::string> emittedEvents;
    static void emit(const std::string& event) { emittedEvents.push_back(event); }
    static bool wasEmitted(const std::string& event) {
        for (const auto& e : emittedEvents) if (e == event) return true;
        return false;
    }
    static void reset() { emittedEvents.clear(); }
};
std::vector<std::string> MockEventBus::emittedEvents;

// ============================================================================
// MockUpdate - Simulates ESP32 Update library
// ============================================================================
class MockUpdate {
public:
    static bool beginCalled;
    static bool endCalled;
    static bool aborted;
    static size_t expectedSize;
    static size_t writtenBytes;
    static bool shouldFailBegin;
    static bool shouldFailWrite;
    static bool shouldFailEnd;
    static std::vector<uint8_t> writtenData;
    
    static bool begin(size_t size) {
        if (shouldFailBegin) return false;
        beginCalled = true;
        expectedSize = size;
        writtenBytes = 0;
        writtenData.clear();
        return true;
    }
    
    static size_t write(const uint8_t* data, size_t len) {
        if (shouldFailWrite) return 0;
        writtenBytes += len;
        for (size_t i = 0; i < len; i++) {
            writtenData.push_back(data[i]);
        }
        return len;
    }
    
    static bool end(bool evenIfRemaining = false) {
        if (shouldFailEnd) return false;
        endCalled = true;
        return true;
    }
    
    static void abort() {
        aborted = true;
        writtenData.clear();
        writtenBytes = 0;
    }
    
    static size_t progress() { return writtenBytes; }
    static size_t size() { return expectedSize; }
    
    static void reset() {
        beginCalled = false;
        endCalled = false;
        aborted = false;
        expectedSize = 0;
        writtenBytes = 0;
        shouldFailBegin = false;
        shouldFailWrite = false;
        shouldFailEnd = false;
        writtenData.clear();
    }
};

bool MockUpdate::beginCalled = false;
bool MockUpdate::endCalled = false;
bool MockUpdate::aborted = false;
size_t MockUpdate::expectedSize = 0;
size_t MockUpdate::writtenBytes = 0;
bool MockUpdate::shouldFailBegin = false;
bool MockUpdate::shouldFailWrite = false;
bool MockUpdate::shouldFailEnd = false;
std::vector<uint8_t> MockUpdate::writtenData;

} // namespace Mocks
} // namespace DomoticsCore
#endif

using namespace DomoticsCore::Mocks;

// ============================================================================
// OTA State Machine Logic Under Test
// ============================================================================

enum class OTAState {
    Idle,
    Checking,
    Downloading,
    Applying,
    RebootPending,
    Error
};

class OTALogicUnderTest {
public:
    OTAState state = OTAState::Idle;
    float progress = 0.0f;
    size_t downloadedBytes = 0;
    size_t totalBytes = 0;
    std::string lastError;
    bool autoReboot = true;
    
    // Version comparison
    std::string currentVersion = "1.0.0";
    std::string availableVersion;
    bool allowDowngrades = false;
    
    bool beginUpload(size_t expectedSize) {
        if (state != OTAState::Idle) {
            lastError = "OTA already in progress";
            return false;
        }
        
        if (!MockUpdate::begin(expectedSize)) {
            lastError = "Failed to begin update";
            state = OTAState::Error;
            return false;
        }
        
        state = OTAState::Downloading;
        totalBytes = expectedSize;
        downloadedBytes = 0;
        progress = 0.0f;
        MockEventBus::emit("ota/started");
        return true;
    }
    
    bool acceptChunk(const uint8_t* data, size_t length) {
        if (state != OTAState::Downloading) {
            lastError = "Not in downloading state";
            return false;
        }
        
        size_t written = MockUpdate::write(data, length);
        if (written != length) {
            lastError = "Write failed";
            state = OTAState::Error;
            MockEventBus::emit("ota/error");
            return false;
        }
        
        downloadedBytes += written;
        progress = totalBytes > 0 ? (float)downloadedBytes / totalBytes : 0.0f;
        MockEventBus::emit("ota/progress");
        return true;
    }
    
    bool finalizeUpload() {
        if (state != OTAState::Downloading) {
            lastError = "Not in downloading state";
            return false;
        }
        
        state = OTAState::Applying;
        
        if (!MockUpdate::end()) {
            lastError = "Failed to finalize update";
            state = OTAState::Error;
            MockEventBus::emit("ota/error");
            return false;
        }
        
        state = OTAState::RebootPending;
        MockEventBus::emit("ota/complete");
        return true;
    }
    
    void abortUpload(const std::string& reason) {
        MockUpdate::abort();
        lastError = reason;
        state = OTAState::Idle;
        downloadedBytes = 0;
        progress = 0.0f;
        MockEventBus::emit("ota/aborted");
    }
    
    // Version comparison logic
    bool shouldUpdate(const std::string& newVersion) {
        if (newVersion.empty()) return false;
        
        int cmp = compareVersions(currentVersion, newVersion);
        
        if (cmp < 0) return true;  // newVersion is higher
        if (cmp > 0 && allowDowngrades) return true;  // Downgrade allowed
        return false;  // Same version or downgrade not allowed
    }
    
    // Simple semantic version comparison
    int compareVersions(const std::string& v1, const std::string& v2) {
        int major1 = 0, minor1 = 0, patch1 = 0;
        int major2 = 0, minor2 = 0, patch2 = 0;
        
        sscanf(v1.c_str(), "%d.%d.%d", &major1, &minor1, &patch1);
        sscanf(v2.c_str(), "%d.%d.%d", &major2, &minor2, &patch2);
        
        if (major1 != major2) return major1 - major2;
        if (minor1 != minor2) return minor1 - minor2;
        return patch1 - patch2;
    }
    
    bool isIdle() const { return state == OTAState::Idle; }
    bool isBusy() const { return state != OTAState::Idle && state != OTAState::Error; }
};

// ============================================================================
// Tests
// ============================================================================

OTALogicUnderTest* ota = nullptr;

void setUp(void) {
    MockUpdate::reset();
    MockEventBus::reset();
    ota = new OTALogicUnderTest();
}

void tearDown(void) {
    delete ota;
    ota = nullptr;
}

// T141: Test OTA initial state
void test_ota_initial_state(void) {
    TEST_ASSERT_EQUAL(OTAState::Idle, ota->state);
    TEST_ASSERT_TRUE(ota->isIdle());
    TEST_ASSERT_FALSE(ota->isBusy());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ota->progress);
}

// T142: Test OTA state machine transitions
void test_ota_state_transitions(void) {
    // Idle -> Downloading
    TEST_ASSERT_TRUE(ota->beginUpload(1024));
    TEST_ASSERT_EQUAL(OTAState::Downloading, ota->state);
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ota/started"));
    
    // Downloading -> Applying -> RebootPending
    uint8_t data[1024] = {0};
    TEST_ASSERT_TRUE(ota->acceptChunk(data, 1024));
    TEST_ASSERT_TRUE(ota->finalizeUpload());
    TEST_ASSERT_EQUAL(OTAState::RebootPending, ota->state);
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ota/complete"));
}

// T143: Test OTA chunk handling
void test_ota_chunk_handling(void) {
    ota->beginUpload(1000);
    
    uint8_t chunk1[400] = {1};
    uint8_t chunk2[400] = {2};
    uint8_t chunk3[200] = {3};
    
    TEST_ASSERT_TRUE(ota->acceptChunk(chunk1, 400));
    TEST_ASSERT_EQUAL_FLOAT(0.4f, ota->progress);
    TEST_ASSERT_EQUAL(400, ota->downloadedBytes);
    
    TEST_ASSERT_TRUE(ota->acceptChunk(chunk2, 400));
    TEST_ASSERT_EQUAL_FLOAT(0.8f, ota->progress);
    
    TEST_ASSERT_TRUE(ota->acceptChunk(chunk3, 200));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, ota->progress);
    TEST_ASSERT_EQUAL(1000, ota->downloadedBytes);
}

// T144: Test OTA abort/cleanup
void test_ota_abort_cleanup(void) {
    ota->beginUpload(1024);
    
    uint8_t data[512] = {0};
    ota->acceptChunk(data, 512);
    TEST_ASSERT_EQUAL(512, ota->downloadedBytes);
    
    ota->abortUpload("User cancelled");
    
    TEST_ASSERT_EQUAL(OTAState::Idle, ota->state);
    TEST_ASSERT_EQUAL(0, ota->downloadedBytes);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ota->progress);
    TEST_ASSERT_TRUE(MockUpdate::aborted);
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ota/aborted"));
}

// T145: Test OTA version comparison
void test_ota_version_comparison(void) {
    ota->currentVersion = "1.2.3";
    
    // Higher version - should update
    TEST_ASSERT_TRUE(ota->shouldUpdate("1.2.4"));
    TEST_ASSERT_TRUE(ota->shouldUpdate("1.3.0"));
    TEST_ASSERT_TRUE(ota->shouldUpdate("2.0.0"));
    
    // Same version - should not update
    TEST_ASSERT_FALSE(ota->shouldUpdate("1.2.3"));
    
    // Lower version - should not update (downgrade disabled)
    TEST_ASSERT_FALSE(ota->shouldUpdate("1.2.2"));
    TEST_ASSERT_FALSE(ota->shouldUpdate("1.1.0"));
    TEST_ASSERT_FALSE(ota->shouldUpdate("0.9.9"));
    
    // Enable downgrades
    ota->allowDowngrades = true;
    TEST_ASSERT_TRUE(ota->shouldUpdate("1.2.2"));
    TEST_ASSERT_TRUE(ota->shouldUpdate("1.0.0"));
}

// Additional: Test error handling on begin failure
void test_ota_begin_failure(void) {
    MockUpdate::shouldFailBegin = true;
    
    TEST_ASSERT_FALSE(ota->beginUpload(1024));
    TEST_ASSERT_EQUAL(OTAState::Error, ota->state);
    TEST_ASSERT_FALSE(ota->lastError.empty());
}

// Additional: Test error handling on write failure
void test_ota_write_failure(void) {
    ota->beginUpload(1024);
    
    MockUpdate::shouldFailWrite = true;
    uint8_t data[512] = {0};
    
    TEST_ASSERT_FALSE(ota->acceptChunk(data, 512));
    TEST_ASSERT_EQUAL(OTAState::Error, ota->state);
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ota/error"));
}

// Additional: Test cannot begin while busy
void test_ota_cannot_begin_while_busy(void) {
    ota->beginUpload(1024);
    TEST_ASSERT_EQUAL(OTAState::Downloading, ota->state);
    
    // Try to begin another upload
    TEST_ASSERT_FALSE(ota->beginUpload(2048));
    TEST_ASSERT_EQUAL(OTAState::Downloading, ota->state);  // State unchanged
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ota_initial_state);
    RUN_TEST(test_ota_state_transitions);
    RUN_TEST(test_ota_chunk_handling);
    RUN_TEST(test_ota_abort_cleanup);
    RUN_TEST(test_ota_version_comparison);
    RUN_TEST(test_ota_begin_failure);
    RUN_TEST(test_ota_write_failure);
    RUN_TEST(test_ota_cannot_begin_while_busy);
    
    return UNITY_END();
}
