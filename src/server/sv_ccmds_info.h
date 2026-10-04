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
================
SV_ClientStats_f

Packets, usercmds and game module time per client over the last second.
A command of its own, as tools parse the output of status.
================
*/
static void SV_ClientStats_f( void )
{
	int				i, j, k;
	int				rate;
	client_t		*cl;
	char			spaces[32];
	char			displayName[MAX_NAME_LENGTH];
	usercmdCounts_t	all;

	// make sure server is running
	if ( !com_sv_running->integer )
	{
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() > 1 )
	{
		if ( Q_stricmp( Cmd_Argv(1), "reset" ) )
		{
			Com_Printf( "Usage: clientstats [reset]\n" );
			return;
		}

		for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
		{
			Com_Memset( &cl->usercmdStats.peak, 0, sizeof( cl->usercmdStats.peak ) );
			cl->usercmdStats.totalDropped = 0;
		}
		Com_Printf( "Cleared the highest values and dropped usercmds of every client.\n" );
		return;
	}

	rate = SV_UsercmdRateCap( sv_maxUsercmdRate->integer );
	if ( rate )
	{
		Com_Printf( "sv_maxUsercmdRate: %i usercmds per second, %i in a row\n", rate, SV_UsercmdBurst( rate ) );
	}
	else
	{
		Com_Printf( "sv_maxUsercmdRate: off\n" );
	}

	Com_Memset( &all, 0, sizeof( all ) );

	Com_Printf ("num name            pkt/s cmd/s drop/s think/s maxcmd/s maxthink/s  dropped\n");
	Com_Printf ("--- --------------- ----- ----- ------ ------- -------- ---------- --------\n");
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
	{
		const usercmdStats_t	*stats = &cl->usercmdStats;
		int						dropped;

		if ( cl->state < CS_CONNECTED )
		{
			continue;
		}

		// pad and cut the name to 15 visible characters, like status
		k = Q_PrintStrlen(cl->name, MV_USE102COLOR);
		if ( k < 0 ) k = 0; // Should never happen
		for( j = 0; j < (15 - k); j++ ) spaces[j] = ' ';
		spaces[j] = 0;
		Q_PrintStrCopy( displayName, cl->name, sizeof(displayName), 0, 15, MV_USE102COLOR );

		if ( stats->totalDropped <= 0 )
		{
			dropped = 0;
		}
		else if ( stats->totalDropped > 0x7fffffff )
		{
			dropped = 0x7fffffff;
		}
		else
		{
			dropped = (int)stats->totalDropped;
		}

		Com_Printf ("%3i %s^7%s %5i %5i %6i %7i %8i %10i %8i\n",
			i,
			displayName,
			spaces,
			stats->last.packets,
			stats->last.cmds,
			stats->last.dropped,
			stats->last.thinkUsec,
			stats->peak.cmds,
			stats->peak.thinkUsec,
			dropped
			);

		all.packets += stats->last.packets;
		all.cmds += stats->last.cmds;
		all.dropped += stats->last.dropped;
		all.thinkUsec += stats->last.thinkUsec;
	}
	Com_Printf ("    all             %5i %5i %6i %7i\n", all.packets, all.cmds, all.dropped, all.thinkUsec);
	Com_Printf ("think: microseconds in GAME_CLIENT_THINK (bots move in the game frame)\n");
	Com_Printf ("max: highest per second, dropped: total, both since connecting\n");
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
