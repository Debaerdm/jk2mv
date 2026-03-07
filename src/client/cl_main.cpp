// cl_main.c  -- client main loop

#include "client.h"
#include "../qcommon/strip.h"
#include <limits.h>
#include "snd_local.h"
#include <mv_setup.h>

#if !defined(G2_H_INC)
	#include "../ghoul2/G2_local.h"
#endif

#ifdef G2_COLLISION_ENABLED
#if !defined (MINIHEAP_H_INC)
#include "../qcommon/MiniHeap.h"
#endif
#endif

#ifdef _DONETPROFILE_
#include "../qcommon/INetProfile.h"
#endif

// ============================================================================
// MODULAR INCLUDES - Aggressive Phase 2 Refactoring
// Each module handles a distinct responsibility from the original cl_main.cpp
// ============================================================================

#include "cl_video.h"          // AVI recording (CL_Video_f, CL_StopVideo_f, CL_OpenAVIForWriting)
#include "cl_reliable.h"       // Reliable command queue management
#include "cl_demo.h"           // Demo recording/playback system
#include "cl_connect.h"        // Connection establishment & challenge/response
#include "cl_download.h"       // File download system with UI prompts
#include "cl_serverinfo.h"     // Server browser, ping requests, server lists
#include "cl_shutdown.h"       // Shutdown, disconnect, memory flushing
#include "cl_console_cmds.h"   // Console command handlers
#include "cl_init.h"           // Client initialization & main loop

// ============================================================================
// GLOBAL STATE - kept centralized for compatibility
// ============================================================================

cvar_t	*cl_nodelta;
cvar_t	*cl_debugMove;
cvar_t	*cl_noprint;
cvar_t	*cl_motd;
cvar_t	*rcon_client_password;
cvar_t	*rconAddress;
cvar_t	*cl_timeout;
cvar_t	*cl_maxpackets;
cvar_t	*cl_packetdup;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;
cvar_t	*cl_freezeDemo;
cvar_t	*cl_drawRecording;
cvar_t	*cl_shownet;
cvar_t	*cl_showSend;
cvar_t	*cl_timedemo;
cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;
cvar_t	*cl_aviMotionJpegQuality;
cvar_t	*cl_forceavidemo;
cvar_t	*cl_freelook;
cvar_t	*cl_sensitivity;
cvar_t	*cl_mouseAccel;
cvar_t	*cl_showMouseRate;
cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;
cvar_t	*m_filter;
cvar_t	*cl_activeAction;
cvar_t	*cl_motdString;
cvar_t	*mv_allowDownload;
cvar_t	*cl_conXOffset;
cvar_t	*cl_inGameVideo;
cvar_t	*cl_serverStatusResendTime;
cvar_t	*cl_trn;
cvar_t	*cl_framerate;
cvar_t	*cl_autolodscale;
cvar_t	*mv_slowrefresh;
cvar_t	*mv_coloredTextShadows;
cvar_t	*mv_consoleShiftRequirement;
cvar_t	*mv_menuOverride;
cvar_t	*cl_downloadName;
cvar_t	*cl_downloadLocalName;
cvar_t	*cl_downloadSize;
cvar_t	*cl_downloadCount;
cvar_t	*cl_downloadTime;
cvar_t	*cl_downloadProtocol;

vec3_t cl_windVec;

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
vm_t				*cgvm;

netadr_t rcon_address;

refexport_t	re;

ping_t	cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];
int serverStatusCount;

#ifdef G2_COLLISION_ENABLED
CMiniHeap *G2VertSpaceClient = 0;
#endif

#if defined __USEA3D && defined __A3D_GEOM
	void hA3Dg_ExportRenderGeom (refexport_t *incoming_re);
#endif

extern void SV_BotFrame( int time );

// ============================================================================
// STRIPPED DOWN cl_main.cpp - All heavy lifting moved to dedicated modules
// This file now serves as the central orchestrator and state container
// ============================================================================
