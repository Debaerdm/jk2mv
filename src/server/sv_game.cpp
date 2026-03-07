// sv_game.cpp -- interface to the game dll
// Refactored into modular architecture

#include "server.h"

#include "../game/botlib.h"
#include "../qcommon/strip.h"

#if !defined(CROFFSYSTEM_H_INC)
	#include "../qcommon/RoffSystem.h"
#endif

#if !defined(G2_H_INC)
	#include "../ghoul2/G2_local.h"
#endif

#include "../api/mvapi.h"

botlib_export_t	*botlib_export;

#ifdef G2_COLLISION_ENABLED
extern CMiniHeap *G2VertSpaceServer;
#endif

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_game.cpp (1200 lines) decomposed into 6 focused modules:
//
// 1. Entity Management      - Array access, conversions
// 2. Communication          - Game<->Server messaging
// 3. World Interaction      - PVS, collision, brush models
// 4. Game Data              - Data location, MVAPI setup
// 5. Syscall Dispatcher     - Massive syscall switch (~800 lines)
// 6. VM Lifecycle           - Init, shutdown, restart
// ============================================================================

#include "sv_game_entities.h"
#include "sv_game_communication.h"
#include "sv_game_world.h"
#include "sv_game_data.h"
#include "sv_game_syscalls.h"
#include "sv_game_vm.h"
