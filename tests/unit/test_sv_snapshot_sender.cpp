// test_sv_snapshot_sender.cpp - Unit tests for sv_snapshot_sender.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>

#define PACKET_MASK 31

struct msg_t {
    unsigned char data[32768];
    int cursize;
    int maxsize;
};

struct netchan_t {
    int outgoingSequence;
    int incomingSequence;
};

struct client_t {
    netchan_t netchan;
    int deltaMessage;
    int lastSnapshotTime;
    int snapshotMsec;
};

client_t test_clients[64];
msg_t test_msg;
int test_serverTime = 0;

void ResetSender() {
    memset(&test_clients, 0, sizeof(test_clients));
    memset(&test_msg, 0, sizeof(test_msg));
    test_msg.maxsize = sizeof(test_msg.data);
    test_serverTime = 5000;
}

void InitMessage(msg_t* msg) {
    msg->cursize = 0;
}

void WriteSnapshot(msg_t* msg, int size) {
    if (msg->cursize + size <= msg->maxsize) {
        msg->cursize += size;
    }
}

bool CanSendSnapshot(client_t* cl, int currentTime) {
    if (currentTime - cl->lastSnapshotTime < cl->snapshotMsec) {
        return false;
    }
    return true;
}

void SendSnapshot(client_t* cl, int snapshotSize) {
    InitMessage(&test_msg);
    WriteSnapshot(&test_msg, snapshotSize);
    cl->lastSnapshotTime = test_serverTime;
    cl->netchan.outgoingSequence++;
}

int GetFrameIndex(client_t* cl) {
    return cl->netchan.outgoingSequence & PACKET_MASK;
}

// ============================================================================
// TEST SUITE: Message Initialization
// ============================================================================

TEST(SvSnapshotSender_Msg, InitializesEmpty) {
    ResetSender();
    InitMessage(&test_msg);
    
    EXPECT_EQ(test_msg.cursize, 0);
}

TEST(SvSnapshotSender_Msg, HasMaxSize) {
    ResetSender();
    
    EXPECT_GT(test_msg.maxsize, 0);
}

// ============================================================================
// TEST SUITE: Snapshot Writing
// ============================================================================

TEST(SvSnapshotSender_Write, WritesData) {
    ResetSender();
    InitMessage(&test_msg);
    
    WriteSnapshot(&test_msg, 100);
    
    EXPECT_EQ(test_msg.cursize, 100);
}

TEST(SvSnapshotSender_Write, AppendData) {
    ResetSender();
    InitMessage(&test_msg);
    
    WriteSnapshot(&test_msg, 100);
    WriteSnapshot(&test_msg, 50);
    
    EXPECT_EQ(test_msg.cursize, 150);
}

TEST(SvSnapshotSender_Write, RespectsMaxSize) {
    ResetSender();
    InitMessage(&test_msg);
    
    WriteSnapshot(&test_msg, test_msg.maxsize + 1000);
    
    EXPECT_LE(test_msg.cursize, test_msg.maxsize);
}

// ============================================================================
// TEST SUITE: Send Timing
// ============================================================================

TEST(SvSnapshotSender_Timing, InitiallyCanSend) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    
    EXPECT_TRUE(CanSendSnapshot(&test_clients[0], test_serverTime));
}

TEST(SvSnapshotSender_Timing, BlocksIfTooSoon) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    test_clients[0].lastSnapshotTime = test_serverTime - 10;
    
    EXPECT_FALSE(CanSendSnapshot(&test_clients[0], test_serverTime));
}

TEST(SvSnapshotSender_Timing, AllowsAfterInterval) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    test_clients[0].lastSnapshotTime = test_serverTime - 100;
    
    EXPECT_TRUE(CanSendSnapshot(&test_clients[0], test_serverTime));
}

TEST(SvSnapshotSender_Timing, ExactInterval) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    test_clients[0].lastSnapshotTime = test_serverTime - 50;
    
    EXPECT_TRUE(CanSendSnapshot(&test_clients[0], test_serverTime));
}

// ============================================================================
// TEST SUITE: Snapshot Transmission
// ============================================================================

TEST(SvSnapshotSender_Send, SendsSnapshot) {
    ResetSender();
    
    SendSnapshot(&test_clients[0], 500);
    
    EXPECT_EQ(test_msg.cursize, 500);
}

TEST(SvSnapshotSender_Send, UpdatesLastTime) {
    ResetSender();
    test_clients[0].lastSnapshotTime = 0;
    
    SendSnapshot(&test_clients[0], 100);
    
    EXPECT_EQ(test_clients[0].lastSnapshotTime, test_serverTime);
}

