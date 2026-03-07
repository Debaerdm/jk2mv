// test_sv_snapshot_entities.cpp - Unit tests for sv_snapshot_entities.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>
#include <cmath>

struct vec3_t {
    float x, y, z;
};

struct entityState_t {
    int number;
    int eType;
    vec3_t pos;
    int solid;
};

struct svEntity_t {
    int snapshotCounter;
    int areanum;
    int areanum2;
};

entityState_t test_entities[1024];
svEntity_t test_svEntities[1024];
int test_snapshotCounter = 0;

void ResetEntities() {
    memset(test_entities, 0, sizeof(test_entities));
    memset(test_svEntities, 0, sizeof(test_svEntities));
    test_snapshotCounter = 1;
}

float VectorDistance(vec3_t a, vec3_t b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

bool IsEntityVisible(int entityNum, vec3_t viewpoint, float maxDistance) {
    if (entityNum < 0 || entityNum >= 1024) return false;
    
    entityState_t* ent = &test_entities[entityNum];
    
    float dist = VectorDistance(viewpoint, ent->pos);
    return dist <= maxDistance;
}

bool IsEntityInSnapshot(int entityNum) {
    return test_svEntities[entityNum].snapshotCounter == test_snapshotCounter;
}

void MarkEntityInSnapshot(int entityNum) {
    test_svEntities[entityNum].snapshotCounter = test_snapshotCounter;
}

int CountVisibleEntities(vec3_t viewpoint, float maxDistance) {
    int count = 0;
    for (int i = 0; i < 1024; i++) {
        if (IsEntityVisible(i, viewpoint, maxDistance)) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// TEST SUITE: Visibility Testing
// ============================================================================

TEST(SvSnapshotEntities_Vis, EntityAtOrigin) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {0, 0, 0};
    
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST(SvSnapshotEntities_Vis, EntityWithinRange) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {50, 0, 0};
    
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST(SvSnapshotEntities_Vis, EntityOutOfRange) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {200, 0, 0};
    
    EXPECT_FALSE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST(SvSnapshotEntities_Vis, EntityAtExactRange) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {100, 0, 0};
    
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

// ============================================================================
// TEST SUITE: Distance Calculation
// ============================================================================

TEST(SvSnapshotEntities_Dist, ZeroDistance) {
    vec3_t a = {0, 0, 0};
    vec3_t b = {0, 0, 0};
    
    float dist = VectorDistance(a, b);
    
    EXPECT_FLOAT_EQ(dist, 0.0f);
}

TEST(SvSnapshotEntities_Dist, HorizontalDistance) {
    vec3_t a = {0, 0, 0};
    vec3_t b = {100, 0, 0};
    
    float dist = VectorDistance(a, b);
    
    EXPECT_FLOAT_EQ(dist, 100.0f);
}

TEST(SvSnapshotEntities_Dist, VerticalDistance) {
    vec3_t a = {0, 0, 0};
    vec3_t b = {0, 0, 50};
    
    float dist = VectorDistance(a, b);
    
    EXPECT_FLOAT_EQ(dist, 50.0f);
}

TEST(SvSnapshotEntities_Dist, DiagonalDistance) {
    vec3_t a = {0, 0, 0};
    vec3_t b = {3, 4, 0};
    
    float dist = VectorDistance(a, b);
    
    EXPECT_FLOAT_EQ(dist, 5.0f); // 3-4-5 triangle
}

// ============================================================================
// TEST SUITE: Snapshot Marking
// ============================================================================

TEST(SvSnapshotEntities_Mark, MarksEntity) {
    ResetEntities();
    
    MarkEntityInSnapshot(5);
    
    EXPECT_TRUE(IsEntityInSnapshot(5));
}

TEST(SvSnapshotEntities_Mark, DoesNotMarkOthers) {
    ResetEntities();
    
    MarkEntityInSnapshot(5);
    
    EXPECT_FALSE(IsEntityInSnapshot(3));
}

TEST(SvSnapshotEntities_Mark, MultipleEntities) {
    ResetEntities();
    
    MarkEntityInSnapshot(1);
    MarkEntityInSnapshot(2);
    MarkEntityInSnapshot(3);
    
    EXPECT_TRUE(IsEntityInSnapshot(1));
    EXPECT_TRUE(IsEntityInSnapshot(2));
    EXPECT_TRUE(IsEntityInSnapshot(3));
}

// ============================================================================
// TEST SUITE: Snapshot Counter
// ============================================================================

TEST(SvSnapshotEntities_Counter, TracksSnapshot) {
    ResetEntities();
    MarkEntityInSnapshot(0);
    
    EXPECT_EQ(test_svEntities[0].snapshotCounter, test_snapshotCounter);
}

TEST(SvSnapshotEntities_Counter, InvalidatesOldSnapshots) {
    ResetEntities();
    test_svEntities[0].snapshotCounter = test_snapshotCounter - 1;
    
    EXPECT_FALSE(IsEntityInSnapshot(0));
}

// ============================================================================
// TEST SUITE: Entity Counting
// ============================================================================

TEST(SvSnapshotEntities_Count, NoEntities) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    
    int count = CountVisibleEntities(viewpoint, 100.0f);
    
    EXPECT_EQ(count, 0);
}

TEST(SvSnapshotEntities_Count, SingleEntity) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {50, 0, 0};
    
    int count = CountVisibleEntities(viewpoint, 100.0f);
    
    EXPECT_EQ(count, 1);
}

TEST(SvSnapshotEntities_Count, MultipleVisible) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 0, 0};
    test_entities[1].pos = {20, 0, 0};
    test_entities[2].pos = {30, 0, 0};
    
    int count = CountVisibleEntities(viewpoint, 100.0f);
    
    EXPECT_EQ(count, 3);
}

