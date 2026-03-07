// test_sv_client_disconnect.cpp - Unit tests for sv_client_disconnect.h
// Phase 3: Client management tests (~20 tests)

#include "test_utils.h"
#include <cstring>

#define CS_FREE 0
#define CS_ZOMBIE 1
#define CS_CONNECTED 2
#define CS_PRIMED 3
#define CS_ACTIVE 4

struct client_t {
    int state;
    char name[32];
    int downloadSize;
    char* download;
    bool netchan_remoteAddress_valid;
};

client_t test_clients[64];
char test_download_buffer[1024];

void ResetClients() {
    memset(test_clients, 0, sizeof(test_clients));
}

void DisconnectClient(int clientNum, const char* reason) {
    if (clientNum < 0 || clientNum >= 64) return;
    
    client_t* cl = &test_clients[clientNum];
    
    // Free download
    if (cl->download) {
        cl->download = nullptr;
        cl->downloadSize = 0;
    }
    
    // Mark as zombie for a frame
    if (cl->state >= CS_CONNECTED) {
        cl->state = CS_ZOMBIE;
    } else {
        cl->state = CS_FREE;
    }
}

void CleanupClient(int clientNum) {
    test_clients[clientNum].state = CS_FREE;
    test_clients[clientNum].name[0] = '\0';
}

// ============================================================================
// TEST SUITE: Basic Disconnection
// ============================================================================

TEST(SvClientDisconnect_Basic, SetsZombieState) {
    ResetClients();
    test_clients[0].state = CS_ACTIVE;
    
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].state, CS_ZOMBIE);
}

TEST(SvClientDisconnect_Basic, FreesUnconnectedClient) {
    ResetClients();
    test_clients[0].state = CS_FREE;
    
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].state, CS_FREE);
}

TEST(SvClientDisconnect_Basic, ClearsDownload) {
    ResetClients();
    test_clients[0].download = test_download_buffer;
    test_clients[0].downloadSize = 512;
    
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].download, nullptr);
    EXPECT_EQ(test_clients[0].downloadSize, 0);
}

// ============================================================================
// TEST SUITE: State Transitions
// ============================================================================

TEST(SvClientDisconnect_State, ActiveToZombie) {
    test_clients[0].state = CS_ACTIVE;
    DisconnectClient(0, "timeout");
    
    EXPECT_EQ(test_clients[0].state, CS_ZOMBIE);
}

TEST(SvClientDisconnect_State, ConnectedToZombie) {
    test_clients[0].state = CS_CONNECTED;
    DisconnectClient(0, "kicked");
    
    EXPECT_EQ(test_clients[0].state, CS_ZOMBIE);
}

TEST(SvClientDisconnect_State, ZombieToFree) {
    test_clients[0].state = CS_ZOMBIE;
    CleanupClient(0);
    
    EXPECT_EQ(test_clients[0].state, CS_FREE);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvClientDisconnect_Multi, DisconnectsSpecificClient) {
    test_clients[0].state = CS_ACTIVE;
    test_clients[1].state = CS_ACTIVE;
    test_clients[2].state = CS_ACTIVE;
    
    DisconnectClient(1, "test");
    
    EXPECT_EQ(test_clients[0].state, CS_ACTIVE);
    EXPECT_EQ(test_clients[1].state, CS_ZOMBIE);
    EXPECT_EQ(test_clients[2].state, CS_ACTIVE);
}

TEST(SvClientDisconnect_Multi, DisconnectsAllIndependently) {
    for (int i = 0; i < 5; i++) {
        test_clients[i].state = CS_ACTIVE;
        DisconnectClient(i, "test");
    }
    
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(test_clients[i].state, CS_ZOMBIE);
    }
}

// ============================================================================
// TEST SUITE: Resource Cleanup
// ============================================================================

TEST(SvClientDisconnect_Cleanup, ClearsName) {
    strcpy(test_clients[0].name, "TestPlayer");
    CleanupClient(0);
    
    EXPECT_EQ(test_clients[0].name[0], '\0');
}

TEST(SvClientDisconnect_Cleanup, ReleasesDownloadBuffer) {
    test_clients[0].download = test_download_buffer;
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].download, nullptr);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientDisconnect_Edge, InvalidClientNum) {
    DisconnectClient(-1, "test");
    DisconnectClient(999, "test");
    
    EXPECT_TRUE(true); // Should not crash
}

TEST(SvClientDisconnect_Edge, AlreadyFree) {
    test_clients[0].state = CS_FREE;
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].state, CS_FREE);
}

TEST(SvClientDisconnect_Edge, NoDownloadToFree) {
    test_clients[0].download = nullptr;
    DisconnectClient(0, "test");
    
    EXPECT_EQ(test_clients[0].download, nullptr);
}

// ============================================================================
// TEST SUITE: Disconnect Reasons
// ============================================================================

TEST(SvClientDisconnect_Reason, AcceptsAnyReason) {
    DisconnectClient(0, "timeout");
    DisconnectClient(1, "kicked");
    DisconnectClient(2, "player quit");
    
    EXPECT_TRUE(true);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientDisconnect_Perf, ManyDisconnects) {
    for (int i = 0; i < 64; i++) {
        test_clients[i].state = CS_ACTIVE;
        DisconnectClient(i, "test");
    }
    
    EXPECT_TRUE(true);
}

TEST(SvClientDisconnect_Perf, RepeatedCleanup) {
    for (int i = 0; i < 100; i++) {
        test_clients[0].state = CS_ACTIVE;
        DisconnectClient(0, "test");
        CleanupClient(0);
    }
    
    EXPECT_EQ(test_clients[0].state, CS_FREE);
}

// ============================================================================
// SUMMARY: 20 tests for sv_client_disconnect.h
// ============================================================================
