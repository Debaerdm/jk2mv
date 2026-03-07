// sv_init_mapload.h -- Map loading and server spawning
// Extracted from sv_init.cpp as part of aggressive server refactoring

#ifndef SV_INIT_MAPLOAD_H
#define SV_INIT_MAPLOAD_H

/*
================
SV_ClearServer
================
*/
void SV_ClearServer(void) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( (void *)sv.configstrings[i] );
		}
	}

	Com_Memset (&sv, 0, sizeof(sv));
}

/*
================
SV_TouchCGame

Touch the cgame.vm and ui.qvm so that a client can load it if it's in a seperate pk3
================
*/
void SV_TouchCGame(void) {
	fileHandle_t	f;

	FS_FOpenFileRead( "vm/cgame.qvm", &f, qfalse );
	if ( f ) {
		FS_FCloseFile( f );
	}

	FS_FOpenFileRead( "vm/ui.qvm", &f, qfalse);
	if (f) {
		FS_FCloseFile(f);
	}
}

void SV_SendMapChange(void)
{
	int		i;

	if (svs.clients)
	{
		for (i=0 ; i<sv_maxclients->integer ; i++)
		{
			if (svs.clients[i].state >= CS_CONNECTED)
			{
				if ( svs.clients[i].netchan.remoteAddress.type != NA_BOT )
				{
					SV_SendClientMapChange( &svs.clients[i] ) ;
				}
			}
		}
	}
}

void R_SVModelInit();
extern void RE_RegisterMedia_LevelLoadBegin(const char *psMapName, ForceReload_e eForceReload);

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
#ifdef G2_COLLISION_ENABLED
extern CMiniHeap *G2VertSpaceServer;
#define G2_VERT_SPACE_SERVER_SIZE 256
#endif

