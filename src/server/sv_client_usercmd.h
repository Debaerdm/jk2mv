// sv_client_usercmd.h -- Client usercmd processing
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_USERCMD_H
#define SV_CLIENT_USERCMD_H

/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink (int client, const usercmd_t *cmd) {
	int64_t	startTime;

	if (client < 0 || sv_maxclients->integer <= client) {
		Com_DPrintf( S_COLOR_YELLOW "SV_ClientThink: bad clientNum %i\n", client );
		return;
	}

	svs.clients[client].lastUsercmd = *cmd;

	if ( svs.clients[client].state != CS_ACTIVE ) {
		return;
	}

	if ( svs.clients[client].lastUserInfoCount >= INFO_CHANGE_MAX_COUNT && svs.clients[client].lastUserInfoChange < svs.time && svs.clients[client].userinfoPostponed[0] )
	{
		client_t *cl = &svs.clients[client];
		char info[MAX_INFO_STRING];

		Q_strncpyz( cl->userinfo, cl->userinfoPostponed, sizeof(cl->userinfo) );
		SV_UserinfoChanged( cl );

		VM_Call( gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients );

		SV_GetConfigstring(CS_PLAYERS + (cl - svs.clients), info, sizeof(info));
		Info_SetValueForKey(cl->userinfo, "name", Info_ValueForKey(info, "n"));
		Q_strncpyz(cl->name, Info_ValueForKey(info, "n"), sizeof(cl->name));

		cl->userinfoPostponed[0] = 0;
		cl->lastUserInfoCount = 0;
		cl->lastUserInfoChange = svs.time + INFO_CHANGE_MIN_INTERVAL;
	}

	startTime = Sys_Microseconds();

	VM_Call( gvm, GAME_CLIENT_THINK, client );

	// for clientstats
	svs.clients[client].usercmdStats.current.cmds++;
	svs.clients[client].usercmdStats.current.thinkUsec += (int)( Sys_Microseconds() - startTime );
}

/*
==================
SV_UserMove

The message usually contains all the movement commands
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.

With sv_maxUsercmdRate, only the usercmds the client may run now
are run, the newest ones (see SV_UsercmdSchedule).
==================
*/
static void SV_UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	int			i, key;
	int			cmdCount;
	usercmd_t	nullcmd;
	usercmd_t	cmds[MAX_PACKET_USERCMDS];
	usercmd_t	*cmd, *oldcmd;
	int			rate;
	int			serverTimes[MAX_PACKET_USERCMDS];
	int			run[MAX_PACKET_USERCMDS];
	int			numRun, dropped;

	if ( delta ) {
		cl->deltaMessage = cl->messageAcknowledge;
	} else {
		cl->deltaMessage = -1;
	}

	cmdCount = MSG_ReadByte( msg );

	if ( cmdCount < 1 ) {
		Com_Printf( "cmdCount < 1\n" );
		return;
	}

	if ( cmdCount > MAX_PACKET_USERCMDS ) {
		Com_Printf( "cmdCount > MAX_PACKET_USERCMDS\n" );
		return;
	}

	key = sv.checksumFeed;
	key ^= cl->messageAcknowledge;
	key ^= Com_HashKey(cl->reliableCommands[ cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ], 32);

	Com_Memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;
	for ( i = 0 ; i < cmdCount ; i++ ) {
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmdKey( msg, key, oldcmd, cmd );
		oldcmd = cmd;
	}

	if ( !sv_pingFix->integer || cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked == -1 )
		cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = (sv_pingFix->integer ? Sys_Milliseconds() : svs.time);

	if ( cl->state == CS_PRIMED ) {
		SV_SendServerCommand(cl, "print \"^1[ ^7This server is running JK2MV ^1v^7" JK2MV_VERSION " ^1| ^7http://jk2mv.org ^1]\n\"");

		SV_ClientEnterWorld( cl, &cmds[0] );
	}

	if (sv_pure->integer != 0 && cl->pureAuthentic == 0) {
		SV_DropClient( cl, "Cannot validate pure client!");
		return;
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = -1;
		return;
	}

	rate = SV_UsercmdRateCap( sv_maxUsercmdRate->integer );

	// never cap the local player of a listen server
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		rate = 0;
	}

	if ( !rate ) {
		for ( i =  0 ; i < cmdCount ; i++ ) {
			if ( cmds[i].serverTime > cmds[cmdCount-1].serverTime ) {
				continue;
			}
			if ( cmds[i].serverTime <= cl->lastUsercmd.serverTime ) {
				continue;
			}
			SV_ClientThink (cl - svs.clients, &cmds[ i ]);
		}
		return;
	}

	for ( i = 0 ; i < cmdCount ; i++ ) {
		serverTimes[i] = cmds[i].serverTime;
	}

	numRun = SV_UsercmdSchedule( &cl->usercmdBucket, rate, Sys_Milliseconds(),
		serverTimes, cmdCount, cl->lastUsercmd.serverTime, run, &dropped );
	cl->usercmdStats.current.dropped += dropped;

	for ( i = 0 ; i < numRun ; i++ ) {
		SV_ClientThink( cl - svs.clients, &cmds[ run[i] ] );
	}
}

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage( client_t *cl, msg_t *msg ) {
	int			c;
	int			serverId;

	// for clientstats
	cl->usercmdStats.current.packets++;

	MSG_Bitstream(msg);

	serverId = MSG_ReadLong( msg );
	cl->messageAcknowledge = MSG_ReadLong( msg );

	if (cl->messageAcknowledge < 0) {
		return;
	}

	cl->reliableAcknowledge = MSG_ReadLong( msg );

	if (cl->reliableAcknowledge < cl->reliableSequence - MAX_RELIABLE_COMMANDS) {
		cl->reliableAcknowledge = cl->reliableSequence;
		return;
	}

	if ( serverId != sv.serverId &&
		!*cl->downloadName ) {
		if ( serverId == sv.restartedServerId ) {
			return;
		}
		if ( cl->messageAcknowledge > cl->gamestateMessageNum ) {
			Com_DPrintf( "%s : dropped gamestate, resending\n", cl->name );
			SV_SendClientGameState( cl );
		}
		return;
	}

	if( cl->oldServerTime && serverId == sv.serverId ){
		Com_DPrintf( "%s acknowledged gamestate\n", cl->name );
		cl->oldServerTime = 0;
	}

	do {
		c = MSG_ReadByte( msg );
		if ( c == clc_EOF ) {
			break;
		}
		if ( c != clc_clientCommand ) {
			break;
		}
		if ( !SV_ClientCommand( cl, msg ) ) {
			return;
		}
		if (cl->state == CS_ZOMBIE) {
			return;
		}
	} while ( 1 );

	if ( c == clc_move ) {
		SV_UserMove( cl, msg, qtrue );
	} else if ( c == clc_moveNoDelta ) {
		SV_UserMove( cl, msg, qfalse );
	} else if ( c != clc_EOF ) {
		Com_Printf( "WARNING: bad command byte for client %i\n", (int)(cl - svs.clients) );
	}
}

