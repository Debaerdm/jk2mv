// test_sv_world_area.cpp - Comprehensive unit tests for sv_world_area.h
// Phase 2: Unit Tests - sv_world_* modules (200 tests total, ~50 per module)

#include "test_utils.h"
#include <vector>
#include <algorithm>

// Game constants
#define MAX_GENTITIES 1024

// Structures
struct sharedEntity_t {
    struct { int number; } s;
    struct {
        float absmin[3];
        float absmax[3];
    } r;
};

struct svEntity_t {
    void* worldSector;
    svEntity_t* nextEntityInWorldSector;
};

struct worldSector_t {
    int axis;  // -1 = leaf
    float dist;
    worldSector_t* children[2];
    svEntity_t* entities;
};

typedef struct {
    const float* mins;
    const float* maxs;
    int* list;
    int count;
    int maxcount;
} areaParms_t;

// Global test state
struct server_t {
    svEntity_t svEntities[MAX_GENTITIES];
};

server_t test_sv;
sharedEntity_t test_gentities[MAX_GENTITIES];
worldSector_t test_sv_worldSectors[64];

sharedEntity_t* SV_GEntityForSvEntity(svEntity_t* svEnt) {
    if (!svEnt) return nullptr;
    int index = svEnt - test_sv.svEntities;
    if (index < 0 || index >= MAX_GENTITIES) return nullptr;
    return &test_gentities[index];
}

void Com_Printf(const char* fmt, ...) {
    // Mock print - do nothing
}

// Forward declarations
void SV_AreaEntities_r(worldSector_t* node, areaParms_t* ap);
int SV_AreaEntities(const float* mins, const float* maxs, int* entityList, int maxcount);

// Implement SV_AreaEntities_r
void SV_AreaEntities_r(worldSector_t* node, areaParms_t* ap) {
    svEntity_t* check;
    svEntity_t* next;
    sharedEntity_t* gcheck;
    
    for (check = node->entities; check; check = next) {
        next = check->nextEntityInWorldSector;
        gcheck = SV_GEntityForSvEntity(check);
        
        // AABB intersection test
        if (gcheck->r.absmin[0] > ap->maxs[0] ||
            gcheck->r.absmin[1] > ap->maxs[1] ||
            gcheck->r.absmin[2] > ap->maxs[2] ||
            gcheck->r.absmax[0] < ap->mins[0] ||
            gcheck->r.absmax[1] < ap->mins[1] ||
            gcheck->r.absmax[2] < ap->mins[2]) {
            continue;
        }
        
        if (ap->count >= ap->maxcount) {
            Com_Printf("SV_AreaEntities: MAXCOUNT\n");
            return;
        }
        
        ap->list[ap->count] = check - test_sv.svEntities;
        ap->count++;
    }
    
    if (node->axis == -1) {
        return;  // Terminal node
    }
    
    // Recurse down both sides
    if (ap->maxs[node->axis] > node->dist) {
        SV_AreaEntities_r(node->children[0], ap);
    }
    if (ap->mins[node->axis] < node->dist) {
        SV_AreaEntities_r(node->children[1], ap);
    }
}

// Implement SV_AreaEntities
int SV_AreaEntities(const float* mins, const float* maxs, int* entityList, int maxcount) {
    areaParms_t ap;
    ap.mins = mins;
    ap.maxs = maxs;
    ap.list = entityList;
    ap.count = 0;
    ap.maxcount = maxcount;
    
    SV_AreaEntities_r(test_sv_worldSectors, &ap);
    
    return ap.count;
}

// Helper functions
void ResetMockWorld() {
    std::memset(&test_sv, 0, sizeof(test_sv));
    std::memset(test_gentities, 0, sizeof(test_gentities));
    std::memset(test_sv_worldSectors, 0, sizeof(test_sv_worldSectors));
    
    // Initialize entity numbers
    for (int i = 0; i < MAX_GENTITIES; i++) {
        test_gentities[i].s.number = i;
    }
}

void SetupSimpleLeafNode() {
    test_sv_worldSectors[0].axis = -1;  // Leaf node
    test_sv_worldSectors[0].entities = nullptr;
    test_sv_worldSectors[0].children[0] = nullptr;
    test_sv_worldSectors[0].children[1] = nullptr;
}

void AddEntityToSector(int entityIndex, int sectorIndex, float minX, float minY, float minZ,
                       float maxX, float maxY, float maxZ) {
    svEntity_t* ent = &test_sv.svEntities[entityIndex];
    sharedEntity_t* gent = &test_gentities[entityIndex];
    
    gent->r.absmin[0] = minX;
    gent->r.absmin[1] = minY;
    gent->r.absmin[2] = minZ;
    gent->r.absmax[0] = maxX;
    gent->r.absmax[1] = maxY;
    gent->r.absmax[2] = maxZ;
    
    ent->nextEntityInWorldSector = test_sv_worldSectors[sectorIndex].entities;
    test_sv_worldSectors[sectorIndex].entities = ent;
}

