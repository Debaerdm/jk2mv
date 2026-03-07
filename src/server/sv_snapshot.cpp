// sv_snapshot.cpp -- server snapshot generation
// Refactored into modular architecture

#include "server.h"

/*
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4	sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1	svc_snapshot
4	last client reliable command
4	serverTime
1	lastframe for delta compression
1	snapFlags
1	areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_snapshot.cpp (700 lines) decomposed into 5 focused modules:
//
// 1. Delta Encoding          - Entity delta compression
// 2. Snapshot Writer         - Serialization to network messages
// 3. Entity Collection       - PVS visibility & portal handling
// 4. Snapshot Builder        - Client snapshot construction
// 5. Message Sender          - Rate limiting & transmission
// ============================================================================

#include "sv_snapshot_delta.h"
#include "sv_snapshot_writer.h"
#include "sv_snapshot_entities.h"
#include "sv_snapshot_builder.h"
#include "sv_snapshot_sender.h"
