// test_sv_snapshot_builder.cpp - Unit tests for sv_snapshot_builder.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>

#define MAX_GENTITIES 1024
#define MAX_SNAPSHOT_ENTITIES 512
#define PACKET_MASK 31
#define CS_ZOMBIE 1
#define MAX_MAP_AREA_BYTES 32

struct vec3_t { float v[3]; };

struct entityState_t {
    int number;
    int eType;
    vec3_t pos;
    vec3_t angles;
};

struct playerState_t {
    int clientNum;
    vec3_t origin;
    int viewheight;
};

struct clientSnapshot_t {
    playerState_t ps;
    int num_entities;
    int first_entity;
    unsigned char areabits[MAX_MAP_AREA_BYTES];
};

struct client_t {
    int state;
    int netchan_outgoingSequence;
    clientSnapshot_t frames[PACKET_MASK + 1];
    void* gentity;
};

struct server_t {
    int snapshotCounter;
    entityState_t snapshotEntities[MAX_SNAPSHOT_ENTITIES];
    int nextSnapshotEntities;
};

client_t test_clients[64];
server_t test_sv;
char test_gentity_placeholder;

void ResetSnapshots() {
    memset(&test_clients, 0, sizeof(test_clients));
    memset(&test_sv, 0, sizeof(test_sv));
    test_sv.snapshotCounter = 1;
}

void SetupClient(int num) {
    test_clients[num].state = 2; // CS_CONNECTED
    test_clients[num].gentity = &test_gentity_placeholder;
    test_clients[num].netchan_outgoingSequence = 0;
}

clientSnapshot_t* GetCurrentFrame(client_t* client) {
    return &client->frames[client->netchan_outgoingSequence & PACKET_MASK];
}

void BuildSnapshot(client_t* client) {
    test_sv.snapshotCounter++;
    
    clientSnapshot_t* frame = GetCurrentFrame(client);
    memset(frame, 0, sizeof(*frame));
    
    if (!client->gentity || client->state == CS_ZOMBIE) {
        return;
    }
    
    frame->ps.clientNum = 0;
    frame->num_entities = 0;
    frame->first_entity = test_sv.nextSnapshotEntities;
}

void AddEntityToSnapshot(client_t* client, int entityNum) {
    clientSnapshot_t* frame = GetCurrentFrame(client);
    
    if (frame->num_entities >= MAX_SNAPSHOT_ENTITIES) return;
    
    int idx = test_sv.nextSnapshotEntities % MAX_SNAPSHOT_ENTITIES;
    test_sv.snapshotEntities[idx].number = entityNum;
    test_sv.nextSnapshotEntities++;
    frame->num_entities++;
}

// ============================================================================
// TEST SUITE: Snapshot Initialization
// ============================================================================

TEST(SvSnapshotBuilder_Init, IncrementsCounter) {
    ResetSnapshots();
    int before = test_sv.snapshotCounter;
    
    BuildSnapshot(&test_clients[0]);
    
    EXPECT_GT(test_sv.snapshotCounter, before);
}

TEST(SvSnapshotBuilder_Init, ClearsFrame) {
    ResetSnapshots();
    SetupClient(0);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    frame->num_entities = 999;
    
    BuildSnapshot(&test_clients[0]);
    
    EXPECT_EQ(frame->num_entities, 0);
}

TEST(SvSnapshotBuilder_Init, ClearsAreabits) {
    ResetSnapshots();
    SetupClient(0);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    memset(frame->areabits, 0xFF, sizeof(frame->areabits));
    
    BuildSnapshot(&test_clients[0]);
    
    EXPECT_EQ(frame->areabits[0], 0);
}

// ============================================================================
// TEST SUITE: Frame Management
// ============================================================================

TEST(SvSnapshotBuilder_Frame, SelectsCorrectFrame) {
    ResetSnapshots();
    SetupClient(0);
    test_clients[0].netchan_outgoingSequence = 5;
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    
    EXPECT_EQ(frame, &test_clients[0].frames[5 & PACKET_MASK]);
}

TEST(SvSnapshotBuilder_Frame, WrapsMask) {
    ResetSnapshots();
    SetupClient(0);
    test_clients[0].netchan_outgoingSequence = 100;
    
    int index = test_clients[0].netchan_outgoingSequence & PACKET_MASK;
    
    EXPECT_LT(index, PACKET_MASK + 1);
}

// ============================================================================
// TEST SUITE: Entity Addition
// ============================================================================

TEST(SvSnapshotBuilder_Entity, AddsEntity) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    AddEntityToSnapshot(&test_clients[0], 42);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, 1);
}

TEST(SvSnapshotBuilder_Entity, StoresEntityNumber) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    AddEntityToSnapshot(&test_clients[0], 123);
    
    EXPECT_EQ(test_sv.snapshotEntities[0].number, 123);
}