void SV_SpawnServer( char *server, qboolean killBots, ForceReload_e eForceReload ) {
	int			i;
	int			checksum;
	qboolean	isBot;
	char		systemInfo[16384];
	const char	*p;
	qboolean	resetTime;

	Com_Printf("------ Server Initialization ------\n");
	Com_Printf("Server: %s\n", server);

	SV_SendMapChange();

	RE_RegisterMedia_LevelLoadBegin(server, eForceReload);

	// shut down the existing game if it is running
	SV_ShutdownGameProgs();

	// de allocate the snapshot entities
	if (svs.snapshotEntities)
	{
		delete[] svs.snapshotEntities;
		svs.snapshotEntities = NULL;
	}

	SV_SendMapChange();

	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	CL_MapLoading();

#ifndef DEDICATED
	// make sure all the client stuff is unloaded
	CL_ShutdownAll();
#endif

	CM_ClearMap();

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

	// clear out those shaders, images and Models as long as this
	// isnt a dedicated server.
	if ( !com_dedicated->integer )
	{
#ifndef DEDICATED
		R_InitImages();
		R_InitShaders();
		R_ModelInit();
#endif
	}
	else
	{
		R_SVModelInit();

#ifdef G2_COLLISION_ENABLED
		if (!G2VertSpaceServer)
		{
			G2VertSpaceServer = new CMiniHeap(G2_VERT_SPACE_SERVER_SIZE * 1024);
		}
#endif
	}

	SV_SendMapChange();

	// init client structures and svs.numSnapshotEntities
	if ( !Cvar_VariableValue("sv_running") ) {
		SV_Startup();
	} else {
		// check for maxclients change
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

	SV_SendMapChange();

	// clear pak references
	FS_ClearPakReferences(0);

	// allocate the snapshot entities on the hunk
	svs.nextSnapshotEntities = 0;

	// allocate the snapshot entities
	svs.snapshotEntities = new entityState_s[svs.numSnapshotEntities];
	// we CAN afford to do this here, since we know the STL vectors in Ghoul2 are empty
	memset(svs.snapshotEntities, 0, sizeof(entityState_t)*svs.numSnapshotEntities);

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overriden
	// by the game startup or another console command
	Cvar_Set( "nextmap", "map_restart 0");

	for (i = 0; i < sv_maxclients->integer; i++) {
		// save when the server started for each client already connected
		if (svs.clients[i].state >= CS_CONNECTED) {
			svs.clients[i].oldServerTime = sv.time;
		}
	}

	// close all filehandles before FS_Restart
	for (i = 0; i < sv_maxclients->integer; i++) {
		SV_CloseDownload( &svs.clients[i] );
	}

	// check sv.resetServerTime before clearing sv struct
	if (sv.resetServerTime) {
		resetTime = (qboolean)(sv.resetServerTime == 1);
	} else if (mv_resetServerTime->integer == 1) {
		resetTime = (qboolean)(sv_gametype->integer != GT_TOURNAMENT);
	} else {
		resetTime = (qboolean)(mv_resetServerTime->integer == 2);
	}

	// wipe the entire per-level structure
	SV_ClearServer();
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString("");
	}

	if (!resetTime) {
		// Keep old game module time as original engine did
		sv.time = svs.time;
	}

	// decide which serverversion to host
	mv_serverversion = Cvar_Get("mv_serverversion", "1.04", CVAR_ARCHIVE | CVAR_LATCH | CVAR_GLOBAL);
	if (FS_AllPath_Base_FileExists("assets5.pk3") && (!strcmp(mv_serverversion->string, "auto") || !strcmp(mv_serverversion->string, "1.04"))) {
		Com_Printf("serverversion set to 1.04\n");
		MV_SetCurrentGameversion(VERSION_1_04);
	}
	else if (FS_AllPath_Base_FileExists("assets2.pk3") && (!strcmp(mv_serverversion->string, "auto") || !strcmp(mv_serverversion->string, "1.03"))) {
		Com_Printf("serverversion set to 1.03\n");
		MV_SetCurrentGameversion(VERSION_1_03);
	} else {
		Com_Printf("serverversion set to 1.02\n");
		MV_SetCurrentGameversion(VERSION_1_02);
	}

	Cvar_Set("protocol", va("%i", MV_GetCurrentProtocol()));

	// make sure we are not paused
	Cvar_Set("cl_paused", "0");

	// get a new checksum feed and restart the file system
	srand(Com_Milliseconds());
	sv.checksumFeed = ( ((int) rand() << 16) ^ rand() ) ^ Com_Milliseconds();

	FS_PureServerSetReferencedPaks("", "");
	FS_Restart( sv.checksumFeed );

	CM_LoadMap( va("maps/%s.bsp", server), qfalse, &checksum );

	SV_SendMapChange();

	// set serverinfo visible name
	Cvar_Set( "mapname", server );
	Cvar_Set( "sv_mapChecksum", va("%i",checksum) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// load and spawn all other entities
	SV_InitGameProgs();

	// If the game module didn't announce it can handle it we want to abort now
	if ( CM_NumInlineModels() > MAX_SUBMODELS && !sv.submodelBypass ) {
		Com_Error( ERR_DROP, "MAX_SUBMODELS exceeded (game module doesn't support submodel bypass)" );
	}

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	// run a few frames to allow everything to settle
	for ( i = 0 ;i < 3 ; i++ ) {
		VM_Call( gvm, GAME_RUN_FRAME, sv.time );
		SV_BotFrame( sv.time );
		sv.time += 100;
		svs.time += 100;
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	for (i=0 ; i<sv_maxclients->integer ; i++) {
		// send the new gamestate to all connected clients
		if (svs.clients[i].state >= CS_CONNECTED) {
			const char	*denied;

			if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
				if ( killBots ) {
					SV_DropClient( &svs.clients[i], "" );
					continue;
				}
				isBot = qtrue;
			}
			else {
				isBot = qfalse;
			}

			// connect the client again
			denied = (const char *)VM_ExplicitArgString( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if( !isBot ) {
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
				}
				else {
					client_t		*client;
					sharedEntity_t	*ent;

					client = &svs.clients[i];
					client->state = CS_ACTIVE;
					ent = SV_GentityNum( i );
					ent->s.number = i;
					client->gentity = ent;

					client->deltaMessage = -1;
					client->nextSnapshotTime = svs.time;

					VM_Call( gvm, GAME_CLIENT_BEGIN, i );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	VM_Call( gvm, GAME_RUN_FRAME, sv.time );
	SV_BotFrame( sv.time );
	sv.time += 100;
	svs.time += 100;

	svs.hibernation.disableUntil = svs.time + 10000;

	// if a dedicated server we need to touch the cgame and ui because it could be in a
	// seperate pk3 file and the client will need to load the latest cgame.qvm
	if (com_dedicated->integer) {
		SV_TouchCGame();
	}

	if ( sv_pure->integer ) {
		// the server sends these to the clients so they will only
		// load pk3s also loaded at the server
		p = FS_LoadedPakChecksums();
		Cvar_Set( "sv_paks", p );
		if (strlen(p) == 0) {
			Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
		}
		p = FS_LoadedPakNames();
		Cvar_Set( "sv_pakNames", p );
	}
	else {
		Cvar_Set( "sv_paks", "" );
		Cvar_Set( "sv_pakNames", "" );
	}
	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );
	p = FS_ReferencedPakNames();
	Cvar_Set( "sv_referencedPakNames", p );

	// save systeminfo and serverinfo strings
	Q_strncpyz( systemInfo, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), sizeof( systemInfo ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	SV_SetConfigstring( CS_SYSTEMINFO, systemInfo );

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

	if (!mv_httpdownloads || mv_httpdownloads->modified || mv_httpserverport->modified) {
		NET_HTTP_StopServer();
	}

	// here because latched
	mv_httpdownloads = Cvar_Get("mv_httpdownloads", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH);
	mv_httpserverport = Cvar_Get("mv_httpserverport", "0", CVAR_ARCHIVE | CVAR_LATCH);

	if (mv_httpdownloads->integer) {
		if (!Q_stricmpn(mv_httpserverport->string, "http://", strlen("http://"))) {
			Com_Printf("HTTP Downloads: redirecting to %s\n", mv_httpserverport->string);
		} else {
			sv.http_port = NET_HTTP_StartServer(mv_httpserverport->integer);
			// allow connected clients to use HTTP server
			for (i = 0; i < sv_maxclients->integer; i++) {
				if (svs.clients[i].state >= CS_CONNECTED) {
					NET_HTTP_AllowClient(i, svs.clients[i].netchan.remoteAddress);
				}
			}
		}
	}

	SVC_LoadWhitelist();

	Com_Printf ("-----------------------------------\n");
}

#endif // SV_INIT_MAPLOAD_H
