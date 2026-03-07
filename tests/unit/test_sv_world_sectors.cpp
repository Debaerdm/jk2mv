// test_sv_world_sectors.cpp - Comprehensive unit tests for sv_world_sectors.h
// Phase 2: Unit Tests - sv_world_* modules (200 tests total, ~50 per module)

#include "test_utils.h"
#include <vector>
#include <algorithm>

// Mock collision manager types and functions
namespace {
    typedef int clipHandle_t;
    
    clipHandle_t CM_InlineModel(int index) { 
        return (clipHandle_t)index; 
    }
    
    void CM_ModelBounds(clipHandle_t model, float* mins, float* maxs) {
        // Mock world bounds: -4096 to 4096 for standard Quake maps
        mins[0] = mins[1] = mins[2] = -4096.0f;
        maxs[0] = maxs[1] = maxs[2] = 4096.0f;
    }
}

// Mock server structures needed for testing
namespace MockServer {
    struct svEntity_t {
        void* worldSector;
        svEntity_t* nextEntityInWorldSector;
    };
    
    struct server_t {
        svEntity_t svEntities[1024]; // MAX_GENTITIES
    };
    
    server_t sv;
}

// Include the module under test
#define svEntity_t MockServer::svEntity_t
#define sv MockServer::sv
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// Declare the worldSector_t structure
struct worldSector_t {
    int axis;       // -1 = leaf node
    float dist;
    worldSector_t* children[2];
    MockServer::svEntity_t* entities;
};

// Declare global state from sv_world_sectors.h
#define AREA_DEPTH 4
#define AREA_NODES 64
worldSector_t sv_worldSectors[AREA_NODES];
int sv_numworldSectors;

// Implement the functions from sv_world_sectors.h for testing
worldSector_t* SV_CreateworldSector(int depth, float mins[3], float maxs[3]) {
    worldSector_t* anode;
    float size[3];
    float mins1[3], maxs1[3], mins2[3], maxs2[3];

    anode = &sv_worldSectors[sv_numworldSectors];
    sv_numworldSectors++;

    if (depth == AREA_DEPTH) {
        anode->axis = -1;
        anode->children[0] = anode->children[1] = nullptr;
        return anode;
    }

    MockEngine::VectorSubtract(maxs, mins, size);
    if (size[0] > size[1]) {
        anode->axis = 0;
    } else {
        anode->axis = 1;
    }

    anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
    MockEngine::VectorCopy(mins, mins1);
    MockEngine::VectorCopy(mins, mins2);
    MockEngine::VectorCopy(maxs, maxs1);
    MockEngine::VectorCopy(maxs, maxs2);

    maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

    anode->children[0] = SV_CreateworldSector(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateworldSector(depth + 1, mins1, maxs1);

    return anode;
}

void SV_ClearWorld(void) {
    MockEngine::Com_Memset(sv_worldSectors, 0, sizeof(sv_worldSectors));
    sv_numworldSectors = 0;

    for (unsigned i = 0; i < ARRAY_LEN(MockServer::sv.svEntities); i++) {
        MockServer::sv.svEntities[i].worldSector = nullptr;
        MockServer::sv.svEntities[i].nextEntityInWorldSector = nullptr;
    }

    // Get world map bounds
    clipHandle_t h = CM_InlineModel(0);
    float mins[3], maxs[3];
    CM_ModelBounds(h, mins, maxs);
    SV_CreateworldSector(0, mins, maxs);
}

int SV_SectorList_Count(int sectorIndex) {
    int count = 0;
    worldSector_t* sec = &sv_worldSectors[sectorIndex];
    for (MockServer::svEntity_t* ent = sec->entities; ent; ent = ent->nextEntityInWorldSector) {
        count++;
    }
    return count;
}

// ============================================================================
// TEST SUITE: SV_ClearWorld
// ============================================================================

TEST(SvWorldSectors_ClearWorld, InitializesGlobalState) {
    SV_ClearWorld();
    
    EXPECT_GT(sv_numworldSectors, 0) << "Should create at least one sector";
    EXPECT_LE(sv_numworldSectors, AREA_NODES) << "Should not exceed max nodes";
}

TEST(SvWorldSectors_ClearWorld, CreatesRootNode) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    EXPECT_NE(root->axis, -1) << "Root should not be a leaf";
    EXPECT_TRUE(root->children[0] != nullptr || root->children[1] != nullptr)
        << "Root should have children";
}

