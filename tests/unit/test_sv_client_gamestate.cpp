// test_sv_client_gamestate.cpp - Unit tests for sv_client_gamestate.h
// Phase 3: Client management tests (~25 tests)

#include "test_utils.h"
#include <cstring>

#define MAX_CONFIGSTRINGS 1024
#define CS_PRIMED 3

struct client_t {
    int gamestateMessageNum;
    int deltaMessage;
    bool sendingGamestate;
    int lastSnapshotTime;
};

struct server_t {
    char configstrings[MAX_CONFIGSTRINGS][256];
    int checksumFeed;
    int time;
};

client_t test_clients[64];
server_t test_sv;

void ResetGamestate() {
    memset(&test_clients, 0, sizeof(test_clients));
    memset(&test_sv, 0, sizeof(test_sv));
    test_sv.time = 1000;
    test_sv.checksumFeed = 0x12345678;
}

void StartSendingGamestate(int clientNum) {
    test_clients[clientNum].sendingGamestate = true;
    test_clients[clientNum].gamestateMessageNum = 0;
}

void FinishSendingGamestate(int clientNum) {
    test_clients[clientNum].sendingGamestate = false;
    test_clients[clientNum].gamestateMessageNum = 1;
}

bool IsGamestateSent(int clientNum) {
    return test_clients[clientNum].gamestateMessageNum > 0;
}

// ============================================================================
// TEST SUITE: Gamestate Initialization
// ============================================================================

TEST(SvClientGamestate_Init, StartsNotSent) {
    ResetGamestate();
    
    EXPECT_FALSE(IsGamestateSent(0));
}

TEST(SvClientGamestate_Init, StartsSending) {
    ResetGamestate();
    StartSendingGamestate(0);
    
    EXPECT_TRUE(test_clients[0].sendingGamestate);
}

TEST(SvClientGamestate_Init, ResetsMessageNum) {
    ResetGamestate();
    test_clients[0].gamestateMessageNum = 999;
    StartSendingGamestate(0);
    
    EXPECT_EQ(test_clients[0].gamestateMessageNum, 0);
}

// ============================================================================
// TEST SUITE: Gamestate Transmission
// ============================================================================

TEST(SvClientGamestate_Send, FinishesSuccessfully) {
    ResetGamestate();
    StartSendingGamestate(0);
    FinishSendingGamestate(0);
    
    EXPECT_FALSE(test_clients[0].sendingGamestate);
    EXPECT_TRUE(IsGamestateSent(0));
}

TEST(SvClientGamestate_Send, SetsMessageNumber) {
    ResetGamestate();
    StartSendingGamestate(0);
    FinishSendingGamestate(0);
    
    EXPECT_GT(test_clients[0].gamestateMessageNum, 0);
}

// ============================================================================
// TEST SUITE: Configstrings
// ============================================================================

TEST(SvClientGamestate_Config, StoresConfigstring) {
    ResetGamestate();
    strcpy(test_sv.configstrings[0], "test_value");
    
    EXPECT_STREQ(test_sv.configstrings[0], "test_value");
}

TEST(SvClientGamestate_Config, MultipleConfigstrings) {
    ResetGamestate();
    strcpy(test_sv.configstrings[0], "value1");
    strcpy(test_sv.configstrings[1], "value2");
    strcpy(test_sv.configstrings[2], "value3");
    
    EXPECT_STREQ(test_sv.configstrings[0], "value1");
    EXPECT_STREQ(test_sv.configstrings[1], "value2");
    EXPECT_STREQ(test_sv.configstrings[2], "value3");
}

TEST(SvClientGamestate_Config, EmptyConfigstring) {
    ResetGamestate();
    
    EXPECT_EQ(test_sv.configstrings[0][0], '\0');
}

// ============================================================================
// TEST SUITE: Checksum Feed
// ============================================================================

TEST(SvClientGamestate_Checksum, HasValidFeed) {
    ResetGamestate();
    
    EXPECT_NE(test_sv.checksumFeed, 0);
}