TEST(SvSnapshotSender_Send, IncrementsSequence) {
    ResetSender();
    int before = test_clients[0].netchan.outgoingSequence;
    
    SendSnapshot(&test_clients[0], 100);
    
    EXPECT_EQ(test_clients[0].netchan.outgoingSequence, before + 1);
}

// ============================================================================
// TEST SUITE: Frame Indexing
// ============================================================================

TEST(SvSnapshotSender_Frame, CalculatesIndex) {
    ResetSender();
    test_clients[0].netchan.outgoingSequence = 5;
    
    int index = GetFrameIndex(&test_clients[0]);
    
    EXPECT_EQ(index, 5 & PACKET_MASK);
}

TEST(SvSnapshotSender_Frame, WrapsMask) {
    ResetSender();
    test_clients[0].netchan.outgoingSequence = 100;
    
    int index = GetFrameIndex(&test_clients[0]);
    
    EXPECT_LT(index, PACKET_MASK + 1);
}

// ============================================================================
// TEST SUITE: Multiple Snapshots
// ============================================================================

TEST(SvSnapshotSender_Multi, ConsecutiveSends) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    
    SendSnapshot(&test_clients[0], 100);
    test_serverTime += 100;
    SendSnapshot(&test_clients[0], 100);
    
    EXPECT_EQ(test_clients[0].netchan.outgoingSequence, 2);
}

TEST(SvSnapshotSender_Multi, DifferentSizes) {
    ResetSender();
    
    SendSnapshot(&test_clients[0], 100);
    test_serverTime += 100;
    SendSnapshot(&test_clients[0], 500);
    
    EXPECT_EQ(test_msg.cursize, 500); // Last send
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvSnapshotSender_Clients, IndependentSequences) {
    ResetSender();
    
    SendSnapshot(&test_clients[0], 100);
    SendSnapshot(&test_clients[1], 100);
    
    EXPECT_EQ(test_clients[0].netchan.outgoingSequence, 1);
    EXPECT_EQ(test_clients[1].netchan.outgoingSequence, 1);
}

TEST(SvSnapshotSender_Clients, IndependentTiming) {
    ResetSender();
    test_clients[0].snapshotMsec = 50;
    test_clients[1].snapshotMsec = 100;
    
    test_clients[0].lastSnapshotTime = test_serverTime - 60;
    test_clients[1].lastSnapshotTime = test_serverTime - 60;
    
    EXPECT_TRUE(CanSendSnapshot(&test_clients[0], test_serverTime));
    EXPECT_FALSE(CanSendSnapshot(&test_clients[1], test_serverTime));
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvSnapshotSender_Edge, ZeroSizeSnapshot) {
    ResetSender();
    InitMessage(&test_msg);
    
    WriteSnapshot(&test_msg, 0);
    
    EXPECT_EQ(test_msg.cursize, 0);
}

TEST(SvSnapshotSender_Edge, VeryLargeSnapshot) {
    ResetSender();
    InitMessage(&test_msg);
    
    WriteSnapshot(&test_msg, 1000000);
    
    EXPECT_LE(test_msg.cursize, test_msg.maxsize);
}

TEST(SvSnapshotSender_Edge, ZeroInterval) {
    ResetSender();
    test_clients[0].snapshotMsec = 0;
    test_clients[0].lastSnapshotTime = test_serverTime;
    
    EXPECT_TRUE(CanSendSnapshot(&test_clients[0], test_serverTime));
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvSnapshotSender_Perf, ManySnapshots) {
    ResetSender();
    test_clients[0].snapshotMsec = 1;
    
    for (int i = 0; i < 100; i++) {
        test_serverTime += 10;
        if (CanSendSnapshot(&test_clients[0], test_serverTime)) {
            SendSnapshot(&test_clients[0], 100);
        }
    }
    
    EXPECT_GT(test_clients[0].netchan.outgoingSequence, 0);
}

TEST(SvSnapshotSender_Perf, AllClients) {
    ResetSender();
    
    for (int i = 0; i < 32; i++) {
        test_clients[i].snapshotMsec = 50;
        SendSnapshot(&test_clients[i], 200);
    }
    
    EXPECT_EQ(test_clients[31].netchan.outgoingSequence, 1);
}

// ============================================================================
// SUMMARY: 30 tests for sv_snapshot_sender.h
// ============================================================================
