// sv_init_lifecycle.h -- Server lifecycle (init, shutdown)
// Extracted from sv_init.cpp as part of aggressive server refactoring

#ifndef SV_INIT_LIFECYCLE_H
#define SV_INIT_LIFECYCLE_H

/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_BotInitBotLib(void);

void SV_Init (void) {
	SV_AddOperatorCommands ();

	Cvar_Get("JK2MV", JK2MV_VERSION, CVAR_SERVERINFO | CVAR_ROM);

	mv_serverversion = Cvar_Get("mv_serverversion", "1.04", CVAR_ARCHIVE | CVAR_LATCH);

	mv_fixnamecrash = Cvar_Get("mv_fixnamecrash", "1", CVAR_ARCHIVE);
	mv_fixforcecrash = Cvar_Get("mv_fixforcecrash", "1", CVAR_ARCHIVE);
	mv_fixgalaking = Cvar_Get("mv_fixgalaking", "1", CVAR_ARCHIVE);
	mv_fixbrokenmodels = Cvar_Get("mv_fixbrokenmodels", "1", CVAR_ARCHIVE);
	mv_fixturretcrash = Cvar_Get("mv_fixturretcrash", "1", CVAR_ARCHIVE);
	mv_blockchargejump = Cvar_Get("mv_blockchargejump", "1", CVAR_ARCHIVE);
	mv_blockspeedhack = Cvar_Get("mv_blockspeedhack", "1", CVAR_ARCHIVE);
	mv_fixsaberstealing = Cvar_Get("mv_fixsaberstealing", "1", CVAR_ARCHIVE);
	mv_fixplayerghosting = Cvar_Get("mv_fixplayerghosting", "1", CVAR_ARCHIVE);

	mv_resetServerTime = Cvar_Get("mv_resetServerTime", "1", CVAR_ARCHIVE);

	// serverinfo vars
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO);
	Cvar_Get ("fraglimit", "20", CVAR_SERVERINFO);
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);

	Cvar_Get ("g_maxHolocronCarry", "3", CVAR_SERVERINFO);
	Cvar_Get ("g_privateDuel", "1", CVAR_SERVERINFO );
	Cvar_Get ("g_saberLocking", "1", CVAR_SERVERINFO );
	Cvar_Get ("g_maxForceRank", "6", CVAR_SERVERINFO );
	Cvar_Get ("duel_fraglimit", "10", CVAR_SERVERINFO);
	Cvar_Get ("g_forceBasedTeams", "0", CVAR_SERVERINFO);
	Cvar_Get ("g_duelWeaponDisable", "1", CVAR_SERVERINFO);

	sv_gametype = Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH );
	sv_needpass = Cvar_Get ("g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM );
	Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
	sv_privateClients = Cvar_Get ("sv_privateClients", "0", CVAR_SERVERINFO);
	sv_hostname = Cvar_Get ("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
	sv_maxclients = Cvar_Get ("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
	sv_minSnaps = Cvar_Get("sv_minSnaps", "1", CVAR_ARCHIVE);
	sv_maxSnaps = Cvar_Get("sv_maxSnaps", "30", CVAR_ARCHIVE);
	sv_enforceSnaps = Cvar_Get("sv_enforceSnaps", "0", CVAR_ARCHIVE);
	sv_minRate = Cvar_Get("sv_minRate", "1000", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxRate = Cvar_Get ("sv_maxRate", "90000", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxOOBRate = Cvar_Get ("sv_maxOOBRate", "20", CVAR_ARCHIVE | CVAR_GLOBAL );
	sv_minPing = Cvar_Get ("sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxPing = Cvar_Get ("sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_floodProtect = Cvar_Get ("sv_floodProtect", "3", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_allowAnonymous = Cvar_Get ("sv_allowAnonymous", "0", CVAR_SERVERINFO);

	// systeminfo
	Cvar_Get ("sv_cheats", "0", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_serverid = Cvar_Get ("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );
#ifndef DLL_ONLY
	sv_pure = Cvar_Get ("sv_pure", "0", CVAR_SYSTEMINFO );
#else
	sv_pure = Cvar_Get ("sv_pure", "0", CVAR_SYSTEMINFO | CVAR_INIT | CVAR_ROM );
#endif
	Cvar_Get ("sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );

	Cvar_Get ("mv_cs_remaps", "", CVAR_SYSTEMINFO | CVAR_ROM | CVAR_INTERNAL );

	// server vars
	sv_rconPassword = Cvar_Get ("rconPassword", "", CVAR_TEMP );
	sv_privatePassword = Cvar_Get ("sv_privatePassword", "", CVAR_TEMP );
	sv_fps = Cvar_Get ("sv_fps", "20", CVAR_TEMP );
	sv_timeout = Cvar_Get ("sv_timeout", "200", CVAR_TEMP );
	sv_zombietime = Cvar_Get ("sv_zombietime", "2", CVAR_TEMP );
	Cvar_Get ("nextmap", "", CVAR_TEMP );

	sv_allowDownload = Cvar_Get ("sv_allowDownload", "0", CVAR_SERVERINFO);
	sv_master[0] = Cvar_Get("sv_master1", "masterjk2.ravensoft.com", CVAR_ROM);
	Cvar_Set("sv_master1", "masterjk2.ravensoft.com");
	sv_master[1] = Cvar_Get("sv_master2", "master.jk2mv.org", CVAR_ROM);
	Cvar_Set("sv_master2", "master.jk2mv.org");
	sv_master[2] = Cvar_Get("sv_master3", "master.jkhub.org", CVAR_ROM);
	Cvar_Set("sv_master3", "master.jkhub.org");
	sv_master[3] = Cvar_Get("sv_master4", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[4] = Cvar_Get("sv_master5", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[5] = Cvar_Get("sv_master6", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[6] = Cvar_Get("sv_master7", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[7] = Cvar_Get("sv_master8", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_reconnectlimit = Cvar_Get ("sv_reconnectlimit", "3", 0);
	sv_showloss = Cvar_Get ("sv_showloss", "0", 0);
	sv_padPackets = Cvar_Get ("sv_padPackets", "0", 0);
	sv_killserver = Cvar_Get ("sv_killserver", "0", 0);
	sv_mapChecksum = Cvar_Get ("sv_mapChecksum", "", CVAR_ROM);

	sv_hibernateFps = Cvar_Get("sv_hibernateFps", "4", CVAR_ARCHIVE | CVAR_GLOBAL);
	mv_apiConnectionless = Cvar_Get("mv_apiConnectionless", "1", CVAR_ARCHIVE | CVAR_INIT | CVAR_VM_NOWRITE);
	sv_pingFix = Cvar_Get("sv_pingFix", "1", CVAR_ARCHIVE);
	sv_autoWhitelist = Cvar_Get("sv_autoWhitelist", "1", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_dynamicSnapshots = Cvar_Get("sv_dynamicSnapshots", "1", CVAR_ARCHIVE);

	SP_Register("str_server",SP_REGISTER_REQUIRED);

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

	SVC_LoadWhitelist();
}

/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( char *message ) {
	int			i, j;
	client_t	*cl;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for (i=0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++) {
			if (cl->state >= CS_CONNECTED) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"", message );
					SV_SendServerCommand( cl, "disconnect" );
				}
				// force a snapshot to be sent
				cl->nextSnapshotTime = -1;
				SV_SendClientSnapshot( cl );
			}
		}
	}
}

/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( char *finalmsg )
{
	if ( !com_sv_running || !com_sv_running->integer )
	{
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();

 	// de allocate the snapshot entities
	if (svs.snapshotEntities)
	{
		delete[] svs.snapshotEntities;
		svs.snapshotEntities = NULL;
	}

#ifdef G2_COLLISION_ENABLED
	if ( com_dedicated->integer && G2VertSpaceServer)
	{
		delete G2VertSpaceServer;
		G2VertSpaceServer = 0;
	}
#endif

	// free current level
	SV_ClearServer();

	// free server static data
	if ( svs.clients ) {
		Z_Free( svs.clients );
	}
	Com_Memset( &svs, 0, sizeof( svs ) );

	Cvar_Set( "sv_running", "0" );
	Cvar_Set("ui_singlePlayerActive", "0");

	Com_Printf( "---------------------------\n" );

	// disconnect any local clients
	CL_Disconnect( qfalse );

	NET_HTTP_StopServer();
	sv.http_port = 0;
}

#endif // SV_INIT_LIFECYCLE_H
