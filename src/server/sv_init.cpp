// sv_init.cpp -- server initialization
// Refactored into modular architecture

#include "server.h"

#include <mv_setup.h>

#include "../qcommon/q_shared.h"

/*
Ghoul2 Insert Start
*/
#if !defined(TR_LOCAL_H)
	#include "../renderer/tr_local.h"
#endif

#ifdef G2_COLLISION_ENABLED
#if !defined (MINIHEAP_H_INC)
#include "../qcommon/MiniHeap.h"
#endif
#endif

#include "../qcommon/strip.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_init.cpp (820 lines) decomposed into 5 focused modules:
//
// 1. Configstrings         - Configstring management (set, get, add, chunking)
// 2. Userinfo              - Userinfo handling
// 3. Clients               - Client array management (startup, maxclients, baseline)
// 4. Map Loading           - Map loading & server spawning (the BEAST - 350 lines!)
// 5. Lifecycle             - Init & shutdown
// ============================================================================

#include "sv_init_configstrings.h"
#include "sv_init_userinfo.h"
#include "sv_init_clients.h"
#include "sv_init_mapload.h"
#include "sv_init_lifecycle.h"