int SV_ClientRate( client_t *client )
{
	int minRate = sv_minRate->integer;
	int maxRate = sv_maxRate->integer;

	if ( !maxRate ) maxRate = 90000;

	if ( minRate < 1000 ) minRate = 1000;
	if ( maxRate < 1000 ) maxRate = 1000;

	if ( minRate > maxRate ) minRate = maxRate;

	return Com_Clampi( minRate, maxRate, client->rate );
}

int SV_ClientSnaps( client_t *client )
{
	int maxSnaps = Com_Clampi( 1, sv_fps->integer, sv_maxSnaps->integer );
	int minSnaps = Com_Clampi( 1, maxSnaps, sv_minSnaps->integer );

	int wishSnaps = sv_enforceSnaps->integer ? sv_fps->integer : atoi(Info_ValueForKey(client->userinfo, "snaps"));

	return Com_Clampi( minSnaps, maxSnaps, wishSnaps );
}

void SV_ClientUpdateSnaps( client_t *client )
{
	int snapsMsec = 1000 / SV_ClientSnaps( client );

	if ( snapsMsec != client->snapshotMsec ) {
		client->nextSnapshotTime = -1;
		client->snapshotMsec = snapsMsec;
	}
}

/*
==================
SV_UpdateUsercmdStats

Called every server frame: closes the second of usercmd counts that
clientstats shows
==================
*/
void SV_UpdateUsercmdStats( void ) {
	int			now = Sys_Milliseconds();
	int			elapsed = (int)( (unsigned)now - (unsigned)svs.usercmdStatsTime );
	int			i;
	client_t	*cl;

	if ( elapsed < 0 ) {
		// the clock went back, count this second again
		svs.usercmdStatsTime = now;
		return;
	}

	if ( elapsed < 1000 ) {
		return;
	}

	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ ) {
		if ( cl->state != CS_FREE ) {
			SV_UsercmdStatsRoll( &cl->usercmdStats, elapsed );
		}
	}

	svs.usercmdStatsTime = now;
}

#endif // SV_CLIENT_USERCMD_H
