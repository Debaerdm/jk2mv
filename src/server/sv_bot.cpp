// sv_bot.cpp -- server bot integration with botlib
// Refactored into modular architecture

#include "server.h"
#include "../game/botlib.h"

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_bot.cpp (650 lines) decomposed into 4 focused modules:
//
// 1. Client              - Bot client slot allocation/free
// 2. Debug               - Debug polygons and lines visualization
// 3. Imports             - All BotImport_* functions (trace, memory, BSP, FS)
// 4. Lifecycle           - Init, cvars, frame, AI interface
// ============================================================================

// Used by the debug module before sv_bot_lifecycle.h defines bot_enable;
// botlib_export is defined in sv_game.cpp
extern botlib_export_t	*botlib_export;
extern int				bot_enable;

#include "sv_bot_client.h"
#include "sv_bot_debug.h"
#include "sv_bot_imports.h"
#include "sv_bot_lifecycle.h"