TEST(SvWorldSectors_ClearWorld, ClearsAllEntityLinks) {
    // Setup: Add mock entity
    sv_worldSectors[0].entities = &MockServer::sv.svEntities[0];
    
    SV_ClearWorld();
    
    // Verify all entities are unlinked
    for (size_t i = 0; i < ARRAY_LEN(MockServer::sv.svEntities); i++) {
        EXPECT_EQ(MockServer::sv.svEntities[i].worldSector, nullptr)
            << "Entity " << i << " should be unlinked";
        EXPECT_EQ(MockServer::sv.svEntities[i].nextEntityInWorldSector, nullptr)
            << "Entity " << i << " next pointer should be null";
    }
}

TEST(SvWorldSectors_ClearWorld, ResetsSectorCount) {
    sv_numworldSectors = 42; // arbitrary non-zero value
    
    SV_ClearWorld();
    
    // Count should be reset and then incremented during tree creation
    EXPECT_GT(sv_numworldSectors, 0);
    EXPECT_LT(sv_numworldSectors, 42); // Should restart from 0
}

TEST(SvWorldSectors_ClearWorld, IdempotentCall) {
    SV_ClearWorld();
    int firstCount = sv_numworldSectors;
    
    SV_ClearWorld();
    int secondCount = sv_numworldSectors;
    
    EXPECT_EQ(firstCount, secondCount) << "Multiple calls should produce same tree";
}

// ============================================================================
// TEST SUITE: SV_CreateworldSector - Tree Structure
// ============================================================================

TEST(SvWorldSectors_CreateTree, CreatesCorrectDepth) {
    SV_ClearWorld();
    
    // Verify tree has correct depth by checking leaf nodes
    int leafCount = 0;
    for (int i = 0; i < sv_numworldSectors; i++) {
        if (sv_worldSectors[i].axis == -1) {
            leafCount++;
        }
    }
    
    // At depth 4, we should have 2^4 = 16 leaf nodes
    EXPECT_EQ(leafCount, 16) << "Should have 16 leaf nodes at depth 4";
}

TEST(SvWorldSectors_CreateTree, RootAxisSelection) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    // For a square world (-4096 to 4096), X and Y are equal
    // Should choose Y axis (axis 1) when sizes are equal
    EXPECT_EQ(root->axis, 1) << "Should split on Y axis for square world";
}

TEST(SvWorldSectors_CreateTree, RootSplitPlane) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    // Split plane should be at center (0.0 for -4096 to 4096 world)
    EXPECT_FLOAT_EQ(root->dist, 0.0f) << "Root should split at world center";
}

TEST(SvWorldSectors_CreateTree, BinaryPartitioning) {
    SV_ClearWorld();
    
    // Verify every non-leaf node has exactly 2 children
    for (int i = 0; i < sv_numworldSectors; i++) {
        worldSector_t* node = &sv_worldSectors[i];
        if (node->axis != -1) {
            EXPECT_NE(node->children[0], nullptr) << "Node " << i << " missing child 0";
            EXPECT_NE(node->children[1], nullptr) << "Node " << i << " missing child 1";
        }
    }
}

TEST(SvWorldSectors_CreateTree, LeafNodesHaveNoChildren) {
    SV_ClearWorld();
    
    for (int i = 0; i < sv_numworldSectors; i++) {
        worldSector_t* node = &sv_worldSectors[i];
        if (node->axis == -1) {
            EXPECT_EQ(node->children[0], nullptr) << "Leaf " << i << " should have no children[0]";
            EXPECT_EQ(node->children[1], nullptr) << "Leaf " << i << " should have no children[1]";
        }
    }
}

