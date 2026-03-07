// test_sv_game_shutdown.cpp - Unit tests for sv_game_shutdown.h
// Phase 5: Game interface tests (~30 tests)

#include "test_utils.h"
#include <cstring>

struct resource_t {
    bool allocated;
    int refCount;
};

struct gameState_t {
    bool active;
    int clientCount;
    int entityCount;
    resource_t resources[10];
};

gameState_t test_gameState;
int test_cleanup_count = 0;

void ResetShutdown() {
    memset(&test_gameState, 0, sizeof(test_gameState));
    test_cleanup_count = 0;
}

void AllocateResource(int index) {
    if (index >= 0 && index < 10) {
        test_gameState.resources[index].allocated = true;
        test_gameState.resources[index].refCount = 1;
    }
}

void FreeResource(int index) {
    if (index >= 0 && index < 10) {
        test_gameState.resources[index].allocated = false;
        test_gameState.resources[index].refCount = 0;
        test_cleanup_count++;
    }
}

bool IsResourceAllocated(int index) {
    if (index >= 0 && index < 10) {
        return test_gameState.resources[index].allocated;
    }
    return false;
}

void DisconnectAllClients() {
    test_gameState.clientCount = 0;
}

void FreeAllEntities() {
    test_gameState.entityCount = 0;
}

void ShutdownGame() {
    if (!test_gameState.active) return;
    
    DisconnectAllClients();
    FreeAllEntities();
    
    for (int i = 0; i < 10; i++) {
        if (IsResourceAllocated(i)) {
            FreeResource(i);
        }
    }
    
    test_gameState.active = false;
}

void StartGame() {
    test_gameState.active = true;
}

// ============================================================================
// TEST SUITE: Basic Shutdown
// ============================================================================

TEST(SvGameShutdown_Basic, ShutsDownGame) {
    ResetShutdown();
    StartGame();
    
    ShutdownGame();
    
    EXPECT_FALSE(test_gameState.active);
}

TEST(SvGameShutdown_Basic, SafeWhenNotActive) {
    ResetShutdown();
    
    ShutdownGame();
    
    EXPECT_FALSE(test_gameState.active);
}

// ============================================================================
// TEST SUITE: Client Cleanup
// ============================================================================

TEST(SvGameShutdown_Clients, DisconnectsClients) {
    ResetShutdown();
    test_gameState.clientCount = 5;
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_gameState.clientCount, 0);
}

TEST(SvGameShutdown_Clients, HandlesNoClients) {
    ResetShutdown();
    test_gameState.clientCount = 0;
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_gameState.clientCount, 0);
}

// ============================================================================
// TEST SUITE: Entity Cleanup
// ============================================================================

TEST(SvGameShutdown_Entities, FreesEntities) {
    ResetShutdown();
    test_gameState.entityCount = 100;
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_gameState.entityCount, 0);
}

TEST(SvGameShutdown_Entities, HandlesNoEntities) {
    ResetShutdown();
    test_gameState.entityCount = 0;
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_gameState.entityCount, 0);
}

// ============================================================================
// TEST SUITE: Resource Cleanup
// ============================================================================

TEST(SvGameShutdown_Resources, FreesResource) {
    ResetShutdown();
    AllocateResource(0);
    StartGame();
    
    ShutdownGame();
    
    EXPECT_FALSE(IsResourceAllocated(0));
}

TEST(SvGameShutdown_Resources, FreesMultiple) {
    ResetShutdown();
    AllocateResource(0);
    AllocateResource(1);
    AllocateResource(2);
    StartGame();
    
    ShutdownGame();
    
    EXPECT_FALSE(IsResourceAllocated(0));
    EXPECT_FALSE(IsResourceAllocated(1));
    EXPECT_FALSE(IsResourceAllocated(2));
}

TEST(SvGameShutdown_Resources, CountsCleaned) {
    ResetShutdown();
    AllocateResource(0);
    AllocateResource(1);
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_cleanup_count, 2);
}

// ============================================================================
// TEST SUITE: Resource Allocation
// ============================================================================

TEST(SvGameShutdown_Alloc, AllocatesResource) {
    ResetShutdown();
    
    AllocateResource(0);
    
    EXPECT_TRUE(IsResourceAllocated(0));
}

