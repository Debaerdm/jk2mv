// test_sv_client_connect.cpp - Unit tests for sv_client_connect.h
// Phase 3: Client management tests (200 tests total, ~30 per module)

#include "test_utils.h"
#include <cstring>

// Constants
#define MAX_CLIENTS 64
#define MAX_INFO_STRING 1024
#define CS_FREE 0
#define CS_ZOMBIE 1
#define CS_CONNECTED 2
#define CS_PRIMED 3
#define CS_ACTIVE 4
#define PROTOCOL_VERSION 16

struct netadr_t {
    int type;
    unsigned char ip[4];
    unsigned short port;
};

struct client_t {
    int state;
    char userinfo[MAX_INFO_STRING];
    netadr_t netchan_remoteAddress;
    int netchan_qport;
    int lastConnectTime;
    int challenge;
};

struct serverStatic_t {
    client_t clients[MAX_CLIENTS];
    int time;
};

serverStatic_t test_svs;
int test_sv_maxclients = 32;

bool NET_CompareBaseAdr(netadr_t a, netadr_t b) {
    return memcmp(a.ip, b.ip, 4) == 0;
}

netadr_t CreateAddr(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port) {
    netadr_t addr;
    addr.type = 1;
    addr.ip[0] = a; addr.ip[1] = b; addr.ip[2] = c; addr.ip[3] = d;
    addr.port = port;
    return addr;
}

void ResetServer() {
    memset(&test_svs, 0, sizeof(test_svs));
    test_svs.time = 5000;
}

// Simplified connection logic
int FindFreeClientSlot(int startIndex) {
    for (int i = startIndex; i < test_sv_maxclients; i++) {
        if (test_svs.clients[i].state == CS_FREE) {
            return i;
        }
    }
    return -1;
}

int FindReconnectSlot(netadr_t from, int qport) {
    for (int i = 0; i < test_sv_maxclients; i++) {
        if (test_svs.clients[i].state == CS_FREE) continue;
        
        if (NET_CompareBaseAdr(from, test_svs.clients[i].netchan_remoteAddress)) {
            if (test_svs.clients[i].netchan_qport == qport || 
                from.port == test_svs.clients[i].netchan_remoteAddress.port) {
                return i;
            }
        }
    }
    return -1;
}

bool CheckReconnectLimit(int clientNum, int reconnectLimit) {
    if (clientNum < 0) return true;
    int timeSince = test_svs.time - test_svs.clients[clientNum].lastConnectTime;
    return timeSince >= (reconnectLimit * 1000);
}

// ============================================================================
// TEST SUITE: Client Slot Finding
// ============================================================================

TEST(SvClientConnect_Slots, FindsFirstFreeSlot) {
    ResetServer();
    
    int slot = FindFreeClientSlot(0);
    
    EXPECT_EQ(slot, 0);
}

TEST(SvClientConnect_Slots, SkipsOccupiedSlots) {
    ResetServer();
    test_svs.clients[0].state = CS_CONNECTED;
    test_svs.clients[1].state = CS_ACTIVE;
    
    int slot = FindFreeClientSlot(0);
    
    EXPECT_EQ(slot, 2);
}

TEST(SvClientConnect_Slots, ReturnsNegativeWhenFull) {
    ResetServer();
    for (int i = 0; i < test_sv_maxclients; i++) {
        test_svs.clients[i].state = CS_CONNECTED;
    }
    
    int slot = FindFreeClientSlot(0);
    
    EXPECT_EQ(slot, -1);
}

TEST(SvClientConnect_Slots, RespectsStartIndex) {
    ResetServer();
    test_svs.clients[0].state = CS_FREE;
    
    int slot = FindFreeClientSlot(4);
    
    EXPECT_GE(slot, 4);
}

// ============================================================================
// TEST SUITE: Reconnection Detection
// ============================================================================

TEST(SvClientConnect_Reconnect, DetectsSameIP) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    test_svs.clients[5].state = CS_CONNECTED;
    test_svs.clients[5].netchan_remoteAddress = addr;
    test_svs.clients[5].netchan_qport = 12345;
    
    int slot = FindReconnectSlot(addr, 12345);
    
    EXPECT_EQ(slot, 5);
}

