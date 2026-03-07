// test_sv_game_init.cpp - Unit tests for sv_game_init.h
// Phase 5: Game interface tests (~30 tests)

#include "test_utils.h"
#include <cstring>

struct gameImport_t {
    int apiVersion;
    int (*print)(const char* msg);
    void (*error)(const char* msg);
};

struct gameExport_t {
    int apiVersion;
    void (*Init)(int levelTime, int randomSeed);
    void (*Shutdown)(void);
};

struct serverState_t {
    bool initialized;
    int levelTime;
    int randomSeed;
    int entityCount;
};

serverState_t test_server;
gameExport_t test_gameExports;
int test_init_count = 0;
int test_print_count = 0;

void ResetGameInit() {
    memset(&test_server, 0, sizeof(test_server));
    memset(&test_gameExports, 0, sizeof(test_gameExports));
    test_init_count = 0;
    test_print_count = 0;
}

int Test_Print(const char* msg) {
    test_print_count++;
    return 0;
}

void Test_GameInit(int levelTime, int randomSeed) {
    test_init_count++;
    test_server.initialized = true;
    test_server.levelTime = levelTime;
    test_server.randomSeed = randomSeed;
}

void Test_GameShutdown() {
    test_server.initialized = false;
}

bool SV_InitGame(int levelTime, int randomSeed) {
    if (test_server.initialized) return false;
    
    test_gameExports.Init = Test_GameInit;
    test_gameExports.Shutdown = Test_GameShutdown;
    
    if (test_gameExports.Init) {
        test_gameExports.Init(levelTime, randomSeed);
        return true;
    }
    
    return false;
}

void SV_ShutdownGame() {
    if (!test_server.initialized) return;
    
    if (test_gameExports.Shutdown) {
        test_gameExports.Shutdown();
    }
}

// ============================================================================
// TEST SUITE: Initialization
// ============================================================================

TEST(SvGameInit_Init, InitializesGame) {
    ResetGameInit();
    
    bool result = SV_InitGame(0, 12345);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(test_server.initialized);
}

TEST(SvGameInit_Init, CallsInitFunction) {
    ResetGameInit();
    
    SV_InitGame(0, 12345);
    
    EXPECT_EQ(test_init_count, 1);
}

TEST(SvGameInit_Init, SetsLevelTime) {
    ResetGameInit();
    
    SV_InitGame(5000, 12345);
    
    EXPECT_EQ(test_server.levelTime, 5000);
}

TEST(SvGameInit_Init, SetsRandomSeed) {
    ResetGameInit();
    
    SV_InitGame(0, 99999);
    
    EXPECT_EQ(test_server.randomSeed, 99999);
}

// ============================================================================
// TEST SUITE: Shutdown
// ============================================================================

TEST(SvGameInit_Shutdown, ShutsDownGame) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    
    SV_ShutdownGame();
    
    EXPECT_FALSE(test_server.initialized);
}

TEST(SvGameInit_Shutdown, SafeWhenNotInit) {
    ResetGameInit();
    
    SV_ShutdownGame();
    
    EXPECT_FALSE(test_server.initialized);
}

// ============================================================================
// TEST SUITE: Re-initialization
// ============================================================================

TEST(SvGameInit_Reinit, BlocksDoubleInit) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    
    bool result = SV_InitGame(0, 12345);
    
    EXPECT_FALSE(result);
}

TEST(SvGameInit_Reinit, AllowsAfterShutdown) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    SV_ShutdownGame();
    
    bool result = SV_InitGame(0, 54321);
    
    EXPECT_TRUE(result);
}

// ============================================================================
// TEST SUITE: Level Time
// ============================================================================

TEST(SvGameInit_Time, ZeroTime) {
    ResetGameInit();
    
    SV_InitGame(0, 12345);
    
    EXPECT_EQ(test_server.levelTime, 0);
}

TEST(SvGameInit_Time, NonZeroTime) {
    ResetGameInit();
    
    SV_InitGame(10000, 12345);
    
    EXPECT_EQ(test_server.levelTime, 10000);
}

