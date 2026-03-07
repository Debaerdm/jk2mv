// sv_ccmds_kick.h -- Player kick console commands
// Extracted from sv_ccmds.cpp as part of aggressive server refactoring

#ifndef SV_CCMDS_KICK_H
#define SV_CCMDS_KICK_H

static void SV_KickByName( const char *name )
{
	client_t	*cl;
	int			i;

	// make sure server is running
	if ( !com_sv_running->integer )
	{
		return;
	}

	cl = SV_GetPlayerByFedName(name);
	if ( !cl )
	{
		if ( !Q_stricmp(name, "all") )
		{
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ )
			{
				if ( !cl->state )
				{
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK )
				{
					continue;
				}
				SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp(name, "allbots") )
		{
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ )
			{
				if ( !cl->state )
				{
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT )
				{
					continue;
				}
				SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK )
	{
		SV_SendServerCommand(NULL, "print \"%s\"", SV_GetStripEdString("SVINGAME","CANNOT_KICK_HOST"));
		return;
	}

	SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	client_t	*cl;
	int			i;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	if (!Q_stricmp(Cmd_Argv(1), "Padawan"))
	{ //if you try to kick the default name, also try to kick ""
		SV_KickByName("");
	}

	cl = SV_GetPlayerByName();
	if ( !cl ) {
		if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", SV_GetStripEdString("SVINGAME","CANNOT_KICK_HOST"));
		return;
	}

	SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kicknum <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", SV_GetStripEdString("SVINGAME","CANNOT_KICK_HOST"));
		return;
	}

	SV_DropClient( cl, SV_GetStripEdString("SVINGAME","WAS_KICKED"));	// "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

#endif // SV_CCMDS_KICK_H
