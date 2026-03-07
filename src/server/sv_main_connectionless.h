// sv_main_connectionless.h -- Connectionless packet handling
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_CONNECTIONLESS_H
#define SV_MAIN_CONNECTIONLESS_H

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/
void SVC_Status( netadr_t from ) {
	char	player[1024];
	char	status[MAX_MSGLEN];
	int		i;
	client_t	*cl;
	playerState_t	*ps;
	size_t	statusLength;
	size_t	playerLength;
	char	infostring[MAX_INFO_STRING];

	strcpy( infostring, Cvar_InfoString( CVAR_SERVERINFO ) );

	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	// add "demo" to the sv_keywords if restricted
	if ( Cvar_VariableValue( "fs_restrict" ) ) {
		char	keywords[MAX_INFO_STRING];

		Com_sprintf( keywords, sizeof( keywords ), "demo %s",
			Info_ValueForKey( infostring, "sv_keywords" ) );
		Info_SetValueForKey( infostring, "sv_keywords", keywords );
	}

	status[0] = 0;
	statusLength = 0;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state >= CS_CONNECTED ) {
			ps = SV_GameClientNum( i );
			Com_sprintf (player, sizeof(player), "%i %i \"%s\"\n",
				ps->persistant[PERS_SCORE], cl->ping, cl->name);
			playerLength = strlen(player);
			if (statusLength + playerLength >= sizeof(status) ) {
				break;		// can't hold any more
			}
			strcpy (status + statusLength, player);
			statusLength += playerLength;
		}
	}

	Info_SetValueForKey(infostring, "version", com_version->string);

	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status );
}

/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
void SVC_Info( netadr_t from ) {
	int		i, count, wDisable;
	const char *gamedir;
	char	infostring[MAX_INFO_STRING];

	// q3infoboom exploit
	if (strlen(Cmd_Argv(1)) > 128)
		return;

	// don't count privateclients
	count = 0;
	for ( i = sv_privateClients->integer ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
		}
	}

	infostring[0] = 0;

	// echo back the parameter to status. so servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", Cmd_Argv(1) );

	Info_SetValueForKey( infostring, "protocol", va("%i", MV_GetCurrentProtocol()) );
	Info_SetValueForKey( infostring, "hostname", sv_hostname->string );
	Info_SetValueForKey( infostring, "mapname", sv_mapname->string );
	Info_SetValueForKey( infostring, "clients", va("%i", count) );
	Info_SetValueForKey( infostring, "sv_maxclients",
		va("%i", sv_maxclients->integer - sv_privateClients->integer ) );
	Info_SetValueForKey( infostring, "gametype", va("%i", sv_gametype->integer ) );
	Info_SetValueForKey( infostring, "needpass", va("%i", sv_needpass->integer ) );
	Info_SetValueForKey( infostring, "truejedi", va("%i", Cvar_VariableIntegerValue( "g_jediVmerc" ) ) );
	if ( sv_gametype->integer == GT_TOURNAMENT )
	{
		wDisable = Cvar_VariableIntegerValue( "g_duelWeaponDisable" );
	}
	else
	{
		wDisable = Cvar_VariableIntegerValue( "g_weaponDisable" );
	}
	Info_SetValueForKey( infostring, "wdisable", va("%i", wDisable ) );
	Info_SetValueForKey( infostring, "fdisable", va("%i", Cvar_VariableIntegerValue( "g_forcePowerDisable" ) ) );

	if( sv_minPing->integer ) {
		Info_SetValueForKey( infostring, "minPing", va("%i", sv_minPing->integer) );
	}
	if( sv_maxPing->integer ) {
		Info_SetValueForKey( infostring, "maxPing", va("%i", sv_maxPing->integer) );
	}
	gamedir = Cvar_VariableString( "fs_game" );
	if( *gamedir ) {
		Info_SetValueForKey( infostring, "game", gamedir );
	}
	Info_SetValueForKey( infostring, "sv_allowAnonymous", va("%i", sv_allowAnonymous->integer) );

	// webserver port
	if (mv_httpdownloads->integer) {
		if (Q_stristr(mv_httpserverport->string, "http://")) {
			Info_SetValueForKey(infostring, "mvhttpurl", mv_httpserverport->string);
		} else {
			Info_SetValueForKey(infostring, "mvhttp", va("%i", sv.http_port));
		}
	}

	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring );
}

