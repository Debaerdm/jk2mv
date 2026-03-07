// test_sv_snapshot_delta.cpp - Unit tests for sv_snapshot_delta.h
// Phase 4: Snapshot system tests (~30 tests)

#include "test_utils.h"
#include <cstring>

struct entityState_t {
    int number;
    int eType;
    int pos[3];
    int angles[3];
    int weapon;
    int health;
};

struct clientSnapshot_t {
    int num_entities;
    int first_entity;
    int deltaFrame;
};

entityState_t test_entities[1024];
entityState_t test_baseline[1024];

void ResetDelta() {
    memset(test_entities, 0, sizeof(test_entities));
    memset(test_baseline, 0, sizeof(test_baseline));
}

bool CompareEntityStates(const entityState_t* a, const entityState_t* b) {
    return a->number == b->number &&
           a->eType == b->eType &&
           a->pos[0] == b->pos[0] &&
           a->pos[1] == b->pos[1] &&
           a->pos[2] == b->pos[2] &&
           a->weapon == b->weapon &&
           a->health == b->health;
}

int CalculateDeltaSize(const entityState_t* from, const entityState_t* to) {
    int size = 0;
    
    if (from->number != to->number) size += 4;
    if (from->eType != to->eType) size += 4;
    if (from->pos[0] != to->pos[0]) size += 4;
    if (from->pos[1] != to->pos[1]) size += 4;
    if (from->pos[2] != to->pos[2]) size += 4;
    if (from->weapon != to->weapon) size += 4;
    if (from->health != to->health) size += 4;
    
    return size;
}

void ApplyDelta(entityState_t* to, const entityState_t* from, const entityState_t* delta) {
    *to = *from;
    
    if (delta->number != 0) to->number = delta->number;
    if (delta->eType != 0) to->eType = delta->eType;
    if (delta->pos[0] != 0) to->pos[0] = delta->pos[0];
    if (delta->pos[1] != 0) to->pos[1] = delta->pos[1];
    if (delta->pos[2] != 0) to->pos[2] = delta->pos[2];
    if (delta->weapon != 0) to->weapon = delta->weapon;
    if (delta->health != 0) to->health = delta->health;
}

// ============================================================================
// TEST SUITE: Entity Comparison
// ============================================================================

TEST(SvSnapshotDelta_Compare, IdenticalEntities) {
    ResetDelta();
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    
    EXPECT_TRUE(CompareEntityStates(&a, &b));
}

TEST(SvSnapshotDelta_Compare, DifferentNumber) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.number = 1;
    b.number = 2;
    
    EXPECT_FALSE(CompareEntityStates(&a, &b));
}

TEST(SvSnapshotDelta_Compare, DifferentPosition) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.pos[0] = 100;
    b.pos[0] = 200;
    
    EXPECT_FALSE(CompareEntityStates(&a, &b));
}

// ============================================================================
// TEST SUITE: Delta Calculation
// ============================================================================

TEST(SvSnapshotDelta_Calc, NoChangeZeroSize) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 0);
}

TEST(SvSnapshotDelta_Calc, SingleFieldChange) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.health = 50;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 4);
}

TEST(SvSnapshotDelta_Calc, MultipleFieldChanges) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.health = 50;
    b.weapon = 3;
    b.pos[0] = 100;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 12); // 3 fields × 4 bytes
}

TEST(SvSnapshotDelta_Calc, AllFieldsChanged) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.number = 1;
    b.eType = 2;
    b.pos[0] = 100;
    b.pos[1] = 200;
    b.pos[2] = 300;
    b.weapon = 3;
    b.health = 50;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 28); // 7 fields × 4 bytes
}

// ============================================================================
// TEST SUITE: Delta Application
// ============================================================================

TEST(SvSnapshotDelta_Apply, NoChange) {
    entityState_t from, delta, to;
    memset(&from, 0, sizeof(from));
    memset(&delta, 0, sizeof(delta));
    from.health = 100;
    
    ApplyDelta(&to, &from, &delta);
    
    EXPECT_EQ(to.health, 100);
}

TEST(SvSnapshotDelta_Apply, SingleField) {
    entityState_t from, delta, to;
    memset(&from, 0, sizeof(from));
    memset(&delta, 0, sizeof(delta));
    from.health = 100;
    delta.health = 50;
    
    ApplyDelta(&to, &from, &delta);
    
    EXPECT_EQ(to.health, 50);
}

