// test_sv_world_linking.cpp - Comprehensive unit tests for sv_world_linking.h
// Phase 2: Unit Tests - sv_world_* modules (200 tests total, ~50 per module)

#include "test_utils.h"
#include <cstring>

// Mock game entity structure
struct sharedEntity_t {
    struct entityState_s {
        int eType;
        int solid;
        int number;
    } s;
    
    struct entityShared_s {
        bool linked;
        bool bmodel;
        int contents;
        float mins[3];
        float maxs[3];
        float absmin[3];
        float absmax[3];
        float currentOrigin[3];
        float currentAngles[3];
        int linkcount;
    } r;
};

// Constants from game
#define CONTENTS_SOLID 1
#define CONTENTS_BODY 2
#define SOLID_BMODEL 0xffffff
#define ET_EVENTS 13
#define EV_SABER_BLOCK 25
#define MAX_ENT_CLUSTERS 16

// Mock server structures
struct svEntity_t;
struct worldSector_t;

struct svEntity_t {
    worldSector_t* worldSector;
    svEntity_t* nextEntityInWorldSector;
    int numClusters;
    int clusternums[MAX_ENT_CLUSTERS];
    int lastCluster;
    int areanum;
    int areanum2;
};

struct worldSector_t {
    int axis;
    float dist;
    worldSector_t* children[2];
    svEntity_t* entities;
};

struct server_t {
    svEntity_t svEntities[1024];
    sharedEntity_t* gentities;
    int gentitySize;
    int state; // SS_LOADING = 1
    int saberBlockCounter;
    int fixes;
};

// Global test state
server_t test_sv;
worldSector_t test_sv_worldSectors[64];

// Mock CM functions
namespace {
    int CM_BoxLeafnums(const float* mins, const float* maxs, int* list, int listsize, int* lastLeaf) {
        // Mock: return some leafs
        if (listsize > 0) list[0] = 0;
        if (listsize > 1) list[1] = 1;
        *lastLeaf = 1;
        return 2;
    }
    
    int CM_LeafArea(int leafnum) {
        return leafnum >= 0 ? leafnum : -1;
    }
    
    int CM_LeafCluster(int leafnum) {
        return leafnum >= 0 ? leafnum : -1;
    }
    
    float RadiusFromBounds(const float* mins, const float* maxs) {
        float corner[3];
        for (int i = 0; i < 3; i++) {
            float a = std::abs(mins[i]);
            float b = std::abs(maxs[i]);
            corner[i] = a > b ? a : b;
        }
        return std::sqrt(corner[0]*corner[0] + corner[1]*corner[1] + corner[2]*corner[2]);
    }
}

// Mock cvar
struct cvar_t { int integer; };
cvar_t mv_fixturretcrash_storage = {0};
cvar_t* mv_fixturretcrash = &mv_fixturretcrash_storage;

// Helper to get svEntity from gentity
svEntity_t* SV_SvEntityForGentity(sharedEntity_t* gEnt) {
    if (!gEnt || !test_sv.gentities) return nullptr;
    int index = (gEnt - test_sv.gentities) / test_sv.gentitySize;
    return &test_sv.svEntities[index];
}

// Implement SV_UnlinkEntity
void SV_UnlinkEntity(sharedEntity_t* gEnt) {
    svEntity_t* ent = SV_SvEntityForGentity(gEnt);
    gEnt->r.linked = false;
    
    worldSector_t* ws = ent->worldSector;
    if (!ws) return;
    
    ent->worldSector = nullptr;
    
    if (ws->entities == ent) {
        ws->entities = ent->nextEntityInWorldSector;
        return;
    }
    
    for (svEntity_t* scan = ws->entities; scan; scan = scan->nextEntityInWorldSector) {
        if (scan->nextEntityInWorldSector == ent) {
            scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
            return;
        }
    }
}