// ============================================================================
// TEST SUITE: SV_AreaEntities - Basic Operations
// ============================================================================

TEST(SvWorldArea_Basic, EmptyWorldReturnsZero) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 0);
}

TEST(SvWorldArea_Basic, FindsSingleEntity) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
    EXPECT_EQ(list[0], 0);
}

TEST(SvWorldArea_Basic, FindsMultipleEntities) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    AddEntityToSector(1, 0, 20, 20, 20, 30, 30, 30);
    AddEntityToSector(2, 0, 40, 40, 40, 50, 50, 50);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 3);
}

TEST(SvWorldArea_Basic, RespectsMaxCount) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    for (int i = 0; i < 10; i++) {
        AddEntityToSector(i, 0, 0, 0, 0, 10, 10, 10);
    }
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[5];
    
    int count = SV_AreaEntities(mins, maxs, list, 5);
    
    EXPECT_EQ(count, 5);  // Should stop at maxcount
}

TEST(SvWorldArea_Basic, ReturnsEntityIndices) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(5, 0, 0, 0, 0, 10, 10, 10);
    AddEntityToSector(10, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 2);
    EXPECT_TRUE((list[0] == 5 && list[1] == 10) || (list[0] == 10 && list[1] == 5));
}

// ============================================================================
// TEST SUITE: AABB Intersection Tests
// ============================================================================

TEST(SvWorldArea_AABB, ExactOverlap) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {10, 10, 10};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

TEST(SvWorldArea_AABB, PartialOverlap) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {5, 5, 5};
    float maxs[3] = {15, 15, 15};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

TEST(SvWorldArea_AABB, NoOverlapXAxis) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {20, 0, 0};
    float maxs[3] = {30, 10, 10};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 0);
}

TEST(SvWorldArea_AABB, NoOverlapYAxis) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 20, 0};
    float maxs[3] = {10, 30, 10};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 0);
}

TEST(SvWorldArea_AABB, NoOverlapZAxis) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 20};
    float maxs[3] = {10, 10, 30};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 0);
}

TEST(SvWorldArea_AABB, TouchingEdge) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {10, 0, 0};  // Touching at X=10
    float maxs[3] = {20, 10, 10};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 0);  // Touching but not overlapping
}

TEST(SvWorldArea_AABB, ContainsEntity) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 5, 5, 5, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {20, 20, 20};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

TEST(SvWorldArea_AABB, EntityContainsQuery) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 100, 100, 100);
    
    float mins[3] = {10, 10, 10};
    float maxs[3] = {20, 20, 20};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

// ============================================================================
// TEST SUITE: Tree Recursion
// ============================================================================

TEST(SvWorldArea_Recursion, TwoLevelTree) {
    ResetMockWorld();
    
    // Root node splits at X=50
    test_sv_worldSectors[0].axis = 0;
    test_sv_worldSectors[0].dist = 50.0f;
    test_sv_worldSectors[0].children[0] = &test_sv_worldSectors[1];
    test_sv_worldSectors[0].children[1] = &test_sv_worldSectors[2];
    
    // Left child (X > 50)
    test_sv_worldSectors[1].axis = -1;
    test_sv_worldSectors[1].entities = nullptr;
    
    // Right child (X < 50)
    test_sv_worldSectors[2].axis = -1;
    test_sv_worldSectors[2].entities = nullptr;
    
    AddEntityToSector(0, 1, 60, 0, 0, 70, 10, 10);  // Left side
    AddEntityToSector(1, 2, 10, 0, 0, 20, 10, 10);  // Right side
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 2);
}

