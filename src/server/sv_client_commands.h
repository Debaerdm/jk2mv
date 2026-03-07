// sv_client_commands.h -- Client command execution
// Extracted from sv_client.cpp as part of aggressive server refactoring

#ifndef SV_CLIENT_COMMANDS_H
#define SV_CLIENT_COMMANDS_H

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
const char *SV_GetStripEdString(char *refSection, char *refName);
static void SV_Disconnect_f( client_t *cl ) {
	SV_DropClient( cl, SV_GetStripEdString("SVINGAME","DISCONNECTED") );
}

/*
=================
SV_VerifyPaks_f

If we are pure, disconnect the client if they do no meet the following conditions:

1. the first two checksums match our view of cgame and ui
2. there are no any additional checksums that we do not have

=================
*/
static void SV_VerifyPaks_f( client_t *cl ) {
	int nChkSum1, nChkSum2, nClientPaks, nServerPaks, i, j, nCurArg;
	int nClientChkSum[1024];
	int nServerChkSum[1024];
	const char *pPaks, *pArg;
	qboolean bGood = qtrue;

	if ( sv_pure->integer != 0 ) {

		nChkSum1 = nChkSum2 = 0;
		bGood = (qboolean)(FS_FileIsInPAK("vm/cgame.qvm", &nChkSum1) == 1);
		if (bGood)
			bGood = (qboolean)(FS_FileIsInPAK("vm/ui.qvm", &nChkSum2) == 1);

		nClientPaks = Cmd_Argc();

		nCurArg = 1;

		while (bGood) {

			if (nClientPaks < 6) {
				bGood = qfalse;
				break;
			}
			pArg = Cmd_Argv(nCurArg++);
			if (!pArg || *pArg == '@' || atoi(pArg) != nChkSum1 ) {
				bGood = qfalse;
				break;
			}
			pArg = Cmd_Argv(nCurArg++);
			if (!pArg || *pArg == '@' || atoi(pArg) != nChkSum2 ) {
				bGood = qfalse;
				break;
			}
			pArg = Cmd_Argv(nCurArg++);
			if (*pArg != '@') {
				bGood = qfalse;
				break;
			}
			for (i = 0; nCurArg < nClientPaks; i++) {
				nClientChkSum[i] = atoi(Cmd_Argv(nCurArg++));
			}

			nClientPaks = i - 1;

			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nClientPaks; j++) {
					if (i == j)
						continue;
					if (nClientChkSum[i] == nClientChkSum[j]) {
						bGood = qfalse;
						break;
					}
				}
				if (bGood == qfalse)
					break;
			}
			if (bGood == qfalse)
				break;

			pPaks = FS_LoadedPakPureChecksums();
			Cmd_TokenizeString( pPaks );
			nServerPaks = Cmd_Argc();
			if (nServerPaks > 1024)
				nServerPaks = 1024;

			for (i = 0; i < nServerPaks; i++) {
				nServerChkSum[i] = atoi(Cmd_Argv(i));
			}

			for (i = 0; i < nClientPaks; i++) {
				for (j = 0; j < nServerPaks; j++) {
					if (nClientChkSum[i] == nServerChkSum[j]) {
						break;
					}
				}
				if (j >= nServerPaks) {
					bGood = qfalse;
					break;
				}
			}
			if ( bGood == qfalse ) {
				break;
			}

			nChkSum1 = sv.checksumFeed;
			for (i = 0; i < nClientPaks; i++) {
				nChkSum1 ^= nClientChkSum[i];
			}
			nChkSum1 ^= nClientPaks;
			if (nChkSum1 != nClientChkSum[nClientPaks]) {
				bGood = qfalse;
				break;
			}

			break;
		}

		if (bGood) {
			cl->pureAuthentic = 1;
		}
		else {
			cl->pureAuthentic = 0;
			cl->nextSnapshotTime = -1;
			cl->state = CS_ACTIVE;
			SV_SendClientSnapshot( cl );
			SV_DropClient( cl, "Unpure client detected. Invalid .PK3 files referenced!" );
		}
	}
}

