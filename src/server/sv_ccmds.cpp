// sv_ccmds.cpp -- server console commands (operator-only)
// Refactored into modular architecture

#include "server.h"
#include "../strings/str_server.h"
#include "../qcommon/strip.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_ccmds.cpp (950 lines) decomposed into 6 focused modules:
//
// 1. Helpers            - Player lookup, string utilities
// 2. Map                - Map loading (map, devmap, map_restart)
// 3. Kick               - Player kick commands
// 4. Info               - Server/client info commands
// 5. Misc               - Say, forcetoggle, heartbeat, whitelist
// 6. Registration       - Command registration/removal
// ============================================================================

#include "sv_ccmds_helpers.h"
#include "sv_ccmds_map.h"
#include "sv_ccmds_kick.h"
#include "sv_ccmds_info.h"
#include "sv_ccmds_misc.h"
#include "sv_ccmds_registration.h"
