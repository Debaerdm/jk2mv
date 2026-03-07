// sv_client.cpp -- Server code for dealing with clients
// Refactored into modular architecture during aggressive server phase 1

#include "server.h"
#include "../qcommon/strip.h"
#include <mv_setup.h>

// ============================================================================
// MODULAR CLIENT SUBSYSTEM
// 
// Each header below encapsulates a cohesive responsibility:
// - Challenge system & DOS protection
// - Connection establishment & validation
// - Disconnection handling
// - Gamestate transmission
// - File download system
// - Client command execution
// - Userinfo validation & crash fixes
// - Usercmd processing & movement
// ============================================================================

#include "sv_client_challenge.h"      // SV_GetChallenge
#include "sv_client_userinfo.h"       // SV_UserinfoChanged, SV_UpdateUserinfo_f
#include "sv_client_connect.h"        // SV_DirectConnect
#include "sv_client_disconnect.h"     // SV_DropClient
#include "sv_client_gamestate.h"      // SV_SendClientGameState, SV_ClientEnterWorld
#include "sv_client_download.h"       // SV_BeginDownload_f, SV_WriteDownloadToClient
#include "sv_client_commands.h"       // SV_ExecuteClientCommand, SV_ClientCommand
#include "sv_client_usercmd.h"        // SV_UserMove, SV_ClientThink, SV_ExecuteClientMessage
