// sv_ccmds_info.h -- Server information console commands
// Extracted from sv_ccmds.cpp as part of aggressive server refactoring

#ifndef SV_CCMDS_INFO_H
#define SV_CCMDS_INFO_H

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void )
{
	int				i;
	client_t		*cl;
	playerState_t	*ps;
	const char		*s;
	int				ping;
	char			state[32];
	qboolean		avoidTruncation = qfalse;

	int				j, k;
	char			spaces[32];
	char			displayName[MAX_NAME_LENGTH];

	// make sure server is running
	if ( !com_sv_running->integer )
	{
		Com_Printf( "%s", SP_GetStringText(STR_SERVER_SERVER_NOT_RUNNING) );
		return;
	}

	if ( Cmd_Argc() > 1 )
	{
		if (!Q_stricmp("notrunc", Cmd_Argv(1)))
		{
			avoidTruncation = qtrue;
		}
	}

	Com_Printf ("map: %s\n", sv_mapname->string );

	Com_Printf ("num score ping name            lastmsg address               qport rate\n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
	{
		if (!cl->state)
		{
			continue;
		}

		if (cl->state == CS_CONNECTED)
		{
			strcpy(state, "CNCT ");
		}
		else if (cl->state == CS_ZOMBIE)
		{
			strcpy(state, "ZMBI ");
		}
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			sprintf(state, "%4i", ping);
		}

		ps = SV_GameClientNum( i );
		s = NET_AdrToString( cl->netchan.remoteAddress );

		// Count the length of the visible characters in the name and if it's less than 15 fill the rest with spaces
		k = Q_PrintStrlen(cl->name, MV_USE102COLOR);
		if ( k < 0 ) k = 0; // Should never happen
		for( j = 0; j < (15 - k); j++ ) spaces[j] = ' ';
		spaces[j] = 0;

		if (!avoidTruncation) {
			// Limit the visible length of the name to 15 characters (not counting colors)
			Q_PrintStrCopy( displayName, cl->name, sizeof(displayName), 0, 15, MV_USE102COLOR );
		} else {
			Q_strncpyz( displayName, cl->name, sizeof(displayName) );
		}

		Com_Printf ("%3i %5i %s %s^7%s %7i %21s %5i %5i\n",
			i,
			ps->persistant[PERS_SCORE],
			state,
			displayName,
			spaces,
			svs.time - cl->lastPacketTime,
			s,
			cl->netchan.qport,
			SV_ClientRate(cl)
			);
	}
	Com_Printf ("\n");
}

/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
	}
}

/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}

/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByName();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}

#endif // SV_CCMDS_INFO_H
