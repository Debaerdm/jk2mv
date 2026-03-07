// sv_client_usercmd.h -- Usercmd processing & movement
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_USERCMD_H
#define SV_CLIENT_USERCMD_H

int SV_ClientRate( client_t *client )
{
	int minRate = sv_minRate->integer;
	int maxRate = sv_maxRate->integer;

	// Special case for sv_maxRate 0: "unlimited" was hardcoded to 90000 in jk2ded
	if ( !maxRate ) maxRate = 90000;

	// Never allow rates below 1000 (was already hardcoded to 1000 in jk2ded)
	if ( minRate < 1000 ) minRate = 1000;
	if ( maxRate < 1000 ) maxRate = 1000;

	// If the minimum is higher than the maximum settle for the lower value
	if ( minRate > maxRate ) minRate = maxRate;

	// Ensure the rate is within the allowed range
	return Com_Clampi( minRate, maxRate, client->rate );
}

int SV_ClientSnaps( client_t *client )
{
	int maxSnaps = Com_Clampi( 1, sv_fps->integer, sv_maxSnaps->integer );
	int minSnaps = Com_Clampi( 1, maxSnaps, sv_minSnaps->integer );

	// Get the desired snaps value (either sv_fps or the value from the userinfo)
	int wishSnaps = sv_enforceSnaps->integer ? sv_fps->integer : atoi(Info_ValueForKey(client->userinfo, "snaps"));

	// Ensure the snaps value is within the allowed range
	return Com_Clampi( minSnaps, maxSnaps, wishSnaps );
}

void SV_ClientUpdateSnaps( client_t *client )
{
	int snapsMsec = 1000 / SV_ClientSnaps( client );

	if ( snapsMsec != client->snapshotMsec ) {
		// Reset next snapshot so we avoid desync between server frame time and snapshot send time
		client->nextSnapshotTime = -1;
		client->snapshotMsec = snapsMsec;
	}
}

/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink (int client, const usercmd_t *cmd) {
	if (client < 0 || sv_maxclients->integer <= client) {
		Com_DPrintf( S_COLOR_YELLOW "SV_ClientThink: bad clientNum %i\n", client );
		return;
	}

	svs.clients[client].lastUsercmd = *cmd;

	if ( svs.clients[client].state != CS_ACTIVE ) {
		return;		// may have been kicked during the last usercmd
	}

	if ( svs.clients[client].lastUserInfoCount >= INFO_CHANGE_MAX_COUNT && svs.clients[client].lastUserInfoChange < svs.time && svs.clients[client].userinfoPostponed[0] )
	{ // Update postponed userinfo changes now
		client_t *cl = &svs.clients[client];
		char info[MAX_INFO_STRING];

		Q_strncpyz( cl->userinfo, cl->userinfoPostponed, sizeof(cl->userinfo) );
		SV_UserinfoChanged( cl );

		// call prog code to allow overrides
		VM_Call( gvm, GAME_CLIENT_USERINFO_CHANGED, cl - svs.clients );

		// get the name out of the game and set it in the engine
		SV_GetConfigstring(CS_PLAYERS + (cl - svs.clients), info, sizeof(info));
		Info_SetValueForKey(cl->userinfo, "name", Info_ValueForKey(info, "n"));
		Q_strncpyz(cl->name, Info_ValueForKey(info, "n"), sizeof(cl->name));

		// clear it
		cl->userinfoPostponed[0] = 0;
		cl->lastUserInfoCount = 0;
		cl->lastUserInfoChange = svs.time + INFO_CHANGE_MIN_INTERVAL;
	}

	VM_Call( gvm, GAME_CLIENT_THINK, client );
}