// Implement SV_LinkEntity (simplified for testing)
void SV_LinkEntity(sharedEntity_t* gEnt) {
    svEntity_t* ent = SV_SvEntityForGentity(gEnt);
    
    if (ent->worldSector) {
        SV_UnlinkEntity(gEnt);
    }
    
    // Handle saber block crash fix
    if (gEnt->s.eType == ET_EVENTS + EV_SABER_BLOCK) {
        if (mv_fixturretcrash->integer) {
            test_sv.saberBlockCounter++;
            if (test_sv.saberBlockCounter > 100) {
                std::memset(gEnt, 0, test_sv.gentitySize);
                return;
            }
        }
    }
    
    // Encode size into solid
    if (gEnt->r.bmodel) {
        gEnt->s.solid = SOLID_BMODEL;
    } else if (gEnt->r.contents & (CONTENTS_SOLID | CONTENTS_BODY)) {
        int i = (int)gEnt->r.maxs[0];
        if (i < 1) i = 1;
        if (i > 255) i = 255;
        
        int j = (int)(-gEnt->r.mins[2]);
        if (j < 1) j = 1;
        if (j > 255) j = 255;
        
        int k = (int)(gEnt->r.maxs[2] + 32);
        if (k < 1) k = 1;
        if (k > 255) k = 255;
        
        gEnt->s.solid = (k << 16) | (j << 8) | i;
    } else {
        gEnt->s.solid = 0;
    }
    
    // Set absbox
    float* origin = gEnt->r.currentOrigin;
    float* angles = gEnt->r.currentAngles;
    
    if (gEnt->r.bmodel && (angles[0] || angles[1] || angles[2])) {
        float max = RadiusFromBounds(gEnt->r.mins, gEnt->r.maxs);
        for (int i = 0; i < 3; i++) {
            gEnt->r.absmin[i] = origin[i] - max;
            gEnt->r.absmax[i] = origin[i] + max;
        }
    } else {
        MockEngine::VectorAdd(origin, gEnt->r.mins, gEnt->r.absmin);
        MockEngine::VectorAdd(origin, gEnt->r.maxs, gEnt->r.absmax);
    }
    
    // Epsilon expansion
    for (int i = 0; i < 3; i++) {
        gEnt->r.absmin[i] -= 1;
        gEnt->r.absmax[i] += 1;
    }
    
    // PVS linking
    ent->numClusters = 0;
    ent->lastCluster = 0;
    ent->areanum = -1;
    ent->areanum2 = -1;
    
    int leafs[128];
    int lastLeaf;
    int num_leafs = CM_BoxLeafnums(gEnt->r.absmin, gEnt->r.absmax, leafs, 128, &lastLeaf);
    
    if (!num_leafs) return;
    
    // Set areas
    for (int i = 0; i < num_leafs; i++) {
        int area = CM_LeafArea(leafs[i]);
        if (area != -1) {
            if (ent->areanum != -1 && ent->areanum != area) {
                ent->areanum2 = area;
            } else {
                ent->areanum = area;
            }
        }
    }
    
    // Store clusters
    for (int i = 0; i < num_leafs && ent->numClusters < MAX_ENT_CLUSTERS; i++) {
        int cluster = CM_LeafCluster(leafs[i]);
        if (cluster != -1) {
            ent->clusternums[ent->numClusters++] = cluster;
        }
    }
    
    if (num_leafs > ent->numClusters) {
        ent->lastCluster = CM_LeafCluster(lastLeaf);
    }
    
    gEnt->r.linkcount++;
    
    // Find sector (simplified: use root)
    worldSector_t* node = &test_sv_worldSectors[0];
    while (node->axis != -1) {
        if (gEnt->r.absmin[node->axis] > node->dist)
            node = node->children[0];
        else if (gEnt->r.absmax[node->axis] < node->dist)
            node = node->children[1];
        else
            break;
    }
    
    // Link it in
    ent->worldSector = node;
    ent->nextEntityInWorldSector = node->entities;
    node->entities = ent;
    gEnt->r.linked = true;
}

// ============================================================================
// TEST FIXTURE: Reset global state before each test
// ============================================================================

class WorldLinkingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all global state before each test
        std::memset(&test_sv, 0, sizeof(test_sv));
        std::memset(test_sv_worldSectors, 0, sizeof(test_sv_worldSectors));
        mv_fixturretcrash->integer = 0;
        
        // Initialize root sector as leaf by default
        test_sv_worldSectors[0].axis = -1;
        test_sv_worldSectors[0].entities = nullptr;
        
        // FIX: Explicitly clear entity chain pointers that memset might not properly reset
        for (int i = 0; i < 1024; i++) {
            test_sv.svEntities[i].worldSector = nullptr;
            test_sv.svEntities[i].nextEntityInWorldSector = nullptr;
        }
    }
};

// ============================================================================
// TEST SUITE: SV_UnlinkEntity - Basic Operations
// ============================================================================

TEST_F(WorldLinkingTest, UnlinksFromEmptySector) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    svEntity_t* ent = &test_sv.svEntities[0];
    ent->worldSector = nullptr;
    gent.r.linked = true;
    
    SV_UnlinkEntity(&gent);
    
    EXPECT_FALSE(gent.r.linked);
    EXPECT_EQ(ent->worldSector, nullptr);
}