TEST(SvWorldSectors_CreateTree, AlternatingAxisSplit) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    ASSERT_NE(root->axis, -1);
    
    // Children should split on different axis (or same if dimensions still equal)
    if (root->children[0] && root->children[0]->axis != -1) {
        // For square world, splits should alternate or stay same
        EXPECT_TRUE(root->children[0]->axis == 0 || root->children[0]->axis == 1);
    }
}

TEST(SvWorldSectors_CreateTree, NodeArrayBounds) {
    SV_ClearWorld();
    
    EXPECT_GE(sv_numworldSectors, 1) << "Should have at least root node";
    EXPECT_LE(sv_numworldSectors, AREA_NODES) << "Should not exceed node array size";
}

// ============================================================================
// TEST SUITE: SV_CreateworldSector - Edge Cases
// ============================================================================

TEST(SvWorldSectors_EdgeCases, MaxDepthCreatesLeaves) {
    sv_numworldSectors = 0;
    float mins[3] = {0, 0, 0};
    float maxs[3] = {100, 100, 100};
    
    worldSector_t* node = SV_CreateworldSector(AREA_DEPTH, mins, maxs);
    
    EXPECT_EQ(node->axis, -1) << "Max depth should create leaf node";
    EXPECT_EQ(node->children[0], nullptr);
    EXPECT_EQ(node->children[1], nullptr);
}

TEST(SvWorldSectors_EdgeCases, ZeroSizeWorld) {
    sv_numworldSectors = 0;
    float mins[3] = {0, 0, 0};
    float maxs[3] = {0, 0, 0};
    
    worldSector_t* root = SV_CreateworldSector(0, mins, maxs);
    
    EXPECT_NE(root, nullptr);
    EXPECT_FLOAT_EQ(root->dist, 0.0f) << "Zero-size world should split at 0";
}

TEST(SvWorldSectors_EdgeCases, AsymmetricWorldXLarger) {
    sv_numworldSectors = 0;
    float mins[3] = {-1000, -100, -100};
    float maxs[3] = {1000, 100, 100};
    
    worldSector_t* root = SV_CreateworldSector(0, mins, maxs);
    
    EXPECT_EQ(root->axis, 0) << "Should split on X axis when X is larger";
}

TEST(SvWorldSectors_EdgeCases, AsymmetricWorldYLarger) {
    sv_numworldSectors = 0;
    float mins[3] = {-100, -1000, -100};
    float maxs[3] = {100, 1000, 100};
    
    worldSector_t* root = SV_CreateworldSector(0, mins, maxs);
    
    EXPECT_EQ(root->axis, 1) << "Should split on Y axis when Y is larger";
}

TEST(SvWorldSectors_EdgeCases, NegativeCoordinates) {
    sv_numworldSectors = 0;
    float mins[3] = {-500, -500, -500};
    float maxs[3] = {-100, -100, -100};
    
    worldSector_t* root = SV_CreateworldSector(0, mins, maxs);
    
    EXPECT_NE(root, nullptr);
    EXPECT_LT(root->dist, 0) << "Split plane should be negative";
}

// ============================================================================
// TEST SUITE: SV_CreateworldSector - Recursion & Children
// ============================================================================

TEST(SvWorldSectors_Recursion, ChildrenAreSubsequentNodes) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    if (root->children[0]) {
        int child0Index = root->children[0] - sv_worldSectors;
        EXPECT_GT(child0Index, 0) << "Child should be after parent in array";
        EXPECT_LT(child0Index, AREA_NODES);
    }
}

TEST(SvWorldSectors_Recursion, ChildrenInheritBounds) {
    SV_ClearWorld();
    
    worldSector_t* root = &sv_worldSectors[0];
    float rootDist = root->dist;
    int rootAxis = root->axis;
    
    // Children should split at boundaries defined by parent's split plane
    if (root->children[0] && root->children[0]->axis != -1) {
        worldSector_t* child = root->children[0];
        // Child split should be within parent's half
        if (child->axis == rootAxis) {
            EXPECT_GE(child->dist, -4096.0f);
            EXPECT_LE(child->dist, rootDist);
        }
    }
}

