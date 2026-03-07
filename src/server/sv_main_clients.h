// sv_main_clients.h -- Client lifecycle management
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_CLIENTS_H
#define SV_MAIN_CLIENTS_H

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
void SV_CalcPings( void ) {
	int			i, j;
	client_t	*cl;
	int			total, count;
	int			delta;
	playerState_t	*ps;

	for (i=0 ; i < sv_maxclients->integer ; i++) {
		cl = &svs.clients[i];
		if ( cl->state != CS_ACTIVE ) {
			cl->ping = 999;
			continue;
		}
		if ( !cl->gentity ) {
			cl->ping = 999;
			continue;
		}
		if ( cl->gentity->r.svFlags & SVF_BOT ) {
			cl->ping = 0;
			continue;
		}

		total = 0;
		count = 0;
		for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
			if ( cl->frames[j].messageAcked == -1 ) {
				continue;
			}
			delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
			count++;
			total += delta;
		}
		if (!count) {
			cl->ping = 999;
		} else {
			cl->ping = total/count;
			if ( cl->ping > 999 ) {
				cl->ping = 999;
			}
			if ( sv_pingFix->integer && cl->ping < 1 )
			{
				cl->ping = 1;
			}
		}

		// let the game dll know about the ping
		ps = SV_GameClientNum( i );
		ps->ping = cl->ping;
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer
seconds, drop the conneciton.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts( void ) {
	int		i;
	client_t	*cl;
	int			droppoint;
	int			zombiepoint;

	droppoint = svs.time - 1000 * sv_timeout->integer;
	zombiepoint = svs.time - 1000 * sv_zombietime->integer;

	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		// message times may be wrong across a changelevel
		if (cl->lastPacketTime > svs.time) {
			cl->lastPacketTime = svs.time;
		}

		if (cl->state == CS_ZOMBIE
		&& cl->lastPacketTime < zombiepoint) {
			Com_DPrintf( "Going from CS_ZOMBIE to CS_FREE for %s\n", cl->name );
			cl->state = CS_FREE;	// can now be reused
			continue;
		}
		if ( cl->state >= CS_CONNECTED && cl->lastPacketTime < droppoint) {
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient (cl, "timed out");
				cl->state = CS_FREE;	// don't bother with zombie state
			}
		} else {
			cl->timeoutCount = 0;
		}
	}
}

/*
==================
SV_CheckPaused
==================
*/
qboolean SV_CheckPaused( void ) {
	int		count;
	client_t	*cl;
	int		i;

	if ( !cl_paused->integer ) {
		return qfalse;
	}

	// only pause if there is just a single client connected
	count = 0;
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++) {
		if ( cl->state >= CS_CONNECTED && cl->netchan.remoteAddress.type != NA_BOT ) {
			count++;
		}
	}

	if ( count > 1 ) {
		// don't pause
		sv_paused->integer = 0;
		return qfalse;
	}

	sv_paused->integer = 1;
	return qtrue;
}

void SV_CheckCvars(void) {
	static int lastModHostname = -1, lastModFramerate = -1, lastModSnapsMin = -1, lastModSnapsMax = -1;
	static int lastModEnforceSnaps = -1;
	qboolean changed = qfalse;

	if (sv_hostname->modificationCount != lastModHostname) {
		char hostname[MAX_INFO_STRING];
		char *c = hostname;
		lastModHostname = sv_hostname->modificationCount;

		strcpy(hostname, sv_hostname->string);
		while (*c)
		{
			if ((*c == '\\') || (*c == ';') || (*c == '"'))
			{
				*c = '.';
				changed = qtrue;
			}
			c++;
		}
		if (changed)
		{
			Cvar_Set("sv_hostname", hostname);
		}
	}

	// check limits on client "snaps" value based on server framerate and snapshot rate
	if (sv_fps->modificationCount != lastModFramerate ||
		sv_minSnaps->modificationCount != lastModSnapsMin ||
		sv_maxSnaps->modificationCount != lastModSnapsMax ||
		sv_enforceSnaps->modificationCount != lastModEnforceSnaps)
	{
		client_t *cl;
		int i;

		lastModFramerate = sv_fps->modificationCount;
		lastModSnapsMin = sv_minSnaps->modificationCount;
		lastModSnapsMax = sv_maxSnaps->modificationCount;
		lastModEnforceSnaps = sv_enforceSnaps->modificationCount;

		for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
			if ( cl->state >= CS_CONNECTED ) {
				SV_ClientUpdateSnaps( cl );
			}
		}
	}
}

#endif // SV_MAIN_CLIENTS_H