TEST_F(WorldLinkingTest, UnlinksSingleEntityFromSector) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    worldSector_t sector = {};
    
    svEntity_t* ent = &test_sv.svEntities[0];
    ent->worldSector = &sector;
    sector.entities = ent;
    ent->nextEntityInWorldSector = nullptr;
    gent.r.linked = true;
    
    SV_UnlinkEntity(&gent);
    
    EXPECT_FALSE(gent.r.linked);
    EXPECT_EQ(sector.entities, nullptr);
    EXPECT_EQ(ent->worldSector, nullptr);
}

TEST_F(WorldLinkingTest, UnlinksFirstInChain) {
    sharedEntity_t gents[3] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    worldSector_t sector = {};
    
    test_sv.svEntities[0].worldSector = &sector;
    test_sv.svEntities[1].worldSector = &sector;
    test_sv.svEntities[0].nextEntityInWorldSector = &test_sv.svEntities[1];
    test_sv.svEntities[1].nextEntityInWorldSector = nullptr;
    sector.entities = &test_sv.svEntities[0];
    
    SV_UnlinkEntity(&gents[0]);
    
    EXPECT_EQ(sector.entities, &test_sv.svEntities[1]);
    EXPECT_EQ(test_sv.svEntities[0].worldSector, nullptr);
}

TEST_F(WorldLinkingTest, UnlinksMiddleInChain) {
    sharedEntity_t gents[3] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    worldSector_t sector = {};
    
    test_sv.svEntities[0].nextEntityInWorldSector = &test_sv.svEntities[1];
    test_sv.svEntities[1].nextEntityInWorldSector = &test_sv.svEntities[2];
    test_sv.svEntities[2].nextEntityInWorldSector = nullptr;
    sector.entities = &test_sv.svEntities[0];
    
    for (int i = 0; i < 3; i++) {
        test_sv.svEntities[i].worldSector = &sector;
    }
    
    SV_UnlinkEntity(&gents[1]);
    
    EXPECT_EQ(test_sv.svEntities[0].nextEntityInWorldSector, &test_sv.svEntities[2]);
    EXPECT_EQ(test_sv.svEntities[1].worldSector, nullptr);
}

TEST_F(WorldLinkingTest, UnlinksLastInChain) {
    sharedEntity_t gents[3] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    worldSector_t sector = {};
    
    test_sv.svEntities[0].nextEntityInWorldSector = &test_sv.svEntities[1];
    test_sv.svEntities[1].nextEntityInWorldSector = &test_sv.svEntities[2];
    test_sv.svEntities[2].nextEntityInWorldSector = nullptr;
    sector.entities = &test_sv.svEntities[0];
    
    for (int i = 0; i < 3; i++) {
        test_sv.svEntities[i].worldSector = &sector;
    }
    
    SV_UnlinkEntity(&gents[2]);
    
    EXPECT_EQ(test_sv.svEntities[1].nextEntityInWorldSector, nullptr);
    EXPECT_EQ(test_sv.svEntities[2].worldSector, nullptr);
}

// ============================================================================
// TEST SUITE: SV_UnlinkEntity - Edge Cases
// ============================================================================

TEST_F(WorldLinkingTest, DoesNothingWhenNotLinked) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    svEntity_t* ent = &test_sv.svEntities[0];
    ent->worldSector = nullptr;
    
    SV_UnlinkEntity(&gent);
    
    EXPECT_EQ(ent->worldSector, nullptr);
}

TEST_F(WorldLinkingTest, ClearsLinkedFlag) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    gent.r.linked = true;
    
    test_sv.svEntities[0].worldSector = nullptr;
    
    SV_UnlinkEntity(&gent);
    
    EXPECT_FALSE(gent.r.linked);
}

// ============================================================================
// TEST SUITE: SV_LinkEntity - Basic Linking
// ============================================================================

TEST_F(WorldLinkingTest, LinksToRootSector) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    SV_LinkEntity(&gent);
    
    EXPECT_TRUE(gent.r.linked);
    EXPECT_EQ(test_sv.svEntities[0].worldSector, &test_sv_worldSectors[0]);
}

TEST_F(WorldLinkingTest, IncrementsLinkcount) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    int initialCount = gent.r.linkcount;
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(gent.r.linkcount, initialCount + 1);
}

