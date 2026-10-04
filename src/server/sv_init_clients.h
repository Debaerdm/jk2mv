// sv_init_clients.h -- Client array and baseline management
// Extracted from sv_init.cpp as part of aggressive server refactoring

#ifndef SV_INIT_CLIENTS_H
#define SV_INIT_CLIENTS_H

/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void ) {
	sharedEntity_t *svent;
	int				entnum;

	for ( entnum = 0; entnum < sv.num_entities ; entnum++ ) {
		svent = SV_GentityNum(entnum);
		if (!svent->r.linked) {
			continue;
		}
		svent->s.number = entnum;

		// take current state as baseline
		sv.svEntities[entnum].baseline = svent->s;
	}
}

/*
===============
SV_BoundMaxClients

===============
*/
void SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va("%i", minimum) );
	} else if ( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_Set( "sv_maxclients", va("%i", MAX_CLIENTS) );
	}
}

/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}
	SV_BoundMaxClients( 1 );

	svs.clients = (struct client_s *)Z_Malloc (sizeof(client_t) * sv_maxclients->integer, TAG_CLIENTS, qtrue );
	if ( com_dedicated->integer ) {
		Cvar_Set( "r_ghoul2animsmooth", "0");
		Cvar_Set( "r_ghoul2unsqashaftersmooth", "0");
	}
	svs.initialized = qtrue;

	Cvar_Set( "sv_running", "1" );
}

/*
==================
SV_ChangeMaxClients
==================
*/
void SV_ChangeMaxClients( void ) {
	int		oldMaxClients;
	int		i;
	client_t	*oldClients;
	int		count;

	// get the highest client number in use
	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if (i > count)
				count = i;
		}
	}
	count++;

	oldMaxClients = sv_maxclients->integer;
	// never go below the highest client number in use
	SV_BoundMaxClients( count );
	// if still the same
	if ( sv_maxclients->integer == oldMaxClients ) {
		return;
	}

	oldClients = (struct client_s *)Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( i = 0 ; i < count ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		}
		else {
			Com_Memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
	svs.clients = (struct client_s *)Z_Malloc ( sv_maxclients->integer * sizeof(client_t), TAG_CLIENTS, qtrue );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof(client_t) );

	// copy the clients over
	for ( i = 0 ; i < count ; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );
}

/*
===============
SV_AllocSnapshotEntities

Sizes the snapshot entity ring for sv_maxclients, taking a latched
sv_snapshotEntityBudget now, and allocates it.  Called on every map load,
once the previous ring is freed.
===============
*/
void SV_AllocSnapshotEntities( void ) {
	int		budget;
	int		wanted;

	// take a latched value now
	Cvar_Get( "sv_snapshotEntityBudget", XSTRING( SNAPSHOT_ENTITY_BUDGET_DEFAULT ), CVAR_ARCHIVE | CVAR_LATCH );
	budget = SV_ClampSnapshotEntityBudget( sv_snapshotEntityBudget->integer );
	if ( budget != sv_snapshotEntityBudget->integer ) {
		Cvar_Set( "sv_snapshotEntityBudget", va( "%i", budget ) );
	}
	sv_snapshotEntityBudget->modified = qfalse;

	// a listen server used to keep 4 frames per slot instead of 32, but
	// remote clients delta from frames as old as on a dedicated server, and
	// there the bots and the local client build a snapshot every client
	// frame rather than every server frame
	wanted = SV_SnapshotEntityRingSize( sv_maxclients->integer, PACKET_BACKUP, budget );

	// the largest rings may not fit in the address space of a 32-bit server,
	// and a smaller ring only costs more full snapshots
	svs.numSnapshotEntities = wanted;
	while ( ( svs.snapshotEntities = new (std::nothrow) entityState_s[svs.numSnapshotEntities] ) == NULL ) {
		if ( svs.numSnapshotEntities <= SNAPSHOT_ENTITY_RING_MIN ) {
			Com_Error( ERR_FATAL, "Couldn't allocate %i snapshot entities", svs.numSnapshotEntities );
		}
		svs.numSnapshotEntities >>= 1;
	}
	if ( svs.numSnapshotEntities < wanted ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: not enough memory for %i snapshot entities (sv_snapshotEntityBudget %i), using %i\n",
			wanted, budget, svs.numSnapshotEntities );
	}
	// we CAN afford to do this here, since we know the STL vectors in Ghoul2 are empty
	memset( svs.snapshotEntities, 0, sizeof( entityState_t ) * svs.numSnapshotEntities );

	Com_DPrintf( "Snapshot entity ring: %i entities, %i KB\n", svs.numSnapshotEntities,
		(int)( (long long)svs.numSnapshotEntities * sizeof( entityState_t ) / 1024 ) );
}

#endif // SV_INIT_CLIENTS_H