/*
=================
SV_ResetPureClient_f
=================
*/
static void SV_ResetPureClient_f( client_t *cl ) {
	cl->pureAuthentic = 0;
}

typedef struct {
	const char	*name;
	void		(*func)( client_t *cl );
} ucmd_t;

static const ucmd_t ucmds[] = {
	{"userinfo", SV_UpdateUserinfo_f},
	{"disconnect", SV_Disconnect_f},
	{"cp", SV_VerifyPaks_f},
	{"vdr", SV_ResetPureClient_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
	{"stopdl", SV_StopDownload_f},
	{"donedl", SV_DoneDownload_f},

	{NULL, NULL}
};

/*
==================
SV_ExecuteClientCommand

Also called by bot code
==================
*/
void SV_ExecuteClientCommand( client_t *cl, const char *s, qboolean clientOK ) {
	const ucmd_t *u;
	const char *cmd;
	const char *arg1;
	const char *arg2;
	qboolean foundCommand = qfalse;

	Cmd_TokenizeString(s);
	cmd = Cmd_Argv(0);
	arg1 = Cmd_Argv(1);
	arg2 = Cmd_Argv(2);

	for (u=ucmds ; u->name ; u++) {
		if (!strcmp (cmd, u->name) ) {
			foundCommand = qtrue;
			u->func( cl );
			break;
		}
	}

	if (clientOK) {
		if (!u->name && sv.state == SS_GAME) {
			if (!Q_stricmp(cmd, "say") || !Q_stricmp(cmd, "say_team") || !Q_stricmp(cmd, "tell")) {
				for (int i = 1; i < Cmd_Argc(); i++) {
					if (strpbrk(Cmd_Argv(i), "\n\r")) {
						return;
					}
				}
			}

			if (!Q_stricmp(cmd, "vsay") || !Q_stricmp(cmd, "vsay_team") || !Q_stricmp(cmd, "vtell") || !Q_stricmp(cmd, "vosay") ||
				!Q_stricmp(cmd, "vosay_team") || !Q_stricmp(cmd, "votell") || !Q_stricmp(cmd, "vtaunt")) {
				return;
			}

			if ((!Q_stricmp(cmd, "callvote") || !Q_stricmp(cmd, "callteamvote"))) {
				if (strpbrk(arg1, ";\n\r") || strpbrk(arg2, ";\n\r")) {
					return;
				}
			}

			if (!Q_stricmp(cmd, "team") && (!Q_stricmp(arg1, "follow1") || !Q_stricmp(arg1, "follow2"))) {
				return;
			}

			VM_Call( gvm, GAME_CLIENT_COMMAND, cl - svs.clients );
		}
	}
	else if ( !foundCommand )
	{
		Com_DPrintf( "client text ignored for %s\n", cl->name );
	}
}

/*
===============
SV_ClientCommand
===============
*/
static qboolean SV_ClientCommand( client_t *cl, msg_t *msg ) {
	int		seq;
	const char	*s;
	qboolean clientOk = qtrue;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	if ( cl->lastClientCommand >= seq ) {
		return qtrue;
	}

	Com_DPrintf( "clientCommand: %s : %i : %s\n", cl->name, seq, s );

	if ( seq > cl->lastClientCommand + 1 ) {
		Com_Printf( "Client %s lost %i clientCommands\n", cl->name,
			seq - cl->lastClientCommand + 1 );
		SV_DropClient( cl, "Lost reliable commands" );
		return qfalse;
	}

	if ( !com_cl_running->integer && sv_floodProtect->integer )
	{
		if ( sv_floodProtect->integer == 1 && svs.time < cl->nextReliableTime )
		{
			clientOk = qfalse;
		}
		else if ( SVC_RateLimit(&cl->cmdBucket, sv_floodProtect->integer, 1000, svs.time) )
		{
			clientOk = qfalse;
		}
	}

	cl->nextReliableTime = svs.time + 1000;

	SV_ExecuteClientCommand( cl, s, clientOk );

	cl->lastClientCommand = seq;
	Com_sprintf(cl->lastClientCommandString, sizeof(cl->lastClientCommandString), "%s", s);

	return qtrue;
}

#endif // SV_CLIENT_COMMANDS_H