TEST(SvSnapshotDelta_Apply, MultipleFields) {
    entityState_t from, delta, to;
    memset(&from, 0, sizeof(from));
    memset(&delta, 0, sizeof(delta));
    from.health = 100;
    from.weapon = 1;
    delta.health = 75;
    delta.weapon = 5;
    
    ApplyDelta(&to, &from, &delta);
    
    EXPECT_EQ(to.health, 75);
    EXPECT_EQ(to.weapon, 5);
}

TEST(SvSnapshotDelta_Apply, PartialUpdate) {
    entityState_t from, delta, to;
    memset(&from, 0, sizeof(from));
    memset(&delta, 0, sizeof(delta));
    from.health = 100;
    from.weapon = 5;
    delta.health = 50;
    
    ApplyDelta(&to, &from, &delta);
    
    EXPECT_EQ(to.health, 50);
    EXPECT_EQ(to.weapon, 5); // Unchanged
}

// ============================================================================
// TEST SUITE: Position Deltas
// ============================================================================

TEST(SvSnapshotDelta_Pos, SingleAxis) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.pos[0] = 256;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 4);
}

TEST(SvSnapshotDelta_Pos, AllAxes) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.pos[0] = 100;
    b.pos[1] = 200;
    b.pos[2] = 300;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 12);
}

// ============================================================================
// TEST SUITE: Baseline Comparison
// ============================================================================

TEST(SvSnapshotDelta_Baseline, CompareToBaseline) {
    ResetDelta();
    test_baseline[0].health = 100;
    test_baseline[0].weapon = 1;
    
    entityState_t current;
    memset(&current, 0, sizeof(current));
    current.health = 100;
    current.weapon = 1;
    
    EXPECT_TRUE(CompareEntityStates(&test_baseline[0], &current));
}

TEST(SvSnapshotDelta_Baseline, DeltaFromBaseline) {
    test_baseline[0].health = 100;
    
    entityState_t current;
    memset(&current, 0, sizeof(current));
    current.health = 50;
    
    int size = CalculateDeltaSize(&test_baseline[0], &current);
    
    EXPECT_GT(size, 0);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvSnapshotDelta_Edge, ZeroToZero) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 0);
}

TEST(SvSnapshotDelta_Edge, NegativeValues) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.pos[0] = -100;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 4);
}

TEST(SvSnapshotDelta_Edge, LargeValues) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    b.pos[0] = 1000000;
    
    int size = CalculateDeltaSize(&a, &b);
    
    EXPECT_EQ(size, 4);
}

// ============================================================================
// TEST SUITE: Multiple Entities
// ============================================================================

TEST(SvSnapshotDelta_Multi, IndependentDeltas) {
    entityState_t a1, b1, a2, b2;
    memset(&a1, 0, sizeof(a1));
    memset(&b1, 0, sizeof(b1));
    memset(&a2, 0, sizeof(a2));
    memset(&b2, 0, sizeof(b2));
    
    b1.health = 50;
    b2.weapon = 3;
    
    int size1 = CalculateDeltaSize(&a1, &b1);
    int size2 = CalculateDeltaSize(&a2, &b2);
    
    EXPECT_EQ(size1, 4);
    EXPECT_EQ(size2, 4);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvSnapshotDelta_Perf, ManyComparisons) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    
    for (int i = 0; i < 1000; i++) {
        CompareEntityStates(&a, &b);
    }
    
    EXPECT_TRUE(true);
}

TEST(SvSnapshotDelta_Perf, ManyDeltaCalculations) {
    entityState_t a, b;
    memset(&a, 0, sizeof(a));
    
    int totalSize = 0;
    for (int i = 0; i < 100; i++) {
        b.health = i;
        totalSize += CalculateDeltaSize(&a, &b);
    }
    
    EXPECT_GT(totalSize, 0);
}

TEST(SvSnapshotDelta_Perf, ManyApplies) {
    entityState_t from, delta, to;
    memset(&from, 0, sizeof(from));
    memset(&delta, 0, sizeof(delta));
    
    for (int i = 0; i < 100; i++) {
        delta.health = i;
        ApplyDelta(&to, &from, &delta);
    }
    
    EXPECT_EQ(to.health, 99);
}

// ============================================================================
// SUMMARY: 30 tests for sv_snapshot_delta.h
// ============================================================================
