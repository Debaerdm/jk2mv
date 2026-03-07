// sv_snapshot.cpp -- client snapshot generation and transmission
// Refactored into modular architecture

#include "server.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_snapshot.cpp (700 lines) decomposed into 5 focused modules:
//
// 1. Delta Encoding          - Entity delta compression
// 2. Writer                  - Snapshot serialization to network
// 3. Entities                - PVS-based entity visibility & collection
// 4. Builder                 - Snapshot construction from game state
// 5. Sender                  - Network transmission & rate limiting
// ============================================================================

#include "sv_snapshot_delta.h"
#include "sv_snapshot_writer.h"
#include "sv_snapshot_entities.h"
#include "sv_snapshot_builder.h"
#include "sv_snapshot_sender.h"
