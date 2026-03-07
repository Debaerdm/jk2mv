// sv_client_connect.h -- Client connection establishment
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_CONNECT_H
#define SV_CLIENT_CONNECT_H

/*
==================
SV_DirectConnect

A "connect" OOB command has been received
==================
*/
void SV_DirectConnect( netadr_t from ) {
	char		userinfo[MAX_INFO_STRING];
	int			i;
	client_t	*cl, *newcl;
	client_t	temp;
	sharedEntity_t *ent;
	int			clientNum;
	int			version;
	int			qport;
	int			challenge;
	char		*password;
	int			startIndex;
	const char	*denied;
	int			count;
	const char	*ip;

	Com_DPrintf ("SVC_DirectConnect ()\n");

	Q_strncpyz( userinfo, Cmd_Argv(1), sizeof(userinfo) );

	version = atoi( Info_ValueForKey( userinfo, "protocol" ) );
	if ( version != MV_GetCurrentProtocol() ) {
		NET_OutOfBandPrint(NS_SERVER, from, "print\nServer uses protocol version %i.\n", MV_GetCurrentProtocol());
		Com_DPrintf ("rejected connect from version %i\n", version);
		return;
	}

	challenge = atoi( Info_ValueForKey( userinfo, "challenge" ) );
	qport = atoi( Info_ValueForKey( userinfo, "qport" ) );

	// quick reject
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		if ( NET_CompareBaseAdr( from, cl->netchan.remoteAddress )
			&& ( cl->netchan.qport == qport
			|| from.port == cl->netchan.remoteAddress.port ) ) {
			if (( svs.time - cl->lastConnectTime)
				< (sv_reconnectlimit->integer * 1000)) {
				Com_DPrintf ("%s:reconnect rejected : too soon\n", NET_AdrToString (from));
				return;
			}
			break;
		}
	}

	// don't let "ip" overflow userinfo string
	if ( NET_IsLocalAddress (from) )
		ip = "localhost";
	else
		ip = NET_AdrToString( from );

	if ( !Info_SetValueForKey( userinfo, "ip", ip ) )
	{
		NET_OutOfBandPrint( NS_SERVER, from,
			"print\nUserinfo string length exceeded.  "
			"Try removing setu cvars from your config.\n" );
		return;
	}

	// q3fill protection
	if (!Sys_IsLANAddress(from)) {
		int connectingip = 0;

		for (i = 0; i < sv_maxclients->integer; i++) {
			if (svs.clients[i].netchan.remoteAddress.type != NA_BOT && svs.clients[i].state == CS_CONNECTED && NET_CompareBaseAdr(svs.clients[i].netchan.remoteAddress, from)) {
				connectingip++;
			}
		}

		if (connectingip >= 3) {
			NET_OutOfBandPrint(NS_SERVER, from, "print\nPlease wait...\n");
			Com_DPrintf("%s:connect rejected : too many simultaneously connecting clients\n", NET_AdrToString(from));
			return;
		}
	}

	// see if the challenge is valid (LAN clients don't need to challenge)
	if ( !NET_IsLocalAddress (from) ) {
		int		ping;
		challenge_t *challengeptr;

		for (i=0 ; i<MAX_CHALLENGES ; i++) {
			if (NET_CompareAdr(from, svs.challenges[i].adr)) {
				if ( challenge == svs.challenges[i].challenge ) {
					break;		// good
				}
			}
		}
		if (i == MAX_CHALLENGES) {
			NET_OutOfBandPrint( NS_SERVER, from, "print\nNo or bad challenge for address.\n" );
			return;
		}

		challengeptr = &svs.challenges[i];
		if (challengeptr->wasrefused) {
			return;
		}

		ping = svs.time - svs.challenges[i].pingTime;

		if ( !Sys_IsLANAddress( from ) ) {
			if ( ( sv_minPing->value && ping < sv_minPing->value ) && !svs.hibernation.enabled ) {
				NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is for high pings only\n" );
				Com_DPrintf ("Client %i rejected on a too low ping\n", i);
				challengeptr->wasrefused = qtrue;
				return;
			}

			if ( ( sv_maxPing->value && ping > sv_maxPing->value ) && !svs.hibernation.enabled ) {
				NET_OutOfBandPrint( NS_SERVER, from, "print\nServer is for low pings only\n" );
				Com_DPrintf ("Client %i rejected on a too high ping\n", i);
				challengeptr->wasrefused = qtrue;
				return;
			}
		}

		Com_Printf("Client %i connecting with %i challenge ping\n", i, ping);
		challengeptr->connected = qtrue;
	}

	newcl = &temp;
	Com_Memset (newcl, 0, sizeof(client_t));

	// if there is already a slot for this ip, reuse it
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state == CS_FREE ) {
			continue;
		}
		if ( NET_CompareBaseAdr( from, cl->netchan.remoteAddress )
			&& ( cl->netchan.qport == qport
			|| from.port == cl->netchan.remoteAddress.port ) ) {
			Com_Printf ("%s:reconnect\n", NET_AdrToString (from));
			newcl = cl;
			VM_Call( gvm, GAME_CLIENT_DISCONNECT, newcl - svs.clients );
			goto gotnewcl;
		}
	}

	// find a client slot
	password = Info_ValueForKey( userinfo, "password" );
	if ( *password && !strcmp( password, sv_privatePassword->string ) ) {
		startIndex = 0;
	} else {
		startIndex = sv_privateClients->integer;
	}

	newcl = NULL;
	for ( i = startIndex; i < sv_maxclients->integer ; i++ ) {
		cl = &svs.clients[i];
		if (cl->state == CS_FREE) {
			newcl = cl;
			break;
		}
	}

	if ( !newcl ) {
		if ( NET_IsLocalAddress( from ) ) {
			count = 0;
			for ( i = startIndex; i < sv_maxclients->integer ; i++ ) {
				cl = &svs.clients[i];
				if (cl->netchan.remoteAddress.type == NA_BOT) {
					count++;
				}
			}
			if (count >= sv_maxclients->integer - startIndex) {
				SV_DropClient(&svs.clients[sv_maxclients->integer - 1], "only bots on server");
				newcl = &svs.clients[sv_maxclients->integer - 1];
			}
			else {
				Com_Error( ERR_FATAL, "server is full on local connect" );
				return;
			}
		}
		else {
			const char *SV_GetStripEdString(char *refSection, char *refName);
			NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", SV_GetStripEdString("SVINGAME","SERVER_IS_FULL"));
			Com_DPrintf ("Rejected a connection.\n");
			return;
		}
	}

	cl->reliableAcknowledge = 0;
	cl->reliableSequence = 0;

