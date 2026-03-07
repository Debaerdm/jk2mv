// sv_client_gamestate.h -- Gamestate transmission
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_GAMESTATE_H
#define SV_CLIENT_GAMESTATE_H

/*
================
SV_SendClientGameState

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each new map load.

It will be resent if the client acknowledges a later message but has
the wrong gamestate.
================
*/
void SV_SendClientGameState( client_t *client ) {
	int			start;
	entityState_t	*base, nullstate;
	msg_t		msg;
	byte		msgBuffer[MAX_MSGLEN];

	while(client->state&&client->netchan.unsentFragments)
	{
		Com_Printf ("[ISM]SV_SendClientGameState() [2] for %s, writing out old fragments\n", client->name);
		SV_Netchan_TransmitNextFragment(&client->netchan);
	}

	Com_DPrintf ("SV_SendClientGameState() for %s\n", client->name);
	Com_DPrintf( "Going from CS_CONNECTED to CS_PRIMED for %s\n", client->name );
	if (client->state == CS_CONNECTED)
		client->state = CS_PRIMED;
	client->pureAuthentic = 0;

	client->gamestateMessageNum = client->netchan.outgoingSequence;

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) );

	MSG_WriteLong( &msg, client->lastClientCommand );

	SV_UpdateServerCommandsToClient( client, &msg );

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, client->reliableSequence );

	for ( start = 0 ; start < MAX_CONFIGSTRINGS ; start++ ) {
		if (sv.configstrings[start][0]) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, start );
			MSG_WriteBigString( &msg, sv.configstrings[start] );
		}
	}

	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( start = 0 ; start < MAX_GENTITIES; start++ ) {
		base = &sv.svEntities[start].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, base, qtrue );
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, client - svs.clients);

	MSG_WriteLong( &msg, sv.checksumFeed);

	SV_SendMessageToClient( &msg, client );
}

void SV_SendClientMapChange( client_t *client )
{
	msg_t		msg;
	byte		msgBuffer[MAX_MSGLEN];

	MSG_Init( &msg, msgBuffer, sizeof( msgBuffer ) );

	MSG_WriteLong( &msg, client->lastClientCommand );

	SV_UpdateServerCommandsToClient( client, &msg );

	MSG_WriteByte( &msg, svc_mapchange );

	SV_SendMessageToClient( &msg, client );
}

/*
==================
SV_ClientEnterWorld
==================
*/
void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd ) {
	int		clientNum;
	sharedEntity_t *ent;

	Com_DPrintf( "Going from CS_PRIMED to CS_ACTIVE for %s\n", client->name );
	client->state = CS_ACTIVE;

	if (sv_autoWhitelist->integer) {
		SVC_WhitelistAdr( client->netchan.remoteAddress );
	}

	clientNum = client - svs.clients;
	ent = SV_GentityNum( clientNum );
	ent->s.number = clientNum;
	client->gentity = ent;

	client->lastUserInfoChange = 0;
	client->lastUserInfoCount = 0;

	client->deltaMessage = -1;
	client->nextSnapshotTime = svs.time;
	client->lastUsercmd = *cmd;

	VM_Call( gvm, GAME_CLIENT_BEGIN, client - svs.clients );
}

#endif // SV_CLIENT_GAMESTATE_H