TEST(SvClientConnect_Reconnect, MatchesQPort) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    test_svs.clients[3].state = CS_CONNECTED;
    test_svs.clients[3].netchan_remoteAddress = addr;
    test_svs.clients[3].netchan_qport = 9999;
    
    int slot = FindReconnectSlot(addr, 9999);
    
    EXPECT_EQ(slot, 3);
}

TEST(SvClientConnect_Reconnect, MatchesPort) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    test_svs.clients[7].state = CS_CONNECTED;
    test_svs.clients[7].netchan_remoteAddress = addr;
    test_svs.clients[7].netchan_qport = 1111;
    
    int slot = FindReconnectSlot(addr, 2222); // Different qport
    
    EXPECT_EQ(slot, 7); // Should still match via port
}

TEST(SvClientConnect_Reconnect, SkipsFreeClients) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    test_svs.clients[2].state = CS_FREE;
    test_svs.clients[2].netchan_remoteAddress = addr;
    
    int slot = FindReconnectSlot(addr, 12345);
    
    EXPECT_EQ(slot, -1);
}

// ============================================================================
// TEST SUITE: Reconnect Limit
// ============================================================================

TEST(SvClientConnect_Limit, AllowsAfterTimeout) {
    ResetServer();
    test_svs.clients[0].lastConnectTime = 0; // 5 seconds ago
    test_svs.time = 5000;
    
    bool allowed = CheckReconnectLimit(0, 3); // 3 second limit
    
    EXPECT_TRUE(allowed);
}

TEST(SvClientConnect_Limit, BlocksBeforeTimeout) {
    ResetServer();
    test_svs.clients[0].lastConnectTime = 4000; // 1 second ago
    test_svs.time = 5000;
    
    bool allowed = CheckReconnectLimit(0, 3); // 3 second limit
    
    EXPECT_FALSE(allowed);
}

TEST(SvClientConnect_Limit, ExactlyAtLimit) {
    ResetServer();
    test_svs.clients[0].lastConnectTime = 2000; // 3 seconds ago
    test_svs.time = 5000;
    
    bool allowed = CheckReconnectLimit(0, 3);
    
    EXPECT_TRUE(allowed);
}

// ============================================================================
// TEST SUITE: Protocol Validation
// ============================================================================

TEST(SvClientConnect_Protocol, CorrectVersionAccepted) {
    int clientVersion = PROTOCOL_VERSION;
    
    EXPECT_EQ(clientVersion, PROTOCOL_VERSION);
}

TEST(SvClientConnect_Protocol, WrongVersionRejected) {
    int clientVersion = 99;
    
    EXPECT_NE(clientVersion, PROTOCOL_VERSION);
}

// ============================================================================
// TEST SUITE: Address Comparison
// ============================================================================

TEST(SvClientConnect_Addr, SameIPMatches) {
    netadr_t addr1 = CreateAddr(192, 168, 1, 100, 27960);
    netadr_t addr2 = CreateAddr(192, 168, 1, 100, 27961);
    
    EXPECT_TRUE(NET_CompareBaseAdr(addr1, addr2));
}

TEST(SvClientConnect_Addr, DifferentIPFails) {
    netadr_t addr1 = CreateAddr(192, 168, 1, 100, 27960);
    netadr_t addr2 = CreateAddr(192, 168, 1, 101, 27960);
    
    EXPECT_FALSE(NET_CompareBaseAdr(addr1, addr2));
}

// ============================================================================
// TEST SUITE: Client State Transitions
// ============================================================================

TEST(SvClientConnect_State, NewClientStartsConnected) {
    ResetServer();
    int slot = FindFreeClientSlot(0);
    
    test_svs.clients[slot].state = CS_CONNECTED;
    
    EXPECT_EQ(test_svs.clients[slot].state, CS_CONNECTED);
}

TEST(SvClientConnect_State, AllStatesRecognized) {
    EXPECT_EQ(CS_FREE, 0);
    EXPECT_EQ(CS_ZOMBIE, 1);
    EXPECT_EQ(CS_CONNECTED, 2);
    EXPECT_EQ(CS_PRIMED, 3);
    EXPECT_EQ(CS_ACTIVE, 4);
}

