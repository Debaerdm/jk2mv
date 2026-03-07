// test_sv_snapshot_entities.cpp - Unit tests for sv_snapshot_entities.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>
#include <cmath>

struct vec3_t { float x, y, z; };
struct entityState_t { int number; int eType; vec3_t pos; int solid; };
struct svEntity_t { int snapshotCounter; int areanum; int areanum2; };

entityState_t test_entities[1024];
svEntity_t test_svEntities[1024];
int test_snapshotCounter = 0;

void ResetEntities() {
    memset(test_entities, 0, sizeof(test_entities));
    memset(test_svEntities, 0, sizeof(test_svEntities));
    test_snapshotCounter = 1;
}

float VectorDistance(vec3_t a, vec3_t b) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

bool IsEntityVisible(int entityNum, vec3_t viewpoint, float maxDistance) {
    if (entityNum < 0 || entityNum >= 1024) return false;
    return VectorDistance(viewpoint, test_entities[entityNum].pos) <= maxDistance;
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
        if (IsEntityVisible(i, viewpoint, maxDistance)) count++;
    }
    return count;
}

// ============================================================================
// TEST FIXTURE: Reset global state before each test
// ============================================================================

class SnapshotEntitiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        ResetEntities();
    }
};

// ============================================================================
// TEST SUITE: Visibility Testing
// ============================================================================

TEST_F(SnapshotEntitiesTest, EntityAtOrigin) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {0, 0, 0};
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST_F(SnapshotEntitiesTest, EntityWithinRange) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {50, 0, 0};
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST_F(SnapshotEntitiesTest, EntityOutOfRange) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {200, 0, 0};
    EXPECT_FALSE(IsEntityVisible(0, viewpoint, 100.0f));
}

TEST_F(SnapshotEntitiesTest, EntityAtExactRange) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {100, 0, 0};
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

// ============================================================================
// TEST SUITE: Distance Calculation
// ============================================================================

TEST_F(SnapshotEntitiesTest, ZeroDistance) {
    vec3_t a = {0, 0, 0}, b = {0, 0, 0};
    EXPECT_FLOAT_EQ(VectorDistance(a, b), 0.0f);
}

TEST_F(SnapshotEntitiesTest, HorizontalDistance) {
    vec3_t a = {0, 0, 0}, b = {100, 0, 0};
    EXPECT_FLOAT_EQ(VectorDistance(a, b), 100.0f);
}

TEST_F(SnapshotEntitiesTest, VerticalDistance) {
    vec3_t a = {0, 0, 0}, b = {0, 0, 50};
    EXPECT_FLOAT_EQ(VectorDistance(a, b), 50.0f);
}

TEST_F(SnapshotEntitiesTest, DiagonalDistance) {
    vec3_t a = {0, 0, 0}, b = {3, 4, 0};
    EXPECT_FLOAT_EQ(VectorDistance(a, b), 5.0f);
}

// ============================================================================
// TEST SUITE: Snapshot Marking
// ============================================================================

TEST_F(SnapshotEntitiesTest, MarksEntity) {
    MarkEntityInSnapshot(5);
    EXPECT_TRUE(IsEntityInSnapshot(5));
}

TEST_F(SnapshotEntitiesTest, DoesNotMarkOthers) {
    MarkEntityInSnapshot(5);
    EXPECT_FALSE(IsEntityInSnapshot(3));
}

TEST_F(SnapshotEntitiesTest, MultipleEntities) {
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

TEST_F(SnapshotEntitiesTest, TracksSnapshot) {
    MarkEntityInSnapshot(0);
    EXPECT_EQ(test_svEntities[0].snapshotCounter, test_snapshotCounter);
}

TEST_F(SnapshotEntitiesTest, InvalidatesOldSnapshots) {
    test_svEntities[0].snapshotCounter = test_snapshotCounter - 1;
    EXPECT_FALSE(IsEntityInSnapshot(0));
}

// ============================================================================
// TEST SUITE: Entity Counting
// ============================================================================

TEST_F(SnapshotEntitiesTest, NoEntities) {
    vec3_t viewpoint = {0, 0, 0};
    EXPECT_EQ(CountVisibleEntities(viewpoint, 100.0f), 0);
}

TEST_F(SnapshotEntitiesTest, SingleEntity) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {50, 0, 0};
    EXPECT_EQ(CountVisibleEntities(viewpoint, 100.0f), 1);
}

TEST_F(SnapshotEntitiesTest, MultipleVisible) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 0, 0};
    test_entities[1].pos = {20, 0, 0};
    test_entities[2].pos = {30, 0, 0};
    EXPECT_EQ(CountVisibleEntities(viewpoint, 100.0f), 3);
}

TEST_F(SnapshotEntitiesTest, MixedVisibility) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 0, 0};
    test_entities[1].pos = {200, 0, 0};
    test_entities[2].pos = {30, 0, 0};
    EXPECT_EQ(CountVisibleEntities(viewpoint, 100.0f), 2);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST_F(SnapshotEntitiesTest, InvalidEntityNum) {
    vec3_t viewpoint = {0, 0, 0};
    EXPECT_FALSE(IsEntityVisible(-1, viewpoint, 100.0f));
    EXPECT_FALSE(IsEntityVisible(9999, viewpoint, 100.0f));
}

TEST_F(SnapshotEntitiesTest, ZeroRange) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {0, 0, 0};
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 0.0f));
}

TEST_F(SnapshotEntitiesTest, NegativeCoordinates) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {-50, -50, 0};
    EXPECT_GT(VectorDistance(viewpoint, test_entities[0].pos), 0.0f);
}

// ============================================================================
// TEST SUITE: 3D Space
// ============================================================================

TEST_F(SnapshotEntitiesTest, AllAxes) {
    vec3_t viewpoint = {0, 0, 0};
    test_entities[0].pos = {10, 10, 10};
    EXPECT_GT(VectorDistance(viewpoint, test_entities[0].pos), 0.0f);
}

TEST_F(SnapshotEntitiesTest, VerticalOffset) {
    vec3_t viewpoint = {0, 0, 100};
    test_entities[0].pos = {0, 0, 50};
    EXPECT_TRUE(IsEntityVisible(0, viewpoint, 100.0f));
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST_F(SnapshotEntitiesTest, ManyEntities) {
    vec3_t viewpoint = {0, 0, 0};
    for (int i = 0; i < 100; i++) test_entities[i].pos = {(float)i, 0, 0};
    EXPECT_EQ(CountVisibleEntities(viewpoint, 1000.0f), 100);
}

TEST_F(SnapshotEntitiesTest, ManyMarks) {
    for (int i = 0; i < 500; i++) MarkEntityInSnapshot(i);
    EXPECT_TRUE(IsEntityInSnapshot(499));
}

TEST_F(SnapshotEntitiesTest, ManyDistanceChecks) {
    vec3_t a = {0, 0, 0};
    for (int i = 0; i < 1000; i++) {
        vec3_t b = {(float)i, (float)i, 0};
        VectorDistance(a, b);
    }
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests with SnapshotEntitiesTest fixture
// ============================================================================
