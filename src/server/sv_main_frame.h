// sv_main_frame.h -- Main server frame loop
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_FRAME_H
#define SV_MAIN_FRAME_H

/*
==================
SV_FrameMsec
Return time in millseconds until processing of the next server frame.
==================
*/
int SV_FrameMsec() {
	if (sv_fps) {
		int frameMsec;

		if ( svs.hibernation.enabled && svs.hibernation.disableUntil <= svs.time ) {
			frameMsec = 1000.0f / (sv_hibernateFps->integer > 0 ? sv_hibernateFps->integer : 5);
		} else {
			frameMsec = 1000.0f / sv_fps->value;
		}

		if (frameMsec < sv.timeResidual)
			return 0;
		else
			return frameMsec - sv.timeResidual;
	} else
		return 1;
}

static void MV_FixSaberStealing( void )
{
	if ( mv_fixsaberstealing->integer && !(sv.fixes & MVFIX_SABERSTEALING) ) {
		playerState_t	*ps;
		int				i;

		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			ps = SV_GameClientNum( i );

			if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
				ps->saberEntityNum = ENTITYNUM_NONE;
			}
		}
	}
}

/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
void SV_Frame( int msec ) {
	int		frameMsec;
	int		startTime;

	// the menu kills the server with this cvar
	if ( sv_killserver->integer ) {
		SV_Shutdown ("Server was killed");
		Cvar_Set( "sv_killserver", "0" );
		return;
	}

	if ( !com_sv_running->integer ) {
		return;
	}

	// allow pause if only the local client is connected
	if ( SV_CheckPaused() ) {
		return;
	}

	// if it isn't time for the next frame, do nothing
	if ( sv_fps->integer < 1 ) {
		Cvar_Set( "sv_fps", "10" );
	}
	frameMsec = 1000 / sv_fps->integer ;

	sv.timeResidual += msec;

	// Check for hibernation mode
	int players = 0;
	for (int i = 0; i < sv_maxclients->integer; i++) {
		if (svs.clients[i].state >= CS_CONNECTED && svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
			players++;
		}
	}

	if ( sv_hibernateFps->integer && !svs.hibernation.enabled && !players ) {
		svs.hibernation.enabled = true;
		Com_Printf("Server switched to hibernation mode\n");
	} else if  (!sv_hibernateFps->integer && svs.hibernation.enabled ) {
		svs.hibernation.enabled = false;
		Com_Printf("Server restored from hibernation\n");
	}

	if (!com_dedicated->integer) SV_BotFrame( sv.time + sv.timeResidual );

	// if time is about to hit the 32nd bit, kick all clients
	// and clear sv.time, rather
	// than checking for negative time wraparound everywhere.
	// 2giga-milliseconds = 23 days, so it won't be too often
	if ( svs.time > 0x70000000 ) {
		SV_Shutdown( "Restarting server due to time wrapping" );
		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "mapname" ) ) );
		return;
	}
	// this can happen considerably earlier when lots of clients play and the map doesn't change
	if ( svs.nextSnapshotEntities >= 0x7FFFFFFE - svs.numSnapshotEntities ) {
		SV_Shutdown( "Restarting server due to numSnapshotEntities wrapping" );
		Cbuf_AddText( va( "map %s\n", Cvar_VariableString( "mapname" ) ) );
		return;
	}

	if( sv.restartTime && sv.time >= sv.restartTime ) {
		sv.restartTime = 0;
		Cbuf_AddText( "map_restart 0\n" );
		return;
	}

	// update infostrings if anything has been changed
	if ( cvar_modifiedFlags & CVAR_SERVERINFO ) {
		SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
		cvar_modifiedFlags &= ~CVAR_SERVERINFO;
	}
	if ( cvar_modifiedFlags & CVAR_SYSTEMINFO ) {
		SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
		cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	}

	if ( com_speeds->integer ) {
		startTime = Sys_Milliseconds ();
	} else {
		startTime = 0;	// quite a compiler warning
	}

	// update ping based on the all received frames
	SV_CalcPings();

	if (com_dedicated->integer) SV_BotFrame( sv.time );

	if (sv.saberBlockTime < sv.time) {
		sv.saberBlockCounter = 0;
		sv.saberBlockTime = sv.time + 1000;
	}

	// run the game simulation in chunks
	while ( sv.timeResidual >= frameMsec ) {
		sv.timeResidual -= frameMsec;
		sv.time += frameMsec;
		svs.time += frameMsec;

		// let everything in the world think and move
		VM_Call( gvm, GAME_RUN_FRAME, sv.time );
		MV_FixSaberStealing();
	}

	if ( com_speeds->integer ) {
		time_game = Sys_Milliseconds () - startTime;
	}

	// check timeouts
	SV_CheckTimeouts();

	// send messages back to the clients
	SV_SendClientMessages();

	SV_CheckCvars();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat();
}

#endif // SV_MAIN_FRAME_H