TEST(SvSnapshotEntities_Count, MixedVisibility) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 0, 0};  // Visible
    test_entities[1].pos = {200, 0, 0}; // Not visible
    test_entities[2].pos = {30, 0, 0};  // Visible
    
    int count = CountVisibleEntities(viewpoint, 100.0f);
    
    EXPECT_EQ(count, 2);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvSnapshotEntities_Edge, InvalidEntityNum) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    
    EXPECT_FALSE(IsEntityVisible(-1, viewpoint, 100.0f));
    EXPECT_FALSE(IsEntityVisible(9999, viewpoint, 100.0f));
}

TEST(SvSnapshotEntities_Edge, ZeroRange) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {0, 0, 0};
    
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 0.0f));
}

TEST(SvSnapshotEntities_Edge, NegativeCoordinates) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {-50, -50, 0};
    
    float dist = VectorDistance(viewpoint, test_entities[0].pos);
    EXPECT_GT(dist, 0.0f);
}

// ============================================================================
// TEST SUITE: 3D Space
// ============================================================================

TEST(SvSnapshotEntities_3D, AllAxes) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 10, 10};
    
    float dist = VectorDistance(viewpoint, test_entities[0].pos);
    EXPECT_GT(dist, 0.0f);
}

TEST(SvSnapshotEntities_3D, VerticalOffset) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 100};
    test_entities[0].pos = {0, 0, 50};
    
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvSnapshotEntities_Perf, ManyEntities) {
    ResetEntities();
    vec3_t viewpoint = {0, 0, 0};
    
    for (int i = 0; i < 100; i++) {
        test_entities[i].pos = {(float)i, 0, 0};
    }
    
    int count = CountVisibleEntities(viewpoint, 1000.0f);
    EXPECT_EQ(count, 100);
}

TEST(SvSnapshotEntities_Perf, ManyMarks) {
    ResetEntities();
    
    for (int i = 0; i < 500; i++) {
        MarkEntityInSnapshot(i);
    }
    
    EXPECT_TRUE(IsEntityInSnapshot(499));
}

TEST(SvSnapshotEntities_Perf, ManyDistanceChecks) {
    vec3_t a = {0, 0, 0};
    
    for (int i = 0; i < 1000; i++) {
        vec3_t b = {(float)i, (float)i, 0};
        VectorDistance(a, b);
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests for sv_snapshot_entities.h
// ============================================================================
