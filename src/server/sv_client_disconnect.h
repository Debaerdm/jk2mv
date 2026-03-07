// sv_client_disconnect.h -- Client disconnect logic
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_DISCONNECT_H
#define SV_CLIENT_DISCONNECT_H

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing -- SV_FinalMessage() will handle that
=====================
*/
void SV_DropClient( client_t *drop, const char *reason ) {
	int		i;
	challenge_t	*challenge;

	if ( drop->state == CS_ZOMBIE ) {
		return;
	}

	if (drop->netchan.remoteAddress.type != NA_BOT) {
		challenge = &svs.challenges[0];

		for (i = 0; i < MAX_CHALLENGES; i++, challenge++) {
			if (NET_CompareAdr(drop->netchan.remoteAddress, challenge->adr)) {
				Com_Memset(challenge, 0, sizeof(*challenge));
				break;
			}
		}
	}

	if ( !drop->gentity || !(drop->gentity->r.svFlags & SVF_BOT) ) {
		challenge = &svs.challenges[0];

		for (i = 0 ; i < MAX_CHALLENGES ; i++, challenge++) {
			if ( NET_CompareAdr( drop->netchan.remoteAddress, challenge->adr ) ) {
				challenge->connected = qfalse;
				break;
			}
		}
	}

	SV_CloseDownload( drop );
	NET_HTTP_DenyClient( drop - svs.clients );

	SV_SendServerCommand( NULL, "print \"%s" S_COLOR_WHITE " %s\n\"", drop->name, reason );

	Com_DPrintf( "Going to CS_ZOMBIE for %s\n", drop->name );
	drop->state = CS_ZOMBIE;

	VM_Call( gvm, GAME_CLIENT_DISCONNECT, drop - svs.clients );

	SV_SendServerCommand( drop, "disconnect" );

	if ( drop->netchan.remoteAddress.type == NA_BOT ) {
		SV_BotFreeClient( drop - svs.clients );
	}

	SV_SetUserinfo( drop - svs.clients, "" );

	int players = 0;
	for (i = 0; i < sv_maxclients->integer; i++) {
		if (svs.clients[i].state >= CS_CONNECTED && svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
			players++;
		}
	}

	for (i=0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			break;
		}
	}
	if ( i == sv_maxclients->integer ) {
		SV_Heartbeat_f();
	}
}

#endif // SV_CLIENT_DISCONNECT_H
