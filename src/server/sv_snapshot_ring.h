// sv_snapshot_ring.h -- size and bookkeeping of the snapshot entity ring
//
// Every snapshot the server builds copies the states of its entities into
// svs.snapshotEntities, a ring shared by all clients, so the frames a
// client deltas from stay available for a while.  The ring holds
// sv_maxclients * PACKET_BACKUP * sv_snapshotEntityBudget entities, rounded
// up to a power of two so an entry is found with a mask.  A client whose
// delta base has left the ring gets a full snapshot instead.
//
// No engine dependency, so the unit tests use it as is.

#ifndef SV_SNAPSHOT_RING_H
#define SV_SNAPSHOT_RING_H

#define	MAX_SNAPSHOT_ENTITIES	1024	// entities a snapshot can hold on the server

// sv_snapshotEntityBudget: while snapshots average no more entities than
// this, the ring keeps the last PACKET_BACKUP frames of every slot even when
// each slot builds a snapshot every server frame, as bots do, so no client
// loses a delta base it may still use.  64 is the classic ring.  With 31
// bots on ffa_bespin at sv_fps 40, snapshots average about 60 entities, more
// in fights: the classic ring kept at worst the last 0.62 s of snapshots,
// less than half the 1.4 s a client at 20 snapshots a second may delta
// from, and 128 kept 1.37 s.  A larger ring mostly keeps frames no client
// can ask for.
#define	SNAPSHOT_ENTITY_BUDGET_MIN		64
#define	SNAPSHOT_ENTITY_BUDGET_DEFAULT	128		// a plain number: also the cvar's default string
#define	SNAPSHOT_ENTITY_BUDGET_MAX		MAX_SNAPSHOT_ENTITIES

// the smallest ring: two full snapshots, so a snapshot can't overwrite its
// own entities or those of the snapshot built just before it
#define	SNAPSHOT_ENTITY_RING_MIN		( 2 * MAX_SNAPSHOT_ENTITIES )

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
snapshots each (PACKET_BACKUP), rounded up to a power of two and never below
SNAPSHOT_ENTITY_RING_MIN.
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

	size = SNAPSHOT_ENTITY_RING_MIN;
	while ( size < wanted && size < ( 1 << 30 ) ) {
		size <<= 1;
	}
	return size;
}

/*
===============
SV_SnapshotEntityIndex

Slot of the ring that holds entry n of the counter.  ringSize is a power of
two, so the mask gives the same slot as n % ringSize for any n >= 0.
===============
*/
static inline int SV_SnapshotEntityIndex( int n, int ringSize ) {
	return n & ( ringSize - 1 );
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
