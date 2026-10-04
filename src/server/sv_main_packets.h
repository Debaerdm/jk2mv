// sv_main_packets.h -- Packet event processing
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_PACKETS_H
#define SV_MAIN_PACKETS_H

/*
=================
SV_PacketEvent

Main entry point for incoming packets
=================
*/
void SV_PacketEvent( netadr_t from, msg_t *msg ) {
	static unsigned	droppedDisconnects;
	static int		lastDroppedMsg;
	int			i;
	client_t	*cl;
	int			qport;
	int			now;

	// check for connectionless packet (0xffffffff) first
	if ( msg->cursize >= 4 && *(int *)msg->data == -1) {
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );				// sequence number
	qport = MSG_ReadShort( msg ) & 0xffff;

	// find which client the message is from
	for (i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if (cl->state == CS_FREE) {
			continue;
		}
		if ( !NET_CompareBaseAdr( from, cl->netchan.remoteAddress ) ) {
			continue;
		}
		// it is possible to have multiple clients from a single IP
		// address, so they are differentiated by the qport variable
		if (cl->netchan.qport != qport) {
			continue;
		}

		// the IP port can't be used to differentiate them, because
		// some address translating routers periodically change UDP
		// port assignments
		if (cl->netchan.remoteAddress.port != from.port) {
			Com_Printf( "SV_ReadPackets: fixing up a translated port\n" );
			cl->netchan.remoteAddress.port = from.port;
		}

		// make sure it is a valid, in sequence packet
		if (SV_Netchan_Process(cl, msg)) {
			// zombie clients still need to do the Netchan_Process
			// to make sure they don't need to retransmit the final
			// reliable message, but they don't do any other processing
			if (cl->state != CS_ZOMBIE) {
				cl->lastPacketTime = svs.time;	// don't timeout
				SV_ExecuteClientMessage( cl, msg );
			}
		}
		return;
	}

	// if we received a sequenced packet from an address we don't reckognize,
	// send an out of band disconnect packet to it, 10 per second at most,
	// or a flood of packets with a spoofed source would be reflected at it
	now = Sys_Milliseconds();
	if ( SVC_RateLimitDisconnect( from, now ) ) {
		droppedDisconnects++;
		if ( lastDroppedMsg + 1000 < now ) {
			Com_DPrintf( "SV_PacketEvent: rate limit exceeded, dropped %u disconnect replies\n", droppedDisconnects );
			droppedDisconnects = 0;
			lastDroppedMsg = now;
		}
		return;
	}

	NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
}

#endif // SV_MAIN_PACKETS_H
