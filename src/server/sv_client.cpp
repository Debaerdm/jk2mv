// sv_client.cpp -- server code for dealing with clients
// Refactored into modular architecture

#include "server.h"
#include "../qcommon/strip.h"

#include <mv_setup.h>

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_client.cpp (1400 lines) decomposed into 8 focused modules:
//
// 1. Challenge System        - OOB challenge handling
// 2. Connection Logic        - DirectConnect, slot allocation
// 3. Disconnect Logic        - Client drop, cleanup
// 4. Gamestate Transmission  - Initial gamestate, map changes
// 5. Download System         - File download handling
// 6. Command Execution       - Client command processing
// 7. Userinfo Validation     - Crash fixes (name, force, galak, models)
// 8. Usercmd Processing      - Movement, rate limiting
// ============================================================================

#include "sv_client_challenge.h"
#include "sv_client_connect.h"
#include "sv_client_disconnect.h"
#include "sv_client_gamestate.h"
#include "sv_client_download.h"
#include "sv_client_userinfo.h"
#include "sv_client_commands.h"
#include "sv_client_usercmd.h"