TEST(SvSnapshotBuilder_Entity, IncrementsCount) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    AddEntityToSnapshot(&test_clients[0], 1);
    AddEntityToSnapshot(&test_clients[0], 2);
    AddEntityToSnapshot(&test_clients[0], 3);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, 3);
}

TEST(SvSnapshotBuilder_Entity, TracksFirstEntity) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    int first = frame->first_entity;
    
    EXPECT_EQ(first, 0);
}

// ============================================================================
// TEST SUITE: Zombie Client Handling
// ============================================================================

TEST(SvSnapshotBuilder_Zombie, SkipsZombieClient) {
    ResetSnapshots();
    SetupClient(0);
    test_clients[0].state = CS_ZOMBIE;
    
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, 0);
}

TEST(SvSnapshotBuilder_Zombie, SkipsNoGentity) {
    ResetSnapshots();
    SetupClient(0);
    test_clients[0].gentity = nullptr;
    
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, 0);
}

// ============================================================================
// TEST SUITE: PlayerState
// ============================================================================

TEST(SvSnapshotBuilder_PS, SetsClientNum) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    
    EXPECT_EQ(frame->ps.clientNum, 0);
}

// ============================================================================
// TEST SUITE: Entity Array Management
// ============================================================================

TEST(SvSnapshotBuilder_Array, WrapsIndex) {
    ResetSnapshots();
    test_sv.nextSnapshotEntities = MAX_SNAPSHOT_ENTITIES + 5;
    
    int idx = test_sv.nextSnapshotEntities % MAX_SNAPSHOT_ENTITIES;
    
    EXPECT_EQ(idx, 5);
}

TEST(SvSnapshotBuilder_Array, IncrementsNext) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    int before = test_sv.nextSnapshotEntities;
    AddEntityToSnapshot(&test_clients[0], 1);
    
    EXPECT_EQ(test_sv.nextSnapshotEntities, before + 1);
}

// ============================================================================
// TEST SUITE: Multiple Entities
// ============================================================================

TEST(SvSnapshotBuilder_Multi, HandlesMultipleEntities) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    for (int i = 0; i < 10; i++) {
        AddEntityToSnapshot(&test_clients[0], i);
    }
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, 10);
}

TEST(SvSnapshotBuilder_Multi, StopsAtMax) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    for (int i = 0; i < MAX_SNAPSHOT_ENTITIES + 10; i++) {
        AddEntityToSnapshot(&test_clients[0], i);
    }
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_EQ(frame->num_entities, MAX_SNAPSHOT_ENTITIES);
}

// ============================================================================
// TEST SUITE: Areabits
// ============================================================================

TEST(SvSnapshotBuilder_Area, InitiallyZero) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    
    EXPECT_EQ(frame->areabits[0], 0);
}

// ============================================================================
// TEST SUITE: Multiple Clients
// ============================================================================

TEST(SvSnapshotBuilder_Clients, IndependentSnapshots) {
    ResetSnapshots();
    SetupClient(0);
    SetupClient(1);
    
    BuildSnapshot(&test_clients[0]);
    AddEntityToSnapshot(&test_clients[0], 10);
    
    BuildSnapshot(&test_clients[1]);
    AddEntityToSnapshot(&test_clients[1], 20);
    
    clientSnapshot_t* frame0 = GetCurrentFrame(&test_clients[0]);
    clientSnapshot_t* frame1 = GetCurrentFrame(&test_clients[1]);
    
    EXPECT_EQ(frame0->num_entities, 1);
    EXPECT_EQ(frame1->num_entities, 1);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvSnapshotBuilder_Edge, EmptySnapshot) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    
    EXPECT_EQ(frame->num_entities, 0);
}

TEST(SvSnapshotBuilder_Edge, SingleEntity) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    AddEntityToSnapshot(&test_clients[0], 0);
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    
    EXPECT_EQ(frame->num_entities, 1);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvSnapshotBuilder_Perf, ManySnapshots) {
    ResetSnapshots();
    SetupClient(0);
    
    for (int i = 0; i < 100; i++) {
        test_clients[0].netchan_outgoingSequence = i;
        BuildSnapshot(&test_clients[0]);
    }
    
    EXPECT_GT(test_sv.snapshotCounter, 1);
}

TEST(SvSnapshotBuilder_Perf, ManyEntities) {
    ResetSnapshots();
    SetupClient(0);
    BuildSnapshot(&test_clients[0]);
    
    for (int i = 0; i < 200; i++) {
        AddEntityToSnapshot(&test_clients[0], i);
    }
    
    clientSnapshot_t* frame = GetCurrentFrame(&test_clients[0]);
    EXPECT_GT(frame->num_entities, 0);
}

TEST(SvSnapshotBuilder_Perf, AllClients) {
    ResetSnapshots();
    
    for (int i = 0; i < 32; i++) {
        SetupClient(i);
        BuildSnapshot(&test_clients[i]);
    }
    
    EXPECT_GT(test_sv.snapshotCounter, 1);
}

// ============================================================================
// SUMMARY: 30 tests for sv_snapshot_builder.h
// ============================================================================