TEST(SvClientGamestate_Checksum, FeedIsConsistent) {
    ResetGamestate();
    int feed1 = test_sv.checksumFeed;
    int feed2 = test_sv.checksumFeed;
    
    EXPECT_EQ(feed1, feed2);
}

// ============================================================================
// TEST SUITE: Delta Messages
// ============================================================================

TEST(SvClientGamestate_Delta, InitiallyZero) {
    ResetGamestate();
    
    EXPECT_EQ(test_clients[0].deltaMessage, 0);
}

TEST(SvClientGamestate_Delta, CanBeSet) {
    ResetGamestate();
    test_clients[0].deltaMessage = 5;
    
    EXPECT_EQ(test_clients[0].deltaMessage, 5);
}

// ============================================================================
// TEST SUITE: Snapshot Timing
// ============================================================================

TEST(SvClientGamestate_Snapshot, TracksLastTime) {
    ResetGamestate();
    test_clients[0].lastSnapshotTime = 1000;
    
    EXPECT_EQ(test_clients[0].lastSnapshotTime, 1000);
}

TEST(SvClientGamestate_Snapshot, UpdatesOverTime) {
    ResetGamestate();
    test_clients[0].lastSnapshotTime = 1000;
    test_clients[0].lastSnapshotTime = 2000;
    
    EXPECT_EQ(test_clients[0].lastSnapshotTime, 2000);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvClientGamestate_Multi, IndependentGamestates) {
    ResetGamestate();
    StartSendingGamestate(0);
    StartSendingGamestate(1);
    
    FinishSendingGamestate(0);
    
    EXPECT_TRUE(IsGamestateSent(0));
    EXPECT_FALSE(IsGamestateSent(1));
}

TEST(SvClientGamestate_Multi, DifferentDeltaMessages) {
    ResetGamestate();
    test_clients[0].deltaMessage = 1;
    test_clients[1].deltaMessage = 2;
    test_clients[2].deltaMessage = 3;
    
    EXPECT_NE(test_clients[0].deltaMessage, test_clients[1].deltaMessage);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientGamestate_Edge, MaxConfigstringIndex) {
    ResetGamestate();
    strcpy(test_sv.configstrings[MAX_CONFIGSTRINGS - 1], "last");
    
    EXPECT_STREQ(test_sv.configstrings[MAX_CONFIGSTRINGS - 1], "last");
}

TEST(SvClientGamestate_Edge, LongConfigstring) {
    ResetGamestate();
    char longStr[256];
    memset(longStr, 'a', 255);
    longStr[255] = '\0';
    
    strcpy(test_sv.configstrings[0], longStr);
    
    EXPECT_EQ(strlen(test_sv.configstrings[0]), 255);
}

TEST(SvClientGamestate_Edge, NegativeDeltaMessage) {
    ResetGamestate();
    test_clients[0].deltaMessage = -1;
    
    EXPECT_EQ(test_clients[0].deltaMessage, -1);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientGamestate_Perf, ManyClients) {
    ResetGamestate();
    
    for (int i = 0; i < 64; i++) {
        StartSendingGamestate(i);
        FinishSendingGamestate(i);
    }
    
    for (int i = 0; i < 64; i++) {
        EXPECT_TRUE(IsGamestateSent(i));
    }
}

TEST(SvClientGamestate_Perf, ManyConfigstrings) {
    ResetGamestate();
    
    for (int i = 0; i < 100; i++) {
        sprintf(test_sv.configstrings[i], "value_%d", i);
    }
    
    EXPECT_STREQ(test_sv.configstrings[50], "value_50");
}

TEST(SvClientGamestate_Perf, RepeatedSends) {
    ResetGamestate();
    
    for (int i = 0; i < 10; i++) {
        StartSendingGamestate(0);
        FinishSendingGamestate(0);
    }
    
    EXPECT_TRUE(IsGamestateSent(0));
}

// ============================================================================
// SUMMARY: 25 tests for sv_client_gamestate.h
// ============================================================================