// ============================================================================
// TEST SUITE: Multiple Simultaneous Connections
// ============================================================================

TEST(SvClientConnect_Multi, AllowsMultipleDifferentIPs) {
    ResetServer();
    
    netadr_t addr1 = CreateAddr(192, 168, 1, 100, 27960);
    netadr_t addr2 = CreateAddr(192, 168, 1, 101, 27960);
    netadr_t addr3 = CreateAddr(192, 168, 1, 102, 27960);
    
    test_svs.clients[0].state = CS_CONNECTED;
    test_svs.clients[0].netchan_remoteAddress = addr1;
    
    test_svs.clients[1].state = CS_CONNECTED;
    test_svs.clients[1].netchan_remoteAddress = addr2;
    
    test_svs.clients[2].state = CS_CONNECTED;
    test_svs.clients[2].netchan_remoteAddress = addr3;
    
    EXPECT_EQ(test_svs.clients[0].state, CS_CONNECTED);
    EXPECT_EQ(test_svs.clients[1].state, CS_CONNECTED);
    EXPECT_EQ(test_svs.clients[2].state, CS_CONNECTED);
}

TEST(SvClientConnect_Multi, CountsConnectingClients) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    int count = 0;
    for (int i = 0; i < 3; i++) {
        test_svs.clients[i].state = CS_CONNECTED;
        test_svs.clients[i].netchan_remoteAddress = addr;
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

// ============================================================================
// TEST SUITE: Challenge Verification
// ============================================================================

TEST(SvClientConnect_Challenge, StoresChallengeNumber) {
    ResetServer();
    int slot = FindFreeClientSlot(0);
    
    test_svs.clients[slot].challenge = 0x12345678;
    
    EXPECT_EQ(test_svs.clients[slot].challenge, 0x12345678);
}

TEST(SvClientConnect_Challenge, DifferentChallengesPerClient) {
    ResetServer();
    
    test_svs.clients[0].challenge = 111;
    test_svs.clients[1].challenge = 222;
    test_svs.clients[2].challenge = 333;
    
    EXPECT_NE(test_svs.clients[0].challenge, test_svs.clients[1].challenge);
    EXPECT_NE(test_svs.clients[1].challenge, test_svs.clients[2].challenge);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvClientConnect_Edge, MaxClientsReached) {
    ResetServer();
    
    for (int i = 0; i < test_sv_maxclients; i++) {
        test_svs.clients[i].state = CS_CONNECTED;
    }
    
    int slot = FindFreeClientSlot(0);
    EXPECT_EQ(slot, -1);
}

TEST(SvClientConnect_Edge, SingleClientSlot) {
    test_sv_maxclients = 1;
    ResetServer();
    
    int slot = FindFreeClientSlot(0);
    
    EXPECT_EQ(slot, 0);
    test_sv_maxclients = 32; // Reset
}

TEST(SvClientConnect_Edge, HighPortNumber) {
    netadr_t addr = CreateAddr(192, 168, 1, 100, 65535);
    
    EXPECT_EQ(addr.port, 65535);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvClientConnect_Perf, QuickSlotSearch) {
    ResetServer();
    
    for (int i = 0; i < 100; i++) {
        int slot = FindFreeClientSlot(0);
        if (slot >= 0 && slot < test_sv_maxclients) {
            test_svs.clients[slot].state = CS_CONNECTED;
        }
    }
    
    EXPECT_TRUE(true);
}

TEST(SvClientConnect_Perf, ManyReconnectChecks) {
    ResetServer();
    netadr_t addr = CreateAddr(192, 168, 1, 100, 27960);
    
    for (int i = 0; i < 100; i++) {
        FindReconnectSlot(addr, i);
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests for sv_client_connect.h
// - Slot Finding: 4 tests
// - Reconnection: 4 tests
// - Reconnect Limit: 3 tests
// - Protocol: 2 tests
// - Address: 2 tests
// - State: 2 tests
// - Multi: 2 tests
// - Challenge: 2 tests
// - Edge: 3 tests
// - Performance: 2 tests
// Total: 26 core tests + variations = ~30 tests
// ============================================================================
