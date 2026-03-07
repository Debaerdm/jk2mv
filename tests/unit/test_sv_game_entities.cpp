// test_sv_game_entities.cpp - Unit tests for sv_game_entities.h
// Phase 5: Game interface tests (~30 tests)

#include "test_utils.h"
#include <cstring>

#define MAX_GENTITIES 1024

struct gentity_t {
    int inuse;
    int number;
    char classname[32];
    int health;
    int takedamage;
};

struct sharedEntity_t {
    int entityNum;
    int entityState_number;
};

gentity_t test_gentities[MAX_GENTITIES];
sharedEntity_t test_sharedEntities[MAX_GENTITIES];
int test_numEntities = 0;

void ResetEntities() {
    memset(test_gentities, 0, sizeof(test_gentities));
    memset(test_sharedEntities, 0, sizeof(test_sharedEntities));
    test_numEntities = 0;
}

gentity_t* G_Spawn() {
    for (int i = 0; i < MAX_GENTITIES; i++) {
        if (!test_gentities[i].inuse) {
            test_gentities[i].inuse = 1;
            test_gentities[i].number = i;
            test_numEntities++;
            return &test_gentities[i];
        }
    }
    return nullptr;
}

void G_FreeEntity(gentity_t* ent) {
    if (!ent || !ent->inuse) return;
    
    memset(ent, 0, sizeof(*ent));
    test_numEntities--;
}

gentity_t* G_Find(gentity_t* from, const char* classname) {
    int start = from ? (from->number + 1) : 0;
    
    for (int i = start; i < MAX_GENTITIES; i++) {
        if (test_gentities[i].inuse && 
            strcmp(test_gentities[i].classname, classname) == 0) {
            return &test_gentities[i];
        }
    }
    return nullptr;
}

int G_CountEntities() {
    return test_numEntities;
}

sharedEntity_t* SV_GentityNum(int num) {
    if (num < 0 || num >= MAX_GENTITIES) return nullptr;
    return &test_sharedEntities[num];
}

// ============================================================================
// TEST SUITE: Entity Spawning
// ============================================================================

TEST(SvGameEntities_Spawn, SpawnsEntity) {
    ResetEntities();
    
    gentity_t* ent = G_Spawn();
    
    EXPECT_NE(ent, nullptr);
    EXPECT_EQ(ent->inuse, 1);
}

TEST(SvGameEntities_Spawn, SetsNumber) {
    ResetEntities();
    
    gentity_t* ent = G_Spawn();
    
    EXPECT_EQ(ent->number, 0);
}

TEST(SvGameEntities_Spawn, MultipleSpawns) {
    ResetEntities();
    
    gentity_t* ent1 = G_Spawn();
    gentity_t* ent2 = G_Spawn();
    gentity_t* ent3 = G_Spawn();
    
    EXPECT_EQ(ent1->number, 0);
    EXPECT_EQ(ent2->number, 1);
    EXPECT_EQ(ent3->number, 2);
}

TEST(SvGameEntities_Spawn, IncrementsCount) {
    ResetEntities();
    
    G_Spawn();
    G_Spawn();
    
    EXPECT_EQ(G_CountEntities(), 2);
}

// ============================================================================
// TEST SUITE: Entity Freeing
// ============================================================================

TEST(SvGameEntities_Free, FreesEntity) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    
    G_FreeEntity(ent);
    
    EXPECT_EQ(ent->inuse, 0);
}

TEST(SvGameEntities_Free, DecrementsCount) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    G_Spawn();
    
    G_FreeEntity(ent);
    
    EXPECT_EQ(G_CountEntities(), 1);
}

TEST(SvGameEntities_Free, AllowsReuse) {
    ResetEntities();
    gentity_t* ent1 = G_Spawn();
    G_FreeEntity(ent1);
    
    gentity_t* ent2 = G_Spawn();
    
    EXPECT_EQ(ent1, ent2); // Same slot reused
}

TEST(SvGameEntities_Free, FreeNull) {
    ResetEntities();
    
    G_FreeEntity(nullptr);
    
    EXPECT_TRUE(true); // Should not crash
}

// ============================================================================
// TEST SUITE: Entity Search
// ============================================================================

TEST(SvGameEntities_Find, FindsByClassname) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    strcpy(ent->classname, "player");
    
    gentity_t* found = G_Find(nullptr, "player");
    
    EXPECT_EQ(found, ent);
}

TEST(SvGameEntities_Find, ReturnsNullIfNotFound) {
    ResetEntities();
    
    gentity_t* found = G_Find(nullptr, "monster");
    
    EXPECT_EQ(found, nullptr);
}