/*
================
SV_FlushRedirect

================
*/
void SV_FlushRedirect( char *outputbuf ) {
	NET_OutOfBandPrint( NS_SERVER, svs.redirectAddress, "print\n%s", outputbuf );
}

/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
void SVC_RemoteCommand( netadr_t from, msg_t *msg ) {
	qboolean	valid;
#define	SV_OUTPUTBUF_LENGTH MAX_STRING_CHARS
	char		sv_outputbuf[SV_OUTPUTBUF_LENGTH];

	if ( !strlen( sv_rconPassword->string ) ||
		strcmp (Cmd_Argv(1), sv_rconPassword->string) ) {
		valid = qfalse;
		Com_DPrintf ("Bad rcon from %s: %s\n", NET_AdrToString (from), Cmd_ArgsFrom(2) );
	} else {
		valid = qtrue;
		Com_DPrintf ("Rcon from %s: %s\n", NET_AdrToString (from), Cmd_ArgsFrom(2) );
	}

	// start redirecting all print outputs to the packet
	svs.redirectAddress = from;
	Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect, qfalse);

	if ( !strlen( sv_rconPassword->string ) ) {
		Com_Printf ("No rconpassword set.\n");
	} else if ( !valid ) {
		Com_Printf ("Bad rconpassword.\n");
	} else {
		SVC_WhitelistAdr( from );

		Cmd_DropArg (1);
		Cmd_DropArg (0);
		Cmd_Execute ();
	}

	Com_EndRedirect ();
}

/*
===============
MVAPI_GetConnectionlessPacket
===============
*/
static mvaddr_t curraddr;
static char currmessage[MAX_STRING_CHARS];

qboolean MVAPI_GetConnectionlessPacket(mvaddr_t *addr, char *buf, int bufsize) {
	if (!mv_apiConnectionless->integer) {
		return qtrue;
	}

	if (currmessage[0] == 0) {
		return qtrue;
	}

	Com_Memcpy(addr, &curraddr, sizeof(curraddr));
	Q_strncpyz(buf, currmessage, bufsize);
	return qfalse;
}

/*
===============
MVAPI_SendConnectionlessPacket
===============
*/
qboolean MVAPI_SendConnectionlessPacket(const mvaddr_t *addr, const char *message) {
	netadr_t nativeAdr;

	if (!mv_apiConnectionless->integer) {
		return qtrue;
	}

	if (addr->type != MV_IPV4) {
		return qtrue;
	}

	nativeAdr.type = NA_IP;
	nativeAdr.ip[0] = addr->ip.v4[0];
	nativeAdr.ip[1] = addr->ip.v4[1];
	nativeAdr.ip[2] = addr->ip.v4[2];
	nativeAdr.ip[3] = addr->ip.v4[3];
	nativeAdr.port = addr->port;

	NET_OutOfBandPrint(NS_SERVER, nativeAdr, "%s", message);
	return qfalse;
}

qboolean mvStructConversionDisabled = qfalse;
qboolean MVAPI_DisableStructConversion(qboolean disable)
{
	mvStructConversionDisabled = disable;

	return qfalse;
}

// ============================================================================
// CONNECTIONLESS PACKET DISPATCHER
// ============================================================================

