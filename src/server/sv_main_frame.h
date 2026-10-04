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
	int		gameFrames;
	int64_t	lap, now, gameStart;
	uint64_t	built;

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

	// every stage is timed for serverstats (sv_main_stats.h)
	lap = Sys_Microseconds();

	// if it isn't time for the next frame, do nothing
	if ( sv_fps->integer < 1 ) {
		Cvar_Set( "sv_fps", "10" );
	} else if ( sv_fps->integer > 1000 ) {
		// 1000 / sv_fps would be 0 and the frame loop below would never end
		Cvar_Set( "sv_fps", "1000" );
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

	if (!com_dedicated->integer) {
		lap = SV_StatsLap( SVSTAT_OTHER, lap );
		SV_BotFrame( sv.time + sv.timeResidual );
		lap = SV_StatsLap( SVSTAT_BOTAI, lap );
	}

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
	if ( SV_SnapshotEntitiesWrapping( svs.nextSnapshotEntities, sv_maxclients->integer ) ) {
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

	lap = SV_StatsLap( SVSTAT_OTHER, lap );
	gameStart = lap;

	// update ping based on the all received frames
	SV_CalcPings();
	lap = SV_StatsLap( SVSTAT_PINGS, lap );

	if (com_dedicated->integer) {
		SV_BotFrame( sv.time );
		lap = SV_StatsLap( SVSTAT_BOTAI, lap );
	}

	if (sv.saberBlockTime < sv.time) {
		sv.saberBlockCounter = 0;
		sv.saberBlockTime = sv.time + 1000;
	}

	// run the game simulation in chunks
	gameFrames = 0;
	while ( sv.timeResidual >= frameMsec ) {
		sv.timeResidual -= frameMsec;
		sv.time += frameMsec;
		svs.time += frameMsec;

		// let everything in the world think and move
		VM_Call( gvm, GAME_RUN_FRAME, sv.time );
		MV_FixSaberStealing();
		gameFrames++;
	}
	lap = SV_StatsLap( SVSTAT_GAME, lap );

	if ( com_speeds->integer ) {
		time_game = (int)( lap - gameStart );
	}

	// check timeouts
	SV_CheckTimeouts();
	lap = SV_StatsLap( SVSTAT_OTHER, lap );

	// send messages back to the clients
	built = svStats.pending.usec[SVSTAT_SNAPBUILD];
	SV_SendClientMessages();
	now = Sys_Microseconds();
	// the snapshots were built in there, and that time has its own stage
	built = svStats.pending.usec[SVSTAT_SNAPBUILD] - built;
	if ( now - lap > (int64_t)built ) {
		svStats.pending.usec[SVSTAT_SNAPSEND] += (uint64_t)( now - lap ) - built;
	}
	lap = now;

	SV_CheckCvars();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat();
	lap = SV_StatsLap( SVSTAT_OTHER, lap );

	SV_StatsEndFrame( lap, gameFrames, frameMsec,
		svs.hibernation.enabled && svs.hibernation.disableUntil <= svs.time ? SVSTAT_HIBERNATING : 0 );
}

#endif // SV_MAIN_FRAME_H