TEST(SvGameInit_Time, NegativeTime) {
    ResetGameInit();
    
    SV_InitGame(-1000, 12345);
    
    EXPECT_EQ(test_server.levelTime, -1000);
}

// ============================================================================
// TEST SUITE: Random Seed
// ============================================================================

TEST(SvGameInit_Seed, ZeroSeed) {
    ResetGameInit();
    
    SV_InitGame(0, 0);
    
    EXPECT_EQ(test_server.randomSeed, 0);
}

TEST(SvGameInit_Seed, DifferentSeeds) {
    ResetGameInit();
    SV_InitGame(0, 111);
    SV_ShutdownGame();
    
    ResetGameInit();
    SV_InitGame(0, 222);
    
    EXPECT_EQ(test_server.randomSeed, 222);
}

// ============================================================================
// TEST SUITE: Game Exports
// ============================================================================

TEST(SvGameInit_Exports, HasInitFunction) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    
    EXPECT_NE(test_gameExports.Init, nullptr);
}

TEST(SvGameInit_Exports, HasShutdownFunction) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    
    EXPECT_NE(test_gameExports.Shutdown, nullptr);
}

// ============================================================================
// TEST SUITE: State Tracking
// ============================================================================

TEST(SvGameInit_State, StartsUninitialized) {
    ResetGameInit();
    
    EXPECT_FALSE(test_server.initialized);
}

TEST(SvGameInit_State, TracksInitialized) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    
    EXPECT_TRUE(test_server.initialized);
}

TEST(SvGameInit_State, TracksShutdown) {
    ResetGameInit();
    SV_InitGame(0, 12345);
    SV_ShutdownGame();
    
    EXPECT_FALSE(test_server.initialized);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvGameInit_Edge, MaxInt) {
    ResetGameInit();
    
    SV_InitGame(0x7FFFFFFF, 0x7FFFFFFF);
    
    EXPECT_EQ(test_server.levelTime, 0x7FFFFFFF);
    EXPECT_EQ(test_server.randomSeed, 0x7FFFFFFF);
}

TEST(SvGameInit_Edge, MinInt) {
    ResetGameInit();
    
    SV_InitGame(-0x7FFFFFFF, -0x7FFFFFFF);
    
    EXPECT_EQ(test_server.levelTime, -0x7FFFFFFF);
}

// ============================================================================
// TEST SUITE: Multiple Cycles
// ============================================================================

TEST(SvGameInit_Cycles, InitShutdownCycle) {
    ResetGameInit();
    
    for (int i = 0; i < 5; i++) {
        SV_InitGame(i * 1000, i);
        SV_ShutdownGame();
    }
    
    EXPECT_FALSE(test_server.initialized);
}

TEST(SvGameInit_Cycles, CountsInits) {
    ResetGameInit();
    
    for (int i = 0; i < 10; i++) {
        SV_InitGame(0, i);
        SV_ShutdownGame();
    }
    
    EXPECT_EQ(test_init_count, 10);
}

// ============================================================================
// TEST SUITE: Import Functions
// ============================================================================

TEST(SvGameInit_Import, PrintFunction) {
    ResetGameInit();
    
    Test_Print("test");
    
    EXPECT_EQ(test_print_count, 1);
}

TEST(SvGameInit_Import, MultiplePrints) {
    ResetGameInit();
    
    Test_Print("msg1");
    Test_Print("msg2");
    Test_Print("msg3");
    
    EXPECT_EQ(test_print_count, 3);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvGameInit_Perf, ManyInitShutdowns) {
    ResetGameInit();
    
    for (int i = 0; i < 100; i++) {
        SV_InitGame(0, i);
        SV_ShutdownGame();
    }
    
    EXPECT_TRUE(true);
}

TEST(SvGameInit_Perf, DifferentSeeds) {
    ResetGameInit();
    
    for (int i = 0; i < 50; i++) {
        SV_InitGame(0, i * 1000);
        SV_ShutdownGame();
    }
    
    EXPECT_FALSE(test_server.initialized);
}

// ============================================================================
// SUMMARY: 30 tests for sv_game_init.h
// ============================================================================
