// sv_world.cpp -- world query functions (collision detection, ray tracing)
// Refactored into modular architecture

#include "server.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_world.cpp (630 lines) decomposed into 4 focused modules:
//
// 1. Sectors             - World sector BSP tree (spatial partitioning)
// 2. Linking             - Entity link/unlink to world sectors + PVS
// 3. Area                - Area entity queries (bounding box intersection)
// 4. Trace               - Collision detection & ray tracing
// ============================================================================

#include "sv_world_sectors.h"
#include "sv_world_linking.h"
#include "sv_world_area.h"
#include "sv_world_trace.h"