/*
==================
SV_UserMove

The message usually contains all the movement commands
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_UserMove( client_t *cl, msg_t *msg, qboolean delta ) {
	int			i, key;
	int			cmdCount;
	usercmd_t	nullcmd;
	usercmd_t	cmds[MAX_PACKET_USERCMDS];
	usercmd_t	*cmd, *oldcmd;

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

	// use the checksum feed in the key
	key = sv.checksumFeed;
	// also use the message acknowledge
	key ^= cl->messageAcknowledge;
	// also use the last acknowledged server command in the key
	key ^= Com_HashKey(cl->reliableCommands[ cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ], 32);

	Com_Memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;
	for ( i = 0 ; i < cmdCount ; i++ ) {
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmdKey( msg, key, oldcmd, cmd );
		oldcmd = cmd;
	}

	// save time for ping calculation
	// With sv_pingFix enabled we store the time of the first acknowledge, instead of the last. And we use a time value that is not limited by sv_fps.
	if ( !sv_pingFix->integer || cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked == -1 )
		cl->frames[ cl->messageAcknowledge & PACKET_MASK ].messageAcked = (sv_pingFix->integer ? Sys_Milliseconds() : svs.time);

	// if this is the first usercmd we have received
	// this gamestate, put the client into the world
	if ( cl->state == CS_PRIMED ) {
		SV_SendServerCommand(cl, "print \"^1[ ^7This server is running JK2MV ^1v^7" JK2MV_VERSION " ^1| ^7http://jk2mv.org ^1]\n\"");

		SV_ClientEnterWorld( cl, &cmds[0] );
		// the moves can be processed normaly
	}
	//
	if (sv_pure->integer != 0 && cl->pureAuthentic == 0) {
		SV_DropClient( cl, "Cannot validate pure client!");
		return;
	}

	if ( cl->state != CS_ACTIVE ) {
		cl->deltaMessage = -1;
		return;
	}

	// usually, the first couple commands will be duplicates
	// of ones we have previously received, but the servertimes
	// in the commands will cause them to be immediately discarded
	for ( i =  0 ; i < cmdCount ; i++ ) {
		// if this is a cmd from before a map_restart ignore it
		if ( cmds[i].serverTime > cmds[cmdCount-1].serverTime ) {
			continue;
		}
		// extremely lagged or cmd from before a map_restart
		//if ( cmds[i].serverTime > sv.time + 3000 ) {
		//	continue;
		//}
		// don't execute if this is an old cmd which is already executed
		// these old cmds are included when cl_packetdup > 0
		if ( cmds[i].serverTime <= cl->lastUsercmd.serverTime ) {
			continue;
		}
		SV_ClientThink (cl - svs.clients, &cmds[ i ]);
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

	MSG_Bitstream(msg);

	serverId = MSG_ReadLong( msg );
	cl->messageAcknowledge = MSG_ReadLong( msg );

	if (cl->messageAcknowledge < 0) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
		//SV_DropClient( cl, "illegible client message" );
		return;
	}

	cl->reliableAcknowledge = MSG_ReadLong( msg );

	// NOTE: when the client message is fux0red the acknowledgement numbers
	// can be out of range, this could cause the server to send thousands of server
	// commands which the server thinks are not yet acknowledged in SV_UpdateServerCommandsToClient
	if (cl->reliableAcknowledge < cl->reliableSequence - MAX_RELIABLE_COMMANDS) {
		// usually only hackers create messages like this
		// it is more annoying for them to let them hanging
		//SV_DropClient( cl, "illegible client message" );
		cl->reliableAcknowledge = cl->reliableSequence;
		return;
	}
	// if this is a usercmd from a previous gamestate,
	// ignore it or retransmit the current gamestate
	//
	// if the client was downloading, let it stay at whatever serverId and
	// gamestate it was at.  This allows it to keep downloading even when
	// the gamestate changes.  After the download is finished, we'll
	// notice and send it a new game state
	if ( serverId != sv.serverId &&
		!*cl->downloadName ) {
		if ( serverId == sv.restartedServerId ) {
			// they just haven't caught the map_restart yet
			return;
		}
		// if we can tell that the client has dropped the last
		// gamestate we sent them, resend it
		if ( cl->messageAcknowledge > cl->gamestateMessageNum ) {
			Com_DPrintf( "%s : dropped gamestate, resending\n", cl->name );
			SV_SendClientGameState( cl );
		}
		return;
	}

	// this client has acknowledged the new gamestate so it's
	// safe to start sending it the real time again
	if( cl->oldServerTime && serverId == sv.serverId ){
		Com_DPrintf( "%s acknowledged gamestate\n", cl->name );
		cl->oldServerTime = 0;
	}

	// read optional clientCommand strings
	do {
		c = MSG_ReadByte( msg );
		if ( c == clc_EOF ) {
			break;
		}
		if ( c != clc_clientCommand ) {
			break;
		}
		if ( !SV_ClientCommand( cl, msg ) ) {
			return;	// we couldn't execute it because of the flood protection
		}
		if (cl->state == CS_ZOMBIE) {
			return;	// disconnect command
		}
	} while ( 1 );

	// read the usercmd_t
	if ( c == clc_move ) {
		SV_UserMove( cl, msg, qtrue );
	} else if ( c == clc_moveNoDelta ) {
		SV_UserMove( cl, msg, qfalse );
	} else if ( c != clc_EOF ) {
		Com_Printf( "WARNING: bad command byte for client %i\n", (int)(cl - svs.clients) );
	}
}

#endif // SV_CLIENT_USERCMD_H
