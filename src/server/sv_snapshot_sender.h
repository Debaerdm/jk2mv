// sv_snapshot_sender.h -- Message sending and rate limiting
// Extracted from sv_snapshot.cpp as part of aggressive server refactoring

#ifndef SV_SNAPSHOT_SENDER_H
#define SV_SNAPSHOT_SENDER_H

/*
====================
SV_RateMsec

Return the number of msec a given size message is supposed
to take to clear, based on the current rate
====================
*/
#define	HEADER_RATE_BYTES	48		// include our header, IP header, and some overhead

static int SV_RateMsec( client_t *client, int messageSize ) {
	int		rate = SV_ClientRate( client );
	int		rateMsec;

	// individual messages will never be larger than fragment size
	if ( messageSize > 1500 ) {
		messageSize = 1500;
	}
	rateMsec = ( messageSize + HEADER_RATE_BYTES ) * 1000 / rate;

	return rateMsec;
}

/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
void SV_SendMessageToClient( msg_t *msg, client_t *client ) {
	int			rateMsec;

	// MW - my attempt to fix illegible server message errors caused by
	// packet fragmentation of initial snapshot.
	while(client->state&&client->netchan.unsentFragments)
	{
		// send additional message fragments if the last message
		// was too large to send at once
		Com_Printf ("[ISM]SV_SendClientGameState() [1] for %s, writing out old fragments\n", client->name);
		SV_Netchan_TransmitNextFragment(&client->netchan);
	}

	// record information about the message
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
	// With sv_pingFix enabled we use a time value that is not limited by sv_fps.
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = (sv_pingFix->integer ? Sys_Milliseconds() : svs.time);
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = -1;

	// send the datagram
	SV_Netchan_Transmit( client, msg );

	// set nextSnapshotTime based on rate and requested number of updates

	// local clients get snapshots every frame
	if ( client->netchan.remoteAddress.type == NA_LOOPBACK || Sys_IsLANAddress (client->netchan.remoteAddress) ) {
		client->nextSnapshotTime = svs.time - 1;
		return;
	}

	// normal rate / snapshotMsec calculation
	rateMsec = SV_RateMsec( client, msg->cursize );

	if ( rateMsec < client->snapshotMsec ) {
		// never send more packets than this, no matter what the rate is at
		rateMsec = client->snapshotMsec;
		client->rateDelayed = qfalse;
	} else {
		client->rateDelayed = qtrue;
	}

	client->nextSnapshotTime = svs.time + rateMsec;

	// don't pile up empty snapshots while connecting
	if ( client->state != CS_ACTIVE ) {
		// a gigantic connection message may have already put the nextSnapshotTime
		// more than a second away, so don't shorten it
		// do shorten if client is downloading
		if ( !*client->downloadName && client->nextSnapshotTime < svs.time + 1000 ) {
			client->nextSnapshotTime = svs.time + 1000;
		}
	}
}

/*
=======================
SV_SendClientSnapshot

Also called by SV_FinalMessage
=======================
*/
void SV_SendClientSnapshot( client_t *client ) {
	byte		msg_buf[MAX_MSGLEN];
	msg_t		msg;
	msg_t		msgBak;

	// build the snapshot
	SV_BuildClientSnapshot( client );

	// bots need to have their snapshots build, but
	// the query them directly without needing to be sent
	if ( client->gentity && client->gentity->r.svFlags & SVF_BOT ) {
		return;
	}

	MSG_Init (&msg, msg_buf, sizeof(msg_buf));
	msg.allowoverflow = qtrue;

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// (re)send any reliable server commands
	if ( !SV_UpdateServerCommandsToClient(client, &msg, qtrue) ) {
		// If we can't fit all commands in a single message send what we got and
		// don't even try to send entities
		SV_SendMessageToClient( &msg, client );
		return;
	}

	// Backup the msg state in case the snapshot would overflow it
	memcpy( &msgBak, &msg, sizeof(msgBak) );

	// send over all the relevant entityState_t
	// and the playerState_t
	SV_WriteSnapshotToClient( client, &msg );

	if ( sv_dynamicSnapshots->integer && msg.overflowed && !msgBak.overflowed ) {
		// The entity states were too much and the message overflowed. So send
		// the old state of the message from before we tried to append the
		// entity states. As the net code doesn't send the msg_buf content after
		// the current size of the message we don't have to clear anything and
		// we can just use the old msg values (which point to the updated buffer).
		SV_SendMessageToClient( &msgBak, client );
		return;
	}

	// Backup the msg state in case the download would overflow it
	memcpy( &msgBak, &msg, sizeof(msgBak) );

	// Add any download data if the client is downloading
	SV_WriteDownloadToClient( client, &msg );

	if ( sv_dynamicSnapshots->integer && msg.overflowed && !msgBak.overflowed ) {
		// Downloads usually don't happen in situations that are likely to have
		// message overflows, but let's make sure and apply the same logic we
		// used for the entity states.
		SV_SendMessageToClient( &msgBak, client );
		return;
	}

	// check for overflow
	if ( msg.overflowed ) {
		Com_Printf ("WARNING: msg overflowed for %s\n", client->name);
		MSG_Clear (&msg);
	}

	SV_SendMessageToClient( &msg, client );
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void ) {
	int			i;
	client_t	*c;

	// send a message to each connected client
	for (i=0, c = svs.clients ; i < sv_maxclients->integer ; i++, c++) {
		if (!c->state) {
			continue;		// not connected
		}

		if ( svs.time < c->nextSnapshotTime ) {
			continue;		// not time yet
		}

		// send additional message fragments if the last message
		// was too large to send at once
		if ( c->netchan.unsentFragments ) {
			c->nextSnapshotTime = svs.time +
				SV_RateMsec( c, c->netchan.unsentLength - c->netchan.unsentFragmentStart );
			SV_Netchan_TransmitNextFragment( &c->netchan );
			continue;
		}

		if ( sv.vmPlayerSnapshots && !VM_Call(gvm, GAME_MVAPI_PLAYERSNAPSHOT, i) ) {
			continue;
		}

		// generate and send a new message
		SV_SendClientSnapshot( c );
	}

	if ( sv.vmPlayerSnapshots ) {
		VM_Call( gvm, GAME_MVAPI_PLAYERSNAPSHOT, -1 );
	}
}

#endif // SV_SNAPSHOT_SENDER_H
