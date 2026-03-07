// sv_main.cpp -- server main program
// Refactored into modular architecture

#include "server.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

serverStatic_t	svs;				// persistant server info
server_t		sv;					// local server
vm_t			*gvm = NULL;				// game virtual machine

// ============================================================================
// CVAR DECLARATIONS
// ============================================================================

cvar_t	*sv_fps;
cvar_t	*sv_timeout;
cvar_t	*sv_zombietime;
cvar_t	*sv_rconPassword;
cvar_t	*sv_privatePassword;
cvar_t	*sv_allowDownload;
cvar_t	*mv_httpdownloads;
cvar_t	*mv_httpserverport;
cvar_t	*sv_maxclients;
cvar_t	*sv_privateClients;
cvar_t	*sv_hostname;
cvar_t	*sv_master[MAX_MASTER_SERVERS];
cvar_t	*sv_reconnectlimit;
cvar_t	*sv_showloss;
cvar_t	*sv_padPackets;
cvar_t	*sv_killserver;
cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_serverid;
cvar_t	*sv_minSnaps;
cvar_t	*sv_maxSnaps;
cvar_t	*sv_enforceSnaps;
cvar_t	*sv_minRate;
cvar_t	*sv_maxRate;
cvar_t	*sv_maxOOBRate;
cvar_t	*sv_minPing;
cvar_t	*sv_maxPing;
cvar_t	*sv_gametype;
cvar_t	*sv_pure;
cvar_t	*sv_floodProtect;
cvar_t	*sv_allowAnonymous;
cvar_t	*sv_needpass;
cvar_t	*mv_serverversion;
cvar_t  *sv_hibernateFps;
cvar_t	*mv_apiConnectionless;
cvar_t	*sv_pingFix;
cvar_t	*sv_autoWhitelist;
cvar_t	*sv_dynamicSnapshots;

// jk2mv's toggleable fixes
cvar_t	*mv_fixnamecrash;
cvar_t	*mv_fixforcecrash;
cvar_t	*mv_fixgalaking;
cvar_t	*mv_fixbrokenmodels;
cvar_t	*mv_fixturretcrash;
cvar_t	*mv_blockchargejump;
cvar_t	*mv_blockspeedhack;
cvar_t	*mv_fixsaberstealing;
cvar_t	*mv_fixplayerghosting;

// jk2mv engine flags
cvar_t	*mv_resetServerTime;

// ============================================================================
// MODULAR ARCHITECTURE
// ============================================================================
// Original sv_main.cpp (950 lines) decomposed into 7 focused modules:
//
// 1. Messaging              - Server commands & broadcasts
// 2. Master Server          - Heartbeat communication
// 3. Rate Limiting          - DDoS protection & whitelist
// 4. Connectionless         - Status, info, rcon, MVAPI
// 5. Packets                - Packet event processing
// 6. Clients                - Ping calc, timeouts, paused state
// 7. Frame                  - Main server loop & hibernation
// ============================================================================

#include "sv_main_messaging.h"
#include "sv_main_master.h"
#include "sv_main_ratelimit.h"
#include "sv_main_connectionless.h"
#include "sv_main_packets.h"
#include "sv_main_clients.h"
#include "sv_main_frame.h"