typedef enum {
	SVC_INVALID,
	SVC_CONNECT,
	SVC_GETSTATUS,
	SVC_FIRST = SVC_GETSTATUS,
	SVC_GETINFO,
	SVC_GETCHALLENGE,
	SVC_RCON,
	SVC_DISCONNECT,
	SVC_MVAPI,
	SVC_MAX
} svcType_t;

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
void SV_ConnectionlessPacket( netadr_t from, msg_t *msg ) {
	static unsigned droppedAdr;
	static unsigned dropped[SVC_MAX];
	static int lastMsgAdr;
	static int lastMsg[SVC_MAX];
	static leakyBucket_t bucket[SVC_MAX];
	static const char * const commands[SVC_MAX] = {
		"invalid",
		"connect",
		"getstatus",
		"getinfo",
		"getchallenge",
		"rcon",
		"disconnect",
		"mvapi"
	};

	char	*s;
	char	*c;

	svcType_t cmd = SVC_INVALID;
	int now = Sys_Milliseconds();

	if (SVC_RateLimitAddress(from, 10, 1000, now)) {
		if (com_developer && com_developer->integer) {
			Com_DPrintf("SV_ConnectionlessPacket: rate limit from %s exceeded, dropping request\n", NET_AdrToString(from));
		}
		droppedAdr++;
		return;
	}

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );		// skip the -1 marker

	if (!Q_strncmp("connect", (const char *)&msg->data[4], 7)) {
		Huff_Decompress(msg, 12);
		cmd = SVC_CONNECT;
	}

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );
	c = Q_strlwr(Cmd_Argv(0));

	for (int i = SVC_FIRST; i < SVC_MAX && cmd == SVC_INVALID; i++) {
		if (!strcmp(c, commands[i])) {
			cmd = (svcType_t)i;
		}
	}

	int rate = Com_Clampi(1, 1000, sv_maxOOBRate->integer);
	int period = 1000 / rate;
	int burst = rate;

	// Whitelisted IPs get 2x burst capacity
	if (SVC_IsWhitelisted(from)) {
		burst *= 2;
	}

	if (SVC_RateLimit(&bucket[cmd], burst, period, now)) {
		dropped[cmd]++;
		return;
	}

	if (dropped[cmd] > 0 && lastMsg[cmd] + 1000 < now) {
		Com_Printf("SV_ConnectionlessPacket: \"%s\" rate limit exceeded, dropped %d requests\n", commands[cmd], dropped[cmd]);
		dropped[cmd] = 0;
		lastMsg[cmd] = now;
	}

	if (droppedAdr > 0 && lastMsgAdr + 1000 < now) {
		Com_Printf("SV_ConnectionlessPacket: IP rate limit exceeded, dropped %d requests\n", droppedAdr);
		droppedAdr = 0;
		lastMsgAdr = now;
	}

	Com_DPrintf ("SV packet %s : %s\n", NET_AdrToString(from), c);

	switch (cmd) {
	case SVC_GETSTATUS:
		SVC_Status( from );
		break;
	case SVC_GETINFO:
		SVC_Info( from );
		break;
	case SVC_GETCHALLENGE:
		SV_GetChallenge( from );
		break;
	case SVC_CONNECT:
		SV_DirectConnect( from );
		break;
	case SVC_RCON:
		SVC_RemoteCommand( from, msg );
		break;
	case SVC_DISCONNECT:
		// if a client starts up a local server, we may see some spurious
		// server disconnect messages when their new server sees our final
		// sequenced messages to the old client
		break;
	case SVC_MVAPI:
		if (VM_MVAPILevel(gvm) >= 1 && from.type == NA_IP) {
			Q_strncpyz(currmessage, s, sizeof(currmessage));
			curraddr.type = MV_IPV4;
			curraddr.ip.v4[0] = from.ip[0];
			curraddr.ip.v4[1] = from.ip[1];
			curraddr.ip.v4[2] = from.ip[2];
			curraddr.ip.v4[3] = from.ip[3];
			curraddr.port = from.port;

			VM_Call(gvm, GAME_MVAPI_RECV_CONNECTIONLESSPACKET);

			currmessage[0] = 0;
		}
		break;
	default:
		Com_DPrintf("bad connectionless packet from %s:\n%s\n", NET_AdrToString(from), s);
		break;
	}
}

#endif // SV_MAIN_CONNECTIONLESS_H