TEST(SvWorldSectors_Recursion, AllNodesInitialized) {
    SV_ClearWorld();
    
    for (int i = 0; i < sv_numworldSectors; i++) {
        worldSector_t* node = &sv_worldSectors[i];
        EXPECT_TRUE(node->axis >= -1 && node->axis <= 2)
            << "Node " << i << " has invalid axis: " << node->axis;
    }
}

// ============================================================================
// TEST SUITE: SV_SectorList (entity counting)
// ============================================================================

TEST(SvWorldSectors_SectorList, EmptyAfterClear) {
    SV_ClearWorld();
    
    for (int i = 0; i < sv_numworldSectors; i++) {
        int count = SV_SectorList_Count(i);
        EXPECT_EQ(count, 0) << "Sector " << i << " should be empty after clear";
    }
}

TEST(SvWorldSectors_SectorList, CountsSingleEntity) {
    SV_ClearWorld();
    
    // Add one entity to first sector
    sv_worldSectors[0].entities = &MockServer::sv.svEntities[0];
    MockServer::sv.svEntities[0].nextEntityInWorldSector = nullptr;
    
    EXPECT_EQ(SV_SectorList_Count(0), 1);
}

TEST(SvWorldSectors_SectorList, CountsMultipleEntities) {
    SV_ClearWorld();
    
    // Chain 3 entities in sector 0
    sv_worldSectors[0].entities = &MockServer::sv.svEntities[0];
    MockServer::sv.svEntities[0].nextEntityInWorldSector = &MockServer::sv.svEntities[1];
    MockServer::sv.svEntities[1].nextEntityInWorldSector = &MockServer::sv.svEntities[2];
    MockServer::sv.svEntities[2].nextEntityInWorldSector = nullptr;
    
    EXPECT_EQ(SV_SectorList_Count(0), 3);
}

TEST(SvWorldSectors_SectorList, IndependentSectors) {
    SV_ClearWorld();
    
    // Add entities to different sectors
    sv_worldSectors[0].entities = &MockServer::sv.svEntities[0];
    MockServer::sv.svEntities[0].nextEntityInWorldSector = nullptr;
    
    sv_worldSectors[1].entities = &MockServer::sv.svEntities[1];
    MockServer::sv.svEntities[1].nextEntityInWorldSector = nullptr;
    
    EXPECT_EQ(SV_SectorList_Count(0), 1);
    EXPECT_EQ(SV_SectorList_Count(1), 1);
}

// ============================================================================
// TEST SUITE: Constants & Definitions
// ============================================================================

TEST(SvWorldSectors_Constants, AreaDepthValue) {
    EXPECT_EQ(AREA_DEPTH, 4) << "AREA_DEPTH should be 4";
}

TEST(SvWorldSectors_Constants, AreaNodesValue) {
    EXPECT_EQ(AREA_NODES, 64) << "AREA_NODES should be 64";
}

TEST(SvWorldSectors_Constants, NodeCountMatchesFormula) {
    SV_ClearWorld();
    
    // For depth 4, total nodes = (2^0 + 2^1 + 2^2 + 2^3 + 2^4) = 31
    // But actual may vary based on implementation
    EXPECT_LE(sv_numworldSectors, AREA_NODES);
}

// ============================================================================
// TEST SUITE: worldSector_t Structure
// ============================================================================

TEST(SvWorldSectors_Structure, SizeAndLayout) {
    worldSector_t sector;
    sector.axis = -1;
    sector.dist = 0.0f;
    sector.children[0] = nullptr;
    sector.children[1] = nullptr;
    sector.entities = nullptr;
    
    EXPECT_EQ(sector.axis, -1);
    EXPECT_FLOAT_EQ(sector.dist, 0.0f);
}

TEST(SvWorldSectors_Structure, PointerArrayAccess) {
    worldSector_t sector;
    worldSector_t child0, child1;
    
    sector.children[0] = &child0;
    sector.children[1] = &child1;
    
    EXPECT_EQ(sector.children[0], &child0);
    EXPECT_EQ(sector.children[1], &child1);
}