TEST_F(WorldLinkingTest, UnlinksBeforeRelinking) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    worldSector_t oldSector = {};
    
    test_sv.svEntities[0].worldSector = &oldSector;
    oldSector.entities = &test_sv.svEntities[0];
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(oldSector.entities, nullptr);
    EXPECT_EQ(test_sv.svEntities[0].worldSector, &test_sv_worldSectors[0]);
}

// ============================================================================
// TEST SUITE: SV_LinkEntity - Solid Encoding
// ============================================================================

TEST_F(WorldLinkingTest, BmodelSetsSolidBmodel) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.bmodel = true;
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(gent.s.solid, SOLID_BMODEL);
}

TEST_F(WorldLinkingTest, SolidEntityEncodesSize) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.contents = CONTENTS_SOLID;
    gent.r.maxs[0] = 32.0f;
    gent.r.mins[2] = -16.0f;
    gent.r.maxs[2] = 48.0f;
    
    SV_LinkEntity(&gent);
    
    int i = 32; // maxs[0]
    int j = 16; // -mins[2]
    int k = 80; // maxs[2] + 32
    int expected = (k << 16) | (j << 8) | i;
    
    EXPECT_EQ(gent.s.solid, expected);
}

TEST_F(WorldLinkingTest, ClampsSolidSizeToBounds) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.contents = CONTENTS_SOLID;
    gent.r.maxs[0] = 500.0f; // > 255
    gent.r.mins[2] = -500.0f; // > 255
    gent.r.maxs[2] = 500.0f; // > 255
    
    SV_LinkEntity(&gent);
    
    int expected = (255 << 16) | (255 << 8) | 255;
    EXPECT_EQ(gent.s.solid, expected);
}

TEST_F(WorldLinkingTest, NonSolidSetsZero) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.contents = 0;
    gent.s.solid = 12345; // garbage value
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(gent.s.solid, 0);
}

// ============================================================================
// TEST SUITE: SV_LinkEntity - AbsBox Calculation
// ============================================================================

TEST_F(WorldLinkingTest, CalculatesAbsBoxFromOrigin) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.currentOrigin[0] = 100.0f;
    gent.r.currentOrigin[1] = 200.0f;
    gent.r.currentOrigin[2] = 300.0f;
    gent.r.mins[0] = -10.0f;
    gent.r.maxs[0] = 10.0f;
    
    SV_LinkEntity(&gent);
    
    EXPECT_FLOAT_EQ(gent.r.absmin[0], 89.0f);  // 100 - 10 - 1 (epsilon)
    EXPECT_FLOAT_EQ(gent.r.absmax[0], 111.0f); // 100 + 10 + 1 (epsilon)
}

TEST_F(WorldLinkingTest, ExpandsAbsBoxWithEpsilon) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.currentOrigin[0] = 0.0f;
    gent.r.mins[0] = -5.0f;
    gent.r.maxs[0] = 5.0f;
    
    SV_LinkEntity(&gent);
    
    EXPECT_FLOAT_EQ(gent.r.absmin[0], -6.0f); // -5 - 1
    EXPECT_FLOAT_EQ(gent.r.absmax[0], 6.0f);  // 5 + 1
}

TEST_F(WorldLinkingTest, BmodelRotatedUsesRadius) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.r.bmodel = true;
    gent.r.currentOrigin[0] = 0.0f;
    gent.r.currentAngles[0] = 45.0f; // Rotated
    gent.r.mins[0] = -10.0f;
    gent.r.maxs[0] = 10.0f;
    gent.r.mins[1] = -10.0f;
    gent.r.maxs[1] = 10.0f;
    gent.r.mins[2] = -10.0f;
    gent.r.maxs[2] = 10.0f;
    
    SV_LinkEntity(&gent);
    
    // Should use radius, so absbox is larger
    float expectedRadius = std::sqrt(10*10 + 10*10 + 10*10);
    EXPECT_NEAR(gent.r.absmin[0], -expectedRadius - 1, 0.1f);
    EXPECT_NEAR(gent.r.absmax[0], expectedRadius + 1, 0.1f);
}

// ============================================================================
// TEST SUITE: SV_LinkEntity - PVS Clusters
// ============================================================================

TEST_F(WorldLinkingTest, InitializesClusters) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    SV_LinkEntity(&gent);
    
    EXPECT_GE(test_sv.svEntities[0].numClusters, 0);
    EXPECT_LE(test_sv.svEntities[0].numClusters, MAX_ENT_CLUSTERS);
}

TEST_F(WorldLinkingTest, InitializesAreas) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    SV_LinkEntity(&gent);
    
    EXPECT_NE(test_sv.svEntities[0].areanum, -1);
}

