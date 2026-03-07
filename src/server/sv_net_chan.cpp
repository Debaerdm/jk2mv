// sv_net_chan.cpp -- server network channel encryption and transmission
// Refactored into modular architecture

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "server.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_net_chan.cpp (170 lines) decomposed into 3 focused modules:
//
// 1. Crypto              - XOR cipher encode/decode
// 2. Transmit            - Packet transmission + fragments
// 3. Process             - Packet reception + decoding
// ============================================================================

#include "sv_netchan_crypto.h"
#include "sv_netchan_transmit.h"
#include "sv_netchan_process.h"