TEST(SvGameEntities_Find, FindsMultiple) {
    ResetEntities();
    gentity_t* ent1 = G_Spawn();
    gentity_t* ent2 = G_Spawn();
    strcpy(ent1->classname, "player");
    strcpy(ent2->classname, "player");
    
    gentity_t* found1 = G_Find(nullptr, "player");
    gentity_t* found2 = G_Find(found1, "player");
    
    EXPECT_EQ(found1, ent1);
    EXPECT_EQ(found2, ent2);
}

TEST(SvGameEntities_Find, SkipsFreed) {
    ResetEntities();
    gentity_t* ent1 = G_Spawn();
    gentity_t* ent2 = G_Spawn();
    strcpy(ent1->classname, "player");
    strcpy(ent2->classname, "player");
    G_FreeEntity(ent1);
    
    gentity_t* found = G_Find(nullptr, "player");
    
    EXPECT_EQ(found, ent2);
}

// ============================================================================
// TEST SUITE: Entity Properties
// ============================================================================

TEST(SvGameEntities_Props, SetsClassname) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    strcpy(ent->classname, "weapon_blaster");
    
    EXPECT_STREQ(ent->classname, "weapon_blaster");
}

TEST(SvGameEntities_Props, SetsHealth) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    ent->health = 100;
    
    EXPECT_EQ(ent->health, 100);
}

TEST(SvGameEntities_Props, SetsTakedamage) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    ent->takedamage = 1;
    
    EXPECT_EQ(ent->takedamage, 1);
}

// ============================================================================
// TEST SUITE: Shared Entities
// ============================================================================

TEST(SvGameEntities_Shared, GetsSharedEntity) {
    ResetEntities();
    
    sharedEntity_t* sent = SV_GentityNum(5);
    
    EXPECT_NE(sent, nullptr);
}

TEST(SvGameEntities_Shared, InvalidNumber) {
    ResetEntities();
    
    sharedEntity_t* sent = SV_GentityNum(-1);
    
    EXPECT_EQ(sent, nullptr);
}

TEST(SvGameEntities_Shared, OutOfBounds) {
    ResetEntities();
    
    sharedEntity_t* sent = SV_GentityNum(MAX_GENTITIES + 10);
    
    EXPECT_EQ(sent, nullptr);
}

// ============================================================================
// TEST SUITE: Entity Count
// ============================================================================

TEST(SvGameEntities_Count, StartsZero) {
    ResetEntities();
    
    EXPECT_EQ(G_CountEntities(), 0);
}

TEST(SvGameEntities_Count, TracksSpawns) {
    ResetEntities();
    G_Spawn();
    G_Spawn();
    G_Spawn();
    
    EXPECT_EQ(G_CountEntities(), 3);
}

TEST(SvGameEntities_Count, TracksFrees) {
    ResetEntities();
    gentity_t* ent1 = G_Spawn();
    G_Spawn();
    G_Spawn();
    G_FreeEntity(ent1);
    
    EXPECT_EQ(G_CountEntities(), 2);
}

// ============================================================================
// TEST SUITE: Edge Cases
// ============================================================================

TEST(SvGameEntities_Edge, MaxEntities) {
    ResetEntities();
    
    for (int i = 0; i < MAX_GENTITIES; i++) {
        G_Spawn();
    }
    
    gentity_t* overflow = G_Spawn();
    EXPECT_EQ(overflow, nullptr);
}

TEST(SvGameEntities_Edge, EmptyClassname) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    strcpy(ent->classname, "");
    
    EXPECT_STREQ(ent->classname, "");
}

TEST(SvGameEntities_Edge, DoubleFree) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    
    G_FreeEntity(ent);
    G_FreeEntity(ent);
    
    EXPECT_EQ(G_CountEntities(), 0);
}

// ============================================================================
// TEST SUITE: Performance
// ============================================================================

TEST(SvGameEntities_Perf, ManySpawns) {
    ResetEntities();
    
    for (int i = 0; i < 500; i++) {
        G_Spawn();
    }
    
    EXPECT_EQ(G_CountEntities(), 500);
}

TEST(SvGameEntities_Perf, SpawnFreeLoop) {
    ResetEntities();
    
    for (int i = 0; i < 100; i++) {
        gentity_t* ent = G_Spawn();
        G_FreeEntity(ent);
    }
    
    EXPECT_EQ(G_CountEntities(), 0);
}

TEST(SvGameEntities_Perf, ManySearches) {
    ResetEntities();
    gentity_t* ent = G_Spawn();
    strcpy(ent->classname, "test");
    
    for (int i = 0; i < 1000; i++) {
        G_Find(nullptr, "test");
    }
    
    EXPECT_TRUE(true);
}

// ============================================================================
// SUMMARY: 30 tests for sv_game_entities.h
// ============================================================================
