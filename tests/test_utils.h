// test_utils.h - Common test utilities and mocks for JK2MV server tests

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <gtest/gtest.h>
#include <cstring>

// Mock common engine functions used by server modules
namespace MockEngine {

// Mock Com_Memset
inline void* Com_Memset(void* dest, int val, size_t count) {
    return memset(dest, val, count);
}

// Mock Com_Printf (silent for tests)
inline void Com_Printf(const char* fmt, ...) {
    // Silent in tests unless DEBUG_TEST_OUTPUT defined
#ifdef DEBUG_TEST_OUTPUT
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

// Mock Com_DPrintf (silent for tests)
inline void Com_DPrintf(const char* fmt, ...) {
    // Always silent
}

// Mock VectorCopy
inline void VectorCopy(const float* in, float* out) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

// Mock VectorAdd
inline void VectorAdd(const float* v1, const float* v2, float* out) {
    out[0] = v1[0] + v2[0];
    out[1] = v1[1] + v2[1];
    out[2] = v1[2] + v2[2];
}

// Mock VectorSubtract
inline void VectorSubtract(const float* v1, const float* v2, float* out) {
    out[0] = v1[0] - v2[0];
    out[1] = v1[1] - v2[1];
    out[2] = v1[2] - v2[2];
}

} // namespace MockEngine

// Base test fixture for server tests
class ServerTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all server tests
    }

    void TearDown() override {
        // Common cleanup
    }
};

// Test fixture with mock server state
class ServerWithStateTest : public ServerTestBase {
protected:
    // Mock server structures can be added here
    // Example:
    // struct {
    //     svEntity_t svEntities[MAX_GENTITIES];
    //     int numEntities;
    // } mockServer;

    void SetUp() override {
        ServerTestBase::SetUp();
        // Initialize mock server state
    }
};

// Helper: Create test vector
inline void MakeVec3(float* v, float x, float y, float z) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// Helper: Compare vectors with epsilon
inline bool Vec3Equal(const float* v1, const float* v2, float epsilon = 0.001f) {
    return fabs(v1[0] - v2[0]) < epsilon &&
           fabs(v1[1] - v2[1]) < epsilon &&
           fabs(v1[2] - v2[2]) < epsilon;
}

#endif // TEST_UTILS_H