gotnewcl:
	*newcl = temp;
	clientNum = newcl - svs.clients;
	ent = SV_GentityNum( clientNum );
	newcl->gentity = ent;
	newcl->challenge = challenge;

	Netchan_Setup (NS_SERVER, &newcl->netchan , from, qport);
	NET_HTTP_AllowClient( clientNum, from );

	Q_strncpyz( newcl->userinfo, userinfo, sizeof(newcl->userinfo) );
	SV_UserinfoChanged(newcl);

	denied = (const char *)VM_Call( gvm, GAME_CLIENT_CONNECT, clientNum, qtrue, qfalse );
	if ( denied ) {
		denied = (const char *)VM_ExplicitArgString( gvm, (intptr_t)denied );
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", denied );
		Com_DPrintf ("Game rejected a connection: %s.\n", denied);
		return;
	}

	if ( svs.hibernation.enabled ) {
		svs.hibernation.enabled = false;
		Com_Printf("Server restored from hibernation\n");
	}

	SV_UserinfoChanged( newcl );

	NET_OutOfBandPrint( NS_SERVER, from, "connectResponse" );

	Com_DPrintf( "Going from CS_FREE to CS_CONNECTED for %s\n", newcl->name );

	newcl->state = CS_CONNECTED;
	newcl->nextSnapshotTime = svs.time;
	newcl->lastPacketTime = svs.time;
	newcl->lastConnectTime = svs.time;
	newcl->gamestateMessageNum = -1;
	newcl->lastUserInfoChange = 0;
	newcl->lastUserInfoCount = 0;

	count = 0;
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
		}
	}
	if ( count == 1 || count == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}
}

#endif // SV_CLIENT_CONNECT_H