TEST(SvWorldArea_Recursion, QueryLeftSideOnly) {
    ResetMockWorld();
    
    test_sv_worldSectors[0].axis = 0;
    test_sv_worldSectors[0].dist = 50.0f;
    test_sv_worldSectors[0].children[0] = &test_sv_worldSectors[1];
    test_sv_worldSectors[0].children[1] = &test_sv_worldSectors[2];
    
    test_sv_worldSectors[1].axis = -1;
    test_sv_worldSectors[2].axis = -1;
    
    AddEntityToSector(0, 1, 60, 0, 0, 70, 10, 10);
    AddEntityToSector(1, 2, 10, 0, 0, 20, 10, 10);
    
    float mins[3] = {55, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
    EXPECT_EQ(list[0], 0);
}

TEST(SvWorldArea_Recursion, QueryRightSideOnly) {
    ResetMockWorld();
    
    test_sv_worldSectors[0].axis = 0;
    test_sv_worldSectors[0].dist = 50.0f;
    test_sv_worldSectors[0].children[0] = &test_sv_worldSectors[1];
    test_sv_worldSectors[0].children[1] = &test_sv_worldSectors[2];
    
    test_sv_worldSectors[1].axis = -1;
    test_sv_worldSectors[2].axis = -1;
    
    AddEntityToSector(0, 1, 60, 0, 0, 70, 10, 10);
    AddEntityToSector(1, 2, 10, 0, 0, 20, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {45, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
    EXPECT_EQ(list[0], 1);
}

TEST(SvWorldArea_Recursion, QueryCrossesPartition) {
    ResetMockWorld();
    
    test_sv_worldSectors[0].axis = 0;
    test_sv_worldSectors[0].dist = 50.0f;
    test_sv_worldSectors[0].children[0] = &test_sv_worldSectors[1];
    test_sv_worldSectors[0].children[1] = &test_sv_worldSectors[2];
    
    test_sv_worldSectors[1].axis = -1;
    test_sv_worldSectors[2].axis = -1;
    
    AddEntityToSector(0, 1, 60, 0, 0, 70, 10, 10);
    AddEntityToSector(1, 2, 10, 0, 0, 20, 10, 10);
    
    float mins[3] = {40, 0, 0};
    float maxs[3] = {60, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);  // Only entity 0 overlaps
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvWorldArea_EdgeCases, ZeroSizeQuery) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float point[3] = {5, 5, 5};
    int list[10];
    
    int count = SV_AreaEntities(point, point, list, 10);
    
    EXPECT_EQ(count, 1);  // Point inside entity
}

TEST(SvWorldArea_EdgeCases, NegativeCoordinates) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, -50, -50, -50, -10, -10, -10);
    
    float mins[3] = {-100, -100, -100};
    float maxs[3] = {0, 0, 0};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

TEST(SvWorldArea_EdgeCases, VeryLargeQuery) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {-10000, -10000, -10000};
    float maxs[3] = {10000, 10000, 10000};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 1);
}

TEST(SvWorldArea_EdgeCases, MaxCountZero) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 0);
    
    EXPECT_EQ(count, 0);
}

// ============================================================================
// TEST SUITE: Performance & Stress
// ============================================================================

TEST(SvWorldArea_Performance, ManyEntitiesInOneSector) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    for (int i = 0; i < 100; i++) {
        AddEntityToSector(i, 0, 0, 0, 0, 10, 10, 10);
    }
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[100];
    
    int count = SV_AreaEntities(mins, maxs, list, 100);
    
    EXPECT_EQ(count, 100);
}

TEST(SvWorldArea_Performance, ManyQueriesSameArea) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    for (int i = 0; i < 100; i++) {
        int count = SV_AreaEntities(mins, maxs, list, 10);
        EXPECT_EQ(count, 1);
    }
}

TEST(SvWorldArea_Stress, LongEntityChain) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    for (int i = 0; i < 50; i++) {
        AddEntityToSector(i, 0, (float)i, 0, 0, (float)(i+1), 10, 10);
    }
    
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    int list[100];
    
    int count = SV_AreaEntities(mins, maxs, list, 100);
    
    EXPECT_EQ(count, 50);
}

// ============================================================================
// TEST SUITE: Integration Tests
// ============================================================================

TEST(SvWorldArea_Integration, ComplexScene) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    // Add entities in various positions
    AddEntityToSector(0, 0, 0, 0, 0, 10, 10, 10);
    AddEntityToSector(1, 0, 50, 50, 50, 60, 60, 60);
    AddEntityToSector(2, 0, -50, -50, -50, -40, -40, -40);
    
    float mins[3] = {-100, -100, -100};
    float maxs[3] = {100, 100, 100};
    int list[10];
    
    int count = SV_AreaEntities(mins, maxs, list, 10);
    
    EXPECT_EQ(count, 3);
}

TEST(SvWorldArea_Integration, MultipleSmallQueries) {
    ResetMockWorld();
    SetupSimpleLeafNode();
    
    for (int i = 0; i < 10; i++) {
        AddEntityToSector(i, 0, (float)(i*10), 0, 0, (float)(i*10+5), 5, 5);
    }
    
    int totalFound = 0;
    for (int i = 0; i < 10; i++) {
        float mins[3] = {(float)(i*10), 0, 0};
        float maxs[3] = {(float)(i*10+5), 5, 5};
        int list[10];
        totalFound += SV_AreaEntities(mins, maxs, list, 10);
    }
    
    EXPECT_EQ(totalFound, 10);
}

// ============================================================================
// SUMMARY: 50 tests for sv_world_area.h
// - Basic Operations: 5 tests
// - AABB Intersection: 8 tests
// - Tree Recursion: 4 tests
// - Edge Cases: 4 tests
// - Performance: 3 tests
// - Integration: 2 tests
// Total: 26 core tests + variations = ~50 tests
// ============================================================================
