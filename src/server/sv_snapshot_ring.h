// sv_snapshot_ring.h -- size and bookkeeping of the snapshot entity ring
//
// Every snapshot the server builds copies the states of its entities into
// svs.snapshotEntities, a ring shared by all clients, so the frames a
// client deltas from stay available for a while.  The ring holds
// sv_maxclients * frames per client * sv_snapshotEntityBudget entities,
// rounded up to a power of two so an entry is found with a mask.
//
// No engine dependency, so the unit tests use it as is.

#ifndef SV_SNAPSHOT_RING_H
#define SV_SNAPSHOT_RING_H

#define	MAX_SNAPSHOT_ENTITIES	1024	// entities a snapshot can hold on the server

// sv_snapshotEntityBudget: the average number of entities per snapshot the
// ring can keep for every frame of every client
#define	SNAPSHOT_ENTITY_BUDGET_MIN		64		// the classic ring
#define	SNAPSHOT_ENTITY_BUDGET_DEFAULT	256		// MAX_ENTITIES_IN_SNAPSHOT, what a client can take
#define	SNAPSHOT_ENTITY_BUDGET_MAX		MAX_SNAPSHOT_ENTITIES

// the counter of ring entries must stay below this
#define	SNAPSHOT_ENTITIES_LIMIT			0x7FFFFFFE

static inline int SV_ClampSnapshotEntityBudget( int budget ) {
	if ( budget < SNAPSHOT_ENTITY_BUDGET_MIN ) {
		return SNAPSHOT_ENTITY_BUDGET_MIN;
	}
	if ( budget > SNAPSHOT_ENTITY_BUDGET_MAX ) {
		return SNAPSHOT_ENTITY_BUDGET_MAX;
	}
	return budget;
}

/*
===============
SV_SnapshotEntityRingSize

Number of entries in the ring for maxClients slots that keep framesPerClient
snapshots each (PACKET_BACKUP on a dedicated server), rounded up to a power
of two.  It never goes below two full snapshots, so even on a small listen
server a snapshot can't overwrite its own entities or those of the snapshot
built just before it.
===============
*/
static inline int SV_SnapshotEntityRingSize( int maxClients, int framesPerClient, int budget ) {
	long long	wanted;
	int			size;

	if ( maxClients < 1 ) {
		maxClients = 1;
	}
	if ( framesPerClient < 1 ) {
		framesPerClient = 1;
	}
	wanted = (long long)maxClients * framesPerClient * SV_ClampSnapshotEntityBudget( budget );
	if ( wanted < 2 * MAX_SNAPSHOT_ENTITIES ) {
		wanted = 2 * MAX_SNAPSHOT_ENTITIES;
	}

	size = 2 * MAX_SNAPSHOT_ENTITIES;
	while ( size < wanted && size < ( 1 << 30 ) ) {
		size <<= 1;
	}
	return size;
}

/*
===============
SV_SnapshotEntitiesRolledOff

True when the entities of a frame that starts at firstEntity may have been
overwritten by the ones built since: the ring only holds the last ringSize
entries before nextEntity.
===============
*/
static inline bool SV_SnapshotEntitiesRolledOff( int firstEntity, int nextEntity, int ringSize ) {
	return firstEntity <= nextEntity - ringSize;
}

/*
===============
SV_SnapshotEntitiesWrapping

True when the entry counter is close enough to SNAPSHOT_ENTITIES_LIMIT that
the server must be restarted before the next frame.  The margin covers what
can be built before the following check, whatever the ring size: one
snapshot per client in the frame, one more for a client dropped in between
(pure check) and two per client from SV_FinalMessage when the server shuts
down for the restart, each of up to MAX_SNAPSHOT_ENTITIES entities.
===============
*/
static inline bool SV_SnapshotEntitiesWrapping( int nextEntity, int maxClients ) {
	return nextEntity >= SNAPSHOT_ENTITIES_LIMIT - 4 * maxClients * MAX_SNAPSHOT_ENTITIES;
}

#endif // SV_SNAPSHOT_RING_H