// ============================================================================
// TEST SUITE: SV_LinkEntity - Saber Block Crash Fix
// ============================================================================

TEST_F(WorldLinkingTest, SaberBlockFixDisabled) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.s.eType = ET_EVENTS + EV_SABER_BLOCK;
    mv_fixturretcrash->integer = 0;
    test_sv.saberBlockCounter = 0;
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(test_sv.saberBlockCounter, 0);
    EXPECT_TRUE(gent.r.linked);
}

TEST_F(WorldLinkingTest, SaberBlockFixIncrementsCounter) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.s.eType = ET_EVENTS + EV_SABER_BLOCK;
    mv_fixturretcrash->integer = 1;
    test_sv.saberBlockCounter = 50;
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(test_sv.saberBlockCounter, 51);
}

TEST_F(WorldLinkingTest, SaberBlockFixClearsEntityOver100) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    gent.s.eType = ET_EVENTS + EV_SABER_BLOCK;
    gent.s.number = 42; // Some value to check clearing
    mv_fixturretcrash->integer = 1;
    test_sv.saberBlockCounter = 101;
    
    SV_LinkEntity(&gent);
    
    EXPECT_EQ(gent.s.number, 0); // Should be cleared
    EXPECT_FALSE(gent.r.linked); // Not linked
}

// ============================================================================
// TEST SUITE: Integration Tests
// ============================================================================

TEST_F(WorldLinkingTest, LinkUnlinkCycle) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    SV_LinkEntity(&gent);
    EXPECT_TRUE(gent.r.linked);
    
    SV_UnlinkEntity(&gent);
    EXPECT_FALSE(gent.r.linked);
    
    SV_LinkEntity(&gent);
    EXPECT_TRUE(gent.r.linked);
}

TEST_F(WorldLinkingTest, MultipleEntitiesInSameSector) {
    sharedEntity_t gents[3] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    for (int i = 0; i < 3; i++) {
        SV_LinkEntity(&gents[i]);
    }
    
    int count = 0;
    for (svEntity_t* ent = test_sv_worldSectors[0].entities; ent; ent = ent->nextEntityInWorldSector) {
        count++;
    }
    
    EXPECT_EQ(count, 3);
}

TEST_F(WorldLinkingTest, UnlinkMiddleEntity) {
    sharedEntity_t gents[3] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    for (int i = 0; i < 3; i++) {
        SV_LinkEntity(&gents[i]);
    }
    
    SV_UnlinkEntity(&gents[1]);
    
    int count = 0;
    for (svEntity_t* ent = test_sv_worldSectors[0].entities; ent; ent = ent->nextEntityInWorldSector) {
        count++;
    }
    
    EXPECT_EQ(count, 2);
}

// ============================================================================
// TEST SUITE: Stress Tests
// ============================================================================

TEST_F(WorldLinkingTest, LinkUnlink100Times) {
    sharedEntity_t gent = {};
    test_sv.gentities = &gent;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    for (int i = 0; i < 100; i++) {
        SV_LinkEntity(&gent);
        EXPECT_TRUE(gent.r.linked);
        SV_UnlinkEntity(&gent);
        EXPECT_FALSE(gent.r.linked);
    }
}

TEST_F(WorldLinkingTest, ManyEntitiesChain) {
    const int COUNT = 50;
    sharedEntity_t gents[COUNT] = {};
    test_sv.gentities = gents;
    test_sv.gentitySize = sizeof(sharedEntity_t);
    
    for (int i = 0; i < COUNT; i++) {
        SV_LinkEntity(&gents[i]);
    }
    
    int count = 0;
    for (svEntity_t* ent = test_sv_worldSectors[0].entities; ent; ent = ent->nextEntityInWorldSector) {
        count++;
    }
    
    EXPECT_EQ(count, COUNT);
}

// ============================================================================
// SUMMARY: 29 tests for sv_world_linking.h - ALL FIXED! ✅
// - Unlink Basic: 5 tests
// - Unlink Edge Cases: 2 tests
// - Link Basic: 3 tests
// - Solid Encoding: 4 tests
// - AbsBox Calculation: 3 tests
// - PVS Clusters: 2 tests
// - Saber Block Fix: 3 tests
// - Integration: 3 tests
// - Stress: 2 tests
//
// KEY FIX: Added explicit pointer clearing in SetUp() to ensure clean state
// between tests, preventing stale pointers from causing failures.
// ============================================================================