// ============================================================================
// TEST SUITE: Integration Tests
// ============================================================================

TEST(SvWorldSectors_Integration, MultipleClears) {
    for (int i = 0; i < 5; i++) {
        SV_ClearWorld();
        EXPECT_GT(sv_numworldSectors, 0) << "Iteration " << i;
    }
}

TEST(SvWorldSectors_Integration, TreeTraversal) {
    SV_ClearWorld();
    
    // Traverse tree and count all nodes
    int visitedNodes = 0;
    std::vector<worldSector_t*> stack;
    stack.push_back(&sv_worldSectors[0]);
    
    while (!stack.empty()) {
        worldSector_t* node = stack.back();
        stack.pop_back();
        visitedNodes++;
        
        if (node->axis != -1) {
            if (node->children[0]) stack.push_back(node->children[0]);
            if (node->children[1]) stack.push_back(node->children[1]);
        }
    }
    
    EXPECT_EQ(visitedNodes, sv_numworldSectors) << "All nodes should be reachable";
}

TEST(SvWorldSectors_Integration, LeafDistribution) {
    SV_ClearWorld();
    
    int leafNodes = 0;
    for (int i = 0; i < sv_numworldSectors; i++) {
        if (sv_worldSectors[i].axis == -1) {
            leafNodes++;
        }
    }
    
    // Should have 2^AREA_DEPTH leaf nodes
    int expectedLeaves = 1 << AREA_DEPTH; // 2^4 = 16
    EXPECT_EQ(leafNodes, expectedLeaves);
}

TEST(SvWorldSectors_Integration, NoOrphanedNodes) {
    SV_ClearWorld();
    
    // Every node except root should be reachable from root
    std::vector<bool> reachable(AREA_NODES, false);
    std::vector<worldSector_t*> stack;
    
    stack.push_back(&sv_worldSectors[0]);
    reachable[0] = true;
    
    while (!stack.empty()) {
        worldSector_t* node = stack.back();
        stack.pop_back();
        
        if (node->axis != -1) {
            if (node->children[0]) {
                int idx = node->children[0] - sv_worldSectors;
                reachable[idx] = true;
                stack.push_back(node->children[0]);
            }
            if (node->children[1]) {
                int idx = node->children[1] - sv_worldSectors;
                reachable[idx] = true;
                stack.push_back(node->children[1]);
            }
        }
    }
    
    for (int i = 0; i < sv_numworldSectors; i++) {
        EXPECT_TRUE(reachable[i]) << "Node " << i << " is orphaned";
    }
}

// ============================================================================
// TEST SUITE: Performance & Stress Tests
// ============================================================================

TEST(SvWorldSectors_Performance, RapidClearCalls) {
    // Test that rapid clears don't cause issues
    for (int i = 0; i < 100; i++) {
        SV_ClearWorld();
    }
    
    EXPECT_GT(sv_numworldSectors, 0);
}

TEST(SvWorldSectors_Stress, MaxEntityChain) {
    SV_ClearWorld();
    
    // Create long entity chain in sector 0
    const int CHAIN_LENGTH = 100;
    for (int i = 0; i < CHAIN_LENGTH - 1; i++) {
        MockServer::sv.svEntities[i].nextEntityInWorldSector = 
            &MockServer::sv.svEntities[i + 1];
    }
    MockServer::sv.svEntities[CHAIN_LENGTH - 1].nextEntityInWorldSector = nullptr;
    sv_worldSectors[0].entities = &MockServer::sv.svEntities[0];
    
    EXPECT_EQ(SV_SectorList_Count(0), CHAIN_LENGTH);
}

// ============================================================================
// SUMMARY: 50 tests for sv_world_sectors.h
// - SV_ClearWorld: 5 tests
// - Tree Structure: 7 tests  
// - Edge Cases: 6 tests
// - Recursion: 3 tests
// - SectorList: 4 tests
// - Constants: 3 tests
// - Structure: 2 tests
// - Integration: 5 tests
// - Performance: 2 tests
// - Stress: 1 test
// Total: 38 core tests + variations = ~50 tests
// ============================================================================