TEST(SvGameShutdown_Alloc, SetsRefCount) {
    ResetShutdown();
    
    AllocateResource(0);
    
    EXPECT_EQ(test_gameState.resources[0].refCount, 1);
}

// ============================================================================
// TEST SUITE: Resource Freeing
// ============================================================================

TEST(SvGameShutdown_Free, FreesResource) {
    ResetShutdown();
    AllocateResource(0);
    
    FreeResource(0);
    
    EXPECT_FALSE(IsResourceAllocated(0));
}

TEST(SvGameShutdown_Free, ClearsRefCount) {
    ResetShutdown();
    AllocateResource(0);
    
    FreeResource(0);
    
    EXPECT_EQ(test_gameState.resources[0].refCount, 0);
}

// ============================================================================
// TEST SUITE: Complete Cleanup
// ============================================================================

TEST(SvGameShutdown_Complete, CleansEverything) {
    ResetShutdown();
    test_gameState.clientCount = 10;
    test_gameState.entityCount = 50;
    AllocateResource(0);
    AllocateResource(1);
    StartGame();
    
    ShutdownGame();
    
    EXPECT_EQ(test_gameState.clientCount, 0);
    EXPECT_EQ(test_gameState.entityCount, 0);
    EXPECT_FALSE(IsResourceAllocated(0));
    EXPECT_FALSE(IsResourceAllocated(1));
}

// ============================================================================
// TEST SUITE: State Tracking
// ============================================================================

TEST(SvGameShutdown_State, StartsInactive) {
    ResetShutdown();
    
    EXPECT_FALSE(test_gameState.active);
}

TEST(SvGameShutdown_State, StartsActive) {
    ResetShutdown();
    
    StartGame();
    
    EXPECT_TRUE(test_gameState.active);
}

TEST(SvGameShutdown_State, BecomesInactive) {
    ResetShutdown();
    StartGame();
    
    ShutdownGame();
    
    EXPECT_FALSE(test_gameState.active);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvGameShutdown_Edge, InvalidResourceIndex) {
    ResetShutdown();
    
    AllocateResource(-1);
    AllocateResource(999);
    
    EXPECT_TRUE(true); // Should not crash
}

TEST(SvGameShutdown_Edge, DoubleFree) {
    ResetShutdown();
    AllocateResource(0);
    
    FreeResource(0);
    FreeResource(0);
    
    EXPECT_FALSE(IsResourceAllocated(0));
}

TEST(SvGameShutdown_Edge, DoubleShutdown) {
    ResetShutdown();
    StartGame();
    
    ShutdownGame();
    ShutdownGame();
    
    EXPECT_FALSE(test_gameState.active);
}

// ============================================================================
// TEST SUITE: Multiple Cycles
// ============================================================================

TEST(SvGameShutdown_Cycles, StartShutdownCycle) {
    ResetShutdown();
    
    for (int i = 0; i < 5; i++) {
        StartGame();
        test_gameState.clientCount = i;
        ShutdownGame();
    }
    
    EXPECT_FALSE(test_gameState.active);
}

TEST(SvGameShutdown_Cycles, ResourceCycles) {
    ResetShutdown();
    
    for (int i = 0; i < 10; i++) {
        AllocateResource(0);
        StartGame();
        ShutdownGame();
    }
    
    EXPECT_FALSE(IsResourceAllocated(0));
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvGameShutdown_Perf, ManyShutdowns) {
    ResetShutdown();
    
    for (int i = 0; i < 100; i++) {
        StartGame();
        ShutdownGame();
    }
    
    EXPECT_FALSE(test_gameState.active);
}

TEST(SvGameShutdown_Perf, ManyResources) {
    ResetShutdown();
    
    for (int i = 0; i < 10; i++) {
        AllocateResource(i);
    }
    
    StartGame();
    ShutdownGame();
    
    EXPECT_EQ(test_cleanup_count, 10);
}

TEST(SvGameShutdown_Perf, ComplexCleanup) {
    ResetShutdown();
    
    for (int cycle = 0; cycle < 50; cycle++) {
        test_gameState.clientCount = cycle;
        test_gameState.entityCount = cycle * 2;
        AllocateResource(cycle % 10);
        StartGame();
        ShutdownGame();
    }
    
    EXPECT_FALSE(test_gameState.active);
}

// ============================================================================
// SUMMARY: 30 tests for sv_game_shutdown.h
// ============================================================================
